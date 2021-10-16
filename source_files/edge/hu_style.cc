//----------------------------------------------------------------------------
//  EDGE Heads-up-display Style code
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
#include "hu_style.h"
#include "hu_draw.h"

#include "dm_defs.h"
#include "dm_state.h"
#include "r_local.h"
#include "r_colormap.h"
#include "r_draw.h"
#include "r_modes.h"
#include "r_image.h"


// Edge has lots of style
style_container_c hu_styles;


style_c::style_c(styledef_c *_def) : def(_def), bg_image(NULL)
{
	for (int T = 0; T < styledef_c::NUM_TXST; T++)
		fonts[T] = NULL;
}

style_c::~style_c()
{
	/* nothing to do */
}


void style_c::Load()
{
	if (def->bg.image_name.c_str())
	{
		const char *name = def->bg.image_name.c_str();

		bg_image = W_ImageLookup(name, INS_Flat, ILF_Null);

		if (! bg_image)
			bg_image = W_ImageLookup(name, INS_Graphic);
	}

	for (int T = 0; T < styledef_c::NUM_TXST; T++)
	{
		if (def->text[T].font)
			fonts[T] = hu_fonts.Lookup(def->text[T].font);
	}
}


void style_c::DrawBackground()
{
	float alpha = PERCENT_2_FLOAT(def->bg.translucency);

	if (alpha < 0.02)
		return;

	HUD_SetAlpha(alpha);

	if (! bg_image)
	{
		if (def->bg.colour != RGB_NO_VALUE)
			HUD_SolidBox(0, 0, 320, 200, def->bg.colour);

		HUD_SetAlpha();
		return;
	}

	if (def->special & (SYLSP_Tiled | SYLSP_TiledNoScale))
	{
		HUD_SetScale(def->bg.scale);

		HUD_TileImage(0, 0, 320, 200, bg_image);

		HUD_SetScale();
	}
	else
	{
		HUD_StretchImage(0, 0, 320, 200, bg_image);
	}

	HUD_SetAlpha();
}

// ---> style_container_c class

//
// style_container_c::CleanupObject()
//
void style_container_c::CleanupObject(void *obj)
{
	style_c *a = *(style_c**)obj;

	if (a) delete a;
}

//
// style_container_c::Lookup()
//
// Never returns NULL.
//
style_c* style_container_c::Lookup(styledef_c *def)
{
	SYS_ASSERT(def);

	for (epi::array_iterator_c it = GetIterator(0); it.IsValid(); it++)
	{
		style_c *st = ITERATOR_TO_TYPE(it, style_c*);

		if (def == st->def)
			return st;
	}

	style_c *new_st = new style_c(def);

	new_st->Load();
	Insert(new_st);

	return new_st;
}


//
// HL_WriteText
//
// Compatibility crud...
//
void HL_WriteText(style_c *style, int text_type, int x, int y, const char *str, float scale)
{
	HUD_SetFont(style->fonts[text_type]);
	HUD_SetScale(scale * style->def->text[text_type].scale);

	const colourmap_c *colmap = style->def->text[text_type].colmap;

	if (colmap)
		HUD_SetTextColor(V_GetFontColor(colmap));

	HUD_DrawText(x, y, str);

	HUD_SetFont();
	HUD_SetScale();
	HUD_SetTextColor();
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
