//----------------------------------------------------------------------------
//  EDGE Heads-up-display Font code
//----------------------------------------------------------------------------
//
//  Copyright (c) 2004-2024 The EDGE Team.
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

#include "i_defs.h"
#include "i_defs_gl.h"

#include "main.h"
#include "filesystem.h"
#include "font.h"
#include "image_data.h"
#include "str_util.h"

#include "dm_defs.h"
#include "dm_state.h"
#include "hu_font.h"
#include "r_local.h"
#include "r_colormap.h"
#include "r_draw.h"
#include "r_texgl.h"
#include "r_modes.h"
#include "r_image.h"
#include "w_files.h"
#include "w_wad.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define DUMMY_WIDTH 8

extern image_data_c *ReadAsEpiBlock(image_c *rim);

// all the fonts that's fit to print
font_container_c hu_fonts;

int current_font_size;

static const int res_font_sizes[3]        = {12, 24, 48};
static const int res_font_bitmap_sizes[3] = {512, 1024, 2048};

font_c::font_c(fontdef_c *_def) : def(_def)
{
    font_image = nullptr;
    ttf_info   = nullptr;
    ttf_buffer = nullptr;
    Z_Clear(ttf_kern_scale, float, 3);
    Z_Clear(ttf_ref_yshift, float, 3);
    Z_Clear(ttf_ref_height, float, 3);
}

font_c::~font_c()
{ }

void font_c::BumpPatchName(char *name)
{
    // loops to increment the 10s (100s, etc) digit
    for (char *s = name + strlen(name) - 1; s >= name; s--)
    {
        // only handle digits and letters
        if (!isalnum(*s))
            break;

        if (*s == '9')
        {
            *s = '0';
            continue;
        }
        if (*s == 'Z')
        {
            *s = 'A';
            continue;
        }
        if (*s == 'z')
        {
            *s = 'a';
            continue;
        }

        (*s) += 1;
        break;
    }
}

void font_c::LoadPatches()
{
    // range of characters
    int first = 9999;
    int last = 0;
	const image_c **images;
	const image_c *missing;
    const fontpatch_c *pat;

    // determine full range
    for (pat = def->patches; pat; pat = pat->next)
    {
        if (pat->char1 < first)
            first = pat->char1;

        if (pat->char2 > last)
            last = pat->char2;
    }

    int total = last - first + 1;

    SYS_ASSERT(def->patches);
    SYS_ASSERT(total >= 1);

	images = new const image_c *[total];
	memset(images, 0, sizeof(const image_c *) * total);

	// Atlas Stuff
	std::unordered_map<int, image_data_c *> patch_data;
    std::vector<image_data_c *> temp_imdata;

	missing = def->missing_patch != "" ?
		W_ImageLookup(def->missing_patch.c_str(), INS_Graphic, ILF_Font|ILF_Null) : nullptr;
	image_data_c *missing_imdata = nullptr;

	if (missing)
	{
		image_data_c *tmp_img = ReadAsEpiBlock((image_c *)(missing));
		if (tmp_img->bpp == 1)
		{
			image_data_c *rgb_img = R_PalettisedToRGB(tmp_img, (const uint8_t *) &playpal_data[0], missing->opacity);
			delete tmp_img;
			missing_imdata = rgb_img;
		}
		else
			missing_imdata = tmp_img;
        missing_imdata->offset_x = missing->offset_x;
        missing_imdata->offset_y = missing->offset_y;
        missing_imdata->scale_x = missing->scale_x;
        missing_imdata->scale_y = missing->scale_y;
	}

    // First pass, add the images that are good
    for (pat = def->patches; pat; pat = pat->next)
    {
        // patch name
        char pname[40];

        SYS_ASSERT(strlen(pat->patch1.c_str()) < 36);
        strcpy(pname, pat->patch1.c_str());

        for (int ch = pat->char1; ch <= pat->char2; ch++, BumpPatchName(pname))
        {
#if 0 // DEBUG
			I_Printf("- LoadFont [%s] : char %d = %s\n", def->name.c_str(), ch, pname);
#endif
            int idx = ch - first;
            SYS_ASSERT(0 <= idx && idx < total);

			images[idx] = W_ImageLookup(pname, INS_Graphic, ILF_Font|ILF_Null);

			if (images[idx])
			{
				image_data_c *tmp_img = ReadAsEpiBlock((image_c *)(images[idx]));
				if (tmp_img->bpp == 1)
				{
					image_data_c *rgb_img = R_PalettisedToRGB(tmp_img, (const uint8_t *) &playpal_data[0], images[idx]->opacity);
					delete tmp_img;
					tmp_img = rgb_img;
				}
                tmp_img->offset_x = images[idx]->offset_x;
                tmp_img->offset_y = images[idx]->offset_y;
                tmp_img->scale_x = images[idx]->scale_x;
                tmp_img->scale_y = images[idx]->scale_y;
				patch_data.try_emplace(cp437_unicode_values[(uint8_t)ch], tmp_img);
                temp_imdata.push_back(tmp_img);
			}
		}
	}

	// Second pass to try lower->uppercase fallbacks, or failing that add the missing image (if present)
	for (int ch = 0; ch < 256; ch++)
	{
        if (!patch_data.count(cp437_unicode_values[(uint8_t)ch]))
        {
            if ('a' <= ch && ch <= 'z' && patch_data.count(cp437_unicode_values[(uint8_t)(toupper(ch))]))
                patch_data.try_emplace(cp437_unicode_values[(uint8_t)ch], patch_data.at(cp437_unicode_values[(uint8_t)(toupper(ch))]));
            else if (missing_imdata)
                patch_data.try_emplace(cp437_unicode_values[(uint8_t)ch], missing_imdata);  
        }
	}

	image_atlas_c *atlas = Image_Pack(patch_data);
	for (auto patch : temp_imdata)
        delete patch;
	delete missing_imdata;
	if (atlas)
	{
        // Uncomment this to save the generated atlas. Note: will be inverted.
        /*std::string atlas_png = epi::PathAppend(home_dir, epi::StringFormat("atlas_%s.png", def->name.c_str()));
        if (epi::FileExists(atlas_png))
            epi::FS_Remove(atlas_png);
        PNG_Save(atlas_png, atlas->data);*/
		p_cache.atlas_rects = atlas->rects;
		glGenTextures(1, &p_cache.atlas_texid);
		glBindTexture(GL_TEXTURE_2D, p_cache.atlas_texid);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlas->data->width, atlas->data->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, atlas->data->pixels);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glGenTextures(1, &p_cache.atlas_smoothed_texid);
		glBindTexture(GL_TEXTURE_2D, p_cache.atlas_smoothed_texid);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlas->data->width, atlas->data->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, atlas->data->pixels);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        atlas->data->Whiten();
        glGenTextures(1, &p_cache.atlas_whitened_texid);
		glBindTexture(GL_TEXTURE_2D, p_cache.atlas_whitened_texid);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlas->data->width, atlas->data->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, atlas->data->pixels);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glGenTextures(1, &p_cache.atlas_whitened_smoothed_texid);
		glBindTexture(GL_TEXTURE_2D, p_cache.atlas_whitened_smoothed_texid);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlas->data->width, atlas->data->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, atlas->data->pixels);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		delete atlas;
	}
	else
		I_Error("Failed to create atlas for patch font %s!\n", def->name.c_str());

    if (p_cache.atlas_rects.empty())
    {
        I_Warning("Font [%s] has no loaded patches !\n", def->name.c_str());
        p_cache.width = p_cache.height = 7;
        return;
    }

    image_rect_c Nom;

	if (p_cache.atlas_rects.count(cp437_unicode_values[(uint8_t)('M')]))
		Nom = p_cache.atlas_rects.at(cp437_unicode_values[(uint8_t)('M')]);
    else if (p_cache.atlas_rects.count(cp437_unicode_values[(uint8_t)('m')]))
		Nom = p_cache.atlas_rects.at(cp437_unicode_values[(uint8_t)('m')]);
    else if (p_cache.atlas_rects.count(cp437_unicode_values[(uint8_t)('0')]))
		Nom = p_cache.atlas_rects.at(cp437_unicode_values[(uint8_t)('0')]);
    else // backup plan: just use first patch found
        Nom = p_cache.atlas_rects.begin()->second;

	if (def->default_size > 0.0)
	{
		p_cache.height = def->default_size;
		p_cache.width  = def->default_size * (Nom.iw / Nom.ih);
		p_cache.ratio = p_cache.width / p_cache.height;
	}
	else
	{
		p_cache.width  = Nom.iw;
		p_cache.height = Nom.ih;
		p_cache.ratio = Nom.iw / Nom.ih;
	}
	spacing = def->spacing;
}

void font_c::LoadFontImage()
{
    if (!font_image)
    {
        if (!def->image_name.empty())
            font_image = W_ImageLookup(def->image_name.c_str(), INS_Graphic, ILF_Exact | ILF_Null);
        else
            I_Error("LoadFontImage: NULL image name provided for font %s!", def->name.c_str());
        if (!font_image)
            I_Error("LoadFontImage: Image %s not found for font %s!", def->image_name.c_str(), def->name.c_str());
        int char_height = font_image->actual_h / 16;
        int char_width  = font_image->actual_w / 16;
        im_char_height  = (def->default_size == 0.0 ? char_height : def->default_size) * font_image->scale_y;
        im_char_width   = (def->default_size == 0.0 ? char_width : def->default_size) * font_image->scale_x;
        im_mono_width   = 0;
        spacing         = def->spacing;
        // Determine individual character widths and ratios
        individual_char_widths       = new float[256];
        individual_char_ratios       = new float[256];
        image_data_c *char_data = ReadAsEpiBlock((image_c *)font_image);
        for (int i = 0; i < 256; i++)
        {
            int px = i % 16;
            int py = 15 - i / 16;
            individual_char_widths[i] =
                char_data->ImageCharacterWidth(px * char_width, py * char_height, px * char_width + char_width,
                                               py * char_height + char_height) *
                font_image->scale_x;
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

        for (size_t i = 0; i < hu_fonts.size(); i++)
        {
            if (epi::StringCaseCompareASCII(hu_fonts[i]->def->ttf_name, def->ttf_name) == 0)
            {
                if (hu_fonts[i]->ttf_buffer)
                    ttf_buffer = hu_fonts[i]->ttf_buffer;
                if (hu_fonts[i]->ttf_info)
                    ttf_info = hu_fonts[i]->ttf_info;
            }
        }

        if (!ttf_buffer)
        {
            epi::File *F;

            if (!epi::GetExtension(def->ttf_name).empty()) // check for pack file
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

        if (stbtt_FindGlyphIndex(ttf_info, cp437_unicode_values[(uint8_t)('M')]) > 0)
        {
            ch              = 'M';
            ref.glyph_index = stbtt_FindGlyphIndex(ttf_info, cp437_unicode_values[(uint8_t)ch]);
        }
        else if (stbtt_FindGlyphIndex(ttf_info, cp437_unicode_values[(uint8_t)('O')]) > 0)
        {
            ch              = 'O';
            ref.glyph_index = stbtt_FindGlyphIndex(ttf_info, cp437_unicode_values[(uint8_t)ch]);
        }
        else if (stbtt_FindGlyphIndex(ttf_info, cp437_unicode_values[(uint8_t)('W')]) > 0)
        {
            ch              = 'W';
            ref.glyph_index = stbtt_FindGlyphIndex(ttf_info, cp437_unicode_values[(uint8_t)ch]);
        }
        else
        {
            for (char c = 32; c < 127; c++)
            {
                if (stbtt_FindGlyphIndex(ttf_info, cp437_unicode_values[(uint8_t)(c)]) > 0)
                {
                    ch              = c;
                    ref.glyph_index = stbtt_FindGlyphIndex(ttf_info, cp437_unicode_values[(uint8_t)ch]);
                    break;
                }
            }
        }

        if (ref.glyph_index == 0)
            I_Error("LoadFontTTF: No suitable characters in font %s.\n", def->name.c_str());

        for (int i = 0; i < 3; i++)
        {
            ttf_atlas[i]                                   = new stbtt_pack_range;
            ttf_atlas[i]->first_unicode_codepoint_in_range = 0;
            ttf_atlas[i]->array_of_unicode_codepoints      = (int *)cp437_unicode_values;
            ttf_atlas[i]->font_size                        = res_font_sizes[i];
            ttf_atlas[i]->num_chars                        = 256;
            ttf_atlas[i]->chardata_for_range               = new stbtt_packedchar[256];

            if (def->default_size == 0.0)
                def->default_size = 7.0f;

            ttf_kern_scale[i] = stbtt_ScaleForPixelHeight(ttf_info, def->default_size);

            unsigned char *temp_bitmap = new unsigned char[res_font_bitmap_sizes[i] * res_font_bitmap_sizes[i]];

            stbtt_pack_context *spc = new stbtt_pack_context;
            stbtt_PackBegin(spc, temp_bitmap, res_font_bitmap_sizes[i], res_font_bitmap_sizes[i], 0, 1, NULL);
            stbtt_PackSetOversampling(spc, 2, 2);
            stbtt_PackFontRanges(spc, ttf_buffer, 0, ttf_atlas[i], 1);
            stbtt_PackEnd(spc);
            glGenTextures(1, &ttf_tex_id[i]);
            glBindTexture(GL_TEXTURE_2D, ttf_tex_id[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, res_font_bitmap_sizes[i], res_font_bitmap_sizes[i], 0, GL_ALPHA,
                         GL_UNSIGNED_BYTE, temp_bitmap);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glGenTextures(1, &ttf_smoothed_tex_id[i]);
            glBindTexture(GL_TEXTURE_2D, ttf_smoothed_tex_id[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, res_font_bitmap_sizes[i], res_font_bitmap_sizes[i], 0, GL_ALPHA,
                         GL_UNSIGNED_BYTE, temp_bitmap);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            delete[] temp_bitmap;
            float x          = 0.0f;
            float y          = 0.0f;
            float ascent     = 0.0f;
            float descent    = 0.0f;
            float linegap    = 0.0f;
            ref.char_quad[i] = new stbtt_aligned_quad;
            stbtt_GetPackedQuad(ttf_atlas[i]->chardata_for_range, res_font_bitmap_sizes[i], res_font_bitmap_sizes[i],
                                (uint8_t)ch, &x, &y, ref.char_quad[i], 0);
            stbtt_GetScaledFontVMetrics(ttf_buffer, 0, res_font_sizes[i], &ascent, &descent, &linegap);
            ref.width[i]      = (ref.char_quad[i]->x1 - ref.char_quad[i]->x0) * (def->default_size / res_font_sizes[i]);
            ref.height[i]     = (ref.char_quad[i]->y1 - ref.char_quad[i]->y0) * (def->default_size / res_font_sizes[i]);
            ttf_char_width[i] = ref.width[i];
            ttf_char_height[i] = (ascent - descent) * (def->default_size / res_font_sizes[i]);
            ref.y_shift[i] =
                (ttf_char_height[i] - ref.height[i]) + (ref.char_quad[i]->y1 * (def->default_size / res_font_sizes[i]));
            ttf_ref_yshift[i] = ref.y_shift[i];
            ttf_ref_height[i] = ref.height[i];
        }
        ttf_glyph_map.try_emplace((uint8_t)ch, ref);
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
        return ttf_char_width[current_font_size] + spacing;

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
        return ttf_char_height[current_font_size];

    I_Error("font_c::NominalHeight : unknown FONT type %d\n", def->type);
    return 1; /* NOT REACHED */
}

const image_c *font_c::CharImage(char ch) const
{
    if (def->type == FNTYP_Image)
        return font_image;

    if (def->type == FNTYP_TrueType)
    {
        if (ttf_glyph_map.find((uint8_t)ch) != ttf_glyph_map.end())
            // Create or return dummy image
            return W_ImageLookup("FONT_DUMMY_IMAGE", INS_Graphic, ILF_Font);
        else
            return NULL;
    }

    SYS_ASSERT(def->type == FNTYP_Patch);

    if (ch == ' ')
        return NULL;

    if (p_cache.atlas_rects.count(cp437_unicode_values[(uint8_t)ch]))
        return W_ImageLookup("FONT_DUMMY_IMAGE", INS_Graphic, ILF_Font);
    else
        return NULL;
}

float font_c::CharRatio(char ch)
{
    SYS_ASSERT(def->type == FNTYP_Image);

    if (ch == ' ')
        return 0.4f;
    else
        return individual_char_ratios[(uint8_t)ch];
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
            return individual_char_widths[(uint8_t)ch] + spacing;
    }

    if (def->type == FNTYP_TrueType)
    {
        auto find_glyph = ttf_glyph_map.find((uint8_t)ch);
        if (find_glyph != ttf_glyph_map.end())
            return (find_glyph->second.width[current_font_size] + spacing) * v_pixelaspect.f;
        else
        {
            ttf_char_t character;
            for (int i = 0; i < 3; i++)
            {
                character.char_quad[i] = new stbtt_aligned_quad;
                float x                = 0.0f;
                float y                = 0.0f;
                stbtt_GetPackedQuad(ttf_atlas[i]->chardata_for_range, res_font_bitmap_sizes[i],
                                    res_font_bitmap_sizes[i], (uint8_t)ch, &x, &y, character.char_quad[i], 0);
                if (ch == ' ')
                    character.width[i] = ttf_char_width[i] * 3 / 5;
                else
                    character.width[i] = (character.char_quad[i]->x1 - character.char_quad[i]->x0) *
                                         (def->default_size / res_font_sizes[i]);
                character.height[i] =
                    (character.char_quad[i]->y1 - character.char_quad[i]->y0) * (def->default_size / res_font_sizes[i]);
                character.y_shift[i] = (ttf_char_height[i] - character.height[i]) +
                                       (character.char_quad[i]->y1 * (def->default_size / res_font_sizes[i]));
            }
            character.glyph_index = stbtt_FindGlyphIndex(ttf_info, cp437_unicode_values[(uint8_t)ch]);
            ttf_glyph_map.try_emplace((uint8_t)ch, character);
            return (character.width[current_font_size] + spacing) * v_pixelaspect.f;
        }
    }

    SYS_ASSERT(def->type == FNTYP_Patch);

    if (ch == ' ')
        return p_cache.width * 3 / 5 + spacing;


    if (!p_cache.atlas_rects.count(cp437_unicode_values[(uint8_t)ch]))
        return DUMMY_WIDTH;

    image_rect_c rect = p_cache.atlas_rects.at(cp437_unicode_values[(uint8_t)ch]);

    if (def->default_size > 0.0)
        return (def->default_size * ((float)rect.iw) / rect.ih) + spacing;
    else
        return rect.iw + spacing;
}

//
// Returns the maximum number of characters which can fit within pixel_w
// pixels.  The string may not contain any newline characters.
//
int font_c::MaxFit(int pixel_w, const char *str)
{
    int         w = 0;
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

    auto find_glyph = ttf_glyph_map.find((uint8_t)ch);
    if (find_glyph != ttf_glyph_map.end())
        return find_glyph->second.glyph_index;
    else
    {
        ttf_char_t character;
        for (int i = 0; i < 3; i++)
        {
            character.char_quad[i] = new stbtt_aligned_quad;
            float x                = 0.0f;
            float y                = 0.0f;
            stbtt_GetPackedQuad(ttf_atlas[i]->chardata_for_range, res_font_bitmap_sizes[i], res_font_bitmap_sizes[i],
                                (uint8_t)ch, &x, &y, character.char_quad[i], 0);
            if (ch == ' ')
                character.width[i] = ttf_char_width[i] * 3 / 5;
            else
                character.width[i] =
                    (character.char_quad[i]->x1 - character.char_quad[i]->x0) * (def->default_size / res_font_sizes[i]);
            character.height[i] =
                (character.char_quad[i]->y1 - character.char_quad[i]->y0) * (def->default_size / res_font_sizes[i]);
            character.y_shift[i] = (ttf_char_height[i] - character.height[i]) +
                                   (character.char_quad[i]->y1 * (def->default_size / res_font_sizes[i]));
        }
        character.glyph_index = stbtt_FindGlyphIndex(ttf_info, cp437_unicode_values[(uint8_t)ch]);
        ttf_glyph_map.try_emplace((uint8_t)ch, character);
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

    if (!str)
        return 0;

    std::string_view width_checker = str;

    for (size_t i = 0; i < width_checker.size(); i++)
    {
        w += CharWidth(width_checker[i]);
        if (def->type == FNTYP_TrueType && i + 1 < width_checker.size())
            w += stbtt_GetGlyphKernAdvance(ttf_info, GetGlyphIndex(width_checker[i]),
                                           GetGlyphIndex(width_checker[i + 1])) *
                 ttf_kern_scale[current_font_size];
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

//----------------------------------------------------------------------------
//  font_container_c class
//----------------------------------------------------------------------------

// Never returns NULL.
//
font_c *font_container_c::Lookup(fontdef_c *def)
{
    SYS_ASSERT(def);

    for (auto iter = begin(); iter != end(); iter++)
    {
        font_c *f = *iter;

        if (def == f->def)
            return f;
    }

    font_c *new_f = new font_c(def);

    new_f->Load();
    push_back(new_f);

    return new_f;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
