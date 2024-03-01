//----------------------------------------------------------------------------
//  EDGE Weapon (player sprites) Action Code
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

#include "main.h"

// maximum weapons player can hold at once
static constexpr uint8_t kMaximumWeapons = 64;

//
// Overlay psprites are scaled shapes
// drawn directly on the view screen,
// coordinates are given for a 320*200 view screen.
//
enum PlayerSpriteType
{
    kPlayerSpriteWeapon = 0,
    kPlayerSpriteFlash,
    kPlayerSpriteCrosshair,
    kPlayerSpriteUnused,
    // -AJA- Savegame code relies on kTotalPlayerSpriteTypes == 4.
    kTotalPlayerSpriteTypes
};

struct PlayerSprite
{
    // current state.  nullptr state means not active
    const State *state;

    // state to enter next.
    const State *next_state;

    // time (in tics) remaining for current state
    int tics;

    // screen position values (0 is normal)
    float screen_x, screen_y;

    // translucency values
    float visibility;
    float target_visibility;
};

enum PlayerWeaponFlag
{
    kPlayerWeaponNoFlag   = 0,
    kPlayerWeaponRemoving = 0x0001,  // weapon is being removed (or upgraded)
};

//
// Per-player Weapon Info.
//
struct PlayerWeapon
{
    WeaponDefinition *info;

    // player has this weapon.
    bool owned;

    // various flags
    PlayerWeaponFlag flags;

    // current clip sizes
    int clip_size[4];

    // reload clip counts
    int reload_count[4];

    int model_skin;
};

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
