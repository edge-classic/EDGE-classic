//----------------------------------------------------------------------------
// EDGE Tic Command Definition
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

#ifndef __E_TICCMD_H__
#define __E_TICCMD_H__

#include <stdint.h>

// The data sampled per tick (single player)
// and transmitted to other peers (multiplayer).
// Mainly movements/button commands per game tick,

typedef struct
{
    // horizontal turning, *65536 for angle delta
    int16_t angleturn;

    // vertical angle for mlook, *65536 for angle delta
    int16_t mlookturn;

    uint16_t unused;

    // active player number, -1 for "dropped out" player
    int16_t player_idx;

    // /32 for move
    int8_t forwardmove;

    // /32 for move
    int8_t sidemove;

    // -MH- 1998/08/23 upward movement
    int8_t upwardmove;

    uint8_t buttons;

    uint16_t extbuttons;

    uint8_t chatchar;

    uint8_t unused2, unused3;
} ticcmd_t;

//
// Button/action code definitions.
//
typedef enum
{
    // Press "Fire".
    BT_ATTACK = 1,

    // Use button, to open doors, activate switches.
    BT_USE = 2,

    // Flag, weapon change pending.
    // If true, the next 4 bits hold weapon num.
    BT_CHANGE = 4,

    // The 3bit weapon mask and shift, convenience.
    BT_WEAPONMASK  = (8 + 16 + 32 + 64),
    BT_WEAPONSHIFT = 3,
} buttoncode_e;

// special weapon numbers
#define BT_NEXT_WEAPON 14
#define BT_PREV_WEAPON 15

//
// Extended Buttons: EDGE Specfics
// -ACB- 1998/07/03
//
typedef enum
{
    EBT_CENTER = 4,

    // -AJA- 2000/02/08: support for second attack.
    EBT_SECONDATK = 8,

    // -AJA- 2000/03/18: more control over zooming
    EBT_ZOOM = 16,

    // -AJA- 2004/11/10: manual weapon reload
    EBT_RELOAD = 32,

    // -AJA- 2009/09/07: custom action buttons
    EBT_ACTION1 = 64,
    EBT_ACTION2 = 128,

    EBT_INVPREV   = 256,
    EBT_INVUSE    = 512,
    EBT_INVNEXT   = 1024,
    EBT_THIRDATK  = 2048,
    EBT_FOURTHATK = 4096
} extbuttoncode_e;

#endif // __E_TICCMD_H__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
