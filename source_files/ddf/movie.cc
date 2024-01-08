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

#include "local.h"

#include "path.h"

#include "movie.h"

static moviedef_c *dynamic_movie;

static void DDF_MovieGetType(const char *info, void *storage);
static void DDF_MovieGetSpecial(const char *info, void *storage);
static void DDF_MovieGetScaling(const char *info, void *storage);

#define DDF_CMD_BASE dummy_movie
static moviedef_c dummy_movie;

static const commandlist_t movie_commands[] = {DDF_FIELD("MOVIE_DATA", type, DDF_MovieGetType),
                                               DDF_FIELD("SPECIAL", special, DDF_MovieGetSpecial),
                                               DDF_FIELD("SCALING", scaling, DDF_MovieGetScaling),
 
                                               DDF_CMD_END};

moviedef_container_c moviedefs;

//
//  DDF PARSE ROUTINES
//

static void MovieStartEntry(const char *name, bool extend)
{
    if (!name || !name[0])
        DDF_Error("New movie entry is missing a name!\n");

    dynamic_movie = moviedefs.Lookup(name);

    if (extend)
    {
        if (!dynamic_movie)
            DDF_Error("Unknown movie to extend: %s\n", name);
        return;
    }

    // replaces an existing entry?
    if (dynamic_movie)
    {
        dynamic_movie->Default();
        return;
    }

    // not found, create a new one
    dynamic_movie = new moviedef_c;

    dynamic_movie->name   = name;

    moviedefs.push_back(dynamic_movie);
}

static void MovieParseField(const char *field, const char *contents, int index, bool is_last)
{
#if (DEBUG_DDF)
    I_Debugf("MOVIE_PARSE: %s = %s;\n", field, contents);
#endif

    if (DDF_MainParseField(movie_commands, field, contents, (uint8_t *)dynamic_movie))
        return; // OK

    DDF_Error("Unknown movies.ddf command: %s\n", field);
}

static void MovieFinishEntry(void)
{
    if (dynamic_movie->type == MOVDT_None)
        DDF_Error("No lump or packfile defined for %s!\n", dynamic_movie->name.c_str());
}

static void MovieClearAll(void)
{
    I_Warning("Ignoring #CLEARALL in movies.ddf\n");
}

void DDF_ReadMovies(const std::string &data)
{
    readinfo_t movies;

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
    for (auto movie : moviedefs)
    {
        delete movie;
        movie = nullptr;
    }
    moviedefs.clear();
}

void DDF_MovieCleanUp(void)
{
    moviedefs.shrink_to_fit(); // <-- Reduce to allocated size
}

static void MovieParseInfo(const char *value)
{
    dynamic_movie->info = value;
}

static void DDF_MovieGetType(const char *info, void *storage)
{
    const char *colon = DDF_MainDecodeList(info, ':', true);

    if (colon == NULL || colon == info || (colon - info) >= 16 || colon[1] == 0)
        DDF_Error("Malformed movie type spec: %s\n", info);

    char keyword[20];

    strncpy(keyword, info, colon - info);
    keyword[colon - info] = 0;

    if (DDF_CompareName(keyword, "LUMP") == 0)
    {
        dynamic_movie->type = MOVDT_Lump;
        MovieParseInfo(colon + 1);
    }
    else if (DDF_CompareName(keyword, "PACK") == 0)
    {
        dynamic_movie->type = MOVDT_Package;
        MovieParseInfo(colon + 1);
    }
    else
        DDF_Error("Unknown movie type: %s\n", keyword);
}

static specflags_t movie_specials[] = {{"MUTE", MOVSP_Mute, 0}, {NULL, 0, 0}};

static void DDF_MovieGetSpecial(const char *info, void *storage)
{
    movie_special_e *dest = (movie_special_e *)storage;

    int flag_value;

    switch (DDF_MainCheckSpecialFlag(info, movie_specials, &flag_value, false /* allow_prefixes */, false))
    {
    case CHKF_Positive:
        *dest = (movie_special_e)(*dest | flag_value);
        break;

    case CHKF_Negative:
        *dest = (movie_special_e)(*dest & ~flag_value);
        break;

    case CHKF_User:
    case CHKF_Unknown:
        DDF_WarnError("Unknown movie special: %s\n", info);
        break;
    }
}

static void DDF_MovieGetScaling(const char *info, void *storage)
{
    moviescale_type_e *dest = (moviescale_type_e *)storage;

    if (DDF_CompareName(info, "AUTO") == 0)
        *dest = MOVSC_Autofit;
    if (DDF_CompareName(info, "NONE") == 0)
        *dest = MOVSC_NoScale;
    else if (DDF_CompareName(info, "ZOOM") == 0)
        *dest = MOVSC_Zoom;
    else if (DDF_CompareName(info, "STRETCH") == 0)
        *dest = MOVSC_Stretch;
    else
    {
        DDF_WarnError("Unknown movie scaling mode: %s\n", info);
        *dest = MOVSC_Autofit; // Default
    }
}

// ---> moviedef_c class

moviedef_c::moviedef_c() : name(), info()
{
    Default();
}

//
// Copies all the detail with the exception of ddf info
//
void moviedef_c::CopyDetail(const moviedef_c &src)
{
    type   = src.type;
    info   = src.info;
    scaling = src.scaling;
    special = src.special;
}

void moviedef_c::Default()
{
    info.clear();

    type   = MOVDT_None;

    scaling = MOVSC_Autofit;
    special  = MOVSP_None;
}

// ---> moviedef_container_c class

moviedef_c *moviedef_container_c::Lookup(const char *refname)
{
    if (!refname || !refname[0])
        return NULL;

    for (auto g : moviedefs)
    {
        if (DDF_CompareName(g->name.c_str(), refname) == 0)
            return g;
    }

    return NULL;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
