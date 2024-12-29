//----------------------------------------------------------------------------
//  EDGE TinySoundFont Music Player
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

#include "s_tsf.h"

#include <stdint.h>

#include "HandmadeMath.h"
#include "dm_state.h"
#include "epi.h"
#include "epi_file.h"
#include "epi_filesystem.h"
#include "i_movie.h"
#include "i_system.h"
#include "m_misc.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"
#include "s_blit.h"
// clang-format off
#define MidiFraction TSFFraction
#define MidiSequencer TSFSequencer
typedef struct MidiRealTimeInterface TSFInterface;
#include "s_midi.h"
// clang-format on
#include "s_music.h"
#include "tsf.h"

extern int  sound_device_frequency;

bool tsf_disabled = false;

tsf *edge_tsf = nullptr;

EDGE_DEFINE_CONSOLE_VARIABLE(midi_soundfont, "", (ConsoleVariableFlag)(kConsoleVariableFlagArchive | kConsoleVariableFlagFilepath))

EDGE_DEFINE_CONSOLE_VARIABLE(tsf_player_gain, "0.6", kConsoleVariableFlagArchive)

extern std::vector<std::string> available_soundfonts;

bool StartupTSF(void)
{
    LogPrint("Initializing TinySoundFont...\n");

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
            FatalError("TinySoundFont: Cannot locate default soundfont (Default.sf2)! "
                       "Please check the /soundfont directory "
                       "of your EDGE-Classic install!\n");
    }

    epi::File *raw_sf2 = epi::FileOpen(midi_soundfont.s_, epi::kFileAccessBinary|epi::kFileAccessRead);
    size_t raw_sf2_size = raw_sf2->GetLength();
    uint8_t *raw_sf2_data = raw_sf2->LoadIntoMemory();

    edge_tsf = tsf_load_memory(raw_sf2_data, raw_sf2_size);

    delete[] raw_sf2_data;
    delete raw_sf2;

    for(int ch = 0; ch < 16; ch++)
        tsf_channel_set_bank(edge_tsf, ch, 0);
    tsf_channel_set_bank_preset(edge_tsf, 9, 128, 0);
    tsf_set_output(edge_tsf, TSF_STEREO_INTERLEAVED, sound_device_frequency);
    tsf_set_volume(edge_tsf, tsf_player_gain.f_);

    return true; // OK!
}

// Should only be invoked when switching soundfonts
void RestartTSF(void)
{
    if (tsf_disabled)
        return;

    LogPrint("Restarting TinySoundFont...\n");

    int old_entry = entry_playing;

    StopMusic();

    tsf_close(edge_tsf);
    edge_tsf = nullptr;

    if (!StartupTSF())
    {
        tsf_disabled = true;
        return;
    }

    ChangeMusic(old_entry,
                true); // Restart track that was playing when switched

    return;            // OK!
}

class TSFPlayer : public AbstractMusicPlayer
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

    TSFInterface *tsf_interface_;

  public:
    TSFPlayer(bool looping) : status_(kNotLoaded), looping_(looping)
    {
        SequencerInit();
    }

    ~TSFPlayer()
    {
        Close();
    }

  public:
    TSFSequencer *tsf_sequencer_;

    static void rtNoteOn(void *userdata, uint8_t channel, uint8_t note, uint8_t velocity)
    {
        EPI_UNUSED(userdata);
        tsf_channel_note_on(edge_tsf, channel, note, static_cast<float>(velocity) / 127.0f);
    }

    static void rtNoteOff(void *userdata, uint8_t channel, uint8_t note)
    {
        EPI_UNUSED(userdata);
        tsf_channel_note_off(edge_tsf, channel, note);
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
        tsf_channel_midi_control(edge_tsf, channel, type, value);
    }

    static void rtPatchChange(void *userdata, uint8_t channel, uint8_t patch)
    {
        EPI_UNUSED(userdata);
        tsf_channel_set_presetnumber(edge_tsf, channel, patch, channel == 9);
    }

    static void rtPitchBend(void *userdata, uint8_t channel, uint8_t msb, uint8_t lsb)
    {
        EPI_UNUSED(userdata);
        tsf_channel_set_pitchwheel(edge_tsf, channel, (msb << 7) | lsb);
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

    static void playSynth(void *userdata, uint8_t *stream, size_t length)
    {
        EPI_UNUSED(userdata);
        tsf_render_short(edge_tsf, (short *)stream, length / 2 / sizeof(int16_t), 0);
    }

    void SequencerInit()
    {
        tsf_sequencer_ = new TSFSequencer;
        tsf_interface_ = new TSFInterface;
        EPI_CLEAR_MEMORY(tsf_interface_, MidiRealTimeInterface, 1);

        tsf_interface_->rtUserData           = this;
        tsf_interface_->rt_noteOn            = rtNoteOn;
        tsf_interface_->rt_noteOff           = rtNoteOff;
        tsf_interface_->rt_noteAfterTouch    = rtNoteAfterTouch;
        tsf_interface_->rt_channelAfterTouch = rtChannelAfterTouch;
        tsf_interface_->rt_controllerChange  = rtControllerChange;
        tsf_interface_->rt_patchChange       = rtPatchChange;
        tsf_interface_->rt_pitchBend         = rtPitchBend;
        tsf_interface_->rt_systemExclusive   = rtSysEx;

        tsf_interface_->onPcmRender          = playSynth;
        tsf_interface_->onPcmRender_userdata = this;

        tsf_interface_->pcmSampleRate = sound_device_frequency;
        tsf_interface_->pcmFrameSize  = 2 /*channels*/ * sizeof(int16_t) /*size of one sample*/;

        tsf_interface_->rt_deviceSwitch  = rtDeviceSwitch;
        tsf_interface_->rt_currentDevice = rtCurrentDevice;

        tsf_sequencer_->SetInterface(tsf_interface_);
    }

    bool LoadTrack(const uint8_t *data, int length)
    {
        return tsf_sequencer_->LoadMidi(data, length);
    }

    void Close(void)
    {
        if (status_ == kNotLoaded)
            return;

        // Stop playback
        if (status_ != kStopped)
            Stop();

        if (tsf_sequencer_)
        {
            delete tsf_sequencer_;
            tsf_sequencer_ = nullptr;
        }
        if (tsf_interface_)
        {
            delete tsf_interface_;
            tsf_interface_ = nullptr;
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

        tsf_note_off_all(edge_tsf);
        for(int ch = 0; ch < 16; ch++)
            tsf_channel_sounds_off_all(edge_tsf, ch);

        SoundQueueStop();

        status_ = kStopped;
    }

    void Pause(void)
    {
        if (status_ != kPlaying)
            return;

        tsf_note_off_all(edge_tsf);

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
        if (tsf_player_gain.CheckModified())
        {
            tsf_player_gain.f_ = HMM_Clamp(0.0, tsf_player_gain.f_, 2.0f);
            tsf_player_gain    = tsf_player_gain.f_;
            tsf_set_volume(edge_tsf, tsf_player_gain.f_);
        }

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

        int played = tsf_sequencer_->PlayStream((uint8_t *)buf->data_, kMusicBuffer);

        if (tsf_sequencer_->PositionAtEnd())
            song_done = true;

        buf->length_ = played / 2 / sizeof(int16_t);

        if (song_done) /* EOF */
        {
            if (!looping_)
                return false;
            tsf_sequencer_->Rewind();
            return true;
        }

        return true;
    }
};

AbstractMusicPlayer *PlayTSFMusic(uint8_t *data, int length, bool loop)
{
    if (tsf_disabled)
    {
        delete[] data;
        return nullptr;
    }

    TSFPlayer *player = new TSFPlayer(loop);

    if (!player)
    {
        LogDebug("TinySoundFont player: error initializing!\n");
        delete[] data;
        return nullptr;
    }

    if (!player->LoadTrack(data,
                           length)) // Lobo: quietly log it instead of completely exiting EDGE
    {
        LogDebug("TinySoundFont player: failed to load MIDI file!\n");
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
