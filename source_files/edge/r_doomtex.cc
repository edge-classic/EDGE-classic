//----------------------------------------------------------------------------
//  EDGE Generalised Image Handling
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2024 The EDGE Team.
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
//
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------
//
// -AJA- 2000/06/25: Began this image generalisation, based on Erik
//       Sandberg's w_textur.c/h code.
//
// TODO HERE:
//   -  faster search methods.
//   -  do some optimisation
//

#include <limits.h>

#include "dm_state.h"
#include "e_main.h"
#include "e_search.h"
#include "epi.h"
#include "epi_doomdefs.h"
#include "epi_endian.h"
#include "epi_file.h"
#include "epi_filesystem.h"
#include "i_defs_gl.h"
#include "im_data.h"
#include "im_filter.h"
#include "im_funcs.h"
#include "m_argv.h"
#include "m_misc.h"
#include "p_local.h"
#include "r_colormap.h"
#include "r_defs.h"
#include "r_gldefs.h"
#include "r_image.h"
#include "r_sky.h"
#include "r_texgl.h"
#include "w_files.h"
#include "w_texture.h"
#include "w_wad.h"

// posts are runs of non masked source pixels
struct TexturePost
{
    // -1 is the last post in a column
    uint8_t top_delta;

    // length data bytes follows
    uint8_t length; // length data bytes follows
};

// Dummy image, for when texture/flat/graphic is unknown.  Row major
// order.  Could be packed, but why bother ?
static constexpr uint8_t dummy_graphic[kDummyImageSize * kDummyImageSize] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

//
//  UTILITY
//
static void DrawColumnIntoEpiBlock(const Image *rim, const ImageData *img, const TexturePost *patchcol, int x, int y)
{
    EPI_ASSERT(patchcol);

    int w1 = rim->actual_width_;
    int h1 = rim->actual_height_;
    int w2 = rim->total_width_;

    // clip horizontally
    if (x < 0 || x >= w1)
        return;

    int top = -1;

    while (patchcol->top_delta != 0xFF)
    {
        int delta = patchcol->top_delta;
        int count = patchcol->length;

        const uint8_t *src  = (const uint8_t *)patchcol + 3;
        uint8_t       *dest = img->pixels_ + x;

        // logic for DeePsea's tall patches
        if (delta <= top)
        {
            top += delta;
        }
        else
        {
            top = delta;
        }

        for (int i = 0; i < count; i++, src++)
        {
            int y2 = y + top + i;

            if (y2 < 0 || y2 >= h1)
                continue;

            if (*src == kTransparentPixelIndex)
                dest[(h1 - 1 - y2) * w2] = playpal_black;
            else
                dest[(h1 - 1 - y2) * w2] = *src;
        }

        // jump to next column
        patchcol = (const TexturePost *)((const uint8_t *)patchcol + patchcol->length + 4);
    }
}

//------------------------------------------------------------------------

//
//  BLOCK READING STUFF
//

//
// ReadFlatAsBlock
//
// Loads a flat from the wad and returns the image block for it.
// Doesn't do any mipmapping (this is too "raw" if you follow).
//
static ImageData *ReadFlatAsEpiBlock(Image *rim)
{
    EPI_ASSERT(rim->source_type_ == kImageSourceFlat || rim->source_type_ == kImageSourceRawBlock);

    int tw = HMM_MAX(rim->total_width_, 1);
    int th = HMM_MAX(rim->total_height_, 1);

    int w = rim->actual_width_;
    int h = rim->actual_height_;

    ImageData *img = new ImageData(tw, th, 1);

    uint8_t *dest = img->pixels_;

    // clear initial image to black
    img->Clear(playpal_black);

    const uint8_t *src = nullptr;

    if (rim->source_.graphic.packfile_name)
    {
        epi::File *f = OpenFileFromPack(rim->source_.graphic.packfile_name);
        if (f)
            src = (const uint8_t *)f->LoadIntoMemory();
        delete f;
    }
    else
        src = (const uint8_t *)LoadLumpIntoMemory(rim->source_.flat.lump);

    if (!src)
        FatalError("ReadFlatAsEpiBlock: Failed to load %s!\n", rim->name_.c_str());

    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            uint8_t src_pix = src[y * w + x];

            uint8_t *dest_pix = &dest[(h - 1 - y) * tw + x];

            // make sure kTransparentPixelIndex values (which do not occur
            // naturally in Doom images) are properly remapped.
            if (src_pix == kTransparentPixelIndex)
                dest_pix[0] = playpal_black;
            else
                dest_pix[0] = src_pix;
        }

    delete[] src;

    // CW: Textures MUST tile! If actual size not total size, manually tile
    // [ AJA: this does not make them tile, just fills in the black gaps ]
    img->FillMarginX(rim->actual_width_);
    img->FillMarginY(rim->actual_height_);

    return img;
}

//
// ReadTextureAsBlock
//
// Loads a texture from the wad and returns the image block for it.
// Doesn't do any mipmapping (this is too "raw" if you follow).
//
//---- This routine will also update the `solid' flag
//---- if texture turns out to be solid.
//
static ImageData *ReadTextureAsEpiBlock(Image *rim)
{
    EPI_ASSERT(rim->source_type_ == kImageSourceTexture);

    TextureDefinition *tdef = rim->source_.texture.tdef;
    EPI_ASSERT(tdef);

    int tw = rim->total_width_;
    int th = rim->total_height_;

    ImageData *img = new ImageData(tw, th, 1);

    // Clear initial pixels to either totally transparent, or totally
    // black (if we know the image should be solid).
    //
    //---- If the image turns
    //---- out to be solid instead of transparent, the transparent pixels
    //---- will be blackened.

    if (rim->opacity_ == kOpacitySolid)
        img->Clear(playpal_black);
    else
        img->Clear(kTransparentPixelIndex);

    int           i;
    TexturePatch *patch;

    // Composite the columns into the block.
    for (i = 0, patch = tdef->patches; i < tdef->patch_count; i++, patch++)
    {
        const Patch *realpatch = (const Patch *)LoadLumpIntoMemory(patch->patch);

        int realsize = GetLumpLength(patch->patch);

        int x1 = patch->origin_x;
        int y1 = patch->origin_y;
        int x2 = x1 + AlignedLittleEndianS16(realpatch->width);

        int x = HMM_MAX(0, x1);

        x2 = HMM_MIN(tdef->width, x2);

        for (; x < x2; x++)
        {
            int offset = AlignedLittleEndianS32(realpatch->column_offset[x - x1]);

            if (offset < 0)
                FatalError("Negative image offset 0x%08x in image [%s]\n", offset, rim->name_.c_str());

            if (offset >= realsize)
                FatalError("Excessive image offset 0x%08x in image [%s]\n", offset, rim->name_.c_str());

            const TexturePost *patchcol = (const TexturePost *)((const uint8_t *)realpatch + offset);

            DrawColumnIntoEpiBlock(rim, img, patchcol, x, y1);
        }

        delete[] realpatch;
    }

    // CW: Textures MUST tile! If actual size not total size, manually tile
    // [ AJA: this does not make them tile, just fills in the black gaps ]
    img->FillMarginX(rim->actual_width_);
    img->FillMarginY(rim->actual_height_);

    return img;
}

//
// ReadPatchAsBlock
//
// Loads a patch from the wad and returns the image block for it.
// Very similiar to ReadTextureAsBlock() above.  Doesn't do any
// mipmapping (this is too "raw" if you follow).
//
//---- This routine will also update the `solid' flag
//---- if it turns out to be 100% solid.
//
static ImageData *ReadPatchAsEpiBlock(Image *rim)
{
    EPI_ASSERT(rim->source_type_ == kImageSourceGraphic || rim->source_type_ == kImageSourceSprite ||
               rim->source_type_ == kImageSourceTXHI);

    int         lump          = rim->source_.graphic.lump;
    const char *packfile_name = rim->source_.graphic.packfile_name;

    // handle PNG/JPEG/TGA images
    if (!rim->source_.graphic.is_patch)
    {
        epi::File *f;

        if (packfile_name)
            f = OpenFileFromPack(packfile_name);
        else
            f = LoadLumpAsFile(lump);

        ImageData *img = LoadImageData(f);

        // close it
        delete f;

        if (!img)
            FatalError("Error loading image in lump: %s\n", packfile_name ? packfile_name : GetLumpNameFromIndex(lump));

        // Try and manually tile, or at least fill in the black gaps ]
        img->FillMarginX(rim->actual_width_);
        img->FillMarginY(rim->actual_height_);

        return img;
    }

    int tw = rim->total_width_;
    int th = rim->total_height_;

    ImageData *img = new ImageData(tw, th, 1);

    // Clear initial pixels to either totally transparent, or totally
    // black (if we know the image should be solid).
    //
    //---- If the image turns
    //---- out to be solid instead of transparent, the transparent pixels
    //---- will be blackened.

    if (rim->opacity_ == kOpacitySolid)
        img->Clear(playpal_black);
    else
        img->Clear(kTransparentPixelIndex);

    // Composite the columns into the block.
    const Patch *realpatch = nullptr;
    int          realsize  = 0;

    if (packfile_name)
    {
        epi::File *f = OpenFileFromPack(packfile_name);
        if (f)
        {
            realpatch = (const Patch *)f->LoadIntoMemory();
            realsize  = f->GetLength();
        }
        else
            FatalError("ReadPatchAsEpiBlock: Failed to load %s!\n", packfile_name);
        delete f;
    }
    else
    {
        realpatch = (const Patch *)LoadLumpIntoMemory(lump);
        realsize  = GetLumpLength(lump);
    }

    EPI_ASSERT(realpatch);
    EPI_ASSERT(rim->actual_width_ == AlignedLittleEndianS16(realpatch->width));
    EPI_ASSERT(rim->actual_height_ == AlignedLittleEndianS16(realpatch->height));

    // 2023.11.07 - These were previously left as total_w/h, which accounts
    // for power-of-two sizing and was messing up patch font atlas generation.
    // Not sure if there are any bad side effects yet - Dasho
    img->used_width_  = rim->actual_width_;
    img->used_height_ = rim->actual_height_;

    for (int x = 0; x < rim->actual_width_; x++)
    {
        int offset = AlignedLittleEndianS32(realpatch->column_offset[x]);

        if (offset < 0 || offset >= realsize)
            FatalError("Bad image offset 0x%08x in image [%s]\n", offset, rim->name_.c_str());

        const TexturePost *patchcol = (const TexturePost *)((const uint8_t *)realpatch + offset);

        DrawColumnIntoEpiBlock(rim, img, patchcol, x, 0);
    }

    delete[] realpatch;

    return img;
}

//
// ReadDummyAsBlock
//
// Creates a dummy image.
//
static ImageData *ReadDummyAsEpiBlock(Image *rim)
{
    EPI_ASSERT(rim->source_type_ == kImageSourceDummy);
    EPI_ASSERT(rim->actual_width_ == rim->total_width_);
    EPI_ASSERT(rim->actual_height_ == rim->total_height_);
    EPI_ASSERT(rim->total_width_ == kDummyImageSize);
    EPI_ASSERT(rim->total_height_ == kDummyImageSize);

    ImageData *img = new ImageData(kDummyImageSize, kDummyImageSize, 4);

    // copy pixels
    for (int y = 0; y < kDummyImageSize; y++)
        for (int x = 0; x < kDummyImageSize; x++)
        {
            uint8_t *dest_pix = img->PixelAt(x, y);

            if (dummy_graphic[(kDummyImageSize - 1 - y) * kDummyImageSize + x])
            {
                *dest_pix++ = (rim->source_.dummy.fg & 0xFF0000) >> 16;
                *dest_pix++ = (rim->source_.dummy.fg & 0x00FF00) >> 8;
                *dest_pix++ = (rim->source_.dummy.fg & 0x0000FF);
                *dest_pix++ = 255;
            }
            else if (rim->source_.dummy.bg == kTransparentPixelIndex)
            {
                *dest_pix++ = 0;
                *dest_pix++ = 0;
                *dest_pix++ = 0;
                *dest_pix++ = 0;
            }
            else
            {
                *dest_pix++ = (rim->source_.dummy.bg & 0xFF0000) >> 16;
                *dest_pix++ = (rim->source_.dummy.bg & 0x00FF00) >> 8;
                *dest_pix++ = (rim->source_.dummy.bg & 0x0000FF);
                *dest_pix++ = 255;
            }
        }

    return img;
}

static ImageData *CreateUserColourImage(Image *rim, ImageDefinition *def)
{
    int tw = HMM_MAX(rim->total_width_, 1);
    int th = HMM_MAX(rim->total_height_, 1);

    ImageData *img = new ImageData(tw, th, 3);

    uint8_t *dest = img->pixels_;

    for (int y = 0; y < img->height_; y++)
        for (int x = 0; x < img->width_; x++)
        {
            *dest++ = epi::GetRGBARed(def->colour_);
            *dest++ = epi::GetRGBAGreen(def->colour_);
            *dest++ = epi::GetRGBABlue(def->colour_);
        }

    return img;
}

epi::File *OpenUserFileOrLump(ImageDefinition *def)
{
    switch (def->type_)
    {
    case kImageDataFile: {
        // -AJA- 2005/01/15: filenames in DDF relative to APPDIR
        std::string data_file = epi::PathAppendIfNotAbsolute(game_directory, def->info_);
        return epi::FileOpen(data_file, epi::kFileAccessRead | epi::kFileAccessBinary);
    }

    case kImageDataPackage:
        return OpenFileFromPack(def->info_);

    case kImageDataLump: {
        int lump = CheckLumpNumberForName(def->info_.c_str());
        if (lump < 0)
            return nullptr;

        return LoadLumpAsFile(lump);
    }

    default:
        return nullptr;
    }
}

static ImageData *CreateUserFileImage(Image *rim, ImageDefinition *def)
{
    epi::File *f = OpenUserFileOrLump(def);

    if (!f)
        FatalError("Missing image file: %s\n", def->info_.c_str());

    ImageData *img = LoadImageData(f);

    // close it
    delete f;

    if (!img)
        FatalError("Error occurred loading image file: %s\n", def->info_.c_str());

    rim->opacity_ = DetermineOpacity(img, &rim->is_empty_);

    if (def->is_font_)
        return img;

    if (def->fix_trans_ == kTransparencyFixBlacken)
        BlackenClearAreas(img);

    EPI_ASSERT(rim->total_width_ == img->width_);
    EPI_ASSERT(rim->total_height_ == img->height_);

    // CW: Textures MUST tile! If actual size not total size, manually tile
    // [ AJA: this does not make them tile, just fills in the black gaps ]
    if (rim->opacity_ == kOpacitySolid)
    {
        img->FillMarginX(rim->actual_width_);
        img->FillMarginY(rim->actual_height_);
    }

    return img;
}

//
// ReadUserAsEpiBlock
//
// Loads or Creates the user defined image.
// Doesn't do any mipmapping (this is too "raw" if you follow).
//
static ImageData *ReadUserAsEpiBlock(Image *rim)
{
    EPI_ASSERT(rim->source_type_ == kImageSourceUser);

    ImageDefinition *def = rim->source_.user.def;

    switch (def->type_)
    {
    case kImageDataColor:
        return CreateUserColourImage(rim, def);

    case kImageDataFile:
    case kImageDataLump:
    case kImageDataPackage:
        return CreateUserFileImage(rim, def);

    default:
        FatalError("ReadUserAsEpiBlock: Coding error, unknown type %d\n", def->type_);
    }
}

//
// ReadAsEpiBlock
//
// Read the image from the wad into an image_data_c class.
// The image returned is normally palettised (bpp == 1), and the
// palette must be determined from rim->source_palette_.  Mainly
// just a switch to more specialised image readers.
//
// Never returns nullptr.
//
ImageData *ReadAsEpiBlock(Image *rim)
{
    switch (rim->source_type_)
    {
    case kImageSourceFlat:
    case kImageSourceRawBlock:
        return ReadFlatAsEpiBlock(rim);

    case kImageSourceTexture:
        return ReadTextureAsEpiBlock(rim);

    case kImageSourceGraphic:
    case kImageSourceSprite:
    case kImageSourceTXHI:
        return ReadPatchAsEpiBlock(rim);

    case kImageSourceDummy:
        return ReadDummyAsEpiBlock(rim);

    case kImageSourceUser:
        return ReadUserAsEpiBlock(rim);

    default:
        FatalError("ReadAsBlock: unknown source_type %d !\n", rim->source_type_);
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
