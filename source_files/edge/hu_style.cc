//----------------------------------------------------------------------------
//  EDGE Heads-up-display Style code
//----------------------------------------------------------------------------
//
//  Copyright (c) 2004-2024 The EDGE Team.
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


#include "hu_style.h"
#include "hu_draw.h"

#include "dm_defs.h"
#include "dm_state.h"
#include "r_colormap.h"
#include "r_draw.h"
#include "r_modes.h"
#include "r_image.h"

// Edge has lots of style
style_container_c hu_styles;

style_c::style_c(StyleDefinition *_def) : def(_def), bg_image(nullptr)
{
    for (int T = 0; T < StyleDefinition::kTotalTextSections; T++)
        fonts[T] = nullptr;
}

style_c::~style_c()
{
    /* nothing to do */
}

void style_c::Load()
{
    if (def->bg_.image_name_.c_str())
    {
        const char *name = def->bg_.image_name_.c_str();

        bg_image = W_ImageLookup(name, kImageNamespaceFlat, ILF_Null);

        if (!bg_image)
            bg_image = W_ImageLookup(name, kImageNamespaceGraphic);
    }

    for (int T = 0; T < StyleDefinition::kTotalTextSections; T++)
    {
        if (def->text_[T].font_)
            fonts[T] = hu_fonts.Lookup(def->text_[T].font_);
    }
}

void style_c::DrawBackground()
{
    float alpha = def->bg_.translucency_;

    if (alpha < 0.02)
        return;

    HUD_SetAlpha(alpha);

    float WS_x = -130;        // Lobo: fixme, this should be calculated, not arbitrary hardcoded ;)
    float WS_w = SCREENWIDTH; // 580;

    if (!bg_image)
    {
        if (!(def->special_ & kStyleSpecialStretchFullScreen))
        {
            WS_x = 1;   // cannot be 0 or WS is invoked
            WS_w = 319; // cannot be 320 or WS is invoked
        }

        if (def->bg_.colour_ != kRGBANoValue)
            HUD_SolidBox(WS_x, 0, WS_w, 200, def->bg_.colour_);
        /*else
            HUD_SolidBox(WS_x, 0, WS_w, 200, T_BLACK);
*/
        HUD_SetAlpha();
        return;
    }

    if (def->special_ & (kStyleSpecialTiled | kStyleSpecialTiledNoScale))
    {
        HUD_SetScale(def->bg_.scale_);

        // HUD_TileImage(0, 0, 320, 200, bg_image);
        HUD_TileImage(WS_x, 0, WS_w, 200, bg_image, 0.0, 0.0);
        HUD_SetScale();
    }
    // Lobo: handle our new special
    if (def->special_ & kStyleSpecialStretchFullScreen)
    {
        HUD_SetScale(def->bg_.scale_);

        HUD_StretchImage(WS_x, 0, WS_w, 200, bg_image, 0.0, 0.0);
        // HUD_DrawImage(CenterX, 0, bg_image);

        HUD_SetScale();
    }

    // Lobo: positioning and size will be determined by images.ddf
    if (def->special_ == 0)
    {
        // Lobo: calculate centering on screen
        float CenterX = 0;

        CenterX = 160;
        CenterX -= (bg_image->actual_w * bg_image->scale_x) / 2;

        HUD_SetScale(def->bg_.scale_);
        // HUD_StretchImage(0, 0, 320, 200, bg_image);
        HUD_DrawImage(CenterX, 0, bg_image);

        HUD_SetScale();
    }

    HUD_SetAlpha();
}

// ---> style_container_c class

//
// style_container_c::Lookup()
//
// Never returns nullptr.
//
style_c *style_container_c::Lookup(StyleDefinition *def)
{
    SYS_ASSERT(def);

    for (auto iter = begin(); iter != end(); iter++)
    {
        style_c *st = *iter;

        if (def == st->def)
            return st;
    }

    style_c *new_st = new style_c(def);

    new_st->Load();
    push_back(new_st);

    return new_st;
}

//
// HL_WriteText
//
void HL_WriteText(style_c *style, int text_type, int x, int y, const char *str, float scale)
{
    HUD_SetFont(style->fonts[text_type]);
    HUD_SetScale(scale * style->def->text_[text_type].scale_);

    const Colormap *colmap = style->def->text_[text_type].colmap_;

    if (colmap)
        HUD_SetTextColor(V_GetFontColor(colmap));

    HUD_DrawText(x, y, str);

    HUD_SetFont();
    HUD_SetScale();
    HUD_SetTextColor();
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
