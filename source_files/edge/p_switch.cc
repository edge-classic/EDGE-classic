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



#include "main.h"
#include "switch.h"

#include "dm_defs.h"
#include "dm_state.h"
#include "g_game.h"
#include "p_local.h"
#include "r_state.h"
#include "s_sound.h"
#include "w_texture.h"

std::vector<Button *> active_buttons;

void InitializeSwitchList(void)
{
    // only called at game initialization.
    for (auto sw : switchdefs)
    {
        sw->cache_.image[0] = W_ImageLookup(sw->on_name_.c_str(), kImageNamespaceTexture, ILF_Null);
        sw->cache_.image[1] = W_ImageLookup(sw->off_name_.c_str(), kImageNamespaceTexture, ILF_Null);
    }
}

//
// Start a button counting down till it turns off.
//
static void StartButton(SwitchDefinition *sw, line_t *line, ButtonPosition w, const image_c *image)
{
    // See if button is already pressed
    if (ButtonIsPressed(line))
        return;

    Button *b = nullptr;

    std::vector<Button *>::iterator BI;

    for (BI = active_buttons.begin(); BI != active_buttons.end(); BI++)
    {
        if ((*BI)->button_timer == 0)
        {
            b = *BI;
            break;
        }
    }

    if (!b)
    {
        b = new Button;

        active_buttons.push_back(b);
    }

    b->line      = line;
    b->where     = w;
    b->button_timer    = sw->time_;
    b->off_sound = sw->off_sfx_;
    b->button_image    = image;
}

//
// ChangeSwitchTexture
//
// Function that changes wall texture.
// Tell it if switch is ok to use again.
//
// -KM- 1998/09/01 All switches referencing a certain tag are switched
//

#define CHECK_SW(PART) (sw->cache_.image[k] == side->PART.image)
#define SET_SW(PART)   side->PART.image = sw->cache_.image[k ^ 1]
#define OLD_SW         sw->cache_.image[k]

void ChangeSwitchTexture(line_t *line, bool useAgain, LineSpecial specials, bool noSound)
{
    for (int j = 0; j < total_level_lines; j++)
    {
        if (line != &level_lines[j])
        {
            if (line->tag == 0 || line->tag != level_lines[j].tag || (specials & kLineSpecialSwitchSeparate) ||
                (useAgain && line->special && line->special != level_lines[j].special))
            {
                continue;
            }
        }

        side_t *side = level_lines[j].side[0];

        Position *sfx_origin = &level_lines[j].frontsector->sfx_origin;

        ButtonPosition pos = kButtonNone;

        // Note: reverse order, give priority to newer switches.
        for (auto iter = switchdefs.rbegin(); iter != switchdefs.rend() && (pos == kButtonNone); iter++)
        {
            SwitchDefinition *sw = *iter;

            if (!sw->cache_.image[0] && !sw->cache_.image[1])
                continue;

            int k;

            // some like it both ways...
            for (k = 0; k < 2; k++)
            {
                if (CHECK_SW(top))
                {
                    SET_SW(top);
                    pos = kButtonTop;
                    break;
                }
                else if (CHECK_SW(middle))
                {
                    SET_SW(middle);
                    pos = kButtonMiddle;
                    break;
                }
                else if (CHECK_SW(bottom))
                {
                    SET_SW(bottom);
                    pos = kButtonBottom;
                    break;
                }
            } // k < 2

            if (pos != kButtonNone)
            {
                // -KM- 98/07/31 Implement sounds
                if (!noSound && sw->on_sfx_)
                {
                    SYS_ASSERT(sfx_origin);
                    S_StartFX(sw->on_sfx_, SNCAT_Level, sfx_origin);
                    noSound = true;
                }

                if (useAgain)
                    StartButton(sw, &level_lines[j], pos, OLD_SW);

                break;
            }
        } // it.IsValid() - switchdefs
    }     // j < total_level_lines
}

#undef CHECK_SW
#undef SET_SW
#undef OLD_SW

void ClearButtons(void)
{
    std::vector<Button *>::iterator BI;

    for (BI = active_buttons.begin(); BI != active_buttons.end(); BI++)
    {
        delete (*BI);
    }
    active_buttons.clear();
}

bool ButtonIsPressed(line_t *ld)
{
    std::vector<Button *>::iterator BI;

    for (BI = active_buttons.begin(); BI != active_buttons.end(); BI++)
    {
        if ((*BI)->button_timer > 0 && (*BI)->line == ld)
            return true;
    }

    return false;
}

void UpdateButtons(void)
{
    std::vector<Button *>::iterator BI;

    for (BI = active_buttons.begin(); BI != active_buttons.end(); BI++)
    {
        Button *b = *BI;
        SYS_ASSERT(b);

        if (b->button_timer == 0)
            continue;

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
                S_StartFX(b->off_sound, SNCAT_Level, &b->line->frontsector->sfx_origin);
            }

            Z_Clear(b, Button, 1);
        }
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
