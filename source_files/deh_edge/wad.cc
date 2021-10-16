//------------------------------------------------------------------------
//  WAD I/O
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
#include "dh_plugin.h"
#include "wad.h"

#include "system.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace Deh_Edge
{

#define PWAD_HEADER  "PWAD"

#define MAX_LUMPS  2000

#define DEBUG_DDF  0


namespace WAD
{
	// --- TYPES ---

	//
	// Wad Info
	//
	typedef struct
	{
		char id[4];        // IWAD (whole) or PWAD (part)
		int numlumps;      // Number of Lumps
		int infotableofs;  // Info table offset
	}
	wadinfo_t;

	//
	// Lump stuff
	//
	typedef struct lump_s
	{
		byte *data;          // Data
		int filepos;         // Position in file
		int size;            // Size
		char name[8];        // Name
	}
	lump_t;

	// Lump list
	lump_t **lumplist = NULL;
	int num_lumps;

	lump_t *cur_lump = NULL;
	int cur_max_size;

	char wad_msg_buf[1024];

	//
	// PadFile
	//
	// Pads a file to the nearest 4 bytes.
	//
	void PadFile(FILE *fp)
	{
		unsigned char zeros[4] = { 0, 0, 0, 0 };
		
		int num = ftell(fp) % 4;

		if (num != 0)
		{
			fwrite(&zeros, 1, 4 - num, fp);
		}
	}

	//
	// LumpExists
	//
	int LumpExists(const char *name)
	{
		int i;

		for (i = 0; i < num_lumps; i++)
		{
			if (strncmp(lumplist[i]->name, name, 8) == 0)
				return i;
		}

		return -1;
	}
}

//
// WAD::NewLump
//
void WAD::NewLump(const char *name)
{
	if (cur_lump)
		InternalError("WAD_NewLump: current lump not finished.\n");

	// Check for existing lump, overwrite if need be.
	int i = WAD::LumpExists(name);

	if (i >= 0)
	{
		free(lumplist[i]->data);
	}
	else
	{
		i = num_lumps;

		num_lumps++;

		if (num_lumps > MAX_LUMPS)
			FatalError("Too many lumps ! (%d)\n", MAX_LUMPS);

		lumplist[i] = (lump_t *) calloc(sizeof(lump_t), 1);

		if (! lumplist[i])
			FatalError("Out of memory (adding %dth lump)\n", num_lumps);
	}

	cur_lump = lumplist[i];
	cur_max_size = 4;

	cur_lump->size = 0;
	cur_lump->data = (byte *) malloc(cur_max_size);

	if (! cur_lump->data)
		FatalError("Out of memory (New lump data)\n");

	strncpy(cur_lump->name, name, 8);
}

//
// WAD::AddData
//
void WAD::AddData(const byte *data, int size)
{
	if (! cur_lump)
		InternalError("WAD_AddData: no current lump.\n");
	
	if (cur_lump->size + size > cur_max_size)
	{
		while (cur_lump->size + size > cur_max_size)
			cur_max_size *= 2;

		cur_lump->data = (byte *) realloc(cur_lump->data, cur_max_size);

		if (! cur_lump->data)
			FatalError("Out of memory (adding %d bytes)\n", size);
	}

	memcpy(cur_lump->data + cur_lump->size, data, size);

	cur_lump->size += size;
}

//
// WAD::Printf
//
void WAD::Printf(const char *str, ...)
{
	va_list args;

	va_start(args, str);
	vsprintf(wad_msg_buf, str, args);
	va_end(args);

#if (DEBUG_DDF)
	fprintf(stderr, "%s", wad_msg_buf);
#else
	WAD::AddData((byte *) wad_msg_buf, strlen(wad_msg_buf));
#endif
}

//
// WAD::FinishLump
//
void WAD::FinishLump(void)
{
	if (! cur_lump)
		InternalError("WAD_FinishLump: not started.\n");

	cur_lump = NULL;
	cur_max_size = 0;
}

//
// WAD::WriteFile
//
dehret_e WAD::WriteFile(const char *name)
{
	if (cur_lump)
		InternalError("WAD_WriteFile: lump not finished.\n");

	FILE *fp;
	const char *pwadstr = PWAD_HEADER;

	// Open File
	fp = fopen(name, "wb");

	if (!fp)
	{
		int err_num = errno;

		SetErrorMsg("Cannot create output file: %s\n[%s]\n", name, strerror(err_num));
		return DEH_E_NoFile;
	}

	// Write Header
	int raw_num = Endian_U32(num_lumps);

	fwrite((void*)pwadstr, 1, 4, fp);
	fwrite((void*)&raw_num, sizeof(int), 1, fp);
	fwrite((void*)&raw_num, sizeof(int), 1, fp); // dummy - write later

	ProgressMinor(1, num_lumps+3);

	// Write Lumps
	int i;

	for (i = 0; i < num_lumps; i++)
	{
		PadFile(fp);
		lumplist[i]->filepos = ftell(fp);
		fwrite((void*)lumplist[i]->data, 1, lumplist[i]->size, fp);

		ProgressMinor(i+2, num_lumps+3);
	}

	PadFile(fp);
	int raw_offset = Endian_U32(ftell(fp));

	// Write Lumpinfos (directory table)
	for (i = 0; i < num_lumps; i++)
	{
		int raw_pos  = Endian_U32(lumplist[i]->filepos);
		int raw_size = Endian_U32(lumplist[i]->size);

		fwrite((void*)&raw_pos, sizeof(int), 1, fp);
		fwrite((void*)&raw_size, sizeof(int), 1, fp);
		fwrite((void*)lumplist[i]->name, 1, 8, fp);
	}

	ProgressMinor(num_lumps+2, num_lumps+3);

	// Write table offset
	fseek(fp, 8, SEEK_SET);
	fwrite((void*)&raw_offset, sizeof(int), 1, fp);

	// Close File
	fclose(fp);

	return DEH_OK;
}

//
// WAD::Startup
//
void WAD::Startup(void)
{
	lumplist = (lump_t **) calloc(sizeof(lump_t *), MAX_LUMPS);

	if (! lumplist)
		FatalError("Out of memory (WAD_Startup)\n");
	
	num_lumps = 0;
}

//
// WAD::Shutdown
//
void WAD::Shutdown(void)
{
	int i;

	// go through lump list and free lumps
	for (i = 0; i < num_lumps; i++)
	{
		free(lumplist[i]->data);
		free(lumplist[i]);
	}

	num_lumps = 0;

	if (lumplist)
	{
		free(lumplist);
		lumplist = 0;
	}
}

}  // Deh_Edge
