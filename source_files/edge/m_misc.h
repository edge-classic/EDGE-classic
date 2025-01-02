//----------------------------------------------------------------------------
//  EDGE Misc: Screenshots, Menu and defaults Code
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
//
// 1998/07/02 -MH- Added key_fly_up and key_fly_down
//

#pragma once

#include "con_var.h"

//
// MISC
//
enum ConfigurationValueType
{
    kConfigInteger = 0,
    kConfigEnum    = 0,
    kConfigBoolean = 1,
    kConfigKey     = 2,
};

struct ConfigurationDefault
{
    int         type;
    const char *name;
    void       *location;
    int         default_value;
};

void ResetDefaults(int dummy, ConsoleVariable *dummy_cvar = nullptr);
void LoadDefaults(void);
void LoadBranding(void);
void SaveDefaults(void);

void TakeScreenshot(bool show_msg);
void CreateSaveScreenshot(void);

#ifdef __GNUC__
void WarningOrError(const char *error, ...) __attribute__((format(printf, 1, 2)));
void DebugOrError(const char *error, ...) __attribute__((format(printf, 1, 2)));
#else
void WarningOrError(const char *error, ...);
void DebugOrError(const char *error, ...);
#endif

extern bool show_obituaries;

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
