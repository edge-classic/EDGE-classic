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
#define EDGE_DEFAULT_SCREENWIDTH  (1000000) // Super high number to force scaling to native res
#define EDGE_DEFAULT_SCREENHEIGHT (1000000) // Super high number to force scaling to native res
#define EDGE_DEFAULT_SCREENBITS   (32)
#define EDGE_DEFAULT_DISPLAYMODE  (2)

// Controls (Key/Mouse Buttons)
#define EDGE_DEFAULT_KEY_FIRE        (kMouse1 + (kGamepadTriggerRight << 16))
#define EDGE_DEFAULT_KEY_SECONDATK   ('e' + (kGamepadTriggerLeft << 16))
#define EDGE_DEFAULT_KEY_USE         (kSpace + (kGamepadA << 16))
#define EDGE_DEFAULT_KEY_UP          (kUpArrow + ('w' << 16))
#define EDGE_DEFAULT_KEY_DOWN        (kDownArrow + ('s' << 16))
#define EDGE_DEFAULT_KEY_LEFT        (kLeftArrow)
#define EDGE_DEFAULT_KEY_RIGHT       (kRightArrow)
#define EDGE_DEFAULT_KEY_FLYUP       ('/' + (kGamepadLeftStick << 16))
#define EDGE_DEFAULT_KEY_FLYDOWN     ('c' + (kGamepadRightStick << 16))
#define EDGE_DEFAULT_KEY_SPEED       (kRightShift)
#define EDGE_DEFAULT_KEY_STRAFE      (kRightAlt + (kMouse3 << 16))
#define EDGE_DEFAULT_KEY_STRAFELEFT  (',' + ('a' << 16))
#define EDGE_DEFAULT_KEY_STRAFERIGHT ('.' + ('d' << 16))
#define EDGE_DEFAULT_KEY_AUTORUN     (kCapsLock)

#define EDGE_DEFAULT_KEY_LOOKUP     (kPageUp)
#define EDGE_DEFAULT_KEY_LOOKDOWN   (kPageDown)
#define EDGE_DEFAULT_KEY_LOOKCENTER (kHome)
#define EDGE_DEFAULT_KEY_MLOOK      ('m')
#define EDGE_DEFAULT_KEY_ZOOM       ('z' + (kGamepadUp << 16))
#define EDGE_DEFAULT_KEY_MAP        (kTab + (kGamepadBack << 16))
#define EDGE_DEFAULT_KEY_180        (0)
#define EDGE_DEFAULT_KEY_RELOAD     ('r' + (kGamepadX << 16))
#define EDGE_DEFAULT_KEY_NEXTWEAPON (kMouseWheelUp + (kGamepadRightShoulder << 16))
#define EDGE_DEFAULT_KEY_PREVWEAPON (kMouseWheelDown + (kGamepadLeftShoulder << 16))
#define EDGE_DEFAULT_KEY_TALK       ('t')
#define EDGE_DEFAULT_KEY_CONSOLE    (kTilde)
#define EDGE_DEFAULT_KEY_ACTION1    ('o' + (kGamepadY << 16))
#define EDGE_DEFAULT_KEY_ACTION2    ('p' + (kGamepadB << 16))
#define EDGE_DEFAULT_KEY_PREVINV    ('[' + (kGamepadLeft << 16))
#define EDGE_DEFAULT_KEY_NEXTINV    (']' + (kGamepadRight << 16))
#define EDGE_DEFAULT_KEY_USEINV     (kEnter + (kGamepadDown << 16))

// Controls (Analogue)
#define EDGE_DEFAULT_MOUSE_XAXIS (2 * kAxisTurn - 1)
#define EDGE_DEFAULT_MOUSE_YAXIS (2 * kAxisMouselook - 1)

#define EDGE_DEFAULT_JOY_XAXIS (2 * kAxisTurn - 1)
#define EDGE_DEFAULT_JOY_YAXIS (2 * kAxisForward)

// Misc
#define EDGE_DEFAULT_MENULANGUAGE (0)

// Sound and Music
#define EDGE_DEFAULT_SOUND_STEREO (1) // Stereo
#define EDGE_DEFAULT_MIX_CHANNELS (2) // 32 channels

// Video Options
#define EDGE_DEFAULT_USE_SMOOTHING  (0)
#define EDGE_DEFAULT_USE_MIPMAPPING (2)
#define EDGE_DEFAULT_USE_DLIGHTS    (1)
#define EDGE_DEFAULT_DETAIL_LEVEL   (2)
#define EDGE_DEFAULT_HQ2X_SCALING   (0)
#define EDGE_DEFAULT_SCREEN_HUD     (0)
#define EDGE_DEFAULT_CROSSHAIR      (0)
#define EDGE_DEFAULT_MAP_OVERLAY    (0)
#define EDGE_DEFAULT_ROTATEMAP      (0)
#define EDGE_DEFAULT_INVUL_FX       (1) // TEXTURED
#define EDGE_DEFAULT_WIPE_METHOD    (1)
#define EDGE_DEFAULT_PNG_SCRSHOTS   (1)

// Gameplay Options
#define EDGE_DEFAULT_AUTOAIM        (0)
#define EDGE_DEFAULT_MLOOK          (1)
#define EDGE_DEFAULT_JUMP           (1)
#define EDGE_DEFAULT_CROUCH         (1)
#define EDGE_DEFAULT_KICKING        (0)
#define EDGE_DEFAULT_WEAPON_SWITCH  (1)
#define EDGE_DEFAULT_MORE_BLOOD     (0)
#define EDGE_DEFAULT_HAVE_EXTRA     (1)
#define EDGE_DEFAULT_TRUE3DGAMEPLAY (1)
#define EDGE_DEFAULT_PASS_MISSILE   (1)
#define EDGE_DEFAULT_RES_RESPAWN    (1) // Resurrect Mode
#define EDGE_DEFAULT_ITEMRESPAWN    (0)
#define EDGE_DEFAULT_FASTPARM       (0)
#define EDGE_DEFAULT_RESPAWN        (0)

#define EDGE_DEFAULT_AM_KEYDOORBLINK (1)
#define EDGE_DEFAULT_AM_KEYDOORTEXT  (0)

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
