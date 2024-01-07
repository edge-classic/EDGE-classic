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

#ifndef __DEH_EDGE_PLUGIN_H__
#define __DEH_EDGE_PLUGIN_H__

// DDF
#include "collection.h"

#include <vector>
#include <string>

// Callback functions
typedef struct dehconvfuncs_s
{
    // Fatal errors are called as a last resort when something serious
    // goes wrong, e.g. out of memory.  This routine should show the
    // error to the user and abort the program.
    //
    void (*fatal_error)(const char *str, ...);

    // The print_msg routine is used to display informational messages
    // and warning messages (when enabled).
    //
    void (*print_msg)(const char *str, ...);
} dehconvfuncs_t;

typedef enum
{
    // everything was ship-shape.
    DEH_OK = 0,

    // an unknown error occurred (this is the catch-all value).
    DEH_E_Unknown,

    // problem parsing input file (maybe it wasn't a DeHackEd patch).
    DEH_E_ParseError
} dehret_e;

/* ------------ interface functions ------------ */

// startup: set the interface functions, reset static vars, etc..
void DehEdgeStartup(const dehconvfuncs_t *funcs);

// return the message for the last error, or an empty string if there
// was none.  Also clears the current error.  Never returns NULL.
const char *DehEdgeGetError(void);

// set quiet mode (disables warnings).
dehret_e DehEdgeSetQuiet(int quiet);

// add a single patch file (possibly from a WAD lump).
dehret_e DehEdgeAddLump(const char *data, int length);

// convert all the DeHackEd patch files into DDF.
dehret_e DehEdgeRunConversion(ddf_collection_c *dest);

// shut down: free all memory, close all files, etc..
void DehEdgeShutdown(void);

#endif /* __DEH_EDGE_PLUGIN_H__ */
