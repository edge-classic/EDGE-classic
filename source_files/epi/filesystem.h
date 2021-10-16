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
#include "timestamp.h"

namespace epi
{

// Forward declarations
class file_c;

// A Filesystem directory entry
class filesys_direntry_c
{
public:
	std::string name;
	unsigned int size;
	bool is_dir;

public:
	filesys_direntry_c() : name(), size(0), is_dir(false)
	{ }

	~filesys_direntry_c()
	{ }
};


// A Filesystem directory
class filesystem_dir_c : public array_c
{
public:
	filesystem_dir_c();
	~filesystem_dir_c();

private:
	void CleanupObject(void *obj);
		
public:
	bool AddEntry(filesys_direntry_c* fs_entry);

	int GetSize(void) { return array_entries; }

	filesys_direntry_c *operator[](int idx); 	
};

// ---- The Filesystem ----

// Directory Functions
bool FS_GetCurrDir(char *dir, unsigned int bufsize); 
bool FS_SetCurrDir(const char *dir);

bool FS_IsDir(const char *dir);
bool FS_MakeDir(const char *dir);
bool FS_RemoveDir(const char *dir);

bool FS_ReadDir(filesystem_dir_c *fsd, const char *dir, const char *mask);

// File Functions
bool FS_Access(const char *name, unsigned int flags);
file_c *FS_Open(const char *name, unsigned int flags);

// NOTE: there's no FS_Close() function, just delete the object.

bool FS_Copy(const char *src, const char *dest);
bool FS_Delete(const char *name);
bool FS_Rename(const char *oldname, const char *newname);

bool FS_GetModifiedTime(const char *filename, timestamp_c& t);

} // namespace epi

#endif /*__EPI_FILESYSTEM_H__*/

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
