//----------------------------------------------------------------------------
//  EDGE<->AJBSP Bridging code
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2009  The EDGE Team.
//  Adapted for AJBSP - Dashodanger 2021
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

#ifdef HAVE_EDGE_AJBSP_H
#include "edge_ajbsp.h"
#else
#include "edge_ajbsp.h"
#endif

#include "e_main.h"
#include "l_ajbsp.h"

static const nodebuildfuncs_t display_funcs =
{
	I_Printf,
	I_Debugf,
	I_Error,
	E_ProgressMessage
};

//
// AJ_BuildNodes
//
// Attempt to build nodes for the WAD file containing the given
// WAD file.  Returns true if successful, otherwise false.
//
bool AJ_BuildNodes(const char *filename, const char *outname)
{
	L_WriteDebug("AJ_BuildNodes: STARTED\n");
	L_WriteDebug("# source: '%s'\n", filename);
	L_WriteDebug("#   dest:  '%s'\n", outname);

	int ret = AJBSP_Build(filename, outname, &display_funcs);

	if (ret != 0)
	{
		L_WriteDebug("AJ_BuildNodes: FAILED\n");

		return false;
	}

	L_WriteDebug("AJ_BuildNodes: SUCCESS\n");

	return true;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
