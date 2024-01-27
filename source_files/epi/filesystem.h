//----------------------------------------------------------------------------
//  EDGE Filesystem API
//----------------------------------------------------------------------------
//
//  Copyright (c) 2003-2024 The EDGE Team.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
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
#include "str_util.h"

namespace epi
{

// Access Types
enum Access
{
    kFileAccessRead   = 0x1,
    kFileAccessWrite  = 0x2,
    kFileAccessAppend = 0x4,
    kFileAccessBinary = 0x8
};

// Forward declarations
class file_c;

// A Filesystem directory entry
class dir_entry_c
{
  public:
    std::string           name;
    size_t                size   = 0;
    bool                  is_dir = false;
};

// Path and Filename Functions
std::string FS_GetFilename(std::string_view path);
std::string FS_GetStem(std::string_view path);
std::string FS_GetDirectory(std::string_view path);
std::string FS_GetExtension(std::string_view path);
std::string FS_MakeRelative(std::string_view parent, std::string_view child);
std::string FS_PathAppend(std::string_view parent, std::string_view child);
std::string SanitizePath(std::string_view path);
bool FS_IsAbsolute(std::string_view path);
void FS_ReplaceExtension(std::string &path, std::string_view ext);

// Directory Functions
bool FS_SetCurrDir(std::string_view dir);
bool FS_IsDir(std::string_view dir);
bool FS_MakeDir(std::string_view dir);
bool FS_ReadDir(std::vector<dir_entry_c> &fsd, std::string &dir, const char *mask);
bool FS_WalkDir(std::vector<dir_entry_c> &fsd, std::string &dir);
bool FS_OpenDir(const std::string &src); // Opens a directory in explorer, finder, etc

// File Functions
bool    FS_Exists(std::string_view name);
bool    FS_Access(std::string_view name);
file_c *FS_Open(std::string_view name, unsigned int flags);
FILE   *FS_OpenRawFile(std::string_view name, unsigned int flags);
// NOTE: there's no FS_Close() function, just delete the object.
bool FS_Copy(std::string_view src, std::string_view dest);
bool FS_Delete(std::string_view name);

// General Filesystem Functions
// Performs a sync for platforms with virtualized file systems
void FS_Sync(bool populate = false);

} // namespace epi

#endif /*__EPI_FILESYSTEM_H__*/

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
