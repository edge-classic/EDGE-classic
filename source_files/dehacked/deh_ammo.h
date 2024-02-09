//------------------------------------------------------------------------
//  AMMO Handling
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

#pragma once

namespace dehacked
{

// Ammunition types defined.
enum AmmoType
{
    kAmmoTypeBullet,  // Pistol / chaingun ammo.
    kAmmoTypeShell,   // Shotgun / double barreled shotgun.
    kAmmoTypeCell,    // Plasma rifle, BFG.
    kAmmoTypeRocket,  // Missile launcher.
    kAmmoTypeUnused,  // Not used
    kAmmoTypeNoAmmo,  // Fist / chainsaw
    kTotalAmmoTypes
};

namespace ammo
{
extern int player_max[4];
extern int pickups[4];

void Init();
void Shutdown();

void MarkAmmo(int a_num);
void AmmoDependencies();

const char *GetAmmo(int type);

void AlterAmmo(int new_val);
}  // namespace ammo

}  // namespace dehacked