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

#ifndef __DDF_ATK_H__
#define __DDF_ATK_H__

// ------------------------------------------------------------------
// --------------------ATTACK TYPE STRUCTURES------------------------
// ------------------------------------------------------------------

// -KM- 1998/11/25 Added BFG SPRAY attack type.

class atkdef_container_c : public std::vector<atkdef_c *>
{
   public:
    atkdef_container_c();
    ~atkdef_container_c();

   public:
    // Search Functions
    atkdef_c *Lookup(const char *refname);
};

// -----EXTERNALISATIONS-----

extern atkdef_container_c atkdefs;  // -ACB- 2004/06/09 Implemented

void DDF_ReadAtks(const std::string &data);

#endif  // __DDF_ATK_H__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
