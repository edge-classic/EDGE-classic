//----------------------------------------------------------------------------
//  EDGE Option Menu (header)
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

#ifndef __M_NETGAME_H__
#define __M_NETGAME_H__

#include "e_event.h"

extern int netgame_menuon;  // 1 = HOST, 2 = JOIN, 3 = PLAYERS

void M_NetGameInit(void);
void M_NetGameDrawer(void);
void M_NetGameTicker(void);
bool M_NetGameResponder(event_t * ev, int ch);

#endif /* __M_NETGAME_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
