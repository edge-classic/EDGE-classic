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



#include "str_util.h"
#include "filesystem.h"

#include "dm_state.h"
#include "dstrings.h"
#include "e_main.h"
#include "g_game.h"
#include "f_interm.h"
#include "m_math.h"
#include "m_random.h"
#include "p_local.h"
#include "p_spec.h"
#include "r_state.h"
#include "sv_chunk.h"
#include "sv_main.h"
#include "w_wad.h"

savestruct_t *sv_known_structs;
savearray_t  *sv_known_arrays;

// the current element of an array being read/written
void *sv_current_elem;

// sv_mobj.c
extern savestruct_t sv_struct_mobj;
extern savestruct_t sv_struct_spawnpoint;
extern savestruct_t sv_struct_iteminque;

extern savearray_t sv_array_mobj;
extern savearray_t sv_array_iteminque;

// sv_play.c
extern savestruct_t sv_struct_player;
extern savestruct_t sv_struct_playerweapon;
extern savestruct_t sv_struct_playerammo;
extern savestruct_t sv_struct_playerinv;
extern savestruct_t sv_struct_playercounter;
extern savestruct_t sv_struct_psprite;

extern savearray_t sv_array_player;

// sv_level.c
extern savestruct_t sv_struct_surface;
extern savestruct_t sv_struct_side;
extern savestruct_t sv_struct_line;
extern savestruct_t sv_struct_regprops;
extern savestruct_t sv_struct_exfloor;
extern savestruct_t sv_struct_sector;

extern savearray_t sv_array_side;
extern savearray_t sv_array_line;
extern savearray_t sv_array_exfloor;
extern savearray_t sv_array_sector;

// sv_misc.c
extern savestruct_t sv_struct_button;
extern savestruct_t sv_struct_light;
extern savestruct_t sv_struct_trigger;
extern savestruct_t sv_struct_drawtip;
extern savestruct_t sv_struct_plane_move;
extern savestruct_t sv_struct_slider_move;

extern savearray_t sv_array_button;
extern savearray_t sv_array_light;
extern savearray_t sv_array_trigger;
extern savearray_t sv_array_drawtip;
extern savearray_t sv_array_plane_move;
extern savearray_t sv_array_slider_move;

//----------------------------------------------------------------------------
//
//  GET ROUTINES
//

bool SR_GetByte(void *storage, int index, void *extra)
{
    (void)extra;

    ((unsigned char *)storage)[index] = SV_GetByte();
    return true;
}

bool SR_GetShort(void *storage, int index, void *extra)
{
    (void)extra;

    ((unsigned short *)storage)[index] = SV_GetShort();
    return true;
}

bool SR_GetInt(void *storage, int index, void *extra)
{
    (void)extra;

    ((unsigned int *)storage)[index] = SV_GetInt();
    return true;
}

bool SR_GetAngle(void *storage, int index, void *extra)
{
    (void)extra;

    ((BAMAngle *)storage)[index] = SV_GetAngle();
    return true;
}

bool SR_GetFloat(void *storage, int index, void *extra)
{
    (void)extra;

    ((float *)storage)[index] = SV_GetFloat();
    return true;
}

bool SR_GetBoolean(void *storage, int index, void *extra)
{
    (void)extra;

    ((bool *)storage)[index] = SV_GetInt() ? true : false;
    return true;
}

bool SR_GetVec2(void *storage, int index, void *extra)
{
    (void)extra;

    ((HMM_Vec2 *)storage)[index].X = SV_GetFloat();
    ((HMM_Vec2 *)storage)[index].Y = SV_GetFloat();
    return true;
}

bool SR_GetVec3(void *storage, int index, void *extra)
{
    (void)extra;

    ((HMM_Vec3 *)storage)[index].X = SV_GetFloat();
    ((HMM_Vec3 *)storage)[index].Y = SV_GetFloat();
    ((HMM_Vec3 *)storage)[index].Z = SV_GetFloat();
    return true;
}

bool SR_GetFloatFromInt(void *storage, int index, void *extra)
{
    (void)extra;

    ((float *)storage)[index] = (float)SV_GetInt();
    return true;
}

//
// For backwards compatibility with old savegames, keep the mlook angle
// stored in the savegame file as a slope.  Because we forbid looking
// directly up and down, there is no problem with infinity.
//
bool SR_GetAngleFromSlope(void *storage, int index, void *extra)
{
    (void)extra;

    ((BAMAngle *)storage)[index] = epi::BAMFromATan(SV_GetFloat());
    return true;
}

//----------------------------------------------------------------------------
//
//  COMMON PUT ROUTINES
//

void SR_PutByte(void *storage, int index, void *extra)
{
    SV_PutByte(((unsigned char *)storage)[index]);
}

void SR_PutShort(void *storage, int index, void *extra)
{
    SV_PutShort(((unsigned short *)storage)[index]);
}

void SR_PutInt(void *storage, int index, void *extra)
{
    SV_PutInt(((unsigned int *)storage)[index]);
}

void SR_PutAngle(void *storage, int index, void *extra)
{
    SV_PutAngle(((BAMAngle *)storage)[index]);
}

void SR_PutFloat(void *storage, int index, void *extra)
{
    SV_PutFloat(((float *)storage)[index]);
}

void SR_PutBoolean(void *storage, int index, void *extra)
{
    SV_PutInt(((bool *)storage)[index] ? 1 : 0);
}

void SR_PutVec2(void *storage, int index, void *extra)
{
    SV_PutFloat(((HMM_Vec2 *)storage)[index].X);
    SV_PutFloat(((HMM_Vec2 *)storage)[index].Y);
}

void SR_PutVec3(void *storage, int index, void *extra)
{
    SV_PutFloat(((HMM_Vec3 *)storage)[index].X);
    SV_PutFloat(((HMM_Vec3 *)storage)[index].Y);
    SV_PutFloat(((HMM_Vec3 *)storage)[index].Z);
}

void SR_PutAngleToSlope(void *storage, int index, void *extra)
{
    BAMAngle val = ((BAMAngle *)storage)[index];

    SYS_ASSERT(val < kBAMAngle90 || val > kBAMAngle270);

    SV_PutFloat(epi::BAMTan(val));
}

//----------------------------------------------------------------------------
//
//  ADMININISTRATION
//

static void AddKnownStruct(savestruct_t *S)
{
    S->next          = sv_known_structs;
    sv_known_structs = S;
}

static void AddKnownArray(savearray_t *A)
{
    A->next         = sv_known_arrays;
    sv_known_arrays = A;
}

void SV_MainInit(void)
{
    // One-time initialisation.  Sets up lists of known structures
    // and arrays.

    SV_ChunkInit();

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

savestruct_t *SV_MainLookupStruct(const char *name)
{
    savestruct_t *cur;

    for (cur = sv_known_structs; cur; cur = cur->next)
        if (strcmp(cur->struct_name, name) == 0)
            return cur;

    // not found
    return nullptr;
}

savearray_t *SV_MainLookupArray(const char *name)
{
    savearray_t *cur;

    for (cur = sv_known_arrays; cur; cur = cur->next)
        if (strcmp(cur->array_name, name) == 0)
            return cur;

    // not found
    return nullptr;
}

//----------------------------------------------------------------------------
//
//  TEST CODE
//

#if 0
void SV_MainTestPrimitives(void)
{
	int i;
	int version;

	if (! SV_OpenWriteFile("savegame/prim.tst", 0x7654))
		EDGEError("SV_MainTestPrimitives: couldn't create output\n");

	SV_PutByte(0x00);
	SV_PutByte(0x55);
	SV_PutByte(0xAA);
	SV_PutByte(0xFF);

	SV_PutShort(0x0000);
	SV_PutShort(0x4567);
	SV_PutShort(0xCDEF);
	SV_PutShort(0xFFFF);

	SV_PutInt(0x00000000);
	SV_PutInt(0x11223344);
	SV_PutInt(0xbbccddee);
	SV_PutInt(0xffffffff);

	SV_PutAngle(kBAMAngle1);
	SV_PutAngle(kBAMAngle45);
	SV_PutAngle(kBAMAngle135);
	SV_PutAngle(kBAMAngle180);
	SV_PutAngle(kBAMAngle270);
	SV_PutAngle(kBAMAngle315);
	SV_PutAngle(0 - kBAMAngle1);

	SV_PutFloat(0.0f);
	SV_PutFloat(0.001f);   SV_PutFloat(-0.001f);
	SV_PutFloat(0.1f);     SV_PutFloat(-0.1f);
	SV_PutFloat(0.25f);    SV_PutFloat(-0.25f);
	SV_PutFloat(1.0f);     SV_PutFloat(-1.0f);
	SV_PutFloat(2.0f);     SV_PutFloat(-2.0f);
	SV_PutFloat(3.1416f);  SV_PutFloat(-3.1416f);
	SV_PutFloat(1000.0f);  SV_PutFloat(-1000.0f);
	SV_PutFloat(1234567890.0f);  
	SV_PutFloat(-1234567890.0f);

	SV_PutString(nullptr);
	SV_PutString("");
	SV_PutString("A");
	SV_PutString("123");
	SV_PutString("HELLO world !");

	SV_PutMarker("ABCD");
	SV_PutMarker("xyz3");

	SV_CloseWriteFile();

	// ------------------------------------------------------------ //

	if (! SV_OpenReadFile("savegame/prim.tst"))
		EDGEError("SV_MainTestPrimitives: couldn't open input\n");

	if (! SV_VerifyHeader(&version))
		EDGEError("SV_MainTestPrimitives: couldn't open input\n");

	EDGEDebugf("TEST HEADER: version=0x%04x\n", version);

	for (i=0; i < 4; i++)
	{
		unsigned int val = SV_GetByte();
		EDGEDebugf("TEST BYTE: 0x%02x %d\n", val, (char) val);
	}

	for (i=0; i < 4; i++)
	{
		unsigned int val = SV_GetShort();
		EDGEDebugf("TEST SHORT: 0x%02x %d\n", val, (short) val);
	}

	for (i=0; i < 4; i++)
	{
		unsigned int val = SV_GetInt();
		EDGEDebugf("TEST INT: 0x%02x %d\n", val, (int) val);
	}

	for (i=0; i < 7; i++)
	{
		BAMAngle val = SV_GetAngle();
		EDGEDebugf("TEST ANGLE: 0x%08x = %1.6f\n", (unsigned int) val,
				ANG_2_FLOAT(val));
	}

	for (i=0; i < 17; i++)
	{
		float val = SV_GetFloat();
		EDGEDebugf("TEST FLOAT: %1.6f\n", val);
	}

	for (i=0; i < 5; i++)
	{
		const char *val = SV_GetString();
		EDGEDebugf("TEST STRING: [%s]\n", val ? val : "--nullptr--");
		SV_FreeString(val);
	}

	for (i=0; i < 2; i++)
	{
		char val[6];
		SV_GetMarker(val);
		EDGEDebugf("TEST MARKER: [%s]\n", val);
	}

	SV_CloseReadFile();
}

#endif // TEST CODE

//----------------------------------------------------------------------------

const char *SV_SlotName(int slot)
{
    SYS_ASSERT(slot < 1000);

    static char buffer[256];

    sprintf(buffer, "slot%03d", slot);

    return buffer;
}

const char *SV_MapName(const MapDefinition *map)
{
    // ensure the name is LOWER CASE
    static char buffer[256];

    strcpy(buffer, map->name_.c_str());

    for (char *pos = buffer; *pos; pos++)
        *pos = epi::ToLowerASCII(*pos);

    return buffer;
}

std::string SV_FileName(const char *slot_name, const char *map_name)
{
    std::string temp(epi::StringFormat("%s/%s.%s", slot_name, map_name, kSaveGameExtension));

    return epi::PathAppend(save_dir, temp);
}

std::string SV_DirName(const char *slot_name)
{
    return epi::PathAppend(save_dir, slot_name);
}

void SV_ClearSlot(const char *slot_name)
{
    std::string full_dir = SV_DirName(slot_name);

    // make sure the directory exists
    epi::MakeDirectory(full_dir);

    std::vector<epi::DirectoryEntry> fsd;

    if (!ReadDirectory(fsd, full_dir, "*.*"))
    {
        EDGEDebugf("Failed to read directory: %s\n", full_dir.c_str());
        return;
    }

    EDGEDebugf("SV_ClearSlot: removing %d files\n", (int)fsd.size());

    for (size_t i = 0; i < fsd.size(); i++)
    {
        if (fsd[i].is_dir)
            continue;
        std::string cur_file = epi::PathAppend(full_dir, epi::GetFilename(fsd[i].name));
        EDGEDebugf("  Deleting %s\n", cur_file.c_str());

        epi::FileDelete(cur_file);
    }
}

void SV_CopySlot(const char *src_name, const char *dest_name)
{
    std::string src_dir  = SV_DirName(src_name);
    std::string dest_dir = SV_DirName(dest_name);

    std::vector<epi::DirectoryEntry> fsd;

    if (!ReadDirectory(fsd, src_dir, "*.*"))
    {
        EDGEError("SV_CopySlot: failed to read dir: %s\n", src_dir.c_str());
        return;
    }

    EDGEDebugf("SV_CopySlot: copying %d files\n", (int)fsd.size());

    for (size_t i = 0; i < fsd.size(); i++)
    {
        if (fsd[i].is_dir)
            continue;

        std::string fn = epi::GetFilename(fsd[i].name);
        std::string src_file  = epi::PathAppend(src_dir, fn);
        std::string dest_file = epi::PathAppend(dest_dir, fn);

        EDGEDebugf("  Copying %s --> %s\n", src_file.c_str(), dest_file.c_str());

        if (!epi::FileCopy(src_file, dest_file))
            EDGEError("SV_CopySlot: failed to copy '%s' to '%s'\n", src_file.c_str(),
                    dest_file.c_str());
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
