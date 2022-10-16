//----------------------------------------------------------------------------
//  EDGE Console Main
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2022  The EDGE Team.
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

#ifndef __CON_MAIN_H
#define __CON_MAIN_H

#include "types.h"

#include <vector>

void CON_TryCommand(const char *cmd);

// Prints messages.  cf printf.
void CON_Printf(const char *message,...) GCCATTR((format(printf, 1, 2)));

void CON_PrintEndoom(int en_lump);

void CON_ClearLines();

// Like CON_Printf, but appends an extra '\n'. Should be used for player
// messages that need more than MessageLDF.

void CON_Message(const char *message, ...) GCCATTR((format(printf, 1, 2)));

// Looks up the string in LDF, appends an extra '\n', and then writes it to
// the console. Should be used for most player messages.
void CON_MessageLDF(const char *lookup, ...);

void CON_ImportantMessageLDF(const char *lookup, ...);

// -ACB- 1999/09/22
// Introduced because MSVC and DJGPP handle #defines differently
void CON_PlayerMessage(int plyr, const char *message, ...) GCCATTR((format(printf, 2, 3)));
// Looks up in LDF.
void CON_PlayerMessageLDF(int plyr, const char *message, ...);

// this color will apply to the next CON_Message or CON_Printf call.
void CON_MessageColor(rgbcol_t col);

typedef enum
{
	vs_notvisible,     // invisible
	vs_maximal,        // fullscreen + a command line
	vs_toggle
}
visible_t;

// Displays/Hides the console.
void CON_SetVisible(visible_t v);

int CON_MatchAllCmds(std::vector<const char *>& list,
                     const char *pattern);

#endif // __CON_MAIN_H

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
