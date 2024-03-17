//------------------------------------------------------------------------
//  DEH_PLUGIN.H : Plug-in Interface
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

#pragma once

#include <vector>

#include "ddf_collection.h"

enum DehackedResult
{
    // everything was ship-shape.
    kDehackedConversionOK = 0,
    // an unknown error occurred (this is the catch-all value).
    kDehackedConversionError,
    // problem parsing input file (maybe it wasn't a DeHackEd patch).
    kDehackedConversionParseError
};

/* ------------ interface functions ------------ */

// startup: set the interface functions, reset static vars, etc..
void DehackedStartup();

// return the message for the last error, or an empty string if there
// was none.  Also clears the current error.  Never returns nullptr.
const char *DehackedGetError(void);

// set quiet mode (disables warnings).
DehackedResult DehackedSetQuiet(int quiet);

// add a single patch file (possibly from a WAD lump).
DehackedResult DehackedAddLump(const char *data, int length);

// convert all the DeHackEd patch files into DDF.
DehackedResult DehackedRunConversion(std::vector<DDFFile> *dest);

// shut down: free all memory, close all files, etc..
void DehackedShutdown(void);