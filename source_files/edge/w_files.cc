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


data_file_c::data_file_c(const char *_fname, int _kind, epi::file_c*_file) :
		file_name(NULL), kind(_kind), file(_file), wad(NULL)
{
	file_name = strdup(_fname);
}

data_file_c::~data_file_c()
{
	free((void*)file_name);
}


int W_GetNumFiles(void)
{
	return (int)data_files.size();
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
