//----------------------------------------------------------------------------
//  EDGE file handling
//----------------------------------------------------------------------------
//
//  Copyright (c) 2022-2024 The EDGE Team.
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

#ifndef __W_FILES__
#define __W_FILES__

#include "dm_defs.h"

// EPI
#include "file.h"

typedef enum
{
    FLKIND_IWad = 0, // iwad file
    FLKIND_PWad,     // normal .wad file
    FLKIND_EWad,     // edge wad, priority loading
    FLKIND_XWad,     // ajbsp node wad

    FLKIND_Folder,   // a folder somewhere
    FLKIND_EFolder,  // edge folder, priority loading
    FLKIND_EPK,      // edge package (.epk)
    FLKIND_EEPK,     // edge epks, priority loading (same extension as epk)
    FLKIND_PackWAD,  // WADs within pack files; should only be used for maps
    FLKIND_IPK,      // standalone game EPK (same extension as epk)
    FLKIND_IFolder,  // standalone game folder
    FLKIND_IPackWAD, // IWADs within pack files :/

    FLKIND_DDF, // .ddf or .ldf file
    FLKIND_RTS, // .rts script  file
    FLKIND_Deh  // .deh or .bex file
} filekind_e;

class wad_file_c;
class pack_file_c;

class data_file_c
{
  public:
    // full name of file
    std::string name;

    // type of file (FLKIND_XXX)
    filekind_e kind;

    // file object   [ TODO review when active ]
    epi::File *file;

    // for FLKIND_IWad, PWad, EWad, GWad.
    wad_file_c *wad;

    // for FLKIND_PK3
    pack_file_c *pack;

  public:
    data_file_c(std::string _name, filekind_e _kind);
    ~data_file_c();
};

extern std::vector<data_file_c *> data_files;

size_t W_AddFilename(std::string file, filekind_e kind);
int    W_GetNumFiles();
void   W_ShowFiles();

void   W_ProcessMultipleFiles();
size_t W_AddPending(std::string file, filekind_e kind);
void   ProcessFile(data_file_c *df);

epi::File *W_OpenPackFile(const std::string &name);

void W_DoPackSubstitutions(void);

uint8_t *W_OpenPackOrLumpInMemory(const std::string &name, const std::vector<std::string> &extensions, int *length);

int W_CheckPackForName(const std::string &name);

#endif // __W_FILES__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
