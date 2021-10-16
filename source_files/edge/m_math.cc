//----------------------------------------------------------------------------
//  EDGE Floating Point Math Stuff
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

#include <math.h>

#include "m_math.h"

float M_Sin (angle_t ang)
{
	return (float) sin ((double) ang * M_PI / (float) ANG180);
}

float M_Cos (angle_t ang)
{
	return (float) cos ((double) ang * M_PI / (float) ANG180);
}

float M_Tan (angle_t ang)
{
	return (float) tan ((double) ang * M_PI / (float) ANG180);
}

angle_t M_ATan (float slope)
{
	return (angle_t) ((float) ANG180 * atan (slope) / M_PI);
}

void M_Angle2Matrix (angle_t ang, vec2_t * x, vec2_t * y)
{
	x->x = M_Cos (ang);
	x->y = M_Sin (ang);

    y->x = -x->y;
    y->y =  x->x;
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
