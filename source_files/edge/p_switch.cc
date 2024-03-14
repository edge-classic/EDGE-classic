//----------------------------------------------------------------------------
//  EDGE Switch Handling Code
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

#include "dm_defs.h"
#include "dm_state.h"
#include "epi.h"
#include "g_game.h"
#include "main.h"
#include "p_local.h"
#include "r_state.h"
#include "s_sound.h"
#include "switch.h"
#include "w_texture.h"

std::vector<Button *> active_buttons;

void InitializeSwitchList(void)
{
    // only called at game initialization.
    for (SwitchDefinition *sw : switchdefs)
    {
        sw->cache_.image[0] = ImageLookup(
            sw->on_name_.c_str(), kImageNamespaceTexture, kImageLookupNull);
        sw->cache_.image[1] = ImageLookup(
            sw->off_name_.c_str(), kImageNamespaceTexture, kImageLookupNull);
    }
}

//
// Start a button counting down till it turns off.
//
static void StartButton(SwitchDefinition *sw, Line *line, ButtonPosition w,
                        const Image *image)
{
    // See if button is already pressed
    if (ButtonIsPressed(line)) return;

    Button *b = nullptr;

    for (std::vector<Button *>::iterator iter     = active_buttons.begin(),
                                         iter_end = active_buttons.end();
         iter != iter_end; iter++)
    {
        b = *iter;
        EPI_ASSERT(b);
        if (b->button_timer == 0) { break; }
        else { b = nullptr; }
    }

    if (!b)
    {
        b = new Button;

        active_buttons.push_back(b);
    }

    b->line         = line;
    b->where        = w;
    b->button_timer = sw->time_;
    b->off_sound    = sw->off_sfx_;
    b->button_image = image;
}

//
// ChangeSwitchTexture
//
// Function that changes wall texture.
// Tell it if switch is ok to use again.
//
// -KM- 1998/09/01 All switches referencing a certain tag are switched
//

#define EDGE_CHECK_SWITCH(PART) (sw->cache_.image[k] == side->PART.image)
#define EDGE_SET_SWITCH(PART)   side->PART.image = sw->cache_.image[k ^ 1]

void ChangeSwitchTexture(Line *line, bool useAgain, LineSpecial specials,
                         bool noSound)
{
    for (int j = 0; j < total_level_lines; j++)
    {
        if (line != &level_lines[j])
        {
            if (line->tag == 0 || line->tag != level_lines[j].tag ||
                (specials & kLineSpecialSwitchSeparate) ||
                (useAgain && line->special &&
                 line->special != level_lines[j].special))
            {
                continue;
            }
        }

        Side *side = level_lines[j].side[0];

        Position *sound_effects_origin =
            &level_lines[j].front_sector->sound_effects_origin;

        ButtonPosition pos = kButtonNone;

        // Note: reverse order, give priority to newer switches.
        for (std::vector<SwitchDefinition *>::reverse_iterator
                 iter     = switchdefs.rbegin(),
                 iter_end = switchdefs.rend();
             iter != iter_end && (pos == kButtonNone); iter++)
        {
            SwitchDefinition *sw = *iter;

            if (!sw->cache_.image[0] && !sw->cache_.image[1]) continue;

            int k;

            // some like it both ways...
            for (k = 0; k < 2; k++)
            {
                if (EDGE_CHECK_SWITCH(top))
                {
                    EDGE_SET_SWITCH(top);
                    pos = kButtonTop;
                    break;
                }
                else if (EDGE_CHECK_SWITCH(middle))
                {
                    EDGE_SET_SWITCH(middle);
                    pos = kButtonMiddle;
                    break;
                }
                else if (EDGE_CHECK_SWITCH(bottom))
                {
                    EDGE_SET_SWITCH(bottom);
                    pos = kButtonBottom;
                    break;
                }
            }  // k < 2

            if (pos != kButtonNone)
            {
                // -KM- 98/07/31 Implement sounds
                if (!noSound && sw->on_sfx_)
                {
                    EPI_ASSERT(sound_effects_origin);
                    StartSoundEffect(sw->on_sfx_, kCategoryLevel,
                                     sound_effects_origin);
                    noSound = true;
                }

                if (useAgain)
                    StartButton(sw, &level_lines[j], pos, sw->cache_.image[k]);

                break;
            }
        }  // it.IsValid() - switchdefs
    }      // j < total_level_lines
}

#undef EDGE_CHECK_SWITCH
#undef EDGE_SET_SWITCH

void ClearButtons(void)
{
    for (std::vector<Button *>::iterator iter     = active_buttons.begin(),
                                         iter_end = active_buttons.end();
         iter != iter_end; iter++)
    {
        Button *b = *iter;
        delete b;
    }
    active_buttons.clear();
}

bool ButtonIsPressed(Line *ld)
{
    for (std::vector<Button *>::iterator iter     = active_buttons.begin(),
                                         iter_end = active_buttons.end();
         iter != iter_end; iter++)
    {
        Button *b = *iter;
        EPI_ASSERT(b);
        if (b->button_timer > 0 && b->line == ld) return true;
    }

    return false;
}

void UpdateButtons(void)
{
    for (std::vector<Button *>::iterator iter     = active_buttons.begin(),
                                         iter_end = active_buttons.end();
         iter != iter_end; iter++)
    {
        Button *b = *iter;
        EPI_ASSERT(b);

        if (b->button_timer == 0) continue;

        b->button_timer--;

        if (b->button_timer == 0)
        {
            switch (b->where)
            {
                case kButtonTop:
                    b->line->side[0]->top.image = b->button_image;
                    break;

                case kButtonMiddle:
                    b->line->side[0]->middle.image = b->button_image;
                    break;

                case kButtonBottom:
                    b->line->side[0]->bottom.image = b->button_image;
                    break;

                case kButtonNone:
                    FatalError("INTERNAL ERROR: bwhere is kButtonNone!\n");
            }

            if (b->off_sound)
            {
                StartSoundEffect(b->off_sound, kCategoryLevel,
                                 &b->line->front_sector->sound_effects_origin);
            }

            EPI_CLEAR_MEMORY(b, Button, 1);
        }
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
