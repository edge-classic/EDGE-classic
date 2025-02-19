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

static ma_decoder mp3_decoder;
static ma_sound   mp3_stream;
class MP3Player : public AbstractMusicPlayer
{
  public:
    MP3Player();
    ~MP3Player() override;

  private:
    const uint8_t *mp3_data_;

  public:
    bool OpenMemory(const uint8_t *data, int length);

    void Close(void) override;

    void Play(bool loop) override;
    void Stop(void) override;

    void Pause(void) override;
    void Resume(void) override;

    void Ticker(void) override;
};

//----------------------------------------------------------------------------

MP3Player::MP3Player() : mp3_data_(nullptr)
{
    status_ = kNotLoaded;
}

MP3Player::~MP3Player()
{
    Close();
}

bool MP3Player::OpenMemory(const uint8_t *data, int length)
{
    if (status_ != kNotLoaded)
        Close();

    ma_decoder_config decode_config = ma_decoder_config_init_default();
    decode_config.format            = ma_format_f32;
    decode_config.encodingFormat    = ma_encoding_format_mp3;

    if (ma_decoder_init_memory(data, length, &decode_config, &mp3_decoder) != MA_SUCCESS)
    {
        LogWarning("Failed to load MP3 music (corrupt ogg?)\n");
        return false;
    }

    if (ma_sound_init_from_data_source(&music_engine, &mp3_decoder,
                                       MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION, NULL,
                                       &mp3_stream) != MA_SUCCESS)
    {
        ma_decoder_uninit(&mp3_decoder);
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

    ma_sound_uninit(&mp3_stream);

    ma_decoder_uninit(&mp3_decoder);

    delete[] mp3_data_;

    status_ = kNotLoaded;
}

void MP3Player::Pause()
{
    if (status_ != kPlaying)
        return;

    ma_sound_stop(&mp3_stream);

    status_ = kPaused;
}

void MP3Player::Resume()
{
    if (status_ != kPaused)
        return;

    ma_sound_start(&mp3_stream);

    status_ = kPlaying;
}

void MP3Player::Play(bool loop)
{
    if (status_ != kNotLoaded && status_ != kStopped)
        return;

    looping_ = loop;

    ma_sound_set_looping(&mp3_stream, looping_ ? MA_TRUE : MA_FALSE);

    // Let 'er rip (maybe)
    if (playing_movie)
        status_ = kPaused;
    else
    {
        status_ = kPlaying;
        ma_sound_start(&mp3_stream);
    }
}

void MP3Player::Stop()
{
    if (status_ != kPlaying && status_ != kPaused)
        return;

    ma_sound_set_volume(&mp3_stream, 0);
    ma_sound_stop(&mp3_stream);

    status_ = kStopped;
}

void MP3Player::Ticker()
{
    if (status_ == kPlaying)
    {
        ma_engine_set_volume(&music_engine, music_volume.f_ * 0.25f);

        if (pc_speaker_mode)
            Stop();
        if (ma_sound_at_end(&mp3_stream)) // This should only be true if finished and not set to looping
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
    decode_config.format            = ma_format_f32;
    decode_config.encodingFormat    = ma_encoding_format_mp3;

    ma_decoder decode;

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
