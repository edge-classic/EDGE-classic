//----------------------------------------------------------------------------
//  EDGE LibCSID Music Player
//----------------------------------------------------------------------------
// 
//  Copyright (c) 2023 - The EDGE Team.
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

#include "libcRSID.h"

#define SID_BUFFER 4096
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

	cRSID_C64instance* C64 = nullptr;
	cRSID_SIDheader* C64_song = nullptr;

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

sidplayer_c::sidplayer_c() : status(NOT_LOADED)
{
	mono_buffer = new s16_t[SID_BUFFER * 2];
}

sidplayer_c::~sidplayer_c()
{
	Close();

	if (mono_buffer)
		delete[] mono_buffer;
}

void sidplayer_c::PostOpenInit()
{   
	cRSID_initSIDtune(C64, C64_song, 0);

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

	cRSID_generateSound(C64, (unsigned char *)data_buf, SID_BUFFER);

	buf->length = SID_BUFFER / sizeof(s16_t) / 2;

	if (!dev_stereo)
		ConvertToMono(buf->data_L, mono_buffer, buf->length);

    return true;
}

bool sidplayer_c::OpenMemory(byte *data, int length)
{
	SYS_ASSERT(data);

	if (status != NOT_LOADED)
		Close();

	C64 = cRSID_init(dev_freq);

	if (!C64)
	{
		I_Warning("[sidplayer_c]) Failed to initialize CRSID!\n");
		return false;
	}

	C64_song = cRSID_processSIDfile(C64, data, length);

    if (!C64_song)
    {
		I_Warning("[sidplayer_c::Open](DataLump) Failed\n");
		return false;
    }

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
	while (status == PLAYING && !var_pc_speaker_mode)
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

abstract_music_c * S_PlaySIDMusic(byte *data, int length, bool looping)
{
	sidplayer_c *player = new sidplayer_c();

	if (! player->OpenMemory(data, length))
	{
		delete[] data;
		delete player;
		return NULL;
	}

	// cRSID retains the data after initializing the track
	delete[] data;

	player->Play(looping);

	return player;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
