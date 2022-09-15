//----------------------------------------------------------------------------
//  EDGE PK3 Support Code
//----------------------------------------------------------------------------
//
//  Copyright (c) 2022  The EDGE Team.
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

#include "i_defs.h"

#include <list>
#include <vector>
#include <algorithm>

#include "miniz.h"

#include "w_files.h"


class pack_entry_c
{
public:
	std::string name;

	// position, length

public:
	pack_entry_c(const char *_name) : name(_name)
	{ }

	~pack_entry_c()
	{ }
};


class pack_file_c
{
public:
	std::vector<pack_entry_c> entries;

public:
	pack_file_c()
	{ }

	~pack_file_c()
	{ }
};

//----------------------------------------------------------------------------

void ProcessPackage(data_file_c *df, size_t file_index)
{
	// TODO
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
