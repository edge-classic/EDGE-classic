//----------------------------------------------------------------------------
//  EDGE Default Settings
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

#pragma once

// Screen resolution
#define CFGDEF_SCREENWIDTH                                                     \
    (1000000)  // Super high number to force scaling to native res
#define CFGDEF_SCREENHEIGHT                                                    \
    (1000000)  // Super high number to force scaling to native res
#define CFGDEF_SCREENBITS  (32)
#define CFGDEF_DISPLAYMODE (2)

// Controls (Key/Mouse Buttons)
#define CFGDEF_KEY_FIRE        (kMouse1 + (kGamepadTriggerRight << 16))
#define CFGDEF_KEY_SECONDATK   ('e' + (kGamepadTriggerLeft << 16))
#define CFGDEF_KEY_USE         (kSpace + (kGamepadA << 16))
#define CFGDEF_KEY_UP          (kUpArrow + ('w' << 16))
#define CFGDEF_KEY_DOWN        (kDownArrow + ('s' << 16))
#define CFGDEF_KEY_LEFT        (kLeftArrow)
#define CFGDEF_KEY_RIGHT       (kRightArrow)
#define CFGDEF_KEY_FLYUP       ('/' + (kGamepadLeftStick << 16))
#define CFGDEF_KEY_FLYDOWN     ('c' + (kGamepadRightStick << 16))
#define CFGDEF_KEY_SPEED       (kRightShift)
#define CFGDEF_KEY_STRAFE      (kRightAlt + (kMouse3 << 16))
#define CFGDEF_KEY_STRAFELEFT  (',' + ('a' << 16))
#define CFGDEF_KEY_STRAFERIGHT ('.' + ('d' << 16))
#define CFGDEF_KEY_AUTORUN     (kCapsLock)

#define CFGDEF_KEY_LOOKUP     (kPageUp)
#define CFGDEF_KEY_LOOKDOWN   (kPageDown)
#define CFGDEF_KEY_LOOKCENTER (kHome)
#define CFGDEF_KEY_MLOOK      ('m')
#define CFGDEF_KEY_ZOOM       ('z' + (kGamepadUp << 16))
#define CFGDEF_KEY_MAP        (kTab + (kGamepadBack << 16))
#define CFGDEF_KEY_180        (0)
#define CFGDEF_KEY_RELOAD     ('r' + (kGamepadX << 16))
#define CFGDEF_KEY_NEXTWEAPON (kMouseWheelUp + (kGamepadRightShoulder << 16))
#define CFGDEF_KEY_PREVWEAPON (kMouseWheelDown + (kGamepadLeftShoulder << 16))
#define CFGDEF_KEY_TALK       ('t')
#define CFGDEF_KEY_CONSOLE    (kTilde)
#define CFGDEF_KEY_ACTION1    ('o' + (kGamepadY << 16))
#define CFGDEF_KEY_ACTION2    ('p' + (kGamepadB << 16))
#define CFGDEF_KEY_PREVINV    ('[' + (kGamepadLeft << 16))
#define CFGDEF_KEY_NEXTINV    (']' + (kGamepadRight << 16))
#define CFGDEF_KEY_USEINV     (kEnter + (kGamepadDown << 16))

// Controls (Analogue)
#define CFGDEF_MOUSE_XAXIS (2 * kAxisTurn - 1)
#define CFGDEF_MOUSE_YAXIS (2 * kAxisMouselook - 1)

#define CFGDEF_JOY_XAXIS (2 * kAxisTurn - 1)
#define CFGDEF_JOY_YAXIS (2 * kAxisForward)

// Misc
#define CFGDEF_MENULANGUAGE (0)
#define CFGDEF_SHOWMESSAGES (1)

// Sound and Music
#define CFGDEF_SOUND_STEREO (1)  // Stereo
#define CFGDEF_MIX_CHANNELS (2)  // 32 channels

// Video Options
#define CFGDEF_USE_SMOOTHING (1)
#define CFGDEF_USE_DLIGHTS   (1)
#define CFGDEF_DETAIL_LEVEL  (2)
#define CFGDEF_HQ2X_SCALING  (3)
#define CFGDEF_SCREEN_HUD    (0)
#define CFGDEF_CROSSHAIR     (0)
#define CFGDEF_MAP_OVERLAY   (0)
#define CFGDEF_ROTATEMAP     (0)
#define CFGDEF_INVUL_FX      (1)  // TEXTURED
#define CFGDEF_WIPE_METHOD   (1)
#define CFGDEF_PNG_SCRSHOTS  (1)

// Gameplay Options
#define CFGDEF_AUTOAIM        (0)
#define CFGDEF_MLOOK          (1)
#define CFGDEF_JUMP           (1)
#define CFGDEF_CROUCH         (1)
#define CFGDEF_KICKING        (0)
#define CFGDEF_WEAPON_SWITCH  (1)
#define CFGDEF_MORE_BLOOD     (0)
#define CFGDEF_HAVE_EXTRA     (1)
#define CFGDEF_TRUE3DGAMEPLAY (1)
#define CFGDEF_PASS_MISSILE   (1)
#define CFGDEF_RES_RESPAWN    (1)  // Resurrect Mode
#define CFGDEF_ITEMRESPAWN    (0)
#define CFGDEF_FASTPARM       (0)
#define CFGDEF_RESPAWN        (0)

#define CFGDEF_AM_KEYDOORBLINK (1)
#define CFGDEF_AM_KEYDOORTEXT  (0)

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
