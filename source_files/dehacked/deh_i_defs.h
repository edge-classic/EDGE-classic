//----------------------------------------------------------------------------
//  System Specific Header
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

#ifndef __DEH_SYSTEM_SPECIFIC_DEFS__
#define __DEH_SYSTEM_SPECIFIC_DEFS__

// COMMON STUFF...
#define FLOAT_IEEE_754
namespace Deh_Edge
{
typedef unsigned char byte;
}

#include <cstddef>

// Windows
#if defined(WIN32) || defined(_WIN32) || defined(_WIN64)

#define STRICT
#ifndef _WINDOWS
#define _WINDOWS
#endif
#define WIN32_LEAN_AND_MEAN
#ifndef WIN32
#define WIN32
#endif

// Access() define values. Nicked from DJGPP's <unistd.h>
#ifndef R_OK
#define R_OK 0x02
#define W_OK 0x04
#endif

#endif

#endif /*__DEH_SYSTEM_SPECIFIC_DEFS__*/
