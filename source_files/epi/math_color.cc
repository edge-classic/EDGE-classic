//------------------------------------------------------------------------
//  EPI Colour types (RGBA and HSV)
//----------------------------------------------------------------------------
//
//  Copyright (c) 2004-2024 The EDGE Team.
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

#include "math_color.h"
#include "epi.h"
#include "HandmadeMath.h"
namespace epi
{

HSVColor::HSVColor(const RGBAColor &col)
{
    uint8_t r = GetRGBARed(col);
    uint8_t g = GetRGBAGreen(col);
    uint8_t b = GetRGBABlue(col);

    int m = HMM_MIN(r, HMM_MIN(b, g));

    v_ = HMM_MAX(r, HMM_MAX(b, g));

    s_ = (v_ == 0) ? 0 : (v_ - m) * 255 / v_;

    if (v_ <= m)
    {
        h_ = 0;
        return;
    }

    int r1 = (v_ - r) * 59 / (v_ - m);
    int g1 = (v_ - g) * 59 / (v_ - m);
    int b1 = (v_ - b) * 59 / (v_ - m);

    if (v_ == r && m == g)
        h_ = 300 + b1;
    else if (v_ == r)
        h_ = 60 - g1;

    else if (v_ == g && m == b)
        h_ = 60 + r1;
    else if (v_ == g)
        h_ = 180 - b1;

    else if (m == r)
        h_ = 180 + g1;
    else
        h_ = 300 - r1;

    EPI_ASSERT(0 <= h_ && h_ <= 360);
}

RGBAColor HSVColor::ToRGBA() const
{
    EPI_ASSERT(0 <= h_ && h_ <= 360);

    int sextant = (h_ % 360) / 60;
    int frac    = h_ % 60;

    int p1 = 255 - s_;
    int p2 = 255 - (s_ * frac) / 59;
    int p3 = 255 - (s_ * (59 - frac)) / 59;

    p1 = p1 * v_ / 255;
    p2 = p2 * v_ / 255;
    p3 = p3 * v_ / 255;

    int r, g, b;

    EPI_ASSERT(0 <= sextant && sextant <= 5);

    switch (sextant)
    {
        case 0:
            r = v_, g = p3, b = p1;
            break;
        case 1:
            r = p2, g = v_, b = p1;
            break;
        case 2:
            r = p1, g = v_, b = p3;
            break;
        case 3:
            r = p1, g = p2, b = v_;
            break;
        case 4:
            r = p3, g = p1, b = v_;
            break;
        default:
            r = v_, g = p1, b = p2;
            break;
    }

    EPI_ASSERT(0 <= r && r <= 255);
    EPI_ASSERT(0 <= g && g <= 255);
    EPI_ASSERT(0 <= b && b <= 255);

    return MakeRGBA(r, g, b);
}

}  // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
