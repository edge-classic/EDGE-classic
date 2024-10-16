//----------------------------------------------------------------------------
//  EDGE cRSID Music Player
//----------------------------------------------------------------------------
//
//  Copyright (c) 2023-2024 - The EDGE Team.
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

#include "s_sid.h"

#include "ddf_playlist.h"
#include "epi.h"
#include "epi_endian.h"
#include "epi_file.h"
#include "epi_filesystem.h"
#include "i_system.h"
#include "libcRSID.h"
#include "s_blit.h"
#include "s_cache.h"
#include "s_music.h"
#include "snd_gather.h"
#include "w_wad.h"

extern int  sound_device_frequency;

class SIDPlayer : public AbstractMusicPlayer
{
  public:
    SIDPlayer();
    ~SIDPlayer();

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

    cRSID_C64instance *C64_      = nullptr;
    cRSID_SIDheader   *C64_song_ = nullptr;

  public:
    bool OpenMemory(uint8_t *data, int length);

    virtual void Close(void);

    virtual void Play(bool loop);
    virtual void Stop(void);

    virtual void Pause(void);
    virtual void Resume(void);

    virtual void Ticker(void);

  private:
    void PostOpenInit(void);

    bool StreamIntoBuffer(SoundData *buf);
};

//----------------------------------------------------------------------------

SIDPlayer::SIDPlayer() : status_(kNotLoaded)
{
}

SIDPlayer::~SIDPlayer()
{
    Close();
}

void SIDPlayer::PostOpenInit()
{
    cRSID_initSIDtune(C64_, C64_song_, 0);

    // Loaded, but not kPlaying
    status_ = kStopped;
}

bool SIDPlayer::StreamIntoBuffer(SoundData *buf)
{
    cRSID_generateFloat(C64_, buf->data_, kMusicBuffer);

    buf->length_ = kMusicBuffer / 2 / sizeof(float);

    return true;
}

bool SIDPlayer::OpenMemory(uint8_t *data, int length)
{
    EPI_ASSERT(data);

    if (status_ != kNotLoaded)
        Close();

    C64_ = cRSID_init(sound_device_frequency);

    if (!C64_)
    {
        LogWarning("[SIDPlayer]) Failed to initialize CRSID!\n");
        return false;
    }

    C64_song_ = cRSID_processSIDfile(C64_, data, length);

    if (!C64_song_)
    {
        LogWarning("[SIDPlayer::Open](DataLump) Failed\n");
        return false;
    }

    PostOpenInit();
    return true;
}

void SIDPlayer::Close()
{
    if (status_ == kNotLoaded)
        return;

    // Stop playback
    if (status_ != kStopped)
        Stop();

    // Reset individual player gain
    music_player_gain = 1.0f;

    status_ = kNotLoaded;
}

void SIDPlayer::Pause()
{
    if (status_ != kPlaying)
        return;

    status_ = kPaused;
}

void SIDPlayer::Resume()
{
    if (status_ != kPaused)
        return;

    status_ = kPlaying;
}

void SIDPlayer::Play(bool loop)
{
    if (status_ != kNotLoaded && status_ != kStopped)
        return;

    status_  = kPlaying;
    looping_ = loop;

    // Set individual player gain
    music_player_gain = 0.6f;

    // Load up initial buffer data
    Ticker();
}

void SIDPlayer::Stop()
{
    if (status_ != kPlaying && status_ != kPaused)
        return;

    SoundQueueStop();

    status_ = kStopped;
}

void SIDPlayer::Ticker()
{
    while (status_ == kPlaying && !pc_speaker_mode)
    {
        SoundData *buf = SoundQueueGetFreeBuffer(kMusicBuffer);

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
            // finished kPlaying
            SoundQueueReturnBuffer(buf);
            Stop();
        }
    }
}

//----------------------------------------------------------------------------

AbstractMusicPlayer *PlaySIDMusic(uint8_t *data, int length, bool looping)
{
    SIDPlayer *player = new SIDPlayer();

    if (!player->OpenMemory(data, length))
    {
        delete[] data;
        delete player;
        return NULL;
    }

    // cRSID retains the data after initializing the track
    delete[] data;

    player->Play(looping);

    return player;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
