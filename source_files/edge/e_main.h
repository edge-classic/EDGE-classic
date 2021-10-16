//----------------------------------------------------------------------------
//  EDGE Main Header
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

#ifndef __E_MAIN__
#define __E_MAIN__

#include "e_event.h"

void E_Main(int argc, const char **argv);
void E_Idle(void);
void E_Tick(void);

void E_TitleTicker(void);
void E_AdvanceTitle(void);
void E_StartTitle(void);
void E_EngineShutdown(void);
void E_ForceWipe(void);
void E_Display(void);

// startup progress stuff

void E_ProgressMessage(const char *message);
void E_LocalProgress(int step, int total);

void E_NodeMessage(const char *message);
void E_NodeProgress(int perc);

typedef enum
{
	APP_STATE_ACTIVE       = 0x1,
	APP_STATE_PENDING_QUIT = 0x2
}
app_state_flags_e;

extern int app_state;

extern bool m_screenshot_required;
extern bool need_save_screenshot;

#endif // __E_MAIN__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
