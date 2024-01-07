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

#include "i_defs.h"
#include "m_random.h"
#include <random>

std::ranlux24_base                            m_rand;
std::ranlux24_base                            p_rand;
std::uniform_int_distribution<unsigned short> rand_roll(0, 255);
std::uniform_int_distribution<unsigned short> coal_roll(0, 0xFFFF);

static int p_index = 0;
static int p_step  = 1;

void M_Random_Init(void)
{
    m_rand.seed(I_GetMicros());
}

//
// M_Random
//
// Returns a number from 0 to 255.
//
// -AJA- Note: this function should be called for all random values
// that do not interfere with netgame synchronisation (for example,
// selection of a random sound).
//
int M_Random(void)
{
    return rand_roll(m_rand);
}

//
// M_RandomNegPos
//
// Returns a number between -255 and 255, but skewed so that values near
// zero have a higher probability.  Replaces "P_Random()-P_Random()" in
// the code, which as Lee Killough points out can produce different
// results depending upon the order of evaluation.
//
// -AJA- Note: same usage rules as P_Random.
//
int M_RandomNegPos(void)
{
    int r1 = M_Random();
    int r2 = M_Random();

    return r1 - r2;
}

//
// P_Random
//
// Returns a number from 0 to 255.
//
// -AJA- Note: that this function should be called for all random values
// values that determine netgame synchronisation (for example,
// which way a monster should travel).
//
int P_Random(void)
{
    p_index += p_step;
    p_index &= 0xff;

    if (p_index == 0)
        p_step += (47 * 2);

    p_rand.seed(p_index + p_step);

    return rand_roll(p_rand);
}

//
// C_Random
//
// Returns a number from 0 to 65535 for COALAPI usage
//
int C_Random(void)
{
    return coal_roll(m_rand);
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
    return (chance <= 0) ? false : (chance >= 1) ? true : (M_Random() / 255.0f < chance) ? true : false;
}

//
// P_RandomTest
//
bool P_RandomTest(percent_t chance)
{
    return (chance <= 0) ? false : (chance >= 1) ? true : (P_Random() / 255.0f < chance) ? true : false;
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
