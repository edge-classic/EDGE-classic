//----------------------------------------------------------------------------
//  EDGE Zone Memory Allocation Code 
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

#include "i_defs.h"

#include "utility.h"

#include "z_zone.h"


void Z_Free(void *ptr)
{
	free(ptr);
}


void *Z_Malloc(int size)
{
	if (size == 0)
		return NULL;

	void *p = malloc(size);

	if (p == NULL)
		I_Error("Z_Malloc: failed on allocation of %i bytes", size);

	return p;
}


void Z_Init(void)
{
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
