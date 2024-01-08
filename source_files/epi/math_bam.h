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

#ifndef __EPI_BAM__
#define __EPI_BAM__

#define ANGLEBITS 32

#define ANG0   0x00000000
#define ANG1   0x00B60B61
#define ANG5   0x038E38E3
#define ANG15  0x0AAAAAAA
#define ANG30  0x15555555
#define ANG45  0x20000000
#define ANG60  0x2AAAAAAA
#define ANG90  0x40000000
#define ANG135 0x60000000
#define ANG180 0x80000000
#define ANG225 0xa0000000
#define ANG270 0xc0000000
#define ANG315 0xe0000000
#define ANG360 0xffffffff

typedef uint32_t bam_angle_t;

namespace epi
{

inline bam_angle_t BAM_FromDegrees(int deg)
{
    return deg * 11930464 + deg * 7 / 10;
}

inline bam_angle_t BAM_FromDegrees(float deg)
{
    return (bam_angle_t)((deg < 0 ? (deg + 360.0f) : double(deg)) * 11930464.7084f);
}

inline bam_angle_t BAM_FromDegrees(double deg)
{
    return (bam_angle_t)((deg < 0 ? (deg + 360.0) : deg) * 11930464.7084);
}

inline bam_angle_t BAM_FromRadians(double rad)
{
    if (rad < 0)
        rad += M_PI * 2.0;
    
    return (bam_angle_t)(rad * 683565275.42);
}

inline float Degrees_FromBAM(bam_angle_t bam)
{
    return double(bam) * 0.0000000838190156f;
}

inline double Radians_FromBAM(bam_angle_t bam)
{
    return double(bam) * 0.000000001462918079601944;
}

inline bam_angle_t BAM_FromATan(float slope)
{
    return BAM_FromRadians(atan(slope));
}

inline float BAM_Sin(bam_angle_t bam)
{
    return sin(Radians_FromBAM(bam));
}

inline float BAM_Cos(bam_angle_t bam)
{
    return cos(Radians_FromBAM(bam));
}

inline float BAM_Tan(bam_angle_t bam)
{
    return tan(Radians_FromBAM(bam));
}

} // namespace epi

#endif /* __EPI_BAM__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
