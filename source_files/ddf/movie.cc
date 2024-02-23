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
//
// Movie Setup and Parser Code
//

#include "movie.h"

#include <string.h>

#include "local.h"

static MovieDefinition *dynamic_movie;

static void DDF_MovieGetType(const char *info, void *storage);
static void DDF_MovieGetSpecial(const char *info, void *storage);
static void DDF_MovieGetScaling(const char *info, void *storage);

#define DDF_CMD_BASE dummy_movie
static MovieDefinition dummy_movie;

static const DDFCommandList movie_commands[] = {
    DDF_FIELD("MOVIE_DATA", type_, DDF_MovieGetType),
    DDF_FIELD("SPECIAL", special_, DDF_MovieGetSpecial),
    DDF_FIELD("SCALING", scaling_, DDF_MovieGetScaling),

    DDF_CMD_END};

MovieDefinitionContainer moviedefs;

//
//  DDF PARSE ROUTINES
//

static void MovieStartEntry(const char *name, bool extend)
{
    if (!name || !name[0]) DDF_Error("New movie entry is missing a name!\n");

    dynamic_movie = moviedefs.Lookup(name);

    if (extend)
    {
        if (!dynamic_movie) DDF_Error("Unknown movie to extend: %s\n", name);
        return;
    }

    // replaces an existing entry?
    if (dynamic_movie)
    {
        dynamic_movie->Default();
        return;
    }

    // not found, create a new one
    dynamic_movie = new MovieDefinition;

    dynamic_movie->name_ = name;

    moviedefs.push_back(dynamic_movie);
}

static void MovieParseField(const char *field, const char *contents, int index,
                            bool is_last)
{
#if (DEBUG_DDF)
    LogDebug("MOVIE_PARSE: %s = %s;\n", field, contents);
#endif

    if (DDF_MainParseField(movie_commands, field, contents,
                           (uint8_t *)dynamic_movie))
        return;  // OK

    DDF_Error("Unknown movies.ddf command: %s\n", field);
}

static void MovieFinishEntry(void)
{
    if (dynamic_movie->type_ == kMovieDataNone)
        DDF_Error("No lump or packfile defined for %s!\n",
                  dynamic_movie->name_.c_str());
}

static void MovieClearAll(void)
{
    LogWarning("Ignoring #CLEARALL in movies.ddf\n");
}

void DDF_ReadMovies(const std::string &data)
{
    DDFReadInfo movies;

    movies.tag      = "MOVIES";
    movies.lumpname = "DDFMOVIE";

    movies.start_entry  = MovieStartEntry;
    movies.parse_field  = MovieParseField;
    movies.finish_entry = MovieFinishEntry;
    movies.clear_all    = MovieClearAll;

    DDF_MainReadFile(&movies, data);
}

void DDF_MovieInit(void)
{
    for (MovieDefinition *movie : moviedefs)
    {
        delete movie;
        movie = nullptr;
    }
    moviedefs.clear();
}

void DDF_MovieCleanUp(void)
{
    moviedefs.shrink_to_fit();  // <-- Reduce to allocated size
}

static void MovieParseInfo(const char *value) { dynamic_movie->info_ = value; }

static void DDF_MovieGetType(const char *info, void *storage)
{
    const char *colon = DDF_MainDecodeList(info, ':', true);

    if (colon == nullptr || colon == info || (colon - info) >= 16 ||
        colon[1] == 0)
        DDF_Error("Malformed movie type spec: %s\n", info);

    char keyword[20];

    strncpy(keyword, info, colon - info);
    keyword[colon - info] = 0;

    if (DDF_CompareName(keyword, "LUMP") == 0)
    {
        dynamic_movie->type_ = kMovieDataLump;
        MovieParseInfo(colon + 1);
    }
    else if (DDF_CompareName(keyword, "PACK") == 0)
    {
        dynamic_movie->type_ = kMovieDataPackage;
        MovieParseInfo(colon + 1);
    }
    else
        DDF_Error("Unknown movie type: %s\n", keyword);
}

static DDFSpecialFlags movie_specials[] = {{"MUTE", kMovieSpecialMute, 0},
                                           {nullptr, 0, 0}};

static void DDF_MovieGetSpecial(const char *info, void *storage)
{
    MovieSpecial *dest = (MovieSpecial *)storage;

    int flag_value;

    switch (DDF_MainCheckSpecialFlag(info, movie_specials, &flag_value,
                                     false /* allow_prefixes */, false))
    {
        case kDDFCheckFlagPositive:
            *dest = (MovieSpecial)(*dest | flag_value);
            break;

        case kDDFCheckFlagNegative:
            *dest = (MovieSpecial)(*dest & ~flag_value);
            break;

        case kDDFCheckFlagUser:
        case kDDFCheckFlagUnknown:
            DDF_WarnError("Unknown movie special: %s\n", info);
            break;
    }
}

static void DDF_MovieGetScaling(const char *info, void *storage)
{
    MovieScaling *dest = (MovieScaling *)storage;

    if (DDF_CompareName(info, "AUTO") == 0) *dest = kMovieScalingAutofit;
    if (DDF_CompareName(info, "NONE") == 0)
        *dest = kMovieScalingNoScale;
    else if (DDF_CompareName(info, "ZOOM") == 0)
        *dest = kMovieScalingZoom;
    else if (DDF_CompareName(info, "STRETCH") == 0)
        *dest = kMovieScalingStretch;
    else
    {
        DDF_WarnError("Unknown movie scaling mode: %s\n", info);
        *dest = kMovieScalingAutofit;  // Default
    }
}

// ---> MovieDefinition class

MovieDefinition::MovieDefinition() : name_(), info_() { Default(); }

//
// Copies all the detail with the exception of ddf info
//
void MovieDefinition::CopyDetail(const MovieDefinition &src)
{
    type_    = src.type_;
    info_    = src.info_;
    scaling_ = src.scaling_;
    special_ = src.special_;
}

void MovieDefinition::Default()
{
    info_.clear();

    type_ = kMovieDataNone;

    scaling_ = kMovieScalingAutofit;
    special_ = kMovieSpecialNone;
}

// ---> MovieDefinitionContainer class

MovieDefinition *MovieDefinitionContainer::Lookup(const char *refname)
{
    if (!refname || !refname[0]) return nullptr;

    for (MovieDefinition *g : moviedefs)
    {
        if (DDF_CompareName(g->name_.c_str(), refname) == 0) return g;
    }

    return nullptr;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
