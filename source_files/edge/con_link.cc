//----------------------------------------------------------------------------
//  LIST OF ALL CVARS
//----------------------------------------------------------------------------
//
//  Copyright (c) 2007-2009  The EDGE Team.
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

#include "i_defs.h"

#include "con_var.h"

#include "e_input.h"  // jaxis_group_c


extern cvar_c edge_compat;

extern cvar_c ddf_strict, ddf_lax, ddf_quiet;

extern cvar_c g_skill, g_gametype;
extern cvar_c g_mlook, g_autoaim;
extern cvar_c g_jumping, g_crouching;
extern cvar_c g_true3d, g_aggression;
extern cvar_c g_moreblood, g_noextra;
extern cvar_c g_fastmon, g_passmissile;
extern cvar_c g_weaponkick, g_weaponswitch;

extern cvar_c am_rotate, am_smoothing;
extern cvar_c am_gridsize;

extern cvar_c m_language;
extern cvar_c m_busywait, m_screenhud;
extern cvar_c m_messages, m_obituaries;
extern cvar_c m_goobers;

extern cvar_c sys_directx, sys_waveout;

extern cvar_c in_running, in_stageturn, in_shiftlook;
extern cvar_c in_keypad;
extern cvar_c in_grab;

extern cvar_c mouse_x_axis, mouse_y_axis;
extern cvar_c mouse_x_sens, mouse_y_sens;

extern cvar_c joy_dead, joy_peak, joy_tuning;

extern cvar_c r_width, r_height, r_depth, r_fullscreen;
extern cvar_c r_colormaterial, r_colorlighting;
extern cvar_c r_dumbsky, r_dumbmulti, r_dumbcombine, r_dumbclamp;
extern cvar_c r_nearclip, r_farclip, r_fadepower;
extern cvar_c r_fov, r_aspect;
extern cvar_c r_mipmapping, r_smoothing;
extern cvar_c r_dithering, r_hq2x;
extern cvar_c r_dynlight, r_invultex;
extern cvar_c r_gamma, r_detaillevel;
extern cvar_c r_wipemethod, r_wipereverse;
extern cvar_c r_teleportflash;
extern cvar_c r_crosshair, r_crosscolor;
extern cvar_c r_crosssize, r_crossbright;
extern cvar_c r_precache_tex, r_precache_sprite, r_precache_model;

extern cvar_c s_volume, s_mixchan, s_quietfactor;
extern cvar_c s_rate, s_bits, s_stereo;
extern cvar_c s_musicvol, s_musicdevice;
extern cvar_c tim_quietfactor;

extern cvar_c debug_fullbright, debug_hom;
extern cvar_c debug_mouse,      debug_joyaxis;
extern cvar_c debug_fps,        debug_pos;

extern cvar_c debug_nomonsters, debug_subsector;


#ifndef __linux__
#define S_MUSICDEV_CFG  "0"  // native
#else
#define S_MUSICDEV_CFG  "1"  // tinysoundfont
#endif


// Flag letters:
// =============
//
//   r : read only, user cannot change it
//   c : config file (saved and loaded)
//   h : cheat
//


cvar_link_t  all_cvars_OLD[] =
{
	/* General Stuff */

    { "language",       &m_language,     CVAR_ARCHIVE,   "ENGLISH" },

    { "ddf_strict",     &ddf_strict,     CVAR_ARCHIVE,   "0"  },
    { "ddf_lax",        &ddf_lax,        CVAR_ARCHIVE,   "0"  },
    { "ddf_quiet",      &ddf_quiet,      CVAR_ARCHIVE,   "0"  },

    { "aggression",     &g_aggression,   CVAR_ARCHIVE,   "0"  },

	/* Input Stuff */

    { "in_grab",        &in_grab,        CVAR_ARCHIVE,   "1"  },
	{ "in_keypad",      &in_keypad,      CVAR_ARCHIVE,   "1"  },
	{ "in_running",     &in_running,     CVAR_ARCHIVE,   "0"  },
	{ "in_stageturn",   &in_stageturn,   CVAR_ARCHIVE,   "1"  },

	{ "joy_dead",       &joy_dead,       CVAR_ARCHIVE,   "0.15" },
	{ "joy_peak",       &joy_peak,       CVAR_ARCHIVE,   "0.95" },
	{ "joy_tuning",     &joy_peak,       CVAR_ARCHIVE,   "1.0"  },

	{ "goobers",        &m_goobers,      0,    "0" },
	{ "m_busywait",     &m_busywait,     CVAR_ARCHIVE,   "1"  },

	/* Rendering Stuff */

	{ "r_aspect",       &r_aspect,       CVAR_ARCHIVE,   "1.777" },
	{ "r_fov",          &r_fov,          CVAR_ARCHIVE,   "90" },

	{ "r_crosshair",    &r_crosshair,    CVAR_ARCHIVE,   "0"  },
	{ "r_crosscolor",   &r_crosscolor,   CVAR_ARCHIVE,   "0"  },
	{ "r_crosssize",    &r_crosssize,    CVAR_ARCHIVE,   "16" },
	{ "r_crossbright",  &r_crossbright,  CVAR_ARCHIVE,   "1.0" },

	{ "r_nearclip",     &r_nearclip,     CVAR_ARCHIVE,   "4"  },
	{ "r_farclip",      &r_farclip,      CVAR_ARCHIVE,   "64000" },
	{ "r_fadepower",    &r_fadepower,    CVAR_ARCHIVE,   "1"  },

	{ "r_precache_tex",    &r_precache_tex,    CVAR_ARCHIVE, "1" },
	{ "r_precache_sprite", &r_precache_sprite, CVAR_ARCHIVE, "1" },
	{ "r_precache_model",  &r_precache_model,  CVAR_ARCHIVE, "1" },

	{ "r_colormaterial",&r_colormaterial, 0,   "1"  },
	{ "r_colorlighting",&r_colorlighting, 0,   "1"  },
	{ "r_dumbsky",      &r_dumbsky,       0,   "0"  },
	{ "r_dumbmulti",    &r_dumbmulti,     0,   "0"  },
	{ "r_dumbcombine",  &r_dumbcombine,   0,   "0"  },
	#ifdef APPLE_SILICON
		{ "r_dumbclamp",    &r_dumbclamp,     0,   "1"  },
	#else
		{ "r_dumbclamp",    &r_dumbclamp,     0,   "0"  },
	#endif
	{ "am_smoothing",   &am_smoothing,   CVAR_ARCHIVE,   "1"  },
	{ "am_gridsize",    &am_gridsize,    CVAR_ARCHIVE,   "128" },

	/* Sound Stuff */

	/* Debugging Stuff */

	{ "debug_fullbright", &debug_fullbright, CVAR_CHEAT, "0" },
	{ "debug_hom",        &debug_hom,        CVAR_CHEAT, "0" },
	{ "debug_joyaxis",    &debug_joyaxis,    0,  "0" },
	{ "debug_mouse",      &debug_mouse,      0,  "0" },
	{ "debug_pos",        &debug_pos,        CVAR_CHEAT, "0" },
	{ "debug_fps",        &debug_fps,        CVAR_ARCHIVE, "0" },

#if 0 // FIXME
    { "edge_compat",    &edge_compat,    0,    "0"  },

    { "sys_directx",    &sys_directx,    CVAR_ARCHIVE,   "0"  },
    { "sys_waveout",    &sys_waveout,    CVAR_ARCHIVE,   "0"  },

    { "g_skill",        &g_skill,        CVAR_ARCHIVE,   "3"  },
    { "g_gametype",     &g_gametype,     0,    "0"  },
    { "g_mlook",        &g_mlook,        CVAR_ARCHIVE,   "1"  },
    { "g_autoaim",      &g_autoaim,      CVAR_ARCHIVE,   "1"  },
    { "g_jumping",      &g_jumping,      CVAR_ARCHIVE,   "0"  },
    { "g_crouching",    &g_crouching,    CVAR_ARCHIVE,   "0"  },
    { "g_true3d",       &g_true3d,       CVAR_ARCHIVE,   "1"  },
    { "g_noextra",      &g_noextra,      CVAR_ARCHIVE,   "0"  },
    { "g_moreblood",    &g_moreblood,    CVAR_ARCHIVE,   "0"  },
    { "g_fastmon",      &g_fastmon,      CVAR_ARCHIVE,   "0"  },
    { "g_passmissile",  &g_passmissile,  CVAR_ARCHIVE,   "1"  },
    { "g_weaponkick",   &g_weaponkick,   CVAR_ARCHIVE,   "0"  },
    { "g_weaponswitch", &g_weaponswitch, CVAR_ARCHIVE,   "1"  },

	{ "am_rotate",      &am_rotate,      CVAR_ARCHIVE,   "0"  },

	{ "m_messages",     &m_messages,     CVAR_ARCHIVE,   "1"  },
	{ "m_obituaries",   &m_obituaries,   CVAR_ARCHIVE,   "1"  },
	{ "m_screenhud",    &m_screenhud,    CVAR_ARCHIVE,   "0"  },

	{ "r_width",        &r_width,        CVAR_ARCHIVE,   "640"   },
	{ "r_height",       &r_height,       CVAR_ARCHIVE,   "480"   },
    { "r_depth",        &r_depth,        CVAR_ARCHIVE,   "32"    },
    { "r_fullscreen",   &r_fullscreen,   CVAR_ARCHIVE,   "1"     },

	{ "r_gamma",        &r_gamma,        CVAR_ARCHIVE,   "1"  },

	{ "r_mipmapping",   &r_mipmapping,   CVAR_ARCHIVE,   "0"  },
	{ "r_smoothing",    &r_smoothing,    CVAR_ARCHIVE,   "0"  },
	{ "r_dithering",    &r_dithering,    CVAR_ARCHIVE,   "0"  },
	{ "r_hq2x",         &r_hq2x,         CVAR_ARCHIVE,   "0"  },

	{ "r_dynlight",     &r_dynlight,     CVAR_ARCHIVE,   "1"  },
	{ "r_detaillevel",  &r_detaillevel,  CVAR_ARCHIVE,   "1"  },
	{ "r_invultex",     &r_invultex,     CVAR_ARCHIVE,   "1"  },
	{ "r_wipemethod",   &r_wipemethod,   CVAR_ARCHIVE,   "1" /* Melt */ },
	{ "r_wipereverse",  &r_wipereverse,  CVAR_ARCHIVE,   "0"  },
	{ "r_teleportflash",&r_teleportflash,CVAR_ARCHIVE,   "1"  },

	{ "s_volume",       &s_volume,       CVAR_ARCHIVE,   "0.5"  },
	{ "s_mixchan",      &s_mixchan,      CVAR_ARCHIVE,   "32"   },
	{ "s_rate",         &s_rate,         CVAR_ARCHIVE,   "22050" },
	{ "s_bits",         &s_bits,         CVAR_ARCHIVE,   "16" },
	{ "s_stereo",       &s_stereo,       CVAR_ARCHIVE,   "1"  },
	{ "s_musicvol",     &s_musicvol,     CVAR_ARCHIVE,   "0.5"  },
	{ "s_musicdevice",  &s_musicdevice,  CVAR_ARCHIVE,   S_MUSICDEV_CFG },

	{ "s_quietfactor",  &s_quietfactor,  CVAR_ARCHIVE,   "1"  },
	{ "tim_quietfactor",&tim_quietfactor,CVAR_ARCHIVE,   "1"  },

	{ "in_shiftlook",   &in_shiftlook,   CVAR_ARCHIVE,   "1"  },

	{ "mouse_x.axis",   &mouse_x_axis,   CVAR_ARCHIVE,   "1" /* AXIS_TURN */  },
	{ "mouse_x.sens",   &mouse_x_sens,   CVAR_ARCHIVE,   "10"  },
	{ "mouse_y.axis",   &mouse_y_axis,   CVAR_ARCHIVE,   "4" /* AXIS_MLOOK */ },
	{ "mouse_y.sens",   &mouse_y_sens,   CVAR_ARCHIVE,   "10" },

	{ "debug_nomonsters", &debug_nomonsters, CVAR_CHEAT, "0" },
	{ "debug_subsector",  &debug_subsector,  CVAR_CHEAT, "0" },
#endif

//---- END OF LIST -----------------------------------------------------------

	{ NULL, NULL, 0, NULL }
};

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
