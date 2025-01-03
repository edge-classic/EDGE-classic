//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Main)
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

#pragma once

#include "ddf_types.h"
#include "epi.h"

// ----------------------------------------------------------------
// -------------------------MUSIC PLAYLIST-------------------------
// ----------------------------------------------------------------

// Playlist entry class

enum DDFMusicType
{
    kDDFMusicUnknown = 0,
    kDDFMusicMIDI,
#if EDGE_MUS_SUPPORT
    kDDFMusicMUS,
#endif
#if EDGE_OGG_SUPPORT
    kDDFMusicOGG,
#endif
#if EDGE_MP3_SUPPORT
    kDDFMusicMP3,
#endif
#if EDGE_SID_SUPPORT
    kDDFMusicSID,
#endif
#if EDGE_FLAC_SUPPORT
    kDDFMusicFLAC,
#endif
#if EDGE_TRACKER_SUPPORT
    kDDFMusicM4P,
#endif
#if EDGE_IMF_SUPPORT
    kDDFMusicIMF280,
    kDDFMusicIMF560,
    kDDFMusicIMF700,
#endif
    kTotalDDFMusicTypes
};

enum DDFMusicDataType
{
    kDDFMusicDataUnknown    = 0,
    kDDFMusicDataLump       = 1,
    kDDFMusicDataFile       = 2,
    kDDFMusicDataPackage    = 3,
    kTotalDDFMusicDataTypes = 4
};

class PlaylistEntry
{
  public:
    PlaylistEntry();
    ~PlaylistEntry();

  public:
    void Default(void);
    void CopyDetail(PlaylistEntry &src);

    // Member vars....
    int number_;

    DDFMusicType     type_;
    DDFMusicDataType infotype_;

    std::string info_;

  private:
    // disable copy construct and assignment operator
    explicit PlaylistEntry(PlaylistEntry &rhs)
    {
        EPI_UNUSED(rhs);
    }
    PlaylistEntry &operator=(PlaylistEntry &rhs)
    {
        EPI_UNUSED(rhs);
        return *this;
    }
};

class PlaylistEntryContainer : public std::vector<PlaylistEntry *>
{
  public:
    PlaylistEntryContainer()
    {
    }
    ~PlaylistEntryContainer()
    {
        for (std::vector<PlaylistEntry *>::iterator iter = begin(), iter_end = end(); iter != iter_end; iter++)
        {
            PlaylistEntry *pl = *iter;
            delete pl;
            pl = nullptr;
        }
    }

  public:
    PlaylistEntry *Find(int number);
    int            FindLast(const char *name);
    int            FindFree();
};

// -------EXTERNALISATIONS-------

extern PlaylistEntryContainer playlist; // -ACB- 2004/06/04 Implemented

void DDFReadMusicPlaylist(const std::string &data);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
