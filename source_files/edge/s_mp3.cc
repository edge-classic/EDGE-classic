//----------------------------------------------------------------------------
//  EDGE MP3 Music Player
//----------------------------------------------------------------------------
// 
//  Copyright (c) 2021-2023  The EDGE Team.
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

	if (got_size == 0)  /* EOF */
	{
		if (! looping)
			return false;
		drmp3_seek_to_pcm_frame(mp3_dec, 0);
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

	// reset player gain
	mus_player_gain = 1.0f;

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

	// Set individual player type gain
	mus_player_gain = 0.3f;

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

abstract_music_c * S_PlayMP3Music(byte *data, int length, bool looping)
{
	mp3player_c *player = new mp3player_c();

	if (! player->OpenMemory(data, length))
	{
		delete[] data;
		delete player;
		return NULL;
	}

	player->Play(looping);

	return player;
}


bool S_LoadMP3Sound(epi::sound_data_c *buf, const byte *data, int length)
{
	drmp3 mp3;

	if (!drmp3_init_memory(&mp3, data, length, nullptr))
	{
		I_Warning("Failed to load MP3 sound (corrupt mp3?)\n");
		return false;
	}

	if (mp3.channels > 2)
	{
		I_Warning("MP3 SFX Loader: too many channels: %d\n", mp3.channels);
		drmp3_uninit(&mp3);
		return false;
	}

	drmp3_uint64 framecount = drmp3_get_pcm_frame_count(&mp3);

	if (framecount <= 0) // I think the initial loading would fail if this were the case, but just as a sanity check - Dasho
	{
		I_Warning("MP3 SFX Loader: no samples!\n");
		drmp3_uninit(&mp3);
		return false;
	}

	I_Debugf("MP3 SFX Loader: freq %d Hz, %d channels\n",
		mp3.sampleRate, mp3.channels);

	bool is_stereo = (mp3.channels > 1);

	buf->freq = mp3.sampleRate;

	epi::sound_gather_c gather;

	s16_t *buffer = gather.MakeChunk(framecount, is_stereo);

	gather.CommitChunk(drmp3_read_pcm_frames_s16(&mp3, framecount, buffer));

	if (! gather.Finalise(buf, is_stereo))
		I_Warning("MP3 SFX Loader: no samples!\n");

	drmp3_uninit(&mp3);

	return true;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
