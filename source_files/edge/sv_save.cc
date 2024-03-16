//----------------------------------------------------------------------------
//  EDGE New SaveGame Handling (Saving)
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
#include "epi.h"
#include "f_interm.h"
#include "g_game.h"
#include "i_system.h"
#include "m_math.h"
#include "m_misc.h"
#include "m_random.h"
#include "p_local.h"
#include "p_spec.h"
#include "r_state.h"
#include "sv_chunk.h"
#include "sv_main.h"
#include "w_wad.h"

void BeginSaveGameSave(void)
{
    LogDebug("SV_BeginSave...\n");

    ClearAllStaleReferences();
}

void FinishSaveGameSave(void)
{
    LogDebug("SV_FinishSave...\n");
}

void SaveGameStructSave(void *base, SaveStruct *info)
{
    SaveField *cur;
    char      *storage;
    int        offset;
    int        i;

    SavePushWriteChunk(info->marker);

    for (cur = info->fields; cur->type.kind != kSaveFieldInvalid; cur++)
    {
        // ignore read-only (fudging) fields
        if (!cur->field_put)
            continue;

        offset = cur->offset_pointer - info->dummy_base;

        storage = ((char *)base) + offset;

        for (i = 0; i < cur->count; i++)
        {
            switch (cur->type.kind)
            {
            case kSaveFieldStruct:
            case kSaveFieldIndex:
                (*cur->field_put)(storage, i, (char *)cur->type.name);
                break;

            default:
                (*cur->field_put)(storage, i, nullptr);
                break;
            }
        }
    }

    SavePopWriteChunk();
}

static void SV_SaveSTRU(SaveStruct *S)
{
    int        i, num;
    SaveField *F;

    // count number of fields
    for (num = 0; S->fields[num].type.kind != kSaveFieldInvalid; num++)
    { /* nothing here */
    }

    SaveChunkPutInteger(num);

    SaveChunkPutString(S->struct_name);
    SaveChunkPutString(S->marker);

    // write out the fields

    for (i = 0, F = S->fields; i < num; i++, F++)
    {
        SaveChunkPutByte((uint8_t)F->type.kind);
        SaveChunkPutByte((uint8_t)F->type.size);
        SaveChunkPutShort((uint16_t)F->count);
        SaveChunkPutString(F->field_name);

        if (F->type.kind == kSaveFieldStruct || F->type.kind == kSaveFieldIndex)
        {
            SaveChunkPutString(F->type.name);
        }
    }
}

static void SV_SaveARRY(SaveArray *A)
{
    int num_elem = (*A->count_elems)();

    SaveChunkPutInteger(num_elem);

    SaveChunkPutString(A->array_name);
    SaveChunkPutString(A->sdef->struct_name);
}

static void SV_SaveDATA(SaveArray *A)
{
    int num_elem = (*A->count_elems)();
    int i;

    SaveChunkPutString(A->array_name);

    for (i = 0; i < num_elem; i++)
    {
        sv_current_elem = (*A->get_elem)(i);

        EPI_ASSERT(sv_current_elem);

        SaveGameStructSave(sv_current_elem, A->sdef);
    }
}

void SaveAllSaveChunks(void)
{
    SaveStruct *stru;
    SaveArray  *arry;

    // Structure Area
    for (stru = sv_known_structs; stru; stru = stru->next)
    {
        if (!stru->define_me)
            continue;

        SavePushWriteChunk("Stru");
        SV_SaveSTRU(stru);
        SavePopWriteChunk();
    }

    // Array Area
    for (arry = sv_known_arrays; arry; arry = arry->next)
    {
        if (!arry->define_me)
            continue;

        SavePushWriteChunk("Arry");
        SV_SaveARRY(arry);
        SavePopWriteChunk();
    }

    // Data Area
    for (arry = sv_known_arrays; arry; arry = arry->next)
    {
        if (!arry->define_me)
            continue;

        SavePushWriteChunk("Data");
        SV_SaveDATA(arry);
        SavePopWriteChunk();
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
