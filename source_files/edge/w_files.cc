//----------------------------------------------------------------------------
//  EDGE file handling
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
//
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------

#include "i_defs.h"

#include <list>
#include <vector>
#include <algorithm>

#include "w_files.h"


std::vector<data_file_c *> data_files;


data_file_c::data_file_c(const char *_name, int _kind) :
		name(_name), kind(_kind), file(NULL), wad(NULL)
{ }

data_file_c::~data_file_c()
{ }


int W_GetNumFiles(void)
{
	return (int)data_files.size();
}

//----------------------------------------------------------------------------

//
// W_AddFilename
//
size_t W_AddFilename(const char *file, int kind)
{
	I_Debugf("Added filename: %s\n", file);

	size_t index = data_files.size();

	data_file_c *df = new data_file_c(file, kind);
	data_files.push_back(df);

	return index;
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
