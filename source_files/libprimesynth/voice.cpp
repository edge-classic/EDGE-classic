#include "voice.h"
#include <cmath>
#include <stdexcept>

namespace primesynth {
static constexpr unsigned int CALC_INTERVAL = 64;

// for compatibility
static constexpr double ATTEN_FACTOR = 0.4;

Voice::Voice(std::size_t noteID, double outputRate, const Sample& sample, const GeneratorSet& generators,
             const ModulatorParameterSet& modparams, std::uint8_t key, std::uint8_t velocity)
    : noteID_(noteID),
      sampleBuffer_(sample.buffer),
      generators_(generators),
      actualKey_(key),
      percussion_(false),
      fineTuning_(0.0),
      coarseTuning_(0.0),
      steps_(0),
      status_(State::Playing),
      index_(sample.start),
      deltaIndex_(0u),
      volume_({1.0, 1.0}),
      amp_(0.0),
      deltaAmp_(0.0),
      volEnv_(outputRate, CALC_INTERVAL),
      modEnv_(outputRate, CALC_INTERVAL),
      vibLFO_(outputRate, CALC_INTERVAL),
      modLFO_(outputRate, CALC_INTERVAL) {
    rtSample_.mode = static_cast<SampleMode>(0b11 & generators.getOrDefault(sf::Generator::SampleModes));
    const std::int16_t overriddenSampleKey = generators.getOrDefault(sf::Generator::OverridingRootKey);
    rtSample_.pitch = (overriddenSampleKey > 0 ? overriddenSampleKey : sample.key) - 0.01 * sample.correction;

    static constexpr std::uint32_t COARSE_UNIT = 32768;
    rtSample_.start = sample.start + COARSE_UNIT * generators.getOrDefault(sf::Generator::StartAddrsCoarseOffset) +
                      generators.getOrDefault(sf::Generator::StartAddrsOffset);
    rtSample_.end = sample.end + COARSE_UNIT * generators.getOrDefault(sf::Generator::EndAddrsCoarseOffset) +
                    generators.getOrDefault(sf::Generator::EndAddrsOffset);
    rtSample_.startLoop = sample.startLoop +
                          COARSE_UNIT * generators.getOrDefault(sf::Generator::StartloopAddrsCoarseOffset) +
                          generators.getOrDefault(sf::Generator::StartloopAddrsOffset);
    rtSample_.endLoop = sample.endLoop +
                        COARSE_UNIT * generators.getOrDefault(sf::Generator::EndloopAddrsCoarseOffset) +
                        generators.getOrDefault(sf::Generator::EndloopAddrsOffset);

    // fix invalid sample range
    const auto bufferSize = static_cast<std::uint32_t>(sample.buffer.size());
    rtSample_.start = std::min(bufferSize - 1, rtSample_.start);
    rtSample_.end = std::max(rtSample_.start + 1, std::min(bufferSize, rtSample_.end));
    rtSample_.startLoop = std::max(rtSample_.start, std::min(rtSample_.end - 1, rtSample_.startLoop));
    rtSample_.endLoop = std::max(rtSample_.startLoop + 1, std::min(rtSample_.end, rtSample_.endLoop));

    deltaIndexRatio_ = 1.0 / conv::keyToHertz(rtSample_.pitch) * sample.sampleRate / outputRate;

    for (const auto& mp : modparams.getParameters()) {
        modulators_.emplace_back(mp);
    }

    const std::int16_t genVelocity = generators.getOrDefault(sf::Generator::Velocity);
    updateSFController(sf::GeneralController::NoteOnVelocity, genVelocity > 0 ? genVelocity : velocity);

    const std::int16_t genKey = generators.getOrDefault(sf::Generator::Keynum);
    const std::int16_t overriddenKey = genKey > 0 ? genKey : key;
    keyScaling_ = 60 - overriddenKey;
    updateSFController(sf::GeneralController::NoteOnKeyNumber, overriddenKey);

    double minModulatedAtten = ATTEN_FACTOR * generators_.getOrDefault(sf::Generator::InitialAttenuation);
    for (const auto& mod : modulators_) {
        if (mod.getDestination() == sf::Generator::InitialAttenuation && mod.canBeNegative()) {
            // mod may increase volume
            minModulatedAtten -= std::abs(mod.getAmount());
        }
    }
    minAtten_ = sample.minAtten + std::max(0.0, minModulatedAtten);

    for (int i = 0; i < NUM_GENERATORS; ++i) {
        modulated_.at(i) = generators.getOrDefault(static_cast<sf::Generator>(i));
    }
    static const auto INIT_GENERATORS = {
        sf::Generator::Pan,           sf::Generator::DelayModLFO,   sf::Generator::FreqModLFO,
        sf::Generator::DelayVibLFO,   sf::Generator::FreqVibLFO,    sf::Generator::DelayModEnv,
        sf::Generator::AttackModEnv,  sf::Generator::HoldModEnv,    sf::Generator::DecayModEnv,
        sf::Generator::SustainModEnv, sf::Generator::ReleaseModEnv, sf::Generator::DelayVolEnv,
        sf::Generator::AttackVolEnv,  sf::Generator::HoldVolEnv,    sf::Generator::DecayVolEnv,
        sf::Generator::SustainVolEnv, sf::Generator::ReleaseVolEnv, sf::Generator::CoarseTune};
    for (const auto& generator : INIT_GENERATORS) {
        updateModulatedParams(generator);
    }
}

std::size_t Voice::getNoteID() const {
    return noteID_;
}

std::uint8_t Voice::getActualKey() const {
    return actualKey_;
}

std::int16_t Voice::getExclusiveClass() const {
    return generators_.getOrDefault(sf::Generator::ExclusiveClass);
}

const Voice::State& Voice::getStatus() const {
    return status_;
}

StereoValue Voice::render() const {
    const std::uint32_t i = index_.getIntegerPart();
    const double r = index_.getFractionalPart();
    const double interpolated = (1.0 - r) * sampleBuffer_.at(i) + r * sampleBuffer_.at(i + 1);
    return amp_ * volume_ * (interpolated / INT16_MAX);
}

void Voice::setPercussion(bool percussion) {
    percussion_ = percussion;
}

void Voice::updateSFController(sf::GeneralController controller, double value) {
    for (auto& mod : modulators_) {
        if (mod.updateSFController(controller, value)) {
            updateModulatedParams(mod.getDestination());
        }
    }
}

void Voice::updateMIDIController(std::uint8_t controller, std::uint8_t value) {
    for (auto& mod : modulators_) {
        if (mod.updateMIDIController(controller, value)) {
            updateModulatedParams(mod.getDestination());
        }
    }
}

void Voice::updateFineTuning(double fineTuning) {
    fineTuning_ = fineTuning;
    updateModulatedParams(sf::Generator::FineTune);
}

void Voice::updateCoarseTuning(double coarseTuning) {
    coarseTuning_ = coarseTuning;
    updateModulatedParams(sf::Generator::CoarseTune);
}

void Voice::release(bool sustained) {
    if (status_ != State::Playing && status_ != State::Sustained) {
        return;
    }

    if (sustained) {
        status_ = State::Sustained;
    } else {
        status_ = State::Released;
        volEnv_.release();
        modEnv_.release();
    }
}

void Voice::update() {
    const bool calc = steps_++ % CALC_INTERVAL == 0;

    if (calc) {
        // dynamic range of signed 16 bit samples in centibel
        static const double DYNAMIC_RANGE = 200.0 * std::log10(INT16_MAX + 1.0);
        if (volEnv_.getPhase() == Envelope::Phase::Finished ||
            (volEnv_.getPhase() > Envelope::Phase::Attack &&
             minAtten_ + 960.0 * (1.0 - volEnv_.getValue()) >= DYNAMIC_RANGE)) {
            status_ = State::Finished;
            return;
        }

        volEnv_.update();
    }

    index_ += deltaIndex_;

    switch (rtSample_.mode) {
    case SampleMode::UnLooped:
    case SampleMode::UnUsed:
        if (index_.getIntegerPart() >= rtSample_.end) {
            status_ = State::Finished;
            return;
        }
        break;
    case SampleMode::Looped:
        if (index_.getIntegerPart() >= rtSample_.endLoop) {
            index_ -= FixedPoint(rtSample_.endLoop - rtSample_.startLoop);
        }
        break;
    case SampleMode::LoopedUntilRelease:
        if (status_ == State::Released) {
            if (index_.getIntegerPart() >= rtSample_.end) {
                status_ = State::Finished;
                return;
            }
        } else if (index_.getIntegerPart() >= rtSample_.endLoop) {
            index_ -= FixedPoint(rtSample_.endLoop - rtSample_.startLoop);
        }
        break;
    default:
        throw std::runtime_error("unknown sample mode");
    }

    amp_ += deltaAmp_;

    if (calc) {
        modEnv_.update();
        vibLFO_.update();
        modLFO_.update();

        const double modEnvValue =
            modEnv_.getPhase() == Envelope::Phase::Attack ? conv::convex(modEnv_.getValue()) : modEnv_.getValue();
        const double pitch =
            voicePitch_ + 0.01 * (getModulatedGenerator(sf::Generator::ModEnvToPitch) * modEnvValue +
                                  getModulatedGenerator(sf::Generator::VibLfoToPitch) * vibLFO_.getValue() +
                                  getModulatedGenerator(sf::Generator::ModLfoToPitch) * modLFO_.getValue());
        deltaIndex_ = FixedPoint(deltaIndexRatio_ * conv::keyToHertz(pitch));

        const double attenModLFO = getModulatedGenerator(sf::Generator::ModLfoToVolume) * modLFO_.getValue();
        const double targetAmp = volEnv_.getPhase() == Envelope::Phase::Attack
                                     ? volEnv_.getValue() * conv::attenuationToAmplitude(attenModLFO)
                                     : conv::attenuationToAmplitude(960.0 * (1.0 - volEnv_.getValue()) + attenModLFO);
        deltaAmp_ = (targetAmp - amp_) / CALC_INTERVAL;
    }
}

double Voice::getModulatedGenerator(sf::Generator type) const {
    return modulated_.at(static_cast<std::size_t>(type));
}

StereoValue calculatePannedVolume(double pan) {
    if (pan <= -500.0) {
        return {1.0, 0.0};
    } else if (pan >= 500.0) {
        return {0.0, 1.0};
    } else {
        static constexpr double FACTOR = 3.141592653589793 / 2000.0;
        return {std::sin(FACTOR * (-pan + 500.0)), std::sin(FACTOR * (pan + 500.0))};
    }
}

void Voice::updateModulatedParams(sf::Generator destination) {
    double& modulated = modulated_.at(static_cast<std::size_t>(destination));
    modulated = generators_.getOrDefault(destination);
    if (destination == sf::Generator::InitialAttenuation) {
        modulated *= ATTEN_FACTOR;
    }
    for (const auto& mod : modulators_) {
        if (mod.getDestination() == destination) {
            modulated += mod.getValue();
        }
    }

    switch (destination) {
    case sf::Generator::Pan:
    case sf::Generator::InitialAttenuation:
        volume_ = conv::attenuationToAmplitude(getModulatedGenerator(sf::Generator::InitialAttenuation)) *
                  calculatePannedVolume(getModulatedGenerator(sf::Generator::Pan));
        break;
    case sf::Generator::DelayModLFO:
        modLFO_.setDelay(modulated);
        break;
    case sf::Generator::FreqModLFO:
        modLFO_.setFrequency(modulated);
        break;
    case sf::Generator::DelayVibLFO:
        vibLFO_.setDelay(modulated);
        break;
    case sf::Generator::FreqVibLFO:
        vibLFO_.setFrequency(modulated);
        break;
    case sf::Generator::DelayModEnv:
        modEnv_.setParameter(Envelope::Phase::Delay, modulated);
        break;
    case sf::Generator::AttackModEnv:
        modEnv_.setParameter(Envelope::Phase::Attack, modulated);
        break;
    case sf::Generator::HoldModEnv:
    case sf::Generator::KeynumToModEnvHold:
        modEnv_.setParameter(Envelope::Phase::Hold,
                             getModulatedGenerator(sf::Generator::HoldModEnv) +
                                 getModulatedGenerator(sf::Generator::KeynumToModEnvHold) * keyScaling_);
        break;
    case sf::Generator::DecayModEnv:
    case sf::Generator::KeynumToModEnvDecay:
        modEnv_.setParameter(Envelope::Phase::Decay,
                             getModulatedGenerator(sf::Generator::DecayModEnv) +
                                 getModulatedGenerator(sf::Generator::KeynumToModEnvDecay) * keyScaling_);
        break;
    case sf::Generator::SustainModEnv:
        modEnv_.setParameter(Envelope::Phase::Sustain, modulated);
        break;
    case sf::Generator::ReleaseModEnv:
        modEnv_.setParameter(Envelope::Phase::Release, modulated);
        break;
    case sf::Generator::DelayVolEnv:
        volEnv_.setParameter(Envelope::Phase::Delay, modulated);
        break;
    case sf::Generator::AttackVolEnv:
        volEnv_.setParameter(Envelope::Phase::Attack, modulated);
        break;
    case sf::Generator::HoldVolEnv:
    case sf::Generator::KeynumToVolEnvHold:
        volEnv_.setParameter(Envelope::Phase::Hold,
                             getModulatedGenerator(sf::Generator::HoldVolEnv) +
                                 getModulatedGenerator(sf::Generator::KeynumToVolEnvHold) * keyScaling_);
        break;
    case sf::Generator::DecayVolEnv:
    case sf::Generator::KeynumToVolEnvDecay:
        volEnv_.setParameter(Envelope::Phase::Decay,
                             getModulatedGenerator(sf::Generator::DecayVolEnv) +
                                 getModulatedGenerator(sf::Generator::KeynumToVolEnvDecay) * keyScaling_);
        break;
    case sf::Generator::SustainVolEnv:
        volEnv_.setParameter(Envelope::Phase::Sustain, modulated);
        break;
    case sf::Generator::ReleaseVolEnv:
        volEnv_.setParameter(Envelope::Phase::Release, modulated);
        break;
    case sf::Generator::CoarseTune:
    case sf::Generator::FineTune:
    case sf::Generator::ScaleTuning:
    case sf::Generator::Pitch:
        voicePitch_ = rtSample_.pitch + 0.01 * getModulatedGenerator(sf::Generator::Pitch) +
                      0.01 * generators_.getOrDefault(sf::Generator::ScaleTuning) * (actualKey_ - rtSample_.pitch) +
                      coarseTuning_ + getModulatedGenerator(sf::Generator::CoarseTune) +
                      0.01 * (fineTuning_ + getModulatedGenerator(sf::Generator::FineTune));
        break;
    }
}
}
