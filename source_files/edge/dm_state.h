//----------------------------------------------------------------------------
//  EDGE Global State Variables
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
//
// -MH- 1998/07/02 "lookupdown" --> "true_3d_gameplay"
//
// -ACB- 1999/10/07 Removed Sound Parameters - New Sound API
//

#pragma once

#include "con_var.h"
#include "dm_defs.h"
#include "e_player.h"

class Image;

extern GameFlags global_flags;

extern GameFlags level_flags;

// Selected by user.
extern SkillLevel game_skill;

// Flag: true only if started as net deathmatch.
// An enum might handle altdeath/cooperative better.
extern int deathmatch;

// -------------------------
// Status flags for refresh.
//

// Depending on view size - no status bar?
// Note that there is no way to disable the
//  status bar explicitely.
extern bool menu_active; // Menu overlayed?
extern bool rts_menu_active;
extern bool paused;      // Game Pause?

// Timer, for scores.
extern int  level_time_elapsed; // tics in game play for par
extern bool fast_forward_active;

//?
extern GameState game_state;

extern int make_tic;

inline bool InDeathmatch(void)
{
    return (deathmatch > 0);
}
inline bool InCooperativeMatch(void)
{
    return (deathmatch == 0 && total_players > 1);
}
inline bool InSinglePlayerMatch(void)
{
    return (deathmatch == 0 && total_players <= 1);
}

// Dasho - Should this truly be hard capped at 200 ?
constexpr uint8_t kMaximumHealth = 200;
constexpr uint8_t kMaximumArmor  = 200;

//-----------------------------------------
// Internal parameters, used for engine.
//

// File handling stuff.
extern std::string configuration_file;
extern std::string branding_file;

extern std::string game_base;

extern std::string cache_directory;
extern std::string game_directory;
extern std::string home_directory;
extern std::string save_directory;
extern std::string screenshot_directory;

// if true, load all graphics at level load
extern bool precache;

// if true, enable HOM detection (hall of mirrors effect)
extern ConsoleVariable debug_hall_of_mirrors;

extern int save_page;

extern int quicksave_slot;

// debug flag to cancel adaptiveness
extern bool single_tics;

// Needed to store the number of the dummy sky flat.
// Used for rendering, as well as tracking projectiles etc.

extern const Image *sky_flat_image;

#define EDGE_IMAGE_IS_SKY(plane) ((plane).image == sky_flat_image)

// misc stuff
extern int screen_hud;

extern int reduce_flash;

enum InvulnerabilityEffectType
{
    kInvulnerabilitySimple = 0, // plain inverse blending
    kInvulnerabilityTextured,   // upload new textures
    kTotalInvulnerabilityEffects
};

extern int invulnerability_effect;

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
