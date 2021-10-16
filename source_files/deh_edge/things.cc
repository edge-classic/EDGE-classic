//------------------------------------------------------------------------
//  THING Conversion
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
#include "things.h"

#include "ammo.h"
#include "buffer.h"
#include "dh_plugin.h"
#include "info.h"
#include "frames.h"
#include "misc.h"
#include "mobj.h"
#include "patch.h"
#include "rscript.h"
#include "storage.h"
#include "sounds.h"
#include "system.h"
#include "text.h"
#include "util.h"
#include "wad.h"
#include "weapons.h"

#include <assert.h>
#include <ctype.h>
#include <string.h>

namespace Deh_Edge
{

#define DEBUG_MONST  0


#define EF_DISLOYAL    'D'
#define EF_TRIG_HAPPY  'H'
#define EF_BOSSMAN     'B'
#define EF_LOUD        'L'
#define EF_NO_RAISE    'R'
#define EF_NO_GRUDGE   'G'
#define EF_NO_ITEM_BK  'I'


// XXX needed to get original name (warning message)
extern spritename_t sprnames[NUMSPRITES_BEX];

#define CAST_MAX  20


void Things::Startup(void)
{
	memset(mobj_modified, 0, sizeof(mobj_modified));
}

namespace Things
{
	bool got_one;
	bool flags_got_one;

	int cast_mobjs[CAST_MAX];

	void BeginLump(void)
	{
		WAD::NewLump("DDFTHING");

		WAD::Printf(GEN_BY_COMMENT);

		WAD::Printf("<THINGS>\n\n");

		if (target_version >= 129)
			WAD::Printf("#VERSION 1.29\n");
	}

	void FinishLump(void)
	{
		WAD::Printf("\n");
		WAD::FinishLump();
	}

	typedef struct 
	{
		int flag;
		const char *name;  // for EDGE
		const char *bex;  // NULL if same as EDGE name
	}
	flagname_t;

	const flagname_t flagnamelist[] =
	{
		{ MF_NOBLOCKMAP,   "NOBLOCKMAP",     NULL },
		{ MF_NOBLOOD,      "DAMAGESMOKE",   "NOBLOOD" },
		{ MF_NOCLIP,       "NOCLIP",         NULL },
		{ MF_NOGRAVITY,    "NOGRAVITY",      NULL },
		{ MF_NOSECTOR,     "NOSECTOR",       NULL },
		{ MF_NOTDMATCH,    "NODEATHMATCH",  "NOTDMATCH" },

		{ MF_AMBUSH,       "AMBUSH",         NULL },
		{ MF_CORPSE,       "CORPSE",         NULL },
		{ MF_COUNTITEM,    "COUNT_AS_ITEM", "COUNTITEM" },
		{ MF_COUNTKILL,    "COUNT_AS_KILL", "COUNTKILL" },
		{ MF_DROPOFF,      "DROPOFF",        NULL },
		{ MF_DROPPED,      "DROPPED",        NULL },
		{ MF_FLOAT,        "FLOAT",          NULL },
		{ MF_MISSILE,      "MISSILE",        NULL },
		{ MF_PICKUP,       "PICKUP",         NULL },
		{ MF_SHADOW,       "FUZZY",         "SHADOW" },
		{ MF_SHOOTABLE,    "SHOOTABLE",      NULL },
		{ MF_SKULLFLY,     "SKULLFLY",       NULL },
		{ MF_SLIDE,        "SLIDER",        "SLIDE" },
		{ MF_SOLID,        "SOLID",          NULL },
		{ MF_SPAWNCEILING, "SPAWNCEILING",   NULL },
		{ MF_SPECIAL,      "SPECIAL",        NULL },
		{ MF_TELEPORT,     "TELEPORT",       NULL },

		// BOOM and MBF flags...
		{ MF_BOUNCES,      "BOUNCE",   "BOUNCES" },
		{ MF_STEALTH,      "STEALTH",   NULL },
		{ MF_TOUCHY,       "TOUCHY",    NULL },

		// Boom/MBF flags which don't produce an Edge SPECIAL
        { MF_TRANSLATION1, NULL, "TRANSLATION1" },
        { MF_TRANSLATION2, NULL, "TRANSLATION2" },
        { MF_TRANSLATION1, NULL, "TRANSLATION"  },  // bug compat
        { MF_TRANSLUCENT,  NULL, "TRANSLUCENT" },
        { MF_TRANSLUCENT,  NULL, "TRANSLUC50"  },
        { MF_TRANSLUCENT,  NULL, "TRANSLUC25"  },  // XXX: 25,75 unsupported
        { MF_TRANSLUCENT,  NULL, "TRANSLUC75"  },  //

		{ 0, NULL, NULL }  // End sentinel
	};

	const flagname_t extflaglist[] =
	{
		{ EF_DISLOYAL,    "DISLOYAL,ATTACK_HURTS", NULL },  // Must be first
		{ EF_TRIG_HAPPY,  "TRIGGER_HAPPY", NULL },
		{ EF_BOSSMAN,     "BOSSMAN", NULL },
		{ EF_LOUD,        "ALWAYS_LOUD", NULL },
		{ EF_NO_RAISE,    "NO_RESURRECT", NULL },
		{ EF_NO_GRUDGE,   "NO_GRUDGE,NEVERTARGETED", NULL },
		{ EF_NO_ITEM_BK,  "NO_RESPAWN", NULL },

		{ 0, NULL, NULL }  // End sentinel
	};

	const char *flag_bex_ignored[] =
	{
		"INFLOAT", "JUSTATTACKED", "JUSTHIT", 
		"UNUSED1", "UNUSED2", "UNUSED3", "UNUSED4",

		NULL
	};

	bool CheckIsMonster(const mobjinfo_t *info, int mt_num, int player,
		bool use_act_flags)
	{
		if (player > 0)
			return false;

		if (info->doomednum <= 0)
			return false;

		if (info->name[0] == '*')
			return false;

#if (! DEBUG_MONST)
		if (info->flags & MF_COUNTKILL)
			return true;
#endif

		if (info->flags & (MF_SPECIAL | MF_COUNTITEM))
			return false;

		int score = 0;

		// values determined by statistical analysis of major DEH patches
		// (Standard DOOM, Batman, Mordeth, Wheel-of-Time, Osiris). 

		if (info->flags & MF_SOLID) score += 25;
		if (info->flags & MF_SHOOTABLE) score += 72;

		if (info->painstate) score += 91;
		if (info->missilestate || info->meleestate) score += 91;
		if (info->deathstate) score += 72;
		if (info->raisestate) score += 31;

		if (use_act_flags)
		{
			if (Frames::act_flags & AF_CHASER) score += 78;
			if (Frames::act_flags & AF_FALLER) score += 61;
		}

		if (info->speed > 0) score += 87;

#if (DEBUG_MONST)
		Debug_PrintMsg("[%20.20s:%-4d] %c%c%c%c%c %c%c%c%c %c%c %d = %d\n",
			info->name, info->doomednum,
			(info->flags & MF_SOLID) ? 'S' : '-',
			(info->flags & MF_SHOOTABLE) ? 'H' : '-',
			(info->flags & MF_FLOAT) ? 'F' : '-',
			(info->flags & MF_MISSILE) ? 'M' : '-',
			(info->flags & MF_NOBLOOD) ? 'B' : '-',
			(info->painstate) ? 'p' : '-',
			(info->deathstate) ? 'd' : '-',
			(info->raisestate) ? 'r' : '-',
			(info->missilestate || info->meleestate) ? 'm' : '-',
			(Frames::act_flags & AF_CHASER) ? 'C' : '-',
			(Frames::act_flags & AF_FALLER) ? 'F' : '-',
			info->speed, score);
#endif

		return score >= (use_act_flags ? 370 : 300);
	}

	const char *GetExtFlags(int mt_num, int player)
	{
		if (player > 0)
			return "D";

		switch (mt_num)
		{
			case MT_INS:
			case MT_INV: return "I";

			case MT_POSSESSED:
			case MT_SHOTGUY:
			case MT_CHAINGUY: return "D";

			case MT_SKULL: return "DHM";
			case MT_UNDEAD: return "H";

			case MT_VILE: return "GR";
			case MT_CYBORG: return "BHR";
			case MT_SPIDER: return "BHR";

			case MT_BOSSSPIT: return "B";
			case MT_BOSSBRAIN: return (target_version >= 129) ? "L" : "B";

			default:
				break;
		}

		return "";
	}

	void AddOneFlag(const mobjinfo_t *info, const char *name)
	{
		if (! flags_got_one)
		{
			flags_got_one = true;

			if (info->name[0] == '*')
				WAD::Printf("PROJECTILE_SPECIAL = ");
			else
				WAD::Printf("SPECIAL = ");
		}
		else
			WAD::Printf(",");

		WAD::Printf("%s", name);
	}

	void HandleFlags(const mobjinfo_t *info, int mt_num, int player)
	{
		int i;
		int cur_f = info->flags;

		// strangely absent from MT_PLAYER
		if (player)
			cur_f |= MF_SLIDE;
		
		// this can cause EDGE 1.27 to crash
		if (! player)
			cur_f &= ~MF_PICKUP;

		// EDGE requires teleportman in sector. (DOOM uses thinker list)
		if (mt_num == MT_TELEPORTMAN)
			cur_f &= ~MF_NOSECTOR;

		// special workaround for negative MASS values
		if (info->mass < 0)
			cur_f |= MF_SPAWNCEILING | MF_NOGRAVITY;

		bool is_monster = CheckIsMonster(info, mt_num, player, true);
		bool force_disloyal = (is_monster && Misc::monster_infight == 221);

		flags_got_one = false;

		for (i = 0; flagnamelist[i].name != NULL; i++)
		{
			if (0 == (cur_f & flagnamelist[i].flag))
				continue;

			cur_f &= ~flagnamelist[i].flag;

			AddOneFlag(info, flagnamelist[i].name);
		}

		const char *eflags = GetExtFlags(mt_num, player);

		for (i = 0; extflaglist[i].name != NULL; i++)
		{
			char ch = (char) extflaglist[i].flag;

			if (! strchr(eflags, ch))
				continue;

			if (ch == EF_DISLOYAL)
			{
				force_disloyal = true;
				continue;
			}

			AddOneFlag(info, extflaglist[i].name);
		}

		if (force_disloyal)
			AddOneFlag(info, extflaglist[0].name);

		if (is_monster)
			AddOneFlag(info, "MONSTER");

		if (flags_got_one)
			WAD::Printf(";\n");

		if (cur_f & MF_TRANSLATION)
		{
			if ((cur_f & MF_TRANSLATION) == 0x4000000)
				WAD::Printf("PALETTE_REMAP = PLAYER_DK_GREY;\n");
			else if ((cur_f & MF_TRANSLATION) == 0x8000000)
				WAD::Printf("PALETTE_REMAP = PLAYER_BROWN;\n");
			else
				WAD::Printf("PALETTE_REMAP = PLAYER_DULL_RED;\n");

			cur_f &= ~MF_TRANSLATION;
		}

		if (cur_f & MF_TRANSLUCENT)
		{
			WAD::Printf("TRANSLUCENCY = 50%%;\n");
			cur_f &= ~MF_TRANSLUCENT;
		}

		if (cur_f & MF_FRIEND)
		{
			if (! player)
				WAD::Printf("SIDE = 16777215;\n");

			cur_f &= ~MF_FRIEND;
		}

		if (cur_f != 0)
			PrintWarn("Unconverted flags 0x%08x in entry [%s]\n",
				cur_f, info->name);
	}

	const int height_fixes[] =
	{
	   MT_MISC14, 60, MT_MISC29, 78, MT_MISC30, 58, MT_MISC31, 46,
	   MT_MISC33, 38, MT_MISC34, 50, MT_MISC38, 56, MT_MISC39, 48,
	   MT_MISC41, 96, MT_MISC42, 96, MT_MISC43, 96, MT_MISC44, 72,
	   MT_MISC45, 72, MT_MISC46, 72, MT_MISC70, 64, MT_MISC72, 52,
	   MT_MISC73, 40, MT_MISC74, 64, MT_MISC75, 64, MT_MISC76, 120,

	   MT_MISC36, 56, MT_MISC37, 56, MT_MISC47, 56, MT_MISC48, 128,
	   MT_MISC35, 56, MT_MISC40, 56, MT_MISC50, 56, MT_MISC77, 42,

     -1 /* the end */
	};

	void FixHeights(void)
	{
		for (int i = 0; height_fixes[i] >= 0; i += 2)
		{
			int mt_num = height_fixes[i];

			assert(mt_num < NUMMOBJTYPES_BEX);

			mobjinfo_t *mobj = mobjinfo + mt_num;

			/* Kludge for Aliens TC (and others) that put these thibgs on
			 * the ceiling -- they need the 16 height for correct display,
			 */
			if (mobj->flags & MF_SPAWNCEILING)
				continue;

			if (mobj->height != 16*FRACUNIT)
				continue;

			mobj->height = height_fixes[i+1] * FRACUNIT;

			// Note: Storage::ApplyAll() has already been called, hence
			//       we are being a bit sneaky here and using RememberMod
			//       just to restore the changed heights.
			Storage::RememberMod(&mobj->height, 16*FRACUNIT);
		}
	}

	void CollectTheCast(void)
	{
		for (int i = 0; i < CAST_MAX; i++)
			cast_mobjs[i] = -1;

		for (int mt_num = 0; mt_num < NUMMOBJTYPES_BEX; mt_num++)
		{
			int order = 0;

			// cast objects are required to have CHASE and DEATH states
			if (mobjinfo[mt_num].seestate == S_NULL ||
				mobjinfo[mt_num].deathstate == S_NULL)
				continue;

			switch (mt_num)
			{
				case MT_PLAYER:    order =  1; break;
				case MT_POSSESSED: order =  2; break;
				case MT_SHOTGUY:   order =  3; break;
				case MT_CHAINGUY:  order =  4; break;
				case MT_TROOP:     order =  5; break;
				case MT_SERGEANT:  order =  6; break;
				case MT_SKULL:     order =  7; break;
				case MT_HEAD:      order =  8; break;
				case MT_KNIGHT:    order =  9; break;
				case MT_BRUISER:   order = 10; break;
				case MT_BABY:      order = 11; break;
				case MT_PAIN:      order = 12; break;
				case MT_UNDEAD:    order = 13; break;
				case MT_FATSO:     order = 14; break;
				case MT_VILE:      order = 15; break;
				case MT_SPIDER:    order = 16; break;
				case MT_CYBORG:    order = 17; break;

				default:
					continue;
			}

			cast_mobjs[order] = mt_num;
		}
	}

	const char *GetSpeed(int speed)
	{
		// Interestingly, speed is fixed point for attacks, but
		// plain int for things.  Here we automatically handle both.

		static char num_buf[128];

		if (speed >= 1024)
			sprintf(num_buf, "%1.2f", F_FIXED(speed));
		else
			sprintf(num_buf, "%d", speed);

		return num_buf;
	}

	void HandleSounds(const mobjinfo_t *info, int mt_num)
	{
		if (info->activesound != sfx_None)
		{
			if (info->flags & MF_PICKUP)
				WAD::Printf("PICKUP_SOUND = %s;\n",
					Sounds::GetSound(info->activesound));
			else
				WAD::Printf("ACTIVE_SOUND = %s;\n",
					Sounds::GetSound(info->activesound));
		}
		else if (mt_num == MT_TELEPORTMAN)
			WAD::Printf("ACTIVE_SOUND = %s;\n", Sounds::GetSound(sfx_telept));

		if (info->seesound != sfx_None)
			WAD::Printf("SIGHTING_SOUND = %s;\n", Sounds::GetSound(info->seesound));
		else if (mt_num == MT_BOSSSPIT)
			WAD::Printf("SIGHTING_SOUND = %s;\n", Sounds::GetSound(sfx_bossit));

		if (info->attacksound != sfx_None && info->meleestate != S_NULL)
		{
			WAD::Printf("STARTCOMBAT_SOUND = %s;\n",
				Sounds::GetSound(info->attacksound));
		}

		if (info->painsound != sfx_None)
			WAD::Printf("PAIN_SOUND = %s;\n", Sounds::GetSound(info->painsound));

		if (info->deathsound != sfx_None)
			WAD::Printf("DEATH_SOUND = %s;\n", Sounds::GetSound(info->deathsound));
	}

	void HandleFrames(const mobjinfo_t *info, int mt_num)
	{
		Frames::ResetAll();

		// special cases...

		if (mt_num == MT_TELEPORTMAN)
		{
			WAD::Printf("TRANSLUCENCY = 50%%;\n");
			WAD::Printf("\n");
			WAD::Printf("STATES(IDLE) = %s:A:-1:NORMAL:TRANS_SET(0%%);\n",
				TextStr::GetSprite(SPR_TFOG));

			// EDGE doesn't use the TELEPORT_FOG object, instead it uses
			// the CHASE states of the TELEPORT_FLASH object (i.e. the one
			// used to find the destination in the target sector).

			if (0 == Frames::BeginGroup(mobjinfo[MT_TFOG].spawnstate, 'E'))
			{
				PrintWarn("Teleport fog has no spawn states.\n");
				return;
			}

			Frames::SpreadGroups();
			Frames::OutputGroup(mobjinfo[MT_TFOG].spawnstate, 'E');

			return;
		}

		// --- collect states into groups ---

		int count = 0;

		count += Frames::BeginGroup(info->raisestate,   'R');
		count += Frames::BeginGroup(info->xdeathstate,  'X');
		count += Frames::BeginGroup(info->deathstate,   'D');
		count += Frames::BeginGroup(info->painstate,    'P');
		count += Frames::BeginGroup(info->missilestate, 'M');
		count += Frames::BeginGroup(info->meleestate,   'L');
		count += Frames::BeginGroup(info->seestate,     'E');
		count += Frames::BeginGroup(info->spawnstate,   'S');

		if (count == 0)
		{
			// only occurs with special/invisible objects, currently only
			// with teleport target (handled above) and brain spit targets.

			if (mt_num != MT_BOSSTARGET)
				PrintWarn("Mobj [%s:%d] has no states.\n", info->name, info->doomednum);

			WAD::Printf("TRANSLUCENCY = 0%%;\n");

			WAD::Printf("\n");
			WAD::Printf("STATES(IDLE) = %s:A:-1:NORMAL:NOTHING;\n",
				TextStr::GetSprite(SPR_CAND));

			return;
		}

		Frames::SpreadGroups();

		Frames::OutputGroup(info->spawnstate,   'S');
		Frames::OutputGroup(info->seestate,     'E');
		Frames::OutputGroup(info->meleestate,   'L');
		Frames::OutputGroup(info->missilestate, 'M');
		Frames::OutputGroup(info->painstate,    'P');
		Frames::OutputGroup(info->deathstate,   'D');
		Frames::OutputGroup(info->xdeathstate,  'X');
		Frames::OutputGroup(info->raisestate,   'R');

		// the A_VileChase action is another special case
		if (Frames::act_flags & AF_RAISER)
		{
			if (Frames::BeginGroup(S_VILE_HEAL1, 'H') > 0)
			{
				Frames::SpreadGroups();
				Frames::OutputGroup(S_VILE_HEAL1, 'H');
			}
		}

		// Workaround for EDGE 1.27, which does not Remove a thing when
		// it has no see-states but does a successful A_Look().

		if (target_version <= 127 && (Frames::act_flags & AF_LOOK) &&
			info->seestate == S_NULL && info->spawnstate != S_NULL)
		{
			state_t *st = states + info->spawnstate;

			WAD::Printf("\n");
			WAD::Printf("// A_Look with missing seestate workaround\n");
			WAD::Printf("STATES(CHASE) = %s:%c:1:NORMAL:NOTHING,#REMOVE;\n",
				TextStr::GetSprite(st->sprite),
				'A' + ((int) st->frame & 31));
		}
	}

	const int NUMPLAYERS = 8;

	typedef struct
	{
		const char *name;
		int num;
		const char *remap;
	}
	playerinfo_t;

	const playerinfo_t player_info[NUMPLAYERS] =
	{
		{ "OUR_HERO",    1, "PLAYER_GREEN" },
		{ "PLAYER2",     2, "PLAYER_DK_GREY" },
		{ "PLAYER3",     3, "PLAYER_BROWN" },
		{ "PLAYER4",     4, "PLAYER_DULL_RED" },
		{ "PLAYER5",  4001, "PLAYER_ORANGE" },
		{ "PLAYER6",  4002, "PLAYER_LT_GREY" },
		{ "PLAYER7",  4003, "PLAYER_LT_RED" },
		{ "PLAYER8",  4004, "PLAYER_PINK"  }
	};

	void HandlePlayer(const mobjinfo_t *info, int player)
	{
		if (player <= 0)
			return;

		assert(player <= NUMPLAYERS);

		const playerinfo_t *pi = player_info + (player - 1);

		WAD::Printf("PLAYER = %d;\n", player);
		WAD::Printf("SIDE = %d;\n", 1 << (player - 1));
		WAD::Printf("PALETTE_REMAP = %s;\n", pi->remap);

		WAD::Printf("INITIAL_BENEFIT = \n");
		WAD::Printf("    BULLETS.LIMIT(%d), ", Ammo::plr_max[am_bullet]);
		WAD::Printf(    "SHELLS.LIMIT(%d), ",  Ammo::plr_max[am_shell]);
		WAD::Printf(    "ROCKETS.LIMIT(%d), ", Ammo::plr_max[am_rocket]);
		WAD::Printf(    "CELLS.LIMIT(%d),\n",  Ammo::plr_max[am_cell]);
		WAD::Printf("    PELLETS.LIMIT(%d), ", 200);
		WAD::Printf(    "NAILS.LIMIT(%d), ",   100);
		WAD::Printf(    "GRENADES.LIMIT(%d), ", 50);
		WAD::Printf(    "GAS.LIMIT(%d),\n",    300);
		
		if (target_version >= 129)
		{
			WAD::Printf("    AMMO9.LIMIT(%d), ",   100);
			WAD::Printf(    "AMMO10.LIMIT(%d), ",  200);
			WAD::Printf(    "AMMO11.LIMIT(%d), ",  50);
			WAD::Printf(    "AMMO12.LIMIT(%d),\n", 300);
			WAD::Printf("    AMMO13.LIMIT(%d), ",  100);
			WAD::Printf(    "AMMO14.LIMIT(%d), ",  200);
			WAD::Printf(    "AMMO15.LIMIT(%d), ",  50);
			WAD::Printf(    "AMMO16.LIMIT(%d),\n", 300);
		}

		WAD::Printf("    BULLETS(%d);\n",      Misc::init_ammo);
	}

	typedef struct
	{
		int spr_num;
		const char *benefit;
		int par_num;
		int amount, limit;
		const char *ldf;
		int sound;
	}
	pickupitem_t;

	const pickupitem_t pickup_item[] =
	{
		// Health & Armor....
		{ SPR_BON1, "HEALTH", 2, 1,200, "GotHealthPotion", sfx_itemup },  
		{ SPR_STIM, "HEALTH", 2, 10,100, "GotStim", sfx_itemup },  
		{ SPR_MEDI, "HEALTH", 2, 25,100, "GotMedi", sfx_itemup },  
		{ SPR_BON2, "GREEN_ARMOUR", 2, 1,200, "GotArmourHelmet", sfx_itemup },  
		{ SPR_ARM1, "GREEN_ARMOUR", 2, 100,100, "GotArmour", sfx_itemup },  
		{ SPR_ARM2, "BLUE_ARMOUR", 2, 200,200, "GotMegaArmour", sfx_itemup },  

		// Keys....
		{ SPR_BKEY, "KEY_BLUECARD",   0, 0,0, "GotBlueCard", sfx_itemup },
		{ SPR_YKEY, "KEY_YELLOWCARD", 0, 0,0, "GotYellowCard", sfx_itemup },
		{ SPR_RKEY, "KEY_REDCARD",    0, 0,0, "GotRedCard", sfx_itemup },
		{ SPR_BSKU, "KEY_BLUESKULL",  0, 0,0, "GotBlueSkull", sfx_itemup },
		{ SPR_YSKU, "KEY_YELLOWSKULL",0, 0,0, "GotYellowSkull", sfx_itemup },
		{ SPR_RSKU, "KEY_REDSKULL",   0, 0,0, "GotRedSkull", sfx_itemup },

		// Ammo....
		{ SPR_CLIP, "BULLETS", 1, 10,0, "GotClip", sfx_itemup },
		{ SPR_AMMO, "BULLETS", 1, 50,0, "GotClipBox", sfx_itemup },
		{ SPR_SHEL, "SHELLS",  1, 4,0,  "GotShells", sfx_itemup },
		{ SPR_SBOX, "SHELLS",  1, 20,0, "GotShellBox", sfx_itemup },
		{ SPR_ROCK, "ROCKETS", 1, 1,0,  "GotRocket", sfx_itemup },
		{ SPR_BROK, "ROCKETS", 1, 5,0,  "GotRocketBox", sfx_itemup },
		{ SPR_CELL, "CELLS",   1, 20,0, "GotCell", sfx_itemup },
		{ SPR_CELP, "CELLS",   1, 100,0, "GotCellPack", sfx_itemup },

		// Powerups....
		{ SPR_SOUL, "HEALTH", 2, 100,200, "GotSoul", sfx_getpow },  
		{ SPR_PMAP, "POWERUP_AUTOMAP", 0, 0,0, "GotMap", sfx_getpow },
		{ SPR_PINS, "POWERUP_PARTINVIS", 2, 100,100, "GotInvis", sfx_getpow },  
		{ SPR_PINV, "POWERUP_INVULNERABLE", 2, 30,30, "GotInvulner", sfx_getpow },  
		{ SPR_PVIS, "POWERUP_LIGHTGOGGLES", 2, 120,120, "GotVisor", sfx_getpow },  
		{ SPR_SUIT, "POWERUP_ACIDSUIT", 2, 60,60, "GotSuit", sfx_getpow },  

		// Weapons....
		{ SPR_CSAW, "CHAINSAW", 0, 0,0, "GotChainSaw", sfx_wpnup },
		{ SPR_SHOT, "SHOTGUN,SHELLS", 1, 8,0, "GotShotGun", sfx_wpnup },
		{ SPR_SGN2, "SUPERSHOTGUN,SHELLS", 1, 8,0, "GotDoubleBarrel", sfx_wpnup },
		{ SPR_MGUN, "CHAINGUN,BULLETS", 1, 20,0, "GotChainGun", sfx_wpnup },
		{ SPR_LAUN, "ROCKET_LAUNCHER,ROCKETS", 1, 2,0, "GotRocketLauncher", sfx_wpnup },
		{ SPR_PLAS, "PLASMA_RIFLE,CELLS", 1, 40,0, "GotPlasmaGun", sfx_wpnup },
		{ SPR_BFUG, "BFG9000,CELLS", 1, 40,0, "GotBFG", sfx_wpnup },
		
		{ -1, NULL, 0,0,0, NULL }
	};

	void HandleItem(const mobjinfo_t *info, int mt_num)
	{
		if (! (info->flags & MF_SPECIAL))
			return;

		if (info->spawnstate == S_NULL)
			return;

		int spr_num = states[info->spawnstate].sprite;

		// special cases:

		if (spr_num == SPR_PSTR) // Berserk
		{
			WAD::Printf("PICKUP_BENEFIT = POWERUP_BERSERK(60:60),HEALTH(100:100);\n");
			WAD::Printf("PICKUP_MESSAGE = GotBerserk;\n");
			WAD::Printf("PICKUP_SOUND = %s;\n", Sounds::GetSound(sfx_getpow));
			if (target_version >= 129)
				WAD::Printf("PICKUP_EFFECT = SWITCH_WEAPON(FIST);\n");
			return;
		}
		else if (spr_num == SPR_MEGA)  // Megasphere
		{
			WAD::Printf("PICKUP_BENEFIT = ");
			WAD::Printf("HEALTH(%d:%d),", Misc::mega_health, Misc::mega_health);
			WAD::Printf("BLUE_ARMOUR(%d:%d);\n", Misc::max_armour, Misc::max_armour);
			WAD::Printf("PICKUP_MESSAGE = GotMega;\n");
			WAD::Printf("PICKUP_SOUND = %s;\n", Sounds::GetSound(sfx_getpow));
			return;
		}
		else if (spr_num == SPR_BPAK)  // Backpack full of AMMO
		{
			WAD::Printf("PICKUP_BENEFIT = \n");
			WAD::Printf("    BULLETS.LIMIT(%d), ", 2 * Ammo::plr_max[am_bullet]);
			WAD::Printf("    SHELLS.LIMIT(%d),\n", 2 * Ammo::plr_max[am_shell]);
			WAD::Printf("    ROCKETS.LIMIT(%d), ", 2 * Ammo::plr_max[am_rocket]);
			WAD::Printf("    CELLS.LIMIT(%d),\n",  2 * Ammo::plr_max[am_cell]);
			WAD::Printf("    BULLETS(10), SHELLS(4), ROCKETS(1), CELLS(20);\n");
			WAD::Printf("PICKUP_MESSAGE = GotBackpack;\n");
			WAD::Printf("PICKUP_SOUND = %s;\n", Sounds::GetSound(sfx_itemup));
			return;
		}

		int i;

		for (i = 0; pickup_item[i].benefit != NULL; i++)
		{
			if (spr_num == pickup_item[i].spr_num)
				break;
		}

		const pickupitem_t *pu = pickup_item + i;

		if (pu->benefit == NULL)  // not found
		{
			PrintWarn("Unknown pickup sprite \"%s\" for item [%s]\n",
				sprnames[spr_num].orig_name, info->name);
			return;
		}

		int amount = pu->amount;
		int limit  = pu->limit;

		// handle patchable amounts

		switch (spr_num)
		{
			// Armor & health...
			case SPR_BON2:   // "ARMOUR_HELMET"  
				limit  = Misc::max_armour;
				break;

			case SPR_ARM1:   // "GREEN_ARMOUR"
				amount = Misc::green_armour_class * 100;
				limit  = Misc::max_armour;
				break;

			case SPR_ARM2:   // "BLUE_ARMOUR"  
				amount = Misc::blue_armour_class * 100;
				limit  = Misc::max_armour;
				break;

			case SPR_BON1:    // "HEALTH_POTION"
				limit  = Misc::max_health;  // Note: *not* MEDIKIT
				break;

			case SPR_SOUL:   // "SOULSPHERE"  
				amount = Misc::soul_health;
				limit  = Misc::soul_limit;
				break;

			// Ammo...
			case SPR_CLIP:   // "CLIP"
			case SPR_AMMO:   // "BOX_OF_BULLETS"  
				amount = Ammo::pickups[am_bullet];
				break;

			case SPR_SHEL:   // "SHELLS"  
			case SPR_SBOX:   // "BOX_OF_SHELLS"  
				amount = Ammo::pickups[am_shell];
				break;

			case SPR_ROCK:   // "ROCKET"  
			case SPR_BROK:   // "BOX_OF_ROCKETS"  
				amount = Ammo::pickups[am_rocket];
				break;

			case SPR_CELL:   // "CELLS"  
			case SPR_CELP:   // "CELL_PACK"  
				amount = Ammo::pickups[am_cell];
				break;

			default:
				break;
		}

		// big boxes of ammo
		if (spr_num == SPR_AMMO || spr_num == SPR_BROK ||
			spr_num == SPR_CELP || spr_num == SPR_SBOX)
		{
			amount *= 5;
		}

		if (pu->par_num == 2 && amount > limit)
			amount = limit;

		WAD::Printf("PICKUP_BENEFIT = %s", pu->benefit);

		if (pu->par_num == 1)
			WAD::Printf("(%d)", amount);
		else if (pu->par_num == 2)
			WAD::Printf("(%d:%d)", amount, limit);

		WAD::Printf(";\n");
		WAD::Printf("PICKUP_MESSAGE = %s;\n", pu->ldf);

		if (info->activesound == sfx_None)
			WAD::Printf("PICKUP_SOUND = %s;\n", Sounds::GetSound(pu->sound));
	}

	const char *cast_titles[17] =
	{
	    "OurHeroName",     "ZombiemanName",
        "ShotgunGuyName",  "HeavyWeaponDudeName",
        "ImpName",         "DemonName",
	    "LostSoulName",    "CacodemonName",
        "HellKnightName",  "BaronOfHellName",
        "ArachnotronName", "PainElementalName",
        "RevenantName",    "MancubusName",
        "ArchVileName",    "SpiderMastermindName",
        "CyberdemonName"
	};

	void HandleCastOrder(const mobjinfo_t *info, int mt_num, int player)
	{
		if (player >= 2)
			return;

		int pos   = 1;
		int order = 1;

		for (; pos < CAST_MAX; pos++)
		{
			// ignore missing members (ensure real order is contiguous)
			if (cast_mobjs[pos] < 0)
				continue;

			if (cast_mobjs[pos] == mt_num)
				break;

			order++;
		}

		if (pos >= CAST_MAX)  // not found
			return;

		WAD::Printf("CASTORDER = %d;\n", order);

		if (target_version >= 128)
			WAD::Printf("CAST_TITLE = %s;\n", cast_titles[pos - 1]);
	}

	void HandleDropItem(const mobjinfo_t *info, int mt_num)
	{
		const char *item = NULL;

		switch (mt_num)
		{
			case MT_WOLFSS:
			case MT_POSSESSED: item = "CLIP"; break;

			case MT_SHOTGUY:   item = "SHOTGUN"; break;
			case MT_CHAINGUY:  item = "CHAINGUN"; break;

			default:
				return;
		}

		assert(item);

		WAD::Printf("DROPITEM = \"%s\";\n", item);
	}

	void HandleAttacks(const mobjinfo_t *info, int mt_num)
	{
		if (Frames::attack_slot[Frames::RANGE])
		{
			WAD::Printf("RANGE_ATTACK = %s;\n",
				Frames::attack_slot[Frames::RANGE]);
			WAD::Printf("MINATTACK_CHANCE = 25%%;\n");
		}

		if (Frames::attack_slot[Frames::COMBAT])
		{
			WAD::Printf("CLOSE_ATTACK = %s;\n",
				Frames::attack_slot[Frames::COMBAT]);
		}
		else if (info->meleestate && info->name[0] != '*' &&
			     target_version < 129)
		{
			PrintWarn("No close attack in melee states of [%s].\n",
				info->name);
			WAD::Printf("CLOSE_ATTACK = DEMON_CLOSECOMBAT; // dummy attack\n");
		}

		if (Frames::attack_slot[Frames::SPARE])
			WAD::Printf("SPARE_ATTACK = %s;\n",
				Frames::attack_slot[Frames::SPARE]);
	}

	void ConvertMobj(const mobjinfo_t *info, int mt_num, int player);
}

void Things::ConvertMobj(const mobjinfo_t *info, int mt_num, int player)
{
	if (! got_one)
	{
		got_one = true;
		BeginLump();
	}
	
	if (info->name[0] == '*')  // attack
		return;

	if (player > 0)
		WAD::Printf("[%s:%d]\n", player_info[player-1].name, player_info[player-1].num);
	else if (info->doomednum < 0)
		WAD::Printf("[%s]\n", info->name);
	else
		WAD::Printf("[%s:%d]\n", info->name, info->doomednum);

	WAD::Printf("RADIUS = %1.1f;\n", F_FIXED(info->radius));
	WAD::Printf("HEIGHT = %1.1f;\n", F_FIXED(info->height));

	if (info->spawnhealth != 1000)
		WAD::Printf("SPAWNHEALTH = %d;\n", info->spawnhealth);

	if (player > 0)
		WAD::Printf("SPEED = 1;\n");
	else if (info->speed != 0)
		WAD::Printf("SPEED = %s;\n", GetSpeed(info->speed));

	if (info->mass != 100 && info->mass > 0)
		WAD::Printf("MASS = %d;\n", info->mass);

	if (info->reactiontime != 0)
		WAD::Printf("REACTION_TIME = %dT;\n", info->reactiontime);

	if (info->painchance >= 256)
		WAD::Printf("PAINCHANCE = 100%%;\n");
	else if (info->painchance > 0)
		WAD::Printf("PAINCHANCE = %1.1f%%;\n",
			(float)info->painchance * 100.0 / 256.0);

	if (mt_num == MT_BOSSSPIT)
		WAD::Printf("SPIT_SPOT = BRAIN_SPAWNSPOT;\n");

	HandleCastOrder(info, mt_num, player);
	HandleDropItem(info, mt_num);
	HandlePlayer(info, player);
	HandleItem(info, mt_num);
	HandleSounds(info, mt_num);
	HandleFrames(info, mt_num);

	WAD::Printf("\n");

	HandleFlags(info, mt_num, player);
	HandleAttacks(info, mt_num);

	if (Frames::act_flags & AF_EXPLODE)
		WAD::Printf("EXPLODE_DAMAGE.VAL = 128;\n");
	else if (Frames::act_flags & AF_DETONATE)
		WAD::Printf("EXPLODE_DAMAGE.VAL = %d;\n", info->damage);

	if ((Frames::act_flags & AF_KEENDIE) && target_version < 129)
		Rscript::MarkKeenDie(mt_num);

	WAD::Printf("\n");
}

void Things::ConvertTHING(void)
{
	CollectTheCast();

	got_one = false;

	for (int i = 0; i < NUMMOBJTYPES_BEX; i++)
	{
	    if (! all_mode && ! mobj_modified[i])
			continue;

		if (i == MT_PLAYER)
		{
			for (int p = 1; p <= NUMPLAYERS; p++)
				ConvertMobj(mobjinfo + i, i, p);

			continue;
		}

		ConvertMobj(mobjinfo + i, i, 0);
	}

	if (true)  // XXX Modified
		ConvertMobj(&brain_explode_mobj, MT_ROCKET /* dummy */, 0);

	if (got_one)
		FinishLump();
}

void Things::MarkThing(int mt_num)
{
	assert(0 <= mt_num && mt_num < NUMMOBJTYPES_BEX);

	mobj_modified[mt_num] = true;

	// handle merged things/attacks

	if (mt_num == MT_TFOG)
		mobj_modified[MT_TELEPORTMAN] = true;
	
	if (mt_num == MT_SPAWNFIRE)
		mobj_modified[MT_SPAWNSHOT] = true;
}


//------------------------------------------------------------------------

namespace Things
{
	const fieldreference_t mobj_field[] =
	{
		{ "ID #",             &mobjinfo[0].doomednum, FT_ANY },
		{ "Initial frame",    &mobjinfo[0].spawnstate, FT_FRAME },
		{ "Hit points",       &mobjinfo[0].spawnhealth, FT_GTEQ1 },
		{ "First moving frame", &mobjinfo[0].seestate, FT_FRAME },
		{ "Alert sound",      &mobjinfo[0].seesound, FT_SOUND },
		{ "Reaction time",    &mobjinfo[0].reactiontime, FT_NONEG },
		{ "Attack sound",     &mobjinfo[0].attacksound, FT_SOUND },
		{ "Injury frame",     &mobjinfo[0].painstate, FT_FRAME },
		{ "Pain chance",      &mobjinfo[0].painchance, FT_NONEG },
		{ "Pain sound",       &mobjinfo[0].painsound, FT_SOUND },
		{ "Close attack frame", &mobjinfo[0].meleestate, FT_FRAME },
		{ "Far attack frame", &mobjinfo[0].missilestate, FT_FRAME },
		{ "Death frame",      &mobjinfo[0].deathstate, FT_FRAME },
		{ "Exploding frame",  &mobjinfo[0].xdeathstate, FT_FRAME },
		{ "Death sound",      &mobjinfo[0].deathsound, FT_SOUND },
		{ "Speed",            &mobjinfo[0].speed, FT_NONEG },
		{ "Width",            &mobjinfo[0].radius, FT_NONEG },
		{ "Height",           &mobjinfo[0].height, FT_NONEG },
		{ "Mass",             &mobjinfo[0].mass, FT_NONEG },
		{ "Missile damage",   &mobjinfo[0].damage, FT_NONEG },
		{ "Action sound",     &mobjinfo[0].activesound, FT_SOUND },
		{ "Bits",             &mobjinfo[0].flags, FT_BITS },
		{ "Respawn frame",    &mobjinfo[0].raisestate, FT_FRAME },

		{ NULL, NULL, 0 }   // End sentinel
	};
}

void Things::AlterThing(int new_val)
{
	int mt_num = Patch::active_obj - 1;  // NOTE WELL

	assert(0 <= mt_num && mt_num < NUMMOBJTYPES_BEX);

	const char *field_name = Patch::line_buf;

	int stride = ((char*) (mobjinfo+1)) - ((char*) mobjinfo);

	if (! Things::AlterOneField(mobj_field, field_name, mt_num * stride, new_val))
	{
		PrintWarn("UNKNOWN THING FIELD: %s\n", field_name);
		return;
	}

	MarkThing(mt_num);
}

void Things::AlterBexBits(char *bit_str)
{
	int mt_num = Patch::active_obj - 1;  // NOTE WELL

	assert(0 <= mt_num && mt_num < NUMMOBJTYPES_BEX);

	static const char *delims = "+|, \t\f\r";

	int new_flags = 0;
	int i;

	for (char *token = strtok(bit_str, delims);
	     token != NULL;
		 token = strtok(NULL, delims))
	{
		assert(token[0] != 0);  // tokens should be non-empty

		if (isdigit(token[0]))
		{
			int flags;

			if (sscanf(token, " %i ", &flags) == 1)
				new_flags |= flags;
			else
                PrintWarn("Line %d: unreadable BITS value: %s\n",
					Patch::line_num, token);

			continue;
		}

		// see whether name is in known list
		for (i = 0; flagnamelist[i].flag != 0; i++)
		{
			const char *name = flagnamelist[i].bex ?
				flagnamelist[i].bex : flagnamelist[i].name;

			assert(name);

			if (StrCaseCmp(token, name) == 0)
				break;
		}

		if (flagnamelist[i].flag != 0)
		{
			new_flags |= flagnamelist[i].flag;
			continue;
		}

		// see whether we should ignore this flag
		for (i = 0; flag_bex_ignored[i]; i++)
			if (StrCaseCmp(token, flag_bex_ignored[i]) == 0)
				break;

		if (flag_bex_ignored[i])
			continue;

		PrintWarn("Line %d: unknown BITS mnemonic: %s\n",
			Patch::line_num, token);
	}

	Storage::RememberMod(&mobjinfo[mt_num].flags, new_flags);

	MarkThing(mt_num);
}

namespace Things
{
	bool ValidateValue(const fieldreference_t *ref, int new_val)
	{
		if (ref->field_type == FT_ANY || ref->field_type == FT_BITS)
			return true;

		if (new_val < 0 || (new_val == 0 && ref->field_type == FT_GTEQ1))
		{
			PrintWarn("Line %d: bad value '%d' for %s\n",
				Patch::line_num, new_val, ref->deh_name);
			return false;
		}

		if (ref->field_type == FT_NONEG || ref->field_type == FT_GTEQ1)
			return true;

		if (ref->field_type == FT_SUBSPR)  // ignore the bright bit
			new_val &= ~32768;

		int min_obj = 0;
		int max_obj = 0;

		if (Patch::patch_fmt <= 5)
		{
			switch (ref->field_type)
			{
				case FT_AMMO:   max_obj = NUMAMMO - 1; break;
				case FT_FRAME:  max_obj = NUMSTATES - 1; break;
				case FT_SOUND:  max_obj = NUMSFX - 1; break;
				case FT_SPRITE: max_obj = NUMSPRITES - 1; break;
				case FT_SUBSPR: max_obj = 31; break;

				default:
					InternalError("Bad field type %d\n", ref->field_type);
			}
		}
		else  /* patch_fmt == 6, allow BOOM/MBF stuff */
		{
			switch (ref->field_type)
			{
				case FT_AMMO:   max_obj = NUMAMMO - 1; break;
				case FT_FRAME:  max_obj = NUMSTATES_BEX - 1; break;
				case FT_SOUND:  max_obj = NUMSFX_BEX - 1; break;
				case FT_SPRITE: max_obj = NUMSPRITES_BEX - 1; break;
				case FT_SUBSPR: max_obj = 31; break;

				default:
					InternalError("Bad field type %d\n", ref->field_type);
			}
		}

		if (new_val < min_obj || new_val > max_obj)
		{
			PrintWarn("Line %d: bad value '%d' for %s\n",
				Patch::line_num, new_val, ref->deh_name);

			return false;
		}

		return true;
	}
}

bool Things::AlterOneField(const fieldreference_t *refs, const char *deh_field,
		int entry_offset, int new_val)
{
	assert(entry_offset >= 0);

	for (; refs->deh_name; refs++)
	{
		if (StrCaseCmp(refs->deh_name, deh_field) != 0)
			continue;

		// found it...

		if (ValidateValue(refs, new_val))
		{
			// prevent BOOM/MBF specific flags from being set using
			// numeric notation.  Only settable via AA+BB+CC notation.
			if (refs->field_type == FT_BITS)
				new_val &= ~ ALL_BEX_FLAGS;

			// Yup, we play a bit dirty here
			char *dest = ((char *) (refs->var)) + entry_offset;

			Storage::RememberMod((int *)dest, new_val);
		}

		return true;
	}

	return false;
}

}  // Deh_Edge
