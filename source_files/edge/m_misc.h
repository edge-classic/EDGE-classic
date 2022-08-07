//----------------------------------------------------------------------------
//  EDGE Misc: Screenshots, Menu and defaults Code
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
//
// 1998/07/02 -MH- Added key_flyup and key_flydown
//

#ifndef __M_MISC__
#define __M_MISC__


#include "file.h"
#include "utility.h"

//
// MISC
//
typedef enum
{
	CFGT_Int = 0,
	CFGT_Boolean = 1,
	CFGT_Key = 2,
}
cfg_type_e;

#define CFGT_Enum  CFGT_Int

typedef struct
{
	int type;
	const char *name;
	void *location;
	int defaultvalue;
}
default_t;

void M_ResetDefaults(int _dummy);
void M_LoadDefaults(void);
void M_SaveDefaults(void);

void M_InitMiscConVars(void);
void M_ScreenShot(bool show_msg);
void M_MakeSaveScreenShot(void);

byte *M_GetFileData(const char *filename, int *length);
std::string M_ComposeFileName(const char *dir, const char *file);
epi::file_c *M_OpenComposedEPIFile(const char *dir, const char *file);
void M_WarnError(const char *error,...) GCCATTR((format(printf, 1, 2)));
void M_DebugError(const char *error,...) GCCATTR((format(printf, 1, 2)));

extern unsigned short save_screenshot[160][100];
extern bool save_screenshot_valid;

extern int  display_desync;
extern bool force_directx;
extern bool force_waveout;

extern bool var_obituaries;

extern bool var_pc_speaker_mode;
extern int var_sound_stereo;
extern int var_mix_channels;
extern int var_quiet_factor;

extern bool var_cache_sfx;

#endif  /* __M_MISC__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
