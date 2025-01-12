//----------------------------------------------------------------------------
//  EDGE Resolution Handling
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
//
// Original Author: Chi Hoang
//

#pragma once

#include "con_var.h"

enum WindowMode
{
    kWindowModeInvalid = -1,
    kWindowModeWindowed,
    kWindowModeBorderless
};

// Screen mode information
struct DisplayMode
{
    int        width       = 0;
    int        height      = 0;
    int        depth       = 0;
    WindowMode window_mode = kWindowModeWindowed;
};

// Exported Vars
extern int                        current_screen_width;
extern int                        current_screen_height;
extern int                        current_screen_depth;
extern WindowMode                 current_window_mode;
extern DisplayMode                borderless_mode;
extern std::vector<DisplayMode *> screen_modes;
// CVARs related to Alt+Enter toggling
extern ConsoleVariable toggle_windowed_width;
extern ConsoleVariable toggle_windowed_height;
extern ConsoleVariable toggle_windowed_depth;
extern ConsoleVariable toggle_windowed_window_mode;

// Exported Func
bool EquivalentDisplayDepth(int depth1, int depth2);

void AddDisplayResolution(DisplayMode *mode);
void DumpResolutionList(void);

enum ResolutionIncrement
{
    kIncrementSize = 0,
    kIncrementWindowMode,
};

bool IncrementResolution(DisplayMode *mode, int what, int dir);
// update the given screen mode with the next highest (dir=1)
// or next lowest (dir=-1) attribute given by 'what' parameter,
// either the size, depth or fullscreen/windowed mode.  Returns
// true on succses.  If no such resolution exists (whether or
// not the given mode exists) then false is returned.

void ToggleFullscreen(void);

void SetInitialResolution(void);
bool ChangeResolution(DisplayMode *mode);

void SoftInitializeResolution(void);

// only call these when it really is time to do the actual resolution
// or view size change, i.e. at the start of a frame.

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
