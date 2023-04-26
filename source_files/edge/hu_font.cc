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
#include "i_defs_gl.h"

#include "main.h"
#include "font.h"
#include "image_data.h"

#include "dm_defs.h"
#include "dm_state.h"
#include "hu_font.h"
#include "r_local.h"
#include "r_colormap.h"
#include "r_draw.h"
#include "r_modes.h"
#include "r_image.h"
#include "w_files.h"
#include "w_wad.h"
#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define DUMMY_WIDTH  8

extern epi::image_data_c *ReadAsEpiBlock(image_c *rim);

// all the fonts that's fit to print
font_container_c hu_fonts;


font_c::font_c(fontdef_c *_def) : def(_def)
{
	p_cache.first = 0;
	p_cache.last  = -1;

	p_cache.images = nullptr;
	p_cache.missing = nullptr;

	font_image = nullptr;
	ttf_buffer = nullptr;
	ttf_info = nullptr;
	ttf_kern_scale = 0;
	ttf_ref_yshift = 0;
	ttf_ref_height = 0;
}

font_c::~font_c()
{
	if (p_cache.images)
		delete[] p_cache.images;
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

	p_cache.missing = def->missing_patch != "" ?
		W_ImageLookup(def->missing_patch.c_str(), INS_Graphic, ILF_Font|ILF_Null) : NULL;

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

	if (def->default_size > 0.0)
	{
		p_cache.height = def->default_size;
		p_cache.width  = def->default_size * (IM_WIDTH(Nom) / IM_HEIGHT(Nom));
		p_cache.ratio = p_cache.width / p_cache.height;
	}
	else
	{
		p_cache.width  = IM_WIDTH(Nom);
		p_cache.height = IM_HEIGHT(Nom);
		p_cache.ratio = IM_WIDTH(Nom) / IM_HEIGHT(Nom);
	}
	spacing = def->spacing;
}

void font_c::LoadFontImage()
{
	if (!font_image)
	{
		if (!def->image_name.empty())
			font_image = W_ImageLookup(def->image_name.c_str(), INS_Graphic, ILF_Exact|ILF_Null);
		else
			I_Error("LoadFontImage: NULL image name provided for font %s!", def->name.c_str());
		if (!font_image)
			I_Error("LoadFontImage: Image %s not found for font %s!", def->image_name.c_str(), def->name.c_str());
		int char_height = font_image->actual_h / 16;
		int char_width = font_image->actual_w / 16;
		im_char_height = (def->default_size == 0.0 ? char_height : def->default_size) * font_image->scale_y;
		im_char_width = (def->default_size == 0.0 ? char_width : def->default_size) * font_image->scale_x;
		im_mono_width = 0;
		spacing = def->spacing;
		// Determine individual character widths and ratios
		individual_char_widths = new float[256];
		individual_char_ratios = new float[256];
		epi::image_data_c *char_data = ReadAsEpiBlock((image_c *)font_image);
		for (int i = 0; i < 256; i++)
		{
			int px =      i % 16;
			int py = 15 - i / 16;
			individual_char_widths[i] = char_data->ImageCharacterWidth(px * char_width, py * char_height, px * char_width + char_width, py * char_height + char_height) * font_image->scale_x;
			if (def->default_size > 0.0)
				individual_char_widths[i] *= (def->default_size / char_width);
			if (individual_char_widths[i] > im_mono_width)
				im_mono_width = individual_char_widths[i];
			individual_char_ratios[i] = individual_char_widths[i] / im_char_height;
		}
		delete char_data;
	}
}

void font_c::LoadFontTTF()
{
	if (!ttf_buffer)
	{
		if (def->ttf_name.empty())
		{
			I_Error("LoadFontTTF: No TTF file/lump name provided for font %s!", def->name.c_str());
		}

		for (int i=0; i < hu_fonts.GetSize(); i++)
		{
			if (epi::strcmp(hu_fonts[i]->def->ttf_name,def->ttf_name) == 0)
			{
				if (hu_fonts[i]->ttf_buffer)
					ttf_buffer = hu_fonts[i]->ttf_buffer;
				if (hu_fonts[i]->ttf_info)
					ttf_info = hu_fonts[i]->ttf_info;
			}
		}

		if (!ttf_buffer)
		{
			epi::file_c *F;
			
			if (std::filesystem::path(def->ttf_name).has_extension()) // check for pack file
				F = W_OpenPackFile(def->ttf_name);
			else
				F = W_OpenLump(W_CheckNumForName(def->ttf_name.c_str()));

			if (!F)
				I_Error("LoadFontTTF: '%s' not found for font %s.\n", def->ttf_name.c_str(), def->name.c_str()); 

			ttf_buffer = F->LoadIntoMemory();

			delete F;
		}

		if (!ttf_info)
		{
			ttf_info = new stbtt_fontinfo;
			if (!stbtt_InitFont(ttf_info, ttf_buffer, 0))
				I_Error("LoadFontTTF: Could not initialize font %s.\n", def->name.c_str());
		}

		ttf_char_t ref;

		ref.glyph_index = 0;

		char ch = 0;

		if (stbtt_FindGlyphIndex(ttf_info, cp437_unicode_values[static_cast<u8_t>('M')]) > 0)
		{
			ch = 'M';
			ref.glyph_index = stbtt_FindGlyphIndex(ttf_info, cp437_unicode_values[static_cast<u8_t>(ch)]);
		} 
		else if (stbtt_FindGlyphIndex(ttf_info, cp437_unicode_values[static_cast<u8_t>('O')]) > 0)
		{
			ch = 'O';
			ref.glyph_index = stbtt_FindGlyphIndex(ttf_info, cp437_unicode_values[static_cast<u8_t>(ch)]);
		}
		else if (stbtt_FindGlyphIndex(ttf_info, cp437_unicode_values[static_cast<u8_t>('W')]) > 0)
		{
			ch = 'W';
			ref.glyph_index = stbtt_FindGlyphIndex(ttf_info, cp437_unicode_values[static_cast<u8_t>(ch)]);
		}
		else
		{
			for (char c=32; c < 127; c++)
			{
				if (stbtt_FindGlyphIndex(ttf_info, cp437_unicode_values[static_cast<u8_t>(c)]) > 0)
				{
					ch = c;
					ref.glyph_index = stbtt_FindGlyphIndex(ttf_info, cp437_unicode_values[static_cast<u8_t>(ch)]);
					break;
				}
			}
		}

		if (ref.glyph_index == 0)
			I_Error("LoadFontTTF: No suitable characters in font %s.\n", def->name.c_str());

		ref.packed_char = new stbtt_packedchar;
		ref.char_quad = new stbtt_aligned_quad;

		if (def->default_size == 0.0)
			def->default_size = 7.0f;

		ttf_kern_scale = stbtt_ScaleForPixelHeight(ttf_info, def->default_size);

		unsigned char *temp_bitmap = new unsigned char [64*64];

		stbtt_pack_context *spc = new stbtt_pack_context;
		stbtt_PackBegin(spc, temp_bitmap, 64, 64, 0, 1, NULL);
		stbtt_PackSetOversampling(spc, 1, 1);
		stbtt_PackFontRange(spc, ttf_buffer, 0, 48, cp437_unicode_values[static_cast<u8_t>(ch)], 1, ref.packed_char);
		stbtt_PackEnd(spc);
		glGenTextures(1, &ref.tex_id);
		glBindTexture(GL_TEXTURE_2D, ref.tex_id);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 64,64, 0, GL_ALPHA, GL_UNSIGNED_BYTE, temp_bitmap);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glGenTextures(1, &ref.smoothed_tex_id);
		glBindTexture(GL_TEXTURE_2D, ref.smoothed_tex_id);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 64,64, 0, GL_ALPHA, GL_UNSIGNED_BYTE, temp_bitmap);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		delete[] temp_bitmap;
		float x = 0.0f;
		float y = 0.0f;
		float ascent = 0.0f;
		float descent = 0.0f;
		float linegap = 0.0f;
		stbtt_GetPackedQuad(ref.packed_char, 64, 64, 0, &x, &y, ref.char_quad, 0);
		stbtt_GetScaledFontVMetrics(ttf_buffer, 0, 48, &ascent, &descent, &linegap);
		ref.width = (ref.char_quad->x1 - ref.char_quad->x0) * (def->default_size / 48.0);
		ref.height = (ref.char_quad->y1 - ref.char_quad->y0) * (def->default_size / 48.0);
		ttf_char_width = ref.width;
		ttf_char_height = (ascent - descent) * (def->default_size / 48.0);
		ref.y_shift = (ttf_char_height - ref.height) + (ref.char_quad->y1 * (def->default_size / 48.0));
		ttf_ref_yshift = ref.y_shift;
		ttf_ref_height = ref.height;
		ttf_glyph_map.try_emplace(static_cast<u8_t>(ch), ref);
		spacing = def->spacing + 0.5; // + 0.5 for at least a minimal buffer between letters by default
	}
}

void font_c::Load()
{
	switch (def->type)
	{
		case FNTYP_Patch:
			LoadPatches();
			break;

		case FNTYP_Image:
			LoadFontImage();
			break;

		case FNTYP_TrueType:
			LoadFontTTF();
			break;

		default:
			I_Error("Coding error, unknown font type %d\n", def->type);
			break; /* NOT REACHED */
	}
}


float font_c::NominalWidth() const
{
	if (def->type == FNTYP_Image)
		return im_char_width + spacing;

	if (def->type == FNTYP_Patch)
		return p_cache.width + spacing;

	if (def->type == FNTYP_TrueType)
		return ttf_char_width + spacing;

	I_Error("font_c::NominalWidth : unknown FONT type %d\n", def->type);
	return 1; /* NOT REACHED */
}

float font_c::NominalHeight() const
{
	if (def->type == FNTYP_Image)
		return im_char_height;

	if (def->type == FNTYP_Patch)
		return p_cache.height;

	if (def->type == FNTYP_TrueType)
		return ttf_char_height;

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
	if (def->type == FNTYP_Image)
		return font_image;

	if (def->type == FNTYP_TrueType)
	{
		if (ttf_glyph_map.find(static_cast<u8_t>(ch)) != ttf_glyph_map.end())
			// Create or return faux backup image
			return W_ImageLookup("TTFDUMMY", INS_Graphic, ILF_Font);
		else
			return NULL;
	}

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

float font_c::CharRatio(char ch)
{
	SYS_ASSERT(def->type == FNTYP_Image);

	if (ch == ' ')
		return 0.4f;
	else
		return individual_char_ratios[static_cast<u8_t>(ch)];
}

//
// Returns the width of the IBM cp437 char in the font.
//
float font_c::CharWidth(char ch)
{
	if (def->type == FNTYP_Image)
	{
		if (ch == ' ')
			return im_char_width * 2 / 5 + spacing;
		else
			return individual_char_widths[static_cast<u8_t>(ch)] + spacing;
	}

	if (def->type == FNTYP_TrueType)
	{
		auto find_glyph = ttf_glyph_map.find(static_cast<u8_t>(ch));
		if (find_glyph != ttf_glyph_map.end())
		{
			return find_glyph->second.width + spacing;
		}
		else
		{
			ttf_char_t character;
			character.packed_char = new stbtt_packedchar;
			character.char_quad = new stbtt_aligned_quad;
			unsigned char *temp_bitmap = new unsigned char [64 * 64];
			stbtt_pack_context *spc = new stbtt_pack_context;
			stbtt_PackBegin(spc, temp_bitmap, 64, 64, 0, 1, NULL);
			stbtt_PackSetOversampling(spc, 1, 1);
			stbtt_PackFontRange(spc, ttf_buffer, 0, 48, cp437_unicode_values[static_cast<u8_t>(ch)], 1, character.packed_char);
			stbtt_PackEnd(spc);
			glGenTextures(1, &character.tex_id);
			glBindTexture(GL_TEXTURE_2D, character.tex_id);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 64,64, 0, GL_ALPHA, GL_UNSIGNED_BYTE, temp_bitmap);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glGenTextures(1, &character.smoothed_tex_id);
			glBindTexture(GL_TEXTURE_2D, character.smoothed_tex_id);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 64,64, 0, GL_ALPHA, GL_UNSIGNED_BYTE, temp_bitmap);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			delete[] temp_bitmap;
			float x = 0.0f;
			float y = 0.0f;
			stbtt_GetPackedQuad(character.packed_char, 64, 64, 0, &x, &y, character.char_quad, 0);
			if (ch == ' ')
				character.width = ttf_char_width * 3 / 5;
			else
				character.width = (character.char_quad->x1 - character.char_quad->x0) * (def->default_size / 48.0);
			character.height = (character.char_quad->y1 - character.char_quad->y0) * (def->default_size / 48.0);
			character.y_shift = (ttf_char_height - character.height) + (character.char_quad->y1 * (def->default_size / 48.0));
			character.glyph_index = stbtt_FindGlyphIndex(ttf_info, cp437_unicode_values[static_cast<u8_t>(ch)]);
			ttf_glyph_map.try_emplace(static_cast<u8_t>(ch), character);
			return character.width + spacing;
		}
	}

	SYS_ASSERT(def->type == FNTYP_Patch);

	if (ch == ' ')
		return p_cache.width * 3 / 5 + spacing;

	const image_c *im = CharImage(ch);

	if (! im)
		return DUMMY_WIDTH;

	if (def->default_size > 0.0)
		return (def->default_size * (IM_WIDTH(im) / IM_HEIGHT(im))) + spacing;
	else
		return IM_WIDTH(im) + spacing;
}


//
// Returns the maximum number of characters which can fit within pixel_w
// pixels.  The string may not contain any newline characters.
//
int font_c::MaxFit(int pixel_w, const char *str)
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
// Get glyph index for TTF Character. If character hasn't been cached yet, cache it.
//
int font_c::GetGlyphIndex(char ch)
{
		assert(def->type == FNTYP_TrueType);

		auto find_glyph = ttf_glyph_map.find(static_cast<u8_t>(ch));
		if (find_glyph != ttf_glyph_map.end())
		{
			return find_glyph->second.glyph_index;
		}
		else
		{
			ttf_char_t character;
			character.packed_char = new stbtt_packedchar;
			character.char_quad = new stbtt_aligned_quad;
			unsigned char *temp_bitmap = new unsigned char [64 * 64];
			stbtt_pack_context *spc = new stbtt_pack_context;
			stbtt_PackBegin(spc, temp_bitmap, 64, 64, 0, 1, NULL);
			stbtt_PackSetOversampling(spc, 1, 1);
			stbtt_PackFontRange(spc, ttf_buffer, 0, 48, cp437_unicode_values[static_cast<u8_t>(ch)], 1, character.packed_char);
			stbtt_PackEnd(spc);
			glGenTextures(1, &character.tex_id);
			glBindTexture(GL_TEXTURE_2D, character.tex_id);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 64,64, 0, GL_ALPHA, GL_UNSIGNED_BYTE, temp_bitmap);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glGenTextures(1, &character.smoothed_tex_id);
			glBindTexture(GL_TEXTURE_2D, character.smoothed_tex_id);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 64,64, 0, GL_ALPHA, GL_UNSIGNED_BYTE, temp_bitmap);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			delete[] temp_bitmap;
			float x = 0.0f;
			float y = 0.0f;
			stbtt_GetPackedQuad(character.packed_char, 64, 64, 0, &x, &y, character.char_quad, 0);
			if (ch == ' ')
				character.width = ttf_char_width * 3 / 5;
			else
				character.width = (character.char_quad->x1 - character.char_quad->x0) * (def->default_size / 48.0);
			character.height = (character.char_quad->y1 - character.char_quad->y0) * (def->default_size / 48.0);
			character.y_shift = (ttf_char_height - character.height) + (character.char_quad->y1 * (def->default_size / 48.0));
			character.glyph_index = stbtt_FindGlyphIndex(ttf_info, cp437_unicode_values[static_cast<u8_t>(ch)]);
			ttf_glyph_map.try_emplace(static_cast<u8_t>(ch), character);
			return character.glyph_index;
		}
}

//
// Find string width from hu_font chars.  The string may not contain
// any newline characters.
//
float font_c::StringWidth(const char *str)
{
	float w = 0;

	if (!str) return 0;

	std::string_view width_checker = str;

	for (size_t i = 0; i < width_checker.size(); i++)
	{
		w += CharWidth(width_checker[i]);
		if (def->type == FNTYP_TrueType && i+1 < width_checker.size())
			w += stbtt_GetGlyphKernAdvance(ttf_info, GetGlyphIndex(width_checker[i]), GetGlyphIndex(width_checker[i+1])) * ttf_kern_scale;
	}

	return w;
}


//
// Find number of lines in string.
//
int font_c::StringLines(const char *str) const
{
	int slines = 1;

	for (; *str; str++)
		if (*str == '\n')
			slines++;

	return slines;
}


void font_c::DrawChar320(float x, float y, char ch, float scale, float aspect,
    const colourmap_c *colmap, float alpha) const
{
	SYS_ASSERT(def->type == FNTYP_Patch);

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
