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
#include "l_deh.h"
#include "w_files.h"

// EPI
#include "epi.h"
#include "file.h"
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
	std::string fullpath;

	u32_t pos, length;

	pack_entry_c(const std::string& _name, const std::string& _path, u32_t _pos, u32_t _len) :
		name(_name), fullpath(_path), pos(_pos), length(_len)
	{ }

	~pack_entry_c()
	{ }

	bool operator== (const std::string& other) const
	{
		return epi::case_cmp(name, other.c_str()) == 0;
	}

	bool HasExtension(const char *match) const
	{
		std::string ext = epi::PATH_GetExtension(name.c_str());
		return epi::case_cmp(ext, match) == 0;
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

	size_t AddEntry(const std::string& _name, const std::string& _path, u32_t _pos, u32_t _length)
	{
		// check if already there
		for (size_t i = 0 ; i < entries.size() ; i++)
			if (entries[i] == _name)
				return i;

		entries.push_back(pack_entry_c(_name, _path, _pos, _length));
		return entries.size() - 1;
	}

	bool operator== (const std::string& other) const
	{
		return epi::case_cmp(name, other) == 0;
	}
};


class pack_file_c
{
public:
	data_file_c *parent;

	bool is_folder;

	// first entry here is always the top-level (with no name).
	// everything else is from a second-level directory.
	// things in deeper directories are not stored.
	std::vector<pack_dir_c> dirs;

public:
	pack_file_c(data_file_c *_par, bool _folder) : parent(_par), is_folder(_folder), dirs()
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

	epi::file_c * OpenEntry(size_t dir, size_t index)
	{
		if (is_folder)
			return OpenEntry_Folder(dir, index);
		else
			return OpenEntry_Zip(dir, index);
	}

	byte * LoadEntry(size_t dir, size_t index, int& length)
	{
		epi::file_c *f = OpenEntry(dir, index);
		if (f == NULL)
		{
			length = 0;
			return new byte[1];
		}

		byte * data = f->LoadIntoMemory();
		length      = f->GetLength();

		// close file
		delete f;

		if (data == NULL)
		{
			length = 0;
			return new byte[1];
		}

		return data;
	}

private:
	epi::file_c * OpenEntry_Folder(size_t dir, size_t index);
	epi::file_c * OpenEntry_Zip   (size_t dir, size_t index);
};


//----------------------------------------------------------------------------

// -AJA- this compares the name in "natural order", which means that
//       "x15" comes after "x1" and "x2" (not between them).
//       more precisely: we treat strings of digits as a single char.
struct Compare_packentry_pred
{
	inline bool operator() (const pack_entry_c& AE, const pack_entry_c& BE) const
	{
		const std::string& A = AE.name;
		const std::string& B = BE.name;

		size_t x = 0;
		size_t y = 0;

		for (;;)
		{
			// reached the end of one/both strings?
			if (x >= A.size() || y >= B.size())
				return x >= A.size() && y < B.size();

			int xc = (int)(unsigned char)A[x++];
			int yc = (int)(unsigned char)B[y++];

			// handle a sequence of digits
			if (isdigit(xc))
			{
				xc = 200 + (xc - '0');
				while (x < A.size() && isdigit(A[x]) && xc < 214'000'000)
					xc = (xc * 10) + (int)(A[x++] - '0');
			}
			if (isdigit(yc))
			{
				yc = 200 + (yc - '0');
				while (y < B.size() && isdigit(B[y]) && yc < 214'000'000)
					yc = (yc * 10) + (int)(B[y++] - '0');
			}

			if (xc != yc)
				return xc < yc;
		}
	}
};

void pack_dir_c::SortEntries()
{
	std::sort(entries.begin(), entries.end(), Compare_packentry_pred());
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
			pack->dirs[d].AddEntry(filename, fsd[i].name, 0, 0);
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

	pack_file_c *pack = new pack_file_c(df, true);

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
			pack->dirs[0].AddEntry(filename, fsd[i].name, 0, 0);
		}
	}

	return pack;
}


epi::file_c * pack_file_c::OpenEntry_Folder(size_t dir, size_t index)
{
	const std::string& filename = dirs[dir].entries[index].fullpath;

	epi::file_c * f = epi::FS_Open(filename.c_str(), epi::file_c::ACCESS_READ | epi::file_c::ACCESS_BINARY);

	// this generally won't happen, file was found during a dir scan
	if (f == NULL)
		I_Error("Failed to open file: %s\n", filename.c_str());

	return f;
}


//----------------------------------------------------------------------------
//  ZIP READING
//----------------------------------------------------------------------------

static pack_file_c * ProcessZip(data_file_c *df)
{
	I_Warning("Skipping PK3 package: %s\n", df->name.c_str());
	return new pack_file_c(df, false);
}


epi::file_c * pack_file_c::OpenEntry_Zip(size_t dir, size_t index)
{
	// TODO !!!
	I_Error("OpenEntry_Zip called.\n");
	return NULL;
}


//----------------------------------------------------------------------------
//  GENERAL STUFF
//----------------------------------------------------------------------------

static void ProcessDehackedInPack(pack_file_c *pack)
{
	data_file_c *df = pack->parent;

	for (size_t i = 0 ; i < pack->dirs[0].entries.size() ; i++)
	{
		pack_entry_c& entry = pack->dirs[0].entries[i];

		if (entry.HasExtension(".deh") || entry.HasExtension(".bex"))
		{
			I_Printf("Converting DEH file%s: %s\n",
				pack->is_folder ? "" : " in PK3", entry.name.c_str());

			int length = -1;
			const byte *data = pack->LoadEntry(0, i, length);

			// FIXME does not handle multiple files!!
			df->deh = DH_ConvertLump(data, length);
			if (df->deh == NULL)
				I_Error("Failed to convert DeHackEd LUMP in: %s\n", df->name.c_str());

			delete[] data;
		}
	}
}


void ProcessPackage(data_file_c *df, size_t file_index)
{
	if (df->kind == FLKIND_Folder)
		df->pack = ProcessFolder(df);
	else
		df->pack = ProcessZip(df);

	df->pack->SortEntries();

	ProcessDehackedInPack(df->pack);
}


epi::file_c * Pack_OpenFile(pack_file_c *pack, const char *base_name)
{
	std::string name = base_name;

	for (size_t i = 0 ; i < pack->dirs[0].entries.size() ; i++)
	{
		if (pack->dirs[0].entries[i] == name)
		{
			return pack->OpenEntry(0, i);
		}
	}

	// not found
	return NULL;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
