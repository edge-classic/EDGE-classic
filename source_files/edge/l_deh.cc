//----------------------------------------------------------------------------
//  EDGE DEH Interface
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2009  The EDGE Team.
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
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------

#include "i_defs.h"

#include "l_deh.h"

// EPI
#include "file.h"
#include "filesystem.h"

// DEH_EDGE
#include "deh_edge.h"


static char dh_message[1024];

//
// DH_PrintMsg
//
static void GCCATTR((format (printf,1,2)))
	DH_PrintMsg(const char *str, ...)
{
	va_list args;

	va_start(args, str);
	vsprintf(dh_message, str, args);
	va_end(args);

	I_Printf("DEH_EDGE: %s", dh_message);
}

//
// DH_FatalError
//
// Terminates the program reporting an error.
//
static void GCCATTR((format (printf,1,2)))
	DH_FatalError(const char *str, ...)
{
	va_list args;

	va_start(args, str);
	vsprintf(dh_message, str, args);
	va_end(args);

	I_Error("Converting DEH patch failed: %s\n", dh_message);
}

//
// DH_ProgressText
//
static void DH_ProgressText(const char *str)
{
	/* nothing needed */
}

//
// DH_ProgressBar
//
static void DH_ProgressBar(int percentage)
{
	/* nothing needed */
}

static const dehconvfuncs_t edge_dehconv_funcs =
{
	DH_FatalError,
	DH_PrintMsg,
	DH_ProgressBar,
	DH_ProgressText
};


deh_container_c * DH_ConvertFile(const char *filename)
{
	epi::file_c *F = epi::FS_Open(filename, epi::file_c::ACCESS_READ | epi::file_c::ACCESS_BINARY);
	if (F == NULL)
	{
		DH_PrintMsg("FAILED to open file: %s\n", filename);
		return NULL;
	}

	int length = F->GetLength();
	byte *data = F->LoadIntoMemory();

	if (data == NULL)
	{
		DH_PrintMsg("FAILED to read file: %s\n", filename);
		delete F;
		return NULL;
	}

	deh_container_c * container = DH_ConvertLump(data, length);

	// close file, free that data
	delete F;
	delete[] data;

	return container;
}


deh_container_c * DH_ConvertLump(const byte *data, int length)
{
	DehEdgeStartup(&edge_dehconv_funcs);

	dehret_e ret = DehEdgeAddLump((const char *)data, length);

	if (ret != DEH_OK)
	{
		DH_PrintMsg("FAILED to add lump:\n");
		DH_PrintMsg("- %s\n", DehEdgeGetError());

		DehEdgeShutdown();
		return NULL;
	}

	deh_container_c * container = new deh_container_c();

	ret = DehEdgeRunConversion(container);

	DehEdgeShutdown();

	if (ret != DEH_OK)
	{
		DH_PrintMsg("CONVERSION FAILED:\n");
		DH_PrintMsg("- %s\n", DehEdgeGetError());

		delete container;
		return NULL;
	}

	return container;
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
