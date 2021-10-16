//----------------------------------------------------------------------------
//  EDGE Refresh Local Header
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
//
// DESCRIPTION:
//   Refresh (R_*) module, global header.
//   All the rendering/drawing stuff is here.
//

#ifndef __R_LOCAL__
#define __R_LOCAL__

// Binary Angles, sine/cosine/atan lookups.
#include "m_math.h"

// Screen size related parameters.
#include "dm_defs.h"

// Include the refresh/render data structs.
#include "w_flat.h"

//
// Separate header file for each module.
//
#include "r_misc.h"
#include "r_bsp.h"
#include "r_things.h"

#endif // __R_LOCAL__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
