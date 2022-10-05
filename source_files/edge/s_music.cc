//----------------------------------------------------------------------------
//  EDGE Music handling Code
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
// -ACB- 1999/11/13 Written
//

#include "i_defs.h"

#include <stdlib.h>

#include "file.h"
#include "filesystem.h"
#include "sound_types.h"

#include "main.h"

#include "dm_state.h"
#include "s_sound.h"
#include "s_music.h"
#include "s_ogg.h"
#include "s_mp3.h"
#include "s_tsf.h"
#include "s_gme.h"
#include "s_mod.h"
#include "s_opl.h"
#include "s_sid.h"
#include "m_misc.h"
#include "w_wad.h"

// music slider value
int mus_volume;

bool nomusic = false;

// Current music handle
static abstract_music_c *music_player;

int  entry_playing = -1;
static bool entry_looped;


void S_ChangeMusic(int entrynum, bool loop)
{
	if (nomusic)
		return;

	// -AJA- playlist number 0 reserved to mean "no music"
	if (entrynum <= 0)
	{
		S_StopMusic();
		return;
	}

	// -AJA- don't restart the current song (DOOM compatibility)
	if (entrynum == entry_playing && entry_looped)
		return;

	S_StopMusic();

	entry_playing = entrynum;
	entry_looped  = loop;

	// when we cannot find the music entry, no music will play
	const pl_entry_c *play = playlist.Find(entrynum);
	if (!play)
	{
		I_Warning("Could not find music entry [%d]\n", entrynum);
		return;
	}

	float volume = slider_to_gain[mus_volume];

	// open the file or lump, and read it into memory
	epi::file_c *F;

	switch (play->infotype)
	{
		case MUSINF_FILE:
		{
			std::string fn = M_ComposeFileName(game_dir.c_str(), play->info.c_str());

			F = epi::FS_Open(fn.c_str(), epi::file_c::ACCESS_READ | epi::file_c::ACCESS_BINARY);

			if (! F)
			{
				I_Warning("S_ChangeMusic: Can't Find File '%s'\n", fn.c_str());
				return;
			}

			break;
		}

		case MUSINF_LUMP:
		{
			int lump = W_CheckNumForName(play->info.c_str());
			if (lump < 0)
			{
				I_Warning("S_ChangeMusic: LUMP '%s' not found.\n", play->info.c_str()); 
				return;
			}

			F = W_OpenLump(lump);
			break;
		}

		default:
			I_Printf("S_ChangeMusic: invalid method %d for MUS/MIDI\n", play->infotype);
			return;
	}

	int length = F->GetLength();

	byte *data = F->LoadIntoMemory();

	// close file now
	delete F;

	if (! data)
	{
		I_Warning("S_ChangeMusic: Error loading data.\n");
		return;
	}
	if (length < 4)
	{
		delete data;

		I_Printf("S_ChangeMusic: ignored short data (%d bytes)\n", length);
		return;
	}

	auto fmt = epi::Sound_DetectFormat(data, std::min(length, 32));

	if (fmt == epi::FMT_OGG)
	{
		delete data;

		music_player = S_PlayOGGMusic(play, volume, loop);
		return;
	}

	if (fmt == epi::FMT_MP3)
	{
		delete data;

		music_player = S_PlayMP3Music(play, volume, loop);
		return;
	}

	if (fmt == epi::FMT_MOD)
	{
		delete data;

		music_player = S_PlayMODMusic(play, volume, loop);
		return;
	}

	if (fmt == epi::FMT_GME)
	{
		delete data;

		music_player = S_PlayGMEMusic(play, volume, loop);
		return;
	}

	if (fmt == epi::FMT_SID)
	{
		delete data;

		music_player = S_PlaySIDMusic(play, volume, loop);
		return;
	}

	// TODO: these calls free the data, but we probably should free it here

	if (fmt == epi::FMT_MIDI || fmt == epi::FMT_MUS)
	{
		if (var_opl_music)
		{
			music_player = S_PlayOPL(data, length, fmt == epi::FMT_MUS, volume, loop);
			return;
		}
		else
		{
			music_player = S_PlayTSF(data, length, fmt == epi::FMT_MUS, volume, loop);
			return;
		}
	}

	delete data;

	I_Printf("S_ChangeMusic: unknown format (not MUS or MIDI)\n");
	return;
}


void S_ResumeMusic(void)
{
	if (music_player)
		music_player->Resume();
}


void S_PauseMusic(void)
{
	if (music_player)
		music_player->Pause();
}


void S_StopMusic(void)
{
	// You can't stop the rock!! This does...

	if (music_player)
	{
		music_player->Stop();
		delete music_player;
		music_player = NULL;
	}

	entry_playing = -1;
	entry_looped  = false;
}


void S_MusicTicker(void)
{
	if (music_player)
		music_player->Ticker();
}


void S_ChangeMusicVolume(void)
{
	if (music_player)
		music_player->Volume(slider_to_gain[mus_volume]);
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
