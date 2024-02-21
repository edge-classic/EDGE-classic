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

#pragma once

#include <stdint.h>

// The data sampled per tick (single player)
// and transmitted to other peers (multiplayer).
// Mainly movements/button commands per game tick,

struct EventTicCommand
{
    // horizontal turning, *65536 for angle delta
    int16_t angle_turn;
    // vertical angle for mlook, *65536 for angle delta
    int16_t  mouselook_turn;
    uint16_t unused;
    // active player number, -1 for "dropped out" player
    int16_t player_index;
    // /32 for move
    int8_t forward_move;
    // /32 for move
    int8_t side_move;
    // -MH- 1998/08/23 upward movement
    int8_t   upward_move;
    uint8_t  buttons;
    uint16_t extended_buttons;
    uint8_t  chat_character;
    uint8_t  unused2, unused3;
};

//
// Button/action code definitions.
//
enum EventButtonCode
{
    // Press "Fire".
    kButtonCodeAttack = 1,
    // Use button, to open doors, activate switches.
    kButtonCodeUse = 2,
    // Flag, weapon change pending.
    // If true, the next 4 bits hold weapon num.
    kButtonCodeChangeWeapon = 4,
    // The 3bit weapon mask and shift, convenience.
    kButtonCodeWeaponMask      = (8 + 16 + 32 + 64),
    kButtonCodeWeaponMaskShift = 3,
};

// special weapon numbers
constexpr uint8_t kButtonCodeNextWeapon     = 14;
constexpr uint8_t kButtonCodePreviousWeapon = 15;

//
// Extended Buttons: EDGE Specfics
// -ACB- 1998/07/03
//
enum EventExtendedButtonCode
{
    kExtendedButtonCodeCenter = 4,
    // -AJA- 2000/02/08: support for second attack.
    kExtendedButtonCodeSecondAttack = 8,
    // -AJA- 2000/03/18: more control over zooming
    kExtendedButtonCodeZoom = 16,
    // -AJA- 2004/11/10: manual weapon reload
    kExtendedButtonCodeReload = 32,
    // -AJA- 2009/09/07: custom action buttons
    kExtendedButtonCodeAction1           = 64,
    kExtendedButtonCodeAction2           = 128,
    kExtendedButtonCodeInventoryPrevious = 256,
    kExtendedButtonCodeInventoryUse      = 512,
    kExtendedButtonCodeInventoryNext     = 1024,
    kExtendedButtonCodeThirdAttack       = 2048,
    kExtendedButtonCodeFourthAttack      = 4096
};

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
