//----------------------------------------------------------------------------
//  EDGE Colour Code
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

#pragma once

#include "ddf_colormap.h"
#include "r_defs.h"

class AbstractShader;

void InitializePalette(void);

// -ACB- 1999/10/11 Gets an RGB colour from the current palette
void PalettedColourToRGB(int indexcol, uint8_t *returncol, RGBAColor last_damage_colour, float damageAmount);

// -AJA- 1999/07/03: Some palette stuff.
extern uint8_t playpal_data[14][256][3];

enum PaletteTypes
{
    kPaletteNormal = 0,
    kPalettePain   = 1,
    kPaletteBonus  = 2,
    kPaletteSuit   = 3
};

void SetPalette(int type, float amount);
void PaletteTicker(void);

// -AJA- 1999/07/10: Some stuff for colmap.ddf.

void TranslatePalette(uint8_t *new_pal, const uint8_t *old_pal, const Colormap *trans);

void GetColormapRgb(const Colormap *colmap, float *r, float *g, float *b);

RGBAColor GetFontColor(const Colormap *colmap);
RGBAColor ParseFontColor(const char *name, bool strict = false);

AbstractShader *GetColormapShader(const struct RegionProperties *props, int light_add = 0, Sector *sec = nullptr);

// colour indices from palette
extern int playpal_black, playpal_white, playpal_gray;

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
