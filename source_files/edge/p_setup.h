//----------------------------------------------------------------------------
//  EDGE Level Loading/Setup Code
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

#ifndef __P_SETUP__
#define __P_SETUP__

#include "dm_defs.h"

#include "math_crc.h"

extern epi::crc32_c mapsector_CRC;
extern epi::crc32_c mapline_CRC;
extern epi::crc32_c mapthing_CRC;

extern int mapthing_NUM;

// Called by startup code.
void P_Init(void);

void P_SetupLevel(void);

void P_Shutdown(void);

#endif /* __P_SETUP__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
