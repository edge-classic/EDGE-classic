//----------------------------------------------------------------------------
//  EDGE Main Menu Code
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
//
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------

#ifndef __M_MENU__
#define __M_MENU__

#include "e_event.h"

struct sfx_s;
  
// Menu navigation stuff
extern int key_menu_open;
extern int key_menu_up;
extern int key_menu_down;
extern int key_menu_left;
extern int key_menu_right;
extern int key_menu_select;
extern int key_menu_cancel;

// Program stuff
extern int key_screenshot;
extern int key_save_game;
extern int key_load_game;
extern int key_sound_controls;
extern int key_options_menu;
extern int key_quick_save;
extern int key_end_game;
extern int key_message_toggle;
extern int key_quick_load;
extern int key_quit_edge;
extern int key_gamma_toggle;

// the so-called "bastard sfx" used for the menus
extern struct sfx_s * sfx_swtchn;
extern struct sfx_s * sfx_tink;
extern struct sfx_s * sfx_radio;
extern struct sfx_s * sfx_oof;
extern struct sfx_s * sfx_pstop;
extern struct sfx_s * sfx_stnmov;
extern struct sfx_s * sfx_pistol;
extern struct sfx_s * sfx_swtchx;

//
// MENUS
//
// Called by main loop,
// saves config file and calls I_Quit when user exits.
// Even when the menu is not displayed,
// this can resize the view and change game parameters.
// Does all the real work of the menu interaction.
bool M_Responder(event_t * ev);

// Called by main loop,
// only used for menu (skull cursor) animation.
void M_Ticker(void);

// Called by main loop,
// draws the menus directly into the screen buffer.
void M_Drawer(void);

// Called by D_DoomMain,
// loads the config file.
void M_Init(void);

// Called by intro code to force menu up upon a keypress,
// does nothing if menu is already up.
void M_StartControlPanel(void);

// 25-6-98 KM
void M_StartMessage(const char *string, void (* routine)(int response), 
    bool input);

// -KM- 1998/07/21
// String will be printed as a prompt.
// Routine should be void Foobar(char *string)
// and will be called with the input returned
// or NULL if user pressed escape.

void M_StartMessageInput(const char *string, 
    void (* routine)(const char *response));

void M_EndGame(int choice, cvar_c *cvar = nullptr);
void M_QuitEDGE(int choice);
void M_ImmediateQuit(void);
void M_DrawThermo(int x, int y, int thermWidth, int thermDot, int div);
void M_DrawFracThermo(int x, int y, float thermDot, float increment, int div,
    float min, float max, std::string fmt_string);
void M_ClearMenus(void);

#endif // __M_MENU__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
