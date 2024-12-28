//----------------------------------------------------------------------------
//  EDGE RAD Music Player
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

#include "ddf_playlist.h"
#include "epi.h"
#include "epi_endian.h"
#include "epi_file.h"
#include "epi_filesystem.h"
#include "i_movie.h"
#include "opal.h"
#include "radplay.h"
#include "s_blit.h"
#include "s_cache.h"
#include "s_music.h"
#include "snd_gather.h"
#include "w_wad.h"

extern int  sound_device_frequency;

// Works better with the RAD code if these are 'global'
Opal      *edge_opal = nullptr;
RADPlayer *edge_rad  = nullptr;

class RadPlayer : public AbstractMusicPlayer
{
  public:
    RadPlayer();
    ~RadPlayer();

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

    int      sample_count_;
    int      sample_update_;
    int      sample_rate_;
    uint8_t *tune_;

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

RadPlayer::RadPlayer() : status_(kNotLoaded), tune_(nullptr)
{
}

RadPlayer::~RadPlayer()
{
    Close();
}

void RadPlayer::PostOpen()
{
    sample_count_  = 0;
    sample_update_ = sound_device_frequency / sample_rate_;
    // Loaded, but not playing
    status_ = kStopped;
}

bool RadPlayer::StreamIntoBuffer(SoundData *buf)
{
    int16_t *data_buf = buf->data_;

    bool song_done = false;
    int  samples   = 0;

    for (int i = 0; i < kMusicBuffer; i += 2)
    {
        edge_opal->Sample(data_buf + i, data_buf + i + 1);
        samples++;
        sample_count_++;
        if (sample_count_ >= sample_update_)
        {
            sample_count_ = 0;
            song_done     = edge_rad->Update();
        }
    }

    buf->length_ = samples;

    if (song_done) /* EOF */
    {
        if (!looping_)
            return false;
        return true;
    }

    return true;
}

bool RadPlayer::OpenMemory(uint8_t *data, int length)
{
    EPI_ASSERT(data);

    const char *err = RADValidate(data, length);

    if (err)
    {
        LogWarning("RAD: Cannot play tune: %s\n", err);
        return false;
    }

    edge_opal = new Opal(sound_device_frequency);
    edge_rad  = new RADPlayer;
    edge_rad->Init(data, [](void *arg, uint16_t reg_num, uint8_t val) { EPI_UNUSED(arg); edge_opal->Port(reg_num, val); }, 0);

    sample_rate_ = edge_rad->GetHertz();

    if (sample_rate_ <= 0)
    {
        LogWarning("RAD: failure to load song!\n");
        delete edge_rad;
        edge_rad = nullptr;
        delete edge_opal;
        edge_opal = nullptr;
        return false;
    }

    // The player needs to free this afterwards
    tune_ = data;

    PostOpen();
    return true;
}

void RadPlayer::Close()
{
    if (status_ == kNotLoaded)
        return;

    // Stop playback
    if (status_ != kStopped)
        Stop();

    delete edge_rad;
    edge_rad = nullptr;
    delete edge_opal;
    edge_opal = nullptr;
    delete[] tune_;

    status_ = kNotLoaded;
}

void RadPlayer::Pause()
{
    if (status_ != kPlaying)
        return;

    status_ = kPaused;
}

void RadPlayer::Resume()
{
    if (status_ != kPaused)
        return;

    status_ = kPlaying;
}

void RadPlayer::Play(bool loop)
{
    if (status_ != kNotLoaded && status_ != kStopped)
        return;

    status_  = kPlaying;
    looping_ = loop;

    // Load up initial buffer data
    Ticker();
}

void RadPlayer::Stop()
{
    if (status_ != kPlaying && status_ != kPaused)
        return;

    SoundQueueStop();

    edge_rad->Stop();

    status_ = kStopped;
}

void RadPlayer::Ticker()
{
    while (status_ == kPlaying && !pc_speaker_mode && !playing_movie)
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
            // finished playing
            SoundQueueReturnBuffer(buf);
            Stop();
        }
    }
}

//----------------------------------------------------------------------------

AbstractMusicPlayer *PlayRADMusic(uint8_t *data, int length, bool looping)
{
    RadPlayer *player = new RadPlayer();

    if (!player->OpenMemory(data, length))
    {
        delete[] data;
        delete player;
        return nullptr;
    }

    player->Play(looping);

    return player;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
