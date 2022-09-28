//------------------------------------------------------------------------
//  MUSIC Definitions
//------------------------------------------------------------------------
//
//  DEH_EDGE  Copyright (C) 2004-2005  The EDGE Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License (in COPYING.txt) for more details.
//
//------------------------------------------------------------------------
//
//  DEH_EDGE is based on:
//
//  +  DeHackEd source code, by Greg Lewis.
//  -  DOOM source code (C) 1993-1996 id Software, Inc.
//  -  Linux DOOM Hack Editor, by Sam Lantinga.
//  -  PrBoom's DEH/BEX code, by Ty Halderman, TeamTNT.
//
//------------------------------------------------------------------------

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "deh_i_defs.h"
#include "deh_edge.h"

#include "deh_buffer.h"
#include "deh_patch.h"
#include "deh_music.h"
#include "deh_storage.h"
#include "deh_system.h"
#include "deh_util.h"
#include "deh_wad.h"


namespace Deh_Edge
{

//
// Information about all the music
//

musicinfo_t S_music[NUMMUSIC] =
{
    { NULL, -1, "" },  // dummy entry

    { "e1m1", 33, "" },
    { "e1m2", 34, "" },
    { "e1m3", 35, "" },
    { "e1m4", 36, "" },
    { "e1m5", 37, "" },
    { "e1m6", 38, "" },
    { "e1m7", 39, "" },
    { "e1m8", 40, "" },
    { "e1m9", 41, "" },
    { "e2m1", 42, "" },
    { "e2m2", 43, "" },
    { "e2m3", 44, "" },
    { "e2m4", 45, "" },
    { "e2m5", 46, "" },
    { "e2m6", 47, "" },
    { "e2m7", 48, "" },
    { "e2m8", 49, "" },
    { "e2m9", 50, "" },
    { "e3m1", 51, "" },
    { "e3m2", 52, "" },
    { "e3m3", 53, "" },
    { "e3m4", 54, "" },
    { "e3m5", 55, "" },
    { "e3m6", 56, "" },
    { "e3m7", 57, "" },
    { "e3m8", 58, "" },
    { "e3m9", 59, "" },

    { "inter",  63, "" },
    { "intro",  62, "" },
    { "bunny",  67, "" },
    { "victor", 61, "" },
    { "introa", 68, "" },
    { "runnin",  1, "" },
    { "stalks",  2, "" },
    { "countd",  3, "" },
    { "betwee",  4, "" },
    { "doom",    5, "" },
    { "the_da",  6, "" },
    { "shawn",   7, "" },
    { "ddtblu",  8, "" },
    { "in_cit",  9, "" },
    { "dead",   10, "" },
    { "stlks2", 11, "" },
    { "theda2", 12, "" },
    { "doom2",  13, "" },
    { "ddtbl2", 14, "" },
    { "runni2", 15, "" },
    { "dead2",  16, "" },
    { "stlks3", 17, "" },
    { "romero", 18, "" },
    { "shawn2", 19, "" },
    { "messag", 20, "" },
    { "count2", 21, "" },
    { "ddtbl3", 22, "" },
    { "ampie",  23, "" },
    { "theda3", 24, "" },
    { "adrian", 25, "" },
    { "messg2", 26, "" },
    { "romer2", 27, "" },
    { "tense",  28, "" },
    { "shawn3", 29, "" },
    { "openin", 30, "" },
    { "evil",   31, "" },
    { "ultima", 32, "" },
    { "read_m", 60, "" },
    { "dm2ttl", 65, "" },
    { "dm2int", 64, "" } 
};


//------------------------------------------------------------------------

namespace Music
{
	bool got_one;
}


void Music::Init()
{ }


void Music::Shutdown()
{ }


namespace Music
{
	void BeginMusicLump(void)
	{
		WAD::NewLump("DDFPLAY");

		WAD::Printf("<PLAYLISTS>\n\n");
	}

	void FinishMusicLump(void)
	{
		WAD::Printf("\n");
	}

	void WriteMusic(int m_num)
	{
		musicinfo_t *mus = S_music + m_num;

		if (! got_one)
		{
			got_one = true;
			BeginMusicLump();
		}

		WAD::Printf("[%02d] ", mus->ddf_num);

		const char *lump = !mus->new_name.empty() ? mus->new_name.c_str() : mus->orig_name;

		WAD::Printf("MUSICINFO = MUS:LUMP:\"D_%s\";\n", StrUpper(lump));
	}
}


void Music::ConvertMUS()
{
	got_one = false;

	for (int i = 1; i < NUMMUSIC; i++)
	{
	    if (! all_mode && S_music[i].new_name.empty())
			continue;

		WriteMusic(i);
	}

	if (got_one)
		FinishMusicLump();
}


bool Music::ReplaceMusic(const char *before, const char *after)
{
	for (int j = 1; j < NUMMUSIC; j++)
	{
		if (StrCaseCmp(S_music[j].orig_name, before) != 0)
			continue;

		if (!S_music[j].new_name.empty())
			S_music[j].new_name.clear();

		S_music[j].new_name = StringDup(after);

		return true;
	}

	return false;
}


void Music::AlterBexMusic(const char *new_val)
{
	const char *old_val = Patch::line_buf;

	if (strlen(old_val) < 1 || strlen(old_val) > 6)
	{
		PrintWarn("Bad length for music name '%s'.\n", old_val);
		return;
	}

	if (strlen(new_val) < 1 || strlen(new_val) > 6)
	{
		PrintWarn("Bad length for music name '%s'.\n", new_val);
		return;
	}

	if (! ReplaceMusic(old_val, new_val))
		PrintWarn("Line %d: unknown music name '%s'.\n",
			Patch::line_num, old_val);
}

}  // Deh_Edge
