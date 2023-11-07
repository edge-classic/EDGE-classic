//------------------------------------------------------------------------
//  COAL General Stuff
//----------------------------------------------------------------------------
//
//  Copyright (c) 2006-2023  The EDGE Team.
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

#ifndef __VM_COAL_H__
#define __VM_COAL_H__

void VM_InitCoal();
void VM_QuitCoal();

void VM_AddScript(int type, std::string &data, const std::string &source);
void VM_LoadScripts();

void VM_RegisterHUD();
void VM_RegisterPlaysim();

// HUD stuff
void VM_NewGame(void);
void VM_LoadGame(void);
void VM_SaveGame(void);
void VM_BeginLevel(void);
void VM_EndLevel(void);
void VM_RunHud(void);

#endif // __VM_COAL_H__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
