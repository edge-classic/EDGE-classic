//----------------------------------------------------------------------------
//  EDGE XMP Music Player
//----------------------------------------------------------------------------
// 
//  Copyright (c) 2022 - The EDGE-Classic Team
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
#include "s_xmp.h"
#include "w_wad.h"

#include "xmp.h"

#define XMP_BUFFER 4096 // From XMP's site, one second of a song is roughly 50 frames - Dasho

extern bool dev_stereo;  // FIXME: encapsulation
extern int  dev_freq;

class xmpplayer_c : public abstract_music_c
{
public:
	 xmpplayer_c();
	~xmpplayer_c();
private:

	enum status_e
	{
		NOT_LOADED, PLAYING, PAUSED, STOPPED
	};
	
	int status;
	bool looping;

	xmp_context mod_track;

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

xmpplayer_c::xmpplayer_c() : status(NOT_LOADED)
{
	mono_buffer = new s16_t[XMP_BUFFER * 2];
}

xmpplayer_c::~xmpplayer_c()
{
	Close();

	if (mono_buffer)
		delete[] mono_buffer;
}

void xmpplayer_c::PostOpenInit()
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

bool xmpplayer_c::StreamIntoBuffer(epi::sound_data_c *buf)
{
	s16_t *data_buf;

	bool song_done = false;

	if (!dev_stereo)
		data_buf = mono_buffer;
	else
		data_buf = buf->data_L;

	int played = 0;

	int did_play = xmp_play_buffer(mod_track, data_buf, XMP_BUFFER, 0, &played);

	if (did_play < -XMP_END) // ERROR
	{
		I_Debugf("[xmpplayer_c::StreamIntoBuffer] Failed\n");
		return false;
	}

	if (did_play == -XMP_END)
		song_done = true;

	buf->length = played / 2 / sizeof(s16_t);

	if (!dev_stereo)
		ConvertToMono(buf->data_L, mono_buffer, buf->length);

	if (song_done)  /* EOF */
	{
		if (! looping)
			return false;
		xmp_restart_module(mod_track);
		return true;
	}

    return (true);
}


void xmpplayer_c::Volume(float gain)
{
	// not needed, music volume is handled in s_blit.cc
	// (see mix_channel_c::ComputeMusicVolume).
}


bool xmpplayer_c::OpenMemory(byte *data, int length)
{
	SYS_ASSERT(data);

	mod_track = xmp_create_context();

	if (!mod_track)
	{
		I_Warning("xmpplayer_c: failure to create xmp context\n");
		return false;
	}

    if (xmp_load_module_from_memory(mod_track, data, length) != 0)
    {
		I_Warning("[xmpplayer_c::Open](DataLump) Failed!\n");
		return false;
    }

	PostOpenInit();
	return true;
}


void xmpplayer_c::Close()
{
	if (status == NOT_LOADED)
		return;

	// Stop playback
	if (status != STOPPED)
		Stop();
		
	xmp_end_player(mod_track);
	xmp_release_module(mod_track);
	xmp_free_context(mod_track);

	status = NOT_LOADED;
}


void xmpplayer_c::Pause()
{
	if (status != PLAYING)
		return;

	xmp_stop_module(mod_track);

	status = PAUSED;
}


void xmpplayer_c::Resume()
{
	if (status != PAUSED)
		return;

	status = PLAYING;
}


void xmpplayer_c::Play(bool loop)
{
    if (status != NOT_LOADED && status != STOPPED) return;

	status = PLAYING;
	looping = loop;

	xmp_start_player(mod_track, dev_freq, 0);

	// Load up initial buffer data
	Ticker();
}


void xmpplayer_c::Stop()
{
	if (status != PLAYING && status != PAUSED)
		return;

	S_QueueStop();

	xmp_stop_module(mod_track);

	status = STOPPED;
}


void xmpplayer_c::Ticker()
{
	while (status == PLAYING)
	{
		epi::sound_data_c *buf = S_QueueGetFreeBuffer(XMP_BUFFER, 
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

abstract_music_c * S_PlayXMPMusic(byte *data, int length, float volume, bool looping)
{
	xmpplayer_c *player = new xmpplayer_c();

	if (! player->OpenMemory(data, length))
	{
		delete[] data;
		delete player;
		return NULL;
	}

	delete[] data;

	player->Volume(volume);
	player->Play(looping);

	return player;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
