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

#include "math_bam.h"

#include "HandmadeMath.h"

float  M_PointToSegDistance(HMM_Vec2 seg_a, HMM_Vec2 seg_b, HMM_Vec2 point);
HMM_Vec3  M_TripleCrossProduct(HMM_Vec3 v1, HMM_Vec3 v2, HMM_Vec3 v3);
HMM_Vec3  M_LinePlaneIntersection(HMM_Vec3 line_a, HMM_Vec3 line_b, HMM_Vec3 plane_a, HMM_Vec3 plane_b, HMM_Vec3 plane_c,
                                HMM_Vec3 plane_normal);
HMM_Vec3  M_LinePlaneIntersection(HMM_Vec3 line_a, HMM_Vec3 line_b, HMM_Vec3 plane_a, HMM_Vec3 plane_b, HMM_Vec3 plane_c);
void    M_Angle2Matrix(bam_angle ang, HMM_Vec2 *x, HMM_Vec2 *y);
int     M_PointInTri(HMM_Vec2 v1, HMM_Vec2 v2, HMM_Vec2 v3, HMM_Vec2 test);
void    M_Vec2Rotate(HMM_Vec2 &vec, const bam_angle &ang);

#endif //__M_MATH_H__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
