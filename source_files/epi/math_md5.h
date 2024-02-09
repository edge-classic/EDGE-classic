//------------------------------------------------------------------------
//  EDGE MD5 : Message-Digest (Secure Hash)
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
//  Based on the MD5 code in the Linux kernel, which says:
//
//  |  The code for MD5 transform was taken from Colin Plumb's
//  |  implementation, which has been placed in the public domain.  The
//  |  MD5 cryptographic checksum was devised by Ronald Rivest, and is
//  |  documented in RFC 1321, "The MD5 Message Digest Algorithm".
//
//------------------------------------------------------------------------

#pragma once

#include <stdint.h>

#include <string>

namespace epi
{

class MD5Hash
{
    /* sealed */
   private:
    uint8_t hash_[16];

   public:
    MD5Hash();
    MD5Hash(const uint8_t *message, unsigned int len);

    ~MD5Hash() {}

    void Compute(const uint8_t *message, unsigned int len);

    std::string ToString();

   private:
    // a class used while computing the MD5 sum.
    // Not actually used with a member variable.

    class PackHash
    {
       public:
        uint32_t pack_[4];

        PackHash();
        ~PackHash() {}

        void Transform(const uint32_t extra[16]);
        void TransformBytes(const uint8_t chunk[64]);
        void Encode(uint8_t *hash);
    };
};

}  // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
