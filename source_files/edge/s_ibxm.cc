//----------------------------------------------------------------------------
//  EDGE IBXM Music Player
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
#include "s_ibxm.h"
#include "w_wad.h"

#include "ibxm.h"

extern bool dev_stereo;  // FIXME: encapsulation
extern int  dev_freq;

// Function for s_music.cc to check if a lump is in mod tracker format
bool S_CheckIBXM (byte *data, int length)
{
	ibxm_data *mod_check = new ibxm_data;
	mod_check->buffer = (char *)data;
	mod_check->length = length;
	bool is_mod_music = false;
	// Check for MOD format
	switch( ibxm_data_u16be( mod_check, 1082 ) ) 
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
	if( ibxm_data_u16le( mod_check, 58 ) == 0x0104 )
		is_mod_music = true;
	// Check for S3M format
	if( ibxm_data_u32le( mod_check, 44 ) == 0x4d524353 )
		is_mod_music = true;
	delete[] mod_check;
	mod_check = NULL;
	return is_mod_music;
}

class ibxmplayer_c : public abstract_music_c
{
public:
	 ibxmplayer_c();
	~ibxmplayer_c();
private:

	enum status_e
	{
		NOT_LOADED, PLAYING, PAUSED, STOPPED
	};
	
	int status;
	bool looping;

	ibxm_module *mod_track;
	ibxm_replay *mod_replay;
	ibxm_data *mod_data;
	int max_buf;

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

ibxmplayer_c::ibxmplayer_c() : status(NOT_LOADED)
{
}

ibxmplayer_c::~ibxmplayer_c()
{
	Close();

	if (mono_buffer)
		delete[] mono_buffer;
}

void ibxmplayer_c::PostOpenInit()
{   
	// Loaded, but not playing
	max_buf = ibxm_calculate_mix_buf_len(dev_freq);
	mono_buffer = new s16_t[max_buf * 2];
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

bool ibxmplayer_c::StreamIntoBuffer(epi::sound_data_c *buf)
{
	s16_t *data_buf;

	bool song_done = false;

	if (!dev_stereo)
		data_buf = mono_buffer;
	else
		data_buf = buf->data_L;

	int did_play = ibxm_replay_get_audio(mod_replay, (int *)data_buf, 0);

	if (did_play < 0) // ERROR
	{
		I_Debugf("[ibxmplayer_c::StreamIntoBuffer] Failed\n");
		return false;
	}

	if (did_play == 0)
		song_done = true;

	buf->length = did_play * 2;

	if (!dev_stereo)
		ConvertToMono(buf->data_L, mono_buffer, buf->length);

	if (song_done)  /* EOF */
	{
		if (! looping)
			return false;
		ibxm_replay_set_sequence_pos(mod_replay, 0);
		return true;
	}

    return (true);
}


void ibxmplayer_c::Volume(float gain)
{
	// not needed, music volume is handled in s_blit.cc
	// (see mix_channel_c::ComputeMusicVolume).
}


bool ibxmplayer_c::OpenLump(const char *lumpname)
{
	SYS_ASSERT(lumpname);

	if (status != NOT_LOADED)
		Close();

	int lump = W_CheckNumForName(lumpname);
	if (lump < 0)
	{
		I_Warning("ibxmplayer_c: LUMP '%s' not found.\n", lumpname);
		return false;
	}

	epi::file_c *F = W_OpenLump(lump);

	int length = F->GetLength();

	byte *data = F->LoadIntoMemory();

	if (! data)
	{
		delete F;
		I_Warning("ibxmplayer_c: Error loading data.\n");
		return false;
	}
	if (length < 4)
	{
		delete F;
		I_Debugf("ibxmplayer_c: ignored short data (%d bytes)\n", length);
		return false;
	}

	mod_data = new ibxm_data;
	mod_data->length = length;
	mod_data->buffer = (char *)data;
	char *err_msg = new char[64];
	mod_track = ibxm_module_load(mod_data, err_msg);

	if (!mod_track)
	{
		I_Warning("modplayer_c: failure to load module: %s\n", err_msg);
		delete[] mod_data;
		mod_data = NULL;
		delete[] err_msg;
		err_msg = NULL;
		return false;
	}

	mod_replay = ibxm_new_replay(mod_track, dev_freq / 2, 0);

    if (!mod_replay)
    {
		I_Warning("[ibxmplayer_c::Open](DataLump) Failed!\n");
		ibxm_dispose_module(mod_track);
		delete[] mod_data;
		mod_data = NULL;
		delete[] err_msg;
		err_msg = NULL;
		return false;
    }

	PostOpenInit();
	return true;
}

bool ibxmplayer_c::OpenFile(const char *filename)
{
	SYS_ASSERT(filename);

	if (status != NOT_LOADED)
		Close();

	FILE *mod_loader = fopen(filename, "rb");

	if (!mod_loader)
	{
		I_Warning("sidplayer_c: Could not open file: '%s'\n", filename);
		return false;		
	}

	// Basically the same as EPI::File's GetLength() method
	long cur_pos = ftell(mod_loader);      // Get existing position

    fseek(mod_loader, 0, SEEK_END);        // Seek to the end of file
    long len = ftell(mod_loader);          // Get the position - it our length

    fseek(mod_loader, cur_pos, SEEK_SET);  // Reset existing position
   
   	mod_data = new ibxm_data;
	mod_data->length = len;
	mod_data->buffer = (char *)new byte[len];

	if (fread(mod_data->buffer, 1, len, mod_loader) != len)
	{
		I_Warning("sidplayer_c: Could not open file: '%s'\n", filename);
		fclose(mod_loader);
		if (mod_data)
			delete []mod_data;
		return false;		
	}

	fclose(mod_loader);

	char *err_msg = new char[64];
	mod_track = ibxm_module_load(mod_data, err_msg);

	if (!mod_track)
	{
		I_Warning("modplayer_c: failure to load module: %s\n", err_msg);
		delete[] mod_data;
		mod_data = NULL;
		delete[] err_msg;
		err_msg = NULL;
		return false;
	}

	mod_replay = ibxm_new_replay(mod_track, dev_freq / 2, 0);

    if (!mod_replay)
    {
		I_Warning("[ibxmplayer_c::Open](DataLump) Failed!\n");
		ibxm_dispose_module(mod_track);
		delete[] mod_data;
		mod_data = NULL;
		delete[] err_msg;
		err_msg = NULL;
		return false;
    }

	PostOpenInit();
	return true;
}


void ibxmplayer_c::Close()
{
	if (status == NOT_LOADED)
		return;

	// Stop playback
	if (status != STOPPED)
		Stop();
		
	ibxm_dispose_replay(mod_replay);
	ibxm_dispose_module(mod_track);
	delete[] mod_data;
	mod_data = NULL;

	status = NOT_LOADED;
}


void ibxmplayer_c::Pause()
{
	if (status != PLAYING)
		return;

	status = PAUSED;
}


void ibxmplayer_c::Resume()
{
	if (status != PAUSED)
		return;

	status = PLAYING;
}


void ibxmplayer_c::Play(bool loop)
{
    if (status != NOT_LOADED && status != STOPPED) return;

	status = PLAYING;
	looping = loop;

	// Load up initial buffer data
	Ticker();
}


void ibxmplayer_c::Stop()
{
	if (status != PLAYING && status != PAUSED)
		return;

	S_QueueStop();

	status = STOPPED;
}


void ibxmplayer_c::Ticker()
{
	while (status == PLAYING)
	{
		epi::sound_data_c *buf = S_QueueGetFreeBuffer(max_buf, 
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

abstract_music_c * S_PlayIBXMMusic(const pl_entry_c *musdat, float volume, bool looping)
{
	ibxmplayer_c *player = new ibxmplayer_c();

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
