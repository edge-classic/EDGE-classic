//----------------------------------------------------------------------------
//  EDGE Option Menu (header)
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

#include "e_event.h"

extern int network_game_menu_on; // 1 = HOST, 2 = JOIN, 3 = PLAYERS

void NetworkGameInitialize(void);
void NetworkGameDrawer(void);
void NetworkGameTicker(void);
bool NetworkGameResponder(InputEvent *ev, int ch);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
