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

#ifndef __DDF_MUS_H__
#define __DDF_MUS_H__

#include "epi.h"
#include "types.h"

// ----------------------------------------------------------------
// -------------------------MUSIC PLAYLIST-------------------------
// ----------------------------------------------------------------

// Playlist entry class

// FIXME: Move enums in pl_entry_c class?
typedef enum
{
    MUS_UNKNOWN = 0,
    MUS_MIDI,
    MUS_MUS,
    MUS_OGG,
    MUS_MP3,
    MUS_FLAC,
    MUS_M4P,
    MUS_RAD,
    MUS_IMF280,
    MUS_IMF560,
    MUS_IMF700,
    ENDOFMUSTYPES
} musictype_t;

typedef enum
{
    MUSINF_UNKNOWN   = 0,
    MUSINF_LUMP      = 1,
    MUSINF_FILE      = 2,
    MUSINF_PACKAGE   = 3,
    ENDOFMUSINFTYPES = 4
} musicinftype_e;

class pl_entry_c
{
   public:
    pl_entry_c();
    ~pl_entry_c();

   public:
    void Default(void);
    void CopyDetail(pl_entry_c &src);

    // Member vars....
    int number;

    musictype_t    type;
    musicinftype_e infotype;

    std::string info;

   private:
    // disable copy construct and assignment operator
    explicit pl_entry_c(pl_entry_c &rhs) { (void)rhs; }
    pl_entry_c &operator=(pl_entry_c &rhs)
    {
        (void)rhs;
        return *this;
    }
};

// Our playlist entry container
class pl_entry_container_c : public std::vector<pl_entry_c *>
{
   public:
    pl_entry_container_c() {}
    ~pl_entry_container_c()
    {
        for (auto iter = begin(); iter != end(); iter++)
        {
            pl_entry_c *pl = *iter;
            delete pl;
            pl = nullptr;
        }
    }

   public:
    pl_entry_c *Find(int number);
    int         FindLast(const char *name);
    int         FindFree();
};

// -------EXTERNALISATIONS-------

extern pl_entry_container_c playlist;  // -ACB- 2004/06/04 Implemented

void DDF_ReadMusicPlaylist(const std::string &data);

#endif  // __DDF_MUS_H__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
