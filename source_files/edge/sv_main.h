//----------------------------------------------------------------------------
//  EDGE New SaveGame Handling (Main defs)
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
// TERMINOLOGY:
//
//   - "known" here means an array/structure that is currently built
//     into EDGE.
//
//   - "loaded" here means an array/structure definition that has been
//     loaded from the savegame file.
//

#pragma once

#include <stdint.h>

#include "dm_defs.h"
#include "e_player.h"
#include "p_local.h"

class Image;
class MapDefinition;

//
// STRUCTURE TABLE STUFF
//

enum SaveFieldKind
{
    kSaveFieldInvalid = 0, // invalid values can be helpful
    kSaveFieldNumeric,
    kSaveFieldIndex,
    kSaveFieldString,
    kSaveFieldStruct
};

struct SaveFieldType
{
    // basic kind of field (for SDEF chunk)
    SaveFieldKind kind;

    // number of bytes for kSaveFieldNumeric (1, 2, 4 or 8)
    int size;

    // name of structure for kSaveFieldStruct, or name of array
    // for kSaveFieldIndex.
    const char *name;
};

// This describes a single field
struct SaveField
{
    // offset of field in structure (actually a ptr into dummy struct)
    const char *offset_pointer;

    // name of field in savegame system
    const char *field_name;

    // number of sequential elements
    int count;

    // field type information
    SaveFieldType type;

    // get & put routines.  The extra parameter depends on the type, for
    // kSaveFieldStruct it is the name of the structure, for kSaveFieldIndex
    // it is the name of the array.  When `field_put' is nullptr, then this
    // field is not saved into the output SDEF chunk.
    bool (*field_get)(void *storage, int index);
    void (*field_put)(void *storage, int index);

    // for loaded info, this points to the known version of the field,
    // otherwise nullptr if the loaded field is unknown.
    SaveField *known_field;
};

// NOTE: requires an instantiated dummy struct for "base"
#define EDGE_SAVE_FIELD(base, field, name, num, fkind, fsize, fname, getter, putter)                                   \
    {(const char *)&base.field, name, num, {fkind, fsize, fname}, getter, putter, nullptr}

// This describes a single structure
struct SaveStruct
{
    // link in list of structure definitions
    SaveStruct *next;

    // structure name (for SDEF/ADEF chunks)
    const char *struct_name;

    // four letter marker
    const char *marker;

    // array of field definitions
    SaveField *fields;

    // address of dummy struct (used to compute field offsets)
    const char *dummy_base;

    // this must be true to put the definition into the savegame file.
    // Allows compatibility structures that are read-only.
    bool define_me;

    // only used when loading.  For loaded info, this refers to the
    // known struct of the same name (or nullptr if none).  For known info,
    // this points to the loaded info (or nullptr if absent).
    SaveStruct *counterpart;
};

// This describes a single array
struct SaveArray
{
    // link in list of array definitions
    SaveArray *next;

    // array name (for ADEF and STOR chunks)
    const char *array_name;

    // array type.  For loaded info, this points to the loaded
    // structure.  Never nullptr.
    SaveStruct *sdef;

    // this must be true to put the definition into the savegame file.
    // Allows compatibility arrays that are read-only.
    bool define_me;

    // load this array even when loading in HUB mode.  There are
    // some things we _don't_ want to load when going back to a
    // visited level: players and active_hubs in particular.
    bool allow_hub;

    // array routines.  Not used for loaded info.
    int (*count_elems)(void);
    void *(*get_elem)(int index);
    void (*create_elems)(int num_elems);
    void (*finalise_elems)(void);

    // only used when loading.  For loaded info, this refers to the
    // known array (or nullptr if none).  For known info, this points to
    // the loaded info (or nullptr if absent).
    SaveArray *counterpart;

    // number of elements to be loaded.
    int loaded_size;
};

//
//  COMMON GET ROUTINES
//
//  Note the `SR_' prefix.
//
bool SaveGameGetInteger(void *storage, int index);
bool SaveGameGetAngle(void *storage, int index);
bool SaveGameGetFloat(void *storage, int index);
bool SaveGameGetBoolean(void *storage, int index);
bool SaveGameGetVec2(void *storage, int index);
bool SaveGameGetVec3(void *storage, int index);
bool SaveGameGetAngleFromSlope(void *storage, int index);

//
//  COMMON PUT ROUTINES
//
//  Note the `SR_' prefix.
//
void SaveGamePutInteger(void *storage, int index);
void SaveGamePutAngle(void *storage, int index);
void SaveGamePutFloat(void *storage, int index);
void SaveGamePutBoolean(void *storage, int index);
void SaveGamePutVec2(void *storage, int index);
void SaveGamePutVec3(void *storage, int index);
void SaveGamePutAngleToSlope(void *storage, int index);

//
//  GLOBAL STUFF
//

struct CrcCheck
{
    // number of items
    int count;

    // CRC computed over all the items
    uint32_t crc;
};

// this structure contains everything for the top-level [GLOB] chunk.
// Strings are copies and need to be freed.
struct SaveGlobals
{
    // [IVAR] stuff:

    const char *game;
    const char *level;
    GameFlags   flags;
    int         hub_tag;
    const char *hub_first;

    int level_time;
    int exit_time;
    uint64_t p_random;
    int total_kills;
    int total_items;
    int total_secrets;

    int console_player;
    int skill;
    int netgame;

    const Image *sky_image; // -AJA- added 2003/12/19

    const char *description;
    const char *desc_date;

    CrcCheck mapsector;
    CrcCheck mapline;
    CrcCheck mapthing;

    CrcCheck rscript;
    CrcCheck ddfatk;
    CrcCheck ddfgame;
    CrcCheck ddflevl;
    CrcCheck ddfline;
    CrcCheck ddfsect;
    CrcCheck ddfmobj;
    CrcCheck ddfweap;

    // [WADS] info
    int          wad_num;
    const char **wad_names;

    // [PLYR] info, for DEMO FILES only!
    void *players[kMaximumPlayers];
};

SaveGlobals *SaveGlobalsNew(void);
SaveGlobals *SaveGlobalsLoad(void);
void         SaveGlobalsSave(SaveGlobals *globs);
void         SaveGlobalsFree(SaveGlobals *globs);

//
//  ADMININISTRATION
//

void InitializeSaveSystem(void);

SaveStruct *SaveStructLookup(const char *name);
SaveArray  *SaveArrayLookup(const char *name);

void BeginSaveGameLoad(bool is_hub);
void FinishSaveGameLoad(void);

bool SaveGameStructLoad(void *base, SaveStruct *info);
bool LoadAllSaveChunks(void);

void BeginSaveGameSave(void);
void FinishSaveGameSave(void);

void SaveGameStructSave(void *base, SaveStruct *info);
void SaveAllSaveChunks(void);

const char *SaveSlotName(int slot);
const char *SaveMapName(const MapDefinition *map);

std::string SaveFilename(const char *slot_name, const char *map_name);

void SaveClearSlot(const char *slot_name);
void SaveCopySlot(const char *src_name, const char *dest_name);

//
//  EXTERNAL DEFS
//

extern void *sv_current_elem;

extern SaveStruct *sv_known_structs;
extern SaveArray  *sv_known_arrays;

bool SaveGameGetMapObject(void *storage, int index);
void SaveGamePutMapObject(void *storage, int index);

int   SaveGameMapObjectGetIndex(MapObject *elem);
void *SaveGameMapObjectFindByIndex(int index);

int   SaveGamePlayerGetIndex(Player *elem);
void *SaveGamePlayerFindByIndex(int index);

bool SaveGameLevelGetImage(void *storage, int index);
void SaveGameLevelPutImage(void *storage, int index);

bool SaveGameLevelGetColormap(void *storage, int index);
void SaveGameLevelPutColormap(void *storage, int index);

bool SaveGameGetLine(void *storage, int index);
void SaveGamePutLine(void *storage, int index);

bool SaveGameGetSector(void *storage, int index);
void SaveGamePutSector(void *storage, int index);

bool SaveGameSectorGetExtrafloor(void *storage, int index);
void SaveGameSectorPutExtrafloor(void *storage, int index);

bool SaveGameGetRADScript(void *storage, int index);
void SaveGamePutRADScript(void *storage, int index);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
