//----------------------------------------------------------------------------
//  EDGE Floating Point Math Stuff
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2024 The EDGE Team.
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
// MathPointInTriangle is adapted from the PNPOLY algorithm with the following
// license:
//
// Copyright (c) 1970-2003, Wm. Randolph Franklin
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimers. Redistributions in binary
// form must reproduce the above copyright notice in the documentation and/or
// other materials provided with the distribution. The name of W. Randolph
// Franklin may not be used to endorse or promote products derived from this
// Software without specific prior written permission. THE SOFTWARE IS PROVIDED
// "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
// LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
// PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
// ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "m_math.h"

#include <math.h>

#include <vector>
#ifdef __APPLE__
#include <math.h>
#endif

void MathBAMAngleToMatrix(BAMAngle ang, HMM_Vec2 *x, HMM_Vec2 *y)
{
    x->X = epi::BAMCos(ang);
    x->Y = epi::BAMSin(ang);

    y->X = -x->Y;
    y->Y = x->X;
}

HMM_Vec3 MathTripleCrossProduct(HMM_Vec3 v1, HMM_Vec3 v2, HMM_Vec3 v3)
{
    return HMM_Cross(HMM_SubV3(v2, v1), HMM_SubV3(v3, v1));
}

// If the plane normal is precalculated; otherwise use the other version
HMM_Vec3 MathLinePlaneIntersection(HMM_Vec3 line_a, HMM_Vec3 line_b, HMM_Vec3 plane_c, HMM_Vec3 plane_normal)
{
    float    n             = HMM_DotV3(plane_normal, HMM_SubV3(plane_c, line_a));
    HMM_Vec3 line_subtract = HMM_SubV3(line_b, line_a);
    float    d             = HMM_DotV3(plane_normal, line_subtract);
    return HMM_AddV3(line_a, HMM_MulV3F(line_subtract, n / d));
}

HMM_Vec3 MathLinePlaneIntersection(HMM_Vec3 line_a, HMM_Vec3 line_b, HMM_Vec3 plane_a, HMM_Vec3 plane_b,
                                   HMM_Vec3 plane_c)
{
    HMM_Vec3 plane_normal  = MathTripleCrossProduct(plane_a, plane_b, plane_c);
    float    n             = HMM_DotV3(plane_normal, HMM_SubV3(plane_c, line_a));
    HMM_Vec3 line_subtract = HMM_SubV3(line_b, line_a);
    float    d             = HMM_DotV3(plane_normal, line_subtract);
    return HMM_AddV3(line_a, HMM_MulV3F(line_subtract, n / d));
}

float MathPointToSegDistance(HMM_Vec2 seg_a, HMM_Vec2 seg_b, HMM_Vec2 point)
{
    HMM_Vec2 seg_ab = HMM_SubV2(seg_b, seg_a);
    HMM_Vec2 seg_bp = HMM_SubV2(point, seg_b);
    HMM_Vec2 seg_ap = HMM_SubV2(point, seg_a);

    if (HMM_DotV2(seg_ab, seg_bp) > 0)
        return HMM_LenV2(HMM_SubV2(point, seg_b));
    else if (HMM_DotV2(seg_ab, seg_ap) < 0)
        return HMM_LenV2(HMM_SubV2(point, seg_a));
    else
        return abs(seg_ab.X * seg_ap.Y - seg_ab.Y * seg_ap.X) / HMM_LenV2(seg_ab);
}

int MathPointInTriangle(HMM_Vec2 v1, HMM_Vec2 v2, HMM_Vec2 v3, HMM_Vec2 test)
{
    std::vector<HMM_Vec2> tri_vec = {v1, v2, v3};
    int                   i       = 0;
    int                   j       = 0;
    int                   c       = 0;
    for (i = 0, j = 2; i < 3; j = i++)
    {
        if (((tri_vec[i].Y > test.Y) != (tri_vec[j].Y > test.Y)) &&
            (test.X <
             (tri_vec[j].X - tri_vec[i].X) * (test.Y - tri_vec[i].Y) / (tri_vec[j].Y - tri_vec[i].Y) + tri_vec[i].X))
            c = !c;
    }
    return c;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
