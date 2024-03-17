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

// clang-format off
#define DR_FLAC_NO_CRC      1
#define DR_FLAC_NO_WIN32_IO 1
#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"
// clang-format on
#include "ddf_playlist.h"
#include "epi.h"
#include "epi_endian.h"
#include "epi_file.h"
#include "epi_filesystem.h"
#include "s_blit.h"
#include "s_cache.h"
#include "s_music.h"
#include "snd_gather.h"
#include "w_wad.h"

#define FLAC_FRAMES 1024

extern bool sound_device_stereo; // FIXME: encapsulation
extern int  sound_device_frequency;

class FlacPlayer : public AbstractMusicPlayer
{
  public:
    FlacPlayer();
    ~FlacPlayer();

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

    drflac *flac_track_; // I had to make it rhyme

    uint8_t *flac_data_; // Passed in from s_music; must be deleted on close

    int16_t *mono_buffer_;

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

FlacPlayer::FlacPlayer() : status_(kNotLoaded)
{
    mono_buffer_ = new int16_t[FLAC_FRAMES * 2];
}

FlacPlayer::~FlacPlayer()
{
    Close();

    if (mono_buffer_)
        delete[] mono_buffer_;
}

void FlacPlayer::PostOpen()
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

bool FlacPlayer::StreamIntoBuffer(SoundData *buf)
{
    int16_t *data_buf;

    bool song_done = false;

    if (!sound_device_stereo)
        data_buf = mono_buffer_;
    else
        data_buf = buf->data_left_;

    drflac_uint64 frames = drflac_read_pcm_frames_s16(flac_track_, FLAC_FRAMES, data_buf);

    if (frames < FLAC_FRAMES)
        song_done = true;

    buf->length_ = frames;

    buf->frequency_ = flac_track_->sampleRate;

    if (!sound_device_stereo)
        ConvertToMono(buf->data_left_, mono_buffer_, buf->length_);

    if (song_done) /* EOF */
    {
        if (!looping_)
            return false;
        drflac_seek_to_pcm_frame(flac_track_, 0);
        return true;
    }

    return (true);
}

bool FlacPlayer::OpenMemory(uint8_t *data, int length)
{
    EPI_ASSERT(data);

    flac_track_ = drflac_open_memory(data, length, nullptr);

    if (!flac_track_)
    {
        LogWarning("PlayFlacMusic: Error opening song!\n");
        return false;
    }

    // data is only released when the player is closed
    flac_data_ = data;

    PostOpen();
    return true;
}

void FlacPlayer::Close()
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

void FlacPlayer::Pause()
{
    if (status_ != kPlaying)
        return;

    status_ = kPaused;
}

void FlacPlayer::Resume()
{
    if (status_ != kPaused)
        return;

    status_ = kPlaying;
}

void FlacPlayer::Play(bool loop)
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

void FlacPlayer::Stop()
{
    if (status_ != kPlaying && status_ != kPaused)
        return;

    SoundQueueStop();

    status_ = kStopped;
}

void FlacPlayer::Ticker()
{
    while (status_ == kPlaying && !pc_speaker_mode)
    {
        SoundData *buf = SoundQueueGetFreeBuffer(FLAC_FRAMES, (sound_device_stereo) ? kMixInterleaved : kMixMono);

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

AbstractMusicPlayer *PlayFlacMusic(uint8_t *data, int length, bool looping)
{
    FlacPlayer *player = new FlacPlayer();

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
