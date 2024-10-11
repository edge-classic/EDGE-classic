//----------------------------------------------------------------------------
//  EDGE Network Menu Code
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
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------

#include "m_netgame.h"

#include "con_main.h"
#include "ddf_font.h"
#include "ddf_main.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "dstrings.h"
#include "e_main.h"
#include "epi.h"
#include "epi_str_util.h"
#include "f_interm.h"
#include "g_game.h"
#include "hu_draw.h"
#include "hu_stuff.h"
#include "hu_style.h"
#include "m_argv.h"
#include "m_menu.h"
#include "m_misc.h"
#include "m_option.h"
#include "m_random.h"
#include "n_network.h"
#include "p_setup.h"
#include "r_colormap.h"
#include "r_draw.h"
#include "r_modes.h"
#include "s_sound.h"
#include "sv_chunk.h"
#include "sv_main.h"
#include "w_wad.h"

extern GameFlags default_game_flags;

extern ConsoleVariable bot_skill;

int network_game_menu_on;

static Style *network_game_host_style;
static Style *network_game_list_style;

static NewGameParameters *network_game_parameters;

static int host_position;

static int host_want_bots;

static constexpr uint8_t kTotalHostOptions = 11;

static void ListAccept(void);

EDGE_DEFINE_CONSOLE_VARIABLE(player_deathmatch_damage_resistance, "9", kConsoleVariableFlagArchive)

static void DrawKeyword(int index, Style *style, int y, const char *keyword, const char *value)
{
    int x = 160;

    bool is_selected = (index == host_position);

    x = x - 10;
    x = x - (style->fonts_[StyleDefinition::kTextSectionText]->StringWidth(keyword) *
             style->definition_->text_[StyleDefinition::kTextSectionText].scale_);
    HUDWriteText(style, (index < 0) ? 3 : is_selected ? 2 : 0, x, y, keyword);

    x = 160;
    HUDWriteText(style, StyleDefinition::kTextSectionAlternate, x + 10, y, value);

    if (is_selected)
    {
        if (style->fonts_[StyleDefinition::kTextSectionAlternate]->definition_->type_ == kFontTypeImage)
        {
            int cursor = 16;
            HUDWriteText(style, StyleDefinition::kTextSectionTitle,
                         x - style->fonts_[StyleDefinition::kTextSectionTitle]->StringWidth((const char *)&cursor) / 2,
                         y, (const char *)&cursor);
        }
        else if (style->fonts_[StyleDefinition::kTextSectionAlternate]->definition_->type_ == kFontTypeTrueType)
            HUDWriteText(style, StyleDefinition::kTextSectionTitle,
                         x - style->fonts_[StyleDefinition::kTextSectionTitle]->StringWidth("+") / 2, y, "+");
        else
            HUDWriteText(style, StyleDefinition::kTextSectionTitle,
                         x - style->fonts_[StyleDefinition::kTextSectionTitle]->StringWidth("*") / 2, y, "*");
    }
}

static const char *GetModeName(char mode)
{
    switch (mode)
    {
    case 0:
        return language["BotCoop"];
    case 1:
        return language["BotOldDM"];
    case 2:
        return language["BotNewDM"];
    default:
        return "????";
    }
}

static const char *GetSkillName(SkillLevel skill)
{
    switch (skill)
    {
    case kSkillBaby:
        return language["MenuDifficulty1"];
    case kSkillEasy:
        return language["MenuDifficulty2"];
    case kSkillMedium:
        return language["MenuDifficulty3"];
    case kSkillHard:
        return language["MenuDifficulty4"];
    case kSkillNightmare:
        return language["MenuDifficulty5"];

    default:
        return "????";
    }
}

static const char *GetBotSkillName(int sk)
{
    switch (sk)
    {
    case 0:
        return language["BotDifficulty1"];
    case 1:
        return language["BotDifficulty2"];
    case 2:
        return language["BotDifficulty3"];
    case 3:
        return language["BotDifficulty4"];
    case 4:
        return language["BotDifficulty5"];

    default:
        return "????";
    }
}

static const char *GetPlayerDamageResistanceNameName(int res)
{
    switch (res)
    {
    case 0:
        return "-90%";
    case 1:
        return "-80%";
    case 2:
        return "-70%";
    case 3:
        return "-60%";
    case 4:
        return "-50%";
    case 5:
        return "-40%";
    case 6:
        return "-30%";
    case 7:
        return "-20%";
    case 8:
        return "-10%";
    case 9:
        return "Normal";
    case 10:
        return "+10%";
    case 11:
        return "+20%";
    case 12:
        return "+30%";
    case 13:
        return "+40%";
    case 14:
        return "+50%";
    case 15:
        return "+60%";
    case 16:
        return "+70%";
    case 17:
        return "+80%";
    case 18:
        return "+90%";

    default:
        return "????";
    }
}

//----------------------------------------------------------------------------

void OptionMenuNetworkHostBegun(void)
{
    host_position = 0;

    if (network_game_parameters)
        delete network_game_parameters;

    network_game_parameters = new NewGameParameters;

    network_game_parameters->CopyFlags(&global_flags);

    network_game_parameters->map_ = LookupMap("1");

    if (!network_game_parameters->map_)
        network_game_parameters->map_ = mapdefs[0];

    host_want_bots = 0;
}

static void ChangeGame(NewGameParameters *param, int dir)
{
    GameDefinition *closest  = nullptr;
    GameDefinition *furthest = nullptr;

    for (auto def : gamedefs)
    {
        MapDefinition *first_map = mapdefs.Lookup(def->firstmap_.c_str());

        if (!first_map || !MapExists(first_map))
            continue;

        const char *old_name = param->map_->episode_->name_.c_str();
        const char *new_name = def->name_.c_str();

        int compare = DDFCompareName(new_name, old_name);

        if (compare == 0)
            continue;

        if (compare * dir > 0)
        {
            if (!closest || dir * DDFCompareName(new_name, closest->name_.c_str()) < 0)
                closest = def;
        }
        else
        {
            if (!furthest || dir * DDFCompareName(new_name, furthest->name_.c_str()) < 0)
                furthest = def;
        }
    }

    LogDebug("DIR: %d  CURRENT: %s   CLOSEST: %s   FURTHEST: %s\n", dir,
             network_game_parameters->map_->episode_->name_.c_str(), closest ? closest->name_.c_str() : "none",
             furthest ? furthest->name_.c_str() : "none");

    if (closest)
    {
        param->map_ = mapdefs.Lookup(closest->firstmap_.c_str());
        EPI_ASSERT(param->map_);
        return;
    }

    // could not find the next/previous map, hence wrap around
    if (furthest)
    {
        param->map_ = mapdefs.Lookup(furthest->firstmap_.c_str());
        EPI_ASSERT(param->map_);
        return;
    }
}

static void ChangeLevel(NewGameParameters *param, int dir)
{
    MapDefinition *closest  = nullptr;
    MapDefinition *furthest = nullptr;

    for (MapDefinition *def : mapdefs)
    {
        if (def->episode_ != param->map_->episode_)
            continue;

        const char *old_name = param->map_->name_.c_str();
        const char *new_name = def->name_.c_str();

        int compare = DDFCompareName(new_name, old_name);

        if (compare == 0)
            continue;

        if (compare * dir > 0)
        {
            if (!closest || dir * DDFCompareName(new_name, closest->name_.c_str()) < 0)
                closest = def;
        }
        else
        {
            if (!furthest || dir * DDFCompareName(new_name, furthest->name_.c_str()) < 0)
                furthest = def;
        }
    }

    if (closest)
    {
        param->map_ = closest;
        return;
    }

    // could not find the next/previous map, hence wrap around
    if (furthest)
        param->map_ = furthest;
}

static void HostChangeOption(int opt, int key)
{
    int dir = (key == kLeftArrow || key == kGamepadLeft) ? -1 : +1;

    switch (opt)
    {
    case 0: // Game
        ChangeGame(network_game_parameters, dir);
        break;

    case 1: // Level
        ChangeLevel(network_game_parameters, dir);
        break;

    case 2: // Mode
    {
        network_game_parameters->deathmatch_ += dir;

        if (network_game_parameters->deathmatch_ < 0)
            network_game_parameters->deathmatch_ = 2;
        else if (network_game_parameters->deathmatch_ > 2)
            network_game_parameters->deathmatch_ = 0;

        break;
    }

    case 3: // Skill
        network_game_parameters->skill_ = (SkillLevel)((int)network_game_parameters->skill_ + dir);
        if ((int)network_game_parameters->skill_ < (int)kSkillBaby || (int)network_game_parameters->skill_ > 250)
            network_game_parameters->skill_ = kSkillNightmare;
        else if ((int)network_game_parameters->skill_ > (int)kSkillNightmare)
            network_game_parameters->skill_ = kSkillBaby;

        break;

    case 4: // Bots
        host_want_bots += dir;

        if (host_want_bots < 0)
            host_want_bots = kMaximumPlayers-1;
        else if (host_want_bots > kMaximumPlayers-1)
            host_want_bots = 0;

        break;

    case 5: // Bot Skill
        bot_skill = bot_skill.d_ + dir;
        bot_skill = HMM_Clamp(0, bot_skill.d_, 4);

        break;

    case 6:
        player_deathmatch_damage_resistance = player_deathmatch_damage_resistance.d_ + dir;
        player_deathmatch_damage_resistance = HMM_Clamp(0, player_deathmatch_damage_resistance.d_, 18);

        break;

    case 7: // Monsters
        if (network_game_parameters->flags_->fast_monsters)
        {
            network_game_parameters->flags_->fast_monsters = false;
            network_game_parameters->flags_->no_monsters   = (dir > 0);
        }
        else if (network_game_parameters->flags_->no_monsters == (dir < 0))
        {
            network_game_parameters->flags_->fast_monsters = true;
            network_game_parameters->flags_->no_monsters   = false;
        }
        else
            network_game_parameters->flags_->no_monsters = (dir < 0);

        break;

    case 8: // Item-Respawn
        network_game_parameters->flags_->items_respawn = !network_game_parameters->flags_->items_respawn;
        break;

    case 9: // Team-Damage
        network_game_parameters->flags_->team_damage = !network_game_parameters->flags_->team_damage;
        break;

    default:
        break;
    }
}

static void HostAccept(void)
{
    // create local player and bots
    network_game_parameters->SinglePlayer(host_want_bots);

    network_game_parameters->level_skip_ = true;

    network_game_menu_on = 3;

#if 1
    ListAccept();
#endif
}

void OptionMenuDrawHostMenu(void)
{
    EPI_ASSERT(network_game_host_style);

    network_game_host_style->DrawBackground();

    int CenterX;
    CenterX = 160;
    CenterX -=
        (network_game_host_style->fonts_[StyleDefinition::kTextSectionHeader]->StringWidth("Bot Match Settings") *
         network_game_host_style->definition_->text_[StyleDefinition::kTextSectionHeader].scale_) /
        2;

    HUDWriteText(network_game_host_style, StyleDefinition::kTextSectionHeader, CenterX, 25, "Bot Match Settings");

    int y      = 40;
    int idx    = 0;
    int deltay = 2 +
                 (network_game_host_style->fonts_[StyleDefinition::kTextSectionText]->NominalHeight() *
                  network_game_host_style->definition_->text_[StyleDefinition::kTextSectionText].scale_) +
                 network_game_host_style->definition_->entry_spacing_;

    if (!network_game_parameters->map_->episode_->description_.empty())
        DrawKeyword(idx, network_game_host_style, y, "Episode",
                    language[network_game_parameters->map_->episode_->description_.c_str()]);
    else
        DrawKeyword(idx, network_game_host_style, y, "Episode",
                    language[network_game_parameters->map_->episode_name_.c_str()]);

    y += deltay;
    idx++;

    DrawKeyword(idx, network_game_host_style, y, "Level", network_game_parameters->map_->name_.c_str());
    y += deltay + (deltay / 2);
    idx++;

    DrawKeyword(idx, network_game_host_style, y, "Mode", GetModeName(network_game_parameters->deathmatch_));
    y += deltay;
    idx++;

    DrawKeyword(idx, network_game_host_style, y, "Skill", GetSkillName(network_game_parameters->skill_));
    y += deltay;
    idx++;

    DrawKeyword(idx, network_game_host_style, y, "Bots", epi::StringFormat("%d", host_want_bots).c_str());
    y += deltay;
    idx++;

    int skill = HMM_Clamp(0, bot_skill.d_, 4);
    DrawKeyword(idx, network_game_host_style, y, "Bot Skill", GetBotSkillName(skill));
    y += deltay;
    idx++;

    int dm_damage_resistance = HMM_Clamp(0, player_deathmatch_damage_resistance.d_, 18);
    DrawKeyword(idx, network_game_host_style, y, "Player Damage Resistance",
                GetPlayerDamageResistanceNameName(dm_damage_resistance));
    y += deltay;
    idx++;

    int x =
        150 - (network_game_host_style->fonts_[StyleDefinition::kTextSectionText]->StringWidth("(Deathmatch Only)") *
               network_game_host_style->definition_->text_[StyleDefinition::kTextSectionText].scale_);
    HUDWriteText(network_game_host_style, idx - 1 == host_position ? 2 : 0, x, y, "(Deathmatch Only)");
    y += deltay;

    DrawKeyword(idx, network_game_host_style, y, "Monsters",
                network_game_parameters->flags_->no_monsters     ? "OFF"
                : network_game_parameters->flags_->fast_monsters ? "FAST"
                                                                 : "ON");
    y += deltay;
    idx++;

    DrawKeyword(idx, network_game_host_style, y, "Item Respawn",
                network_game_parameters->flags_->items_respawn ? "ON" : "OFF");
    y += deltay;
    idx++;

    DrawKeyword(idx, network_game_host_style, y, "Team Damage",
                network_game_parameters->flags_->team_damage ? "ON" : "OFF");
    y += (deltay * 2);
    idx++;

    CenterX = 160;
    CenterX -= (network_game_host_style->fonts_[StyleDefinition::kTextSectionText]->StringWidth("Start") *
                network_game_host_style->definition_->text_[StyleDefinition::kTextSectionText].scale_) /
               2;

    HUDWriteText(network_game_host_style,
                 (host_position == idx) ? StyleDefinition::kTextSectionHelp : StyleDefinition::kTextSectionText,
                 CenterX, y, "Start");
}

bool OptionMenuNetworkHostResponder(InputEvent *ev, int ch)
{
    if (ch == kEnter || ch == kGamepadA || ch == kMouse1)
    {
        if (host_position == (kTotalHostOptions - 1))
        {
            HostAccept();
            StartSoundEffect(sound_effect_pistol);
            return true;
        }
    }

    if (ch == kDownArrow || ch == kMouseWheelDown || ch == kGamepadDown)
    {
        host_position = (host_position + 1) % kTotalHostOptions;
        StartSoundEffect(sound_effect_pstop);
        return true;
    }
    else if (ch == kUpArrow || ch == kMouseWheelUp || ch == kGamepadUp)
    {
        host_position = (host_position + kTotalHostOptions - 1) % kTotalHostOptions;
        StartSoundEffect(sound_effect_pstop);
        return true;
    }

    if (ch == kLeftArrow || ch == kRightArrow || ch == kGamepadLeft || ch == kGamepadRight || ch == kEnter ||
        ch == kGamepadA || ch == kMouse1)
    {
        HostChangeOption(host_position, ch);
        StartSoundEffect(sound_effect_stnmov);
        return true;
    }

    return false;
}

void OptionMenuNetworkHostTicker(void)
{
    // nothing needed
}

//----------------------------------------------------------------------------

static void NetGameStartLevel(void)
{
    // -KM- 1998/12/17 Clear the intermission.
    IntermissionClear();

    DeferredNewGame(*network_game_parameters);
}

void OptionMenuDrawPlayerList(void)
{
    HUDSetAlpha(0.64f);
    HUDSolidBox(0, 0, 320, 200, SG_BLACK_RGBA32);
    HUDSetAlpha();

    HUDWriteText(network_game_list_style, 2, 80, 10, "PLAYER LIST");

    int y = 30;
    int i;

    int humans = 0;

    ///	for (i = 0; i < network_game_parameters->total_players; i++)
    ///		if (! (network_game_parameters->players[i] & kPlayerFlagBot))
    ///			humans++;

    for (i = 0; i < network_game_parameters->total_players_; i++)
    {
        int flags = network_game_parameters->players_[i];

        if (flags & kPlayerFlagBot)
            continue;

        humans++;

        int bots_here = 0;

        for (int j = 0; j < network_game_parameters->total_players_; j++)
        {
            if (network_game_parameters->players_[j] & kPlayerFlagBot)
                bots_here++;
        }

        HUDWriteText(network_game_list_style, (flags & kPlayerFlagNetwork) ? 0 : 3, 20, y,
                     epi::StringFormat("PLAYER %d", humans).c_str());

        HUDWriteText(network_game_list_style, 1, 100, y, "Local");

        HUDWriteText(network_game_list_style, (flags & kPlayerFlagNetwork) ? 0 : 3, 200, y,
                     epi::StringFormat("%d BOTS", bots_here).c_str());
        y += 10;
    }

    HUDWriteText(network_game_list_style, 2, 40, 140, "Press <ENTER> to Start Game");
}

static void ListAccept()
{
    StartSoundEffect(sound_effect_pistol);

    network_game_menu_on = 0;
    MenuClear();

    NetGameStartLevel();
}

bool OptionMenuNetListResponder(InputEvent *ev, int ch)
{
    if (ch == kEnter || ch == kGamepadA)
    {
        ListAccept();
        return true;
    }

    return false;
}

void OptionMenuNetListTicker(void)
{
    // nothing needed
}

//----------------------------------------------------------------------------

void NetworkGameInitialize(void)
{
    network_game_menu_on = 0;

    host_position = 0;

    // load styles
    StyleDefinition *def;
    Style           *ng_default;

    def = styledefs.Lookup("OPTIONS");
    if (!def)
        def = default_style;
    ng_default = hud_styles.Lookup(def);

    def                     = styledefs.Lookup("HOST NETGAME");
    network_game_host_style = def ? hud_styles.Lookup(def) : ng_default;

    def                     = styledefs.Lookup("NET PLAYER LIST");
    network_game_list_style = def ? hud_styles.Lookup(def) : ng_default;
}

void NetworkGameDrawer(void)
{
    switch (network_game_menu_on)
    {
    case 1:
        OptionMenuDrawHostMenu();
        return;
    case 3:
        OptionMenuDrawPlayerList();
        return;
    }

    FatalError("INTERNAL ERROR: network_game_menu_on=%d\n", network_game_menu_on);
}

bool NetworkGameResponder(InputEvent *ev, int ch)
{
    switch (ch)
    {
    case kMouse2:
    case kMouse3:
    case kEscape:
    case kGamepadB: {
        network_game_menu_on = 0;
        MenuClear();

        StartSoundEffect(sound_effect_pistol);
        return true;
    }
    }

    switch (network_game_menu_on)
    {
    case 1:
        return OptionMenuNetworkHostResponder(ev, ch);
    case 3:
        return OptionMenuNetListResponder(ev, ch);
    }

    return false;
}

void NetworkGameTicker(void)
{
    switch (network_game_menu_on)
    {
    case 1:
        return OptionMenuNetworkHostTicker();
    case 3:
        return OptionMenuNetListTicker();
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
