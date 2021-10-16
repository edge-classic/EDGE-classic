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

#include "i_defs.h"
#include "frames.h"

#include "buffer.h"
#include "convert.h"
#include "dh_plugin.h"
#include "info.h"
#include "patch.h"
#include "sounds.h"
#include "storage.h"
#include "system.h"
#include "text.h"
#include "things.h"
#include "util.h"
#include "wad.h"
#include "weapons.h"

#include <assert.h>
#include <ctype.h>
#include <string.h>

namespace Deh_Edge
{

#define DEBUG_RANGES  0  // must enable one in info.cpp too
#define DEBUG_FRAMES  0

#define MAX_ACT_NAME  256


bool state_modified[NUMSTATES_BEX];

statedyn_t state_dyn[NUMSTATES_BEX];


const char *Frames::attack_slot[3];

int Frames::act_flags;

namespace Frames
{
	int lowest_touched;
	int highest_touched;

	// forward decls
	void InstallRandomJump(int src, int first);
	const char *GroupToName(char group, bool use_spawn);
	const char *RedirectorName(int next_st, bool use_spawn);
	void SpecialAction(char *buf, state_t *st, bool use_spawn);
	void OutputState(char group, int cur, bool use_spawn);
}


#define IS_WEAPON(group)  (islower(group))


typedef struct
{
	const char *ddf_name;

	int act_flags;

	// attacks implied by the action, often NULL.  The format is
	// "X:ATTACK_NAME" where X is 'R' for range attacks, 'C' for
	// close-combat attacks, and 'S' for spare attacks.
	const char *atk_1;
	const char *atk_2;

	const char *bex_name;
}
actioninfo_t;

const actioninfo_t action_info[NUMACTIONS_BEX] =
{
    { "NOTHING", 0, NULL, NULL,    "A_NULL" },

    { "W:LIGHT0", 0, NULL, NULL,   "A_Light0" },
    { "W:READY", 0, NULL, NULL,    "A_WeaponReady" },
    { "W:LOWER", 0, NULL, NULL,    "A_Lower" },
    { "W:RAISE", 0, NULL, NULL,    "A_Raise" },
    { "W:SHOOT", 0, "C:PLAYER_PUNCH", NULL, "A_Punch" },
    { "W:REFIRE", 0, NULL, NULL,            "A_ReFire" },
    { "W:SHOOT", AF_FLASH, "R:PLAYER_PISTOL", NULL,   "A_FirePistol" },
    { "W:LIGHT1", 0, NULL, NULL,            "A_Light1" },
    { "W:SHOOT", AF_FLASH, "R:PLAYER_SHOTGUN", NULL,  "A_FireShotgun" },
    { "W:LIGHT2", 0, NULL, NULL,            "A_Light2" },
    { "W:SHOOT", AF_FLASH, "R:PLAYER_SHOTGUN2", NULL, "A_FireShotgun2" },
    { "W:CHECKRELOAD", 0, NULL, NULL,       "A_CheckReload" },
    { "W:PLAYSOUND(DBOPN)", 0, NULL, NULL,  "A_OpenShotgun2" },
    { "W:PLAYSOUND(DBLOAD)", 0, NULL, NULL, "A_LoadShotgun2" },
    { "W:PLAYSOUND(DBCLS)", 0, NULL, NULL,  "A_CloseShotgun2" },
    { "W:SHOOT", AF_FLASH, "R:PLAYER_CHAINGUN", NULL, "A_FireCGun" },
    { "W:FLASH", AF_FLASH, NULL, NULL,        "A_GunFlash" },
    { "W:SHOOT", 0, "R:PLAYER_MISSILE", NULL, "A_FireMissile" },
    { "W:SHOOT", 0, "C:PLAYER_SAW", NULL,     "A_Saw" },
    { "W:SHOOT", AF_FLASH, "R:PLAYER_PLASMA", NULL,            "A_FirePlasma" },
    { "W:PLAYSOUND(BFG)", 0, NULL, NULL,      "A_BFGsound" },
    { "W:SHOOT", 0, "R:PLAYER_BFG9000", NULL, "A_FireBFG" },

    { "SPARE_ATTACK", 0, NULL, NULL,          "A_BFGSpray" },
    { "EXPLOSIONDAMAGE", AF_EXPLODE, NULL, NULL, "A_Explode" },
    { "MAKEPAINSOUND", 0, NULL, NULL,         "A_Pain" },
    { "PLAYER_SCREAM", 0, NULL, NULL,         "A_PlayerScream" },
    { "MAKEDEAD", AF_FALLER, NULL, NULL,      "A_Fall" },
    { "MAKEOVERKILLSOUND", 0, NULL, NULL,     "A_XScream" },
    { "LOOKOUT", AF_LOOK, NULL, NULL,         "A_Look" },
    { "CHASE", AF_CHASER, NULL, NULL,         "A_Chase" },
    { "FACETARGET", 0, NULL, NULL,            "A_FaceTarget" },
    { "RANGE_ATTACK", 0, "R:FORMER_HUMAN_PISTOL", NULL,   "A_PosAttack" },
    { "MAKEDEATHSOUND", 0, NULL, NULL,        "A_Scream" },
    { "RANGE_ATTACK", 0, "R:FORMER_HUMAN_SHOTGUN", NULL,  "A_SPosAttack" },
    { "RESCHASE", AF_CHASER | AF_RAISER, NULL, NULL,      "A_VileChase" },
    { "PLAYSOUND(VILATK)", 0, NULL, NULL,     "A_VileStart" },
    { "RANGE_ATTACK", 0, "R:ARCHVILE_FIRE", NULL,         "A_VileTarget" },
    { "EFFECTTRACKER", 0, NULL, NULL,         "A_VileAttack" },
    { "TRACKERSTART", 0, NULL, NULL,          "A_StartFire" },
    { "TRACKERFOLLOW", 0, NULL, NULL,         "A_Fire" },
    { "TRACKERACTIVE", 0, NULL, NULL,         "A_FireCrackle" },
    { "RANDOM_TRACER", 0, NULL, NULL,         "A_Tracer" },
    { "PLAYSOUND(SKESWG)", AF_FACE, NULL, NULL,                "A_SkelWhoosh" },
    { "CLOSE_ATTACK", AF_FACE, "C:REVENANT_CLOSECOMBAT", NULL, "A_SkelFist" },
    { "RANGE_ATTACK", 0, "R:REVENANT_MISSILE", NULL,           "A_SkelMissile" },
    { "PLAYSOUND(MANATK)", AF_FACE, NULL, NULL,                "A_FatRaise" },
    { "RANGE_ATTACK", AF_SPREAD, "R:MANCUBUS_FIREBALL", NULL,  "A_FatAttack1" },
    { "RANGE_ATTACK", AF_SPREAD, "R:MANCUBUS_FIREBALL", NULL,  "A_FatAttack2" },
    { "RANGE_ATTACK", AF_SPREAD, "R:MANCUBUS_FIREBALL", NULL,  "A_FatAttack3" },
    { "NOTHING", 0, NULL, NULL,  "A_BossDeath" },
    { "RANGE_ATTACK", 0, "R:FORMER_HUMAN_CHAINGUN", NULL,      "A_CPosAttack" },
    { "REFIRE_CHECK", 0, NULL, NULL,  "A_CPosRefire" },
    { "COMBOATTACK", 0, "R:IMP_FIREBALL", "C:IMP_CLOSECOMBAT", "A_TroopAttack" },
    { "CLOSE_ATTACK", 0, "C:DEMON_CLOSECOMBAT", NULL,          "A_SargAttack" },
    { "COMBOATTACK", 0, "R:CACO_FIREBALL", "C:CACO_CLOSECOMBAT","A_HeadAttack" },
    { "COMBOATTACK", 0, "R:BARON_FIREBALL", "C:BARON_CLOSECOMBAT","A_BruisAttack" },
    { "RANGE_ATTACK", 0, "R:SKULL_ASSAULT", NULL,  "A_SkullAttack" },
    { "WALKSOUND_CHASE", 0, NULL, NULL,            "A_Metal" },
    { "REFIRE_CHECK", 0, NULL, NULL,               "A_SpidRefire" },
    { "WALKSOUND_CHASE", 0, NULL, NULL,            "A_BabyMetal" },
    { "RANGE_ATTACK", 0, "R:ARACHNOTRON_PLASMA", NULL, "A_BspiAttack" },
    { "PLAYSOUND(HOOF)", 0, NULL, NULL,            "A_Hoof" },
    { "RANGE_ATTACK", 0, "R:CYBERDEMON_MISSILE", NULL, "A_CyberAttack" },
    { "RANGE_ATTACK", 0, "R:ELEMENTAL_SPAWNER", NULL,  "A_PainAttack" },
    { "SPARE_ATTACK", AF_MAKEDEAD, "S:ELEMENTAL_DEATHSPAWN", NULL, "A_PainDie" },
    { "KEEN_DIE", AF_SPECIAL | AF_KEENDIE | AF_MAKEDEAD, NULL, NULL, "A_KeenDie" },
    { "MAKEPAINSOUND", 0, NULL, NULL,       "A_BrainPain" },
    { "BRAINSCREAM", 0, NULL, NULL,         "A_BrainScream" },
    { "BRAINDIE", 0, NULL, NULL,            "A_BrainDie" },
    { "NOTHING", 0, NULL, NULL,             "A_BrainAwake" },
    { "BRAINSPIT", 0, "R:BRAIN_CUBE", NULL, "A_BrainSpit" },
    { "MAKEACTIVESOUND", 0, NULL, NULL,     "A_SpawnSound" },
    { "CUBETRACER", 0, NULL, NULL,          "A_SpawnFly" },
    { "BRAINMISSILEEXPLODE", 0, NULL, NULL, "A_BrainExplode" },
    { "CUBESPAWN", 0, NULL, NULL,           "A_CubeSpawn" },  // NEW

	// BOOM and MBF:
	// FIXME !!! Require special treatment for EDGE

	{ "EXPLODE", AF_SPECIAL, NULL, NULL,    "A_Die" },
	{ "STOP",    0,          NULL, NULL,    "A_Stop" },
	{ "EXPLOSIONDAMAGE", AF_DETONATE, NULL, NULL, "A_Detonate" },
	{ "NOTHING", AF_UNIMPL, NULL, NULL,    "A_Mushroom" },

	{ "NOTHING", AF_SPECIAL, NULL, NULL,    "A_Spawn" },
	{ "NOTHING", AF_SPECIAL, NULL, NULL,    "A_Turn" },
	{ "NOTHING", AF_SPECIAL, NULL, NULL,    "A_Face" },
	{ "NOTHING", AF_UNIMPL,  NULL, NULL,    "A_Scratch" },
	{ "NOTHING", AF_SPECIAL, NULL, NULL,    "A_PlaySound" },
	{ "NOTHING", AF_SPECIAL, NULL, NULL,    "A_RandomJump" },  // special
	{ "NOTHING", AF_SPECIAL, NULL, NULL,    "A_LineEffect" }
};


//------------------------------------------------------------------------

typedef struct
{
	int obj_num;  // thing or weapon
	int start1, end1;
	int start2, end2;
}
staterange_t;

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
	{ MT_PUSH, S_TNT1, S_TNT1, -1,-1 },
	{ MT_PULL, S_TNT1, S_TNT1, -1,-1 },
	{ MT_DOGS, S_DOGS_STND, S_DOGS_RAISE6, -1,-1 },

    { MT_STEALTHBABY, S_BSPI_STND, S_BSPI_RAISE7, -1,-1 },
    { MT_STEALTHVILE, S_VILE_STND, S_VILE_DIE10, -1,-1 },
    { MT_STEALTHBRUISER, S_BOSS_STND, S_BOSS_RAISE7, -1,-1 },
    { MT_STEALTHHEAD, S_HEAD_STND, S_HEAD_RAISE6, -1,-1 },
    { MT_STEALTHCHAINGUY, S_CPOS_STND, S_CPOS_RAISE7, -1,-1 },
    { MT_STEALTHSERGEANT, S_SARG_STND, S_SARG_RAISE6, -1,-1 },
    { MT_STEALTHKNIGHT, S_BOS2_STND, S_BOS2_RAISE7, -1,-1 },
    { MT_STEALTHIMP, S_TROO_STND, S_TROO_RAISE5, -1,-1 },
    { MT_STEALTHFATSO, S_FATT_STND, S_FATT_RAISE8, -1,-1 },
    { MT_STEALTHUNDEAD, S_SKEL_STND, S_SKEL_RAISE6, -1,-1 },
    { MT_STEALTHSHOTGUY, S_SPOS_STND, S_SPOS_RAISE5, -1,-1 },
    { MT_STEALTHZOMBIE, S_POSS_STND, S_POSS_RAISE4, -1,-1 },

    { -1, -1,-1, -1, -1 }  // End sentinel
};

const staterange_t weapon_range[] =
{
	// Weapons...
    { wp_fist, S_PUNCH, S_PUNCH5, -1,-1 },
    { wp_chainsaw, S_SAW, S_SAW3, -1,-1 },
    { wp_pistol, S_PISTOL, S_PISTOLFLASH, S_LIGHTDONE, S_LIGHTDONE },
    { wp_shotgun, S_SGUN, S_SGUNFLASH2, S_LIGHTDONE, S_LIGHTDONE },
    { wp_chaingun, S_CHAIN, S_CHAINFLASH2, S_LIGHTDONE, S_LIGHTDONE },
    { wp_missile, S_MISSILE, S_MISSILEFLASH4, S_LIGHTDONE, S_LIGHTDONE },
    { wp_plasma, S_PLASMA, S_PLASMAFLASH2, S_LIGHTDONE, S_LIGHTDONE },
    { wp_bfg, S_BFG, S_BFGFLASH2, S_LIGHTDONE, S_LIGHTDONE },
    { wp_supershotgun, S_DSGUN, S_DSGUNFLASH2, S_LIGHTDONE, S_LIGHTDONE },

    { -1, -1,-1, -1, -1 }  // End sentinel
};

namespace Frames
{
	void MarkState(int st_num)
	{
		// this is possible since binary patches store the dummy state
		if (st_num == S_NULL)
			return;

		assert(1 <= st_num && st_num < NUMSTATES_BEX);

		state_modified[st_num] = true;
	}

	void StateDependRange(int st_lo, int st_hi)
	{
		// Notes:
		//   While it's possible for weapons to use thing states, and vice
		//   versa, it can only happen when those weapons/things are
		//   modified, so they don't need to be marked here.

		assert(st_lo <= st_hi);
		assert(st_lo >= 0);
		assert(st_hi < NUMSTATES_BEX);

		if (st_lo == S_NULL)
			return;

		// does range crosses the weapon/thing boundary ?
		if (st_lo <= S_LAST_WEAPON_STATE && st_hi > S_LAST_WEAPON_STATE)
		{
			StateDependRange(st_lo, S_LAST_WEAPON_STATE);
			StateDependRange(S_LAST_WEAPON_STATE + 1, st_hi);
			return;
		}

		if (st_hi <= S_LAST_WEAPON_STATE)
		{
			for (int w = 0; weapon_range[w].obj_num >= 0; w++)
			{
				const staterange_t *R = weapon_range + w;

				if ((st_hi >= R->start1 && st_lo <= R->end1) ||
				    (st_hi >= R->start2 && st_lo <= R->end2))
				{
					Weapons::MarkWeapon(R->obj_num);
				}
			}
			return;
		}

		// check things.

		for (int t = 0; thing_range[t].obj_num >= 0; t++)
		{
			const staterange_t *R = thing_range + t;

			if ((st_hi >= R->start1 && st_lo <= R->end1) ||
				(st_hi >= R->start2 && st_lo <= R->end2))
			{
				Things::MarkThing(R->obj_num);
			}
		}
	}

	void StateDependencies(void)
	{
		for (int lo = 1; lo < NUMSTATES_BEX; )
		{
			if (! state_modified[lo])
			{
				lo++; continue;
			}

			int hi = lo;

			while (hi + 1 < NUMSTATES_BEX && state_modified[hi])
				hi++;

			StateDependRange(lo, hi);

			lo = hi + 1;
		}
	}
}


//------------------------------------------------------------------------

void Frames::Startup(void)
{
	memset(state_modified, 0, sizeof(state_modified));
	memset(state_dyn, 0, sizeof(state_dyn));
}

void Frames::ResetAll(void)
{
	for (int i = 0; i < NUMSTATES_BEX; i++)
	{
		state_dyn[i].group = 0;
		state_dyn[i].gr_idx = 0;
		state_dyn[i].gr_next = S_NULL;
	}

	attack_slot[0] = attack_slot[1] = attack_slot[2] = NULL;

	act_flags = 0;

	lowest_touched  = 99999;
	highest_touched = S_NULL;
}

int Frames::BeginGroup(int first, char group)
{
	if (first == S_NULL)
		return 0;

	state_dyn[first].group = group;
	state_dyn[first].gr_idx = 1;

	return 1;
}

void Frames::InstallRandomJump(int src, int first)
{
	assert(state_dyn[src].gr_idx > 0);
	assert(state_dyn[first].group == 0);

	char group = state_dyn[src].group;

	// step 1: find the last state in the current group
	for (int i = 1; i < NUMSTATES_BEX; i++)
	{
		if (state_dyn[i].group != group)
			continue;

		if (state_dyn[i].gr_idx > state_dyn[src].gr_idx)
			src = i;
	}

	assert(group != 0);
	assert(state_dyn[src].gr_next == S_NULL);

	// step 2: follow state chain as far as possible

	for (;;)
	{
		state_dyn[src].gr_next = first;

		state_dyn[first].group = group;
		state_dyn[first].gr_idx = state_dyn[src].gr_idx + 1;

		if (states[first].tics < 0)  // hibernation
			break;

		src   = first;
		first = states[first].nextstate;

		if (first == S_NULL)
			break;

		if (state_dyn[first].group != 0)
			break;
	}
}

void Frames::SpreadGroups(void)
{
	for (;;)
	{
		bool changes = false;

		for (int i = 1; i < NUMSTATES_BEX; i++)
		{
			if (state_dyn[i].group == 0)
				continue;

			if (states[i].tics < 0)  // hibernation
				continue;

			int next = states[i].nextstate;

			if (next == S_NULL)
				continue;

			if (state_dyn[next].group != 0)
				continue;

			state_dyn[i].gr_next = next;

			state_dyn[next].group  = state_dyn[i].group;
			state_dyn[next].gr_idx = state_dyn[i].gr_idx + 1;

			changes = true;
		}

		if (! changes)
			break;
	}

	// now sort out the A_RandomJump targets

	for (;;)
	{
		bool changes = false;

		for (int i = 1; i < NUMSTATES_BEX; i++)
		{
			if (state_dyn[i].group == 0)
				continue;

			if (! (states[i].action == A_RandomJump &&
				   states[i].misc1 > 0 && states[i].misc1 < NUMSTATES_BEX))
				continue;

			int first = states[i].misc1;

			if (state_dyn[first].group != 0)
				continue;

			InstallRandomJump(i, first);

			changes = true;
		}

		if (! changes)
			break;
	}
}

bool Frames::CheckSpawnRemove(int first)
{
	assert(first != S_NULL);

	for (;;)
	{
		if (state_dyn[first].group != 'S')
			break;

		if (states[first].tics < 0)  // hibernation
			break;

		if (states[first].nextstate == S_NULL)
			return true;

		// don't look in the random-jump states
		if (states[first].nextstate != state_dyn[first].gr_next)
			break;

		first = state_dyn[first].gr_next;

		if (first == S_NULL)
			break;
	}

	return false;
}

bool Frames::CheckWeaponFlash(int first)
{
	// fairly simple test, we don't need to detect looping or such here,
	// just following the states upto a small maximum is enough.

	for (int len = 0; len < 30; len++)
	{
		if (first == S_NULL)
			break;

		if (states[first].tics < 0)  // hibernation
			break;

		int act = states[first].action;

		assert(0 <= act && act < NUMACTIONS_BEX);

		if (action_info[act].act_flags & AF_FLASH)
			return true;

		first = states[first].nextstate;
	}

	return false;
}

namespace Frames
{
	void UpdateAttacks(char group, char *act_name, int action)
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
}

const char *Frames::GroupToName(char group, bool use_spawn)
{
	assert(group != 0);

	switch (group)
	{
		case 'S': return use_spawn ? "SPAWN" : "IDLE";
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

const char *Frames::RedirectorName(int next_st, bool use_spawn)
{
	static char name_buf[MAX_ACT_NAME];

	char next_gr = state_dyn[next_st].group;
	int next_idx = state_dyn[next_st].gr_idx;

	assert(next_gr != 0);
	assert(next_idx > 0);

	if (next_idx == 1)
		sprintf(name_buf, "%s", GroupToName(next_gr, use_spawn));
	else
		sprintf(name_buf, "%s:%d", GroupToName(next_gr, use_spawn), next_idx);

	return name_buf;
}

#define MISC_TO_ANGLE(m)  ((int)(m) / 11930465)

void Frames::SpecialAction(char *act_name, state_t *st, bool use_spawn)
{
	switch (st->action)
	{
		case A_Die:
			if (target_version >= 129)
				strcpy(act_name, "DIE");
			else
			{
				PrintWarn("Action A_DIE only supported in EDGE 1.29 and higher.\n");
				strcpy(act_name, "EXPLODE");
			}
			break;

		case A_KeenDie:
			if (target_version >= 129)
				strcpy(act_name, "KEEN_DIE");
			else
				strcpy(act_name, "NOTHING");
			break;

		case A_RandomJump:
			if (st->misc1 <= 0 || st->misc1 >= NUMSTATES_BEX)
				strcpy(act_name, "NOTHING");
			else
			{
				int perc = (st->misc2 <= 0) ? 0 : (st->misc2 >= 256) ? 100 :
						   (st->misc2 * 100 / 256);

				sprintf(act_name, "JUMP(%s,%d%%)",
					RedirectorName(st->misc1, use_spawn), perc);
			}
			break;

		case A_Turn:
		  	sprintf(act_name, "TURN(%d)", MISC_TO_ANGLE(st->misc1));
			break;

		  case A_Face:
		  	sprintf(act_name, "FACE(%d)", MISC_TO_ANGLE(st->misc1));
			break;

		  case A_PlaySound:
		  	if (st->misc1 <= 0 || st->misc1 >= NUMSFX_BEX)
				strcpy(act_name, "NOTHING");
			else
			{
				sprintf(act_name, "PLAYSOUND(%s)",
					Sounds::GetSound(st->misc1));
			}
			break;

		case A_LineEffect:
		  	if (st->misc1 <= 0)
				strcpy(act_name, "NOTHING");
			else
			{
				sprintf(act_name, "ACTIVATE_LINETYPE(%d,%d)",
					st->misc1, st->misc2);
			}
			break;

		case A_Spawn:
			if (target_version < 131)
				PrintWarn("Action A_SPAWN only supported in EDGE 1.31 and higher.\n");
			
			else if (st->misc1 < 1 || st->misc1 > NUMMOBJTYPES_BEX)
				PrintWarn("Action A_SPAWN: illegal type (%d)\n", st->misc1);

			else
			{
				const mobjinfo_t *type = &mobjinfo[st->misc1 - 1];

				if (! type->name || type->doomednum <= 0)
					PrintWarn("Action A_SPAWN unusable type (%d = %s)\n",
							  st->misc1, type->name ? type->name : "\"\"");
				else
				{
					sprintf(act_name, "SPAWN(%s)", type->name);
					return; // success !
				}
			}

			// fall-back
			strcpy(act_name, "NOTHING");
			break;

		default:
			InternalError("Bad special action %d\n", st->action);
	}
}

void Frames::OutputState(char group, int cur, bool use_spawn)
{
	assert(cur > 0);

	state_t *st = states + cur;

	assert(st->action >= 0 && st->action < NUMACTIONS_BEX);

	const char *bex_name = action_info[st->action].bex_name;

	if (cur <= S_LAST_WEAPON_STATE)
		act_flags |= AF_WEAPON_ST;
	else
		act_flags |= AF_THING_ST;

	if (cur != S_LIGHTDONE)
	{
		if (cur < lowest_touched)  lowest_touched  = cur;
		if (cur > highest_touched) highest_touched = cur;
	}

	if (action_info[st->action].act_flags & AF_UNIMPL)
		PrintWarn("Frame %d: action %s is not yet supported.\n", cur,
			bex_name);

	char act_name[MAX_ACT_NAME];

	bool weap_act = false;

	if (action_info[st->action].act_flags & AF_SPECIAL)
	{
		SpecialAction(act_name, st, use_spawn);
	}
	else
	{
		strcpy(act_name, action_info[st->action].ddf_name);

		weap_act = (act_name[0] == 'W' && act_name[1] == ':');

		if (weap_act)
			strcpy(act_name, action_info[st->action].ddf_name + 2);
	}

	if (st->action != S_NULL && weap_act == ! IS_WEAPON(group) &&
		StrCaseCmp(act_name, "NOTHING") != 0)
	{
		if (weap_act)
			PrintWarn("Frame %d: weapon action %s used in thing.\n", cur, bex_name);
		else
			PrintWarn("Frame %d: thing action %s used in weapon.\n", cur, bex_name);

		strcpy(act_name, "NOTHING");
	}

	if (st->action == S_NULL || weap_act == (IS_WEAPON(group) ? true : false))
	{
		UpdateAttacks(group, act_name, st->action);
	}

	// If the death states contain A_PainDie or A_KeenDie, then we
	// need to add an A_Fall action for proper operation in EDGE.
	if (action_info[st->action].act_flags & AF_MAKEDEAD)
	{
		WAD::Printf("    %s:%c:0:%s:MAKEDEAD,  // %s\n",
			TextStr::GetSprite(st->sprite),
			'A' + ((int) st->frame & 31),
			(st->frame >= 32768) ? "BRIGHT" : "NORMAL",
			(st->action == A_PainDie) ? "A_PainDie" : "A_KeenDie");
	}
	
	if (action_info[st->action].act_flags & AF_FACE)
	{
		WAD::Printf("    %s:%c:0:%s:FACE_TARGET,\n",
			TextStr::GetSprite(st->sprite),
			'A' + ((int) st->frame & 31),
			(st->frame >= 32768) ? "BRIGHT" : "NORMAL");
	}

	// special handling for Mancubus attacks...
	if (action_info[st->action].act_flags & AF_SPREAD)
	{
		if ((act_flags & AF_SPREAD) == 0)
		{
			WAD::Printf("    %s:%c:0:%s:RESET_SPREADER,\n",
				TextStr::GetSprite(st->sprite),
				'A' + ((int) st->frame & 31),
				(st->frame >= 32768) ? "BRIGHT" : "NORMAL");
		}

		WAD::Printf("    %s:%c:0:%s:%s,  // A_FatAttack\n",
			TextStr::GetSprite(st->sprite),
			'A' + ((int) st->frame & 31),
			(st->frame >= 32768) ? "BRIGHT" : "NORMAL", act_name);
	}

	int tics = (int) st->tics;

	// kludge for EDGE and Batman TC.  EDGE waits 35 tics before exiting the
	// level from A_BrainDie, but standard Doom does it immediately.  Oddly,
	// Batman TC goes into a loop calling A_BrainDie every tic.
	if (tics >= 0 && tics < 44 && StrCaseCmp(act_name, "BRAINDIE") == 0)
		tics = 44;

	WAD::Printf("    %s:%c:%d:%s:%s",
		TextStr::GetSprite(st->sprite),
		'A' + ((int) st->frame & 31), tics,
		(st->frame >= 32768) ? "BRIGHT" : "NORMAL", act_name);

	if (st->action != S_NULL && weap_act == ! IS_WEAPON(group))
		return;

	act_flags |= action_info[st->action].act_flags;
}

void Frames::OutputGroup(int first, char group)
{
	if (first == S_NULL)
		return;

	assert(state_dyn[first].group != 0);

	bool use_spawn = (group == 'S') && CheckSpawnRemove(first);

	WAD::Printf("\n");
	WAD::Printf("STATES(%s) =\n", GroupToName(group, use_spawn));

	int cur = first;

	for (;;)
	{
		OutputState(group, cur, use_spawn);

		bool is_last = (state_dyn[cur].gr_next == S_NULL);

		int next = states[cur].nextstate;

		if (states[cur].tics < 0)
		{
			// go into hibernation (nothing needed)
		}
		else if (next == S_NULL)
		{
			WAD::Printf(",#REMOVE");
		}
		else if (next != state_dyn[cur].gr_next)
		{
			// check if finished and DON'T need a redirector
			if (! (is_last && next == first && 
				   (group == 'S' || group == 'E' ||
				    group == 'u' || group == 'd' || group == 'r')))
			{
				WAD::Printf(",#%s", RedirectorName(next, use_spawn));
			}
		}

		if (is_last)
		{
#if (DEBUG_FRAMES)
			WAD::Printf("; // %d\n", cur);
#else
			WAD::Printf(";\n");
#endif
			return;
		}

#if (DEBUG_FRAMES)
		WAD::Printf(", // %d\n", cur);
#else
		WAD::Printf(",\n");
#endif

		cur = state_dyn[cur].gr_next;

		assert(cur != S_NULL);
	}
}


//------------------------------------------------------------------------

namespace Frames
{
	const fieldreference_t frame_field[] =
	{
		{ "Sprite number",    &states[0].sprite, FT_SPRITE },
		{ "Sprite subnumber", &states[0].frame,  FT_SUBSPR },
		{ "Duration",         &states[0].tics, FT_ANY },
		{ "Next frame",       &states[0].nextstate, FT_FRAME },
		{ "Unknown 1",        &states[0].misc1, FT_ANY },
		{ "Unknown 2",        &states[0].misc2, FT_ANY },

		{ NULL, NULL, 0 }   // End sentinel
	};
}

void Frames::AlterFrame(int new_val)
{
	int st_num = Patch::active_obj;
	const char *field_name = Patch::line_buf;

	assert(0 <= st_num && st_num < NUMSTATES_BEX);

	if (StrCaseCmp(field_name, "Action pointer") == 0)
	{
		PrintWarn("Line %d: raw Action pointer not supported.\n",
			Patch::line_num);
		return;
	}

	int stride = ((char*) (states+1)) - ((char*) states);

	if (! Things::AlterOneField(frame_field, field_name, st_num * stride, new_val))
	{
		PrintWarn("UNKNOWN FRAME FIELD: %s\n", field_name);
		return;
	}

	MarkState(st_num);
}

void Frames::AlterPointer(int new_val)
{
	int ptr_num = Patch::active_obj;
	assert(0 <= ptr_num && ptr_num < POINTER_NUM_BEX);

	int st_num = pointerToFrame[ptr_num];
	const char *deh_field = Patch::line_buf;

	assert(0 <= st_num && st_num < NUMSTATES_BEX);

	if (StrCaseCmp(deh_field, "Codep Frame") != 0)
	{
		PrintWarn("UNKNOWN POINTER FIELD: %s\n", deh_field);
		return;
	}

	if (new_val < 0 || new_val >= NUMSTATES_BEX)
	{
		PrintWarn("Line %d: Illegal Codep frame number: %d\n",
			Patch::line_num, new_val);
		return;
	}

	Storage::RememberMod(&states[st_num].action, states[new_val].action);

	MarkState(st_num);
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

	if (st_num < 0 || st_num >= NUMSTATES_BEX)
	{
		PrintWarn("Line %d: illegal FRAME number: %d\n",
			Patch::line_num, st_num);
		return;
	}

	// the S_NULL state is never output, no need to change it
	if (st_num == S_NULL)
		return;

	int action;

	for (action = 0; action < NUMACTIONS_BEX; action++)
	{
		// use +2 here to ignore the "A_" prefix
		if (StrCaseCmp(action_info[action].bex_name + 2, new_action) == 0)
			break;
	}

	if (action >= NUMACTIONS_BEX)
	{
		PrintWarn("Line %d: unknown action %s for CODEPTR.\n",
			Patch::line_num, new_action);
		return;
	}

	Storage::RememberMod(&states[st_num].action, action);

	MarkState(st_num);
}


//------- DEBUGGING ------------------------------------------------------

#if (DEBUG_RANGES)
extern const char *state_names[NUMSTATES_BEX];
#endif

void Frames::DebugRange(const char *kind, const char *entry)
{
#if (DEBUG_RANGES)
	fprintf(stderr, "%s [%s] : ", kind, entry);
	
	if (highest_touched == S_NULL)
	{
		fprintf(stderr, "empty\n");
	}
	else
	{
		assert(0 < lowest_touched);
		assert(lowest_touched <= highest_touched);
		assert(highest_touched < NUMSTATES_BEX);

		fprintf(stderr, "%s -> %s\n", state_names[lowest_touched],
			state_names[highest_touched]);
	}
#endif
}

}  // Deh_Edge
