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

// EPI
#include "file.h"
#include "file_sub.h"
#include "filesystem.h"

// DDF
#include "wadfixes.h"

#include "dstrings.h"
#include "w_files.h"
#include "w_wad.h"


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

//----------------------------------------------------------------------------

std::vector<data_file_c *> pending_files;

size_t W_AddPending(const char *file, int kind)
{
	size_t index = pending_files.size();

	data_file_c *df = new data_file_c(file, kind);
	pending_files.push_back(df);

	return index;
}

// TODO tidy this
extern void ProcessFixers(data_file_c *df);
extern void ProcessDehacked(data_file_c *df);
extern void ProcessWad(data_file_c *df, size_t file_index);
extern void ProcessSingleLump(data_file_c *df);

extern std::string W_BuildNodesForWad(data_file_c *df);


void W_ReadWADFIXES(void)
{
	I_Printf("Loading WADFIXES\n");

	int length;
	char *data = (char *) W_LoadLump("WADFIXES", &length);

	DDF_ReadFixes(data, length);

	W_DoneWithLump(data);
}


static void ProcessFile(data_file_c *df)
{
	size_t file_index = data_files.size();
	data_files.push_back(df);

	// open the file and add to directory
	const char *filename = df->name.c_str();

    epi::file_c *file = epi::FS_Open(filename, epi::file_c::ACCESS_READ | epi::file_c::ACCESS_BINARY);
	if (file == NULL)
	{
		I_Error("Couldn't open file %s\n", filename);
		return;
	}

	I_Printf("  Adding %s\n", filename);

	if (file_index == 1)  // 1 means "edge-defs.wad"
		W_ReadWADFIXES();

	df->file = file;  // FIXME review lifetime of this open file

	// for RTS scripts, adding the data_file is enough
	if (df->kind == FLKIND_RTS)
		return;

	if (df->kind <= FLKIND_HWad)
	{
		ProcessWad(df, file_index);
	}
	else
	{
		ProcessSingleLump(df);
	}

	// handle DeHackEd patch files
	ProcessDehacked(df);

	// handle fixer-uppers
	ProcessFixers(df);
}


//
// W_InitMultipleFiles
//
void W_InitMultipleFiles(void)
{
	// open all the files, add all the lumps.
	// NOTE: we rebuild the list, since new files can get added as we go along,
	//       and they should appear *after* the one which produced it.

	std::vector<data_file_c *> copied_files(data_files);
	data_files.clear();

	for (size_t i = 0 ; i < copied_files.size() ; i++)
	{
		ProcessFile(copied_files[i]);

		for (size_t k = 0 ; k < pending_files.size() ; k++)
		{
			ProcessFile(pending_files[k]);
		}

		pending_files.clear();
	}

//??	if (lumpinfo.empty())
//??		I_Error("W_InitMultipleFiles: no files found!\n");
}


//
// W_BuildNodes
//
void W_BuildNodes(void)
{
	for (size_t i = 0 ; i < data_files.size() ; i++)
	{
		data_file_c *df = data_files[i];

		if (df->wad != NULL)
		{
			std::string gwa_filename = W_BuildNodesForWad(df);

			if (! gwa_filename.empty())
			{
				size_t new_index = W_AddFilename(gwa_filename.c_str(), FLKIND_GWad);

				ProcessFile(data_files[new_index]);
			}
		}
	}
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
