//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Music Playlist Handling)
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

#include "local.h"

#include "playlist.h"
#include "str_compare.h"
static pl_entry_c *dynamic_plentry;

pl_entry_container_c playlist;

//
// DDF_MusicParseInfo
//
// Parses the music information given.
//
static void DDF_MusicParseInfo(const char *info)
{
    static const char *const musstrtype[] = {"UNKNOWN", "MIDI", "MUS", "OGG", "MP3", "FLAC",
                                             "M4P", "RAD",  "IMF280", "IMF560", "IMF700", nullptr};
    static const char *const musinftype[] = {"UNKNOWN", "LUMP", "FILE", "PACK", nullptr};

    char charbuff[256];
    int  pos, i;

    // Get the music type
    i   = 0;
    pos = 0;

    while (info[pos] != ':' && i < 255)
    {
        if (info[i] == '\0')
            DDF_Error("DDF_MusicParseInfo: Premature end of music info\n");

        charbuff[i] = info[pos];

        i++;
        pos++;
    }

    if (i == 255)
        DDF_Error("DDF_MusicParseInfo: Music info too big\n");

    // -AJA- terminate charbuff with trailing \0.
    charbuff[i] = 0;

    i = MUS_UNKNOWN;
    while (i != ENDOFMUSTYPES && epi::StringCaseCompareASCII(charbuff, musstrtype[i]) != 0)
        i++;

    if (i == ENDOFMUSTYPES)
    {
        i = MUSINF_UNKNOWN;
        while (musinftype[i] != nullptr && epi::StringCaseCompareASCII(charbuff, musinftype[i]) != 0)
            i++;
        if (i == ENDOFMUSINFTYPES)
            DDF_Warning("DDF_MusicParseInfo: Unknown music type: '%s'\n", charbuff);
        else
        {
            dynamic_plentry->infotype = (musicinftype_e)i;
            // Remained is the string reference: filename/lumpname
            pos++;
            dynamic_plentry->info = &info[pos];
            return;
        }
    }
    else
        dynamic_plentry->type = (musictype_t)i;

    // Data Type
    i = 0;
    pos++;
    while (info[pos] != ':' && i < 255)
    {
        if (info[pos] == '\0')
            DDF_Error("DDF_MusicParseInfo: Premature end of music info\n");

        charbuff[i] = info[pos];

        pos++;
        i++;
    }

    if (i == 255)
        DDF_Error("DDF_MusicParseInfo: Music info too big\n");

    // -AJA- terminate charbuff with trailing \0.
    charbuff[i] = 0;

    i = MUSINF_UNKNOWN;
    while (musinftype[i] != nullptr && epi::StringCaseCompareASCII(charbuff, musinftype[i]) != 0)
        i++;

    if (i == ENDOFMUSINFTYPES)
        DDF_Warning("DDF_MusicParseInfo: Unknown music info: '%s'\n", charbuff);
    else
        dynamic_plentry->infotype = (musicinftype_e)i; // technically speaking this is proper

    // Remained is the string reference: filename/lumpname
    pos++;
    dynamic_plentry->info = &info[pos];

    return;
}

//
//  DDF PARSE ROUTINES
//

static void PlaylistStartEntry(const char *name, bool extend)
{
    int number = HMM_MAX(0, atoi(name));

    if (number == 0)
        DDF_Error("Bad music number in playlist.ddf: %s\n", name);

    dynamic_plentry = playlist.Find(number);

    if (extend)
    {
        if (!dynamic_plentry)
            DDF_Error("Unknown playlist to extend: %s\n", name);
        return;
    }

    // replaces an existing entry
    if (dynamic_plentry)
    {
        dynamic_plentry->Default();
        return;
    }

    // not found, create a new entry
    dynamic_plentry = new pl_entry_c;

    dynamic_plentry->number = number;

    playlist.push_back(dynamic_plentry);
}

static void PlaylistParseField(const char *field, const char *contents, int index, bool is_last)
{
#if (DEBUG_DDF)
    I_Debugf("PLAYLIST_PARSE: %s = %s;\n", field, contents);
#endif

    if (DDF_CompareName(field, "MUSICINFO") == 0)
    {
        DDF_MusicParseInfo(contents);
        return;
    }

    DDF_WarnError("Unknown playlist.ddf command: %s\n", field);
}

static void PlaylistFinishEntry(void)
{
    // nothing needed
}

static void PlaylistClearAll(void)
{
    // 100% safe to just remove all entries
    for (auto pl : playlist)
    {
        delete pl;
        pl = nullptr;
    }
    playlist.clear();
}

void DDF_ReadMusicPlaylist(const std::string &data)
{
    readinfo_t playlistinfo;

    playlistinfo.tag      = "PLAYLISTS";
    playlistinfo.lumpname = "DDFPLAY";

    playlistinfo.start_entry  = PlaylistStartEntry;
    playlistinfo.parse_field  = PlaylistParseField;
    playlistinfo.finish_entry = PlaylistFinishEntry;
    playlistinfo.clear_all    = PlaylistClearAll;

    DDF_MainReadFile(&playlistinfo, data);
}

void DDF_MusicPlaylistInit(void)
{
    // -ACB- 2004/05/04 Use container
    PlaylistClearAll();
}

void DDF_MusicPlaylistCleanUp(void)
{
    // -ACB- 2004/05/04 Cut our playlist down to size
    playlist.shrink_to_fit();
}

// --> pl_entry_c class

//
// pl_entry_c constructor
//
pl_entry_c::pl_entry_c() : number(0)
{
    Default();
}

//
// pl_entry_c destructor
//
pl_entry_c::~pl_entry_c()
{
}

//
// pl_entry_c::CopyDetail()
//
// Copy everything with exception ddf identifier
//
void pl_entry_c::CopyDetail(pl_entry_c &src)
{
    type     = src.type;
    infotype = src.infotype;
    info     = src.info;
}

//
// pl_entry_c::Default()
//
void pl_entry_c::Default()
{
    type     = MUS_UNKNOWN;
    infotype = MUSINF_UNKNOWN;
    info.clear();
}

// --> pl_entry_containter_c class

//
// pl_entry_c* pl_entry_container_c::Find()
//
pl_entry_c *pl_entry_container_c::Find(int number)
{
    for (auto iter = begin(); iter != end(); iter++)
    {
        pl_entry_c *p = *iter;
        if (p->number == number)
            return p;
    }

    return nullptr;
}

int pl_entry_container_c::FindLast(const char *name)
{
    for (auto iter = rbegin(); iter != rend(); iter++)
    {
        pl_entry_c *p = *iter;
        if (DDF_CompareName(p->info.c_str(), name) == 0)
            return p->number;
    }

    return -1;
}

int pl_entry_container_c::FindFree()
{
    int                   HighestNum = 0;

    for (auto iter = begin(); iter != end(); iter++)
    {
        pl_entry_c *p = *iter;
        if (p->number > HighestNum)
        {
            HighestNum = p->number;
        }
    }
    HighestNum = HighestNum + 1;
    return HighestNum;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
