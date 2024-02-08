//------------------------------------------------------------------------
//  UTILITIES
//------------------------------------------------------------------------
//
//  DEH_EDGE  Copyright (C) 2004-2024 The EDGE Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
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

#include "deh_edge.h"

#include "deh_system.h"
#include "deh_util.h"

namespace dehacked
{

//
// StrCaseCmp
//
int StrCaseCmp(const char *A, const char *B)
{
    for (; *A || *B; A++, B++)
    {
        // this test also catches end-of-string conditions
        if (epi::ToUpperASCII(*A) != epi::ToUpperASCII(*B))
            return (epi::ToUpperASCII(*A) - epi::ToUpperASCII(*B));
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
        if (epi::ToUpperASCII(*A) != epi::ToUpperASCII(*B))
            return (epi::ToUpperASCII(*A) - epi::ToUpperASCII(*B));
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

    SYS_ASSERT(strlen(name) < sizeof(up_buf) - 1);

    char *dest = up_buf;

    while (*name)
        *dest++ = epi::ToUpperASCII(*name++);

    *dest = 0;

    return up_buf;
}

//
// StrSanitize
//
const char *StrSanitize(const char *name)
{
    static char clean_buf[512];

    SYS_ASSERT(strlen(name) < sizeof(clean_buf) - 1);

    char *dest = clean_buf;

    while (*name)
    {
        char ch = *name++;
        if (epi::IsAlphanumericASCII(ch) || ch == '_')
            *dest++ = ch;
    }

    *dest = 0;

    return clean_buf;
}

//
// StringNew
//
char *StringNew(int length)
{
    char *s = (char *)calloc(length, 1);

    if (!s)
        I_Error("Dehacked: Error - Out of memory (%d bytes for string)\n", length);

    return s;
}

//
// StringDup
//
char *StringDup(const char *orig)
{
    char *s = strdup(orig);

    if (!s)
        I_Error("Dehacked: Error - Out of memory (copy string)\n");

    return s;
}

} // namespace dehacked
