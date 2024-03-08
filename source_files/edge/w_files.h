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

#pragma once

#include <string>
#include <vector>

#include "dm_defs.h"
#include "file.h"

enum FileKind
{
    kFileKindIWad = 0,  // iwad file
    kFileKindPWad,      // normal .wad file
    kFileKindEWad,      // edge wad, priority loading
    kFileKindXWad,      // ajbsp node wad

    kFileKindFolder,    // a folder somewhere
    kFileKindEFolder,   // edge folder, priority loading
    kFileKindEpk,       // edge package (.epk)
    kFileKindEEpk,      // edge epks, priority loading (same extension as epk)
    kFileKindPackWad,   // WADs within pack files; should only be used for maps
    kFileKindIpk,       // standalone game EPK (same extension as epk)
    kFileKindIFolder,   // standalone game folder
    kFileKindIPackWad,  // IWADs within pack files :/

    kFileKindDdf,      // .ddf or .ldf file
    kFileKindRts,      // .rts script  file
    kFileKindDehacked  // .deh or .bex file
};

class WadFile;
class PackFile;

class DataFile
{
   public:
    // full name of file
    std::string name_;

    // type of file (kFileKindXXX)
    FileKind kind_;

    // file object   [ TODO review when active ]
    epi::File *file_;

    // for kFileKindIWad, PWad, EWad, XWad.
    WadFile *wad_;

    // for kFileKindEpk
    PackFile *pack_;

   public:
    DataFile(std::string name, FileKind kind);
    ~DataFile();
};

extern std::vector<DataFile *> data_files;

size_t AddDataFile(std::string file, FileKind kind);
int    GetTotalFiles();
void   ShowLoadedFiles();

void   ProcessMultipleFiles();
size_t AddPendingFile(std::string file, FileKind kind);
void   ProcessFile(DataFile *df);

epi::File *OpenFileFromPack(const std::string &name);

void DoPackSubstitutions(void);

uint8_t *OpenPackOrLumpInMemory(const std::string              &name,
                                const std::vector<std::string> &extensions,
                                int                            *length);

int CheckPackFilesForName(const std::string &name);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
