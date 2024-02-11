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

mapdef_container_c mapdefs;

#undef DDF_CMD_BASE
#define DDF_CMD_BASE dummy_finale
static map_finaledef_c dummy_finale;

static const commandlist_t finale_commands[] = {
    DF("TEXT", text, DDF_MainGetString),
    DF("TEXT_GRAPHIC", text_back, DDF_MainGetLumpName),
    DF("TEXT_FLAT", text_flat, DDF_MainGetLumpName),
    DF("TEXT_SPEED", text_speed, DDF_MainGetFloat),
    DF("TEXT_WAIT", text_wait, DDF_MainGetNumeric),
    DF("COLOURMAP", text_colmap, DDF_MainGetColourmap),
    DF("GRAPHIC", pics, DDF_LevelGetPic),
    DF("GRAPHIC_WAIT", picwait, DDF_MainGetTime),
    DF("MOVIE", movie, DDF_MainGetString),
    DF("CAST", docast, DDF_MainGetBoolean),
    DF("BUNNY", dobunny, DDF_MainGetBoolean),
    DF("MUSIC", music, DDF_MainGetNumeric),

    DDF_CMD_END};

// -KM- 1998/11/25 Finales are all go.

static mapdef_c *dynamic_level;

#undef DDF_CMD_BASE
#define DDF_CMD_BASE dummy_level
static mapdef_c dummy_level;

static const commandlist_t level_commands[] = {
    // sub-commands
    DDF_SUB_LIST("PRE", f_pre, finale_commands),
    DDF_SUB_LIST("END", f_end, finale_commands),

    DF("LUMPNAME", lump, DDF_MainGetLumpName),
    DF("DESCRIPTION", description, DDF_MainGetString),
    DF("AUTHOR", author, DDF_MainGetString),
    DF("NAME_GRAPHIC", namegraphic, DDF_MainGetLumpName),
    DF("SKY_TEXTURE", sky, DDF_MainGetLumpName),
    DF("SKY_STRETCH", forced_skystretch, DDF_LevelGetSkyStretch),
    DF("MUSIC_ENTRY", music, DDF_MainGetNumeric),
    DF("SURROUND_FLAT", surround, DDF_MainGetLumpName),
    DF("NEXT_MAP", nextmapname, DDF_MainGetLumpName),
    DF("SECRET_MAP", secretmapname, DDF_MainGetLumpName),
    DF("AUTOTAG", autotag, DDF_MainGetNumeric),
    DF("PARTIME", partime, DDF_MainGetTime),
    DF("EPISODE", episode_name, DDF_MainGetString),
    DF("STATS", wistyle, DDF_LevelGetWistyle),
    DF("LEAVING_BACKGROUND", leavingbggraphic, DDF_MainGetLumpName),
    DF("ENTERING_BACKGROUND", enteringbggraphic, DDF_MainGetLumpName),
    DF("INDOOR_FOG_COLOR", indoor_fog_cmap, DDF_MainGetColourmap),
    DF("INDOOR_FOG_DENSITY", indoor_fog_density, DDF_MainGetPercent),
    DF("OUTDOOR_FOG_COLOR", outdoor_fog_cmap, DDF_MainGetColourmap),
    DF("OUTDOOR_FOG_DENSITY", outdoor_fog_density, DDF_MainGetPercent),

    DDF_CMD_END};

static specflags_t map_specials[] = {
    {"JUMPING", MPF_Jumping, 0},
    {"MLOOK", MPF_Mlook, 0},
    {"FREELOOK", MPF_Mlook, 0},  // -AJA- backwards compat.
    {"CHEATS", MPF_Cheats, 0},
    {"ITEM_RESPAWN", MPF_ItemRespawn, 0},
    {"FAST_MONSTERS", MPF_FastParm, 0},
    {"RESURRECT_RESPAWN", MPF_ResRespawn, 0},
    {"TELEPORT_RESPAWN", MPF_ResRespawn, 1},
    {"TRUE3D", MPF_True3D, 0},
    {"ENEMY_STOMP", MPF_Stomp, 0},
    {"MORE_BLOOD", MPF_MoreBlood, 0},
    {"NORMAL_BLOOD", MPF_MoreBlood, 1},
    {"RESPAWN", MPF_Respawn, 0},
    {"AUTOAIM", MPF_AutoAim, 0},
    {"AA_MLOOK", MPF_AutoAimMlook, 0},
    {"EXTRAS", MPF_Extras, 0},
    {"RESET_PLAYER", MPF_ResetPlayer, 0},
    {"LIMIT_ZOOM", MPF_LimitZoom, 0},
    {"CROUCHING", MPF_Crouching, 0},
    {"WEAPON_KICK", MPF_Kicking, 0},

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

    dynamic_level       = new mapdef_c;
    dynamic_level->name = name;

    mapdefs.push_back(dynamic_level);
}

static void LevelDoTemplate(const char *contents)
{
    mapdef_c *other = mapdefs.Lookup(contents);

    if (!other || other == dynamic_level)
        DDF_Error("Unknown level template: '%s'\n", contents);

    dynamic_level->CopyDetail(*other);
}

static void LevelParseField(const char *field, const char *contents, int index,
                            bool is_last)
{
#if (DEBUG_DDF)
    I_Debugf("LEVEL_PARSE: %s = %s;\n", field, contents);
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
    if (dynamic_level->episode_name.empty())
        DDF_Error("Level entry must have an EPISODE name!\n");

    if (dynamic_level->indoor_fog_cmap)
        dynamic_level->indoor_fog_color =
            dynamic_level->indoor_fog_cmap->gl_colour;

    if (dynamic_level->outdoor_fog_cmap)
        dynamic_level->outdoor_fog_color =
            dynamic_level->outdoor_fog_cmap->gl_colour;
}

static void LevelClearAll(void)
{
    // 100% safe to delete the level entries -- no refs
    for (auto map : mapdefs)
    {
        delete map;
        map = nullptr;
    }
    mapdefs.clear();
}

void DDF_ReadLevels(const std::string &data)
{
    readinfo_t levels;

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
    if (mapdefs.empty()) I_Error("There are no levels defined in DDF !\n");

    mapdefs.shrink_to_fit();

    // lookup episodes

    for (auto iter = mapdefs.rbegin(); iter != mapdefs.rend(); iter++)
    {
        mapdef_c *m = *iter;

        m->episode = gamedefs.Lookup(m->episode_name.c_str());

        if (m->episode_name.empty())
            I_Printf("WARNING: Cannot find episode name for map entry [%s]\n",
                     m->name.c_str());
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
        case CHKF_Positive:
            dynamic_level->force_on |= flag_value;
            dynamic_level->force_off &= ~flag_value;
            break;

        case CHKF_Negative:
            dynamic_level->force_on &= ~flag_value;
            dynamic_level->force_off |= flag_value;
            break;

        case CHKF_User:
            dynamic_level->force_on &= ~flag_value;
            dynamic_level->force_off &= ~flag_value;
            break;

        case CHKF_Unknown:
            DDF_WarnError("DDF_LevelGetSpecials: Unknown level special: %s",
                          info);
            break;
    }
}

void DDF_LevelGetSkyStretch(const char *info, void *storage)
{
    skystretch_e *stretch = (skystretch_e *)storage;

    if (epi::StringCaseCompareASCII(info, "MIRROR") == 0)
        *stretch = SKS_Mirror;
    else if (epi::StringCaseCompareASCII(info, "REPEAT") == 0)
        *stretch = SKS_Repeat;
    else if (epi::StringCaseCompareASCII(info, "STRETCH") == 0)
        *stretch = SKS_Stretch;
    else if (epi::StringCaseCompareASCII(info, "VANILLA") == 0)
        *stretch = SKS_Vanilla;
    else  // Unknown
        *stretch = SKS_Unset;
}

static specflags_t wistyle_names[] = {
    {"DOOM", WISTYLE_Doom, 0}, {"NONE", WISTYLE_None, 0}, {nullptr, 0, 0}};

void DDF_LevelGetWistyle(const char *info, void *storage)
{
    int flag_value;

    if (CHKF_Positive != DDF_MainCheckSpecialFlag(info, wistyle_names,
                                                  &flag_value, false, false))
    {
        DDF_WarnError("DDF_LevelGetWistyle: Unknown stats: %s", info);
        return;
    }

    ((intermission_style_e *)storage)[0] = (intermission_style_e)flag_value;
}

//------------------------------------------------------------------

// --> map finale definition class

map_finaledef_c::map_finaledef_c() : pics() { Default(); }

map_finaledef_c::map_finaledef_c(map_finaledef_c &rhs) : pics() { Copy(rhs); }

map_finaledef_c::~map_finaledef_c() {}

void map_finaledef_c::Copy(map_finaledef_c &src)
{
    text = src.text;

    text_back   = src.text_back;
    text_flat   = src.text_flat;
    text_speed  = src.text_speed;
    text_wait   = src.text_wait;
    text_colmap = src.text_colmap;

    movie = src.movie;

    pics    = src.pics;
    picwait = src.picwait;

    docast  = src.docast;
    dobunny = src.dobunny;
    music   = src.music;
}

void map_finaledef_c::Default()
{
    text.clear();
    text_back.clear();
    text_flat.clear();
    text_speed  = 3.0f;
    text_wait   = 150;
    text_colmap = nullptr;

    movie.clear();

    pics.clear();
    picwait = 0;

    docast  = false;
    dobunny = false;
    music   = 0;
}

map_finaledef_c &map_finaledef_c::operator=(map_finaledef_c &rhs)
{
    if (&rhs != this) Copy(rhs);

    return *this;
}

// --> map definition class

mapdef_c::mapdef_c() : name() { Default(); }

mapdef_c::~mapdef_c() {}

void mapdef_c::CopyDetail(mapdef_c &src)
{
    ///---	next = src.next;				// FIXME!!
    /// Gamestate data

    description = src.description;
    namegraphic = src.namegraphic;
    lump        = src.lump;
    sky         = src.sky;
    surround    = src.surround;
    author      = src.author;

    music   = src.music;
    partime = src.partime;

    episode_name = src.episode_name;

    force_on  = src.force_on;
    force_off = src.force_off;

    nextmapname   = src.nextmapname;
    secretmapname = src.secretmapname;

    autotag = src.autotag;

    wistyle           = src.wistyle;
    leavingbggraphic  = src.leavingbggraphic;
    enteringbggraphic = src.enteringbggraphic;

    f_pre = src.f_pre;
    f_end = src.f_end;

    forced_skystretch = src.forced_skystretch;

    indoor_fog_cmap     = src.indoor_fog_cmap;
    indoor_fog_color    = src.indoor_fog_color;
    indoor_fog_density  = src.indoor_fog_density;
    outdoor_fog_cmap    = src.outdoor_fog_cmap;
    outdoor_fog_color   = src.outdoor_fog_color;
    outdoor_fog_density = src.outdoor_fog_density;
}

void mapdef_c::Default()
{
    description.clear();
    namegraphic.clear();
    lump.clear();
    sky.clear();
    surround.clear();
    author.clear();

    music   = 0;
    partime = 0;

    episode = nullptr;
    episode_name.clear();

    force_on  = MPF_None;
    force_off = MPF_None;

    nextmapname.clear();
    secretmapname.clear();

    autotag = 0;

    wistyle = WISTYLE_Doom;

    leavingbggraphic.clear();
    enteringbggraphic.clear();

    f_pre.Default();
    f_end.Default();

    forced_skystretch = SKS_Unset;

    indoor_fog_cmap     = nullptr;
    indoor_fog_color    = kRGBANoValue;
    indoor_fog_density  = 0;
    outdoor_fog_cmap    = nullptr;
    outdoor_fog_color   = kRGBANoValue;
    outdoor_fog_density = 0;
}

//
// mapdef_c* mapdef_container_c::Lookup()
//
// Looks an gamedef by name, returns a fatal error if it does not exist.
//
mapdef_c *mapdef_container_c::Lookup(const char *refname)
{
    if (!refname || !refname[0]) return nullptr;

    for (auto iter = rbegin(); iter != rend(); iter++)
    {
        mapdef_c *m = *iter;

        // ignore maps with unknown episode_name
        // if (! m->episode)
        //	continue;

        // Lobo 2022: Allow warping and IDCLEVing to arbitrarily
        //  named maps. We have to have a levels.ddf entry AND an episode
        //  so we need to create them on the fly if they are missing.
        if (DDF_CompareName(m->name.c_str(), refname) == 0)
        {
            // Invent a temp episode if we don't have one
            if (m->episode_name.empty())
            {
                gamedef_c *temp_gamedef;

                temp_gamedef       = new gamedef_c;
                temp_gamedef->name = "TEMPEPI";
                m->episode_name    = temp_gamedef->name;
                m->episode         = temp_gamedef;

                // We must have a default sky
                if (m->sky.empty()) m->sky = "SKY1";
            }
            return m;
        }
    }

    // If we're here then it is a map which has no corresponding
    //  levels.ddf entry.

    // 1. check if the actual map lump exists
    if (W_CheckNumForName(refname) >= 0 &&
        W_GetKindForLump(W_CheckNumForName(refname)) == 3)  // LMKIND_Marker
    {
        // 2. make a levels.ddf entry
        mapdef_c *temp_level;
        temp_level              = new mapdef_c;
        temp_level->name        = refname;
        temp_level->description = refname;
        temp_level->lump        = refname;

        // 3. we also need to assign an episode
        gamedef_c *temp_gamedef;
        temp_gamedef             = new gamedef_c;
        temp_gamedef->name       = "TEMPEPI";
        temp_level->episode_name = temp_gamedef->name;
        temp_level->episode      = temp_gamedef;

        // 4. Finally We must have a default sky
        if (temp_level->sky.empty()) temp_level->sky = "SKY1";

        mapdefs.push_back(temp_level);

        return temp_level;
    }

    return nullptr;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
