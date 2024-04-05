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
#include "ddf_main.h"
#include "deh_edge.h"
#include "i_system.h"

EDGE_DEFINE_CONSOLE_VARIABLE(debug_dehacked, "0", kConsoleVariableFlagArchive)

void ConvertDehacked(const uint8_t *data, int length, const std::string &source)
{
    DehackedStartup();

    DehackedResult ret = DehackedAddLump((const char *)data, length);

    if (ret != kDehackedConversionOK)
    {
        LogPrint("Dehacked: FAILED to add lump:\n");
        LogPrint("- %s\n", DehackedGetError());

        DehackedShutdown();

        FatalError("Failed to convert Dehacked file: %s\n", source.c_str());
    }

    std::vector<DDFFile> col;

    ret = DehackedRunConversion(&col);

    DehackedShutdown();

    if (ret != kDehackedConversionOK)
    {
        FatalError("Failed to convert Dehacked file: %s\n", source.c_str());
    }

    if (debug_dehacked.d_ > 0)
        DDFDumpCollection(col);

    DDFAddCollection(col, source);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
