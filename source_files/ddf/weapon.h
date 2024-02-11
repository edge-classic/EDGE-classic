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

#ifndef __DDF_WEAPON_H__
#define __DDF_WEAPON_H__

#include "epi.h"
#include "states.h"
#include "types.h"

// ------------------------------------------------------------------
// -----------------------WEAPON HANDLING----------------------------
// ------------------------------------------------------------------

#define WEAPON_KEYS 10

class weapondef_container_c : public std::vector<weapondef_c *>
{
   public:
    weapondef_container_c();
    ~weapondef_container_c();

   public:
    // Search Functions
    int          FindFirst(const char *name, int startpos = -1);
    weapondef_c *Lookup(const char *refname);
};

// -------EXTERNALISATIONS-------

extern weapondef_container_c weapondefs;  // -ACB- 2004/07/14 Implemented

void DDF_ReadWeapons(const std::string &data);

#endif  // __DDF_WEAPON_H__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
