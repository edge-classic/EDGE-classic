//----------------------------------------------------------------------------
//  EDGE Heads-Up-Display Code
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

#include "hu_stuff.h"
#include "hu_style.h"
#include "hu_draw.h"

#include "con_main.h"
#include "con_gui.h"
#include "dm_state.h"
#include "e_input.h"
#include "e_main.h"
#include "g_game.h"
#include "f_interm.h"
#include "m_menu.h"
#include "n_network.h"
#include "s_sound.h"
#include "w_wad.h"

//
// Locally used constants, shortcuts.
//
// -ACB- 1998/08/09 Removed the HU_TITLE stuff; Use currmap->description.
//
#define HU_TITLEHEIGHT 1
#define HU_TITLEX      0
#define HU_TITLEY      (200 - 32 - 10)

#define HU_INPUTX      HU_MSGX
#define HU_INPUTY      (HU_MSGY + HU_MSGHEIGHT * 8)
#define HU_INPUTWIDTH  64
#define HU_INPUTHEIGHT 1

bool chat_on;

std::string w_map_title;

static bool message_on;
static bool important_message_on;
static bool message_no_overwrite;

static std::string w_message;
static std::string w_important_message;
static int         message_counter;
static int         important_message_counter;

style_c *automap_style;
style_c *message_style;
style_c *important_message_style;

//
// Heads-up Init
//
void HU_Init(void)
{
    // should use language["HeadsUpInit"], but LDF hasn't been loaded yet
    E_ProgressMessage("Setting up HUD...\n");
    hudtic = 0;
}

// -ACB- 1998/08/09 Used currmap to set the map name in string
void HU_Start(void)
{
    const char *string;

    SYS_ASSERT(currmap);

    styledef_c *map_styledef = styledefs.Lookup("AUTOMAP");
    if (!map_styledef)
        map_styledef = default_style;
    automap_style = hu_styles.Lookup(map_styledef);

    styledef_c *messages_styledef = styledefs.Lookup("MESSAGES");
    if (!messages_styledef)
        messages_styledef = default_style;
    message_style = hu_styles.Lookup(messages_styledef);

    styledef_c *important_messages_styledef = styledefs.Lookup("IMPORTANT_MESSAGES");
    if (!important_messages_styledef)
        important_messages_styledef = default_style;
    important_message_style = hu_styles.Lookup(important_messages_styledef);

    message_on           = false;
    important_message_on = false;
    message_no_overwrite = false;

    // -ACB- 1998/08/09 Use currmap settings
    // if (currmap->description && language.IsValidRef(currmap->description))
    if (currmap->description != "") // Lobo 2022: if it's wrong, show it anyway
    {
        I_Printf("\n");
        I_Printf("--------------------------------------------------\n");

        CON_MessageColor(SG_GREEN_RGBA32);

        string = language[currmap->description];
        I_Printf("Entering %s\n", string);

        w_map_title = std::string(string);
    }

    // Reset hudtic each map so it doesn't go super high? - Dasho
    hudtic = 0;
}

void HU_Drawer(void)
{
    CON_ShowFPS();
    CON_ShowPosition();

    short tempY;
    short y;
    if (message_on)
    {

        tempY = 0;
        tempY += message_style->fonts[0]->StringLines(w_message.c_str()) *
                 (message_style->fonts[0]->NominalHeight() * message_style->def->text[0].scale);
        tempY /= 2;
        if (message_style->fonts[0]->StringLines(w_message.c_str()) > 1)
            tempY += message_style->fonts[0]->NominalHeight() * message_style->def->text[0].scale;

        y = tempY;

        message_style->DrawBackground();
        HUD_SetAlignment(0, 0); // center it
        HUD_SetAlpha(message_style->def->text->translucency);
        HL_WriteText(message_style, 0, 160, y, w_message.c_str());
        HUD_SetAlignment();
        HUD_SetAlpha();
    }

    if (important_message_on)
    {
        tempY = 0;
        tempY -= important_message_style->fonts[0]->StringLines(w_important_message.c_str()) *
                 (important_message_style->fonts[0]->NominalHeight() * important_message_style->def->text[0].scale);
        tempY /= 2;
        y = 90 - tempY;
        important_message_style->DrawBackground();
        HUD_SetAlignment(0, 0); // center it
        HUD_SetAlpha(important_message_style->def->text->translucency);
        HL_WriteText(important_message_style, 0, 160, y, w_important_message.c_str());
        HUD_SetAlignment();
        HUD_SetAlpha();
    }

    // TODO: chat messages
}

// Starts displaying the message.
void HU_StartMessage(const char *msg)
{
    // only display message if necessary
    if (!message_no_overwrite)
    {
        w_message = std::string(msg);

        message_on           = true;
        message_counter      = HU_MSGTIMEOUT;
        message_no_overwrite = false;
    }
}

// Starts displaying the message.
void HU_StartImportantMessage(const char *msg)
{

    // only display message if necessary
    if (!message_no_overwrite)
    {
        w_important_message = std::string(msg);

        important_message_on      = true;
        important_message_counter = HU_IMPMSGTIMEOUT;
        message_no_overwrite      = false;
    }
}

void HU_Ticker(void)
{
    // tick down message counter if message is up
    if (message_counter && !--message_counter)
    {
        message_on           = false;
        message_no_overwrite = false;
    }

    if (important_message_counter && !--important_message_counter)
    {
        important_message_on = false;
        message_no_overwrite = false;
    }

    // check for incoming chat characters
    if (!netgame)
        return;

    for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
    {
        player_t *p = players[pnum];
        if (!p)
            continue;

        if (pnum == consoleplayer)
            continue;

        char c = p->cmd.chatchar;

        p->cmd.chatchar = 0;

        if (c)
        {
            /* TODO: chat stuff */
        }
    }
}

void HU_QueueChatChar(char c)
{
    // TODO
}

char HU_DequeueChatChar(void)
{
    return 0; // TODO
}

bool HU_Responder(event_t *ev)
{
    if (ev->type != ev_keyup && ev->type != ev_keydown)
        return false;

    int c = ev->value.key.sym;

    if (ev->type != ev_keydown)
        return false;

    // TODO: chat stuff
    if (false)
    {
        //...
        return true;
    }

    if (c == KEYD_ENTER)
    {
        message_on      = true;
        message_counter = HU_MSGTIMEOUT;
    }

    return false;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
