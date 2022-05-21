//----------------------------------------------------------------------------
//  Block of memory with File interface
//----------------------------------------------------------------------------
//
//  Copyright (c) 2007-2008  The EDGE Team.
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

#include "file_memory.h"

namespace epi
{

//
// Constructor
//
mem_file_c::mem_file_c(const byte *_block, int _len, bool copy_it)
{
	SYS_ASSERT(_block);
	SYS_ASSERT(_len >= 0);

	pos = 0;
	copied = false;

	if (_len == 0)
	{
		data = NULL;
		length = 0;
		return;
	}

	if (copy_it)
	{
		data = new byte[_len];
		length = _len;

		memcpy(data, _block, _len);
		copied = true;
	}
	else
	{
		data = (byte *)_block;
		length = _len;
	}
}

//
// Destructor
//
mem_file_c::~mem_file_c()
{
	if (data && copied)
	{
		delete[] data;
		data = NULL;
	}

	length = 0;
}

unsigned int mem_file_c::Read(void *dest, unsigned int size)
{
	SYS_ASSERT(dest);
	SYS_ASSERT(size >= 0);
		
	unsigned int avail = length - pos;

	if (size > avail)
		size = avail;

	if (size == 0)
		return 0;  // EOF

	memcpy(dest, data + pos, size);
	pos += size;

	return size;
}

bool mem_file_c::Seek(int offset, int seekpoint)
{
	int new_pos = 0;

    switch (seekpoint)
    {
        case SEEKPOINT_START:   { new_pos = 0; break; }
        case SEEKPOINT_CURRENT: { new_pos = pos; break; }
        case SEEKPOINT_END:     { new_pos = length; break; }

        default:
			return false;
    }

	new_pos += offset;

	// Note: allow position at the very end (last byte + 1).
	if (new_pos < 0 || new_pos > length)
		return false;

	pos = new_pos;
	return true;
}

unsigned int mem_file_c::Write(const void *src, unsigned int size)
{
	// FIXME

	I_Error("mem_file_c::Write called.\n");

	return 0;  /* read only, cobber */
}


} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
