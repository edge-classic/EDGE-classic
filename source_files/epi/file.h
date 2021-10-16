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

#ifndef __EPI_FILE_CLASS__
#define __EPI_FILE_CLASS__ 

#include <limits.h>

namespace epi
{

// base class of the EDGE Platform Inferface File
class file_c
{
public:
    // Access Types
    enum access_e
    {
        ACCESS_READ   = 0x1,
        ACCESS_WRITE  = 0x2,
        ACCESS_APPEND = 0x4,
        ACCESS_BINARY = 0x8
    };

	// Seek reference points
    enum seek_e
	{
		SEEKPOINT_START,
		SEEKPOINT_CURRENT,
		SEEKPOINT_END,
		SEEKPOINT_NUMTYPES
	};

protected:

public:
	file_c() {}
	virtual ~file_c() {}

	virtual int GetLength() = 0;
	virtual int GetPosition() = 0;

	virtual unsigned int Read(void *dest, unsigned int size) = 0;
	virtual unsigned int Write(const void *src, unsigned int size) = 0;

	virtual bool Seek(int offset, int seekpoint) = 0;

public:
	byte *LoadIntoMemory(int max_size = INT_MAX);
	// load the file into memory, reading from the current
	// position, and reading no more than the 'max_size'
	// parameter (in bytes).  An extra NUL byte is appended
	// to the result buffer.  Returns NULL on failure.
	// The returned buffer must be freed with delete[].
};


// standard File class using ANSI C functions
class ansi_file_c: public file_c
{
private:
    FILE *fp;

public:
    ansi_file_c(FILE *_filep);
    ~ansi_file_c();

public:
    int GetLength();
    int GetPosition();

    unsigned int Read(void *dest, unsigned int size);
    unsigned int Write(const void *src, unsigned int size);

    bool Seek(int offset, int seekpoint);
};

// utility function:
bool FS_FlagsToAnsiMode(int flags, char *mode);
	
} // namespace epi

#endif /* __EPI_FILE_CLASS__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
