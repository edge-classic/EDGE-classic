//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C)      2022 Andrew Apted
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//   System interface for music.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "genmidi.h"

// GENMIDI lump:

static genmidi_lump_t genmidi;

// Load instrument table from GENMIDI lump:

bool GM_LoadInstruments(const uint8_t *data, size_t length)
{
    if (length < sizeof(genmidi_lump_t))
    {
        return false;
    }

    // DMX does not check header

    memcpy((uint8_t *) &genmidi, data, sizeof(genmidi_lump_t));

    return true;
}

genmidi_instr_t * GM_GetInstrument(int key)
{
    if (key < 0 || key >= GENMIDI_NUM_INSTRS)
        return NULL;

    return &genmidi.instrs[key];
}

genmidi_instr_t * GM_GetPercussion(int key)
{
    key = key - 35;

    if (key < 0 || key >= GENMIDI_NUM_PERCUSSION)
        return NULL;

    return &genmidi.instrs[GENMIDI_NUM_INSTRS + key];
}
