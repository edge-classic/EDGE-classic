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

#ifndef __AJBSP_BSP_H__
#define __AJBSP_BSP_H__

#include "epi.h"
#include "AlmostEquals.h"

constexpr char *kAJBSPVersion = "1.04";

//
// Node Build Information Structure
//

constexpr uint8_t kSplitCostDefault = 11;

struct BuildInfo
{
    // use a faster method to pick nodes
    bool fast;

    // create GL Nodes?
    bool gl_nodes;

    bool force_v5;
    bool force_xnod;
    bool force_compress;

    int split_cost;

    // this affects how some messages are shown
    int verbosity;

    // from here on, various bits of internal state
    int total_warnings;
    int total_minor_issues;
};

enum BuildResult
{
    // everything went peachy keen
    kBuildOK = 0,

    // when saving the map, one or more lumps overflowed
    kBuildLumpOverflow
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

// retrieve the name of a particular level.
const char *GetLevelName(int lev_idx);

// build the nodes of a particular level.  if cancelled, returns the
// BUILD_Cancelled result and the wad is unchanged.  otherwise the wad
// is updated to store the new lumps and returns either kBuildOK or
// kBuildLumpOverflow if some limits were exceeded.
BuildResult BuildLevel(int lev_idx);

} // namespace ajbsp

#endif /* __AJBSP_BSP_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
