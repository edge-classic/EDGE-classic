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


//
// Z_StrDup
//
// Duplicates the string.
char *Z_StrDup(const char *s)
{
	int size;
	char *ret;

	if (s == NULL)
		return NULL;

	size = strlen(s) + 1;

	ret = Z_New(char, size);

	Z_MoveData(ret, s, char, size);

	return ret;
}
#ifdef DEVELOPERS
typedef struct mallocheader_s
{
	int size;
} mallocheader_t;

#define CHECK_PTR(h) SYS_ASSERT_MSG((*(int *)((char *)(h) + (h)->size + sizeof(mallocheader_t)) == ZONEID), ("Block without ZONEID"))
#endif

//
// Z_Free
//
void Z_Free(void *ptr)
{
	// -ES- FIXME: Decide whether we should allow Z_Free(NULL) and Z_Malloc(0)
	if (ptr == NULL)
		return;
#ifdef DEVELOPERS
	{
		mallocheader_t *h = (mallocheader_t *)ptr - 1;
		CHECK_PTR(h);
#ifdef INTOLERANT_MATH
		// -ES- 2000/04/08 Trash all data.
		memset(h, -1, h->size + sizeof(mallocheader_t) + sizeof(int));
#endif
		free(h);
	}
#else
	free(ptr);
#endif
}


//
// Z_ReMalloc
//
// Zone reallocation.
//
void *Z_ReMalloc2(void *ptr, int size)
{
#if defined INTOLERANT_MATH && defined DEVELOPERS
	// The intolerant remalloc will always trash the old memory block with -1:s,
	// and also trash anything undefined in the new block.
	void *newp;
	mallocheader_t *h;

	if (size == 0)
	{
		if (ptr)
			Z_Free(ptr);
		return NULL;
	}

	if (ptr == NULL)
		return Z_Malloc(size);

	// All trashing of undefined data will be done by Z_Malloc/Z_Free.
	newp = Z_Malloc(size);
	h = (mallocheader_t *)ptr - 1;
	CHECK_PTR(h);
	memmove(newp, ptr, MIN(size, h->size));
	Z_Free(ptr);

	return newp;
#else

#ifdef DEVELOPERS
	mallocheader_t *h, *newp;
#else
	void *h, *newp;
#endif
	int allocsize;

	if (size == 0)
	{
		if (ptr)
			Z_Free(ptr);

		return NULL;
	}

#ifdef DEVELOPERS
	if (ptr == NULL)
	{
		h = NULL;
	}
	else
	{
		h = (mallocheader_t *)ptr - 1;
		CHECK_PTR(h);
	}
	allocsize = size + sizeof(mallocheader_t) + sizeof(int);
#else
	h = ptr;
	allocsize = size;
#endif

#ifdef DEVELOPERS
	while (NULL == (newp = (mallocheader_t*)realloc(h, allocsize)))
#else
	while (NULL == (newp = realloc(h, allocsize)))
#endif
	{
		I_Error("Z_ReMalloc: failed on allocation of %i bytes", size);
	}

#ifdef DEVELOPERS
	newp->size = size;
	newp++;
	*(int *)((char *)newp + size) = ZONEID;
#endif
	return newp;
#endif
}

//
// Z_Malloc
//
// Zone allocation.

void *Z_Malloc2(int size)
{
#ifdef DEVELOPERS
	mallocheader_t *p;
#else
	void *p;
#endif
	int allocsize;

	if (size == 0)
		return NULL;

#ifdef DEVELOPERS
	allocsize = sizeof(mallocheader_t) + size + sizeof(int);
#else
	allocsize = size;
#endif

#ifdef DEVELOPERS
	while (NULL == (p = (mallocheader_t*)malloc(allocsize)))
#else
	while (NULL == (p = malloc(allocsize)))
#endif
	{
		I_Error("Z_Malloc: failed on allocation of %i bytes", size);
	}
#ifdef DEVELOPERS
	p->size = size;
	p++;
	*(int *)((char *)p + size) = ZONEID;
#ifdef INTOLERANT_MATH
	// -ES- 2000/03/28 Will turn all floats into -NaN, making uninitialised
	// elements easier to find.
	memset(p, -1, size);
#endif
#endif
	return p;
}


//
// Z_Init
//
void Z_Init(void)
{
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
