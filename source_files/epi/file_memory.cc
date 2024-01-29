//----------------------------------------------------------------------------
//  Block of memory with File interface
//----------------------------------------------------------------------------
//
//  Copyright (c) 2007-2024 The EDGE Team.
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

#include "epi.h"

#include "file_memory.h"

namespace epi
{

//
// Constructor
//
MemFile::MemFile(const uint8_t *block, int len, bool copy_it)
{
    SYS_ASSERT(block);
    SYS_ASSERT(len >= 0);

    pos_    = 0;
    copied_ = false;

    if (len == 0)
    {
        data_   = nullptr;
        length_ = 0;
        return;
    }

    if (copy_it)
    {
        data_   = new uint8_t[len];
        length_ = len;

        memcpy(data_, block, len);
        copied_ = true;
    }
    else
    {
        data_   = (uint8_t *)block;
        length_ = len;
    }
}

//
// Destructor
//
MemFile::~MemFile()
{
    if (data_ && copied_)
    {
        delete[] data_;
        data_ = nullptr;
    }

    length_ = 0;
}

unsigned int MemFile::Read(void *dest, unsigned int size)
{
    SYS_ASSERT(dest);

    unsigned int avail = length_ - pos_;

    if (size > avail)
        size = avail;

    if (size == 0)
        return 0; // EOF

    memcpy(dest, data_ + pos_, size);
    pos_ += size;

    return size;
}

bool MemFile::Seek(int offset, int seekpoint)
{
    int new_pos = 0;

    switch (seekpoint)
    {
    case kSeekpointStart: {
        new_pos = 0;
        break;
    }
    case kSeekpointCurrent: {
        new_pos = pos_;
        break;
    }
    case kSeekpointEnd: {
        new_pos = length_;
        break;
    }

    default:
        return false;
    }

    new_pos += offset;

    // Note: allow position at the very end (last byte + 1).
    if (new_pos < 0 || new_pos > length_)
        return false;

    pos_ = new_pos;
    return true;
}

unsigned int MemFile::Write(const void *src, unsigned int size)
{
    (void)src;
    (void)size;

    I_Error("MemFile::Write called.\n");

    return 0; /* read only, cobber */
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
