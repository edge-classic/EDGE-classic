//----------------------------------------------------------------------------
//  EDGE OGG Music Player
//----------------------------------------------------------------------------
//
//  Copyright (c) 2004-2024 The EDGE Team.
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

#include "s_ogg.h"

#include "epi.h"
#include "epi_endian.h"
#include "epi_filesystem.h"
#include "s_blit.h"
#include "s_cache.h"
#include "s_music.h"
#include "snd_gather.h"
// clang-format off
#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_NO_STDIO
#include "stb_vorbis.h"
// clang-format on

class OGGPlayer : public AbstractMusicPlayer
{
  public:
    OGGPlayer();
    ~OGGPlayer();

  private:
    enum Status
    {
        kNotLoaded,
        kPlaying,
        kPaused,
        kStopped
    };

    int status_;

    bool looping_;
    bool is_stereo_;

    stb_vorbis *ogg_decoder_;

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

OGGPlayer::OGGPlayer() : status_(kNotLoaded), ogg_decoder_(nullptr)
{
}

OGGPlayer::~OGGPlayer()
{
    Close();
}

void OGGPlayer::PostOpen()
{
    // Loaded, but not playing
    status_ = kStopped;
}

bool OGGPlayer::StreamIntoBuffer(SoundData *buf)
{
    int got_size = stb_vorbis_get_samples_short_interleaved(ogg_decoder_, 2, buf->data_, kMusicBuffer * 2);

    if (got_size == 0) /* EOF */
    {
        if (!looping_)
            return false;
        stb_vorbis_seek_start(ogg_decoder_);
        return true;
    }

    if (got_size < 0) /* ERROR */
    {
        LogDebug("[oggplayer_c::StreamIntoBuffer] Failed\n");
        return false;
    }

    buf->length_ = got_size;

    return true;
}

bool OGGPlayer::OpenMemory(uint8_t *data, int length)
{
    if (status_ != kNotLoaded)
        Close();

    int ogg_error = 0;
    ogg_decoder_ = stb_vorbis_open_memory(data, length, &ogg_error, nullptr);

    if (ogg_error || !ogg_decoder_)
    {
        LogWarning("OGGPlayer: unable to load file\n");
        if (ogg_decoder_)
        {
            stb_vorbis_close(ogg_decoder_);
            ogg_decoder_ = nullptr;
        }
        return false;
    }

    if (ogg_decoder_->channels > 2 || ogg_decoder_->channels < 1)
    {
        LogWarning("OGGPlayer: unsupported number of channels: %d\n", ogg_decoder_->channels);
        stb_vorbis_close(ogg_decoder_);
        ogg_decoder_ = nullptr;
        return false;
    }

    PostOpen();
    return true;
}

void OGGPlayer::Close()
{
    if (status_ == kNotLoaded)
        return;

    // Stop playback
    Stop();

    if (ogg_decoder_)
    {
        stb_vorbis_close(ogg_decoder_);
        ogg_decoder_ = nullptr;
    }

    // Reset player gain
    music_player_gain = 1.0f;

    status_ = kNotLoaded;
}

void OGGPlayer::Pause()
{
    if (status_ != kPlaying)
        return;

    status_ = kPaused;
}

void OGGPlayer::Resume()
{
    if (status_ != kPaused)
        return;

    status_ = kPlaying;
}

void OGGPlayer::Play(bool loop)
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

void OGGPlayer::Stop()
{
    if (status_ != kPlaying && status_ != kPaused)
        return;

    SoundQueueStop();

    status_ = kStopped;
}

void OGGPlayer::Ticker()
{
    while (status_ == kPlaying)
    {
        SoundData *buf =
            SoundQueueGetFreeBuffer(kMusicBuffer);

        if (!buf)
            break;

        if (StreamIntoBuffer(buf))
        {
            SoundQueueAddBuffer(buf, ogg_decoder_->sample_rate);
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

AbstractMusicPlayer *PlayOGGMusic(uint8_t *data, int length, bool looping)
{
    OGGPlayer *player = new OGGPlayer();

    if (!player->OpenMemory(data, length))
    {
        delete[] data;
        delete player;
        return nullptr;
    }

    player->Play(looping);

    return player;
}

bool LoadOGGSound(SoundData *buf, const uint8_t *data, int length)
{
    int ogg_error = 0;
    stb_vorbis *ogg = stb_vorbis_open_memory(data, length, &ogg_error, nullptr);

    if (ogg_error || !ogg)
    {
        LogWarning("OGG SFX Loader: unable to load file\n");
        if (ogg)
            stb_vorbis_close(ogg);
        return false;
    }

    if (ogg->channels > 2 || ogg->channels < 1)
    {
        LogWarning("OGG SFX Loader: unsupported number of channels: %d\n", ogg->channels);
        stb_vorbis_close(ogg);
        return false;
    }

    LogDebug("OGG SFX Loader: freq %d Hz, %d channels\n", ogg->sample_rate, ogg->channels);

    buf->frequency_ = ogg->sample_rate;

    uint32_t total_samples = stb_vorbis_stream_length_in_samples(ogg);

    SoundGatherer gather;

    int16_t *buffer = gather.MakeChunk(total_samples, true);

    gather.CommitChunk(stb_vorbis_get_samples_short_interleaved(ogg, 2, buffer, total_samples * 2));

    if (!gather.Finalise(buf))
        FatalError("OGG SFX Loader: no samples!\n");

    stb_vorbis_close(ogg);

    return true;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
