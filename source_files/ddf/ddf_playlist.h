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

// ----------------------------------------------------------------
// -------------------------MUSIC PLAYLIST-------------------------
// ----------------------------------------------------------------

// Playlist entry class

enum DDFMusicType
{
    kDdfMusicUnknown = 0,
    kDdfMusicMIDI,
    kDdfMusicMUS,
    kDdfMusicOGG,
    kDdfMusicMP3,
    kDdfMusicSID,
    kDdfMusicFLAC,
    kDdfMusicM4P,
    kDdfMusicRAD,
    kDdfMusicIMF280,
    kDdfMusicIMF560,
    kDdfMusicIMF700,
    kTotalDdfMusicTypes
};

enum DDFMusicDataType
{
    kDdfMusicDataUnknown    = 0,
    kDdfMusicDataLump       = 1,
    kDdfMusicDataFile       = 2,
    kDdfMusicDataPackage    = 3,
    kTotalDdfMusicDataTypes = 4
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
        (void)rhs;
    }
    PlaylistEntry &operator=(PlaylistEntry &rhs)
    {
        (void)rhs;
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

void DdfReadMusicPlaylist(const std::string &data);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
