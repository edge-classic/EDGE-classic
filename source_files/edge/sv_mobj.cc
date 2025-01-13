//----------------------------------------------------------------------------
//  EDGE New SaveGame Handling (Things)
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
// See the file "docs/save_sys.txt" for a complete description of the
// new savegame system.
//
// This file handles:
//    MapObject        [MOBJ]
//    spawnspot_t   [SPWN]
//    RespawnQueueItem   [ITMQ]
//

#include <string.h>

#include "epi.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"
#include "p_setup.h"
#include "stb_sprintf.h"
#include "sv_chunk.h"
#include "sv_main.h"

// forward decls.
int   SaveGameMapObjectCountElems(void);
int   SaveGameMapObjectGetIndex(MapObject *elem);
void *SaveGameMapObjectFindByIndex(int index);
void  SaveGameMapObjectCreateElems(int num_elems);
void  SaveGameMapObjectFinaliseElems(void);

int   SV_ItemqCountElems(void);
int   SV_ItemqGetIndex(RespawnQueueItem *elem);
void *SV_ItemqFindByIndex(int index);
void  SV_ItemqCreateElems(int num_elems);
void  SV_ItemqFinaliseElems(void);

bool SaveGameMapObjectGetPlayer(void *storage, int index);
bool SaveGameGetMapObject(void *storage, int index);
bool SaveGameMapObjectGetType(void *storage, int index);
bool SaveGameMapObjectGetState(void *storage, int index);
bool SaveGameMapObjectGetSpawnPoint(void *storage, int index);
bool SaveGameMapObjectGetAttack(void *storage, int index);
bool SaveGameMapObjectGetWUDs(void *storage, int index);

void SaveGameMapObjectPutPlayer(void *storage, int index);
void SaveGamePutMapObject(void *storage, int index);
void SaveGameMapObjectPutType(void *storage, int index);
void SaveGameMapObjectPutState(void *storage, int index);
void SaveGameMapObjectPutSpawnPoint(void *storage, int index);
void SaveGameMapObjectPutAttack(void *storage, int index);
void SaveGameMapObjectPutWUDs(void *storage, int index);

//----------------------------------------------------------------------------
//
//  MOBJ STRUCTURE AND ARRAY
//

class DummyMapObject : public MapObject
{

};

static DummyMapObject dummy_map_object;

static SaveField sv_fields_mobj[] = {
    EDGE_SAVE_FIELD(dummy_map_object, x, "x", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat, SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_map_object, y, "y", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat, SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_map_object, z, "z", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat, SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_map_object, angle_, "angle", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetAngle,
                    SaveGamePutAngle),
    EDGE_SAVE_FIELD(dummy_map_object, floor_z_, "floorz", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_map_object, ceiling_z_, "ceilingz", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_map_object, dropoff_z_, "dropoffz", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_map_object, radius_, "radius", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_map_object, height_, "height", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_map_object, scale_, "scale", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_map_object, aspect_, "aspect", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_map_object, alpha_, "alpha", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_map_object, momentum_, "mom", 1, kSaveFieldNumeric, 12, nullptr, SaveGameGetVec3,
                    SaveGamePutVec3),
    EDGE_SAVE_FIELD(dummy_map_object, health_, "health", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_map_object, spawn_health_, "spawnhealth", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_map_object, speed_, "speed", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_map_object, fuse_, "fuse", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_map_object, morph_timeout_, "morphtimeout", 1, kSaveFieldNumeric, 4, nullptr,
                    SaveGameGetInteger, SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_map_object, pre_become_, "preBecome", 1, kSaveFieldString, 0, nullptr,
                    SaveGameMapObjectGetType, SaveGameMapObjectPutType),
    EDGE_SAVE_FIELD(dummy_map_object, info_, "info", 1, kSaveFieldString, 0, nullptr, SaveGameMapObjectGetType,
                    SaveGameMapObjectPutType),
    EDGE_SAVE_FIELD(dummy_map_object, state_, "state", 1, kSaveFieldString, 0, nullptr, SaveGameMapObjectGetState,
                    SaveGameMapObjectPutState),
    EDGE_SAVE_FIELD(dummy_map_object, next_state_, "next_state", 1, kSaveFieldString, 0, nullptr,
                    SaveGameMapObjectGetState, SaveGameMapObjectPutState),
    EDGE_SAVE_FIELD(dummy_map_object, tics_, "tics", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_map_object, flags_, "flags", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_map_object, extended_flags_, "extendedflags", 1, kSaveFieldNumeric, 4, nullptr,
                    SaveGameGetInteger, SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_map_object, hyper_flags_, "hyperflags", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_map_object, move_direction_, "movedir", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_map_object, move_count_, "movecount", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_map_object, reaction_time_, "reactiontime", 1, kSaveFieldNumeric, 4, nullptr,
                    SaveGameGetInteger, SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_map_object, threshold_, "threshold", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_map_object, model_skin_, "model_skin", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_map_object, model_scale_, "model_scale", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_map_object, model_aspect_, "model_aspect", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_map_object, tag_, "tag", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_map_object, wait_until_dead_tags_, "wud_tags", 1, kSaveFieldString, 0, nullptr,
                    SaveGameMapObjectGetWUDs, SaveGameMapObjectPutWUDs),
    EDGE_SAVE_FIELD(dummy_map_object, side_, "side", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_map_object, player_, "player", 1, kSaveFieldIndex, 4, "players", SaveGameMapObjectGetPlayer,
                    SaveGameMapObjectPutPlayer),
    EDGE_SAVE_FIELD(dummy_map_object, spawnpoint_, "spawnpoint", 1, kSaveFieldStruct, 0, "spawnpoint_t",
                    SaveGameMapObjectGetSpawnPoint, SaveGameMapObjectPutSpawnPoint),
    EDGE_SAVE_FIELD(dummy_map_object, original_height_, "origheight", 1, kSaveFieldNumeric, 4, nullptr,
                    SaveGameGetFloat, SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_map_object, visibility_, "visibility", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_map_object, target_visibility_, "vis_target", 1, kSaveFieldNumeric, 4, nullptr,
                    SaveGameGetFloat, SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_map_object, pain_chance_, "painchance", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_map_object, vertical_angle_, "vertangle", 1, kSaveFieldNumeric, 4, nullptr,
                    SaveGameGetAngleFromSlope, SaveGamePutAngleToSlope),
    EDGE_SAVE_FIELD(dummy_map_object, spread_count_, "spreadcount", 1, kSaveFieldNumeric, 4, nullptr,
                    SaveGameGetInteger, SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_map_object, current_attack_, "currentattack", 1, kSaveFieldString, 0, nullptr,
                    SaveGameMapObjectGetAttack, SaveGameMapObjectPutAttack),
    EDGE_SAVE_FIELD(dummy_map_object, source_, "source", 1, kSaveFieldIndex, 4, "mobjs", SaveGameGetMapObject,
                    SaveGamePutMapObject),
    EDGE_SAVE_FIELD(dummy_map_object, target_, "target", 1, kSaveFieldIndex, 4, "mobjs", SaveGameGetMapObject,
                    SaveGamePutMapObject),
    EDGE_SAVE_FIELD(dummy_map_object, tracer_, "tracer", 1, kSaveFieldIndex, 4, "mobjs", SaveGameGetMapObject,
                    SaveGamePutMapObject),
    EDGE_SAVE_FIELD(dummy_map_object, support_object_, "supportobj", 1, kSaveFieldIndex, 4, "mobjs",
                    SaveGameGetMapObject, SaveGamePutMapObject),
    EDGE_SAVE_FIELD(dummy_map_object, above_object_, "above_mo", 1, kSaveFieldIndex, 4, "mobjs", SaveGameGetMapObject,
                    SaveGamePutMapObject),
    EDGE_SAVE_FIELD(dummy_map_object, below_object_, "below_mo", 1, kSaveFieldIndex, 4, "mobjs", SaveGameGetMapObject,
                    SaveGamePutMapObject),
    EDGE_SAVE_FIELD(dummy_map_object, ride_delta_x_, "ride_dx", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_map_object, ride_delta_y_, "ride_dy", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_map_object, on_ladder_, "on_ladder", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_map_object, path_trigger_, "path_trigger", 1, kSaveFieldString, 0, nullptr,
                    SaveGameGetRADScript, SaveGamePutRADScript),
    EDGE_SAVE_FIELD(dummy_map_object, dynamic_light_.r, "dlight_qty", 1, kSaveFieldNumeric, 4, nullptr,
                    SaveGameGetFloat, SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_map_object, dynamic_light_.target, "dlight_target", 1, kSaveFieldNumeric, 4, nullptr,
                    SaveGameGetFloat, SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_map_object, dynamic_light_.color, "dlight_color", 1, kSaveFieldNumeric, 4, nullptr,
                    SaveGameGetInteger, SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_map_object, shot_count_, "shot_count", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_map_object, last_heard_, "lastheard", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_map_object, is_voodoo_, "is_voodoo", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetBoolean,
                    SaveGamePutBoolean),
    // NOT HERE:
    //   subsector & region: these are regenerated.
    //   next,prev,snext,sprev,bnext,bprev: links are regenerated.
    //   tunnel_hash: would be meaningless, and not important.
    //   lastlookup: being reset to zero won't hurt.
    //   ...

    {0, nullptr, 0, {kSaveFieldInvalid, 0, nullptr}, nullptr, nullptr, nullptr}};

SaveStruct sv_struct_mobj = {
    nullptr,                         // link in list
    "mobj_t",                        // structure name
    "mobj",                          // start marker
    sv_fields_mobj,                  // field descriptions
    (const char *)&dummy_map_object, // dummy base
    true,                            // define_me
    nullptr                          // pointer to known struct
};

SaveArray sv_array_mobj = {
    nullptr,                        // link in list
    "mobjs",                        // array name
    &sv_struct_mobj,                // array type
    true,                           // define_me
    true,                           // allow_hub

    SaveGameMapObjectCountElems,    // count routine
    SaveGameMapObjectFindByIndex,   // index routine
    SaveGameMapObjectCreateElems,   // creation routine
    SaveGameMapObjectFinaliseElems, // finalisation routine

    nullptr,                        // pointer to known array
    0                               // loaded size
};

//----------------------------------------------------------------------------
//
//  SPAWNPOINT STRUCTURE
//
static SpawnPoint dummy_spawn_point;

static SaveField sv_fields_spawnpoint[] = {
    EDGE_SAVE_FIELD(dummy_spawn_point, x, "x", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat, SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_spawn_point, y, "y", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat, SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_spawn_point, z, "z", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat, SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_spawn_point, angle, "angle", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetAngle,
                    SaveGamePutAngle),
    EDGE_SAVE_FIELD(dummy_spawn_point, vertical_angle, "slope", 1, kSaveFieldNumeric, 4, nullptr,
                    SaveGameGetAngleFromSlope, SaveGamePutAngleToSlope),
    EDGE_SAVE_FIELD(dummy_spawn_point, info, "info", 1, kSaveFieldString, 0, nullptr, SaveGameMapObjectGetType,
                    SaveGameMapObjectPutType),
    EDGE_SAVE_FIELD(dummy_spawn_point, flags, "flags", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),

    {0, nullptr, 0, {kSaveFieldInvalid, 0, nullptr}, nullptr, nullptr, nullptr}};

SaveStruct sv_struct_spawnpoint = {
    nullptr,                          // link in list
    "spawnpoint_t",                   // structure name
    "spwn",                           // start marker
    sv_fields_spawnpoint,             // field descriptions
    (const char *)&dummy_spawn_point, // dummy base
    true,                             // define_me
    nullptr                           // pointer to known struct
};

//----------------------------------------------------------------------------
//
//  ITEMINQUE STRUCTURE AND ARRAY
//
static RespawnQueueItem dummy_queue_item;

static SaveField sv_fields_iteminque[] = {
    EDGE_SAVE_FIELD(dummy_queue_item, spawnpoint, "spawnpoint", 1, kSaveFieldStruct, 0, "spawnpoint_t",
                    SaveGameMapObjectGetSpawnPoint, SaveGameMapObjectPutSpawnPoint),
    EDGE_SAVE_FIELD(dummy_queue_item, time, "time", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),

    // NOT HERE:
    //   next,prev: links are regenerated.

    {0, nullptr, 0, {kSaveFieldInvalid, 0, nullptr}, nullptr, nullptr, nullptr}};

SaveStruct sv_struct_iteminque = {
    nullptr,                         // link in list
    "iteminque_t",                   // structure name
    "itmq",                          // start marker
    sv_fields_iteminque,             // field descriptions
    (const char *)&dummy_queue_item, // dummy base
    true,                            // define_me
    nullptr                          // pointer to known struct
};

SaveArray sv_array_iteminque = {
    nullptr,               // link in list
    "itemquehead",         // array name
    &sv_struct_iteminque,  // array type
    true,                  // define_me
    true,                  // allow_hub

    SV_ItemqCountElems,    // count routine
    SV_ItemqFindByIndex,   // index routine
    SV_ItemqCreateElems,   // creation routine
    SV_ItemqFinaliseElems, // finalisation routine

    nullptr,               // pointer to known array
    0                      // loaded size
};

//----------------------------------------------------------------------------

//
// SaveGameMapObjectCountElems
//
int SaveGameMapObjectCountElems(void)
{
    MapObject *cur;
    int        count = 0;

    for (cur = map_object_list_head; cur; cur = cur->next_)
        count++;

    return count;
}

//
// SaveGameMapObjectFindByIndex
//
// The index here starts at 0.
//
void *SaveGameMapObjectFindByIndex(int index)
{
    MapObject *cur;

    for (cur = map_object_list_head; cur && index > 0; cur = cur->next_)
        index--;

    if (!cur)
        FatalError("LOADGAME: Invalid Mobj: %d\n", index);

    EPI_ASSERT(index == 0);

    return cur;
}

//
// SaveGameMapObjectGetIndex
//
// Returns the index number (starts at 0 here).
//
int SaveGameMapObjectGetIndex(MapObject *elem)
{
    MapObject *cur;
    int        index;

    for (cur = map_object_list_head, index = 0; cur && cur != elem; cur = cur->next_)
        index++;

    if (!cur)
        FatalError("LOADGAME: No such MobjPtr: %p\n", elem);

    return index;
}

void SaveGameMapObjectCreateElems(int num_elems)
{
    // free existing mobjs
    if (map_object_list_head)
        RemoveAllMapObjects(true);

    EPI_ASSERT(map_object_list_head == nullptr);

    for (; num_elems > 0; num_elems--)
    {
        MapObject *cur = MapObject::Allocate();

        cur->next_     = map_object_list_head;
        cur->previous_ = nullptr;

        if (map_object_list_head)
            map_object_list_head->previous_ = cur;

        map_object_list_head = cur;

        // initialise defaults
        cur->info_  = nullptr;
        cur->state_ = cur->next_state_ = states + 1;

        cur->model_skin_       = 1;
        cur->model_last_frame_ = -1;
    }
}

void SaveGameMapObjectFinaliseElems(void)
{
    for (MapObject *mo = map_object_list_head; mo != nullptr; mo = mo->next_)
    {
        if (mo->info_ == nullptr)
            mo->info_ = mobjtypes.Lookup(0); // template

        // do not link zombie objects into the blockmap
        if (!mo->IsRemoved())
            SetThingPosition(mo);

        // handle reference counts

        if (mo->tracer_)
            mo->tracer_->reference_count_++;
        if (mo->source_)
            mo->source_->reference_count_++;
        if (mo->target_)
            mo->target_->reference_count_++;
        if (mo->support_object_)
            mo->support_object_->reference_count_++;
        if (mo->above_object_)
            mo->above_object_->reference_count_++;
        if (mo->below_object_)
            mo->below_object_->reference_count_++;

        // sanity checks

        // Lobo fix for RTS ONDEATH actions not working
        // when loading a game
        if (seen_monsters.count(mo->info_) == 0)
            seen_monsters.insert(mo->info_);
    }
}

//----------------------------------------------------------------------------

//
// SV_ItemqCountElems
//
int SV_ItemqCountElems(void)
{
    RespawnQueueItem *cur;
    int               count = 0;

    for (cur = respawn_queue_head; cur; cur = cur->next)
        count++;

    return count;
}

//
// SV_ItemqFindByIndex
//
// The index value starts at 0.
//
void *SV_ItemqFindByIndex(int index)
{
    RespawnQueueItem *cur;

    for (cur = respawn_queue_head; cur && index > 0; cur = cur->next)
        index--;

    if (!cur)
        FatalError("LOADGAME: Invalid ItemInQue: %d\n", index);

    EPI_ASSERT(index == 0);
    return cur;
}

//
// SV_ItemqGetIndex
//
// Returns the index number (starts at 0 here).
//
int SV_ItemqGetIndex(RespawnQueueItem *elem)
{
    RespawnQueueItem *cur;
    int               index;

    for (cur = respawn_queue_head, index = 0; cur && cur != elem; cur = cur->next)
        index++;

    if (!cur)
        FatalError("LOADGAME: No such ItemInQue ptr: %p\n", elem);

    return index;
}

//
// SV_ItemqCreateElems
//
void SV_ItemqCreateElems(int num_elems)
{
    ClearRespawnQueue();

    respawn_queue_head = nullptr;

    for (; num_elems > 0; num_elems--)
    {
        RespawnQueueItem *cur = new RespawnQueueItem;

        cur->next     = respawn_queue_head;
        cur->previous = nullptr;

        if (respawn_queue_head)
            respawn_queue_head->previous = cur;

        respawn_queue_head = cur;

        // initialise defaults: leave blank
    }
}

//
// SV_ItemqFinaliseElems
//
void SV_ItemqFinaliseElems(void)
{
    RespawnQueueItem *cur, *next;

    // remove any dead wood
    for (cur = respawn_queue_head; cur; cur = next)
    {
        next = cur->next;

        if (cur->spawnpoint.info)
            continue;

        LogWarning("LOADGAME: discarding empty ItemInQue\n");

        if (next)
            next->previous = cur->previous;

        if (cur->previous)
            cur->previous->next = next;
        else
            respawn_queue_head = next;

        delete cur;
    }
}

//----------------------------------------------------------------------------

bool SaveGameMapObjectGetPlayer(void *storage, int index)
{
    Player **dest = (Player **)storage + index;

    int swizzle = SaveChunkGetInteger();

    *dest = (swizzle == 0) ? nullptr : (Player *)SaveGamePlayerFindByIndex(swizzle - 1);
    return true;
}

void SaveGameMapObjectPutPlayer(void *storage, int index)
{
    Player *elem = ((Player **)storage)[index];

    int swizzle = (elem == nullptr) ? 0 : SaveGamePlayerGetIndex(elem) + 1;

    SaveChunkPutInteger(swizzle);
}

bool SaveGameGetMapObject(void *storage, int index)
{
    MapObject **dest = (MapObject **)storage + index;

    int swizzle = SaveChunkGetInteger();

    *dest = (swizzle == 0) ? nullptr : (MapObject *)SaveGameMapObjectFindByIndex(swizzle - 1);
    return true;
}

void SaveGamePutMapObject(void *storage, int index)
{
    MapObject *elem = ((MapObject **)storage)[index];

    int swizzle;

    swizzle = (elem == nullptr) ? 0 : SaveGameMapObjectGetIndex(elem) + 1;
    SaveChunkPutInteger(swizzle);
}

bool SaveGameMapObjectGetType(void *storage, int index)
{
    MapObjectDefinition **dest = (MapObjectDefinition **)storage + index;

    const char *name = SaveChunkGetString();

    if (!name)
    {
        *dest = nullptr;
        return true;
    }

    // special handling for projectiles (attacks)
    if (epi::StringPrefixCaseCompareASCII(name, "atk:") == 0)
    {
        const AttackDefinition *atk = atkdefs.Lookup(name + 4);

        if (atk)
            *dest = (MapObjectDefinition *)atk->atk_mobj_;
    }
    else
        *dest = (MapObjectDefinition *)mobjtypes.Lookup(name);

    if (!*dest)
    {
        // Note: a missing 'info' field will be fixed up later
        LogWarning("LOADGAME: no such thing type '%s'\n", name);
    }

    SaveChunkFreeString(name);
    return true;
}

void SaveGameMapObjectPutType(void *storage, int index)
{
    MapObjectDefinition *info = ((MapObjectDefinition **)storage)[index];

    SaveChunkPutString((info == nullptr) ? nullptr : info->name_.c_str());
}

bool SaveGameMapObjectGetSpawnPoint(void *storage, int index)
{
    SpawnPoint *dest = (SpawnPoint *)storage + index;

    if (sv_struct_spawnpoint.counterpart)
        return SaveGameStructLoad(dest, sv_struct_spawnpoint.counterpart);

    return true; // presumably
}

void SaveGameMapObjectPutSpawnPoint(void *storage, int index)
{
    SpawnPoint *src = (SpawnPoint *)storage + index;

    SaveGameStructSave(src, &sv_struct_spawnpoint);
}

bool SaveGameMapObjectGetAttack(void *storage, int index)
{
    AttackDefinition **dest = (AttackDefinition **)storage + index;

    const char *name = SaveChunkGetString();

    // Intentional Const Override
    *dest = (name == nullptr) ? nullptr : (AttackDefinition *)atkdefs.Lookup(name);

    SaveChunkFreeString(name);
    return true;
}

void SaveGameMapObjectPutAttack(void *storage, int index)
{
    AttackDefinition *info = ((AttackDefinition **)storage)[index];

    SaveChunkPutString((info == nullptr) ? nullptr : info->name_.c_str());
}

bool SaveGameMapObjectGetWUDs(void *storage, int index)
{
    std::string *dest = (std::string *)storage;

    EPI_ASSERT(index == 0);

    const char *tags = SaveChunkGetString();

    if (tags)
    {
        dest->resize(strlen(tags));
        std::copy(tags, tags + strlen(tags), dest->data());
    }
    else
        *dest = "";

    SaveChunkFreeString(tags);

    return true;
}

void SaveGameMapObjectPutWUDs(void *storage, int index)
{
    std::string *src = (std::string *)storage;

    EPI_ASSERT(index == 0);

    SaveChunkPutString(src->empty() ? nullptr : src->c_str());
}

//----------------------------------------------------------------------------

//
// SaveGameMapObjectGetState
//
bool SaveGameMapObjectGetState(void *storage, int index)
{
    State **dest = (State **)storage + index;

    char  buffer[256];
    char *base_p, *off_p;
    int   base, offset;

    const char                *swizzle;
    const MapObject           *mo = (MapObject *)sv_current_elem;
    const MapObjectDefinition *actual;

    EPI_ASSERT(mo);

    swizzle = SaveChunkGetString();

    if (!swizzle || !mo->info_)
    {
        *dest = nullptr;
        return true;
    }

    epi::CStringCopyMax(buffer, swizzle, 256 - 1);
    SaveChunkFreeString(swizzle);

    // separate string at `:' characters

    base_p = strchr(buffer, ':');

    if (base_p == nullptr || base_p[0] == 0)
        FatalError("Corrupt savegame: bad state 1/2: `%s'\n", buffer);

    *base_p++ = 0;

    off_p = strchr(base_p, ':');

    if (off_p == nullptr || off_p[0] == 0)
        FatalError("Corrupt savegame: bad state 2/2: `%s'\n", base_p);

    *off_p++ = 0;

    // find thing that contains the state
    actual = mo->info_;

    if (buffer[0] != '*')
    {
        // Do we care about those in the disabled group?
        actual = mobjtypes.Lookup(buffer);
        if (!actual)
            FatalError("LOADGAME: no such thing %s for state %s:%s\n", buffer, base_p, off_p);
    }

    // find base state
    offset = strtol(off_p, nullptr, 0) - 1;

    base = DDFStateFindLabel(actual->state_grp_, base_p, true /* quiet */);

    if (!base)
    {
        LogWarning("LOADGAME: no such label `%s' for state.\n", base_p);
        offset = 0;

        if (actual->idle_state_)
            base = actual->idle_state_;
        else if (actual->spawn_state_)
            base = actual->spawn_state_;
        else if (actual->meander_state_)
            base = actual->meander_state_;
        else if (actual->state_grp_.size() > 0)
            base = actual->state_grp_[0].first;
        else
            base = 1;
    }

    *dest = states + base + offset;

    return true;
}

//
// SaveGameMapObjectPutState
//
// The format of the string is:
//
//    THING `:' BASE `:' OFFSET
//
// where THING is usually just "*" for the current thing, but can
// refer to another ddf thing (e.g. "IMP").  BASE is the nearest
// labelled state (e.g. "SPAWN"), or "*" as offset from the thing's
// first state (unlikely to be needed).  OFFSET is the integer offset
// from the base state (e.g. "5"), which BTW starts at 1 (like the ddf
// format).
//
// Alternatively, the string can be nullptr, which means the state
// pointer should be nullptr.
//
// P.S: we go to all this trouble to try and get reasonable behaviour
// when loading with different DDF files than what we saved with.
// Typical example: a new item, monster or weapon gets added to our
// DDF files causing all state numbers to be shifted upwards.
//
void SaveGameMapObjectPutState(void *storage, int index)
{
    State *S = ((State **)storage)[index];

    char swizzle[256];

    int s_num, base;

    const MapObject           *mo = (MapObject *)sv_current_elem;
    const MapObjectDefinition *actual;

    EPI_ASSERT(mo);

    if (S == nullptr || !mo->info_)
    {
        SaveChunkPutString(nullptr);
        return;
    }

    // object has no states ?
    if (mo->info_->state_grp_.empty())
    {
        LogWarning("SAVEGAME: object [%s] has no states !!\n", mo->info_->name_.c_str());
        SaveChunkPutString(nullptr);
        return;
    }

    // get state number, check if valid
    s_num = (int)(S - states);

    if (s_num < 0 || s_num >= num_states)
    {
        LogWarning("SAVEGAME: object [%s] is in invalid state %d\n", mo->info_->name_.c_str(), s_num);

        if (mo->info_->idle_state_)
            s_num = mo->info_->idle_state_;
        else if (mo->info_->spawn_state_)
            s_num = mo->info_->spawn_state_;
        else if (mo->info_->meander_state_)
            s_num = mo->info_->meander_state_;
        else
        {
            SaveChunkPutString("*:*:1");
            return;
        }
    }

    // state gone AWOL into another object ?
    actual = mo->info_;

    if (!DDFStateGroupHasState(actual->state_grp_, s_num))
    {
        LogWarning("SAVEGAME: object [%s] is in AWOL state %d\n", mo->info_->name_.c_str(), s_num);

        bool state_found = false;

        // look for real object
        for (auto iter = mobjtypes.begin(); iter != mobjtypes.end(); iter++)
        {
            actual = *iter;

            if (DDFStateGroupHasState(actual->state_grp_, s_num))
            {
                state_found = true;
                break;
            }
        }

        if (!state_found)
        {
            LogWarning("-- ARGH: state %d cannot be found !!\n", s_num);
            SaveChunkPutString("*:*:1");
            return;
        }

        if (actual->name_.empty())
        {
            LogWarning("-- OOPS: state %d found in unnamed object !!\n", s_num);
            SaveChunkPutString("*:*:1");
            return;
        }
    }

    // find the nearest base state
    base = s_num;

    while (!states[base].label && DDFStateGroupHasState(actual->state_grp_, base - 1))
    {
        base--;
    }

    stbsp_sprintf(swizzle, "%s:%s:%d", (actual == mo->info_) ? "*" : actual->name_.c_str(),
                  states[base].label ? states[base].label : "*", 1 + s_num - base);

    SaveChunkPutString(swizzle);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
