//------------------------------------------------------------------------
//  UTILITIES
//------------------------------------------------------------------------
//
//  Copyright (C) 2001-2023 Andrew Apted
//  Copyright (C) 1997-2003 Andr� Majorel et al
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
//------------------------------------------------------------------------

#include "bsp_local.h"
#include "bsp_utility.h"

namespace ajbsp
{

//------------------------------------------------------------------------
// STRINGS
//------------------------------------------------------------------------

char *StringNew(int length)
{
    // length does not include the trailing NUL.

    char *s = (char *)calloc(length + 1, 1);

    if (!s)
        I_Error("AJBSP: Out of memory (%d bytes for string)\n", length);

    return s;
}

char *StringDup(const char *orig, int limit)
{
    if (!orig)
        return NULL;

    if (limit < 0)
    {
        char *s = strdup(orig);

        if (!s)
            I_Error("AJBSP: Out of memory (copy string)\n");

        return s;
    }

    char *s = StringNew(limit + 1);
    strncpy(s, orig, limit);
    s[limit] = 0;

    return s;
}

char *StringUpper(const char *name)
{
    char *copy = StringDup(name);

    for (char *p = copy; *p; p++)
        *p = toupper(*p);

    return copy;
}

void StringFree(const char *str)
{
    if (str)
    {
        free((void *)str);
    }
}

//------------------------------------------------------------------------
// MEMORY ALLOCATION
//------------------------------------------------------------------------

//
// Allocate memory with error checking.  Zeros the memory.
//
void *UtilCalloc(int size)
{
    void *ret = calloc(1, size);

    if (!ret)
        I_Error("AJBSP: Out of memory (cannot allocate %d bytes)\n", size);

    return ret;
}

//
// Reallocate memory with error checking.
//
void *UtilRealloc(void *old, int size)
{
    void *ret = realloc(old, size);

    if (!ret)
        I_Error("AJBSP: Out of memory (cannot reallocate %d bytes)\n", size);

    return ret;
}

//
// Free the memory with error checking.
//
void UtilFree(void *data)
{
    if (data == NULL)
        I_Error("AJBSP: Trying to free a NULL pointer\n");

    free(data);
}

//------------------------------------------------------------------------
// MATH STUFF
//------------------------------------------------------------------------

//
// Compute angle of line from (0,0) to (dx,dy).
// Result is degrees, where 0 is east and 90 is north.
//
double ComputeAngle(double dx, double dy)
{
    double angle;

    if (AlmostEquals(dx, 0.0))
        return (dy > 0) ? 90.0 : 270.0;

    angle = atan2((double)dy, (double)dx) * 180.0 / HMM_PI;

    if (angle < 0)
        angle += 360.0;

    return angle;
}

} // namespace ajbsp

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
