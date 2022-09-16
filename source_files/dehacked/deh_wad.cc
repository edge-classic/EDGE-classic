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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "deh_i_defs.h"
#include "deh_edge.h"

#include "deh_system.h"
#include "deh_wad.h"

namespace Deh_Edge
{

#define PWAD_HEADER  "PWAD"

#define MAX_LUMPS  2000

#define DEBUG_DDF  0


namespace WAD
{
	deh_container_c * dest_container = NULL;

	deh_lump_c * cur_lump = NULL;

	char wad_msg_buf[1024];
}


void WAD::NewLump(const char *name)
{
	if (cur_lump != NULL)
		InternalError("WAD_NewLump: no container!\n");

	if (cur_lump != NULL)
		InternalError("WAD_NewLump: current lump not finished.\n");

	cur_lump = new deh_lump_c(name);

	dest_container->AddLump(cur_lump);
}


void WAD::Printf(const char *str, ...)
{
	if (cur_lump == NULL)
		InternalError("WAD_Printf: not started.\n");

	va_list args;

	va_start(args, str);
	vsprintf(wad_msg_buf, str, args);
	va_end(args);

#if DEBUG_DDF
	fprintf(stderr, "%s", wad_msg_buf);
#else
	cur_lump->data += (char *) wad_msg_buf;
#endif
}


}  // Deh_Edge
