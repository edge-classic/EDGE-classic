//----------------------------------------------------------------------------
//  EDGE Automap Functions
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2009  The EDGE Team.
// 
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
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

#ifndef __AMMAP_H__
#define __AMMAP_H__

#include "types.h"

#include "dm_defs.h"
#include "e_event.h"
#include "p_mobj.h"

extern bool automapactive;
extern bool rotatemap;
extern bool am_keydoorblink;

//
// Automap drawing structs
//
typedef struct
{
	float x, y;
}
mpoint_t;

typedef struct
{
	mpoint_t a, b;
}
mline_t;

void AM_InitLevel(void);

// Called by main loop.
bool AM_Responder(event_t * ev);

// Called by main loop.
void AM_Ticker(void);

// Called to draw the automap on the screen.
void AM_Render(float x, float y, float w, float h, mobj_t *focus, int flags);

// Called to force the automap to quit
// if the level is completed while it is up.
void AM_Stop(void);


// color setting API

// NOTE: these numbers here must match the COAL API script
typedef enum
{
	AMCOL_Grid = 0,

    AMCOL_Allmap,
    AMCOL_Wall,
    AMCOL_Step,
    AMCOL_Ledge,
    AMCOL_Ceil,
    AMCOL_Secret,

    AMCOL_Player,
    AMCOL_Monster,
    AMCOL_Corpse,
    AMCOL_Item,
    AMCOL_Missile,
    AMCOL_Scenery,

	AM_NUM_COLORS
}
automap_color_e;

void AM_SetColor(int which, rgbcol_t color);


// NOTE: the bit numbers here must match the COAL API script
typedef enum
{
	AMST_Grid    = (1 << 0),  // draw the grid
	AMST_Follow  = (1 << 4),  // follow the player
	AMST_Rotate  = (1 << 5),  // rotate the map (disables grid)
	AMST_HideLines  = (1 << 6),  // turn off all line drawing

	AMST_Things  = (1 << 3),  // draw all objects
	AMST_Walls   = (1 << 2),  // draw all walls (like IDDT)
	AMST_Allmap  = (1 << 1),  // draw like Allmap powerup
}
automap_state_e;

void AM_GetState(int *state, float *zoom);
void AM_SetState(int  state, float  zoom);

#endif /* __AMMAP_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
