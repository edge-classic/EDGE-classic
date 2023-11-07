//------------------------------------------------------------------------
//  WAD I/O
//------------------------------------------------------------------------
//
//  DEH_EDGE  Copyright (C) 2004-2023  The EDGE Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
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

namespace WAD
{

ddf_collection_c *dest_container = NULL;

ddf_file_c *cur_lump = NULL;

char wad_msg_buf[1024];

void NewLump(ddf_type_e type)
{
    if (dest_container == NULL)
        InternalError("WAD_NewLump: no container!\n");

    dest_container->files.push_back(ddf_file_c(type, ""));

    cur_lump = &dest_container->files.back();
}

void Printf(const char *str, ...)
{
    if (cur_lump == NULL)
        InternalError("WAD_Printf: not started.\n");

    va_list args;

    va_start(args, str);
    vsprintf(wad_msg_buf, str, args);
    va_end(args);

    cur_lump->data += (const char *)wad_msg_buf;
}

} // namespace WAD

} // namespace Deh_Edge
