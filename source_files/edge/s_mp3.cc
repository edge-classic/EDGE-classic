//----------------------------------------------------------------------------
//  EDGE MP3 Music Player
//----------------------------------------------------------------------------
//
//  Copyright (c) 2004-2009  The EDGE Team.
//  Adapted from the EDGE Ogg Player in 2021 - Dashodanger
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//----------------------------------------------------------------------------

#include "i_defs.h"

#include "endianess.h"
#include "file.h"
#include "filesystem.h"
#include "sound_gather.h"

#include "playlist.h"

#include "s_cache.h"
#include "s_blit.h"
#include "s_music.h"
#include "s_mp3.h"
#include "w_wad.h"

#define DR_MP3_NO_STDIO
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

extern bool dev_stereo;  // FIXME: encapsulation

class mp3player_c : public abstract_music_c
{
public:
	 mp3player_c();
	~mp3player_c();

private:
	enum status_e
	{
		NOT_LOADED, PLAYING, PAUSED, STOPPED
	};

	int status;

	bool looping;
	bool is_stereo;

	byte *mp3_data = nullptr;
	drmp3 *mp3_dec = nullptr;

	s16_t *mono_buffer;

public:
	bool OpenMemory(byte *data, int length);

	virtual void Close(void);

	virtual void Play(bool loop);
	virtual void Stop(void);

	virtual void Pause(void);
	virtual void Resume(void);

	virtual void Ticker(void);
	virtual void Volume(float gain);

private:
	void PostOpenInit(void);

	bool StreamIntoBuffer(epi::sound_data_c *buf);
};

//----------------------------------------------------------------------------

mp3player_c::mp3player_c() : status(NOT_LOADED)
{
	mono_buffer = new s16_t[DRMP3_MAX_SAMPLES_PER_FRAME * 2];
}

mp3player_c::~mp3player_c()
{
	Close();

	if (mono_buffer)
		delete[] mono_buffer;
}

void mp3player_c::PostOpenInit()
{
    if (mp3_dec->channels == 1) {
        is_stereo = false;
	} else {
        is_stereo = true;
	}
    
	// Loaded, but not playing
	status = STOPPED;
}


static void ConvertToMono(s16_t *dest, const s16_t *src, int len)
{
	const s16_t *s_end = src + len*2;

	for (; src < s_end; src += 2)
	{
		// compute average of samples
		*dest++ = ( (int)src[0] + (int)src[1] ) >> 1;
	}
}


bool mp3player_c::StreamIntoBuffer(epi::sound_data_c *buf)
{
	s16_t *data_buf;

	if (is_stereo && !dev_stereo)
		data_buf = mono_buffer;
	else
		data_buf = buf->data_L;

	int got_size = drmp3_read_pcm_frames_s16(mp3_dec, DRMP3_MAX_SAMPLES_PER_FRAME, data_buf);

	if (mp3_dec->atEnd)  /* EOF */
	{
		if (! looping)
			return false;
		mp3_dec->memory.currentReadPos = 0;
		return true;
	}

	if (got_size < 0)  /* ERROR */
	{
		I_Debugf("[mp3player_c::StreamIntoBuffer] Failed\n");
		return false;
	}

	buf->length = got_size;

	if (is_stereo && !dev_stereo)
		ConvertToMono(buf->data_L, mono_buffer, got_size);

    return (true);
}


void mp3player_c::Volume(float gain)
{
	// not needed, music volume is handled in s_blit.cc
	// (see mix_channel_c::ComputeMusicVolume).
}


bool mp3player_c::OpenMemory(byte *data, int length)
{
	if (status != NOT_LOADED)
		Close();

	mp3_dec = new drmp3;

    if (!drmp3_init_memory(mp3_dec, data, length, nullptr))
    {
		I_Warning("mp3player_c: Could not open MP3 file.\n");
		delete mp3_dec;
		return false;
    }

	if (mp3_dec->channels > 2)
	{
		I_Warning("mp3player_c: MP3 has too many channels: %d\n", mp3_dec->channels);
		drmp3_uninit(mp3_dec);
		return false;
	}

	PostOpenInit();
	return true;
}


void mp3player_c::Close()
{
	if (status == NOT_LOADED)
		return;

	// Stop playback
	if (status != STOPPED)
		Stop();

	drmp3_uninit(mp3_dec);
	delete mp3_dec;
	mp3_dec = nullptr;

	delete[] mp3_data;
	mp3_data = nullptr;

	status = NOT_LOADED;
}


void mp3player_c::Pause()
{
	if (status != PLAYING)
		return;

	status = PAUSED;
}


void mp3player_c::Resume()
{
	if (status != PAUSED)
		return;

	status = PLAYING;
}


void mp3player_c::Play(bool loop)
{
    if (status != NOT_LOADED && status != STOPPED) return;

	status = PLAYING;
	looping = loop;

	// Load up initial buffer data
	Ticker();
}


void mp3player_c::Stop()
{
	if (status != PLAYING && status != PAUSED)
		return;

	S_QueueStop();

	status = STOPPED;
}


void mp3player_c::Ticker()
{
	while (status == PLAYING && !var_pc_speaker_mode)
	{
		epi::sound_data_c *buf = S_QueueGetFreeBuffer(DRMP3_MAX_SAMPLES_PER_FRAME, 
				(is_stereo && dev_stereo) ? epi::SBUF_Interleaved : epi::SBUF_Mono);

		if (! buf)
			break;

		if (StreamIntoBuffer(buf))
		{
			if (buf->length > 0)
				S_QueueAddBuffer(buf, mp3_dec->sampleRate);
			else
				S_QueueReturnBuffer(buf);
		}
		else
		{
			// finished playing
			S_QueueReturnBuffer(buf);
			Stop();
		}
	}
}


//----------------------------------------------------------------------------

abstract_music_c * S_PlayMP3Music(byte *data, int length, float volume, bool looping)
{
	mp3player_c *player = new mp3player_c();

	if (! player->OpenMemory(data, length))
	{
		delete[] data;
		delete player;
		return NULL;
	}

	player->Volume(volume);
	player->Play(looping);

	return player;
}


bool S_LoadMP3Sound(epi::sound_data_c *buf, const byte *data, int length)
{
	drmp3_config info;

	buf->Free(); // In case something's already there

	buf->data_L = drmp3_open_memory_and_read_pcm_frames_s16(data, length, &info, nullptr, nullptr);

    if (!buf->data_L)
    {
		I_Warning("Failed to load MP3 sound (corrupt mp3?)\n");
		return false;
    }

	if (info.channels > 2)
	{
		I_Warning("MP3 SFX Loader: too many channels: %d\n", info.channels);
		buf->Free();
		return false;
	}

	I_Debugf("MP3 SFX Loader: freq %d Hz, %d channels\n",
		info.sampleRate, info.channels);

	buf->freq = info.sampleRate;
	if (info.channels == 2)
	{
		buf->mode = epi::SBUF_Interleaved;
		buf->data_R = nullptr;
	}
	else
	{
		buf->mode = epi::SBUF_Mono;
		buf->data_R = buf->data_L;
	}

	delete[] data;

	return true;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
