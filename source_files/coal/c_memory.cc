//----------------------------------------------------------------------
//  COAL MEMORY BLOCKS
//----------------------------------------------------------------------
//
//  Copyright (C) 2021-2024 The EDGE Team
//  Copyright (C) 2009-2021  Andrew Apted
//
//  COAL is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as
//  published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  COAL is distributed in the hope that it will be useful, but
//  WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
//  the GNU General Public License for more details.
//
//----------------------------------------------------------------------

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "c_local.h"

namespace coal
{

MemoryBlockGroup::MemoryBlockGroup() : pos_(0)
{
    memset(blocks_, 0, sizeof(blocks_));
}

MemoryBlockGroup::~MemoryBlockGroup()
{
    for (int i = 0; i < 256; i++)
        if (blocks_[i])
            delete[] blocks_[i];
}

int MemoryBlockGroup::TryAlloc(int len)
{
    // look in previous blocks (waste less space)
    if (len <= 4096 && pos_ >= 10 && pos_ < 256)
        pos_ -= 10;

    while (pos_ < 256)
    {
        if (len > 4096)
        {
            // Special handling for "big" blocks:
            //
            // We allocate a number of contiguous MemoryBlock structures,
            // returning the address for the first one (allowing the
            // rest to be overwritten).
            //
            // Luckily the MemoryBlock destructor does nothing, delete[]
            // will call it for every MemoryBlock in the array.

            if (blocks_[pos_] && blocks_[pos_]->used > 0)
            {
                pos_++;
                continue;
            }

            if (blocks_[pos_])
                delete[] blocks_[pos_];

            int big_num = 1 + (len >> 12);

            blocks_[pos_]       = new MemoryBlock[big_num];
            blocks_[pos_]->used = len;

            return (pos_ << 12);
        }

        if (!blocks_[pos_])
            blocks_[pos_] = new MemoryBlock[1];

        if (blocks_[pos_]->used + len <= 4096)
        {
            int offset = blocks_[pos_]->used;

            blocks_[pos_]->used += len;

            return (pos_ << 12) | offset;
        }

        // try next block
        pos_++;
    }

    // no space left in this MemoryBlockGroup
    return -1;
}

void MemoryBlockGroup::Reset()
{
    for (int i = 0; i <= pos_; i++)
        if (blocks_[i])
        {
            // need to remove "big" blocks, otherwise the extra
            // space will go unused (a kind of memory leak).
            if (blocks_[i]->used > 4096)
            {
                delete[] blocks_[i];
                blocks_[i] = nullptr;
            }
            else
                blocks_[i]->used = 0;
        }

    pos_ = 0;
}

int MemoryBlockGroup::UsedMemory() const
{
    int result = 0;

    for (int i = 0; i <= pos_; i++)
        if (blocks_[i])
            result += blocks_[i]->used;

    return result;
}

int MemoryBlockGroup::TotalMemory() const
{
    int result = (int)sizeof(MemoryBlockGroup);

    for (int i = 0; i < 256; i++)
        if (blocks_[i])
        {
            int big_num = 1;
            if (blocks_[i]->used > 4096)
                big_num = 1 + (blocks_[i]->used >> 12);

            result += big_num * (int)sizeof(MemoryBlock);
        }

    return result;
}

//----------------------------------------------------------------------

MemoryManager::MemoryManager() : pos_(0)
{
    memset(groups_, 0, sizeof(groups_));
}

MemoryManager::~MemoryManager()
{
    for (int k = 0; k < 256; k++)
        if (groups_[k])
            delete groups_[k];
}

int MemoryManager::Alloc(int len)
{
    if (len == 0)
        return 0;

    for (;;)
    {
        assert(pos_ < 256);

        if (!groups_[pos_])
            groups_[pos_] = new MemoryBlockGroup;

        int result = groups_[pos_]->TryAlloc(len);

        if (result >= 0)
            return (pos_ << 20) | result;

        // try next group
        pos_++;
    }
}

void MemoryManager::Reset()
{
    for (int k = 0; k <= pos_; k++)
        if (groups_[k])
            groups_[k]->Reset();

    pos_ = 0;
}

int MemoryManager::UsedMemory() const
{
    int result = 0;

    for (int k = 0; k <= pos_; k++)
        if (groups_[k])
            result += groups_[k]->UsedMemory();

    return result;
}

int MemoryManager::TotalMemory() const
{
    int result = (int)sizeof(MemoryManager);

    for (int k = 0; k < 256; k++)
        if (groups_[k])
            result += groups_[k]->TotalMemory();

    return result;
}

} // namespace coal

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
