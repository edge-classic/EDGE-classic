//----------------------------------------------------------------------
//  COAL MEMORY BLOCKS
//----------------------------------------------------------------------
// 
//  Copyright (C) 2009  Andrew Apted
//
//  Coal is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as
//  published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  Coal is distributed in the hope that it will be useful, but
//  WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
//  the GNU General Public License for more details.
//
//----------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#include <vector>

#include "coal.h"


namespace coal
{

#include "c_local.h"
#include "c_memory.h"


bgroup_c::bgroup_c() : pos(0)
{
	memset(blocks, 0, sizeof(blocks));
}

bgroup_c::~bgroup_c()
{
	for (int i = 0; i < 256; i++)
		if (blocks[i])
			delete[] blocks[i];
}

int bgroup_c::try_alloc(int len)
{
	// look in previous blocks (waste less space)
	if (len <= 4096 && pos >= 10 && pos < 256)
		pos -= 10;

	while (pos < 256)
	{
		if (len > 4096)
		{
			// Special handling for "big" blocks:
			//
			// We allocate a number of contiguous block_c structures,
			// returning the address for the first one (allowing the
			// rest to be overwritten).
			//
			// Luckily the block_c destructor does nothing, delete[]
			// will call it for every block_c in the array.

			if (blocks[pos] && blocks[pos]->used > 0)
			{
				pos++;
				continue;
			}

			if (blocks[pos])
				delete[] blocks[pos];

			int big_num = 1 + (len >> 12);

			blocks[pos] = new block_c[big_num];
			blocks[pos]->used = len;

			return (pos << 12);
		}

		if (! blocks[pos])
			blocks[pos] = new block_c[1];

		if (blocks[pos]->used + len <= 4096)
		{
			int offset = blocks[pos]->used;

			blocks[pos]->used += len;

			return (pos << 12) | offset;
		}

		// try next block
		pos++;
	}

	// no space left in this bgroup_c
	return -1;
}

void bgroup_c::reset()
{
	for (int i = 0; i <= pos; i++)
		if (blocks[i])
		{
			// need to remove "big" blocks, otherwise the extra
			// space will go unused (a kind of memory leak).
			if (blocks[i]->used > 4096)
			{
				delete[] blocks[i];
				blocks[i] = NULL;
			}
			else
				blocks[i]->used = 0;
		}

	pos = 0;
}

int bgroup_c::usedMemory() const
{
	int result = 0;

	for (int i = 0; i <= pos; i++)
		if (blocks[i])
			result += blocks[i]->used;

	return result;
}

int bgroup_c::totalMemory() const
{
	int result = (int)sizeof(bgroup_c);

	for (int i = 0; i < 256; i++)
		if (blocks[i])
		{
			int big_num = 1;
			if (blocks[i]->used > 4096)
				big_num = 1 + (blocks[i]->used >> 12);

			result += big_num * (int)sizeof(block_c);
		}

	return result;
}


//----------------------------------------------------------------------


bmaster_c::bmaster_c() : pos(0)
{
	memset(groups, 0, sizeof(groups));
}

bmaster_c::~bmaster_c()
{
	for (int k = 0; k < 256; k++)
		if (groups[k])
			delete groups[k];
}


int bmaster_c::alloc(int len)
{
	if (len == 0)
		return 0;

	for (;;)
	{
		assert(pos < 256);

		if (! groups[pos])
			groups[pos] = new bgroup_c;

		int result = groups[pos]->try_alloc(len);

		if (result >= 0)
			return (pos << 20) | result;

		// try next group
		pos++;
	}
}


void bmaster_c::reset()
{
	for (int k = 0; k <= pos; k++)
		if (groups[k])
			groups[k]->reset();

	pos = 0;
}


int bmaster_c::usedMemory() const
{
	int result = 0;

	for (int k = 0; k <= pos; k++)
		if (groups[k])
			result += groups[k]->usedMemory();

	return result;
}

int bmaster_c::totalMemory() const
{
	int result = (int)sizeof(bmaster_c);

	for (int k = 0; k < 256; k++)
		if (groups[k])
			result += groups[k]->totalMemory();

	return result;
}


}  // namespace coal

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
