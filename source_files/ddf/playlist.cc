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

#include "playlist.h"

#include "local.h"
#include "str_compare.h"
static PlaylistEntry *dynamic_plentry;

PlaylistEntryContainer playlist;

//
// DDF_MusicParseInfo
//
// Parses the music information given.
//
static void DDF_MusicParseInfo(const char *info)
{
    static const char *const musstrtype[] = {
        "UNKNOWN", "MIDI", "MUS",    "OGG",    "MP3",    "FLAC",
        "M4P",     "RAD",  "IMF280", "IMF560", "IMF700", nullptr};
    static const char *const musinftype[] = {"UNKNOWN", "LUMP", "FILE", "PACK",
                                             nullptr};

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

    if (i == 255) DDF_Error("DDF_MusicParseInfo: Music info too big\n");

    // -AJA- terminate charbuff with trailing \0.
    charbuff[i] = 0;

    i = kDDFMusicUnknown;
    while (i != kTotalDDFMusicTypes &&
           epi::StringCaseCompareASCII(charbuff, musstrtype[i]) != 0)
        i++;

    if (i == kTotalDDFMusicTypes)
    {
        i = kDDFMusicDataUnknown;
        while (musinftype[i] != nullptr &&
               epi::StringCaseCompareASCII(charbuff, musinftype[i]) != 0)
            i++;
        if (i == kTotalDDFMusicDataTypes)
            DDF_Warning("DDF_MusicParseInfo: Unknown music type: '%s'\n",
                        charbuff);
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
            DDF_Error("DDF_MusicParseInfo: Premature end of music info\n");

        charbuff[i] = info[pos];

        pos++;
        i++;
    }

    if (i == 255) DDF_Error("DDF_MusicParseInfo: Music info too big\n");

    // -AJA- terminate charbuff with trailing \0.
    charbuff[i] = 0;

    i = kDDFMusicDataUnknown;
    while (musinftype[i] != nullptr &&
           epi::StringCaseCompareASCII(charbuff, musinftype[i]) != 0)
        i++;

    if (i == kTotalDDFMusicDataTypes)
        DDF_Warning("DDF_MusicParseInfo: Unknown music info: '%s'\n", charbuff);
    else
        dynamic_plentry->infotype_ =
            (DDFMusicDataType)i;  // technically speaking this is proper

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

    if (number == 0) DDF_Error("Bad music number in playlist.ddf: %s\n", name);

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
    dynamic_plentry = new PlaylistEntry;

    dynamic_plentry->number_ = number;

    playlist.push_back(dynamic_plentry);
}

static void PlaylistParseField(const char *field, const char *contents,
                               int index, bool is_last)
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
    for (PlaylistEntry *pl : playlist)
    {
        delete pl;
        pl = nullptr;
    }
    playlist.clear();
}

void DDF_ReadMusicPlaylist(const std::string &data)
{
    DDFReadInfo playlistinfo;

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

// --> PlaylistEntry class

//
// PlaylistEntry constructor
//
PlaylistEntry::PlaylistEntry() : number_(0) { Default(); }

//
// PlaylistEntry destructor
//
PlaylistEntry::~PlaylistEntry() {}

//
// PlaylistEntry::CopyDetail()
//
// Copy everything with exception ddf identifier
//
void PlaylistEntry::CopyDetail(PlaylistEntry &src)
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
// PlaylistEntry* PlaylistEntryontainer_c::Find()
//
PlaylistEntry *PlaylistEntryContainer::Find(int number)
{
    for (std::vector<PlaylistEntry *>::iterator iter     = begin(),
                                                iter_end = end();
         iter != iter_end; iter++)
    {
        PlaylistEntry *p = *iter;
        if (p->number_ == number) return p;
    }

    return nullptr;
}

int PlaylistEntryContainer::FindLast(const char *name)
{
    for (std::vector<PlaylistEntry *>::reverse_iterator iter     = rbegin(),
                                                        iter_end = rend();
         iter != iter_end; iter++)
    {
        PlaylistEntry *p = *iter;
        if (DDF_CompareName(p->info_.c_str(), name) == 0) return p->number_;
    }

    return -1;
}

int PlaylistEntryContainer::FindFree()
{
    int HighestNum = 0;

    for (std::vector<PlaylistEntry *>::iterator iter     = begin(),
                                                iter_end = end();
         iter != iter_end; iter++)
    {
        PlaylistEntry *p = *iter;
        if (p->number_ > HighestNum) { HighestNum = p->number_; }
    }
    HighestNum = HighestNum + 1;
    return HighestNum;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
