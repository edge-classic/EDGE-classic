//----------------------------------------------------------------------------
//  EDGE RNG
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

// A bit verbose, but hopefully describes what they do decently.
// The "Deterministic" suffixes increment and track the index and step
// for its random number generator so that loading/saving a game
// does not change the outcomes of functions that use them - Dasho
void InitRandomState(void);
int  RandomByte(void);
int  RandomByteSkewToZero(void);
int  RandomByteDeterministic(void);
int  RandomShort(void);
int  RandomByteSkewToZeroDeterministic(void);
bool RandomByteTest(float chance);
bool RandomByteTestDeterministic(float chance);

// Savegame support
int  RandomStateRead(void);
void RandomStateWrite(int value);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
