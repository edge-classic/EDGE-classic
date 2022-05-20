//----------------------------------------------------------------------------
//  EDGE Filesystem Class for Win32
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

#include <time.h>
#include <sys/types.h> // Required for _stat()


namespace epi
{

struct createfile_s
{
	DWORD dwDesiredAccess;                      // access mode
	DWORD dwShareMode;                          // share mode
	DWORD dwCreationDisposition;                // how to create
};


bool FS_BuildWin32FileStruct(int epiflags, createfile_s* finfo)
{
    // Must have some value in epiflags
    if (epiflags == 0)
        return false;

	// Sanity checking...
	SYS_ASSERT(finfo);

    // Check for any invalid combinations
    if ((epiflags & file_c::ACCESS_WRITE) && (epiflags & file_c::ACCESS_APPEND))
        return false;

    if (epiflags & file_c::ACCESS_READ)
    {
		if (epiflags & file_c::ACCESS_WRITE)
		{
			finfo->dwDesiredAccess = GENERIC_READ|GENERIC_WRITE;
			finfo->dwShareMode = 0;
			finfo->dwCreationDisposition = OPEN_ALWAYS;
		}
		else if (epiflags & file_c::ACCESS_APPEND)
		{
			finfo->dwDesiredAccess = GENERIC_READ|GENERIC_WRITE;
			finfo->dwShareMode = 0;
			finfo->dwCreationDisposition = OPEN_EXISTING;
		}
		else
		{
			finfo->dwDesiredAccess = GENERIC_READ;
			finfo->dwShareMode = FILE_SHARE_READ;
			finfo->dwCreationDisposition = OPEN_EXISTING;
		}
    }
    else
    {
		if (epiflags & file_c::ACCESS_WRITE)
		{
			finfo->dwDesiredAccess = GENERIC_WRITE;
			finfo->dwShareMode = 0;
			finfo->dwCreationDisposition = OPEN_ALWAYS;
		}
		else if (epiflags & file_c::ACCESS_APPEND)
		{
			finfo->dwDesiredAccess = GENERIC_WRITE;
			finfo->dwShareMode = 0;
			finfo->dwCreationDisposition = OPEN_EXISTING;
		}
		else
		{
			return false;
		}
	}
    
    return true;
}


bool FS_GetCurrDir(char *dir, unsigned int bufsize)
{
	SYS_ASSERT(dir);

	return (::GetCurrentDirectory(bufsize, (LPSTR)dir) != FALSE);
}

bool FS_SetCurrDir(const char *dir)
{
	SYS_ASSERT(dir);

	return (::SetCurrentDirectory(dir) != FALSE);
}

bool FS_IsDir(const char *dir)
{
	SYS_ASSERT(dir);

	char curpath[MAX_PATH+1];

	FS_GetCurrDir(curpath, MAX_PATH);

	bool result = FS_SetCurrDir(dir);

	FS_SetCurrDir(curpath);
	return result;
}

bool FS_MakeDir(const char *dir)
{
	SYS_ASSERT(dir);

	return (::CreateDirectory(dir, NULL) != FALSE);
}

bool FS_RemoveDir(const char *dir)
{
	SYS_ASSERT(dir);

	return (::RemoveDirectory(dir) != FALSE);
}

bool FS_ReadDir(filesystem_dir_c *fsd, const char *dir, const char *mask)
{
	if (!dir || !fsd || !mask)
		return false;

	std::string prev_dir;

	// Bit of scope for the dir buffer to be held on stack 
	// for the minimum amount of time
	{
		char tmp[MAX_PATH+1];
		
		if (! FS_GetCurrDir(tmp, MAX_PATH))
			return false;

		prev_dir = std::string(tmp);
	}

	if (! FS_SetCurrDir(dir))
		return false;

	WIN32_FIND_DATA fdata;

	HANDLE handle = FindFirstFile(mask, &fdata);
	if (handle == INVALID_HANDLE_VALUE)
	{
		FS_SetCurrDir(prev_dir.c_str());
		return false;
	}

	// Ensure the container is empty
	fsd->Clear();

	do
	{
	    if (strcmp(fdata.cFileName, ".")  == 0 ||
			strcmp(fdata.cFileName, "..") == 0)
		{
		  // skip the funky "." and ".." dirs 
		}
		else
		{
			filesys_direntry_c *entry = new filesys_direntry_c();

			entry->name = std::string(fdata.cFileName);
			entry->size = fdata.nFileSizeLow;
			entry->is_dir = (fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)?true:false; 

			if (! fsd->AddEntry(entry))
			{
				delete entry;
				FindClose(handle);
				FS_SetCurrDir(prev_dir.c_str());
				return false;
			}
		}
	}
	while (FindNextFile(handle, &fdata));

	FindClose(handle);

	FS_SetCurrDir(prev_dir.c_str());
	return true;
}


#if 0 // using common code
bool FS_Access(const char *name, unsigned int flags)
{
	createfile_s fs;
	HANDLE handle;

	if (! FS_BuildWin32FileStruct(flags, &fs))
		return false;

	handle = CreateFile(name, fs.dwDesiredAccess, fs.dwShareMode, 
							NULL, fs.dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);

	if (handle == INVALID_HANDLE_VALUE)	
		return false;

	CloseHandle(handle);
	return true;
}
#endif


bool FS_Copy(const char *src, const char *dest)
{
	SYS_ASSERT(src && dest);

	// Copy src to dest overwriting dest if it exists
	return (::CopyFile(src, dest, FALSE) != FALSE);
}

bool FS_Delete(const char *name)
{
	SYS_ASSERT(name);

	return (::DeleteFile(name) != FALSE);
}

#if 0 // using common code
file_c* FS_Open(const char *name, unsigned int flags)
{
	SYS_ASSERT(name);

	createfile_s fs;
	win32_file_c *file;
	HANDLE handle;

	if (!FS_BuildWin32FileStruct(flags, &fs))
		return false;

	handle = CreateFile(name, fs.dwDesiredAccess, fs.dwShareMode, 
							NULL, fs.dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);

	if (handle == INVALID_HANDLE_VALUE)	
		return false;

    file = new win32_file_c;
    if (!file)
    {
        CloseHandle(handle);
        return NULL;
    }

    file->Setup(this, handle);
    return file;
}
#endif

//
// bool FS_Rename()
//
bool FS_Rename(const char *oldname, const char *newname)
{
	SYS_ASSERT(oldname);
	SYS_ASSERT(newname);

	return (::MoveFile(oldname, newname) != FALSE);
}

//
// FS_GetModifiedTime
//
// Fills in 'timestamp_c' to match the modified time of 'filename'. Returns
// true on success.
//
// -ACB- 2001/06/14
// -ACB- 2004/02/15 Use native win32 functions: they should work!
//
bool FS_GetModifiedTime(const char *filename, timestamp_c& t)
{
	SYSTEMTIME timeinf;
	HANDLE handle;
	WIN32_FIND_DATA fdata;

	// Check the sanity of the coders...
	SYS_ASSERT(filename);

	// Get the file info...
	handle = FindFirstFile(filename, &fdata);
	if (handle == INVALID_HANDLE_VALUE)
		return false;

	// Convert to a time we can actually use...
	if (! FileTimeToSystemTime(&fdata.ftLastWriteTime, &timeinf))
	{
		FindClose(handle);
		return false;
	}

	FindClose(handle);

	t.Set( (byte)timeinf.wDay,    (byte)timeinf.wMonth,
		   (short)timeinf.wYear,  (byte)timeinf.wHour,
		   (byte)timeinf.wMinute, (byte)timeinf.wSecond );

	return true;
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
