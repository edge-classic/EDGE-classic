//------------------------------------------------------------------------
//  WEAPON conversion
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

#ifndef __DEH_WEAPONS_HDR__
#define __DEH_WEAPONS_HDR__

namespace dehacked
{

// This file diverges slightly from the style guide with enum member naming
// as these reflect the historical code pointer/state/flag/etc names - Dasho

enum WeaponFlagMBF21
{
    // Doesn't thrust things
    kMBF21_NOTHRUST = 1,

    // Weapon is silent
    kMBF21_SILENT = 2,

    // Weapon won't autofire when swapped to
    kMBF21_NOAUTOFIRE = 4,

    // Monsters consider it a melee weapon (currently unused)
    kMBF21_FLEEMELEE = 8,

    // Can be switched away from when ammo is picked up
    kMBF21_AUTOSWITCHFROM = 16,

    // Cannot be switched to when ammo is picked up
    kMBF21_NOAUTOSWITCHTO = 32,
};

// Weapon info: sprite frames, ammunition use.
struct WeaponInfo
{
    const char *ddf_name;

    int ammo, ammo_per_shot;
    int bind_key, priority;

    const char *flags;

    int upstate;
    int downstate;
    int readystate;
    int atkstate;
    int flashstate;
    int mbf21_flags;
};

// The defined weapons...
enum WeaponType
{
    kwp_fist,
    kwp_pistol,
    kwp_shotgun,
    kwp_chaingun,
    kwp_missile,
    kwp_plasma,
    kwp_bfg,
    kwp_chainsaw,
    kwp_supershotgun,

    kTotalWeapons
};

extern WeaponInfo weapon_info[kTotalWeapons];

namespace weapons
{
void Init();
void Shutdown();

void MarkWeapon(int kwp_num);

void AlterWeapon(int new_val);

void ConvertWEAP(void);
} // namespace weapons

} // namespace dehacked

#endif /* __DEH_WEAPONS_HDR__ */
