//------------------------------------------------------------------------
//  WAD I/O
//------------------------------------------------------------------------
//
//  DEH_EDGE  Copyright (C) 2004-2024 The EDGE Team
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

#include "deh_wad.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "deh_edge.h"
#include "deh_system.h"
#include "epi.h"

namespace dehacked
{

namespace wad
{

std::vector<DDFFile> *dest_container = nullptr;

DDFFile *cur_lump = nullptr;

char wad_msg_buf[1024];

void NewLump(DDFType type)
{
    if (dest_container == nullptr)
        FatalError("Dehacked: Error - WAD_NewLump: no container!\n");

    dest_container->push_back({type, "", ""});

    cur_lump = &dest_container->back();
}

void Printf(const char *str, ...)
{
    if (cur_lump == nullptr)
        FatalError("Dehacked: Error - WAD_Printf: not started.\n");

    va_list args;

    va_start(args, str);
    vsprintf(wad_msg_buf, str, args);
    va_end(args);

    cur_lump->data += (const char *)wad_msg_buf;
}

} // namespace wad

} // namespace dehacked
