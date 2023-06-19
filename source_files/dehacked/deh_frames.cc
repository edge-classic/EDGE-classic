//------------------------------------------------------------------------
//  FRAME Handling
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
#include <ctype.h>
#include <string.h>
#include <unordered_map>

#include "deh_i_defs.h"
#include "deh_edge.h"

#include "deh_buffer.h"
#include "deh_convert.h"
#include "deh_field.h"
#include "deh_frames.h"
#include "deh_info.h"
#include "deh_patch.h"
#include "deh_sounds.h"
#include "deh_sprites.h"
#include "deh_system.h"
#include "deh_text.h"
#include "deh_things.h"
#include "deh_util.h"
#include "deh_wad.h"
#include "deh_weapons.h"

namespace Deh_Edge
{

#define MAX_ACT_NAME  1024


extern state_t states_orig[NUMSTATES_MBF];

std::vector<state_t *> new_states;

// memory for states using misc1/misc2 or Args1..Args8
std::vector<int> argument_mem;


struct group_info_t
{
	char group;
	std::vector<int> states;
};


namespace Frames
{
	// stuff for determining and outputting groups of states:
	std::unordered_map<char, group_info_t> groups;
	std::unordered_map<int, char> group_for_state;
	std::unordered_map<int, int> offset_for_state;

	const char * attack_slot[3];
	int act_flags;
	bool force_fullbright = false;

	// forward decls
	const state_t * NewStateElseOld(int st_num);
	int  ReadArg(const state_t *st, int i);
	void WriteArg(state_t *st, int i, int value);
	const char *GroupToName(char group);
	const char *RedirectorName(int next_st);

	void SpecialAction(char *buf, const state_t *st);
	void OutputState(char group, int cur, bool do_action);
	bool OutputSpawnState(int first);
	bool SpreadGroupPass(bool alt_jumps);
	void UpdateAttacks(char group, char *act_name, int action);
	bool DependRangeWasModified(int low, int high);

	inline bool IS_WEAPON(char group)
	{
		return islower(group);
	}

	inline int MISC_TO_ANGLE(int m)
	{
		return m / 11930465;
	}
}


struct actioninfo_t
{
	const char *bex_name;

	int act_flags;

	// this is not used when AF_SPECIAL is set
	const char *ddf_name;

	// attacks implied by the action, often NULL.  The format is
	// "X:ATTACK_NAME" where X is 'R' for range attacks, 'C' for
	// close-combat attacks, and 'S' for spare attacks.
	const char *atk_1;
	const char *atk_2;
};


const actioninfo_t action_info[NUMACTIONS_MBF] =
{
	{ "A_NULL",         0,          "NOTHING", NULL, NULL },

	// weapon actions...
	{ "A_Light0",       0,          "W:LIGHT0", NULL, NULL },
	{ "A_WeaponReady",  0,          "W:READY", NULL, NULL },
	{ "A_Lower",        0,          "W:LOWER", NULL, NULL },
	{ "A_Raise",        0,          "W:RAISE", NULL, NULL },
	{ "A_Punch",        0,          "W:SHOOT", "C:PLAYER_PUNCH", NULL },
	{ "A_ReFire",       0,          "W:REFIRE", NULL, NULL },
	{ "A_FirePistol",   AF_FLASH,   "W:SHOOT", "R:PLAYER_PISTOL", NULL },
	{ "A_Light1",       0,          "W:LIGHT1", NULL, NULL },
	{ "A_FireShotgun",  AF_FLASH,   "W:SHOOT", "R:PLAYER_SHOTGUN", NULL },
	{ "A_Light2",       0,          "W:LIGHT2", NULL, NULL },
	{ "A_FireShotgun2", AF_FLASH,   "W:SHOOT", "R:PLAYER_SHOTGUN2", NULL },
	{ "A_CheckReload",  0,          "W:CHECKRELOAD", NULL, NULL },
	{ "A_OpenShotgun2", 0,          "W:PLAYSOUND(DBOPN)", NULL, NULL },
	{ "A_LoadShotgun2", 0,          "W:PLAYSOUND(DBLOAD)", NULL, NULL },
	{ "A_CloseShotgun2",0,          "W:PLAYSOUND(DBCLS)", NULL, NULL },
	{ "A_FireCGun",     AF_FLASH,   "W:SHOOT", "R:PLAYER_CHAINGUN", NULL },
	{ "A_GunFlash",     AF_FLASH,   "W:FLASH", NULL, NULL },
	{ "A_FireMissile",  0,          "W:SHOOT", "R:PLAYER_MISSILE", NULL },
	{ "A_Saw",          0,          "W:SHOOT", "C:PLAYER_SAW", NULL },
	{ "A_FirePlasma",   AF_FLASH,   "W:SHOOT", "R:PLAYER_PLASMA", NULL },
	{ "A_BFGsound",     0,          "W:PLAYSOUND(BFG)", NULL, NULL },
	{ "A_FireBFG",      0,          "W:SHOOT", "R:PLAYER_BFG9000", NULL },

	// thing actions...
	{ "A_BFGSpray",     0,          "SPARE_ATTACK", NULL, NULL },
	{ "A_Explode",      AF_EXPLODE, "EXPLOSIONDAMAGE", NULL, NULL },
	{ "A_Pain",         0,          "MAKEPAINSOUND", NULL, NULL },
	{ "A_PlayerScream", 0,          "PLAYER_SCREAM", NULL, NULL },
	{ "A_Fall",         AF_FALLER,  "MAKEDEAD", NULL, NULL },
	{ "A_XScream",      0,          "MAKEOVERKILLSOUND", NULL, NULL },
	{ "A_Look",         AF_LOOK,    "LOOKOUT", NULL, NULL },
	{ "A_Chase",        AF_CHASER,  "CHASE", NULL, NULL },
	{ "A_FaceTarget",   0,          "FACETARGET", NULL, NULL },
	{ "A_PosAttack",    0,          "RANGE_ATTACK", "R:FORMER_HUMAN_PISTOL", NULL },
	{ "A_Scream",       0,          "MAKEDEATHSOUND", NULL, NULL },
	{ "A_SPosAttack",   0,          "RANGE_ATTACK", "R:FORMER_HUMAN_SHOTGUN", NULL },
	{ "A_VileChase",    AF_CHASER | AF_RAISER,    "RESCHASE", NULL, NULL },
	{ "A_VileStart",    0,          "PLAYSOUND(VILATK)", NULL, NULL },
	{ "A_VileTarget",   0,          "RANGE_ATTACK", "R:ARCHVILE_FIRE", NULL },
	{ "A_VileAttack",   0,          "EFFECTTRACKER", NULL, NULL },
	{ "A_StartFire",    0,          "TRACKERSTART", NULL, NULL },
	{ "A_Fire",         0,          "TRACKERFOLLOW", NULL, NULL },
	{ "A_FireCrackle",  0,          "TRACKERACTIVE", NULL, NULL },
	{ "A_Tracer",       0,          "RANDOM_TRACER", NULL, NULL },
	{ "A_SkelWhoosh",   AF_FACE,    "PLAYSOUND(SKESWG)", NULL, NULL },
	{ "A_SkelFist",     AF_FACE,    "CLOSE_ATTACK", "C:REVENANT_CLOSECOMBAT", NULL },
	{ "A_SkelMissile",  0,          "RANGE_ATTACK", "R:REVENANT_MISSILE", NULL },
	{ "A_FatRaise",     AF_FACE,    "PLAYSOUND(MANATK)", NULL, NULL },
	{ "A_FatAttack1",   AF_SPREAD,  "RANGE_ATTACK", "R:MANCUBUS_FIREBALL", NULL },
	{ "A_FatAttack2",   AF_SPREAD,  "RANGE_ATTACK", "R:MANCUBUS_FIREBALL", NULL },
	{ "A_FatAttack3",   AF_SPREAD,  "RANGE_ATTACK", "R:MANCUBUS_FIREBALL", NULL },
	{ "A_BossDeath",    0,          "NOTHING", NULL, NULL },
	{ "A_CPosAttack",   0,          "RANGE_ATTACK", "R:FORMER_HUMAN_CHAINGUN", NULL },
	{ "A_CPosRefire",   0,          "REFIRE_CHECK", NULL, NULL },
	{ "A_TroopAttack",  0,          "COMBOATTACK", "R:IMP_FIREBALL", "C:IMP_CLOSECOMBAT" },
	{ "A_SargAttack",   0,          "CLOSE_ATTACK", "C:DEMON_CLOSECOMBAT", NULL },
	{ "A_HeadAttack",   0,          "COMBOATTACK", "R:CACO_FIREBALL", "C:CACO_CLOSECOMBAT" },
	{ "A_BruisAttack",  0,          "COMBOATTACK", "R:BARON_FIREBALL", "C:BARON_CLOSECOMBAT" },
	{ "A_SkullAttack",  0,          "RANGE_ATTACK", "R:SKULL_ASSAULT", NULL },
	{ "A_Metal",        0,          "WALKSOUND_CHASE", NULL, NULL },
	{ "A_SpidRefire",   0,          "REFIRE_CHECK", NULL, NULL },
	{ "A_BabyMetal",    0,          "WALKSOUND_CHASE", NULL, NULL },
	{ "A_BspiAttack",   0,          "RANGE_ATTACK", "R:ARACHNOTRON_PLASMA", NULL },
	{ "A_Hoof",         0,          "PLAYSOUND(HOOF)", NULL, NULL },
	{ "A_CyberAttack",  0,          "RANGE_ATTACK", "R:CYBERDEMON_MISSILE", NULL },
	{ "A_PainAttack",   0,          "RANGE_ATTACK", "R:ELEMENTAL_SPAWNER", NULL },
	{ "A_PainDie",      AF_MAKEDEAD, "SPARE_ATTACK", "S:ELEMENTAL_DEATHSPAWN", NULL },
	{ "A_KeenDie",      AF_SPECIAL | AF_KEENDIE | AF_MAKEDEAD, "", NULL, NULL },
	{ "A_BrainPain",    0,          "MAKEPAINSOUND", NULL, NULL },
	{ "A_BrainScream",  0,          "BRAINSCREAM", NULL, NULL },
	{ "A_BrainDie",     0,          "BRAINDIE", NULL, NULL },
	{ "A_BrainAwake",   0,          "NOTHING", NULL, NULL },
	{ "A_BrainSpit",    0,          "BRAINSPIT", "R:BRAIN_CUBE", NULL },
	{ "A_SpawnSound",   0,          "MAKEACTIVESOUND", NULL, NULL },
	{ "A_SpawnFly",     0,          "CUBETRACER", NULL, NULL },
	{ "A_BrainExplode", 0,          "BRAINMISSILEEXPLODE", NULL, NULL },
	{ "A_CubeSpawn",    0,          "CUBESPAWN", NULL, NULL },

	// BOOM and MBF actions...
	{ "A_Die",          AF_SPECIAL, "", NULL, NULL },
	{ "A_Stop",         0,          "STOP", NULL, NULL },
	{ "A_Detonate",     AF_DETONATE,"EXPLOSIONDAMAGE", NULL, NULL },
	{ "A_Mushroom",     0,          "MUSHROOM", NULL, NULL },

	{ "A_Spawn",        AF_SPECIAL, "", NULL, NULL },
	{ "A_Turn",         AF_SPECIAL, "", NULL, NULL },
	{ "A_Face",         AF_SPECIAL, "", NULL, NULL },
	{ "A_Scratch",      AF_SPECIAL, "", NULL, NULL },
	{ "A_PlaySound",    AF_SPECIAL, "", NULL, NULL },
	{ "A_RandomJump",   AF_SPECIAL, "", NULL, NULL },
	{ "A_LineEffect",   AF_SPECIAL, "", NULL, NULL },

	{ "A_FireOldBFG",   0,  "W:SHOOT", "R:INTERNAL_FIRE_OLD_BFG", NULL },
	{ "A_BetaSkullAttack", 0, "RANGE_ATTACK", "R:INTERNAL_BETA_LOST_SOUL_ATTACK", NULL },
};


//------------------------------------------------------------------------

struct staterange_t
{
	int obj_num;  // thing or weapon
	int start1, end1;
	int start2, end2;
};

const staterange_t thing_range[] =
{
	// Things...
	{ MT_PLAYER, S_PLAY, S_PLAY_XDIE9, -1,-1 },
	{ MT_POSSESSED, S_POSS_STND, S_POSS_RAISE4, -1,-1 },
	{ MT_SHOTGUY, S_SPOS_STND, S_SPOS_RAISE5, -1,-1 },
	{ MT_VILE, S_VILE_STND, S_VILE_DIE10, -1,-1 },
	{ MT_UNDEAD, S_SKEL_STND, S_SKEL_RAISE6, -1,-1 },
	{ MT_SMOKE, S_SMOKE1, S_SMOKE5, -1,-1 },
	{ MT_FATSO, S_FATT_STND, S_FATT_RAISE8, -1,-1 },
	{ MT_CHAINGUY, S_CPOS_STND, S_CPOS_RAISE7, -1,-1 },
	{ MT_TROOP, S_TROO_STND, S_TROO_RAISE5, -1,-1 },
	{ MT_SERGEANT, S_SARG_STND, S_SARG_RAISE6, -1,-1 },
	{ MT_SHADOWS, S_SARG_STND, S_SARG_RAISE6, -1,-1 },
	{ MT_HEAD, S_HEAD_STND, S_HEAD_RAISE6, -1,-1 },
	{ MT_BRUISER, S_BOSS_STND, S_BOSS_RAISE7, -1,-1 },
	{ MT_KNIGHT, S_BOS2_STND, S_BOS2_RAISE7, -1,-1 },
	{ MT_SKULL, S_SKULL_STND, S_SKULL_DIE6, -1,-1 },
	{ MT_SPIDER, S_SPID_STND, S_SPID_DIE11, -1,-1 },
	{ MT_BABY, S_BSPI_STND, S_BSPI_RAISE7, -1,-1 },
	{ MT_CYBORG, S_CYBER_STND, S_CYBER_DIE10, -1,-1 },
	{ MT_PAIN, S_PAIN_STND, S_PAIN_RAISE6, -1,-1 },
	{ MT_WOLFSS, S_SSWV_STND, S_SSWV_RAISE5, -1,-1 },
	{ MT_KEEN, S_KEENSTND, S_KEENPAIN2, -1,-1 },
	{ MT_BOSSBRAIN, S_BRAIN, S_BRAIN_DIE4, -1,-1 },
	{ MT_BOSSSPIT, S_BRAINEYE, S_BRAINEYE1, -1,-1 },
	{ MT_BARREL, S_BAR1, S_BEXP5, -1,-1 },
	{ MT_PUFF, S_PUFF1, S_PUFF4, -1,-1 },
	{ MT_BLOOD, S_BLOOD1, S_BLOOD3, -1,-1 },
	{ MT_TFOG, S_TFOG, S_TFOG10, -1,-1 },
	{ MT_IFOG, S_IFOG, S_IFOG5, -1,-1 },
	{ MT_TELEPORTMAN, S_TFOG, S_TFOG10, -1,-1 },

	{ MT_MISC0, S_ARM1, S_ARM1A, -1,-1 },
	{ MT_MISC1, S_ARM2, S_ARM2A, -1,-1 },
	{ MT_MISC2, S_BON1, S_BON1E, -1,-1 },
	{ MT_MISC3, S_BON2, S_BON2E, -1,-1 },
	{ MT_MISC4, S_BKEY, S_BKEY2, -1,-1 },
	{ MT_MISC5, S_RKEY, S_RKEY2, -1,-1 },
	{ MT_MISC6, S_YKEY, S_YKEY2, -1,-1 },
	{ MT_MISC7, S_YSKULL, S_YSKULL2, -1,-1 },
	{ MT_MISC8, S_RSKULL, S_RSKULL2, -1,-1 },
	{ MT_MISC9, S_BSKULL, S_BSKULL2, -1,-1 },
	{ MT_MISC10, S_STIM, S_STIM, -1,-1 },
	{ MT_MISC11, S_MEDI, S_MEDI, -1,-1 },
	{ MT_MISC12, S_SOUL, S_SOUL6, -1,-1 },
	{ MT_INV, S_PINV, S_PINV4, -1,-1 },
	{ MT_MISC13, S_PSTR, S_PSTR, -1,-1 },
	{ MT_INS, S_PINS, S_PINS4, -1,-1 },
	{ MT_MISC14, S_SUIT, S_SUIT, -1,-1 },
	{ MT_MISC15, S_PMAP, S_PMAP6, -1,-1 },
	{ MT_MISC16, S_PVIS, S_PVIS2, -1,-1 },
	{ MT_MEGA, S_MEGA, S_MEGA4, -1,-1 },
	{ MT_CLIP, S_CLIP, S_CLIP, -1,-1 },
	{ MT_MISC17, S_AMMO, S_AMMO, -1,-1 },
	{ MT_MISC18, S_ROCK, S_ROCK, -1,-1 },
	{ MT_MISC19, S_BROK, S_BROK, -1,-1 },
	{ MT_MISC20, S_CELL, S_CELL, -1,-1 },
	{ MT_MISC21, S_CELP, S_CELP, -1,-1 },
	{ MT_MISC22, S_SHEL, S_SHEL, -1,-1 },
	{ MT_MISC23, S_SBOX, S_SBOX, -1,-1 },
	{ MT_MISC24, S_BPAK, S_BPAK, -1,-1 },
	{ MT_MISC25, S_BFUG, S_BFUG, -1,-1 },
	{ MT_CHAINGUN, S_MGUN, S_MGUN, -1,-1 },
	{ MT_MISC26, S_CSAW, S_CSAW, -1,-1 },
	{ MT_MISC27, S_LAUN, S_LAUN, -1,-1 },
	{ MT_MISC28, S_PLAS, S_PLAS, -1,-1 },
	{ MT_SHOTGUN, S_SHOT, S_SHOT, -1,-1 },
	{ MT_SUPERSHOTGUN, S_SHOT2, S_SHOT2, -1,-1 },

	{ MT_MISC29, S_TECHLAMP, S_TECHLAMP4, -1,-1 },
	{ MT_MISC30, S_TECH2LAMP, S_TECH2LAMP4, -1,-1 },
	{ MT_MISC31, S_COLU, S_COLU, -1,-1 },
	{ MT_MISC32, S_TALLGRNCOL, S_TALLGRNCOL, -1,-1 },
	{ MT_MISC33, S_SHRTGRNCOL, S_SHRTGRNCOL, -1,-1 },
	{ MT_MISC34, S_TALLREDCOL, S_TALLREDCOL, -1,-1 },
	{ MT_MISC35, S_SHRTREDCOL, S_SHRTREDCOL, -1,-1 },
	{ MT_MISC36, S_SKULLCOL, S_SKULLCOL, -1,-1 },
	{ MT_MISC37, S_HEARTCOL, S_HEARTCOL2, -1,-1 },
	{ MT_MISC38, S_EVILEYE, S_EVILEYE4, -1,-1 },
	{ MT_MISC39, S_FLOATSKULL, S_FLOATSKULL3, -1,-1 },
	{ MT_MISC40, S_TORCHTREE, S_TORCHTREE, -1,-1 },
	{ MT_MISC41, S_BLUETORCH, S_BLUETORCH4, -1,-1 },
	{ MT_MISC42, S_GREENTORCH, S_GREENTORCH4, -1,-1 },
	{ MT_MISC43, S_REDTORCH, S_REDTORCH4, -1,-1 },
	{ MT_MISC44, S_BTORCHSHRT, S_BTORCHSHRT4, -1,-1 },
	{ MT_MISC45, S_GTORCHSHRT, S_GTORCHSHRT4, -1,-1 },
	{ MT_MISC46, S_RTORCHSHRT, S_RTORCHSHRT4, -1,-1 },
	{ MT_MISC47, S_STALAGTITE, S_STALAGTITE, -1,-1 },
	{ MT_MISC48, S_TECHPILLAR, S_TECHPILLAR, -1,-1 },
	{ MT_MISC49, S_CANDLESTIK, S_CANDLESTIK, -1,-1 },
	{ MT_MISC50, S_CANDELABRA, S_CANDELABRA, -1,-1 },
	{ MT_MISC51, S_BLOODYTWITCH, S_BLOODYTWITCH4, -1,-1 },
	{ MT_MISC60, S_BLOODYTWITCH, S_BLOODYTWITCH4, -1,-1 },
	{ MT_MISC52, S_MEAT2, S_MEAT2, -1,-1 },
	{ MT_MISC53, S_MEAT3, S_MEAT3, -1,-1 },
	{ MT_MISC54, S_MEAT4, S_MEAT4, -1,-1 },
	{ MT_MISC55, S_MEAT5, S_MEAT5, -1,-1 },
	{ MT_MISC56, S_MEAT2, S_MEAT2, -1,-1 },
	{ MT_MISC57, S_MEAT4, S_MEAT4, -1,-1 },
	{ MT_MISC58, S_MEAT3, S_MEAT3, -1,-1 },
	{ MT_MISC59, S_MEAT5, S_MEAT5, -1,-1 },
	{ MT_MISC61, S_HEAD_DIE6, S_HEAD_DIE6, -1,-1 },
	{ MT_MISC62, S_PLAY_DIE7, S_PLAY_DIE7, -1,-1 },
	{ MT_MISC63, S_POSS_DIE5, S_POSS_DIE5, -1,-1 },
	{ MT_MISC64, S_SARG_DIE6, S_SARG_DIE6, -1,-1 },
	{ MT_MISC65, S_SKULL_DIE6, S_SKULL_DIE6, -1,-1 },
	{ MT_MISC66, S_TROO_DIE5, S_TROO_DIE5, -1,-1 },
	{ MT_MISC67, S_SPOS_DIE5, S_SPOS_DIE5, -1,-1 },
	{ MT_MISC68, S_PLAY_XDIE9, S_PLAY_XDIE9, -1,-1 },
	{ MT_MISC69, S_PLAY_XDIE9, S_PLAY_XDIE9, -1,-1 },
	{ MT_MISC70, S_HEADSONSTICK, S_HEADSONSTICK, -1,-1 },
	{ MT_MISC71, S_GIBS, S_GIBS, -1,-1 },
	{ MT_MISC72, S_HEADONASTICK, S_HEADONASTICK, -1,-1 },
	{ MT_MISC73, S_HEADCANDLES, S_HEADCANDLES2, -1,-1 },
	{ MT_MISC74, S_DEADSTICK, S_DEADSTICK, -1,-1 },
	{ MT_MISC75, S_LIVESTICK, S_LIVESTICK2, -1,-1 },
	{ MT_MISC76, S_BIGTREE, S_BIGTREE, -1,-1 },
	{ MT_MISC77, S_BBAR1, S_BBAR3, -1,-1 },
	{ MT_MISC78, S_HANGNOGUTS, S_HANGNOGUTS, -1,-1 },
	{ MT_MISC79, S_HANGBNOBRAIN, S_HANGBNOBRAIN, -1,-1 },
	{ MT_MISC80, S_HANGTLOOKDN, S_HANGTLOOKDN, -1,-1 },
	{ MT_MISC81, S_HANGTSKULL, S_HANGTSKULL, -1,-1 },
	{ MT_MISC82, S_HANGTLOOKUP, S_HANGTLOOKUP, -1,-1 },
	{ MT_MISC83, S_HANGTNOBRAIN, S_HANGTNOBRAIN, -1,-1 },
	{ MT_MISC84, S_COLONGIBS, S_COLONGIBS, -1,-1 },
	{ MT_MISC85, S_SMALLPOOL, S_SMALLPOOL, -1,-1 },
	{ MT_MISC86, S_BRAINSTEM, S_BRAINSTEM, -1,-1 },

	/* BRAIN_DEATH_MISSILE : S_BRAINEXPLODE1, S_BRAINEXPLODE3 */

	// Attacks...
	{ MT_FIRE, S_FIRE1, S_FIRE30, -1,-1 },
	{ MT_TRACER, S_TRACER, S_TRACEEXP3, -1,-1 },
	{ MT_FATSHOT, S_FATSHOT1, S_FATSHOTX3, -1,-1 },
	{ MT_BRUISERSHOT, S_BRBALL1, S_BRBALLX3, -1,-1 },
	{ MT_SPAWNSHOT, S_SPAWN1, S_SPAWNFIRE8, -1,-1 },
	{ MT_TROOPSHOT, S_TBALL1, S_TBALLX3, -1,-1 },
	{ MT_HEADSHOT, S_RBALL1, S_RBALLX3, -1,-1 },
	{ MT_ARACHPLAZ, S_ARACH_PLAZ, S_ARACH_PLEX5, -1,-1 },
	{ MT_ROCKET, S_ROCKET, S_ROCKET, S_EXPLODE1, S_EXPLODE3 },
	{ MT_PLASMA, S_PLASBALL, S_PLASEXP5, -1,-1 },
	{ MT_BFG, S_BFGSHOT, S_BFGLAND6, -1,-1 },
	{ MT_EXTRABFG, S_BFGEXP, S_BFGEXP4, -1,-1 },

	// Boom/MBF stuff...
	{ MT_DOGS, S_DOGS_STND, S_DOGS_RAISE6, -1,-1 },

	{ MT_PLASMA1, S_PLS1BALL, S_PLS1EXP5, -1,-1 },
	{ MT_PLASMA2, S_PLS2BALL, S_PLS2BALLX3, -1,-1 },
	{ MT_SCEPTRE, S_BON3, S_BON3, -1,-1 },
	{ MT_BIBLE,   S_BON4, S_BON4, -1,-1 },

	{ -1, 0, 0, 0, 0 }  // End sentinel
};

const staterange_t weapon_range[] =
{
	{ wp_fist,         S_PUNCH,   S_PUNCH5, -1,-1 },
	{ wp_chainsaw,     S_SAW,     S_SAW3,   -1,-1 },
	{ wp_pistol,       S_PISTOL,  S_PISTOLFLASH,   S_LIGHTDONE, S_LIGHTDONE },
	{ wp_shotgun,      S_SGUN,    S_SGUNFLASH2,    S_LIGHTDONE, S_LIGHTDONE },
	{ wp_chaingun,     S_CHAIN,   S_CHAINFLASH2,   S_LIGHTDONE, S_LIGHTDONE },
	{ wp_missile,      S_MISSILE, S_MISSILEFLASH4, S_LIGHTDONE, S_LIGHTDONE },
	{ wp_plasma,       S_PLASMA,  S_PLASMAFLASH2,  S_LIGHTDONE, S_LIGHTDONE },
	{ wp_bfg,          S_BFG,     S_BFGFLASH2,     S_LIGHTDONE, S_LIGHTDONE },
	{ wp_supershotgun, S_DSGUN,   S_DSGUNFLASH2,   S_LIGHTDONE, S_LIGHTDONE },

	{ -1, 0, 0, 0, 0 }  // End sentinel
};


//------------------------------------------------------------------------

void Frames::Init()
{
	new_states.clear();
	argument_mem.clear();
}


void Frames::Shutdown()
{
	for (size_t i = 0 ; i < new_states.size() ; i++)
		if (new_states[i] != NULL)
			delete new_states[i];

	new_states.clear();
	argument_mem.clear();
}


void Frames::MarkState(int st_num)
{
	// this is possible since binary patches store the dummy state
	if (st_num == S_NULL)
		return;

	// fill any missing slots with NULLs, including the one we want
	while ((int)new_states.size() < st_num+1)
	{
		new_states.push_back(NULL);
	}

	// already have a modified entry?
	if (new_states[st_num] != NULL)
		return;

	state_t * entry = new state_t;
	new_states[st_num] = entry;

	// copy the original info, if we have one
	if (st_num < NUMSTATES_MBF)
	{
		memcpy(entry, &states_orig[st_num], sizeof(state_t));
	}
	else
	{
		// these defaults follow the DSDehacked specs
		entry->sprite = SPR_TNT1;
		entry->frame  = 0;
		entry->tics   = -1;
		entry->action = A_NULL;
		entry->nextstate = st_num;
		entry->argptr = 0;
	}
}


const state_t * Frames::NewStateElseOld(int st_num)
{
	if (st_num < 0)
		return NULL;

	if (st_num < (int)new_states.size())
		if (new_states[st_num] != NULL)
			return new_states[st_num];

	if (st_num < NUMSTATES_MBF)
		return &states_orig[st_num];

	return NULL;
}


state_t * Frames::GetModifiedState(int st_num)
{
	// this is possible since binary patches store the dummy state
	if (st_num == S_NULL)
	{
		static state_t dummy;
		return &dummy;
	}

	MarkState(st_num);

	return new_states[st_num];
}


int Frames::GetStateSprite(int st_num)
{
	const state_t * st = NewStateElseOld(st_num);

	if (st == NULL)
		return -1;

	return st->sprite;
}


bool Frames::CheckMissileState(int st_num)
{
	if (st_num == S_NULL)
		return false;

	const state_t *mis_st = NewStateElseOld(st_num);

	if (mis_st == NULL)
		return false;

	return (mis_st->tics >= 0 && mis_st->nextstate != S_NULL);
}


bool Frames::DependRangeWasModified(int low, int high)
{
	if (high < 0)
		return false;

	assert(low <= high);
	assert(low >  S_NULL);

	if (high >= (int)new_states.size())
		high  = (int)new_states.size() - 1;

	for (int i = low ; i <= high ; i++)
		if (new_states[i] != NULL)
			return true;

	return false;
}


void Frames::StateDependencies()
{
	// the goal here is to mark *existing* things and weapons whose
	// states have been modified, so that we generate the DDF for
	// the thing/weapon which has the new states.  modified or new
	// things/weapons don't need this (already been marked). 

	for (int w = 0 ; weapon_range[w].obj_num >= 0 ; w++)
	{
		const staterange_t& R = weapon_range[w];

		if (DependRangeWasModified(R.start1, R.end1) ||
		    DependRangeWasModified(R.start2, R.end2))
		{
			Weapons::MarkWeapon(R.obj_num);
		}
	}

	// check things....

	for (int t = 0 ; thing_range[t].obj_num >= 0 ; t++)
	{
		const staterange_t& R = thing_range[t];

		if (DependRangeWasModified(R.start1, R.end1) ||
		    DependRangeWasModified(R.start2, R.end2))
		{
			Things::MarkThing(R.obj_num);
		}
	}
}


void Frames::MarkStatesWithSprite(int spr_num)
{
	// only need to handle old states here
	for (int st = 1 ; st < NUMSTATES_MBF ; st++)
		if (states_orig[st].sprite == spr_num)
			MarkState(st);
}


int Frames::ReadArg(const state_t *st, int i)
{
	// the given state can be old or new here.

	if (st->argptr == 0)
		return 0;

	int ofs = (st->argptr - 1) * 8;
	return argument_mem[ofs + i];
}


void Frames::WriteArg(state_t *st, int i, int value)
{
	// the given state MUST be a new one (in new_states).
	// allocates a group of eight ints, unless done before.

	if (st->argptr == 0)
	{
		for (int k = 0 ; k < 8 ; k++)
			argument_mem.push_back(0);

		st->argptr = (int)argument_mem.size() / 8;
	}

	int ofs = (st->argptr - 1) * 8;
	argument_mem[ofs + i] = value;
}


//------------------------------------------------------------------------

void Frames::ResetGroups()
{
	groups.clear();
	group_for_state.clear();
	offset_for_state.clear();

	attack_slot[0] = NULL;
	attack_slot[1] = NULL;
	attack_slot[2] = NULL;

	act_flags = 0;
}


int Frames::BeginGroup(char group, int first)
{
	if (first == S_NULL)
		return 0;

	// create the group info
	groups[group] = group_info_t { group, { first } };

	 group_for_state[first] = group;
	offset_for_state[first] = 1;

	return 1;
}


bool Frames::SpreadGroupPass(bool alt_jumps)
{
	bool changes = false;

	int total = std::max((int)NUMSTATES_MBF, (int)new_states.size());

	for (int i = 1 ; i < total ; i++)
	{
		const state_t *st = NewStateElseOld(i);
		if (st == NULL)
			continue;

		if (group_for_state.find(i) == group_for_state.end())
			continue;

		char group = group_for_state[i];
		group_info_t& G = groups[group];

		// check if this is the very first state of death or overkill sequence.
		// in vanilla Doom (and Boom/MBF/etc), a tics of -1 will be IGNORED when
		// *entering* such a state due to this code in P_KillMobj:
		//    ``` if (target->tics < 1)
		//            target->tics = 1;
		//    ```
		// and that means it *will* enter the next state.

		bool first_death = ((group == 'D' || group == 'X') && G.states.size() == 1);

		// hibernation?
		// if action is A_RandomJump or similar, still need to follow it!
		if (st->tics < 0 && !first_death && !alt_jumps)
			continue;

		int next = st->nextstate;

		if (alt_jumps)
		{
			next = S_NULL;
			if (st->action == A_RandomJump)
				next = ReadArg(st, 0);  // misc1
		}

		if (next == S_NULL)
			continue;

		// require next state to have no group yet
		if (group_for_state.find(next) != group_for_state.end())
			continue;

		G.states.push_back(next);

		 group_for_state[next] = group;
		offset_for_state[next] = (int)G.states.size();

		changes = true;
	}

	return changes;
}


void Frames::SpreadGroups()
{
	for (;;)
	{
		bool changes1 = SpreadGroupPass(false);
		bool changes2 = SpreadGroupPass(true);

		if (! (changes1 || changes2))
			break;
	}
}


bool Frames::CheckWeaponFlash(int first)
{
	// fairly simple test, we don't need to detect looping or such here,
	// just following the states upto a small maximum is enough.

	for (int len = 0 ; len < 30 ; len++)
	{
		if (first == S_NULL)
			break;

		const state_t *st = NewStateElseOld(first);
		if (st == NULL)
			break;

		if (st->tics < 0)  // hibernation
			break;

		int act = st->action;

		assert(0 <= act && act < NUMACTIONS_MBF);

		if (action_info[act].act_flags & AF_FLASH)
			return true;

		first = st->nextstate;
	}

	return false;
}


void Frames::UpdateAttacks(char group, char *act_name, int action)
{
	const char *atk1 = action_info[action].atk_1;
	const char *atk2 = action_info[action].atk_2;

	bool free1 = true;
	bool free2 = true;

	int kind1 = -1;
	int kind2 = -1;

	if (! atk1)
	{
		return;
	}
	else if (IS_WEAPON(group))
	{
		assert(strlen(atk1) >= 3);
		assert(atk1[1] == ':');
		assert(! atk2);

		kind1 = RANGE;
	}
	else
	{
		assert(strlen(atk1) >= 3);
		assert(atk1[1] == ':');

		kind1 = (atk1[0] == 'R') ? RANGE : (atk1[0] == 'C') ? COMBAT : SPARE;
	}

	atk1 += 2;

	free1 = (! attack_slot[kind1] ||
			 StrCaseCmp(attack_slot[kind1], atk1) == 0);

	if (atk2)
	{
		assert(strlen(atk2) >= 3);
		assert(atk2[1] == ':');

		kind2 = (atk2[0] == 'R') ? RANGE : (atk2[0] == 'C') ? COMBAT : SPARE;

		atk2 += 2;

		free2 = (! attack_slot[kind2] ||
				 StrCaseCmp(attack_slot[kind2], atk2) == 0);
	}

	if (free1 && free2)
	{
		attack_slot[kind1] = atk1;

		if (atk2)
			attack_slot[kind2] = atk2;

		return;
	}

	WAD::Printf("    // Specialising %s\n", act_name);

	// do some magic to put the attack name into parenthesis,
	// for example RANGE_ATTACK(IMP_FIREBALL).

	if (StrCaseCmp(act_name, "BRAINSPIT") == 0)
	{
		PrintWarn("Multiple range attacks used with A_BrainSpit.\n");
		return;
	}

	// in this case, we have two attacks (must be a COMBOATTACK), but
	// we don't have the required slots (need both).  Therefore select
	// one of them based on the group.
	if (atk1 && atk2)
	{
		if (group != 'L' && group != 'M')
		{
			PrintWarn("Not enough attack slots for COMBOATTACK.\n");
		}

		if ((group == 'L' && kind2 == COMBAT) ||
			(group == 'M' && kind2 == RANGE))
		{
			atk1  = atk2;
			kind1 = kind2;
		}

		switch (kind1)
		{
			case RANGE:  strcpy(act_name, "RANGE_ATTACK"); break;
			case COMBAT: strcpy(act_name, "CLOSE_ATTACK"); break;
			case SPARE:  strcpy(act_name, "SPARE_ATTACK"); break;

			default: InternalError("Bad attack kind %d\n", kind1);
		}
	}

	strcat(act_name, "(");
	strcat(act_name, atk1);
	strcat(act_name, ")");
}


const char *Frames::GroupToName(char group)
{
	assert(group != 0);

	switch (group)
	{
		case 'S': return "IDLE";
		case 'E': return "CHASE";
		case 'L': return "MELEE";
		case 'M': return "MISSILE";
		case 'P': return "PAIN";
		case 'D': return "DEATH";
		case 'X': return "OVERKILL";
		case 'R': return "RESPAWN";
		case 'H': return "RESURRECT";

		// weapons
		case 'u': return "UP";
		case 'd': return "DOWN";
		case 'r': return "READY";
		case 'a': return "ATTACK";
		case 'f': return "FLASH";

		default:
			InternalError("GroupToName: BAD GROUP '%c'\n", group);
	}

	return NULL;
}


const char *Frames::RedirectorName(int next_st)
{
	static char name_buf[MAX_ACT_NAME];

	// this shouldn't happen since OutputGroup() only visits states
	// which we collected/processed as a group.
	if (group_for_state.find(next_st) == group_for_state.end())
	{
		PrintWarn("Redirection to state %d FAILED\n", next_st);
		return "IDLE";
	}

	char next_group =  group_for_state[next_st];
	int  next_ofs   = offset_for_state[next_st];

	assert(next_group != 0);
	assert(next_ofs > 0);

	if (next_ofs == 1)
		snprintf(name_buf, sizeof(name_buf), "%s", GroupToName(next_group));
	else
		snprintf(name_buf, sizeof(name_buf), "%s:%d", GroupToName(next_group), next_ofs);

	return name_buf;
}


void Frames::SpecialAction(char *act_name, const state_t *st)
{
	switch (st->action)
	{
		case A_Die:
			strcpy(act_name, "DIE");
			break;

		case A_KeenDie:
			strcpy(act_name, "KEEN_DIE");
			break;

		case A_RandomJump:
			{
				int next = ReadArg(st, 0);  // misc1
				int perc = ReadArg(st, 1);  // misc2

				if (next <= 0 || NewStateElseOld(next) == NULL)
				{
					strcpy(act_name, "NOTHING");
				}
				else
				{
					perc = perc * 100 / 256;
					if (perc < 0)   perc = 0;
					if (perc > 100) perc = 100;

					sprintf(act_name, "JUMP(%s,%d%%)", RedirectorName(next), perc);
				}
			}
			break;

		case A_Turn:
			sprintf(act_name, "TURN(%d)", MISC_TO_ANGLE(ReadArg(st, 0)));
			break;

		case A_Face:
			sprintf(act_name, "FACE(%d)", MISC_TO_ANGLE(ReadArg(st, 0)));
			break;

		case A_PlaySound:
			{
				const char * sfx = Sounds::GetSound(ReadArg(st, 0));

				if (StrCaseCmp(sfx, "NULL") == 0)
					strcpy(act_name, "NOTHING");
				else
					sprintf(act_name, "PLAYSOUND(\"%s\")", sfx);
			}
			break;

		case A_Scratch:
			{
				int damage = ReadArg(st, 0); // misc1
				int sfx_id = ReadArg(st, 1); // misc2

				if (damage == 0 && sfx_id == 0)
				{
					sprintf(act_name, "NOTHING");
				}
				else
				{
					const char *sfx = NULL;
					if (sfx_id > 0)
						sfx = Sounds::GetSound(sfx_id);
					if (sfx != NULL && StrCaseCmp(sfx, "NULL") == 0)
						sfx = NULL;

					const char *atk_name = Things::AddScratchAttack(damage, sfx);
					sprintf(act_name, "CLOSE_ATTACK(%s)", atk_name);
				}
			}
			break;

		case A_LineEffect:
			{
				int misc1 = ReadArg(st, 0);
				int misc2 = ReadArg(st, 1);

				if (misc1 <= 0)
					strcpy(act_name, "NOTHING");
				else
					sprintf(act_name, "ACTIVATE_LINETYPE(%d,%d)", misc1, misc2);
			}
			break;

		case A_Spawn:
			{
				int mt_num = ReadArg(st, 0);

				if (! Things::IsSpawnable(mt_num))
				{
					PrintWarn("Action A_SPAWN unusable type (%d)\n", mt_num);
					strcpy(act_name, "NOTHING");
				}
				else
				{
					Things::UseThing(mt_num);
					sprintf(act_name, "SPAWN(%s)", Things::GetMobjName(mt_num));
				}
			}
			break;

		default:
			InternalError("Bad special action %d\n", st->action);
	}
}


void Frames::OutputState(char group, int cur, bool do_action)
{
	assert(cur > 0);

	const state_t *st = NewStateElseOld(cur);
	if (st == NULL)
		st = &states_orig[S_TNT1];

	int action = do_action ? st->action : A_NULL;

	assert(action >= 0 && action < NUMACTIONS_MBF);

	const char *bex_name = action_info[action].bex_name;

	if (cur <= S_LAST_WEAPON_STATE)
		act_flags |= AF_WEAPON_ST;
	else
		act_flags |= AF_THING_ST;

	if (action_info[action].act_flags & AF_UNIMPL)
		PrintWarn("Frame %d: action %s is not yet supported.\n", cur, bex_name);

	char act_name[MAX_ACT_NAME];

	bool weap_act = false;

	if (action_info[action].act_flags & AF_SPECIAL)
	{
		SpecialAction(act_name, st);
	}
	else
	{
		strcpy(act_name, action_info[action].ddf_name);

		weap_act = (act_name[0] == 'W' && act_name[1] == ':');

		if (weap_act)
			strcpy(act_name, action_info[action].ddf_name + 2);
	}

	if (action != A_NULL && (weap_act == ! IS_WEAPON(group)) &&
		StrCaseCmp(act_name, "NOTHING") != 0)
	{
		if (weap_act)
			PrintWarn("Frame %d: weapon action %s used in thing.\n", cur, bex_name);
		else
			PrintWarn("Frame %d: thing action %s used in weapon.\n", cur, bex_name);

		strcpy(act_name, "NOTHING");
	}

	if (action == A_NULL || weap_act == (IS_WEAPON(group) ? true : false))
	{
		UpdateAttacks(group, act_name, action);
	}

	// If the death states contain A_PainDie or A_KeenDie, then we
	// need to add an A_Fall action for proper operation in EDGE.
	if (action_info[action].act_flags & AF_MAKEDEAD)
	{
		WAD::Printf("    %s:%c:0:%s:MAKEDEAD,  // %s\n",
			Sprites::GetSprite(st->sprite),
			'A' + ((int) st->frame & 31),
			(st->frame >= 32768 || force_fullbright) ? "BRIGHT" : "NORMAL",
			(action == A_PainDie) ? "A_PainDie" : "A_KeenDie");
	}

	if (action_info[action].act_flags & AF_FACE)
	{
		WAD::Printf("    %s:%c:0:%s:FACE_TARGET,\n",
			Sprites::GetSprite(st->sprite),
			'A' + ((int) st->frame & 31),
			(st->frame >= 32768 || force_fullbright) ? "BRIGHT" : "NORMAL");
	}

	// special handling for Mancubus attacks...
	if (action_info[action].act_flags & AF_SPREAD)
	{
		if ((act_flags & AF_SPREAD) == 0)
		{
			WAD::Printf("    %s:%c:0:%s:RESET_SPREADER,\n",
				Sprites::GetSprite(st->sprite),
				'A' + ((int) st->frame & 31),
				(st->frame >= 32768 || force_fullbright) ? "BRIGHT" : "NORMAL");
		}

		WAD::Printf("    %s:%c:0:%s:%s,  // A_FatAttack\n",
			Sprites::GetSprite(st->sprite),
			'A' + ((int) st->frame & 31),
			(st->frame >= 32768 || force_fullbright) ? "BRIGHT" : "NORMAL", act_name);
	}

	int tics = (int) st->tics;

	// kludge for EDGE and Batman TC.  EDGE waits 35 tics before exiting the
	// level from A_BrainDie, but standard Doom does it immediately.  Oddly,
	// Batman TC goes into a loop calling A_BrainDie every tic.
	if (tics >= 0 && tics < 44 && StrCaseCmp(act_name, "BRAINDIE") == 0)
		tics = 44;

	WAD::Printf("    %s:%c:%d:%s:%s",
		Sprites::GetSprite(st->sprite),
		'A' + ((int) st->frame & 31), tics,
		(st->frame >= 32768 || force_fullbright) ? "BRIGHT" : "NORMAL", act_name);

	if (action != A_NULL && weap_act == ! IS_WEAPON(group))
		return;

	act_flags |= action_info[action].act_flags;
}


bool Frames::OutputSpawnState(int first)
{
	// returns true if no IDLE states will be needed

	WAD::Printf("\n");
	WAD::Printf("STATES(SPAWN) =\n");

	const state_t *st = NewStateElseOld(first);
	if (st == NULL)
		st = &states_orig[S_TNT1];

	OutputState('S', first, false);

	int next = st->nextstate;

	if (st->tics < 0)
	{
		// goes into hibernation
		WAD::Printf(";\n");
		return true;
	}
	else if (next == S_NULL)
	{
		WAD::Printf(",#REMOVE;\n");
		return true;
	}
	else
	{
		WAD::Printf(",#%s;\n", RedirectorName(next));
		return false;
	}
}


void Frames::OutputGroup(char group)
{
	auto GIT = groups.find(group);

	if (GIT == groups.end())
		return;

	group_info_t& G = GIT->second;

	// generate STATES(SPAWN) here, before doing the IDLE ones.
	// this is to emulate BOOM/MBF, which don't execute the very first
	// action when an object is spawned, but EDGE *does* execute it.
	if (group == 'S')
	{
		if (OutputSpawnState(G.states[0]))
			return;
	}

	WAD::Printf("\n");
	WAD::Printf("STATES(%s) =\n", GroupToName(group));

	for (size_t i = 0 ; i < G.states.size() ; i++)
	{
		int cur = G.states[i];
		bool is_last = (i == G.states.size() - 1);

		OutputState(group, cur, true);

		const state_t *st = NewStateElseOld(cur);
		assert(st);

		int next = st->nextstate;

		if (st->tics < 0)
		{
			// go into hibernation (nothing needed)
		}
		else if (next == S_NULL)
		{
			WAD::Printf(",#REMOVE");
		}
		else if (is_last || next != G.states[i+1])
		{
			WAD::Printf(",#%s", RedirectorName(next));
		}

		if (is_last)
		{
			WAD::Printf(";\n");
			return;
		}

		WAD::Printf(",\n");
	}
}


//------------------------------------------------------------------------

namespace Frames
{
#define FIELD_OFS(xxx)  offsetof(state_t, xxx)

	const fieldreference_t frame_field[] =
	{
		{ "Sprite number",    FIELD_OFS(sprite),    FT_SPRITE },
		{ "Sprite subnumber", FIELD_OFS(frame),     FT_SUBSPR },
		{ "Duration",         FIELD_OFS(tics),      FT_ANY },
		{ "Next frame",       FIELD_OFS(nextstate), FT_FRAME },

		{ NULL, 0, FT_ANY }   // End sentinel
	};
}


void Frames::AlterFrame(int new_val)
{
	int st_num = Patch::active_obj;
	const char *field_name = Patch::line_buf;

	assert(st_num >= 0);

	// the S_NULL state is never output, no need to change it
	if (st_num == S_NULL)
		return;

	MarkState(st_num);

	state_t * st = new_states[st_num];
	assert(st);

	if (StrCaseCmp(field_name, "Action pointer") == 0)
	{
		PrintWarn("Line %d: raw Action pointer not supported.\n",
			Patch::line_num);
		return;
	}

	if (StrCaseCmp(field_name, "Unknown 1") == 0)
	{
		WriteArg(st, 0, new_val);
		return;
	}

	if (StrCaseCmp(field_name, "Unknown 2") == 0)
	{
		WriteArg(st, 1, new_val);
		return;
	}

	if (StrCaseCmpPartial(field_name, "Args") == 0)
	{
		int arg = atoi(field_name + 4);
		if (arg >= 1 && arg <= 8)
		{
			WriteArg(st, arg - 1, new_val);
			return;
		}
	}

	if (! Field_Alter(frame_field, field_name, (int *)st, new_val))
	{
		PrintWarn("UNKNOWN FRAME FIELD: %s\n", field_name);
		return;
	}

	MarkState(st_num);
}


void Frames::AlterPointer(int new_val)
{
	int st_num = Patch::active_obj;
	const char *deh_field = Patch::line_buf;

	assert(st_num >= 0);

	// the S_NULL state is never output, no need to change it
	if (st_num == S_NULL)
		return;

	MarkState(st_num);

	state_t * st = new_states[st_num];
	assert(st);

	if (StrCaseCmp(deh_field, "Codep Frame") != 0)
	{
		PrintWarn("UNKNOWN POINTER FIELD: %s\n", deh_field);
		return;
	}

	if (new_val < 0 || new_val >= NUMSTATES_MBF)
	{
		PrintWarn("Line %d: Illegal Codep frame number: %d\n",
			Patch::line_num, new_val);
		return;
	}

	st->action = states_orig[new_val].action;
}


void Frames::AlterBexCodePtr(const char * new_action)
{
	const char *bex_field = Patch::line_buf;

	if (StrCaseCmpPartial(bex_field, "FRAME ") != 0)
	{
		PrintWarn("Line %d: bad code pointer '%s' - must begin with FRAME.\n",
			Patch::line_num, bex_field);
		return;
	}

	int st_num;

	if (sscanf(bex_field + 6, " %i ", &st_num) != 1)
	{
		PrintWarn("Line %d: unreadable FRAME number: %s\n",
			Patch::line_num, bex_field + 6);
		return;
	}

	if (st_num < 0 || st_num > 32767)
	{
		PrintWarn("Line %d: illegal FRAME number: %d\n",
			Patch::line_num, st_num);
		return;
	}

	// the S_NULL state is never output, no need to change it
	if (st_num == S_NULL)
		return;

	MarkState(st_num);

	state_t * st = new_states[st_num];
	assert(st);

	int action;

	for (action = 0 ; action < NUMACTIONS_MBF ; action++)
	{
		// use +2 here to ignore the "A_" prefix
		if (StrCaseCmp(action_info[action].bex_name + 2, new_action) == 0)
		{
			// found it!
			st->action = action;
			return;
		}
	}

	PrintWarn("Line %d: unknown action %s for CODEPTR.\n",
		Patch::line_num, new_action);
}

}  // Deh_Edge
