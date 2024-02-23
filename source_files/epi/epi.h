//----------------------------------------------------------------------------
//  EDGE Platform Interface Header
//----------------------------------------------------------------------------
//
//  Copyright (c) 2002-2024 The EDGE Team.
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

#pragma once

/* Important functions provided by Engine code */
#ifdef __GNUC__
void EDGEError(const char *error, ...) __attribute__((format(printf, 1, 2)));
void EDGEWarning(const char *warning, ...) __attribute__((format(printf, 1, 2)));
void EDGEPrintf(const char *message, ...) __attribute__((format(printf, 1, 2)));
void EDGEDebugf(const char *message, ...) __attribute__((format(printf, 1, 2)));
#else
void EDGEError(const char *error, ...);
void EDGEWarning(const char *warning, ...);
void EDGEPrintf(const char *message, ...);
void EDGEDebugf(const char *message, ...);
#endif

// basic macros

// Move these to dedicated EPI math file - Dasho
#include <math.h>
inline int RoundToInteger(float x) { return (int)roundf(x); }
inline int RoundToInteger(double x) { return (int)round(x); }

// assertion macro
#define SYS_ASSERT(cond)                                                   \
    ((cond) ? (void)0                                                      \
            : EDGEError("Assertion '%s' failed (%s:%d).\n", #cond, __FILE__, \
                      __LINE__))

//
// Z_Clear
//
// Clears memory to zero.
//
#include <string.h>
#define Z_Clear(ptr, type, num) \
    memset((void *)(ptr), ((ptr) - ((type *)(ptr))), (num) * sizeof(type))

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
