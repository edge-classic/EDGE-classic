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

#include "HandmadeMath.h"

// RGBA 8:8:8:8
typedef uint32_t RGBAColor;

constexpr RGBAColor kRGBANoValue = 0x01FEFEFF; /* EDGE special value, bright cyan */

// X11 Color Presets
// Todo: Perhaps automatically gen DDFCOLM entries for these before loading
// other colormaps to ensure they are "always available" for modders? - Dasho
constexpr RGBAColor kRGBAAliceBlue = 0xF0F8FFFF;
constexpr RGBAColor kRGBAAntiqueWhite = 0xFAEBD7FF;
constexpr RGBAColor kRGBAAqua = 0x00FFFFFF;
constexpr RGBAColor kRGBAAquamarine = 0x7FFFD4FF;
constexpr RGBAColor kRGBAAzure = 0xF0FFFFFF;
constexpr RGBAColor kRGBABeige = 0xF5F5DCFF;
constexpr RGBAColor kRGBABisque = 0xFFE4C4FF;
constexpr RGBAColor kRGBABlack = 0x000000FF;
constexpr RGBAColor kRGBABlanchedAlmond = 0xFFEBCDFF;
constexpr RGBAColor kRGBABlue = 0x0000FFFF;
constexpr RGBAColor kRGBABlueViolet = 0x8A2BE2FF;
constexpr RGBAColor kRGBABrown = 0xA52A2AFF;
constexpr RGBAColor kRGBABurlywood = 0xDEB887FF;
constexpr RGBAColor kRGBACadetBlue = 0x5F9EA0FF;
constexpr RGBAColor kRGBAChartreuse = 0x7FFF00FF;
constexpr RGBAColor kRGBAChocolate = 0xD2691EFF;
constexpr RGBAColor kRGBACoral = 0xFF7F50FF;
constexpr RGBAColor kRGBACornflowerBlue = 0x6495EDFF;
constexpr RGBAColor kRGBACornsilk = 0xFFF8DCFF;
constexpr RGBAColor kRGBACrimson = 0xDC143CFF;
constexpr RGBAColor kRGBACyan = 0x00FFFFFF;
constexpr RGBAColor kRGBADarkBlue = 0x00008BFF;
constexpr RGBAColor kRGBADarkCyan = 0x008B8BFF;
constexpr RGBAColor kRGBADarkGoldenrod = 0xB8860BFF;
constexpr RGBAColor kRGBADarkGray = 0xA9A9A9FF;
constexpr RGBAColor kRGBADarkGreen = 0x006400FF;
constexpr RGBAColor kRGBADarkKhaki = 0xBDB76BFF;
constexpr RGBAColor kRGBADarkMagenta = 0x8B008BFF;
constexpr RGBAColor kRGBADarkOliveGreen = 0x556B2FFF;
constexpr RGBAColor kRGBADarkOrange = 0xFF8C00FF;
constexpr RGBAColor kRGBADarkOrchid = 0x9932CCFF;
constexpr RGBAColor kRGBADarkRed = 0x8B0000FF;
constexpr RGBAColor kRGBADarkSlamon = 0xE9967AFF;
constexpr RGBAColor kRGBADarkSeaGreen = 0x8FBC8FFF;
constexpr RGBAColor kRGBADarkSlateBlue = 0x483D8BFF;
constexpr RGBAColor kRGBADarkSlateGray = 0x2F4F4FFF;
constexpr RGBAColor kRGBADarkTurquoise = 0x00CED1FF;
constexpr RGBAColor kRGBADarkViolet = 0x9400D3FF;
constexpr RGBAColor kRGBADeepPink = 0xFF1493FF;
constexpr RGBAColor kRGBADeepSkyBlue = 0x00BFFFFF;
constexpr RGBAColor kRGBADimGray = 0x696969FF;
constexpr RGBAColor kRGBADodgerBlue = 0x1E90FFFF;
constexpr RGBAColor kRGBAFireBrick = 0xB22222FF;
constexpr RGBAColor kRGBAFloralWhite = 0xFFFAF0FF;
constexpr RGBAColor kRGBAForestGreen = 0x228B22FF;
constexpr RGBAColor kRGBAFuchsia = 0xFF00FFFF;
constexpr RGBAColor kRGBAGainsboro = 0xDCDCDCFF;
constexpr RGBAColor kRGBAGhostWhite = 0xF8F8FFFF;
constexpr RGBAColor kRGBAGold = 0xFFD700FF;
constexpr RGBAColor kRGBAGoldenrod = 0xDAA520FF;
constexpr RGBAColor kRGBAGray = 0xBEBEBEFF;
constexpr RGBAColor kRGBAWebGray = 0x808080FF;
constexpr RGBAColor kRGBAGreen = 0x00FF00FF;
constexpr RGBAColor kRGBAWebGreen = 0x008000FF;
constexpr RGBAColor kRGBAGreenYellow = 0xADFF2FFF;
constexpr RGBAColor kRGBAHoneydew = 0xF0FFF0FF;
constexpr RGBAColor kRGBAHotPink = 0xFF69B4FF;
constexpr RGBAColor kRGBAIndianRed = 0xCD5C5CFF;
constexpr RGBAColor kRGBAIndigo = 0x4B0082FF;
constexpr RGBAColor kRGBAIvory = 0xFFFFF0FF;
constexpr RGBAColor kRGBAKhaki = 0xF0E68CFF;
constexpr RGBAColor kRGBALavender = 0xE6E6FAFF;
constexpr RGBAColor kRGBALavenderBlush = 0xFFF0F5FF;
constexpr RGBAColor kRGBALawnGreen = 0x7CFC00FF;
constexpr RGBAColor kRGBALemonChiffon = 0xFFFACDFF;
constexpr RGBAColor kRGBALightBlue = 0xADD8E6FF;
constexpr RGBAColor kRGBALightCoral = 0xF08080FF;
constexpr RGBAColor kRGBALightCyan = 0xE0FFFFFF;
constexpr RGBAColor kRGBALightGoldenrod = 0xFAFAD2FF;
constexpr RGBAColor kRGBALightGray = 0xD3D3D3FF;
constexpr RGBAColor kRGBALightGreen = 0x90EE90FF;
constexpr RGBAColor kRGBALightPink = 0xFFB6C1FF;
constexpr RGBAColor kRGBALightSalmon = 0xFFA07AFF;
constexpr RGBAColor kRGBALightSeaGreen = 0x20B2AAFF;
constexpr RGBAColor kRGBALightSkyBlue = 0x87CEFAFF;
constexpr RGBAColor kRGBALightSlateGray = 0x778899FF;
constexpr RGBAColor kRGBALightSteelBlue = 0xB0C4DEFF;
constexpr RGBAColor kRGBALightYellow = 0xFFFFE0FF;
constexpr RGBAColor kRGBALime = 0x00FF00FF;
constexpr RGBAColor kRGBALimeGreen = 0x32CD32FF;
constexpr RGBAColor kRGBALinen = 0xFAF0E6FF;
constexpr RGBAColor kRGBAMagenta = 0xFF00FFFF;
constexpr RGBAColor kRGBAMaroon = 0xB03060FF;
constexpr RGBAColor kRGBAWebMaroon = 0x800000FF;
constexpr RGBAColor kRGBAMediumAquamarine = 0x66CDAAFF;
constexpr RGBAColor kRGBAMediumBlue = 0x0000CDFF;
constexpr RGBAColor kRGBAMediumOrchid = 0xBA55D3FF;
constexpr RGBAColor kRGBAMediumPurple = 0x9370DBFF;
constexpr RGBAColor kRGBAMediumSeaGreen = 0x3CB371FF;
constexpr RGBAColor kRGBAMediumSlateBlue = 0x7B68EEFF;
constexpr RGBAColor kRGBAMediumSpringGreen = 0x00FA9AFF;
constexpr RGBAColor kRGBAMediumTurquoise = 0x48D1CCFF;
constexpr RGBAColor kRGBAMediumVioletRed = 0xC71585FF;
constexpr RGBAColor kRGBAMidnightBlue = 0x191970FF;
constexpr RGBAColor kRGBAMintCream = 0xF5FFFAFF;
constexpr RGBAColor kRGBAMistyRose = 0xFFE4E1FF;
constexpr RGBAColor kRGBAMoccasin = 0xFFE4B5FF;
constexpr RGBAColor kRGBANavajoWhite = 0xFFDEADFF;
constexpr RGBAColor kRGBANavyBlue = 0x000080FF;
constexpr RGBAColor kRGBAOldLace = 0xFDF5E6FF;
constexpr RGBAColor kRGBAOlive = 0x808000FF;
constexpr RGBAColor kRGBAOliveDrab = 0x6B8E23FF;
constexpr RGBAColor kRGBAOrange = 0xFFA500FF;
constexpr RGBAColor kRGBAOrangeRed = 0xFF4500FF;
constexpr RGBAColor kRGBAOrchid = 0xDA70D6FF;
constexpr RGBAColor kRGBAPaleGoldenrod = 0xEEE8AAFF;
constexpr RGBAColor kRGBAPaleGreen = 0x98FB98FF;
constexpr RGBAColor kRGBAPaleTurquoise = 0xAFEEEEFF;
constexpr RGBAColor kRGBAPaleVioletRed = 0xDB7093FF;
constexpr RGBAColor kRGBAPapayaWhip = 0xFFEFD5FF;
constexpr RGBAColor kRGBAPeachPuff = 0xFFDAB9FF;
constexpr RGBAColor kRGBAPeru = 0xCD853FFF;
constexpr RGBAColor kRGBAPink = 0xFFC0CBFF;
constexpr RGBAColor kRGBAPlum = 0xDDA0DDFF;
constexpr RGBAColor kRGBAPowderBlue = 0xB0E0E6FF;
constexpr RGBAColor kRGBAPurple = 0xA020F0FF;
constexpr RGBAColor kRGBAWebPurple = 0x800080FF;
constexpr RGBAColor kRGBARebeccaPurple = 0x663399FF;
constexpr RGBAColor kRGBARed = 0xFF0000FF;
constexpr RGBAColor kRGBARosyBrown = 0xBC8F8FFF;
constexpr RGBAColor kRGBARoyalBlue = 0x4169E1FF;
constexpr RGBAColor kRGBASaddleBrown = 0x8B4513FF;
constexpr RGBAColor kRGBASalmon = 0xFA8072FF;
constexpr RGBAColor kRGBASandyBrown = 0xF4A460FF;
constexpr RGBAColor kRGBASeaGreen = 0x2E8B57FF;
constexpr RGBAColor kRGBASeaShell = 0xFFF5EEFF;
constexpr RGBAColor kRGBASienna = 0xA0522DFF;
constexpr RGBAColor kRGBASilver = 0xC0C0C0FF;
constexpr RGBAColor kRGBASkyBlue = 0x87CEEBFF;
constexpr RGBAColor kRGBASlateBlue = 0x6A5ACDFF;
constexpr RGBAColor kRGBASlateGray = 0x708090FF;
constexpr RGBAColor kRGBASnow = 0xFFFAFAFF;
constexpr RGBAColor kRGBASpringGreen = 0x00FF7FFF;
constexpr RGBAColor kRGBASteelBlue = 0x4682B4FF;
constexpr RGBAColor kRGBATan = 0xD2B48CFF;
constexpr RGBAColor kRGBATeal = 0x008080FF;
constexpr RGBAColor kRGBAThistle = 0xD8BFD8FF;
constexpr RGBAColor kRGBATomato = 0xFF6347FF;
constexpr RGBAColor kRGBATransparent = 0x00000000;
constexpr RGBAColor kRGBATurquoise = 0x40E0D0FF;
constexpr RGBAColor kRGBAViolet = 0xEE82EEFF;
constexpr RGBAColor kRGBAWheat = 0xF5DEB3FF;
constexpr RGBAColor kRGBAWhite = 0xFFFFFFFF;
constexpr RGBAColor kRGBAWhiteSmoke = 0xF5F5F5FF;
constexpr RGBAColor kRGBAYellow = 0xFFFF00FF;
constexpr RGBAColor kRGBAYellowGreen = 0x9ACD32FF;

namespace epi
{

inline RGBAColor MakeRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
{
    return (RGBAColor)(((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | ((uint32_t)a << 0));
}

inline RGBAColor MakeRGBAFloat(float r, float g, float b, float a = 1.0f)
{
    return (RGBAColor)(((uint32_t)(r * 255.0f) << 24) | ((uint32_t)(g * 255.0f) << 16) | ((uint32_t)(b * 255.0f) << 8) | ((uint32_t)(a * 255.0f) << 0));
}

inline RGBAColor MakeRGBAClamped(int r, int g, int b, int a = 255)
{
    uint32_t nr = HMM_Clamp(0, r, 255);
	uint32_t ng = HMM_Clamp(0, g, 255);
	uint32_t nb = HMM_Clamp(0, b, 255);
	uint32_t na = HMM_Clamp(0, a, 255);

    return (RGBAColor)((nr << 24) | (ng << 16) | (nb << 8) | (na << 0));
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

inline void SetRGBAAlpha(RGBAColor &rgba, uint8_t alpha)
{
    rgba &= 0xFFFFFF00; 
    rgba |= (uint32_t)alpha;
}

inline void SetRGBAAlpha(RGBAColor &rgba, float alpha)
{
    uint32_t ualpha = (uint32_t)(alpha * 255.0f);
    rgba &= 0xFFFFFF00; 
    rgba |= ualpha;
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
