//----------------------------------------------------------------------------
//  EDGE Bounding-box Code
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

#ifndef __M_BBOX_H__
#define __M_BBOX_H__

#include <limits.h>


// Bounding box coordinate storage.
enum
{
  BOXTOP,
  BOXBOTTOM,
  BOXLEFT,
  BOXRIGHT
};  // bbox coordinates

// Bounding box functions.
void M_ClearBox(float * box);
void M_AddToBox(float * box, float x, float y);
void M_CopyBox(float * box, float * other);
void M_UnionBox(float * box, float * other);

#endif /* __M_BBOX_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
