//----------------------------------------------------------------------------
//  EDGE Rendering Data Handling Code
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2008  The EDGE Team.
// 
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
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

#ifndef __W_TEXTURE__
#define __W_TEXTURE__

#include "r_defs.h"
#include "r_state.h"

// Texture cache reference structure, returned by the texture cache system.
// The actual structure is private.
typedef byte cached_tex_t;

// Retrieve column data.
const byte *W_GetColumn(int col, const cached_tex_t *t);

// Standard cache functions. Call W_CacheTextureNum when you first need
// the lump, and release it with DoneWithTexture.
const cached_tex_t *W_CacheTextureNum(int texnum);
void W_DoneWithTexture(const cached_tex_t *t);

void W_PreCacheTextureNum(int texnum);


// Called by P_Ticker for switches and animations,
// returns the texture number for the texture name.
int W_TextureNumForName(const char *name);
int W_CheckTextureNumForName(const char *name);

void W_InitTextures(void);
int W_FindTextureSequence(const char *start, const char *end,
    int *s_offset, int *e_offset);
const char *W_TextureNameInSet(int set, int offset);
  

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
typedef struct
{
	int originx;
	int originy;
	int patch;  // lump number
}
texpatch_t;

//
// A texturedef_t describes a rectangular texture, which is composed of
// one or more mappatch_t structures that arrange graphic patches.
//
typedef struct texturedef_s
{
	// Keep name for switch changing, etc.  Zero terminated.
	char name[10];

	short width;
	short height;

	// scaling, where 8 is normal and higher is _smaller_
	byte scale_x;
	byte scale_y;

	// which WAD file this texture came from
	short file;

	int palette_lump;

	unsigned short *columnofs;

	// All the patches[patchcount] are drawn back to front into the
	// cached texture.
	short patchcount;
	texpatch_t patches[1];
}
texturedef_t;

#endif // __W_TEXTURE__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
