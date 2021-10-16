//----------------------------------------------------------------------------
//  EDGE Cheat Sequence Checking
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

#ifndef __M_CHEAT__
#define __M_CHEAT__

// -MH- 1998/06/17 for cheat to give jetpack
// -KM- 1998-07-21 Added some extra headers in here
#include "dm_defs.h"
#include "dm_state.h"
#include "e_event.h"

//
// CHEAT SEQUENCE PACKAGE
//
// -KM- 1998/07/21 Needed in am_map.c (iddt cheat)
typedef struct
{
	const char *sequence;
	const char *p;
}
cheatseq_t;

int M_CheckCheat(cheatseq_t * cht, char key);
bool M_CheatResponder(event_t * ev);
void M_CheatInit(void);

#endif // __M_CHEAT__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
