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

#pragma once

#include "types.h"

enum MovieDataType
{
    kMovieDataNone,    // Default/dummy value
    kMovieDataLump,    // load from lump in a WAD
    kMovieDataPackage, // load from an EPK
};

enum MovieScaling
{
    kMovieScalingAutofit, // fit movie to screen as best as possible
    kMovieScalingNoScale, // force movie to play at original size regardless of
                          // display
    kMovieScalingZoom,    // movie will be scaled to fit display height; sides may
                          // be clipped
    kMovieScalingStretch, // movie will stretch to fit display; disregards
                          // aspect ratio
};

enum MovieSpecial
{
    kMovieSpecialNone = 0,
    kMovieSpecialMute = 0x0001, // do not play associated audio track
};

class MovieDefinition
{
  public:
    MovieDefinition();
    ~MovieDefinition(){};

  public:
    void Default(void);
    void CopyDetail(const MovieDefinition &src);

    // Member vars....
    std::string name_;

    MovieDataType type_;

    std::string info_;

    MovieScaling scaling_;

    MovieSpecial special_;

  private:
    // disable copy construct and assignment operator
    explicit MovieDefinition(MovieDefinition &rhs)
    {
        (void)rhs;
    }
    MovieDefinition &operator=(MovieDefinition &rhs)
    {
        (void)rhs;
        return *this;
    }
};

class MovieDefinitionContainer : public std::vector<MovieDefinition *>
{
  public:
    MovieDefinitionContainer()
    {
    }
    ~MovieDefinitionContainer()
    {
        for (std::vector<MovieDefinition *>::iterator iter = begin(), iter_end = end(); iter != iter_end; iter++)
        {
            MovieDefinition *mov = *iter;
            delete mov;
            mov = nullptr;
        }
    }

  public:
    // Search Functions
    MovieDefinition *Lookup(const char *refname);
};

extern MovieDefinitionContainer moviedefs;

void DDF_ReadMovies(const std::string &data);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
