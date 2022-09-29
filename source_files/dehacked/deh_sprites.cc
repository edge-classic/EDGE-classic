//------------------------------------------------------------------------
//  SPRITES
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

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "deh_i_defs.h"
#include "deh_edge.h"

#include "deh_buffer.h"
#include "deh_info.h"
#include "deh_english.h"
#include "deh_frames.h"
#include "deh_mobj.h"
#include "deh_patch.h"
#include "deh_text.h"
#include "deh_storage.h"
#include "deh_sounds.h"
#include "deh_sprites.h"
#include "deh_system.h"
#include "deh_util.h"
#include "deh_wad.h"


namespace Deh_Edge
{

spritename_t sprnames[NUMSPRITES_BEX] =
{
    {"TROO",NULL}, {"SHTG",NULL}, {"PUNG",NULL}, {"PISG",NULL},
	{"PISF",NULL}, {"SHTF",NULL}, {"SHT2",NULL}, {"CHGG",NULL},
	{"CHGF",NULL}, {"MISG",NULL}, {"MISF",NULL}, {"SAWG",NULL},
	{"PLSG",NULL}, {"PLSF",NULL}, {"BFGG",NULL}, {"BFGF",NULL},
	{"BLUD",NULL}, {"PUFF",NULL}, {"BAL1",NULL}, {"BAL2",NULL},
	{"PLSS",NULL}, {"PLSE",NULL}, {"MISL",NULL}, {"BFS1",NULL},
	{"BFE1",NULL}, {"BFE2",NULL}, {"TFOG",NULL}, {"IFOG",NULL},
	{"PLAY",NULL}, {"POSS",NULL}, {"SPOS",NULL}, {"VILE",NULL},
	{"FIRE",NULL}, {"FATB",NULL}, {"FBXP",NULL}, {"SKEL",NULL},
	{"MANF",NULL}, {"FATT",NULL}, {"CPOS",NULL}, {"SARG",NULL},
	{"HEAD",NULL}, {"BAL7",NULL}, {"BOSS",NULL}, {"BOS2",NULL},
	{"SKUL",NULL}, {"SPID",NULL}, {"BSPI",NULL}, {"APLS",NULL},
	{"APBX",NULL}, {"CYBR",NULL}, {"PAIN",NULL}, {"SSWV",NULL},
	{"KEEN",NULL}, {"BBRN",NULL}, {"BOSF",NULL}, {"ARM1",NULL},
	{"ARM2",NULL}, {"BAR1",NULL}, {"BEXP",NULL}, {"FCAN",NULL},
	{"BON1",NULL}, {"BON2",NULL}, {"BKEY",NULL}, {"RKEY",NULL},
	{"YKEY",NULL}, {"BSKU",NULL}, {"RSKU",NULL}, {"YSKU",NULL},
	{"STIM",NULL}, {"MEDI",NULL}, {"SOUL",NULL}, {"PINV",NULL},
	{"PSTR",NULL}, {"PINS",NULL}, {"MEGA",NULL}, {"SUIT",NULL},
	{"PMAP",NULL}, {"PVIS",NULL}, {"CLIP",NULL}, {"AMMO",NULL},
	{"ROCK",NULL}, {"BROK",NULL}, {"CELL",NULL}, {"CELP",NULL},
	{"SHEL",NULL}, {"SBOX",NULL}, {"BPAK",NULL}, {"BFUG",NULL},
	{"MGUN",NULL}, {"CSAW",NULL}, {"LAUN",NULL}, {"PLAS",NULL},
	{"SHOT",NULL}, {"SGN2",NULL}, {"COLU",NULL}, {"SMT2",NULL},
	{"GOR1",NULL}, {"POL2",NULL}, {"POL5",NULL}, {"POL4",NULL},
	{"POL3",NULL}, {"POL1",NULL}, {"POL6",NULL}, {"GOR2",NULL},
	{"GOR3",NULL}, {"GOR4",NULL}, {"GOR5",NULL}, {"SMIT",NULL},
	{"COL1",NULL}, {"COL2",NULL}, {"COL3",NULL}, {"COL4",NULL},
	{"CAND",NULL}, {"CBRA",NULL}, {"COL6",NULL}, {"TRE1",NULL},
	{"TRE2",NULL}, {"ELEC",NULL}, {"CEYE",NULL}, {"FSKU",NULL},
	{"COL5",NULL}, {"TBLU",NULL}, {"TGRN",NULL}, {"TRED",NULL},
	{"SMBT",NULL}, {"SMGT",NULL}, {"SMRT",NULL}, {"HDB1",NULL},
	{"HDB2",NULL}, {"HDB3",NULL}, {"HDB4",NULL}, {"HDB5",NULL},
	{"HDB6",NULL}, {"POB1",NULL}, {"POB2",NULL}, {"BRS1",NULL},
	{"TLMP",NULL}, {"TLP2",NULL},

	// BOOM/MBF/Doom Retro:
	{"TNT1",NULL}, {"DOGS",NULL}, {"PLS1",NULL}, {"PLS2",NULL},
    {"BON3",NULL}, {"BON4",NULL}, {"BLD2",NULL},

    // DEHEXTRA
    {"SP00",NULL},{"SP01",NULL}, {"SP02",NULL}, {"SP03",NULL},
    {"SP04",NULL},{"SP05",NULL}, {"SP06",NULL}, {"SP07",NULL},
    {"SP08",NULL},{"SP09",NULL}, {"SP10",NULL}, {"SP11",NULL},
    {"SP12",NULL},{"SP13",NULL}, {"SP14",NULL}, {"SP15",NULL},
    {"SP16",NULL},{"SP17",NULL}, {"SP18",NULL}, {"SP19",NULL},
    {"SP20",NULL},{"SP21",NULL}, {"SP22",NULL}, {"SP23",NULL},
    {"SP24",NULL},{"SP25",NULL}, {"SP26",NULL}, {"SP27",NULL},
    {"SP28",NULL},{"SP29",NULL}, {"SP30",NULL}, {"SP31",NULL},
    {"SP32",NULL},{"SP33",NULL}, {"SP34",NULL}, {"SP35",NULL},
    {"SP36",NULL},{"SP37",NULL}, {"SP38",NULL}, {"SP39",NULL},
    {"SP40",NULL},{"SP41",NULL}, {"SP42",NULL}, {"SP43",NULL},
    {"SP44",NULL},{"SP45",NULL}, {"SP46",NULL}, {"SP47",NULL},
    {"SP48",NULL},{"SP49",NULL}, {"SP50",NULL}, {"SP51",NULL},
    {"SP52",NULL},{"SP53",NULL}, {"SP54",NULL}, {"SP55",NULL},
    {"SP56",NULL},{"SP57",NULL}, {"SP58",NULL}, {"SP59",NULL},
    {"SP60",NULL},{"SP61",NULL}, {"SP62",NULL}, {"SP63",NULL},
    {"SP64",NULL},{"SP65",NULL}, {"SP66",NULL}, {"SP67",NULL},
    {"SP68",NULL},{"SP69",NULL}, {"SP70",NULL}, {"SP71",NULL},
    {"SP72",NULL},{"SP73",NULL}, {"SP74",NULL}, {"SP75",NULL},
    {"SP76",NULL},{"SP77",NULL}, {"SP78",NULL}, {"SP79",NULL},
    {"SP80",NULL},{"SP81",NULL}, {"SP82",NULL}, {"SP83",NULL},
    {"SP84",NULL},{"SP85",NULL}, {"SP86",NULL}, {"SP87",NULL},
    {"SP88",NULL},{"SP89",NULL}, {"SP90",NULL}, {"SP91",NULL},
    {"SP92",NULL},{"SP93",NULL}, {"SP94",NULL}, {"SP95",NULL},
    {"SP96",NULL},{"SP97",NULL}, {"SP98",NULL}, {"SP99",NULL},
};

//------------------------------------------------------------------------

void Sprites::Init()
{
	for (int s = 0; s < NUMSPRITES_BEX; s++)
	{
		free(sprnames[s].new_name);
		sprnames[s].new_name = NULL;
	}
}


void Sprites::Shutdown()
{
}


void Sprites::SpriteDependencies()
{
	for (int i = 0; i < NUMSPRITES_BEX; i++)
	{
		if (! sprnames[i].new_name)
			continue;

		// find this sprite amongst the states...
		for (int st = 1; st < NUMSTATES_BEX; st++)
			if (states[st].sprite == i)
				Frames::MarkState(st);
	}
}


bool Sprites::ReplaceSprite(const char *before, const char *after)
{
	assert(strlen(before) == 4);
	assert(strlen(after)  == 4);

	for (int i = 0; i < NUMSPRITES_BEX; i++)
	{
		spritename_t *spr = sprnames + i;

		if (StrCaseCmp(before, spr->orig_name) != 0)
			continue;

		if (! spr->new_name)
			spr->new_name = StringNew(5);

		strcpy(spr->new_name, after);

		return true;
	}

	return false;
}


void Sprites::AlterBexSprite(const char *new_val)
{
	const char *old_val = Patch::line_buf;

	if (strlen(old_val) != 4)
	{
		PrintWarn("Bad length for sprite name '%s'.\n", old_val);
		return;
	}

	if (strlen(new_val) != 4)
	{
		PrintWarn("Bad length for sprite name '%s'.\n", new_val);
		return;
	}

	if (! ReplaceSprite(old_val, new_val))
		PrintWarn("Line %d: unknown sprite name '%s'.\n",
			Patch::line_num, old_val);
}


const char * Sprites::GetSprite(int spr_num)
{
	assert(0 <= spr_num && spr_num < NUMSPRITES_BEX);

	const spritename_t *spr = sprnames + spr_num;

	const char *name = spr->new_name ? spr->new_name : spr->orig_name;

	// Boom support: TNT1 is an invisible sprite
	if (StrCaseCmp(name, "TNT1") == 0)
		return "NULL";
	
	return name;
}

}  // Deh_Edge
