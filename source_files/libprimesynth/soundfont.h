#pragma once
#include "soundfont_spec.h"
#include <array>
#include <vector>
#include <string>
#include <memory>

namespace primesynth {
static constexpr std::size_t NUM_GENERATORS = static_cast<std::size_t>(sf::Generator::Last);
static constexpr std::uint16_t PERCUSSION_BANK = 128;

struct Sample {
    std::string name;
    std::uint32_t start, end, startLoop, endLoop, sampleRate;
    std::int8_t key, correction;
    double minAtten;
    const std::vector<std::int16_t>& buffer;

    Sample(const sf::Sample& sample, const std::vector<std::int16_t>& sampleBuffer);
};

class GeneratorSet {
public:
    GeneratorSet();

    std::int16_t getOrDefault(sf::Generator type) const;

    void set(sf::Generator type, std::int16_t amount);
    void merge(const GeneratorSet& b);
    void add(const GeneratorSet& b);

private:
    struct Generator {
        bool used;
        std::int16_t amount;
    };
    std::array<Generator, NUM_GENERATORS> generators_;
};

class ModulatorParameterSet {
public:
    static const ModulatorParameterSet& getDefaultParameters();

    const std::vector<sf::ModList>& getParameters() const;

    void append(const sf::ModList& param);
    void addOrAppend(const sf::ModList& param);
    void merge(const ModulatorParameterSet& b);
    void mergeAndAdd(const ModulatorParameterSet& b);

private:
    std::vector<sf::ModList> params_;
};

struct Zone {
    struct Range {
        std::int8_t min = 0, max = 127;

        bool contains(std::int8_t value) const;
    };

    Range keyRange, velocityRange;
    GeneratorSet generators;
    ModulatorParameterSet modulatorParameters;

    bool isInRange(std::int8_t key, std::int8_t velocity) const;
};

struct Instrument {
    std::string name;
    std::vector<Zone> zones;

    Instrument(std::vector<sf::Inst>::const_iterator instIter, const std::vector<sf::Bag>& ibag,
               const std::vector<sf::ModList>& imod, const std::vector<sf::GenList>& igen);
};

class SoundFont;

struct Preset {
    std::string name;
    std::uint16_t bank, presetID;
    std::vector<Zone> zones;
    const SoundFont& soundFont;

    Preset(std::vector<sf::PresetHeader>::const_iterator phdrIter, const std::vector<sf::Bag>& pbag,
           const std::vector<sf::ModList>& pmod, const std::vector<sf::GenList>& pgen, const SoundFont& sfont);
};

class SoundFont {
public:
    explicit SoundFont(const std::string& filename);

    const std::string& getName() const;
    const std::vector<Sample>& getSamples() const;
    const std::vector<Instrument>& getInstruments() const;
    const std::vector<std::shared_ptr<const Preset>>& getPresetPtrs() const;

private:
    std::string name_;
    std::vector<std::int16_t> sampleBuffer_;
    std::vector<Sample> samples_;
    std::vector<Instrument> instruments_;
    std::vector<std::shared_ptr<const Preset>> presets_;

    void readInfoChunk(std::ifstream& ifs, std::size_t size);
    void readSdtaChunk(std::ifstream& ifs, std::size_t size);
    void readPdtaChunk(std::ifstream& ifs, std::size_t size);
};
}
