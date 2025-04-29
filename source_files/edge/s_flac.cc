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

static ma_decoder flac_decoder;
static ma_sound   flac_stream;
class FLACPlayer : public AbstractMusicPlayer
{
  public:
    FLACPlayer();
    ~FLACPlayer() override;

  private:
    uint8_t *flac_data_;

  public:
    bool OpenMemory(uint8_t *data, int length);

    void Close(void) override;

    void Play(bool loop) override;
    void Stop(void) override;

    void Pause(void) override;
    void Resume(void) override;

    void Ticker(void) override;
};

//----------------------------------------------------------------------------

FLACPlayer::FLACPlayer() : flac_data_(nullptr)
{
    status_ = kNotLoaded;
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
    decode_config.encodingFormat    = ma_encoding_format_flac;

    if (ma_decoder_init_memory(data, length, &decode_config, &flac_decoder) != MA_SUCCESS)
    {
        LogWarning("Failed to load MP3 music (corrupt ogg?)\n");
        return false;
    }

    if (ma_sound_init_from_data_source(&sound_engine, &flac_decoder,
                                       MA_SOUND_FLAG_NO_PITCH | MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION,
                                       NULL, &flac_stream) != MA_SUCCESS)
    {
        ma_decoder_uninit(&flac_decoder);
        LogWarning("Failed to load OGG music (corrupt ogg?)\n");
        return false;
    }

    ma_node_attach_output_bus(&flac_stream, 0, &music_node, 0);

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

    ma_sound_uninit(&flac_stream);

    ma_decoder_uninit(&flac_decoder);

    delete[] flac_data_;

    status_ = kNotLoaded;
}

void FLACPlayer::Pause()
{
    if (status_ != kPlaying)
        return;

    ma_sound_stop(&flac_stream);

    status_ = kPaused;
}

void FLACPlayer::Resume()
{
    if (status_ != kPaused)
        return;

    ma_sound_start(&flac_stream);

    status_ = kPlaying;
}

void FLACPlayer::Play(bool loop)
{
    if (status_ != kNotLoaded && status_ != kStopped)
        return;

    looping_ = loop;

    ma_sound_set_looping(&flac_stream, looping_ ? MA_TRUE : MA_FALSE);

    // Let 'er rip (maybe)
    if (playing_movie)
        status_ = kPaused;
    else
    {
        status_ = kPlaying;
        ma_sound_start(&flac_stream);
    }
}

void FLACPlayer::Stop()
{
    if (status_ != kPlaying && status_ != kPaused)
        return;

    ma_sound_set_volume(&flac_stream, 0);
    ma_sound_stop(&flac_stream);

    status_ = kStopped;
}

void FLACPlayer::Ticker()
{
    if (status_ == kPlaying)
    {
        if (pc_speaker_mode)
            Stop();
        if (ma_sound_at_end(&flac_stream)) // This should only be true if finished and not set to looping
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
