//------------------------------------------------------------------------
//  EDGE CRC : Cyclic Rendundancy Check
//----------------------------------------------------------------------------
//
//  Copyright (c) 2003-2024 The EDGE Team.
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
//  Based on the Adler-32 algorithm as described in RFC-1950.
//
//------------------------------------------------------------------------

#include "epi.h"
#include "math_crc.h"

namespace epi
{

// ---- Primitive routines ----
CRC32 &CRC32::operator+=(uint8_t data)
{
    uint32_t s1 = crc_ & 0xFFFF;
    uint32_t s2 = (crc_ >> 16) & 0xFFFF;

    s1 = (s1 + data) % 65521;
    s2 = (s2 + s1) % 65521;

    crc_ = (s2 << 16) | s1;

    return *this;
}

CRC32 &CRC32::AddBlock(const uint8_t *data, int len)
{
    uint32_t s1 = crc_ & 0xFFFF;
    uint32_t s2 = (crc_ >> 16) & 0xFFFF;

    for (; len > 0; data++, len--)
    {
        s1 = (s1 + *data) % 65521;
        s2 = (s2 + s1) % 65521;
    }

    crc_ = (s2 << 16) | s1;

    return *this;
}

// ---- non-primitive routines ----

CRC32 &CRC32::operator+=(int32_t value)
{
    *this += (uint8_t)(value >> 24);
    *this += (uint8_t)(value >> 16);
    *this += (uint8_t)(value >> 8);
    *this += (uint8_t)(value);

    return *this;
}

CRC32 &CRC32::operator+=(uint32_t value)
{
    *this += (uint8_t)(value >> 24);
    *this += (uint8_t)(value >> 16);
    *this += (uint8_t)(value >> 8);
    *this += (uint8_t)(value);

    return *this;
}

CRC32 &CRC32::operator+=(float value)
{
    bool neg = (value < 0.0f);
    value    = (float)fabs(value);

    int exp;
    int mant = (int)(ldexp(frexp(value, &exp), 30));

    *this += (uint8_t)(neg ? '-' : '+');
    *this += exp;
    *this += mant;

    return *this;
}

CRC32 &CRC32::AddCString(const char *str)
{
    for (; *str; str++)
    {
        *this += (uint8_t)(*str);
    }

    return *this;
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
