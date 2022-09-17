//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Images)
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

#ifndef __DDF_IMAGE_H__
#define __DDF_IMAGE_H__

#include "utility.h"

#include "types.h"

typedef enum
{
	INS_Graphic = 0,
	INS_Texture,
	INS_Flat,
	INS_Sprite,
}
image_namespace_e;

//
// -AJA- 2004/11/16 Images.ddf
//
typedef enum
{
	IMGDT_Colour = 0,   // solid colour
	IMGDT_Builtin,      // built-in pre-fab DYI kit
	IMGDT_File,         // load from an image file
	IMGDT_Lump,         // load from lump in a WAD
	IMGDT_Package,      // load from a PK3 package
}
imagedata_type_e;

typedef enum
{
	BLTIM_Quadratic = 0,
	BLTIM_Linear,
	BLTIM_Shadow
}
builtin_image_e;

typedef enum
{
	IMGSP_None = 0,

	IMGSP_NoAlpha   = 0x0001,   // image does not require an alpha channel
	IMGSP_Mip       = 0x0002,   // force   mip-mapping
	IMGSP_NoMip     = 0x0004,   // disable mip-mapping
	IMGSP_Clamp     = 0x0008,   // clamp image
	IMGSP_Smooth    = 0x0010,   // force smoothing
	IMGSP_NoSmooth  = 0x0020,   // disable smoothing
	IMGSP_Crosshair = 0x0040,   // weapon crosshair (center vertically)
}
image_special_e;

typedef enum
{
	FIXTRN_None = 0,     // no modification (the default)
	FIXTRN_Blacken = 1,  // make 100% transparent pixels Black
}
image_fix_trans_e;

typedef enum
{
	LIF_PNG = 0,
	LIF_JPEG,
	LIF_TGA,
	LIF_DOOM
}
L_image_format_e;

class imagedef_c
{
public:
	imagedef_c();
	~imagedef_c() {};

public:
	void Default(void);
	void CopyDetail(const imagedef_c &src);

	// Member vars....
	std::string name;
	image_namespace_e belong;

	imagedata_type_e type;

	rgbcol_t colour;          // IMGDT_Colour
	builtin_image_e builtin;  // IMGDT_Builtin

	std::string info;       // IMGDT_Package, IMGDT_File, IMGDT_Lump
	L_image_format_e format;  // ditto

	image_special_e special;

	int x_offset, y_offset;

	int fix_trans;   // FIXTRN_XXX value

	bool is_font;

	// COMPOSE specifics:
	//   rgbcol_t base_col;
	//   percent_t base_trans;
	//   compose_overlay_t *overlays;

	// WAD specifics:
	//   std::string palette;
	//   colourmap_c *colmap;

	// CONVERSION
	//   float gamma;
	//   h_formula_t   conv_h;
	//   rgb_formula_t conv_s, conv_v;
	//   rgb_formula_t conv_r, conv_g, conv_b, conv_a;

	// RENDERING specifics:
	float scale, aspect;
	//   percent_t translucency;
	//   angle_t rotation;

private:
	// disable copy construct and assignment operator
	explicit imagedef_c(imagedef_c &rhs) { }
	imagedef_c& operator=(imagedef_c &rhs) { return *this; }
};


// Our imagedefs container
class imagedef_container_c : public epi::array_c 
{
public:
	imagedef_container_c() : epi::array_c(sizeof(imagedef_c*)) {}
	~imagedef_container_c() { Clear(); } 

private:
	void CleanupObject(void *obj);

public:
	int GetSize() {	return array_entries; } 
	int Insert(imagedef_c *a) { return InsertObject((void*)&a); }
	imagedef_c *operator[](int idx) { return *(imagedef_c**)FetchObject(idx); } 

	// Search Functions
	imagedef_c *Lookup(const char *refname, image_namespace_e belong);
};

extern imagedef_container_c imagedefs;

bool DDF_ReadImages(void *data, int size);

#endif  /*__DDF_IMAGE_H__*/

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
