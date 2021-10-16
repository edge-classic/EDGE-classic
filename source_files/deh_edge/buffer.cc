//------------------------------------------------------------------------
//  BUFFER for Parsing
//------------------------------------------------------------------------
//
//  DEH_EDGE  Copyright (C) 2004-2005  The EDGE Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License (in COPYING.txt) for more details.
//
//------------------------------------------------------------------------
//
//  DEH_EDGE is based on:
//
//  +  DeHackEd source code, by Greg Lewis.
//  -  DOOM source code (C) 1993-1996 id Software, Inc.
//  -  Linux DOOM Hack Editor, by Sam Lantinga.
//  -  PrBoom's DEH/BEX code, by Ty Halderman, TeamTNT.
//
//------------------------------------------------------------------------

#include "i_defs.h"
#include "buffer.h"

#include "dh_plugin.h"
#include "system.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

namespace Deh_Edge
{

#define DEBUG_BUFFER  0

class memory_buffer_c : public parse_buffer_api
{
private:
	const char *data;
	int length;

	const char *ptr; // current pointer into data

public:
	memory_buffer_c(const char *_data, int _length) :
		data(_data), length(_length), ptr(_data)
	{
		/* NOTE: data is not copied, but it _is_ freed */
	}

	~memory_buffer_c()
	{
		delete[] data;
		data = NULL;
	}

	bool eof()
	{
		return (ptr >= data + length);
	}

	bool error()
	{
		return false;
	}
	
	int read(void *buf, int count)
	{
		int avail = data + length - ptr;

		if (avail < count)
			count = avail;

		if (count <= 0)
			return 0;

		memcpy(buf, ptr, count);

		ptr += count;

		return count;
	}

	int getch()
	{
		if (eof())
			return EOF;

		return *ptr++;
	}

	void ungetch(int c)  /* NOTE: assumes c == last character read */
	{
		if (ptr > data)
			ptr--;
	}

	bool isBinary() const
	{
		if (length == 0)
			return false;

		int test_len = (length > 260) ? 256 : ((length * 3 + 1) / 4);

		for (; test_len > 0; test_len--)
			if (data[test_len - 1] == 0)
				return true;

		return false;
	}

	void showProgress()
	{
		ProgressMinor(ptr - data, length + 1);
	}
};

//------------------------------------------------------------------------

namespace Buffer
{

parse_buffer_api *OpenFile(const char *filename)
{
	// Note: always binary - we'll handle CR/LF ourselves
	FILE *fp = fopen(filename, "rb");

	if (fp == NULL)
	{
		int err_num = errno;
		SetErrorMsg("Could not open file: %s\n[%s]\n", filename, strerror(err_num));
		return NULL;
	}

	int length;

	// determine file size (by seeking to the end)
	fseek(fp, 0, SEEK_END);
	length = (int) ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (length < 0)
	{
		fclose(fp);

		int err_num = errno;
		SetErrorMsg("Couldn't get file size: %s\n[%s]\n", filename, strerror(err_num));
		return NULL;
	}

	char *f_data = new char[length + 1];

	fread(f_data, sizeof(char), length, fp);
	f_data[length] = 0;

	// close the file
	fclose(fp);

#if (DEBUG_BUFFER)
	Debug_PrintMsg("Loaded file: data %p length %d begin: %02x,%02x,%02x,%02x\n",
		f_data, length, f_data[0], f_data[1], f_data[2], f_data[3]);
#endif

	return new memory_buffer_c(f_data, length);
}

parse_buffer_api *OpenLump(const char *data, int length)
{
	if (length < 0)
		FatalError("Illegal length of lump (%d bytes)\n", length);
	
	char *d_copy = new char[length + 1];

	memcpy(d_copy, data, length);
	d_copy[length] = 0;

	return new memory_buffer_c(d_copy, length);
}

}  // Buffer

}  // Deh_Edge
