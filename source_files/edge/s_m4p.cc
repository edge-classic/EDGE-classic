//----------------------------------------------------------------------------
//  EDGE MOD4PLAY Music Player
//----------------------------------------------------------------------------
//
//  Copyright (c) 2023 - The EDGE Team.
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

#include "endianess.h"
#include "epi.h"
#include "file.h"
#include "filesystem.h"
#include "m4p.h"
#include "playlist.h"
#include "s_blit.h"
#include "s_cache.h"
#include "s_music.h"
#include "snd_gather.h"
#include "w_wad.h"

#define M4P_BUFFER 1024

extern bool sound_device_stereo; // FIXME: encapsulation
extern int  sound_device_frequency;

class M4pPlayer : public AbstractMusicPlayer
{
  public:
    M4pPlayer();
    ~M4pPlayer();

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

  public:
    bool OpenMemory(uint8_t *data, int length);

    virtual void Close(void);

    virtual void Play(bool loop);
    virtual void Stop(void);

    virtual void Pause(void);
    virtual void Resume(void);

    virtual void Ticker(void);

  private:
    void PostOpen(void);

    bool StreamIntoBuffer(SoundData *buf);
};

//----------------------------------------------------------------------------

M4pPlayer::M4pPlayer() : status_(kNotLoaded)
{
    mono_buffer_ = new int16_t[M4P_BUFFER * 2];
}

M4pPlayer::~M4pPlayer()
{
    Close();

    if (mono_buffer_)
        delete[] mono_buffer_;
}

void M4pPlayer::PostOpen()
{
    // Loaded, but not playing
    status_ = kStopped;
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

bool M4pPlayer::StreamIntoBuffer(SoundData *buf)
{
    int16_t *data_buf;

    bool song_done = false;

    if (!sound_device_stereo)
        data_buf = mono_buffer_;
    else
        data_buf = buf->data_left_;

    m4p_GenerateSamples(data_buf, M4P_BUFFER / sizeof(int16_t));

    buf->length_ = M4P_BUFFER / 2;

    if (!sound_device_stereo)
        ConvertToMono(buf->data_left_, mono_buffer_, buf->length_);

    if (song_done) /* EOF */
    {
        if (!looping_)
            return false;
        m4p_Stop();
        m4p_PlaySong();
        return true;
    }

    return true;
}

bool M4pPlayer::OpenMemory(uint8_t *data, int length)
{
    EPI_ASSERT(data);

    if (!m4p_LoadFromData(data, length, sound_device_frequency, M4P_BUFFER))
    {
        LogWarning("M4P: failure to load song!\n");
        return false;
    }

    PostOpen();
    return true;
}

void M4pPlayer::Close()
{
    if (status_ == kNotLoaded)
        return;

    // Stop playback
    if (status_ != kStopped)
        Stop();

    m4p_Close();
    m4p_FreeSong();

    status_ = kNotLoaded;
}

void M4pPlayer::Pause()
{
    if (status_ != kPlaying)
        return;

    status_ = kPaused;
}

void M4pPlayer::Resume()
{
    if (status_ != kPaused)
        return;

    status_ = kPlaying;
}

void M4pPlayer::Play(bool loop)
{
    if (status_ != kNotLoaded && status_ != kStopped)
        return;

    status_  = kPlaying;
    looping_ = loop;

    m4p_PlaySong();

    // Load up initial buffer data
    Ticker();
}

void M4pPlayer::Stop()
{
    if (status_ != kPlaying && status_ != kPaused)
        return;

    SoundQueueStop();

    m4p_Stop();

    status_ = kStopped;
}

void M4pPlayer::Ticker()
{
    while (status_ == kPlaying && !pc_speaker_mode)
    {
        SoundData *buf = SoundQueueGetFreeBuffer(M4P_BUFFER, (sound_device_stereo) ? kMixInterleaved : kMixMono);

        if (!buf)
            break;

        if (StreamIntoBuffer(buf))
        {
            if (buf->length_ > 0)
            {
                SoundQueueAddBuffer(buf, sound_device_frequency);
            }
            else
            {
                SoundQueueReturnBuffer(buf);
            }
        }
        else
        {
            // finished playing
            SoundQueueReturnBuffer(buf);
            Stop();
        }
    }
}

//----------------------------------------------------------------------------

AbstractMusicPlayer *S_PlayM4PMusic(uint8_t *data, int length, bool looping)
{
    M4pPlayer *player = new M4pPlayer();

    if (!player->OpenMemory(data, length))
    {
        delete[] data;
        delete player;
        return nullptr;
    }

    delete[] data;

    player->Play(looping);

    return player;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
