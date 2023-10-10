//----------------------------------------------------------------------------
//  EDGE Floating Point Math Stuff
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2023  The EDGE Team.
// 
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
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

#ifndef __M_MATH_H__
#define __M_MATH_H__

#include "types.h"


typedef struct vec2_s
{
	float x, y;

	void Set(float _x, float _y)
	{
		x = _x; y = _y;
	}
}
vec2_t;

#define Vec2Add(dest, src)  do {  \
    (dest).x += (src).x; (dest).y += (src).y; } while(0)

#define Vec2Sub(dest, src)  do {  \
    (dest).x -= (src).x; (dest).y -= (src).y; } while(0)

#define Vec2Mul(dest, val)  do {  \
    (dest).x *= (val); (dest).y *= (val); } while(0)

typedef struct vec3_s
{
	float x, y, z;

	void Set(float _x, float _y, float _z)
	{
		x = _x; y = _y; z = _z;
	}
}
vec3_t;

#define Vec3Add(dest, src)  do {  \
    (dest).x += (src).x;  (dest).y += (src).y;  \
    (dest).z += (src).z; } while(0)

#define Vec3Sub(dest, src)  do {  \
    (dest).x -= (src).x;  (dest).y -= (src).y;  \
    (dest).z -= (src).z; } while(0)

#define Vec3Mul(dest, val)  do {  \
    (dest).x *= (val); (dest).y *= (val);  \
    (dest).z *= (val); } while(0)

double M_PointToSegDistance(vec2_t seg_a, vec2_t seg_b, vec2_t point);
vec3_t M_CrossProduct(vec3_t v1, vec3_t v2, vec3_t v3);
vec3_t M_LinePlaneIntersection(vec3_t line_a, vec3_t line_b,
	vec3_t plane_a, vec3_t plane_b, vec3_t plane_c, vec3_t plane_normal);
vec3_t M_LinePlaneIntersection(vec3_t line_a, vec3_t line_b,
	vec3_t plane_a, vec3_t plane_b, vec3_t plane_c);
float M_Tan(angle_t ang)    GCCATTR((const));
angle_t M_ATan(float slope) GCCATTR((const));
float M_Cos(angle_t ang)    GCCATTR((const));
float M_Sin(angle_t ang)    GCCATTR((const));
void M_Angle2Matrix(angle_t ang, vec2_t *x, vec2_t *y);
int M_PointInTri(vec2_t v1, vec2_t v2, vec2_t v3, vec2_t test);
void M_Vec2Rotate(vec2_t &vec, const angle_t &ang);


#endif //__M_MATH_H__


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
