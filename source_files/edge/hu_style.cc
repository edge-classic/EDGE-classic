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

#include "dm_defs.h"
#include "dm_state.h"
#include "hu_draw.h"
#include "r_colormap.h"
#include "r_draw.h"
#include "r_image.h"
#include "r_modes.h"

// Edge has lots of style
StyleContainer hud_styles;

Style::Style(StyleDefinition *definition)
    : definition_(definition), background_image_(nullptr)
{
    for (int T = 0; T < StyleDefinition::kTotalTextSections; T++)
        fonts_[T] = nullptr;
}

Style::~Style() { /* nothing to do */ }

void Style::Load()
{
    if (definition_->bg_.image_name_.c_str())
    {
        const char *name = definition_->bg_.image_name_.c_str();

        background_image_ = ImageLookup(name, kImageNamespaceFlat, kImageLookupNull);

        if (!background_image_)
            background_image_ = ImageLookup(name, kImageNamespaceGraphic);
    }

    for (int T = 0; T < StyleDefinition::kTotalTextSections; T++)
    {
        if (definition_->text_[T].font_)
            fonts_[T] = hud_fonts.Lookup(definition_->text_[T].font_);
    }
}

void Style::DrawBackground()
{
    float alpha = definition_->bg_.translucency_;

    if (alpha < 0.02) return;

    HudSetAlpha(alpha);

    float WS_x = -130;  // Lobo: fixme, this should be calculated, not arbitrary
                        // hardcoded ;)
    float WS_w = current_screen_width;  // 580;

    if (!background_image_)
    {
        if (!(definition_->special_ & kStyleSpecialStretchFullScreen))
        {
            WS_x = 1;    // cannot be 0 or WS is invoked
            WS_w = 319;  // cannot be 320 or WS is invoked
        }

        if (definition_->bg_.colour_ != kRGBANoValue)
            HudSolidBox(WS_x, 0, WS_w, 200, definition_->bg_.colour_);
        /*else
            HudSolidBox(WS_x, 0, WS_w, 200, T_BLACK);
*/
        HudSetAlpha();
        return;
    }

    if (definition_->special_ &
        (kStyleSpecialTiled | kStyleSpecialTiledNoScale))
    {
        HudSetScale(definition_->bg_.scale_);

        // HudTileImage(0, 0, 320, 200, background_image_);
        HudTileImage(WS_x, 0, WS_w, 200, background_image_, 0.0, 0.0);
        HudSetScale();
    }
    // Lobo: handle our new special
    if (definition_->special_ & kStyleSpecialStretchFullScreen)
    {
        HudSetScale(definition_->bg_.scale_);

        HudStretchImage(WS_x, 0, WS_w, 200, background_image_, 0.0, 0.0);
        // HudDrawImage(CenterX, 0, background_image_);

        HudSetScale();
    }

    // Lobo: positioning and size will be determined by images.ddf
    if (definition_->special_ == 0)
    {
        // Lobo: calculate centering on screen
        float CenterX = 0;

        CenterX = 160;
        CenterX -=
            (background_image_->actual_width_ * background_image_->scale_x_) / 2;

        HudSetScale(definition_->bg_.scale_);
        // HudStretchImage(0, 0, 320, 200, background_image_);
        HudDrawImage(CenterX, 0, background_image_);

        HudSetScale();
    }

    HudSetAlpha();
}

// ---> StyleContainer class

//
// StyleContainer::Lookup()
//
// Never returns nullptr.
//
Style *StyleContainer::Lookup(StyleDefinition *definition)
{
    SYS_ASSERT(definition);

    for (std::vector<Style *>::iterator iter = begin(), iter_end = end();
         iter != iter_end; iter++)
    {
        Style *st = *iter;

        if (definition == st->definition_) return st;
    }

    Style *new_st = new Style(definition);

    new_st->Load();
    push_back(new_st);

    return new_st;
}

//
// HudWriteText
//
void HudWriteText(Style *style, int text_type, int x, int y, const char *str,
                  float scale)
{
    HudSetFont(style->fonts_[text_type]);
    HudSetScale(scale * style->definition_->text_[text_type].scale_);

    const Colormap *colmap = style->definition_->text_[text_type].colmap_;

    if (colmap) HudSetTextColor(GetFontColor(colmap));

    HudDrawText(x, y, str);

    HudSetFont();
    HudSetScale();
    HudSetTextColor();
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
