//----------------------------------------------------------------------------
//  EDGE Console Main
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

#ifndef __CON_MAIN_GUI_H
#define __CON_MAIN_GUI_H


// The console 'application' functions.
// Adds the console to the gui.
void CON_Start(void);

// Ticker.  Animates the opening/closing effect of the console
void CON_Ticker(void);

// Responder obeys events.
bool CON_Responder(event_t * ev);

// Drawer. Draws the console.
void CON_Drawer(void);

void CON_ShowFPS(void);

// Initialises the console
void CON_InitConsole(void);

#endif /* __CON_MAIN_GUI_H */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
