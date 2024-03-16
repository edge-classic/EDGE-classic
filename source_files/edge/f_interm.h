//----------------------------------------------------------------------------
//  EDGE Intermission Screen Code
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
//
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------

#pragma once

#include "level.h"

enum IntermissionState
{
    kIntermissionStateNone = -1,
    kIntermissionStateStatScreen,
    kIntermissionStateShowNextLocation
};

struct IntermissionInfo
{
    const char *level; // episode # (0-2)

    const MapDefinition *current_level;
    const MapDefinition *next_level;

    int kills;
    int items;
    int secrets;

    int par_time;
};

extern IntermissionInfo intermission_stats;

// Called by main loop, animate the intermission.
void IntermissionTicker(void);

// Called by main loop,
// draws the intermission directly into the screen buffer.
void IntermissionDrawer(void);

// Setup for an intermission screen.
void IntermissionStart(void);

// Clear Intermission Data
void IntermissionClear(void);

bool IntermissionCheckForAccelerate(void);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
