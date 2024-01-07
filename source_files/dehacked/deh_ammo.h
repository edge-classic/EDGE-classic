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

#ifndef __DEH_AMMO_HDR__
#define __DEH_AMMO_HDR__

namespace Deh_Edge
{

// Ammunition types defined.
typedef enum
{
    am_bullet, // Pistol / chaingun ammo.
    am_shell,  // Shotgun / double barreled shotgun.
    am_cell,   // Plasma rifle, BFG.
    am_rocket, // Missile launcher.

    am_unused, // Not used

    am_noammo, // Fist / chainsaw
    NUMAMMO
} ammotype_e;

namespace Ammo
{
extern int plr_max[4];
extern int pickups[4];

void Init();
void Shutdown();

void MarkAmmo(int a_num);
void AmmoDependencies();

const char *GetAmmo(int type);

void AlterAmmo(int new_val);
} // namespace Ammo

} // namespace Deh_Edge

#endif /* __DEH_AMMO_HDR__ */
