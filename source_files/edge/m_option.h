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

#ifndef __M_OPTION__
#define __M_OPTION__

#include "e_event.h"

extern int option_menuon;

void M_OptMenuInit(void);
void M_OptDrawer(void);
void M_OptTicker(void);
bool M_OptResponder(event_t * ev, int ch);
void M_OptCheckNetgame(void);

#endif // __M_OPTION__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
