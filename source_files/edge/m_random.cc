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

#include <stdint.h>

#include "i_system.h"
#include "prns.h"

prns_t      stateful_rng;
prns_down_t stateless_rng;

void InitRandomState(void)
{
    srand(GetMicroseconds());
    prns_set(&stateful_rng, rand());
    prns_down_init(&stateless_rng, &stateful_rng);
}

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
    return prns_down_next(&stateless_rng) % 256;
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
    return prns_next(&stateful_rng) % 256;
}

//
// RandomShort
//
// Returns a number from 0 to 65535 for COALAPI usage
//
int RandomShort(void)
{
    return prns_down_next(&stateless_rng) % 65536;
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
    return (chance <= 0) ? false : (chance >= 1) ? true : (RandomByte() / 255.0f < chance) ? true : false;
}

//
// RandomByteTestDeterministic
//
bool RandomByteTestDeterministic(float chance)
{
    return (chance <= 0) ? false : (chance >= 1) ? true : (RandomByteDeterministic() / 255.0f < chance) ? true : false;
}

//
// RandomStateRead
//
// These two routines are used for savegames.
//
uint64_t RandomStateRead(void)
{
    return prns_tell(&stateful_rng);
}

//
// RandomStateWrite
//
void RandomStateWrite(uint64_t value)
{
    prns_set(&stateful_rng, value);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
