//------------------------------------------------------------------------
//  EPI Binary Angle Measurement
//----------------------------------------------------------------------------
//
//  Copyright (c) 2004-2024  The EDGE Team.
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

constexpr uint8_t kBAMAngleBits = 32;

constexpr uint32_t kBAMAngle0   = 0x00000000;
constexpr uint32_t kBAMAngle1   = 0x00B60B61;
constexpr uint32_t kBAMAngle5   = 0x038E38E3;
constexpr uint32_t kBAMAngle15  = 0x0AAAAAAA;
constexpr uint32_t kBAMAngle30  = 0x15555555;
constexpr uint32_t kBAMAngle45  = 0x20000000;
constexpr uint32_t kBAMAngle60  = 0x2AAAAAAA;
constexpr uint32_t kBAMAngle90  = 0x40000000;
constexpr uint32_t kBAMAngle135 = 0x60000000;
constexpr uint32_t kBAMAngle180 = 0x80000000;
constexpr uint32_t kBAMAngle225 = 0xa0000000;
constexpr uint32_t kBAMAngle270 = 0xc0000000;
constexpr uint32_t kBAMAngle315 = 0xe0000000;
constexpr uint32_t kBAMAngle360 = 0xffffffff;

typedef uint32_t BAMAngle;

namespace epi
{

inline BAMAngle BAMFromDegrees(int deg)
{
    return deg * 11930464 + deg * 7 / 10;
}

inline BAMAngle BAMFromDegrees(float deg)
{
    return (BAMAngle)((deg < 0 ? (deg + 360.0f) : double(deg)) * 11930464.7084f);
}

inline BAMAngle BAMFromDegrees(double deg)
{
    return (BAMAngle)((deg < 0 ? (deg + 360.0) : deg) * 11930464.7084);
}

inline BAMAngle BAMFromRadians(double rad)
{
    if (rad < 0)
        rad += HMM_PI * 2.0;

    return (BAMAngle)(rad * 683565275.42);
}

inline float DegreesFromBAM(BAMAngle bam)
{
    return double(bam) * 0.0000000838190156f;
}

inline double RadiansFromBAM(BAMAngle bam)
{
    return double(bam) * 0.000000001462918079601944;
}

inline BAMAngle BAMFromATan(float slope)
{
    return BAMFromRadians(atan(slope));
}

inline float BAMSin(BAMAngle bam)
{
    return HMM_SINF(RadiansFromBAM(bam));
}

inline float BAMCos(BAMAngle bam)
{
    return HMM_COSF(RadiansFromBAM(bam));
}

inline float BAMTan(BAMAngle bam)
{
    return HMM_TANF(RadiansFromBAM(bam));
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
