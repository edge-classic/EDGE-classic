//----------------------------------------------------------------------------
//  EDGE Heads-up-display Style code
//----------------------------------------------------------------------------
//
//  Copyright (c) 2004-2023  The EDGE Team.
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
    style_c(styledef_c *_def);
    ~style_c();

    styledef_c *def;

    font_c *fonts[styledef_c::NUM_TXST];

    const image_c *bg_image;

  public:
    void Load();

    // Drawing functions
    void DrawBackground();
};

class style_container_c : public epi::array_c
{
  public:
    style_container_c() : epi::array_c(sizeof(style_c *))
    {
    }
    ~style_container_c()
    {
        Clear();
    }

  private:
    void CleanupObject(void *obj);

  public:
    int GetSize()
    {
        return array_entries;
    }
    int Insert(style_c *a)
    {
        return InsertObject((void *)&a);
    }
    style_c *operator[](int idx)
    {
        return *(style_c **)FetchObject(idx);
    }

    // Search Functions
    style_c *Lookup(styledef_c *def);
};

extern style_container_c hu_styles;

// compatibility crud */
void HL_WriteText(style_c *style, int text_type, int x, int y, const char *str, float scale = 1.0f);

#endif // __HU_STYLE__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
