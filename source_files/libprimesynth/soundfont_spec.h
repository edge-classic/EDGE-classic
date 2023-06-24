#pragma once
#include <cstdint>

namespace primesynth {
namespace sf {
enum class SampleLink : std::uint16_t {
    MonoSample = 1,
    RightSample = 2,
    LeftSample = 4,
    LinkedSample = 8,
    RomMonoSample = 0x8001,
    RomRightSample = 0x8002,
    RomLeftSample = 0x8004,
    RomLinkedSample = 0x8008
};

enum class Generator : std::uint16_t {
    StartAddrsOffset = 0,
    EndAddrsOffset = 1,
    StartloopAddrsOffset = 2,
    EndloopAddrsOffset = 3,
    StartAddrsCoarseOffset = 4,
    ModLfoToPitch = 5,
    VibLfoToPitch = 6,
    ModEnvToPitch = 7,
    InitialFilterFc = 8,
    InitialFilterQ = 9,
    ModLfoToFilterFc = 10,
    ModEnvToFilterFc = 11,
    EndAddrsCoarseOffset = 12,
    ModLfoToVolume = 13,
    ChorusEffectsSend = 15,
    ReverbEffectsSend = 16,
    Pan = 17,
    DelayModLFO = 21,
    FreqModLFO = 22,
    DelayVibLFO = 23,
    FreqVibLFO = 24,
    DelayModEnv = 25,
    AttackModEnv = 26,
    HoldModEnv = 27,
    DecayModEnv = 28,
    SustainModEnv = 29,
    ReleaseModEnv = 30,
    KeynumToModEnvHold = 31,
    KeynumToModEnvDecay = 32,
    DelayVolEnv = 33,
    AttackVolEnv = 34,
    HoldVolEnv = 35,
    DecayVolEnv = 36,
    SustainVolEnv = 37,
    ReleaseVolEnv = 38,
    KeynumToVolEnvHold = 39,
    KeynumToVolEnvDecay = 40,
    Instrument = 41,
    KeyRange = 43,
    VelRange = 44,
    StartloopAddrsCoarseOffset = 45,
    Keynum = 46,
    Velocity = 47,
    InitialAttenuation = 48,
    EndloopAddrsCoarseOffset = 50,
    CoarseTune = 51,
    FineTune = 52,
    SampleID = 53,
    SampleModes = 54,
    ScaleTuning = 56,
    ExclusiveClass = 57,
    OverridingRootKey = 58,
    EndOper = 60,
    Pitch, // non-standard generator, used as a destination of default pitch bend modulator
    Last
};

enum class GeneralController : std::uint8_t {
    NoController = 0,
    NoteOnVelocity = 2,
    NoteOnKeyNumber = 3,
    PolyPressure = 10,
    ChannelPressure = 13,
    PitchWheel = 14,
    PitchWheelSensitivity = 16,
    Link = 127
};

enum class ControllerPalette { General = 0, MIDI = 1 };

enum class SourceDirection { Positive = 0, Negative = 1 };

enum class SourcePolarity { Unipolar = 0, Bipolar = 1 };

enum class SourceType { Linear = 0, Concave = 1, Convex = 2, Switch = 3 };

struct Modulator {
    union {
        GeneralController general;
        std::uint8_t midi;
    } index;
    ControllerPalette palette;
    SourceDirection direction;
    SourcePolarity polarity;
    SourceType type;
};

enum class Transform : std::uint16_t { Linear = 0, AbsoluteValue = 2 };

struct RangesType {
    std::int8_t lo;
    std::int8_t hi;
};

union GenAmountType {
    RangesType ranges;
    std::int16_t shAmount;
    std::uint16_t wAmount;
};

struct VersionTag {
    std::uint16_t major;
    std::uint16_t minor;
};

struct PresetHeader {
    char presetName[20];
    std::uint16_t preset;
    std::uint16_t bank;
    std::uint16_t presetBagNdx;
    std::uint32_t library;
    std::uint32_t genre;
    std::uint32_t morphology;
};

struct Bag {
    std::uint16_t genNdx;
    std::uint16_t modNdx;
};

struct ModList {
    Modulator modSrcOper;
    Generator modDestOper;
    std::int16_t modAmount;
    Modulator modAmtSrcOper;
    Transform modTransOper;
};

struct GenList {
    Generator genOper;
    GenAmountType genAmount;
};

struct Inst {
    char instName[20];
    std::uint16_t instBagNdx;
};

struct Sample {
    char sampleName[20];
    std::uint32_t start;
    std::uint32_t end;
    std::uint32_t startloop;
    std::uint32_t endloop;
    std::uint32_t sampleRate;
    std::int8_t originalKey;
    std::int8_t correction;
    std::uint16_t sampleLink;
    SampleLink sampleType;
};
}
}
