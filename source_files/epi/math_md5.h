//------------------------------------------------------------------------
//  EDGE MD5 : Message-Digest (Secure Hash)
//------------------------------------------------------------------------
// 
//  Copyright (c) 2003-2008  The EDGE Team.
// 
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//------------------------------------------------------------------------
//
//  Based on the MD5 code in the Linux kernel, which says:
//
//  |  The code for MD5 transform was taken from Colin Plumb's
//  |  implementation, which has been placed in the public domain.  The
//  |  MD5 cryptographic checksum was devised by Ronald Rivest, and is
//  |  documented in RFC 1321, "The MD5 Message Digest Algorithm".
//
//------------------------------------------------------------------------

#ifndef __EPI_MD5_H__
#define __EPI_MD5_H__

namespace epi
{

class md5hash_c
{
	/* sealed */

public:
	byte hash[16];

	md5hash_c();
	md5hash_c(const byte *message, unsigned int len);

	~md5hash_c()
  { }

	void Compute(const byte *message, unsigned int len);

private:
	// a class used while computing the MD5 sum.
	// Not actually used with a member variable.

  class packhash_c
  {
	public:
		u32_t pack[4];

		packhash_c();
		~packhash_c() { }

		void Transform(const u32_t extra[16]);
		void TransformBytes(const byte chunk[64]);
		void Encode(byte *hash);
  };
};

} // namespace epi

//------------------------------------------------------------------------
//  
//  DEBUG STUFF
//

#ifdef DEBUG_EPI
namespace debugepi
{
	void TestMd5Hash();
};
#endif  // DEBUG_EPI

#endif  /* __EPI_MD5_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
