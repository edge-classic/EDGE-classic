//----------------------------------------------------------------------------
//  EDGE Filesystem API
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

#ifndef __EPI_FILESYSTEM_H__
#define __EPI_FILESYSTEM_H__

#include <vector>
#include <filesystem>

namespace epi
{

// Forward declarations
class file_c;


// A Filesystem directory entry
class dir_entry_c
{
public:
	std::filesystem::path name;
	size_t size      = 0;
	bool is_dir      = false;
};


// Directory Functions
std::filesystem::path FS_GetCurrDir();
bool FS_SetCurrDir(std::filesystem::path dir);

bool FS_IsDir(std::filesystem::path dir);
bool FS_MakeDir(std::filesystem::path dir);
bool FS_RemoveDir(const char *dir);
#ifdef _WIN32
bool FS_ReadDir(std::vector<dir_entry_c>& fsd, std::filesystem::path dir, std::u32string mask);
#else
bool FS_ReadDir(std::vector<dir_entry_c>& fsd, std::filesystem::path dir, std::string mask);
#endif
// File Functions
bool FS_Access(std::filesystem::path name, unsigned int flags);
file_c *FS_Open(std::filesystem::path name, unsigned int flags);

// NOTE: there's no FS_Close() function, just delete the object.

bool FS_Copy(std::filesystem::path src, std::filesystem::path dest);
bool FS_Delete(std::filesystem::path name);
bool FS_Rename(const char *oldname, const char *newname);

} // namespace epi

#endif /*__EPI_FILESYSTEM_H__*/

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
