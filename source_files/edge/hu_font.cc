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

#include "hu_font.h"

#include "ddf_font.h"
#include "ddf_main.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "epi.h"
#include "epi_filesystem.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"
#include "i_defs_gl.h"
#include "im_data.h"
#include "r_colormap.h"
#include "r_draw.h"
#include "r_image.h"
#include "r_misc.h"
#include "r_modes.h"
#include "r_texgl.h"
#include "stb_truetype.h"
#include "w_files.h"
#include "w_wad.h"

static constexpr uint8_t kDummyCharacterWidth = 8;

extern ImageData *ReadAsEpiBlock(Image *rim);

// all the fonts that's fit to print
FontContainer hud_fonts;

int current_font_size;

static constexpr int truetype_scaling_font_sizes[3]   = {12, 24, 48};
static constexpr int truetype_scaling_bitmap_sizes[3] = {512, 1024, 2048};

ImageFont::ImageFont(FontDefinition *definition)
{
    definition_             = definition;
    font_image_             = nullptr;
    individual_char_ratios_ = nullptr;
    individual_char_widths_ = nullptr;

    if (!definition_->image_name_.empty())
        font_image_ =
            ImageLookup(definition_->image_name_.c_str(), kImageNamespaceGraphic, kImageLookupExact | kImageLookupNull);
    else
        FatalError("LoadFontImage: No image name provided for font %s!", definition_->name_.c_str());
    if (!font_image_)
        FatalError("LoadFontImage: Image %s not found for font %s!", definition_->image_name_.c_str(),
                   definition_->name_.c_str());

    int char_height = font_image_->height_ / 16;
    int char_width  = font_image_->width_ / 16;
    image_character_height_ =
        (definition_->default_size_ == 0.0 ? char_height : definition_->default_size_) * font_image_->scale_y_;
    image_character_width_ =
        (definition_->default_size_ == 0.0 ? char_width : definition_->default_size_) * font_image_->scale_x_;
    image_monospace_width_ = 0;
    spacing_               = definition_->spacing_;
    // Determine individual character widths and ratios
    individual_char_widths_ = new float[256];
    individual_char_ratios_ = new float[256];
    ImageData *char_data    = ReadAsEpiBlock((Image *)font_image_);
    uint16_t   real_left    = 0;
    uint16_t   real_right   = 0;
    RGBAColor  background   = kRGBATransparent;
    // Assumes that first pixel is part of the background; for a spritesheet font this is almost certainly
    // the case
    if (char_data->depth_ == 3)
        background = epi::MakeRGBA(char_data->pixels_[0], char_data->pixels_[1], char_data->pixels_[2]);
    for (int i = 0; i < 256; i++)
    {
        int px = i % 16;
        int py = 15 - i / 16;
        char_data->DetermineRealBounds(nullptr, &real_left, &real_right, nullptr, background, px * char_width,
                                       px * char_width + char_width, py * char_height, py * char_height + char_height);
        individual_char_widths_[i] = font_image_->scale_x_ * (real_right - real_left);
        if (definition_->default_size_ > 0.0)
            individual_char_widths_[i] *= (definition_->default_size_ / char_width);
        if (individual_char_widths_[i] > image_monospace_width_)
            image_monospace_width_ = individual_char_widths_[i];
        individual_char_ratios_[i] = individual_char_widths_[i] / image_character_height_;
    }
    delete char_data;
}

PatchFont::PatchFont(FontDefinition *definition)
{
    definition_ = definition;
    // range of characters
    int              first = 9999;
    int              last  = 0;
    const Image    **images;
    const Image     *missing;
    const FontPatch *pat;

    // determine full range
    for (pat = definition_->patches_; pat; pat = pat->next)
    {
        if (pat->char1 < first)
            first = pat->char1;

        if (pat->char2 > last)
            last = pat->char2;
    }

    int total = last - first + 1;

    EPI_ASSERT(definition_->patches_);
    EPI_ASSERT(total >= 1);

    images = new const Image *[total];
    EPI_CLEAR_MEMORY(images, const Image *, total);

    // Atlas Stuff
    std::unordered_map<int, ImageData *> patch_data;
    std::vector<ImageData *>             temp_imdata;

    missing                   = definition_->missing_patch_ != ""
                                    ? ImageLookup(definition_->missing_patch_.c_str(), kImageNamespaceGraphic,
                                                  kImageLookupFont | kImageLookupNull)
                                    : nullptr;
    ImageData *missing_imdata = nullptr;

    if (missing)
    {
        ImageData *tmp_img      = ReadAsEpiBlock((Image *)(missing));
        uint8_t   *what_palette = nullptr;
        if (missing->source_palette_ >= 0)
            what_palette = LoadLumpIntoMemory(missing->source_palette_);
        if (tmp_img->depth_ == 1)
        {
            ImageData *rgb_img = RGBFromPalettised(
                tmp_img, what_palette ? what_palette : (const uint8_t *)&playpal_data[0], missing->opacity_);
            delete tmp_img;
            missing_imdata = rgb_img;
        }
        else
            missing_imdata = tmp_img;
        missing_imdata->offset_x_ = missing->offset_x_;
        missing_imdata->offset_y_ = missing->offset_y_;
        missing_imdata->scale_x_  = missing->scale_x_;
        missing_imdata->scale_y_  = missing->scale_y_;
        if (what_palette)
            delete[] what_palette;
    }

    // First pass, add the images that are good
    for (pat = definition_->patches_; pat; pat = pat->next)
    {
        // patch name
        char pname[40];

        EPI_ASSERT(strlen(pat->patch1.c_str()) < 36);
        strcpy(pname, pat->patch1.c_str());

        for (int ch = pat->char1; ch <= pat->char2; ch++, BumpPatchName(pname))
        {
            int idx = ch - first;
            EPI_ASSERT(0 <= idx && idx < total);

            images[idx] = ImageLookup(pname, kImageNamespaceGraphic, kImageLookupFont | kImageLookupNull);

            if (images[idx])
            {
                ImageData *tmp_img      = ReadAsEpiBlock((Image *)(images[idx]));
                uint8_t   *what_palette = nullptr;
                if (images[idx]->source_palette_ >= 0)
                    what_palette = LoadLumpIntoMemory(images[idx]->source_palette_);
                if (tmp_img->depth_ == 1)
                {
                    ImageData *rgb_img =
                        RGBFromPalettised(tmp_img, what_palette ? what_palette : (const uint8_t *)&playpal_data[0],
                                          images[idx]->opacity_);
                    delete tmp_img;
                    tmp_img = rgb_img;
                }
                tmp_img->offset_x_ = images[idx]->offset_x_;
                tmp_img->offset_y_ = images[idx]->offset_y_;
                tmp_img->scale_x_  = images[idx]->scale_x_;
                tmp_img->scale_y_  = images[idx]->scale_y_;
                patch_data.try_emplace(kCP437UnicodeValues[(uint8_t)ch], tmp_img);
                temp_imdata.push_back(tmp_img);
                if (what_palette)
                    delete[] what_palette;
            }
        }
    }

    // Second pass to try lower->uppercase fallbacks, or failing that add the
    // missing image (if present)
    for (int ch = 0; ch < 256; ch++)
    {
        if (!patch_data.count(kCP437UnicodeValues[(uint8_t)ch]))
        {
            if ('a' <= ch && ch <= 'z' && patch_data.count(kCP437UnicodeValues[(uint8_t)(epi::ToUpperASCII(ch))]))
                patch_data.try_emplace(kCP437UnicodeValues[(uint8_t)ch],
                                       patch_data.at(kCP437UnicodeValues[(uint8_t)(epi::ToUpperASCII(ch))]));
            else if (missing_imdata)
                patch_data.try_emplace(kCP437UnicodeValues[(uint8_t)ch], missing_imdata);
        }
    }

    delete[] images;

    ImageAtlas *atlas = PackImages(patch_data);
    for (auto patch : temp_imdata)
        delete patch;
    delete missing_imdata;
    if (atlas)
    {
        // Uncomment this to save the generated atlas. Note: will be inverted.
        /*std::string atlas_png = epi::PathAppend(home_directory,
        epi::StringFormat("atlas_%s.png", definition_->name.c_str())); if
        (epi::FileExists(atlas_png)) epi::FS_Remove(atlas_png);
        SavePNG(atlas_png, atlas->data);*/
        patch_font_cache_.atlas_rectangles = atlas->rectangles_;
        render_state->GenTextures(1, &patch_font_cache_.atlas_texture_id);
        render_state->BindTexture(patch_font_cache_.atlas_texture_id);
        render_state->TextureMinFilter(GL_NEAREST);
        render_state->TextureMagFilter(GL_NEAREST);
        render_state->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlas->data_->width_, atlas->data_->height_, 0, GL_RGBA,
                                 GL_UNSIGNED_BYTE, atlas->data_->pixels_);
        render_state->FinishTextures(1, &patch_font_cache_.atlas_texture_id);

        render_state->GenTextures(1, &patch_font_cache_.atlas_smoothed_texture_id);
        render_state->BindTexture(patch_font_cache_.atlas_smoothed_texture_id);
        render_state->TextureMinFilter(GL_LINEAR);
        render_state->TextureMagFilter(GL_LINEAR);
        render_state->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlas->data_->width_, atlas->data_->height_, 0, GL_RGBA,
                                 GL_UNSIGNED_BYTE, atlas->data_->pixels_);
        render_state->FinishTextures(1, &patch_font_cache_.atlas_smoothed_texture_id);

        atlas->data_->Whiten();
        render_state->GenTextures(1, &patch_font_cache_.atlas_whitened_texture_id);
        render_state->BindTexture(patch_font_cache_.atlas_whitened_texture_id);
        render_state->TextureMinFilter(GL_NEAREST);
        render_state->TextureMagFilter(GL_NEAREST);
        render_state->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlas->data_->width_, atlas->data_->height_, 0, GL_RGBA,
                                 GL_UNSIGNED_BYTE, atlas->data_->pixels_);
        render_state->FinishTextures(1, &patch_font_cache_.atlas_whitened_texture_id);

        render_state->GenTextures(1, &patch_font_cache_.atlas_whitened_smoothed_texture_id);
        render_state->BindTexture(patch_font_cache_.atlas_whitened_smoothed_texture_id);
        render_state->TextureMinFilter(GL_LINEAR);
        render_state->TextureMagFilter(GL_LINEAR);
        render_state->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlas->data_->width_, atlas->data_->height_, 0, GL_RGBA,
                                 GL_UNSIGNED_BYTE, atlas->data_->pixels_);
        render_state->FinishTextures(1, &patch_font_cache_.atlas_whitened_smoothed_texture_id);
        delete atlas;
    }
    else
        FatalError("Failed to create atlas for patch font %s!\n", definition_->name_.c_str());

    if (patch_font_cache_.atlas_rectangles.empty())
    {
        LogWarning("Font [%s] has no loaded patches !\n", definition_->name_.c_str());
        patch_font_cache_.width = patch_font_cache_.height = 7;
        return;
    }

    ImageAtlasRectangle Nom;

    if (patch_font_cache_.atlas_rectangles.count(kCP437UnicodeValues[(uint8_t)('M')]))
        Nom = patch_font_cache_.atlas_rectangles.at(kCP437UnicodeValues[(uint8_t)('M')]);
    else if (patch_font_cache_.atlas_rectangles.count(kCP437UnicodeValues[(uint8_t)('m')]))
        Nom = patch_font_cache_.atlas_rectangles.at(kCP437UnicodeValues[(uint8_t)('m')]);
    else if (patch_font_cache_.atlas_rectangles.count(kCP437UnicodeValues[(uint8_t)('0')]))
        Nom = patch_font_cache_.atlas_rectangles.at(kCP437UnicodeValues[(uint8_t)('0')]);
    else // backup plan: just use first patch found
        Nom = patch_font_cache_.atlas_rectangles.begin()->second;

    if (definition_->default_size_ > 0.0)
    {
        patch_font_cache_.height = definition_->default_size_;
        patch_font_cache_.width  = definition_->default_size_ * (Nom.image_width / Nom.image_height);
        patch_font_cache_.ratio  = patch_font_cache_.width / patch_font_cache_.height;
    }
    else
    {
        patch_font_cache_.width  = Nom.image_width;
        patch_font_cache_.height = Nom.image_height;
        patch_font_cache_.ratio  = Nom.image_width / Nom.image_height;
    }
    spacing_ = definition_->spacing_;
}

TTFFont::TTFFont(FontDefinition *definition)
{
    definition_      = definition;
    truetype_info_   = nullptr;
    truetype_buffer_ = nullptr;
    EPI_CLEAR_MEMORY(truetype_atlas_, stbtt_pack_range *, 3);
    EPI_CLEAR_MEMORY(truetype_kerning_scale_, float, 3);
    EPI_CLEAR_MEMORY(truetype_reference_yshift_, float, 3);
    EPI_CLEAR_MEMORY(truetype_texture_id_, unsigned int, 3);
    EPI_CLEAR_MEMORY(truetype_smoothed_texture_id_, unsigned int, 3);

    if (definition_->truetype_name_.empty())
    {
        FatalError("LoadFontTTF: No TTF file/lump name provided for font %s!", definition_->name_.c_str());
    }

    if (hud_fonts.ttf_buffers.count(definition_->truetype_name_))
        truetype_buffer_ = hud_fonts.ttf_buffers[definition_->truetype_name_];

    if (hud_fonts.ttf_infos.count(definition_->truetype_name_))
        truetype_info_ = hud_fonts.ttf_infos[definition_->truetype_name_];

    if (!truetype_buffer_)
    {
        epi::File *F;

        if (!epi::GetExtension(definition_->truetype_name_).empty()) // check for pack file
            F = OpenFileFromPack(definition_->truetype_name_);
        else
            F = LoadLumpAsFile(CheckLumpNumberForName(definition_->truetype_name_.c_str()));

        if (!F)
            FatalError("LoadFontTTF: '%s' not found for font %s.\n", definition_->truetype_name_.c_str(),
                       definition_->name_.c_str());

        truetype_buffer_ = F->LoadIntoMemory();

        hud_fonts.ttf_buffers.try_emplace(definition_->truetype_name_, truetype_buffer_);

        delete F;
    }

    if (!truetype_info_)
    {
        truetype_info_ = new stbtt_fontinfo;
        if (!stbtt_InitFont(truetype_info_, truetype_buffer_, 0))
            FatalError("LoadFontTTF: Could not initialize font %s.\n", definition_->name_.c_str());
        hud_fonts.ttf_infos.try_emplace(definition_->truetype_name_, truetype_info_);
    }

    TrueTypeCharacter ref;

    ref.glyph_index = 0;

    char ch = 0;

    if (stbtt_FindGlyphIndex(truetype_info_, kCP437UnicodeValues[(uint8_t)('M')]) > 0)
    {
        ch              = 'M';
        ref.glyph_index = stbtt_FindGlyphIndex(truetype_info_, kCP437UnicodeValues[(uint8_t)ch]);
    }
    else if (stbtt_FindGlyphIndex(truetype_info_, kCP437UnicodeValues[(uint8_t)('O')]) > 0)
    {
        ch              = 'O';
        ref.glyph_index = stbtt_FindGlyphIndex(truetype_info_, kCP437UnicodeValues[(uint8_t)ch]);
    }
    else if (stbtt_FindGlyphIndex(truetype_info_, kCP437UnicodeValues[(uint8_t)('W')]) > 0)
    {
        ch              = 'W';
        ref.glyph_index = stbtt_FindGlyphIndex(truetype_info_, kCP437UnicodeValues[(uint8_t)ch]);
    }
    else
    {
        for (char c = 32; c < 127; c++)
        {
            if (stbtt_FindGlyphIndex(truetype_info_, kCP437UnicodeValues[(uint8_t)(c)]) > 0)
            {
                ch              = c;
                ref.glyph_index = stbtt_FindGlyphIndex(truetype_info_, kCP437UnicodeValues[(uint8_t)ch]);
                break;
            }
        }
    }

    if (ref.glyph_index == 0)
        FatalError("LoadFontTTF: No suitable characters in font %s.\n", definition_->name_.c_str());

    for (int i = 0; i < 3; i++)
    {
        truetype_atlas_[i]                                   = new stbtt_pack_range;
        truetype_atlas_[i]->first_unicode_codepoint_in_range = 0;
        truetype_atlas_[i]->array_of_unicode_codepoints      = (int *)kCP437UnicodeValues;
        truetype_atlas_[i]->font_size                        = truetype_scaling_font_sizes[i];
        truetype_atlas_[i]->num_chars                        = 256;
        truetype_atlas_[i]->chardata_for_range               = new stbtt_packedchar[256];

        if (definition_->default_size_ == 0.0)
            definition_->default_size_ = 7.0f;

        truetype_kerning_scale_[i] = stbtt_ScaleForPixelHeight(truetype_info_, definition_->default_size_);

        const int32_t bitmap_size = truetype_scaling_bitmap_sizes[i];

        uint8_t *temp_bitmap = new uint8_t[bitmap_size * bitmap_size];

        stbtt_pack_context spc;
        stbtt_PackBegin(&spc, temp_bitmap, bitmap_size, bitmap_size, 0, 1, nullptr);
        stbtt_PackSetOversampling(&spc, 2, 2);
        stbtt_PackFontRanges(&spc, truetype_buffer_, 0, truetype_atlas_[i], 1);
        stbtt_PackEnd(&spc);

        // Convert to RGBA, couldn't get the pack stride to work properly above
        uint8_t *font_bitmap = new uint8_t[bitmap_size * bitmap_size * 4];
        memset(font_bitmap, 255, bitmap_size * bitmap_size * 4);

        uint8_t *src  = temp_bitmap;
        uint8_t *dest = &font_bitmap[3];
        for (int32_t j = 0; j < bitmap_size * bitmap_size; j++, src++, dest += 4)
        {
            *dest = *src;
        }

        render_state->GenTextures(1, &truetype_texture_id_[i]);
        render_state->BindTexture(truetype_texture_id_[i]);
        render_state->TextureMinFilter(GL_NEAREST);
        render_state->TextureMagFilter(GL_NEAREST);
        render_state->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitmap_size, bitmap_size, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                                 font_bitmap);
        render_state->FinishTextures(1, &truetype_texture_id_[i]);

        render_state->GenTextures(1, &truetype_smoothed_texture_id_[i]);
        render_state->BindTexture(truetype_smoothed_texture_id_[i]);
        render_state->TextureMinFilter(GL_LINEAR);
        render_state->TextureMagFilter(GL_LINEAR);
        render_state->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitmap_size, bitmap_size, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                                 font_bitmap);
        render_state->FinishTextures(1, &truetype_smoothed_texture_id_[i]);

        delete[] temp_bitmap;
        delete[] font_bitmap;
        float x       = 0.0f;
        float y       = 0.0f;
        float ascent  = 0.0f;
        float descent = 0.0f;
        float linegap = 0.0f;
        stbtt_GetPackedQuad(truetype_atlas_[i]->chardata_for_range, bitmap_size, bitmap_size, (uint8_t)ch, &x, &y,
                            &ref.character_quad[i], 0);
        stbtt_GetScaledFontVMetrics(truetype_buffer_, 0, truetype_scaling_font_sizes[i], &ascent, &descent, &linegap);
        ref.width[i] = (ref.character_quad[i].x1 - ref.character_quad[i].x0) *
                       (definition_->default_size_ / truetype_scaling_font_sizes[i]);
        ref.height[i] = (ref.character_quad[i].y1 - ref.character_quad[i].y0) *
                        (definition_->default_size_ / truetype_scaling_font_sizes[i]);
        truetype_character_width_[i] = ref.width[i];
        truetype_character_height_[i] =
            (ascent - descent) * (definition_->default_size_ / truetype_scaling_font_sizes[i]);
        ref.y_shift[i] = (truetype_character_height_[i] - ref.height[i]) +
                         (ref.character_quad[i].y1 * (definition_->default_size_ / truetype_scaling_font_sizes[i]));
        truetype_reference_yshift_[i] = ref.y_shift[i];
    }
    truetype_glyph_map_.try_emplace((uint8_t)ch, ref);
    spacing_ = definition_->spacing_ + 0.5; // + 0.5 for at least a minimal buffer
                                            // between letters by default
}

ImageFont::~ImageFont()
{
    if (individual_char_widths_)
    {
        delete[] individual_char_widths_;
        individual_char_widths_ = nullptr;
    }
    if (individual_char_ratios_)
    {
        delete[] individual_char_ratios_;
        individual_char_ratios_ = nullptr;
    }
}

TTFFont::~TTFFont()
{
    for (int i = 0; i < 3; ++i)
    {
        if (truetype_atlas_[i])
        {
            if (truetype_atlas_[i]->chardata_for_range)
                delete[] truetype_atlas_[i]->chardata_for_range;
            delete truetype_atlas_[i];
            truetype_atlas_[i] = nullptr;
        }
    }
}

void PatchFont::BumpPatchName(char *name)
{
    // loops to increment the 10s (100s, etc) digit
    for (char *s = name + strlen(name) - 1; s >= name; s--)
    {
        // only handle digits and letters
        if (!epi::IsAlphanumericASCII(*s))
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

float ImageFont::NominalWidth() const
{
    return image_character_width_ + spacing_;
}

float PatchFont::NominalWidth() const
{
    return patch_font_cache_.width + spacing_;
}

float TTFFont::NominalWidth() const
{
    return truetype_character_width_[current_font_size] + spacing_;
}

float ImageFont::NominalHeight() const
{
    return image_character_height_;
}

float PatchFont::NominalHeight() const
{
    return patch_font_cache_.height;
}

float TTFFont::NominalHeight() const
{
    return truetype_character_height_[current_font_size];
}

bool PatchFont::HasChar(char ch) const
{
    if (ch == ' ')
        return false;
    else
        return patch_font_cache_.atlas_rectangles.count(kCP437UnicodeValues[(uint8_t)ch]) != 0;
}

bool TTFFont::HasChar(char ch) const
{
    return truetype_glyph_map_.find((uint8_t)ch) != truetype_glyph_map_.end();
}

float ImageFont::CharRatio(char ch)
{
    if (ch == ' ')
        return 0.4f;
    else
        return individual_char_ratios_[(uint8_t)ch];
}

//
// Returns the width of the IBM cp437 char in the font.
//
float ImageFont::CharWidth(char ch)
{
    if (ch == ' ')
        return image_character_width_ * 2 / 5 + spacing_;
    else
        return individual_char_widths_[(uint8_t)ch] + spacing_;
}

float PatchFont::CharWidth(char ch)
{
    if (ch == ' ')
        return patch_font_cache_.width * 3 / 5 + spacing_;

    if (!patch_font_cache_.atlas_rectangles.count(kCP437UnicodeValues[(uint8_t)ch]))
        return kDummyCharacterWidth;

    ImageAtlasRectangle rect = patch_font_cache_.atlas_rectangles.at(kCP437UnicodeValues[(uint8_t)ch]);

    if (definition_->default_size_ > 0.0)
        return (definition_->default_size_ * ((float)rect.image_width) / rect.image_height) + spacing_;
    else
        return rect.image_width + spacing_;
}

float TTFFont::CharWidth(char ch)
{
    auto find_glyph = truetype_glyph_map_.find((uint8_t)ch);
    if (find_glyph != truetype_glyph_map_.end())
        return (find_glyph->second.width[current_font_size] + spacing_) * pixel_aspect_ratio.f_;
    else
    {
        TrueTypeCharacter character;
        for (int i = 0; i < 3; i++)
        {
            float x = 0.0f;
            float y = 0.0f;
            stbtt_GetPackedQuad(truetype_atlas_[i]->chardata_for_range, truetype_scaling_bitmap_sizes[i],
                                truetype_scaling_bitmap_sizes[i], (uint8_t)ch, &x, &y, &character.character_quad[i], 0);
            if (ch == ' ')
                character.width[i] = truetype_character_width_[i] * 3 / 5;
            else
                character.width[i] = (character.character_quad[i].x1 - character.character_quad[i].x0) *
                                     (definition_->default_size_ / truetype_scaling_font_sizes[i]);
            character.height[i] = (character.character_quad[i].y1 - character.character_quad[i].y0) *
                                  (definition_->default_size_ / truetype_scaling_font_sizes[i]);
            character.y_shift[i] =
                (truetype_character_height_[i] - character.height[i]) +
                (character.character_quad[i].y1 * (definition_->default_size_ / truetype_scaling_font_sizes[i]));
        }
        character.glyph_index = stbtt_FindGlyphIndex(truetype_info_, kCP437UnicodeValues[(uint8_t)ch]);
        truetype_glyph_map_.try_emplace((uint8_t)ch, character);
        return (character.width[current_font_size] + spacing_) * pixel_aspect_ratio.f_;
    }
}

//
// Get glyph index for TTF Character. If character hasn't been cached yet, cache
// it.
//
int TTFFont::GetGlyphIndex(char ch)
{
    auto find_glyph = truetype_glyph_map_.find((uint8_t)ch);
    if (find_glyph != truetype_glyph_map_.end())
        return find_glyph->second.glyph_index;
    else
    {
        TrueTypeCharacter character;
        for (int i = 0; i < 3; i++)
        {
            float x = 0.0f;
            float y = 0.0f;
            stbtt_GetPackedQuad(truetype_atlas_[i]->chardata_for_range, truetype_scaling_bitmap_sizes[i],
                                truetype_scaling_bitmap_sizes[i], (uint8_t)ch, &x, &y, &character.character_quad[i], 0);
            if (ch == ' ')
                character.width[i] = truetype_character_width_[i] * 3 / 5;
            else
                character.width[i] = (character.character_quad[i].x1 - character.character_quad[i].x0) *
                                     (definition_->default_size_ / truetype_scaling_font_sizes[i]);
            character.height[i] = (character.character_quad[i].y1 - character.character_quad[i].y0) *
                                  (definition_->default_size_ / truetype_scaling_font_sizes[i]);
            character.y_shift[i] =
                (truetype_character_height_[i] - character.height[i]) +
                (character.character_quad[i].y1 * (definition_->default_size_ / truetype_scaling_font_sizes[i]));
        }
        character.glyph_index = stbtt_FindGlyphIndex(truetype_info_, kCP437UnicodeValues[(uint8_t)ch]);
        truetype_glyph_map_.try_emplace((uint8_t)ch, character);
        return character.glyph_index;
    }
}

float TTFFont::GetYShift()
{
    return truetype_reference_yshift_[current_font_size];
}

//
// Find string width from hu_font chars.  The string may not contain
// any newline characters.
//
float ImageFont::StringWidth(const char *str)
{
    float w = 0;

    if (!str)
        return w;

    std::string_view width_check(str);

    for (size_t i = 0; i < width_check.size(); i++)
    {
        w += CharWidth(width_check[i]);
    }

    return w;
}

float PatchFont::StringWidth(const char *str)
{
    float w = 0;

    if (!str)
        return w;

    std::string_view width_check(str);

    for (size_t i = 0; i < width_check.size(); i++)
    {
        w += CharWidth(width_check[i]);
    }

    return w;
}

// The only spots I have used these two functions in have already
// performed the appropriate HasChar(ch) check; be careful to
// do this if you need it elsewhere! - Dasho

float PatchFont::GetCharXOffset(char ch)
{
    return patch_font_cache_.atlas_rectangles[ch].offset_x;
}

float PatchFont::GetCharYOffset(char ch)
{
    return patch_font_cache_.atlas_rectangles[ch].offset_y;
}

float TTFFont::StringWidth(const char *str)
{
    float w = 0;

    if (!str)
        return w;

    std::string_view width_check(str);

    for (size_t i = 0; i < width_check.size(); i++)
    {
        w += CharWidth(width_check[i]);
        if (i + 1 < width_check.size())
            w += stbtt_GetGlyphKernAdvance(truetype_info_, GetGlyphIndex(width_check[i]),
                                           GetGlyphIndex(width_check[i + 1])) *
                 truetype_kerning_scale_[current_font_size];
    }

    return w;
}

//
// Find number of lines in string.
//
int StringLines(std::string_view str)
{
    int                         slines = 1;
    std::string_view::size_type oldpos = 0;
    std::string_view::size_type pos    = 0;

    while (pos != std::string_view::npos)
    {
        pos = str.find('\n', oldpos);
        if (pos != std::string_view::npos)
        {
            slines++;
            oldpos = pos + 1;
        }
    }

    return slines;
}

//----------------------------------------------------------------------------
//  FontContainer class
//----------------------------------------------------------------------------

// Never returns nullptr.
//
Font *FontContainer::Lookup(FontDefinition *definition)
{
    EPI_ASSERT(definition);

    for (std::vector<Font *>::iterator iter = begin(), iter_end = end(); iter != iter_end; iter++)
    {
        Font *f = *iter;

        if (definition == f->definition_)
            return f;
    }

    Font *new_f = nullptr;

    switch (definition->type_)
    {
    case kFontTypePatch:
        new_f = new PatchFont(definition);
        break;

    case kFontTypeImage:
        new_f = new ImageFont(definition);
        break;

    case kFontTypeTrueType:
        new_f = new TTFFont(definition);
        break;

    default:
        FatalError("FontContainer::Lookup, unknown font type for %s\n", definition->name_.c_str());
    }

    push_back(new_f);

    return new_f;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
