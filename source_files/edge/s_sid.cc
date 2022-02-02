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

#define SID_BUFFER 96000 / 50 * 4

extern bool dev_stereo;  // FIXME: encapsulation
extern int  dev_freq;

// Function for s_music.cc to check if a lump is supported by TinySID
bool S_CheckSID (byte *data, int length)
{
	if ((length < 0x7c) || !((data[0x00] == 0x50) || (data[0x00] == 0x52))) {
		return false;	// we need at least a header that starts with "P" or "R"
	}
	return true;
}

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

public:
	bool OpenLump(const char *lumpname);
	bool OpenFile(const char *filename);

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

	int result = 0;

	int got_size = 0;

	bool song_done = false;

	if (!dev_stereo)
		data_buf = mono_buffer;
	else
		data_buf = buf->data_L;

	result = computeAudioSamples();

	if (result == 0)
		return false;

	if (result == -1)
	{
		song_done = true;
		return false;
	}

	got_size = getSoundBufferLen();

	memcpy(data_buf, getSoundBuffer(), got_size * sizeof(s16_t) * 2);
	
	buf->length = got_size;

	if (!dev_stereo)
		ConvertToMono(buf->data_L, mono_buffer, buf->length);

	if (song_done)  // EOF
	{
		if (! looping)
			return false;
		sid_playTune(0, 0);
		return true;
	}

    return (true);
}


void sidplayer_c::Volume(float gain)
{
	// not needed, music volume is handled in s_blit.cc
	// (see mix_channel_c::ComputeMusicVolume).
}


bool sidplayer_c::OpenLump(const char *lumpname)
{
	SYS_ASSERT(lumpname);

	if (status != NOT_LOADED)
		Close();

	int lump = W_CheckNumForName(lumpname);
	if (lump < 0)
	{
		I_Warning("sidplayer_c: LUMP '%s' not found.\n", lumpname);
		return false;
	}

	epi::file_c *F = W_OpenLump(lump);

	int length = F->GetLength();

	byte *data = F->LoadIntoMemory();

	if (! data)
	{
		delete F;
		I_Warning("sidplayer_c: Error loading data.\n");
		return false;
	}
	if (length < 4)
	{
		delete F;
		I_Debugf("sidplayer_c: ignored short data (%d bytes)\n", length);
		return false;
	}

    if (loadSidFile(0, data, length, dev_freq, NULL, NULL, NULL, NULL) != 0)
    {
		I_Warning("[sidplayer_c::Open](DataLump) Failed\n");
		return false;
    }

	PostOpenInit();
	return true;
}

bool sidplayer_c::OpenFile(const char *filename)
{
	SYS_ASSERT(filename);

	if (status != NOT_LOADED)
		Close();

	if (loadSidFile(0, NULL, NULL, dev_freq, (char *)filename, NULL, NULL, NULL) != 0)
    {
		I_Warning("sidplayer_c: Could not open file: '%s'\n", filename);
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

abstract_music_c * S_PlaySIDMusic(const pl_entry_c *musdat, float volume, bool looping)
{
	sidplayer_c *player = new sidplayer_c();

	if (musdat->infotype == MUSINF_LUMP)
	{
		if (! player->OpenLump(musdat->info.c_str()))
		{
			delete player;
			return NULL;
		}
	}
	else if (musdat->infotype == MUSINF_FILE)
	{
		if (! player->OpenFile(musdat->info.c_str()))
		{
			delete player;
			return NULL;
		}
	}
	else
		I_Error("S_PlaySIDMusic: bad format value %d\n", musdat->infotype);

	player->Volume(volume);
	player->Play(looping);

	return player;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
