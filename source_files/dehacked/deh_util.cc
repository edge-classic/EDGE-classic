//------------------------------------------------------------------------
//  UTILITIES
//------------------------------------------------------------------------
//
//  DEH_EDGE  Copyright (C) 2004-2005  The EDGE Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License (in COPYING.txt) for more details.
//
//------------------------------------------------------------------------
//
//  DEH_EDGE is based on:
//
//  +  DeHackEd source code, by Greg Lewis.
//  -  DOOM source code (C) 1993-1996 id Software, Inc.
//  -  Linux DOOM Hack Editor, by Sam Lantinga.
//  -  PrBoom's DEH/BEX code, by Ty Halderman, TeamTNT.
//
//------------------------------------------------------------------------

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>

#include "deh_i_defs.h"
#include "deh_edge.h"

#include "deh_system.h"
#include "deh_util.h"


namespace Deh_Edge
{

//
// StrCaseCmp
//
int StrCaseCmp(const char *A, const char *B)
{
	for (; *A || *B; A++, B++)
	{
		// this test also catches end-of-string conditions
		if (toupper(*A) != toupper(*B))
			return (toupper(*A) - toupper(*B));
	}

	return 0;
}

//
// StrCaseCmpPartial
//
// Checks that the string B occurs at the front of string A.
// NOTE: This function is not symmetric, A can be longer than B and
// still match, but the match always fails if A is shorter than B.
//
int StrCaseCmpPartial(const char *A, const char *B)
{
	for (; *B; A++, B++)
	{
		// this test also catches end-of-string conditions
		if (toupper(*A) != toupper(*B))
			return (toupper(*A) - toupper(*B));
	}

	return 0;
}

//
// StrMaxCopy
//
void StrMaxCopy(char *dest, const char *src, int max)
{
	for (; *src && max > 0; max--)
	{
		*dest++ = *src++;
	}

	*dest = 0;
}

//
// StrUpper
//
const char *StrUpper(const char *name)
{
	static char up_buf[512];

	assert(strlen(name) < sizeof(up_buf) - 1);

	char *dest = up_buf;

	while (*name)
		*dest++ = toupper(*name++);

	*dest = 0;

	return up_buf;
}

//
// StringNew
//
char *StringNew(int length)
{
	char *s = (char *) calloc(length, 1);

	if (! s)
		FatalError("Out of memory (%d bytes for string)\n", length);
	
	return s;
}

//
// StringDup
//
char *StringDup(const char *orig)
{
	char *s = strdup(orig);

	if (! s)
		FatalError("Out of memory (copy string)\n");
	
	return s;
}

}  // Deh_Edge
