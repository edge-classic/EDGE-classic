//----------------------------------------------------------------------------
//  EDGE XMP Music Player
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
#include "s_xmp.h"
#include "w_wad.h"

#include "xmp.h"

#define XMP_15_SECONDS XMP_MAX_FRAMESIZE * 750 // From XMP's site, one second of a song is roughly 50 frames - Dasho

extern bool dev_stereo;  // FIXME: encapsulation
extern int  dev_freq;

// Function for s_music.cc to check if a lump is in mod tracker format
bool S_CheckXMP (byte *data, int length)
{
    if (xmp_test_module_from_memory(data, length, NULL) != 0)
    {
		return false;
    }
	else
	{
		return true;		
	}
}

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

xmpplayer_c::xmpplayer_c() : status(NOT_LOADED)
{
	mono_buffer = new s16_t[XMP_15_SECONDS * 2];
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
	// Unlike the other players, libxmp seems to already handle this internally, so just copy the buffer - Dasho
	memcpy(dest, src, len);
}

bool xmpplayer_c::StreamIntoBuffer(epi::sound_data_c *buf)
{
	s16_t *data_buf;

	bool song_done = false;

	if (!dev_stereo)
		data_buf = mono_buffer;
	else
		data_buf = buf->data_L;

	int did_play = xmp_play_buffer(mod_track, data_buf, XMP_15_SECONDS, 0);

	if (did_play < -XMP_END) // ERROR
	{
		I_Debugf("[xmpplayer_c::StreamIntoBuffer] Failed\n");
		return false;
	}

	if (did_play == -XMP_END)
		song_done = true;

	buf->length = XMP_15_SECONDS * sizeof(s16_t);

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


bool xmpplayer_c::OpenLump(const char *lumpname)
{
	SYS_ASSERT(lumpname);

	if (status != NOT_LOADED)
		Close();

	int lump = W_CheckNumForName(lumpname);
	if (lump < 0)
	{
		I_Warning("xmpplayer_c: LUMP '%s' not found.\n", lumpname);
		return false;
	}

	epi::file_c *F = W_OpenLump(lump);

	int length = F->GetLength();

	byte *data = F->LoadIntoMemory();

	if (! data)
	{
		delete F;
		I_Warning("xmpplayer_c: Error loading data.\n");
		return false;
	}
	if (length < 4)
	{
		delete F;
		I_Debugf("xmpplayer_c: ignored short data (%d bytes)\n", length);
		return false;
	}

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

bool xmpplayer_c::OpenFile(const char *filename)
{
	SYS_ASSERT(filename);

	if (status != NOT_LOADED)
		Close();

	mod_track = xmp_create_context();

	if (!mod_track)
	{
		I_Warning("xmpplayer_c: failure to create xmp context\n");
		return false;
	}	

    if (xmp_load_module(mod_track, filename) != 0)
    {
		I_Warning("xmpplayer_c: Could not open file: '%s'\n", filename);
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

	xmp_start_player(mod_track, dev_freq, dev_stereo ? 0 : XMP_FORMAT_MONO);

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
		epi::sound_data_c *buf = S_QueueGetFreeBuffer(XMP_15_SECONDS, 
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

abstract_music_c * S_PlayXMPMusic(const pl_entry_c *musdat, float volume, bool looping)
{
	xmpplayer_c *player = new xmpplayer_c();

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
		I_Error("S_PlayXMPMusic: bad format value %d\n", musdat->infotype);

	player->Volume(volume);
	player->Play(looping);

	return player;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
