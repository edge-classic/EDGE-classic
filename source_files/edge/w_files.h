//----------------------------------------------------------------------------
//  EDGE file handling
//----------------------------------------------------------------------------
//
//  Copyright (c) 2022  The EDGE Team.
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

#ifndef __W_FILES__
#define __W_FILES__

#include "dm_defs.h"

#include "file.h"
#include "utility.h"

typedef enum
{
	FLKIND_IWad = 0,  // iwad file
	FLKIND_PWad,      // normal .wad file
	FLKIND_EWad,      // EDGE.wad
	FLKIND_GWad,      // ajbsp node wad
	FLKIND_HWad,      // deHacked wad

	FLKIND_Lump,      // raw lump (no extension)

	FLKIND_DDF,       // .ddf or .ldf file
	FLKIND_RTS,       // .rts script
	FLKIND_Deh        // .deh or .bex file
}
filekind_e;

// TODO

#endif // __W_FILES__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
