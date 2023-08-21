//----------------------------------------------------------------------------
//  Subset of an existing File
//----------------------------------------------------------------------------
//
//  Copyright (c) 2007-2023  The EDGE Team.
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

namespace epi
{

sub_file_c::sub_file_c(file_c *_parent, int _start, int _len) :
	parent(_parent), start(_start), length(_len), pos(0)
{
	SYS_ASSERT(parent != NULL);
	SYS_ASSERT(start  >= 0);
	SYS_ASSERT(length >= 0);
}


sub_file_c::~sub_file_c()
{
	parent = NULL;
}


unsigned int sub_file_c::Read(void *dest, unsigned int size)
{
	// EOF ?
	if (pos >= length)
		return 0;

	size = std::min(size, (unsigned int) (length - pos));

	// we must always seek before a read, because other things may also be
	// reading the parent file.
	parent->Seek(start + pos, SEEKPOINT_START);

	unsigned int got = parent->Read(dest, size);

	pos += (int)got;

	return got;
}


bool sub_file_c::Seek(int offset, int seekpoint)
{
	int new_pos = 0;

    switch (seekpoint)
    {
        case SEEKPOINT_START:   { new_pos = 0;      break; }
        case SEEKPOINT_CURRENT: { new_pos = pos;    break; }
        case SEEKPOINT_END:     { new_pos = length; break; }

        default: return false;
    }

	new_pos += offset;

	// NOTE: we allow position at the very end (last byte + 1).
	if (new_pos < 0 || new_pos > length)
		return false;

	pos = new_pos;

	return true;
}


unsigned int sub_file_c::Write(const void *src, unsigned int size)
{
	I_Error("sub_file_c::Write called.\n");

	return 0;  /* read only, cobber */
}


} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
