//----------------------------------------------------------------------------
//  EDGE Default Settings
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2023  The EDGE Team.
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

#ifndef __DEFAULT_SETTINGS__
#define __DEFAULT_SETTINGS__

// Screen resolution
#define CFGDEF_SCREENWIDTH      (1000000)  // Super high number to force scaling to native res
#define CFGDEF_SCREENHEIGHT     (1000000) // Super high number to force scaling to native res
#define CFGDEF_SCREENBITS       (32)
#define CFGDEF_DISPLAYMODE       (2)

// Controls (Key/Mouse Buttons)
#define CFGDEF_KEY_FIRE         (KEYD_RCTRL + (KEYD_MOUSE1 << 16))
#define CFGDEF_KEY_SECONDATK    ('e')
#define CFGDEF_KEY_USE          (KEYD_SPACE)
#define CFGDEF_KEY_UP           (KEYD_UPARROW +   ('w' << 16))
#define CFGDEF_KEY_DOWN         (KEYD_DOWNARROW + ('s' << 16))
#define CFGDEF_KEY_LEFT         (KEYD_LEFTARROW)
#define CFGDEF_KEY_RIGHT        (KEYD_RIGHTARROW)
#define CFGDEF_KEY_FLYUP        (KEYD_INSERT + ('/' << 16))
#define CFGDEF_KEY_FLYDOWN      (KEYD_DELETE + ('c' << 16))
#define CFGDEF_KEY_SPEED        (KEYD_RSHIFT)
#define CFGDEF_KEY_STRAFE       (KEYD_RALT + (KEYD_MOUSE3 << 16))
#define CFGDEF_KEY_STRAFELEFT   (',' + ('a' << 16))
#define CFGDEF_KEY_STRAFERIGHT  ('.' + ('d' << 16))
#define CFGDEF_KEY_AUTORUN      (KEYD_CAPSLOCK)

#define CFGDEF_KEY_LOOKUP       (KEYD_PGUP)
#define CFGDEF_KEY_LOOKDOWN     (KEYD_PGDN)
#define CFGDEF_KEY_LOOKCENTER   (KEYD_HOME)
#define CFGDEF_KEY_MLOOK        ('m')
#define CFGDEF_KEY_ZOOM         ('z' + ('\\' << 16))
#define CFGDEF_KEY_MAP          (KEYD_TAB)
#define CFGDEF_KEY_180          (0)
#define CFGDEF_KEY_RELOAD       ('r')
#define CFGDEF_KEY_NEXTWEAPON   (KEYD_WHEEL_UP)
#define CFGDEF_KEY_PREVWEAPON   (KEYD_WHEEL_DN)
#define CFGDEF_KEY_TALK         ('t')
#define CFGDEF_KEY_CONSOLE      (KEYD_TILDE)
#define CFGDEF_KEY_ACTION1      ('[')
#define CFGDEF_KEY_ACTION2      (']')

// Controls (Analogue)
#define CFGDEF_MOUSE_XAXIS      (2*AXIS_TURN-1)
#define CFGDEF_MOUSE_YAXIS      (2*AXIS_MLOOK-1)

#define CFGDEF_JOY_XAXIS        (2*AXIS_TURN-1)
#define CFGDEF_JOY_YAXIS        (2*AXIS_FORWARD)

// Misc
#define CFGDEF_MENULANGUAGE     (0)
#define CFGDEF_SHOWMESSAGES     (1)

// Sound and Music
#define CFGDEF_SOUND_STEREO     (1)  // Stereo
#define CFGDEF_MIX_CHANNELS     (2)  // 32 channels

// Video Options
#define CFGDEF_USE_SMOOTHING    (1)
#define CFGDEF_USE_DLIGHTS      (1)
#define CFGDEF_DETAIL_LEVEL     (2)
#define CFGDEF_USE_MIPMAPPING   (2)
#define CFGDEF_HQ2X_SCALING     (3)
#define CFGDEF_SCREEN_HUD       (0)
#define CFGDEF_CROSSHAIR        (0)
#define CFGDEF_MAP_OVERLAY      (0)
#define CFGDEF_ROTATEMAP        (0)
#define CFGDEF_INVUL_FX         (1)  // TEXTURED
#define CFGDEF_WIPE_METHOD      (1)
#define CFGDEF_PNG_SCRSHOTS     (1)

// Gameplay Options
#define CFGDEF_AUTOAIM          (0)
#define CFGDEF_MLOOK            (1)
#define CFGDEF_JUMP             (1)
#define CFGDEF_CROUCH           (1)
#define CFGDEF_KICKING          (0)
#define CFGDEF_WEAPON_SWITCH    (1)
#define CFGDEF_MORE_BLOOD       (0)
#define CFGDEF_HAVE_EXTRA       (1)
#define CFGDEF_TRUE3DGAMEPLAY   (1)
#define CFGDEF_PASS_MISSILE     (1)
#define CFGDEF_RES_RESPAWN      (1)       // Resurrect Mode 
#define CFGDEF_ITEMRESPAWN      (0)
#define CFGDEF_FASTPARM         (0)
#define CFGDEF_RESPAWN          (0)

#define CFGDEF_AM_KEYDOORBLINK        (1)
#define CFGDEF_AM_KEYDOORTEXT        (0)

#endif /* __CFGDEF_SETTINGS__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
