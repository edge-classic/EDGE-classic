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

class AttackDefinitionContainer : public std::vector<AttackDefinition *>
{
   public:
    AttackDefinitionContainer();
    ~AttackDefinitionContainer();

   public:
    AttackDefinition *Lookup(const char *refname);
};

extern AttackDefinitionContainer atkdefs;  // -ACB- 2004/06/09 Implemented

void DDF_ReadAtks(const std::string &data);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
