//------------------------------------------------------------------------
//  SYSTEM : System specific code
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

#include "deh_system.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "deh_edge.h"
#include "stb_sprintf.h"

namespace dehacked
{

char global_error_buf[1024];
bool has_error_msg = false;

//
// System_Startup
//
void System_Startup(void)
{
    has_error_msg = false;
}

/* -------- text output code ----------------------------- */

void SetErrorMsg(const char *str, ...)
{
    va_list args;

    va_start(args, str);
    stbsp_vsprintf(global_error_buf, str, args);
    va_end(args);

    has_error_msg = true;
}

const char *GetErrorMsg(void)
{
    if (!has_error_msg)
        return "";

    has_error_msg = false;

    return global_error_buf;
}

} // namespace dehacked
