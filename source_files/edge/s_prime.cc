//----------------------------------------------------------------------------
//  EDGE Primesynth Music Player
//----------------------------------------------------------------------------
//
//  Copyright (c) 2023-2024 The EDGE Team.
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

#include "i_defs.h"

#include "file.h"
#include "filesystem.h"
#include "str_util.h"

#include "m_misc.h"
#include "s_blit.h"
#include "s_music.h"
#include "s_prime.h"

#include "dm_state.h"

#define BW_MidiSequencer PrimeSequencer
typedef struct BW_MidiRtInterface PrimeInterface;
#include "midi_sequencer_impl.hpp"

#include "synthesizer.h"

#define PRIME_SAMPLES 1024

extern bool dev_stereo;
extern int  dev_freq;

bool prime_disabled = false;

primesynth::Synthesizer *edge_synth = nullptr;

DEF_CVAR(s_soundfont, "", (CVAR_ARCHIVE | CVAR_PATH))

DEF_CVAR(s_primegain, "0.4", CVAR_ARCHIVE)

extern std::vector<std::filesystem::path> available_soundfonts;

static void ConvertToMono(int16_t *dest, const int16_t *src, int len)
{
    const int16_t *s_end = src + len * 2;

    for (; src < s_end; src += 2)
    {
        // compute average of samples
        *dest++ = ((int)src[0] + (int)src[1]) >> 1;
    }
}

bool S_StartupPrime(void)
{
    I_Printf("Initializing Primesynth...\n");

    // Check for presence of previous CVAR value's file
    bool cvar_good = false;
    for (size_t i = 0; i < available_soundfonts.size(); i++)
    {
        if (epi::case_cmp(s_soundfont.s, available_soundfonts.at(i).generic_u8string()) == 0)
        {
            cvar_good = true;
            break;
        }
    }

    if (!cvar_good)
    {
        I_Warning("Cannot find previously used soundfont %s, falling back to default!\n", s_soundfont.c_str());
        s_soundfont = std::filesystem::path(std::filesystem::path(game_dir).append("soundfont")).append("Default.sf2").generic_u8string();
        if (!std::filesystem::exists(std::filesystem::u8path(s_soundfont.s)))
            I_Error("Primesynth: Cannot locate default soundfont (Default.sf2)! Please check the /soundfont directory "
                    "of your EDGE-Classic install!\n");
    }

    edge_synth = new primesynth::Synthesizer;
    edge_synth->setVolume(s_primegain.f);
    edge_synth->loadSoundFont(std::filesystem::u8path(s_soundfont.s).generic_string());

    // Primesynth should throw an exception if the soundfont loading fails, so I guess we're good if we get here
    return true; // OK!
}

// Should only be invoked when switching soundfonts
void S_RestartPrime(void)
{
    if (prime_disabled)
        return;

    I_Printf("Restarting Primesynth...\n");

    int old_entry = entry_playing;

    S_StopMusic();

    delete edge_synth;
    edge_synth = nullptr;

    if (!S_StartupPrime())
    {
        prime_disabled = true;
        return;
    }

    S_ChangeMusic(old_entry, true); // Restart track that was playing when switched

    return; // OK!
}

class prime_player_c : public abstract_music_c
{
  private:
    enum status_e
    {
        NOT_LOADED,
        PLAYING,
        PAUSED,
        STOPPED
    };

    int  status;
    bool looping;

    PrimeInterface *prime_iface;

    int16_t *mono_buffer;

  public:
    prime_player_c(uint8_t *_data, int _length, bool _looping) : status(NOT_LOADED), looping(_looping)
    {
        mono_buffer = new int16_t[PRIME_SAMPLES * 2];
        SequencerInit();
    }

    ~prime_player_c()
    {
        Close();

        if (mono_buffer)
            delete[] mono_buffer;
    }

  public:
    PrimeSequencer *prime_seq;

    static void rtNoteOn(void *userdata, uint8_t channel, uint8_t note, uint8_t velocity)
    {
        edge_synth->processChannelMessage(primesynth::midi::MessageStatus::NoteOn, channel, note, velocity);
    }

    static void rtNoteOff(void *userdata, uint8_t channel, uint8_t note)
    {
        edge_synth->processChannelMessage(primesynth::midi::MessageStatus::NoteOff, channel, note);
    }

    static void rtNoteAfterTouch(void *userdata, uint8_t channel, uint8_t note, uint8_t atVal)
    {
        edge_synth->processChannelMessage(primesynth::midi::MessageStatus::KeyPressure, channel, note, atVal);
    }

    static void rtChannelAfterTouch(void *userdata, uint8_t channel, uint8_t atVal)
    {
        edge_synth->processChannelMessage(primesynth::midi::MessageStatus::ChannelPressure, channel, atVal);
    }

    static void rtControllerChange(void *userdata, uint8_t channel, uint8_t type, uint8_t value)
    {
        edge_synth->processChannelMessage(primesynth::midi::MessageStatus::ControlChange, channel, type, value);
    }

    static void rtPatchChange(void *userdata, uint8_t channel, uint8_t patch)
    {
        edge_synth->processChannelMessage(primesynth::midi::MessageStatus::ProgramChange, channel, patch);
    }

    static void rtPitchBend(void *userdata, uint8_t channel, uint8_t msb, uint8_t lsb)
    {
        edge_synth->processChannelMessage(primesynth::midi::MessageStatus::PitchBend, channel, lsb, msb);
    }

    static void rtSysEx(void *userdata, const uint8_t *msg, size_t size)
    {
        edge_synth->processSysEx(reinterpret_cast<const char *>(msg), size);
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
        edge_synth->render_s16(reinterpret_cast<int16_t *>(stream), length / 2);
    }

    void SequencerInit()
    {
        prime_seq   = new PrimeSequencer;
        prime_iface = new PrimeInterface;
        std::memset(prime_iface, 0, sizeof(BW_MidiRtInterface));

        prime_iface->rtUserData           = this;
        prime_iface->rt_noteOn            = rtNoteOn;
        prime_iface->rt_noteOff           = rtNoteOff;
        prime_iface->rt_noteAfterTouch    = rtNoteAfterTouch;
        prime_iface->rt_channelAfterTouch = rtChannelAfterTouch;
        prime_iface->rt_controllerChange  = rtControllerChange;
        prime_iface->rt_patchChange       = rtPatchChange;
        prime_iface->rt_pitchBend         = rtPitchBend;
        prime_iface->rt_systemExclusive   = rtSysEx;

        prime_iface->onPcmRender          = playSynth;
        prime_iface->onPcmRender_userData = this;

        prime_iface->pcmSampleRate = dev_freq;
        prime_iface->pcmFrameSize  = 2 /*channels*/ * 2 /*size of one sample*/;

        prime_iface->rt_deviceSwitch  = rtDeviceSwitch;
        prime_iface->rt_currentDevice = rtCurrentDevice;

        prime_seq->setInterface(prime_iface);
    }

    bool LoadTrack(const uint8_t *data, int length)
    {
        return prime_seq->loadMIDI(data, length);
    }

    void Close(void)
    {
        if (status == NOT_LOADED)
            return;

        // Stop playback
        if (status != STOPPED)
            Stop();

        if (prime_seq)
        {
            delete prime_seq;
            prime_seq = nullptr;
        }
        if (prime_iface)
        {
            delete prime_iface;
            prime_iface = nullptr;
        }

        status = NOT_LOADED;
    }

    void Play(bool loop)
    {
        if (!(status == NOT_LOADED || status == STOPPED))
            return;

        status  = PLAYING;
        looping = loop;

        // Load up initial buffer data
        Ticker();
    }

    void Stop(void)
    {
        if (!(status == PLAYING || status == PAUSED))
            return;

        edge_synth->stop();

        S_QueueStop();

        status = STOPPED;
    }

    void Pause(void)
    {
        if (status != PLAYING)
            return;

        edge_synth->pause();

        status = PAUSED;
    }

    void Resume(void)
    {
        if (status != PAUSED)
            return;

        status = PLAYING;
    }

    void Ticker(void)
    {
        if (s_primegain.CheckModified())
        {
            s_primegain.f = CLAMP(0.0, s_primegain.f, 2.0f);
            s_primegain   = s_primegain.f;
            edge_synth->setVolume(s_primegain.f);
        }

        while (status == PLAYING && !var_pc_speaker_mode)
        {
            epi::sound_data_c *buf =
                S_QueueGetFreeBuffer(PRIME_SAMPLES, dev_stereo ? epi::SBUF_Interleaved : epi::SBUF_Mono);

            if (!buf)
                break;

            if (StreamIntoBuffer(buf))
            {
                S_QueueAddBuffer(buf, dev_freq);
            }
            else
            {
                // finished playing
                S_QueueReturnBuffer(buf);

                Stop();
            }
        }
    }

  private:
    bool StreamIntoBuffer(epi::sound_data_c *buf)
    {
        int16_t *data_buf;

        bool song_done = false;

        if (!dev_stereo)
            data_buf = mono_buffer;
        else
            data_buf = buf->data_L;

        int played = prime_seq->playStream(reinterpret_cast<uint8_t *>(data_buf), PRIME_SAMPLES);

        if (prime_seq->positionAtEnd())
            song_done = true;

        buf->length = played / 4;

        if (!dev_stereo)
            ConvertToMono(buf->data_L, mono_buffer, buf->length);

        if (song_done) /* EOF */
        {
            if (!looping)
                return false;
            prime_seq->rewind();
            return true;
        }

        return true;
    }
};

abstract_music_c *S_PlayPrime(uint8_t *data, int length, bool loop)
{
    if (prime_disabled)
    {
        delete[] data;
        return nullptr;
    }

    prime_player_c *player = new prime_player_c(data, length, loop);

    if (!player)
    {
        I_Debugf("Primesynth player: error initializing!\n");
        delete[] data;
        return nullptr;
    }

    if (!player->LoadTrack(data, length)) // Lobo: quietly log it instead of completely exiting EDGE
    {
        I_Debugf("Primesynth player: failed to load MIDI file!\n");
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
