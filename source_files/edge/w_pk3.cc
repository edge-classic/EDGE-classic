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
#include "w_files.h"

// EPI
#include "epi.h"
#include "filesystem.h"
#include "path.h"

#include <list>
#include <vector>
#include <algorithm>

#include "miniz.h"


class pack_entry_c
{
public:
	std::string name;

	u32_t pos, length;

	pack_entry_c(const std::string& _name, u32_t _pos, u32_t _len) :
		name(_name), pos(_pos), length(_len)
	{ }

	~pack_entry_c()
	{ }

	bool operator== (const std::string& other) const
	{
		// FIXME !!!
		return false;
	}
};


class pack_dir_c
{
public:
	std::string name;
	std::vector<pack_entry_c> entries;

	pack_dir_c(const std::string& _name) : name(_name), entries()
	{ }

	~pack_dir_c()
	{ }

	void SortEntries();

	size_t AddEntry(const std::string& _name, u32_t _pos, u32_t _length)
	{
		// check if already there
		for (size_t i = 0 ; i < entries.size() ; i++)
			if (entries[i] == _name)
				return i;

		entries.push_back(pack_entry_c(_name, _pos, _length));
		return entries.size() - 1;
	}

	bool operator== (const std::string& other) const
	{
		// FIXME !!!
		return false;
	}
};


class pack_file_c
{
public:
	data_file_c *parent = NULL;

	// first entry here is always the top-level (with no name).
	// everything else is from a second-level directory.
	// things in deeper directories are not stored.
	std::vector<pack_dir_c> dirs;

public:
	pack_file_c(data_file_c *_par) : parent(_par), dirs()
	{ }

	~pack_file_c()
	{ }

	size_t AddDir(const std::string& name)
	{
		// check if already there
		for (size_t i = 0 ; i < dirs.size() ; i++)
			if (dirs[i] == name)
				return i;

		dirs.push_back(pack_dir_c(name));
		return dirs.size() - 1;
	}

	void ProcessSubDir(const std::string& dirname);
	void SortEntries();
};


//----------------------------------------------------------------------------

void pack_dir_c::SortEntries()
{
	// FIXME
}

void pack_file_c::SortEntries()
{
	for (size_t i = 0 ; i < dirs.size() ; i++)
		dirs[i].SortEntries();
}


//----------------------------------------------------------------------------
//  DIRECTORY READING
//----------------------------------------------------------------------------

void ProcessSubDir(pack_file_c *pack, const std::string& fullpath)
{
	std::vector<epi::dir_entry_c> fsd;

	std::string dirname = epi::PATH_GetFilename(fullpath.c_str());

	if (! epi::FS_ReadDir(fsd, fullpath.c_str(), "*.*"))
	{
		I_Warning("Failed to read dir: %s\n", fullpath.c_str());
		return;
	}

	size_t d = pack->AddDir(dirname);

	for (size_t i = 0 ; i < fsd.size() ; i++)
	{
		if (! fsd[i].is_dir)
		{
			std::string filename = epi::PATH_GetFilename(fsd[i].name.c_str());
			pack->dirs[d].AddEntry(filename, 0, 0);
		}
	}
}


static pack_file_c * ProcessFolder(data_file_c *df)
{
	std::vector<epi::dir_entry_c> fsd;

	if (! epi::FS_ReadDir(fsd, df->name.c_str(), "*.*"))
	{
		I_Error("Failed to read dir: %s\n", df->name.c_str());
	}

	pack_file_c *pack = new pack_file_c(df);

	// top-level files go in here
	pack->AddDir("");

	for (size_t i = 0 ; i < fsd.size() ; i++)
	{
		if (fsd[i].is_dir)
		{
			ProcessSubDir(pack, fsd[i].name);
		}
		else
		{
			std::string filename = epi::PATH_GetFilename(fsd[i].name.c_str());
			pack->dirs[0].AddEntry(filename, 0, 0);
		}
	}

	return pack;
}


//----------------------------------------------------------------------------
//  ZIP READING
//----------------------------------------------------------------------------

static pack_file_c * ProcessZip(data_file_c *df)
{
	I_Warning("Skipping PK3 package: %s\n", df->name.c_str());
	return new pack_file_c(df);
}


void ProcessPackage(data_file_c *df, size_t file_index)
{
	if (df->kind == FLKIND_Folder)
		df->pack = ProcessFolder(df);
	else
		df->pack = ProcessZip(df);

	df->pack->SortEntries();
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
