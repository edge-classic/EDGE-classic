#pragma once
#include "envelope.h"
#include "fixed_point.h"
#include "lfo.h"
#include "modulator.h"
#include "soundfont.h"
#include "stereo_value.h"

namespace primesynth {
class Voice {
public:
    enum class State { Playing, Sustained, Released, Finished };

    Voice(std::size_t noteID, double outputRate, const Sample& sample, const GeneratorSet& generators,
          const ModulatorParameterSet& modparams, std::uint8_t key, std::uint8_t velocity);

    std::size_t getNoteID() const;
    std::uint8_t getActualKey() const;
    std::int16_t getExclusiveClass() const;
    const State& getStatus() const;
    StereoValue render() const;

    void setPercussion(bool percussion);
    void updateSFController(sf::GeneralController controller, double value);
    void updateMIDIController(std::uint8_t controller, std::uint8_t value);
    void updateFineTuning(double fineTuning);
    void updateCoarseTuning(double coarseTuning);
    void release(bool sustained);
    void update();

private:
    enum class SampleMode { UnLooped, Looped, UnUsed, LoopedUntilRelease };

    struct RuntimeSample {
        SampleMode mode;
        double pitch;
        std::uint32_t start, end, startLoop, endLoop;
    };

    const std::size_t noteID_;
    const std::uint8_t actualKey_;
    const std::vector<std::int16_t>& sampleBuffer_;
    GeneratorSet generators_;
    RuntimeSample rtSample_;
    int keyScaling_;
    std::vector<Modulator> modulators_;
    double minAtten_;
    std::array<double, NUM_GENERATORS> modulated_;
    bool percussion_;
    double fineTuning_, coarseTuning_;
    double deltaIndexRatio_;
    unsigned int steps_;
    State status_;
    double voicePitch_;
    FixedPoint index_, deltaIndex_;
    StereoValue volume_;
    double amp_, deltaAmp_;
    Envelope volEnv_, modEnv_;
    LFO vibLFO_, modLFO_;

    double getModulatedGenerator(sf::Generator type) const;
    void updateModulatedParams(sf::Generator destination);
};
}
