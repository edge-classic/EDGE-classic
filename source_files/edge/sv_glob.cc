//----------------------------------------------------------------------------
//  EDGE New SaveGame Handling (Globals)
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
// TODO HERE:
//   + implement ReadWADS and ReadVIEW.
//

#include <limits.h>
#include <stddef.h>

#include "epi.h"
#include "sv_chunk.h"
#include "sv_main.h"

// forward decls:
static void SaveGlobalGetInteger(const char *info, void *storage);
static void SaveGlobalGetString(const char *info, void *storage);
static void SaveGlobalGetCheckCRC(const char *info, void *storage);
static void SaveGlobalGetLevelFlags(const char *info, void *storage);
static void SaveGlobalGetImage(const char *info, void *storage);

static const char *SaveGlobalPutInteger(void *storage);
static const char *SaveGlobalPutString(void *storage);
static const char *SaveGlobalPutCheckCRC(void *storage);
static const char *SaveGlobalPutLevelFlags(void *storage);
static const char *SaveGlobalPutImage(void *storage);

static SaveGlobals *current_global = nullptr;

struct GlobalCommand
{
    // global name
    const char *name;

    // parse function.  `storage' is where the data should go (for
    // routines that don't modify the current_global structure directly).
    void (*parse_function)(const char *info, void *storage);

    // stringify function.  Return string must be freed.
    const char *(*stringify_function)(void *storage);

    // field offset
    size_t pointer_offset;
};

static const GlobalCommand global_commands[] = {
    {"GAME", SaveGlobalGetString, SaveGlobalPutString, offsetof(SaveGlobals, game)},
    {"LEVEL", SaveGlobalGetString, SaveGlobalPutString, offsetof(SaveGlobals, level)},
    {"FLAGS", SaveGlobalGetLevelFlags, SaveGlobalPutLevelFlags, offsetof(SaveGlobals, flags)},
    {"HUB_TAG", SaveGlobalGetInteger, SaveGlobalPutInteger, offsetof(SaveGlobals, hub_tag)},
    {"HUB_FIRST", SaveGlobalGetString, SaveGlobalPutString, offsetof(SaveGlobals, hub_first)},

    {"GRAVITY", SaveGlobalGetInteger, SaveGlobalPutInteger, offsetof(SaveGlobals, flags.menu_gravity_factor)},
    {"LEVEL_TIME", SaveGlobalGetInteger, SaveGlobalPutInteger, offsetof(SaveGlobals, level_time)},
    {"EXIT_TIME", SaveGlobalGetInteger, SaveGlobalPutInteger, offsetof(SaveGlobals, exit_time)},
    {"P_RANDOM", SaveGlobalGetInteger, SaveGlobalPutInteger, offsetof(SaveGlobals, p_random)},

    {"TOTAL_KILLS", SaveGlobalGetInteger, SaveGlobalPutInteger, offsetof(SaveGlobals, total_kills)},
    {"TOTAL_ITEMS", SaveGlobalGetInteger, SaveGlobalPutInteger, offsetof(SaveGlobals, total_items)},
    {"TOTAL_SECRETS", SaveGlobalGetInteger, SaveGlobalPutInteger, offsetof(SaveGlobals, total_secrets)},
    {"CONSOLE_PLAYER", SaveGlobalGetInteger, SaveGlobalPutInteger, offsetof(SaveGlobals, console_player)},
    {"SKILL", SaveGlobalGetInteger, SaveGlobalPutInteger, offsetof(SaveGlobals, skill)},
    {"NETGAME", SaveGlobalGetInteger, SaveGlobalPutInteger, offsetof(SaveGlobals, netgame)},
    {"SKY_IMAGE", SaveGlobalGetImage, SaveGlobalPutImage, offsetof(SaveGlobals, sky_image)},

    {"DESCRIPTION", SaveGlobalGetString, SaveGlobalPutString, offsetof(SaveGlobals, description)},
    {"DESC_DATE", SaveGlobalGetString, SaveGlobalPutString, offsetof(SaveGlobals, desc_date)},

    {"MAPSECTOR", SaveGlobalGetCheckCRC, SaveGlobalPutCheckCRC, offsetof(SaveGlobals, mapsector)},
    {"MAPLINE", SaveGlobalGetCheckCRC, SaveGlobalPutCheckCRC, offsetof(SaveGlobals, mapline)},
    {"MAPTHING", SaveGlobalGetCheckCRC, SaveGlobalPutCheckCRC, offsetof(SaveGlobals, mapthing)},

    {"RSCRIPT", SaveGlobalGetCheckCRC, SaveGlobalPutCheckCRC, offsetof(SaveGlobals, rscript)},
    {"DDFATK", SaveGlobalGetCheckCRC, SaveGlobalPutCheckCRC, offsetof(SaveGlobals, ddfatk)},
    {"DDFGAME", SaveGlobalGetCheckCRC, SaveGlobalPutCheckCRC, offsetof(SaveGlobals, ddfgame)},
    {"DDFLEVL", SaveGlobalGetCheckCRC, SaveGlobalPutCheckCRC, offsetof(SaveGlobals, ddflevl)},
    {"DDFLINE", SaveGlobalGetCheckCRC, SaveGlobalPutCheckCRC, offsetof(SaveGlobals, ddfline)},
    {"DDFSECT", SaveGlobalGetCheckCRC, SaveGlobalPutCheckCRC, offsetof(SaveGlobals, ddfsect)},
    {"DDFMOBJ", SaveGlobalGetCheckCRC, SaveGlobalPutCheckCRC, offsetof(SaveGlobals, ddfmobj)},
    {"DDFWEAP", SaveGlobalGetCheckCRC, SaveGlobalPutCheckCRC, offsetof(SaveGlobals, ddfweap)},

    {nullptr, nullptr, nullptr, 0}};

//----------------------------------------------------------------------------
//
//  PARSERS
//

static void SaveGlobalGetInteger(const char *info, void *storage)
{
    int *dest = (int *)storage;

    EPI_ASSERT(info && storage);

    *dest = strtol(info, nullptr, 0);
}

static void SaveGlobalGetString(const char *info, void *storage)
{
    char **dest = (char **)storage;

    EPI_ASSERT(info && storage);

    // free any previous string
    SaveChunkFreeString(*dest);

    if (info[0] == 0)
        *dest = nullptr;
    else
        *dest = (char *)SaveChunkCopyString(info);
}

static void SaveGlobalGetCheckCRC(const char *info, void *storage)
{
    CrcCheck *dest = (CrcCheck *)storage;

    EPI_ASSERT(info && storage);

    sscanf(info, "%d %u", &dest->count, &dest->crc);
}

static void SaveGlobalGetLevelFlags(const char *info, void *storage)
{
    GameFlags *dest = (GameFlags *)storage;
    int        flags;

    EPI_ASSERT(info && storage);

    flags = strtol(info, nullptr, 0);

    EPI_CLEAR_MEMORY(dest, GameFlags, 1);

    dest->jump               = (flags & kMapFlagJumping) ? true : false;
    dest->crouch             = (flags & kMapFlagCrouching) ? true : false;
    dest->mouselook          = (flags & kMapFlagMlook) ? true : false;
    dest->items_respawn      = (flags & kMapFlagItemRespawn) ? true : false;
    dest->fast_monsters      = (flags & kMapFlagFastParm) ? true : false;
    dest->true_3d_gameplay   = (flags & kMapFlagTrue3D) ? true : false;
    dest->more_blood         = (flags & kMapFlagMoreBlood) ? true : false;
    dest->cheats             = (flags & kMapFlagCheats) ? true : false;
    dest->enemies_respawn    = (flags & kMapFlagRespawn) ? true : false;
    dest->enemy_respawn_mode = (flags & kMapFlagResRespawn) ? true : false;
    dest->have_extra         = (flags & kMapFlagExtras) ? true : false;
    dest->limit_zoom         = (flags & kMapFlagLimitZoom) ? true : false;
    dest->kicking            = (flags & kMapFlagKicking) ? true : false;
    dest->weapon_switch      = (flags & kMapFlagWeaponSwitch) ? true : false;
    dest->pass_missile       = (flags & kMapFlagPassMissile) ? true : false;
    dest->team_damage        = (flags & kMapFlagTeamDamage) ? true : false;
    dest->autoaim = kAutoAimOff;
    if (flags & kMapFlagAutoAimVertical) 
        dest->autoaim = kAutoAimVertical;
    if (flags & kMapFlagAutoAimVerticalSnap) 
        dest->autoaim = kAutoAimVerticalSnap;
    if (flags & kMapFlagAutoAimFull)
        dest->autoaim = kAutoAimFull;
    if (flags & kMapFlagAutoAimFullSnap)
        dest->autoaim = kAutoAimFullSnap;
}

static void SaveGlobalGetImage(const char *info, void *storage)
{
    // based on SaveGameLevelGetImage...

    const Image **dest = (const Image **)storage;

    EPI_ASSERT(info && storage);

    if (info[0] == 0)
    {
        (*dest) = nullptr;
        return;
    }

    if (info[1] != ':')
        LogWarning("SaveGlobalGetImage: invalid image string `%s'\n", info);

    (*dest) = ImageParseSaveString(info[0], info + 2);
}

//----------------------------------------------------------------------------
//
//  STRINGIFIERS
//

static const char *SaveGlobalPutInteger(void *storage)
{
    int *src = (int *)storage;
    char buffer[40];

    EPI_ASSERT(storage);

    sprintf(buffer, "%d", *src);

    return SaveChunkCopyString(buffer);
}

static const char *SaveGlobalPutString(void *storage)
{
    char **src = (char **)storage;

    EPI_ASSERT(storage);

    if (*src == nullptr)
        return SaveChunkCopyString("");

    return SaveChunkCopyString(*src);
}

static const char *SaveGlobalPutCheckCRC(void *storage)
{
    CrcCheck *src = (CrcCheck *)storage;
    char      buffer[80];

    EPI_ASSERT(storage);

    sprintf(buffer, "%d %u", src->count, src->crc);

    return SaveChunkCopyString(buffer);
}

static const char *SaveGlobalPutLevelFlags(void *storage)
{
    GameFlags *src = (GameFlags *)storage;
    int        flags;

    EPI_ASSERT(storage);

    flags = 0;

    if (src->jump)
        flags |= kMapFlagJumping;
    if (src->crouch)
        flags |= kMapFlagCrouching;
    if (src->mouselook)
        flags |= kMapFlagMlook;
    if (src->items_respawn)
        flags |= kMapFlagItemRespawn;
    if (src->fast_monsters)
        flags |= kMapFlagFastParm;
    if (src->true_3d_gameplay)
        flags |= kMapFlagTrue3D;
    if (src->more_blood)
        flags |= kMapFlagMoreBlood;
    if (src->cheats)
        flags |= kMapFlagCheats;
    if (src->enemies_respawn)
        flags |= kMapFlagRespawn;
    if (src->enemy_respawn_mode)
        flags |= kMapFlagResRespawn;
    if (src->have_extra)
        flags |= kMapFlagExtras;
    if (src->limit_zoom)
        flags |= kMapFlagLimitZoom;
    if (src->kicking)
        flags |= kMapFlagKicking;
    if (src->weapon_switch)
        flags |= kMapFlagWeaponSwitch;
    if (src->pass_missile)
        flags |= kMapFlagPassMissile;
    if (src->team_damage)
        flags |= kMapFlagTeamDamage;
    if (src->autoaim != kAutoAimOff)
    {
        if (src->autoaim == kAutoAimVertical)
            flags |= kMapFlagAutoAimVertical;
        else if (src->autoaim == kAutoAimVerticalSnap)
            flags |= kMapFlagAutoAimVerticalSnap;
        else if (src->autoaim == kAutoAimFull)
            flags |= kMapFlagAutoAimFull;
        else if (src->autoaim == kAutoAimFullSnap)
            flags |= kMapFlagAutoAimFullSnap;
    }

    return SaveGlobalPutInteger(&flags);
}

static const char *SaveGlobalPutImage(void *storage)
{
    // based on SaveGameLevelPutImage...

    const Image **src = (const Image **)storage;
    char          buffer[64];

    EPI_ASSERT(storage);

    if (*src == nullptr)
        return SaveChunkCopyString("");

    ImageMakeSaveString(*src, buffer, buffer + 2);
    buffer[1] = ':';

    return SaveChunkCopyString(buffer);
}

//----------------------------------------------------------------------------
//
//  MISCELLANY
//

SaveGlobals *SaveGlobalsNew(void)
{
    SaveGlobals *globs;

    globs = new SaveGlobals;

    EPI_CLEAR_MEMORY(globs, SaveGlobals, 1);

    globs->exit_time = INT_MAX;

    return globs;
}

void SaveGlobalsFree(SaveGlobals *globs)
{
    SaveChunkFreeString(globs->game);
    SaveChunkFreeString(globs->level);
    SaveChunkFreeString(globs->hub_first);
    SaveChunkFreeString(globs->description);
    SaveChunkFreeString(globs->desc_date);

    if (globs->wad_names)
        delete[] globs->wad_names;

    delete globs;
}

//----------------------------------------------------------------------------
//
//  LOADING GLOBALS
//

static bool GlobalReadVariable(SaveGlobals *globs)
{
    const char *var_name;
    const char *var_data;

    int   i;
    void *storage;

    if (!SavePushReadChunk("Vari"))
        return false;

    var_name = SaveChunkGetString();
    var_data = SaveChunkGetString();

    if (!SavePopReadChunk() || !var_name || !var_data)
    {
        SaveChunkFreeString(var_name);
        SaveChunkFreeString(var_data);

        return false;
    }

    // find variable in list
    for (i = 0; global_commands[i].name; i++)
    {
        if (strcmp(global_commands[i].name, var_name) == 0)
            break;
    }

    if (global_commands[i].name)
    {
        // found it, so parse it
        storage = ((char *)globs) + global_commands[i].pointer_offset;

        (*global_commands[i].parse_function)(var_data, storage);
    }
    else
    {
        LogDebug("GlobalReadVariable: unknown global: %s\n", var_name);
    }

    SaveChunkFreeString(var_name);
    SaveChunkFreeString(var_data);

    return true;
}

static bool GlobalReadWads(SaveGlobals *glob)
{
    if (!SavePushReadChunk("Wads"))
        return false;

    //!!! IMPLEMENT THIS

    SavePopReadChunk();
    return true;
}

SaveGlobals *SaveGlobalsLoad(void)
{
    char marker[6];

    SaveGlobals *globs;

    SaveChunkGetMarker(marker);

    if (strcmp(marker, "Glob") != 0 || !SavePushReadChunk("Glob"))
        return nullptr;

    current_global = globs = SaveGlobalsNew();

    // read through all the chunks, picking the bits we need

    for (;;)
    {
        if (SaveGetError() != 0)
            break; /// set error !!

        if (SaveRemainingChunkSize() == 0)
            break;

        SaveChunkGetMarker(marker);

        if (strcmp(marker, "Vari") == 0)
        {
            GlobalReadVariable(globs);
            continue;
        }
        if (strcmp(marker, "Wads") == 0)
        {
            GlobalReadWads(globs);
            continue;
        }

        // skip chunk
        LogWarning("LOADGAME: Unknown GLOB chunk [%s]\n", marker);

        if (!SaveSkipReadChunk(marker))
            break;
    }

    SavePopReadChunk(); /// check err

    return globs;
}

//----------------------------------------------------------------------------
//
//  SAVING GLOBALS
//

static void GlobalWriteVariables(SaveGlobals *globs)
{
    int i;

    for (i = 0; global_commands[i].name; i++)
    {
        void *storage = ((char *)globs) + global_commands[i].pointer_offset;

        const char *data = (*global_commands[i].stringify_function)(storage);
        EPI_ASSERT(data);

        SavePushWriteChunk("Vari");
        SaveChunkPutString(global_commands[i].name);
        SaveChunkPutString(data);
        SavePopWriteChunk();

        SaveChunkFreeString(data);
    }
}

static void GlobalWriteWads(SaveGlobals *globs)
{
    int i;

    if (!globs->wad_names)
        return;

    EPI_ASSERT(globs->wad_num > 0);

    SavePushWriteChunk("Wads");
    SaveChunkPutInteger(globs->wad_num);

    for (i = 0; i < globs->wad_num; i++)
        SaveChunkPutString(globs->wad_names[i]);

    SavePopWriteChunk();
}

void SaveGlobalsSave(SaveGlobals *globs)
{
    current_global = globs;

    SavePushWriteChunk("Glob");

    GlobalWriteVariables(globs);
    GlobalWriteWads(globs);

    // all done
    SavePopWriteChunk();
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
