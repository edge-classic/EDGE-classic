//----------------------------------------------------------------------------
//  EDGE WAD Support Code
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
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------

#pragma once

#include <vector>

#include "dm_defs.h"
#include "epi_file.h"

class DataFile;
struct WadTextureResource
{
    // lump numbers, or -1 if nonexistent
    int palette  = -1;
    int pnames   = -1;
    int texture1 = -1;
    int texture2 = -1;
};

struct GameCheck
{
    // Friendly string for selector dialog box (if multiple games found)
    // TODO: Read EDGEGAME file/lump for custom friendly title
    const char *display_name;

    // game_base to set if this IWAD is used
    const char *base;

    // (usually) unique lumps to check for in a potential IWAD
    const char *unique_lumps[2];
};

extern const std::vector<GameCheck> game_checker;

int CheckLumpNumberForName(const char *name);
// Like above, but returns the data file index instead of the sortedlump index
int CheckDataFileIndexForName(const char *name);

int CheckGraphicLumpNumberForName(const char *name);
int CheckXGLLumpNumberForName(const char *name);
int CheckMapLumpNumberForName(const char *name);
int CheckPatchLumpNumberForName(const char *name);

// Unlike check, will FatalError if not present
int GetLumpNumberForName(const char *name);

int GetLumpLength(int lump);

uint8_t *LoadLumpIntoMemory(int lump, int *length = nullptr);
uint8_t *LoadLumpIntoMemory(const char *name, int *length = nullptr);

std::string LoadLumpAsString(int lump);
std::string LoadLumpAsString(const char *name);

bool        IsLumpIndexValid(int lump);
bool        VerifyLump(int lump, const char *name);
const char *GetLumpNameFromIndex(int lump);

epi::File *LoadLumpAsFile(int lump);
epi::File *LoadLumpAsFile(const char *name);

int               GetPaletteForLump(int lump);
int               FindFlatSequence(const char *start, const char *end, int *s_offset, int *e_offset);
std::vector<int> *GetFlatListForWAD(int file);
std::vector<int> *GetSpriteListForWAD(int file);
std::vector<int> *GetPatchListForWAD(int file);
void              GetTextureLumpsForWAD(int file, WadTextureResource *res);
void              ProcessTXHINamespaces(void);
int               GetDataFileIndexForLump(int lump);

// auxiliary functions to help us deal with when to use skyboxes
bool DisableStockSkybox(const char *ActualSky);

bool IsLumpInPwad(const char *name);

bool IsLumpInAnyWad(const char *name);

// Returns index into game_checker vector if valid game found, else -1
int CheckForUniqueGameLumps(epi::File *file);

void BuildXGLNodes(void);
#ifdef EDGE_CLASSIC
void ReadUMAPINFOLumps(void);
#endif

int GetKindForLump(int lump);

void CloseWADFile(DataFile *df);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
