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
class File;

// A Filesystem directory entry
struct DirectoryEntry
{
    std::string name;
    size_t      size   = 0;
    bool        is_dir = false;
};

// Path and Filename Functions
std::string GetFilename(std::string_view path);
std::string GetStem(std::string_view path);
std::string GetDirectory(std::string_view path);
std::string GetExtension(std::string_view path);
std::string MakePathRelative(std::string_view parent, std::string_view child);
std::string PathAppend(std::string_view parent, std::string_view child);
std::string SanitizePath(std::string_view path);
bool        IsPathAbsolute(std::string_view path);
void        ReplaceExtension(std::string &path, std::string_view ext);

// Directory Functions
bool CurrentDirectorySet(std::string_view dir);
bool IsDirectory(std::string_view dir);
bool MakeDirectory(std::string_view dir);
bool ReadDirectory(std::vector<DirectoryEntry> &fsd, std::string &dir,
                   const char *mask);
bool WalkDirectory(std::vector<DirectoryEntry> &fsd, std::string &dir);
bool OpenDirectory(
    const std::string &src);  // Opens a directory in explorer, finder, etc

// File Functions
bool  FileExists(std::string_view name);
bool  TestFileAccess(std::string_view name);
File *FileOpen(std::string_view name, unsigned int flags);
FILE *FileOpenRaw(std::string_view name, unsigned int flags);
// NOTE: there's no CloseFile function, just delete the object.
bool FileCopy(std::string_view src, std::string_view dest);
bool FileDelete(std::string_view name);

// General Filesystem Functions
// Performs a sync for platforms with virtualized file systems
void SyncFilesystem(bool populate = false);

}  // namespace epi

#endif /*__EPI_FILESYSTEM_H__*/

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
