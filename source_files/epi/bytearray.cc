//----------------------------------------------------------------------------
//  EDGE ByteArray Class
//----------------------------------------------------------------------------
//
//  Copyright (c) 2002-2008  The EDGE Team.
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
//----------------------------------------------------------------------------

#include "epi.h"
#include "bytearray.h"


namespace epi
{

bytearray_c::bytearray_c(int initial_size) :
	array(NULL), length(initial_size), space(0)
{
	if (length > 0)
		array = new byte[length];
}

bytearray_c::~bytearray_c()
{
	if (array)
	{
		delete[] array;
		array = NULL;
	}
}

void bytearray_c::Resize(int new_len)
{
	SYS_ASSERT(new_len >= 0);

	Resize(new_len, 0);
}

void bytearray_c::Resize(int new_len, int new_space)
{
	// ASSERT(new_len >= 0);
	// ASSERT(new_space >= 0);

	int total = new_len + new_space;

	if (total == 0)
	{
		if (array)
		{
			delete[] array;
			array = NULL;
		}

		length = 0;
		space  = 0;
		return;
	}

	if (length + space == total)
	{
		length = new_len;
		space  = new_space;
		return;
	}

	byte *new_array = new byte[total];

	if (array)
	{
		if (new_len > 0 && length > 0)
			memcpy(new_array, array, MIN(length, new_len));

		delete[] array;
	}

	array  = new_array;
	length = new_len;
	space  = new_space;
}

void bytearray_c::Clear()
{
	space += length;
	length = 0;
}

void bytearray_c::Read(int pos, void *data, int count) const
{
	if (count != 0)
	{
		SYS_ASSERT(data);
		SYS_ASSERT(count >= 0 && pos >= 0 && pos + count <= length);
		
		memcpy(data, array + pos, count);
	}
}

void bytearray_c::Write(int pos, const void *data, int count)
{
	if (count != 0)
	{
		SYS_ASSERT(data);
		SYS_ASSERT(count >= 0 && pos >= 0 && pos + count <= length);

		memcpy(array + pos, data, count);
	}
}

byte& bytearray_c::operator[](int pos) const
{
#ifndef NDEBUG
	if (pos < 0 || pos >= length)
		I_Error("bytearray_c::operator[] out of bounds (%d >= %d)\n",
				pos, length);
#endif

	return array[pos];
}

void bytearray_c::Append(byte value)
{
	RequireSpace(1);

	array[length++] = value;

	space--;

	// ASSERT(space >= 0);
}

void bytearray_c::Append(const void *data, int count)
{
	if (count == 0)
		return;

	RequireSpace(count);

	memcpy(array + length, data, count);

	length += count;
	space  -= count;

	// ASSERT(space >= 0);
}

}  // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
