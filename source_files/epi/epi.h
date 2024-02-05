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

#ifndef __EDGE_PLATFORM_INTERFACE__
#define __EDGE_PLATFORM_INTERFACE__

#ifdef EDGE_WEB
#include <emscripten.h>
#endif

// standard C headers
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <string.h>
#include <ctype.h>
#include <math.h>

// common C++ headers
#include <string>
#include <vector>

// basic types
#include <stdint.h>

#ifdef __GNUC__
#define GCCATTR(xyz) __attribute__(xyz)
#else
#define GCCATTR(xyz) /* nothing */
#endif

#include "HandmadeMath.h"
#include "str_compare.h"

/* Important functions provided by Engine code */

void I_Error(const char *error, ...) GCCATTR((format(printf, 1, 2)));
void I_Warning(const char *warning, ...) GCCATTR((format(printf, 1, 2)));
void I_Printf(const char *message, ...) GCCATTR((format(printf, 1, 2)));
void I_Debugf(const char *message, ...) GCCATTR((format(printf, 1, 2)));

// basic macros

#ifndef NULL
#define NULL nullptr
#endif

#ifndef I_ROUND
#define I_ROUND(x) ((int)round(x))
#endif

// assertion macro
#define SYS_ASSERT(cond) ((cond) ? (void)0 : I_Error("Assertion '%s' failed (%s:%d).\n", #cond, __FILE__, __LINE__))

//
// Z_Clear
//
// Clears memory to zero.
//
#define Z_Clear(ptr, type, num) memset((void *)(ptr), ((ptr) - ((type *)(ptr))), (num) * sizeof(type))

//
// Z_StrNCpy
//
// Copies up to max characters of src into dest, and then applies a
// terminating zero (so dest must hold at least max+1 characters).
// The terminating zero is always applied (there is no reason not to)
//
#define Z_StrNCpy(dest, src, max) (void)(strncpy((dest), (src), (max)), (dest)[(max)] = 0)

#endif /* __EDGE_PLATFORM_INTERFACE__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
