//----------------------------------------------------------------------------
//  EDGE FluidLite Music Player
//----------------------------------------------------------------------------
//
//  Copyright (c) 2022-2024 The EDGE Team.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//----------------------------------------------------------------------------

#include "s_fluid.h"

#include "HandmadeMath.h"
#include "dm_state.h"
#include "file.h"
#include "filesystem.h"
#include "fluidlite.h"
#include "i_system.h"
#include "m_misc.h"
// clang-format off
#define MidiFraction FluidFraction
#define MidiSequencer FluidSequencer
typedef struct MidiRealTimeInterface FluidInterface;
#include "midi_sequencer_impl.hpp"
// clang-format on
#include "s_blit.h"
#include "s_music.h"
#include "str_compare.h"
#include "str_util.h"

#define FLUID_NUM_SAMPLES 4096

extern bool sound_device_stereo;
extern int  sound_device_frequency;

bool fluid_disabled = false;

fluid_synth_t    *edge_fluid            = nullptr;
fluid_settings_t *edge_fluid_settings   = nullptr;
fluid_sfloader_t *edge_fluid_sf2_loader = nullptr;

EDGE_DEFINE_CONSOLE_VARIABLE(midi_soundfont, "", kConsoleVariableFlagArchive)

EDGE_DEFINE_CONSOLE_VARIABLE(fluid_player_gain, "0.3",
                             (ConsoleVariableFlag)(kConsoleVariableFlagArchive | kConsoleVariableFlagFilepath))

extern std::vector<std::string> available_soundfonts;

static void FluidError(int level, char *message, void *data)
{
    (void)level;
    (void)data;
    FatalError("Fluidlite: %s\n", message);
}

static void *edge_fluid_fopen(fluid_fileapi_t *fileapi, const char *filename)
{
    FILE *fp = epi::FileOpenRaw(filename, epi::kFileAccessRead | epi::kFileAccessBinary);
    if (!fp)
        return nullptr;
    return fp;
}

static void ConvertToMono(int16_t *dest, const int16_t *src, int len)
{
    const int16_t *s_end = src + len * 2;

    for (; src < s_end; src += 2)
    {
        // compute average of samples
        *dest++ = ((int)src[0] + (int)src[1]) >> 1;
    }
}

bool StartupFluid(void)
{
    LogPrint("Initializing FluidLite...\n");

    // Check for presence of previous CVAR value's file
    bool cvar_good = false;
    for (size_t i = 0; i < available_soundfonts.size(); i++)
    {
        if (epi::StringCaseCompareASCII(midi_soundfont.s_, available_soundfonts.at(i)) == 0)
        {
            cvar_good = true;
            break;
        }
    }

    if (!cvar_good)
    {
        LogWarning("Cannot find previously used soundfont %s, falling back to "
                   "default!\n",
                   midi_soundfont.c_str());
        midi_soundfont = epi::SanitizePath(epi::PathAppend(game_directory, "soundfont/Default.sf2"));
        if (!epi::FileExists(midi_soundfont.s_))
            FatalError("Fluidlite: Cannot locate default soundfont (Default.sf2)! "
                       "Please check the /soundfont directory "
                       "of your EDGE-Classic install!\n");
    }

    // Initialize settings and change values from default if needed
    fluid_set_log_function(FLUID_PANIC, FluidError, nullptr);
    fluid_set_log_function(FLUID_ERR, nullptr, nullptr);
    fluid_set_log_function(FLUID_WARN, nullptr, nullptr);
    fluid_set_log_function(FLUID_DBG, nullptr, nullptr);
    edge_fluid_settings = new_fluid_settings();
    fluid_settings_setstr(edge_fluid_settings, "synth.reverb.active", "no");
    fluid_settings_setstr(edge_fluid_settings, "synth.chorus.active", "no");
    fluid_settings_setnum(edge_fluid_settings, "synth.gain", fluid_player_gain.f_);
    fluid_settings_setnum(edge_fluid_settings, "synth.sample-rate", sound_device_frequency);
    fluid_settings_setnum(edge_fluid_settings, "synth.polyphony", 64);
    edge_fluid = new_fluid_synth(edge_fluid_settings);

    // Register loader that uses our custom function to provide
    // a FILE pointer
    edge_fluid_sf2_loader          = new_fluid_defsfloader();
    edge_fluid_sf2_loader->fileapi = new fluid_fileapi_t;
    fluid_init_default_fileapi(edge_fluid_sf2_loader->fileapi);
    edge_fluid_sf2_loader->fileapi->fopen = edge_fluid_fopen;
    fluid_synth_add_sfloader(edge_fluid, edge_fluid_sf2_loader);

    if (fluid_synth_sfload(edge_fluid, midi_soundfont.c_str(), 1) == -1)
    {
        LogWarning("FluidLite: Initialization failure.\n");
        delete_fluid_synth(edge_fluid);
        delete_fluid_settings(edge_fluid_settings);
        return false;
    }

    fluid_synth_program_reset(edge_fluid);

    return true; // OK!
}

// Should only be invoked when switching soundfonts
void RestartFluid(void)
{
    if (fluid_disabled)
        return;

    LogPrint("Restarting FluidLite...\n");

    int old_entry = entry_playing;

    StopMusic();

    delete_fluid_synth(edge_fluid);
    delete_fluid_settings(edge_fluid_settings);
    edge_fluid            = nullptr;
    edge_fluid_settings   = nullptr;
    edge_fluid_sf2_loader = nullptr; // This is already deleted upon invoking delete_fluid_synth

    if (!StartupFluid())
    {
        fluid_disabled = true;
        return;
    }

    ChangeMusic(old_entry,
                true); // Restart track that was playing when switched

    return;            // OK!
}

class FluidPlayer : public AbstractMusicPlayer
{
  private:
    enum Status
    {
        kNotLoaded,
        kPlaying,
        kPaused,
        kStopped
    };

    int  status_;
    bool looping_;

    FluidInterface *fluid_interface_;

    int16_t *mono_buffer_;

  public:
    FluidPlayer(uint8_t *data, int _length, bool looping) : status_(kNotLoaded), looping_(looping)
    {
        mono_buffer_ = new int16_t[FLUID_NUM_SAMPLES * 2];
        SequencerInit();
    }

    ~FluidPlayer()
    {
        Close();

        if (mono_buffer_)
            delete[] mono_buffer_;
    }

  public:
    FluidSequencer *fluid_sequencer_;

    static void rtNoteOn(void *userdata, uint8_t channel, uint8_t note, uint8_t velocity)
    {
        fluid_synth_noteon(edge_fluid, channel, note, velocity);
    }

    static void rtNoteOff(void *userdata, uint8_t channel, uint8_t note)
    {
        fluid_synth_noteoff(edge_fluid, channel, note);
    }

    static void rtNoteAfterTouch(void *userdata, uint8_t channel, uint8_t note, uint8_t atVal)
    {
        fluid_synth_key_pressure(edge_fluid, channel, note, atVal);
    }

    static void rtChannelAfterTouch(void *userdata, uint8_t channel, uint8_t atVal)
    {
        fluid_synth_channel_pressure(edge_fluid, channel, atVal);
    }

    static void rtControllerChange(void *userdata, uint8_t channel, uint8_t type, uint8_t value)
    {
        fluid_synth_cc(edge_fluid, channel, type, value);
    }

    static void rtPatchChange(void *userdata, uint8_t channel, uint8_t patch)
    {
        fluid_synth_program_change(edge_fluid, channel, patch);
    }

    static void rtPitchBend(void *userdata, uint8_t channel, uint8_t msb, uint8_t lsb)
    {
        fluid_synth_pitch_bend(edge_fluid, channel, (msb << 7) | lsb);
    }

    static void rtSysEx(void *userdata, const uint8_t *msg, size_t size)
    {
        fluid_synth_sysex(edge_fluid, (const char *)msg, (int)size, nullptr, nullptr, nullptr, 0);
    }

    static void rtDeviceSwitch(void *userdata, size_t track, const char *data, size_t length)
    {
        (void)userdata;
        (void)track;
        (void)data;
        (void)length;
    }

    static size_t rtCurrentDevice(void *userdata, size_t track)
    {
        (void)userdata;
        (void)track;
        return 0;
    }

    static void playSynth(void *userdata, uint8_t *stream, size_t length)
    {
        fluid_synth_write_s16(edge_fluid, (int)length / 4, stream, 0, 2, stream + 2, 0, 2);
    }

    void SequencerInit()
    {
        fluid_sequencer_ = new FluidSequencer;
        fluid_interface_ = new FluidInterface;
        memset(fluid_interface_, 0, sizeof(MidiRealTimeInterface));

        fluid_interface_->rtUserData           = this;
        fluid_interface_->rt_noteOn            = rtNoteOn;
        fluid_interface_->rt_noteOff           = rtNoteOff;
        fluid_interface_->rt_noteAfterTouch    = rtNoteAfterTouch;
        fluid_interface_->rt_channelAfterTouch = rtChannelAfterTouch;
        fluid_interface_->rt_controllerChange  = rtControllerChange;
        fluid_interface_->rt_patchChange       = rtPatchChange;
        fluid_interface_->rt_pitchBend         = rtPitchBend;
        fluid_interface_->rt_systemExclusive   = rtSysEx;

        fluid_interface_->onPcmRender          = playSynth;
        fluid_interface_->onPcmRender_userdata = this;

        fluid_interface_->pcmSampleRate = sound_device_frequency;
        fluid_interface_->pcmFrameSize  = 2 /*channels*/ * 2 /*size of one sample*/;

        fluid_interface_->rt_deviceSwitch  = rtDeviceSwitch;
        fluid_interface_->rt_currentDevice = rtCurrentDevice;

        fluid_sequencer_->SetInterface(fluid_interface_);
    }

    bool LoadTrack(const uint8_t *data, int length)
    {
        return fluid_sequencer_->LoadMidi(data, length);
    }

    void Close(void)
    {
        if (status_ == kNotLoaded)
            return;

        // Stop playback
        if (status_ != kStopped)
            Stop();

        if (fluid_sequencer_)
        {
            delete fluid_sequencer_;
            fluid_sequencer_ = nullptr;
        }
        if (fluid_interface_)
        {
            delete fluid_interface_;
            fluid_interface_ = nullptr;
        }

        status_ = kNotLoaded;
    }

    void Play(bool loop)
    {
        if (!(status_ == kNotLoaded || status_ == kStopped))
            return;

        status_  = kPlaying;
        looping_ = loop;

        // Load up initial buffer data
        Ticker();
    }

    void Stop(void)
    {
        if (!(status_ == kPlaying || status_ == kPaused))
            return;

        fluid_synth_all_voices_stop(edge_fluid);

        SoundQueueStop();

        status_ = kStopped;
    }

    void Pause(void)
    {
        if (status_ != kPlaying)
            return;

        fluid_synth_all_voices_pause(edge_fluid);

        status_ = kPaused;
    }

    void Resume(void)
    {
        if (status_ != kPaused)
            return;

        status_ = kPlaying;
    }

    void Ticker(void)
    {
        if (fluid_player_gain.CheckModified())
        {
            fluid_player_gain.f_ = HMM_Clamp(0.0, fluid_player_gain.f_, 2.0f);
            fluid_player_gain    = fluid_player_gain.f_;
            fluid_synth_set_gain(edge_fluid, fluid_player_gain.f_);
        }

        while (status_ == kPlaying && !pc_speaker_mode)
        {
            SoundData *buf =
                SoundQueueGetFreeBuffer(FLUID_NUM_SAMPLES, sound_device_stereo ? kMixInterleaved : kMixMono);

            if (!buf)
                break;

            if (StreamIntoBuffer(buf))
            {
                SoundQueueAddBuffer(buf, sound_device_frequency);
            }
            else
            {
                // finished playing
                SoundQueueReturnBuffer(buf);

                Stop();
            }
        }
    }

  private:
    bool StreamIntoBuffer(SoundData *buf)
    {
        int16_t *data_buf;

        bool song_done = false;

        if (!sound_device_stereo)
            data_buf = mono_buffer_;
        else
            data_buf = buf->data_left_;

        int played = fluid_sequencer_->PlayStream((uint8_t *)data_buf, FLUID_NUM_SAMPLES);

        if (fluid_sequencer_->PositionAtEnd())
            song_done = true;

        buf->length_ = played / 4;

        if (!sound_device_stereo)
            ConvertToMono(buf->data_left_, mono_buffer_, buf->length_);

        if (song_done) /* EOF */
        {
            if (!looping_)
                return false;
            fluid_sequencer_->Rewind();
            return true;
        }

        return true;
    }
};

AbstractMusicPlayer *PlayFluidMusic(uint8_t *data, int length, bool loop)
{
    if (fluid_disabled)
    {
        delete[] data;
        return nullptr;
    }

    FluidPlayer *player = new FluidPlayer(data, length, loop);

    if (!player)
    {
        LogDebug("FluidLite player: error initializing!\n");
        delete[] data;
        return nullptr;
    }

    if (!player->LoadTrack(data,
                           length)) // Lobo: quietly log it instead of completely exiting EDGE
    {
        LogDebug("FluidLite player: failed to load MIDI file!\n");
        delete[] data;
        delete player;
        return nullptr;
    }

    delete[] data;

    player->Play(loop);

    return player;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
