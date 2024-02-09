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
//
//  DEH_EDGE is based on:
//
//  +  DeHackEd source code, by Greg Lewis.
//  -  DOOM source code (C) 1993-1996 id Software, Inc.
//  -  Linux DOOM Hack Editor, by Sam Lantinga.
//  -  PrBoom's DEH/BEX code, by Ty Halderman, TeamTNT.
//
//------------------------------------------------------------------------

#include "deh_ammo.h"

#include "deh_buffer.h"
#include "deh_edge.h"
#include "deh_patch.h"
#include "deh_system.h"
#include "deh_things.h"

#include <string.h>

namespace dehacked
{

namespace ammo
{
int player_max[4];
int pickups[4];

bool ammo_modified[4];

void MarkAmmo(int a_num);
} // namespace ammo

void ammo::Init()
{
    // doubled for backpack
    player_max[0] = 200;
    player_max[1] = 50;
    player_max[2] = 300;
    player_max[3] = 50;

    // multiplied by 5 for boxes
    pickups[0] = 10;
    pickups[1] = 4;
    pickups[2] = 20;
    pickups[3] = 1;

    memset(ammo_modified, 0, sizeof(ammo_modified));
}

void ammo::Shutdown()
{
}

void ammo::MarkAmmo(int a_num)
{
   SYS_ASSERT(0 <= a_num && a_num < kTotalAmmoTypes && a_num != kAmmoTypeUnused);

    ammo_modified[a_num] = true;
}

void ammo::AmmoDependencies()
{
    bool any = ammo_modified[0] || ammo_modified[1] || ammo_modified[2] || ammo_modified[3];

    if (any)
    {
        things::MarkThing(kMT_PLAYER);
        things::MarkThing(kMT_MISC24); // backpack
    }

    if (ammo_modified[kAmmoTypeBullet])
    {
        things::MarkThing(kMT_CLIP);   // "CLIP"
        things::MarkThing(kMT_MISC17); // "BOX_OF_BULLETS"
    }

    if (ammo_modified[kAmmoTypeShell])
    {
        things::MarkThing(kMT_MISC22); // "SHELLS"
        things::MarkThing(kMT_MISC23); // "BOX_OF_SHELLS"
    }

    if (ammo_modified[kAmmoTypeRocket])
    {
        things::MarkThing(kMT_MISC18); // "ROCKET"
        things::MarkThing(kMT_MISC19); // "BOX_OF_ROCKETS"
    }

    if (ammo_modified[kAmmoTypeCell])
    {
        things::MarkThing(kMT_MISC20); // "CELLS"
        things::MarkThing(kMT_MISC21); // "CELL_PACK"
    }
}

const char *ammo::GetAmmo(int type)
{
    switch (type)
    {
    case kAmmoTypeBullet:
        return "BULLETS";
    case kAmmoTypeShell:
        return "SHELLS";
    case kAmmoTypeRocket:
        return "ROCKETS";
    case kAmmoTypeCell:
        return "CELLS";
    case kAmmoTypeNoAmmo:
        return "NOAMMO";
    }

    I_Error("Dehacked: Internal Error - Bad ammo type %d\n", type);
    return NULL;
}

void ammo::AlterAmmo(int new_val)
{
    int         a_num     = patch::active_obj;
    const char *deh_field = patch::line_buf;

    SYS_ASSERT(0 <= a_num && a_num < kTotalAmmoTypes && a_num != kAmmoTypeUnused);

    bool max_m = (0 == epi::StringCaseCompareASCII(deh_field, "Max ammo"));
    bool per_m = (0 == epi::StringCaseCompareASCII(deh_field, "Per ammo"));

    if (!max_m && !per_m)
    {
        I_Debugf("Dehacked: Warning - UNKNOWN AMMO FIELD: %s\n", deh_field);
        return;
    }

    if (new_val > 10000)
        new_val = 10000;

    if (new_val < 0)
    {
        I_Debugf("Dehacked: Warning - Bad value '%d' for AMMO field: %s\n", new_val, deh_field);
        return;
    }

    if (max_m)
        player_max[a_num] = new_val;
    if (per_m)
        pickups[a_num] = new_val;

    MarkAmmo(a_num);
}

} // namespace dehacked
