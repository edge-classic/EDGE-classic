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

static constexpr uint16_t kHUDMessageTimeout          = (4 * kTicRate);
static constexpr uint16_t kHUDImportantMessageTimeout = (4 * kTicRate);

std::string current_map_title;

static bool important_message_on;

static std::string current_important_message;
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

static void UpdatePickupMessages(ConsoleVariable *self)
{
    // Account for 0 index from options menu
    while (queued_messages.size() > self->d_ + 1)
        queued_messages.pop_back();
}

EDGE_DEFINE_CONSOLE_VARIABLE_WITH_CALLBACK_CLAMPED(maximum_pickup_messages, "3", kConsoleVariableFlagArchive, UpdatePickupMessages, 0, 3);

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

        current_map_title = string;
    }

    // Reset hud_tic each map so it doesn't go super high? - Dasho
    hud_tic = 0;
}

void HUDDrawer(void)
{
    ConsoleShowFPS();
    ConsoleShowPosition();

    short y = 0;
    if (!queued_messages.empty())
    {
        float alpha = message_style->definition_->text_->translucency_;
        message_style->DrawBackground();
        if (!message_style->definition_->entry_align_string_.empty())
        {
            switch (message_style->definition_->entry_alignment_)
            {
                case StyleDefinition::kAlignmentLeft:
                {
                    HUDSetAlignment(-1, -1);
                    for (size_t i = 0, i_end = queued_messages.size(); i < i_end; i++)
                    {
                        const HUDMessage &msg = queued_messages[i];
                        const char *current_message = msg.message.c_str();
                        if (i > 0)
                            y += HUDStringHeight(queued_messages[i-1].message.c_str());
                        if (msg.counter < kTicRate)
                            HUDSetAlpha(alpha * msg.counter / kTicRate);
                        HUDWriteText(message_style, 0, hud_x_left, y, current_message);
                    }
                    break;
                }
                case StyleDefinition::kAlignmentRight:
                {
                    HUDSetAlignment(1, -1);
                    for (size_t i = 0, i_end = queued_messages.size(); i < i_end; i++)
                    {
                        const HUDMessage &msg = queued_messages[i];
                        const char *current_message = msg.message.c_str();
                        if (i > 0)
                            y += HUDStringHeight(queued_messages[i-1].message.c_str());
                        if (msg.counter < kTicRate)
                            HUDSetAlpha(alpha * msg.counter / kTicRate);
                        HUDWriteText(message_style, 0, hud_x_right, y, current_message);
                    }
                    break;
                }
                default:
                {
                    HUDSetAlignment(0, -1);
                    for (size_t i = 0, i_end = queued_messages.size(); i < i_end; i++)
                    {
                        const HUDMessage &msg = queued_messages[i];
                        const char *current_message = msg.message.c_str();
                        if (i > 0)
                            y += HUDStringHeight(queued_messages[i-1].message.c_str());
                        if (msg.counter < kTicRate)
                            HUDSetAlpha(alpha * msg.counter / kTicRate);
                        HUDWriteText(message_style, 0, 160, y, current_message);
                    }
                    break;
                }
            }
        }
        else
        {
            HUDSetAlignment(0, -1); // center it
            for (size_t i = 0, i_end = queued_messages.size(); i < i_end; i++)
            {
                const HUDMessage &msg = queued_messages[i];
                const char *current_message = msg.message.c_str();
                if (i > 0)
                    y += HUDStringHeight(queued_messages[i-1].message.c_str());
                if (msg.counter < kTicRate)
                    HUDSetAlpha(alpha * msg.counter / kTicRate);
                HUDWriteText(message_style, 0, 160, y, current_message);
            }
        }

        HUDSetAlignment();
        HUDSetAlpha();
    }

    if (important_message_on)
    {
        short tempY = 0;
        tempY -= StringLines(current_important_message) *
                 (important_message_style->fonts_[0]->NominalHeight() *
                  important_message_style->definition_->text_[0].scale_);
        tempY /= 2;
        y = 90 - tempY;
        important_message_style->DrawBackground();
        HUDSetAlignment(0, 0); // center it
        if (important_message_counter < kTicRate)
            HUDSetAlpha(important_message_style->definition_->text_->translucency_ * important_message_counter / kTicRate);
        else
            HUDSetAlpha(important_message_style->definition_->text_->translucency_);
        HUDWriteText(important_message_style, 0, 160, y, current_important_message.c_str());
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
    if (queued_messages.size() > maximum_pickup_messages.d_ + 1)
        queued_messages.pop_back();
}

// Starts displaying the message.
void HUDStartImportantMessage(const char *msg)
{
    current_important_message = msg;

    important_message_on      = true;
    important_message_counter = kHUDImportantMessageTimeout;
}

void HUDTicker(void)
{
    // tick down messages
    for (int i = queued_messages.size() - 1; i >= 0; i--)
    {
        HUDMessage &msg = queued_messages[i];
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
