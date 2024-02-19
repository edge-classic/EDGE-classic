//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Main)
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

#ifndef __DDF_MAIN_H__
#define __DDF_MAIN_H__

#include "collection.h"
#include "epi.h"
#include "file.h"
#include "filesystem.h"
#include "types.h"

#define DEBUG_DDF 0

// Forward declarations
struct mobj_s;
struct sfx_s;

class AttackDefinition;
class Colormap;
class GameDefinition;
class MapDefinition;
class MapObjectDefinition;
class PlaylistEntry;
class WeaponDefinition;

#include "attack.h"
#include "flat.h"
#include "game.h"
#include "language.h"
#include "level.h"
#include "line.h"
#include "movie.h"
#include "playlist.h"
#include "sfx.h"
#include "states.h"
#include "thing.h"
#include "weapon.h"

// State updates, number of tics / second.
#define TICRATE 35

// Misc playsim constants
#define CEILSPEED  1.0f
#define FLOORSPEED 1.0f

#define GRAVITY       8.0f
#define FRICTION      0.9063f
#define VISCOSITY     0.0f
#define DRAG          0.99f
#define RIDE_FRICTION 0.7f

// Info for the JUMP action
typedef struct act_jump_info_s
{
    // chance value
    float chance;

   public:
    act_jump_info_s();
    ~act_jump_info_s();
} act_jump_info_t;

// Info for the BECOME action
typedef struct act_become_info_s
{
    const MapObjectDefinition *info;
    std::string       info_ref;

    LabelOffset start;

   public:
    act_become_info_s();
    ~act_become_info_s();
} act_become_info_t;

// Info for the MORPH action
typedef struct act_morph_info_s
{
    const MapObjectDefinition *info;
    std::string       info_ref;

    LabelOffset start;

   public:
    act_morph_info_s();
    ~act_morph_info_s();
} act_morph_info_t;

// Info for the weapon BECOME action
typedef struct wep_become_info_s
{
    const WeaponDefinition *info;
    std::string        info_ref;

    LabelOffset start;

   public:
    wep_become_info_s();
    ~wep_become_info_s();
} wep_become_info_t;

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

void DDF_Load(epi::File *f);

bool        DDF_MainParseCondition(const char *str, condition_check_t *cond);
void        DDF_MainGetWhenAppear(const char *info, void *storage);
void        DDF_MainGetRGB(const char *info, void *storage);
bool        DDF_MainDecodeBrackets(const char *info, char *outer, char *inner,
                                   int buf_len);
const char *DDF_MainDecodeList(const char *info, char divider, bool simple);
void        DDF_GetLumpNameForFile(const char *filename, char *lumpname);

int DDF_CompareName(const char *A, const char *B);

void DDF_MainAddDefine(const char *name, const char *value);
void DDF_MainAddDefine(const std::string &name, const std::string &value);
const char *DDF_MainGetDefine(const char *name);
void        DDF_MainFreeDefines();

bool DDF_WeaponIsUpgrade(WeaponDefinition *weap, WeaponDefinition *old);

bool        DDF_IsBoomLineType(int num);
bool        DDF_IsBoomSectorType(int num);
void        DDF_BoomClearGenTypes(void);
LineType   *DDF_BoomGetGenLine(int number);
SectorType *DDF_BoomGetGenSector(int number);

DDFType DDF_LumpToType(const std::string &name);
DDFType DDF_FilenameToType(const std::string &path);

void DDF_AddFile(DDFType type, std::string &data, const std::string &source);
void DDF_AddCollection(std::vector<DDFFile> &col, const std::string &source);
void DDF_ParseEverything();

void DDF_DumpFile(const std::string &data);
void DDF_DumpCollection(const std::vector<DDFFile> &col);

#endif /* __DDF_MAIN_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
