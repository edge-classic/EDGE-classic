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
// M_PointInTri is adapted from the PNPOLY algorithm with the following license:
//
// Copyright (c) 1970-2003, Wm. Randolph Franklin
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
//
// Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimers.
// Redistributions in binary form must reproduce the above copyright notice in the documentation and/or other materials provided with the distribution.
// The name of W. Randolph Franklin may not be used to endorse or promote products derived from this Software without specific prior written permission.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

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

vec3_t M_CrossProduct(vec3_t v1, vec3_t v2, vec3_t v3)
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

// If the plane normal is precalculated; otherwise use the other version
vec3_t M_LinePlaneIntersection(vec3_t line_a, vec3_t line_b,
	vec3_t plane_a, vec3_t plane_b, vec3_t plane_c, vec3_t plane_normal)
{
	float n = M_DotProduct(plane_normal, {plane_c.x-line_a.x,plane_c.y-line_a.y,plane_c.z-line_a.z});
	vec3_t line_subtract{line_b.x-line_a.x,line_b.y-line_a.y,line_b.z-line_a.z};
	float d = M_DotProduct(plane_normal, line_subtract);
	float u = n/d;
	return{line_a.x+u*line_subtract.x, line_a.y+u*line_subtract.y, line_a.z+u*line_subtract.z};
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

int M_PointInTri(vec2_t v1, vec2_t v2, vec2_t v3, vec2_t test)
{
	std::vector<vec2_t> tri_vec = {v1,v2,v3};
	int i = 0;
	int j = 0;
	int c = 0;
	for (i=0, j=2; i < 3; j = i++)
	{
		if ( ((tri_vec[i].y > test.y) != (tri_vec[j].y > test.y)) &&
			(test.x < (tri_vec[j].x - tri_vec[i].x) * (test.y - tri_vec[i].y) / (tri_vec[j].y - tri_vec[i].y) + tri_vec[i].x) )
			c = !c;
	}
	return c;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
