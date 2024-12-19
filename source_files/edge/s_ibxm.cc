//----------------------------------------------------------------------------
//  EDGE IBXM (Tracker Module) Music Player
//----------------------------------------------------------------------------
//
//  Copyright (c) 2022-2024 - The EDGE Team.
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
#include "ibxm.h"
#include "s_blit.h"
#include "s_cache.h"
#include "s_music.h"
#include "snd_gather.h"
#include "w_wad.h"

extern int  sound_device_frequency;

bool CheckIBXMFormat (uint8_t *data, int length)
{
	ibxm_data mod_check;
	mod_check.buffer = (char *)data;
	mod_check.length = length;
	bool is_mod_music = false;
	// Check for MOD format
	switch( ibxm_data_u16be(&mod_check, 1082)) 
	{
		case 0x4b2e: /* M.K. */
		case 0x4b21: /* M!K! */
		case 0x5434: /* FLT4 */
		case 0x484e: /* xCHN */
		case 0x4348: /* xxCH */
			is_mod_music = true;
			break;
		default:
			break;
	}
	// Check for XM format
	if( ibxm_data_u16le(&mod_check, 58) == 0x0104 )
		is_mod_music = true;
	// Check for S3M format
	if( ibxm_data_u32le(&mod_check, 44) == 0x4d524353 )
		is_mod_music = true;
	return is_mod_music;
}

class IBXMPlayer : public AbstractMusicPlayer
{
  public:
    IBXMPlayer();
    ~IBXMPlayer();

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

    ibxm_module *ibxm_track_;
	ibxm_replay *ibxm_replayer_;
	ibxm_data *ibxm_raw_track_;
	int ibxm_buffer_length_;

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

IBXMPlayer::IBXMPlayer() : status_(kNotLoaded)
{
}

IBXMPlayer::~IBXMPlayer()
{
    Close();
}

void IBXMPlayer::PostOpen()
{
    // Loaded, but not playing
    ibxm_buffer_length_ = ibxm_calculate_mix_buf_len(sound_device_frequency);
    status_ = kStopped;
}

bool IBXMPlayer::StreamIntoBuffer(SoundData *buf)
{
    bool song_done = false;

    int got_size = ibxm_replay_get_audio(ibxm_replayer_, (int *)buf->data_, 0);

    if (got_size < 0) // ERROR
	{
		LogDebug("[ibxmplayer_c::StreamIntoBuffer] Failed\n");
		return false;
	}

    if (got_size == 0)
        song_done = true;

    buf->length_ = got_size * 2;

    if (song_done)  /* EOF */
	{
		if (!looping_)
			return false;
		ibxm_replay_set_sequence_pos(ibxm_replayer_, 0);
	}

    return true;
}

bool IBXMPlayer::OpenMemory(uint8_t *data, int length)
{
    EPI_ASSERT(data);

    ibxm_raw_track_ = (ibxm_data *)calloc(1, sizeof(ibxm_data));

    ibxm_raw_track_->length = length;
    ibxm_raw_track_->buffer = (char *)data;
    std::string load_error;
    load_error.resize(64);
    ibxm_track_ = ibxm_module_load(ibxm_raw_track_, load_error.data());

    if (!ibxm_track_)
    {
        LogWarning("IBXMPlayer: failure to load module: %s\n", load_error.c_str());
        delete[] data;
        free(ibxm_raw_track_);
        return false;
    }

    ibxm_replayer_ = ibxm_new_replay(ibxm_track_, sound_device_frequency / 2, 0);

    if (!ibxm_replayer_)
    {
        LogWarning("IBXMPlayer::OpenMemory failed!\n");
        ibxm_dispose_module(ibxm_track_);
        delete[] data;
        free(ibxm_raw_track_);
        return false;
    }

    PostOpen();
    return true;
}

void IBXMPlayer::Close()
{
    if (status_ == kNotLoaded)
        return;

    // Stop playback
    if (status_ != kStopped)
        Stop();

    ibxm_dispose_replay(ibxm_replayer_);
	ibxm_dispose_module(ibxm_track_);
	delete[] ibxm_raw_track_->buffer;
    free(ibxm_raw_track_);

    status_ = kNotLoaded;
}

void IBXMPlayer::Pause()
{
    if (status_ != kPlaying)
        return;

    status_ = kPaused;
}

void IBXMPlayer::Resume()
{
    if (status_ != kPaused)
        return;

    status_ = kPlaying;
}

void IBXMPlayer::Play(bool loop)
{
    if (status_ != kNotLoaded && status_ != kStopped)
        return;

    status_  = kPlaying;
    looping_ = loop;

    // Load up initial buffer data
    Ticker();
}

void IBXMPlayer::Stop()
{
    if (status_ != kPlaying && status_ != kPaused)
        return;

    SoundQueueStop();

    status_ = kStopped;
}

void IBXMPlayer::Ticker()
{
    while (status_ == kPlaying && !pc_speaker_mode && !playing_movie)
    {
        SoundData *buf = SoundQueueGetFreeBuffer(ibxm_buffer_length_);

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

AbstractMusicPlayer *PlayIBXMMusic(uint8_t *data, int length, bool looping)
{
    IBXMPlayer *player = new IBXMPlayer();

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
