//------------------------------------------------------------------------
//  BUFFER for Parsing
//------------------------------------------------------------------------
//
//  DEH_EDGE  Copyright (C) 2004-2024 The EDGE Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License (in COPYING.txt) for more details.
//
//------------------------------------------------------------------------
//
//  DEH_EDGE is based on:
//
//  +  DeHackEd source code, by Greg Lewis.
//  -  DOOM source code (C) 1993-1996 id Software, Inc.
//  -  Linux DOOM Hack Editor, by Sam Lantinga.
//  -  PrBoom's DEH/BEX code, by Ty Halderman, TeamTNT.
//
//------------------------------------------------------------------------

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "deh_i_defs.h"
#include "deh_edge.h"

#include "deh_buffer.h"
#include "deh_system.h"

namespace Deh_Edge
{

input_buffer_c::input_buffer_c(const char *_data, int _length) : data(_data), ptr(_data), length(_length)
{
    if (length < 0)
        FatalError("Illegal length of lump (%d bytes)\n", length);
}

input_buffer_c::~input_buffer_c()
{
}

bool input_buffer_c::eof()
{
    return (ptr >= data + length);
}

bool input_buffer_c::error()
{
    return false;
}

int input_buffer_c::read(void *buf, int count)
{
    int avail = data + length - ptr;

    if (avail < count)
        count = avail;

    if (count <= 0)
        return 0;

    memcpy(buf, ptr, count);

    ptr += count;

    return count;
}

int input_buffer_c::getch()
{
    if (eof())
        return EOF;

    return *ptr++;
}

void input_buffer_c::ungetch(int c)
{
    // NOTE: assumes c == last character read

    if (ptr > data)
        ptr--;
}

bool input_buffer_c::isBinary() const
{
    if (length == 0)
        return false;

    int test_len = (length > 260) ? 256 : ((length * 3 + 1) / 4);

    for (; test_len > 0; test_len--)
        if (data[test_len - 1] == 0)
            return true;

    return false;
}

} // namespace Deh_Edge
