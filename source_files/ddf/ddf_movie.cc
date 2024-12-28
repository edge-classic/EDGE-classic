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

#include "ddf_movie.h"

#include <string.h>

#include "ddf_local.h"

static MovieDefinition *dynamic_movie;

static void DDFMovieGetType(const char *info, void *storage);
static void DDFMovieGetSpecial(const char *info, void *storage);
static void DDFMovieGetScaling(const char *info, void *storage);

static MovieDefinition dummy_movie;

static const DDFCommandList movie_commands[] = {DDF_FIELD("MOVIE_DATA", dummy_movie, type_, DDFMovieGetType),
                                                DDF_FIELD("SPECIAL", dummy_movie, special_, DDFMovieGetSpecial),
                                                DDF_FIELD("SCALING", dummy_movie, scaling_, DDFMovieGetScaling),

                                                {nullptr, nullptr, 0, nullptr}};

MovieDefinitionContainer moviedefs;

//
//  DDF PARSE ROUTINES
//

static void MovieStartEntry(const char *name, bool extend)
{
    if (!name || !name[0])
        DDFError("New movie entry is missing a name!\n");

    dynamic_movie = moviedefs.Lookup(name);

    if (extend)
    {
        if (!dynamic_movie)
            DDFError("Unknown movie to extend: %s\n", name);
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

static void MovieParseField(const char *field, const char *contents, int index, bool is_last)
{
#if (DDF_DEBUG)
    LogDebug("MOVIE_PARSE: %s = %s;\n", field, contents);
#endif
    EPI_UNUSED(index);
    EPI_UNUSED(is_last);
    if (DDFMainParseField(movie_commands, field, contents, (uint8_t *)dynamic_movie))
        return; // OK

    DDFError("Unknown movies.ddf command: %s\n", field);
}

static void MovieFinishEntry(void)
{
    if (dynamic_movie->type_ == kMovieDataNone)
        DDFError("No lump or packfile defined for %s!\n", dynamic_movie->name_.c_str());
}

static void MovieClearAll(void)
{
    LogWarning("Ignoring #CLEARALL in movies.ddf\n");
}

void DDFReadMovies(const std::string &data)
{
    DDFReadInfo movies;

    movies.tag      = "MOVIES";
    movies.lumpname = "DDFMOVIE";

    movies.start_entry  = MovieStartEntry;
    movies.parse_field  = MovieParseField;
    movies.finish_entry = MovieFinishEntry;
    movies.clear_all    = MovieClearAll;

    DDFMainReadFile(&movies, data);
}

void DDFMovieInit(void)
{
    for (MovieDefinition *movie : moviedefs)
    {
        delete movie;
        movie = nullptr;
    }
    moviedefs.clear();
}

void DDFMovieCleanUp(void)
{
    moviedefs.shrink_to_fit(); // <-- Reduce to allocated size
}

static void MovieParseInfo(const char *value)
{
    dynamic_movie->info_ = value;
}

static void DDFMovieGetType(const char *info, void *storage)
{
    EPI_UNUSED(storage);
    const char *colon = DDFMainDecodeList(info, ':', true);

    if (colon == nullptr || colon == info || (colon - info) >= 16 || colon[1] == 0)
        DDFError("Malformed movie type spec: %s\n", info);

    char keyword[20];

    strncpy(keyword, info, colon - info);
    keyword[colon - info] = 0;

    if (DDFCompareName(keyword, "LUMP") == 0)
    {
        dynamic_movie->type_ = kMovieDataLump;
        MovieParseInfo(colon + 1);
    }
    else if (DDFCompareName(keyword, "PACK") == 0)
    {
        dynamic_movie->type_ = kMovieDataPackage;
        MovieParseInfo(colon + 1);
    }
    else
        DDFError("Unknown movie type: %s\n", keyword);
}

static DDFSpecialFlags movie_specials[] = {{"MUTE", kMovieSpecialMute, 0}, {nullptr, 0, 0}};

static void DDFMovieGetSpecial(const char *info, void *storage)
{
    MovieSpecial *dest = (MovieSpecial *)storage;

    int flag_value;

    switch (DDFMainCheckSpecialFlag(info, movie_specials, &flag_value, false /* allow_prefixes */, false))
    {
    case kDDFCheckFlagPositive:
        *dest = (MovieSpecial)(*dest | flag_value);
        break;

    case kDDFCheckFlagNegative:
        *dest = (MovieSpecial)(*dest & ~flag_value);
        break;

    case kDDFCheckFlagUser:
    case kDDFCheckFlagUnknown:
        DDFWarnError("Unknown movie special: %s\n", info);
        break;
    }
}

static void DDFMovieGetScaling(const char *info, void *storage)
{
    MovieScaling *dest = (MovieScaling *)storage;

    if (DDFCompareName(info, "AUTO") == 0)
        *dest = kMovieScalingAutofit;
    if (DDFCompareName(info, "NONE") == 0)
        *dest = kMovieScalingNoScale;
    else if (DDFCompareName(info, "ZOOM") == 0)
        *dest = kMovieScalingZoom;
    else if (DDFCompareName(info, "STRETCH") == 0)
        *dest = kMovieScalingStretch;
    else
    {
        DDFWarnError("Unknown movie scaling mode: %s\n", info);
        *dest = kMovieScalingAutofit; // Default
    }
}

// ---> MovieDefinition class

MovieDefinition::MovieDefinition() : name_(), info_()
{
    Default();
}

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
    if (!refname || !refname[0])
        return nullptr;

    for (MovieDefinition *g : moviedefs)
    {
        if (DDFCompareName(g->name_.c_str(), refname) == 0)
            return g;
    }

    return nullptr;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
