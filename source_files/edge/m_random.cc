//----------------------------------------------------------------------------
//  EDGE Random LUT
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

#include "i_defs.h"
#include "m_random.h"

unsigned char rndtable[256] =
{
    0, 8, 109, 220, 222, 241, 149, 107, 75, 248, 254, 140, 16, 66,
    74, 21, 211, 47, 80, 242, 154, 27, 205, 128, 161, 89, 77, 36,
    95, 110, 85, 48, 212, 140, 211, 249, 22, 79, 200, 50, 28, 188,
    52, 140, 202, 120, 68, 145, 62, 70, 184, 190, 91, 197, 152, 224,
    149, 104, 25, 178, 252, 182, 202, 182, 141, 197, 4, 81, 181, 242,
    145, 42, 39, 227, 156, 198, 225, 193, 219, 93, 122, 175, 249, 0,
    175, 143, 70, 239, 46, 246, 163, 53, 163, 109, 168, 135, 2, 235,
    25, 92, 20, 145, 138, 77, 69, 166, 78, 176, 173, 212, 166, 113,
    94, 161, 41, 50, 239, 49, 111, 164, 70, 60, 2, 37, 171, 75,
    136, 156, 11, 56, 42, 146, 138, 229, 73, 146, 77, 61, 98, 196,
    135, 106, 63, 197, 195, 86, 96, 203, 113, 101, 170, 247, 181, 113,
    80, 250, 108, 7, 255, 237, 129, 226, 79, 107, 112, 166, 103, 241,
    24, 223, 239, 120, 198, 58, 60, 82, 128, 3, 184, 66, 143, 224,
    145, 224, 81, 206, 163, 45, 63, 90, 168, 114, 59, 33, 159, 95,
    28, 139, 123, 98, 125, 196, 15, 70, 194, 253, 54, 14, 109, 226,
    71, 17, 161, 93, 186, 87, 244, 138, 20, 52, 123, 251, 26, 36,
    17, 46, 52, 231, 232, 76, 31, 221, 84, 37, 216, 165, 212, 106,
    197, 242, 98, 43, 39, 175, 254, 145, 190, 84, 118, 222, 187, 136,
    120, 163, 236, 249
};

static int m_index = 0;
static int p_index = 0;
static int p_step = 1;

// 
// M_Random
//
// Returns a number from 0 to 255.
//
// -AJA- Note: this function should be called for all random values
// that do not interfere with demo/netgame synchronisation (for example,
// selection of a random sound).
//
int M_Random(void)
{
	return rndtable[++m_index & 0xff];
}

// 
// P_Random
//
// Returns a number from 0 to 255.
//
// -AJA- Note: that this function should be called for all random values
// values that determine demo/netgame synchronisation (for example,
// which way a monster should travel).
//
int P_Random(void)
{
	p_index += p_step;
	p_index &= 0xff;

	if (p_index == 0)
		p_step += (47 * 2);

	return rndtable[p_index];
}

// 
// P_RandomNegPos
//
// Returns a number between -255 and 255, but skewed so that values near
// zero have a higher probability.  Replaces "P_Random()-P_Random()" in
// the code, which as Lee Killough points out can produce different
// results depending upon the order of evaluation.
//
// -AJA- Note: same usage rules as P_Random.
//
int P_RandomNegPos(void)
{
	int r1 = P_Random();
	int r2 = P_Random();

	return r1 - r2;
}

//
// M_RandomTest
//
bool M_RandomTest(percent_t chance)
{
    return (chance <= 0) ? false :
           (chance >= 1) ? true :
           (M_Random()/255.0f < chance) ? true : false;
}

//
// P_RandomTest
//
bool P_RandomTest(percent_t chance)
{
    return (chance <= 0) ? false :
           (chance >= 1) ? true :
           (P_Random()/255.0f < chance) ? true : false;
}

//
// P_ReadRandomState
//
// These two routines are used for savegames.
//
int P_ReadRandomState(void)
{
	return (p_index & 0xff) | ((p_step & 0xff) << 8);
}

//
// P_WriteRandomState
//
void P_WriteRandomState(int value)
{
	p_index = (value & 0xff);
	p_step  = 1 + ((value >> 8) & 0xfe);
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
