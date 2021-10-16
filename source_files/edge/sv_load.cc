//----------------------------------------------------------------------------
//  EDGE New SaveGame Handling (Loading)
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

#include "dm_state.h"
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
#include "z_zone.h"


static savestruct_t *loaded_struct_list;
static savearray_t  *loaded_array_list;

bool sv_loading_hub;


//----------------------------------------------------------------------------
//
//  ADMININISTRATION
//

static void AddLoadedStruct(savestruct_t *S)
{
	S->next = loaded_struct_list;
	loaded_struct_list = S;
}

static void AddLoadedArray(savearray_t *A)
{
	A->next = loaded_array_list;
	loaded_array_list = A;
}

savestruct_t *SV_LookupLoadedStruct(const char *name)
{
	savestruct_t *S;

	for (S = loaded_struct_list; S; S = S->next)
		if (strcmp(S->struct_name, name) == 0)
			return S;

	// not found
	return NULL;
}

savearray_t *SV_LookupLoadedArray(const char *name)
{
	savearray_t *A;

	for (A = loaded_array_list; A; A = A->next)
		if (strcmp(A->array_name, name) == 0)
			return A;

	// not found
	return NULL;
}


//----------------------------------------------------------------------------
//
//  LOADING STUFF
//

void SV_BeginLoad(bool is_hub)
{
	sv_loading_hub = is_hub;

	/* Prepare main code for loading, e.g. initialise some lists */

	savestruct_t *S;
	savearray_t *A;

	L_WriteDebug("SV_BeginLoad...\n");

	loaded_struct_list = NULL;
	loaded_array_list  = NULL;

	// clear counterpart fields
	for (S=sv_known_structs; S; S=S->next)
		S->counterpart = NULL;

	for (A=sv_known_arrays; A; A=A->next)
		A->counterpart = NULL;
}

static void LoadFreeStruct(savestruct_t *S)
{
	SV_FreeString(S->struct_name);
	SV_FreeString(S->marker);

	delete[] S->fields;

	delete S;
}

static void LoadFreeArray(savearray_t *A)
{
	SV_FreeString(A->array_name);

	delete A;
}

void SV_FinishLoad(void)
{
	// Finalise all the arrays, and free some stuff after loading
	// has finished.

	L_WriteDebug("SV_FinishLoad...\n");

	while (loaded_struct_list)
	{
		savestruct_t *S = loaded_struct_list;
		loaded_struct_list = S->next;

		LoadFreeStruct(S);
	}

	while (loaded_array_list)
	{
		savearray_t *A = loaded_array_list;
		loaded_array_list = A->next;

		if (A->counterpart &&
			(!sv_loading_hub || A->counterpart->allow_hub))
		{
			(* A->counterpart->finalise_elems)();
		}

		LoadFreeArray(A);
	}
}

static savefield_t *StructFindField(savestruct_t *info, const char *name)
{
	savefield_t *F;

	for (F = info->fields; F->type.kind != SFKIND_Invalid; F++)
	{
		if (strcmp(name, F->field_name) == 0)
			return F;
	}

	return NULL;
}

static void StructSkipField(savefield_t *field)
{
	char marker[6];
	int i;

	switch (field->type.kind)
	{
	case SFKIND_Struct:
		SV_GetMarker(marker);
		//!!! compare marker with field->type.name
		SV_SkipReadChunk(marker);
		break;

	case SFKIND_String:
		SV_FreeString(SV_GetString());
		break;

	case SFKIND_Numeric:
	case SFKIND_Index:
		for (i=0; i < field->type.size; i++)
			SV_GetByte();
		break;

	default:
		I_Error("SV_LoadStruct: BAD TYPE IN FIELD.\n");
	}
}

bool SV_LoadStruct(void *base, savestruct_t *info)
{
	// the savestruct_t here is the "loaded" one.

	char marker[6];

	SV_GetMarker(marker);

	if (strcmp(marker, info->marker) != 0 || ! SV_PushReadChunk(marker))
		return false;

	for (savefield_t *F = info->fields; F->type.kind != SFKIND_Invalid; F++)
	{
		savefield_t *actual = F->known_field;

		// if this field no longer exists, ignore it
		if (! actual)
		{
			for (int i=0; i < F->count; i++)
				StructSkipField(F);
			continue;
		}

		SYS_ASSERT(actual->field_get);
		SYS_ASSERT(info->counterpart);

		int offset = actual->offset_p - info->counterpart->dummy_base;

		char *storage = ((char *) base) + offset;

		for (int i=0; i < F->count; i++)
		{
			// if there are extra elements in the savegame, ignore them
			if (i >= actual->count)
			{
				StructSkipField(F);
				continue;
			}
			switch (actual->type.kind)
			{
			case SFKIND_Struct:
			case SFKIND_Index:
				(* actual->field_get)(storage, i, (char *)actual->type.name);
				break;

			default:
				(* actual->field_get)(storage, i, NULL);
				break;
			}
		}
	}

	SV_PopReadChunk();

	return true;
}

static bool SV_LoadSTRU(void)
{
	savestruct_t *S = new savestruct_t;

	Z_Clear(S, savestruct_t, 1);

	int numfields = SV_GetInt();

	S->struct_name = SV_GetString();
	S->counterpart = SV_MainLookupStruct(S->struct_name);

	// make the counterparts refer to each other
	if (S->counterpart)
	{
		SYS_ASSERT(S->counterpart->counterpart == NULL);
		S->counterpart->counterpart = S;
	}

	S->marker = SV_GetString();

	if (strlen(S->marker) != 4)
		I_Error("LOADGAME: Corrupt savegame (STRU bad marker)\n");

	S->fields = new savefield_t[numfields+1];

	Z_Clear(S->fields, savefield_t, numfields+1);

	//
	// -- now load in all the fields --
	//

	int i;
	savefield_t *F;

	for (i=0, F=S->fields; i < numfields; i++, F++)
	{
		F->type.kind = (savefieldkind_e) SV_GetByte();
		F->type.size = SV_GetByte();
		F->count = SV_GetShort();
		F->field_name = SV_GetString();

		if (F->type.kind == SFKIND_Struct ||
			F->type.kind == SFKIND_Index)
		{
			F->type.name = SV_GetString();
		}

		F->known_field = NULL;

		if (S->counterpart)
			F->known_field = StructFindField(S->counterpart, F->field_name);

		// ??? compare names for STRUCT and INDEX
	}

	// terminate the array
	F->type.kind = SFKIND_Invalid;

	AddLoadedStruct(S);

	return true;
}

static bool SV_LoadARRY(void)
{
	savearray_t *A = new savearray_t;

	Z_Clear(A, savearray_t, 1);

	A->loaded_size = SV_GetInt();

	A->array_name = SV_GetString();
	A->counterpart = SV_MainLookupArray(A->array_name);

	// make the counterparts refer to each other
	if (A->counterpart)
	{
		SYS_ASSERT(A->counterpart->counterpart == NULL);
		A->counterpart->counterpart = A;
	}

	const char *struct_name = SV_GetString();

	A->sdef = SV_LookupLoadedStruct(struct_name);

	if (A->sdef == NULL)
		I_Error("LOADGAME: Coding Error ! (no STRU `%s' for ARRY)\n", struct_name);

	SV_FreeString(struct_name);

	// create array
	if (A->counterpart &&
		(!sv_loading_hub || A->counterpart->allow_hub))
	{
		(* A->counterpart->create_elems)(A->loaded_size);
	}

	AddLoadedArray(A);

	return true;
}

static bool SV_LoadDATA(void)
{
	const char *array_name = SV_GetString();

	savearray_t *A = SV_LookupLoadedArray(array_name);

	if (! A)
		I_Error("LOADGAME: Coding Error ! (no ARRY `%s' for DATA)\n", array_name);

	SV_FreeString(array_name);

	for (int i=0; i < A->loaded_size; i++)
	{
		//??? check error too ???
		if (SV_RemainingChunkSize() == 0)
			return false;

		if (A->counterpart &&
			(!sv_loading_hub || A->counterpart->allow_hub))
		{
			sv_current_elem = (* A->counterpart->get_elem)(i);

			if (! sv_current_elem)
				I_Error("SV_LoadDATA: FIXME: skip elems\n");

			if (! SV_LoadStruct(sv_current_elem, A->sdef))
				return false;
		}
		else
		{
			// SKIP THE WHOLE STRUCT
			char marker[6];
			SV_GetMarker(marker);

			if (! SV_SkipReadChunk(marker))
				return false;
		}
	}

	///  if (SV_RemainingChunkSize() != 0)   //???
	///    return false;

	return true;
}

bool SV_LoadEverything(void)
{
	bool result;

	for (;;)
	{
		if (SV_GetError() != 0)
			break;  /// FIXME: set error !!

		char marker[6];

		SV_GetMarker(marker);

		if (strcmp(marker, DATA_END_MARKER) == 0)
			break;

		// Structure Area
		if (strcmp(marker, "Stru") == 0)
		{
			SV_PushReadChunk("Stru");
			result = SV_LoadSTRU();
			result = SV_PopReadChunk() && result;

			if (! result)
				return false;

			continue;
		}

		// Array Area
		if (strcmp(marker, "Arry") == 0)
		{
			SV_PushReadChunk("Arry");
			result = SV_LoadARRY();
			result = SV_PopReadChunk() && result;

			if (! result)
				return false;

			continue;
		}

		// Data Area
		if (strcmp(marker, "Data") == 0)
		{
			SV_PushReadChunk("Data");
			result = SV_LoadDATA();
			result = SV_PopReadChunk() && result;

			if (! result)
				return false;

			continue;
		}

		I_Warning("LOADGAME: Unexpected top-level chunk [%s]\n", marker);

		if (! SV_SkipReadChunk(marker))
			return false;
	}

	return true;
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
