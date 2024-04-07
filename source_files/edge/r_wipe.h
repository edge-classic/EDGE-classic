//----------------------------------------------------------------------------
//  EDGE OpenGL Rendering (Definitions)
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
// DESCRIPTION:
//   Mission start screen wipe/melt, special effects.
//

#pragma once

//
// SCREEN WIPE PACKAGE
//

enum ScreenWipe
{
    // no wiping
    kScreenWipeNone,
    // weird screen melt
    kScreenWipeMelt,
    // cross-fading
    kScreenWipeCrossfade,
    // pixel fading
    kScreenWipePixelfade,

    // new screen simply scrolls in from the given side of the screen
    // (or if reversed, the old one scrolls out to the given side)
    kScreenWipeTop,
    kScreenWipeBottom,
    kScreenWipeLeft,
    kScreenWipeRight,

    kScreenWipeSpooky,

    // Opens like doors
    kScreenWipeDoors,

    kTotalScreenWipeTypes
};

extern ScreenWipe wipe_method;

// for enum cvars
extern const char kScreenWipeEnumStr[];

void InitializeWipe(ScreenWipe effect);
void StopWipe(void);
bool DoWipe(void);
// Primarily for movie use; replaces the initial wipe texture with all black
void BlackoutWipeTexture(void);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
