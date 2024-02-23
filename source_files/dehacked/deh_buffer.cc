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

#include "deh_buffer.h"

#include <stdio.h>
#include <string.h>

#include "deh_edge.h"
#include "deh_system.h"
#include "epi.h"

namespace dehacked
{

InputBuffer::InputBuffer(const char *data, int length)
    : data_(data), pointer_(data), length_(length)
{
    if (length_ < 0)
        EDGEError("Dehacked: Error - Illegal length of lump (%d bytes)\n",
                length_);
}

InputBuffer::~InputBuffer() {}

bool InputBuffer::EndOfFile() { return (pointer_ >= data_ + length_); }

bool InputBuffer::Error() { return false; }

int InputBuffer::Read(void *buffer, int count)
{
    int available = data_ + length_ - pointer_;

    if (available < count) count = available;

    if (count <= 0) return 0;

    memcpy(buffer, pointer_, count);

    pointer_ += count;

    return count;
}

int InputBuffer::GetCharacter()
{
    if (EndOfFile()) return EOF;

    return *pointer_++;
}

void InputBuffer::UngetCharacter(int character)
{
    // NOTE: assumes c == last character read
    (void)character;  // what was supposed to be done with this? - Dasho

    if (pointer_ > data_) pointer_--;
}

bool InputBuffer::IsBinary() const
{
    if (length_ == 0) return false;

    int test_length = (length_ > 260) ? 256 : ((length_ * 3 + 1) / 4);

    for (; test_length > 0; test_length--)
        if (data_[test_length - 1] == 0) return true;

    return false;
}

}  // namespace dehacked
