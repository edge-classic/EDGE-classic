//----------------------------------------------------------------------------
//  EPI - String Interning and Mapping
//----------------------------------------------------------------------------
//
//  Copyright (c) 2024 The EDGE Team.
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
// Based on the ZDoom FName implementation with the following copyright:
//
// Copyright 2005-2007 Randy Heit
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 3. The name of the author may not be used to endorse or promote products
//    derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
// NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
// THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//---------------------------------------------------------------------------

#include "epi_ename.h"

#include <string.h>

#include "epi_str_compare.h"
#include "epi_str_util.h"

// The number of bytes to allocate to each NameBlock unless somebody is evil
// and wants a really long name. In that case, it gets its own NameBlock
// that is just large enough to hold it.
static constexpr int kBlockSize = 4096;

// How many entries to grow the NameArray by when it needs to grow.
static constexpr int kNameGrowAmount = 256;

namespace epi
{

// Name text is stored in a linked list of NameBlock structures. This
// is really the header for the block, with the remainder of the block
// being populated by text for names.
struct EName::NameManager::NameBlock
{
    size_t     next_alloc;
    NameBlock *next_block;
};

EName::NameManager EName::name_data_;
bool               EName::NameManager::inited_;
size_t             EName::NameManager::known_name_count_;

const char *predefined_names[] = {
#define EPI_XX(n)    #n,
#define EPI_XY(n, s) s,
#include "epi_known_enames.h"
#undef EPI_XX
#undef EPI_XY
};

// Returns the index of a name. If the name does not exist and noCreate is
// true, then it returns false. If the name does not exist and noCreate is
// false, then the name is added to the table and its new index is returned.
int EName::NameManager::FindName(std::string_view text, bool no_create)
{
    if (!inited_)
    {
        InitBuckets();
    }

    if (text.empty())
    {
        return 0;
    }

    std::string upper_text(text);
    epi::StringUpperASCII(upper_text);
    uint64_t hash     = StringHash64(upper_text);
    uint16_t bucket   = hash % kHashSize;
    int      scanner  = buckets_[bucket];
    size_t   text_len = upper_text.size();

    // See if the name already exists.
    while (scanner >= 0)
    {
        if (name_array_[scanner].hash == hash &&
            StringCompareMax(name_array_[scanner].text, upper_text, text_len) == 0 &&
            name_array_[scanner].text[text_len] == '\0')
        {
            return scanner;
        }
        scanner = name_array_[scanner].next_hash;
    }

    // If we get here, then the name does not exist.
    if (no_create)
    {
        return 0;
    }

    return AddName(upper_text, text_len, hash, bucket);
}

// Sets up the hash table and inserts all the default names into the table.
void EName::NameManager::InitBuckets()
{
    inited_ = true;
    memset(buckets_, -1, sizeof(buckets_));

    known_name_count_ = sizeof(predefined_names) / sizeof(const char *);

    // Register built-in names. 'None' must be name 0.
    for (size_t i = 0; i < known_name_count_; ++i)
    {
        EPI_ASSERT((0 == FindName(predefined_names[i], true)) && "Predefined name already inserted");
        FindName(predefined_names[i], false);
    }
}

// Adds a new name to the name table.
int EName::NameManager::AddName(std::string_view text, size_t text_len, uint64_t hash, uint16_t bucket)
{
    char      *textstore;
    NameBlock *block = blocks_;
    size_t     len   = text_len + 1;

    // Get a block large enough for the name. Only the first block in the
    // list is ever considered for name storage.
    if (block == nullptr || block->next_alloc + len >= kBlockSize)
    {
        block = AddBlock(len);
    }

    // Copy the string into the block.
    textstore = (char *)block + block->next_alloc;
    memcpy(textstore, text.data(), text_len);
    textstore[text_len] = '\0';
    block->next_alloc += len;

    // Add an entry for the name to the NameArray
    if (num_names_ >= max_names_)
    {
        // If no names have been defined yet, make the first allocation
        // large enough to hold all the predefined names.
        max_names_ += max_names_ == 0 ? known_name_count_ + kNameGrowAmount : kNameGrowAmount;

        name_array_ = (NameEntry *)realloc(name_array_, max_names_ * sizeof(NameEntry));
    }

    name_array_[num_names_].text      = textstore;
    name_array_[num_names_].hash      = hash;
    name_array_[num_names_].next_hash = buckets_[bucket];
    buckets_[bucket]                  = num_names_;

    return num_names_++;
}

// Creates a new NameBlock at least large enough to hold the required
// number of chars.
EName::NameManager::NameBlock *EName::NameManager::AddBlock(size_t len)
{
    NameBlock *block;

    len += sizeof(NameBlock);
    if (len < kBlockSize)
    {
        len = kBlockSize;
    }
    block             = (NameBlock *)malloc(len);
    block->next_alloc = sizeof(NameBlock);
    block->next_block = blocks_;
    blocks_           = block;
    return block;
}

// Release all the memory used for name bookkeeping.
EName::NameManager::~NameManager()
{
    NameBlock *block, *next;

    for (block = blocks_; block != nullptr; block = next)
    {
        next = block->next_block;
        free(block);
    }
    blocks_ = nullptr;

    if (name_array_ != nullptr)
    {
        free(name_array_);
        name_array_ = nullptr;
    }
    num_names_ = max_names_ = 0;
    memset(buckets_, -1, sizeof(buckets_));
}

} // namespace epi