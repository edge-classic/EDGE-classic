//----------------------------------------------------------------------------
//  EDGE Rendering Data Handling Code
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
//
// DESCRIPTION:
//  Refresh module, data I/O, caching, retrieval of graphics
//  by name.
//

#pragma once

#include <stdint.h>

void        InitializeTextures(void);
int         FindTextureSequence(const char *start, const char *end, int *s_offset, int *e_offset);
const char *TextureNameInSet(int set, int offset);

//
// Graphics:
// ^^^^^^^^^
// DOOM graphics for walls and sprites is stored in vertical runs of
// opaque pixels (posts).
//
// A column is composed of zero or more posts, a patch or sprite is
// composed of zero or more columns.
//

//
// A single patch from a texture definition, basically a rectangular area
// within the texture rectangle.
//
// Note: Block origin (always UL), which has already accounted
// for the internal origin of the patch.
//
struct TexturePatch
{
    int origin_x;
    int origin_y;
    int patch; // lump number
};

//
// A TextureDefinition describes a rectangular texture, which is composed of
// one or more mapPatch structures that arrange graphic patches.
//
struct TextureDefinition
{
    // Keep name for switch changing, etc.  Zero terminated.
    char name[10];

    short width;
    short height;

    // scaling, where 8 is normal and higher is _smaller_
    uint8_t scale_x;
    uint8_t scale_y;

    // which WAD file this texture came from
    short file;

    int palette_lump;

    unsigned short *column_offset;

    // All the patches[patchcount] are drawn back to front into the
    // cached texture.
    short        patch_count;
    TexturePatch patches[1];
};

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
