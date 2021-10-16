//------------------------------------------------------------------------
//  SOUND Definitions
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

#include "i_defs.h"
#include "sounds.h"

#include "buffer.h"
#include "dh_plugin.h"
#include "patch.h"
#include "storage.h"
#include "system.h"
#include "util.h"
#include "wad.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>


namespace Deh_Edge
{

// workaround for EDGE 1.27 bug with sound replacements
#define EDGE127_BUG  (target_version <= 127)


//
// Information about all the music
//

musicinfo_t S_music[NUMMUSIC] =
{
    { NULL, -1, NULL },  // dummy entry

    { "e1m1", 33, NULL },
    { "e1m2", 34, NULL },
    { "e1m3", 35, NULL },
    { "e1m4", 36, NULL },
    { "e1m5", 37, NULL },
    { "e1m6", 38, NULL },
    { "e1m7", 39, NULL },
    { "e1m8", 40, NULL },
    { "e1m9", 41, NULL },
    { "e2m1", 42, NULL },
    { "e2m2", 43, NULL },
    { "e2m3", 44, NULL },
    { "e2m4", 45, NULL },
    { "e2m5", 46, NULL },
    { "e2m6", 47, NULL },
    { "e2m7", 48, NULL },
    { "e2m8", 49, NULL },
    { "e2m9", 50, NULL },
    { "e3m1", 51, NULL },
    { "e3m2", 52, NULL },
    { "e3m3", 53, NULL },
    { "e3m4", 54, NULL },
    { "e3m5", 55, NULL },
    { "e3m6", 56, NULL },
    { "e3m7", 57, NULL },
    { "e3m8", 58, NULL },
    { "e3m9", 59, NULL },

    { "inter",  63, NULL },
    { "intro",  62, NULL },
    { "bunny",  67, NULL },
    { "victor", 61, NULL },
    { "introa", 68, NULL },
    { "runnin",  1, NULL },
    { "stalks",  2, NULL },
    { "countd",  3, NULL },
    { "betwee",  4, NULL },
    { "doom",    5, NULL },
    { "the_da",  6, NULL },
    { "shawn",   7, NULL },
    { "ddtblu",  8, NULL },
    { "in_cit",  9, NULL },
    { "dead",   10, NULL },
    { "stlks2", 11, NULL },
    { "theda2", 12, NULL },
    { "doom2",  13, NULL },
    { "ddtbl2", 14, NULL },
    { "runni2", 15, NULL },
    { "dead2",  16, NULL },
    { "stlks3", 17, NULL },
    { "romero", 18, NULL },
    { "shawn2", 19, NULL },
    { "messag", 20, NULL },
    { "count2", 21, NULL },
    { "ddtbl3", 22, NULL },
    { "ampie",  23, NULL },
    { "theda3", 24, NULL },
    { "adrian", 25, NULL },
    { "messg2", 26, NULL },
    { "romer2", 27, NULL },
    { "tense",  28, NULL },
    { "shawn3", 29, NULL },
    { "openin", 30, NULL },
    { "evil",   31, NULL },
    { "ultima", 32, NULL },
    { "read_m", 60, NULL },
    { "dm2ttl", 65, NULL },
    { "dm2int", 64, NULL } 
};


//------------------------------------------------------------------------
//
// Information about all the sfx
//

sfxinfo_t S_sfx[NUMSFX_BEX] =
{
	// S_sfx[0] needs to be a dummy for odd reasons.
	{ "none", 0,  0, 0, -1, -1, NULL },

	{ "pistol", 0,  64, 0, -1, -1, NULL },
	{ "shotgn", 0,  64, 0, -1, -1, NULL },
	{ "sgcock", 0,  64, 0, -1, -1, NULL },
	{ "dshtgn", 0,  64, 0, -1, -1, NULL },
	{ "dbopn",  0,  64, 0, -1, -1, NULL },
	{ "dbcls",  0,  64, 0, -1, -1, NULL },
	{ "dbload", 0,  64, 0, -1, -1, NULL },
	{ "plasma", 0,  64, 0, -1, -1, NULL },
	{ "bfg",    0,  64, 0, -1, -1, NULL },
	{ "sawup",  2,  64, 0, -1, -1, NULL },
	{ "sawidl", 2, 118, 0, -1, -1, NULL },
	{ "sawful", 2,  64, 0, -1, -1, NULL },
	{ "sawhit", 2,  64, 0, -1, -1, NULL },
	{ "rlaunc", 0,  64, 0, -1, -1, NULL },
	{ "rxplod", 0,  70, 0, -1, -1, NULL },
	{ "firsht", 0,  70, 0, -1, -1, NULL },
	{ "firxpl", 0,  70, 0, -1, -1, NULL },
	{ "pstart", 18, 100, 0, -1, -1, NULL },
	{ "pstop",  18, 100, 0, -1, -1, NULL },
	{ "doropn", 0,  100, 0, -1, -1, NULL },
	{ "dorcls", 0,  100, 0, -1, -1, NULL },
	{ "stnmov", 18, 119, 0, -1, -1, NULL },
	{ "swtchn", 0,  78, 0, -1, -1, NULL },
	{ "swtchx", 0,  78, 0, -1, -1, NULL },
	{ "plpain", 0,  96, 0, -1, -1, NULL },
	{ "dmpain", 0,  96, 0, -1, -1, NULL },
	{ "popain", 0,  96, 0, -1, -1, NULL },
	{ "vipain", 0,  96, 0, -1, -1, NULL },
	{ "mnpain", 0,  96, 0, -1, -1, NULL },
	{ "pepain", 0,  96, 0, -1, -1, NULL },
	{ "slop",   0,  78, 0, -1, -1, NULL },
	{ "itemup", 20, 78, 0, -1, -1, NULL },
	{ "wpnup",  21, 78, 0, -1, -1, NULL },
	{ "oof",    0,  96, 0, -1, -1, NULL },
	{ "telept", 0,  32, 0, -1, -1, NULL },
	{ "posit1", 3,  98, 0, -1, -1, NULL },
	{ "posit2", 3,  98, 0, -1, -1, NULL },
	{ "posit3", 3,  98, 0, -1, -1, NULL },
	{ "bgsit1", 4,  98, 0, -1, -1, NULL },
	{ "bgsit2", 4,  98, 0, -1, -1, NULL },
	{ "sgtsit", 5,  98, 0, -1, -1, NULL },
	{ "cacsit", 6,  98, 0, -1, -1, NULL },
	{ "brssit", 7,  94, 0, -1, -1, NULL },
	{ "cybsit", 8,  92, 0, -1, -1, NULL },
	{ "spisit", 9,  90, 0, -1, -1, NULL },
	{ "bspsit", 10, 90, 0, -1, -1, NULL },
	{ "kntsit", 11, 90, 0, -1, -1, NULL },
	{ "vilsit", 12, 90, 0, -1, -1, NULL },
	{ "mansit", 13, 90, 0, -1, -1, NULL },
	{ "pesit",  14, 90, 0, -1, -1, NULL },
	{ "sklatk", 0,  70, 0, -1, -1, NULL },
	{ "sgtatk", 0,  70, 0, -1, -1, NULL },
	{ "skepch", 0,  70, 0, -1, -1, NULL },
	{ "vilatk", 0,  70, 0, -1, -1, NULL },
	{ "claw",   0,  70, 0, -1, -1, NULL },
	{ "skeswg", 0,  70, 0, -1, -1, NULL },
	{ "pldeth", 0,  32, 0, -1, -1, NULL },
	{ "pdiehi", 0,  32, 0, -1, -1, NULL },
	{ "podth1", 0,  70, 0, -1, -1, NULL },
	{ "podth2", 0,  70, 0, -1, -1, NULL },
	{ "podth3", 0,  70, 0, -1, -1, NULL },
	{ "bgdth1", 0,  70, 0, -1, -1, NULL },
	{ "bgdth2", 0,  70, 0, -1, -1, NULL },
	{ "sgtdth", 0,  70, 0, -1, -1, NULL },
	{ "cacdth", 0,  70, 0, -1, -1, NULL },
	{ "skldth", 0,  70, 0, -1, -1, NULL },
	{ "brsdth", 0,  32, 0, -1, -1, NULL },
	{ "cybdth", 0,  32, 0, -1, -1, NULL },
	{ "spidth", 0,  32, 0, -1, -1, NULL },
	{ "bspdth", 0,  32, 0, -1, -1, NULL },
	{ "vildth", 0,  32, 0, -1, -1, NULL },
	{ "kntdth", 0,  32, 0, -1, -1, NULL },
	{ "pedth",  0,  32, 0, -1, -1, NULL },
	{ "skedth", 0,  32, 0, -1, -1, NULL },
	{ "posact", 3,  120, 0, -1, -1, NULL },
	{ "bgact",  4,  120, 0, -1, -1, NULL },
	{ "dmact",  15, 120, 0, -1, -1, NULL },
	{ "bspact", 10, 100, 0, -1, -1, NULL },
	{ "bspwlk", 16, 100, 0, -1, -1, NULL },
	{ "vilact", 12, 100, 0, -1, -1, NULL },
	{ "noway",  0,  78, 0, -1, -1, NULL },
	{ "barexp", 0,  60, 0, -1, -1, NULL },
	{ "punch",  0,  64, 0, -1, -1, NULL },
	{ "hoof",   0,  70, 0, -1, -1, NULL },
	{ "metal",  0,  70, 0, -1, -1, NULL },
	{ "chgun",  0,  64, sfx_pistol, 150, 0, NULL },
	{ "tink",   0,  60, 0, -1, -1, NULL },
	{ "bdopn",  0, 100, 0, -1, -1, NULL },
	{ "bdcls",  0, 100, 0, -1, -1, NULL },
	{ "itmbk",  0, 100, 0, -1, -1, NULL },
	{ "flame",  0,  32, 0, -1, -1, NULL },
	{ "flamst", 0,  32, 0, -1, -1, NULL },
	{ "getpow", 0,  60, 0, -1, -1, NULL },
	{ "bospit", 0,  70, 0, -1, -1, NULL },
	{ "boscub", 0,  70, 0, -1, -1, NULL },
	{ "bossit", 0,  70, 0, -1, -1, NULL },
	{ "bospn",  0,  70, 0, -1, -1, NULL },
	{ "bosdth", 0,  70, 0, -1, -1, NULL },
	{ "manatk", 0,  70, 0, -1, -1, NULL },
	{ "mandth", 0,  70, 0, -1, -1, NULL },
	{ "sssit",  0,  70, 0, -1, -1, NULL },
	{ "ssdth",  0,  70, 0, -1, -1, NULL },
	{ "keenpn", 0,  70, 0, -1, -1, NULL },
	{ "keendt", 0,  70, 0, -1, -1, NULL },
	{ "skeact", 0,  70, 0, -1, -1, NULL },
	{ "skesit", 0,  70, 0, -1, -1, NULL },
	{ "skeatk", 0,  70, 0, -1, -1, NULL },
	{ "radio",  0,  60, 0, -1, -1, NULL },

	// BOOM and MBF sounds...
	{ "dgsit",  0,  98, 0, -1, -1, NULL },
	{ "dgatk",  0,  70, 0, -1, -1, NULL },
	{ "dgact",  0, 120, 0, -1, -1, NULL },
	{ "dgdth",  0,  70, 0, -1, -1, NULL },
	{ "dgpain", 0,  96, 0, -1, -1, NULL }
};


//------------------------------------------------------------------------

void Sounds::Startup(void)
{
	for (int s = 0; s < NUMSFX_BEX; s++)
	{
		free(S_sfx[s].new_name);
		S_sfx[s].new_name = NULL;
	}

	for (int m = 0; m < NUMMUSIC; m++)
	{
		free(S_music[m].new_name);
		S_music[m].new_name = NULL;
	}
}

namespace Sounds
{
	bool some_sound_modified = false;
	bool got_one;

	
	void MarkSound(int s_num)
	{
		// can happen since the binary patches contain the dummy sound
		if (s_num == sfx_None)
			return;

		assert(1 <= s_num && s_num < NUMSFX_BEX);

		some_sound_modified = true;
	}

	void AlterSound(int new_val)
	{
		int s_num = Patch::active_obj;
		const char *deh_field = Patch::line_buf;

		assert(0 <= s_num && s_num < NUMSFX_BEX);

		if (StrCaseCmpPartial(deh_field, "Zero") == 0 ||
		    StrCaseCmpPartial(deh_field, "Neg. One") == 0)
			return;

		if (StrCaseCmp(deh_field, "Offset") == 0)
		{
			PrintWarn("Line %d: raw sound Offset not supported.\n", Patch::line_num);
			return;
		}

		if (StrCaseCmp(deh_field, "Value") == 0)  // priority
		{
			if (new_val < 0)
			{
				PrintWarn("Line %d: bad sound priority value: %d.\n",
					Patch::line_num, new_val);
				new_val = 0;
			}

			Storage::RememberMod(&S_sfx[s_num].priority, new_val);

			MarkSound(s_num);
			return;
		}

		if (StrCaseCmp(deh_field, "Zero/One") == 0)  // singularity, ignored
			return;

		PrintWarn("UNKNOWN SOUND FIELD: %s\n", deh_field);
	}

	const char *GetEdgeSfxName(int sound_id)
	{
		assert(sound_id != sfx_None);

		switch (sound_id)
		{
			// EDGE uses different names for the DOG sounds
			case sfx_dgsit:  return "DOG_SIGHT";
			case sfx_dgatk:  return "DOG_BITE";
			case sfx_dgact:  return "DOG_LOOK";
			case sfx_dgdth:  return "DOG_DIE";
			case sfx_dgpain: return "DOG_PAIN";

			default: break;
		}

		return S_sfx[sound_id].orig_name;
	}

	const char *GetSound(int sound_id)
	{
		assert(sound_id != sfx_None);
		assert(strlen(S_sfx[sound_id].orig_name) < 16);

		// handle random sounds
		switch (sound_id)
		{
			case sfx_podth1: case sfx_podth2: case sfx_podth3:
				return "\"PODTH?\"";

			case sfx_posit1: case sfx_posit2: case sfx_posit3:
				return "\"POSIT?\"";

			case sfx_bgdth1: case sfx_bgdth2:
				return "\"BGDTH?\"";

			case sfx_bgsit1: case sfx_bgsit2:
				return "\"BGSIT?\"";

			default: break;
		}

		static char name_buf[40];

		sprintf(name_buf, "\"%s\"", StrUpper(GetEdgeSfxName(sound_id)));

		return name_buf;
	}

	void BeginSoundLump(void)
	{
		WAD::NewLump("DDFSFX");

		WAD::Printf(GEN_BY_COMMENT);
		WAD::Printf("<SOUNDS>\n\n");

		if (EDGE127_BUG)
			WAD::Printf("#CLEARALL\n\n");
	}

	void FinishSoundLump(void)
	{
		if (EDGE127_BUG)
		{
			WAD::Printf(
				"[JPRISE] LUMP_NAME=\"DSJPRISE\"; SINGULAR=29;\n"
				"[JPMOVE] LUMP_NAME=\"DSJPMOVE\"; SINGULAR=29;\n"
				"[JPIDLE] LUMP_NAME=\"DSJPIDLE\"; SINGULAR=29;\n"
				"[JPDOWN] LUMP_NAME=\"DSJPDOWN\"; SINGULAR=29;\n"
				"[JPFLOW] LUMP_NAME=\"DSJPFLOW\"; SINGULAR=29;\n"
				"[CRUSH] LUMP_NAME=\"DSCRUSH\"; PRIORITY=100;\n"
			);
		}

		WAD::Printf("\n");
		WAD::FinishLump();
	}

	void BeginMusicLump(void)
	{
		WAD::NewLump("DDFPLAY");

		WAD::Printf(GEN_BY_COMMENT);
		WAD::Printf("<PLAYLISTS>\n\n");
	}

	void FinishMusicLump(void)
	{
		WAD::Printf("\n");
		WAD::FinishLump();
	}

	void WriteSound(int s_num)
	{
		sfxinfo_t *sound = S_sfx + s_num;

		if (! got_one)
		{
			got_one = true;
			BeginSoundLump();
		}

		WAD::Printf("[%s]\n", StrUpper(GetEdgeSfxName(s_num)));

		const char *lump = sound->new_name ? sound->new_name : sound->orig_name;

		if (sound->link)
		{
			sfxinfo_t *link = S_sfx + sound->link;

			lump = link->new_name ? link->new_name : GetEdgeSfxName(sound->link);
		}

		WAD::Printf("LUMP_NAME = \"DS%s\";\n", StrUpper(lump));
		WAD::Printf("PRIORITY = %d;\n", sound->priority);

		if (sound->singularity != 0)
			WAD::Printf("SINGULAR = %d;\n", sound->singularity);

		if (s_num == sfx_stnmov)
			WAD::Printf("LOOP = TRUE;\n");

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

		const char *lump = mus->new_name ? mus->new_name : mus->orig_name;

		WAD::Printf("MUSICINFO = MUS:LUMP:\"D_%s\";\n", StrUpper(lump));
	}
}

void Sounds::ConvertSFX(void)
{
	if (! all_mode && ! some_sound_modified)
		return;

	got_one = false;

	for (int i = 1; i < NUMSFX_BEX; i++)
	{
	    if (! EDGE127_BUG && ! all_mode && S_sfx[i].new_name == NULL)
			continue;

		WriteSound(i);
	}
		
	if (got_one)
		FinishSoundLump();
}

void Sounds::ConvertMUS(void)
{
	got_one = false;

	for (int i = 1; i < NUMMUSIC; i++)
	{
	    if (! all_mode && ! S_music[i].new_name)
			continue;

		WriteMusic(i);
	}
		
	if (got_one)
		FinishMusicLump();
}


//------------------------------------------------------------------------

bool Sounds::ReplaceSound(const char *before, const char *after)
{
	for (int i = 1; i < NUMSFX_BEX; i++)
	{
		if (StrCaseCmp(S_sfx[i].orig_name, before) != 0)
			continue;

		if (S_sfx[i].new_name)
			free(S_sfx[i].new_name);

		S_sfx[i].new_name = StringDup(after);

		MarkSound(i);

		return true;
	}

	return false;
}

bool Sounds::ReplaceMusic(const char *before, const char *after)
{
	for (int j = 1; j < NUMMUSIC; j++)
	{
		if (StrCaseCmp(S_music[j].orig_name, before) != 0)
			continue;

		if (S_music[j].new_name)
			free(S_music[j].new_name);

		S_music[j].new_name = StringDup(after);

		return true;
	}

	return false;
}

void Sounds::AlterBexSound(const char *new_val)
{
	const char *old_val = Patch::line_buf;

	if (strlen(old_val) < 1 || strlen(old_val) > 6)
	{
		PrintWarn("Bad length for sound name '%s'.\n", old_val);
		return;
	}

	if (strlen(new_val) < 1 || strlen(new_val) > 6)
	{
		PrintWarn("Bad length for sound name '%s'.\n", new_val);
		return;
	}

	if (! ReplaceSound(old_val, new_val))
		PrintWarn("Line %d: unknown sound name '%s'.\n",
			Patch::line_num, old_val);
}

void Sounds::AlterBexMusic(const char *new_val)
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
