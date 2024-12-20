//----------------------------------------------------------------------------
//  EDGE FLAC Music Player
//----------------------------------------------------------------------------
//
//  Copyright (c) 2022-2023 - The EDGE Team.
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

#include "s_flac.h"

#include "ddf_playlist.h"
#include "dr_flac.h"
#include "epi.h"
#include "epi_endian.h"
#include "epi_file.h"
#include "epi_filesystem.h"
#include "i_movie.h"
#include "s_blit.h"
#include "s_cache.h"
#include "s_music.h"
#include "snd_gather.h"
#include "w_wad.h"

extern int  sound_device_frequency;

class FLACPlayer : public AbstractMusicPlayer
{
  public:
    FLACPlayer();
    ~FLACPlayer();

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
    bool is_stereo_;

    drflac *flac_track_; // I had to make it rhyme

    uint8_t *flac_data_; // Passed in from s_music; must be deleted on close

  public:
    bool OpenMemory(uint8_t *data, int length);

    virtual void Close(void);

    virtual void Play(bool loop);
    virtual void Stop(void);

    virtual void Pause(void);
    virtual void Resume(void);

    virtual void Ticker(void);

    void PostOpen(void);

  private:
    bool StreamIntoBuffer(SoundData *buf);
};

//----------------------------------------------------------------------------

FLACPlayer::FLACPlayer() : status_(kNotLoaded)
{
}

FLACPlayer::~FLACPlayer()
{
    Close();
}

void FLACPlayer::PostOpen()
{
    if (flac_track_->channels == 1)
    {
        is_stereo_ = false;
    }
    else
    {
        is_stereo_ = true;
    }

    // Loaded, but not playing

    status_ = kStopped;
}

bool FLACPlayer::StreamIntoBuffer(SoundData *buf)
{
    bool song_done = false;

    drflac_uint64 frames = drflac_read_pcm_frames_s16(flac_track_, kMusicBuffer, buf->data_);

    if (frames < kMusicBuffer)
        song_done = true;

    buf->length_ = frames;

    buf->frequency_ = flac_track_->sampleRate;

    if (song_done) /* EOF */
    {
        if (!looping_)
            return false;
        drflac_seek_to_pcm_frame(flac_track_, 0);
        return true;
    }

    return (true);
}

bool FLACPlayer::OpenMemory(uint8_t *data, int length)
{
    EPI_ASSERT(data);

    flac_track_ = drflac_open_memory(data, length, nullptr);

    if (!flac_track_)
    {
        LogWarning("PlayFLACMusic: Error opening song!\n");
        return false;
    }

    // data is only released when the player is closed
    flac_data_ = data;

    PostOpen();
    return true;
}

void FLACPlayer::Close()
{
    if (status_ == kNotLoaded)
        return;

    // Stop playback
    if (status_ != kStopped)
        Stop();

    drflac_close(flac_track_);
    delete[] flac_data_;

    // reset player gain
    music_player_gain = 1.0f;

    status_ = kNotLoaded;
}

void FLACPlayer::Pause()
{
    if (status_ != kPlaying)
        return;

    status_ = kPaused;
}

void FLACPlayer::Resume()
{
    if (status_ != kPaused)
        return;

    status_ = kPlaying;
}

void FLACPlayer::Play(bool loop)
{
    if (status_ != kNotLoaded && status_ != kStopped)
        return;

    status_  = kPlaying;
    looping_ = loop;

    // Set individual player type gain
    music_player_gain = 0.6f;

    // Load up initial buffer data
    Ticker();
}

void FLACPlayer::Stop()
{
    if (status_ != kPlaying && status_ != kPaused)
        return;

    SoundQueueStop();

    status_ = kStopped;
}

void FLACPlayer::Ticker()
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
                SoundQueueAddBuffer(buf, buf->frequency_);
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

AbstractMusicPlayer *PlayFLACMusic(uint8_t *data, int length, bool looping)
{
    FLACPlayer *player = new FLACPlayer();

    if (!player->OpenMemory(data, length))
    {
        delete[] data;
        delete player;
        return nullptr;
    }

    // data is freed when Close() is called on the player; must be retained
    // until then

    player->Play(looping);

    return player;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
