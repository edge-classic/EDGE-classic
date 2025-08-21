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

#include "ddf_playlist.h"

#include "ddf_local.h"
#include "epi_str_compare.h"
static PlaylistEntry *dynamic_plentry;

PlaylistEntryContainer playlist;

static std::vector<std::string> supported_music_types;

static void InitializeMusicTypes()
{
    supported_music_types.push_back("UNKNOWN");
    supported_music_types.push_back("MIDI");
    supported_music_types.push_back("OGG");
    supported_music_types.push_back("MP3");
    supported_music_types.push_back("FLAC");
#ifdef EDGE_CLASSIC
    supported_music_types.push_back("MUS");
    supported_music_types.push_back("TRACKER");
    supported_music_types.push_back("SID");
    supported_music_types.push_back("IMF280");
    supported_music_types.push_back("IMF560");
    supported_music_types.push_back("IMF700");
#endif
}

//
// DDFMusicParseInfo
//
// Parses the music information given.
//
static void DDFMusicParseInfo(const char *info)
{
    if (supported_music_types.empty())
        InitializeMusicTypes();

    static const char *const musinftype[] = {"UNKNOWN", "LUMP", "FILE", "PACK", nullptr};

    char charbuff[256];
    int  pos, i;

    // Get the music type
    i   = 0;
    pos = 0;

    while (info[pos] != ':' && i < 255)
    {
        if (info[i] == '\0')
            DDFError("DDFMusicParseInfo: Premature end of music info\n");

        charbuff[i] = info[pos];

        i++;
        pos++;
    }

    if (i == 255)
        DDFError("DDFMusicParseInfo: Music info too big\n");

    // -AJA- terminate charbuff with trailing \0.
    charbuff[i] = 0;

    i = kDDFMusicUnknown;
    while (i != kTotalDDFMusicTypes && epi::StringCaseCompareASCII(charbuff, supported_music_types[i]) != 0)
        i++;

    if (i == kTotalDDFMusicTypes)
    {
        i = kDDFMusicDataUnknown;
        while (musinftype[i] != nullptr && epi::StringCaseCompareASCII(charbuff, musinftype[i]) != 0)
            i++;
        if (i == kTotalDDFMusicDataTypes)
            DDFWarning("DDFMusicParseInfo: Unknown music type: '%s'\n", charbuff);
        else
        {
            dynamic_plentry->infotype_ = (DDFMusicDataType)i;
            // Remained is the string reference: filename/lumpname
            pos++;
            dynamic_plentry->info_ = &info[pos];
            return;
        }
    }
    else
        dynamic_plentry->type_ = (DDFMusicType)i;

    // Data Type
    i = 0;
    pos++;
    while (info[pos] != ':' && i < 255)
    {
        if (info[pos] == '\0')
            DDFError("DDFMusicParseInfo: Premature end of music info\n");

        charbuff[i] = info[pos];

        pos++;
        i++;
    }

    if (i == 255)
        DDFError("DDFMusicParseInfo: Music info too big\n");

    // -AJA- terminate charbuff with trailing \0.
    charbuff[i] = 0;

    i = kDDFMusicDataUnknown;
    while (musinftype[i] != nullptr && epi::StringCaseCompareASCII(charbuff, musinftype[i]) != 0)
        i++;

    if (i == kTotalDDFMusicDataTypes)
        DDFWarning("DDFMusicParseInfo: Unknown music info: '%s'\n", charbuff);
    else
        dynamic_plentry->infotype_ = (DDFMusicDataType)i; // technically speaking this is proper

    // Remained is the string reference: filename/lumpname
    pos++;
    dynamic_plentry->info_ = &info[pos];

    return;
}

//
//  DDF PARSE ROUTINES
//

static void PlaylistStartEntry(const char *name, bool extend)
{
    int number = HMM_MAX(0, atoi(name));

    if (number == 0)
        DDFError("Bad music number in playlist.ddf: %s\n", name);

    dynamic_plentry = playlist.Find(number);

    if (extend)
    {
        if (!dynamic_plentry)
            DDFError("Unknown playlist to extend: %s\n", name);
        return;
    }

    // replaces an existing entry
    if (dynamic_plentry)
    {
        dynamic_plentry->Default();
        return;
    }

    // not found, create a new entry
    dynamic_plentry = new PlaylistEntry;

    dynamic_plentry->number_ = number;

    playlist.push_back(dynamic_plentry);
}

static void PlaylistParseField(const char *field, const char *contents, int index, bool is_last)
{
#if (DDF_DEBUG)
    LogDebug("PLAYLIST_PARSE: %s = %s;\n", field, contents);
#endif
    EPI_UNUSED(index);
    EPI_UNUSED(is_last);
    if (DDFCompareName(field, "MUSICINFO") == 0)
    {
        DDFMusicParseInfo(contents);
        return;
    }

    DDFWarnError("Unknown playlist.ddf command: %s\n", field);
}

static void PlaylistFinishEntry(void)
{
    // nothing needed
}

static void PlaylistClearAll(void)
{
    // 100% safe to just remove all entries
    for (PlaylistEntry *pl : playlist)
    {
        delete pl;
        pl = nullptr;
    }
    playlist.clear();
}

void DDFReadMusicPlaylist(const std::string &data)
{
    DDFReadInfo playlistinfo;

    playlistinfo.tag      = "PLAYLISTS";
    playlistinfo.lumpname = "DDFPLAY";

    playlistinfo.start_entry  = PlaylistStartEntry;
    playlistinfo.parse_field  = PlaylistParseField;
    playlistinfo.finish_entry = PlaylistFinishEntry;
    playlistinfo.clear_all    = PlaylistClearAll;

    DDFMainReadFile(&playlistinfo, data);
}

void DDFMusicPlaylistInit(void)
{
    // -ACB- 2004/05/04 Use container
    PlaylistClearAll();
}

void DDFMusicPlaylistCleanUp(void)
{
    // -ACB- 2004/05/04 Cut our playlist down to size
    playlist.shrink_to_fit();
}

// --> PlaylistEntry class

//
// PlaylistEntry constructor
//
PlaylistEntry::PlaylistEntry() : number_(0)
{
    Default();
}

//
// PlaylistEntry destructor
//
PlaylistEntry::~PlaylistEntry()
{
}

//
// PlaylistEntry::CopyDetail()
//
// Copy everything with exception ddf identifier
//
void PlaylistEntry::CopyDetail(const PlaylistEntry &src)
{
    type_     = src.type_;
    infotype_ = src.infotype_;
    info_     = src.info_;
}

//
// PlaylistEntry::Default()
//
void PlaylistEntry::Default()
{
    type_     = kDDFMusicUnknown;
    infotype_ = kDDFMusicDataUnknown;
    info_.clear();
}

// --> PlaylistEntryontainter_c class

//
// PlaylistEntry* PlaylistEntryContainer::Find()
//
PlaylistEntry *PlaylistEntryContainer::Find(int number)
{
    for (std::vector<PlaylistEntry *>::iterator iter = begin(), iter_end = end(); iter != iter_end; iter++)
    {
        PlaylistEntry *p = *iter;
        if (p->number_ == number)
            return p;
    }

    return nullptr;
}

int PlaylistEntryContainer::FindLast(const char *name)
{
    for (std::vector<PlaylistEntry *>::reverse_iterator iter = rbegin(), iter_end = rend(); iter != iter_end; iter++)
    {
        PlaylistEntry *p = *iter;
        if (DDFCompareName(p->info_.c_str(), name) == 0)
            return p->number_;
    }

    return -1;
}

int PlaylistEntryContainer::FindFree()
{
    int HighestNum = 0;

    for (std::vector<PlaylistEntry *>::iterator iter = begin(), iter_end = end(); iter != iter_end; iter++)
    {
        PlaylistEntry *p = *iter;
        if (p->number_ > HighestNum)
        {
            HighestNum = p->number_;
        }
    }
    HighestNum = HighestNum + 1;
    return HighestNum;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
