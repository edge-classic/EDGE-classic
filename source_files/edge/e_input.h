//----------------------------------------------------------------------------
//  EDGE Input handling
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
//
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------

#ifndef __E_INPUT_H__
#define __E_INPUT_H__

#include "e_event.h"
#include "e_ticcmd.h"

void E_ClearInput(void);
void E_BuildTiccmd(ticcmd_t * cmd);
void E_ReleaseAllKeys(void);
void E_SetTurboScale(int scale);
void E_UpdateKeyState(void);

void E_ProcessEvents(void);
void E_PostEvent(event_t * ev);

bool E_IsKeyPressed(int keyvar);
bool E_MatchesKey(int keyvar, int key);

const char *E_GetKeyName(int key);

bool INP_Responder(event_t * ev);

// -KM- 1998/09/01 Analogue binding stuff, These hold what axis they bind to.
extern int mouse_xaxis;
extern int mouse_yaxis;

extern int mouse_xsens;
extern int mouse_ysens;

extern int joy_axis[6];
//
// -ACB- 1998/09/06 Analogue binding:
//                   Two stage turning, angleturn control
//                   horzmovement control, vertmovement control
//                   strafemovediv;
//
extern int var_turnspeed;
extern int var_mlookspeed;
extern int var_forwardspeed;
extern int var_sidespeed;
extern int var_flyspeed;


/* keyboard stuff */

extern int key_right;
extern int key_left;
extern int key_lookup;
extern int key_lookdown;
extern int key_lookcenter;

// -ES- 1999/03/28 Zoom Key
extern int key_zoom;
extern int key_up;
extern int key_down;

extern int key_strafeleft;
extern int key_straferight;

// -ACB- for -MH- 1998/07/19 Flying keys
extern int key_flyup;
extern int key_flydown;

extern int key_fire;
extern int key_use;
extern int key_strafe;
extern int key_speed;
extern int key_autorun;
extern int key_nextweapon;
extern int key_prevweapon;
extern int key_map;
extern int key_180;
extern int key_talk;
extern int key_console;
extern int key_pause;

extern int key_mlook;  // -AJA- 1999/07/27.
extern int key_secondatk;
extern int key_reload;  // -AJA- 2004/11/10
extern int key_action1; // -AJA- 2009/09/07
extern int key_action2; // -AJA- 2009/09/07

// -AJA- 2010/06/13: weapon and automap stuff
extern int key_weapons[10];

extern int key_am_up;
extern int key_am_down;
extern int key_am_left;
extern int key_am_right;

extern int key_am_zoomin;
extern int key_am_zoomout;

extern int key_am_follow;
extern int key_am_grid;
extern int key_am_mark;
extern int key_am_clear;

#endif  /* __E_INPUT_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
