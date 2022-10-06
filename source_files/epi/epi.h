//----------------------------------------------------------------------------
//  EDGE Platform Interface Header
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

#ifndef __EDGE_PLATFORM_INTERFACE__
#define __EDGE_PLATFORM_INTERFACE__

#if defined(WIN32) || defined(_WIN32) || defined(_WIN64)
  #define _WINDOWS
  #define WIN32_LEAN_AND_MEAN
  #ifndef WIN32
  #define WIN32
  #endif

  #include <windows.h>

  #undef min
  #undef max
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
#include <cstdint>

typedef std::int8_t   s8_t;
typedef std::int16_t  s16_t;
typedef std::int32_t  s32_t;
typedef std::int64_t  s64_t;

typedef std::uint8_t   u8_t;
typedef std::uint16_t  u16_t;
typedef std::uint32_t  u32_t;
typedef std::uint64_t  u64_t;

typedef u8_t byte;


#ifdef GNUC
#define GCCATTR(xyz) attribute (xyz)
#else
#define GCCATTR(xyz) /* nothing */
#endif


// string comparisons
#include "str_compare.h"


/* Important functions provided by Engine code */

void I_Error(const char *error,...) GCCATTR((format(printf, 1, 2)));
void I_Warning(const char *warning,...) GCCATTR((format(printf, 1, 2)));
void I_Printf(const char *message,...) GCCATTR((format(printf, 1, 2)));
void I_Debugf(const char *message,...) GCCATTR((format(printf, 1, 2)));


// basic macros

#ifndef NULL
#define NULL  ((void*) 0)
#endif

#ifndef M_PI
#define M_PI  3.14159265358979323846
#endif

// TODO replace these with std::max() and std::min()
#ifndef MAX
#define MAX(a,b)  ((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b)  ((a) < (b) ? (a) : (b))
#endif

// TODO replace this with plain abs() for integer, fabs() for float
#ifndef ABS
#define ABS(a)  ((a) < 0 ? -(a) : (a))
#endif

#ifndef I_ROUND
#define I_ROUND(x)  ((int) round(x))
#endif

#ifndef CLAMP
#define CLAMP(low,x,high)  ((x) < (low) ? (low) : (x) > (high) ? (high) : (x))
#endif


// assertion macro
#ifdef NDEBUG
#define SYS_ASSERT(cond)  ((void) 0)
#else
#define SYS_ASSERT(cond)  \
	((cond) ? (void)0 : I_Error("Assertion '%s' failed (%s:%d).\n", #cond , __FILE__ , __LINE__ ))
#endif // NDEBUG


//
// Z_Clear
//
// Clears memory to zero.
//
#define Z_Clear(ptr, type, num)  \
	memset((void *)(ptr), ((ptr) - ((type *)(ptr))), (num) * sizeof(type))


//
// Z_StrNCpy
//
// Copies up to max characters of src into dest, and then applies a
// terminating zero (so dest must hold at least max+1 characters).
// The terminating zero is always applied (there is no reason not to)
//
#define Z_StrNCpy(dest, src, max) \
	(void)(strncpy((dest), (src), (max)), (dest)[(max)] = 0)


#endif /* __EDGE_PLATFORM_INTERFACE__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
