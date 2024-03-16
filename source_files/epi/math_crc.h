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

#pragma once

#include <stdint.h>

namespace epi
{

class CRC32
{
    /* sealed */
  private:
    uint32_t crc_;

  public:
    CRC32()
    {
        Reset();
    }
    CRC32(const CRC32 &rhs)
    {
        crc_ = rhs.crc_;
    }
    ~CRC32()
    {
    }

    bool operator==(const CRC32 &other) const
    {
        return crc_ == other.crc_;
    }

    CRC32 &operator=(const CRC32 &rhs)
    {
        crc_ = rhs.crc_;
        return *this;
    }

    CRC32 &operator+=(uint8_t value);
    CRC32 &operator+=(int32_t value);
    CRC32 &operator+=(uint32_t value);
    CRC32 &operator+=(float value);

    CRC32 &AddBlock(const uint8_t *data, int len);
    CRC32 &AddCString(const char *str);

    void Reset(void)
    {
        crc_ = 1;
    }

    uint32_t GetCRC(void) const
    {
        return crc_;
    }
};

}; // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
