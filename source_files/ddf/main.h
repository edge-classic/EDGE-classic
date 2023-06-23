//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Main)
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2008  The EDGE Team.
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

#ifndef __DDF_MAIN_H__
#define __DDF_MAIN_H__

#include <filesystem>

#include "epi.h"
#include "file.h"
#include "arrays.h"

#include "types.h"

#define DEBUG_DDF  0

// Forward declarations
struct mobj_s;
struct sfx_s;

class atkdef_c;
class colourmap_c;
class gamedef_c;
class mapdef_c;
class mobjtype_c;
class pl_entry_c;
class weapondef_c;


#include "thing.h"
#include "attack.h"
#include "states.h"
#include "weapon.h"

#include "line.h"
#include "level.h"
#include "game.h"

#include "playlist.h"
#include "sfx.h"
#include "language.h"

#include "flat.h"

// State updates, number of tics / second.
#define TICRATE   35

// Misc playsim constants
#define CEILSPEED   		1.0f
#define FLOORSPEED  		1.0f

#define GRAVITY     		8.0f
#define FRICTION    		0.9063f
#define VISCOSITY   		0.0f
#define DRAG        		0.99f
#define RIDE_FRICTION    	0.7f


// Info for the JUMP action
typedef struct act_jump_info_s
{
	// chance value
	percent_t chance; 

public:
	 act_jump_info_s();
	~act_jump_info_s();
}
act_jump_info_t;


// Info for the BECOME action
typedef struct act_become_info_s
{
	const mobjtype_c *info;
	std::string info_ref;

	label_offset_c start;

public:
	 act_become_info_s();
	~act_become_info_s();
}
act_become_info_t;

// Info for the MORPH action
typedef struct act_morph_info_s
{
	const mobjtype_c *info;
	std::string info_ref;

	label_offset_c start;

public:
	 act_morph_info_s();
	~act_morph_info_s();
}
act_morph_info_t;

// Info for the weapon BECOME action
typedef struct wep_become_info_s
{
	const weapondef_c *info;
	std::string info_ref;

	label_offset_c start;

public:
	 wep_become_info_s();
	~wep_become_info_s();
}
wep_become_info_t;


// ------------------------------------------------------------------
// -------------------------EXTERNALISATIONS-------------------------
// ------------------------------------------------------------------

// if true, prefer to crash out on various errors
extern bool strict_errors;

// if true, prefer to ignore or fudge various (serious) errors
extern bool lax_errors;

// if true, disable warning messages
extern bool no_warnings;

void DDF_Init();
void DDF_CleanUp();

void DDF_Load(epi::file_c *f);

bool DDF_MainParseCondition(const char *str, condition_check_t *cond);
void DDF_MainGetWhenAppear(const char *info, void *storage);
void DDF_MainGetRGB(const char *info, void *storage);
bool DDF_MainDecodeBrackets(const char *info, char *outer, char *inner, int buf_len);
const char *DDF_MainDecodeList(const char *info, char divider, bool simple);
void DDF_GetLumpNameForFile(const char *filename, char *lumpname);

int DDF_CompareName(const char *A, const char *B);

void DDF_MainAddDefine(const char *name, const char *value);
void DDF_MainAddDefine(const std::string& name, const std::string& value);
const char *DDF_MainGetDefine(const char *name);
void DDF_MainFreeDefines();

bool DDF_WeaponIsUpgrade(weapondef_c *weap, weapondef_c *old);

bool DDF_IsBoomLineType(int num);
bool DDF_IsBoomSectorType(int num);
void DDF_BoomClearGenTypes(void);
linetype_c *DDF_BoomGetGenLine(int number);
sectortype_c *DDF_BoomGetGenSector(int number);

ddf_type_e DDF_LumpToType(const std::string& name);
ddf_type_e DDF_FilenameToType(const std::filesystem::path& path);

void DDF_AddFile(ddf_type_e type, std::string& data, const std::string& source);
void DDF_AddCollection(ddf_collection_c *col, const std::string& source);
void DDF_ParseEverything();

void DDF_DumpFile(const std::string& data);
void DDF_DumpCollection(ddf_collection_c *col);

#endif /* __DDF_MAIN_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
