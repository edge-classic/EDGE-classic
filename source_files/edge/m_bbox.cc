//----------------------------------------------------------------------------
//  EDGE Bounding Box Code
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

#include "i_defs.h"

#include <float.h>

#include "m_bbox.h"

void M_ClearBox(float * box)
{
	box[BOXTOP]    = box[BOXRIGHT] = -FLT_MAX;
	box[BOXBOTTOM] = box[BOXLEFT]  =  FLT_MAX;
}

void M_AddToBox(float * box, float x, float y)
{
	if (x < box[BOXLEFT])
		box[BOXLEFT] = x;

	if (x > box[BOXRIGHT])
		box[BOXRIGHT] = x;

	if (y < box[BOXBOTTOM])
		box[BOXBOTTOM] = y;

	if (y > box[BOXTOP])
		box[BOXTOP] = y;
}

void M_CopyBox(float * box, float * other)
{
	box[BOXLEFT]   = other[BOXLEFT];
	box[BOXRIGHT]  = other[BOXRIGHT];
	box[BOXTOP]    = other[BOXTOP];
	box[BOXBOTTOM] = other[BOXBOTTOM];
}

void M_UnionBox(float * box, float * other)
{
	if (other[BOXLEFT] < box[BOXLEFT])
		box[BOXLEFT] = other[BOXLEFT];

	if (other[BOXRIGHT] > box[BOXRIGHT])
		box[BOXRIGHT] = other[BOXRIGHT];

	if (other[BOXBOTTOM] < box[BOXBOTTOM])
		box[BOXBOTTOM] = other[BOXBOTTOM];

	if (other[BOXTOP] > box[BOXTOP])
		box[BOXTOP] = other[BOXTOP];
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
