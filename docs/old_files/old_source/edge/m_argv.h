//----------------------------------------------------------------------------
//  EDGE Arguments/Parameters Code
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2009  The EDGE Team.
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

#ifndef __M_ARGV__
#define __M_ARGV__

#include "i_defs.h"

// Returns the position of the given parameter
// in the arg list (0 if not found).
int M_CheckParm(const char *check);
int M_CheckNextParm(const char *check, int prev);
const char *M_GetParm(const char *check);
void M_InitArguments(int argc, const char **argv);
void M_ApplyResponseFile(const char *name, int position);
void M_CheckBooleanParm(const char *parm, bool *boolval, bool reverse);
void M_CheckBooleanCVar(const char *parm, cvar_c *var, bool reverse);
const char *M_GetArgument(int argnum);
int M_GetArgCount(void);
void M_DebugDumpArgs(void);

#endif // __M_ARGV__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
