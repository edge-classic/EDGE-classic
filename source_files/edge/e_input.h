//----------------------------------------------------------------------------
//  EDGE Input handling
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

#include "con_var.h"
#include "e_event.h"
#include "e_ticcmd.h"

void EventClearInput(void);
void EventBuildTicCommand(EventTicCommand *cmd);
void EventReleaseAllKeys(void);
void EventSetTurboScale(int scale);
void EventUpdateKeyState(void);

void EventProcessEvents(void);
void EventPostEvent(InputEvent *ev);

bool EventIsKeyPressed(int keyvar);
bool EventMatchesKey(int keyvar, int key);

const char *EventGetKeyName(int key);

bool EventInputResponderResponder(InputEvent *ev);

// -KM- 1998/09/01 Analogue binding stuff, These hold what axis they bind to.
extern int mouse_x_axis;
extern int mouse_y_axis;

extern ConsoleVariable mouse_x_sensitivity;
extern ConsoleVariable mouse_y_sensitivity;

extern int    joystick_axis[4];
extern float *joystick_deadzones[6];
//
// -ACB- 1998/09/06 Analogue binding:
//                   Two stage turning, angle_turn control
//                   horzmovement control, vertmovement control
//                   strafemovediv;
//
extern ConsoleVariable turn_speed;
extern ConsoleVariable vertical_look_speed;
extern ConsoleVariable forward_speed;
extern ConsoleVariable side_speed;
extern ConsoleVariable fly_speed;

/* keyboard stuff */

extern int key_right;
extern int key_left;
extern int key_look_up;
extern int key_look_down;
extern int key_look_center;

// -ES- 1999/03/28 Zoom Key
extern int key_zoom;
extern int key_up;
extern int key_down;

extern int key_strafe_left;
extern int key_strafe_right;

// -ACB- for -MH- 1998/07/19 Flying keys
extern int key_fly_up;
extern int key_fly_down;

extern int key_fire;
extern int key_use;
extern int key_strafe;
extern int key_speed;
extern int key_autorun;
extern int key_next_weapon;
extern int key_previous_weapon;
extern int key_map;
extern int key_180;
extern int key_talk;
extern int key_console;
extern int key_pause;

extern int key_mouselook; // -AJA- 1999/07/27.
extern int key_second_attack;
extern int key_third_attack;
extern int key_fourth_attack;
extern int key_reload;  // -AJA- 2004/11/10
extern int key_action1; // -AJA- 2009/09/07
extern int key_action2; // -AJA- 2009/09/07

// -AJA- 2010/06/13: weapon and automap stuff
extern int key_weapons[10];

extern int key_automap_up;
extern int key_automap_down;
extern int key_automap_left;
extern int key_automap_right;

extern int key_automap_zoom_in;
extern int key_automap_zoom_out;

extern int key_automap_follow;
extern int key_automap_grid;
extern int key_automap_mark;
extern int key_automap_clear;

// Dasho: Inventory handling
extern int key_inventory_previous;
extern int key_inventory_use;
extern int key_inventory_next;

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
