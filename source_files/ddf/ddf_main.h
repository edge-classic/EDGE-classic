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
#include "ddf_reverb.h"
#include "ddf_sfx.h"
#include "ddf_states.h"
#include "ddf_thing.h"
#include "ddf_weapon.h"
#include "epi_str_hash.h"

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
    float chance  = 1.0f;
    int   amount  = 0;
    int   amount2 = 0;
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

void DDFInit();
void DDFCleanUp();

bool        DDFMainParseCondition(const char *str, ConditionCheck *cond);
void        DDFMainGetWhenAppear(const char *info, void *storage);
void        DDFMainGetRGB(const char *info, void *storage);
bool        DDFMainDecodeBrackets(const char *info, char *outer, char *inner, int buf_len);
const char *DDFMainDecodeList(const char *info, char divider, bool simple);
void        DDFGetLumpNameForFile(const char *filename, char *lumpname);

int DDFCompareName(const char *A, const char *B);

void        DDFMainAddDefine(const char *name, const char *value);
void        DDFMainAddDefine(const std::string &name, const std::string &value);
const char *DDFMainGetDefine(const char *name);
void        DDFMainFreeDefines();

bool DDFWeaponIsUpgrade(WeaponDefinition *weap, WeaponDefinition *old);

bool        DDFIsBoomLineType(int num);
bool        DDFIsBoomSectorType(int num);
void        DDFBoomClearGeneralizedTypes(void);
LineType   *DDFBoomGetGeneralizedLine(int number);
SectorType *DDFBoomGetGeneralizedSector(int number);

DDFType DDFLumpToType(const std::string &name);
DDFType DDFFilenameToType(const std::string &path);

void DDFAddFile(DDFType type, std::string &data, const std::string &source);
void DDFAddCollection(std::vector<DDFFile> &col, const std::string &source);
void DDFParseEverything();

void DDFDumpFile(const std::string &data);
void DDFDumpCollection(const std::vector<DDFFile> &col);

// Strip spaces and underscores before producing a StringHash
// from a DDF name entry
epi::StringHash DDFCreateStringHash(std::string_view name);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
