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

#include <string>
#include <vector>

#include "ddf_types.h"

//-------------------------------------------------------------------------
//-----------------------  THING STATE STUFF   ----------------------------
//-------------------------------------------------------------------------

enum StateFrameFlag
{
    kStateFrameFlagWeapon     = (1 << 0),
    kStateFrameFlagModel      = (1 << 1),
    kStateFrameFlagUnmapped   = (1 << 2), // model_frame not yet looked up
    kStateFrameFlagFast       = (1 << 3)  // MBF21: Specific frame is twice as fast on Nightmare 
};

struct State
{
    // sprite ref
    short sprite;

    // frame ref (begins at 0)
    short frame;

    // brightness (0 to 255)
    short bright;

    short flags;

    // duration in tics
    int tics;

    // model frame name like "run2", normally NULL
    const char *model_frame;

    // label for state, or nullptr
    const char *label;

    // routine to be performed
    void (*action)(class MapObject *object);

    // parameter for routine, or nullptr
    void *action_par;

    int rts_tag_type;

    // next state ref.  0 means "remove me"
    int nextstate;

    // jump state ref.  0 not valid
    int jumpstate;
};

// -------EXTERNALISATIONS-------

extern State *states;
extern int    num_states;

extern std::vector<std::string> ddf_sprite_names;
extern std::vector<std::string> ddf_model_names;

int DDFStateFindLabel(const std::vector<StateRange> &group, const char *label, bool quiet = false);

bool DDFStateGroupHasState(const std::vector<StateRange> &group, int st);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
