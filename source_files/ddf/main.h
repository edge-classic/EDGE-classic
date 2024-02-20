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

#pragma once

#define DEBUG_DDF 0

#include <string>
#include <vector>

#include "collection.h"
#include "file.h"
#include "types.h"

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
constexpr uint8_t kTicRate = 35;

// Misc playsim constants
constexpr float kCeilingSpeedDefault = 1.0f;
constexpr float kFloorSpeedDefault   = 1.0f;
constexpr float kGravityDefault      = 8.0f;
constexpr float kFrictionDefault     = 0.9063f;
constexpr float kViscosityDefault    = 0.0f;
constexpr float kDragDefault         = 0.99f;
constexpr float kRideFrictionDefault = 0.7f;

struct JumpActionInfo
{
    float chance = 1.0f;
};

class BecomeActionInfo
{
   public:
    const MapObjectDefinition *info_;
    std::string                info_ref_;

    LabelOffset start_;

   public:
    BecomeActionInfo();
    ~BecomeActionInfo();
};

class MorphActionInfo
{
   public:
    const MapObjectDefinition *info_;
    std::string                info_ref_;

    LabelOffset start_;

   public:
    MorphActionInfo();
    ~MorphActionInfo();
};

class WeaponBecomeActionInfo
{
   public:
    const WeaponDefinition *info_;
    std::string             info_ref_;

    LabelOffset start_;

   public:
    WeaponBecomeActionInfo();
    ~WeaponBecomeActionInfo();
};

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

bool        DDF_MainParseCondition(const char *str, ConditionCheck *cond);
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

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
