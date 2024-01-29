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

#ifndef __W_EPK__
#define __W_EPK__

#include "dm_defs.h"

// EPI
#include "file.h"

class data_file_c;

class pack_file_c;

epi::File *Pack_FileOpen(pack_file_c *pack, const std::string &name);

epi::File *Pack_OpenMatch(pack_file_c *pack, const std::string &name, const std::vector<std::string> &extensions);

// Equivalent to W_IsLumpInPwad....doesn't care or check filetype itself
int Pack_FindStem(pack_file_c *pack, const std::string &name);

// Checks if exact filename is found in a pack; used to help load order determination
bool Pack_FindFile(pack_file_c *pack, const std::string &name);

// Check images/sound/etc that may override WAD-oriented lumps or definitions
void Pack_ProcessSubstitutions(pack_file_c *pack, int pack_index);

// Process /hires folder contents
void Pack_ProcessHiresSubstitutions(pack_file_c *pack, int pack_index);

// Check /sprites directory for sprites to automatically add during W_InitSprites
std::vector<std::string> Pack_GetSpriteList(pack_file_c *pack);

// Only populate the pack directory; used for ad-hoc folder/EPK checks
void Pack_PopulateOnly(data_file_c *df);

// Check pack for valid IWADs. Return associated game_checker index if found
int Pack_CheckForIWADs(data_file_c *df);

// Populate pack directory and process appropriate files (COAL, DDF, etc)
void Pack_ProcessAll(data_file_c *df, size_t file_index);

#endif /* __W_PK3__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
