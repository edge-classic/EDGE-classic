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
//
// Image Setup and Parser Code
//

#include "local.h"

#include "path.h"

#include "image.h"

static imagedef_c *dynamic_image;

static void DDF_ImageGetType(const char *info, void *storage);
static void DDF_ImageGetSpecial(const char *info, void *storage);
static void DDF_ImageGetFixTrans(const char *info, void *storage);

// -ACB- 1998/08/10 Use DDF_MainGetLumpName for getting the..lump name.
// -KM- 1998/09/27 Use DDF_MainGetTime for getting tics

#define DDF_CMD_BASE  dummy_image
static imagedef_c dummy_image;

static const commandlist_t image_commands[] =
{
	DDF_FIELD("IMAGE_DATA", type,     DDF_ImageGetType),
	DDF_FIELD("SPECIAL",    special,  DDF_ImageGetSpecial),
	DDF_FIELD("X_OFFSET",   x_offset, DDF_MainGetNumeric),
	DDF_FIELD("Y_OFFSET",   y_offset, DDF_MainGetNumeric),
	DDF_FIELD("SCALE",      scale,    DDF_MainGetFloat),
	DDF_FIELD("ASPECT",     aspect,   DDF_MainGetFloat),
	DDF_FIELD("FIX_TRANS",  fix_trans, DDF_ImageGetFixTrans),
	DDF_FIELD("IS_FONT",  is_font, DDF_MainGetBoolean),

	DDF_CMD_END
};

imagedef_container_c imagedefs;

static image_namespace_e GetImageNamespace(const char *prefix)
{
	if (DDF_CompareName(prefix, "gfx") == 0)
		return INS_Graphic;

	if (DDF_CompareName(prefix, "tex") == 0)
		return INS_Texture;

	if (DDF_CompareName(prefix, "flat") == 0)
		return INS_Flat;

	if (DDF_CompareName(prefix, "spr") == 0)
		return INS_Sprite;
	
	DDF_Error("Invalid image prefix '%s' (use: gfx,tex,flat,spr)\n", prefix);
	return INS_Flat; /* NOT REACHED */ 
}

//
//  DDF PARSE ROUTINES
//

static void ImageStartEntry(const char *name, bool extend)
{
	if (!name || !name[0])
		DDF_Error("New image entry is missing a name!\n");

//	I_Debugf("ImageStartEntry [%s]\n", name);

	image_namespace_e belong = INS_Graphic;

	const char *pos = strchr(name, ':');

	if (! pos)
		DDF_Error("Missing image prefix.\n");

	if (pos)
	{
		std::string nspace(name, pos - name);

		if (nspace.empty())
			DDF_Error("Missing image prefix.\n");

		belong = GetImageNamespace(nspace.c_str());

		name = pos + 1;

		if (! name[0])
			DDF_Error("Missing image name.\n");
	}

	// W_Image code has limited space for the name
	if (strlen(name) > 15)
		DDF_Error("Image name [%s] too long.\n", name);

	dynamic_image = imagedefs.Lookup(name, belong);

	if (extend)
	{
		if (! dynamic_image)
			DDF_Error("Unknown image to extend: %s\n", name);
		return;
	}

	// replaces an existing entry?
	if (dynamic_image)
	{
		dynamic_image->Default();
		return;
	}

	// not found, create a new one
	dynamic_image = new imagedef_c;

	dynamic_image->name   = name;
	dynamic_image->belong = belong;

	imagedefs.Insert(dynamic_image);
}


static void ImageParseField(const char *field, const char *contents, int index, bool is_last)
{
#if (DEBUG_DDF)  
	I_Debugf("IMAGE_PARSE: %s = %s;\n", field, contents);
#endif

	if (DDF_MainParseField(image_commands, field, contents, (byte *)dynamic_image))
		return;  // OK

	DDF_Error("Unknown images.ddf command: %s\n", field);
}


static void ImageFinishEntry(void)
{
	if (dynamic_image->type == IMGDT_File)
	{
        const char *filename = dynamic_image->info.c_str();

		// determine format
        std::string ext(epi::PATH_GetExtension(filename));

		if (DDF_CompareName(ext.c_str(), ".png") == 0)
			dynamic_image->format = LIF_PNG;
		else if (DDF_CompareName(ext.c_str(), ".jpg")  == 0 ||
				 DDF_CompareName(ext.c_str(), ".jpeg") == 0)
			dynamic_image->format = LIF_JPEG;
		else if (DDF_CompareName(ext.c_str(), ".tga") == 0)
			dynamic_image->format = LIF_TGA;
		else
			DDF_Error("Unknown image extension for '%s'\n", filename);
	}

	// Add these automatically so modders don't have to remember them
	if (dynamic_image->is_font)
	{
		dynamic_image->special = (image_special_e)(dynamic_image->special | IMGSP_Clamp);
		dynamic_image->special = (image_special_e)(dynamic_image->special | IMGSP_NoMip);
	}

	// TODO: check more stuff...
}


static void ImageClearAll(void)
{
	I_Warning("Ignoring #CLEARALL in images.ddf\n");
}


bool DDF_ReadImages(void *data, int size)
{
	readinfo_t images;

	images.memfile = (char*)data;
	images.memsize = size;
	images.tag = "IMAGES";
	images.entries_per_dot = 2;

	if (images.memfile)
	{
		images.message = NULL;
		images.filename = NULL;
		images.lumpname = "DDFIMAGE";
	}
	else
	{
		images.message = "DDF_InitImages";
		images.filename = "images.ddf";
		images.lumpname = NULL;
	}

	images.start_entry  = ImageStartEntry;
	images.parse_field  = ImageParseField;
	images.finish_entry = ImageFinishEntry;
	images.clear_all    = ImageClearAll;

	return DDF_MainReadFile(&images);
}


void DDF_ImageInit(void)
{
	imagedefs.Clear();
}

void DDF_ImageCleanUp(void)
{
	imagedefs.Trim();		// <-- Reduce to allocated size
}


static void ImageParseColour(const char *value)
{
	DDF_MainGetRGB(value, &dynamic_image->colour);
}


static void ImageParseBuiltin(const char *value)
{
	if (DDF_CompareName(value, "LINEAR") == 0)
		dynamic_image->builtin = BLTIM_Linear;
	else if (DDF_CompareName(value, "QUADRATIC") == 0)
		dynamic_image->builtin = BLTIM_Quadratic;
	else if (DDF_CompareName(value, "SHADOW") == 0)
		dynamic_image->builtin = BLTIM_Shadow;
	else
		DDF_Error("Unknown image BUILTIN kind: %s\n", value);
}


static void ImageParseInfo(const char *value)
{
	// ouch, hard work here...
	dynamic_image->info = value;
}


static void ImageParseLump(const char *spec)
{
	const char *colon = DDF_MainDecodeList(spec, ':', true);

	if (! colon || colon == spec || (colon - spec) >= 16 || colon[1] == 0)
		DDF_Error("Malformed image lump spec: 'LUMP:%s'\n", spec);

	char keyword[20];

	strncpy(keyword, spec, colon - spec);
	keyword[colon - spec] = 0;

	// store the lump name
	dynamic_image->info.Set(colon + 1);

	if (DDF_CompareName(keyword, "PNG") == 0)
	{
		dynamic_image->format = LIF_PNG;
	}
	else if (DDF_CompareName(keyword, "JPG") == 0 ||
	         DDF_CompareName(keyword, "JPEG") == 0)
	{
		dynamic_image->format = LIF_JPEG;
	}
	else if (DDF_CompareName(keyword, "TGA") == 0)
	{
		dynamic_image->format = LIF_TGA;
	}
	else
		DDF_Error("Unknown image format: %s (use PNG or JPEG)\n", keyword);
}


static void DDF_ImageGetType(const char *info, void *storage)
{
	const char *colon = DDF_MainDecodeList(info, ':', true);

	if (! colon || colon == info || (colon - info) >= 16 || colon[1] == 0)
		DDF_Error("Malformed image type spec: %s\n", info);

	char keyword[20];

	strncpy(keyword, info, colon - info);
	keyword[colon - info] = 0;

	if (DDF_CompareName(keyword, "COLOUR") == 0)
	{
		dynamic_image->type = IMGDT_Colour;
		ImageParseColour(colon + 1);
	}
	else if (DDF_CompareName(keyword, "BUILTIN") == 0)
	{
		dynamic_image->type = IMGDT_Builtin;
		ImageParseBuiltin(colon + 1);
	}
	else if (DDF_CompareName(keyword, "FILE") == 0)
	{
		dynamic_image->type = IMGDT_File;
		ImageParseInfo(colon + 1);
	}
	else if (DDF_CompareName(keyword, "LUMP") == 0)
	{
		dynamic_image->type = IMGDT_Lump;
		ImageParseLump(colon + 1);
	}
	else
		DDF_Error("Unknown image type: %s\n", keyword);
}

static specflags_t image_specials[] =
{
    {"NOALPHA",       IMGSP_NoAlpha,   0},
    {"FORCE_MIP",     IMGSP_Mip,       0},
    {"FORCE_NOMIP",   IMGSP_NoMip,     0},
    {"FORCE_CLAMP",   IMGSP_Clamp,     0},
    {"FORCE_SMOOTH",  IMGSP_Smooth,    0},
    {"FORCE_NOSMOOTH",IMGSP_NoSmooth,  0},
    {"CROSSHAIR",     IMGSP_Crosshair, 0},
    {NULL, 0, 0}
};


static void DDF_ImageGetSpecial(const char *info, void *storage)
{
	image_special_e *dest = (image_special_e *)storage;

	int flag_value;

	switch (DDF_MainCheckSpecialFlag(info, image_specials,
			&flag_value, false /* allow_prefixes */, false))
	{
		case CHKF_Positive:
			*dest = (image_special_e)(*dest | flag_value);
			break;

		case CHKF_Negative:
			*dest = (image_special_e)(*dest & ~flag_value);
			break;

		case CHKF_User:
		case CHKF_Unknown:
			DDF_WarnError("Unknown image special: %s\n", info);
			break;
	}
}


static void DDF_ImageGetFixTrans(const char *info, void *storage)
{
	image_fix_trans_e *var = (image_fix_trans_e *)storage;

	if (DDF_CompareName(info, "NONE") == 0)
	{
		*var = FIXTRN_None;
	}
	else if (DDF_CompareName(info, "BLACKEN") == 0)
	{
		*var = FIXTRN_Blacken;
	}
	else
		DDF_Error("Unknown FIX_TRANS type: %s\n", info);
}


// ---> imagedef_c class

imagedef_c::imagedef_c() : name(), belong(INS_Graphic), info()
{
	Default();
}

//
// Copies all the detail with the exception of ddf info
//
void imagedef_c::CopyDetail(const imagedef_c &src)
{
	type    = src.type;
	colour  = src.colour;
	builtin = src.builtin;
	info    = src.info;
	format  = src.format;

	special  = src.special;
	x_offset = src.x_offset;
	y_offset = src.y_offset;
	scale    = src.scale;
	aspect   = src.aspect;
	fix_trans = src.fix_trans;
	is_font = src.is_font;
}

void imagedef_c::Default()
{
	type    = IMGDT_Colour;
	colour  = 0x000000;  // black
	builtin = BLTIM_Quadratic;
	format  = LIF_PNG;

	info.clear();

	special  = IMGSP_None;
	x_offset = y_offset = 0;

	scale  = 1.0f;
	aspect = 1.0f;
	fix_trans = FIXTRN_None;
	is_font = false;
}


// ---> imagedef_container_c class

void imagedef_container_c::CleanupObject(void *obj)
{
	imagedef_c *a = *(imagedef_c**)obj;

	if (a) delete a;
}


imagedef_c * imagedef_container_c::Lookup(const char *refname, image_namespace_e belong)
{
	if (!refname || !refname[0])
		return NULL;

	epi::array_iterator_c it;

	for (it = GetBaseIterator(); it.IsValid(); it++)
	{
		imagedef_c *g = ITERATOR_TO_TYPE(it, imagedef_c*);

		if (DDF_CompareName(g->name.c_str(), refname) == 0 && g->belong == belong)
			return g;
	}

	return NULL;
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
