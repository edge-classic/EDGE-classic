//----------------------------------------------------------------------------
//  EDGE EPK Support Code
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

#pragma once

#include "epi_str_hash.h"

class DataFile;

class PackFile;

epi::File *OpenPackFile(PackFile *pack, const std::string &name);

epi::File *OpenPackMatch(PackFile *pack, const std::string &name, const std::vector<std::string> &extensions);

// Equivalent to IsLumpInPwad....doesn't care or check filetype itself
int FindStemInPack(PackFile *pack, epi::StringHash name_hash);

// Checks if exact filename is found in a pack; used to help load order
// determination
bool FindPackFile(PackFile *pack, const std::string &name);

// Check images/sound/etc that may override WAD-oriented lumps or definitions
void ProcessPackSubstitutions(PackFile *pack, int pack_index);

// Process /hires folder contents
void ProcessHiresPackSubstitutions(PackFile *pack, int pack_index);

// Check /sprites directory for sprites to automatically add during
// InitializeSprites
std::vector<std::string> GetPackSpriteList(PackFile *pack);

// Only populate the pack directory; used for ad-hoc folder/EPK checks
void PopulatePackOnly(DataFile *df);

// Check pack for valid IWADs. Return associated game_checker index if found
int CheckPackForIWADs(DataFile *df);

// Populate pack directory and process appropriate files (COAL, DDF, etc)
void ProcessAllInPack(DataFile *df, size_t file_index);

void ClosePackFile(DataFile *df);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
