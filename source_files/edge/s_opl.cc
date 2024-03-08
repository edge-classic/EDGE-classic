//----------------------------------------------------------------------------
//  EDGE Opal Music Player
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

#include "s_opl.h"

#include "dm_state.h"
#include "file.h"
#include "filesystem.h"
#include "i_system.h"
#include "m_misc.h"
// clang-format off
#define MidiSequencer OPLSequencer
typedef struct MidiRealTimeInterface OPLInterface;
#include "midi_sequencer_impl.hpp"
// clang-format on
#include "playlist.h"
#include "radmidi.h"
#include "s_blit.h"
#include "s_music.h"
#include "snd_types.h"
#include "str_compare.h"
#include "str_util.h"
#include "w_files.h"
#include "w_wad.h"

OPLPlayer *edge_opl = nullptr;

#define OPL_SAMPLES 1024

extern bool sound_device_stereo;
extern int  sound_device_frequency;

bool opl_disabled = false;

EDGE_DEFINE_CONSOLE_VARIABLE(
    opl_instrument_bank, "GENMIDI",
    (ConsoleVariableFlag)(kConsoleVariableFlagArchive |
                          kConsoleVariableFlagFilepath))

extern std::vector<std::string> available_opl_banks;

bool StartupOpal(void)
{
    LogPrint("Initializing OPL player...\n");

    if (edge_opl) delete edge_opl;

    edge_opl = new OPLPlayer(sound_device_frequency);

    if (!edge_opl) return false;

    // Check if CVAR value is still good
    bool cvar_good = false;
    if (opl_instrument_bank.s_ == "GENMIDI")
        cvar_good = true;
    else
    {
        for (size_t i = 0; i < available_opl_banks.size(); i++)
        {
            if (epi::StringCaseCompareASCII(opl_instrument_bank.s_,
                                            available_opl_banks.at(i)) == 0)
                cvar_good = true;
        }
    }

    if (!cvar_good)
    {
        LogWarning(
            "Cannot find previously used GENMIDI %s, falling back to "
            "default!\n",
            opl_instrument_bank.c_str());
        opl_instrument_bank = "GENMIDI";
    }

    int        length;
    uint8_t   *data = nullptr;
    epi::File *F    = nullptr;

    if (opl_instrument_bank.s_ == "GENMIDI")
    {
        data = OpenPackOrLumpInMemory("GENMIDI", {".op2"}, &length);
        if (!data)
        {
            LogDebug("no GENMIDI lump !\n");
            return false;
        }
    }
    else
    {
        F = epi::FileOpen(opl_instrument_bank.s_,
                          epi::kFileAccessRead | epi::kFileAccessBinary);
        if (!F)
        {
            LogWarning("StartupOpal: Error opening GENMIDI!\n");
            return false;
        }
        length = F->GetLength();
        data   = F->LoadIntoMemory();
    }

    if (!data)
    {
        LogWarning("StartupOpal: Error loading instruments!\n");
        if (F) delete F;
        return false;
    }

    if (!edge_opl->loadPatches((const uint8_t *)data, (size_t)length))
    {
        LogWarning("StartupOpal: Error loading instruments!\n");
        delete F;
        delete[] data;
        return false;
    }

    delete F;
    delete[] data;

    // OK
    return true;
}

// Should only be invoked when switching GENMIDI lumps
void RestartOpal(void)
{
    if (opl_disabled) return;

    int old_entry = entry_playing;

    StopMusic();

    if (!StartupOpal())
    {
        opl_disabled = true;
        return;
    }

    ChangeMusic(old_entry,
                  true);  // Restart track that was playing when switched

    return;  // OK!
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

class OpalPlayer : public AbstractMusicPlayer
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

    int16_t *mono_buffer_;

    OPLInterface *opl_interface_;

   public:
    OpalPlayer(bool looping) : status_(kNotLoaded), looping_(looping)
    {
        mono_buffer_ = new int16_t[2 * OPL_SAMPLES];
        SequencerInit();
    }

    ~OpalPlayer()
    {
        Close();

        if (mono_buffer_) delete[] mono_buffer_;
    }

   public:
    OPLSequencer *opl_sequencer_;

    static void rtNoteOn(void *userdata, uint8_t channel, uint8_t note,
                         uint8_t velocity)
    {
        edge_opl->midiNoteOn(channel, note, velocity);
    }

    static void rtNoteOff(void *userdata, uint8_t channel, uint8_t note)
    {
        edge_opl->midiNoteOff(channel, note);
    }

    static void rtNoteAfterTouch(void *userdata, uint8_t channel, uint8_t note,
                                 uint8_t atVal)
    {
        (void)userdata;
        (void)channel;
        (void)note;
        (void)atVal;
    }

    static void rtChannelAfterTouch(void *userdata, uint8_t channel,
                                    uint8_t atVal)
    {
        (void)userdata;
        (void)channel;
        (void)atVal;
    }

    static void rtControllerChange(void *userdata, uint8_t channel,
                                   uint8_t type, uint8_t value)
    {
        edge_opl->midiControlChange(channel, type, value);
    }

    static void rtPatchChange(void *userdata, uint8_t channel, uint8_t patch)
    {
        edge_opl->midiProgramChange(channel, patch);
    }

    static void rtPitchBend(void *userdata, uint8_t channel, uint8_t msb,
                            uint8_t lsb)
    {
        edge_opl->midiPitchControl(channel, (msb - 64) / 127.0);
    }

    static void rtSysEx(void *userdata, const uint8_t *msg, size_t size)
    {
        edge_opl->midiSysEx(msg, size);
    }

    static void rtDeviceSwitch(void *userdata, size_t track, const char *data,
                               size_t length)
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

    static void rtRawOPL(void *userdata, uint8_t reg, uint8_t value)
    {
        edge_opl->midiRawOPL(reg, value);
    }

    static void playSynth(void *userdata, uint8_t *stream, size_t length)
    {
        (void)userdata;
        edge_opl->generate((int16_t *)(stream), length / (2 * sizeof(int16_t)));
    }

    void SequencerInit()
    {
        opl_sequencer_ = new OPLSequencer;
        opl_interface_ = new OPLInterface;
        memset(opl_interface_, 0, sizeof(MidiRealTimeInterface));

        opl_interface_->rtUserData           = this;
        opl_interface_->rt_noteOn            = rtNoteOn;
        opl_interface_->rt_noteOff           = rtNoteOff;
        opl_interface_->rt_noteAfterTouch    = rtNoteAfterTouch;
        opl_interface_->rt_channelAfterTouch = rtChannelAfterTouch;
        opl_interface_->rt_controllerChange  = rtControllerChange;
        opl_interface_->rt_patchChange       = rtPatchChange;
        opl_interface_->rt_pitchBend         = rtPitchBend;
        opl_interface_->rt_systemExclusive   = rtSysEx;

        opl_interface_->onPcmRender          = playSynth;
        opl_interface_->onPcmRender_userdata = this;

        opl_interface_->pcmSampleRate = sound_device_frequency;
        opl_interface_->pcmFrameSize =
            2 /*channels*/ *
            2 /*size of one sample*/;  // OPL3 is 2 'channels' regardless of the
                                       // sound_device_stereo setting

        opl_interface_->rt_deviceSwitch  = rtDeviceSwitch;
        opl_interface_->rt_currentDevice = rtCurrentDevice;
        opl_interface_->rt_rawOPL        = rtRawOPL;

        opl_sequencer_->SetInterface(opl_interface_);
    }

    bool LoadTrack(const uint8_t *data, int length, uint16_t rate)
    {
        return opl_sequencer_->LoadMidi(data, length, rate);
    }

    void Close(void)
    {
        if (status_ == kNotLoaded) return;

        // Stop playback
        if (status_ != kStopped) Stop();

        if (opl_sequencer_)
        {
            delete opl_sequencer_;
            opl_sequencer_ = nullptr;
        }
        if (opl_interface_)
        {
            delete opl_interface_;
            opl_interface_ = nullptr;
        }

        status_ = kNotLoaded;
    }

    void Play(bool loop)
    {
        if (!(status_ == kNotLoaded || status_ == kStopped)) return;

        status_  = kPlaying;
        looping_ = loop;

        // Load up initial buffer data
        Ticker();
    }

    void Stop(void)
    {
        if (!(status_ == kPlaying || status_ == kPaused)) return;

        edge_opl->reset();

        SoundQueueStop();

        status_ = kStopped;
    }

    void Pause(void)
    {
        if (status_ != kPlaying) return;

        status_ = kPaused;
    }

    void Resume(void)
    {
        if (status_ != kPaused) return;

        status_ = kPlaying;
    }

    void Ticker(void)
    {
        while (status_ == kPlaying && !pc_speaker_mode)
        {
            SoundData *buf = SoundQueueGetFreeBuffer(
                OPL_SAMPLES, sound_device_stereo ? kMixInterleaved : kMixMono);

            if (!buf) break;

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

        int played =
            opl_sequencer_->PlayStream((uint8_t *)(data_buf), OPL_SAMPLES);

        if (opl_sequencer_->PositionAtEnd()) song_done = true;

        buf->length_ = played / (2 * sizeof(int16_t));

        if (!sound_device_stereo)
            ConvertToMono(buf->data_left_, mono_buffer_, buf->length_);

        if (song_done) /* EOF */
        {
            if (!looping_) return false;
            opl_sequencer_->Rewind();
            return true;
        }

        return true;
    }
};

AbstractMusicPlayer *PlayOplMusic(uint8_t *data, int length, bool loop,
                                  int type)
{
    if (opl_disabled)
    {
        delete[] data;
        return nullptr;
    }

    OpalPlayer *player = new OpalPlayer(loop);

    if (!player)
    {
        LogDebug("OPL player: error initializing!\n");
        delete[] data;
        return nullptr;
    }

    uint16_t rate;

    switch (type)
    {
        case kDDFMusicIMF280:
            rate = 280;
            break;
        case kDDFMusicIMF560:
            rate = 560;
            break;
        case kDDFMusicIMF700:
            rate = 700;
            break;
        default:
            rate = 0;
            break;
    }

    if (!player->LoadTrack(
            data, length,
            rate))  // Lobo: quietly log it instead of completely exiting EDGE
    {
        LogDebug("OPL player: failed to load MIDI file!\n");
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
