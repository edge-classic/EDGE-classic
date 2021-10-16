//----------------------------------------------------------------------------
//  EDGE New SaveGame Handling (Debugging)
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2008  The EDGE Team.
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
// TODO HERE:
//   + DumpGLOB: handle Push/PopChunk in outer, not inner.
//   - implement DumpWADS and DumpVIEW.
//

#include "i_defs.h"

#include "g_game.h"
#include "sv_chunk.h"
#include "sv_main.h"
#include "z_zone.h"


//----------------------------------------------------------------------------
//
//  DUMP GLOBALS
//

static bool GlobDumpVARI(void)
{
	const char *var_name;
	const char *var_data;

	if (! SV_PushReadChunk("Vari")) //!!!
		return false;

	var_name = SV_GetString();
	var_data = SV_GetString();

	if (! SV_PopReadChunk() || !var_name || !var_data)
	{
		SV_FreeString(var_name);
		SV_FreeString(var_data);

		return false;
	}

	L_WriteDebug("      Var: %s=%s\n", var_name, var_data);

	SV_FreeString(var_name);
	SV_FreeString(var_data);

	return true;
}

static bool GlobDumpWADS(void)
{
	L_WriteDebug("      Wad info\n");

	//!!! IMPLEMENT THIS
	return true;
}

static bool GlobDumpVIEW(void)
{
	L_WriteDebug("      Screenshot\n");

	//!!! IMPLEMENT THIS
	return true;
}

static bool SV_DumpGLOB(void)
{
	char marker[6];

	L_WriteDebug("   Global Area:\n");

	// read through all the chunks, picking the bits we need

	for (;;)
	{
		if (SV_GetError() != 0)
		{
			L_WriteDebug("   *  Unknown Error !\n");
			return false;
		}

		if (SV_RemainingChunkSize() < 4)
			break;

		SV_GetMarker(marker);

		if (strcmp(marker, "Vari") == 0)
		{
			GlobDumpVARI();
			continue;
		}
		if (strcmp(marker, "Wads") == 0)
		{
			GlobDumpWADS();
			continue;
		}
		if (strcmp(marker, "View") == 0)
		{
			GlobDumpVIEW();
			continue;
		}

		// skip unknown chunk
		L_WriteDebug("      Unknown GLOB chunk [%s]\n", marker);

		if (! SV_SkipReadChunk(marker))
		{
			L_WriteDebug("   *  Skipping unknown chunk failed !\n");
			return false;
		}
	}

	L_WriteDebug("   *  End of globals\n");

	return true;
}


//----------------------------------------------------------------------------
//
//  DUMP STRUCTURE / ARRAY / DATA
//

static bool SV_DumpSTRU(void)
{
	const char *struct_name;
	const char *marker;

	int i, fields;

	fields = SV_GetInt();
	struct_name = SV_GetString();
	marker = SV_GetString();

	L_WriteDebug("   Struct def: %s  Fields: %d  Marker: [%s]\n",
			struct_name, fields, marker);

	SV_FreeString(struct_name);
	SV_FreeString(marker);

	// -- now dump all the fields --

	for (i=0; i < fields; i++)
	{
		savefieldkind_e kind;
		int size, count;

		const char *field_name;
		const char *sub_type = NULL;
		char count_buf[40];

		kind = (savefieldkind_e) SV_GetByte();
		size = SV_GetByte();
		count = SV_GetShort();
		field_name = SV_GetString();

		if (kind == SFKIND_Struct ||
				kind == SFKIND_Index)
		{
			sub_type = SV_GetString();
		}

		if (count == 1)
			count_buf[0] = 0;
		else
			sprintf(count_buf, "[%d]", count);

		L_WriteDebug("      Field: %s%s  Kind: %s%s  Size: %d\n",
				field_name, count_buf,
				(kind == SFKIND_Numeric) ? "Numeric" :
				(kind == SFKIND_String) ? "String" :
				(kind == SFKIND_Index) ? "Index in " :
				(kind == SFKIND_Struct) ? "Struct " : "???",
				sub_type ? sub_type : "", size);

		SV_FreeString(field_name);
		SV_FreeString(sub_type);
	}

	return true;
}

static bool SV_DumpARRY(void)
{
	const char *array_name;
	const char *struct_name;

	int count;

	count = SV_GetInt();
	array_name  = SV_GetString();
	struct_name = SV_GetString();

	L_WriteDebug("   Array def: %s  Count: %d  Struct: %s\n",
			array_name, count, struct_name);

	SV_FreeString(array_name);
	SV_FreeString(struct_name);

	return true;
}

static bool SV_DumpDATA(void)
{
	const char *array_name = SV_GetString();

	L_WriteDebug("   Data for array %s  Size: %d\n", array_name,
			SV_RemainingChunkSize());

	SV_FreeString(array_name);

	return true;
}


//----------------------------------------------------------------------------

//
// SV_DumpSaveGame
//
// Dumps the contents of a savegame file to the debug file.  Very
// useful for debugging.
//
void SV_DumpSaveGame(int slot)
{
	char marker[6];
	int version;

	std::string fn(G_FileNameFromSlot(slot));

	L_WriteDebug("DUMPING SAVE GAME: %d  FILE: %s\n", slot, fn.c_str());

	if (! SV_OpenReadFile(fn.c_str()))
	{
		L_WriteDebug("*  Unable to open file !\n");
		return;
	}

	L_WriteDebug("   File opened OK.\n");

	if (! SV_VerifyHeader(&version))
	{
		L_WriteDebug("*  VerifyHeader failed !\n");
		SV_CloseReadFile();
		return;
	}

	L_WriteDebug("   Header OK.  Version: %x.%02x  PL: %x\n",
			(version >> 16) & 0xFF, (version >> 8) & 0xFF, version & 0xFF);

	if (! SV_VerifyContents())
	{
		L_WriteDebug("*  VerifyContents failed !\n");
		SV_CloseReadFile();
		return;
	}

	L_WriteDebug("   Body OK.\n");

	for (;;)
	{
		if (SV_GetError() != 0)
		{
			L_WriteDebug("   Unknown Error !\n");
			break;
		}

		SV_GetMarker(marker);

		if (strcmp(marker, DATA_END_MARKER) == 0)
		{
			L_WriteDebug("   End-of-Data marker found.\n");
			break;
		}

		// global area
		if (strcmp(marker, "Glob") == 0)
		{
			SV_PushReadChunk("Glob");

			if (! SV_DumpGLOB())
			{
				L_WriteDebug("   Error while dumping [GLOB]\n");
				break;
			}

			if (! SV_PopReadChunk())
			{
				L_WriteDebug("   Error popping [GLOB]\n");
				break;
			}

			continue;
		}

		// structure area
		if (strcmp(marker, "Stru") == 0)
		{
			SV_PushReadChunk("Stru");

			if (! SV_DumpSTRU())
			{
				L_WriteDebug("   Error while dumping [STRU]\n");
				break;
			}

			if (! SV_PopReadChunk())
			{
				L_WriteDebug("   Error popping [STRU]\n");
				break;
			}

			continue;
		}

		// array area
		if (strcmp(marker, "Arry") == 0)
		{
			SV_PushReadChunk("Arry");

			if (! SV_DumpARRY())
			{
				L_WriteDebug("   Error while dumping [ARRY]\n");
				break;
			}

			if (! SV_PopReadChunk())
			{
				L_WriteDebug("   Error popping [ARRY]\n");
				break;
			}

			continue;
		}

		// data area
		if (strcmp(marker, "Data") == 0)
		{
			SV_PushReadChunk("Data");

			if (! SV_DumpDATA())
			{
				L_WriteDebug("   Error while dumping [DATA]\n");
				break;
			}

			if (! SV_PopReadChunk())
			{
				L_WriteDebug("   Error popping [DATA]\n");
				break;
			}

			continue;
		}

		// skip unknown chunk
		L_WriteDebug("   Unknown top-level chunk [%s]\n", marker);

		if (! SV_SkipReadChunk(marker))
		{
			L_WriteDebug("   Skipping unknown chunk failed !\n");
			break;
		}
	}

	SV_CloseReadFile();

	L_WriteDebug("*  DUMP FINISHED\n");
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
