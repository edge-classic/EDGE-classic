//----------------------------------------------------------------------------
//  EDGE OpenGL Rendering (Things)
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

#ifndef __RGL_THING__
#define __RGL_THING__

#include "r_defs.h"
#include "r_gldefs.h"
#include "w_sprite.h"

void RGL_WalkThing(drawsub_c *dsub, mobj_t *mo);
void RGL_DrawSortThings(drawfloor_t *dfloor);

void RGL_DrawWeaponSprites(player_t * p);
void RGL_DrawWeaponModel(player_t * p);
void RGL_DrawCrosshair(player_t * p);

#endif  /* __RGL_THING__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
