//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Game settings)
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
//
// Overall Game Setup and Parser Code
//

#include <string.h>

#include "local.h"

#undef DF
#define DF DDF_FIELD

gamedef_container_c gamedefs;

static gamedef_c *dynamic_gamedef;

static wi_animdef_c  buffer_animdef;
static wi_framedef_c buffer_framedef;

static void DDF_GameGetPic(const char *info, void *storage);
static void DDF_GameGetAnim(const char *info, void *storage);
static void DDF_GameGetMap(const char *info, void *storage);
static void DDF_GameGetLighting(const char *info, void *storage);

#define DDF_CMD_BASE dummy_gamedef
static gamedef_c dummy_gamedef;

static const commandlist_t gamedef_commands[] = {DF("INTERMISSION_GRAPHIC", background, DDF_MainGetLumpName),
                                                 DF("INTERMISSION_CAMERA", bg_camera, DDF_MainGetString),
                                                 DF("INTERMISSION_MUSIC", music, DDF_MainGetNumeric),
                                                 DF("SPLAT_GRAPHIC", splatpic, DDF_MainGetLumpName),
                                                 DF("YAH1_GRAPHIC", yah[0], DDF_MainGetLumpName),
                                                 DF("YAH2_GRAPHIC", yah[1], DDF_MainGetLumpName),
                                                 DF("PERCENT_SOUND", percent, DDF_MainLookupSound),
                                                 DF("DONE_SOUND", done, DDF_MainLookupSound),
                                                 DF("ENDMAP_SOUND", endmap, DDF_MainLookupSound),
                                                 DF("NEXTMAP_SOUND", nextmap, DDF_MainLookupSound),
                                                 DF("ACCEL_SOUND", accel_snd, DDF_MainLookupSound),
                                                 DF("FRAG_SOUND", frag_snd, DDF_MainLookupSound),
                                                 DF("FIRSTMAP", firstmap, DDF_MainGetLumpName),
                                                 DF("NAME_GRAPHIC", namegraphic, DDF_MainGetLumpName),
                                                 DF("TITLE_MOVIE", titlemovie, DDF_MainGetString),
                                                 DF("TITLE_MUSIC", titlemusic, DDF_MainGetNumeric),
                                                 DF("TITLE_TIME", titletics, DDF_MainGetTime),
                                                 DF("SPECIAL_MUSIC", special_music, DDF_MainGetNumeric),
                                                 DF("LIGHTING", lighting, DDF_GameGetLighting),
                                                 DF("DESCRIPTION", description, DDF_MainGetString),
                                                 DF("NO_SKILL_MENU", no_skill_menu, DDF_MainGetBoolean),

                                                 DDF_CMD_END};

//
//  DDF PARSE ROUTINES
//

static void GameStartEntry(const char *name, bool extend)
{
    if (!name || !name[0])
    {
        DDF_WarnError("New game entry is missing a name!");
        name = "GAME_WITH_NO_NAME";
    }

    // instantiate the static entries
    buffer_animdef.Default();
    buffer_framedef.Default();

    // replaces an existing entry?
    dynamic_gamedef = gamedefs.Lookup(name);

    if (extend)
    {
        if (!dynamic_gamedef)
            DDF_Error("Unknown game to extend: %s\n", name);
        return;
    }

    if (dynamic_gamedef)
    {
        dynamic_gamedef->Default();
        return;
    }

    // not found, create a new one
    dynamic_gamedef       = new gamedef_c;
    dynamic_gamedef->name = name;

    gamedefs.push_back(dynamic_gamedef);
}

static void GameDoTemplate(const char *contents)
{
    gamedef_c *other = gamedefs.Lookup(contents);

    if (!other || other == dynamic_gamedef)
        DDF_Error("Unknown game template: '%s'\n", contents);

    dynamic_gamedef->CopyDetail(*other);
}

static void GameParseField(const char *field, const char *contents, int index, bool is_last)
{
#if (DEBUG_DDF)
    I_Debugf("GAME_PARSE: %s = %s;\n", field, contents);
#endif

    if (DDF_CompareName(field, "TEMPLATE") == 0)
    {
        GameDoTemplate(contents);
        return;
    }

    // handle some special fields...
    if (DDF_CompareName(field, "TITLE_GRAPHIC") == 0)
    {
        DDF_GameGetPic(contents, nullptr);
        return;
    }
    else if (DDF_CompareName(field, "MAP") == 0)
    {
        DDF_GameGetMap(contents, nullptr);
        return;
    }
    else if (DDF_CompareName(field, "ANIM") == 0)
    {
        DDF_GameGetAnim(contents, &buffer_framedef);
        return;
    }

    if (DDF_MainParseField(gamedef_commands, field, contents, (uint8_t *)dynamic_gamedef))
        return; // OK

    DDF_WarnError("Unknown games.ddf command: %s\n", field);
}

static void GameFinishEntry(void)
{
    // TODO: check stuff...
}

static void GameClearAll(void)
{
    // 100% safe to delete all game entries
    for (auto game : gamedefs)
    {
        delete game;
        game = nullptr;
    }
    gamedefs.clear();
}

void DDF_ReadGames(const std::string &data)
{
    readinfo_t games;

    games.tag      = "GAMES";
    games.lumpname = "DDFGAME";

    games.start_entry  = GameStartEntry;
    games.parse_field  = GameParseField;
    games.finish_entry = GameFinishEntry;
    games.clear_all    = GameClearAll;

    DDF_MainReadFile(&games, data);
}

void DDF_GameInit(void)
{
    GameClearAll();
}

void DDF_GameCleanUp(void)
{
    if (gamedefs.empty())
        I_Error("There are no games defined in DDF !\n");
}

static void DDF_GameAddFrame(void)
{
    wi_framedef_c *f = new wi_framedef_c(buffer_framedef);

    buffer_animdef.frames.push_back(f);

    buffer_framedef.Default();
}

static void DDF_GameAddAnim(void)
{
    wi_animdef_c *a = new wi_animdef_c(buffer_animdef);

    if (a->level[0])
        a->type = wi_animdef_c::WI_LEVEL;
    else
        a->type = wi_animdef_c::WI_NORMAL;

    dynamic_gamedef->anims.push_back(a);

    buffer_animdef.Default();
}

static void ParseFrame(const char *info, wi_framedef_c *f)
{
    const char *p = strchr(info, ':');
    if (!p || p == info)
        DDF_Error("Bad frame def: '%s' (missing pic name)\n", info);

    f->pic = std::string(info, p - info);

    p++;

    if (sscanf(p, " %d : %d : %d ", &f->tics, &f->x, &f->y) != 3)
        DDF_Error("Bad frame definition: '%s'\n", info);
}

static void DDF_GameGetAnim(const char *info, void *storage)
{
    wi_framedef_c *f = (wi_framedef_c *)storage;

    if (DDF_CompareName(info, "#END") == 0)
    {
        DDF_GameAddAnim();
        return;
    }

    const char *p = info;

    if (info[0] == '#')
    {
        if (buffer_animdef.frames.size() > 0)
            DDF_Error("Invalid # command: '%s'\n", info);

        p = strchr(info, ':');
        if (!p || p <= info + 1)
            DDF_Error("Invalid # command: '%s'\n", info);

        buffer_animdef.level = std::string(info + 1, p - (info + 1));

        p++;
    }

    ParseFrame(p, f);

    // this assumes 'f' points to buffer_framedef
    DDF_GameAddFrame();
}

static void ParseMap(const char *info, wi_mapposdef_c *mp)
{
    const char *p = strchr(info, ':');
    if (!p || p == info)
        DDF_Error("Bad map def: '%s' (missing level name)\n", info);

    mp->name = std::string(info, p - info);

    p++;

    if (sscanf(p, " %d : %d ", &mp->x, &mp->y) != 2)
        DDF_Error("Bad map definition: '%s'\n", info);
}

static void DDF_GameGetMap(const char *info, void *storage)
{
    wi_mapposdef_c *mp = new wi_mapposdef_c();

    ParseMap(info, mp);

    dynamic_gamedef->mappos.push_back(mp);
}

static void DDF_GameGetPic(const char *info, void *storage)
{
    dynamic_gamedef->titlepics.push_back(info);
}

static specflags_t lighting_names[] = {{"DOOM", LMODEL_Doom, 0},
                                       {"DOOMISH", LMODEL_Doomish, 0},
                                       {"FLAT", LMODEL_Flat, 0},
                                       {"VERTEX", LMODEL_Vertex, 0},
                                       {nullptr, 0, 0}};

void DDF_GameGetLighting(const char *info, void *storage)
{
    int flag_value;

    if (CHKF_Positive != DDF_MainCheckSpecialFlag(info, lighting_names, &flag_value, false, false))
    {
        DDF_WarnError("GAMES.DDF LIGHTING: Unknown model: %s", info);
        return;
    }

    ((lighting_model_e *)storage)[0] = (lighting_model_e)flag_value;
}

// --> world intermission mappos class

//
// wi_mapposdef_c Constructor
//
wi_mapposdef_c::wi_mapposdef_c()
{
}

//
// wi_mapposdef_c Copy constructor
//
wi_mapposdef_c::wi_mapposdef_c(wi_mapposdef_c &rhs)
{
    Copy(rhs);
}

//
// wi_mapposdef_c Destructor
//
wi_mapposdef_c::~wi_mapposdef_c()
{
}

//
// wi_mapposdef_c::Copy()
//
void wi_mapposdef_c::Copy(wi_mapposdef_c &src)
{
    name = src.name;
    x    = src.x;
    y    = src.y;
}

//
// wi_mapposdef_c assignment operator
//
wi_mapposdef_c &wi_mapposdef_c::operator=(wi_mapposdef_c &rhs)
{
    if (&rhs != this)
        Copy(rhs);

    return *this;
}

// --> world intermission mappos container class

//
// wi_mapposdef_container_c Constructor
//
wi_mapposdef_container_c::wi_mapposdef_container_c()
{
}

//
// wi_mapposdef_container_c Copy constructor
//
wi_mapposdef_container_c::wi_mapposdef_container_c(wi_mapposdef_container_c &rhs)
{
    Copy(rhs);
}

//
// wi_mapposdef_container_c Destructor
//
wi_mapposdef_container_c::~wi_mapposdef_container_c()
{
    for (auto iter = begin(); iter != end(); iter++)
    {
        wi_mapposdef_c *wi = *iter;
        delete wi;
        wi = nullptr;
    }
}

//
// wi_mapposdef_container_c::Copy()
//
void wi_mapposdef_container_c::Copy(wi_mapposdef_container_c &src)
{
    for (auto wi : src)
    {
        if (wi)
        {
            wi_mapposdef_c *wi2 = new wi_mapposdef_c(*wi);
            push_back(wi2);
        }
    }
}

//
// wi_mapposdef_container_c assignment operator
//
wi_mapposdef_container_c &wi_mapposdef_container_c::operator=(wi_mapposdef_container_c &rhs)
{
    if (&rhs != this)
    {
        for (auto iter = begin(); iter != end(); iter++)
        {
            wi_mapposdef_c *wi = *iter;
            delete wi;
            wi = nullptr;
        }
        clear();
        Copy(rhs);
    }

    return *this;
}

// --> world intermission framedef class

//
// wi_framedef_c Constructor
//
wi_framedef_c::wi_framedef_c()
{
    Default();
}

//
// wi_framedef_c Copy constructor
//
wi_framedef_c::wi_framedef_c(wi_framedef_c &rhs)
{
    Copy(rhs);
}

//
// wi_framedef_c Destructor
//
wi_framedef_c::~wi_framedef_c()
{
}

//
// wi_framedef_c::Copy()
//
void wi_framedef_c::Copy(wi_framedef_c &src)
{
    pic  = src.pic;
    tics = src.tics;
    x    = src.x;
    y    = src.y;
}

//
// wi_framedef_c::Default()
//
void wi_framedef_c::Default()
{
    pic.clear();
    tics = 0;
    x = y = 0;
}

//
// wi_framedef_c assignment operator
//
wi_framedef_c &wi_framedef_c::operator=(wi_framedef_c &rhs)
{
    if (&rhs != this)
        Copy(rhs);

    return *this;
}

// --> world intermission framedef container class

//
// wi_framedef_container_c Constructor
//
wi_framedef_container_c::wi_framedef_container_c()
{
}

//
// wi_framedef_container_c Copy constructor
//
wi_framedef_container_c::wi_framedef_container_c(wi_framedef_container_c &rhs)
{
    Copy(rhs);
}

//
// wi_framedef_container_c Destructor
//
wi_framedef_container_c::~wi_framedef_container_c()
{
    for (auto iter = begin(); iter != end(); iter++)
    {
        wi_framedef_c *wi = *iter;
        delete wi;
        wi = nullptr;
    }
}

//
// wi_framedef_container_c::Copy()
//
void wi_framedef_container_c::Copy(wi_framedef_container_c &src)
{
    for (auto f : src)
    {
        if (f)
        {
            wi_framedef_c *f2 = new wi_framedef_c(*f);
            push_back(f2);
        }
    }
}

//
// wi_framedef_container_c assignment operator
//
wi_framedef_container_c &wi_framedef_container_c::operator=(wi_framedef_container_c &rhs)
{
    if (&rhs != this)
    {
        for (auto iter = begin(); iter != end(); iter++)
        {
            wi_framedef_c *wi = *iter;
            delete wi;
            wi = nullptr;
        }
        clear();
        Copy(rhs);
    }

    return *this;
}

// --> world intermission animdef class

//
// wi_animdef_c Constructor
//
wi_animdef_c::wi_animdef_c()
{
    Default();
}

//
// wi_animdef_c Copy constructor
//
wi_animdef_c::wi_animdef_c(wi_animdef_c &rhs)
{
    Copy(rhs);
}

//
// wi_animdef_c Destructor
//
wi_animdef_c::~wi_animdef_c()
{
}

//
// void Copy()
//
void wi_animdef_c::Copy(wi_animdef_c &src)
{
    type   = src.type;
    level  = src.level;
    frames = src.frames;
}

//
// wi_animdef_c::Default()
//
void wi_animdef_c::Default()
{
    type = WI_NORMAL;
    level.clear();

    for (auto frame : frames)
    {
        delete frame;
        frame = nullptr;
    }
    frames.clear();
}

//
// wi_animdef_c assignment operator
//
wi_animdef_c &wi_animdef_c::operator=(wi_animdef_c &rhs)
{
    if (&rhs != this)
        Copy(rhs);

    return *this;
}

// --> world intermission anim container class

//
// wi_animdef_container_c Constructor
//
wi_animdef_container_c::wi_animdef_container_c()
{
}

//
// wi_animdef_container_c Copy constructor
//
wi_animdef_container_c::wi_animdef_container_c(wi_animdef_container_c &rhs)
{
    Copy(rhs);
}

//
// wi_animdef_container_c Destructor
//
wi_animdef_container_c::~wi_animdef_container_c()
{
    for (auto iter = begin(); iter != end(); iter++)
    {
        wi_animdef_c *wi = *iter;
        delete wi;
        wi = nullptr;
    }
}

//
// wi_animdef_container_c::Copy()
//
void wi_animdef_container_c::Copy(wi_animdef_container_c &src)
{
    for (auto a : src)
    {
        if (a)
        {
            wi_animdef_c *a2 = new wi_animdef_c(*a);
            push_back(a2);
        }
    }
}

//
// wi_animdef_container_c assignment operator
//
wi_animdef_container_c &wi_animdef_container_c::operator=(wi_animdef_container_c &rhs)
{
    if (&rhs != this)
    {
        for (auto iter = begin(); iter != end(); iter++)
        {
            wi_animdef_c *wi = *iter;
            delete wi;
            wi = nullptr;
        }
        clear();
        Copy(rhs);
    }

    return *this;
}

// --> game definition class

//
// gamedef_c Constructor
//
gamedef_c::gamedef_c() : name(), titlepics()
{
    Default();
}

//
// gamedef_c Destructor
//
gamedef_c::~gamedef_c()
{
}

//
// gamedef_c::CopyDetail()
//
void gamedef_c::CopyDetail(gamedef_c &src)
{
    anims  = src.anims;
    mappos = src.mappos;

    background = src.background;
    splatpic   = src.splatpic;

    yah[0] = src.yah[0];
    yah[1] = src.yah[1];

    bg_camera = src.bg_camera;
    music     = src.music;

    percent       = src.percent;
    done          = src.done;
    endmap        = src.endmap;
    nextmap       = src.nextmap;
    accel_snd     = src.accel_snd;
    frag_snd      = src.frag_snd;
    no_skill_menu = src.no_skill_menu;

    firstmap    = src.firstmap;
    namegraphic = src.namegraphic;

    titlepics  = src.titlepics;
    titlemovie = src.titlemovie;
    titlemusic = src.titlemusic;
    titletics  = src.titletics;

    special_music = src.special_music;
    lighting      = src.lighting;
    description   = src.description;
}

//
// gamedef_c::Default()
//
void gamedef_c::Default()
{
    for (auto a : anims)
    {
        delete a;
        a = nullptr;
    }
    anims.clear();
    for (auto m : mappos)
    {
        delete m;
        m = nullptr;
    }
    mappos.clear();

    background.clear();
    splatpic.clear();

    yah[0].clear();
    yah[1].clear();

    bg_camera.clear();
    music         = 0;
    no_skill_menu = false;

    percent   = sfx_None;
    done      = sfx_None;
    endmap    = sfx_None;
    nextmap   = sfx_None;
    accel_snd = sfx_None;
    frag_snd  = sfx_None;

    firstmap.clear();
    namegraphic.clear();

    titlepics.clear();
    titlemovie.clear();
    movie_played = false;
    titlemusic = 0;
    titletics  = TICRATE * 4;

    special_music = 0;
    lighting      = LMODEL_Doomish;
    description.clear();
}

// --> game definition container class

//
// gamedef_container_c Constructor
//
gamedef_container_c::gamedef_container_c()
{
}

//
// gamedef_container_c Destructor
//
gamedef_container_c::~gamedef_container_c()
{
    for (auto iter = begin(); iter != end(); iter++)
    {
        gamedef_c *game = *iter;
        delete game;
        game = nullptr;
    }
}

//
// gamedef_c* gamedef_container_c::Lookup()
//
// Looks an gamedef by name, returns a fatal error if it does not exist.
//
gamedef_c *gamedef_container_c::Lookup(const char *refname)
{
    if (!refname || !refname[0])
        return nullptr;

    for (auto iter = begin(); iter != end(); iter++)
    {
        gamedef_c *game = *iter;
        if (DDF_CompareName(game->name.c_str(), refname) == 0)
            return game;
    }

    return nullptr;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
