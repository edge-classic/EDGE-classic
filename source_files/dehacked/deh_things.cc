//------------------------------------------------------------------------
//  THING Conversion
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

#include "deh_things.h"

#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>

#include <string>

#include "deh_ammo.h"
#include "deh_buffer.h"
#include "deh_edge.h"
#include "deh_field.h"
#include "deh_frames.h"
#include "deh_info.h"
#include "deh_misc.h"
#include "deh_mobj.h"
#include "deh_patch.h"
#include "deh_rscript.h"
#include "deh_sounds.h"
#include "deh_sprites.h"
#include "deh_system.h"
#include "deh_text.h"
#include "deh_wad.h"
#include "deh_weapons.h"
#include "epi.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"
#include "stb_sprintf.h"
namespace dehacked
{

#define DEHACKED_DEBUG_MONSTERS 0

static constexpr char kExtraFlagDisloyal     = 'D';
static constexpr char kExtraFlagTriggerHappy = 'H';
static constexpr char kExtraFlagBossMan      = 'B';
static constexpr char kExtraFlagLoud         = 'L';
static constexpr char kExtraFlagNoRaise      = 'R';
static constexpr char kExtraFlagNoGrudge     = 'G';
static constexpr char kExtraFlagNoItemBk     = 'I';

static constexpr uint8_t kCastMaximum = 18;

extern DehackedMapObjectDefinition mobjinfo[kTotalDehackedMapObjectTypesPortCompatibility];

extern DehackedMapObjectDefinition brain_explode_mobj;

std::vector<DehackedMapObjectDefinition *> new_mobjinfo;

static inline float FixedToFloat(int fixed)
{
    return (float)fixed / 65536.0f;
}

namespace things
{
DehackedMapObjectDefinition       *NewMobj(int mt_num);
const DehackedMapObjectDefinition *OldMobj(int mt_num);
const DehackedMapObjectDefinition *NewMobjElseOld(int mt_num);
} // namespace things

//----------------------------------------------------------------------------
//
//  ATTACKS
//
//----------------------------------------------------------------------------

static constexpr char kAttackFlagFaceTarget = 'F';
static constexpr char kAttackFlagSight      = 'S';
static constexpr char kAttackFlagKillFail   = 'K';
static constexpr char kAttackFlagNoTrace    = 't';
static constexpr char kAttackFlagTooClose   = 'c';
static constexpr char kAttackFlagKeepFire   = 'e';
static constexpr char kAttackFlagPuffSmoke  = 'p';

namespace Attacks
{
bool got_one;
bool flag_got_one;

struct ScratchAttack
{
    int         damage;
    std::string sfx;
    std::string fullname;
};

std::vector<ScratchAttack> scratchers;

const char *AddScratchAttack(int damage, const std::string &sfx);

void BeginLump(void)
{
    wad::NewLump(kDDFTypeAttack);

    wad::Printf("<ATTACKS>\n\n");
}

void FinishLump(void)
{
    wad::Printf("\n");
}

struct ExtraAttack
{
    int         mt_num;
    const char *atk_type;
    int         atk_height;
    int         translucency;
    const char *flags;
};

const ExtraAttack attack_extra[] = {{kMT_FIRE, "TRACKER", 0, 75, "FS"},
                                    {kMT_TRACER, "PROJECTILE", 48, 75, "cptF"},
                                    {kMT_FATSHOT, "FIXED_SPREADER", 32, 75, ""},
                                    {kMT_TROOPSHOT, "PROJECTILE", 32, 75, "F"},
                                    {kMT_BRUISERSHOT, "PROJECTILE", 32, 75, "F"},
                                    {kMT_HEADSHOT, "PROJECTILE", 32, 75, "F"},
                                    {kMT_ARACHPLAZ, "PROJECTILE", 16, 50, "eF"},
                                    {kMT_ROCKET, "PROJECTILE", 44, 75, "FK"},
                                    {kMT_PLASMA, "PROJECTILE", 32, 75, "eK"},
                                    {kMT_BFG, "PROJECTILE", 32, 50, "K"},
                                    {kMT_EXTRABFG, "SPRAY", 0, 75, ""},
                                    {kMT_SPAWNSHOT, "SHOOTTOSPOT", 16, 100, ""},

                                    {-1, nullptr, 0, 0, ""}};

void HandleSounds(const DehackedMapObjectDefinition *info, int mt_num)
{
    if (info->seesound != ksfx_None)
        wad::Printf("LAUNCH_SOUND = \"%s\";\n", sounds::GetSound(info->seesound).c_str());

    if (info->deathsound != ksfx_None)
        wad::Printf("DEATH_SOUND = \"%s\";\n", sounds::GetSound(info->deathsound).c_str());

    if (info->rip_sound != ksfx_None)
        wad::Printf("RIP_SOUND = \"%s\";\n", sounds::GetSound(info->rip_sound).c_str());

    if (mt_num == kMT_FIRE)
    {
        wad::Printf("ATTEMPT_SOUND = \"%s\";\n", sounds::GetSound(ksfx_vilatk).c_str());
        wad::Printf("ENGAGED_SOUND = \"%s\";\n", sounds::GetSound(ksfx_barexp).c_str());
    }

    if (mt_num == kMT_FATSHOT)
        wad::Printf("ATTEMPT_SOUND = \"%s\";\n", sounds::GetSound(ksfx_manatk).c_str());
}

void HandleFrames(const DehackedMapObjectDefinition *info, int mt_num)
{
    frames::ResetGroups();

    if (info->fullbright)
        frames::force_fullbright = true;

    // special cases...

    if (mt_num == kMT_SPAWNSHOT)
    {
        // EDGE merges kMT_SPAWNSHOT and kMT_SPAWNFIRE into a single
        // attack ("BRAIN_CUBE").

        int count = 0;

        const DehackedMapObjectDefinition *spawnfire = things::NewMobjElseOld(kMT_SPAWNFIRE);
        EPI_ASSERT(spawnfire);

        count += frames::BeginGroup('D', spawnfire->spawnstate);
        count += frames::BeginGroup('S', info->spawnstate);

        if (count != 2)
            LogDebug("Dehacked: Warning - Brain cube is missing spawn/fire "
                     "states.\n");

        if (count == 0)
        {
            frames::force_fullbright = false;
            return;
        }

        frames::SpreadGroups();

        frames::OutputGroup('S');
        frames::OutputGroup('D');

        frames::force_fullbright = false;
        return;
    }

    // --- collect states into groups ---

    int count = 0;

    count += frames::BeginGroup('D', info->deathstate);
    count += frames::BeginGroup('E', info->seestate);
    count += frames::BeginGroup('S', info->spawnstate);

    if (count == 0)
    {
        LogDebug("Dehacked: Warning - Attack [%s] has no states.\n", things::GetMobjName(mt_num) + 1);
        frames::force_fullbright = false;
        return;
    }

    frames::SpreadGroups();

    frames::OutputGroup('S');
    frames::OutputGroup('E');
    frames::OutputGroup('D');

    frames::force_fullbright = false;
}

void AddAtkSpecial(const char *name)
{
    if (!flag_got_one)
    {
        flag_got_one = true;
        wad::Printf("ATTACK_SPECIAL = ");
    }
    else
        wad::Printf(",");

    wad::Printf("%s", name);
}

void HandleAtkSpecials(const ExtraAttack *ext, bool plr_rocket)
{
    flag_got_one = false;

    if (strchr(ext->flags, kAttackFlagFaceTarget) && !plr_rocket)
        AddAtkSpecial("FACE_TARGET");

    if (strchr(ext->flags, kAttackFlagSight))
        AddAtkSpecial("NEED_SIGHT");

    if (strchr(ext->flags, kAttackFlagKillFail))
        AddAtkSpecial("KILL_FAILED_SPAWN");

    if (strchr(ext->flags, kAttackFlagPuffSmoke))
        AddAtkSpecial("SMOKING_TRACER");

    if (flag_got_one)
        wad::Printf(";\n");
}

void CheckPainElemental(void)
{
    // two attacks which refer to the LOST_SOUL's missile states
    // (ELEMENTAL_SPAWNER and ELEMENTAL_DEATHSPAWN).  check if
    // those states are still valid, recreate attacks if not.

    const DehackedMapObjectDefinition *skull = things::NewMobjElseOld(kMT_SKULL);
    EPI_ASSERT(skull);

    if (frames::CheckMissileState(skull->missilestate))
        return;

    // need to write out new versions

    if (!got_one)
    {
        got_one = true;
        BeginLump();
    }

    const char *spawn_at = nullptr;

    if (skull->seestate != kS_NULL)
        spawn_at = "CHASE:1";
    else if (skull->missilestate != kS_NULL)
        spawn_at = "MISSILE:1";
    else if (skull->meleestate != kS_NULL)
        spawn_at = "MELEE:1";
    else
        spawn_at = "IDLE:1";

    wad::Printf("[ELEMENTAL_SPAWNER]\n");
    wad::Printf("ATTACKTYPE = SPAWNER;\n");
    wad::Printf("ATTACK_HEIGHT = 8;\n");
    wad::Printf("ATTACK_SPECIAL = PRESTEP_SPAWN,FACE_TARGET;\n");
    wad::Printf("SPAWNED_OBJECT = LOST_SOUL;\n");
    wad::Printf("SPAWN_OBJECT_STATE = %s;\n", spawn_at);

    wad::Printf("SPAWN_LIMIT = 21;\n");

    wad::Printf("\n");
    wad::Printf("[ELEMENTAL_DEATHSPAWN]\n");
    wad::Printf("ATTACKTYPE = TRIPLE_SPAWNER;\n");
    wad::Printf("ATTACK_HEIGHT = 8;\n");
    wad::Printf("ATTACK_SPECIAL = PRESTEP_SPAWN,FACE_TARGET;\n");
    wad::Printf("SPAWNED_OBJECT = LOST_SOUL;\n");
    wad::Printf("SPAWN_OBJECT_STATE = %s;\n", spawn_at);
}

void ConvertAttack(const DehackedMapObjectDefinition *info, int mt_num, bool plr_rocket);
void ConvertScratch(const ScratchAttack *atk);
} // namespace Attacks

const char *Attacks::AddScratchAttack(int damage, const std::string &sfx)
{
    std::string safe_sfx = "QUIET";
    if (!sfx.empty())
    {
        safe_sfx.clear();
        for (const char &ch : sfx)
        {
            if (epi::IsAlphanumericASCII(ch) || ch == '_')
                safe_sfx.push_back(ch);
        }
    }

    static char namebuf[256];
    stbsp_snprintf(namebuf, sizeof(namebuf), "SCRATCH_%s_%d", safe_sfx.c_str(), damage);

    // already have it?
    for (size_t i = 0; i < scratchers.size(); i++)
    {
        if (strcmp(scratchers[i].fullname.c_str(), namebuf) == 0)
            return namebuf;
    }

    scratchers.push_back({damage, (sfx.empty() ? "" : sfx), namebuf});

    return namebuf;
}

void Attacks::ConvertScratch(const ScratchAttack *atk)
{
    if (!got_one)
    {
        got_one = true;
        BeginLump();
    }

    wad::Printf("[%s]\n", atk->fullname.c_str());

    wad::Printf("ATTACKTYPE=CLOSECOMBAT;\n");
    wad::Printf("DAMAGE.VAL=%d;\n", atk->damage);
    wad::Printf("DAMAGE.MAX=%d;\n", atk->damage);
    wad::Printf("ATTACKRANGE=80;\n");
    wad::Printf("ATTACK_SPECIAL=FACE_TARGET;\n");

    if (atk->sfx != "")
    {
        wad::Printf("ENGAGED_SOUND=%s;\n", atk->sfx.c_str());
    }

    wad::Printf("\n");
}

void Attacks::ConvertAttack(const DehackedMapObjectDefinition *info, int mt_num, bool plr_rocket)
{
    if (info->name[0] != '*') // thing?
        return;

    // kMT_SPAWNFIRE is handled specially (in other code)
    if (mt_num == kMT_SPAWNFIRE)
        return;

    if (!got_one)
    {
        got_one = true;
        BeginLump();
    }

    if (plr_rocket)
        wad::Printf("[%s]\n", "PLAYER_MISSILE");
    else
        wad::Printf("[%s]\n", things::GetMobjName(mt_num) + 1);

    // find attack in the extra table...
    const ExtraAttack *ext = nullptr;

    for (int j = 0; attack_extra[j].atk_type; j++)
        if (attack_extra[j].mt_num == mt_num)
        {
            ext = attack_extra + j;
            break;
        }

    if (!ext)
        FatalError("Dehacked: Error - Missing attack %s in extra table.\n", things::GetMobjName(mt_num) + 1);

    wad::Printf("ATTACKTYPE = %s;\n", ext->atk_type);

    wad::Printf("RADIUS = %1.1f;\n", FixedToFloat(info->radius));
    wad::Printf("HEIGHT = %1.1f;\n", FixedToFloat(info->height));

    if (info->spawnhealth != 1000)
        wad::Printf("SPAWNHEALTH = %d;\n", info->spawnhealth);

    if (info->speed != 0)
        wad::Printf("SPEED = %s;\n", things::GetSpeed(info->speed));

    if (info->mass != 100)
        wad::Printf("MASS = %d;\n", info->mass);

    if (mt_num == kMT_BRUISERSHOT)
        wad::Printf("FAST = 1.4;\n");
    else if (mt_num == kMT_TROOPSHOT || mt_num == kMT_HEADSHOT)
        wad::Printf("FAST = 2.0;\n");

    if (plr_rocket)
        wad::Printf("ATTACK_HEIGHT = 32;\n");
    else if (ext->atk_height != 0)
        wad::Printf("ATTACK_HEIGHT = %d;\n", ext->atk_height);

    if (mt_num == kMT_FIRE)
    {
        wad::Printf("DAMAGE.VAL = 20;\n");
        wad::Printf("EXPLODE_DAMAGE.VAL = 70;\n");
    }
    else if (mt_num == kMT_EXTRABFG)
    {
        wad::Printf("DAMAGE.VAL   = 65;\n");
        wad::Printf("DAMAGE.ERROR = 50;\n");
    }
    else if (info->damage > 0)
    {
        wad::Printf("DAMAGE.VAL = %d;\n", info->damage);
        wad::Printf("DAMAGE.MAX = %d;\n", info->damage * 8);
    }

    if (info->splash_group >= 0)
        wad::Printf("SPLASH_GROUP = %d;\n",
                    info->splash_group + 1); // We don't want a '0' splash group when it hits DDF

    if (mt_num == kMT_BFG)
        wad::Printf("SPARE_ATTACK = BFG9000_SPRAY;\n");

    if (ext->translucency != 100)
        wad::Printf("TRANSLUCENCY = %d%%;\n", ext->translucency);

    if (strchr(ext->flags, kAttackFlagPuffSmoke))
        wad::Printf("PUFF = SMOKE;\n");

    if (strchr(ext->flags, kAttackFlagTooClose))
        wad::Printf("TOO_CLOSE_RANGE = 196;\n");

    if (strchr(ext->flags, kAttackFlagNoTrace))
    {
        wad::Printf("NO_TRACE_CHANCE = 50%%;\n");

        wad::Printf("TRACE_ANGLE = 9;\n");
    }

    if (strchr(ext->flags, kAttackFlagKeepFire))
        wad::Printf("KEEP_FIRING_CHANCE = 4%%;\n");

    HandleAtkSpecials(ext, plr_rocket);
    HandleSounds(info, mt_num);
    HandleFrames(info, mt_num);

    wad::Printf("\n");

    things::HandleFlags(info, mt_num, 0);

    if (frames::attack_slot[0] || frames::attack_slot[1] || frames::attack_slot[2])
    {
        LogDebug("Dehacked: Warning - Attack [%s] contained an attacking action.\n", things::GetMobjName(mt_num) + 1);
        things::HandleAttacks(info, mt_num);
    }

    if (frames::act_flags & kActionFlagExplode)
        wad::Printf("EXPLODE_DAMAGE.VAL = 128;\n");
    else if (info->damage)
    {
        if (frames::act_flags & kActionFlagDetonate)
            wad::Printf("EXPLODE_DAMAGE.VAL = %d;\n", info->damage);
        wad::Printf("PROJECTILE_DAMAGE.VAL = %d;\n", info->damage);
        wad::Printf("PROJECTILE_DAMAGE.MAX = %d;\n", info->damage * 8);
    }

    wad::Printf("\n");
}

//----------------------------------------------------------------------------
//
//  THINGS
//
//----------------------------------------------------------------------------

const int height_fixes[] = {
    kMT_MISC14, 60, kMT_MISC29, 78, kMT_MISC30, 58, kMT_MISC31, 46,  kMT_MISC33, 38, kMT_MISC34, 50,  kMT_MISC38, 56,
    kMT_MISC39, 48, kMT_MISC41, 96, kMT_MISC42, 96, kMT_MISC43, 96,  kMT_MISC44, 72, kMT_MISC45, 72,  kMT_MISC46, 72,
    kMT_MISC70, 64, kMT_MISC72, 52, kMT_MISC73, 40, kMT_MISC74, 64,  kMT_MISC75, 64, kMT_MISC76, 120,

    kMT_MISC36, 56, kMT_MISC37, 56, kMT_MISC47, 56, kMT_MISC48, 128, kMT_MISC35, 56, kMT_MISC40, 56,  kMT_MISC50, 56,
    kMT_MISC77, 42,

    -1,         -1 /* the end */
};

namespace things
{
int cast_mobjs[kCastMaximum];

void BeginLump();
void FinishLump();

bool CheckIsMonster(const DehackedMapObjectDefinition *info, int player, bool use_act_flags);
} // namespace things

void things::Init()
{
    new_mobjinfo.clear();
    Attacks::scratchers.clear();
}

void things::Shutdown()
{
    for (size_t i = 0; i < new_mobjinfo.size(); i++)
        if (new_mobjinfo[i] != nullptr)
            delete new_mobjinfo[i];

    new_mobjinfo.clear();

    Attacks::scratchers.clear();
}

void things::BeginLump()
{
    wad::NewLump(kDDFTypeThing);

    wad::Printf("<THINGS>\n\n");
}

void things::FinishLump(void)
{
    wad::Printf("\n");
}

void things::MarkThing(int mt_num)
{
    EPI_ASSERT(mt_num >= 0);

    // handle merged things/attacks
    if (mt_num == kMT_TFOG)
        MarkThing(kMT_TELEPORTMAN);

    if (mt_num == kMT_SPAWNFIRE)
        MarkThing(kMT_SPAWNSHOT);

    // fill any missing slots with nullptrs, including the one we want
    while ((int)new_mobjinfo.size() < mt_num + 1)
    {
        new_mobjinfo.push_back(nullptr);
    }

    // already have a modified entry?
    if (new_mobjinfo[mt_num] != nullptr)
        return;

    // create new entry, copy original info if we have it
    DehackedMapObjectDefinition *entry = new DehackedMapObjectDefinition;
    new_mobjinfo[mt_num]               = entry;

    if (mt_num < kTotalDehackedMapObjectTypesPortCompatibility)
    {
        memcpy(entry, &mobjinfo[mt_num], sizeof(DehackedMapObjectDefinition));
    }
    else
    {
        EPI_CLEAR_MEMORY(entry, DehackedMapObjectDefinition, 1);

        entry->name      = "X"; // only needed to differentiate from an attack
        entry->doomednum = -1;

        // DEHEXTRA things have a default doomednum
        // Dasho: Only specify a doomed number if the "ID #" field
        // is used
        /*if (kMT_EXTRA00 <= mt_num && mt_num <= kMT_EXTRA99)
        {
            entry->doomednum = mt_num;
        }*/

        // Set some default MBF21 values to be non-applicable unless actually set
        entry->proj_group = entry->splash_group = entry->infight_group = entry->fast_speed = entry->melee_range = -2;
    }
}

void things::UseThing(int mt_num)
{
    // only create something when our standard DDF lacks it
    if (mt_num >= kMT_DOGS)
        MarkThing(mt_num);
}

void things::MarkAllMonsters()
{
    for (int i = 0; i < kTotalDehackedMapObjectTypesPortCompatibility; i++)
    {
        if (i == kMT_PLAYER)
            continue;

        const DehackedMapObjectDefinition *mobj = &mobjinfo[i];

        if (CheckIsMonster(mobj, 0, false))
            MarkThing(i);
    }
}

DehackedMapObjectDefinition *things::GetModifiedMobj(int mt_num)
{
    MarkThing(mt_num);

    return &mobjinfo[mt_num];
}

const char *things::GetMobjName(int mt_num)
{
    EPI_ASSERT(mt_num >= 0);

    if (mt_num < kTotalDehackedMapObjectTypesPortCompatibility)
        return mobjinfo[mt_num].name;

    static char buffer[64];

    if (kMT_EXTRA00 <= mt_num && mt_num <= kMT_EXTRA99)
        stbsp_snprintf(buffer, sizeof(buffer), "MT_EXTRA%02d", mt_num - kMT_EXTRA00);
    else
        stbsp_snprintf(buffer, sizeof(buffer), "DEHACKED_%d", mt_num + 1);

    return buffer;
}

void things::SetPlayerHealth(int new_value)
{
    MarkThing(kMT_PLAYER);

    new_mobjinfo[kMT_PLAYER]->spawnhealth = new_value;
}

const DehackedMapObjectDefinition *things::OldMobj(int mt_num)
{
    if (mt_num < kTotalDehackedMapObjectTypesPortCompatibility)
        return &mobjinfo[mt_num];

    return nullptr;
}

DehackedMapObjectDefinition *things::NewMobj(int mt_num)
{
    if (mt_num < (int)new_mobjinfo.size())
        return new_mobjinfo[mt_num];

    return nullptr;
}

const DehackedMapObjectDefinition *things::NewMobjElseOld(int mt_num)
{
    const DehackedMapObjectDefinition *info = NewMobj(mt_num);
    if (info != nullptr)
        return info;

    return OldMobj(mt_num);
}

int things::GetMobjMBF21Flags(int mt_num)
{
    const DehackedMapObjectDefinition *info = NewMobjElseOld(mt_num);
    if (info == nullptr)
        return 0;
    return info->mbf21_flags;
}

bool things::IsSpawnable(int mt_num)
{
    // attacks are not spawnable via A_Spawn
    if (mt_num < kTotalDehackedMapObjectTypesPortCompatibility && mobjinfo[mt_num].name[0] == '*')
        return false;

    const DehackedMapObjectDefinition *info = NewMobjElseOld(mt_num);
    if (info == nullptr)
        return false;

    return (info->doomednum > 0 || mt_num >= kTotalDehackedMapObjectTypesPortCompatibility);
}

const char *things::AddScratchAttack(int damage, const std::string &sfx)
{
    return Attacks::AddScratchAttack(damage, sfx);
}

namespace things
{
struct FlagName
{
    long long int flag; // flag in DehackedMapObjectDefinition (kMF_XXX), 0 if ignored
    const char   *bex;  // name in a DEHACKED or BEX file
    const char   *conv; // edge name, nullptr if none, can be multiple
};

const FlagName flag_list[] = {
    {kMF_SPECIAL, "SPECIAL", "SPECIAL"},
    {kMF_SOLID, "SOLID", "SOLID"},
    {kMF_SHOOTABLE, "SHOOTABLE", "SHOOTABLE"},
    {kMF_NOSECTOR, "NOSECTOR", "NOSECTOR"},
    {kMF_NOBLOCKMAP, "NOBLOCKMAP", "NOBLOCKMAP"},
    {kMF_AMBUSH, "AMBUSH", "AMBUSH"},
    {0, "JUSTHIT", nullptr},
    {0, "JUSTATTACKED", nullptr},
    {kMF_SPAWNCEILING, "SPAWNCEILING", "SPAWNCEILING"},
    {kMF_NOGRAVITY, "NOGRAVITY", "NOGRAVITY"},
    {kMF_DROPOFF, "DROPOFF", "DROPOFF"},
    {kMF_PICKUP, "PICKUP", "PICKUP"},
    {kMF_NOCLIP, "NOCLIP", "NOCLIP"},
    {kMF_SLIDE, "SLIDE", "SLIDER"},
    {kMF_FLOAT, "FLOAT", "FLOAT"},
    {kMF_TELEPORT, "TELEPORT", "TELEPORT"},
    {kMF_MISSILE, "MISSILE", "MISSILE"},
    {kMF_DROPPED, "DROPPED", "DROPPED"},
    {kMF_SHADOW, "SHADOW", "FUZZY"},
    {kMF_NOBLOOD, "NOBLOOD", "DAMAGESMOKE"},
    {kMF_CORPSE, "CORPSE", "CORPSE"},
    {0, "INFLOAT", nullptr},
    {kMF_COUNTKILL, "COUNTKILL", "COUNT_AS_KILL"},
    {kMF_COUNTITEM, "COUNTITEM", "COUNT_AS_ITEM"},
    {kMF_SKULLFLY, "SKULLFLY", "SKULLFLY"},
    {kMF_NOTDMATCH, "NOTDMATCH", "NODEATHMATCH"},
    {kMF_TRANSLATION1, "TRANSLATION1", nullptr},
    {kMF_TRANSLATION2, "TRANSLATION2", nullptr},
    {kMF_TRANSLATION1, "TRANSLATION", nullptr}, // bug compat
    {kMF_TOUCHY, "TOUCHY", "TOUCHY"},
    {kMF_BOUNCES, "BOUNCES", "BOUNCE"},
    {kMF_FRIEND, "FRIEND", nullptr},
    {kMF_TRANSLUCENT, "TRANSLUCENT", nullptr},
    {kMF_TRANSLUCENT, "TRANSLUC50", nullptr},
    // BOOM and MBF flags...
    //{ kMF_STEALTH,      "STEALTH",       "STEALTH" },

    {kMF_UNUSED1, "UNUSED1", nullptr},
    {kMF_UNUSED2, "UNUSED2", nullptr},
    {kMF_UNUSED3, "UNUSED3", nullptr},
    {kMF_UNUSED4, "UNUSED4", nullptr},

    {0, nullptr, nullptr} // End sentinel
};

const FlagName mbf21flag_list[] = {
    {kMBF21_LOGRAV, "LOGRAV", "LOGRAV"},
    {kMBF21_DMGIGNORED, "DMGIGNORED", "NEVERTARGETED"},
    {kMBF21_NORADIUSDMG, "NORADIUSDMG", "EXPLODE_IMMUNE"},
    {kMBF21_RANGEHALF, "RANGEHALF", "TRIGGER_HAPPY"},
    {kMBF21_NOTHRESHOLD, "NOTHRESHOLD", "NOGRUDGE"},
    {kMBF21_BOSS, "BOSS", "BOSSMAN"},
    {kMBF21_RIP, "RIP", "BORE"},
    {kMBF21_FULLVOLSOUNDS, "FULLVOLSOUNDS", "ALWAYS_LOUD"},

    {kMBF21_HIGHERMPROB, "HIGHERMPROB", "HIGHERMPROB"},
    {kMBF21_SHORTMRANGE, "SHORTMRANGE", "SHORTMRANGE"},
    {kMBF21_LONGMELEE, "LONGMELEE", "LONGMELEE"},
    {kMBF21_FORCERADIUSDMG, "FORCERADIUSDMG", "FORCERADIUSDMG"},

    {kMBF21_MAP07BOSS1, "MAP07BOSS1", nullptr},
    {kMBF21_MAP07BOSS2, "MAP07BOSS2", nullptr},
    {kMBF21_E1M8BOSS, "E1M8BOSS", nullptr},
    {kMBF21_E2M8BOSS, "E2M8BOSS", nullptr},
    {kMBF21_E3M8BOSS, "E3M8BOSS", nullptr},
    {kMBF21_E4M6BOSS, "E4M6BOSS", nullptr},
    {kMBF21_E4M8BOSS, "E4M8BOSS", nullptr},

    {0, nullptr, nullptr} // End sentinel
};

// these are extra flags we add for certain monsters.
// they do not correspond to anything in DEHACKED / BEX / MBF21.
const FlagName extflaglist[] = {
    {kExtraFlagDisloyal, nullptr, "DISLOYAL,ATTACK_HURTS"}, // must be first
    {kExtraFlagTriggerHappy, nullptr, "TRIGGER_HAPPY"},
    {kExtraFlagBossMan, nullptr, "BOSSMAN"},
    {kExtraFlagLoud, nullptr, "ALWAYS_LOUD"},
    {kExtraFlagNoRaise, nullptr, "NO_RESURRECT"},
    {kExtraFlagNoGrudge, nullptr, "NO_GRUDGE,NEVERTARGETED"},
    {kExtraFlagNoItemBk, nullptr, "NO_RESPAWN"},

    {0, nullptr, nullptr} // End sentinel
};

int ParseBits(const FlagName *list, char *bit_str)
{
    int new_flags = 0;

    // these delimiters are the same as what Boom/MBF uses
    static const char *delims = "+|, \t\f\r";

    for (char *token = strtok(bit_str, delims); token != nullptr; token = strtok(nullptr, delims))
    {
        // tokens should be non-empty
        EPI_ASSERT(token[0] != 0);

        if (epi::IsDigitASCII(token[0]) || token[0] == '-')
        {
            int flags;

            if (sscanf(token, " %i ", &flags) == 1)
                new_flags |= flags;
            else
                LogDebug("Dehacked: Warning - Line %d: unreadable BITS value: %s\n", patch::line_num, token);

            continue;
        }

        // find the name in the given list
        int i;
        for (i = 0; list[i].bex != nullptr; i++)
            if (epi::StringCaseCompareASCII(token, list[i].bex) == 0)
                break;

        if (list[i].bex == nullptr)
        {
            LogDebug("Dehacked: Warning - Line %d: unknown BITS mnemonic: %s\n", patch::line_num, token);
            continue;
        }

        new_flags |= list[i].flag;
    }

    return new_flags;
}

bool CheckIsMonster(const DehackedMapObjectDefinition *info, int player, bool use_act_flags)
{
    if (player > 0)
        return false;

    if (info->doomednum <= 0)
        return false;

    if (info->name[0] == '*')
        return false;

    if (info->flags & kMF_COUNTKILL)
        return true;

    if (info->flags & (kMF_SPECIAL | kMF_COUNTITEM))
        return false;

    int score = 0;

    // values determined by statistical analysis of major DEH patches
    // (Standard DOOM, Batman, Mordeth, Wheel-of-Time, Osiris).

    if (info->flags & kMF_SOLID)
        score += 25;
    if (info->flags & kMF_SHOOTABLE)
        score += 72;

    if (info->painstate)
        score += 91;
    if (info->missilestate || info->meleestate)
        score += 91;
    if (info->deathstate)
        score += 72;
    if (info->raisestate)
        score += 31;

    if (use_act_flags)
    {
        if (frames::act_flags & kActionFlagChase)
            score += 78;
        if (frames::act_flags & kActionFlagFall)
            score += 61;
    }

    if (info->speed > 0)
        score += 87;

#if (DEHACKED_DEBUG_MONSTERS)
    Debug_PrintMsg("[%20.20s:%-4d] %c%c%c%c%c %c%c%c%c %c%c %d = %d\n", GetMobjName(mt_num), info->doomednum,
                   (info->flags & kMF_SOLID) ? 'S' : '-', (info->flags & kMF_SHOOTABLE) ? 'H' : '-',
                   (info->flags & kMF_FLOAT) ? 'F' : '-', (info->flags & kMF_MISSILE) ? 'M' : '-',
                   (info->flags & kMF_NOBLOOD) ? 'B' : '-', (info->painstate) ? 'p' : '-',
                   (info->deathstate) ? 'd' : '-', (info->raisestate) ? 'r' : '-',
                   (info->missilestate || info->meleestate) ? 'm' : '-', (frames::act_flags & AF_CHASER) ? 'C' : '-',
                   (frames::act_flags & AF_FALLER) ? 'F' : '-', info->speed, score);
#endif

    return score >= (use_act_flags ? 370 : 300);
}

const char *GetExtFlags(int mt_num, int player)
{
    if (player > 0)
        return "D";

    switch (mt_num)
    {
    case kMT_INS:
    case kMT_INV:
        return "I";

    case kMT_POSSESSED:
    case kMT_SHOTGUY:
    case kMT_CHAINGUY:
        return "D";

    case kMT_SKULL:
        return "DHM";
    case kMT_UNDEAD:
        return "H";

    case kMT_VILE:
        return "GR";
    case kMT_CYBORG:
        return "BHR";
    case kMT_SPIDER:
        return "BHR";

    case kMT_BOSSSPIT:
        return "B";
    case kMT_BOSSBRAIN:
        return "L";

    default:
        break;
    }

    return "";
}

void AddOneFlag(const DehackedMapObjectDefinition *info, const char *name, bool &got_a_flag)
{
    if (!got_a_flag)
    {
        got_a_flag = true;

        if (info->name[0] == '*')
            wad::Printf("PROJECTILE_SPECIAL = ");
        else
            wad::Printf("SPECIAL = ");
    }
    else
        wad::Printf(",");

    wad::Printf("%s", name);
}

void HandleFlags(const DehackedMapObjectDefinition *info, int mt_num, int player)
{
    int  i;
    int  cur_f      = info->flags;
    bool got_a_flag = false;

    // strangely absent from kMT_PLAYER
    if (player)
        cur_f |= kMF_SLIDE;

    // this can cause EDGE 1.27 to crash
    if (!player)
        cur_f &= ~kMF_PICKUP;

    // EDGE requires teleportman in sector. (DOOM uses thinker list)
    if (mt_num == kMT_TELEPORTMAN)
        cur_f &= ~kMF_NOSECTOR;

    // special workaround for negative MASS values
    if (info->mass < 0)
        cur_f |= kMF_SPAWNCEILING | kMF_NOGRAVITY;

    bool is_monster     = CheckIsMonster(info, player, true);
    bool force_disloyal = (is_monster && miscellaneous::monster_infight == 221);

    for (i = 0; flag_list[i].bex != nullptr; i++)
    {
        if (0 == (cur_f & flag_list[i].flag))
            continue;

        if (flag_list[i].conv != nullptr)
            AddOneFlag(info, flag_list[i].conv, got_a_flag);
    }

    const char *eflags = GetExtFlags(mt_num, player);

    for (i = 0; extflaglist[i].bex != nullptr; i++)
    {
        char ch = (char)extflaglist[i].flag;

        if (!strchr(eflags, ch))
            continue;

        if (ch == kExtraFlagDisloyal)
        {
            force_disloyal = true;
            continue;
        }

        AddOneFlag(info, extflaglist[i].conv, got_a_flag);
    }

    cur_f = info->mbf21_flags;

    for (i = 0; mbf21flag_list[i].bex != nullptr; i++)
    {
        if (0 == (cur_f & mbf21flag_list[i].flag))
            continue;

        if (mbf21flag_list[i].conv != nullptr)
            AddOneFlag(info, mbf21flag_list[i].conv, got_a_flag);
    }

    cur_f = info->flags;

    if (force_disloyal)
        AddOneFlag(info, extflaglist[0].conv, got_a_flag);

    if (is_monster)
        AddOneFlag(info, "MONSTER", got_a_flag);

    // Dasho - For MBF compat, we need to make bouncy things shootable when they are defined
    // via Dehacked.
    if (cur_f & kMF_BOUNCES)
    {
        if (!(cur_f & kMF_SHOOTABLE))
            AddOneFlag(info, "SHOOTABLE", got_a_flag);
    }

    AddOneFlag(info, "DEHACKED_COMPAT", got_a_flag);

    if (got_a_flag)
        wad::Printf(";\n");

    if (cur_f & kMF_TRANSLATION)
    {
        if ((cur_f & kMF_TRANSLATION) == 0x4000000)
            wad::Printf("PALETTE_REMAP = PLAYER_DK_GREY;\n");
        else if ((cur_f & kMF_TRANSLATION) == 0x8000000)
            wad::Printf("PALETTE_REMAP = PLAYER_BROWN;\n");
        else
            wad::Printf("PALETTE_REMAP = PLAYER_DULL_RED;\n");
    }

    if (cur_f & kMF_TRANSLUCENT)
    {
        wad::Printf("TRANSLUCENCY = 50%%;\n");
    }

    if ((cur_f & kMF_FRIEND) && !player)
    {
        wad::Printf("SIDE = 16777215;\n");
    }
}

void FixHeights()
{
    for (int i = 0; height_fixes[i] >= 0; i += 2)
    {
        int mt_num = height_fixes[i];
        int new_h  = height_fixes[i + 1];

        EPI_ASSERT(mt_num < kTotalDehackedMapObjectTypesPortCompatibility);

        // if the thing was not modified, nothing to do here
        if (mt_num >= (int)new_mobjinfo.size())
            continue;

        DehackedMapObjectDefinition *info = new_mobjinfo[mt_num];
        if (info == nullptr)
            continue;

        /* Kludge for Aliens TC (and others) that put these things on
         * the ceiling -- they need the 16 height for correct display,
         */
        if (info->flags & kMF_SPAWNCEILING)
            continue;

        if (info->height != 16 * kFracUnit)
            continue;

        info->height = new_h * kFracUnit;
    }
}

void CollectTheCast()
{
    for (int i = 0; i < kCastMaximum; i++)
        cast_mobjs[i] = -1;

    for (int mt_num = 0; mt_num < kTotalDehackedMapObjectTypesPortCompatibility; mt_num++)
    {
        int order = 0;

        // cast objects are required to have CHASE and DEATH states
        const DehackedMapObjectDefinition *info = NewMobjElseOld(mt_num);

        if (info->seestate == kS_NULL || info->deathstate == kS_NULL)
            continue;

        switch (mt_num)
        {
        case kMT_PLAYER:
            order = 1;
            break;
        case kMT_POSSESSED:
            order = 2;
            break;
        case kMT_SHOTGUY:
            order = 3;
            break;
        case kMT_CHAINGUY:
            order = 4;
            break;
        case kMT_TROOP:
            order = 5;
            break;
        case kMT_SERGEANT:
            order = 6;
            break;
        case kMT_SKULL:
            order = 7;
            break;
        case kMT_HEAD:
            order = 8;
            break;
        case kMT_KNIGHT:
            order = 9;
            break;
        case kMT_BRUISER:
            order = 10;
            break;
        case kMT_BABY:
            order = 11;
            break;
        case kMT_PAIN:
            order = 12;
            break;
        case kMT_UNDEAD:
            order = 13;
            break;
        case kMT_FATSO:
            order = 14;
            break;
        case kMT_VILE:
            order = 15;
            break;
        case kMT_SPIDER:
            order = 16;
            break;
        case kMT_CYBORG:
            order = 17;
            break;

        default:
            continue;
        }

        if (order >= kCastMaximum)
        {
            FatalError("CollectTheCast() - Overflow");
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
        stbsp_sprintf(num_buf, "%1.2f", FixedToFloat(speed));
    else
        stbsp_sprintf(num_buf, "%d", speed);

    return num_buf;
}

void HandleSounds(const DehackedMapObjectDefinition *info, int mt_num)
{
    if (info->activesound != ksfx_None)
    {
        if (info->flags & kMF_PICKUP)
            wad::Printf("PICKUP_SOUND = \"%s\";\n", sounds::GetSound(info->activesound).c_str());
        else
            wad::Printf("ACTIVE_SOUND = \"%s\";\n", sounds::GetSound(info->activesound).c_str());
    }
    else if (mt_num == kMT_TELEPORTMAN)
        wad::Printf("ACTIVE_SOUND = \"%s\";\n", sounds::GetSound(ksfx_telept).c_str());

    if (info->seesound != ksfx_None)
        wad::Printf("SIGHTING_SOUND = \"%s\";\n", sounds::GetSound(info->seesound).c_str());

    // Dasho - Commented this out; Eviternity's boss will play his opening dialogue twice if we keep this.
    // Assume that anyone actually editing this thing will play a sound if they want to
    // else if (mt_num == kMT_BOSSSPIT)
    // wad::Printf("SIGHTING_SOUND = \"%s\";\n", sounds::GetSound(ksfx_bossit).c_str());

    // Dasho - Removed melee state requirement, as the MBF21 A_MonsterBulletAttack codepointer
    // uses this sound
    if (info->attacksound != ksfx_None) // && info->meleestate != kS_NULL)
        wad::Printf("STARTCOMBAT_SOUND = \"%s\";\n", sounds::GetSound(info->attacksound).c_str());

    if (info->painsound != ksfx_None)
        wad::Printf("PAIN_SOUND = \"%s\";\n", sounds::GetSound(info->painsound).c_str());

    if (info->deathsound != ksfx_None)
        wad::Printf("DEATH_SOUND = \"%s\";\n", sounds::GetSound(info->deathsound).c_str());

    if (info->rip_sound != ksfx_None)
        wad::Printf("RIP_SOUND = \"%s\";\n", sounds::GetSound(info->rip_sound).c_str());
}

void HandleFrames(const DehackedMapObjectDefinition *info, int mt_num)
{
    frames::ResetGroups();

    // special cases...

    if (mt_num == kMT_TELEPORTMAN)
    {
        wad::Printf("TRANSLUCENCY = 50%%;\n");
        wad::Printf("\n");
        wad::Printf("STATES(IDLE) = %s:A:-1:NORMAL:TRANS_SET(0%%);\n", sprites::GetSprite(kSPR_TFOG));

        // EDGE doesn't use the TELEPORT_FOG object, instead it uses
        // the CHASE states of the TELEPORT_FLASH object (i.e. the one
        // used to find the destination in the target sector).

        const DehackedMapObjectDefinition *tfog = NewMobjElseOld(kMT_TFOG);
        EPI_ASSERT(tfog);

        if (0 == frames::BeginGroup('E', tfog->spawnstate))
        {
            LogDebug("Dehacked: Warning - Teleport fog has no spawn states.\n");
            return;
        }

        frames::SpreadGroups();
        frames::OutputGroup('E');

        return;
    }

    // --- collect states into groups ---

    int count = 0;

    // do more important states AFTER less important ones
    count += frames::BeginGroup('R', info->raisestate);
    count += frames::BeginGroup('X', info->xdeathstate);
    count += frames::BeginGroup('D', info->deathstate);
    count += frames::BeginGroup('P', info->painstate);
    count += frames::BeginGroup('M', info->missilestate);
    count += frames::BeginGroup('L', info->meleestate);
    count += frames::BeginGroup('E', info->seestate);
    count += frames::BeginGroup('S', info->spawnstate);

    if (count == 0)
    {
        // only occurs with special/invisible objects, currently only
        // with teleport target (handled above) and brain spit targets.

        if (mt_num != kMT_BOSSTARGET)
            LogDebug("Dehacked: Warning - Mobj [%s:%d] has no states.\n", GetMobjName(mt_num), info->doomednum);

        wad::Printf("TRANSLUCENCY = 0%%;\n");

        wad::Printf("\n");
        wad::Printf("STATES(IDLE) = %s:A:-1:NORMAL:NOTHING;\n", sprites::GetSprite(kSPR_CAND));

        return;
    }

    frames::SpreadGroups();

    frames::OutputGroup('S');
    frames::OutputGroup('E');
    frames::OutputGroup('L');
    frames::OutputGroup('M');
    frames::OutputGroup('P');
    frames::OutputGroup('D');
    frames::OutputGroup('X');
    frames::OutputGroup('R');

    // the A_VileChase action is another special case
    if (frames::act_flags & kActionFlagRaise)
    {
        if (frames::BeginGroup('H', kS_VILE_HEAL1) > 0)
        {
            frames::SpreadGroups();
            frames::OutputGroup('H');
        }
    }
}

const int NUMPLAYERS = 8;

struct PlayerInfo
{
    const char *name;
    int         num;
    const char *remap;
};

const PlayerInfo player_info[NUMPLAYERS] = {{"OUR_HERO", 1, "PLAYER_GREEN"},    {"PLAYER2", 2, "PLAYER_DK_GREY"},
                                            {"PLAYER3", 3, "PLAYER_BROWN"},     {"PLAYER4", 4, "PLAYER_DULL_RED"},
                                            {"PLAYER5", 4001, "PLAYER_ORANGE"}, {"PLAYER6", 4002, "PLAYER_LT_GREY"},
                                            {"PLAYER7", 4003, "PLAYER_LT_RED"}, {"PLAYER8", 4004, "PLAYER_PINK"}};

void HandlePlayer(int player)
{
    if (player <= 0)
        return;

    EPI_ASSERT(player <= NUMPLAYERS);

    const PlayerInfo *pi = player_info + (player - 1);

    wad::Printf("PLAYER = %d;\n", player);
    wad::Printf("SIDE = %d;\n", 1 << (player - 1));
    wad::Printf("PALETTE_REMAP = %s;\n", pi->remap);

    wad::Printf("INITIAL_BENEFIT = \n");
    wad::Printf("    BULLETS.LIMIT(%d), ", ammo::player_max[kAmmoTypeBullet]);
    wad::Printf("SHELLS.LIMIT(%d), ", ammo::player_max[kAmmoTypeShell]);
    wad::Printf("ROCKETS.LIMIT(%d), ", ammo::player_max[kAmmoTypeRocket]);
    wad::Printf("CELLS.LIMIT(%d),\n", ammo::player_max[kAmmoTypeCell]);
    wad::Printf("    PELLETS.LIMIT(%d), ", 200);
    wad::Printf("NAILS.LIMIT(%d), ", 100);
    wad::Printf("GRENADES.LIMIT(%d), ", 50);
    wad::Printf("GAS.LIMIT(%d),\n", 300);

    wad::Printf("    AMMO9.LIMIT(%d), ", 100);
    wad::Printf("AMMO10.LIMIT(%d), ", 200);
    wad::Printf("AMMO11.LIMIT(%d), ", 50);
    wad::Printf("AMMO12.LIMIT(%d),\n", 300);
    wad::Printf("    AMMO13.LIMIT(%d), ", 100);
    wad::Printf("AMMO14.LIMIT(%d), ", 200);
    wad::Printf("AMMO15.LIMIT(%d), ", 50);
    wad::Printf("AMMO16.LIMIT(%d),\n", 300);

    wad::Printf("    BULLETS(%d);\n", miscellaneous::init_ammo);
}

struct PickupItem
{
    int         spr_num;
    const char *benefit;
    int         par_num;
    int         amount, limit;
    const char *ldf;
    int         sound;
};

const PickupItem pickup_item[] = {
    // Health & Armor....
    {kSPR_BON1, "HEALTH", 2, 1, 200, "GotHealthPotion", ksfx_itemup},
    {kSPR_STIM, "HEALTH", 2, 10, 100, "GotStim", ksfx_itemup},
    {kSPR_MEDI, "HEALTH", 2, 25, 100, "GotMedi", ksfx_itemup},
    {kSPR_BON2, "GREEN_ARMOUR", 2, 1, 200, "GotArmourHelmet", ksfx_itemup},
    {kSPR_ARM1, "GREEN_ARMOUR", 2, 100, 100, "GotArmour", ksfx_itemup},
    {kSPR_ARM2, "BLUE_ARMOUR", 2, 200, 200, "GotMegaArmour", ksfx_itemup},

    // Keys....
    {kSPR_BKEY, "KEY_BLUECARD", 0, 0, 0, "GotBlueCard", ksfx_itemup},
    {kSPR_YKEY, "KEY_YELLOWCARD", 0, 0, 0, "GotYellowCard", ksfx_itemup},
    {kSPR_RKEY, "KEY_REDCARD", 0, 0, 0, "GotRedCard", ksfx_itemup},
    {kSPR_BSKU, "KEY_BLUESKULL", 0, 0, 0, "GotBlueSkull", ksfx_itemup},
    {kSPR_YSKU, "KEY_YELLOWSKULL", 0, 0, 0, "GotYellowSkull", ksfx_itemup},
    {kSPR_RSKU, "KEY_REDSKULL", 0, 0, 0, "GotRedSkull", ksfx_itemup},

    // Ammo....
    {kSPR_CLIP, "BULLETS", 1, 10, 0, "GotClip", ksfx_itemup},
    {kSPR_AMMO, "BULLETS", 1, 50, 0, "GotClipBox", ksfx_itemup},
    {kSPR_SHEL, "SHELLS", 1, 4, 0, "GotShells", ksfx_itemup},
    {kSPR_SBOX, "SHELLS", 1, 20, 0, "GotShellBox", ksfx_itemup},
    {kSPR_ROCK, "ROCKETS", 1, 1, 0, "GotRocket", ksfx_itemup},
    {kSPR_BROK, "ROCKETS", 1, 5, 0, "GotRocketBox", ksfx_itemup},
    {kSPR_CELL, "CELLS", 1, 20, 0, "GotCell", ksfx_itemup},
    {kSPR_CELP, "CELLS", 1, 100, 0, "GotCellPack", ksfx_itemup},

    // Powerups....
    {kSPR_SOUL, "HEALTH", 2, 100, 200, "GotSoul", ksfx_getpow},
    {kSPR_PMAP, "POWERUP_AUTOMAP", 0, 0, 0, "GotMap", ksfx_getpow},
    {kSPR_PINS, "POWERUP_PARTINVIS", 2, 100, 100, "GotInvis", ksfx_getpow},
    {kSPR_PINV, "POWERUP_INVULNERABLE", 2, 30, 30, "GotInvulner", ksfx_getpow},
    {kSPR_PVIS, "POWERUP_LIGHTGOGGLES", 2, 120, 120, "GotVisor", ksfx_getpow},
    {kSPR_SUIT, "POWERUP_ACIDSUIT", 2, 60, 60, "GotSuit", ksfx_getpow},

    // Weapons....
    {kSPR_CSAW, "CHAINSAW", 0, 0, 0, "GotChainSaw", ksfx_wpnup},
    {kSPR_SHOT, "SHOTGUN,SHELLS", 1, 8, 0, "GotShotGun", ksfx_wpnup},
    {kSPR_SGN2, "SUPERSHOTGUN,SHELLS", 1, 8, 0, "GotDoubleBarrel", ksfx_wpnup},
    {kSPR_MGUN, "CHAINGUN,BULLETS", 1, 20, 0, "GotChainGun", ksfx_wpnup},
    {kSPR_LAUN, "ROCKET_LAUNCHER,ROCKETS", 1, 2, 0, "GotRocketLauncher", ksfx_wpnup},
    {kSPR_PLAS, "PLASMA_RIFLE,CELLS", 1, 40, 0, "GotPlasmaGun", ksfx_wpnup},
    {kSPR_BFUG, "BFG9000,CELLS", 1, 40, 0, "GotBFG", ksfx_wpnup},

    {-1, nullptr, 0, 0, 0, nullptr, ksfx_None}};

void HandleItem(const DehackedMapObjectDefinition *info, int mt_num)
{
    if (!(info->flags & kMF_SPECIAL))
        return;

    if (info->spawnstate == kS_NULL)
        return;

    int spr_num = frames::GetStateSprite(info->spawnstate);

    // special cases:

    if (spr_num == kSPR_PSTR) // Berserk
    {
        wad::Printf("PICKUP_BENEFIT = POWERUP_BERSERK(60:60),HEALTH(100:100);\n");
        wad::Printf("PICKUP_MESSAGE = GotBerserk;\n");
        wad::Printf("PICKUP_SOUND = %s;\n", sounds::GetSound(ksfx_getpow).c_str());
        wad::Printf("PICKUP_EFFECT = SWITCH_WEAPON(FIST);\n");
        return;
    }
    else if (spr_num == kSPR_MEGA) // Megasphere
    {
        wad::Printf("PICKUP_BENEFIT = ");
        wad::Printf("HEALTH(%d:%d),", miscellaneous::mega_health, miscellaneous::mega_health);
        wad::Printf("BLUE_ARMOUR(%d:%d);\n", miscellaneous::max_armour, miscellaneous::max_armour);
        wad::Printf("PICKUP_MESSAGE = GotMega;\n");
        wad::Printf("PICKUP_SOUND = %s;\n", sounds::GetSound(ksfx_getpow).c_str());
        return;
    }
    else if (spr_num == kSPR_BPAK) // Backpack full of AMMO
    {
        wad::Printf("PICKUP_BENEFIT = \n");
        wad::Printf("    BULLETS.LIMIT(%d), ", 2 * ammo::player_max[kAmmoTypeBullet]);
        wad::Printf("    SHELLS.LIMIT(%d),\n", 2 * ammo::player_max[kAmmoTypeShell]);
        wad::Printf("    ROCKETS.LIMIT(%d), ", 2 * ammo::player_max[kAmmoTypeRocket]);
        wad::Printf("    CELLS.LIMIT(%d),\n", 2 * ammo::player_max[kAmmoTypeCell]);
        wad::Printf("    BULLETS(10), SHELLS(4), ROCKETS(1), CELLS(20);\n");
        wad::Printf("PICKUP_MESSAGE = GotBackpack;\n");
        wad::Printf("PICKUP_SOUND = %s;\n", sounds::GetSound(ksfx_itemup).c_str());
        return;
    }

    int i;

    for (i = 0; pickup_item[i].benefit != nullptr; i++)
    {
        if (spr_num == pickup_item[i].spr_num)
            break;
    }

    const PickupItem *pu = pickup_item + i;

    if (pu->benefit == nullptr) // not found
    {
        LogDebug("Dehacked: Warning - Unknown pickup sprite \"%s\" for item [%s]\n", sprites::GetOriginalName(spr_num),
                 GetMobjName(mt_num));
        return;
    }

    int amount = pu->amount;
    int limit  = pu->limit;

    // handle patchable amounts

    switch (spr_num)
    {
    // Armor & health...
    case kSPR_BON2: // "ARMOUR_HELMET"
        limit = miscellaneous::max_armour;
        break;

    case kSPR_ARM1: // "GREEN_ARMOUR"
        amount = miscellaneous::green_armour_class * 100;
        limit  = miscellaneous::max_armour;
        break;

    case kSPR_ARM2: // "BLUE_ARMOUR"
        amount = miscellaneous::blue_armour_class * 100;
        limit  = miscellaneous::max_armour;
        break;

    case kSPR_BON1:                        // "HEALTH_POTION"
        limit = miscellaneous::max_health; // Note: *not* MEDIKIT
        break;

    case kSPR_SOUL:                        // "SOULSPHERE"
        amount = miscellaneous::soul_health;
        limit  = miscellaneous::soul_limit;
        break;

    // Ammo...
    case kSPR_CLIP: // "CLIP"
    case kSPR_AMMO: // "BOX_OF_BULLETS"
        amount = ammo::pickups[kAmmoTypeBullet];
        break;

    case kSPR_SHEL: // "SHELLS"
    case kSPR_SBOX: // "BOX_OF_SHELLS"
        amount = ammo::pickups[kAmmoTypeShell];
        break;

    case kSPR_ROCK: // "ROCKET"
    case kSPR_BROK: // "BOX_OF_ROCKETS"
        amount = ammo::pickups[kAmmoTypeRocket];
        break;

    case kSPR_CELL: // "CELLS"
    case kSPR_CELP: // "CELL_PACK"
        amount = ammo::pickups[kAmmoTypeCell];
        break;

    default:
        break;
    }

    // big boxes of ammo
    if (spr_num == kSPR_AMMO || spr_num == kSPR_BROK || spr_num == kSPR_CELP || spr_num == kSPR_SBOX)
    {
        amount *= 5;
    }

    if (pu->par_num == 2 && amount > limit)
        amount = limit;

    wad::Printf("PICKUP_BENEFIT = %s", pu->benefit);

    if (pu->par_num == 1)
        wad::Printf("(%d)", amount);
    else if (pu->par_num == 2)
        wad::Printf("(%d:%d)", amount, limit);

    wad::Printf(";\n");
    wad::Printf("PICKUP_MESSAGE = %s;\n", pu->ldf);

    if (info->activesound == ksfx_None)
        wad::Printf("PICKUP_SOUND = %s;\n", sounds::GetSound(pu->sound).c_str());
}

const char *cast_titles[kCastMaximum] = {
    "OurHeroName",  "ZombiemanName", "ShotgunGuyName", "HeavyWeaponDudeName",  "ImpName",         "DemonName",
    "LostSoulName", "CacodemonName", "HellKnightName", "BaronOfHellName",      "ArachnotronName", "PainElementalName",
    "RevenantName", "MancubusName",  "ArchVileName",   "SpiderMastermindName", "CyberdemonName"};

void HandleCastOrder(int mt_num, int player)
{
    if (player >= 2)
        return;

    int pos   = 1;
    int order = 1;

    for (; pos < kCastMaximum; pos++)
    {
        // ignore missing members (ensure real order is contiguous)
        if (cast_mobjs[pos] < 0)
            continue;

        if (cast_mobjs[pos] == mt_num)
            break;

        order++;
    }

    if (pos >= kCastMaximum) // not found
        return;

    wad::Printf("CASTORDER = %d;\n", order);
    wad::Printf("CAST_TITLE = %s;\n", cast_titles[pos - 1]);
}

void HandleDropItem(const DehackedMapObjectDefinition *info, int mt_num);
void HandleAttacks(const DehackedMapObjectDefinition *info, int mt_num);
void ConvertMobj(const DehackedMapObjectDefinition *info, int mt_num, int player, bool brain_missile, bool &got_one);
void HandleBlood(const DehackedMapObjectDefinition *info);
} // namespace things

void things::HandleDropItem(const DehackedMapObjectDefinition *info, int mt_num)
{
    const char *item = nullptr;

    if (info->dropped_item == 0)
    {
        return; // I think '0' is used to clear out normal drops - Dasho
    }
    else if (info->dropped_item - 1 > kMT_PLAYER)
    {
        item = GetMobjName(info->dropped_item - 1);
        if (!item)
            return;
    }
    else
    {
        switch (mt_num)
        {
        case kMT_WOLFSS:
        case kMT_POSSESSED:
            item = "CLIP";
            break;

        case kMT_SHOTGUY:
            item = "SHOTGUN";
            break;
        case kMT_CHAINGUY:
            item = "CHAINGUN";
            break;

        default:
            return;
        }
    }

    EPI_ASSERT(item);

    wad::Printf("DROPITEM = \"%s\";\n", item);
}

void things::HandleBlood(const DehackedMapObjectDefinition *info)
{
    const char *splat = nullptr;

    switch (info->blood_color)
    {
    case 1:
        splat = "DEHEXTRA_BLOOD_GREY";
        break;
    case 2:
        splat = "DEHEXTRA_BLOOD_GREEN";
        break;
    case 3:
        splat = "DEHEXTRA_BLOOD_BLUE";
        break;
    case 4:
        splat = "DEHEXTRA_BLOOD_YELLOW";
        break;
    case 5:
        splat = "DEHEXTRA_BLOOD_BLACK";
        break;
    case 6:
        splat = "DEHEXTRA_BLOOD_PURPLE";
        break;
    case 7:
        splat = "DEHEXTRA_BLOOD_WHITE";
        break;
    case 8:
        splat = "DEHEXTRA_BLOOD_ORANGE";
        break;
    // Red, or fallback if a bad value
    case 0:
    default:
        break;
    }

    if (splat)
        wad::Printf("BLOOD = \"%s\";\n", splat);
}

void things::HandleAttacks(const DehackedMapObjectDefinition *info, int mt_num)
{
    if (frames::attack_slot[frames::kAttackMethodRanged])
    {
        wad::Printf("RANGE_ATTACK = %s;\n", frames::attack_slot[frames::kAttackMethodRanged]);
        wad::Printf("MINATTACK_CHANCE = 25%%;\n");
    }

    if (frames::attack_slot[frames::kAttackMethodCombat])
    {
        wad::Printf("CLOSE_ATTACK = %s;\n", frames::attack_slot[frames::kAttackMethodCombat]);
    }
    else if (info->meleestate && info->name[0] != '*')
    {
        LogDebug("Dehacked: Warning - No close attack in melee states of [%s].\n", GetMobjName(mt_num));
        wad::Printf("CLOSE_ATTACK = DEMON_CLOSECOMBAT; // dummy attack\n");
    }

    if (frames::attack_slot[frames::kAttackMethodSpare])
        wad::Printf("SPARE_ATTACK = %s;\n", frames::attack_slot[frames::kAttackMethodSpare]);
}

void things::ConvertMobj(const DehackedMapObjectDefinition *info, int mt_num, int player, bool brain_missile,
                         bool &got_one)
{
    if (info->name[0] == '*') // attack
        return;

    if (!got_one)
    {
        got_one = true;
        BeginLump();
    }

    const char *ddf_name = GetMobjName(mt_num);

    if (brain_missile)
        ddf_name = info->name;

    if (player > 0)
        wad::Printf("[%s:%d]\n", player_info[player - 1].name, player_info[player - 1].num);
    else if (info->doomednum < 0)
        wad::Printf("[%s]\n", ddf_name);
    else
        wad::Printf("[%s:%d]\n", ddf_name, info->doomednum);

    wad::Printf("DEH_THING_ID = %d;\n", mt_num + 1);

    wad::Printf("RADIUS = %1.1f;\n", FixedToFloat(info->radius));

    wad::Printf("HEIGHT = %1.1f;\n", FixedToFloat(info->height));

    if (info->spawnhealth != 1000)
        wad::Printf("SPAWNHEALTH = %d;\n", info->spawnhealth);

    if (player > 0)
        wad::Printf("SPEED = 1;\n");
    else if (info->speed != 0)
        wad::Printf("SPEED = %s;\n", GetSpeed(info->speed));

    if (info->fast_speed > 0)
        wad::Printf("FAST_SPEED = %s;\n", GetSpeed(info->fast_speed));

    if (info->melee_range > 0)
        wad::Printf("MELEE_RANGE = %f;\n", FixedToFloat(info->melee_range));

    if (info->mass != 100 && info->mass > 0)
        wad::Printf("MASS = %d;\n", info->mass);

    if (info->reactiontime != 0)
        wad::Printf("REACTION_TIME = %dT;\n", info->reactiontime);

    if (info->painchance >= 256)
        wad::Printf("PAINCHANCE = 100%%;\n");
    else if (info->painchance > 0)
        wad::Printf("PAINCHANCE = %1.1f%%;\n", (float)info->painchance * 100.0 / 256.0);

    if (info->splash_group >= 0)
        wad::Printf("SPLASH_GROUP = %d;\n",
                    info->splash_group + 1); // We don't want a '0' splash group when it hits DDF

    if (info->infight_group >= 0)
        wad::Printf("INFIGHTING_GROUP = %d;\n",
                    info->infight_group + 1); // We don't want a '0' infighting group when it hits DDF

    if (info->proj_group > -2)                // -1 is a special value here, so negative is still valid
        wad::Printf("PROJECTILE_GROUP = %d;\n", info->proj_group);

    if (info->gib_health != 0)
        wad::Printf("GIB_HEALTH = %1.1f;\n", FixedToFloat(info->gib_health));

    if (info->pickup_width != 0)
        wad::Printf("PICKUP_WIDTH = %1.1f;\n", FixedToFloat(info->pickup_width));

    if (info->projectile_pass_height != 0)
        wad::Printf("PROJECTILE_PASS_HEIGHT = %1.1f;\n", FixedToFloat(info->projectile_pass_height));

    if (mt_num == kMT_BOSSSPIT)
        wad::Printf("SPIT_SPOT = BRAIN_SPAWNSPOT;\n");

    HandleCastOrder(mt_num, player);
    HandleDropItem(info, mt_num);
    HandlePlayer(player);
    HandleItem(info, mt_num);
    HandleSounds(info, mt_num);
    HandleFrames(info, mt_num);

    // DEHEXTRA
    HandleBlood(info);

    wad::Printf("\n");

    HandleFlags(info, mt_num, player);
    HandleAttacks(info, mt_num);

    if (frames::act_flags & kActionFlagExplode)
        wad::Printf("EXPLODE_DAMAGE.VAL = 128;\n");
    else if (info->damage)
    {
        if (frames::act_flags & kActionFlagDetonate)
            wad::Printf("EXPLODE_DAMAGE.VAL = %d;\n", info->damage);
        wad::Printf("PROJECTILE_DAMAGE.VAL = %d;\n", info->damage);
        wad::Printf("PROJECTILE_DAMAGE.MAX = %d;\n", info->damage * 8);
    }

    if ((frames::act_flags & kActionFlagKeenDie))
        rscript::MarkKeenDie(mt_num);

    wad::Printf("\n");
}

void things::ConvertTHING(void)
{
    FixHeights();

    CollectTheCast();

    bool got_one = false;

    for (int i = 0; i < (int)new_mobjinfo.size(); i++)
    {
        const DehackedMapObjectDefinition *info = new_mobjinfo[i];

        if (info == nullptr)
            continue;

        if (i == kMT_PLAYER)
        {
            for (int p = 1; p <= NUMPLAYERS; p++)
                ConvertMobj(info, i, p, false, got_one);

            continue;
        }

        ConvertMobj(info, i, 0, false, got_one);
    }

    // TODO we don't always need this, figure out WHEN WE DO
    if (true)
        ConvertMobj(&brain_explode_mobj, kMT_ROCKET, 0, true, got_one);

    if (got_one)
        FinishLump();
}

void things::ConvertATK()
{
    Attacks::got_one = false;

    for (size_t k = 0; k < Attacks::scratchers.size(); k++)
    {
        Attacks::ConvertScratch(&Attacks::scratchers[k]);
    }

    for (int i = 0; i < (int)new_mobjinfo.size(); i++)
    {
        const DehackedMapObjectDefinition *info = new_mobjinfo[i];

        if (info == nullptr)
            continue;

        Attacks::ConvertAttack(info, i, false);

        if (i == kMT_ROCKET)
            Attacks::ConvertAttack(info, i, true);
    }

    Attacks::CheckPainElemental();

    if (Attacks::got_one)
        Attacks::FinishLump();
}

//------------------------------------------------------------------------

namespace things
{
const FieldReference mobj_field[] = {
    {"ID #", offsetof(DehackedMapObjectDefinition, doomednum), kFieldTypeAny},
    {"Initial frame", offsetof(DehackedMapObjectDefinition, spawnstate), kFieldTypeFrameNumber},
    {"Hit points", offsetof(DehackedMapObjectDefinition, spawnhealth), kFieldTypeOneOrGreater},
    {"First moving frame", offsetof(DehackedMapObjectDefinition, seestate), kFieldTypeFrameNumber},
    {"Alert sound", offsetof(DehackedMapObjectDefinition, seesound), kFieldTypeSoundNumber},
    {"Reaction time", offsetof(DehackedMapObjectDefinition, reactiontime), kFieldTypeZeroOrGreater},
    {"Attack sound", offsetof(DehackedMapObjectDefinition, attacksound), kFieldTypeSoundNumber},
    {"Injury frame", offsetof(DehackedMapObjectDefinition, painstate), kFieldTypeFrameNumber},
    {"Pain chance", offsetof(DehackedMapObjectDefinition, painchance), kFieldTypeZeroOrGreater},
    {"Pain sound", offsetof(DehackedMapObjectDefinition, painsound), kFieldTypeSoundNumber},
    {"Close attack frame", offsetof(DehackedMapObjectDefinition, meleestate), kFieldTypeFrameNumber},
    {"Far attack frame", offsetof(DehackedMapObjectDefinition, missilestate), kFieldTypeFrameNumber},
    {"Death frame", offsetof(DehackedMapObjectDefinition, deathstate), kFieldTypeFrameNumber},
    {"Exploding frame", offsetof(DehackedMapObjectDefinition, xdeathstate), kFieldTypeFrameNumber},
    {"Death sound", offsetof(DehackedMapObjectDefinition, deathsound), kFieldTypeSoundNumber},
    {"Speed", offsetof(DehackedMapObjectDefinition, speed), kFieldTypeZeroOrGreater},
    {"Width", offsetof(DehackedMapObjectDefinition, radius), kFieldTypeZeroOrGreater},
    {"Height", offsetof(DehackedMapObjectDefinition, height), kFieldTypeZeroOrGreater},
    {"Mass", offsetof(DehackedMapObjectDefinition, mass), kFieldTypeZeroOrGreater},
    {"Missile damage", offsetof(DehackedMapObjectDefinition, damage), kFieldTypeZeroOrGreater},
    {"Action sound", offsetof(DehackedMapObjectDefinition, activesound), kFieldTypeSoundNumber},
    {"Bits", offsetof(DehackedMapObjectDefinition, flags), kFieldTypeBitflags},
    {"MBF21 Bits", offsetof(DehackedMapObjectDefinition, mbf21_flags), kFieldTypeBitflags},
    {"Infighting group", offsetof(DehackedMapObjectDefinition, infight_group), kFieldTypeZeroOrGreater},
    {"Projectile group", offsetof(DehackedMapObjectDefinition, proj_group), kFieldTypeAny},
    {"Splash group", offsetof(DehackedMapObjectDefinition, splash_group), kFieldTypeZeroOrGreater},
    {"Rip sound", offsetof(DehackedMapObjectDefinition, rip_sound), kFieldTypeSoundNumber},
    {"Fast speed", offsetof(DehackedMapObjectDefinition, fast_speed), kFieldTypeZeroOrGreater},
    {"Melee range", offsetof(DehackedMapObjectDefinition, melee_range), kFieldTypeZeroOrGreater},
    {"Gib health", offsetof(DehackedMapObjectDefinition, gib_health), kFieldTypeAny},
    {"Dropped item", offsetof(DehackedMapObjectDefinition, dropped_item), kFieldTypeZeroOrGreater},
    {"Pickup width", offsetof(DehackedMapObjectDefinition, pickup_width), kFieldTypeZeroOrGreater},
    {"Projectile pass height", offsetof(DehackedMapObjectDefinition, projectile_pass_height), kFieldTypeZeroOrGreater},
    {"Fullbright", offsetof(DehackedMapObjectDefinition, fullbright), kFieldTypeZeroOrGreater},
    {"Blood color", offsetof(DehackedMapObjectDefinition, blood_color), kFieldTypeZeroOrGreater},
    {"Respawn frame", offsetof(DehackedMapObjectDefinition, raisestate), kFieldTypeFrameNumber},

    {nullptr, 0, kFieldTypeAny} // End sentinel
};
} // namespace things

void things::AlterThing(int new_val)
{
    int mt_num = patch::active_obj - 1; // NOTE WELL
    EPI_ASSERT(mt_num >= 0);

    const char *field_name = patch::line_buf;

    MarkThing(mt_num);

    int *raw_obj = (int *)new_mobjinfo[mt_num];
    EPI_ASSERT(raw_obj != nullptr);

    if (!FieldAlter(mobj_field, field_name, raw_obj, new_val))
    {
        LogDebug("Dehacked: Warning - UNKNOWN THING FIELD: %s\n", field_name);
    }
}

void things::AlterBexBits(char *bit_str)
{
    int mt_num = patch::active_obj - 1; /* NOTE WELL */
    EPI_ASSERT(mt_num >= 0);

    MarkThing(mt_num);

    new_mobjinfo[mt_num]->flags = ParseBits(flag_list, bit_str);
}

void things::AlterMBF21Bits(char *bit_str)
{
    int mt_num = patch::active_obj - 1; /* NOTE WELL */
    EPI_ASSERT(mt_num >= 0);

    MarkThing(mt_num);

    new_mobjinfo[mt_num]->mbf21_flags = ParseBits(mbf21flag_list, bit_str);
}

} // namespace dehacked
