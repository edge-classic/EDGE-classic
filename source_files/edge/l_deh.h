//----------------------------------------------------------------------------
//  EDGE DEH Interface
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

#ifndef __L_DEH__
#define __L_DEH__

class deh_container_c;

deh_container_c * DH_ConvertFile(const char *filename);
deh_container_c * DH_ConvertLump(const byte *data, int length, const char *lumpname);

#endif  // __L_DEH__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
