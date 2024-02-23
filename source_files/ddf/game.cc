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

GameDefinitionContainer gamedefs;

static GameDefinition *dynamic_gamedef;

static IntermissionAnimationInfo buffer_animdef;
static IntermissionFrameInfo     buffer_framedef;

static void DDF_GameGetPic(const char *info, void *storage);
static void DDF_GameGetAnim(const char *info, void *storage);
static void DDF_GameGetMap(const char *info, void *storage);
static void DDF_GameGetLighting(const char *info, void *storage);

#define DDF_CMD_BASE dummy_gamedef
static GameDefinition dummy_gamedef;

static const DDFCommandList gamedef_commands[] = {
    DF("INTERMISSION_GRAPHIC", background_, DDF_MainGetLumpName),
    DF("INTERMISSION_CAMERA", bg_camera_, DDF_MainGetString),
    DF("INTERMISSION_MUSIC", music_, DDF_MainGetNumeric),
    DF("SPLAT_GRAPHIC", splatpic_, DDF_MainGetLumpName),
    DF("YAH1_GRAPHIC", you_are_here_[0], DDF_MainGetLumpName),
    DF("YAH2_GRAPHIC", you_are_here_[1], DDF_MainGetLumpName),
    DF("PERCENT_SOUND", percent_, DDF_MainLookupSound),
    DF("DONE_SOUND", done_, DDF_MainLookupSound),
    DF("ENDMAP_SOUND", endmap_, DDF_MainLookupSound),
    DF("NEXTMAP_SOUND", next_map_, DDF_MainLookupSound),
    DF("ACCEL_SOUND", accel_snd_, DDF_MainLookupSound),
    DF("FRAG_SOUND", frag_snd_, DDF_MainLookupSound),
    DF("FIRSTMAP", firstmap_, DDF_MainGetLumpName),
    DF("NAME_GRAPHIC", namegraphic_, DDF_MainGetLumpName),
    DF("TITLE_MOVIE", titlemovie_, DDF_MainGetString),
    DF("TITLE_MUSIC", titlemusic_, DDF_MainGetNumeric),
    DF("TITLE_TIME", titletics_, DDF_MainGetTime),
    DF("SPECIAL_MUSIC", special_music_, DDF_MainGetNumeric),
    DF("LIGHTING", lighting_, DDF_GameGetLighting),
    DF("DESCRIPTION", description_, DDF_MainGetString),
    DF("NO_SKILL_MENU", no_skill_menu_, DDF_MainGetBoolean),

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
        if (!dynamic_gamedef) DDF_Error("Unknown game to extend: %s\n", name);
        return;
    }

    if (dynamic_gamedef)
    {
        dynamic_gamedef->Default();
        return;
    }

    // not found, create a new one
    dynamic_gamedef        = new GameDefinition;
    dynamic_gamedef->name_ = name;

    gamedefs.push_back(dynamic_gamedef);
}

static void GameDoTemplate(const char *contents)
{
    GameDefinition *other = gamedefs.Lookup(contents);

    if (!other || other == dynamic_gamedef)
        DDF_Error("Unknown game template: '%s'\n", contents);

    dynamic_gamedef->CopyDetail(*other);
}

static void GameParseField(const char *field, const char *contents, int index,
                           bool is_last)
{
#if (DEBUG_DDF)
    LogDebug("GAME_PARSE: %s = %s;\n", field, contents);
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

    if (DDF_MainParseField(gamedef_commands, field, contents,
                           (uint8_t *)dynamic_gamedef))
        return;  // OK

    DDF_WarnError("Unknown games.ddf command: %s\n", field);
}

static void GameFinishEntry(void)
{
    // TODO: check stuff...
}

static void GameClearAll(void)
{
    // 100% safe to delete all game entries
    for (GameDefinition *game : gamedefs)
    {
        delete game;
        game = nullptr;
    }
    gamedefs.clear();
}

void DDF_ReadGames(const std::string &data)
{
    DDFReadInfo games;

    games.tag      = "GAMES";
    games.lumpname = "DDFGAME";

    games.start_entry  = GameStartEntry;
    games.parse_field  = GameParseField;
    games.finish_entry = GameFinishEntry;
    games.clear_all    = GameClearAll;

    DDF_MainReadFile(&games, data);
}

void DDF_GameInit(void) { GameClearAll(); }

void DDF_GameCleanUp(void)
{
    if (gamedefs.empty()) FatalError("There are no games defined in DDF !\n");
}

static void DDF_GameAddFrame(void)
{
    IntermissionFrameInfo *f = new IntermissionFrameInfo(buffer_framedef);

    buffer_animdef.frames_.push_back(f);

    buffer_framedef.Default();
}

static void DDF_GameAddAnim(void)
{
    IntermissionAnimationInfo *a = new IntermissionAnimationInfo(buffer_animdef);

    if (a->level_[0])
        a->type_ = IntermissionAnimationInfo::kIntermissionAnimationInfoLevel;
    else
        a->type_ = IntermissionAnimationInfo::kIntermissionAnimationInfoNormal;

    dynamic_gamedef->anims_.push_back(a);

    buffer_animdef.Default();
}

static void ParseFrame(const char *info, IntermissionFrameInfo *f)
{
    const char *p = strchr(info, ':');
    if (!p || p == info)
        DDF_Error("Bad frame def: '%s' (missing pic name)\n", info);

    f->pic_ = std::string(info, p - info);

    p++;

    if (sscanf(p, " %d : %d : %d ", &f->tics_, &f->x_, &f->y_) != 3)
        DDF_Error("Bad frame definition: '%s'\n", info);
}

static void DDF_GameGetAnim(const char *info, void *storage)
{
    IntermissionFrameInfo *f = (IntermissionFrameInfo *)storage;

    if (DDF_CompareName(info, "#END") == 0)
    {
        DDF_GameAddAnim();
        return;
    }

    const char *p = info;

    if (info[0] == '#')
    {
        if (buffer_animdef.frames_.size() > 0)
            DDF_Error("Invalid # command: '%s'\n", info);

        p = strchr(info, ':');
        if (!p || p <= info + 1) DDF_Error("Invalid # command: '%s'\n", info);

        buffer_animdef.level_ = std::string(info + 1, p - (info + 1));

        p++;
    }

    ParseFrame(p, f);

    // this assumes 'f' points to buffer_framedef
    DDF_GameAddFrame();
}

static void ParseMap(const char *info, IntermissionMapPositionInfo *mp)
{
    const char *p = strchr(info, ':');
    if (!p || p == info)
        DDF_Error("Bad map def: '%s' (missing level name)\n", info);

    mp->name_ = std::string(info, p - info);

    p++;

    if (sscanf(p, " %d : %d ", &mp->x_, &mp->y_) != 2)
        DDF_Error("Bad map definition: '%s'\n", info);
}

static void DDF_GameGetMap(const char *info, void *storage)
{
    IntermissionMapPositionInfo *mp = new IntermissionMapPositionInfo();

    ParseMap(info, mp);

    dynamic_gamedef->mappos_.push_back(mp);
}

static void DDF_GameGetPic(const char *info, void *storage)
{
    dynamic_gamedef->titlepics_.push_back(info);
}

static DDFSpecialFlags lighting_names[] = {{"DOOM", kLightingModelDoom, 0},
                                       {"DOOMISH", kLightingModelDoomish, 0},
                                       {"FLAT", kLightingModelFlat, 0},
                                       {"VERTEX", kLightingModelVertex, 0},
                                       {nullptr, 0, 0}};

void DDF_GameGetLighting(const char *info, void *storage)
{
    int flag_value;

    if (kDDFCheckFlagPositive != DDF_MainCheckSpecialFlag(info, lighting_names,
                                                  &flag_value, false, false))
    {
        DDF_WarnError("GAMES.DDF LIGHTING: Unknown model: %s", info);
        return;
    }

    ((LightingModel *)storage)[0] = (LightingModel)flag_value;
}

// --> world intermission mappos class

//
// wi_mapposdef_c Constructor
//
IntermissionMapPositionInfo::IntermissionMapPositionInfo() {}

//
// wi_mapposdef_c Copy constructor
//
IntermissionMapPositionInfo::IntermissionMapPositionInfo(IntermissionMapPositionInfo &rhs)
{
    Copy(rhs);
}

//
// wi_mapposdef_c Destructor
//
IntermissionMapPositionInfo::~IntermissionMapPositionInfo() {}

//
// wi_mapposdef_c::Copy()
//
void IntermissionMapPositionInfo::Copy(IntermissionMapPositionInfo &src)
{
    name_ = src.name_;
    x_    = src.x_;
    y_    = src.y_;
}

//
// wi_mapposdef_c assignment operator
//
IntermissionMapPositionInfo &IntermissionMapPositionInfo::operator=(
    IntermissionMapPositionInfo &rhs)
{
    if (&rhs != this) Copy(rhs);

    return *this;
}

// --> world intermission mappos container class

//
// wi_mapposdef_container_c Constructor
//
IntermissionMapPositionInfoContainer::IntermissionMapPositionInfoContainer() {}

//
// wi_mapposdef_container_c Copy constructor
//
IntermissionMapPositionInfoContainer::IntermissionMapPositionInfoContainer(
    IntermissionMapPositionInfoContainer &rhs)
{
    Copy(rhs);
}

//
// wi_mapposdef_container_c Destructor
//
IntermissionMapPositionInfoContainer::~IntermissionMapPositionInfoContainer()
{
    for (std::vector<IntermissionMapPositionInfo *>::iterator iter     = begin(),
                                                          iter_end = end();
         iter != iter_end; iter++)
    {
        IntermissionMapPositionInfo *wi = *iter;
        delete wi;
        wi = nullptr;
    }
}

//
// wi_mapposdef_container_c::Copy()
//
void IntermissionMapPositionInfoContainer::Copy(
    IntermissionMapPositionInfoContainer &src)
{
    for (IntermissionMapPositionInfo *wi : src)
    {
        if (wi)
        {
            IntermissionMapPositionInfo *wi2 = new IntermissionMapPositionInfo(*wi);
            push_back(wi2);
        }
    }
}

//
// wi_mapposdef_container_c assignment operator
//
IntermissionMapPositionInfoContainer &IntermissionMapPositionInfoContainer::operator=(
    IntermissionMapPositionInfoContainer &rhs)
{
    if (&rhs != this)
    {
        for (std::vector<IntermissionMapPositionInfo *>::iterator iter = begin(),
                                                              iter_end = end();
             iter != iter_end; iter++)
        {
            IntermissionMapPositionInfo *wi = *iter;
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
IntermissionFrameInfo::IntermissionFrameInfo() { Default(); }

//
// wi_framedef_c Copy constructor
//
IntermissionFrameInfo::IntermissionFrameInfo(IntermissionFrameInfo &rhs) { Copy(rhs); }

//
// wi_framedef_c Destructor
//
IntermissionFrameInfo::~IntermissionFrameInfo() {}

//
// wi_framedef_c::Copy()
//
void IntermissionFrameInfo::Copy(IntermissionFrameInfo &src)
{
    pic_  = src.pic_;
    tics_ = src.tics_;
    x_    = src.x_;
    y_    = src.y_;
}

//
// wi_framedef_c::Default()
//
void IntermissionFrameInfo::Default()
{
    pic_.clear();
    tics_ = 0;
    x_ = y_ = 0;
}

//
// wi_framedef_c assignment operator
//
IntermissionFrameInfo &IntermissionFrameInfo::operator=(IntermissionFrameInfo &rhs)
{
    if (&rhs != this) Copy(rhs);

    return *this;
}

// --> world intermission framedef container class

//
// wi_framedef_container_c Constructor
//
IntermissionFrameInfoContainer::IntermissionFrameInfoContainer() {}

//
// wi_framedef_container_c Copy constructor
//
IntermissionFrameInfoContainer::IntermissionFrameInfoContainer(
    IntermissionFrameInfoContainer &rhs)
{
    Copy(rhs);
}

//
// wi_framedef_container_c Destructor
//
IntermissionFrameInfoContainer::~IntermissionFrameInfoContainer()
{
    for (std::vector<IntermissionFrameInfo *>::iterator iter     = begin(),
                                                    iter_end = end();
         iter != iter_end; iter++)
    {
        IntermissionFrameInfo *wi = *iter;
        delete wi;
        wi = nullptr;
    }
}

//
// wi_framedef_container_c::Copy()
//
void IntermissionFrameInfoContainer::Copy(IntermissionFrameInfoContainer &src)
{
    for (IntermissionFrameInfo *f : src)
    {
        if (f)
        {
            IntermissionFrameInfo *f2 = new IntermissionFrameInfo(*f);
            push_back(f2);
        }
    }
}

//
// wi_framedef_container_c assignment operator
//
IntermissionFrameInfoContainer &IntermissionFrameInfoContainer::operator=(
    IntermissionFrameInfoContainer &rhs)
{
    if (&rhs != this)
    {
        for (std::vector<IntermissionFrameInfo *>::iterator iter     = begin(),
                                                        iter_end = end();
             iter != iter_end; iter++)
        {
            IntermissionFrameInfo *wi = *iter;
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
IntermissionAnimationInfo::IntermissionAnimationInfo() { Default(); }

//
// wi_animdef_c Copy constructor
//
IntermissionAnimationInfo::IntermissionAnimationInfo(IntermissionAnimationInfo &rhs)
{
    Copy(rhs);
}

//
// wi_animdef_c Destructor
//
IntermissionAnimationInfo::~IntermissionAnimationInfo() {}

//
// void Copy()
//
void IntermissionAnimationInfo::Copy(IntermissionAnimationInfo &src)
{
    type_   = src.type_;
    level_  = src.level_;
    frames_ = src.frames_;
}

//
// wi_animdef_c::Default()
//
void IntermissionAnimationInfo::Default()
{
    type_ = kIntermissionAnimationInfoNormal;
    level_.clear();

    for (IntermissionFrameInfo *frame : frames_)
    {
        delete frame;
        frame = nullptr;
    }
    frames_.clear();
}

//
// wi_animdef_c assignment operator
//
IntermissionAnimationInfo &IntermissionAnimationInfo::operator=(
    IntermissionAnimationInfo &rhs)
{
    if (&rhs != this) Copy(rhs);

    return *this;
}

// --> world intermission anim container class

//
// wi_animdef_container_c Constructor
//
IntermissionAnimationInfoContainer::IntermissionAnimationInfoContainer() {}

//
// wi_animdef_container_c Copy constructor
//
IntermissionAnimationInfoContainer::IntermissionAnimationInfoContainer(
    IntermissionAnimationInfoContainer &rhs)
{
    Copy(rhs);
}

//
// wi_animdef_container_c Destructor
//
IntermissionAnimationInfoContainer::~IntermissionAnimationInfoContainer()
{
    for (std::vector<IntermissionAnimationInfo *>::iterator iter     = begin(),
                                                        iter_end = end();
         iter != iter_end; iter++)
    {
        IntermissionAnimationInfo *wi = *iter;
        delete wi;
        wi = nullptr;
    }
}

//
// wi_animdef_container_c::Copy()
//
void IntermissionAnimationInfoContainer::Copy(IntermissionAnimationInfoContainer &src)
{
    for (IntermissionAnimationInfo *a : src)
    {
        if (a)
        {
            IntermissionAnimationInfo *a2 = new IntermissionAnimationInfo(*a);
            push_back(a2);
        }
    }
}

//
// wi_animdef_container_c assignment operator
//
IntermissionAnimationInfoContainer &IntermissionAnimationInfoContainer::operator=(
    IntermissionAnimationInfoContainer &rhs)
{
    if (&rhs != this)
    {
        for (std::vector<IntermissionAnimationInfo *>::iterator iter     = begin(),
                                                            iter_end = end();
             iter != iter_end; iter++)
        {
            IntermissionAnimationInfo *wi = *iter;
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
GameDefinition::GameDefinition() : name_(), titlepics_() { Default(); }

//
// gamedef_c Destructor
//
GameDefinition::~GameDefinition() {}

//
// gamedef_c::CopyDetail()
//
void GameDefinition::CopyDetail(GameDefinition &src)
{
    anims_  = src.anims_;
    mappos_ = src.mappos_;

    background_ = src.background_;
    splatpic_   = src.splatpic_;

    you_are_here_[0] = src.you_are_here_[0];
    you_are_here_[1] = src.you_are_here_[1];

    bg_camera_ = src.bg_camera_;
    music_     = src.music_;

    percent_       = src.percent_;
    done_          = src.done_;
    endmap_        = src.endmap_;
    next_map_       = src.next_map_;
    accel_snd_     = src.accel_snd_;
    frag_snd_      = src.frag_snd_;
    no_skill_menu_ = src.no_skill_menu_;

    firstmap_    = src.firstmap_;
    namegraphic_ = src.namegraphic_;

    titlepics_  = src.titlepics_;
    titlemovie_ = src.titlemovie_;
    titlemusic_ = src.titlemusic_;
    titletics_  = src.titletics_;

    special_music_ = src.special_music_;
    lighting_      = src.lighting_;
    description_   = src.description_;
}

//
// gamedef_c::Default()
//
void GameDefinition::Default()
{
    for (IntermissionAnimationInfo *a : anims_)
    {
        delete a;
        a = nullptr;
    }
    anims_.clear();
    for (IntermissionMapPositionInfo *m : mappos_)
    {
        delete m;
        m = nullptr;
    }
    mappos_.clear();

    background_.clear();
    splatpic_.clear();

    you_are_here_[0].clear();
    you_are_here_[1].clear();

    bg_camera_.clear();
    music_         = 0;
    no_skill_menu_ = false;

    percent_   = nullptr;
    done_      = nullptr;
    endmap_    = nullptr;
    next_map_   = nullptr;
    accel_snd_ = nullptr;
    frag_snd_  = nullptr;

    firstmap_.clear();
    namegraphic_.clear();

    titlepics_.clear();
    titlemovie_.clear();
    movie_played_ = false;
    titlemusic_   = 0;
    titletics_    = kTicRate * 4;

    special_music_ = 0;
    lighting_      = kLightingModelDoomish;
    description_.clear();
}

// --> game definition container class

//
// gamedef_container_c Constructor
//
GameDefinitionContainer::GameDefinitionContainer() {}

//
// gamedef_container_c Destructor
//
GameDefinitionContainer::~GameDefinitionContainer()
{
    for (std::vector<GameDefinition *>::iterator iter     = begin(),
                                                 iter_end = end();
         iter != iter_end; iter++)
    {
        GameDefinition *game = *iter;
        delete game;
        game = nullptr;
    }
}

//
// gamedef_c* gamedef_container_c::Lookup()
//
// Looks an gamedef by name, returns a fatal error if it does not exist.
//
GameDefinition *GameDefinitionContainer::Lookup(const char *refname)
{
    if (!refname || !refname[0]) return nullptr;

    for (std::vector<GameDefinition *>::iterator iter     = begin(),
                                                 iter_end = end();
         iter != iter_end; iter++)
    {
        GameDefinition *game = *iter;
        if (DDF_CompareName(game->name_.c_str(), refname) == 0) return game;
    }

    return nullptr;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
