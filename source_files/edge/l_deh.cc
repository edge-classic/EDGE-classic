//----------------------------------------------------------------------------
//  EDGE DEH Interface
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2024 The EDGE Team.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
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


#include "l_deh.h"

#include "con_var.h"

// EPI
#include "file.h"
#include "filesystem.h"

// DDF
#include "main.h"

// DEH_EDGE
#include "deh_edge.h"

DEF_CVAR(debug_dehacked, "0", CVAR_ARCHIVE)

void ConvertDehacked(const uint8_t *data, int length, const std::string &source)
{
    DehackedStartup();

    DehackedResult ret = DehackedAddLump((const char *)data, length);

    if (ret != kDehackedConversionOK)
    {
        I_Printf("Dehacked: FAILED to add lump:\n");
        I_Printf("- %s\n", DehackedGetError());

        DehackedShutdown();

        I_Error("Failed to convert Dehacked file: %s\n", source.c_str());
    }

    std::vector<DDFFile> col;

    ret = DehackedRunConversion(&col);

    DehackedShutdown();

    if (ret != kDehackedConversionOK)
    {
        I_Error("Failed to convert Dehacked file: %s\n", source.c_str());
    }

    if (debug_dehacked.d > 0)
        DDF_DumpCollection(col);

    DDF_AddCollection(col, source);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
