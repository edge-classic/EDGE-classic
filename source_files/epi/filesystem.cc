//----------------------------------------------------------------------------
//  EDGE Filesystem Class
//----------------------------------------------------------------------------
//
//  Copyright (c) 2003-2008  The EDGE Team.
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

#include "file.h"
#include "filesystem.h"

#define MAX_MODE_CHARS  32

namespace epi
{

bool FS_Access(std::filesystem::path name, unsigned int flags)
{
	SYS_ASSERT(!name.empty());

    char mode[MAX_MODE_CHARS];

    if (! FS_FlagsToAnsiMode(flags, mode)) return false;

	FILE *fp = EPIFOPEN(name, mode);

    if (!fp)
        return false;

    fclose(fp);
    return true;
}


file_c* FS_Open(std::filesystem::path name, unsigned int flags)
{
	SYS_ASSERT(!name.empty());

    char mode[MAX_MODE_CHARS];

    if (! FS_FlagsToAnsiMode(flags, mode)) return NULL;

	FILE *fp = EPIFOPEN(name, mode);

    if (!fp) return NULL;

	return new ansi_file_c(fp);
}


std::filesystem::path FS_GetCurrDir()
{
	return std::filesystem::current_path();
}


bool FS_SetCurrDir(std::filesystem::path dir)
{
	SYS_ASSERT(!dir.empty());
	try
	{
		std::filesystem::current_path(dir);
	}
	catch (std::filesystem::filesystem_error const& ex)
	{
		I_Warning("Failed to set current directory! Error: %s\n", ex.what());
		return false;
	}
	return true;
}


bool FS_IsDir(std::filesystem::path dir)
{
	SYS_ASSERT(!dir.empty());
	return std::filesystem::is_directory(dir);
}


bool FS_MakeDir(std::filesystem::path dir)
{
	SYS_ASSERT(!dir.empty());
	return std::filesystem::create_directory(dir);
}


bool FS_RemoveDir(const char *dir)
{
	SYS_ASSERT(dir);
	return std::filesystem::remove(dir);
}


#ifdef _WIN32
bool FS_ReadDir(std::vector<dir_entry_c>& fsd, std::filesystem::path dir, std::u32string mask)
#else
bool FS_ReadDir(std::vector<dir_entry_c>& fsd, std::filesystem::path dir, std::string mask)
#endif
{
	if (dir.empty() || mask.empty())
		return false;

	std::filesystem::path prev_dir = FS_GetCurrDir();
	std::filesystem::path mask_ext = std::filesystem::path(mask).extension(); // Allows us to retain the *.extension syntax - Dasho

	if (prev_dir.empty())
		return false;

	if (! FS_SetCurrDir(dir))
		return false;

	// Ensure the container is empty
	fsd.clear();

	for (auto const& entry : std::filesystem::directory_iterator{std::filesystem::current_path()})
	{
		if (epi::case_cmp(mask_ext.u8string(), ".*") != 0 &&
			epi::case_cmp(mask_ext.u8string(), entry.path().extension().u8string()) != 0)
			continue;

		bool is_dir = entry.is_directory();
		size_t size = is_dir ? 0 : entry.file_size();

		fsd.push_back(dir_entry_c { entry.path(), size, is_dir });
	}

	FS_SetCurrDir(prev_dir);
	return true;
}


bool FS_Copy(std::filesystem::path src, std::filesystem::path dest)
{
	SYS_ASSERT(!src.empty() && !dest.empty());

	// Copy src to dest overwriting dest if it exists
	return std::filesystem::copy_file(src, dest, std::filesystem::copy_options::overwrite_existing);
}


bool FS_Delete(std::filesystem::path name)
{
	SYS_ASSERT(!name.empty());

	return std::filesystem::remove(name);
}


bool FS_Rename(const char *oldname, const char *newname)
{
	SYS_ASSERT(oldname);
	SYS_ASSERT(newname);
	try
	{
		std::filesystem::rename(oldname, newname);
	}
	catch (std::filesystem::filesystem_error const& ex)
	{
		I_Warning("Failed to rename file! Error: %s\n", ex.what());
		return false;
	}
	return true;
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
