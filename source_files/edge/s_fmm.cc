//----------------------------------------------------------------------------
//  EDGE FMMIDI Music Player
//----------------------------------------------------------------------------
//
//  Copyright (c) 2023-2024  The EDGE Team.
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

#include "s_fmm.h"

#include <stdint.h>
#include <string.h>

#include "dm_state.h"
#include "epi_file.h"
#include "epi_filesystem.h"
#include "epi_str_util.h"
#include "i_movie.h"
#include "i_system.h"
#include "m_misc.h"
#include "midisynth.hpp"
#include "s_blit.h"
// clang-format off
#define MidiFraction FMMFraction
#define MidiSequencer FMMSequencer
typedef struct MidiRealTimeInterface FMMInterface;
#include "s_midi.h"
// clang-format on

extern int  sound_device_frequency;

// Should only be invoked when switching MIDI players
void RestartFMM(void)
{
    int old_entry = entry_playing;

    StopMusic();

    ChangeMusic(old_entry, true); // Restart track that was kPlaying when switched

    return;                       // OK!
}

class FMMPlayer : public AbstractMusicPlayer
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

    FMMInterface *fmm_interface_;

  public:
    FMMPlayer(uint8_t *data, int length, bool looping) : status_(kNotLoaded), looping_(looping)
    {
        SequencerInit();
    }

    ~FMMPlayer()
    {
        Close();
    }

  public:
    FMMSequencer               *fmm_sequencer_;
    midisynth::synthesizer     *fmm_synth_;
    midisynth::fm_note_factory *fmm_note_factory_;

    static void rtNoteOn(void *userdata, uint8_t channel, uint8_t note, uint8_t velocity)
    {
        FMMPlayer *player = (FMMPlayer *)userdata;
        player->fmm_synth_->note_on(channel, note, velocity);
    }

    static void rtNoteOff(void *userdata, uint8_t channel, uint8_t note)
    {
        FMMPlayer *player = (FMMPlayer *)userdata;
        player->fmm_synth_->note_off(channel, note, 0);
    }

    static void rtNoteAfterTouch(void *userdata, uint8_t channel, uint8_t note, uint8_t atVal)
    {
        FMMPlayer *player = (FMMPlayer *)userdata;
        player->fmm_synth_->polyphonic_key_pressure(channel, note, atVal);
    }

    static void rtChannelAfterTouch(void *userdata, uint8_t channel, uint8_t atVal)
    {
        FMMPlayer *player = (FMMPlayer *)userdata;
        player->fmm_synth_->channel_pressure(channel, atVal);
    }

    static void rtControllerChange(void *userdata, uint8_t channel, uint8_t type, uint8_t value)
    {
        FMMPlayer *player = (FMMPlayer *)userdata;
        player->fmm_synth_->control_change(channel, type, value);
    }

    static void rtPatchChange(void *userdata, uint8_t channel, uint8_t patch)
    {
        FMMPlayer *player = (FMMPlayer *)userdata;
        player->fmm_synth_->program_change(channel, patch);
    }

    static void rtPitchBend(void *userdata, uint8_t channel, uint8_t msb, uint8_t lsb)
    {
        FMMPlayer *player = (FMMPlayer *)userdata;
        player->fmm_synth_->pitch_bend_change(channel, (msb << 7) | lsb);
    }

    static void rtSysEx(void *userdata, const uint8_t *msg, size_t size)
    {
        FMMPlayer *player = (FMMPlayer *)userdata;
        player->fmm_synth_->sysex_message(msg, size);
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
        FMMPlayer *player = (FMMPlayer *)userdata;
        player->fmm_synth_->synthesize((int_least16_t *)stream, length / 4, sound_device_frequency);
    }

    void SequencerInit()
    {
        fmm_sequencer_ = new FMMSequencer;
        fmm_interface_ = new FMMInterface;
        memset(fmm_interface_, 0, sizeof(MidiRealTimeInterface));

        fmm_interface_->rtUserData           = this;
        fmm_interface_->rt_noteOn            = rtNoteOn;
        fmm_interface_->rt_noteOff           = rtNoteOff;
        fmm_interface_->rt_noteAfterTouch    = rtNoteAfterTouch;
        fmm_interface_->rt_channelAfterTouch = rtChannelAfterTouch;
        fmm_interface_->rt_controllerChange  = rtControllerChange;
        fmm_interface_->rt_patchChange       = rtPatchChange;
        fmm_interface_->rt_pitchBend         = rtPitchBend;
        fmm_interface_->rt_systemExclusive   = rtSysEx;

        fmm_interface_->onPcmRender          = playSynth;
        fmm_interface_->onPcmRender_userdata = this;

        fmm_interface_->pcmSampleRate = sound_device_frequency;
        fmm_interface_->pcmFrameSize  = 2 /*channels*/ * 2 /*size of one sample*/;

        fmm_interface_->rt_deviceSwitch  = rtDeviceSwitch;
        fmm_interface_->rt_currentDevice = rtCurrentDevice;

        fmm_sequencer_->SetInterface(fmm_interface_);
    }

    bool LoadTrack(const uint8_t *data, int length)
    {
        return fmm_sequencer_->LoadMidi(data, length);
    }

    void Close(void)
    {
        if (status_ == kNotLoaded)
            return;

        // Stop playback
        if (status_ != kStopped)
            Stop();

        if (fmm_sequencer_)
        {
            delete fmm_sequencer_;
            fmm_sequencer_ = nullptr;
        }
        if (fmm_interface_)
        {
            delete fmm_interface_;
            fmm_interface_ = nullptr;
        }
        if (fmm_note_factory_)
        {
            delete fmm_note_factory_;
            fmm_note_factory_ = nullptr;
        }
        if (fmm_synth_)
        {
            delete fmm_synth_;
            fmm_synth_ = nullptr;
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

        fmm_synth_->all_sound_off_immediately();

        SoundQueueStop();

        status_ = kStopped;
    }

    void Pause(void)
    {
        if (status_ != kPlaying)
            return;

        fmm_synth_->all_sound_off();

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

        int played = fmm_sequencer_->PlayStream((uint8_t *)data_buf, kMusicBuffer);

        if (fmm_sequencer_->PositionAtEnd())
            song_done = true;

        buf->length_ = played / 4;

        if (song_done) /* EOF */
        {
            if (!looping_)
                return false;
            fmm_sequencer_->Rewind();
            return true;
        }

        return true;
    }
};

AbstractMusicPlayer *PlayFMMMusic(uint8_t *data, int length, bool loop)
{
    FMMPlayer *player = new FMMPlayer(data, length, loop);

    if (!player)
    {
        LogDebug("FMMIDI player: error initializing!\n");
        delete[] data;
        return nullptr;
    }

    player->fmm_note_factory_ = new midisynth::fm_note_factory;
    if (!player->fmm_note_factory_)
    {
        LogDebug("FMMIDI player: error initializing!\n");
        delete[] data;
        delete player;
        return nullptr;
    }

    player->fmm_synth_ = new midisynth::synthesizer(player->fmm_note_factory_);
    if (!player->fmm_synth_)
    {
        LogDebug("FMMIDI player: error initializing!\n");
        delete[] data;
        delete player;
        return nullptr;
    }

    if (!player->LoadTrack(data, length)) // Lobo: quietly log it instead of completely exiting EDGE
    {
        LogDebug("FMMIDI player: failed to load MIDI file!\n");
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
