//----------------------------------------------------------------------------
//  EDGE Default Settings
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2009  The EDGE Team.
// 
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
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
#define CFGDEF_SCREENWIDTH      (640)
#define CFGDEF_SCREENHEIGHT     (480)
#define CFGDEF_SCREENBITS       (32)
#define CFGDEF_FULLSCREEN       (1)

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
#define CFGDEF_MOUSESENSITIVITY (10)
#define CFGDEF_TURNSPEED        (7)   // == 1.0 (the maximum)
#define CFGDEF_MLOOKSPEED       (7)
#define CFGDEF_FORWARDMOVESPEED (7)
#define CFGDEF_SIDEMOVESPEED    (7)

#define CFGDEF_JOY_XAXIS        (2*AXIS_TURN-1)
#define CFGDEF_JOY_YAXIS        (2*AXIS_FORWARD)

// Misc
#define CFGDEF_MENULANGUAGE     (0)
#define CFGDEF_SHOWMESSAGES     (1)

// Sound and Music
#define CFGDEF_SOUND_VOLUME     (8)
#define CFGDEF_MUSIC_VOLUME     (8)
#define CFGDEF_SAMPLE_RATE      (1)  // 22050Hz
#define CFGDEF_SOUND_BITS       (1)  // 16-bit
#define CFGDEF_SOUND_STEREO     (1)  // Stereo
#define CFGDEF_MIX_CHANNELS     (2)  // 32 channels
#define CFGDEF_QUIET_FACTOR     (1)

#ifdef LINUX
#define CFGDEF_MUSIC_DEVICE     (1)  // Timidity
#else
#define CFGDEF_MUSIC_DEVICE     (0)  // System
#endif

// Video Options
#define CFGDEF_CURRENT_GAMMA    (2)
#define CFGDEF_USE_SMOOTHING    (0)
#define CFGDEF_USE_DLIGHTS      (0)
#define CFGDEF_DOOM_FADING      (1)
#define CFGDEF_DETAIL_LEVEL     (1)
#define CFGDEF_USE_MIPMAPPING   (0)
#define CFGDEF_HQ2X_SCALING     (0)
#define CFGDEF_SCREEN_HUD       (0)
#define CFGDEF_SHADOWS          (0)
#define CFGDEF_CROSSHAIR        (0)
#define CFGDEF_MAP_OVERLAY      (0)
#define CFGDEF_ROTATEMAP        (0)
#define CFGDEF_INVUL_FX         (2)  // TEXTURED
#define CFGDEF_TELEPT_FLASH     (1)
#define CFGDEF_WIPE_METHOD      (1)
#define CFGDEF_PNG_SCRSHOTS     (1)
#define CFGDEF_TELEPT_EFFECT    (0)
#define CFGDEF_TELEPT_REVERSE   (0)
#define CFGDEF_WIPE_REVERSE     (0)

// Gameplay Options
#define CFGDEF_AUTOAIM          (1)
#define CFGDEF_MLOOK            (1)
#define CFGDEF_JUMP             (1)
#define CFGDEF_CROUCH           (1)
#define CFGDEF_KICKING          (0)
#define CFGDEF_WEAPON_SWITCH    (1)
#define CFGDEF_MORE_BLOOD       (0)
#define CFGDEF_HAVE_EXTRA       (0)
#define CFGDEF_TRUE3DGAMEPLAY   (1)
#define CFGDEF_PASS_MISSILE     (1)
#define CFGDEF_MENU_GRAV        (MENU_GRAV_NORMAL)
#define CFGDEF_RES_RESPAWN      (1)       // Resurrect Mode 
#define CFGDEF_ITEMRESPAWN      (0)
#define CFGDEF_FASTPARM         (0)
#define CFGDEF_RESPAWN          (0)

#endif /* __CFGDEF_SETTINGS__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
