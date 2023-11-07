//----------------------------------------------------------------------------
//  EDGE RNG
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2023  The EDGE Team.
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

#ifndef __M_RANDOM__
#define __M_RANDOM__

#include "types.h"

void M_Random_Init(void);
int  M_Random(void);
int  M_RandomNegPos(void);
int  P_Random(void);
int  C_Random(void);
int  P_RandomNegPos(void);
bool M_RandomTest(percent_t chance);
bool P_RandomTest(percent_t chance);

// Savegame support
int  P_ReadRandomState(void);
void P_WriteRandomState(int value);

#endif // __M_RANDOM__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
