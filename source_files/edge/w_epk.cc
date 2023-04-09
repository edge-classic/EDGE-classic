//----------------------------------------------------------------------------
//  EDGE EPK Support Code
//----------------------------------------------------------------------------
//
//  Copyright (c) 2022-2023  The EDGE Team.
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
#include "r_image.h"
#include "w_files.h"
#include "vm_coal.h"

// EPI
#include "epi.h"
#include "file.h"
#include "filesystem.h"
#include "path.h"
#include "sound_types.h"
#include "str_util.h"

// DDF
#include "main.h"
#include "colormap.h"
#include "wadfixes.h"

#include <list>
#include <vector>
#include <algorithm>

#include "miniz.h"


class pack_entry_c
{
public:
	// this name is relative to parent (if any), i.e. no slashes
	std::string name;

	// only for Folder: the full pathname to file (for FS_Open).
	std::string fullpath;

	// only for EPK: the index into the archive.
	mz_uint file_idx;

	pack_entry_c(const std::string& _name, const std::string& _path, mz_uint _idx) :
		name(_name), fullpath(_path), file_idx(_idx)
	{ }

	~pack_entry_c()
	{ }

	bool operator== (const std::string& other) const
	{
		return epi::case_cmp(name, other.c_str()) == 0;
	}

	bool HasExtension(const char *match) const
	{
		std::string ext = epi::PATH_GetExtension(UTFSTR(name)).u8string();
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

	size_t AddEntry(const std::string& _name, const std::string& _path, mz_uint _idx)
	{
		// check if already there
		for (size_t i = 0 ; i < entries.size() ; i++)
			if (entries[i] == _name)
				return i;

		entries.push_back(pack_entry_c(_name, _path, _idx));
		return entries.size() - 1;
	}

	int Find(const std::string& name_in) const
	{
		for (int i = 0 ; i < (int)entries.size() ; i++)
			if (entries[i] == name_in)
				return i;

		return -1; // not found
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

	mz_zip_archive *arch;

public:
	pack_file_c(data_file_c *_par, bool _folder) : parent(_par), is_folder(_folder), dirs(), arch(NULL)
	{ }

	~pack_file_c()
	{
		if (arch != NULL)
			delete arch;
	}

	size_t AddDir(const std::string& name)
	{
		// check if already there
		for (size_t i = 0 ; i < dirs.size() ; i++)
			if (dirs[i] == name)
				return i;

		dirs.push_back(pack_dir_c(name));
		return dirs.size() - 1;
	}

	int FindDir(const std::string& name) const
	{
		for (int i = 0 ; i < (int)dirs.size() ; i++)
			if (dirs[i] == name)
				return i;

		return -1; // not found
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

	epi::file_c * OpenFileByName(const std::string& name)
	{
		if (is_folder)
			return OpenFile_Folder(name);
		else
			return OpenFile_Zip(name);
	}

	int EntryLength(size_t dir, size_t index)
	{
		epi::file_c *f = OpenEntry(dir, index);
		if (f == NULL)
			return 0;

		int length = f->GetLength();
		delete f;  // close it

		return length;
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

	epi::file_c * OpenFile_Folder(const std::string& name);
	epi::file_c * OpenFile_Zip   (const std::string& name);
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

	std::string dirname = epi::PATH_GetFilename(UTFSTR(fullpath)).u8string();

	if (! epi::FS_ReadDir(fsd, UTFSTR(fullpath), UTFSTR("*.*")))
	{
		I_Warning("Failed to read dir: %s\n", fullpath.c_str());
		return;
	}

	size_t d = pack->AddDir(dirname);

	for (size_t i = 0 ; i < fsd.size() ; i++)
	{
		if (! fsd[i].is_dir)
		{
			std::string filename = epi::PATH_GetFilename(fsd[i].name).u8string();
			pack->dirs[d].AddEntry(filename, fsd[i].name.u8string(), 0);
		}
	}
}


static pack_file_c * ProcessFolder(data_file_c *df)
{
	std::vector<epi::dir_entry_c> fsd;

	if (! epi::FS_ReadDir(fsd, df->name, UTFSTR("*.*")))
	{
		I_Error("Failed to read dir: %s\n", df->name.u8string().c_str());
	}

	pack_file_c *pack = new pack_file_c(df, true);

	// top-level files go in here
	pack->AddDir("");

	for (size_t i = 0 ; i < fsd.size() ; i++)
	{
		if (fsd[i].is_dir)
		{
			ProcessSubDir(pack, fsd[i].name.u8string());
		}
		else
		{
			std::string filename = epi::PATH_GetFilename(fsd[i].name).u8string();
			pack->dirs[0].AddEntry(filename, fsd[i].name.u8string(), 0);
		}
	}

	return pack;
}


epi::file_c * pack_file_c::OpenEntry_Folder(size_t dir, size_t index)
{
	const std::string& filename = dirs[dir].entries[index].fullpath;

	epi::file_c * F = epi::FS_Open(filename.c_str(), epi::file_c::ACCESS_READ | epi::file_c::ACCESS_BINARY);

	// this generally won't happen, file was found during a dir scan
	if (F == NULL)
		I_Error("Failed to open file: %s\n", filename.c_str());

	return F;
}


epi::file_c * pack_file_c::OpenFile_Folder(const std::string& name)
{
	std::filesystem::path fullpath = epi::PATH_Join(parent->name, UTFSTR(name));

	// NOTE: it is okay here when file does not exist
	return epi::FS_Open(fullpath, epi::file_c::ACCESS_READ | epi::file_c::ACCESS_BINARY);
}


//----------------------------------------------------------------------------
//  ZIP READING
//----------------------------------------------------------------------------

static pack_file_c * ProcessZip(data_file_c *df)
{
	pack_file_c *pack = new pack_file_c(df, false);

	pack->arch = new mz_zip_archive;

	// this is necessary (but stupid)
	memset(pack->arch, 0, sizeof(mz_zip_archive));

	if (! mz_zip_reader_init_file(pack->arch, df->name.u8string().c_str(), 0))
	{
		switch (mz_zip_get_last_error(pack->arch))
		{
			case MZ_ZIP_FILE_OPEN_FAILED:
			case MZ_ZIP_FILE_READ_FAILED:
			case MZ_ZIP_FILE_SEEK_FAILED:
				I_Error("Failed to open EPK file: %s\n", df->name.u8string().c_str());

			default:
				I_Error("Not a EPK file (or is corrupted): %s\n", df->name.u8string().c_str());
		}
	}

	// create the top-level directory
	pack->AddDir("");

	mz_uint total = mz_zip_reader_get_num_files(pack->arch);

	for (mz_uint idx = 0 ; idx < total ; idx++)
	{
		// skip directories
		if (mz_zip_reader_is_file_a_directory(pack->arch, idx))
			continue;

		// get the filename
		char filename[1024];

		mz_zip_reader_get_filename(pack->arch, idx, filename, sizeof(filename));

		// decode into DIR + FILE
		char *p = filename;
		while (*p != 0 && *p != '/' && *p != '\\')
			p++;

		if (p == filename)
			continue;

		size_t dir_idx  = 0;
		char * basename = filename;

		if (*p != 0)
		{
			*p++ = 0;

			basename = p;
			if (basename[0] == 0)
				continue;

			// skip file if it has more sub-directories
			while (*p != 0 && *p != '/' && *p != '\\')
				p++;

			if (*p != 0)
				continue;

			dir_idx = pack->AddDir(filename);
		}

		pack->dirs[dir_idx].AddEntry(basename, "", idx);

		// DEBUG
		//   fprintf(stderr, "FILE %d : dir %d '%s'\n", (int)idx, (int)dir_idx, basename);
	}

	return pack;
}


class epk_file_c : public epi::file_c
{
private:
	pack_file_c * pack;

	mz_uint file_idx;

	mz_uint length = 0;
	mz_uint pos    = 0;

	mz_zip_reader_extract_iter_state *iter = NULL;

public:
	epk_file_c(pack_file_c *_pack, mz_uint _idx) : pack(_pack), file_idx(_idx)
	{
		// determine length
		mz_zip_archive_file_stat stat;
		if (mz_zip_reader_file_stat(pack->arch, file_idx, &stat))
			length = (mz_uint) stat.m_uncomp_size;

		iter = mz_zip_reader_extract_iter_new(pack->arch, file_idx, 0);
		SYS_ASSERT(iter);
	}

	~epk_file_c()
	{
		if (iter != NULL)
			mz_zip_reader_extract_iter_free(iter);
	}

	int GetLength()
	{
		return (int)length;
	}

	int GetPosition()
	{
		return (int)pos;
	}

	unsigned int Read(void *dest, unsigned int count)
	{
		if (pos >= length)
			return 0;

		// never read more than what GetLength() reports
		if (count > length - pos)
			count = length - pos;

		size_t got = mz_zip_reader_extract_iter_read(iter, dest, count);

		pos += got;

		return got;
	}

	unsigned int Write(const void *src, unsigned int count)
	{
		// not implemented
		return count;
	}

	bool Seek(int offset, int seekpoint)
	{
		mz_uint want_pos = pos;

		if (seekpoint == epi::file_c::SEEKPOINT_START) want_pos = 0;
		if (seekpoint == epi::file_c::SEEKPOINT_END)   want_pos = length;

		if (offset < 0)
		{
			offset = -offset;
			if ((mz_uint)offset >= want_pos)
				want_pos = 0;
			else
				want_pos -= (mz_uint)offset;
		}
		else
		{
			want_pos += (mz_uint)offset;
		}

		// cannot go beyond the end (except TO very end)
		if (want_pos > length)
			return false;

		if (want_pos == length)
		{
			pos = length;
			return true;
		}

		// to go backwards, we are forced to rewind to beginning
		if (want_pos < pos)
		{
			Rewind();
		}

		// trivial success when already there
		if (want_pos == pos)
			return true;

		SkipForward(want_pos - pos);
		return true;
	}

private:
	void Rewind()
	{
		mz_zip_reader_extract_iter_free(iter);
		iter = mz_zip_reader_extract_iter_new(pack->arch, file_idx, 0);
		SYS_ASSERT(iter);

		pos = 0;
	}

	void SkipForward(unsigned int count)
	{
		byte buffer[1024];

		while (count > 0)
		{
			size_t want = std::min((size_t)count, sizeof(buffer));
			size_t got  = mz_zip_reader_extract_iter_read(iter, buffer, want);

			// reached end of file?
			if (got == 0)
				break;

			pos   += got;
			count -= got;
		}
	}
};


epi::file_c * pack_file_c::OpenEntry_Zip(size_t dir, size_t index)
{
	epk_file_c *F = new epk_file_c(this, dirs[dir].entries[index].file_idx);
	return F;
}


epi::file_c * pack_file_c::OpenFile_Zip(const std::string& name)
{
	// this ignores case by default
	int idx = mz_zip_reader_locate_file(arch, name.c_str(), NULL, 0);
	if (idx < 0)
		return NULL;

	epk_file_c *F = new epk_file_c(this, (mz_uint)idx);
	return F;
}


//----------------------------------------------------------------------------
//  GENERAL STUFF
//----------------------------------------------------------------------------

static void ProcessDDFInPack(pack_file_c *pack)
{
	data_file_c *df = pack->parent;

	std::string bare_filename = epi::PATH_GetFilename(df->name).u8string();
	if (bare_filename.empty())
		bare_filename = df->name.u8string();

	for (size_t i = 0 ; i < pack->dirs[0].entries.size() ; i++)
	{
		pack_entry_c& entry = pack->dirs[0].entries[i];

		std::string source = entry.name;
		source += " in ";
		source += bare_filename;

		// this handles RTS scripts too!
		ddf_type_e type = DDF_FilenameToType(UTFSTR(entry.name));

		if (type != DDF_UNKNOWN)
		{
			int length = -1;
			const byte *raw_data = pack->LoadEntry(0, i, length);

			std::string data((const char *)raw_data);
			delete[] raw_data;

			DDF_AddFile(type, data, source);
			continue;
		}

		if (entry.HasExtension(".deh") || entry.HasExtension(".bex"))
		{
			I_Printf("Converting DEH file%s: %s\n",
				pack->is_folder ? "" : " in EPK", entry.name.c_str());

			int length = -1;
			const byte *data = pack->LoadEntry(0, i, length);

			DEH_Convert(data, length, source);
			delete[] data;

			continue;
		}
	}
}

static void ProcessCoalAPIInPack(pack_file_c *pack)
{
	const char *name = "coal_api.ec";

	int idx = pack->dirs[0].Find(name);
	if (idx < 0)
		I_Error("coal_api.ec not found in edge-defs.epk; unable to initialize COAL!\n");

	data_file_c *df = pack->parent;

	std::string bare_filename = epi::PATH_GetFilename(df->name).u8string();
	if (bare_filename.empty())
		bare_filename = df->name.u8string();

	std::string source = name;
	source += " in ";
	source += bare_filename;

	int length = -1;
	const byte *raw_data = pack->LoadEntry(0, idx, length);

	std::string data((const char *)raw_data);
	delete[] raw_data;

	VM_AddScript(0, data, source);
}

static void ProcessCoalHUDInPack(pack_file_c *pack)
{
	const char *name = "coal_hud.ec";

	int idx = pack->dirs[0].Find(name);
	if (idx < 0)
		return;

	data_file_c *df = pack->parent;

	std::string bare_filename = epi::PATH_GetFilename(df->name).u8string();
	if (bare_filename.empty())
		bare_filename = df->name.u8string();

	std::string source = name;
	source += " in ";
	source += bare_filename;

	int length = -1;
	const byte *raw_data = pack->LoadEntry(0, idx, length);

	std::string data((const char *)raw_data);
	delete[] raw_data;

	VM_AddScript(0, data, source);
}


static bool TextureNameFromFilename(std::string& buf, const std::string& stem, bool is_sprite)
{
	// returns false if the name is invalid (e.g. longer than 8 chars).

	size_t pos = 0;

	buf.clear();

	while (pos < stem.size())
	{
		if (buf.size() >= 8)
			return false;

		int ch = (unsigned char) stem[pos++];

		// sprites allow a few special characters, but remap caret --> backslash
		if (is_sprite && ch == '^')
			ch = '\\';
		else if (is_sprite && (ch == '[' || ch == ']'))
			{ /* ok */ }
		else if (isalnum(ch) || ch == '_')
			{ /* ok */ }
		else
			return false;

		buf.push_back((char) ch);
	}

	epi::str_upper(buf);

	return (buf.size() > 0);
}


void Pack_ProcessImages(pack_file_c *pack, const std::string& dir_name, const std::string& prefix)
{
	int d = pack->FindDir(dir_name);
	if (d < 0)
		return;

	for (size_t i = 0 ; i < pack->dirs[d].entries.size() ; i++)
	{
		pack_entry_c& entry = pack->dirs[d].entries[i];

		// split filename in stem + extension
		std::string stem = epi::PATH_GetBasename(UTFSTR(entry.name)).u8string();
		std::string ext  = epi::PATH_GetExtension(UTFSTR(entry.name)).u8string();

		epi::str_lower(ext);

		if (ext == ".png" || ext == ".tga" || ext == ".jpg" || ext == ".jpeg" || ext == ".lmp") // Note: .lmp is assumed to be Doom-format image
		{
			std::string texname;
			std::string packpath = dir_name;

			if (! TextureNameFromFilename(texname, stem, prefix == "spr"))
			{
				I_Warning("Illegal image name in EPK: %s\n", entry.name.c_str());
				continue;
			}

			I_Debugf("- Adding image file in EPK: %s/%s\n", dir_name.c_str(), entry.name.c_str());

			packpath.append("/").append(entry.name);

			if (dir_name == "textures")
				AddImage_SmartPack(texname.c_str(), IMSRC_TX_HI, packpath.c_str(), real_textures);
			else if (dir_name == "graphics")
				AddImage_SmartPack(texname.c_str(), IMSRC_Graphic, packpath.c_str(), real_graphics);
			else if (dir_name == "flats")
				AddImage_SmartPack(texname.c_str(), IMSRC_Flat, packpath.c_str(), real_flats);
			else if (dir_name == "sprites")
				AddImage_SmartPack(texname.c_str(), IMSRC_Sprite, packpath.c_str(), real_sprites);
		}
		else
		{
			I_Warning("Unknown image type in EPK: %s\n", entry.name.c_str());
		}
	}
}


/*static void ProcessSoundsInPack(pack_file_c *pack)
{
	data_file_c *df = pack->parent;

	int d = pack->FindDir("sounds");
	if (d < 0)
		return;

	std::string text = "<SOUNDS>\n\n";

	for (size_t i = 0 ; i < pack->dirs[d].entries.size() ; i++)
	{
		pack_entry_c& entry = pack->dirs[d].entries[i];

		epi::sound_format_e fmt = epi::Sound_FilenameToFormat(entry.name);

		if (fmt == epi::FMT_Unknown)
		{
			I_Warning("Unknown sound type in EPK: %s\n", entry.name.c_str());
			continue;
		}

		// stem must consist of only digits
		std::string stem = epi::PATH_GetBasename(UTFSTR(entry.name)).u8string();
		std::string sfxname;

		if (! TextureNameFromFilename(sfxname, stem, false))
		{
			I_Warning("Illegal sound name in EPK: %s\n", entry.name.c_str());
			continue;
		}

		I_Debugf("- Adding sound file in EPK: %s\n", entry.name.c_str());

		// generate DDF for it...
		text += "[";
		text += sfxname;
		text += "]\n";

		text += "pack_name = \"";
		text += "sounds/";
		text += entry.name;
		text += "\";\n";

		text += "priority  = 64;\n";
		text += "\n";
	}

	// DEBUG:
	// DDF_DumpFile(text);

	DDF_AddFile(DDF_SFX, text, df->name.u8string());
}*/


/*static void ProcessMusicsInPack(pack_file_c *pack)
{
	data_file_c *df = pack->parent;

	int d = pack->FindDir("music");
	if (d < 0)
		return;

	std::string text = "<PLAYLISTS>\n\n";

	for (size_t i = 0 ; i < pack->dirs[d].entries.size() ; i++)
	{
		pack_entry_c& entry = pack->dirs[d].entries[i];

		epi::sound_format_e fmt = epi::Sound_FilenameToFormat(entry.name);

		if (fmt == epi::FMT_Unknown)
		{
			I_Warning("Unknown music type in EPK: %s\n", entry.name.c_str());
			continue;
		}

		// stem must consist of only digits
		std::string stem = epi::PATH_GetBasename(entry.name).u8string();

		bool valid = stem.size() > 0;
		for (char ch : stem)
			if (ch < '0' || ch > '9')
				valid = false;

		if (! valid)
		{
			I_Warning("Non-numeric music name in EPK: %s\n", entry.name.c_str());
			continue;
		}

		I_Debugf("- Adding music file in EPK: %s\n", entry.name.c_str());

		// generate DDF for it...
		text += "[";
		text += stem;
		text += "]\n";

		text += "MUSICINFO=MUS:PACK:\"";
		text += "music/";
		text += entry.name;
		text += "\";\n\n";
	}

	// DEBUG:
	// DDF_DumpFile(text);

	DDF_AddFile(DDF_Playlist, text, df->name.u8string());
}*/

static void ProcessColourmapsInPack(pack_file_c *pack)
{
	int d = pack->FindDir("colormaps");
	if (d < 0)
		return;

	for (size_t i = 0 ; i < pack->dirs[d].entries.size() ; i++)
	{
		pack_entry_c& entry = pack->dirs[d].entries[i];

		// split filename in stem + extension
		std::string stem = epi::PATH_GetBasename(UTFSTR(entry.name)).u8string();
		std::string ext  = epi::PATH_GetExtension(UTFSTR(entry.name)).u8string();

		// extension is currently ignored
		(void)ext;

		std::string colname;

		if (! TextureNameFromFilename(colname, stem, false))
		{
			I_Warning("Illegal colourmap name in EPK: %s\n", entry.name.c_str());
			continue;
		}

		std::string fullname = "colormaps/";
		fullname += entry.name;

		int size = pack->EntryLength(d, i);

		DDF_AddRawColourmap(colname.c_str(), size, fullname.c_str());
	}
}


epi::file_c * Pack_OpenFile(pack_file_c *pack, const std::string& name)
{
	// when file does not exist, this returns NULL.

	// disallow absolute names
	if (name.size() >= 1 && (name[0] == '/' || name[0] == '\\'))
		return NULL;

	if (name.size() >= 2 && name[1] == ':')
		return NULL;

	// try the top-level directory
	int ent = pack->dirs[0].Find(name);
	if (ent >= 0)
	{
		return pack->OpenEntry(0, (size_t)ent);
	}

	// try an arbitrary place
	return pack->OpenFileByName(name);
}

void ProcessPackage(data_file_c *df, size_t file_index)
{
	if (df->kind == FLKIND_Folder || df->kind == FLKIND_EFolder)
		df->pack = ProcessFolder(df);
	else
		df->pack = ProcessZip(df);

	df->pack->SortEntries();

	// parse the WADFIXES file from edge-defs folder or `edge-defs.epk` immediately
	if ((df->kind == FLKIND_EFolder || df->kind == FLKIND_EEPK) && file_index == 0)
	{
		I_Printf("Loading WADFIXES\n");
		epi::file_c *wadfixes = Pack_OpenFile(df->pack, "wadfixes.ddf");
		DDF_ReadFixes(wadfixes->ReadText());
		delete wadfixes;
	}

	ProcessColourmapsInPack(df->pack);

	/*ProcessSoundsInPack(df->pack);
	ProcessMusicsInPack(df->pack);*/

	ProcessDDFInPack(df->pack);
	// parse COALAPI only from edge-defs folder or `edge-defs.epk`
	if ((df->kind == FLKIND_EFolder || df->kind == FLKIND_EEPK) && file_index == 0)
		ProcessCoalAPIInPack(df->pack);
	ProcessCoalHUDInPack(df->pack);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
