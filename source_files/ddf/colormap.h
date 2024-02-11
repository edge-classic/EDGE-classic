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

#ifndef __DDF_COLORMAP_H__
#define __DDF_COLORMAP_H__

#include "epi.h"
#include "types.h"

// -AJA- 1999/07/09: colmap.ddf structures.

typedef enum
{
    // Default value
    COLSP_None = 0x0000,

    // don't apply gun-flash type effects (looks silly for fog)
    COLSP_NoFlash = 0x0001,

    // for fonts, apply the FONTWHITEN mapping first
    COLSP_Whiten = 0x0002
} colourspecial_e;

typedef struct colmapcache_s
{
    uint8_t *data;
    int      size;
} colmapcache_t;

class colourmap_c
{
   public:
    colourmap_c();
    ~colourmap_c();

   public:
    void CopyDetail(colourmap_c &src);
    void Default();

    // Member vars...
    std::string name;

    std::string lump_name;
    std::string pack_name;

    int start;
    int length;

    colourspecial_e special;

    // colours for GL renderer
    RGBAColor gl_colour;

    RGBAColor font_colour;  // (computed only, not in DDF)

    colmapcache_t cache;

    void *analysis;

   private:
    // disable copy construct and assignment operator
    explicit colourmap_c(colourmap_c &rhs) { (void)rhs; }
    colourmap_c &operator=(colourmap_c &rhs)
    {
        (void)rhs;
        return *this;
    }
};

// Colourmap container
class colourmap_container_c : public std::vector<colourmap_c *>
{
   public:
    colourmap_container_c();
    ~colourmap_container_c();

   public:
    // Search Functions
    colourmap_c *Lookup(const char *refname);
};

extern colourmap_container_c colourmaps;  // -ACB- 2004/06/10 Implemented

void DDF_ReadColourMaps(const std::string &data);

void DDF_AddRawColourmap(const char *name, int size, const char *pack_name);

#endif /* __DDF_COLORMAP_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
