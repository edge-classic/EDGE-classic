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
#include "epi_file.h"
#include "epi_filesystem.h"
#include "i_system.h"
#include "m_misc.h"
// clang-format off
#define MidiFraction OPLFraction
#define MidiSequencer OPLSequencer
typedef struct MidiRealTimeInterface OPLInterface;
#include "midi_sequencer_impl.hpp"
// clang-format on
#include "ddf_playlist.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"
#include "radmidi.h"
#include "s_blit.h"
#include "s_music.h"
#include "snd_types.h"
#include "w_files.h"
#include "w_wad.h"

OPLPlayer *edge_opl = nullptr;

extern int  sound_device_frequency;

bool opl_disabled = false;

bool StartupOpal(void)
{
    LogPrint("Initializing OPL player...\n");

    if (edge_opl)
        delete edge_opl;

    edge_opl = new OPLPlayer(sound_device_frequency);

    if (!edge_opl)
        return false;

    if (!edge_opl->loadPatches())
    {
        LogWarning("StartupOpal: Error loading instruments!\n");
        return false;
    }

    // OK
    return true;
}

// Should only be invoked when changing MIDI player options
void RestartOpal(void)
{
    if (opl_disabled)
        return;

    int old_entry = entry_playing;

    StopMusic();

    if (!StartupOpal())
    {
        opl_disabled = true;
        return;
    }

    ChangeMusic(old_entry,
                true); // Restart track that was playing when switched

    return;            // OK!
}

static void ConvertToMono(float *dest, const float *src, int len)
{
    const float *s_end = src + len * 2;

    for (; src < s_end; src += 2)
    {
        // compute average of samples
        *dest++ = (src[0] + src[1]) * 0.5f;
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

    OPLInterface *opl_interface_;

  public:
    OpalPlayer(bool looping) : status_(kNotLoaded), looping_(looping)
    {
        SequencerInit();
    }

    ~OpalPlayer()
    {
        Close();
    }

  public:
    OPLSequencer *opl_sequencer_;

    static void rtNoteOn(void *userdata, uint8_t channel, uint8_t note, uint8_t velocity)
    {
        edge_opl->midiNoteOn(channel, note, velocity);
    }

    static void rtNoteOff(void *userdata, uint8_t channel, uint8_t note)
    {
        edge_opl->midiNoteOff(channel, note);
    }

    static void rtNoteAfterTouch(void *userdata, uint8_t channel, uint8_t note, uint8_t atVal)
    {
        (void)userdata;
        (void)channel;
        (void)note;
        (void)atVal;
    }

    static void rtChannelAfterTouch(void *userdata, uint8_t channel, uint8_t atVal)
    {
        (void)userdata;
        (void)channel;
        (void)atVal;
    }

    static void rtControllerChange(void *userdata, uint8_t channel, uint8_t type, uint8_t value)
    {
        edge_opl->midiControlChange(channel, type, value);
    }

    static void rtPatchChange(void *userdata, uint8_t channel, uint8_t patch)
    {
        edge_opl->midiProgramChange(channel, patch);
    }

    static void rtPitchBend(void *userdata, uint8_t channel, uint8_t msb, uint8_t lsb)
    {
        edge_opl->midiPitchControl(channel, (msb - 64) / 127.0);
    }

    static void rtSysEx(void *userdata, const uint8_t *msg, size_t size)
    {
        edge_opl->midiSysEx(msg, size);
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

    static void rtRawOPL(void *userdata, uint8_t reg, uint8_t value)
    {
        edge_opl->midiRawOPL(reg, value);
    }

    static void playSynth(void *userdata, uint8_t *stream, size_t length)
    {
        (void)userdata;
        edge_opl->generate((float *)(stream), length / (2 * sizeof(float)));
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
        opl_interface_->pcmFrameSize  = 2 /*channels*/ * sizeof(float) /*size of one sample*/;

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
        if (status_ == kNotLoaded)
            return;

        // Stop playback
        if (status_ != kStopped)
            Stop();

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

        music_player_gain = 1.0f;

        status_ = kNotLoaded;
    }

    void Play(bool loop)
    {
        if (!(status_ == kNotLoaded || status_ == kStopped))
            return;

        status_  = kPlaying;
        looping_ = loop;

        music_player_gain = 4.0f;

        // Load up initial buffer data
        Ticker();
    }

    void Stop(void)
    {
        if (!(status_ == kPlaying || status_ == kPaused))
            return;

        edge_opl->reset();

        SoundQueueStop();

        status_ = kStopped;
    }

    void Pause(void)
    {
        if (status_ != kPlaying)
            return;

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
        while (status_ == kPlaying && !pc_speaker_mode)
        {
            SoundData *buf = SoundQueueGetFreeBuffer(kMusicBuffer);

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
        bool song_done = false;

        int played = opl_sequencer_->PlayStream((uint8_t *)(buf->data_), kMusicBuffer);

        if (opl_sequencer_->PositionAtEnd())
            song_done = true;

        buf->length_ = played / 2 / sizeof(float);

        if (song_done) /* EOF */
        {
            if (!looping_)
                return false;
            opl_sequencer_->Rewind();
            return true;
        }

        return true;
    }
};

AbstractMusicPlayer *PlayOPLMusic(uint8_t *data, int length, bool loop, int type)
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

    if (!player->LoadTrack(data, length,
                           rate)) // Lobo: quietly log it instead of completely exiting EDGE
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
