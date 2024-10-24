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
#include "dr_mp3.h"
#include "epi.h"
#include "epi_endian.h"
#include "epi_file.h"
#include "epi_filesystem.h"
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

    uint8_t *mp3_data_    = nullptr;
    drmp3   *mp3_decoder_ = nullptr;

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

MP3Player::MP3Player() : status_(kNotLoaded)
{
}

MP3Player::~MP3Player()
{
    Close();
}

void MP3Player::PostOpen()
{
    // Loaded, but not playing
    status_ = kStopped;
}

bool MP3Player::StreamIntoBuffer(SoundData *buf)
{
    int got_size = drmp3_read_pcm_frames_s16(mp3_decoder_, kMusicBuffer, buf->data_);

    if (got_size == 0) /* EOF */
    {
        if (!looping_)
            return false;
        drmp3_seek_to_pcm_frame(mp3_decoder_, 0);
        return true;
    }

    if (got_size < 0) /* ERROR */
    {
        LogDebug("[mp3player_c::StreamIntoBuffer] Failed\n");
        return false;
    }

    buf->length_ = got_size;

    return true;
}

bool MP3Player::OpenMemory(uint8_t *data, int length)
{
    if (status_ != kNotLoaded)
        Close();

    mp3_decoder_ = new drmp3;

    if (!drmp3_init_memory(mp3_decoder_, data, length, nullptr))
    {
        LogWarning("mp3player_c: Could not open MP3 file.\n");
        delete mp3_decoder_;
        return false;
    }

    if (mp3_decoder_->channels > 2)
    {
        LogWarning("mp3player_c: MP3 has too many channels: %d\n", mp3_decoder_->channels);
        drmp3_uninit(mp3_decoder_);
        return false;
    }

    // Force stereo for MP3 music if not already the case
    mp3_decoder_->channels = 2;
    mp3_data_ = data;
    PostOpen();
    return true;
}

void MP3Player::Close()
{
    if (status_ == kNotLoaded)
        return;

    // Stop playback
    if (status_ != kStopped)
        Stop();

    drmp3_uninit(mp3_decoder_);
    delete mp3_decoder_;
    mp3_decoder_ = nullptr;

    delete[] mp3_data_;
    mp3_data_ = nullptr;

    // reset player gain
    music_player_gain = 1.0f;

    status_ = kNotLoaded;
}

void MP3Player::Pause()
{
    if (status_ != kPlaying)
        return;

    status_ = kPaused;
}

void MP3Player::Resume()
{
    if (status_ != kPaused)
        return;

    status_ = kPlaying;
}

void MP3Player::Play(bool loop)
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

void MP3Player::Stop()
{
    if (status_ != kPlaying && status_ != kPaused)
        return;

    SoundQueueStop();

    status_ = kStopped;
}

void MP3Player::Ticker()
{
    while (status_ == kPlaying && !pc_speaker_mode)
    {
        SoundData *buf =
            SoundQueueGetFreeBuffer(kMusicBuffer);

        if (!buf)
            break;

        if (StreamIntoBuffer(buf))
        {
            if (buf->length_ > 0)
                SoundQueueAddBuffer(buf, mp3_decoder_->sampleRate);
            else
                SoundQueueReturnBuffer(buf);
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
    drmp3 mp3;

    if (!drmp3_init_memory(&mp3, data, length, nullptr))
    {
        LogWarning("Failed to load MP3 sound (corrupt mp3?)\n");
        return false;
    }

    if (mp3.channels > 2)
    {
        LogWarning("MP3 SFX Loader: too many channels: %d\n", mp3.channels);
        drmp3_uninit(&mp3);
        return false;
    }

    drmp3_uint64 framecount = drmp3_get_pcm_frame_count(&mp3);

    if (framecount <= 0) // I think the initial loading would fail if this were
                         // the case, but just as a sanity check - Dasho
    {
        LogWarning("MP3 SFX Loader: no samples!\n");
        drmp3_uninit(&mp3);
        return false;
    }

    LogDebug("MP3 SFX Loader: freq %d Hz, %d channels\n", mp3.sampleRate, mp3.channels);

    bool is_stereo = (mp3.channels > 1);

    buf->frequency_ = mp3.sampleRate;

    SoundGatherer gather;

    int16_t *buffer = gather.MakeChunk(framecount, is_stereo);

    gather.CommitChunk(drmp3_read_pcm_frames_s16(&mp3, framecount, buffer));

    if (!gather.Finalise(buf))
        LogWarning("MP3 SFX Loader: no samples!\n");

    drmp3_uninit(&mp3);

    return true;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
