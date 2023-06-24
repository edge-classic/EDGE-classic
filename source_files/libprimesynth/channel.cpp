#include "channel.h"

namespace primesynth {
Channel::Channel(double outputRate)
    : outputRate_(outputRate),
      controllers_(),
      rpns_(),
      keyPressures_(),
      channelPressure_(0),
      pitchBend_(1 << 13),
      dataEntryMode_(DataEntryMode::RPN),
      pitchBendSensitivity_(2.0),
      fineTuning_(0.0),
      coarseTuning_(0.0),
      currentNoteID_(0) {
    controllers_.at(static_cast<std::size_t>(midi::ControlChange::Volume)) = 100;
    controllers_.at(static_cast<std::size_t>(midi::ControlChange::Pan)) = 64;
    controllers_.at(static_cast<std::size_t>(midi::ControlChange::Expression)) = 127;
    controllers_.at(static_cast<std::size_t>(midi::ControlChange::RPNLSB)) = 127;
    controllers_.at(static_cast<std::size_t>(midi::ControlChange::RPNMSB)) = 127;
    voices_.reserve(128);
}

midi::Bank Channel::getBank() const {
    return {controllers_.at(static_cast<std::size_t>(midi::ControlChange::BankSelectMSB)),
            controllers_.at(static_cast<std::size_t>(midi::ControlChange::BankSelectLSB))};
}

bool Channel::hasPreset() const {
    return static_cast<bool>(preset_);
}

void Channel::noteOff(std::uint8_t key) {
    const bool sustained = controllers_.at(static_cast<std::size_t>(midi::ControlChange::Sustain)) >= 64;

    std::lock_guard<std::mutex> lockGuard(mutex_);
    for (const auto& voice : voices_) {
        if (voice->getActualKey() == key) {
            voice->release(sustained);
        }
    }
}

void Channel::noteOn(std::uint8_t key, std::uint8_t velocity) {
    if (velocity == 0) {
        noteOff(key);
        return;
    }

    for (const Zone& presetZone : preset_->zones) {
        if (presetZone.isInRange(key, velocity)) {
            const std::int16_t instID = presetZone.generators.getOrDefault(sf::Generator::Instrument);
            const auto& inst = preset_->soundFont.getInstruments().at(instID);
            for (const Zone& instZone : inst.zones) {
                if (instZone.isInRange(key, velocity)) {
                    const std::int16_t sampleID = instZone.generators.getOrDefault(sf::Generator::SampleID);
                    const auto& sample = preset_->soundFont.getSamples().at(sampleID);

                    auto generators = instZone.generators;
                    generators.add(presetZone.generators);

                    auto modparams = instZone.modulatorParameters;
                    modparams.mergeAndAdd(presetZone.modulatorParameters);
                    modparams.merge(ModulatorParameterSet::getDefaultParameters());

                    auto voice = std::make_unique<Voice>(currentNoteID_, outputRate_, sample, generators, modparams,
                                                         key, velocity);
                    voice->setPercussion(preset_->bank == PERCUSSION_BANK);
                    addVoice(std::move(voice));
                }
            }
        }
    }
    ++currentNoteID_;
}

void Channel::keyPressure(std::uint8_t key, std::uint8_t value) {
    keyPressures_.at(key) = value;

    std::lock_guard<std::mutex> lockGuard(mutex_);
    for (const auto& voice : voices_) {
        if (voice->getActualKey() == key) {
            voice->updateSFController(sf::GeneralController::PolyPressure, value);
        }
    }
}

void Channel::controlChange(std::uint8_t controller, std::uint8_t value) {
    controllers_.at(controller) = value;

    std::lock_guard<std::mutex> lockGuard(mutex_);
    switch (static_cast<midi::ControlChange>(controller)) {
    case midi::ControlChange::DataEntryMSB:
    case midi::ControlChange::DataEntryLSB:
        if (dataEntryMode_ == DataEntryMode::RPN) {
            const std::uint16_t rpn = getSelectedRPN();
            if (rpn < static_cast<std::uint16_t>(midi::RPN::Last)) {
                const std::uint16_t data =
                    midi::joinBytes(controllers_.at(static_cast<std::size_t>(midi::ControlChange::DataEntryMSB)),
                                    controllers_.at(static_cast<std::size_t>(midi::ControlChange::DataEntryLSB)));
                rpns_.at(rpn) = data;
                updateRPN();
            }
        }
        break;
    case midi::ControlChange::Sustain:
        if (value < 64) {
            for (const auto& voice : voices_) {
                if (voice->getStatus() == Voice::State::Sustained) {
                    voice->release(false);
                }
            }
        }
        break;
    case midi::ControlChange::DataIncrement:
        if (dataEntryMode_ == DataEntryMode::RPN) {
            const std::uint16_t rpn = getSelectedRPN();
            if (rpn < static_cast<std::uint16_t>(midi::RPN::Last) && rpns_.at(rpn) >> 7 < 127) {
                rpns_.at(rpn) += 1 << 7;
                updateRPN();
            }
        }
        break;
    case midi::ControlChange::DataDecrement:
        if (dataEntryMode_ == DataEntryMode::RPN) {
            const std::uint16_t rpn = getSelectedRPN();
            if (rpn < static_cast<std::uint16_t>(midi::RPN::Last) && rpns_.at(rpn) >> 7 > 0) {
                rpns_.at(rpn) -= 1 << 7;
                updateRPN();
            }
        }
        break;
    case midi::ControlChange::NRPNMSB:
    case midi::ControlChange::NRPNLSB:
        dataEntryMode_ = DataEntryMode::NRPN;
        break;
    case midi::ControlChange::RPNMSB:
    case midi::ControlChange::RPNLSB:
        dataEntryMode_ = DataEntryMode::RPN;
        break;
    case midi::ControlChange::AllSoundOff:
        voices_.clear();
        break;
    case midi::ControlChange::ResetAllControllers:
        // See "General MIDI System Level 1 Developer Guidelines" Second Revision
        // p.5 'Response to "Reset All Controllers" Message'
        keyPressures_ = {};
        channelPressure_ = 0;
        pitchBend_ = 1 << 13;
        for (const auto& voice : voices_) {
            voice->updateSFController(sf::GeneralController::ChannelPressure, channelPressure_);
            voice->updateSFController(sf::GeneralController::PitchWheel, pitchBend_);
        }
        for (std::uint8_t i = 1; i < 122; ++i) {
            if ((91 <= i && i <= 95) || (70 <= i && i <= 79)) {
                continue;
            }
            switch (static_cast<midi::ControlChange>(i)) {
            case midi::ControlChange::Volume:
            case midi::ControlChange::Pan:
            case midi::ControlChange::BankSelectLSB:
            case midi::ControlChange::AllSoundOff:
                break;
            case midi::ControlChange::Expression:
            case midi::ControlChange::RPNLSB:
            case midi::ControlChange::RPNMSB:
                controllers_.at(i) = 127;
                for (const auto& voice : voices_) {
                    voice->updateMIDIController(i, 127);
                }
                break;
            default:
                controllers_.at(i) = 0;
                for (const auto& voice : voices_) {
                    voice->updateMIDIController(i, 0);
                }
                break;
            }
        }
        break;
    case midi::ControlChange::AllNotesOff: {
        // See "The Complete MIDI 1.0 Detailed Specification" Rev. April 2006
        // p.A-6 'The Relationship Between the Hold Pedal and "All Notes Off"'

        // All Notes Off is affected by CC 64 (Sustain)
        const bool sustained = controllers_.at(static_cast<std::size_t>(midi::ControlChange::Sustain)) >= 64;
        for (const auto& voice : voices_) {
            voice->release(sustained);
        }
        break;
    }
    default:
        for (const auto& voice : voices_) {
            voice->updateMIDIController(controller, value);
        }
        break;
    }
}

void Channel::channelPressure(std::uint8_t value) {
    channelPressure_ = value;
    std::lock_guard<std::mutex> lockGuard(mutex_);
    for (const auto& voice : voices_) {
        voice->updateSFController(sf::GeneralController::ChannelPressure, value);
    }
}

void Channel::pitchBend(std::uint16_t value) {
    pitchBend_ = value;
    std::lock_guard<std::mutex> lockGuard(mutex_);
    for (const auto& voice : voices_) {
        voice->updateSFController(sf::GeneralController::PitchWheel, value);
    }
}

void Channel::setPreset(const std::shared_ptr<const Preset>& preset) {
    preset_ = preset;
}

StereoValue Channel::render() {
    StereoValue sum{0.0, 0.0};
    std::lock_guard<std::mutex> lockGuard(mutex_);
    for (const auto& voice : voices_) {
        if (voice->getStatus() == Voice::State::Finished) {
            continue;
        }
        voice->update();

        if (voice->getStatus() == Voice::State::Finished) {
            continue;
        }
        sum += voice->render();
    }
    return sum;
}

std::uint16_t Channel::getSelectedRPN() const {
    return midi::joinBytes(controllers_.at(static_cast<std::size_t>(midi::ControlChange::RPNMSB)),
                           controllers_.at(static_cast<std::size_t>(midi::ControlChange::RPNLSB)));
}

void Channel::addVoice(std::unique_ptr<Voice> voice) {
    voice->updateSFController(sf::GeneralController::PolyPressure, keyPressures_.at(voice->getActualKey()));
    voice->updateSFController(sf::GeneralController::ChannelPressure, channelPressure_);
    voice->updateSFController(sf::GeneralController::PitchWheel, pitchBend_);
    voice->updateSFController(sf::GeneralController::PitchWheelSensitivity, pitchBendSensitivity_);
    voice->updateFineTuning(fineTuning_);
    voice->updateCoarseTuning(coarseTuning_);
    for (std::uint8_t i = 0; i < midi::NUM_CONTROLLERS; ++i) {
        voice->updateMIDIController(i, controllers_.at(i));
    }

    const auto exclusiveClass = voice->getExclusiveClass();

    std::lock_guard<std::mutex> lockGuard(mutex_);
    if (exclusiveClass != 0) {
        for (const auto& v : voices_) {
            if (v->getNoteID() != currentNoteID_ && v->getExclusiveClass() == exclusiveClass) {
                v->release(false);
            }
        }
    }

    for (auto& v : voices_) {
        if (v->getStatus() == Voice::State::Finished) {
            v = std::move(voice);
            return;
        }
    }
    voices_.emplace_back(std::move(voice));
}

void Channel::updateRPN() {
    const std::uint16_t rpn = getSelectedRPN();
    const auto data = static_cast<std::int32_t>(rpns_.at(rpn));
    switch (static_cast<midi::RPN>(rpn)) {
    case midi::RPN::PitchBendSensitivity:
        pitchBendSensitivity_ = data / 128.0;
        for (const auto& voice : voices_) {
            voice->updateSFController(sf::GeneralController::PitchWheelSensitivity, pitchBendSensitivity_);
        }
        break;
    case midi::RPN::FineTuning: {
        fineTuning_ = (data - 8192) / 81.92;
        for (const auto& voice : voices_) {
            voice->updateFineTuning(fineTuning_);
        }
        break;
    }
    case midi::RPN::CoarseTuning: {
        coarseTuning_ = (data - 8192) / 128.0;
        for (const auto& voice : voices_) {
            voice->updateCoarseTuning(coarseTuning_);
        }
        break;
    }
    }
}
}
