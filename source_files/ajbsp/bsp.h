//------------------------------------------------------------------------
//
//  AJ-BSP  Copyright (C) 2000-2023  Andrew Apted, et al
//          Copyright (C) 1994-1998  Colin Reed
//          Copyright (C) 1997-1998  Lee Killough
//
//  Originally based on the program 'BSP', version 2.3.
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
//------------------------------------------------------------------------

#pragma once

#include <stdint.h>

#include <string>

#include "AlmostEquals.h"
#include "epi.h"
//
// Node Build Information Structure
//

constexpr uint8_t kSplitCostDefault = 11;

struct BuildInfo
{
    bool compress_nodes;

    int split_cost;

    // from here on, various bits of internal state
    int total_warnings;
    int total_minor_issues;
};

enum BuildResult
{
    // everything went peachy keen
    kBuildOK = 0,

    // not used at the moment, I think we just throw FatalError if needed -
    // Dasho
    kBuildError
};

namespace ajbsp
{

// set the build information.  must be done before anything else.
void ResetInfo();

// attempt to open a wad.  on failure, the FatalError method in the
// BuildInfo interface is called.
void OpenWad(std::string filename);

// attempt to open a wad from memory; only intended for the use
// of WAD files inside archives
void OpenMem(std::string filename, uint8_t *raw_wad, int raw_length);

// close a previously opened wad.
void CloseWad();

// create/finish an XWA file
void CreateXWA(std::string filename);
void FinishXWA();

// give the number of levels detected in the wad.
int LevelsInWad();

// build the nodes of a particular level.  if cancelled, returns the
// BUILD_Cancelled result and the wad is unchanged.  otherwise the wad
// is updated to store the new lumps and returns either kBuildOK or
// kBuildError
BuildResult BuildLevel(int level_index);

}  // namespace ajbsp

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
