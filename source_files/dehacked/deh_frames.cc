//------------------------------------------------------------------------
//  FRAME Handling
//------------------------------------------------------------------------
//
//  DEH_EDGE  Copyright (C) 2004-2024 The EDGE Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
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

#include "deh_frames.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

#include <unordered_map>

#include "deh_buffer.h"
#include "deh_edge.h"
#include "deh_field.h"
#include "deh_info.h"
#include "deh_patch.h"
#include "deh_sounds.h"
#include "deh_sprites.h"
#include "deh_system.h"
#include "deh_text.h"
#include "deh_things.h"
#include "deh_wad.h"
#include "deh_weapons.h"
#include "epi.h"
#include "str_compare.h"
namespace dehacked
{

constexpr uint16_t kMaximumActionNameLength = 1024;

extern State states_orig[kTotalMBFStates];

std::vector<State *> new_states;

// memory for states using misc1/misc2 or Args1..Args8
std::vector<int> argument_mem;

struct GroupInfo
{
    char             group;
    std::vector<int> states;
};

namespace frames
{
// stuff for determining and outputting groups of states:
std::unordered_map<char, GroupInfo> groups;
std::unordered_map<int, char>       group_for_state;
std::unordered_map<int, int>        offset_for_state;

const char *attack_slot[3];
int         act_flags;
bool        force_fullbright = false;

// forward decls
const State *NewStateElseOld(int st_num);
int          ReadArg(const State *st, int i);
void         WriteArg(State *st, int i, int value);
const char  *GroupToName(char group);
const char  *RedirectorName(int next_st);

void SpecialAction(char *buf, const State *st);
void OutputState(char group, int cur, bool do_action);
bool OutputSpawnState(int first);
bool SpreadGroupPass(bool alt_jumps);
void UpdateAttacks(char group, char *act_name, int action);
bool DependRangeWasModified(int low, int high);

inline bool IS_WEAPON(char group) { return islower(group); }

inline int MISC_TO_ANGLE(int m) { return m / 11930465; }
}  // namespace frames

struct ActionInfo
{
    const char *bex_name;

    int act_flags;

    // this is not used when kActionFlagSpecial is set
    const char *ddf_name;

    // attacks implied by the action, often nullptr.  The format is
    // "X:ATTACK_NAME" where X is 'R' for range attacks, 'C' for
    // close-combat attacks, and 'S' for spare attacks.
    const char *atk_1;
    const char *atk_2;
};

const ActionInfo action_info[kTotalMBF21Actions] = {
    { "A_nullptr", 0, "NOTHING", nullptr, nullptr },

    // weapon actions...
    { "A_Light0", 0, "W:LIGHT0", nullptr, nullptr },
    { "A_WeaponReady", 0, "W:READY", nullptr, nullptr },
    { "A_Lower", 0, "W:LOWER", nullptr, nullptr },
    { "A_Raise", 0, "W:RAISE", nullptr, nullptr },
    { "A_Punch", 0, "W:SHOOT", "C:PLAYER_PUNCH", nullptr },
    { "A_ReFire", 0, "W:REFIRE", nullptr, nullptr },
    { "A_FirePistol", kActionFlagFlash, "W:SHOOT", "R:PLAYER_PISTOL", nullptr },
    { "A_Light1", 0, "W:LIGHT1", nullptr, nullptr },
    { "A_FireShotgun", kActionFlagFlash, "W:SHOOT", "R:PLAYER_SHOTGUN",
      nullptr },
    { "A_Light2", 0, "W:LIGHT2", nullptr, nullptr },
    { "A_FireShotgun2", kActionFlagFlash, "W:SHOOT", "R:PLAYER_SHOTGUN2",
      nullptr },
    { "A_CheckReload", 0, "W:CHECKRELOAD", nullptr, nullptr },
    { "A_OpenShotgun2", 0, "W:PLAYSOUND(DBOPN)", nullptr, nullptr },
    { "A_LoadShotgun2", 0, "W:PLAYSOUND(DBLOAD)", nullptr, nullptr },
    { "A_CloseShotgun2", 0, "W:PLAYSOUND(DBCLS)", nullptr, nullptr },
    { "A_FireCGun", kActionFlagFlash, "W:SHOOT", "R:PLAYER_CHAINGUN", nullptr },
    { "A_GunFlash", kActionFlagFlash, "W:FLASH", nullptr, nullptr },
    { "A_FireMissile", 0, "W:SHOOT", "R:PLAYER_MISSILE", nullptr },
    { "A_Saw", 0, "W:SHOOT", "C:PLAYER_SAW", nullptr },
    { "A_FirePlasma", kActionFlagFlash, "W:SHOOT", "R:PLAYER_PLASMA", nullptr },
    { "A_BFGsound", 0, "W:PLAYSOUND(BFG)", nullptr, nullptr },
    { "A_FireBFG", 0, "W:SHOOT", "R:PLAYER_BFG9000", nullptr },

    // thing actions...
    { "A_BFGSpray", 0, "SPARE_ATTACK", nullptr, nullptr },
    { "A_Explode", kActionFlagExplode, "EXPLOSIONDAMAGE", nullptr, nullptr },
    { "A_Pain", 0, "MAKEPAINSOUND", nullptr, nullptr },
    { "A_PlayerScream", 0, "PLAYER_SCREAM", nullptr, nullptr },
    { "A_Fall", kActionFlagFall, "MAKEDEAD", nullptr, nullptr },
    { "A_XScream", 0, "MAKEOVERKILLSOUND", nullptr, nullptr },
    { "A_Look", kActionFlagLook, "LOOKOUT", nullptr, nullptr },
    { "A_Chase", kActionFlagChase, "CHASE", nullptr, nullptr },
    { "A_FaceTarget", 0, "FACETARGET", nullptr, nullptr },
    { "A_PosAttack", 0, "RANGE_ATTACK", "R:FORMER_HUMAN_PISTOL", nullptr },
    { "A_Scream", 0, "MAKEDEATHSOUND", nullptr, nullptr },
    { "A_SPosAttack", 0, "RANGE_ATTACK", "R:FORMER_HUMAN_SHOTGUN", nullptr },
    { "A_VileChase", kActionFlagChase | kActionFlagRaise, "RESCHASE", nullptr,
      nullptr },
    { "A_VileStart", 0, "PLAYSOUND(VILATK)", nullptr, nullptr },
    { "A_VileTarget", 0, "RANGE_ATTACK", "R:ARCHVILE_FIRE", nullptr },
    { "A_VileAttack", 0, "EFFECTTRACKER", nullptr, nullptr },
    { "A_StartFire", 0, "TRACKERSTART", nullptr, nullptr },
    { "A_Fire", 0, "TRACKERFOLLOW", nullptr, nullptr },
    { "A_FireCrackle", 0, "TRACKERACTIVE", nullptr, nullptr },
    { "A_Tracer", 0, "RANDOM_TRACER", nullptr, nullptr },
    { "A_SkelWhoosh", kActionFlagFaceTarget, "PLAYSOUND(SKESWG)", nullptr,
      nullptr },
    { "A_SkelFist", kActionFlagFaceTarget, "CLOSE_ATTACK",
      "C:REVENANT_CLOSECOMBAT", nullptr },
    { "A_SkelMissile", 0, "RANGE_ATTACK", "R:REVENANT_MISSILE", nullptr },
    { "A_FatRaise", kActionFlagFaceTarget, "PLAYSOUND(MANATK)", nullptr,
      nullptr },
    { "A_FatAttack1", kActionFlagSpread, "RANGE_ATTACK", "R:MANCUBUS_FIREBALL",
      nullptr },
    { "A_FatAttack2", kActionFlagSpread, "RANGE_ATTACK", "R:MANCUBUS_FIREBALL",
      nullptr },
    { "A_FatAttack3", kActionFlagSpread, "RANGE_ATTACK", "R:MANCUBUS_FIREBALL",
      nullptr },
    { "A_BossDeath", 0, "NOTHING", nullptr, nullptr },
    { "A_CPosAttack", 0, "RANGE_ATTACK", "R:FORMER_HUMAN_CHAINGUN", nullptr },
    { "A_CPosRefire", 0, "REFIRE_CHECK", nullptr, nullptr },
    { "A_TroopAttack", 0, "COMBOATTACK", "R:IMP_FIREBALL",
      "C:IMP_CLOSECOMBAT" },
    { "A_SargAttack", 0, "CLOSE_ATTACK", "C:DEMON_CLOSECOMBAT", nullptr },
    { "A_HeadAttack", 0, "COMBOATTACK", "R:CACO_FIREBALL",
      "C:CACO_CLOSECOMBAT" },
    { "A_BruisAttack", 0, "COMBOATTACK", "R:BARON_FIREBALL",
      "C:BARON_CLOSECOMBAT" },
    { "A_SkullAttack", 0, "RANGE_ATTACK", "R:SKULL_ASSAULT", nullptr },
    { "A_Metal", 0, "WALKSOUND_CHASE", nullptr, nullptr },
    { "A_SpidRefire", 0, "REFIRE_CHECK", nullptr, nullptr },
    { "A_BabyMetal", 0, "WALKSOUND_CHASE", nullptr, nullptr },
    { "A_BspiAttack", 0, "RANGE_ATTACK", "R:ARACHNOTRON_PLASMA", nullptr },
    { "A_Hoof", 0, "PLAYSOUND(HOOF)", nullptr, nullptr },
    { "A_CyberAttack", 0, "RANGE_ATTACK", "R:CYBERDEMON_MISSILE", nullptr },
    { "A_PainAttack", 0, "RANGE_ATTACK", "R:ELEMENTAL_SPAWNER", nullptr },
    { "A_PainDie", kActionFlagMakeDead, "SPARE_ATTACK",
      "S:ELEMENTAL_DEATHSPAWN", nullptr },
    { "A_KeenDie",
      kActionFlagSpecial | kActionFlagKeenDie | kActionFlagMakeDead, "",
      nullptr, nullptr },
    { "A_BrainPain", 0, "MAKEPAINSOUND", nullptr, nullptr },
    { "A_BrainScream", 0, "BRAINSCREAM", nullptr, nullptr },
    { "A_BrainDie", 0, "BRAINDIE", nullptr, nullptr },
    { "A_BrainAwake", 0, "NOTHING", nullptr, nullptr },
    { "A_BrainSpit", 0, "BRAINSPIT", "R:BRAIN_CUBE", nullptr },
    { "A_SpawnSound", 0, "MAKEACTIVESOUND", nullptr, nullptr },
    { "A_SpawnFly", 0, "CUBETRACER", nullptr, nullptr },
    { "A_BrainExplode", 0, "BRAINMISSILEEXPLODE", nullptr, nullptr },
    { "A_CubeSpawn", 0, "CUBESPAWN", nullptr, nullptr },

    // BOOM and MBF actions...
    { "A_Die", kActionFlagSpecial, "", nullptr, nullptr },
    { "A_Stop", 0, "STOP", nullptr, nullptr },
    { "A_Detonate", kActionFlagDetonate, "EXPLOSIONDAMAGE", nullptr, nullptr },
    { "A_Mushroom", 0, "MUSHROOM", nullptr, nullptr },

    { "A_Spawn", kActionFlagSpecial, "", nullptr, nullptr },
    { "A_Turn", kActionFlagSpecial, "", nullptr, nullptr },
    { "A_Face", kActionFlagSpecial, "", nullptr, nullptr },
    { "A_Scratch", kActionFlagSpecial, "", nullptr, nullptr },
    { "A_PlaySound", kActionFlagSpecial, "", nullptr, nullptr },
    { "A_RandomJump", kActionFlagSpecial, "", nullptr, nullptr },
    { "A_LineEffect", kActionFlagSpecial, "", nullptr, nullptr },

    { "A_FireOldBFG", 0, "W:SHOOT", "R:INTERNAL_FIRE_OLD_BFG", nullptr },
    { "A_BetaSkullAttack", 0, "RANGE_ATTACK",
      "R:INTERNAL_BETA_LOST_SOUL_ATTACK", nullptr },

    // MBF21 actions...
    { "A_RefireTo", kActionFlagSpecial, "", nullptr, nullptr }
};

//------------------------------------------------------------------------

struct StateRange
{
    int obj_num;  // thing or weapon
    int start1, end1;
    int start2, end2;
};

const StateRange thing_range[] = {
    // Things...
    { kMT_PLAYER, kS_PLAY, kS_PLAY_XDIE9, -1, -1 },
    { kMT_POSSESSED, kS_POSS_STND, kS_POSS_RAISE4, -1, -1 },
    { kMT_SHOTGUY, kS_SPOS_STND, kS_SPOS_RAISE5, -1, -1 },
    { kMT_VILE, kS_VILE_STND, kS_VILE_DIE10, -1, -1 },
    { kMT_UNDEAD, kS_SKEL_STND, kS_SKEL_RAISE6, -1, -1 },
    { kMT_SMOKE, kS_SMOKE1, kS_SMOKE5, -1, -1 },
    { kMT_FATSO, kS_FATT_STND, kS_FATT_RAISE8, -1, -1 },
    { kMT_CHAINGUY, kS_CPOS_STND, kS_CPOS_RAISE7, -1, -1 },
    { kMT_TROOP, kS_TROO_STND, kS_TROO_RAISE5, -1, -1 },
    { kMT_SERGEANT, kS_SARG_STND, kS_SARG_RAISE6, -1, -1 },
    { kMT_SHADOWS, kS_SARG_STND, kS_SARG_RAISE6, -1, -1 },
    { kMT_HEAD, kS_HEAD_STND, kS_HEAD_RAISE6, -1, -1 },
    { kMT_BRUISER, kS_BOSS_STND, kS_BOSS_RAISE7, -1, -1 },
    { kMT_KNIGHT, kS_BOS2_STND, kS_BOS2_RAISE7, -1, -1 },
    { kMT_SKULL, kS_SKULL_STND, kS_SKULL_DIE6, -1, -1 },
    { kMT_SPIDER, kS_SPID_STND, kS_SPID_DIE11, -1, -1 },
    { kMT_BABY, kS_BSPI_STND, kS_BSPI_RAISE7, -1, -1 },
    { kMT_CYBORG, kS_CYBER_STND, kS_CYBER_DIE10, -1, -1 },
    { kMT_PAIN, kS_PAIN_STND, kS_PAIN_RAISE6, -1, -1 },
    { kMT_WOLFSS, kS_SSWV_STND, kS_SSWV_RAISE5, -1, -1 },
    { kMT_KEEN, kS_KEENSTND, kS_KEENPAIN2, -1, -1 },
    { kMT_BOSSBRAIN, kS_BRAIN, kS_BRAIN_DIE4, -1, -1 },
    { kMT_BOSSSPIT, kS_BRAINEYE, kS_BRAINEYE1, -1, -1 },
    { kMT_BARREL, kS_BAR1, kS_BEXP5, -1, -1 },
    { kMT_PUFF, kS_PUFF1, kS_PUFF4, -1, -1 },
    { kMT_BLOOD, kS_BLOOD1, kS_BLOOD3, -1, -1 },
    { kMT_TFOG, kS_TFOG, kS_TFOG10, -1, -1 },
    { kMT_IFOG, kS_IFOG, kS_IFOG5, -1, -1 },
    { kMT_TELEPORTMAN, kS_TFOG, kS_TFOG10, -1, -1 },

    { kMT_MISC0, kS_ARM1, kS_ARM1A, -1, -1 },
    { kMT_MISC1, kS_ARM2, kS_ARM2A, -1, -1 },
    { kMT_MISC2, kS_BON1, kS_BON1E, -1, -1 },
    { kMT_MISC3, kS_BON2, kS_BON2E, -1, -1 },
    { kMT_MISC4, kS_BKEY, kS_BKEY2, -1, -1 },
    { kMT_MISC5, kS_RKEY, kS_RKEY2, -1, -1 },
    { kMT_MISC6, kS_YKEY, kS_YKEY2, -1, -1 },
    { kMT_MISC7, kS_YSKULL, kS_YSKULL2, -1, -1 },
    { kMT_MISC8, kS_RSKULL, kS_RSKULL2, -1, -1 },
    { kMT_MISC9, kS_BSKULL, kS_BSKULL2, -1, -1 },
    { kMT_MISC10, kS_STIM, kS_STIM, -1, -1 },
    { kMT_MISC11, kS_MEDI, kS_MEDI, -1, -1 },
    { kMT_MISC12, kS_SOUL, kS_SOUL6, -1, -1 },
    { kMT_INV, kS_PINV, kS_PINV4, -1, -1 },
    { kMT_MISC13, kS_PSTR, kS_PSTR, -1, -1 },
    { kMT_INS, kS_PINS, kS_PINS4, -1, -1 },
    { kMT_MISC14, kS_SUIT, kS_SUIT, -1, -1 },
    { kMT_MISC15, kS_PMAP, kS_PMAP6, -1, -1 },
    { kMT_MISC16, kS_PVIS, kS_PVIS2, -1, -1 },
    { kMT_MEGA, kS_MEGA, kS_MEGA4, -1, -1 },
    { kMT_CLIP, kS_CLIP, kS_CLIP, -1, -1 },
    { kMT_MISC17, kS_AMMO, kS_AMMO, -1, -1 },
    { kMT_MISC18, kS_ROCK, kS_ROCK, -1, -1 },
    { kMT_MISC19, kS_BROK, kS_BROK, -1, -1 },
    { kMT_MISC20, kS_CELL, kS_CELL, -1, -1 },
    { kMT_MISC21, kS_CELP, kS_CELP, -1, -1 },
    { kMT_MISC22, kS_SHEL, kS_SHEL, -1, -1 },
    { kMT_MISC23, kS_SBOX, kS_SBOX, -1, -1 },
    { kMT_MISC24, kS_BPAK, kS_BPAK, -1, -1 },
    { kMT_MISC25, kS_BFUG, kS_BFUG, -1, -1 },
    { kMT_CHAINGUN, kS_MGUN, kS_MGUN, -1, -1 },
    { kMT_MISC26, kS_CSAW, kS_CSAW, -1, -1 },
    { kMT_MISC27, kS_LAUN, kS_LAUN, -1, -1 },
    { kMT_MISC28, kS_PLAS, kS_PLAS, -1, -1 },
    { kMT_SHOTGUN, kS_SHOT, kS_SHOT, -1, -1 },
    { kMT_SUPERSHOTGUN, kS_SHOT2, kS_SHOT2, -1, -1 },

    { kMT_MISC29, kS_TECHLAMP, kS_TECHLAMP4, -1, -1 },
    { kMT_MISC30, kS_TECH2LAMP, kS_TECH2LAMP4, -1, -1 },
    { kMT_MISC31, kS_COLU, kS_COLU, -1, -1 },
    { kMT_MISC32, kS_TALLGRNCOL, kS_TALLGRNCOL, -1, -1 },
    { kMT_MISC33, kS_SHRTGRNCOL, kS_SHRTGRNCOL, -1, -1 },
    { kMT_MISC34, kS_TALLREDCOL, kS_TALLREDCOL, -1, -1 },
    { kMT_MISC35, kS_SHRTREDCOL, kS_SHRTREDCOL, -1, -1 },
    { kMT_MISC36, kS_SKULLCOL, kS_SKULLCOL, -1, -1 },
    { kMT_MISC37, kS_HEARTCOL, kS_HEARTCOL2, -1, -1 },
    { kMT_MISC38, kS_EVILEYE, kS_EVILEYE4, -1, -1 },
    { kMT_MISC39, kS_FLOATSKULL, kS_FLOATSKULL3, -1, -1 },
    { kMT_MISC40, kS_TORCHTREE, kS_TORCHTREE, -1, -1 },
    { kMT_MISC41, kS_BLUETORCH, kS_BLUETORCH4, -1, -1 },
    { kMT_MISC42, kS_GREENTORCH, kS_GREENTORCH4, -1, -1 },
    { kMT_MISC43, kS_REDTORCH, kS_REDTORCH4, -1, -1 },
    { kMT_MISC44, kS_BTORCHSHRT, kS_BTORCHSHRT4, -1, -1 },
    { kMT_MISC45, kS_GTORCHSHRT, kS_GTORCHSHRT4, -1, -1 },
    { kMT_MISC46, kS_RTORCHSHRT, kS_RTORCHSHRT4, -1, -1 },
    { kMT_MISC47, kS_STALAGTITE, kS_STALAGTITE, -1, -1 },
    { kMT_MISC48, kS_TECHPILLAR, kS_TECHPILLAR, -1, -1 },
    { kMT_MISC49, kS_CANDLESTIK, kS_CANDLESTIK, -1, -1 },
    { kMT_MISC50, kS_CANDELABRA, kS_CANDELABRA, -1, -1 },
    { kMT_MISC51, kS_BLOODYTWITCH, kS_BLOODYTWITCH4, -1, -1 },
    { kMT_MISC60, kS_BLOODYTWITCH, kS_BLOODYTWITCH4, -1, -1 },
    { kMT_MISC52, kS_MEAT2, kS_MEAT2, -1, -1 },
    { kMT_MISC53, kS_MEAT3, kS_MEAT3, -1, -1 },
    { kMT_MISC54, kS_MEAT4, kS_MEAT4, -1, -1 },
    { kMT_MISC55, kS_MEAT5, kS_MEAT5, -1, -1 },
    { kMT_MISC56, kS_MEAT2, kS_MEAT2, -1, -1 },
    { kMT_MISC57, kS_MEAT4, kS_MEAT4, -1, -1 },
    { kMT_MISC58, kS_MEAT3, kS_MEAT3, -1, -1 },
    { kMT_MISC59, kS_MEAT5, kS_MEAT5, -1, -1 },
    { kMT_MISC61, kS_HEAD_DIE6, kS_HEAD_DIE6, -1, -1 },
    { kMT_MISC62, kS_PLAY_DIE7, kS_PLAY_DIE7, -1, -1 },
    { kMT_MISC63, kS_POSS_DIE5, kS_POSS_DIE5, -1, -1 },
    { kMT_MISC64, kS_SARG_DIE6, kS_SARG_DIE6, -1, -1 },
    { kMT_MISC65, kS_SKULL_DIE6, kS_SKULL_DIE6, -1, -1 },
    { kMT_MISC66, kS_TROO_DIE5, kS_TROO_DIE5, -1, -1 },
    { kMT_MISC67, kS_SPOS_DIE5, kS_SPOS_DIE5, -1, -1 },
    { kMT_MISC68, kS_PLAY_XDIE9, kS_PLAY_XDIE9, -1, -1 },
    { kMT_MISC69, kS_PLAY_XDIE9, kS_PLAY_XDIE9, -1, -1 },
    { kMT_MISC70, kS_HEADSONSTICK, kS_HEADSONSTICK, -1, -1 },
    { kMT_MISC71, kS_GIBS, kS_GIBS, -1, -1 },
    { kMT_MISC72, kS_HEADONASTICK, kS_HEADONASTICK, -1, -1 },
    { kMT_MISC73, kS_HEADCANDLES, kS_HEADCANDLES2, -1, -1 },
    { kMT_MISC74, kS_DEADSTICK, kS_DEADSTICK, -1, -1 },
    { kMT_MISC75, kS_LIVESTICK, kS_LIVESTICK2, -1, -1 },
    { kMT_MISC76, kS_BIGTREE, kS_BIGTREE, -1, -1 },
    { kMT_MISC77, kS_BBAR1, kS_BBAR3, -1, -1 },
    { kMT_MISC78, kS_HANGNOGUTS, kS_HANGNOGUTS, -1, -1 },
    { kMT_MISC79, kS_HANGBNOBRAIN, kS_HANGBNOBRAIN, -1, -1 },
    { kMT_MISC80, kS_HANGTLOOKDN, kS_HANGTLOOKDN, -1, -1 },
    { kMT_MISC81, kS_HANGTSKULL, kS_HANGTSKULL, -1, -1 },
    { kMT_MISC82, kS_HANGTLOOKUP, kS_HANGTLOOKUP, -1, -1 },
    { kMT_MISC83, kS_HANGTNOBRAIN, kS_HANGTNOBRAIN, -1, -1 },
    { kMT_MISC84, kS_COLONGIBS, kS_COLONGIBS, -1, -1 },
    { kMT_MISC85, kS_SMALLPOOL, kS_SMALLPOOL, -1, -1 },
    { kMT_MISC86, kS_BRAINSTEM, kS_BRAINSTEM, -1, -1 },

    /* BRAIN_DEATH_MISSILE : kS_BRAINEXPLODE1, kS_BRAINEXPLODE3 */

    // Attacks...
    { kMT_FIRE, kS_FIRE1, kS_FIRE30, -1, -1 },
    { kMT_TRACER, kS_TRACER, kS_TRACEEXP3, -1, -1 },
    { kMT_FATSHOT, kS_FATSHOT1, kS_FATSHOTX3, -1, -1 },
    { kMT_BRUISERSHOT, kS_BRBALL1, kS_BRBALLX3, -1, -1 },
    { kMT_SPAWNSHOT, kS_SPAWN1, kS_SPAWNFIRE8, -1, -1 },
    { kMT_TROOPSHOT, kS_TBALL1, kS_TBALLX3, -1, -1 },
    { kMT_HEADSHOT, kS_RBALL1, kS_RBALLX3, -1, -1 },
    { kMT_ARACHPLAZ, kS_ARACH_PLAZ, kS_ARACH_PLEX5, -1, -1 },
    { kMT_ROCKET, kS_ROCKET, kS_ROCKET, kS_EXPLODE1, kS_EXPLODE3 },
    { kMT_PLASMA, kS_PLASBALL, kS_PLASEXP5, -1, -1 },
    { kMT_BFG, kS_BFGSHOT, kS_BFGLAND6, -1, -1 },
    { kMT_EXTRABFG, kS_BFGEXP, kS_BFGEXP4, -1, -1 },

    // Boom/MBF stuff...
    { kMT_DOGS, kS_DOGS_STND, kS_DOGS_RAISE6, -1, -1 },

    { kMT_PLASMA1, kS_PLS1BALL, kS_PLS1EXP5, -1, -1 },
    { kMT_PLASMA2, kS_PLS2BALL, kS_PLS2BALLX3, -1, -1 },
    { kMT_SCEPTRE, kS_BON3, kS_BON3, -1, -1 },
    { kMT_BIBLE, kS_BON4, kS_BON4, -1, -1 },

    { -1, 0, 0, 0, 0 }  // End sentinel
};

const StateRange weapon_range[] = {
    { kwp_fist, kS_PUNCH, kS_PUNCH5, -1, -1 },
    { kwp_chainsaw, kS_SAW, kS_SAW3, -1, -1 },
    { kwp_pistol, kS_PISTOL, kS_PISTOLFLASH, kS_LIGHTDONE, kS_LIGHTDONE },
    { kwp_shotgun, kS_SGUN, kS_SGUNFLASH2, kS_LIGHTDONE, kS_LIGHTDONE },
    { kwp_chaingun, kS_CHAIN, kS_CHAINFLASH2, kS_LIGHTDONE, kS_LIGHTDONE },
    { kwp_missile, kS_MISSILE, kS_MISSILEFLASH4, kS_LIGHTDONE, kS_LIGHTDONE },
    { kwp_plasma, kS_PLASMA, kS_PLASMAFLASH2, kS_LIGHTDONE, kS_LIGHTDONE },
    { kwp_bfg, kS_BFG, kS_BFGFLASH2, kS_LIGHTDONE, kS_LIGHTDONE },
    { kwp_supershotgun, kS_DSGUN, kS_DSGUNFLASH2, kS_LIGHTDONE, kS_LIGHTDONE },

    { -1, 0, 0, 0, 0 }  // End sentinel
};

//------------------------------------------------------------------------

void frames::Init()
{
    new_states.clear();
    argument_mem.clear();
}

void frames::Shutdown()
{
    for (size_t i = 0; i < new_states.size(); i++)
        if (new_states[i] != nullptr) delete new_states[i];

    new_states.clear();
    argument_mem.clear();
}

void frames::MarkState(int st_num)
{
    // this is possible since binary patches store the dummy state
    if (st_num == kS_NULL) return;

    // fill any missing slots with nullptrs, including the one we want
    while ((int)new_states.size() < st_num + 1)
    {
        new_states.push_back(nullptr);
    }

    // already have a modified entry?
    if (new_states[st_num] != nullptr) return;

    State *entry       = new State;
    new_states[st_num] = entry;

    // copy the original info, if we have one
    if (st_num < kTotalMBFStates)
    {
        memcpy(entry, &states_orig[st_num], sizeof(State));
    }
    else
    {
        // these defaults follow the DSDehacked specs
        entry->sprite      = kSPR_TNT1;
        entry->frame       = 0;
        entry->tics        = -1;
        entry->action      = kA_NULL;
        entry->next_state  = st_num;
        entry->arg_pointer = 0;
    }
}

const State *frames::NewStateElseOld(int st_num)
{
    if (st_num < 0) return nullptr;

    if (st_num < (int)new_states.size())
    {
        if (new_states[st_num] != nullptr) return new_states[st_num];
    }
    else if (patch::doom_ver ==
             21)  // DSDehacked stuff has to exist I guess - Dasho
    {
        size_t to_add = st_num + 1 - new_states.size();
        for (size_t i = 0; i < to_add; i++)
        {
            State *entry = new State;
            // these defaults follow the DSDehacked specs
            entry->sprite      = kSPR_TNT1;
            entry->frame       = 0;
            entry->tics        = -1;
            entry->action      = kA_NULL;
            entry->next_state  = st_num;
            entry->arg_pointer = 0;
            new_states.push_back(entry);
        }
        return new_states[st_num];
    }

    if (st_num < kTotalMBFStates) return &states_orig[st_num];

    return nullptr;
}

State *frames::GetModifiedState(int st_num)
{
    // this is possible since binary patches store the dummy state
    if (st_num == kS_NULL)
    {
        static State dummy;
        return &dummy;
    }

    MarkState(st_num);

    return new_states[st_num];
}

int frames::GetStateSprite(int st_num)
{
    const State *st = NewStateElseOld(st_num);

    if (st == nullptr) return -1;

    return st->sprite;
}

bool frames::CheckMissileState(int st_num)
{
    if (st_num == kS_NULL) return false;

    const State *mis_st = NewStateElseOld(st_num);

    if (mis_st == nullptr) return false;

    return (mis_st->tics >= 0 && mis_st->next_state != kS_NULL);
}

bool frames::DependRangeWasModified(int low, int high)
{
    if (high < 0) return false;

    EPI_ASSERT(low <= high);
    EPI_ASSERT(low > kS_NULL);

    if (high >= (int)new_states.size()) high = (int)new_states.size() - 1;

    for (int i = low; i <= high; i++)
        if (new_states[i] != nullptr) return true;

    return false;
}

void frames::StateDependencies()
{
    // the goal here is to mark *existing* things and weapons whose
    // states have been modified, so that we generate the DDF for
    // the thing/weapon which has the new states.  modified or new
    // things/weapons don't need this (already been marked).

    for (int w = 0; weapon_range[w].obj_num >= 0; w++)
    {
        const StateRange &R = weapon_range[w];

        if (DependRangeWasModified(R.start1, R.end1) ||
            DependRangeWasModified(R.start2, R.end2))
        {
            weapons::MarkWeapon(R.obj_num);
        }
    }

    // check things....

    for (int t = 0; thing_range[t].obj_num >= 0; t++)
    {
        const StateRange &R = thing_range[t];

        if (DependRangeWasModified(R.start1, R.end1) ||
            DependRangeWasModified(R.start2, R.end2))
        {
            things::MarkThing(R.obj_num);
        }
    }
}

void frames::MarkStatesWithSprite(int spr_num)
{
    // only need to handle old states here
    for (int st = 1; st < kTotalMBFStates; st++)
        if (states_orig[st].sprite == spr_num) MarkState(st);
}

int frames::ReadArg(const State *st, int i)
{
    // the given state can be old or new here.

    if (st->arg_pointer == 0) return 0;

    int ofs = (st->arg_pointer - 1) * 8;
    return argument_mem[ofs + i];
}

void frames::WriteArg(State *st, int i, int value)
{
    // the given state MUST be a new one (in new_states).
    // allocates a group of eight ints, unless done before.

    if (st->arg_pointer == 0)
    {
        for (int k = 0; k < 8; k++) argument_mem.push_back(0);

        st->arg_pointer = (int)argument_mem.size() / 8;
    }

    int ofs               = (st->arg_pointer - 1) * 8;
    argument_mem[ofs + i] = value;
}

//------------------------------------------------------------------------

void frames::ResetGroups()
{
    groups.clear();
    group_for_state.clear();
    offset_for_state.clear();

    attack_slot[0] = nullptr;
    attack_slot[1] = nullptr;
    attack_slot[2] = nullptr;

    act_flags = 0;
}

int frames::BeginGroup(char group, int first)
{
    if (first == kS_NULL) return 0;

    // create the group info
    groups[group] = GroupInfo{ group, { first } };

    group_for_state[first]  = group;
    offset_for_state[first] = 1;

    return 1;
}

bool frames::SpreadGroupPass(bool alt_jumps)
{
    bool changes = false;

    int total = std::max((int)kTotalMBFStates, (int)new_states.size());

    for (int i = 1; i < total; i++)
    {
        const State *st = NewStateElseOld(i);
        if (st == nullptr) continue;

        if (group_for_state.find(i) == group_for_state.end()) continue;

        char       group = group_for_state[i];
        GroupInfo &G     = groups[group];

        // check if this is the very first state of death or overkill sequence.
        // in vanilla Doom (and Boom/MBF/etc), a tics of -1 will be IGNORED when
        // *entering* such a state due to this code in KillMapObject:
        //    ``` if (target->tics < 1)
        //            target->tics = 1;
        //    ```
        // and that means it *will* enter the next state.

        bool first_death =
            ((group == 'D' || group == 'X') && G.states.size() == 1);

        // hibernation?
        // if action is kA_RandomJump or similar, still need to follow it!
        if (st->tics < 0 && !first_death && !alt_jumps) continue;

        int next = st->next_state;

        if (alt_jumps)
        {
            next = kS_NULL;
            if (st->action == kA_RandomJump) next = ReadArg(st, 0);  // misc1
        }

        if (next == kS_NULL) continue;

        // require next state to have no group yet
        if (group_for_state.find(next) != group_for_state.end()) continue;

        G.states.push_back(next);

        group_for_state[next]  = group;
        offset_for_state[next] = (int)G.states.size();

        changes = true;
    }

    return changes;
}

void frames::SpreadGroups()
{
    for (;;)
    {
        bool changes1 = SpreadGroupPass(false);
        bool changes2 = SpreadGroupPass(true);

        if (!(changes1 || changes2)) break;
    }
}

bool frames::CheckWeaponFlash(int first)
{
    // fairly simple test, we don't need to detect looping or such here,
    // just following the states upto a small maximum is enough.

    for (int len = 0; len < 30; len++)
    {
        if (first == kS_NULL) break;

        const State *st = NewStateElseOld(first);
        if (st == nullptr) break;

        if (st->tics < 0)  // hibernation
            break;

        int act = st->action;

        EPI_ASSERT(0 <= act && act < kTotalMBF21Actions);

        if (action_info[act].act_flags & kActionFlagFlash) return true;

        first = st->next_state;
    }

    return false;
}

void frames::UpdateAttacks(char group, char *act_name, int action)
{
    const char *atk1 = action_info[action].atk_1;
    const char *atk2 = action_info[action].atk_2;

    bool free1 = true;
    bool free2 = true;

    int kind1 = -1;
    int kind2 = -1;

    if (!atk1) { return; }
    else if (IS_WEAPON(group))
    {
        EPI_ASSERT(strlen(atk1) >= 3);
        EPI_ASSERT(atk1[1] == ':');
        EPI_ASSERT(!atk2);

        kind1 = kAttackMethodRanged;
    }
    else
    {
        EPI_ASSERT(strlen(atk1) >= 3);
        EPI_ASSERT(atk1[1] == ':');

        kind1 = (atk1[0] == 'R')   ? kAttackMethodRanged
                : (atk1[0] == 'C') ? kAttackMethodCombat
                                   : kAttackMethodSpare;
    }

    atk1 += 2;

    free1 = (!attack_slot[kind1] ||
             epi::StringCaseCompareASCII(attack_slot[kind1], atk1) == 0);

    if (atk2)
    {
        EPI_ASSERT(strlen(atk2) >= 3);
        EPI_ASSERT(atk2[1] == ':');

        kind2 = (atk2[0] == 'R')   ? kAttackMethodRanged
                : (atk2[0] == 'C') ? kAttackMethodCombat
                                   : kAttackMethodSpare;

        atk2 += 2;

        free2 = (!attack_slot[kind2] ||
                 epi::StringCaseCompareASCII(attack_slot[kind2], atk2) == 0);
    }

    if (free1 && free2)
    {
        attack_slot[kind1] = atk1;

        if (atk2) attack_slot[kind2] = atk2;

        return;
    }

    wad::Printf("    // Specialising %s\n", act_name);

    // do some magic to put the attack name into parenthesis,
    // for example RANGE_ATTACK(IMP_FIREBALL).

    if (epi::StringCaseCompareASCII(act_name, "BRAINSPIT") == 0)
    {
        LogDebug(
            "Dehacked: Warning - Multiple range attacks used with "
            "kA_BrainSpit.\n");
        return;
    }

    // in this case, we have two attacks (must be a COMBOATTACK), but
    // we don't have the required slots (need both).  Therefore select
    // one of them based on the group.
    if (atk1 && atk2)
    {
        if (group != 'L' && group != 'M')
        {
            LogDebug(
                "Dehacked: Warning - Not enough attack slots for "
                "COMBOATTACK.\n");
        }

        if ((group == 'L' && kind2 == kAttackMethodCombat) ||
            (group == 'M' && kind2 == kAttackMethodRanged))
        {
            atk1  = atk2;
            kind1 = kind2;
        }

        switch (kind1)
        {
            case kAttackMethodRanged:
                strcpy(act_name, "RANGE_ATTACK");
                break;
            case kAttackMethodCombat:
                strcpy(act_name, "CLOSE_ATTACK");
                break;
            case kAttackMethodSpare:
                strcpy(act_name, "SPARE_ATTACK");
                break;

            default:
                FatalError("Dehacked: Error - Bad attack kind %d\n", kind1);
        }
    }

    strcat(act_name, "(");
    strcat(act_name, atk1);
    strcat(act_name, ")");
}

const char *frames::GroupToName(char group)
{
    EPI_ASSERT(group != 0);

    switch (group)
    {
        case 'S':
            return "IDLE";
        case 'E':
            return "CHASE";
        case 'L':
            return "MELEE";
        case 'M':
            return "MISSILE";
        case 'P':
            return "PAIN";
        case 'D':
            return "DEATH";
        case 'X':
            return "OVERKILL";
        case 'R':
            return "RESPAWN";
        case 'H':
            return "RESURRECT";

        // weapons
        case 'u':
            return "UP";
        case 'd':
            return "DOWN";
        case 'r':
            return "READY";
        case 'a':
            return "ATTACK";
        case 'f':
            return "FLASH";

        default:
            FatalError("Dehacked: Error - GroupToName: BAD GROUP '%c'\n",
                       group);
    }

    return nullptr;
}

const char *frames::RedirectorName(int next_st)
{
    static char name_buf[kMaximumActionNameLength];

    // this shouldn't happen since OutputGroup() only visits states
    // which we collected/processed as a group.
    if (group_for_state.find(next_st) == group_for_state.end())
    {
        LogDebug("Dehacked: Warning - Redirection to state %d FAILED\n",
                 next_st);
        return "IDLE";
    }

    char next_group = group_for_state[next_st];
    int  next_ofs   = offset_for_state[next_st];

    EPI_ASSERT(next_group != 0);
    EPI_ASSERT(next_ofs > 0);

    if (next_ofs == 1)
        snprintf(name_buf, sizeof(name_buf), "%s", GroupToName(next_group));
    else
        snprintf(name_buf, sizeof(name_buf), "%s:%d", GroupToName(next_group),
                 next_ofs);

    return name_buf;
}

void frames::SpecialAction(char *act_name, const State *st)
{
    switch (st->action)
    {
        case kA_Die:
            strcpy(act_name, "DIE");
            break;

        case kA_KeenDie:
            strcpy(act_name, "KEEN_DIE");
            break;

        case kA_RandomJump:
        {
            int next = ReadArg(st, 0);  // misc1
            int perc = ReadArg(st, 1);  // misc2

            if (next <= 0 || NewStateElseOld(next) == nullptr)
            {
                strcpy(act_name, "NOTHING");
            }
            else
            {
                perc = perc * 100 / 256;
                if (perc < 0) perc = 0;
                if (perc > 100) perc = 100;

                sprintf(act_name, "JUMP(%s,%d%%)", RedirectorName(next), perc);
            }
        }
        break;

        case kA_Turn:
            sprintf(act_name, "TURN(%d)", MISC_TO_ANGLE(ReadArg(st, 0)));
            break;

        case kA_Face:
            sprintf(act_name, "FACE(%d)", MISC_TO_ANGLE(ReadArg(st, 0)));
            break;

        case kA_PlaySound:
        {
            const char *sfx = sounds::GetSound(ReadArg(st, 0));

            if (epi::StringCaseCompareASCII(sfx, "NULL") == 0)
                strcpy(act_name, "NOTHING");
            else
                sprintf(act_name, "PLAYSOUND(\"%s\")", sfx);
        }
        break;

        case kA_Scratch:
        {
            int damage = ReadArg(st, 0);  // misc1
            int sfx_id = ReadArg(st, 1);  // misc2

            if (damage == 0 && sfx_id == 0) { sprintf(act_name, "NOTHING"); }
            else
            {
                const char *sfx = nullptr;
                if (sfx_id > 0) sfx = sounds::GetSound(sfx_id);
                if (sfx != nullptr &&
                    epi::StringCaseCompareASCII(sfx, "NULL") == 0)
                    sfx = nullptr;

                const char *atk_name = things::AddScratchAttack(damage, sfx);
                sprintf(act_name, "CLOSE_ATTACK(%s)", atk_name);
            }
        }
        break;

        case kA_LineEffect:
        {
            int misc1 = ReadArg(st, 0);
            int misc2 = ReadArg(st, 1);

            if (misc1 <= 0)
                strcpy(act_name, "NOTHING");
            else
                sprintf(act_name, "ACTIVATE_LINETYPE(%d,%d)", misc1, misc2);
        }
        break;

        case kA_Spawn:
        {
            int kMT_num = ReadArg(st, 0);

            if (!things::IsSpawnable(kMT_num))
            {
                LogDebug(
                    "Dehacked: Warning - Action kA_SPAWN unusable type (%d)\n",
                    kMT_num);
                strcpy(act_name, "NOTHING");
            }
            else
            {
                things::UseThing(kMT_num);
                sprintf(act_name, "SPAWN(%s)", things::GetMobjName(kMT_num));
            }
        }
        break;

        case kA_RefireTo:
        {
            int next = ReadArg(st, 0);  // state
            int perc = ReadArg(st, 1);  // noammocheck

            if (next <= 0 || NewStateElseOld(next) == nullptr)
            {
                strcpy(act_name, "NOTHING");
            }
            else
            {
                perc = perc * 100 / 256;
                if (perc != 0)
                    perc = -1;  // We use the negative percentage in kA_RefireTo
                                // to denote skipping the ammo check (or will)

                sprintf(act_name, "REFIRE_TO(%s,%d%%)", RedirectorName(next),
                        perc);
            }
        }
        break;

        default:
            FatalError("Dehacked: Error - Bad special action %d\n", st->action);
    }
}

void frames::OutputState(char group, int cur, bool do_action)
{
    EPI_ASSERT(cur > 0);

    const State *st = NewStateElseOld(cur);
    if (st == nullptr) st = &states_orig[kS_TNT1];

    int action = do_action ? st->action : kA_NULL;

    EPI_ASSERT(action >= 0 && action < kTotalMBF21Actions);

    const char *bex_name = action_info[action].bex_name;

    if (cur <= kLastWeaponState)
        act_flags |= kActionFlagWeaponState;
    else
        act_flags |= kActionFlagThingState;

    if (action_info[action].act_flags & kActionFlagUnimplemented)
        LogDebug(
            "Dehacked: Warning - Frame %d: action %s is not yet supported.\n",
            cur, bex_name);

    char act_name[kMaximumActionNameLength];

    bool weap_act = false;

    if (action_info[action].act_flags & kActionFlagSpecial)
    {
        SpecialAction(act_name, st);
    }
    else
    {
        strcpy(act_name, action_info[action].ddf_name);

        weap_act = (act_name[0] == 'W' && act_name[1] == ':');

        if (weap_act) strcpy(act_name, action_info[action].ddf_name + 2);
    }

    if (action != kA_NULL && (weap_act == !IS_WEAPON(group)) &&
        epi::StringCaseCompareASCII(act_name, "NOTHING") != 0)
    {
        if (weap_act)
            LogDebug(
                "Dehacked: Warning - Frame %d: weapon action %s used in "
                "thing.\n",
                cur, bex_name);
        else
            LogDebug(
                "Dehacked: Warning - Frame %d: thing action %s used in "
                "weapon.\n",
                cur, bex_name);

        strcpy(act_name, "NOTHING");
    }

    if (action == kA_NULL || weap_act == (IS_WEAPON(group) ? true : false))
    {
        UpdateAttacks(group, act_name, action);
    }

    // If the death states contain kA_PainDie or kA_KeenDie, then we
    // need to add an kA_Fall action for proper operation in EDGE.
    if (action_info[action].act_flags & kActionFlagMakeDead)
    {
        wad::Printf(
            "    %s:%c:0:%s:MAKEDEAD,  // %s\n", sprites::GetSprite(st->sprite),
            'A' + ((int)st->frame & 31),
            (st->frame >= 32768 || force_fullbright) ? "BRIGHT" : "NORMAL",
            (action == kA_PainDie) ? "A_PainDie" : "A_KeenDie");
    }

    if (action_info[action].act_flags & kActionFlagFaceTarget)
    {
        wad::Printf(
            "    %s:%c:0:%s:FACE_TARGET,\n", sprites::GetSprite(st->sprite),
            'A' + ((int)st->frame & 31),
            (st->frame >= 32768 || force_fullbright) ? "BRIGHT" : "NORMAL");
    }

    // special handling for Mancubus attacks...
    if (action_info[action].act_flags & kActionFlagSpread)
    {
        if ((act_flags & kActionFlagSpread) == 0)
        {
            wad::Printf(
                "    %s:%c:0:%s:RESET_SPREADER,\n",
                sprites::GetSprite(st->sprite), 'A' + ((int)st->frame & 31),
                (st->frame >= 32768 || force_fullbright) ? "BRIGHT" : "NORMAL");
        }

        wad::Printf(
            "    %s:%c:0:%s:%s,  // kA_FatAttack\n",
            sprites::GetSprite(st->sprite), 'A' + ((int)st->frame & 31),
            (st->frame >= 32768 || force_fullbright) ? "BRIGHT" : "NORMAL",
            act_name);
    }

    // special handling for kA_CloseShotgun2
    // 2023.11.13: This is not stricly accurate; the real kA_CloseShotgun2 will
    // play the sound before refiring, but with our current sound channel
    // handling this causes the DBCLS sound to play repeatedly and persist even
    // with the refire noises (ex: Harmony re-release chaingun will constantly
    // play its wind-down noise)
    if (epi::StringCaseCompareASCII(action_info[action].bex_name,
                                    "A_CloseShotgun2") == 0)
    {
        wad::Printf(
            "    %s:%c:0:%s:REFIRE,\n", sprites::GetSprite(st->sprite),
            'A' + ((int)st->frame & 31),
            (st->frame >= 32768 || force_fullbright) ? "BRIGHT" : "NORMAL");
    }

    int tics = (int)st->tics;

    // kludge for EDGE and Batman TC.  EDGE waits 35 tics before exiting the
    // level from kA_BrainDie, but standard Doom does it immediately.  Oddly,
    // Batman TC goes into a loop calling kA_BrainDie every tic.
    if (tics >= 0 && tics < 44 &&
        epi::StringCaseCompareASCII(act_name, "BRAINDIE") == 0)
        tics = 44;

    wad::Printf("    %s:%c:%d:%s:%s", sprites::GetSprite(st->sprite),
                'A' + ((int)st->frame & 31), tics,
                (st->frame >= 32768 || force_fullbright) ? "BRIGHT" : "NORMAL",
                act_name);

    if (action != kA_NULL && weap_act == !IS_WEAPON(group)) return;

    act_flags |= action_info[action].act_flags;
}

bool frames::OutputSpawnState(int first)
{
    // returns true if no IDLE states will be needed

    wad::Printf("\n");
    wad::Printf("STATES(SPAWN) =\n");

    const State *st = NewStateElseOld(first);
    if (st == nullptr) st = &states_orig[kS_TNT1];

    OutputState('S', first, false);

    int next = st->next_state;

    if (st->tics < 0)
    {
        // goes into hibernation
        wad::Printf(";\n");
        return true;
    }
    else if (next == kS_NULL)
    {
        wad::Printf(",#REMOVE;\n");
        return true;
    }
    else
    {
        wad::Printf(",#%s;\n", RedirectorName(next));
        return false;
    }
}

void frames::OutputGroup(char group)
{
    auto GIT = groups.find(group);

    if (GIT == groups.end()) return;

    GroupInfo &G = GIT->second;

    // generate STATES(SPAWN) here, before doing the IDLE ones.
    // this is to emulate BOOM/MBF, which don't execute the very first
    // action when an object is spawned, but EDGE *does* execute it.
    if (group == 'S')
    {
        if (OutputSpawnState(G.states[0])) return;
    }

    wad::Printf("\n");
    wad::Printf("STATES(%s) =\n", GroupToName(group));

    for (size_t i = 0; i < G.states.size(); i++)
    {
        int  cur     = G.states[i];
        bool is_last = (i == G.states.size() - 1);

        OutputState(group, cur, true);

        const State *st = NewStateElseOld(cur);
        EPI_ASSERT(st);

        int next = st->next_state;

        if (st->tics < 0)
        {
            // go into hibernation (nothing needed)
        }
        else if (next == kS_NULL) { wad::Printf(",#REMOVE"); }
        else if (is_last || next != G.states[i + 1])
        {
            wad::Printf(",#%s", RedirectorName(next));
        }

        if (is_last)
        {
            wad::Printf(";\n");
            return;
        }

        wad::Printf(",\n");
    }
}

//------------------------------------------------------------------------

namespace frames
{
const FieldReference frame_field[] = {
    { "Sprite number", offsetof(State, sprite), kFieldTypeSpriteNumber },
    { "Sprite subnumber", offsetof(State, frame), kFieldTypeSubspriteNumber },
    { "Duration", offsetof(State, tics), kFieldTypeAny },
    { "Next frame", offsetof(State, next_state), kFieldTypeFrameNumber },

    { nullptr, 0, kFieldTypeAny }  // End sentinel
};
}  // namespace frames

void frames::AlterFrame(int new_val)
{
    int         st_num     = patch::active_obj;
    const char *field_name = patch::line_buf;

    EPI_ASSERT(st_num >= 0);

    // the kS_NULL state is never output, no need to change it
    if (st_num == kS_NULL) return;

    MarkState(st_num);

    State *st = new_states[st_num];
    EPI_ASSERT(st);

    if (epi::StringCaseCompareASCII(field_name, "Action pointer") == 0)
    {
        LogDebug(
            "Dehacked: Warning - Line %d: raw Action pointer not supported.\n",
            patch::line_num);
        return;
    }

    if (epi::StringCaseCompareASCII(field_name, "Unknown 1") == 0)
    {
        WriteArg(st, 0, new_val);
        return;
    }

    if (epi::StringCaseCompareASCII(field_name, "Unknown 2") == 0)
    {
        WriteArg(st, 1, new_val);
        return;
    }

    if (epi::StringPrefixCaseCompareASCII(field_name, "Args") == 0)
    {
        int arg = atoi(field_name + 4);
        if (arg >= 1 && arg <= 8)
        {
            WriteArg(st, arg - 1, new_val);
            return;
        }
    }

    if (!FieldAlter(frame_field, field_name, (int *)st, new_val))
    {
        LogDebug("Dehacked: Warning - UNKNOWN FRAME FIELD: %s\n", field_name);
        return;
    }

    MarkState(st_num);
}

void frames::AlterPointer(int new_val)
{
    int         st_num    = patch::active_obj;
    const char *deh_field = patch::line_buf;

    EPI_ASSERT(st_num >= 0);

    // the kS_NULL state is never output, no need to change it
    if (st_num == kS_NULL) return;

    MarkState(st_num);

    State *st = new_states[st_num];
    EPI_ASSERT(st);

    if (epi::StringCaseCompareASCII(deh_field, "Codep Frame") != 0)
    {
        LogDebug("Dehacked: Warning - UNKNOWN POINTER FIELD: %s\n", deh_field);
        return;
    }

    if (new_val < 0 || new_val >= kTotalMBFStates)
    {
        LogDebug(
            "Dehacked: Warning - Line %d: Illegal Codep frame number: %d\n",
            patch::line_num, new_val);
        return;
    }

    st->action = states_orig[new_val].action;
}

void frames::AlterBexCodePtr(const char *new_action)
{
    const char *bex_field = patch::line_buf;

    if (epi::StringPrefixCaseCompareASCII(bex_field, "FRAME ") != 0)
    {
        LogDebug(
            "Dehacked: Warning - Line %d: bad code pointer '%s' - must begin "
            "with FRAME.\n",
            patch::line_num, bex_field);
        return;
    }

    int st_num;

    if (sscanf(bex_field + 6, " %i ", &st_num) != 1)
    {
        LogDebug("Dehacked: Warning - Line %d: unreadable FRAME number: %s\n",
                 patch::line_num, bex_field + 6);
        return;
    }

    if (st_num < 0 || st_num > 32767)
    {
        LogDebug("Dehacked: Warning - Line %d: illegal FRAME number: %d\n",
                 patch::line_num, st_num);
        return;
    }

    // the kS_NULL state is never output, no need to change it
    if (st_num == kS_NULL) return;

    MarkState(st_num);

    State *st = new_states[st_num];
    EPI_ASSERT(st);

    int action;

    for (action = 0; action < kTotalMBF21Actions; action++)
    {
        // use +2 here to ignore the "A_" prefix
        if (epi::StringCaseCompareASCII(action_info[action].bex_name + 2,
                                        new_action) == 0)
        {
            // found it!
            st->action = action;
            return;
        }
    }

    LogDebug("Dehacked: Warning - Line %d: unknown action %s for CODEPTR.\n",
             patch::line_num, new_action);
}

}  // namespace dehacked
