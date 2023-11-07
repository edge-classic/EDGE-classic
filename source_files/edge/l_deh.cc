//----------------------------------------------------------------------------
//  EDGE DEH Interface
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2023  The EDGE Team.
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

#include "i_defs.h"
#include "l_deh.h"

// EPI
#include "file.h"
#include "filesystem.h"

// DDF
#include "main.h"

// DEH_EDGE
#include "deh_edge.h"

DEF_CVAR(debug_dehacked, "0", CVAR_ARCHIVE)

static char dh_message[1024];

//
// DH_PrintMsg
//
static void GCCATTR((format(printf, 1, 2))) DH_PrintMsg(const char *str, ...)
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
static void GCCATTR((format(printf, 1, 2))) DH_FatalError(const char *str, ...)
{
    va_list args;

    va_start(args, str);
    vsprintf(dh_message, str, args);
    va_end(args);

    I_Error("Converting DEH patch failed: %s\n", dh_message);
}

static const dehconvfuncs_t edge_dehconv_funcs = {
    DH_FatalError,
    DH_PrintMsg,
};

void DEH_Convert(const byte *data, int length, const std::string &source)
{
    DehEdgeStartup(&edge_dehconv_funcs);

    dehret_e ret = DehEdgeAddLump((const char *)data, length);

    if (ret != DEH_OK)
    {
        DH_PrintMsg("FAILED to add lump:\n");
        DH_PrintMsg("- %s\n", DehEdgeGetError());

        DehEdgeShutdown();

        I_Error("Failed to convert DeHackEd file: %s\n", source.c_str());
    }

    ddf_collection_c col;

    ret = DehEdgeRunConversion(&col);

    DehEdgeShutdown();

    if (ret != DEH_OK)
    {
        I_Error("Failed to convert DeHackEd file: %s\n", source.c_str());
    }

    if (debug_dehacked.d > 0)
        DDF_DumpCollection(&col);

    DDF_AddCollection(&col, source);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
