//----------------------------------------------------------------------------
//  EDGE Data Definition File Codes (Flat properties)
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
#include "epi.h"

class FlatDefinition
{
  public:
    FlatDefinition();
    ~FlatDefinition() {};

  public:
    void Default(void);
    void CopyDetail(const FlatDefinition &src);

    // Member vars....
    std::string name_;

    std::string liquid_; // Values are "THIN" and "THICK" - determines swirl
                         // and shader params - Dasho

    struct SoundEffect *footstep_;
    std::string         splash_;
    // Lobo: item to spawn (or nullptr).  The mobjdef pointer is only valid
    // after
    //  DDFflatCleanUp() has been called.
    const MapObjectDefinition *impactobject_;
    std::string                impactobject_ref_;

    const MapObjectDefinition *glowobject_;
    std::string                glowobject_ref_;

    float sink_depth_;
    float bob_depth_;

  private:
    // disable copy construct and assignment operator
    explicit FlatDefinition(FlatDefinition &rhs)
    {
        EPI_UNUSED(rhs);
    }
    FlatDefinition &operator=(FlatDefinition &rhs)
    {
        EPI_UNUSED(rhs);
        return *this;
    }
};

// Our flatdefs container
class FlatDefinitionContainer : public std::vector<FlatDefinition *>
{
  public:
    FlatDefinitionContainer()
    {
    }
    ~FlatDefinitionContainer()
    {
        for (std::vector<FlatDefinition *>::iterator iter = begin(), iter_end = end(); iter != iter_end; iter++)
        {
            FlatDefinition *flt = *iter;
            delete flt;
            flt = nullptr;
        }
    }

  public:
    FlatDefinition *Find(const char *name);
};

extern FlatDefinitionContainer flatdefs; // -DASHO- 2022 Implemented

void DDFReadFlat(const std::string &data);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
