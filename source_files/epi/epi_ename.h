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

#pragma once

#include <stdint.h>

#include <string>

#include "epi.h"

namespace epi
{

enum KnownEName
{
#define EPI_XX(n)    kEName##n,
#define EPI_XY(n, s) kEName##n,
#include "epi_known_enames.h"
#undef EPI_XX
#undef EPI_XY
};

class EName
{
  public:
    EName() = default;
    EName(std::string_view text)
    {
        index_ = name_data_.FindName(text, false);
    }
    EName(std::string_view text, bool no_create)
    {
        index_ = name_data_.FindName(text, no_create);
    }

    EName(const EName &other) = default;
    EName(KnownEName index)
    {
        index_ = index;
    }

    int GetIndex() const
    {
        return index_;
    }
    const char *GetChars() const
    {
        return name_data_.name_array_[index_].text;
    }

    EName &operator=(std::string_view text)
    {
        index_ = name_data_.FindName(text, false);
        return *this;
    }
    EName &operator=(const EName &other) = default;
    EName &operator=(KnownEName index)
    {
        index_ = index;
        return *this;
    }

    int SetName(std::string_view text, bool no_create = false)
    {
        return index_ = name_data_.FindName(text, no_create);
    }

    bool IsValidName() const
    {
        return (unsigned)index_ < (unsigned)name_data_.num_names_;
    }

    // Note that the comparison operators compare the names' indices, not
    // their text, so they cannot be used to do a lexicographical sort.
    bool operator==(const EName &other) const
    {
        return index_ == other.index_;
    }
    bool operator!=(const EName &other) const
    {
        return index_ != other.index_;
    }
    bool operator<(const EName &other) const
    {
        return index_ < other.index_;
    }
    bool operator<=(const EName &other) const
    {
        return index_ <= other.index_;
    }
    bool operator>(const EName &other) const
    {
        return index_ > other.index_;
    }
    bool operator>=(const EName &other) const
    {
        return index_ >= other.index_;
    }

    bool operator==(KnownEName index) const
    {
        return index_ == index;
    }
    bool operator!=(KnownEName index) const
    {
        return index_ != index;
    }
    bool operator<(KnownEName index) const
    {
        return index_ < index;
    }
    bool operator<=(KnownEName index) const
    {
        return index_ <= index;
    }
    bool operator>(KnownEName index) const
    {
        return index_ > index;
    }
    bool operator>=(KnownEName index) const
    {
        return index_ >= index;
    }

  protected:
    int index_;

    struct NameEntry
    {
        char        *text;
        uint64_t     hash;
        int          next_hash;
    };

    class NameManager
    {
        friend class EName;

      private:
        // No constructor because we can't ensure that it actually gets
        // called before any ENames are constructed during startup. This
        // means this class must only exist in the program's BSS section.
        ~NameManager();

        static constexpr uint16_t kHashSize = 1024;
        struct NameBlock;

        NameBlock *blocks_;
        NameEntry *name_array_;
        int        num_names_, max_names_;
        int        buckets_[kHashSize];

        int           FindName(std::string_view text, bool no_create);
        int           AddName(std::string_view text, size_t text_len, uint64_t hash, uint16_t bucket);
        NameBlock    *AddBlock(size_t len);
        void          InitBuckets();
        static bool   inited_;
        static size_t known_name_count_;
    };

    static NameManager name_data_;
};

} // namespace epi