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

#include "i_defs.h"

#include "main.h"
#include "font.h"

#include "dm_defs.h"
#include "dm_state.h"
#include "hu_font.h"
#include "r_local.h"
#include "r_colormap.h"
#include "r_draw.h"
#include "r_modes.h"
#include "r_image.h"

#define DUMMY_WIDTH  4


// all the fonts that's fit to print
font_container_c hu_fonts;


font_c::font_c(fontdef_c *_def) : def(_def)
{
	p_cache.first = 0;
	p_cache.last  = -1;

	p_cache.images = NULL;
	p_cache.missing = NULL;

	im_div.sub_w = 0;
	im_div.sub_h = 0;
}

font_c::~font_c()
{
	if (p_cache.images)
		delete[] p_cache.images;

	// FIXME: im_div.images
}

void font_c::BumpPatchName(char *name)
{
	// loops to increment the 10s (100s, etc) digit
	for (char *s = name + strlen(name) - 1; s >= name; s--)
	{
		// only handle digits and letters
		if (! isalnum(*s))
			break;

		if (*s == '9') { *s = '0'; continue; }
		if (*s == 'Z') { *s = 'A'; continue; }
		if (*s == 'z') { *s = 'a'; continue; }

		(*s) += 1; break;
	}
}

void font_c::LoadPatches()
{
	p_cache.first = 9999;
	p_cache.last  = 0;

	const fontpatch_c *pat;

	// determine full range
	for (pat = def->patches; pat; pat = pat->next)
	{
		if (pat->char1 < p_cache.first)
			p_cache.first = pat->char1;

		if (pat->char2 > p_cache.last)
			p_cache.last = pat->char2;
	}

	int total = p_cache.last - p_cache.first + 1;

	SYS_ASSERT(def->patches);
	SYS_ASSERT(total >= 1);

	p_cache.images = new const image_c *[total];
	memset(p_cache.images, 0, sizeof(const image_c *) * total);

	for (pat = def->patches; pat; pat = pat->next)
	{
		// patch name
		char pname[40];

		SYS_ASSERT(strlen(pat->patch1.c_str()) < 36);
		strcpy(pname, pat->patch1.c_str());

		for (int ch = pat->char1; ch <= pat->char2; ch++, BumpPatchName(pname))
		{
#if 0  // DEBUG
			L_WriteDebug("- LoadFont [%s] : char %d = %s\n", def->name.c_str(), ch, pname);
#endif
			int idx = ch - p_cache.first;
			SYS_ASSERT(0 <= idx && idx < total);

			p_cache.images[idx] = W_ImageLookup(pname, INS_Graphic, ILF_Font|ILF_Null);
		}
	}

	p_cache.missing = def->missing_patch ?
		W_ImageLookup(def->missing_patch, INS_Graphic, ILF_Font|ILF_Null) : NULL;

	const image_c *Nom = NULL;

	if (HasChar('M'))
		Nom = CharImage('M');
	else if (HasChar('m'))
		Nom = CharImage('m');
	else if (HasChar('0'))
		Nom = CharImage('0');
	else
	{
		// backup plan: just use first patch found
		for (int idx = 0; idx < total; idx++)
			if (p_cache.images[idx])
			{
				Nom = p_cache.images[idx];
				break;
			}
	}

	if (! Nom)
	{
		I_Warning("Font [%s] has no loaded patches !\n", def->name.c_str());
		p_cache.width = p_cache.height = 7;
		return;
	}

	p_cache.width  = I_ROUND(IM_WIDTH(Nom));  // XXX: make fields float???
	p_cache.height = I_ROUND(IM_HEIGHT(Nom));
}

void font_c::LoadImageDiv()
{
	// TODO
	I_Error("LoadImageDiv called??\n");
}


void font_c::Load()
{
	switch (def->type)
	{
		case FNTYP_Patch:
			LoadPatches();
			break;

		case FNTYP_Image:
			LoadImageDiv();
			break;

		default:
			I_Error("Coding error, unknown font type %d\n", def->type);
			break; /* NOT REACHED */
	}
}


int font_c::NominalWidth() const
{
	if (def->type == FNTYP_Image)
		return im_div.total_w / 16;

	if (def->type == FNTYP_Patch)
		return p_cache.width;

	I_Error("font_c::NominalHeight : unknown FONT type %d\n", def->type);
	return 1; /* NOT REACHED */
}

int font_c::NominalHeight() const
{
	if (def->type == FNTYP_Image)
		return im_div.total_w / 16;

	if (def->type == FNTYP_Patch)
		return p_cache.height;

	I_Error("font_c::NominalHeight : unknown FONT type %d\n", def->type);
	return 1; /* NOT REACHED */
}


bool font_c::HasChar(char ch) const
{
	SYS_ASSERT(def->type == FNTYP_Patch);

	int idx = int(ch) & 0x00FF;

	if (! (p_cache.first <= idx && idx <= p_cache.last))
		return false;
	
	return (p_cache.images[idx - p_cache.first] != NULL);
}


const image_c *font_c::CharImage(char ch) const
{
	SYS_ASSERT(def->type == FNTYP_Patch);

	if (! HasChar(ch))
	{
		if ('a' <= ch && ch <= 'z' && HasChar(toupper(ch)))
			ch = toupper(ch);
		else if (ch == ' ')
			return NULL;
		else
			return p_cache.missing;
	}

	int idx = int(ch) & 0x00FF;

	SYS_ASSERT(p_cache.first <= idx && idx <= p_cache.last);
	
	return p_cache.images[idx - p_cache.first];
}


//
// Returns the width of the IBM cp437 char in the font.
//
int font_c::CharWidth(char ch) const  // XXX: return float ???
{
	if (def->type == FNTYP_Image)
		return im_div.total_w / 16;
			
	SYS_ASSERT(def->type == FNTYP_Patch);

	if (ch == ' ')
		return p_cache.width * 3 / 5;

	const image_c *im = CharImage(ch);

	if (! im)
		return DUMMY_WIDTH;

	return I_ROUND(IM_WIDTH(im));
}


//
// Returns the maximum number of characters which can fit within pixel_w
// pixels.  The string may not contain any newline characters.
//
int font_c::MaxFit(int pixel_w, const char *str) const
{
	int w = 0;
	const char *s;

	// just add one char at a time until it gets too wide or the string ends.
	for (s = str; *s; s++)
	{
		w += CharWidth(*s);

		if (w > pixel_w)
		{
			// if no character could fit, an infinite loop would probably start,
			// so it's better to just imagine that one character fits.
			if (s == str)
				s++;

			break;
		}
	}

	// extra spaces at the end of the line can always be added
	while (*s == ' ')
		s++;

	return s - str;
}


//
// Find string width from hu_font chars.  The string may not contain
// any newline characters.
//
int font_c::StringWidth(const char *str) const
{
	int w = 0;

	while (*str)
		w += CharWidth(*str++);

	return w;
}


//
// Find number of lines in string.
//
int font_c::StringLines(const char *str) const
{
	int lines = 1;

	for (; *str; str++)
		if (*str == '\n')
			lines++;

	return lines;
}


void font_c::DrawChar320(float x, float y, char ch, float scale, float aspect,
    const colourmap_c *colmap, float alpha) const
{
	SYS_ASSERT(def->type != FNTYP_Image);

	const image_c *image = CharImage(ch);

	if (! image)
		return;
	
	float sc_x = scale * aspect;
	float sc_y = scale;

	y = 200-y;

	RGL_DrawImage(
	    FROM_320(x - IM_OFFSETX(image) * sc_x),
		FROM_200(y + (IM_OFFSETY(image) - IM_HEIGHT(image)) * sc_y),
		FROM_320(IM_WIDTH(image))  * sc_x,
		FROM_200(IM_HEIGHT(image)) * sc_y,
		image, 0.0f, 0.0f,
		IM_RIGHT(image), IM_TOP(image),
		colmap, alpha);
}


//----------------------------------------------------------------------------
//  font_container_c class
//----------------------------------------------------------------------------


void font_container_c::CleanupObject(void *obj)
{
	font_c *a = *(font_c**)obj;

	if (a) delete a;
}


// Never returns NULL.
//
font_c* font_container_c::Lookup(fontdef_c *def)
{
	SYS_ASSERT(def);

	for (epi::array_iterator_c it = GetIterator(0); it.IsValid(); it++)
	{
		font_c *f = ITERATOR_TO_TYPE(it, font_c*);

		if (def == f->def)
			return f;
	}

	font_c *new_f = new font_c(def);

	new_f->Load();
	Insert(new_f);

	return new_f;
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
