//------------------------------------------------------------------------
//  MUSIC Definitions
//------------------------------------------------------------------------
//
//  DEH_EDGE  Copyright (C) 2004-2024 The EDGE Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License (in COPYING.txt) for more details.
//
//------------------------------------------------------------------------
//
//  DEH_EDGE is based on:
//
//  +  DeHackEd source code, by Greg Lewis.
//  -  DOOM source code (C) 1993-1996 id Software, Inc.
//  -  Linux DOOM Hack Editor, by Sam Lantinga.
//  -  PrBoom's DEH/BEX code, by Ty Halderman, TeamTNT.
//
//------------------------------------------------------------------------

#include "deh_music.h"

#include <stdlib.h>
#include <string.h>

#include <map>

#include "deh_buffer.h"
#include "deh_edge.h"
#include "deh_patch.h"
#include "deh_system.h"
#include "deh_wad.h"
#include "epi.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"
namespace dehacked
{

//
// MusicInfo struct.
//
struct MusicInfo
{
    // up to 6-character name
    char name[12];
    int  ddf_num;
};

//
// Information about all the music
//
const MusicInfo S_music_orig[kTotalMusicTypes] = {
    // kmus_None  (a dummy entry)
    {"", -1},

    // Doom I
    {"e1m1", 33},
    {"e1m2", 34},
    {"e1m3", 35},
    {"e1m4", 36},
    {"e1m5", 37},
    {"e1m6", 38},
    {"e1m7", 39},
    {"e1m8", 40},
    {"e1m9", 41},
    {"e2m1", 42},
    {"e2m2", 43},
    {"e2m3", 44},
    {"e2m4", 45},
    {"e2m5", 46},
    {"e2m6", 47},
    {"e2m7", 48},
    {"e2m8", 49},
    {"e2m9", 50},
    {"e3m1", 51},
    {"e3m2", 52},
    {"e3m3", 53},
    {"e3m4", 54},
    {"e3m5", 55},
    {"e3m6", 56},
    {"e3m7", 57},
    {"e3m8", 58},
    {"e3m9", 59},

    // Doom II
    {"inter", 63},
    {"intro", 62},
    {"bunny", 67},
    {"victor", 61},
    {"introa", 68},
    {"runnin", 1},
    {"stalks", 2},
    {"countd", 3},
    {"betwee", 4},
    {"doom", 5},
    {"the_da", 6},
    {"shawn", 7},
    {"ddtblu", 8},
    {"in_cit", 9},
    {"dead", 10},
    {"stlks2", 11},
    {"theda2", 12},
    {"doom2", 13},
    {"ddtbl2", 14},
    {"runni2", 15},
    {"dead2", 16},
    {"stlks3", 17},
    {"romero", 18},
    {"shawn2", 19},
    {"messag", 20},
    {"count2", 21},
    {"ddtbl3", 22},
    {"ampie", 23},
    {"theda3", 24},
    {"adrian", 25},
    {"messg2", 26},
    {"romer2", 27},
    {"tense", 28},
    {"shawn3", 29},
    {"openin", 30},
    {"evil", 31},
    {"ultima", 32},
    {"read_m", 60},
    {"dm2ttl", 65},
    {"dm2int", 64}};

//
// all the modified entries.
// NOTE: some pointers may be nullptr!
//
std::map<int, MusicInfo *> S_music;

//------------------------------------------------------------------------

namespace music
{
void MarkEntry(int num);
void BeginLump();
void FinishLump();
void WriteEntry(int num);
} // namespace music

void music::Init()
{
    for (std::map<int, MusicInfo *>::iterator iter = S_music.begin(), iter_end = S_music.end(); iter != iter_end;
         ++iter)
    {
        delete iter->second;
    }
    S_music.clear();
}

void music::Shutdown()
{
    for (std::map<int, MusicInfo *>::iterator iter = S_music.begin(), iter_end = S_music.end(); iter != iter_end;
         ++iter)
    {
        delete iter->second;
    }
    S_music.clear();
}

void music::MarkEntry(int num)
{
    if (num == kmus_None)
        return;

    // already have a modified entry?
    if (S_music.count(num))
        return;

    MusicInfo *entry = new MusicInfo;
    S_music[num]     = entry;

    // copy the original info
    if (num < kTotalMusicTypes)
    {
        strcpy(entry->name, S_music_orig[num].name);
        entry->ddf_num = S_music_orig[num].ddf_num;
    }
    else
    {
        entry->name[0] = 0;
        entry->ddf_num = 100 + num;
    }
}

void music::BeginLump()
{
    wad::NewLump(kDDFTypePlaylist);

    wad::Printf("<PLAYLISTS>\n");
}

void music::FinishLump()
{
    wad::Printf("\n");
}

void music::WriteEntry(int num)
{
    const MusicInfo *mod   = S_music[num];
    std::string      upper = mod->name;
    epi::StringUpperASCII(upper);
    wad::Printf("\n");
    wad::Printf("[%02d] ", mod->ddf_num);
    wad::Printf("MUSICINFO = MUS:LUMP:\"D_%s\";\n", upper.c_str());
}

void music::ConvertMUS()
{
    bool got_one = false;

    for (std::map<int, MusicInfo *>::iterator iter = S_music.begin(), iter_end = S_music.end(); iter != iter_end;
         ++iter)
    {
        if (iter->second == nullptr)
            continue;

        if (!got_one)
        {
            BeginLump();
            got_one = true;
        }

        WriteEntry(iter->first);
    }

    if (got_one)
        FinishLump();
}

bool music::ReplaceMusic(const char *before, const char *after)
{
    for (int i = 1; i < kTotalMusicTypes; i++)
    {
        if (epi::StringCaseCompareASCII(S_music_orig[i].name, before) != 0)
            continue;

        // create modified entry if it does not exist yet
        MarkEntry(i);

        strcpy(S_music[i]->name, after);
        return true;
    }

    return false;
}

void music::AlterBexMusic(const char *new_val)
{
    const char *old_val = patch::line_buf;

    if (strlen(new_val) < 1 || strlen(new_val) > 6)
    {
        LogDebug("Dehacked: Warning - Bad length for music name '%s'.\n", new_val);
        return;
    }

    // for DSDehacked, support a numeric target
    if (epi::IsDigitASCII(old_val[0]))
    {
        int num = atoi(old_val);
        if (num < 1)
        {
            LogDebug("Dehacked: Warning - Line %d: illegal music entry '%s'.\n", patch::line_num, old_val);
        }
        else
        {
            MarkEntry(num);
            strcpy(S_music[num]->name, new_val);
        }
        return;
    }

    if (strlen(old_val) < 1 || strlen(old_val) > 6)
    {
        LogDebug("Dehacked: Warning - Bad length for music name '%s'.\n", old_val);
        return;
    }

    if (!ReplaceMusic(old_val, new_val))
        LogDebug("Dehacked: Warning - Line %d: unknown music name '%s'.\n", patch::line_num, old_val);
}

} // namespace dehacked
