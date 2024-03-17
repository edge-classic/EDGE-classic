//----------------------------------------------------------------------------
//  EDGE Data Definition File Codes (WAD-specific fixes)
//----------------------------------------------------------------------------
//
//  Copyright (c) 2022-2024 The EDGE Team.
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

#include "ddf_types.h"

class WadFixDefinition
{
  public:
    WadFixDefinition();
    ~WadFixDefinition(){};

  public:
    void Default(void);
    void CopyDetail(WadFixDefinition &src);

    // Member vars....
    std::string name_;

    std::string md5_string_; // Fixes are likely to be for finalized WADs that
                             // won't be updated anymore, but other qualifiers
                             // like unique lumps might be added if necessary

  private:
    // disable copy construct and assignment operator
    explicit WadFixDefinition(WadFixDefinition &rhs)
    {
    }
    WadFixDefinition &operator=(WadFixDefinition &rhs)
    {
        return *this;
    }
};

// Our fixdefs container
class WadFixDefinitionContainer : public std::vector<WadFixDefinition *>
{
  public:
    WadFixDefinitionContainer()
    {
    }
    ~WadFixDefinitionContainer()
    {
        for (std::vector<WadFixDefinition *>::iterator iter = begin(), iter_end = end(); iter != iter_end; iter++)
        {
            WadFixDefinition *f = *iter;
            delete f;
            f = nullptr;
        }
    }

  public:
    WadFixDefinition *Find(const char *name);
};

extern WadFixDefinitionContainer fixdefs; // -DASHO- 2022 Implemented

void DDF_ReadFixes(const std::string &data);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
