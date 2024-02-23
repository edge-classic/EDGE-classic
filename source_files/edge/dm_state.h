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
// -MH- 1998/07/02 "lookupdown" --> "true3dgameplay"
//
// -ACB- 1999/10/07 Removed Sound Parameters - New Sound API
//

#ifndef __D_STATE_H__
#define __D_STATE_H__

#include "types.h"

#include "con_var.h"
// We need globally shared data structures,
//  for defining the global state variables.
#include "dm_data.h"

class image_c;

extern bool devparm; // DEBUG: launched with -devparm

extern gameflags_t global_flags;

extern gameflags_t level_flags;

// Selected by user.
extern skill_t game_skill;

// Flag: true only if started as net deathmatch.
// An enum might handle altdeath/cooperative better.
extern int deathmatch;

// -------------------------
// Status flags for refresh.
//

// Depending on view size - no status bar?
// Note that there is no way to disable the
//  status bar explicitely.
extern bool statusbaractive;
extern bool menuactive; // Menu overlayed?
extern bool rts_menuactive;
extern bool paused; // Game Pause?
extern bool viewactive;
extern bool nodrawers;
extern bool noblit;

// This one is related to the 3-screen display mode.
// kBAMAngle90 = left side, kBAMAngle270 = right
extern BAMAngle viewanglebaseoffset;

// Timer, for scores.
extern int  leveltime; // tics in game play for par
extern bool fast_forward_active;

// -AJA- 2000/12/07: auto quick-load feature
extern bool autoquickload;

//?
extern game_state_e game_state;

extern int maketic;
extern int game_tic;

#define DEATHMATCH() (deathmatch > 0)
#define COOP_MATCH() (deathmatch == 0 && numplayers > 1)
#define SP_MATCH()   (deathmatch == 0 && numplayers <= 1)

#define MAXHEALTH 200
#define MAXARMOUR 200

#define CHEATARMOUR     MAXARMOUR
#define CHEATARMOURTYPE kArmourTypeBlue

//-----------------------------------------
// Internal parameters, used for engine.
//

// File handling stuff.
extern std::string cfgfile;
extern std::string brandingfile;

extern std::string game_base;

extern std::string cache_dir;
extern std::string game_directory;
extern std::string home_directory;
extern std::string save_dir;
extern std::string shot_dir;

// if true, load all graphics at level load
extern bool precache;

// if true, enable HOM detection (hall of mirrors effect)
extern ConsoleVariable debug_hom;

extern int save_page;

extern int quickSaveSlot;

// debug flag to cancel adaptiveness
extern bool singletics;

extern int bodyqueslot;

// Needed to store the number of the dummy sky flat.
// Used for rendering, as well as tracking projectiles etc.

extern const image_c *skyflatimage;

#define IS_SKY(plane) ((plane).image == skyflatimage)

// misc stuff
extern bool swapstereo;
extern bool png_scrshots;

#define NUMHUD 120

extern int screen_hud;

extern int reduce_flash;

typedef enum
{
    INVULFX_Simple = 0, // plain inverse blending
    INVULFX_Textured,   // upload new textures

    NUM_INVULFX
} invulfx_type_e;

extern int var_invul_fx;

#endif /*__D_STATE_H__*/

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
