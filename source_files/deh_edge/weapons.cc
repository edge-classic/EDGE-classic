//------------------------------------------------------------------------
//  WEAPON Conversion
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
#include "weapons.h"

#include "ammo.h"
#include "buffer.h"
#include "dh_plugin.h"
#include "info.h"
#include "frames.h"
#include "misc.h"
#include "mobj.h"
#include "patch.h"
#include "storage.h"
#include "sounds.h"
#include "system.h"
#include "things.h"
#include "wad.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

namespace Deh_Edge
{

#define WF_FREE       'f'
#define WF_REF_INACC  'r'
#define WF_DANGEROUS  'd'
#define WF_NO_THRUST  't'
#define WF_FEEDBACK   'b'

weaponinfo_t weapon_info[NUMWEAPONS] =
{
    {
		"FIST", am_noammo, 0,  1,0, "f",
		S_PUNCHUP, S_PUNCHDOWN, S_PUNCH, S_PUNCH1, S_NULL
    },	
    {
		"PISTOL", am_bullet, 1,  2,2, "fr",
		S_PISTOLUP, S_PISTOLDOWN, S_PISTOL, S_PISTOL1, S_PISTOLFLASH
    },	
    {
		"SHOTGUN", am_shell, 1,  3,3, NULL,
		S_SGUNUP, S_SGUNDOWN, S_SGUN, S_SGUN1, S_SGUNFLASH1
    },
    {
		"CHAINGUN", am_bullet, 1,  4,5, "r",
		S_CHAINUP, S_CHAINDOWN, S_CHAIN, S_CHAIN1, S_CHAINFLASH1
    },
    {
		"ROCKET_LAUNCHER", am_rocket, 1,  5,7, "d",
		S_MISSILEUP, S_MISSILEDOWN, S_MISSILE, S_MISSILE1, S_MISSILEFLASH1
    },
    {
		"PLASMA_RIFLE", am_cell, 1,  6,6, NULL,
		S_PLASMAUP, S_PLASMADOWN, S_PLASMA, S_PLASMA1, S_PLASMAFLASH1
    },
    {
		"BFG_9000", am_cell, 40,  7,8, "d",
		S_BFGUP, S_BFGDOWN, S_BFG, S_BFG1, S_BFGFLASH1
    },
    {
		"CHAINSAW", am_noammo, 0,  1,1, "bt",
		S_SAWUP, S_SAWDOWN, S_SAW, S_SAW1, S_NULL
    },
    {
		"SUPER_SHOTGUN", am_shell, 2,  3,4, NULL,
		S_DSGUNUP, S_DSGUNDOWN, S_DSGUN, S_DSGUN1, S_DSGUNFLASH1
    },	
};

bool weapon_modified[NUMWEAPONS];


//----------------------------------------------------------------------------

void Weapons::Startup(void)
{
	memset(weapon_modified, 0, sizeof(weapon_modified));
}

namespace Weapons
{
	bool got_one;

	void MarkWeapon(int wp_num)
	{
		assert (0 <= wp_num && wp_num < NUMWEAPONS);

		weapon_modified[wp_num] = true;
	}

	void BeginLump(void)
	{
		WAD::NewLump("DDFWEAP");

		WAD::Printf(GEN_BY_COMMENT);
		WAD::Printf("<WEAPONS>\n\n");
	}

	void FinishLump(void)
	{
		WAD::Printf("\n");
		WAD::FinishLump();
	}

	void HandleFlags(const weaponinfo_t *info, int w_num)
	{
		if (! info->flags)
			return;

		if (strchr(info->flags, WF_FREE))
			WAD::Printf("FREE = TRUE;\n");

		if (strchr(info->flags, WF_REF_INACC))
			WAD::Printf("REFIRE_INACCURATE = TRUE;\n");

		if (strchr(info->flags, WF_DANGEROUS))
			WAD::Printf("DANGEROUS = TRUE;\n");

		if (strchr(info->flags, WF_NO_THRUST))
			WAD::Printf("NOTHRUST = TRUE;\n");

		if (strchr(info->flags, WF_FEEDBACK))
			WAD::Printf("FEEDBACK = TRUE;\n");
	}

	void HandleSounds(const weaponinfo_t *info, int w_num)
	{
		if (w_num == wp_chainsaw)
		{
			WAD::Printf("START_SOUND = %s;\n",   Sounds::GetSound(sfx_sawup));
			WAD::Printf("IDLE_SOUND = %s;\n",    Sounds::GetSound(sfx_sawidl));
			WAD::Printf("ENGAGED_SOUND = %s;\n", Sounds::GetSound(sfx_sawful));
			return;
		}

		// otherwise nothing.
	}

	void HandleFrames(const weaponinfo_t *info, int w_num)
	{
		Frames::ResetAll();

		// --- collect states into groups ---

		bool has_flash = Frames::CheckWeaponFlash(info->atkstate);

		int count = 0;

		if (has_flash)
			count += Frames::BeginGroup(info->flashstate,  'f');

		count += Frames::BeginGroup(info->atkstate,    'a');
		count += Frames::BeginGroup(info->readystate,  'r');
		count += Frames::BeginGroup(info->downstate,   'd');
		count += Frames::BeginGroup(info->upstate,     'u');

		if (count == 0)
		{
			PrintWarn("Weapon [%s] has no states.\n", info->ddf_name);
			return;
		}

		Frames::SpreadGroups();

		Frames::OutputGroup(info->upstate,     'u');
		Frames::OutputGroup(info->downstate,   'd');
		Frames::OutputGroup(info->readystate,  'r');
		Frames::OutputGroup(info->atkstate,    'a');

		if (has_flash)
			Frames::OutputGroup(info->flashstate, 'f');
	}

	void HandleAttacks(const weaponinfo_t *info, int w_num)
	{
		int count = 
			(Frames::attack_slot[0] ? 1 : 0) +
			(Frames::attack_slot[1] ? 1 : 0) +
			(Frames::attack_slot[2] ? 1 : 0);

		if (count == 0)
			return;

		if (count > 1)
			PrintWarn("Multiple attacks used in weapon [%s]\n", info->ddf_name);


		WAD::Printf("\n");

		const char *atk = Frames::attack_slot[0];

		if (! atk)
			atk = Frames::attack_slot[1];

		if (! atk)
			atk = Frames::attack_slot[2];

		assert(atk != NULL);

		WAD::Printf("ATTACK = %s;\n", atk);
	}

	void ConvertWeapon(int w_num)
	{
		if (! got_one)
		{
			got_one = true;
			BeginLump();
		}

		const weaponinfo_t *info = weapon_info + w_num;

		WAD::Printf("[%s]\n", info->ddf_name);

		WAD::Printf("AMMOTYPE = %s;\n", Ammo::GetAmmo(info->ammo));

		if (w_num == wp_bfg)
			WAD::Printf("AMMOPERSHOT = %d;\n", Misc::bfg_cells_per_shot);
		else if (info->ammo_per_shot != 0)
			WAD::Printf("AMMOPERSHOT = %d;\n", info->ammo_per_shot);

		WAD::Printf("AUTOMATIC = TRUE;\n");

		WAD::Printf("BINDKEY = %d;\n", info->bind_key);
		WAD::Printf("PRIORITY = %d;\n", info->priority);

		HandleFlags(info, w_num);
		HandleSounds(info, w_num);
		HandleFrames(info, w_num);
		HandleAttacks(info, w_num);

		WAD::Printf("\n");
	}
}

void Weapons::ConvertWEAP(void)
{
	got_one = false;

	for (int i = 0; i < NUMWEAPONS; i++)
	{
	    if (! all_mode && ! weapon_modified[i])
			continue;

		ConvertWeapon(i);
	}
		
	if (got_one)
		FinishLump();
}


//------------------------------------------------------------------------

namespace Weapons
{
	const fieldreference_t weapon_field[] =
	{
		{ "Ammo type", &weapon_info[0].ammo, FT_AMMO },

		// -AJA- these first two fields have misleading dehacked names
		{ "Deselect frame", &weapon_info[0].upstate,    FT_FRAME },
		{ "Select frame",   &weapon_info[0].downstate,  FT_FRAME },
		{ "Bobbing frame",  &weapon_info[0].readystate, FT_FRAME },
		{ "Shooting frame", &weapon_info[0].atkstate,   FT_FRAME },
		{ "Firing frame",   &weapon_info[0].flashstate, FT_FRAME },

		{ NULL, NULL, 0 }   // End sentinel
	};
}

void Weapons::AlterWeapon(int new_val)
{
	int wp_num = Patch::active_obj;
	const char *field_name = Patch::line_buf;

	assert(0 <= wp_num && wp_num < NUMWEAPONS);

	int stride = ((char*) (weapon_info+1)) - ((char*) weapon_info);

	if (! Things::AlterOneField(weapon_field, field_name, wp_num * stride, new_val))
	{
		PrintWarn("UNKNOWN WEAPON FIELD: %s\n", field_name);
		return;
	}

	MarkWeapon(wp_num);
}

}  // Deh_Edge
