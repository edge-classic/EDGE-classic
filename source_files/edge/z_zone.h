//----------------------------------------------------------------------------
//  EDGE Zone Memory Allocation Code 
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2008  The EDGE Team.
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

#ifndef __Z_ZONE__
#define __Z_ZONE__

#define ZONEID  0x1d4a11f1

typedef enum
{
	Z_UrgencyNone    = 0,
	Z_UrgencyLow     = 1,
	Z_UrgencyMedium  = 2,
	Z_UrgencyHigh    = 3,
	Z_UrgencyExtreme = 4
}
z_urgency_e;

// A cache flusher is a function that can find and free unused memory.
typedef void cache_flusher_f(z_urgency_e urge);

// Generic helper functions.
char *Z_StrDup(const char *s);

// Memory handling functions.
void Z_RegisterCacheFlusher(cache_flusher_f *f);
void Z_Init(void);
void *Z_Malloc2(int size);
void *Z_ReMalloc2(void *ptr, int size);
void Z_Free(void *ptr);

#define Z_Malloc Z_Malloc2
#define Z_ReMalloc Z_ReMalloc2

//
// Z_New
//
// Allocates num elements of type. Use this instead of Z_Malloc whenever
// possible.
//
#define Z_New(type, num) ((type *) Z_Malloc((num) * sizeof(type)))

//
// Z_Resize
//
// Reallocates a block. Use instead of Z_ReMalloc wherever possible.
// Unlike normal Z_ReMalloc, the pointer parameter is assigned the new
// value, and there is no return value.
//
#define Z_Resize(ptr,type,n)  \
	(void)((ptr) = (type *) Z_ReMalloc((void *)(ptr), (n) * sizeof(type)))


#endif  /* __Z_ZONE__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
