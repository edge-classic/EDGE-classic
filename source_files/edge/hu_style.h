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

#pragma once

#include "ddf_style.h"
#include "hu_font.h"

class Style
{
    friend class StyleContainer;

  public:
    Style(StyleDefinition *definition);
    ~Style();

    StyleDefinition *definition_;

    Font *fonts_[StyleDefinition::kTotalTextSections];

    const Image *background_image_;

  public:
    void Load();

    // Drawing functions
    void DrawBackground();
};

class StyleContainer : public std::vector<Style *>
{
  public:
    StyleContainer()
    {
    }
    ~StyleContainer()
    {
        for (std::vector<Style *>::iterator iter = begin(), iter_end = end(); iter != iter_end; iter++)
        {
            Style *s = *iter;
            delete s;
            s = nullptr;
        }
    }

  public:
    // Search Functions
    Style *Lookup(StyleDefinition *definition);
};

extern StyleContainer hud_styles;

void HUDWriteText(Style *style, int text_type, int x, int y, const char *str, float scale = 1.0f);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
