//----------------------------------------------------------------------------
//  EDGE OpenGL Rendering (Things)
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

#include "e_player.h"
#include "r_defs.h"
#include "r_gldefs.h"

void RendererWalkThing(DrawSubsector *dsub, MapObject *mo);
void RendererDrawSortThings(DrawFloor *dfloor);

void RendererDrawWeaponSprites(player_t *p);
void RendererDrawWeaponModel(player_t *p);
void RendererDrawCrosshair(player_t *p);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
