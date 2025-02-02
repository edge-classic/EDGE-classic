//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Reverbs)
//----------------------------------------------------------------------------
//
//  Copyright (c) 2025 The EDGE Team.
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

#include <unordered_map>

#include "ddf_types.h"
#include "epi.h"
#include "epi_ename.h"
#include "verblib.h"

class ReverbDefinition
{
  public:
    ReverbDefinition();
    ~ReverbDefinition() {};

  public:
    void Default(void);
    void CopyDetail(const ReverbDefinition &src);
    void ApplyReverb(verblib *reverb);

    // Member vars....
    epi::EName name_;

    float room_size_;
    float damping_level_;
    float wet_level_;
    float dry_level_;
    float reverb_width_;

  private:
    // disable copy construct and assignment operator
    explicit ReverbDefinition(ReverbDefinition &rhs)
    {
        EPI_UNUSED(rhs);
    }
    ReverbDefinition &operator=(ReverbDefinition &rhs)
    {
        EPI_UNUSED(rhs);
        return *this;
    }
};

class ReverbDefinitionContainer : public std::unordered_map<epi::EName, ReverbDefinition *, epi::ContainerENameHash>
{
  public:
    ReverbDefinitionContainer()
    {
    }
    ~ReverbDefinitionContainer()
    {
        for (std::unordered_map<epi::EName, ReverbDefinition *>::iterator iter = begin(), iter_end = end(); iter != iter_end; iter++)
        {
            ReverbDefinition *verb = iter->second;
            delete verb;
            verb = nullptr;
        }
    }

    public:
    // Search Functions
    ReverbDefinition *Lookup(std::string_view refname);
    ReverbDefinition *Lookup(epi::KnownEName ref_ename);
    ReverbDefinition *Lookup(epi::EName ref_ename);
};

extern ReverbDefinitionContainer reverbdefs;

void DDFReadReverbs(const std::string &data);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
