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

#include "i_defs.h"

#include "main.h"
#include "switch.h"

#include "dm_defs.h"
#include "dm_state.h"
#include "g_game.h"
#include "p_local.h"
#include "r_state.h"
#include "s_sound.h"
#include "w_texture.h"

std::vector<button_t *> active_buttons;

void P_InitSwitchList(void)
{
    // only called at game initialization.
    for (auto sw : switchdefs)
    {
        sw->cache.image[0] = W_ImageLookup(sw->on_name.c_str(), INS_Texture, ILF_Null);
        sw->cache.image[1] = W_ImageLookup(sw->off_name.c_str(), INS_Texture, ILF_Null);
    }
}

//
// Start a button counting down till it turns off.
//
static void StartButton(switchdef_c *sw, line_t *line, bwhere_e w, const image_c *image)
{
    // See if button is already pressed
    if (P_ButtonIsPressed(line))
        return;

    button_t *b = nullptr;

    std::vector<button_t *>::iterator BI;

    for (BI = active_buttons.begin(); BI != active_buttons.end(); BI++)
    {
        if ((*BI)->btimer == 0)
        {
            b = *BI;
            break;
        }
    }

    if (!b)
    {
        b = new button_t;

        active_buttons.push_back(b);
    }

    b->line      = line;
    b->where     = w;
    b->btimer    = sw->time;
    b->off_sound = sw->off_sfx;
    b->bimage    = image;
}

//
// P_ChangeSwitchTexture
//
// Function that changes wall texture.
// Tell it if switch is ok to use again.
//
// -KM- 1998/09/01 All switches referencing a certain tag are switched
//

#define CHECK_SW(PART) (sw->cache.image[k] == side->PART.image)
#define SET_SW(PART)   side->PART.image = sw->cache.image[k ^ 1]
#define OLD_SW         sw->cache.image[k]

void P_ChangeSwitchTexture(line_t *line, bool useAgain, line_special_e specials, bool noSound)
{
    for (int j = 0; j < numlines; j++)
    {
        if (line != &lines[j])
        {
            if (line->tag == 0 || line->tag != lines[j].tag || (specials & LINSP_SwitchSeparate) ||
                (useAgain && line->special && line->special != lines[j].special))
            {
                continue;
            }
        }

        side_t *side = lines[j].side[0];

        position_c *sfx_origin = &lines[j].frontsector->sfx_origin;

        bwhere_e pos = BWH_None;

        // Note: reverse order, give priority to newer switches.
        for (auto iter = switchdefs.rbegin(); iter != switchdefs.rend() && (pos == BWH_None); iter++)
        {
            switchdef_c *sw = *iter;

            if (!sw->cache.image[0] && !sw->cache.image[1])
                continue;

            int k;

            // some like it both ways...
            for (k = 0; k < 2; k++)
            {
                if (CHECK_SW(top))
                {
                    SET_SW(top);
                    pos = BWH_Top;
                    break;
                }
                else if (CHECK_SW(middle))
                {
                    SET_SW(middle);
                    pos = BWH_Middle;
                    break;
                }
                else if (CHECK_SW(bottom))
                {
                    SET_SW(bottom);
                    pos = BWH_Bottom;
                    break;
                }
            } // k < 2

            if (pos != BWH_None)
            {
                // -KM- 98/07/31 Implement sounds
                if (!noSound && sw->on_sfx)
                {
                    SYS_ASSERT(sfx_origin);
                    S_StartFX(sw->on_sfx, SNCAT_Level, sfx_origin);
                    noSound = true;
                }

                if (useAgain)
                    StartButton(sw, &lines[j], pos, OLD_SW);

                break;
            }
        } // it.IsValid() - switchdefs
    }     // j < numlines
}

#undef CHECK_SW
#undef SET_SW
#undef OLD_SW

void P_ClearButtons(void)
{
    std::vector<button_t *>::iterator BI;

    for (BI = active_buttons.begin(); BI != active_buttons.end(); BI++)
    {
        delete (*BI);
    }
    active_buttons.clear();
}

bool P_ButtonIsPressed(line_t *ld)
{
    std::vector<button_t *>::iterator BI;

    for (BI = active_buttons.begin(); BI != active_buttons.end(); BI++)
    {
        if ((*BI)->btimer > 0 && (*BI)->line == ld)
            return true;
    }

    return false;
}

void P_UpdateButtons(void)
{
    std::vector<button_t *>::iterator BI;

    for (BI = active_buttons.begin(); BI != active_buttons.end(); BI++)
    {
        button_t *b = *BI;
        SYS_ASSERT(b);

        if (b->btimer == 0)
            continue;

        b->btimer--;

        if (b->btimer == 0)
        {
            switch (b->where)
            {
            case BWH_Top:
                b->line->side[0]->top.image = b->bimage;
                break;

            case BWH_Middle:
                b->line->side[0]->middle.image = b->bimage;
                break;

            case BWH_Bottom:
                b->line->side[0]->bottom.image = b->bimage;
                break;

            case BWH_None:
                I_Error("INTERNAL ERROR: bwhere is BWH_None!\n");
            }

            if (b->off_sound)
            {
                S_StartFX(b->off_sound, SNCAT_Level, &b->line->frontsector->sfx_origin);
            }

            Z_Clear(b, button_t, 1);
        }
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
