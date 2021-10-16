//------------------------------------------------------------------------
//  EDGE CRC : Cyclic Rendundancy Check
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
//  Based on the Adler-32 algorithm as described in RFC-1950.
//
//------------------------------------------------------------------------

#include "epi.h"
#include "math_crc.h"

namespace epi
{

// ---- Primitive routines ----
crc32_c& crc32_c::operator+= (byte data)
{
	u32_t s1 = crc & 0xFFFF;
	u32_t s2 = (crc >> 16) & 0xFFFF;

	s1 = (s1 + data) % 65521;
	s2 = (s2 + s1)   % 65521;

	crc = (s2 << 16) | s1;

	return *this;
}

crc32_c& crc32_c::AddBlock(const byte *data, int len)
{
	/// ASSERT(len >= 0);

	u32_t s1 = crc & 0xFFFF;
	u32_t s2 = (crc >> 16) & 0xFFFF;

	for (; len > 0; data++, len--)
	{
		s1 = (s1 + *data) % 65521;
		s2 = (s2 + s1)    % 65521;
	}

	crc = (s2 << 16) | s1;

	return *this;
}


// ---- non-primitive routines ----


crc32_c& crc32_c::operator+= (s32_t value)
{
	*this += static_cast<byte> (value >> 24);
	*this += static_cast<byte> (value >> 16);
	*this += static_cast<byte> (value >> 8);
	*this += static_cast<byte> (value);

	return *this;
}

crc32_c& crc32_c::operator+= (u32_t value)
{
	*this += static_cast<byte> (value >> 24);
	*this += static_cast<byte> (value >> 16);
	*this += static_cast<byte> (value >> 8);
	*this += static_cast<byte> (value);

	return *this;
}

crc32_c& crc32_c::operator+= (float value)
{
	bool neg = (value < 0.0f);
	value = (float)fabs(value);

	int exp;
	int mant = static_cast<int> (ldexp(frexp(value, &exp), 30));

	*this += static_cast<byte> (neg ? '-' : '+');
	*this += exp;
	*this += mant;

	return *this;
}

crc32_c& crc32_c::AddCStr(const char *str)
{
	for (; *str; str++)
	{
		*this += static_cast<byte> (*str);
	}

	return *this;
}

}  // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
