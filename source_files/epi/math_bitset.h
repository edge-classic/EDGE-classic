//------------------------------------------------------------------------
//  EPI Alphabetical Bit Set
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

#include <string>

// a bitset is a set of named bits, from `A' to `Z'.
typedef int BitSet;

constexpr int kBitSetFull = 0x7FFFFFFF;

namespace epi
{

// Case-insensitive, i.e. 'a' will return the same bit as 'A'
// Returns 0 if not an ASCII alphabetical character
inline BitSet BitSetFromChar(char ch)
{
    if (ch > '@' && ch < '[')
        return (1 << ((ch) - 'A'));
    else if (ch > '`' && ch < '{')
        return (1 << ((ch ^ 0x20) - 'A'));
    else
        return 0;
}

}  // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
