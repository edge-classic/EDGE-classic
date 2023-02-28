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

#ifdef __APPLE__
#include <cmath>
#endif


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
	return (s32_t)((float)ANG180 * atan(slope) / M_PI); // Updated M_ATan from EDGE 2.x branch, works properly with MSVC now
}

void M_Angle2Matrix (angle_t ang, vec2_t * x, vec2_t * y)
{
	x->x = M_Cos (ang);
	x->y = M_Sin (ang);

    y->x = -x->y;
    y->y =  x->x;
}

static vec3_t M_CrossProduct(vec3_t v1, vec3_t v2, vec3_t v3)
{
	vec3_t A{v2.x-v1.x, v2.y-v1.y, v2.z-v1.z};
	vec3_t B{v3.x-v1.x, v3.y-v1.y, v3.z-v1.z};
	float x = (A.y * B.z) - (A.z * B.y);
	float y = (A.z * B.x) - (A.x * B.z);
	float z = (A.x * B.y) - (A.y * B.x);
	return {x,y,z};
}

static vec3_t M_CrossProduct(vec3_t v1, vec3_t v2)
{
	float x = (v1.y * v2.z) - (v1.z * v2.y);
	float y = (v1.z * v2.x) - (v1.x * v2.z);
	float z = (v1.x * v2.y) - (v1.y * v2.x);
	return {x,y,z};
}

static float M_DotProduct(vec3_t v1, vec3_t v2)
{
	return (v1.x*v2.x) + (v1.y*v2.y) + (v1.z*v2.z);
}

vec3_t M_LinePlaneIntersection(vec3_t line_a, vec3_t line_b,
	vec3_t plane_a, vec3_t plane_b, vec3_t plane_c)
{
	vec3_t plane_normal = M_CrossProduct(plane_a, plane_b, plane_c);
	float n = M_DotProduct(plane_normal, {plane_c.x-line_a.x,plane_c.y-line_a.y,plane_c.z-line_a.z});
	vec3_t line_subtract{line_b.x-line_a.x,line_b.y-line_a.y,line_b.z-line_a.z};
	float d = M_DotProduct(plane_normal, line_subtract);
	float u = n/d;
	return{line_a.x+u*line_subtract.x, line_a.y+u*line_subtract.y, line_a.z+u*line_subtract.z};
}

double M_PointToSegDistance(vec2_t seg_a, vec2_t seg_b, vec2_t point)
{
 
    vec2_t seg_ab;
	seg_ab.x = seg_b.x - seg_a.x;
    seg_ab.y = seg_b.y - seg_a.y;
 
    vec2_t seg_bp;
	seg_bp.x = point.x - seg_b.x;
	seg_bp.y = point.y - seg_b.y;
 
    vec2_t seg_ap;
	seg_ap.x = point.x - seg_a.x;
	seg_ap.y = point.y - seg_a.y;
 
    double ab_bp = (seg_ab.x * seg_bp.x + seg_ab.y * seg_bp.y);
    double ab_ap = (seg_ab.x * seg_ap.x + seg_ab.y * seg_ap.y);
 
    if (ab_bp > 0) 
	{
        double y = point.y - seg_b.y;
        double x = point.x - seg_b.x;
        return std::sqrt(x * x + y * y);
    }
    else if (ab_ap < 0) 
	{
        double y = point.y - seg_a.y;
        double x = point.x - seg_a.x;
        return std::sqrt(x * x + y * y);
    }
    else 
	{
        double x1 = seg_ab.x;
        double y1 = seg_ab.y;
        double x2 = seg_ap.x;
        double y2 = seg_ap.y;
        double mod = std::sqrt(x1 * x1 + y1 * y1);
        return std::abs(x1 * y2 - y1 * x2) / mod;
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
