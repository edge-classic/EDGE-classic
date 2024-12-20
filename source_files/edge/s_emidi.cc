//----------------------------------------------------------------------------
//  EDGE Emu de MIDI Music Player
//----------------------------------------------------------------------------
//
//  Copyright (c) 2024  The EDGE Team.
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

#include "s_emidi.h"

#include <stdint.h>
#include <string.h>

#include "CSMFPlay.hpp"
#include "dm_state.h"
#include "epi_file.h"
#include "epi_filesystem.h"
#include "epi_str_util.h"
#include "i_movie.h"
#include "i_system.h"
#include "m_misc.h"
#include "s_blit.h"
// clang-format off
#define MidiFraction EMIDIFraction
#define MidiSequencer EMIDISequencer
typedef struct MidiRealTimeInterface EMIDIInterface;
#include "s_midi.h"
// clang-format on

extern int  sound_device_frequency;

// Should only be invoked when switching MIDI players
void RestartEMIDI(void)
{
    int old_entry = entry_playing;

    StopMusic();

    ChangeMusic(old_entry, true); // Restart track that was kPlaying when switched

    return;                       // OK!
}

class EMIDIPlayer : public AbstractMusicPlayer
{
  private:
  private:
    enum status_
    {
        kNotLoaded,
        kPlaying,
        kPaused,
        kStopped
    };

    int  status_;
    bool looping_;

    EMIDIInterface *emidi_interface_;

  public:
    EMIDIPlayer(uint8_t *data, int length, bool looping) : status_(kNotLoaded), looping_(looping)
    {
        SequencerInit();
    }

    ~EMIDIPlayer()
    {
        Close();
    }

  public:
    EMIDISequencer             *emidi_sequencer_;
    dsa::CSMFPlay              *emidi_synth_;

    static void rtNoteOn(void *userdata, uint8_t channel, uint8_t note, uint8_t velocity)
    {
        EMIDIPlayer *player = (EMIDIPlayer *)userdata;
        player->emidi_synth_->SendMIDIMessage({dsa::CMIDIMsg::NOTE_ON, channel, note, velocity});
    }

    static void rtNoteOff(void *userdata, uint8_t channel, uint8_t note)
    {
        EMIDIPlayer *player = (EMIDIPlayer *)userdata;
        player->emidi_synth_->SendMIDIMessage({dsa::CMIDIMsg::NOTE_OFF, channel, note});
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
        EMIDIPlayer *player = (EMIDIPlayer *)userdata;
        player->emidi_synth_->SendMIDIMessage({dsa::CMIDIMsg::CHANNEL_PRESSURE, channel, atVal});
    }

    static void rtControllerChange(void *userdata, uint8_t channel, uint8_t type, uint8_t value)
    {
        EMIDIPlayer *player = (EMIDIPlayer *)userdata;
        player->emidi_synth_->SendMIDIMessage({dsa::CMIDIMsg::CONTROL_CHANGE, channel, type, value});
    }

    static void rtPatchChange(void *userdata, uint8_t channel, uint8_t patch)
    {
        EMIDIPlayer *player = (EMIDIPlayer *)userdata;
        player->emidi_synth_->SendMIDIMessage({dsa::CMIDIMsg::PROGRAM_CHANGE, channel, patch});
    }

    static void rtPitchBend(void *userdata, uint8_t channel, uint8_t msb, uint8_t lsb)
    {
        EMIDIPlayer *player = (EMIDIPlayer *)userdata;
        player->emidi_synth_->SendMIDIMessage({dsa::CMIDIMsg::PITCH_BEND_CHANGE, channel, lsb, msb});
    }

    static void rtSysEx(void *userdata, const uint8_t *msg, size_t size)
    {
        (void)userdata;
        (void)msg;
        (void)size;
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
        EMIDIPlayer *player = (EMIDIPlayer *)userdata;
        player->emidi_synth_->Render16((int16_t *)stream, length / 4);
    }

    void SequencerInit()
    {
        emidi_sequencer_ = new EMIDISequencer;
        emidi_interface_ = new EMIDIInterface;
        memset(emidi_interface_, 0, sizeof(MidiRealTimeInterface));

        emidi_interface_->rtUserData           = this;
        emidi_interface_->rt_noteOn            = rtNoteOn;
        emidi_interface_->rt_noteOff           = rtNoteOff;
        emidi_interface_->rt_noteAfterTouch    = rtNoteAfterTouch;
        emidi_interface_->rt_channelAfterTouch = rtChannelAfterTouch;
        emidi_interface_->rt_controllerChange  = rtControllerChange;
        emidi_interface_->rt_patchChange       = rtPatchChange;
        emidi_interface_->rt_pitchBend         = rtPitchBend;
        emidi_interface_->rt_systemExclusive   = rtSysEx;

        emidi_interface_->onPcmRender          = playSynth;
        emidi_interface_->onPcmRender_userdata = this;

        emidi_interface_->pcmSampleRate = sound_device_frequency;
        emidi_interface_->pcmFrameSize  = 2 /*channels*/ * 2 /*size of one sample*/;

        emidi_interface_->rt_deviceSwitch  = rtDeviceSwitch;
        emidi_interface_->rt_currentDevice = rtCurrentDevice;

        emidi_sequencer_->SetInterface(emidi_interface_);
    }

    bool LoadTrack(const uint8_t *data, int length)
    {
        return emidi_sequencer_->LoadMidi(data, length);
    }

    void Close(void)
    {
        if (status_ == kNotLoaded)
            return;

        // Stop playback
        if (status_ != kStopped)
            Stop();

        if (emidi_sequencer_)
        {
            delete emidi_sequencer_;
            emidi_sequencer_ = nullptr;
        }
        if (emidi_interface_)
        {
            delete emidi_interface_;
            emidi_interface_ = nullptr;
        }
        if (emidi_synth_)
        {
            delete emidi_synth_;
            emidi_synth_ = nullptr;
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

        music_player_gain = 2.0f;

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
        int16_t *data_buf = buf->data_;

        bool song_done = false;

        int played = emidi_sequencer_->PlayStream((uint8_t *)data_buf, kMusicBuffer);

        if (emidi_sequencer_->PositionAtEnd())
            song_done = true;

        buf->length_ = played / 4;

        if (song_done) /* EOF */
        {
            if (!looping_)
                return false;
            emidi_sequencer_->Rewind();
            return true;
        }

        return true;
    }
};

AbstractMusicPlayer *PlayEMIDIMusic(uint8_t *data, int length, bool loop)
{
    EMIDIPlayer *player = new EMIDIPlayer(data, length, loop);

    if (!player)
    {
        LogDebug("Emu de MIDI player: error initializing!\n");
        delete[] data;
        return nullptr;
    }

    player->emidi_synth_ = new dsa::CSMFPlay(sound_device_frequency, var_midi_player == 2 ? dsa::CSMFPlay::OPLL_MODE : dsa::CSMFPlay::SCC_PSG_MODE);
    if (!player->emidi_synth_)
    {
        LogDebug("Emu de MIDI player: error initializing!\n");
        delete[] data;
        delete player;
        return nullptr;
    }

    if (!player->LoadTrack(data, length)) // Lobo: quietly log it instead of completely exiting EDGE
    {
        LogDebug("Emu de MIDI player: failed to load MIDI file!\n");
        delete[] data;
        delete player;
        return nullptr;
    }

    delete[] data;

    player->emidi_synth_->Start();

    player->Play(loop);

    return player;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
