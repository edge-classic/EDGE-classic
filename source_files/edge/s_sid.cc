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

	byte *sid_data;
	int sid_length;

	int empty_frame_counter;

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

	computeAudioSamples();

	buf->length = getSoundBufferLen();

	memcpy(data_buf, getSoundBuffer(), buf->length * sizeof(s16_t) * 2);

	if (!dev_stereo)
		ConvertToMono(buf->data_L, mono_buffer, buf->length);

	// End-of-track detection seems hit or miss, so if 5 empty frames are rendered in a row,
	// restart the SID
	for (int i = 0; i < buf->length; i++)
	{
		if (data_buf[i] != 0)
		{
			if (empty_frame_counter > 0)
				empty_frame_counter--;
			break;
		}
		if (i == buf->length - 1)
		{
			if (! looping)
				return false;
			else
			{
				empty_frame_counter++;
				if (empty_frame_counter == 5)
				{
					empty_frame_counter = 0;
					loadSidFile(0, sid_data, sid_length, dev_freq, NULL, NULL, NULL, NULL);
					sid_playTune(0, 0);
					break;
				}
			}
		}
	}

    return true;
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

	// Need to keep the song in memory for SID restarts
	sid_length = length;
	sid_data = new byte[sid_length];
	memcpy(sid_data, data, sid_length);

	PostOpenInit();
	return true;
}

bool sidplayer_c::OpenFile(const char *filename)
{
	SYS_ASSERT(filename);

	if (status != NOT_LOADED)
		Close();

	FILE *sid_loader = fopen(filename, "rb");

	if (!sid_loader)
	{
		I_Warning("sidplayer_c: Could not open file: '%s'\n", filename);
		return false;		
	}

	// Basically the same as EPI::File's GetLength() method
	long cur_pos = ftell(sid_loader);      // Get existing position

    fseek(sid_loader, 0, SEEK_END);        // Seek to the end of file
    long len = ftell(sid_loader);          // Get the position - it our length

    fseek(sid_loader, cur_pos, SEEK_SET);  // Reset existing position
   
   	sid_length = len;

	sid_data = new byte[sid_length];

	if (fread(sid_data, 1, sid_length, sid_loader) != sid_length)
	{
		I_Warning("sidplayer_c: Could not open file: '%s'\n", filename);
		fclose(sid_loader);
		if (sid_data)
			delete []sid_data;
		return false;		
	}

	fclose(sid_loader);

	if (loadSidFile(0, sid_data, sid_length, dev_freq, NULL, NULL, NULL, NULL) != 0)
    {
		I_Warning("sidplayer_c: Could not open file: '%s'\n", filename);
		if (sid_data)
			delete []sid_data;
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

	empty_frame_counter = 0;
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
