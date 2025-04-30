//------------------------------------------------------------------------
//  COAL General Stuff
//----------------------------------------------------------------------------
//
//  Copyright (c) 2006-2024 The EDGE Team.
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

#pragma once

#include <string>

// Detects COAL in a pwad or epk
bool GetCOALDetected();
void SetCOALDetected(bool detected);

void InitializeCOAL();
void ShutdownCOAL();

void COALAddScript(int type, std::string &data, const std::string &source);
void COALLoadScripts();

void COALRegisterHUD();
void COALRegisterPlaysim();

// HUD stuff
void COALNewGame(void);
void COALLoadGame(void);
void COALSaveGame(void);
void COALBeginLevel(void);
void COALEndLevel(void);
void COALRunHUD(void);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
