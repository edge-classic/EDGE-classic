//----------------------------------------------------------------------------
//  EDGE New SaveGame Handling (Loading)
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

#include "dm_state.h"
#include "e_main.h"
#include "f_interm.h"
#include "g_game.h"
#include "i_system.h"
#include "m_math.h"
#include "m_random.h"
#include "p_local.h"
#include "p_spec.h"
#include "r_state.h"
#include "sv_chunk.h"
#include "sv_main.h"
#include "w_wad.h"

static SaveStruct *loaded_struct_list;
static SaveArray  *loaded_array_list;

bool sv_loading_hub;

//----------------------------------------------------------------------------
//
//  ADMININISTRATION
//

static void AddLoadedStruct(SaveStruct *S)
{
    S->next            = loaded_struct_list;
    loaded_struct_list = S;
}

static void AddLoadedArray(SaveArray *A)
{
    A->next           = loaded_array_list;
    loaded_array_list = A;
}

SaveStruct *SV_LookupLoadedStruct(const char *name)
{
    SaveStruct *S;

    for (S = loaded_struct_list; S; S = S->next)
        if (strcmp(S->struct_name, name) == 0) return S;

    // not found
    return nullptr;
}

SaveArray *SV_LookupLoadedArray(const char *name)
{
    SaveArray *A;

    for (A = loaded_array_list; A; A = A->next)
        if (strcmp(A->array_name, name) == 0) return A;

    // not found
    return nullptr;
}

//----------------------------------------------------------------------------
//
//  LOADING STUFF
//

void BeginSaveGameLoad(bool is_hub)
{
    sv_loading_hub = is_hub;

    /* Prepare main code for loading, e.g. initialise some lists */

    SaveStruct *S;
    SaveArray  *A;

    LogDebug("SV_BeginLoad...\n");

    loaded_struct_list = nullptr;
    loaded_array_list  = nullptr;

    // clear counterpart fields
    for (S = sv_known_structs; S; S = S->next) S->counterpart = nullptr;

    for (A = sv_known_arrays; A; A = A->next) A->counterpart = nullptr;
}

static void LoadFreeStruct(SaveStruct *S)
{
    SaveChunkFreeString(S->struct_name);
    SaveChunkFreeString(S->marker);

    delete[] S->fields;

    delete S;
}

static void LoadFreeArray(SaveArray *A)
{
    SaveChunkFreeString(A->array_name);

    delete A;
}

void FinishSaveGameLoad(void)
{
    // Finalise all the arrays, and free some stuff after loading
    // has finished.

    LogDebug("SV_FinishLoad...\n");

    while (loaded_struct_list)
    {
        SaveStruct *S      = loaded_struct_list;
        loaded_struct_list = S->next;

        LoadFreeStruct(S);
    }

    while (loaded_array_list)
    {
        SaveArray *A      = loaded_array_list;
        loaded_array_list = A->next;

        if (A->counterpart && (!sv_loading_hub || A->counterpart->allow_hub))
        {
            (*A->counterpart->finalise_elems)();
        }

        LoadFreeArray(A);
    }
}

static SaveField *StructFindField(SaveStruct *info, const char *name)
{
    SaveField *F;

    for (F = info->fields; F->type.kind != kSaveFieldInvalid; F++)
    {
        if (strcmp(name, F->field_name) == 0) return F;
    }

    return nullptr;
}

static void StructSkipField(SaveField *field)
{
    char marker[6];
    int  i;

    switch (field->type.kind)
    {
        case kSaveFieldStruct:
            SaveChunkGetMarker(marker);
            //!!! compare marker with field->type.name
            SaveSkipReadChunk(marker);
            break;

        case kSaveFieldString:
            SaveChunkFreeString(SaveChunkGetString());
            break;

        case kSaveFieldNumeric:
        case kSaveFieldIndex:
            for (i = 0; i < field->type.size; i++) SaveChunkGetByte();
            break;

        default:
            FatalError("SV_LoadStruct: BAD TYPE IN FIELD.\n");
    }
}

bool SaveGameStructLoad(void *base, SaveStruct *info)
{
    // the savestruct_t here is the "loaded" one.

    char marker[6];

    SaveChunkGetMarker(marker);

    if (strcmp(marker, info->marker) != 0 || !SavePushReadChunk(marker))
        return false;

    for (SaveField *F = info->fields; F->type.kind != kSaveFieldInvalid; F++)
    {
        SaveField *actual = F->known_field;

        // if this field no longer exists, ignore it
        if (!actual)
        {
            for (int i = 0; i < F->count; i++) StructSkipField(F);
            continue;
        }

        SYS_ASSERT(actual->field_get);
        SYS_ASSERT(info->counterpart);

        int offset = actual->offset_pointer - info->counterpart->dummy_base;

        char *storage = ((char *)base) + offset;

        for (int i = 0; i < F->count; i++)
        {
            // if there are extra elements in the savegame, ignore them
            if (i >= actual->count)
            {
                StructSkipField(F);
                continue;
            }
            switch (actual->type.kind)
            {
                case kSaveFieldStruct:
                case kSaveFieldIndex:
                    (*actual->field_get)(storage, i, (char *)actual->type.name);
                    break;

                default:
                    (*actual->field_get)(storage, i, nullptr);
                    break;
            }
        }
    }

    SavePopReadChunk();

    return true;
}

static bool SV_LoadSTRU(void)
{
    SaveStruct *S = new SaveStruct;

    Z_Clear(S, SaveStruct, 1);

    int numfields = SaveChunkGetInteger();

    S->struct_name = SaveChunkGetString();
    S->counterpart = SaveStructLookup(S->struct_name);

    // make the counterparts refer to each other
    if (S->counterpart)
    {
        SYS_ASSERT(S->counterpart->counterpart == nullptr);
        S->counterpart->counterpart = S;
    }

    S->marker = SaveChunkGetString();

    if (strlen(S->marker) != 4)
        FatalError("LOADGAME: Corrupt savegame (STRU bad marker)\n");

    S->fields = new SaveField[numfields + 1];

    Z_Clear(S->fields, SaveField, numfields + 1);

    //
    // -- now load in all the fields --
    //

    int        i;
    SaveField *F;

    for (i = 0, F = S->fields; i < numfields; i++, F++)
    {
        F->type.kind  = (SaveFieldKind)SaveChunkGetByte();
        F->type.size  = SaveChunkGetByte();
        F->count      = SaveChunkGetShort();
        F->field_name = SaveChunkGetString();

        if (F->type.kind == kSaveFieldStruct || F->type.kind == kSaveFieldIndex)
        {
            F->type.name = SaveChunkGetString();
        }

        F->known_field = nullptr;

        if (S->counterpart)
            F->known_field = StructFindField(S->counterpart, F->field_name);

        // ??? compare names for STRUCT and INDEX
    }

    // terminate the array
    F->type.kind = kSaveFieldInvalid;

    AddLoadedStruct(S);

    return true;
}

static bool SV_LoadARRY(void)
{
    SaveArray *A = new SaveArray;

    Z_Clear(A, SaveArray, 1);

    A->loaded_size = SaveChunkGetInteger();

    A->array_name  = SaveChunkGetString();
    A->counterpart = SaveArrayLookup(A->array_name);

    // make the counterparts refer to each other
    if (A->counterpart)
    {
        SYS_ASSERT(A->counterpart->counterpart == nullptr);
        A->counterpart->counterpart = A;
    }

    const char *struct_name = SaveChunkGetString();

    A->sdef = SV_LookupLoadedStruct(struct_name);

    if (A->sdef == nullptr)
        FatalError("LOADGAME: Coding Error ! (no STRU `%s' for ARRY)\n",
                   struct_name);

    SaveChunkFreeString(struct_name);

    // create array
    if (A->counterpart && (!sv_loading_hub || A->counterpart->allow_hub))
    {
        (*A->counterpart->create_elems)(A->loaded_size);
    }

    AddLoadedArray(A);

    return true;
}

static bool SV_LoadDATA(void)
{
    const char *array_name = SaveChunkGetString();

    SaveArray *A = SV_LookupLoadedArray(array_name);

    if (!A)
        FatalError("LOADGAME: Coding Error ! (no ARRY `%s' for DATA)\n",
                   array_name);

    SaveChunkFreeString(array_name);

    for (int i = 0; i < A->loaded_size; i++)
    {
        //??? check error too ???
        if (SaveRemainingChunkSize() == 0) return false;

        if (A->counterpart && (!sv_loading_hub || A->counterpart->allow_hub))
        {
            sv_current_elem = (*A->counterpart->get_elem)(i);

            if (!sv_current_elem)
                FatalError("SV_LoadDATA: FIXME: skip elems\n");

            if (!SaveGameStructLoad(sv_current_elem, A->sdef)) return false;
        }
        else
        {
            // SKIP THE WHOLE STRUCT
            char marker[6];
            SaveChunkGetMarker(marker);

            if (!SaveSkipReadChunk(marker)) return false;
        }
    }

    ///  if (SV_RemainingChunkSize() != 0)   //???
    ///    return false;

    return true;
}

bool LoadAllSaveChunks(void)
{
    bool result;

    for (;;)
    {
        if (SaveGetError() != 0) break;  /// FIXME: set error !!

        char marker[6];

        SaveChunkGetMarker(marker);

        if (strcmp(marker, kDataEndMarker) == 0) break;

        // Structure Area
        if (strcmp(marker, "Stru") == 0)
        {
            SavePushReadChunk("Stru");
            result = SV_LoadSTRU();
            result = SavePopReadChunk() && result;

            if (!result) return false;

            continue;
        }

        // Array Area
        if (strcmp(marker, "Arry") == 0)
        {
            SavePushReadChunk("Arry");
            result = SV_LoadARRY();
            result = SavePopReadChunk() && result;

            if (!result) return false;

            continue;
        }

        // Data Area
        if (strcmp(marker, "Data") == 0)
        {
            SavePushReadChunk("Data");
            result = SV_LoadDATA();
            result = SavePopReadChunk() && result;

            if (!result) return false;

            continue;
        }

        LogWarning("LOADGAME: Unexpected top-level chunk [%s]\n", marker);

        if (!SaveSkipReadChunk(marker)) return false;
    }

    return true;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
