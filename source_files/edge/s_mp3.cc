//----------------------------------------------------------------------------
//  EDGE MP3 Music Player
//----------------------------------------------------------------------------
//
//  Copyright (c) 2021-2024 The EDGE Team.
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

#include "s_mp3.h"

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

class MP3Player : public AbstractMusicPlayer
{
  public:
    MP3Player();
    ~MP3Player();

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

    ma_decoder mp3_decoder_;
    ma_sound   mp3_stream_;
    uint8_t    *mp3_data_;

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

MP3Player::MP3Player() : status_(kNotLoaded), mp3_data_(nullptr)
{
    EPI_CLEAR_MEMORY(&mp3_decoder_, ma_decoder, 1);
    EPI_CLEAR_MEMORY(&mp3_stream_, ma_sound, 1);
}

MP3Player::~MP3Player()
{
    Close();
}

bool MP3Player::OpenMemory(uint8_t *data, int length)
{
    if (status_ != kNotLoaded)
        Close();

    ma_decoder_config decode_config = ma_decoder_config_init_default();
    decode_config.format = ma_format_f32;

    if (ma_decoder_init_memory(data, length, &decode_config, &mp3_decoder_) != MA_SUCCESS)
    {
        LogWarning("Failed to load MP3 music (corrupt ogg?)\n");
        return false;
    }

    if (ma_sound_init_from_data_source(&music_engine, &mp3_decoder_, MA_SOUND_FLAG_STREAM|MA_SOUND_FLAG_NO_SPATIALIZATION, NULL, &mp3_stream_) != MA_SUCCESS)
    {
        ma_decoder_uninit(&mp3_decoder_);
        LogWarning("Failed to load OGG music (corrupt ogg?)\n");
        return false;
    }

    mp3_data_ = data;

    // Loaded, but not playing
    status_ = kStopped;

    return true;
}

void MP3Player::Close()
{
    if (status_ == kNotLoaded)
        return;

    // Stop playback
    Stop();

    ma_sound_uninit(&mp3_stream_);

    ma_decoder_uninit(&mp3_decoder_);

    delete[] mp3_data_;

    status_ = kNotLoaded;
}

void MP3Player::Pause()
{
    if (status_ != kPlaying)
        return;

    ma_sound_stop(&mp3_stream_);

    status_ = kPaused;
}

void MP3Player::Resume()
{
    if (status_ != kPaused)
        return;

    ma_sound_start(&mp3_stream_);

    status_ = kPlaying;
}

void MP3Player::Play(bool loop)
{
    if (status_ != kNotLoaded && status_ != kStopped)
        return;

    looping_ = loop;

    ma_sound_set_looping(&mp3_stream_, looping_ ? MA_TRUE : MA_FALSE);

    // Let 'er rip (maybe)
    if (playing_movie)
        status_ = kPaused;
    else
    {
        status_  = kPlaying;
        ma_sound_start(&mp3_stream_);
    }
}

void MP3Player::Stop()
{
    if (status_ != kPlaying && status_ != kPaused)
        return;

    ma_sound_stop(&mp3_stream_);

    ma_decoder_seek_to_pcm_frame(&mp3_decoder_, 0);

    status_ = kStopped;
}

void MP3Player::Ticker()
{
    ma_engine_set_volume(&music_engine, music_volume.f_ * 0.25f);

    if (status_ == kPlaying)
    {
        if (pc_speaker_mode)
            Stop();
        if (ma_sound_at_end(&mp3_stream_)) // This should only be true if finished and not set to looping
            Stop();
    }
}

//----------------------------------------------------------------------------

AbstractMusicPlayer *PlayMP3Music(uint8_t *data, int length, bool looping)
{
    MP3Player *player = new MP3Player();

    if (!player->OpenMemory(data, length))
    {
        delete[] data;
        delete player;
        return nullptr;
    }

    player->Play(looping);

    return player;
}

bool LoadMP3Sound(SoundData *buf, const uint8_t *data, int length)
{
    ma_decoder_config decode_config = ma_decoder_config_init_default();
    decode_config.format = ma_format_f32;
    ma_decoder decode;

    //ma_decoder_init_memory(const void* pData, size_t dataSize, const ma_decoder_config* pConfig, ma_decoder* pDecoder);

    if (ma_decoder_init_memory(data, length, &decode_config, &decode) != MA_SUCCESS)
    {
        LogWarning("Failed to load MP3 sound (corrupt mp3?)\n");
        return false;
    }

    if (decode.outputChannels > 2)
    {
        LogWarning("MP3 SFX Loader: too many channels: %d\n", decode.outputChannels);
        ma_decoder_uninit(&decode);
        return false;
    }

    ma_uint64 frame_count = 0;

    if (ma_decoder_get_length_in_pcm_frames(&decode, &frame_count) != MA_SUCCESS)
    {
        LogWarning("MP3 SFX Loader: no samples!\n");
        ma_decoder_uninit(&decode);
        return false;
    }

    LogDebug("MP3 SFX Loader: freq %d Hz, %d channels\n", decode.outputSampleRate, decode.outputChannels);

    bool is_stereo = (decode.outputChannels > 1);

    buf->frequency_ = decode.outputSampleRate;

    SoundGatherer gather;

    float *buffer = gather.MakeChunk(frame_count, is_stereo);

    ma_uint64 frames_read = 0;

    if (ma_decoder_read_pcm_frames(&decode, buffer, frame_count, &frames_read) != MA_SUCCESS)
    {
        LogWarning("MP3 SFX Loader: failure loading samples!\n");
        gather.DiscardChunk();
        ma_decoder_uninit(&decode);
        return false;
    }

    gather.CommitChunk(frames_read);

    if (!gather.Finalise(buf))
        LogWarning("MP3 SFX Loader: no samples!\n");

    ma_decoder_uninit(&decode);

    return true;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
