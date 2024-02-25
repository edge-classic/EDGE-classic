//----------------------------------------------------------------------------
//  EDGE Cheat Sequence Checking
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

#pragma once

#include "e_event.h"

//
// CHEAT SEQUENCE PACKAGE
//
// -KM- 1998/07/21 Needed in am_map.c (iddt cheat)
struct CheatSequence
{
    const char *sequence;
    const char *p;
};

int  CheatCheckSequence(CheatSequence *cheat, char key);
bool CheatResponder(InputEvent *event);
void CheatInitialize(void);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
