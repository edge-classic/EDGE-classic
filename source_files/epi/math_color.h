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

#pragma once

#include <stdint.h>

// RGBA 8:8:8:8
typedef uint32_t RGBAColor;

constexpr uint32_t kRGBANoValue = 0x01FEFEFF; /* bright CYAN */

namespace epi
{

inline RGBAColor MakeRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
{
    return (RGBAColor)(((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | ((uint32_t)a << 0));
}

inline uint8_t GetRGBARed(RGBAColor rgba)
{
    return (uint8_t)(rgba >> 24);
}

inline uint8_t GetRGBAGreen(RGBAColor rgba)
{
    return (uint8_t)(rgba >> 16);
}

inline uint8_t GetRGBABlue(RGBAColor rgba)
{
    return (uint8_t)(rgba >> 8);
}

inline uint8_t GetRGBAAlpha(RGBAColor rgba)
{
    return (uint8_t)(rgba >> 0);
}

inline RGBAColor MixRGBA(const RGBAColor &mix1, const RGBAColor &mix2, int qty = 128)
{
    int nr = int(GetRGBARed(mix1)) * (255 - qty) + int(GetRGBARed(mix2)) * qty;
    int ng = int(GetRGBAGreen(mix1)) * (255 - qty) + int(GetRGBAGreen(mix2)) * qty;
    int nb = int(GetRGBABlue(mix1)) * (255 - qty) + int(GetRGBABlue(mix2)) * qty;
    int na = int(GetRGBAAlpha(mix1)) * (255 - qty) + int(GetRGBAAlpha(mix2)) * qty;

    return MakeRGBA(uint8_t(nr / 255), uint8_t(ng / 255), uint8_t(nb / 255), uint8_t(na / 255));
}

class HSVColor
{
  public:
    // sealed, value semantics.
    //
    // h is hue (angle from 0 to 359: 0 = RED, 120 = GREEN, 240 = BLUE).
    // s is saturation (0 to 255: 0 = White, 255 = Pure color).
    // v is value (0 to 255: 0 = Darkest, 255 = Brightest).

    short   h_;
    uint8_t s_, v_;

    HSVColor(const RGBAColor &col); // conversion from RGBA

    RGBAColor ToRGBA() const;       // conversion to RGBA

    inline HSVColor &Rotate(int delta)
    {
        int bam = int(h_ + delta) * 372827;

        h_ = short((bam & 0x7FFFFFF) / 372827);

        return *this;
    } // usable range: -1800 to +1800

    inline HSVColor &SetSaturation(int sat)
    {
        s_ = sat;

        return *this;
    }

    inline HSVColor &SetValue(int val)
    {
        v_ = val;

        return *this;
    }
};

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
