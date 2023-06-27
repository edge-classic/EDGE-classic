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
#include "str_util.h"

#include "main.h"

#include "dm_state.h"
#include "s_sound.h"
#include "s_music.h"
#include "s_ogg.h"
#include "s_mp3.h"
#include "s_prime.h"
#include "s_gme.h"
#include "s_m4p.h"
#include "s_opl.h"
#include "s_sid.h"
#include "s_vgm.h"
#include "s_flac.h"
#include "s_fmm.h"
#include "m_misc.h"
#include "w_files.h"
#include "w_wad.h"

// music slider value
DEF_CVAR(mus_volume, "0.15", CVAR_ARCHIVE)

bool nomusic = false;

// Current music handle
static abstract_music_c *music_player;

int  entry_playing = -1;
static bool entry_looped;
bool var_pc_speaker_mode = false;


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

	float volume = mus_volume.f;

	// open the file or lump, and read it into memory
	epi::file_c *F;

	switch (play->infotype)
	{
		case MUSINF_FILE:
		{
			std::filesystem::path fn = M_ComposeFileName(game_dir, UTFSTR(play->info));

			F = epi::FS_Open(fn, epi::file_c::ACCESS_READ | epi::file_c::ACCESS_BINARY);
			if (! F)
			{
				I_Warning("S_ChangeMusic: Can't Find File '%s'\n", fn.u8string().c_str());
				return;
			}
			break;
		}

		case MUSINF_PACKAGE:
		{
			F = W_OpenPackFile(play->info);
			if (! F)
			{
				I_Warning("S_ChangeMusic: PK3 entry '%s' not found.\n", play->info.c_str());
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

	if (! data)
	{
		delete F;
		I_Warning("S_ChangeMusic: Error loading data.\n");
		return;
	}
	if (length < 4)
	{
		delete F;
		delete data;
		I_Printf("S_ChangeMusic: ignored short data (%d bytes)\n", length);
		return;
	}

	epi::sound_format_e fmt = epi::FMT_Unknown;

	// IMF Music is the outlier in that it must be predefined in DDFPLAY with the appropriate
	// IMF frequency, as there is no way of determining this from file information alone
	if (play->type == MUS_IMF280 || play->type == MUS_IMF560 || play->type == MUS_IMF700)
		fmt = epi::FMT_IMF;
	else
	{
		if (play->infotype == MUSINF_LUMP)
		{
			// lumps must use auto-detection based on their contents
			fmt = epi::Sound_DetectFormat(data, length);
		}
		else
		{
			// for FILE and PACK, use the file extension
			fmt = epi::Sound_FilenameToFormat(play->info);
		}
	}

	// NOTE: players are responsible for freeing 'data'

	switch (fmt)
	{
		case epi::FMT_OGG:
			delete F;
			music_player = S_PlayOGGMusic(data, length, volume, loop);
			break;

		case epi::FMT_MP3:
			delete F;
			music_player = S_PlayMP3Music(data, length, volume, loop);
			break;

		case epi::FMT_FLAC:
			delete F;
			music_player = S_PlayFLACMusic(data, length, volume, loop);
			break;

		case epi::FMT_M4P:
			delete F;
			music_player = S_PlayM4PMusic(data, length, volume, loop);
			break;

		case epi::FMT_GME:
			delete F;
			music_player = S_PlayGMEMusic(data, length, volume, loop);
			break;

		case epi::FMT_VGM:
			delete F;
			music_player = S_PlayVGMMusic(data, length, volume, loop);
			break;

		case epi::FMT_SID:
			delete F;
			music_player = S_PlaySIDMusic(data, length, volume, loop);
			break;

		// IMF writes raw OPL registers, so must use the OPL player unconditionally
		case epi::FMT_IMF:
			delete F;
			music_player = S_PlayOPL(data, length, volume, loop, play->type);
			break;

		case epi::FMT_MIDI:
		case epi::FMT_MUS:
		case epi::FMT_WAV: // RIFF MIDI has the same header as WAV
			delete F;
			if (var_midi_player == 0)
			{
				music_player = S_PlayPrime(data, length, volume, loop);
			}
			else if (var_midi_player == 1)
			{
				music_player = S_PlayOPL(data, length, volume, loop, play->type);
			}
			else
			{
				music_player = S_PlayFMM(data, length, volume, loop);
			}
			break;

		default:
			delete F;
			delete data;
			I_Printf("S_ChangeMusic: unknown format\n");
			break;
	}
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
		music_player->Volume(mus_volume.f);
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
