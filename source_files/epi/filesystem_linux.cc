//----------------------------------------------------------------------------
//  EDGE Filesystem Class for Linux
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
//
#include "epi.h"

#include "file.h"
#include "filesystem.h"

#include <dirent.h>
#include <fnmatch.h>
#include <time.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>


#define COPY_BUF_SIZE  1024


namespace epi
{

bool FS_GetCurrDir(char *dir, unsigned int bufsize)
{
	SYS_ASSERT(dir);

	return (getcwd((char *)dir, bufsize) != NULL);
}

bool FS_SetCurrDir(const char *dir)
{
	SYS_ASSERT(dir);

	return (chdir(dir) == 0);
}

bool FS_IsDir(const char *dir)
{
	SYS_ASSERT(dir);

	DIR *result = opendir(dir);

	if (result == NULL)
		return false;

	closedir(result);
	return true;
}

bool FS_MakeDir(const char *dir)
{
	SYS_ASSERT(dir);

	return (mkdir(dir, 0775) == 0);
}

bool FS_RemoveDir(const char *dir)
{
	SYS_ASSERT(dir);

	return (rmdir(dir) == 0);
}

bool FS_ReadDir(filesystem_dir_c *fsd, const char *dir, const char *mask)
{
	SYS_ASSERT(fsd);
	SYS_ASSERT(dir);
	SYS_ASSERT(mask);

	DIR *handle = opendir(dir);
	if (handle == NULL)
		return false;

	char olddir[PATH_MAX];

	FS_GetCurrDir(olddir, PATH_MAX);
	FS_SetCurrDir(dir);

	// Ensure the container is empty
	fsd->Clear();

	for (;;)
	{
		struct dirent *fdata = readdir(handle);
		if (fdata == NULL)
			break;

		if (strlen(fdata->d_name) == 0)
			continue;

		// skip the funky "." and ".." dirs 
		if (strcmp(fdata->d_name, ".")  == 0 ||
			strcmp(fdata->d_name, "..") == 0)
			continue;

		if (fnmatch(mask, fdata->d_name, 0) != 0)
			continue;

		struct stat finfo;

		if (stat(fdata->d_name, &finfo) != 0)
			continue;
		
		filesys_direntry_c *entry = new filesys_direntry_c();

		entry->name = std::string(fdata->d_name);
		entry->size = finfo.st_size;
		entry->is_dir = S_ISDIR(finfo.st_mode) ?true:false;

		if (! fsd->AddEntry(entry))
		{
			delete entry;
			closedir(handle);
			FS_SetCurrDir(olddir);
			return false;
		}
	}

	FS_SetCurrDir(olddir);

	closedir(handle);
	return true;
}


bool FS_Copy(const char *src, const char *dest)
{
	SYS_ASSERT(src && dest);

	bool ok = false;

	file_c *dest_file = NULL;
	file_c *src_file  = NULL;

	unsigned char *buf = NULL;

	int size;
	int pkt_len;

	src_file = FS_Open(src, file_c::ACCESS_READ);
	if (! src_file)
		goto error_occurred;

	dest_file = FS_Open(dest, file_c::ACCESS_WRITE);
	if (! dest_file)
		goto error_occurred;

	buf = new unsigned char[COPY_BUF_SIZE];
	SYS_ASSERT(buf);

	size = src_file->GetLength();

	while (size > 0)
	{
		pkt_len = MIN(size, COPY_BUF_SIZE);

		if (src_file->Read(buf, pkt_len) != (unsigned int)pkt_len)
			goto error_occurred;

		if (dest_file->Write(buf, pkt_len) != (unsigned int)pkt_len)
			goto error_occurred;

		size -= pkt_len;
	}

	ok = true;

error_occurred:

	if (src_file)
		delete src_file;

	if (dest_file)
		delete dest_file;

	if (buf)
		delete[] buf;

	return ok;
}

bool FS_Delete(const char *name)
{
	SYS_ASSERT(name);

    return (unlink(name) == 0);
}

bool FS_Rename(const char *oldname, const char *newname)
{
	SYS_ASSERT(oldname);
	SYS_ASSERT(newname);

	return (rename(oldname, newname) != -1);
}

//
// -ACB- 2001/06/14
//
bool FS_GetModifiedTime(const char *filename, timestamp_c& t)
{
	struct stat buf;
	struct tm timeinf;

	// Check the sanity of the coders...
	SYS_ASSERT(filename);

	// Check the file is invalid
	if (stat(filename, &buf))			
		return false;

	// Convert the 'time_t' of the modified time into something more human
	if(! localtime_r(&buf.st_mtime, &timeinf))
		return false;

	t.Set(timeinf.tm_mday, timeinf.tm_mon+1, timeinf.tm_year+1900,
		  timeinf.tm_hour, timeinf.tm_min,   timeinf.tm_sec);

	return true;
}


} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
