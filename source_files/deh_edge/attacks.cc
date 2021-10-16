//----------------------------------------------------------------------------
//  ATTACK Conversion
//----------------------------------------------------------------------------
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
#include "attacks.h"

#include "dh_plugin.h"
#include "info.h"
#include "frames.h"
#include "misc.h"
#include "mobj.h"
#include "sounds.h"
#include "system.h"
#include "things.h"
#include "wad.h"

#include <stdio.h>
#include <string.h>

namespace Deh_Edge
{

#define KF_FACE_TARG  'F'
#define KF_SIGHT      'S'
#define KF_KILL_FAIL  'K'
#define KF_NO_TRACE   't'
#define KF_TOO_CLOSE  'c'
#define KF_KEEP_FIRE  'e'
#define KF_PUFF_SMK   'p'


namespace Attacks
{
	bool got_one;
	bool flag_got_one;

	void BeginLump(void)
	{
		WAD::NewLump("DDFATK");

		WAD::Printf(GEN_BY_COMMENT);
		WAD::Printf("<ATTACKS>\n\n");
	}

	void FinishLump(void)
	{
		WAD::Printf("\n");
		WAD::FinishLump();
	}

	typedef struct
	{
		int mt_num;
		const char *atk_type;
		int atk_height;
		int translucency;
		const char *flags;
	}
	attackextra_t;

	const attackextra_t attack_extra[] =
	{
		{ MT_FIRE,       "TRACKER", 0, 75, "FS" },
		{ MT_TRACER,     "PROJECTILE", 48, 75, "cptF" },
		{ MT_FATSHOT,    "FIXED_SPREADER", 32, 75, "" },
		{ MT_TROOPSHOT,  "PROJECTILE", 32, 75, "F" },
		{ MT_BRUISERSHOT,"PROJECTILE", 32, 75, "F" },
		{ MT_HEADSHOT,   "PROJECTILE", 32, 75, "F" },
		{ MT_ARACHPLAZ,  "PROJECTILE", 16, 50, "eF" },
		{ MT_ROCKET,     "PROJECTILE", 44, 75, "FK" },
		{ MT_PLASMA,     "PROJECTILE", 32, 75, "eK" },
		{ MT_BFG,        "PROJECTILE", 32, 50, "K" },
		{ MT_EXTRABFG,   "SPRAY", 0, 75, "" },
		{ MT_SPAWNSHOT,  "SHOOTTOSPOT", 16, 100, "" },

		{ -1, NULL, 0, 0, "" }
	};

	void HandleSounds(const mobjinfo_t *info, int mt_num)
	{
		if (info->seesound != sfx_None)
			WAD::Printf("LAUNCH_SOUND = %s;\n", Sounds::GetSound(info->seesound));

		if (info->deathsound != sfx_None)
			WAD::Printf("DEATH_SOUND = %s;\n", Sounds::GetSound(info->deathsound));

		if (mt_num == MT_FIRE)
		{
			WAD::Printf("ATTEMPT_SOUND = %s;\n", Sounds::GetSound(sfx_vilatk));
			WAD::Printf("ENGAGED_SOUND = %s;\n", Sounds::GetSound(sfx_barexp));
		}

		if (mt_num == MT_FATSHOT)
			WAD::Printf("ATTEMPT_SOUND = %s;\n", Sounds::GetSound(sfx_manatk));
	}

	void HandleFrames(const mobjinfo_t *info, int mt_num)
	{
		Frames::ResetAll();

		// special cases...

		if (mt_num == MT_SPAWNSHOT)
		{
			// EDGE merges MT_SPAWNSHOT and MT_SPAWNFIRE into a single
			// attack ("BRAIN_CUBE").

			int count = 0;

			count += Frames::BeginGroup(mobjinfo[MT_SPAWNFIRE].spawnstate, 'D');
			count += Frames::BeginGroup(info->spawnstate, 'S');

			if (count != 2)
				PrintWarn("Brain cube is missing spawn/fire states.\n");

			if (count == 0)
				return;

			Frames::SpreadGroups();

			Frames::OutputGroup(info->spawnstate, 'S');
			Frames::OutputGroup(mobjinfo[MT_SPAWNFIRE].spawnstate, 'D');

			return;
		}

		// --- collect states into groups ---

		int count = 0;

		count += Frames::BeginGroup(info->deathstate,   'D');
		count += Frames::BeginGroup(info->seestate,     'E');
		count += Frames::BeginGroup(info->spawnstate,   'S');

		if (count == 0)
		{
			PrintWarn("Attack [%s] has no states.\n", info->name + 1);
			return;
		}

		Frames::SpreadGroups();

		Frames::OutputGroup(info->spawnstate,   'S');
		Frames::OutputGroup(info->seestate,     'E');
		Frames::OutputGroup(info->deathstate,   'D');
	}

	void AddAtkSpecial(const char *name)
	{
		if (! flag_got_one)
		{
			flag_got_one = true;
			WAD::Printf("ATTACK_SPECIAL = ");
		}
		else
			WAD::Printf(",");

		WAD::Printf("%s", name);
	}

	void HandleAtkSpecials(const mobjinfo_t *info, int mt_num,
		const attackextra_t *ext, bool plr_rocket)
	{
		flag_got_one = false;

		if (strchr(ext->flags, KF_FACE_TARG) && ! plr_rocket)
			AddAtkSpecial("FACE_TARGET");

		if (strchr(ext->flags, KF_SIGHT))
			AddAtkSpecial("NEED_SIGHT");

		if (strchr(ext->flags, KF_KILL_FAIL))
			AddAtkSpecial("KILL_FAILED_SPAWN");

		if (strchr(ext->flags, KF_PUFF_SMK))
			AddAtkSpecial("SMOKING_TRACER");

		if (flag_got_one)
			WAD::Printf(";\n");
	}

	void CheckPainElemental(void)
	{
		// these two attacks refer to the LOST_SOUL's missile states.
		// Hence we need to check that they didn't go away.

		mobjinfo_t *skull = mobjinfo + MT_SKULL;

		if (skull->missilestate != S_NULL)
		{
			state_t *mis_st = states + skull->missilestate;

			if (mis_st->tics >= 0 && mis_st->nextstate != S_NULL)
				return;
		}

		// need to write out new versions

		if (! got_one)
		{
			got_one = true;
			BeginLump();
		}

		const char *spawn_at = NULL;

		if (skull->seestate != S_NULL)
			spawn_at = "CHASE:1";
		else if (skull->missilestate != S_NULL)
			spawn_at = "MISSILE:1";
		else if (skull->meleestate != S_NULL)
			spawn_at = "MELEE:1";
		else
			spawn_at = "IDLE:1";

		WAD::Printf("[ELEMENTAL_SPAWNER]\n");
		WAD::Printf("ATTACKTYPE = SPAWNER;\n");
		WAD::Printf("ATTACK_HEIGHT = 8;\n");
		WAD::Printf("ATTACK_SPECIAL = PRESTEP_SPAWN,FACE_TARGET;\n");
		WAD::Printf("SPAWNED_OBJECT = LOST_SOUL;\n");
		WAD::Printf("SPAWN_OBJECT_STATE = %s;\n", spawn_at);

		if (target_version >= 129)
			WAD::Printf("SPAWN_LIMIT = 21;\n");

		WAD::Printf("\n");
		WAD::Printf("[ELEMENTAL_DEATHSPAWN]\n");
		WAD::Printf("ATTACKTYPE = TRIPLE_SPAWNER;\n");
		WAD::Printf("ATTACK_HEIGHT = 8;\n");
		WAD::Printf("ATTACK_SPECIAL = PRESTEP_SPAWN,FACE_TARGET;\n");
		WAD::Printf("SPAWNED_OBJECT = LOST_SOUL;\n");
		WAD::Printf("SPAWN_OBJECT_STATE = %s;\n", spawn_at);
	}

	void ConvertAttack(const mobjinfo_t *info, int mt_num, bool plr_rocket);
}

void Attacks::ConvertAttack(const mobjinfo_t *info, int mt_num, bool plr_rocket)
{
	if (! got_one)
	{
		got_one = true;
		BeginLump();
	}

	if (info->name[0] != '*' || info->name[1] == 0)  // thing
		return;

	if (plr_rocket)
		WAD::Printf("[%s]\n", "PLAYER_MISSILE");
	else
		WAD::Printf("[%s]\n", info->name + 1);

	// find attack in the extra table...
	const attackextra_t *ext = NULL;

	for (int j = 0; attack_extra[j].atk_type; j++)
		if (attack_extra[j].mt_num == mt_num)
		{
			ext = attack_extra + j;
			break;
		}
	
	if (! ext)
		InternalError("Missing attack %s in extra table.\n", info->name + 1);

	WAD::Printf("ATTACKTYPE = %s;\n", ext->atk_type);

	WAD::Printf("RADIUS = %1.1f;\n", F_FIXED(info->radius));
	WAD::Printf("HEIGHT = %1.1f;\n", F_FIXED(info->height));

	if (info->spawnhealth != 1000)
		WAD::Printf("SPAWNHEALTH = %d;\n", info->spawnhealth);

	if (info->speed != 0)
		WAD::Printf("SPEED = %s;\n", Things::GetSpeed(info->speed));

	if (info->mass != 100)
		WAD::Printf("MASS = %d;\n", info->mass);

	if (mt_num == MT_BRUISERSHOT)
		WAD::Printf("FAST = 1.4;\n");
	else if (mt_num == MT_TROOPSHOT || mt_num == MT_HEADSHOT)
		WAD::Printf("FAST = 2.0;\n");

	if (plr_rocket)
		WAD::Printf("ATTACK_HEIGHT = 32;\n");
	else if (ext->atk_height != 0)
		WAD::Printf("ATTACK_HEIGHT = %d;\n", ext->atk_height);

	if (mt_num == MT_FIRE)
	{
		WAD::Printf("DAMAGE.VAL = 20;\n");
		WAD::Printf("EXPLODE_DAMAGE.VAL = 70;\n");
	}
	else if (mt_num == MT_EXTRABFG)
	{
		WAD::Printf("DAMAGE.VAL   = 65;\n");
		WAD::Printf("DAMAGE.ERROR = 50;\n");
	}
	else if (info->damage > 0)
	{
		WAD::Printf("DAMAGE.VAL = %d;\n", info->damage);
		WAD::Printf("DAMAGE.MAX = %d;\n", info->damage * 8);
	}

	if (mt_num == MT_BFG)
		WAD::Printf("SPARE_ATTACK = BFG9000_SPRAY;\n");

	if (ext->translucency != 100)
		WAD::Printf("TRANSLUCENCY = %d%%;\n", ext->translucency);

	if (strchr(ext->flags, KF_PUFF_SMK))
		WAD::Printf("PUFF = SMOKE;\n");
	
	if (strchr(ext->flags, KF_TOO_CLOSE))
		WAD::Printf("TOO_CLOSE_RANGE = 196;\n");

	if (strchr(ext->flags, KF_NO_TRACE))
	{
		WAD::Printf("NO_TRACE_CHANCE = 50%%;\n");

		// workaround for EDGE's behaviour of A_Tracer
		if (target_version >= 129)
			WAD::Printf("TRACE_ANGLE = 9;\n");
	}

	if (strchr(ext->flags, KF_KEEP_FIRE))
		WAD::Printf("KEEP_FIRING_CHANCE = 4%%;\n");

	HandleAtkSpecials(info, mt_num, ext, plr_rocket);
	HandleSounds(info, mt_num);
	HandleFrames(info, mt_num);

	WAD::Printf("\n");

	Things::HandleFlags(info, mt_num, 0);

	if (Frames::attack_slot[0] || Frames::attack_slot[1] ||
	    Frames::attack_slot[2])
	{
		PrintWarn("Attack [%s] contained an attacking action.\n", info->name + 1);
		Things::HandleAttacks(info, mt_num);
	}

	if (Frames::act_flags & AF_EXPLODE)
		WAD::Printf("EXPLODE_DAMAGE.VAL = 128;\n");

	WAD::Printf("\n");
}

void Attacks::ConvertATK(void)
{
	got_one = false;

	for (int i = 0; i < NUMMOBJTYPES_BEX; i++)
	{
	    if (! all_mode && ! mobj_modified[i])
			continue;

		ConvertAttack(mobjinfo + i, i, false);

		if (i == MT_ROCKET)
			ConvertAttack(mobjinfo + i, i, true);
	}

	CheckPainElemental();

	if (got_one)
		FinishLump();
}

// NOTES
//
//   Handle PLAYER_MISSILE == CYBERDEMON_MISSILE
//   BRAIN_CUBE is a merger between MT_SPAWNSHOT and MT_SPAWNFIRE

}  // Deh_Edge
