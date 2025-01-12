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
#include "epi.h"
#include "epi_endian.h"
#include "epi_file.h"
#include "epi_filesystem.h"
#include "i_movie.h"
#include "i_sound.h"
#include "s_blit.h"
#include "s_cache.h"
#include "s_music.h"
#include "snd_gather.h"
#include "w_wad.h"

extern int sound_device_frequency;

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

    ma_decoder flac_decoder_;
    ma_sound   flac_stream_;
    uint8_t   *flac_data_;

  public:
    bool OpenMemory(uint8_t *data, int length);

    virtual void Close(void);

    virtual void Play(bool loop);
    virtual void Stop(void);

    virtual void Pause(void);
    virtual void Resume(void);

    virtual void Ticker(void);
};

//----------------------------------------------------------------------------

FLACPlayer::FLACPlayer() : status_(kNotLoaded), flac_data_(nullptr)
{
    EPI_CLEAR_MEMORY(&flac_decoder_, ma_decoder, 1);
    EPI_CLEAR_MEMORY(&flac_stream_, ma_sound, 1);
}

FLACPlayer::~FLACPlayer()
{
    Close();
}

bool FLACPlayer::OpenMemory(uint8_t *data, int length)
{
    if (status_ != kNotLoaded)
        Close();

    ma_decoder_config decode_config = ma_decoder_config_init_default();
    decode_config.format            = ma_format_f32;

    if (ma_decoder_init_memory(data, length, &decode_config, &flac_decoder_) != MA_SUCCESS)
    {
        LogWarning("Failed to load MP3 music (corrupt ogg?)\n");
        return false;
    }

    if (ma_sound_init_from_data_source(&music_engine, &flac_decoder_,
                                       MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION, NULL,
                                       &flac_stream_) != MA_SUCCESS)
    {
        ma_decoder_uninit(&flac_decoder_);
        LogWarning("Failed to load OGG music (corrupt ogg?)\n");
        return false;
    }

    flac_data_ = data;

    // Loaded, but not playing
    status_ = kStopped;

    return true;
}

void FLACPlayer::Close()
{
    if (status_ == kNotLoaded)
        return;

    // Stop playback
    Stop();

    ma_sound_uninit(&flac_stream_);

    ma_decoder_uninit(&flac_decoder_);

    delete[] flac_data_;

    status_ = kNotLoaded;
}

void FLACPlayer::Pause()
{
    if (status_ != kPlaying)
        return;

    ma_sound_stop(&flac_stream_);

    status_ = kPaused;
}

void FLACPlayer::Resume()
{
    if (status_ != kPaused)
        return;

    ma_sound_start(&flac_stream_);

    status_ = kPlaying;
}

void FLACPlayer::Play(bool loop)
{
    if (status_ != kNotLoaded && status_ != kStopped)
        return;

    looping_ = loop;

    ma_sound_set_looping(&flac_stream_, looping_ ? MA_TRUE : MA_FALSE);

    // Let 'er rip (maybe)
    if (playing_movie)
        status_ = kPaused;
    else
    {
        status_ = kPlaying;
        ma_sound_start(&flac_stream_);
    }
}

void FLACPlayer::Stop()
{
    if (status_ != kPlaying && status_ != kPaused)
        return;

    ma_sound_stop(&flac_stream_);

    ma_decoder_seek_to_pcm_frame(&flac_decoder_, 0);

    status_ = kStopped;
}

void FLACPlayer::Ticker()
{
    ma_engine_set_volume(&music_engine, music_volume.f_ * 0.25f);

    if (status_ == kPlaying)
    {
        if (pc_speaker_mode)
            Stop();
        if (ma_sound_at_end(&flac_stream_)) // This should only be true if finished and not set to looping
            Stop();
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
