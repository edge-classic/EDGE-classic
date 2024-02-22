//----------------------------------------------------------------------------
//  EDGE Basic Definitions File
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

#ifndef __DEFINITIONS__
#define __DEFINITIONS__

//
// Global parameters/defines.
//

// The current state of the game: whether we are
// playing, or gazing at the intermission screen/final animation

typedef enum
{
    GS_NOTHING = 0,
    GS_TITLESCREEN,
    GS_LEVEL,
    GS_INTERMISSION,
    GS_FINALE,
} game_state_e;

//
// Difficulty/skill settings/filters.
//

// Skill flags.
#define MTF_EASY   1
#define MTF_NORMAL 2
#define MTF_HARD   4

// Deaf monsters/do not react to sound.
#define MTF_AMBUSH 8

// Multiplayer only.
#define MTF_NOT_SINGLE 16

// -AJA- 1999/09/22: Boom compatibility.
#define MTF_NOT_DM   32
#define MTF_NOT_COOP 64

// -AJA- 2000/07/31: Friend flag, from MBF
#define MTF_FRIEND 128

// -AJA- 2004/11/04: This bit should be zero (otherwise old WAD).
#define MTF_RESERVED 256

// -AJA- 2008/03/08: Extrafloor placement
#define MTF_EXFLOOR_MASK  0x3C00
#define MTF_EXFLOOR_SHIFT 10

typedef enum
{
    sk_invalid   = -1,
    sk_baby      = 0,
    sk_easy      = 1,
    sk_medium    = 2,
    sk_hard      = 3,
    sk_nightmare = 4,
    sk_numtypes  = 5
} skill_t;

// -KM- 1998/12/16 Added gameflags typedef here.
typedef enum
{
    AA_OFF,
    AA_ON,
    AA_MLOOK
} autoaim_t;

typedef struct gameflags_s
{
    // checkparm of -nomonsters
    bool nomonsters;

    // checkparm of -fast
    bool fastparm;

    bool respawn;
    bool res_respawn;
    bool itemrespawn;

    bool true3dgameplay;
    int  menu_grav;
    bool more_blood;

    bool      jump;
    bool      crouch;
    bool      mlook;
    autoaim_t autoaim;

    bool cheats;
    bool have_extra;
    bool limit_zoom;

    bool kicking;
    bool weapon_switch;
    bool pass_missile;
    bool team_damage;
} gameflags_t;

#define VISIBLE   (1.0f)
#define VISSTEP   (1.0f / 256.0f)
#define INVISIBLE (0.0f)

//
// DOOM keyboard definition.
// This is the stuff configured by Setup.Exe.
// Most key data are simple ascii (uppercased).
//
#define KEYD_TAB       9
#define KEYD_ENTER     13
#define KEYD_ESCAPE    27
#define KEYD_SPACE     32
#define KEYD_BACKSPACE 127

#define KEYD_TILDE      ('`')
#define KEYD_EQUALS     ('=')
#define KEYD_MINUS      ('-')
#define KEYD_RIGHTARROW (0x80 + 0x2e)
#define KEYD_LEFTARROW  (0x80 + 0x2c)
#define KEYD_UPARROW    (0x80 + 0x2d)
#define KEYD_DOWNARROW  (0x80 + 0x2f)

#define KEYD_RCTRL  (0x80 + 0x1d)
#define KEYD_RSHIFT (0x80 + 0x36)
#define KEYD_RALT   (0x80 + 0x38)
#define KEYD_LALT   KEYD_RALT
#define KEYD_HOME   (0x80 + 0x47)
#define KEYD_PGUP   (0x80 + 0x49)
#define KEYD_END    (0x80 + 0x4f)
#define KEYD_PGDN   (0x80 + 0x51)
#define KEYD_INSERT (0x80 + 0x52)
#define KEYD_DELETE (0x80 + 0x53)

#define KEYD_F1  (0x80 + 0x3b)
#define KEYD_F2  (0x80 + 0x3c)
#define KEYD_F3  (0x80 + 0x3d)
#define KEYD_F4  (0x80 + 0x3e)
#define KEYD_F5  (0x80 + 0x3f)
#define KEYD_F6  (0x80 + 0x40)
#define KEYD_F7  (0x80 + 0x41)
#define KEYD_F8  (0x80 + 0x42)
#define KEYD_F9  (0x80 + 0x43)
#define KEYD_F10 (0x80 + 0x44)
#define KEYD_F11 (0x80 + 0x57)
#define KEYD_F12 (0x80 + 0x58)

#define KEYD_KP0      (0x80 + 0x60)
#define KEYD_KP1      (0x80 + 0x61)
#define KEYD_KP2      (0x80 + 0x62)
#define KEYD_KP3      (0x80 + 0x63)
#define KEYD_KP4      (0x80 + 0x64)
#define KEYD_KP5      (0x80 + 0x65)
#define KEYD_KP6      (0x80 + 0x66)
#define KEYD_KP7      (0x80 + 0x67)
#define KEYD_KP8      (0x80 + 0x68)
#define KEYD_KP9      (0x80 + 0x69)
#define KEYD_KP_DOT   (0x80 + 0x6a)
#define KEYD_KP_PLUS  (0x80 + 0x6b)
#define KEYD_KP_MINUS (0x80 + 0x6c)
#define KEYD_KP_STAR  (0x80 + 0x6d)
#define KEYD_KP_SLASH (0x80 + 0x6e)
#define KEYD_KP_EQUAL (0x80 + 0x6f)
#define KEYD_KP_ENTER (0x80 + 0x70)

#define KEYD_PRTSCR   (0x80 + 0x54)
#define KEYD_NUMLOCK  (0x80 + 0x45)
#define KEYD_SCRLOCK  (0x80 + 0x46)
#define KEYD_CAPSLOCK (0x80 + 0x7e)
#define KEYD_PAUSE    (0x80 + 0x7f)

// Values from here on aren't actually keyboard keys, but buttons
// on joystick or mice.

#define KEYD_MOUSE1   (0x100)
#define KEYD_MOUSE2   (0x101)
#define KEYD_MOUSE3   (0x102)
#define KEYD_MOUSE4   (0x103)
#define KEYD_MOUSE5   (0x104)
#define KEYD_MOUSE6   (0x105)
#define KEYD_WHEEL_UP (0x10e)
#define KEYD_WHEEL_DN (0x10f)

#define KEYD_GP_A          (0x110 + 1)
#define KEYD_GP_B          (0x110 + 2)
#define KEYD_GP_X          (0x110 + 3)
#define KEYD_GP_Y          (0x110 + 4)
#define KEYD_GP_BACK       (0x110 + 5)
#define KEYD_GP_GUIDE      (0x110 + 6)
#define KEYD_GP_START      (0x110 + 7)
#define KEYD_GP_LSTICK     (0x110 + 8)
#define KEYD_GP_RSTICK     (0x110 + 9)
#define KEYD_GP_LSHLD      (0x110 + 10)
#define KEYD_GP_RSHLD      (0x110 + 11)
#define KEYD_GP_UP         (0x110 + 12)
#define KEYD_GP_DOWN       (0x110 + 13)
#define KEYD_GP_LEFT       (0x110 + 14)
#define KEYD_GP_RIGHT      (0x110 + 15)
#define KEYD_GP_MISC1      (0x110 + 12)
#define KEYD_GP_PADDLE1    (0x110 + 13)
#define KEYD_GP_PADDLE2    (0x110 + 14)
#define KEYD_GP_PADDLE3    (0x110 + 15)
#define KEYD_GP_PADDLE4    (0x110 + 16)
#define KEYD_GP_TOUCHPAD   (0x110 + 17)
#define KEYD_TRIGGER_LEFT  (0x110 + 18)
#define KEYD_TRIGGER_RIGHT (0x110 + 19)

// Pseudo-keycodes for program functions - Dasho
#define KEYD_SCREENSHOT    (0x110 + 29)
#define KEYD_SAVEGAME      (0x110 + 30)
#define KEYD_LOADGAME      (0x110 + 31)
#define KEYD_SOUNDCONTROLS (0x110 + 32)
#define KEYD_OPTIONSMENU   (0x110 + 33)
#define KEYD_QUICKSAVE     (0x110 + 34)
#define KEYD_ENDGAME       (0x110 + 35)
#define KEYD_MESSAGETOGGLE (0x110 + 36)
#define KEYD_QUICKLOAD     (0x110 + 37)
#define KEYD_QUITEDGE      (0x110 + 38)
#define KEYD_GAMMATOGGLE   (0x110 + 39)

// -KM- 1998/09/27 Analogue binding, added a fly axis
#define AXIS_DISABLE 0
#define AXIS_TURN    1
#define AXIS_MLOOK   2
#define AXIS_FORWARD 3
#define AXIS_STRAFE  4
#define AXIS_FLY     5 // includes SWIM up/down

#endif // __DEFINITIONS__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
