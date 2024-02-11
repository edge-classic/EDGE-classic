//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Movies)
//----------------------------------------------------------------------------
//
//  Copyright (c) 2023-2024 The EDGE Team.
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

#ifndef __DDF_MOVIE_H__
#define __DDF_MOVIE_H__

#include "epi.h"
#include "types.h"

typedef enum
{
    MOVDT_None,     // Default/dummy value
    MOVDT_Lump,     // load from lump in a WAD
    MOVDT_Package,  // load from an EPK
} moviedata_type_e;

typedef enum
{
    MOVSC_Autofit,  // fit movie to screen as best as possible
    MOVSC_NoScale,  // force movie to play at original size regardless of
                    // display
    MOVSC_Zoom,     // movie will be scaled to fit display height; sides may be
                    // clipped
    MOVSC_Stretch,  // movie will stretch to fit display; disregards aspect
                    // ratio
} moviescale_type_e;

typedef enum
{
    MOVSP_None = 0,
    MOVSP_Mute = 0x0001,  // do not play associated audio track
} movie_special_e;

class moviedef_c
{
   public:
    moviedef_c();
    ~moviedef_c(){};

   public:
    void Default(void);
    void CopyDetail(const moviedef_c &src);

    // Member vars....
    std::string name;

    moviedata_type_e type;

    std::string info;

    moviescale_type_e scaling;

    movie_special_e special;

   private:
    // disable copy construct and assignment operator
    explicit moviedef_c(moviedef_c &rhs) { (void)rhs; }
    moviedef_c &operator=(moviedef_c &rhs)
    {
        (void)rhs;
        return *this;
    }
};

// Our moviedefs container
class moviedef_container_c : public std::vector<moviedef_c *>
{
   public:
    moviedef_container_c() {}
    ~moviedef_container_c()
    {
        for (auto iter = begin(); iter != end(); iter++)
        {
            moviedef_c *mov = *iter;
            delete mov;
            mov = nullptr;
        }
    }

   public:
    // Search Functions
    moviedef_c *Lookup(const char *refname);
};

extern moviedef_container_c moviedefs;

void DDF_ReadMovies(const std::string &data);

#endif /*__DDF_MOVIE_H__*/

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
