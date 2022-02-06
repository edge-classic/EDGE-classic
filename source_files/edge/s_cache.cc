//----------------------------------------------------------------------------
//  Sound Caching
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2009  The EDGE Team.
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
//
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------

#include "i_defs.h"

#include <vector>

#include "file.h"
#include "filesystem.h"
#include "file_memory.h"
#include "sound_data.h"
#include "sound_wav.h"

#include "main.h"
#include "sfx.h"

#include "s_sound.h"
#include "s_cache.h"
#include "s_ogg.h"
#include "s_mp3.h"

#include "dm_state.h"  // game_dir
#include "m_argv.h"
#include "m_misc.h"
#include "m_random.h"
#include "p_mobj.h"
#include "r_defs.h"
#include "w_wad.h"


extern int dev_freq;


static std::vector<epi::sound_data_c *> fx_cache;


static void Load_Silence(epi::sound_data_c *buf)
{
	int length = 256;

	buf->freq = dev_freq;
	buf->Allocate(length, epi::SBUF_Mono);

	memset(buf->data_L, 0, length * sizeof(s16_t));
}

static bool Load_DOOM(epi::sound_data_c *buf, const byte *lump, int length)
{
	buf->freq = lump[2] + (lump[3] << 8);

	if (buf->freq < 8000 || buf->freq > 44100)
		I_Warning("Sound Load: weird frequency: %d Hz\n", buf->freq);

	if (buf->freq < 4000)
		buf->freq = 4000;

	length -= 8;

	buf->Allocate(length, epi::SBUF_Mono);

	// convert to signed 16-bit format
	const byte *src = lump + 8;
	const byte *s_end = src + length;

	s16_t *dest = buf->data_L;

	for (; src < s_end; src++)
		*dest++ = (*src ^ 0x80) << 8;

	return true;
}

static bool Load_WAV(epi::sound_data_c *buf, const byte *lump, int length)
{
	epi::mem_file_c F(lump, length, false);

	return epi::WAV_Load(buf, &F);
}

static bool Load_OGG(epi::sound_data_c *buf, const byte *lump, int length)
{
	return S_LoadOGGSound(buf, lump, length);
}

static bool Load_MP3(epi::sound_data_c *buf, const byte *lump, int length)
{
	return S_LoadMP3Sound(buf, lump, length);
}

//----------------------------------------------------------------------------

void S_CacheInit(void)
{
	// nothing to do
}

void S_FlushData(epi::sound_data_c *fx)
{
	SYS_ASSERT(fx->ref_count == 0);

	fx->Free();
}

void S_CacheClearAll(void)
{
	for (int i = 0; i < (int)fx_cache.size(); i++)
		delete fx_cache[i];

	fx_cache.erase(fx_cache.begin(), fx_cache.end());
}


static bool DoCacheLoad(sfxdef_c *def, epi::sound_data_c *buf)
{
	I_Debugf("S_CacheLoad: [%s]\n", def->name.c_str());

	// open the file or lump, and read it into memory
	epi::file_c *F;

	if (def->file_name && def->file_name[0])
	{
		std::string fn = M_ComposeFileName(game_dir.c_str(), def->file_name.c_str());

		F = epi::FS_Open(fn.c_str(), epi::file_c::ACCESS_READ | epi::file_c::ACCESS_BINARY);

		if (! F)
		{
			M_WarnError("SFX Loader: Can't Find File '%s'\n", fn.c_str());
			return false;
		}
	}
	else 
	{
		int lump = W_CheckNumForName(def->lump_name.c_str());
		if (lump < 0)
		{
			M_WarnError("SFX Loader: Missing sound lump: %s\n", def->lump_name.c_str());
			return false;
		}

		F = W_OpenLump(lump);
		SYS_ASSERT(F);
	}
	
	int length = F->GetLength();

	byte *data = F->LoadIntoMemory();

	// no longer need the epi::file_c
	delete F; F = NULL;

	if (! data || length < 4)
	{
		M_WarnError("SFX Loader: Error loading data.\n");
		return false;
	}

	// Load the data into the buffer

	bool OK = false;
	
	if (memcmp(data, "RIFF", 4) == 0)
		OK = Load_WAV(buf, data, length);
	else if (memcmp(data, "Ogg", 3) == 0)
		OK = Load_OGG(buf, data, length);
	else if (S_CheckMP3(data, length))
		OK = Load_MP3(buf, data, length);
	else
		OK = Load_DOOM(buf, data, length);

	// Create "environmental" sounds - Dasho
	if (OK)
	{
		buf->Mix_Underwater();
		buf->Mix_Airless();
		buf->Mix_Reverb();
	}

	return OK;
}

epi::sound_data_c *S_CacheLoad(sfxdef_c *def)
{
	for (int i = 0; i < (int)fx_cache.size(); i++)
	{
		if (fx_cache[i]->priv_data == (void*)def)
		{
			fx_cache[i]->ref_count++;
			return fx_cache[i];
		}
	}

	// create data structure
	epi::sound_data_c *buf = new epi::sound_data_c();

	fx_cache.push_back(buf);

	buf->priv_data = def;
	buf->ref_count = 1;

	if (! DoCacheLoad(def, buf))
		Load_Silence(buf);	

	return buf;
}

void S_CacheRelease(epi::sound_data_c *data)
{
	SYS_ASSERT(data->ref_count >= 1);

	data->ref_count--;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
