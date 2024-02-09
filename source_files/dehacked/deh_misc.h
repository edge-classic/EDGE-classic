//------------------------------------------------------------------------
//  MISCELLANEOUS Definitions
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

namespace miscellaneous
{
extern int init_ammo;
/* NOTE: initial health is set in mobjinfo[MT_PLAYER] */

extern int max_armour;
extern int max_health;

extern int green_armour_class;
extern int blue_armour_class;
extern int bfg_cells_per_shot;

extern int soul_health;
extern int soul_limit;
extern int mega_health;  // and limit

extern int monster_infight;

// NOTE: we don't support changing the amounts given by cheats
//       (God Mode Health, IDKFA Armor, etc).

void Init();
void Shutdown();

void AlterMisc(int new_val);
}  // namespace miscellaneous

}  // namespace dehacked