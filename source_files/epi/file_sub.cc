//----------------------------------------------------------------------------
//  Subset of an existing File
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

#include "file_sub.h"

namespace epi
{

//
// Constructor
//
sub_file_c::sub_file_c(file_c *_par, int _start, int _len) :
	parent(_par), start(_start), length(_len), remain(_len)
{
	SYS_ASSERT(parent);
	SYS_ASSERT(start >= 0);
	SYS_ASSERT(length >= 0);

	parent->Seek(start, SEEKPOINT_START);
}

//
// Destructor
//
sub_file_c::~sub_file_c()
{
	start = length = -1;
}

int sub_file_c::GetPosition()
{
	return length - remain;
#if 0
	int par_pos = parent->GetPosition();

	int result = par_pos - start;

	if (result < 0) // oopsie
		return 0;

	if (result > length)
		return length;
	
	return result;
#endif
}

unsigned int sub_file_c::Read(void *dest, unsigned int size)
{
	if ((int)size > remain)
		size = remain;

	if (size <= 0)
		return 0;  // EOF

	int read_len = parent->Read(dest, size);

	remain -= read_len;

	return read_len;
}

bool sub_file_c::Seek(int offset, int seekpoint)
{
	int new_pos = 0;

    switch (seekpoint)
    {
        case SEEKPOINT_START:   { new_pos = 0; break; }
        case SEEKPOINT_CURRENT: { new_pos = GetPosition(); break; }
        case SEEKPOINT_END:     { new_pos = length; break; }

        default:
			return false;
    }

	new_pos += offset;

	// Note: allow position at the very end (last byte + 1).
	if (new_pos < 0 || new_pos > length)
		return false;

	remain = length - new_pos;
		
	return parent->Seek(start + new_pos, SEEKPOINT_START);
}

unsigned int sub_file_c::Write(const void *src, unsigned int size)
{
	I_Error("sub_file_c::Write called.\n");

	return 0;  /* read only, cobber */
}


} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
