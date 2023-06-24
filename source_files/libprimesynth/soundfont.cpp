#include "conversion.h"
#include "soundfont.h"
#include <fstream>

namespace primesynth {
std::string achToString(const char ach[20]) {
    return {ach, strnlen(ach, 20)};
}

Sample::Sample(const sf::Sample& sample, const std::vector<std::int16_t>& sampleBuffer)
    : name(achToString(sample.sampleName)),
      start(sample.start),
      end(sample.end),
      startLoop(sample.startloop),
      endLoop(sample.endloop),
      sampleRate(sample.sampleRate),
      key(sample.originalKey),
      correction(sample.correction),
      buffer(sampleBuffer) {
    if (start < end) {
        int sampleMax = 0;
        // if SoundFont file is comformant to specification, generators do not extend sample range beyond start and end
        for (std::size_t i = start; i < end; ++i) {
            sampleMax = std::max(sampleMax, std::abs(sampleBuffer.at(i)));
        }
        minAtten = conv::amplitudeToAttenuation(static_cast<double>(sampleMax) / INT16_MAX);
    } else {
        minAtten = INFINITY;
    }
}

static const std::array<std::int16_t, NUM_GENERATORS> DEFAULT_GENERATOR_VALUES = {
    0,      // startAddrsOffset
    0,      // endAddrsOffset
    0,      // startloopAddrsOffset
    0,      // endloopAddrsOffset
    0,      // startAddrsCoarseOffset
    0,      // modLfoToPitch
    0,      // vibLfoToPitch
    0,      // modEnvToPitch
    13500,  // initialFilterFc
    0,      // initialFilterQ
    0,      // modLfoToFilterFc
    0,      // modEnvToFilterFc
    0,      // endAddrsCoarseOffset
    0,      // modLfoToVolume
    0,      // unused1
    0,      // chorusEffectsSend
    0,      // reverbEffectsSend
    0,      // pan
    0,      // unused2
    0,      // unused3
    0,      // unused4
    -12000, // delayModLFO
    0,      // freqModLFO
    -12000, // delayVibLFO
    0,      // freqVibLFO
    -12000, // delayModEnv
    -12000, // attackModEnv
    -12000, // holdModEnv
    -12000, // decayModEnv
    0,      // sustainModEnv
    -12000, // releaseModEnv
    0,      // keynumToModEnvHold
    0,      // keynumToModEnvDecay
    -12000, // delayVolEnv
    -12000, // attackVolEnv
    -12000, // holdVolEnv
    -12000, // decayVolEnv
    0,      // sustainVolEnv
    -12000, // releaseVolEnv
    0,      // keynumToVolEnvHold
    0,      // keynumToVolEnvDecay
    0,      // instrument
    0,      // reserved1
    0,      // keyRange
    0,      // velRange
    0,      // startloopAddrsCoarseOffset
    -1,     // keynum
    -1,     // velocity
    0,      // initialAttenuation
    0,      // reserved2
    0,      // endloopAddrsCoarseOffset
    0,      // coarseTune
    0,      // fineTune
    0,      // sampleID
    0,      // sampleModes
    0,      // reserved3
    100,    // scaleTuning
    0,      // exclusiveClass
    -1,     // overridingRootKey
    0,      // unused5
    0,      // endOper
    0       // pitch
};

GeneratorSet::GeneratorSet() {
    for (std::size_t i = 0; i < NUM_GENERATORS; ++i) {
        generators_.at(i) = {false, DEFAULT_GENERATOR_VALUES.at(i)};
    }
}

std::int16_t GeneratorSet::getOrDefault(sf::Generator type) const {
    return generators_.at(static_cast<std::size_t>(type)).amount;
}

void GeneratorSet::set(sf::Generator type, std::int16_t amount) {
    generators_.at(static_cast<std::size_t>(type)) = {true, amount};
}

void GeneratorSet::merge(const GeneratorSet& b) {
    for (std::size_t i = 0; i < NUM_GENERATORS; ++i) {
        if (!generators_.at(i).used && b.generators_.at(i).used) {
            generators_.at(i) = b.generators_.at(i);
        }
    }
}

void GeneratorSet::add(const GeneratorSet& b) {
    for (std::size_t i = 0; i < NUM_GENERATORS; ++i) {
        if (b.generators_.at(i).used) {
            generators_.at(i).amount += b.generators_.at(i).amount;
            generators_.at(i).used = true;
        }
    }
}

const ModulatorParameterSet& ModulatorParameterSet::getDefaultParameters() {
    static ModulatorParameterSet params;
    static bool initialized = false;
    if (!initialized) {
        initialized = true;

        // See "SoundFont Technical Specification" Version 2.04
        // p.41 "8.4 Default Modulators"
        {
            // 8.4.1 MIDI Note-On Velocity to Initial Attenuation
            sf::ModList param;
            param.modSrcOper.index.general = sf::GeneralController::NoteOnVelocity;
            param.modSrcOper.palette = sf::ControllerPalette::General;
            param.modSrcOper.direction = sf::SourceDirection::Negative;
            param.modSrcOper.polarity = sf::SourcePolarity::Unipolar;
            param.modSrcOper.type = sf::SourceType::Concave;
            param.modDestOper = sf::Generator::InitialAttenuation;
            param.modAmount = 960;
            param.modAmtSrcOper.index.general = sf::GeneralController::NoController;
            param.modAmtSrcOper.palette = sf::ControllerPalette::General;
            param.modTransOper = sf::Transform::Linear;
            params.append(param);
        }
        {
            // 8.4.2 MIDI Note-On Velocity to Filter Cutoff
            sf::ModList param;
            param.modSrcOper.index.general = sf::GeneralController::NoteOnVelocity;
            param.modSrcOper.palette = sf::ControllerPalette::General;
            param.modSrcOper.direction = sf::SourceDirection::Negative;
            param.modSrcOper.polarity = sf::SourcePolarity::Unipolar;
            param.modSrcOper.type = sf::SourceType::Linear;
            param.modDestOper = sf::Generator::InitialFilterFc;
            param.modAmount = -2400;
            param.modAmtSrcOper.index.general = sf::GeneralController::NoController;
            param.modAmtSrcOper.palette = sf::ControllerPalette::General;
            param.modTransOper = sf::Transform::Linear;
            params.append(param);
        }
        {
            // 8.4.3 MIDI Channel Pressure to Vibrato LFO Pitch Depth
            sf::ModList param;
            param.modSrcOper.index.midi = 13;
            param.modSrcOper.palette = sf::ControllerPalette::MIDI;
            param.modSrcOper.direction = sf::SourceDirection::Positive;
            param.modSrcOper.polarity = sf::SourcePolarity::Unipolar;
            param.modSrcOper.type = sf::SourceType::Linear;
            param.modDestOper = sf::Generator::VibLfoToPitch;
            param.modAmount = 50;
            param.modAmtSrcOper.index.general = sf::GeneralController::NoController;
            param.modAmtSrcOper.palette = sf::ControllerPalette::General;
            param.modTransOper = sf::Transform::Linear;
            params.append(param);
        }
        {
            // 8.4.4 MIDI Continuous Controller 1 to Vibrato LFO Pitch Depth
            sf::ModList param;
            param.modSrcOper.index.midi = 1;
            param.modSrcOper.palette = sf::ControllerPalette::MIDI;
            param.modSrcOper.direction = sf::SourceDirection::Positive;
            param.modSrcOper.polarity = sf::SourcePolarity::Unipolar;
            param.modSrcOper.type = sf::SourceType::Linear;
            param.modDestOper = sf::Generator::VibLfoToPitch;
            param.modAmount = 50;
            param.modAmtSrcOper.index.general = sf::GeneralController::NoController;
            param.modAmtSrcOper.palette = sf::ControllerPalette::General;
            param.modTransOper = sf::Transform::Linear;
            params.append(param);
        }
        {
            // 8.4.5 MIDI Continuous Controller 7 to Initial Attenuation Source
            sf::ModList param;
            param.modSrcOper.index.midi = 7;
            param.modSrcOper.palette = sf::ControllerPalette::MIDI;
            param.modSrcOper.direction = sf::SourceDirection::Negative;
            param.modSrcOper.polarity = sf::SourcePolarity::Unipolar;
            param.modSrcOper.type = sf::SourceType::Concave;
            param.modDestOper = sf::Generator::InitialAttenuation;
            param.modAmount = 960;
            param.modAmtSrcOper.index.general = sf::GeneralController::NoController;
            param.modAmtSrcOper.palette = sf::ControllerPalette::General;
            param.modTransOper = sf::Transform::Linear;
            params.append(param);
        }
        {
            // 8.4.6 MIDI Continuous Controller 10 to Pan Position
            sf::ModList param;
            param.modSrcOper.index.midi = 10;
            param.modSrcOper.palette = sf::ControllerPalette::MIDI;
            param.modSrcOper.direction = sf::SourceDirection::Positive;
            param.modSrcOper.polarity = sf::SourcePolarity::Bipolar;
            param.modSrcOper.type = sf::SourceType::Linear;
            param.modDestOper = sf::Generator::Pan;
            param.modAmount = 500;
            param.modAmtSrcOper.index.general = sf::GeneralController::NoController;
            param.modAmtSrcOper.palette = sf::ControllerPalette::General;
            param.modTransOper = sf::Transform::Linear;
            params.append(param);
        }
        {
            // 8.4.7 MIDI Continuous Controller 11 to Initial Attenuation
            sf::ModList param;
            param.modSrcOper.index.midi = 11;
            param.modSrcOper.palette = sf::ControllerPalette::MIDI;
            param.modSrcOper.direction = sf::SourceDirection::Negative;
            param.modSrcOper.polarity = sf::SourcePolarity::Unipolar;
            param.modSrcOper.type = sf::SourceType::Concave;
            param.modDestOper = sf::Generator::InitialAttenuation;
            param.modAmount = 960;
            param.modAmtSrcOper.index.general = sf::GeneralController::NoController;
            param.modAmtSrcOper.palette = sf::ControllerPalette::General;
            param.modTransOper = sf::Transform::Linear;
            params.append(param);
        }
        {
            // 8.4.8 MIDI Continuous Controller 91 to Reverb Effects Send
            sf::ModList param;
            param.modSrcOper.index.midi = 91;
            param.modSrcOper.palette = sf::ControllerPalette::MIDI;
            param.modSrcOper.direction = sf::SourceDirection::Positive;
            param.modSrcOper.polarity = sf::SourcePolarity::Unipolar;
            param.modSrcOper.type = sf::SourceType::Linear;
            param.modDestOper = sf::Generator::ReverbEffectsSend;
            param.modAmount = 200;
            param.modAmtSrcOper.index.general = sf::GeneralController::NoController;
            param.modAmtSrcOper.palette = sf::ControllerPalette::General;
            param.modTransOper = sf::Transform::Linear;
            params.append(param);
        }
        {
            // 8.4.9 MIDI Continuous Controller 93 to Chorus Effects Send
            sf::ModList param;
            param.modSrcOper.index.midi = 93;
            param.modSrcOper.palette = sf::ControllerPalette::MIDI;
            param.modSrcOper.direction = sf::SourceDirection::Positive;
            param.modSrcOper.polarity = sf::SourcePolarity::Unipolar;
            param.modSrcOper.type = sf::SourceType::Linear;
            param.modDestOper = sf::Generator::ChorusEffectsSend;
            param.modAmount = 200;
            param.modAmtSrcOper.index.general = sf::GeneralController::NoController;
            param.modAmtSrcOper.palette = sf::ControllerPalette::General;
            param.modTransOper = sf::Transform::Linear;
            params.append(param);
        }
        {
            // 8.4.10 MIDI Pitch Wheel to Initial Pitch Controlled by MIDI Pitch Wheel Sensitivity
            sf::ModList param;
            param.modSrcOper.index.general = sf::GeneralController::PitchWheel;
            param.modSrcOper.palette = sf::ControllerPalette::General;
            param.modSrcOper.direction = sf::SourceDirection::Positive;
            param.modSrcOper.polarity = sf::SourcePolarity::Bipolar;
            param.modSrcOper.type = sf::SourceType::Linear;
            param.modDestOper = sf::Generator::Pitch;
            param.modAmount = 12700;
            param.modAmtSrcOper.index.general = sf::GeneralController::PitchWheelSensitivity;
            param.modAmtSrcOper.palette = sf::ControllerPalette::General;
            param.modAmtSrcOper.direction = sf::SourceDirection::Positive;
            param.modAmtSrcOper.polarity = sf::SourcePolarity::Unipolar;
            param.modAmtSrcOper.type = sf::SourceType::Linear;
            param.modTransOper = sf::Transform::Linear;
            params.append(param);
        }
    }
    return params;
}

const std::vector<sf::ModList>& ModulatorParameterSet::getParameters() const {
    return params_;
}

bool operator==(const sf::Modulator& a, const sf::Modulator& b) {
    return a.index.midi == b.index.midi && a.palette == b.palette && a.direction == b.direction &&
           a.polarity == b.polarity && a.type == b.type;
}

bool modulatorsAreIdentical(const sf::ModList& a, const sf::ModList& b) {
    return a.modSrcOper == b.modSrcOper && a.modDestOper == b.modDestOper && a.modAmtSrcOper == b.modAmtSrcOper &&
           a.modTransOper == b.modTransOper;
}

void ModulatorParameterSet::append(const sf::ModList& param) {
    for (const auto& p : params_) {
        if (modulatorsAreIdentical(p, param)) {
            return;
        }
    }
    params_.push_back(param);
}

void ModulatorParameterSet::addOrAppend(const sf::ModList& param) {
    for (auto& p : params_) {
        if (modulatorsAreIdentical(p, param)) {
            p.modAmount += param.modAmount;
            return;
        }
    }
    params_.push_back(param);
}

void ModulatorParameterSet::merge(const ModulatorParameterSet& b) {
    for (const auto& param : b.params_) {
        append(param);
    }
}

void ModulatorParameterSet::mergeAndAdd(const ModulatorParameterSet& b) {
    for (const auto& param : b.params_) {
        addOrAppend(param);
    }
}

bool Zone::Range::contains(std::int8_t value) const {
    return min <= value && value <= max;
}

bool Zone::isInRange(std::int8_t key, std::int8_t velocity) const {
    return keyRange.contains(key) && velocityRange.contains(velocity);
}

void readBags(std::vector<Zone>& zones, std::vector<sf::Bag>::const_iterator bagBegin,
              std::vector<sf::Bag>::const_iterator bagEnd, const std::vector<sf::ModList>& mods,
              const std::vector<sf::GenList>& gens, sf::Generator indexGen) {
    if (bagBegin > bagEnd) {
        throw std::runtime_error("bag indices not monotonically increasing");
    }

    Zone globalZone;

    for (auto it_bag = bagBegin; it_bag != bagEnd; ++it_bag) {
        Zone zone;

        const auto& beginMod = mods.begin() + it_bag->modNdx;
        const auto& endMod = mods.begin() + std::next(it_bag)->modNdx;
        if (beginMod > endMod) {
            throw std::runtime_error("modulator indices not monotonically increasing");
        }
        for (auto it_mod = beginMod; it_mod != endMod; ++it_mod) {
            zone.modulatorParameters.append(*it_mod);
        }

        const auto& beginGen = gens.begin() + it_bag->genNdx;
        const auto& endGen = gens.begin() + std::next(it_bag)->genNdx;
        if (beginGen > endGen) {
            throw std::runtime_error("generator indices not monotonically increasing");
        }
        for (auto it_gen = beginGen; it_gen != endGen; ++it_gen) {
            const auto& range = it_gen->genAmount.ranges;
            switch (it_gen->genOper) {
            case sf::Generator::KeyRange:
                zone.keyRange = {range.lo, range.hi};
                break;
            case sf::Generator::VelRange:
                zone.velocityRange = {range.lo, range.hi};
                break;
            default:
                if (it_gen->genOper < sf::Generator::EndOper) {
                    zone.generators.set(it_gen->genOper, it_gen->genAmount.shAmount);
                }
                break;
            }
        }

        if (beginGen != endGen && std::prev(endGen)->genOper == indexGen) {
            zones.push_back(zone);
        } else if (it_bag == bagBegin && (beginGen != endGen || beginMod != endMod)) {
            globalZone = zone;
        }
    }

    for (auto& zone : zones) {
        zone.generators.merge(globalZone.generators);
        zone.modulatorParameters.merge(globalZone.modulatorParameters);
    }
}

Instrument::Instrument(std::vector<sf::Inst>::const_iterator instIter, const std::vector<sf::Bag>& ibag,
                       const std::vector<sf::ModList>& imod, const std::vector<sf::GenList>& igen)
    : name(achToString(instIter->instName)) {
    readBags(zones, ibag.begin() + instIter->instBagNdx, ibag.begin() + std::next(instIter)->instBagNdx, imod, igen,
             sf::Generator::SampleID);
}

Preset::Preset(std::vector<sf::PresetHeader>::const_iterator phdrIter, const std::vector<sf::Bag>& pbag,
               const std::vector<sf::ModList>& pmod, const std::vector<sf::GenList>& pgen, const SoundFont& sfont)
    : name(achToString(phdrIter->presetName)), bank(phdrIter->bank), presetID(phdrIter->preset), soundFont(sfont) {
    readBags(zones, pbag.begin() + phdrIter->presetBagNdx, pbag.begin() + std::next(phdrIter)->presetBagNdx, pmod, pgen,
             sf::Generator::Instrument);
}

struct RIFFHeader {
    std::uint32_t id;
    std::uint32_t size;
};

RIFFHeader readHeader(std::ifstream& ifs) {
    RIFFHeader header;
    ifs.read(reinterpret_cast<char*>(&header), sizeof(header));
    return header;
}

std::uint32_t readFourCC(std::ifstream& ifs) {
    std::uint32_t id;
    ifs.read(reinterpret_cast<char*>(&id), sizeof(id));
    return id;
}

constexpr std::uint32_t toFourCC(const char str[5]) {
    std::uint32_t fourCC = 0;
    for (int i = 0; i < 4; ++i) {
        fourCC |= str[i] << CHAR_BIT * i;
    }
    return fourCC;
}

SoundFont::SoundFont(const std::string& filename) {
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs) {
        throw std::runtime_error("failed to open file");
    }

    const RIFFHeader riffHeader = readHeader(ifs);
    const std::uint32_t riffType = readFourCC(ifs);
    if (riffHeader.id != toFourCC("RIFF") || riffType != toFourCC("sfbk")) {
        throw std::runtime_error("not a SoundFont file");
    }

    for (std::size_t s = 0; s < riffHeader.size - sizeof(riffType);) {
        const RIFFHeader chunkHeader = readHeader(ifs);
        s += sizeof(chunkHeader) + chunkHeader.size;
        switch (chunkHeader.id) {
        case toFourCC("LIST"): {
            const std::uint32_t chunkType = readFourCC(ifs);
            const std::size_t chunkSize = chunkHeader.size - sizeof(chunkType);
            switch (chunkType) {
            case toFourCC("INFO"):
                readInfoChunk(ifs, chunkSize);
                break;
            case toFourCC("sdta"):
                readSdtaChunk(ifs, chunkSize);
                break;
            case toFourCC("pdta"):
                readPdtaChunk(ifs, chunkSize);
                break;
            default:
                ifs.ignore(chunkSize);
                break;
            }
            break;
        }
        default:
            ifs.ignore(chunkHeader.size);
            break;
        }
    }
}

const std::string& SoundFont::getName() const {
    return name_;
}

const std::vector<Sample>& SoundFont::getSamples() const {
    return samples_;
}

const std::vector<Instrument>& SoundFont::getInstruments() const {
    return instruments_;
}

const std::vector<std::shared_ptr<const Preset>>& SoundFont::getPresetPtrs() const {
    return presets_;
}

void SoundFont::readInfoChunk(std::ifstream& ifs, std::size_t size) {
    for (std::size_t s = 0; s < size;) {
        const RIFFHeader subchunkHeader = readHeader(ifs);
        s += sizeof(subchunkHeader) + subchunkHeader.size;
        switch (subchunkHeader.id) {
        case toFourCC("ifil"): {
            sf::VersionTag ver;
            ifs.read(reinterpret_cast<char*>(&ver), subchunkHeader.size);
            if (ver.major > 2 || ver.minor > 4) {
                throw std::runtime_error("SoundFont later than 2.04 not supported");
            }
            break;
        }
        case toFourCC("INAM"): {
            std::vector<char> buf(subchunkHeader.size);
            ifs.read(buf.data(), buf.size());
            name_ = buf.data();
            break;
        }
        default:
            ifs.ignore(subchunkHeader.size);
            break;
        }
    }
}

void SoundFont::readSdtaChunk(std::ifstream& ifs, std::size_t size) {
    for (std::size_t s = 0; s < size;) {
        const RIFFHeader subchunkHeader = readHeader(ifs);
        s += sizeof(subchunkHeader) + subchunkHeader.size;
        switch (subchunkHeader.id) {
        case toFourCC("smpl"):
            if (subchunkHeader.size == 0) {
                throw std::runtime_error("no sample data found");
            }
            sampleBuffer_.resize(subchunkHeader.size / sizeof(std::int16_t));
            ifs.read(reinterpret_cast<char*>(sampleBuffer_.data()), subchunkHeader.size);
            break;
        default:
            ifs.ignore(subchunkHeader.size);
            break;
        }
    }
}

template <typename T>
void readPdtaList(std::ifstream& ifs, std::vector<T>& list, std::uint32_t totalSize, std::size_t structSize) {
    if (totalSize % structSize != 0) {
        throw std::runtime_error("invalid chunk size");
    }
    list.resize(totalSize / structSize);
    for (std::size_t i = 0; i < totalSize / structSize; ++i) {
        ifs.read(reinterpret_cast<char*>(&list.at(i)), structSize);
    }
}

void readModulator(std::ifstream& ifs, sf::Modulator& mod) {
    std::uint16_t data;
    ifs.read(reinterpret_cast<char*>(&data), 2);

    mod.index.midi = data & 127;
    mod.palette = static_cast<sf::ControllerPalette>((data >> 7) & 1);
    mod.direction = static_cast<sf::SourceDirection>((data >> 8) & 1);
    mod.polarity = static_cast<sf::SourcePolarity>((data >> 9) & 1);
    mod.type = static_cast<sf::SourceType>((data >> 10) & 63);
}

void readModList(std::ifstream& ifs, std::vector<sf::ModList>& list, std::uint32_t totalSize) {
    static const size_t STRUCT_SIZE = 10;
    if (totalSize % STRUCT_SIZE != 0) {
        throw std::runtime_error("invalid chunk size");
    }
    list.reserve(totalSize / STRUCT_SIZE);
    for (std::size_t i = 0; i < totalSize / STRUCT_SIZE; ++i) {
        sf::ModList mod;
        readModulator(ifs, mod.modSrcOper);
        ifs.read(reinterpret_cast<char*>(&mod.modDestOper), 2);
        ifs.read(reinterpret_cast<char*>(&mod.modAmount), 2);
        readModulator(ifs, mod.modAmtSrcOper);
        ifs.read(reinterpret_cast<char*>(&mod.modTransOper), 2);
        list.push_back(mod);
    }
}

void SoundFont::readPdtaChunk(std::ifstream& ifs, std::size_t size) {
    std::vector<sf::PresetHeader> phdr;
    std::vector<sf::Inst> inst;
    std::vector<sf::Bag> pbag, ibag;
    std::vector<sf::ModList> pmod, imod;
    std::vector<sf::GenList> pgen, igen;
    std::vector<sf::Sample> shdr;

    for (std::size_t s = 0; s < size;) {
        const RIFFHeader subchunkHeader = readHeader(ifs);
        s += sizeof(subchunkHeader) + subchunkHeader.size;
        switch (subchunkHeader.id) {
        case toFourCC("phdr"):
            readPdtaList(ifs, phdr, subchunkHeader.size, 38);
            break;
        case toFourCC("pbag"):
            readPdtaList(ifs, pbag, subchunkHeader.size, 4);
            break;
        case toFourCC("pmod"):
            readModList(ifs, pmod, subchunkHeader.size);
            break;
        case toFourCC("pgen"):
            readPdtaList(ifs, pgen, subchunkHeader.size, 4);
            break;
        case toFourCC("inst"):
            readPdtaList(ifs, inst, subchunkHeader.size, 22);
            break;
        case toFourCC("ibag"):
            readPdtaList(ifs, ibag, subchunkHeader.size, 4);
            break;
        case toFourCC("imod"):
            readModList(ifs, imod, subchunkHeader.size);
            break;
        case toFourCC("igen"):
            readPdtaList(ifs, igen, subchunkHeader.size, 4);
            break;
        case toFourCC("shdr"):
            readPdtaList(ifs, shdr, subchunkHeader.size, 46);
            break;
        default:
            ifs.ignore(subchunkHeader.size);
            break;
        }
    }

    // last records of inst, phdr, and shdr sub-chunks indicate end of records, and are ignored

    if (inst.size() < 2) {
        throw std::runtime_error("no instrument found");
    }
    instruments_.reserve(inst.size() - 1);
    for (auto it_inst = inst.begin(); it_inst != std::prev(inst.end()); ++it_inst) {
        instruments_.emplace_back(it_inst, ibag, imod, igen);
    }

    if (phdr.size() < 2) {
        throw std::runtime_error("no preset found");
    }
    presets_.reserve(phdr.size() - 1);
    for (auto it_phdr = phdr.begin(); it_phdr != std::prev(phdr.end()); ++it_phdr) {
        presets_.emplace_back(std::make_shared<Preset>(it_phdr, pbag, pmod, pgen, *this));
    }

    if (shdr.size() < 2) {
        throw std::runtime_error("no sample found");
    }
    samples_.reserve(shdr.size() - 1);
    for (auto it_shdr = shdr.begin(); it_shdr != std::prev(shdr.end()); ++it_shdr) {
        samples_.emplace_back(*it_shdr, sampleBuffer_);
    }
}
}
