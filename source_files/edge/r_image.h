//----------------------------------------------------------------------------
//  EDGE Generalised Image Handling
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2009  The EDGE Team.
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

#ifndef __R_IMAGE__
#define __R_IMAGE__

#include <vector>

#include "main.h"
#include "image.h"

#include "r_defs.h"
#include "r_state.h"

struct texturedef_s;


// the transparent pixel value we use
#define TRANS_PIXEL  247

// Post end marker
#define P_SENTINEL  0xFF

// dynamic light sizing factor
#define DL_OUTER       64.0f
#define DL_OUTER_SQRT   8.0f


typedef enum
{
	OPAC_Unknown = 0,

	OPAC_Solid   = 1,  // utterly solid (alpha = 255 everywhere)
	OPAC_Masked  = 2,  // only uses alpha 255 and 0
	OPAC_Complex = 3,  // uses full range of alpha values
}
image_opacity_e;

typedef enum
{
	LIQ_None = 0,
	LIQ_Thin = 1,
	LIQ_Thick = 2
}
liquid_type_e;

class image_c
{
public:
	// actual image size.  Images that are smaller than their total size
	// are located in the bottom left corner, cannot tile, and are padded
	// with black pixels if solid, or transparent pixels otherwise.
	unsigned short actual_w;
	unsigned short actual_h;

	// total image size, must be a power of two on each axis.
	unsigned short total_w;
	unsigned short total_h;

	// ratio of actual w/h to total w/h of the image for calculating texcoords
	float ratio_w;
	float ratio_h;

    // offset values.  Only used for sprites and on-screen patches.
	short offset_x;
	short offset_y;

    // scale values, where 1.0f is normal.  Higher values stretch the
    // image (on the wall/floor), lower values shrink it.
	float scale_x;
	float scale_y;

    // one of the OPAC_XXX values
	int opacity;

	liquid_type_e liquid_type;

	int swirled_gametic;

	bool is_font;

//!!!!!! private:

	// --- information about where this image came from ---
	char name[16];

	int source_type;  // image_source_e
 
	union
	{
		// case IMSRC_Graphic:
		// case IMSRC_Sprite:
		// case IMSRC_TX_HI:
		struct { int lump; bool is_png; bool is_tga; } graphic;

		// case IMSRC_Flat:
		// case IMSRC_Raw320x200:
		struct { int lump; } flat;

		// case IMSRC_Texture:
		struct { struct texturedef_s *tdef; } texture;

		// case IMSRC_Dummy:
		struct { rgbcol_t fg; rgbcol_t bg; } dummy;

		// case IMSRC_User:
		struct { imagedef_c *def; } user;
	}
	source;

	// palette lump, or -1 to use the "GLOBAL" palette
	int source_palette;

	// --- information about caching ---

	std::vector< struct cached_image_s * > cache;

	// --- animation info ---

	struct
	{
		// current version of this image in the animation.  Initially points
		// to self.  For non-animated images, doesn't change.  Otherwise
		// when the animation flips over, it becomes cur->next.
		image_c *cur;

		// next image in the animation, or NULL.
		image_c *next;

		// tics before next anim change, or 0 if non-animated.
		unsigned short count;

		// animation speed (in tics), or 0 if non-animated.
		unsigned short speed;
	}
	anim;

public:
	 image_c();
	~image_c();

	/* TODO: add methods here... */
};


// macro for converting image_c sizes to cached_image_t sizes
#define MIP_SIZE(size,mip)  MAX(1, (size) >> (mip))

// utility macros (FIXME: replace with class methods)
#define IM_RIGHT(image)  (float((image)->actual_w) / (image)->total_w)
#define IM_TOP(image)    (float((image)->actual_h) / (image)->total_h)

#define IM_WIDTH(image)  ((image)->actual_w * (image)->scale_x)
#define IM_HEIGHT(image) ((image)->actual_h * (image)->scale_y)

#define IM_OFFSETX(image) ((image)->offset_x * (image)->scale_x)
#define IM_OFFSETY(image) ((image)->offset_y * (image)->scale_y)

// deliberately long, should not be used (except for special cases)
#define IM_TOTAL_WIDTH(image)  ((image)->total_w * (image)->scale_x)
#define IM_TOTAL_HEIGHT(image) ((image)->total_h * (image)->scale_y)


//
//  IMAGE LOOKUP
//
typedef enum
{
	ILF_Null    = 0x0001,  // return NULL rather than a dummy image
	ILF_Exact   = 0x0002,  // type must be exactly the same
	ILF_NoNew   = 0x0004,  // image must already exist (don't create it)
	ILF_Font    = 0x0008,  // font character (be careful with backups)
}
image_lookup_flags_e;

const image_c *W_ImageLookup(const char *name, image_namespace_e = INS_Graphic,
	int flags = 0);

const image_c *W_ImageForDummySprite(void);
const image_c *W_ImageForDummySkin(void);
const image_c *W_ImageForHOMDetect(void);

// savegame code (Only)
const image_c *W_ImageParseSaveString(char type, const char *name);
void W_ImageMakeSaveString(const image_c *image, char *type, char *namebuf);


//
//  IMAGE USAGE
//

extern int  var_mipmapping;
extern int  var_smoothing;
extern bool var_dithering;
extern int  hq2x_scaling;

typedef enum
{
	SWIRL_Vanilla = 0,
	SWIRL_SMMU = 1,
	SWIRL_SMMUSWIRL = 2,
	SWIRL_PARALLAX = 3
}
swirl_type_e;
extern swirl_type_e swirling_flats;

bool W_InitImages(void);
void W_UpdateImageAnims(void);
void W_DeleteAllImages(void);

void W_ImageCreateFlats(int *lumps, int number);
void W_ImageCreateTextures(struct texturedef_s ** defs, int number);
const image_c *W_ImageCreateSprite(const char *name, int lump, bool is_weapon);
void W_ImageCreateUser(void);
void W_ImageAddTX(int lump, const char *name, bool hires);
void W_AnimateImageSet(const image_c ** images, int number, int speed);
void W_DrawSavePic(const byte *pixels);

#ifdef USING_GL_TYPES
GLuint W_ImageCache(const image_c *image, bool anim = true,
					const colourmap_c *trans = NULL, bool do_whiten = false);
#endif
void W_ImagePreCache(const image_c *image);


// -AJA- planned....
// rgbcol_t W_ImageGetHue(const image_c *c);

const char *W_ImageGetName(const image_c *image);

// this only needed during initialisation -- r_things.cpp
const image_c ** W_ImageGetUserSprites(int *count);

// internal routines -- only needed by rgl_wipe.c
int W_MakeValidSize(int value);

typedef enum
{
	// Source was a graphic name
	IMSRC_Graphic = 0,

	// INTERNAL ONLY: Source was a raw block of 320x200 bytes (Heretic/Hexen)
	IMSRC_Raw320x200,

	// Source was a sprite name
	IMSRC_Sprite,

	// Source was a flat name
	IMSRC_Flat,

	// Source was a texture name
	IMSRC_Texture,

	// INTERNAL ONLY: Source is from IMAGE.DDF
	IMSRC_User,

	// INTERNAL ONLY: Source is from TX_START/END or HI_START/END
	IMSRC_TX_HI,

	// INTERNAL ONLY: Source is dummy image
	IMSRC_Dummy,
}
image_source_e;


#endif  // __R_IMAGE__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
