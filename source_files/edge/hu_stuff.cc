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

#include <deque>

#include "con_gui.h"
#include "con_main.h"
#include "dm_state.h"
#include "e_input.h"
#include "e_main.h"
#include "epi.h"
#include "f_interm.h"
#include "g_game.h"
#include "hu_draw.h"
#include "hu_style.h"
#include "i_system.h"
#include "m_menu.h"
#include "n_network.h"
#include "s_sound.h"
#include "w_wad.h"

//
// Locally used constants, shortcuts.
//
// -ACB- 1998/08/09 Removed the HUDTITLE stuff; Use current_map->description.
//

static constexpr uint16_t kHUDMessageTimeout          = (4 * kTicRate);
static constexpr uint16_t kHUDImportantMessageTimeout = (4 * kTicRate);

std::string current_map_title;

static bool important_message_on;

static std::string current_message;
static std::string current_important_message_message;
static int         message_counter;
static int         important_message_counter;

Style *automap_style;
Style *message_style;
Style *important_message_style;

struct HUDMessage
{
    std::string message;
    int counter;
};

static std::deque<HUDMessage> queued_messages;

//
// Heads-up Init
//
void HUDInit(void)
{
    // should use language["HeadsUpInit"], but LDF hasn't been loaded yet
    StartupProgressMessage("Setting up HUD...\n");
    hud_tic = 0;
}

// -ACB- 1998/08/09 Used current_map to set the map name in string
void HUDStart(void)
{
    const char *string;

    EPI_ASSERT(current_map);

    StyleDefinition *map_styledef = styledefs.Lookup("AUTOMAP");
    if (!map_styledef)
        map_styledef = default_style;
    automap_style = hud_styles.Lookup(map_styledef);

    StyleDefinition *messages_styledef = styledefs.Lookup("MESSAGES");
    if (!messages_styledef)
        messages_styledef = default_style;
    message_style = hud_styles.Lookup(messages_styledef);

    StyleDefinition *important_messages_styledef = styledefs.Lookup("IMPORTANT_MESSAGES");
    if (!important_messages_styledef)
        important_messages_styledef = default_style;
    important_message_style = hud_styles.Lookup(important_messages_styledef);

    important_message_on = false;

    // -ACB- 1998/08/09 Use current_map settings
    // if (current_map->description &&
    // language.IsValidRef(current_map->description))
    if (current_map->description_ != "") // Lobo 2022: if it's wrong, show it anyway
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

void HUDDrawer(void)
{
    ConsoleShowFPS();
    ConsoleShowPosition();

    short tempY;
    short y;
    if (!queued_messages.empty())
    {
        tempY = 0;
        tempY += message_style->fonts_[0]->StringLines(current_message.c_str()) *
                 (message_style->fonts_[0]->NominalHeight() * message_style->definition_->text_[0].scale_);
        tempY /= 2;
        if (message_style->fonts_[0]->StringLines(current_message.c_str()) > 1)
            tempY += message_style->fonts_[0]->NominalHeight() * message_style->definition_->text_[0].scale_;

        y = tempY;

        tempY *= 2;

        // review to see if this works with mulitple messages - Dasho
        message_style->DrawBackground();
        HUDSetAlignment(0, 0); // center it
        float alpha = message_style->definition_->text_->translucency_;

        for (const HUDMessage &msg : queued_messages)
        {
            if (msg.counter < kTicRate)
                HUDSetAlpha(alpha * msg.counter / kTicRate);
            HUDWriteText(message_style, 0, 160, y, msg.message.c_str());
            y += tempY;
        }

        HUDSetAlignment();
        HUDSetAlpha();
    }

    if (important_message_on)
    {
        tempY = 0;
        tempY -= important_message_style->fonts_[0]->StringLines(current_important_message_message.c_str()) *
                 (important_message_style->fonts_[0]->NominalHeight() *
                  important_message_style->definition_->text_[0].scale_);
        tempY /= 2;
        y = 90 - tempY;
        important_message_style->DrawBackground();
        HUDSetAlignment(0, 0); // center it
        HUDSetAlpha(important_message_style->definition_->text_->translucency_);
        HUDWriteText(important_message_style, 0, 160, y, current_important_message_message.c_str());
        HUDSetAlignment();
        HUDSetAlpha();
    }

    // TODO: chat messages
}

// Starts displaying the message.
void HUDStartMessage(const char *msg)
{
    // only display message if necessary
    queued_messages.push_front({msg, kHUDMessageTimeout});
    if (queued_messages.size() > 4)
        queued_messages.pop_back();
}

// Starts displaying the message.
void HUDStartImportantMessage(const char *msg)
{
    current_important_message_message = std::string(msg);

    important_message_on      = true;
    important_message_counter = kHUDImportantMessageTimeout;
}

void HUDTicker(void)
{
    // tick down messages
    for (std::deque<HUDMessage>::reverse_iterator rbegin = queued_messages.rbegin(), rend = queued_messages.rend(); rbegin != rend; rbegin++)
    {
        HUDMessage &msg = *rbegin;
        msg.counter--;
        if (msg.counter <= 0)
            queued_messages.pop_back();
    }

    if (important_message_counter && !--important_message_counter)
    {
        important_message_on = false;
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
