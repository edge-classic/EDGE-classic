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

#include "m_random.h"

#include <random>

#include "i_system.h"

std::ranlux24_base                            stateless_ranlux24_generator;
std::ranlux24_base                            stateful_ranlux24_generator;
std::uniform_int_distribution<unsigned short> unsigned_8_bit_roll(0, 255);
std::uniform_int_distribution<unsigned short> unsigned_16_bit_roll(0, 0xFFFF);

static int state_index = 0;
static int state_step  = 1;

void RandomInit(void) { stateless_ranlux24_generator.seed(GetMicroseconds()); }

//
// RandomByte
//
// Returns a number from 0 to 255.
//
// -AJA- Note: this function should be called for all random values
// that do not interfere with netgame synchronisation (for example,
// selection of a random sound).
//
int RandomByte(void)
{
    return unsigned_8_bit_roll(stateless_ranlux24_generator);
}

//
// RandomByteSkewToZero
//
// Returns a number between -255 and 255, but skewed so that values near
// zero have a higher probability.  Replaces
// "RandomByteDeterministic()-RandomByteDeterministic()" in the code,
// which as Lee Killough points out can produce different results depending upon
// the order of evaluation.
//
// -AJA- Note: same usage rules as RandomByteDeterministic.
//
int RandomByteSkewToZero(void)
{
    int r1 = RandomByte();
    int r2 = RandomByte();

    return r1 - r2;
}

//
// RandomByteDeterministic
//
// Returns a number from 0 to 255.
//
// -AJA- Note: that this function should be called for all random values
// values that determine netgame synchronisation (for example,
// which way a monster should travel).
//
int RandomByteDeterministic(void)
{
    state_index += state_step;
    state_index &= 0xff;

    if (state_index == 0) state_step += (47 * 2);

    stateful_ranlux24_generator.seed(state_index + state_step);

    return unsigned_8_bit_roll(stateful_ranlux24_generator);
}

//
// RandomShort
//
// Returns a number from 0 to 65535 for COALAPI usage
//
int RandomShort(void)
{
    return unsigned_16_bit_roll(stateless_ranlux24_generator);
}

//
// RandomByteSkewToZeroDeterministic
//
// Returns a number between -255 and 255, but skewed so that values near
// zero have a higher probability.  Replaces
// "RandomByteDeterministic()-RandomByteDeterministic()" in the code,
// which as Lee Killough points out can produce different results depending upon
// the order of evaluation.
//
// -AJA- Note: same usage rules as RandomByteDeterministic.
//
int RandomByteSkewToZeroDeterministic(void)
{
    int r1 = RandomByteDeterministic();
    int r2 = RandomByteDeterministic();

    return r1 - r2;
}

//
// RandomByteTest
//
bool RandomByteTest(float chance)
{
    return (chance <= 0)                      ? false
           : (chance >= 1)                    ? true
           : (RandomByte() / 255.0f < chance) ? true
                                              : false;
}

//
// RandomByteTestDeterministic
//
bool RandomByteTestDeterministic(float chance)
{
    return (chance <= 0)                                   ? false
           : (chance >= 1)                                 ? true
           : (RandomByteDeterministic() / 255.0f < chance) ? true
                                                           : false;
}

//
// RandomStateRead
//
// These two routines are used for savegames.
//
int RandomStateRead(void)
{
    return (state_index & 0xff) | ((state_step & 0xff) << 8);
}

//
// RandomStateWrite
//
void RandomStateWrite(int value)
{
    state_index = (value & 0xff);
    state_step  = 1 + ((value >> 8) & 0xfe);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
