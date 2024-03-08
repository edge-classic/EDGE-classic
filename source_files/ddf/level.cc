//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Level Defines)
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
// Level Setup and Parser Code
//

#include "level.h"

#include "colormap.h"
#include "local.h"
#include "str_compare.h"
// engine code (FIXME don't do wad shit here)
#include "w_wad.h"

#undef DF
#define DF DDF_FIELD

static void DDF_LevelGetSpecials(const char *info);
static void DDF_LevelGetPic(const char *info, void *storage);
static void DDF_LevelGetSkyStretch(const char *info, void *storage);
static void DDF_LevelGetWistyle(const char *info, void *storage);

MapDefinitionContainer mapdefs;

#undef DDF_CMD_BASE
#define DDF_CMD_BASE dummy_finale
static FinaleDefinition dummy_finale;

static const DDFCommandList finale_commands[] = {
    DF("TEXT", text_, DDF_MainGetString),
    DF("TEXT_GRAPHIC", text_back_, DDF_MainGetLumpName),
    DF("TEXT_FLAT", text_flat_, DDF_MainGetLumpName),
    DF("TEXT_SPEED", text_speed_, DDF_MainGetFloat),
    DF("TEXT_WAIT", text_wait_, DDF_MainGetNumeric),
    DF("COLOURMAP", text_colmap_, DDF_MainGetColourmap),
    DF("GRAPHIC", pics_, DDF_LevelGetPic),
    DF("GRAPHIC_WAIT", picwait_, DDF_MainGetTime),
    DF("MOVIE", movie_, DDF_MainGetString),
    DF("CAST", docast_, DDF_MainGetBoolean),
    DF("BUNNY", dobunny_, DDF_MainGetBoolean),
    DF("MUSIC", music_, DDF_MainGetNumeric),

    DDF_CMD_END};

// -KM- 1998/11/25 Finales are all go.

static MapDefinition *dynamic_level;

#undef DDF_CMD_BASE
#define DDF_CMD_BASE dummy_level
static MapDefinition dummy_level;

static const DDFCommandList level_commands[] = {
    // sub-commands
    DDF_SUB_LIST("PRE", f_pre_, finale_commands),
    DDF_SUB_LIST("END", f_end_, finale_commands),

    DF("LUMPNAME", lump_, DDF_MainGetLumpName),
    DF("DESCRIPTION", description_, DDF_MainGetString),
    DF("AUTHOR", author_, DDF_MainGetString),
    DF("NAME_GRAPHIC", namegraphic_, DDF_MainGetLumpName),
    DF("SKY_TEXTURE", sky_, DDF_MainGetLumpName),
    DF("SKY_STRETCH", forced_skystretch_, DDF_LevelGetSkyStretch),
    DF("MUSIC_ENTRY", music_, DDF_MainGetNumeric),
    DF("SURROUND_FLAT", surround_, DDF_MainGetLumpName),
    DF("NEXT_MAP", next_mapname_, DDF_MainGetLumpName),
    DF("SECRET_MAP", secretmapname_, DDF_MainGetLumpName),
    DF("AUTOTAG", autotag_, DDF_MainGetNumeric),
    DF("PARTIME", partime_, DDF_MainGetTime),
    DF("EPISODE", episode_name_, DDF_MainGetString),
    DF("STATS", wistyle_, DDF_LevelGetWistyle),
    DF("LEAVING_BACKGROUND", leavingbggraphic_, DDF_MainGetLumpName),
    DF("ENTERING_BACKGROUND", enteringbggraphic_, DDF_MainGetLumpName),
    DF("INDOOR_FOG_COLOR", indoor_fog_cmap_, DDF_MainGetColourmap),
    DF("INDOOR_FOG_DENSITY", indoor_fog_density_, DDF_MainGetPercent),
    DF("OUTDOOR_FOG_COLOR", outdoor_fog_cmap_, DDF_MainGetColourmap),
    DF("OUTDOOR_FOG_DENSITY", outdoor_fog_density_, DDF_MainGetPercent),

    DDF_CMD_END};

static DDFSpecialFlags map_specials[] = {
    {"JUMPING", kMapFlagJumping, 0},
    {"MLOOK", kMapFlagMlook, 0},
    {"FREELOOK", kMapFlagMlook, 0},  // -AJA- backwards compat.
    {"CHEATS", kMapFlagCheats, 0},
    {"ITEM_RESPAWN", kMapFlagItemRespawn, 0},
    {"FAST_MONSTERS", kMapFlagFastParm, 0},
    {"RESURRECT_RESPAWN", kMapFlagResRespawn, 0},
    {"TELEPORT_RESPAWN", kMapFlagResRespawn, 1},
    {"TRUE3D", kMapFlagTrue3D, 0},
    {"ENEMY_STOMP", kMapFlagStomp, 0},
    {"MORE_BLOOD", kMapFlagMoreBlood, 0},
    {"NORMAL_BLOOD", kMapFlagMoreBlood, 1},
    {"RESPAWN", kMapFlagRespawn, 0},
    {"AUTOAIM", kMapFlagAutoAim, 0},
    {"AA_MLOOK", kMapFlagAutoAimMlook, 0},
    {"EXTRAS", kMapFlagExtras, 0},
    {"RESET_PLAYER", kMapFlagResetPlayer, 0},
    {"LIMIT_ZOOM", kMapFlagLimitZoom, 0},
    {"CROUCHING", kMapFlagCrouching, 0},
    {"WEAPON_KICK", kMapFlagKicking, 0},

    {nullptr, 0, 0}};

//
//  DDF PARSE ROUTINES
//

static void LevelStartEntry(const char *name, bool extend)
{
    if (!name || !name[0])
    {
        DDF_WarnError("New level entry is missing a name!");
        name = "LEVEL_WITH_NO_NAME";
    }

    // instantiate the static entries
    dummy_finale.Default();

    // replaces an existing entry?
    dynamic_level = mapdefs.Lookup(name);

    if (extend)
    {
        if (!dynamic_level) DDF_Error("Unknown level to extend: %s\n", name);
        return;
    }

    if (dynamic_level)
    {
        dynamic_level->Default();
        return;
    }

    dynamic_level        = new MapDefinition;
    dynamic_level->name_ = name;

    mapdefs.push_back(dynamic_level);
}

static void LevelDoTemplate(const char *contents)
{
    MapDefinition *other = mapdefs.Lookup(contents);

    if (!other || other == dynamic_level)
        DDF_Error("Unknown level template: '%s'\n", contents);

    dynamic_level->CopyDetail(*other);
}

static void LevelParseField(const char *field, const char *contents, int index,
                            bool is_last)
{
#if (DEBUG_DDF)
    LogDebug("LEVEL_PARSE: %s = %s;\n", field, contents);
#endif

    if (DDF_CompareName(field, "TEMPLATE") == 0)
    {
        LevelDoTemplate(contents);
        return;
    }

    // -AJA- ignore this for backwards compatibility
    if (DDF_CompareName(field, "LIGHTING") == 0) return;

    // -AJA- this needs special handling (it modifies TWO fields)
    if (DDF_CompareName(field, "SPECIAL") == 0)
    {
        DDF_LevelGetSpecials(contents);
        return;
    }

    if (DDF_MainParseField(level_commands, field, contents,
                           (uint8_t *)dynamic_level))
        return;  // OK

    DDF_WarnError("Unknown levels.ddf command: %s\n", field);
}

static void LevelFinishEntry(void)
{
    // check stuff
    if (dynamic_level->episode_name_.empty())
        DDF_Error("Level entry must have an EPISODE name!\n");

    if (dynamic_level->indoor_fog_cmap_)
        dynamic_level->indoor_fog_color_ =
            dynamic_level->indoor_fog_cmap_->gl_color_;

    if (dynamic_level->outdoor_fog_cmap_)
        dynamic_level->outdoor_fog_color_ =
            dynamic_level->outdoor_fog_cmap_->gl_color_;
}

static void LevelClearAll(void)
{
    // 100% safe to delete the level entries -- no refs
    for (MapDefinition *map : mapdefs)
    {
        delete map;
        map = nullptr;
    }
    mapdefs.clear();
}

void DDF_ReadLevels(const std::string &data)
{
    DDFReadInfo levels;

    levels.tag      = "LEVELS";
    levels.lumpname = "DDFLEVL";

    levels.start_entry  = LevelStartEntry;
    levels.parse_field  = LevelParseField;
    levels.finish_entry = LevelFinishEntry;
    levels.clear_all    = LevelClearAll;

    DDF_MainReadFile(&levels, data);
}

void DDF_LevelInit(void) { LevelClearAll(); }

void DDF_LevelCleanUp(void)
{
    if (mapdefs.empty()) FatalError("There are no levels defined in DDF !\n");

    mapdefs.shrink_to_fit();

    // lookup episodes

    for (std::vector<MapDefinition *>::reverse_iterator
             iter     = mapdefs.rbegin(),
             iter_end = mapdefs.rend();
         iter != iter_end; iter++)
    {
        MapDefinition *m = *iter;

        m->episode_ = gamedefs.Lookup(m->episode_name_.c_str());

        if (m->episode_name_.empty())
            LogPrint("WARNING: Cannot find episode name for map entry [%s]\n",
                     m->name_.c_str());
    }
}

//
// Adds finale pictures to the level's list.
//
void DDF_LevelGetPic(const char *info, void *storage)
{
    std::vector<std::string> *list = (std::vector<std::string> *)storage;

    list->push_back(info);
}

void DDF_LevelGetSpecials(const char *info)
{
    // -AJA- 2000/02/02: reworked this for new system.

    int flag_value;

    // check for depreciated flags...
    if (DDF_CompareName(info, "TRANSLUCENCY") == 0)
    {
        DDF_Warning("Level special '%s' is deprecated.\n", info);
        return;
    }

    switch (
        DDF_MainCheckSpecialFlag(info, map_specials, &flag_value, true, true))
    {
        case kDDFCheckFlagPositive:
            dynamic_level->force_on_ |= flag_value;
            dynamic_level->force_off_ &= ~flag_value;
            break;

        case kDDFCheckFlagNegative:
            dynamic_level->force_on_ &= ~flag_value;
            dynamic_level->force_off_ |= flag_value;
            break;

        case kDDFCheckFlagUser:
            dynamic_level->force_on_ &= ~flag_value;
            dynamic_level->force_off_ &= ~flag_value;
            break;

        case kDDFCheckFlagUnknown:
            DDF_WarnError("DDF_LevelGetSpecials: Unknown level special: %s",
                          info);
            break;
    }
}

void DDF_LevelGetSkyStretch(const char *info, void *storage)
{
    SkyStretch *stretch = (SkyStretch *)storage;

    if (epi::StringCaseCompareASCII(info, "MIRROR") == 0)
        *stretch = kSkyStretchMirror;
    else if (epi::StringCaseCompareASCII(info, "REPEAT") == 0)
        *stretch = kSkyStretchRepeat;
    else if (epi::StringCaseCompareASCII(info, "STRETCH") == 0)
        *stretch = kSkyStretchStretch;
    else if (epi::StringCaseCompareASCII(info, "VANILLA") == 0)
        *stretch = kSkyStretchVanilla;
    else  // Unknown
        *stretch = kSkyStretchUnset;
}

static DDFSpecialFlags wistyle_names[] = {{"DOOM", kIntermissionStyleDoom, 0},
                                      {"NONE", kIntermissionStyleNone, 0},
                                      {nullptr, 0, 0}};

void DDF_LevelGetWistyle(const char *info, void *storage)
{
    int flag_value;

    if (kDDFCheckFlagPositive != DDF_MainCheckSpecialFlag(info, wistyle_names,
                                                  &flag_value, false, false))
    {
        DDF_WarnError("DDF_LevelGetWistyle: Unknown stats: %s", info);
        return;
    }

    ((IntermissionStyle *)storage)[0] = (IntermissionStyle)flag_value;
}

//------------------------------------------------------------------

// --> map finale definition class

FinaleDefinition::FinaleDefinition() : pics_() { Default(); }

FinaleDefinition::FinaleDefinition(FinaleDefinition &rhs) : pics_()
{
    Copy(rhs);
}

FinaleDefinition::~FinaleDefinition() {}

void FinaleDefinition::Copy(FinaleDefinition &src)
{
    text_ = src.text_;

    text_back_   = src.text_back_;
    text_flat_   = src.text_flat_;
    text_speed_  = src.text_speed_;
    text_wait_   = src.text_wait_;
    text_colmap_ = src.text_colmap_;

    movie_ = src.movie_;

    pics_    = src.pics_;
    picwait_ = src.picwait_;

    docast_  = src.docast_;
    dobunny_ = src.dobunny_;
    music_   = src.music_;
}

void FinaleDefinition::Default()
{
    text_.clear();
    text_back_.clear();
    text_flat_.clear();
    text_speed_  = 3.0f;
    text_wait_   = 150;
    text_colmap_ = nullptr;

    movie_.clear();

    pics_.clear();
    picwait_ = 0;

    docast_  = false;
    dobunny_ = false;
    music_   = 0;
}

FinaleDefinition &FinaleDefinition::operator=(FinaleDefinition &rhs)
{
    if (&rhs != this) Copy(rhs);

    return *this;
}

// --> map definition class

MapDefinition::MapDefinition() : name_() { Default(); }

MapDefinition::~MapDefinition() {}

void MapDefinition::CopyDetail(MapDefinition &src)
{
    description_ = src.description_;
    namegraphic_ = src.namegraphic_;
    lump_        = src.lump_;
    sky_         = src.sky_;
    surround_    = src.surround_;
    author_      = src.author_;

    music_   = src.music_;
    partime_ = src.partime_;

    episode_name_ = src.episode_name_;

    force_on_  = src.force_on_;
    force_off_ = src.force_off_;

    next_mapname_   = src.next_mapname_;
    secretmapname_ = src.secretmapname_;

    autotag_ = src.autotag_;

    wistyle_           = src.wistyle_;
    leavingbggraphic_  = src.leavingbggraphic_;
    enteringbggraphic_ = src.enteringbggraphic_;

    f_pre_ = src.f_pre_;
    f_end_ = src.f_end_;

    forced_skystretch_ = src.forced_skystretch_;

    indoor_fog_cmap_     = src.indoor_fog_cmap_;
    indoor_fog_color_    = src.indoor_fog_color_;
    indoor_fog_density_  = src.indoor_fog_density_;
    outdoor_fog_cmap_    = src.outdoor_fog_cmap_;
    outdoor_fog_color_   = src.outdoor_fog_color_;
    outdoor_fog_density_ = src.outdoor_fog_density_;
}

void MapDefinition::Default()
{
    description_.clear();
    namegraphic_.clear();
    lump_.clear();
    sky_.clear();
    surround_.clear();
    author_.clear();

    music_   = 0;
    partime_ = 0;

    episode_ = nullptr;
    episode_name_.clear();

    force_on_  = kMapFlagNone;
    force_off_ = kMapFlagNone;

    next_mapname_.clear();
    secretmapname_.clear();

    autotag_ = 0;

    wistyle_ = kIntermissionStyleDoom;

    leavingbggraphic_.clear();
    enteringbggraphic_.clear();

    f_pre_.Default();
    f_end_.Default();

    forced_skystretch_ = kSkyStretchUnset;

    indoor_fog_cmap_     = nullptr;
    indoor_fog_color_    = kRGBANoValue;
    indoor_fog_density_  = 0;
    outdoor_fog_cmap_    = nullptr;
    outdoor_fog_color_   = kRGBANoValue;
    outdoor_fog_density_ = 0;
}

//
// mapdef_c* mapdef_container_c::Lookup()
//
// Looks an gamedef by name, returns a fatal error if it does not exist.
//
MapDefinition *MapDefinitionContainer::Lookup(const char *refname)
{
    if (!refname || !refname[0]) return nullptr;

    for (std::vector<MapDefinition *>::reverse_iterator iter     = rbegin(),
                                                        iter_end = rend();
         iter != iter_end; iter++)
    {
        MapDefinition *m = *iter;

        // ignore maps with unknown episode_name
        // if (! m->episode)
        //	continue;

        // Lobo 2022: Allow warping and IDCLEVing to arbitrarily
        //  named maps. We have to have a levels.ddf entry AND an episode
        //  so we need to create them on the fly if they are missing.
        if (DDF_CompareName(m->name_.c_str(), refname) == 0)
        {
            // Invent a temp episode if we don't have one
            if (m->episode_name_.empty())
            {
                GameDefinition *temp_gamedef;

                temp_gamedef        = new GameDefinition;
                temp_gamedef->name_ = "TEMPEPI";
                m->episode_name_    = temp_gamedef->name_;
                m->episode_         = temp_gamedef;

                // We must have a default sky
                if (m->sky_.empty()) m->sky_ = "SKY1";
            }
            return m;
        }
    }

    // If we're here then it is a map which has no corresponding
    //  levels.ddf entry.

    // 1. check if the actual map lump exists
    if (CheckLumpNumberForName(refname) >= 0 &&
        GetKindForLump(CheckLumpNumberForName(refname)) == 3)  // kLumpMarker
    {
        // 2. make a levels.ddf entry
        MapDefinition *temp_level;
        temp_level               = new MapDefinition;
        temp_level->name_        = refname;
        temp_level->description_ = refname;
        temp_level->lump_        = refname;

        // 3. we also need to assign an episode
        GameDefinition *temp_gamedef;
        temp_gamedef              = new GameDefinition;
        temp_gamedef->name_       = "TEMPEPI";
        temp_level->episode_name_ = temp_gamedef->name_;
        temp_level->episode_      = temp_gamedef;

        // 4. Finally We must have a default sky
        if (temp_level->sky_.empty()) temp_level->sky_ = "SKY1";

        mapdefs.push_back(temp_level);

        return temp_level;
    }

    return nullptr;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
