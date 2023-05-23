//----------------------------------------------------------------------------
//  EDGE WAD Support Code
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
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------

#ifndef __W_WAD__
#define __W_WAD__

#include "dm_defs.h"

#include "file.h"


class wadtex_resource_c
{
public:
	wadtex_resource_c() : palette(-1), pnames(-1), texture1(-1), texture2(-1)
	{ }

	// lump numbers, or -1 if nonexistent
	int palette;
	int pnames;
	int texture1;
	int texture2;
};

int W_CheckNumForName(const char *name);
// Like above, but returns the data file index instead of the sortedlump index
int W_CheckFileNumForName(const char *name);

int W_CheckNumForName_GFX(const char *name);
int W_CheckNumForName_XGL(const char *name);
int W_CheckNumForName_MAP(const char *name);
int W_CheckNumForTexPatch(const char *name);


int W_GetNumForName(const char *name);

int W_LumpLength(int lump);

byte *W_LoadLump(int lump, int *length = NULL);
byte *W_LoadLump(const char *name, int *length = NULL);

std::string W_LoadString(int lump);
std::string W_LoadString(const char *name);

bool W_VerifyLump(int lump);
bool W_VerifyLumpName(int lump, const char *name);
const char *W_GetLumpName(int lump);

epi::file_c *W_OpenLump(int lump);
epi::file_c *W_OpenLump(const char *name);

int W_GetPaletteForLump(int lump);
int W_FindFlatSequence(const char *start, const char *end, int *s_offset, int *e_offset);
std::vector<int> * W_GetFlatList  (int file);
std::vector<int> * W_GetSpriteList(int file);
std::vector<int> * W_GetPatchList(int file);
void W_GetTextureLumps(int file, wadtex_resource_c *res);
void W_ProcessTX_HI(void);
int W_GetFileForLump(int lump);
void W_ShowLumps(int for_file, const char *match);

//auxiliary functions to help us deal with when to use skyboxes
int W_LoboFindSkyImage(int for_file, const char *match);
bool W_LoboDisableSkybox(const char *ActualSky);

bool W_IsLumpInPwad(const char *name);

bool W_IsLumpInAnyWad(const char *name);

std::string W_CheckForUniqueLumps(epi::file_c *file, int *score);

void W_BuildNodes(void);
void W_ReadUMAPINFOLumps(void);

int W_GetKindForLump(int lump);

#endif // __W_WAD__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
