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

#ifndef __HU_STYLE__
#define __HU_STYLE__

#include "style.h"
#include "hu_font.h"
#include "r_image.h"

class style_c
{
    friend class style_container_c;

  public:
    style_c(StyleDefinition *_def);
    ~style_c();

    StyleDefinition *def;

    font_c *fonts[StyleDefinition::kTotalTextSections];

    const image_c *bg_image;

  public:
    void Load();

    // Drawing functions
    void DrawBackground();
};

class style_container_c : public std::vector<style_c *>
{
  public:
    style_container_c()
    {
    }
    ~style_container_c()
    {
      for (auto iter = begin(); iter != end(); iter++)
      {
          style_c *s = *iter;
          delete s;
          s = nullptr;
      }
    }

  public:
    // Search Functions
    style_c *Lookup(StyleDefinition *def);
};

extern style_container_c hu_styles;

// compatibility crud */
void HL_WriteText(style_c *style, int text_type, int x, int y, const char *str, float scale = 1.0f);

#endif // __HU_STYLE__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
