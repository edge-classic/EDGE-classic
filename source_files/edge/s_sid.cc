//----------------------------------------------------------------------------
//  EDGE TinySID Music Player
//----------------------------------------------------------------------------
// 
//  Copyright (c) 2022 - The EDGE-Classic Community
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
#include "s_sid.h"
#include "w_wad.h"

#include "sidplayer.h"

#define SID_BUFFER 96000 / 50
extern bool dev_stereo;  // FIXME: encapsulation
extern int  dev_freq;

class sidplayer_c : public abstract_music_c
{
public:
	 sidplayer_c();
	~sidplayer_c();
private:

	enum status_e
	{
		NOT_LOADED, PLAYING, PAUSED, STOPPED
	};
	
	int status;
	bool looping;

	s16_t *mono_buffer;

	byte *sid_data;
	int sid_length;

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

sidplayer_c::sidplayer_c() : status(NOT_LOADED)
{
	mono_buffer = new s16_t[SID_BUFFER * 2];
}

sidplayer_c::~sidplayer_c()
{
	Close();

	if (mono_buffer)
		delete[] mono_buffer;

	if (sid_data)
		delete[] sid_data;
}

void sidplayer_c::PostOpenInit()
{   
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

bool sidplayer_c::StreamIntoBuffer(epi::sound_data_c *buf)
{
	s16_t *data_buf;

	if (!dev_stereo)
		data_buf = mono_buffer;
	else
		data_buf = buf->data_L;

	if (computeAudioSamples() == -1)
	{
		if (! looping)
			return false;
		else
		{
			loadSidFile(0, sid_data, sid_length, dev_freq, NULL, NULL, NULL, NULL);
			sid_playTune(0, 0);
			computeAudioSamples();
		}
	}

	buf->length = getSoundBufferLen();

	memcpy(data_buf, getSoundBuffer(), buf->length * sizeof(s16_t) * 2);

	if (!dev_stereo)
		ConvertToMono(buf->data_L, mono_buffer, buf->length);

    return true;
}


void sidplayer_c::Volume(float gain)
{
	// not needed, music volume is handled in s_blit.cc
	// (see mix_channel_c::ComputeMusicVolume).
}


bool sidplayer_c::OpenMemory(byte *data, int length)
{
	SYS_ASSERT(data);

	if (status != NOT_LOADED)
		Close();

    if (loadSidFile(0, data, length, dev_freq, NULL, NULL, NULL, NULL) != 0)
    {
		I_Warning("[sidplayer_c::Open](DataLump) Failed\n");
		return false;
    }

	// Need to keep the song in memory for SID restarts
	sid_data   = data;
	sid_length = length;

	PostOpenInit();
	return true;
}


void sidplayer_c::Close()
{
	if (status == NOT_LOADED)
		return;

	// Stop playback
	if (status != STOPPED)
		Stop();
		
	status = NOT_LOADED;
}


void sidplayer_c::Pause()
{
	if (status != PLAYING)
		return;

	status = PAUSED;
}


void sidplayer_c::Resume()
{
	if (status != PAUSED)
		return;

	status = PLAYING;
}


void sidplayer_c::Play(bool loop)
{
    if (status != NOT_LOADED && status != STOPPED) return;

	status = PLAYING;
	looping = loop;

	sid_playTune(0, 0);

	// Load up initial buffer data
	Ticker();
}


void sidplayer_c::Stop()
{
	if (status != PLAYING && status != PAUSED)
		return;

	S_QueueStop();

	status = STOPPED;
}


void sidplayer_c::Ticker()
{
	while (status == PLAYING)
	{
		epi::sound_data_c *buf = S_QueueGetFreeBuffer(SID_BUFFER, 
				(dev_stereo) ? epi::SBUF_Interleaved : epi::SBUF_Mono);

		if (! buf)
			break;

		if (StreamIntoBuffer(buf))
		{
			if (buf->length > 0)
			{
				S_QueueAddBuffer(buf, dev_freq);
			}
			else
			{
				S_QueueReturnBuffer(buf);
			}
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

abstract_music_c * S_PlaySIDMusic(byte *data, int length, float volume, bool looping)
{
	sidplayer_c *player = new sidplayer_c();

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

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
