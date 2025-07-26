//------------------------------------------------------------------------
//  WEAPON Conversion
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

#include "deh_weapons.h"

#include <stddef.h> // offsetof
#include <stdio.h>
#include <string.h>

#include "deh_ammo.h"
#include "deh_buffer.h"
#include "deh_edge.h"
#include "deh_field.h"
#include "deh_frames.h"
#include "deh_info.h"
#include "deh_misc.h"
#include "deh_patch.h"
#include "deh_sounds.h"
#include "deh_system.h"
#include "deh_things.h"
#include "deh_wad.h"
#include "epi.h"
#include "epi_str_compare.h"
namespace dehacked
{

static constexpr char kWeaponFlagFree             = 'f';
static constexpr char kWeaponFlagRefireInaccurate = 'r';
static constexpr char kWeaponFlagDangerous        = 'd';
static constexpr char kWeaponFlagNoThrust         = 't';
static constexpr char kWeaponFlagFeedback         = 'b';

WeaponInfo weapon_info[kTotalWeapons] = {
    {"FIST", kAmmoTypeNoAmmo, 0, 1, 0, "f", kS_PUNCHUP, kS_PUNCHDOWN, kS_PUNCH, kS_PUNCH1, kS_NULL, 0},
    {"PISTOL", kAmmoTypeBullet, 1, 2, 2, "fr", kS_PISTOLUP, kS_PISTOLDOWN, kS_PISTOL, kS_PISTOL1, kS_PISTOLFLASH, 0},
    {"SHOTGUN", kAmmoTypeShell, 1, 3, 3, nullptr, kS_SGUNUP, kS_SGUNDOWN, kS_SGUN, kS_SGUN1, kS_SGUNFLASH1, 0},
    {"CHAINGUN", kAmmoTypeBullet, 1, 4, 5, "r", kS_CHAINUP, kS_CHAINDOWN, kS_CHAIN, kS_CHAIN1, kS_CHAINFLASH1, 0},
    {"ROCKET_LAUNCHER", kAmmoTypeRocket, 1, 5, 6, "d", kS_MISSILEUP, kS_MISSILEDOWN, kS_MISSILE, kS_MISSILE1,
     kS_MISSILEFLASH1, 0},
    {"PLASMA_RIFLE", kAmmoTypeCell, 1, 6, 7, nullptr, kS_PLASMAUP, kS_PLASMADOWN, kS_PLASMA, kS_PLASMA1,
     kS_PLASMAFLASH1, 0},
    {"BFG_9000", kAmmoTypeCell, 40, 7, 8, "d", kS_BFGUP, kS_BFGDOWN, kS_BFG, kS_BFG1, kS_BFGFLASH1, 0},
    {"CHAINSAW", kAmmoTypeNoAmmo, 0, 1, 1, "bt", kS_SAWUP, kS_SAWDOWN, kS_SAW, kS_SAW1, kS_NULL, 0},
    {"SUPER_SHOTGUN", kAmmoTypeShell, 2, 3, 4, nullptr, kS_DSGUNUP, kS_DSGUNDOWN, kS_DSGUN, kS_DSGUN1, kS_DSGUNFLASH1,
     0},
};

const WeaponInfo *current_weap = nullptr;

bool weapon_modified[kTotalWeapons];

//----------------------------------------------------------------------------

void weapons::Init()
{
    EPI_CLEAR_MEMORY(weapon_modified, bool, 9);
}

void weapons::Shutdown()
{
}

namespace weapons
{

struct FlagName
{
    int         flag;
    const char *name; // for EDGE
    const char *bex;  // nullptr if same as EDGE name
};

const FieldReference weapon_field[] = {
    {"Ammo type", offsetof(WeaponInfo, ammo), kFieldTypeAmmoNumber},
    {"Ammo per shot", offsetof(WeaponInfo, ammo_per_shot), kFieldTypeZeroOrGreater},

    // -AJA- these first two fields have misleading dehacked names
    {"Deselect frame", offsetof(WeaponInfo, upstate), kFieldTypeFrameNumber},
    {"Select frame", offsetof(WeaponInfo, downstate), kFieldTypeFrameNumber},
    {"Bobbing frame", offsetof(WeaponInfo, readystate), kFieldTypeFrameNumber},
    {"Shooting frame", offsetof(WeaponInfo, atkstate), kFieldTypeFrameNumber},
    {"Firing frame", offsetof(WeaponInfo, flashstate), kFieldTypeFrameNumber},
    {"MBF21 Bits", offsetof(WeaponInfo, mbf21_flags), kFieldTypeBitflags},

    {nullptr, 0, kFieldTypeAny} // End sentinel
};

const FlagName mbf21flagnamelist[] = {
    {kMBF21_NOTHRUST, "NOTHRUST", nullptr},
    {kMBF21_SILENT, "SILENT_TO_MONSTERS", "SILENT"},
    {kMBF21_NOAUTOFIRE, "NOAUTOFIRE", nullptr},
    {kMBF21_FLEEMELEE, "FLEEMELEE", nullptr},
    {kMBF21_AUTOSWITCHFROM, "SWITCH", "AUTOSWITCHFROM"},
    {kMBF21_NOAUTOSWITCHTO, "DANGEROUS", "NOAUTOSWITCHTO"},

    {0, nullptr, nullptr} // End sentinel
};
} // namespace weapons

namespace weapons
{
bool got_one;

void MarkWeapon(int wp_num)
{
    EPI_ASSERT(0 <= wp_num && wp_num < kTotalWeapons);

    weapon_modified[wp_num] = true;
}

void BeginLump(void)
{
    wad::NewLump(kDDFTypeWeapon);

    wad::Printf("<WEAPONS>\n\n");
}

void FinishLump(void)
{
    wad::Printf("\n");
}

void HandleFlags(const WeaponInfo *info)
{
    if (!info->flags)
        return;

    if (strchr(info->flags, kWeaponFlagFree))
        wad::Printf("FREE = TRUE;\n");

    if (strchr(info->flags, kWeaponFlagRefireInaccurate))
        wad::Printf("REFIRE_INACCURATE = TRUE;\n");

    if (strchr(info->flags, kWeaponFlagDangerous))
        wad::Printf("DANGEROUS = TRUE;\n");

    if (strchr(info->flags, kWeaponFlagNoThrust))
        wad::Printf("NOTHRUST = TRUE;\n");

    if (strchr(info->flags, kWeaponFlagFeedback))
        wad::Printf("FEEDBACK = TRUE;\n");
}

void AddOneFlag(const char *name, bool &got_a_flag)
{
    if (!got_a_flag)
    {
        got_a_flag = true;

        wad::Printf("SPECIAL = ");
    }
    else
        wad::Printf(",");

    wad::Printf("%s", name);
}

void HandleMBF21Flags(const WeaponInfo *info, int w_num)
{
    int  i;
    int  cur_f      = info->mbf21_flags;
    bool got_a_flag = false;

    for (i = 0; mbf21flagnamelist[i].name != nullptr; i++)
    {
        if (0 == (cur_f & mbf21flagnamelist[i].flag))
            continue;

        cur_f &= ~mbf21flagnamelist[i].flag;

        AddOneFlag(mbf21flagnamelist[i].name, got_a_flag);
    }

    if (got_a_flag)
        wad::Printf(";\n");

    if (cur_f != 0)
        LogDebug("Dehacked: Warning - Unconverted flags 0x%08x in weapontype %d\n", cur_f, w_num);
}

void HandleSounds(const WeaponInfo *info, int w_num)
{
    if (w_num == kwp_chainsaw)
    {
        wad::Printf("START_SOUND = \"%s\";\n", sounds::GetSound(ksfx_sawup).c_str());
        if (info->readystate == kS_SAW)
            wad::Printf("IDLE_SOUND = \"%s\";\n", sounds::GetSound(ksfx_sawidl).c_str());
        if (info->atkstate == kS_SAW2)
        wad::Printf("ENGAGED_SOUND = \"%s\";\n", sounds::GetSound(ksfx_sawful).c_str());
        return;
    }

    // otherwise nothing.
}

void HandleFrames(const WeaponInfo *info)
{
    frames::ResetGroups();

    // --- collect states into groups ---

    bool has_flash = frames::CheckWeaponFlash(info->atkstate);

    int count = 0;

    if (has_flash)
        count += frames::BeginGroup('f', info->flashstate);

    count += frames::BeginGroup('a', info->atkstate);
    count += frames::BeginGroup('r', info->readystate);
    count += frames::BeginGroup('d', info->downstate);
    count += frames::BeginGroup('u', info->upstate);

    if (count == 0)
    {
        LogDebug("Dehacked: Warning - Weapon [%s] has no states.\n", info->ddf_name);
        return;
    }

    current_weap = info;

    frames::SpreadGroups();

    frames::OutputGroup('u');
    frames::OutputGroup('d');
    frames::OutputGroup('r');
    frames::OutputGroup('a');

    if (has_flash)
        frames::OutputGroup('f');

    current_weap = nullptr;
}

void HandleAttacks(const WeaponInfo *info, int w_num)
{
    int count = (frames::attack_slot[0] ? 1 : 0) + (frames::attack_slot[1] ? 1 : 0) + (frames::attack_slot[2] ? 1 : 0);

    if (count == 0)
        return;

    if (count > 1)
        LogDebug("Dehacked: Warning - Multiple attacks used in weapon [%s]\n", info->ddf_name);

    wad::Printf("\n");

    const char *atk = frames::attack_slot[0];

    if (!atk)
        atk = frames::attack_slot[1];

    if (!atk)
        atk = frames::attack_slot[2];

    EPI_ASSERT(atk != nullptr);

    wad::Printf("ATTACK = %s;\n", atk);

    // 2023.11.17 - Added SAWFUL ENGAGE_SOUND for non-chainsaw weapons using the
    // chainsaw attack Fixes, for instance, the Harmony Compatible knife swing
    // being silent
    if (epi::StringCaseCompareASCII(atk, "PLAYER_SAW") == 0 && w_num != kwp_chainsaw)
        wad::Printf("ENGAGED_SOUND = \"%s\";\n", sounds::GetSound(ksfx_sawful).c_str());
}

void ConvertWeapon(int w_num)
{
    if (!got_one)
    {
        got_one = true;
        BeginLump();
    }

    const WeaponInfo *info = weapon_info + w_num;

    wad::Printf("[%s]\n", info->ddf_name);

    wad::Printf("AMMOTYPE = %s;\n", ammo::GetAmmo(info->ammo));

    if (w_num == kwp_bfg)
    {
        // Allow ammo per shot field to govern BFG if using the newest Dehacked versions
        if ((patch::doom_ver == 21 || patch::doom_ver == 2021) && info->ammo_per_shot != 0)
            wad::Printf("AMMOPERSHOT = %d;\n", info->ammo_per_shot);
        else
            wad::Printf("AMMOPERSHOT = %d;\n", miscellaneous::bfg_cells_per_shot);
    }
    else if (info->ammo_per_shot != 0)
        wad::Printf("AMMOPERSHOT = %d;\n", info->ammo_per_shot);
    else if (w_num == kwp_supershotgun)
        wad::Printf("AMMOPERSHOT = 2;\n");
    else
        wad::Printf("AMMOPERSHOT = 1;\n");

    wad::Printf("AUTOMATIC = TRUE;\n");

    wad::Printf("BINDKEY = %d;\n", info->bind_key);

    wad::Printf("PRIORITY = %d;\n", info->priority);

    HandleFlags(info);
    HandleMBF21Flags(info, w_num);
    HandleSounds(info, w_num);
    HandleFrames(info);
    HandleAttacks(info, w_num);

    wad::Printf("\n");
}
} // namespace weapons

void weapons::ConvertWEAP(void)
{
    got_one = false;

    for (int i = 0; i < kTotalWeapons; i++)
    {
        if (!all_mode && !weapon_modified[i])
            continue;

        ConvertWeapon(i);
    }

    if (got_one)
        FinishLump();
}

//------------------------------------------------------------------------

void weapons::AlterWeapon(int new_val)
{
    int         wp_num     = patch::active_obj;
    const char *field_name = patch::line_buf;

    EPI_ASSERT(0 <= wp_num && wp_num < kTotalWeapons);

    int *raw_obj = (int *)&weapon_info[wp_num];

    if (!FieldAlter(weapon_field, field_name, raw_obj, new_val))
    {
        LogDebug("Dehacked: Warning - UNKNOWN WEAPON FIELD: %s\n", field_name);
        return;
    }

    MarkWeapon(wp_num);
}

} // namespace dehacked
