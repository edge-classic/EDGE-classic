//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Colourmaps)
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

#pragma once

#include "ddf_types.h"
#include "epi.h"

enum ColorSpecial
{
    // Default value
    kColorSpecialNone = 0x0000,
    // don't apply gun-flash type effects (looks silly for fog)
    kColorSpecialNoFlash = 0x0001,
    // for fonts, apply the FONTWHITEN mapping first
    kColorSpecialWhiten = 0x0002
};

struct ColormapCache
{
    uint8_t *data;
    int      size;
};

class Colormap
{
  public:
    Colormap();
    ~Colormap();

  public:
    void CopyDetail(const Colormap &src);
    void Default();

    // Member vars...
    std::string name_;

    std::string lump_name_;
    int         lump_index_; // for raw BOOM colourmaps
    std::string pack_name_;

    int start_;
    int length_;

    ColorSpecial special_;

    // colours for GL renderer
    RGBAColor gl_color_;

    RGBAColor font_colour_; // (computed only, not in DDF)

    ColormapCache cache_;

    void *analysis_;

  private:
    // disable copy construct and assignment operator
    explicit Colormap(Colormap &rhs)
    {
        EPI_UNUSED(rhs);
    }
    Colormap &operator=(Colormap &rhs)
    {
        EPI_UNUSED(rhs);
        return *this;
    }
};

class ColormapContainer : public std::vector<Colormap *>
{
  public:
    ColormapContainer();
    ~ColormapContainer();

  public:
    Colormap *Lookup(const char *refname);
};

extern ColormapContainer colormaps; // -ACB- 2004/06/10 Implemented

void DDFReadColourMaps(const std::string &data);

void DDFAddRawColourmap(const char *name, int size, const char *pack_name, int index = -1);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
