//----------------------------------------------------------------------------
//  EDGE Main Menu Code
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

#include <string>

#include "con_var.h"
#include "e_event.h"

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
extern struct SoundEffect *sound_effect_swtchn;
extern struct SoundEffect *sound_effect_tink;
extern struct SoundEffect *sound_effect_radio;
extern struct SoundEffect *sound_effect_oof;
extern struct SoundEffect *sound_effect_pstop;
extern struct SoundEffect *sound_effect_stnmov;
extern struct SoundEffect *sound_effect_pistol;
extern struct SoundEffect *sound_effect_swtchx;

//
// MENUS
//
// Called by main loop,
// saves config file and calls I_Quit when user exits.
// Even when the menu is not displayed,
// this can resize the view and change game parameters.
// Does all the real work of the menu interaction.
bool MenuResponder(InputEvent *ev);

// Called by main loop,
// only used for menu (skull cursor) animation.
void MenuTicker(void);

// Called by main loop,
// draws the menus directly into the screen buffer.
void MenuDrawer(void);

// Called by D_DoomMain,
// loads the config file.
void MenuInitialize(void);

// Called by intro code to force menu up upon a keypress,
// does nothing if menu is already up.
void StartControlPanel(void);

// 25-6-98 KM
void StartMenuMessage(const char *string, void (*routine)(int response), bool input);

// -KM- 1998/07/21
// String will be printed as a prompt.
// Routine should be void Foobar(char *string)
// and will be called with the input returned
// or nullptr if user pressed escape.

void StartMenuMessageInput(const char *string, void (*routine)(const char *response));

void MenuEndGame(int choice, ConsoleVariable *cvar = nullptr);
void QuitEdge(int choice);
void ImmediateQuit(void);
void DrawMenuSlider(int x, int y, float slider_position, float increment, int div, float min, float max,
                    const std::string &format_string);
void MenuClear(void);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
