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
// 1998/07/02 -MH- Added key_flyup and key_flydown
//

#ifndef __M_MISC__
#define __M_MISC__

#include "con_var.h"
#include "file.h"

//
// MISC
//
typedef enum
{
    CFGT_Int     = 0,
    CFGT_Boolean = 1,
    CFGT_Key     = 2,
} cfg_type_e;

#define CFGT_Enum CFGT_Int

typedef struct
{
    int         type;
    const char *name;
    void       *location;
    int         defaultvalue;
} default_t;

void M_ResetDefaults(int _dummy, ConsoleVariable *_dummy_cvar = nullptr);
void M_LoadDefaults(void);
void M_LoadBranding(void);
void M_SaveDefaults(void);

void M_InitMiscConVars(void);
void M_ScreenShot(bool show_msg);
void M_MakeSaveScreenShot(void);

std::string M_ComposeFileName(std::string dir, std::string file);
epi::File          *M_OpenComposedEPIFile(std::string dir, std::string file);
#ifdef __GNUC__
void                  M_WarnError(const char *error, ...) __attribute__((format(printf, 1, 2)));
void                  M_DebugError(const char *error, ...) __attribute__((format(printf, 1, 2)));
#else
void                  M_WarnError(const char *error, ...);
void                  M_DebugError(const char *error, ...);
#endif

extern bool save_screenshot_valid;

extern int display_desync;

extern bool var_obituaries;

extern int var_midi_player;
extern int var_sound_stereo;
extern int var_mix_channels;

extern bool var_cache_sfx;

#endif /* __M_MISC__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
