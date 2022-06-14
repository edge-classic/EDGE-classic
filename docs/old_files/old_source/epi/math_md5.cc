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

#include "epi.h"
#include "math_md5.h"

/* The four core functions - F1 is optimized somewhat */

#define F1(x, y, z)  (z ^ (x & (y ^ z)))
#define F2(x, y, z)  F1(z, x, y)
#define F3(x, y, z)  (x ^ y ^ z)
#define F4(x, y, z)  (y ^ (x | ~z))

/* This is the central step in the MD5 algorithm. */

#define MD5_STEP(f, w, x, y, z, data, s)  \
	(w += f(x, y, z) + (data),  w = (w<<s) | (w>>(32-s)),  w += x)

namespace epi
{

/* The core of the MD5 algorithm, this alters an existing MD5 hash to
 * reflect the addition of 16 longwords of new data.
 */
void md5hash_c::packhash_c::Transform(const u32_t extra[16])
{
	u32_t  a, b, c, d;

	a = pack[0];
	b = pack[1];
	c = pack[2];
	d = pack[3];

	MD5_STEP(F1, a, b, c, d, extra[ 0] + 0xd76aa478,  7);
	MD5_STEP(F1, d, a, b, c, extra[ 1] + 0xe8c7b756, 12);
	MD5_STEP(F1, c, d, a, b, extra[ 2] + 0x242070db, 17);
	MD5_STEP(F1, b, c, d, a, extra[ 3] + 0xc1bdceee, 22);
	MD5_STEP(F1, a, b, c, d, extra[ 4] + 0xf57c0faf,  7);
	MD5_STEP(F1, d, a, b, c, extra[ 5] + 0x4787c62a, 12);
	MD5_STEP(F1, c, d, a, b, extra[ 6] + 0xa8304613, 17);
	MD5_STEP(F1, b, c, d, a, extra[ 7] + 0xfd469501, 22);
	MD5_STEP(F1, a, b, c, d, extra[ 8] + 0x698098d8,  7);
	MD5_STEP(F1, d, a, b, c, extra[ 9] + 0x8b44f7af, 12);
	MD5_STEP(F1, c, d, a, b, extra[10] + 0xffff5bb1, 17);
	MD5_STEP(F1, b, c, d, a, extra[11] + 0x895cd7be, 22);
	MD5_STEP(F1, a, b, c, d, extra[12] + 0x6b901122,  7);
	MD5_STEP(F1, d, a, b, c, extra[13] + 0xfd987193, 12);
	MD5_STEP(F1, c, d, a, b, extra[14] + 0xa679438e, 17);
	MD5_STEP(F1, b, c, d, a, extra[15] + 0x49b40821, 22);

	MD5_STEP(F2, a, b, c, d, extra[ 1] + 0xf61e2562,  5);
	MD5_STEP(F2, d, a, b, c, extra[ 6] + 0xc040b340,  9);
	MD5_STEP(F2, c, d, a, b, extra[11] + 0x265e5a51, 14);
	MD5_STEP(F2, b, c, d, a, extra[ 0] + 0xe9b6c7aa, 20);
	MD5_STEP(F2, a, b, c, d, extra[ 5] + 0xd62f105d,  5);
	MD5_STEP(F2, d, a, b, c, extra[10] + 0x02441453,  9);
	MD5_STEP(F2, c, d, a, b, extra[15] + 0xd8a1e681, 14);
	MD5_STEP(F2, b, c, d, a, extra[ 4] + 0xe7d3fbc8, 20);
	MD5_STEP(F2, a, b, c, d, extra[ 9] + 0x21e1cde6,  5);
	MD5_STEP(F2, d, a, b, c, extra[14] + 0xc33707d6,  9);
	MD5_STEP(F2, c, d, a, b, extra[ 3] + 0xf4d50d87, 14);
	MD5_STEP(F2, b, c, d, a, extra[ 8] + 0x455a14ed, 20);
	MD5_STEP(F2, a, b, c, d, extra[13] + 0xa9e3e905,  5);
	MD5_STEP(F2, d, a, b, c, extra[ 2] + 0xfcefa3f8,  9);
	MD5_STEP(F2, c, d, a, b, extra[ 7] + 0x676f02d9, 14);
	MD5_STEP(F2, b, c, d, a, extra[12] + 0x8d2a4c8a, 20);

	MD5_STEP(F3, a, b, c, d, extra[ 5] + 0xfffa3942,  4);
	MD5_STEP(F3, d, a, b, c, extra[ 8] + 0x8771f681, 11);
	MD5_STEP(F3, c, d, a, b, extra[11] + 0x6d9d6122, 16);
	MD5_STEP(F3, b, c, d, a, extra[14] + 0xfde5380c, 23);
	MD5_STEP(F3, a, b, c, d, extra[ 1] + 0xa4beea44,  4);
	MD5_STEP(F3, d, a, b, c, extra[ 4] + 0x4bdecfa9, 11);
	MD5_STEP(F3, c, d, a, b, extra[ 7] + 0xf6bb4b60, 16);
	MD5_STEP(F3, b, c, d, a, extra[10] + 0xbebfbc70, 23);
	MD5_STEP(F3, a, b, c, d, extra[13] + 0x289b7ec6,  4);
	MD5_STEP(F3, d, a, b, c, extra[ 0] + 0xeaa127fa, 11);
	MD5_STEP(F3, c, d, a, b, extra[ 3] + 0xd4ef3085, 16);
	MD5_STEP(F3, b, c, d, a, extra[ 6] + 0x04881d05, 23);
	MD5_STEP(F3, a, b, c, d, extra[ 9] + 0xd9d4d039,  4);
	MD5_STEP(F3, d, a, b, c, extra[12] + 0xe6db99e5, 11);
	MD5_STEP(F3, c, d, a, b, extra[15] + 0x1fa27cf8, 16);
	MD5_STEP(F3, b, c, d, a, extra[ 2] + 0xc4ac5665, 23);

	MD5_STEP(F4, a, b, c, d, extra[ 0] + 0xf4292244,  6);
	MD5_STEP(F4, d, a, b, c, extra[ 7] + 0x432aff97, 10);
	MD5_STEP(F4, c, d, a, b, extra[14] + 0xab9423a7, 15);
	MD5_STEP(F4, b, c, d, a, extra[ 5] + 0xfc93a039, 21);
	MD5_STEP(F4, a, b, c, d, extra[12] + 0x655b59c3,  6);
	MD5_STEP(F4, d, a, b, c, extra[ 3] + 0x8f0ccc92, 10);
	MD5_STEP(F4, c, d, a, b, extra[10] + 0xffeff47d, 15);
	MD5_STEP(F4, b, c, d, a, extra[ 1] + 0x85845dd1, 21);
	MD5_STEP(F4, a, b, c, d, extra[ 8] + 0x6fa87e4f,  6);
	MD5_STEP(F4, d, a, b, c, extra[15] + 0xfe2ce6e0, 10);
	MD5_STEP(F4, c, d, a, b, extra[ 6] + 0xa3014314, 15);
	MD5_STEP(F4, b, c, d, a, extra[13] + 0x4e0811a1, 21);
	MD5_STEP(F4, a, b, c, d, extra[ 4] + 0xf7537e82,  6);
	MD5_STEP(F4, d, a, b, c, extra[11] + 0xbd3af235, 10);
	MD5_STEP(F4, c, d, a, b, extra[ 2] + 0x2ad7d2bb, 15);
	MD5_STEP(F4, b, c, d, a, extra[ 9] + 0xeb86d391, 21);

	pack[0] += a;
	pack[1] += b;
	pack[2] += c;
	pack[3] += d;
}

md5hash_c::packhash_c::packhash_c()
{
	pack[0] = 0x67452301;
	pack[1] = 0xefcdab89;
	pack[2] = 0x98badcfe;
	pack[3] = 0x10325476;
}

void md5hash_c::packhash_c::TransformBytes(const byte chunk[64])
{
	u32_t extra[16];

	for (int pos=0; pos < 16; pos++, chunk += 4)
	{
		extra[pos] = 
			(chunk[0])       |
			(chunk[1] <<  8) |
			(chunk[2] << 16) |
			(chunk[3] << 24);
	}

	Transform(extra);
}

void md5hash_c::packhash_c::Encode(byte *hash)
{
	for (int pos=0; pos < 4; pos++)
	{
		*hash++ = (pack[pos]      ) & 0xff;
		*hash++ = (pack[pos] >>  8) & 0xff;
		*hash++ = (pack[pos] >> 16) & 0xff;
		*hash++ = (pack[pos] >> 24) & 0xff;
	}
}


//------------------------------------------------------------------------


md5hash_c::md5hash_c()
{
  memset(hash, 0, sizeof(hash));
}

md5hash_c::md5hash_c(const byte *message, unsigned int len)
{
  Compute(message, len);
}

void md5hash_c::Compute(const byte *message, unsigned int len)
{
	packhash_c packed;

	int bit_length = len * 8;

	for (; len >= 64; message += 64, len -= 64)
	{
		packed.TransformBytes(message);
	}

	byte buffer[128];

	if (len > 0)
	{
		memcpy(buffer, message, len);
	}

	/* add single "1" bit */

	buffer[len++] = 0x80;

	/* pad remaining area with zero bits, so that the length becomes
	 * congruous with 448 bits (56 bytes).
	 */

	while ((len % 64) != 56)
	{
		buffer[len++] = 0;
	}

	buffer[len++] = (bit_length      ) & 0xff;
	buffer[len++] = (bit_length >>  8) & 0xff;
	buffer[len++] = (bit_length >> 16) & 0xff;
	buffer[len++] = (bit_length >> 24) & 0xff;

	/* NOTE: we don't support more than 32 bit lengths.  The ANSI C
	 * standard says that the result of >> is undefined if the shift
	 * amount is greater than the number of bits in the left operand.
	 */
	buffer[len++] = 0;
	buffer[len++] = 0;
	buffer[len++] = 0;
	buffer[len++] = 0;

	/// ASSERT(len == 64 || len == 128);

	packed.TransformBytes(buffer);

	if (len == 128)
	{
		packed.TransformBytes(buffer + 64);
	}

	packed.Encode(hash);
}

} // namespace epi


//------------------------------------------------------------------------
//  
//  DEBUG STUFF
//

#ifdef DEBUG_EPI

#include <iostream>

namespace debugepi
{
	void Test_Md5Hash_Write(const epi::md5hash_c& hash);
	void Test_Md5Hash_DoString(const char *str);

	void Test_Md5Hash_Write(const epi::md5hash_c& hash)
	{
		char num_buf[40];

		for (int pos=0; pos < 16; pos++)
		{
			sprintf(num_buf, "%02x", hash.hash[pos]);

			cout << num_buf;
		}
	}

	void Test_Md5Hash_DoString(const char *str)
	{
		cout << "MD5 (\"" << str << "\") = ";

		/// ASSERT(sizeof(char) == sizeof(byte));

		epi::md5hash_c hash((const byte *) str, (unsigned int) strlen(str));

		Test_Md5Hash_Write(hash);

		cout << "\n";
	}

	void Test_Md5Hash()
	{
		cout << "\n===TEST-MD5-HASH==========================\n\n";

		Test_Md5Hash_DoString("");
		Test_Md5Hash_DoString("a");
		Test_Md5Hash_DoString("abc");
		Test_Md5Hash_DoString("message digest");

		cout << "\n==========================================\n\n";
	}
};

#endif  // DEBUG_EPI


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
