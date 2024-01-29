//----------------------------------------------------------------------------
//  Subset of an existing File
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

#include "file_sub.h"

#include "HandmadeMath.h"

namespace epi
{

SubFile::SubFile(File *parent, int start, int len) : parent_(parent), start_(start), length_(len), pos_(0)
{
    SYS_ASSERT(parent_ != nullptr);
    SYS_ASSERT(start_ >= 0);
    SYS_ASSERT(length_ >= 0);
}

SubFile::~SubFile()
{
    parent_ = nullptr;
}

unsigned int SubFile::Read(void *dest, unsigned int size)
{
    // EOF ?
    if (pos_ >= length_)
        return 0;

    size = HMM_MIN(size, (unsigned int)(length_ - pos_));

    // we must always seek before a read, because other things may also be
    // reading the parent file.
    parent_->Seek(start_ + pos_, kSeekpointStart);

    unsigned int got = parent_->Read(dest, size);

    pos_ += (int)got;

    return got;
}

bool SubFile::Seek(int offset, int seekpoint)
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

    // NOTE: we allow position at the very end (last byte + 1).
    if (new_pos < 0 || new_pos > length_)
        return false;

    pos_ = new_pos;

    return true;
}

unsigned int SubFile::Write(const void *src, unsigned int size)
{
    (void)src;
    (void)size;

    I_Error("SubFile::Write called.\n");

    return 0; /* read only, cobber */
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
