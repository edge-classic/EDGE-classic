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

#include "hu_stuff.h"

#include "con_gui.h"
#include "con_main.h"
#include "dm_state.h"
#include "e_input.h"
#include "e_main.h"
#include "f_interm.h"
#include "g_game.h"
#include "hu_draw.h"
#include "hu_style.h"
#include "m_menu.h"
#include "n_network.h"
#include "s_sound.h"
#include "w_wad.h"

//
// Locally used constants, shortcuts.
//
// -ACB- 1998/08/09 Removed the HUDTITLE stuff; Use current_map->description.
//

static constexpr uint16_t kHudMessageTimeout          = (4 * kTicRate);
static constexpr uint16_t kHudImportantMessageTimeout = (4 * kTicRate);

std::string current_map_title;

static bool message_on;
static bool important_message_on;
static bool message_no_overwrite;

static std::string current_message;
static std::string current_important_message_message;
static int         message_counter;
static int         important_message_counter;

Style *automap_style;
Style *message_style;
Style *important_message_style;

//
// Heads-up Init
//
void HudInit(void)
{
    // should use language["HeadsUpInit"], but LDF hasn't been loaded yet
    StartupProgressMessage("Setting up HUD...\n");
    hud_tic = 0;
}

// -ACB- 1998/08/09 Used current_map to set the map name in string
void HudStart(void)
{
    const char *string;

    SYS_ASSERT(current_map);

    StyleDefinition *map_styledef = styledefs.Lookup("AUTOMAP");
    if (!map_styledef) map_styledef = default_style;
    automap_style = hud_styles.Lookup(map_styledef);

    StyleDefinition *messages_styledef = styledefs.Lookup("MESSAGES");
    if (!messages_styledef) messages_styledef = default_style;
    message_style = hud_styles.Lookup(messages_styledef);

    StyleDefinition *important_messages_styledef =
        styledefs.Lookup("IMPORTANT_MESSAGES");
    if (!important_messages_styledef)
        important_messages_styledef = default_style;
    important_message_style = hud_styles.Lookup(important_messages_styledef);

    message_on           = false;
    important_message_on = false;
    message_no_overwrite = false;

    // -ACB- 1998/08/09 Use current_map settings
    // if (current_map->description &&
    // language.IsValidRef(current_map->description))
    if (current_map->description_ !=
        "")  // Lobo 2022: if it's wrong, show it anyway
    {
        LogPrint("\n");
        LogPrint("--------------------------------------------------\n");

        ConsoleMessageColor(SG_GREEN_RGBA32);

        string = language[current_map->description_];
        LogPrint("Entering %s\n", string);

        current_map_title = std::string(string);
    }

    // Reset hud_tic each map so it doesn't go super high? - Dasho
    hud_tic = 0;
}

void HudDrawer(void)
{
    ConsoleShowFPS();
    ConsoleShowPosition();

    short tempY;
    short y;
    if (message_on)
    {
        tempY = 0;
        tempY += message_style->fonts_[0]->StringLines(current_message.c_str()) *
                 (message_style->fonts_[0]->NominalHeight() *
                  message_style->definition_->text_[0].scale_);
        tempY /= 2;
        if (message_style->fonts_[0]->StringLines(current_message.c_str()) > 1)
            tempY += message_style->fonts_[0]->NominalHeight() *
                     message_style->definition_->text_[0].scale_;

        y = tempY;

        message_style->DrawBackground();
        HudSetAlignment(0, 0);  // center it
        HudSetAlpha(message_style->definition_->text_->translucency_);
        HudWriteText(message_style, 0, 160, y, current_message.c_str());
        HudSetAlignment();
        HudSetAlpha();
    }

    if (important_message_on)
    {
        tempY = 0;
        tempY -= important_message_style->fonts_[0]->StringLines(
                     current_important_message_message.c_str()) *
                 (important_message_style->fonts_[0]->NominalHeight() *
                  important_message_style->definition_->text_[0].scale_);
        tempY /= 2;
        y = 90 - tempY;
        important_message_style->DrawBackground();
        HudSetAlignment(0, 0);  // center it
        HudSetAlpha(important_message_style->definition_->text_->translucency_);
        HudWriteText(important_message_style, 0, 160, y,
                     current_important_message_message.c_str());
        HudSetAlignment();
        HudSetAlpha();
    }

    // TODO: chat messages
}

// Starts displaying the message.
void HudStartMessage(const char *msg)
{
    // only display message if necessary
    if (!message_no_overwrite)
    {
        current_message = std::string(msg);

        message_on           = true;
        message_counter      = kHudMessageTimeout;
        message_no_overwrite = false;
    }
}

// Starts displaying the message.
void HudStartImportantMessage(const char *msg)
{
    // only display message if necessary
    if (!message_no_overwrite)
    {
        current_important_message_message = std::string(msg);

        important_message_on      = true;
        important_message_counter = kHudImportantMessageTimeout;
        message_no_overwrite      = false;
    }
}

void HudTicker(void)
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
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
