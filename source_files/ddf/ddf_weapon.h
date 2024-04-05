//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Main)
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2024 The EDGE Team.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//----------------------------------------------------------------------------

#pragma once

#include "ddf_states.h"
#include "ddf_types.h"

// ------------------------------------------------------------------
// -----------------------WEAPON HANDLING----------------------------
// ------------------------------------------------------------------

constexpr uint8_t kTotalWeaponKeys = 10;

class WeaponDefinitionContainer : public std::vector<WeaponDefinition *>
{
  public:
    WeaponDefinitionContainer();
    ~WeaponDefinitionContainer();

  public:
    // Search Functions
    int               FindFirst(const char *name, int startpos = -1);
    WeaponDefinition *Lookup(const char *refname);
};

// -------EXTERNALISATIONS-------

extern WeaponDefinitionContainer weapondefs; // -ACB- 2004/07/14 Implemented

void DDFReadWeapons(const std::string &data);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
