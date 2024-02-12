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

#include "i_defs.h"

#include "main.h"
#include "font.h"

#include "con_main.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "dstrings.h"
#include "e_main.h"
#include "g_game.h"
#include "hu_draw.h"
#include "hu_stuff.h"
#include "hu_style.h"
#include "m_argv.h"
#include "m_menu.h"
#include "m_misc.h"
#include "m_option.h"
#include "m_random.h"
#include "m_netgame.h"
#include "n_network.h"
#include "p_setup.h"
#include "r_local.h"
#include "s_sound.h"
#include "sv_chunk.h"
#include "sv_main.h"
#include "r_draw.h"
#include "r_modes.h"
#include "r_colormap.h"
#include "w_wad.h"
#include "f_interm.h"

#include "str_util.h"

extern gameflags_t default_gameflags;

extern cvar_c bot_skill;

int  netgame_menuon;

static style_c *ng_host_style;
static style_c *ng_list_style;

static newgame_params_c *ng_params;

static int host_pos;

static int host_want_bots;

#define HOST_OPTIONS 11

static void ListAccept(void);

DEF_CVAR(player_dm_dr, "9", CVAR_ARCHIVE)

static void DrawKeyword(int index, style_c *style, int y, const char *keyword, const char *value)
{
    int x = 160;

    bool is_selected = (index == host_pos);

    x = x - 10;
    x = x - (style->fonts[styledef_c::T_TEXT]->StringWidth(keyword) * style->def->text[styledef_c::T_TEXT].scale);
    HL_WriteText(style, (index < 0) ? 3 : is_selected ? 2 : 0, x, y, keyword);

    x = 160;
    HL_WriteText(style, styledef_c::T_ALT, x + 10, y, value);

    if (is_selected)
    {
        if (style->fonts[styledef_c::T_ALT]->def->type_ == kFontTypeImage)
        {
            int cursor = 16;
            HL_WriteText(style, styledef_c::T_TITLE,
                         x - style->fonts[styledef_c::T_TITLE]->StringWidth((const char *)&cursor) / 2, y,
                         (const char *)&cursor);
        }
        else if (style->fonts[styledef_c::T_ALT]->def->type_ == kFontTypeTrueType)
            HL_WriteText(style, styledef_c::T_TITLE, x - style->fonts[styledef_c::T_TITLE]->StringWidth("+") / 2, y,
                         "+");
        else
            HL_WriteText(style, styledef_c::T_TITLE, x - style->fonts[styledef_c::T_TITLE]->StringWidth("*") / 2, y,
                         "*");
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

static const char *GetSkillName(skill_t skill)
{
    switch (skill)
    {
    case sk_baby:
        return language["MenuDifficulty1"];
    case sk_easy:
        return language["MenuDifficulty2"];
    case sk_medium:
        return language["MenuDifficulty3"];
    case sk_hard:
        return language["MenuDifficulty4"];
    case sk_nightmare:
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

static const char *GetPlayerDamResName(int res)
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

void M_NetHostBegun(void)
{
    host_pos = 0;

    if (ng_params)
        delete ng_params;

    ng_params = new newgame_params_c;

    ng_params->CopyFlags(&global_flags);

    ng_params->map = G_LookupMap("1");

    if (!ng_params->map)
        ng_params->map = mapdefs[0];

    host_want_bots = 0;
}

static void ChangeGame(newgame_params_c *param, int dir)
{
    gamedef_c *closest  = nullptr;
    gamedef_c *furthest = nullptr;

    for (auto def : gamedefs)
    {
        mapdef_c *first_map = mapdefs.Lookup(def->firstmap.c_str());

        if (!first_map || !G_MapExists(first_map))
            continue;

        const char *old_name = param->map->episode->name.c_str();
        const char *new_name = def->name.c_str();

        int compare = DDF_CompareName(new_name, old_name);

        if (compare == 0)
            continue;

        if (compare * dir > 0)
        {
            if (!closest || dir * DDF_CompareName(new_name, closest->name.c_str()) < 0)
                closest = def;
        }
        else
        {
            if (!furthest || dir * DDF_CompareName(new_name, furthest->name.c_str()) < 0)
                furthest = def;
        }
    }

    I_Debugf("DIR: %d  CURRENT: %s   CLOSEST: %s   FURTHEST: %s\n", dir, ng_params->map->episode->name.c_str(),
             closest ? closest->name.c_str() : "none", furthest ? furthest->name.c_str() : "none");

    if (closest)
    {
        param->map = mapdefs.Lookup(closest->firstmap.c_str());
        SYS_ASSERT(param->map);
        return;
    }

    // could not find the next/previous map, hence wrap around
    if (furthest)
    {
        param->map = mapdefs.Lookup(furthest->firstmap.c_str());
        SYS_ASSERT(param->map);
        return;
    }
}

static void ChangeLevel(newgame_params_c *param, int dir)
{
    mapdef_c *closest  = nullptr;
    mapdef_c *furthest = nullptr;

    for (auto def : mapdefs)
    {
        if (def->episode != param->map->episode)
            continue;

        const char *old_name = param->map->name.c_str();
        const char *new_name = def->name.c_str();

        int compare = DDF_CompareName(new_name, old_name);

        if (compare == 0)
            continue;

        if (compare * dir > 0)
        {
            if (!closest || dir * DDF_CompareName(new_name, closest->name.c_str()) < 0)
                closest = def;
        }
        else
        {
            if (!furthest || dir * DDF_CompareName(new_name, furthest->name.c_str()) < 0)
                furthest = def;
        }
    }

    if (closest)
    {
        param->map = closest;
        return;
    }

    // could not find the next/previous map, hence wrap around
    if (furthest)
        param->map = furthest;
}

static void HostChangeOption(int opt, int key)
{
    int dir = (key == KEYD_LEFTARROW || key == KEYD_GP_LEFT) ? -1 : +1;

    switch (opt)
    {
    case 0: // Game
        ChangeGame(ng_params, dir);
        break;

    case 1: // Level
        ChangeLevel(ng_params, dir);
        break;

    case 2: // Mode
    {
        ng_params->deathmatch += dir;

        if (ng_params->deathmatch < 0)
            ng_params->deathmatch = 2;
        else if (ng_params->deathmatch > 2)
            ng_params->deathmatch = 0;

        break;
    }

    case 3: // Skill
        ng_params->skill = (skill_t)((int)ng_params->skill + dir);
        if ((int)ng_params->skill < (int)sk_baby || (int)ng_params->skill > 250)
            ng_params->skill = sk_nightmare;
        else if ((int)ng_params->skill > (int)sk_nightmare)
            ng_params->skill = sk_baby;

        break;

    case 4: // Bots
        host_want_bots += dir;

        if (host_want_bots < 0)
            host_want_bots = 3;
        else if (host_want_bots > 3)
            host_want_bots = 0;

        break;

    case 5: // Bot Skill
        bot_skill = bot_skill.d + dir;
        bot_skill = HMM_Clamp(0, bot_skill.d, 4);

        break;

    case 6:
        player_dm_dr = player_dm_dr.d + dir;
        player_dm_dr = HMM_Clamp(0, player_dm_dr.d, 18);

        break;

    case 7: // Monsters
        if (ng_params->flags->fastparm)
        {
            ng_params->flags->fastparm   = false;
            ng_params->flags->nomonsters = (dir > 0);
        }
        else if (ng_params->flags->nomonsters == (dir < 0))
        {
            ng_params->flags->fastparm   = true;
            ng_params->flags->nomonsters = false;
        }
        else
            ng_params->flags->nomonsters = (dir < 0);

        break;

    case 8: // Item-Respawn
        ng_params->flags->itemrespawn = !ng_params->flags->itemrespawn;
        break;

    case 9: // Team-Damage
        ng_params->flags->team_damage = !ng_params->flags->team_damage;
        break;

    default:
        break;
    }
}

static void HostAccept(void)
{
    // create local player and bots
    ng_params->SinglePlayer(host_want_bots);

    ng_params->level_skip = true;

    netgame_menuon = 3;

#if 1
    ListAccept();
#endif
}

void M_DrawHostMenu(void)
{
    SYS_ASSERT(ng_host_style);

    ng_host_style->DrawBackground();

    int CenterX;
    CenterX = 160;
    CenterX -= (ng_host_style->fonts[styledef_c::T_HEADER]->StringWidth("Bot Match Settings") *
                ng_host_style->def->text[styledef_c::T_HEADER].scale) /
               2;

    HL_WriteText(ng_host_style, styledef_c::T_HEADER, CenterX, 25, "Bot Match Settings");

    int y      = 40;
    int idx    = 0;
    int deltay = 2 +
                 (ng_host_style->fonts[styledef_c::T_TEXT]->NominalHeight() *
                  ng_host_style->def->text[styledef_c::T_TEXT].scale) +
                 ng_host_style->def->entry_spacing;

    if (!ng_params->map->episode->description.empty())
        DrawKeyword(idx, ng_host_style, y, "Episode", language[ng_params->map->episode->description.c_str()]);
    else
        DrawKeyword(idx, ng_host_style, y, "Episode", language[ng_params->map->episode_name.c_str()]);

    y += deltay;
    idx++;

    DrawKeyword(idx, ng_host_style, y, "Level", ng_params->map->name.c_str());
    y += deltay + (deltay / 2);
    idx++;

    DrawKeyword(idx, ng_host_style, y, "Mode", GetModeName(ng_params->deathmatch));
    y += deltay;
    idx++;

    DrawKeyword(idx, ng_host_style, y, "Skill", GetSkillName(ng_params->skill));
    y += deltay;
    idx++;

    DrawKeyword(idx, ng_host_style, y, "Bots", epi::StringFormat("%d", host_want_bots).c_str());
    y += deltay;
    idx++;

    int skill = HMM_Clamp(0, bot_skill.d, 4);
    DrawKeyword(idx, ng_host_style, y, "Bot Skill", GetBotSkillName(skill));
    y += deltay;
    idx++;

    int dm_damage_resistance = HMM_Clamp(0, player_dm_dr.d, 18);
    DrawKeyword(idx, ng_host_style, y, "Player Damage Resistance", GetPlayerDamResName(dm_damage_resistance));
    y += deltay;
    idx++;

    int x = 150 - (ng_host_style->fonts[styledef_c::T_TEXT]->StringWidth("(Deathmatch Only)") *
                   ng_host_style->def->text[styledef_c::T_TEXT].scale);
    HL_WriteText(ng_host_style, idx - 1 == host_pos ? 2 : 0, x, y, "(Deathmatch Only)");
    y += deltay;

    DrawKeyword(idx, ng_host_style, y, "Monsters",
                ng_params->flags->nomonsters ? "OFF"
                : ng_params->flags->fastparm ? "FAST"
                                             : "ON");
    y += deltay;
    idx++;

    DrawKeyword(idx, ng_host_style, y, "Item Respawn", ng_params->flags->itemrespawn ? "ON" : "OFF");
    y += deltay;
    idx++;

    DrawKeyword(idx, ng_host_style, y, "Team Damage", ng_params->flags->team_damage ? "ON" : "OFF");
    y += (deltay * 2);
    idx++;

    CenterX = 160;
    CenterX -= (ng_host_style->fonts[styledef_c::T_TEXT]->StringWidth("Start") *
                ng_host_style->def->text[styledef_c::T_TEXT].scale) /
               2;

    HL_WriteText(ng_host_style, (host_pos == idx) ? styledef_c::T_HELP : styledef_c::T_TEXT, CenterX, y, "Start");
}

bool M_NetHostResponder(event_t *ev, int ch)
{
    if (ch == KEYD_ENTER || ch == KEYD_GP_A || ch == KEYD_MOUSE1)
    {
        if (host_pos == (HOST_OPTIONS - 1))
        {
            HostAccept();
            S_StartFX(sfx_pistol);
            return true;
        }
    }

    if (ch == KEYD_DOWNARROW || ch == KEYD_WHEEL_DN || ch == KEYD_GP_DOWN)
    {
        host_pos = (host_pos + 1) % HOST_OPTIONS;
        S_StartFX(sfx_pstop);
        return true;
    }
    else if (ch == KEYD_UPARROW || ch == KEYD_WHEEL_UP || ch == KEYD_GP_UP)
    {
        host_pos = (host_pos + HOST_OPTIONS - 1) % HOST_OPTIONS;
        S_StartFX(sfx_pstop);
        return true;
    }

    if (ch == KEYD_LEFTARROW || ch == KEYD_RIGHTARROW || ch == KEYD_GP_LEFT || ch == KEYD_GP_RIGHT ||
        ch == KEYD_ENTER || ch == KEYD_GP_A || ch == KEYD_MOUSE1)
    {
        HostChangeOption(host_pos, ch);
        S_StartFX(sfx_stnmov);
        return true;
    }

    return false;
}

void M_NetHostTicker(void)
{
    // nothing needed
}

//----------------------------------------------------------------------------

static void NetGameStartLevel(void)
{
    // -KM- 1998/12/17 Clear the intermission.
    WI_Clear();

    G_DeferredNewGame(*ng_params);
}

void M_DrawPlayerList(void)
{
    HUD_SetAlpha(0.64f);
    HUD_SolidBox(0, 0, 320, 200, SG_BLACK_RGBA32);
    HUD_SetAlpha();

    HL_WriteText(ng_list_style, 2, 80, 10, "PLAYER LIST");

    int y = 30;
    int i;

    int humans = 0;

    ///	for (i = 0; i < ng_params->total_players; i++)
    ///		if (! (ng_params->players[i] & PFL_Bot))
    ///			humans++;

    for (i = 0; i < ng_params->total_players; i++)
    {
        int flags = ng_params->players[i];

        if (flags & PFL_Bot)
            continue;

        humans++;

        int bots_here = 0;

        for (int j = 0; j < ng_params->total_players; j++)
        {
            if ((ng_params->players[j] & PFL_Bot) && (ng_params->nodes[j] == ng_params->nodes[i]))
                bots_here++;
        }

        HL_WriteText(ng_list_style, (flags & PFL_Network) ? 0 : 3, 20, y, epi::StringFormat("PLAYER %d", humans).c_str());

        HL_WriteText(ng_list_style, 1, 100, y, "Local");

        HL_WriteText(ng_list_style, (flags & PFL_Network) ? 0 : 3, 200, y,
                     epi::StringFormat("%d BOTS", bots_here).c_str());
        y += 10;
    }

    HL_WriteText(ng_list_style, 2, 40, 140, "Press <ENTER> to Start Game");
}

static void ListAccept()
{
    S_StartFX(sfx_pistol);

    netgame_menuon = 0;
    M_ClearMenus();

    NetGameStartLevel();
}

bool M_NetListResponder(event_t *ev, int ch)
{
    if (ch == KEYD_ENTER || ch == KEYD_GP_A)
    {
        ListAccept();
        return true;
    }

    return false;
}

void M_NetListTicker(void)
{
    // nothing needed
}

//----------------------------------------------------------------------------

void M_NetGameInit(void)
{
    netgame_menuon = 0;

    host_pos = 0;

    // load styles
    styledef_c *def;
    style_c    *ng_default;

    def = styledefs.Lookup("OPTIONS");
    if (!def)
        def = default_style;
    ng_default = hu_styles.Lookup(def);

    def           = styledefs.Lookup("HOST NETGAME");
    ng_host_style = def ? hu_styles.Lookup(def) : ng_default;

    def           = styledefs.Lookup("NET PLAYER LIST");
    ng_list_style = def ? hu_styles.Lookup(def) : ng_default;
}

void M_NetGameDrawer(void)
{
    switch (netgame_menuon)
    {
    case 1:
        M_DrawHostMenu();
        return;
    case 3:
        M_DrawPlayerList();
        return;
    }

    I_Error("INTERNAL ERROR: netgame_menuon=%d\n", netgame_menuon);
}

bool M_NetGameResponder(event_t *ev, int ch)
{
    switch (ch)
    {
    case KEYD_MOUSE2:
    case KEYD_MOUSE3:
    case KEYD_ESCAPE:
    case KEYD_GP_B: {
        netgame_menuon = 0;
        M_ClearMenus();

        S_StartFX(sfx_pistol);
        return true;
    }
    }

    switch (netgame_menuon)
    {
    case 1:
        return M_NetHostResponder(ev, ch);
    case 3:
        return M_NetListResponder(ev, ch);
    }

    return false;
}

void M_NetGameTicker(void)
{
    switch (netgame_menuon)
    {
    case 1:
        return M_NetHostTicker();
    case 3:
        return M_NetListTicker();
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
