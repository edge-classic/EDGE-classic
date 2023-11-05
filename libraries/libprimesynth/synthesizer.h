#pragma once
#include "channel.h"

namespace primesynth {
class Synthesizer {
public:
    Synthesizer(double outputRate = 44100, std::size_t numChannels = 16);

    void render_float(float *buffer, std::size_t samples);
    void render_s16(std::int16_t *buffer, std::size_t samples);

    void loadSoundFont(const std::string& filename);
    void setVolume(double volume);
    void setMIDIStandard(midi::Standard midiStandard, bool fixed = false);
    void processSysEx(const char* data, std::size_t length);
    void processChannelMessage(midi::MessageStatus event, std::uint8_t chan, std::uint8_t param1 = 0, std::uint8_t param2 = 0);
    void pause(void);
    void stop(void);

private:
    midi::Standard midiStd_, defaultMIDIStd_;
    bool stdFixed_;
    std::vector<std::unique_ptr<Channel>> channels_;
    std::vector<std::unique_ptr<SoundFont>> soundFonts_;
    double volume_;

    std::shared_ptr<const Preset> findPreset(std::uint16_t bank, std::uint16_t presetID) const;
};
}
