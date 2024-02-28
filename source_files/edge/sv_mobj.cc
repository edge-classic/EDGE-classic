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



#include "p_setup.h"
#include "sv_chunk.h"
#include "sv_main.h"
#include "str_compare.h"
#include "str_util.h"
#undef SF
#define SF SVFIELD

// forward decls.
int   SV_MobjCountElems(void);
int   SV_MobjFindElem(MapObject *elem);
void *SV_MobjGetElem(int index);
void  SV_MobjCreateElems(int num_elems);
void  SV_MobjFinaliseElems(void);

int   SV_ItemqCountElems(void);
int   SV_ItemqFindElem(RespawnQueueItem *elem);
void *SV_ItemqGetElem(int index);
void  SV_ItemqCreateElems(int num_elems);
void  SV_ItemqFinaliseElems(void);

bool SR_MobjGetPlayer(void *storage, int index, void *extra);
bool SR_MobjGetMobj(void *storage, int index, void *extra);
bool SR_MobjGetType(void *storage, int index, void *extra);
bool SR_MobjGetState(void *storage, int index, void *extra);
bool SR_MobjGetSpawnPoint(void *storage, int index, void *extra);
bool SR_MobjGetAttack(void *storage, int index, void *extra);
bool SR_MobjGetWUDs(void *storage, int index, void *extra);

void SR_MobjPutPlayer(void *storage, int index, void *extra);
void SR_MobjPutMobj(void *storage, int index, void *extra);
void SR_MobjPutType(void *storage, int index, void *extra);
void SR_MobjPutState(void *storage, int index, void *extra);
void SR_MobjPutSpawnPoint(void *storage, int index, void *extra);
void SR_MobjPutAttack(void *storage, int index, void *extra);
void SR_MobjPutWUDs(void *storage, int index, void *extra);

//----------------------------------------------------------------------------
//
//  MOBJ STRUCTURE AND ARRAY
//
static MapObject sv_dummy_mobj;

#define SV_F_BASE sv_dummy_mobj

static savefield_t sv_fields_mobj[] = {
    SF(x, "x", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat), SF(y, "y", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(z, "z", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat), SF(angle_, "angle", 1, SVT_ANGLE, SR_GetAngle, SR_PutAngle),
    SF(floor_z_, "floorz", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(ceiling_z_, "ceilingz", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(dropoff_z_, "dropoffz", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(radius_, "radius", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(height_, "height", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat), 
    SF(scale_, "scale", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(aspect_, "aspect", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(alpha_, "alpha", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(momentum_, "mom", 1, SVT_VEC3, SR_GetVec3, SR_PutVec3),
    SF(health_, "health", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(spawn_health_, "spawnhealth", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(speed_, "speed", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat), SF(fuse_, "fuse", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(morph_timeout_, "morphtimeout", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(pre_become_, "preBecome", 1, SVT_STRING, SR_MobjGetType, SR_MobjPutType),
    SF(info_, "info", 1, SVT_STRING, SR_MobjGetType, SR_MobjPutType),
    SF(state_, "state", 1, SVT_STRING, SR_MobjGetState, SR_MobjPutState),
    SF(next_state_, "next_state", 1, SVT_STRING, SR_MobjGetState, SR_MobjPutState),
    SF(tics_, "tics", 1, SVT_INT, SR_GetInt, SR_PutInt), SF(flags_, "flags", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(extended_flags_, "extendedflags", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(hyper_flags_, "hyperflags", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(move_direction_, "movedir", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(move_count_, "movecount", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(reaction_time_, "reactiontime", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(threshold_, "threshold", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(model_skin_, "model_skin", 1, SVT_INT, SR_GetInt, SR_PutInt), 
    SF(model_scale_, "model_scale", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(model_aspect_, "model_aspect", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(tag_, "tag", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(wait_until_dead_tags_, "wud_tags", 1, SVT_STRING, SR_MobjGetWUDs, SR_MobjPutWUDs),
    SF(side_, "side", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(player_, "player", 1, SVT_INDEX("players"), SR_MobjGetPlayer, SR_MobjPutPlayer),
    SF(spawnpoint_, "spawnpoint", 1, SVT_STRUCT("spawnpoint_t"), SR_MobjGetSpawnPoint, SR_MobjPutSpawnPoint),
    SF(original_height_, "origheight", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(visibility_, "visibility", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(target_visibility_, "vis_target", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(pain_chance_, "painchance", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(vertical_angle_, "vertangle", 1, SVT_FLOAT, SR_GetAngleFromSlope, SR_PutAngleToSlope),
    SF(spread_count_, "spreadcount", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(current_attack_, "currentattack", 1, SVT_STRING, SR_MobjGetAttack, SR_MobjPutAttack),
    SF(source_, "source", 1, SVT_INDEX("mobjs"), SR_MobjGetMobj, SR_MobjPutMobj),
    SF(target_, "target", 1, SVT_INDEX("mobjs"), SR_MobjGetMobj, SR_MobjPutMobj),
    SF(tracer_, "tracer", 1, SVT_INDEX("mobjs"), SR_MobjGetMobj, SR_MobjPutMobj),
    SF(support_object_, "supportobj", 1, SVT_INDEX("mobjs"), SR_MobjGetMobj, SR_MobjPutMobj),
    SF(above_object_, "above_mo", 1, SVT_INDEX("mobjs"), SR_MobjGetMobj, SR_MobjPutMobj),
    SF(below_object_, "below_mo", 1, SVT_INDEX("mobjs"), SR_MobjGetMobj, SR_MobjPutMobj),
    SF(ride_delta_x_, "ride_dx", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(ride_delta_y_, "ride_dy", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(on_ladder_, "on_ladder", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(path_trigger_, "path_trigger", 1, SVT_STRING, SR_TriggerGetScript, SR_TriggerPutScript),
    SF(dynamic_light_.r, "dlight_qty", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(dynamic_light_.target, "dlight_target", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(dynamic_light_.color, "dlight_color", 1, SVT_RGBCOL, SR_GetRGB, SR_PutRGB),
    SF(shot_count_, "shot_count", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(last_heard_, "lastheard", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(is_voodoo_, "is_voodoo", 1, SVT_BOOLEAN, SR_GetBoolean, SR_PutBoolean),
    // NOT HERE:
    //   subsector & region: these are regenerated.
    //   next,prev,snext,sprev,bnext,bprev: links are regenerated.
    //   tunnel_hash: would be meaningless, and not important.
    //   lastlookup: being reset to zero won't hurt.
    //   ...

    SVFIELD_END};

savestruct_t sv_struct_mobj = {
    nullptr,           // link in list
    "mobj_t",       // structure name
    "mobj",         // start marker
    sv_fields_mobj, // field descriptions
    SVDUMMY,        // dummy base
    true,           // define_me
    nullptr            // pointer to known struct
};

#undef SV_F_BASE

savearray_t sv_array_mobj = {
    nullptr,            // link in list
    "mobjs",         // array name
    &sv_struct_mobj, // array type
    true,            // define_me
    true,            // allow_hub

    SV_MobjCountElems,    // count routine
    SV_MobjGetElem,       // index routine
    SV_MobjCreateElems,   // creation routine
    SV_MobjFinaliseElems, // finalisation routine

    nullptr, // pointer to known array
    0     // loaded size
};

//----------------------------------------------------------------------------
//
//  SPAWNPOINT STRUCTURE
//
static SpawnPoint sv_dummy_spawnpoint;

#define SV_F_BASE sv_dummy_spawnpoint

static savefield_t sv_fields_spawnpoint[] = {
    SF(x, "x", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(y, "y", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(z, "z", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(angle, "angle", 1, SVT_ANGLE, SR_GetAngle, SR_PutAngle),
    SF(vertical_angle, "slope", 1, SVT_FLOAT, SR_GetAngleFromSlope, SR_PutAngleToSlope),
    SF(info, "info", 1, SVT_STRING, SR_MobjGetType, SR_MobjPutType),
    SF(flags, "flags", 1, SVT_INT, SR_GetInt, SR_PutInt),

    SVFIELD_END};

savestruct_t sv_struct_spawnpoint = {
    nullptr,                 // link in list
    "spawnpoint_t",       // structure name
    "spwn",               // start marker
    sv_fields_spawnpoint, // field descriptions
    SVDUMMY,              // dummy base
    true,                 // define_me
    nullptr                  // pointer to known struct
};

#undef SV_F_BASE

//----------------------------------------------------------------------------
//
//  ITEMINQUE STRUCTURE AND ARRAY
//
static RespawnQueueItem sv_dummy_iteminque;

#define SV_F_BASE sv_dummy_iteminque

static savefield_t sv_fields_iteminque[] = {
    SF(spawnpoint, "spawnpoint", 1, SVT_STRUCT("spawnpoint_t"), SR_MobjGetSpawnPoint, SR_MobjPutSpawnPoint),
    SF(time, "time", 1, SVT_INT, SR_GetInt, SR_PutInt),

    // NOT HERE:
    //   next,prev: links are regenerated.

    SVFIELD_END};

savestruct_t sv_struct_iteminque = {
    nullptr,                // link in list
    "RespawnQueueItem",       // structure name
    "itmq",              // start marker
    sv_fields_iteminque, // field descriptions
    SVDUMMY,             // dummy base
    true,                // define_me
    nullptr                 // pointer to known struct
};

#undef SV_F_BASE

savearray_t sv_array_iteminque = {
    nullptr,                 // link in list
    "respawn_queue_head",        // array name
    &sv_struct_iteminque, // array type
    true,                 // define_me
    true,                 // allow_hub

    SV_ItemqCountElems,    // count routine
    SV_ItemqGetElem,       // index routine
    SV_ItemqCreateElems,   // creation routine
    SV_ItemqFinaliseElems, // finalisation routine

    nullptr, // pointer to known array
    0     // loaded size
};

//----------------------------------------------------------------------------

//
// SV_MobjCountElems
//
int SV_MobjCountElems(void)
{
    MapObject *cur;
    int     count = 0;

    for (cur = map_object_list_head; cur; cur = cur->next_)
        count++;

    return count;
}

//
// SV_MobjGetElem
//
// The index here starts at 0.
//
void *SV_MobjGetElem(int index)
{
    MapObject *cur;

    for (cur = map_object_list_head; cur && index > 0; cur = cur->next_)
        index--;

    if (!cur)
        FatalError("LOADGAME: Invalid Mobj: %d\n", index);

    SYS_ASSERT(index == 0);

    return cur;
}

//
// SV_MobjFindElem
//
// Returns the index number (starts at 0 here).
//
int SV_MobjFindElem(MapObject *elem)
{
    MapObject *cur;
    int     index;

    for (cur = map_object_list_head, index = 0; cur && cur != elem; cur = cur->next_)
        index++;

    if (!cur)
        FatalError("LOADGAME: No such MobjPtr: %p\n", elem);

    return index;
}

void SV_MobjCreateElems(int num_elems)
{
    // free existing mobjs
    if (map_object_list_head)
        P_RemoveAllMobjs(true);

    SYS_ASSERT(map_object_list_head == nullptr);

    for (; num_elems > 0; num_elems--)
    {
        MapObject *cur = new MapObject;

        cur->next_ = map_object_list_head;
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

void SV_MobjFinaliseElems(void)
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
    int          count = 0;

    for (cur = respawn_queue_head; cur; cur = cur->next)
        count++;

    return count;
}

//
// SV_ItemqGetElem
//
// The index value starts at 0.
//
void *SV_ItemqGetElem(int index)
{
    RespawnQueueItem *cur;

    for (cur = respawn_queue_head; cur && index > 0; cur = cur->next)
        index--;

    if (!cur)
        FatalError("LOADGAME: Invalid ItemInQue: %d\n", index);

    SYS_ASSERT(index == 0);
    return cur;
}

//
// SV_ItemqFindElem
//
// Returns the index number (starts at 0 here).
//
int SV_ItemqFindElem(RespawnQueueItem *elem)
{
    RespawnQueueItem *cur;
    int          index;

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
    P_RemoveItemsInQue();

    respawn_queue_head = nullptr;

    for (; num_elems > 0; num_elems--)
    {
        RespawnQueueItem *cur = new RespawnQueueItem;

        cur->next = respawn_queue_head;
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

bool SR_MobjGetPlayer(void *storage, int index, void *extra)
{
    player_t **dest = (player_t **)storage + index;

    int swizzle = SV_GetInt();

    *dest = (swizzle == 0) ? nullptr : (player_t *)SV_PlayerGetElem(swizzle - 1);
    return true;
}

void SR_MobjPutPlayer(void *storage, int index, void *extra)
{
    player_t *elem = ((player_t **)storage)[index];

    int swizzle = (elem == nullptr) ? 0 : SV_PlayerFindElem(elem) + 1;

    SV_PutInt(swizzle);
}

bool SR_MobjGetMobj(void *storage, int index, void *extra)
{
    MapObject **dest = (MapObject **)storage + index;

    int swizzle = SV_GetInt();

    *dest = (swizzle == 0) ? nullptr : (MapObject *)SV_MobjGetElem(swizzle - 1);
    return true;
}

void SR_MobjPutMobj(void *storage, int index, void *extra)
{
    MapObject *elem = ((MapObject **)storage)[index];

    int swizzle;

    swizzle = (elem == nullptr) ? 0 : SV_MobjFindElem(elem) + 1;
    SV_PutInt(swizzle);
}

bool SR_MobjGetType(void *storage, int index, void *extra)
{
    MapObjectDefinition **dest = (MapObjectDefinition **)storage + index;

    const char *name = SV_GetString();

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

    SV_FreeString(name);
    return true;
}

void SR_MobjPutType(void *storage, int index, void *extra)
{
    MapObjectDefinition *info = ((MapObjectDefinition **)storage)[index];

    SV_PutString((info == nullptr) ? nullptr : info->name_.c_str());
}

bool SR_MobjGetSpawnPoint(void *storage, int index, void *extra)
{
    SpawnPoint *dest = (SpawnPoint *)storage + index;

    if (sv_struct_spawnpoint.counterpart)
        return SV_LoadStruct(dest, sv_struct_spawnpoint.counterpart);

    return true; // presumably
}

void SR_MobjPutSpawnPoint(void *storage, int index, void *extra)
{
    SpawnPoint *src = (SpawnPoint *)storage + index;

    SV_SaveStruct(src, &sv_struct_spawnpoint);
}

bool SR_MobjGetAttack(void *storage, int index, void *extra)
{
    AttackDefinition **dest = (AttackDefinition **)storage + index;

    const char *name = SV_GetString();

    // Intentional Const Override
    *dest = (name == nullptr) ? nullptr : (AttackDefinition *)atkdefs.Lookup(name);

    SV_FreeString(name);
    return true;
}

void SR_MobjPutAttack(void *storage, int index, void *extra)
{
    AttackDefinition *info = ((AttackDefinition **)storage)[index];

    SV_PutString((info == nullptr) ? nullptr : info->name_.c_str());
}

bool SR_MobjGetWUDs(void *storage, int index, void *extra)
{
    std::string *dest = (std::string *)storage;

    SYS_ASSERT(index == 0);

    const char *tags = SV_GetString();

    if (tags)
    {
        dest->resize(strlen(tags));
        std::copy(tags, tags + strlen(tags), dest->data());
    }
    else
        *dest = "";

    SV_FreeString(tags);

    return true;
}

void SR_MobjPutWUDs(void *storage, int index, void *extra)
{
    std::string *src = (std::string *)storage;

    SYS_ASSERT(index == 0);

    SV_PutString(src->empty() ? nullptr : src->c_str());
}

//----------------------------------------------------------------------------

//
// SR_MobjGetState
//
bool SR_MobjGetState(void *storage, int index, void *extra)
{
    State **dest = (State **)storage + index;

    char  buffer[256];
    char *base_p, *off_p;
    int   base, offset;

    const char       *swizzle;
    const MapObject     *mo = (MapObject *)sv_current_elem;
    const MapObjectDefinition *actual;

    SYS_ASSERT(mo);

    swizzle = SV_GetString();

    if (!swizzle || !mo->info_)
    {
        *dest = nullptr;
        return true;
    }

    epi::CStringCopyMax(buffer, swizzle, 256 - 1);
    SV_FreeString(swizzle);

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

    base = DDF_StateFindLabel(actual->state_grp_, base_p, true /* quiet */);

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
// SR_MobjPutState
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
void SR_MobjPutState(void *storage, int index, void *extra)
{
    State *S = ((State **)storage)[index];

    char swizzle[256];

    int s_num, base;

    const MapObject     *mo = (MapObject *)sv_current_elem;
    const MapObjectDefinition *actual;

    SYS_ASSERT(mo);

    if (S == nullptr || !mo->info_)
    {
        SV_PutString(nullptr);
        return;
    }

    // object has no states ?
    if (mo->info_->state_grp_.empty())
    {
        LogWarning("SAVEGAME: object [%s] has no states !!\n", mo->info_->name_.c_str());
        SV_PutString(nullptr);
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
            SV_PutString("*:*:1");
            return;
        }
    }

    // state gone AWOL into another object ?
    actual = mo->info_;

    if (!DDF_StateGroupHasState(actual->state_grp_, s_num))
    {
        LogWarning("SAVEGAME: object [%s] is in AWOL state %d\n", mo->info_->name_.c_str(), s_num);

        bool state_found = false;

        // look for real object
        for (auto iter = mobjtypes.begin(); iter != mobjtypes.end(); iter++)
        {
            actual = *iter;

            if (DDF_StateGroupHasState(actual->state_grp_, s_num))
            {
                state_found = true;
                break;
            }
        }

        if (!state_found)
        {
            LogWarning("-- ARGH: state %d cannot be found !!\n", s_num);
            SV_PutString("*:*:1");
            return;
        }

        if (actual->name_.empty())
        {
            LogWarning("-- OOPS: state %d found in unnamed object !!\n", s_num);
            SV_PutString("*:*:1");
            return;
        }
    }

    // find the nearest base state
    base = s_num;

    while (!states[base].label && DDF_StateGroupHasState(actual->state_grp_, base - 1))
    {
        base--;
    }

    sprintf(swizzle, "%s:%s:%d", (actual == mo->info_) ? "*" : actual->name_.c_str(),
            states[base].label ? states[base].label : "*", 1 + s_num - base);

    SV_PutString(swizzle);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
