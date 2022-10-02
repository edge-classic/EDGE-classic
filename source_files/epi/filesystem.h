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

#include "arrays.h"
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
	std::string name = "";
	size_t size      = 0;
	bool is_dir      = false;
};


// Directory Functions
std::filesystem::path FS_GetCurrDir();
bool FS_SetCurrDir(std::filesystem::path dir);

bool FS_IsDir(const char *dir);
bool FS_MakeDir(const char *dir);
bool FS_RemoveDir(const char *dir);

bool FS_ReadDir(std::vector<dir_entry_c>& fsd, const char *dir, const char *mask);

// File Functions
bool FS_Access(const char *name, unsigned int flags);
file_c *FS_Open(const char *name, unsigned int flags);

// NOTE: there's no FS_Close() function, just delete the object.

bool FS_Copy(const char *src, const char *dest);
bool FS_Delete(const char *name);
bool FS_Rename(const char *oldname, const char *newname);

} // namespace epi

#endif /*__EPI_FILESYSTEM_H__*/

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
