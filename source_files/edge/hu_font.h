//----------------------------------------------------------------------------
//  EDGE Heads-up-display Font code
//----------------------------------------------------------------------------
// 
//  Copyright (c) 2004-2009  The EDGE Team.
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

#ifndef __HU_FONT__
#define __HU_FONT__

#include "r_image.h"

class fontdef_c;


typedef struct
{
	// range of characters
	int first, last;

	const image_c **images;
	const image_c *missing;

	// nominal width and height.  Characters can be larger or smaller
	// than this, but these values give a good guess for formatting
	// purposes.  Only valid once font has been loaded.
	int width, height;
}
patchcache_t;


#define MAX_IMAGE_SUBDIV  4

typedef struct
{
	int total_w, total_h;  // original image's dimensions

	int sub_w, sub_h;  // number of pieces

	const image_c *images[MAX_IMAGE_SUBDIV][MAX_IMAGE_SUBDIV];
}
image_subdiv_t;



class font_c
{
friend class font_container_c;

public:
	font_c(fontdef_c *_def);
	~font_c();

private:
	fontdef_c *def;

	patchcache_t p_cache;

	image_subdiv_t im_div;

public:
	void Load();

	bool HasChar(char ch) const;

	int NominalWidth() const;
	int NominalHeight() const;

	int CharWidth(char ch) const;
	int StringWidth(const char *str) const;
	int StringLines(const char *str) const;
	int MaxFit(int pixel_w, const char *str) const;

	void DrawChar320(float x, float y, char ch, float scale, float aspect,
				     const colourmap_c *colmap, float alpha) const;

	// FIXME: maybe shouldn't be public (assumes FNTYP_Patch !!)
	const image_c *CharImage(char ch) const;

private:
	void BumpPatchName(char *name);
	void LoadPatches();
	void LoadImageDiv();
};

class font_container_c : public epi::array_c 
{
public:
	font_container_c() : epi::array_c(sizeof(font_c*)) {}
	~font_container_c() { Clear(); } 

private:
	void CleanupObject(void *obj);

public:
	int GetSize() {	return array_entries; } 
	int Insert(font_c *a) { return InsertObject((void*)&a); }
	font_c* operator[](int idx) { return *(font_c**)FetchObject(idx); } 

	// Search Functions
	font_c* Lookup(fontdef_c *def);
};

extern font_container_c hu_fonts;

#endif  // __HU_FONT__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
