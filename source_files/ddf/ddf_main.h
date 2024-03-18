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

#define DDF_DEBUG 0

#include <string>
#include <vector>

#include "ddf_collection.h"
#include "ddf_types.h"
#include "epi_file.h"

// Forward declarations
class MapObject;
struct sfx_s;

class AttackDefinition;
class Colormap;
class GameDefinition;
class MapDefinition;
class MapObjectDefinition;
class PlaylistEntry;
class WeaponDefinition;

#include "ddf_attack.h"
#include "ddf_flat.h"
#include "ddf_game.h"
#include "ddf_language.h"
#include "ddf_level.h"
#include "ddf_line.h"
#include "ddf_movie.h"
#include "ddf_playlist.h"
#include "ddf_sfx.h"
#include "ddf_states.h"
#include "ddf_thing.h"
#include "ddf_weapon.h"

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

void DdfInit();
void DdfCleanUp();

bool        DdfMainParseCondition(const char *str, ConditionCheck *cond);
void        DdfMainGetWhenAppear(const char *info, void *storage);
void        DdfMainGetRGB(const char *info, void *storage);
bool        DdfMainDecodeBrackets(const char *info, char *outer, char *inner, int buf_len);
const char *DdfMainDecodeList(const char *info, char divider, bool simple);
void        DdfGetLumpNameForFile(const char *filename, char *lumpname);

int DdfCompareName(const char *A, const char *B);

void        DdfMainAddDefine(const char *name, const char *value);
void        DdfMainAddDefine(const std::string &name, const std::string &value);
const char *DdfMainGetDefine(const char *name);
void        DdfMainFreeDefines();

bool DdfWeaponIsUpgrade(WeaponDefinition *weap, WeaponDefinition *old);

bool        DdfIsBoomLineType(int num);
bool        DdfIsBoomSectorType(int num);
void        DdfBoomClearGeneralizedTypes(void);
LineType   *DdfBoomGetGeneralizedLine(int number);
SectorType *DdfBoomGetGeneralizedSector(int number);

DdfType DdfLumpToType(const std::string &name);
DdfType DdfFilenameToType(const std::string &path);

void DdfAddFile(DdfType type, std::string &data, const std::string &source);
void DdfAddCollection(std::vector<DdfFile> &col, const std::string &source);
void DdfParseEverything();

void DdfDumpFile(const std::string &data);
void DdfDumpCollection(const std::vector<DdfFile> &col);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
