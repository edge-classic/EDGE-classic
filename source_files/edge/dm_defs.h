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

#pragma once

#include <stdint.h>

//
// Global parameters/defines.
//

// The current state of the game: whether we are
// playing, or gazing at the intermission screen/final animation

enum GameState
{
    kGameStateNothing = 0,
    kGameStateTitleScreen,
    kGameStateLevel,
    kGameStateIntermission,
    kGameStateFinale,
};

//
// Difficulty/skill settings/filters.
//

enum SkillLevel
{
    kSkillInvalid     = -1,
    kSkillBaby        = 0,
    kSkillEasy        = 1,
    kSkillMedium      = 2,
    kSkillHard        = 3,
    kSkillNightmare   = 4,
    kTotalSkillLevels = 5
};

// -KM- 1998/12/16 Added gameflags typedef here.
enum AutoAimState
{
    kAutoAimOff,
    kAutoAimOn,
    kAutoAimMouselook
};

struct GameFlags
{
    // checkparm of -nomonsters
    bool no_monsters;

    // checkparm of -fast
    bool fast_monsters;

    bool enemies_respawn;
    bool enemy_respawn_mode;
    bool items_respawn;

    bool true_3d_gameplay;
    int  menu_gravity_factor;
    bool more_blood;

    bool         jump;
    bool         crouch;
    bool         mouselook;
    AutoAimState autoaim;

    bool cheats;
    bool have_extra;
    bool limit_zoom;

    bool kicking;
    bool weapon_switch;
    bool pass_missile;
    bool team_damage;
};

//
// DOOM keyboard definition.
// This is the stuff configured by Setup.Exe.
// Most key data are simple ascii (uppercased).
//

constexpr uint8_t kTab       = 9;
constexpr uint8_t kEnter     = 13;
constexpr uint8_t kEscape    = 27;
constexpr uint8_t kSpace     = 32;
constexpr uint8_t kBackspace = 127;

constexpr uint8_t kTilde      = ('`');
constexpr uint8_t kEquals     = ('=');
constexpr uint8_t kMinus      = ('-');
constexpr uint8_t kRightArrow = (0x80 + 0x2e);
constexpr uint8_t kLeftArrow  = (0x80 + 0x2c);
constexpr uint8_t kUpArrow    = (0x80 + 0x2d);
constexpr uint8_t kDownArrow  = (0x80 + 0x2f);

constexpr uint8_t kRightControl = (0x80 + 0x1d);
constexpr uint8_t kRightShift   = (0x80 + 0x36);
constexpr uint8_t kRightAlt     = (0x80 + 0x38);
constexpr uint8_t kLeftAlt      = kRightAlt;
constexpr uint8_t kHome         = (0x80 + 0x47);
constexpr uint8_t kPageUp       = (0x80 + 0x49);
constexpr uint8_t kEnd          = (0x80 + 0x4f);
constexpr uint8_t kPageDown     = (0x80 + 0x51);
constexpr uint8_t kInsert       = (0x80 + 0x52);
constexpr uint8_t kDelete       = (0x80 + 0x53);

constexpr uint8_t kFunction1  = (0x80 + 0x3b);
constexpr uint8_t kFunction2  = (0x80 + 0x3c);
constexpr uint8_t kFunction3  = (0x80 + 0x3d);
constexpr uint8_t kFunction4  = (0x80 + 0x3e);
constexpr uint8_t kFunction5  = (0x80 + 0x3f);
constexpr uint8_t kFunction6  = (0x80 + 0x40);
constexpr uint8_t kFunction7  = (0x80 + 0x41);
constexpr uint8_t kFunction8  = (0x80 + 0x42);
constexpr uint8_t kFunction9  = (0x80 + 0x43);
constexpr uint8_t kFunction10 = (0x80 + 0x44);
constexpr uint8_t kFunction11 = (0x80 + 0x57);
constexpr uint8_t kFunction12 = (0x80 + 0x58);

constexpr uint8_t kKeypad0      = (0x80 + 0x60);
constexpr uint8_t kKeypad1      = (0x80 + 0x61);
constexpr uint8_t kKeypad2      = (0x80 + 0x62);
constexpr uint8_t kKeypad3      = (0x80 + 0x63);
constexpr uint8_t kKeypad4      = (0x80 + 0x64);
constexpr uint8_t kKeypad5      = (0x80 + 0x65);
constexpr uint8_t kKeypad6      = (0x80 + 0x66);
constexpr uint8_t kKeypad7      = (0x80 + 0x67);
constexpr uint8_t kKeypad8      = (0x80 + 0x68);
constexpr uint8_t kKeypad9      = (0x80 + 0x69);
constexpr uint8_t kKeypadDot    = (0x80 + 0x6a);
constexpr uint8_t kKeypadPlus   = (0x80 + 0x6b);
constexpr uint8_t kKeypadMinus  = (0x80 + 0x6c);
constexpr uint8_t kKeypadStar   = (0x80 + 0x6d);
constexpr uint8_t kKeypadSlash  = (0x80 + 0x6e);
constexpr uint8_t kKeypadEquals = (0x80 + 0x6f);
constexpr uint8_t kKeypadEnter  = (0x80 + 0x70);

constexpr uint8_t kPrintScreen = (0x80 + 0x54);
constexpr uint8_t kNumberLock  = (0x80 + 0x45);
constexpr uint8_t kScrollLock  = (0x80 + 0x46);
constexpr uint8_t kCapsLock    = (0x80 + 0x7e);
constexpr uint8_t kPause       = (0x80 + 0x7f);

// Values from here on aren't actually keyboard keys, but buttons
// on joystick or mice.

constexpr uint16_t kMouse1         = (0x100);
constexpr uint16_t kMouse2         = (0x101);
constexpr uint16_t kMouse3         = (0x102);
constexpr uint16_t kMouse4         = (0x103);
constexpr uint16_t kMouse5         = (0x104);
constexpr uint16_t kMouse6         = (0x105);
constexpr uint16_t kMouseWheelUp   = (0x10e);
constexpr uint16_t kMouseWheelDown = (0x10f);

constexpr uint16_t kGamepadA             = (0x110 + 1);
constexpr uint16_t kGamepadB             = (0x110 + 2);
constexpr uint16_t kGamepadX             = (0x110 + 3);
constexpr uint16_t kGamepadY             = (0x110 + 4);
constexpr uint16_t kGamepadBack          = (0x110 + 5);
constexpr uint16_t kGamepadGuide         = (0x110 + 6);
constexpr uint16_t kGamepadStart         = (0x110 + 7);
constexpr uint16_t kGamepadLeftStick     = (0x110 + 8);
constexpr uint16_t kGamepadRightStick    = (0x110 + 9);
constexpr uint16_t kGamepadLeftShoulder  = (0x110 + 10);
constexpr uint16_t kGamepadRightShoulder = (0x110 + 11);
constexpr uint16_t kGamepadUp            = (0x110 + 12);
constexpr uint16_t kGamepadDown          = (0x110 + 13);
constexpr uint16_t kGamepadLeft          = (0x110 + 14);
constexpr uint16_t kGamepadRight         = (0x110 + 15);
constexpr uint16_t kGamepadMisc1         = (0x110 + 12);
constexpr uint16_t kGamepadPaddle1       = (0x110 + 13);
constexpr uint16_t kGamepadPaddle2       = (0x110 + 14);
constexpr uint16_t kGamepadPaddle3       = (0x110 + 15);
constexpr uint16_t kGamepadPaddle4       = (0x110 + 16);
constexpr uint16_t kGamepadTouchpad      = (0x110 + 17);
constexpr uint16_t kGamepadTriggerLeft   = (0x110 + 18);
constexpr uint16_t kGamepadTriggerRight  = (0x110 + 19);

// Pseudo-keycodes for program functions - Dasho;
constexpr uint16_t kScreenshot    = (0x110 + 29);
constexpr uint16_t kSaveGame      = (0x110 + 30);
constexpr uint16_t kLoadGame      = (0x110 + 31);
constexpr uint16_t kSoundControls = (0x110 + 32);
constexpr uint16_t kOptionsMenu   = (0x110 + 33);
constexpr uint16_t kQuickSave     = (0x110 + 34);
constexpr uint16_t kEndGame       = (0x110 + 35);
constexpr uint16_t kMessageToggle = (0x110 + 36);
constexpr uint16_t kQuickLoad     = (0x110 + 37);
constexpr uint16_t kQuitEdge      = (0x110 + 38);
constexpr uint16_t kGammaToggle   = (0x110 + 39);

// -KM- 1998/09/27 Analogue binding, added a fly axis
constexpr uint8_t kAxisDisable   = 0;
constexpr uint8_t kAxisTurn      = 1;
constexpr uint8_t kAxisMouselook = 2;
constexpr uint8_t kAxisForward   = 3;
constexpr uint8_t kAxisStrafe    = 4;
constexpr uint8_t kAxisFly       = 5; // includes SWIM up/down

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
