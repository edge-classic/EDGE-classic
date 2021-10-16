//----------------------------------------------------------------------------
//  EDGE New SaveGame Handling (Saving)
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2009  The EDGE Team.
// 
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
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

#include "i_defs.h"
#include "sv_chunk.h"

#include "dm_state.h"
#include "e_main.h"
#include "g_game.h"
#include "m_math.h"
#include "m_misc.h"
#include "m_random.h"
#include "p_local.h"
#include "p_spec.h"
#include "r_state.h"
#include "sv_chunk.h"
#include "sv_main.h"
#include "w_wad.h"
#include "f_interm.h"

void SV_BeginSave(void)
{
	L_WriteDebug("SV_BeginSave...\n");

	P_ClearAllStaleRefs();
}

void SV_FinishSave(void)
{
	L_WriteDebug("SV_FinishSave...\n");
}

void SV_SaveStruct(void *base, savestruct_t *info)
{
	savefield_t *cur;
	char *storage;
	int offset;
	int i;

	SV_PushWriteChunk(info->marker);

	for (cur=info->fields; cur->type.kind != SFKIND_Invalid; cur++)
	{
		// ignore read-only (fudging) fields
		if (! cur->field_put)
			continue;

		offset = cur->offset_p - info->dummy_base;

		storage = ((char *)base) + offset;

		for (i=0; i < cur->count; i++)
		{
			switch (cur->type.kind)
			{
				case SFKIND_Struct:
				case SFKIND_Index:
					(* cur->field_put)(storage, i, (char*)cur->type.name);
					break;

				default:
					(* cur->field_put)(storage, i, NULL);
					break;
			}
		}
	}

	SV_PopWriteChunk();
}

static void SV_SaveSTRU(savestruct_t *S)
{
	int i, num;
	savefield_t *F;

	// count number of fields
	for (num=0; S->fields[num].type.kind != SFKIND_Invalid; num++)
	{ /* nothing here */ }

	SV_PutInt(num);

	SV_PutString(S->struct_name);
	SV_PutString(S->marker);

	// write out the fields

	for (i=0, F=S->fields; i < num; i++, F++)
	{
		SV_PutByte((unsigned char) F->type.kind);
		SV_PutByte((unsigned char) F->type.size);
		SV_PutShort((unsigned short) F->count);
		SV_PutString(F->field_name);

		if (F->type.kind == SFKIND_Struct ||
				F->type.kind == SFKIND_Index)
		{
			SV_PutString(F->type.name);
		}
	}
}

static void SV_SaveARRY(savearray_t *A)
{
	int num_elem = (* A->count_elems)();

	SV_PutInt(num_elem);

	SV_PutString(A->array_name);
	SV_PutString(A->sdef->struct_name);
}

static void SV_SaveDATA(savearray_t *A)
{
	int num_elem = (* A->count_elems)();
	int i;

	SV_PutString(A->array_name);

	for (i=0; i < num_elem; i++)
	{
		sv_current_elem = (* A->get_elem)(i);

		SYS_ASSERT(sv_current_elem);

		SV_SaveStruct(sv_current_elem, A->sdef);
	}
}

void SV_SaveEverything(void)
{
	savestruct_t *stru;
	savearray_t  *arry;

	// Structure Area
	for (stru=sv_known_structs; stru; stru=stru->next)
	{
		if (! stru->define_me)
			continue;

		SV_PushWriteChunk("Stru");
		SV_SaveSTRU(stru);
		SV_PopWriteChunk();
	}

	// Array Area
	for (arry=sv_known_arrays; arry; arry=arry->next)
	{
		if (! arry->define_me)
			continue;

		SV_PushWriteChunk("Arry");
		SV_SaveARRY(arry);
		SV_PopWriteChunk();
	}

	// Data Area
	for (arry=sv_known_arrays; arry; arry=arry->next)
	{
		if (! arry->define_me)
			continue;

		SV_PushWriteChunk("Data");
		SV_SaveDATA(arry);
		SV_PopWriteChunk();
	}
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
