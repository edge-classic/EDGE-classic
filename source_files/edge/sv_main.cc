//----------------------------------------------------------------------------
//  EDGE New SaveGame Handling (Main)
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

#include "sv_main.h"

#include "dm_state.h"
#include "dstrings.h"
#include "e_main.h"
#include "epi.h"
#include "f_interm.h"
#include "filesystem.h"
#include "g_game.h"
#include "m_math.h"
#include "m_random.h"
#include "p_local.h"
#include "p_spec.h"
#include "r_state.h"
#include "str_util.h"
#include "sv_chunk.h"
#include "w_wad.h"

SaveStruct *sv_known_structs;
SaveArray  *sv_known_arrays;

// the current element of an array being read/written
void *sv_current_elem;

// sv_mobj.c
extern SaveStruct sv_struct_mobj;
extern SaveStruct sv_struct_spawnpoint;
extern SaveStruct sv_struct_iteminque;

extern SaveArray sv_array_mobj;
extern SaveArray sv_array_iteminque;

// sv_play.c
extern SaveStruct sv_struct_player;
extern SaveStruct sv_struct_playerweapon;
extern SaveStruct sv_struct_playerammo;
extern SaveStruct sv_struct_playerinv;
extern SaveStruct sv_struct_playercounter;
extern SaveStruct sv_struct_psprite;

extern SaveArray sv_array_player;

// sv_level.c
extern SaveStruct sv_struct_surface;
extern SaveStruct sv_struct_side;
extern SaveStruct sv_struct_line;
extern SaveStruct sv_struct_regprops;
extern SaveStruct sv_struct_exfloor;
extern SaveStruct sv_struct_sector;

extern SaveArray sv_array_side;
extern SaveArray sv_array_line;
extern SaveArray sv_array_exfloor;
extern SaveArray sv_array_sector;

// sv_misc.c
extern SaveStruct sv_struct_button;
extern SaveStruct sv_struct_light;
extern SaveStruct sv_struct_trigger;
extern SaveStruct sv_struct_drawtip;
extern SaveStruct sv_struct_plane_move;
extern SaveStruct sv_struct_slider_move;

extern SaveArray sv_array_button;
extern SaveArray sv_array_light;
extern SaveArray sv_array_trigger;
extern SaveArray sv_array_drawtip;
extern SaveArray sv_array_plane_move;
extern SaveArray sv_array_slider_move;

//----------------------------------------------------------------------------
//
//  GET ROUTINES
//

bool SaveGameGetInteger(void *storage, int index, void *extra)
{
    (void)extra;

    ((uint32_t *)storage)[index] = SaveChunkGetInteger();
    return true;
}

bool SaveGameGetAngle(void *storage, int index, void *extra)
{
    (void)extra;

    ((BAMAngle *)storage)[index] = SaveChunkGetAngle();
    return true;
}

bool SaveGameGetFloat(void *storage, int index, void *extra)
{
    (void)extra;

    ((float *)storage)[index] = SaveChunkGetFloat();
    return true;
}

bool SaveGameGetBoolean(void *storage, int index, void *extra)
{
    (void)extra;

    ((bool *)storage)[index] = SaveChunkGetInteger() ? true : false;
    return true;
}

bool SaveGameGetVec2(void *storage, int index, void *extra)
{
    (void)extra;

    ((HMM_Vec2 *)storage)[index].X = SaveChunkGetFloat();
    ((HMM_Vec2 *)storage)[index].Y = SaveChunkGetFloat();
    return true;
}

bool SaveGameGetVec3(void *storage, int index, void *extra)
{
    (void)extra;

    ((HMM_Vec3 *)storage)[index].X = SaveChunkGetFloat();
    ((HMM_Vec3 *)storage)[index].Y = SaveChunkGetFloat();
    ((HMM_Vec3 *)storage)[index].Z = SaveChunkGetFloat();
    return true;
}

//
// For backwards compatibility with old savegames, keep the mlook angle
// stored in the savegame file as a slope.  Because we forbid looking
// directly up and down, there is no problem with infinity.
//
bool SaveGameGetAngleFromSlope(void *storage, int index, void *extra)
{
    (void)extra;

    ((BAMAngle *)storage)[index] = epi::BAMFromATan(SaveChunkGetFloat());
    return true;
}

//----------------------------------------------------------------------------
//
//  COMMON PUT ROUTINES
//

void SaveGamePutInteger(void *storage, int index, void *extra)
{
    SaveChunkPutInteger(((uint32_t *)storage)[index]);
}

void SaveGamePutAngle(void *storage, int index, void *extra)
{
    SaveChunkPutAngle(((BAMAngle *)storage)[index]);
}

void SaveGamePutFloat(void *storage, int index, void *extra)
{
    SaveChunkPutFloat(((float *)storage)[index]);
}

void SaveGamePutBoolean(void *storage, int index, void *extra)
{
    SaveChunkPutInteger(((bool *)storage)[index] ? 1 : 0);
}

void SaveGamePutVec2(void *storage, int index, void *extra)
{
    SaveChunkPutFloat(((HMM_Vec2 *)storage)[index].X);
    SaveChunkPutFloat(((HMM_Vec2 *)storage)[index].Y);
}

void SaveGamePutVec3(void *storage, int index, void *extra)
{
    SaveChunkPutFloat(((HMM_Vec3 *)storage)[index].X);
    SaveChunkPutFloat(((HMM_Vec3 *)storage)[index].Y);
    SaveChunkPutFloat(((HMM_Vec3 *)storage)[index].Z);
}

void SaveGamePutAngleToSlope(void *storage, int index, void *extra)
{
    BAMAngle val = ((BAMAngle *)storage)[index];

    EPI_ASSERT(val < kBAMAngle90 || val > kBAMAngle270);

    SaveChunkPutFloat(epi::BAMTan(val));
}

//----------------------------------------------------------------------------
//
//  ADMININISTRATION
//

static void AddKnownStruct(SaveStruct *S)
{
    S->next          = sv_known_structs;
    sv_known_structs = S;
}

static void AddKnownArray(SaveArray *A)
{
    A->next         = sv_known_arrays;
    sv_known_arrays = A;
}

void SaveSystemInitialize(void)
{
    // One-time initialisation.  Sets up lists of known structures
    // and arrays.

    // sv_mobj.c
    AddKnownStruct(&sv_struct_mobj);
    AddKnownStruct(&sv_struct_spawnpoint);
    AddKnownStruct(&sv_struct_iteminque);

    AddKnownArray(&sv_array_mobj);
    AddKnownArray(&sv_array_iteminque);

    // sv_play.c
    AddKnownStruct(&sv_struct_player);
    AddKnownStruct(&sv_struct_playerweapon);
    AddKnownStruct(&sv_struct_playerammo);
    AddKnownStruct(&sv_struct_playerinv);
    AddKnownStruct(&sv_struct_playercounter);
    AddKnownStruct(&sv_struct_psprite);

    AddKnownArray(&sv_array_player);

    // sv_level.c
    AddKnownStruct(&sv_struct_surface);
    AddKnownStruct(&sv_struct_side);
    AddKnownStruct(&sv_struct_line);
    AddKnownStruct(&sv_struct_regprops);
    AddKnownStruct(&sv_struct_exfloor);
    AddKnownStruct(&sv_struct_sector);

    AddKnownArray(&sv_array_side);
    AddKnownArray(&sv_array_line);
    AddKnownArray(&sv_array_exfloor);
    AddKnownArray(&sv_array_sector);

    // sv_misc.c
    AddKnownStruct(&sv_struct_button);
    AddKnownStruct(&sv_struct_light);
    AddKnownStruct(&sv_struct_trigger);
    AddKnownStruct(&sv_struct_drawtip);
    AddKnownStruct(&sv_struct_plane_move);
    AddKnownStruct(&sv_struct_slider_move);

    AddKnownArray(&sv_array_button);
    AddKnownArray(&sv_array_light);
    AddKnownArray(&sv_array_trigger);
    AddKnownArray(&sv_array_drawtip);
    AddKnownArray(&sv_array_plane_move);
    AddKnownArray(&sv_array_slider_move);
}

SaveStruct *SaveStructLookup(const char *name)
{
    SaveStruct *cur;

    for (cur = sv_known_structs; cur; cur = cur->next)
        if (strcmp(cur->struct_name, name) == 0)
            return cur;

    // not found
    return nullptr;
}

SaveArray *SaveArrayLookup(const char *name)
{
    SaveArray *cur;

    for (cur = sv_known_arrays; cur; cur = cur->next)
        if (strcmp(cur->array_name, name) == 0)
            return cur;

    // not found
    return nullptr;
}

//----------------------------------------------------------------------------

const char *SaveSlotName(int slot)
{
    EPI_ASSERT(slot < 1000);

    static char buffer[256];

    sprintf(buffer, "slot%03d", slot);

    return buffer;
}

const char *SaveMapName(const MapDefinition *map)
{
    // ensure the name is LOWER CASE
    static char buffer[256];

    strcpy(buffer, map->name_.c_str());

    for (char *pos = buffer; *pos; pos++)
        *pos = epi::ToLowerASCII(*pos);

    return buffer;
}

std::string SaveFilename(const char *slot_name, const char *map_name)
{
    std::string temp(epi::StringFormat("%s/%s.%s", slot_name, map_name, kSaveGameExtension));

    return epi::PathAppend(save_directory, temp);
}

std::string SV_DirName(const char *slot_name)
{
    return epi::PathAppend(save_directory, slot_name);
}

void SaveClearSlot(const char *slot_name)
{
    std::string full_dir = SV_DirName(slot_name);

    // make sure the directory exists
    epi::MakeDirectory(full_dir);

    std::vector<epi::DirectoryEntry> fsd;

    if (!ReadDirectory(fsd, full_dir, "*.*"))
    {
        LogDebug("Failed to read directory: %s\n", full_dir.c_str());
        return;
    }

    LogDebug("SV_ClearSlot: removing %d files\n", (int)fsd.size());

    for (size_t i = 0; i < fsd.size(); i++)
    {
        if (fsd[i].is_dir)
            continue;
        std::string cur_file = epi::PathAppend(full_dir, epi::GetFilename(fsd[i].name));
        LogDebug("  Deleting %s\n", cur_file.c_str());

        epi::FileDelete(cur_file);
    }
}

void SaveCopySlot(const char *src_name, const char *dest_name)
{
    std::string src_dir  = SV_DirName(src_name);
    std::string dest_dir = SV_DirName(dest_name);

    std::vector<epi::DirectoryEntry> fsd;

    if (!ReadDirectory(fsd, src_dir, "*.*"))
    {
        FatalError("SV_CopySlot: failed to read dir: %s\n", src_dir.c_str());
        return;
    }

    LogDebug("SV_CopySlot: copying %d files\n", (int)fsd.size());

    for (size_t i = 0; i < fsd.size(); i++)
    {
        if (fsd[i].is_dir)
            continue;

        std::string fn        = epi::GetFilename(fsd[i].name);
        std::string src_file  = epi::PathAppend(src_dir, fn);
        std::string dest_file = epi::PathAppend(dest_dir, fn);

        LogDebug("  Copying %s --> %s\n", src_file.c_str(), dest_file.c_str());

        if (!epi::FileCopy(src_file, dest_file))
            FatalError("SV_CopySlot: failed to copy '%s' to '%s'\n", src_file.c_str(), dest_file.c_str());
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
