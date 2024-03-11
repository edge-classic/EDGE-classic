//----------------------------------------------------------------------------
//  EDGE Main Header
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

void E_Main(int argc, const char **argv);
void E_Idle(void);
void E_Tick(void);

void E_TitleTicker(void);
void E_PickLoadingScreen(void);
void E_AdvanceTitle(void);
void E_StartTitle(void);
void E_EngineShutdown(void);
void E_ForceWipe(void);
void E_Display(void);

// startup progress stuff

void E_ProgressMessage(const char *message);

enum ApplicationStateFlag
{
    kApplicationActive      = 0x1,
    kApplicationPendingQuit = 0x2
};

extern int app_state;

extern bool m_screenshot_required;
extern bool need_save_screenshot;

extern bool custom_MenuMain;
extern bool custom_MenuEpisode;
extern bool custom_MenuDifficulty;

extern ConsoleVariable r_titlescaling;

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
