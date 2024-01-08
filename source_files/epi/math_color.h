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

#ifndef __EPI_MATH_COLOR_H__
#define __EPI_MATH_COLOR_H__

#include "epi.h"

#include "sokol_color.h"

// RGBA 8:8:8:8
typedef uint32_t rgbacol_t;

#define RGB_NO_VALUE 0x01FEFEFF /* bright CYAN */

namespace epi
{

inline rgbacol_t RGBA_Make(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
{
    return (rgbacol_t)(((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | ((uint32_t)a << 0));
}

inline uint8_t RGBA_Red(rgbacol_t rgba)
{
    return (uint8_t)(rgba >> 24);
}

inline uint8_t RGBA_Green(rgbacol_t rgba)
{
    return (uint8_t)(rgba >> 16);
}

inline uint8_t RGBA_Blue(rgbacol_t rgba)
{
    return (uint8_t)(rgba >> 8);
}

inline uint8_t RGBA_Alpha(rgbacol_t rgba)
{
    return (uint8_t)(rgba >> 0);
}

inline rgbacol_t RGBA_Mix(const rgbacol_t &mix1, const rgbacol_t &mix2, int qty = 128)
{
    int nr = int(RGBA_Red(mix1)) * (255 - qty) + int(RGBA_Red(mix2)) * qty;
    int ng = int(RGBA_Green(mix1)) * (255 - qty) + int(RGBA_Green(mix2)) * qty;
    int nb = int(RGBA_Blue(mix1)) * (255 - qty) + int(RGBA_Blue(mix2)) * qty;
    int na = int(RGBA_Alpha(mix1)) * (255 - qty) + int(RGBA_Alpha(mix2)) * qty;

    return RGBA_Make(uint8_t(nr / 255), uint8_t(ng / 255), uint8_t(nb / 255), uint8_t(na / 255));
}

class hsv_col_c
{
  public:
    // sealed, value semantics.
    //
    // h is hue (angle from 0 to 359: 0 = RED, 120 = GREEN, 240 = BLUE).
    // s is saturation (0 to 255: 0 = White, 255 = Pure color).
    // v is value (0 to 255: 0 = Darkest, 255 = Brightest).

    short h;
    uint8_t  s, v;

    hsv_col_c(const rgbacol_t &col); // conversion from RGBA

    rgbacol_t GetRGBA() const; // conversion to RGBA

    inline hsv_col_c &Rotate(int delta)
    {
        int bam = int(h + delta) * 372827;

        h = short((bam & 0x7FFFFFF) / 372827);

        return *this;
    } // usable range: -1800 to +1800
    inline hsv_col_c &SetSaturation(int sat)
    {
        s = sat;

        return *this;
    }

    inline hsv_col_c &SetValue(int val)
    {
        v = val;

        return *this;
    }
};

} // namespace epi

#endif /* __EPI_MATH_COLOR_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
