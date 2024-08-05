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
#include "epi.h"
#include "hu_draw.h"
#include "r_colormap.h"
#include "r_draw.h"
#include "r_image.h"
#include "r_modes.h"

// Edge has lots of style
StyleContainer hud_styles;

Style::Style(StyleDefinition *definition) : definition_(definition), background_image_(nullptr)
{
    for (int T = 0; T < StyleDefinition::kTotalTextSections; T++)
        fonts_[T] = nullptr;
}

Style::~Style()
{ /* nothing to do */
}

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

    if (alpha < 0.02)
        return;

    HUDSetAlpha(alpha);

    float WS_x = -130;                 // Lobo: fixme, this should be calculated, not arbitrary
                                       // hardcoded ;)
    float WS_w = current_screen_width; // 580;

    if (!background_image_)
    {
        if (!(definition_->special_ & kStyleSpecialStretchFullScreen))
        {
            WS_x = 1;   // cannot be 0 or WS is invoked
            WS_w = 319; // cannot be 320 or WS is invoked
        }

        if (definition_->bg_.colour_ != kRGBANoValue)
            HUDSolidBox(WS_x, 0, WS_w, 200, definition_->bg_.colour_);
        /*else
            HUDSolidBox(WS_x, 0, WS_w, 200, T_BLACK);
*/
        HUDSetAlpha();
        return;
    }

    if (definition_->special_ & (kStyleSpecialTiled | kStyleSpecialTiledNoScale))
    {
        HUDSetScale(definition_->bg_.scale_);

        // HUDTileImage(0, 0, 320, 200, background_image_);
        HUDTileImage(WS_x, 0, WS_w, 200, background_image_, 0.0, 0.0);
        HUDSetScale();
    }
    // Lobo: handle our new special
    if (definition_->special_ & kStyleSpecialStretchFullScreen)
    {
        HUDSetScale(definition_->bg_.scale_);

        HUDStretchImage(WS_x, 0, WS_w, 200, background_image_, 0.0, 0.0);
        // HUDDrawImage(CenterX, 0, background_image_);

        HUDSetScale();
    }

    // Lobo: positioning and size will be determined by images.ddf
    if (definition_->special_ == 0)
    {
        // Lobo: calculate centering on screen
        float CenterX = 0;

        CenterX = 160;
        CenterX -= (background_image_->actual_width_ * background_image_->scale_x_) / 2;

        HUDSetScale(definition_->bg_.scale_);
        // HUDStretchImage(0, 0, 320, 200, background_image_);
        HUDDrawImage(CenterX, 0, background_image_);

        HUDSetScale();
    }

    HUDSetAlpha();
}

// ---> StyleContainer class

//
// StyleContainer::Lookup()
//
// Never returns nullptr.
//
Style *StyleContainer::Lookup(StyleDefinition *definition)
{
    EPI_ASSERT(definition);

    for (std::vector<Style *>::iterator iter = begin(), iter_end = end(); iter != iter_end; iter++)
    {
        Style *st = *iter;

        if (definition == st->definition_)
            return st;
    }

    Style *new_st = new Style(definition);

    new_st->Load();
    push_back(new_st);

    return new_st;
}

//
// HUDWriteText
//
void HUDWriteText(Style *style, int text_type, int x, int y, const char *str, float scale)
{
    HUDSetFont(style->fonts_[text_type]);
    HUDSetScale(scale * style->definition_->text_[text_type].scale_);

    const Colormap *Dropshadow_colmap = style->definition_->text_[text_type].dropshadow_colmap_;
    if (Dropshadow_colmap) //we want a dropshadow
    {
        float Dropshadow_Offset = style->definition_->text_[text_type].dropshadow_offset_;
        Dropshadow_Offset *= style->definition_->text_[text_type].scale_ * scale;

        HUDSetTextColor(GetFontColor(Dropshadow_colmap));
        HUDDrawText(x + Dropshadow_Offset, y + Dropshadow_Offset, str);
    }

    HUDSetTextColor(); //set it back to normal just in case
    const Colormap *colmap = style->definition_->text_[text_type].colmap_;
    if (colmap)
        HUDSetTextColor(GetFontColor(colmap));

    HUDDrawText(x, y, str);

    HUDSetFont();
    HUDSetScale();
    HUDSetTextColor();

}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
