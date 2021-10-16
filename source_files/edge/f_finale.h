//----------------------------------------------------------------------------
//  EDGE Finale Definition
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

#ifndef __F_FINALE__
#define __F_FINALE__

#include "e_event.h"
#include "g_game.h"
#include "p_mobj.h"

// Called by main loop.
bool F_Responder(event_t * ev);

// Called by main loop.
void F_Ticker(void);

// Called by main loop.
void F_Drawer(void);

// -KM- 1998/11/25 Finales generalised.
void F_StartFinale(const map_finaledef_c * f, gameaction_e newaction);

#endif // __F_FINALE__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
