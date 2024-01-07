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

#ifndef __R_WIPE__
#define __R_WIPE__

//
// SCREEN WIPE PACKAGE
//

typedef enum
{
    // no wiping
    WIPE_None,
    // weird screen melt
    WIPE_Melt,
    // cross-fading
    WIPE_Crossfade,
    // pixel fading
    WIPE_Pixelfade,

    // new screen simply scrolls in from the given side of the screen
    // (or if reversed, the old one scrolls out to the given side)
    WIPE_Top,
    WIPE_Bottom,
    WIPE_Left,
    WIPE_Right,

    WIPE_Spooky,

    // Opens like doors
    WIPE_Doors,

    WIPE_NUMWIPES
} wipetype_e;

extern wipetype_e wipe_method;

// for enum cvars
extern const char WIPE_EnumStr[];

void RGL_InitWipe(wipetype_e effect);
void RGL_StopWipe(void);
bool RGL_DoWipe(void);
// Primarily for movie use; replaces the initial wipe texture with all black
void RGL_BlackoutWipeTex(void);

#endif // __R_WIPE__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
