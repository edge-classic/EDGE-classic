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
// M_PointInTri is adapted from the PNPOLY algorithm with the following license:
//
// Copyright (c) 1970-2003, Wm. Randolph Franklin
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
// documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to the following conditions:
//
// Redistributions of source code must retain the above copyright notice, this list of conditions and the following
// disclaimers. Redistributions in binary form must reproduce the above copyright notice in the documentation and/or
// other materials provided with the distribution. The name of W. Randolph Franklin may not be used to endorse or
// promote products derived from this Software without specific prior written permission. THE SOFTWARE IS PROVIDED "AS
// IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "i_defs.h"

#include <math.h>

#include "m_math.h"

#ifdef __APPLE__
#include <cmath>
#endif

float M_Sin(angle_t ang)
{
    return (float)sin((double)ang * M_PI / (float)ANG180);
}

float M_Cos(angle_t ang)
{
    return (float)cos((double)ang * M_PI / (float)ANG180);
}

float M_Tan(angle_t ang)
{
    return (float)tan((double)ang * M_PI / (float)ANG180);
}

angle_t M_ATan(float slope)
{
    return (s32_t)((float)ANG180 * atan(slope) /
                   M_PI); // Updated M_ATan from EDGE 2.X branch, works properly with MSVC now
}

void M_Angle2Matrix(angle_t ang, HMM_Vec2 *x, HMM_Vec2 *y)
{
    x->X = M_Cos(ang);
    x->Y = M_Sin(ang);

    y->X = -x->Y;
    y->Y = x->X;
}

HMM_Vec3 M_CrossProduct(HMM_Vec3 v1, HMM_Vec3 v2, HMM_Vec3 v3)
{
    HMM_Vec3 A{v2.X - v1.X, v2.Y - v1.Y, v2.Z - v1.Z};
    HMM_Vec3 B{v3.X - v1.X, v3.Y - v1.Y, v3.Z - v1.Z};
    float  x = (A.Y * B.Z) - (A.Z * B.Y);
    float  y = (A.Z * B.X) - (A.X * B.Z);
    float  z = (A.X * B.Y) - (A.Y * B.X);
    return {x, y, z};
}

static float M_DotProduct(HMM_Vec3 v1, HMM_Vec3 v2)
{
    return (v1.X * v2.X) + (v1.Y * v2.Y) + (v1.Z * v2.Z);
}

// If the plane normal is precalculated; otherwise use the other version
HMM_Vec3 M_LinePlaneIntersection(HMM_Vec3 line_a, HMM_Vec3 line_b, HMM_Vec3 plane_a, HMM_Vec3 plane_b, HMM_Vec3 plane_c,
                               HMM_Vec3 plane_normal)
{
    float  n = M_DotProduct(plane_normal, {plane_c.X - line_a.X, plane_c.Y - line_a.Y, plane_c.Z - line_a.Z});
    HMM_Vec3 line_subtract{line_b.X - line_a.X, line_b.Y - line_a.Y, line_b.Z - line_a.Z};
    float  d = M_DotProduct(plane_normal, line_subtract);
    float  u = n / d;
    return {line_a.X + u * line_subtract.X, line_a.Y + u * line_subtract.Y, line_a.Z + u * line_subtract.Z};
}

HMM_Vec3 M_LinePlaneIntersection(HMM_Vec3 line_a, HMM_Vec3 line_b, HMM_Vec3 plane_a, HMM_Vec3 plane_b, HMM_Vec3 plane_c)
{
    HMM_Vec3 plane_normal = M_CrossProduct(plane_a, plane_b, plane_c);
    float  n = M_DotProduct(plane_normal, {plane_c.X - line_a.X, plane_c.Y - line_a.Y, plane_c.Z - line_a.Z});
    HMM_Vec3 line_subtract{line_b.X - line_a.X, line_b.Y - line_a.Y, line_b.Z - line_a.Z};
    float  d = M_DotProduct(plane_normal, line_subtract);
    float  u = n / d;
    return {line_a.X + u * line_subtract.X, line_a.Y + u * line_subtract.Y, line_a.Z + u * line_subtract.Z};
}

double M_PointToSegDistance(HMM_Vec2 seg_a, HMM_Vec2 seg_b, HMM_Vec2 point)
{

    HMM_Vec2 seg_ab;
    seg_ab.X = seg_b.X - seg_a.X;
    seg_ab.Y = seg_b.Y - seg_a.Y;

    HMM_Vec2 seg_bp;
    seg_bp.X = point.X - seg_b.X;
    seg_bp.Y = point.Y - seg_b.Y;

    HMM_Vec2 seg_ap;
    seg_ap.X = point.X - seg_a.X;
    seg_ap.Y = point.Y - seg_a.Y;

    double ab_bp = (seg_ab.X * seg_bp.X + seg_ab.Y * seg_bp.Y);
    double ab_ap = (seg_ab.X * seg_ap.X + seg_ab.Y * seg_ap.Y);

    if (ab_bp > 0)
    {
        double y = point.Y - seg_b.Y;
        double x = point.X - seg_b.X;
        return sqrt(x * x + y * y);
    }
    else if (ab_ap < 0)
    {
        double y = point.Y - seg_a.Y;
        double x = point.X - seg_a.X;
        return sqrt(x * x + y * y);
    }
    else
    {
        double x1  = seg_ab.X;
        double y1  = seg_ab.Y;
        double x2  = seg_ap.X;
        double y2  = seg_ap.Y;
        double mod = sqrt(x1 * x1 + y1 * y1);
        return abs(x1 * y2 - y1 * x2) / mod;
    }
}

int M_PointInTri(HMM_Vec2 v1, HMM_Vec2 v2, HMM_Vec2 v3, HMM_Vec2 test)
{
    std::vector<HMM_Vec2> tri_vec = {v1, v2, v3};
    int                 i       = 0;
    int                 j       = 0;
    int                 c       = 0;
    for (i = 0, j = 2; i < 3; j = i++)
    {
        if (((tri_vec[i].Y > test.Y) != (tri_vec[j].Y > test.Y)) &&
            (test.X <
             (tri_vec[j].X - tri_vec[i].X) * (test.Y - tri_vec[i].Y) / (tri_vec[j].Y - tri_vec[i].Y) + tri_vec[i].X))
            c = !c;
    }
    return c;
}

void M_Vec2Rotate(HMM_Vec2 &vec, const angle_t &ang)
{
    float s = M_Sin(ang);
    float c = M_Cos(ang);

    float ox = vec.X;
    float oy = vec.Y;

    vec.X = ox * c - oy * s;
    vec.Y = oy * c + ox * s;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
