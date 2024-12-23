//----------------------------------------------------------------------------
//  EDGE IMF Music Player
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

#include "s_imf.h"

#include "dm_state.h"
#include "epi_file.h"
#include "epi_filesystem.h"
#include "i_movie.h"
#include "i_system.h"
#include "m_misc.h"
#include "ddf_playlist.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"
#include "opal.h"
#include "s_blit.h"
// clang-format off
#define MidiFraction IMFFraction
#define MidiSequencer IMFSequencer
typedef struct MidiRealTimeInterface IMFInterface;
#include "s_midi.h"
// clang-format on
#include "s_music.h"
#include "snd_types.h"
#include "w_files.h"
#include "w_wad.h"

extern int  sound_device_frequency;

static Opal *imf_opl = nullptr;

class IMFPlayer : public AbstractMusicPlayer
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

    IMFInterface *imf_interface_;

  public:
    IMFPlayer(bool looping) : status_(kNotLoaded), looping_(looping)
    {
        SequencerInit();
    }

    ~IMFPlayer()
    {
        Close();
    }

  public:
    IMFSequencer *imf_sequencer__;

    static void rtNoteOn(void *userdata, uint8_t channel, uint8_t note, uint8_t velocity)
    {
        EPI_UNUSED(userdata);
        EPI_UNUSED(channel);
        EPI_UNUSED(note);
        EPI_UNUSED(velocity);
    }

    static void rtNoteOff(void *userdata, uint8_t channel, uint8_t note)
    {
        EPI_UNUSED(userdata);
        EPI_UNUSED(channel);
        EPI_UNUSED(note);
    }

    static void rtNoteAfterTouch(void *userdata, uint8_t channel, uint8_t note, uint8_t atVal)
    {
        EPI_UNUSED(userdata);
        EPI_UNUSED(channel);
        EPI_UNUSED(note);
        EPI_UNUSED(atVal);
    }

    static void rtChannelAfterTouch(void *userdata, uint8_t channel, uint8_t atVal)
    {
        EPI_UNUSED(userdata);
        EPI_UNUSED(channel);
        EPI_UNUSED(atVal);
    }

    static void rtControllerChange(void *userdata, uint8_t channel, uint8_t type, uint8_t value)
    {
        EPI_UNUSED(userdata);
        EPI_UNUSED(channel);
        EPI_UNUSED(type);
        EPI_UNUSED(value);
    }

    static void rtPatchChange(void *userdata, uint8_t channel, uint8_t patch)
    {
        EPI_UNUSED(userdata);
        EPI_UNUSED(channel);
        EPI_UNUSED(patch);
    }

    static void rtPitchBend(void *userdata, uint8_t channel, uint8_t msb, uint8_t lsb)
    {
        EPI_UNUSED(userdata);
        EPI_UNUSED(channel);
        EPI_UNUSED(msb);
        EPI_UNUSED(lsb);
    }

    static void rtSysEx(void *userdata, const uint8_t *msg, size_t size)
    {
        EPI_UNUSED(userdata);
        EPI_UNUSED(msg);
        EPI_UNUSED(size);
    }

    static void rtDeviceSwitch(void *userdata, size_t track, const char *data, size_t length)
    {
        EPI_UNUSED(userdata);
        EPI_UNUSED(track);
        EPI_UNUSED(data);
        EPI_UNUSED(length);
    }

    static size_t rtCurrentDevice(void *userdata, size_t track)
    {
        EPI_UNUSED(userdata);
        EPI_UNUSED(track);
        return 0;
    }

    static void rtRawOPL(void *userdata, uint8_t reg, uint8_t value)
    {
        EPI_UNUSED(userdata);
        if ((reg & 0xF0) == 0xC0)
            value |= 0x30;
        imf_opl->Port(reg, value);
    }

    static void playSynth(void *userdata, uint8_t *stream, size_t length)
    {
        EPI_UNUSED(userdata);
        for (size_t i = 0; i < length / 2; i += 2)
            imf_opl->Sample((int16_t *)stream + i, (int16_t *)stream + i + 1);
    }

    void SequencerInit()
    {
        imf_sequencer__ = new IMFSequencer;
        imf_interface_ = new IMFInterface;
        EPI_CLEAR_MEMORY(imf_interface_, MidiRealTimeInterface, 1);

        imf_interface_->rtUserData           = this;
        imf_interface_->rt_noteOn            = rtNoteOn;
        imf_interface_->rt_noteOff           = rtNoteOff;
        imf_interface_->rt_noteAfterTouch    = rtNoteAfterTouch;
        imf_interface_->rt_channelAfterTouch = rtChannelAfterTouch;
        imf_interface_->rt_controllerChange  = rtControllerChange;
        imf_interface_->rt_patchChange       = rtPatchChange;
        imf_interface_->rt_pitchBend         = rtPitchBend;
        imf_interface_->rt_systemExclusive   = rtSysEx;

        imf_interface_->onPcmRender          = playSynth;
        imf_interface_->onPcmRender_userdata = this;

        imf_interface_->pcmSampleRate = sound_device_frequency;
        imf_interface_->pcmFrameSize  = 2 /*channels*/ * sizeof(int16_t) /*size of one sample*/;

        imf_interface_->rt_deviceSwitch  = rtDeviceSwitch;
        imf_interface_->rt_currentDevice = rtCurrentDevice;
        imf_interface_->rt_rawOPL        = rtRawOPL;

        imf_sequencer__->SetInterface(imf_interface_);
    }

    bool LoadTrack(const uint8_t *data, int length, uint16_t rate)
    {
        return imf_sequencer__->LoadMidi(data, length, rate);
    }

    void Close(void)
    {
        if (status_ == kNotLoaded)
            return;

        // Stop playback
        if (status_ != kStopped)
            Stop();

        if (imf_opl)
        {
            delete imf_opl;
            imf_opl = nullptr;
        }
        if (imf_sequencer__)
        {
            delete imf_sequencer__;
            imf_sequencer__ = nullptr;
        }
        if (imf_interface_)
        {
            delete imf_interface_;
            imf_interface_ = nullptr;
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
        while (status_ == kPlaying && !pc_speaker_mode && !playing_movie)
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

        int played = imf_sequencer__->PlayStream((uint8_t *)(buf->data_), kMusicBuffer);

        if (imf_sequencer__->PositionAtEnd())
            song_done = true;

        buf->length_ = played / 2 / sizeof(int16_t);

        if (song_done) /* EOF */
        {
            if (!looping_)
                return false;
            imf_sequencer__->Rewind();
            return true;
        }

        return true;
    }
};

AbstractMusicPlayer *PlayIMFMusic(uint8_t *data, int length, bool loop, int type)
{
    imf_opl = new Opal(sound_device_frequency);
    IMFPlayer *player = new IMFPlayer(loop);

    if (!imf_opl || !player)
    {
        LogDebug("IMF player: error initializing!\n");
        if (imf_opl)
        {
            delete imf_opl;
            imf_opl = nullptr;
        }
        if (player)
            delete player;
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

    if (rate == 0)
    {
        LogDebug("IMF player: no IMF sample rate provided!\n");
        delete[] data;
        delete imf_opl;
        imf_opl = nullptr;
        delete player;
        return nullptr;  
    }

    if (!player->LoadTrack(data, length,
                           rate)) // Lobo: quietly log it instead of completely exiting EDGE
    {
        LogDebug("IMF player: failed to load IMF file!\n");
        delete[] data;
        delete imf_opl;
        imf_opl = nullptr;
        delete player;
        return nullptr;
    }

    delete[] data;

    player->Play(loop);

    return player;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
