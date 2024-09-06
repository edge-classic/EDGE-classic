//----------------------------------------------------------------------------
//  EDGE Thinker & Ticker Code
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

// Called by C_Ticker,
// can call G_PlayerExited.
// Carries out all thinking of monsters and players.
void MapObjectTicker(void);

void HubFastForward(void);

// Needed to pause flat anims, etc when not moving or firing in Erraticism
extern bool erraticism_active;

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
