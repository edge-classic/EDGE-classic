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

#include "r_image.h"

#include <limits.h>

#include <list>

#include "ddf_flat.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "e_main.h"
#include "e_search.h"
#include "epi.h"
#include "epi_doomdefs.h"
#include "epi_endian.h"
#include "epi_file.h"
#include "epi_filesystem.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"
#include "hu_draw.h" // hud_tic
#include "i_defs_gl.h"
#include "i_system.h"
#include "im_data.h"
#include "im_filter.h"
#include "im_funcs.h"
#include "m_argv.h"
#include "m_misc.h"
#include "p_local.h"
#include "r_colormap.h"
#include "r_defs.h"
#include "r_gldefs.h"
#include "r_misc.h"
#include "r_sky.h"
#include "r_texgl.h"
#include "w_epk.h"
#include "w_files.h"
#include "w_texture.h"
#include "w_wad.h"

LiquidSwirl swirling_flats = kLiquidSwirlVanilla;

extern ImageData *ReadAsEpiBlock(Image *rim);

extern epi::File *OpenUserFileOrLump(ImageDefinition *def);

extern void DeleteSkyTextures(void);
extern void DeleteColourmapTextures(void);

extern bool erraticism_active;

//
// This structure is for "cached" images (i.e. ready to be used for
// rendering), and is the non-opaque version of CachedImage.  A
// single structure is used for all image modes (Block and OGL).
//
// Note: multiple modes and/or multiple mips of the same image_c can
// indeed be present in the cache list at any one time.
//
struct CachedImage
{
    // parent image
    Image *parent;

    // colormap used for translated image, normally nullptr
    const Colormap *translation_map;

    // general hue of image (skewed towards pure colors)
    RGBAColor hue;

    // texture identifier within GL
    GLuint texture_id;

    bool is_whitened;
};

Image *ImageContainerLookup(std::list<Image *> &bucket, const char *name, int source_type
                            /* use -2 to prevent USER override */)
{
    // for a normal lookup, we want USER images to override
    if (source_type == -1)
    {
        Image *rim = ImageContainerLookup(bucket, name, kImageSourceUser); // recursion
        if (rim)
            return rim;
    }

    std::list<Image *>::reverse_iterator it;

    // search backwards, we want newer image to override older ones
    for (it = bucket.rbegin(); it != bucket.rend(); it++)
    {
        Image *rim = *it;

        if (source_type >= 0 && source_type != (int)rim->source_type_)
            continue;

        if (epi::StringCaseCompareASCII(name, rim->name_) == 0)
            return rim;
    }

    return nullptr; // not found
}

static void do_Animate(std::list<Image *> &bucket)
{
    std::list<Image *>::iterator it;

    for (it = bucket.begin(); it != bucket.end(); it++)
    {
        Image *rim = *it;

        if (rim->animation_.speed == 0) // not animated ?
            continue;

        if (rim->liquid_type_ > kLiquidImageNone && swirling_flats > kLiquidSwirlVanilla)
            continue;

        EPI_ASSERT(rim->animation_.count > 0);

        rim->animation_.count--;

        if (rim->animation_.count == 0 && rim->animation_.current->animation_.next)
        {
            rim->animation_.current = rim->animation_.current->animation_.next;
            rim->animation_.count   = rim->animation_.speed;
        }
    }
}

// mipmapping enabled ?
// 0 off, 1 bilinear, 2 trilinear
int image_mipmapping = 2;

int image_smoothing = 0;

int hq2x_scaling = 0;

// total set of images
std::list<Image *> real_graphics;
std::list<Image *> real_textures;
std::list<Image *> real_flats;
std::list<Image *> real_sprites;

std::vector<std::string> TX_names;

const Image *sky_flat_image;

static const Image *dummy_sprite;
static const Image *dummy_skin;
static const Image *dummy_hom[2];

// image cache (actually a ring structure)
static std::list<CachedImage *> image_cache;

//----------------------------------------------------------------------------
//
//  IMAGE CREATION
//

Image::Image()
    : actual_width_(0), actual_height_(0), total_width_(0), total_height_(0), width_ratio_(0.0), height_ratio_(0.0),
      source_type_(kImageSourceDummy), source_palette_(-1), cache_()
{
    name_ = "_UNINIT_";

    memset(&source_, 0, sizeof(source_));
    memset(&animation_, 0, sizeof(animation_));
}

Image::~Image()
{ /* TODO: image_c destructor */
}

void StoreBlurredImage(const Image *image)
{
    // const override
    Image *img = (Image *)image;
    if (!img->blurred_version_)
    {
        img->blurred_version_                     = new Image;
        img->blurred_version_->name_              = std::string(img->name_).append("_BLURRED");
        img->blurred_version_->actual_height_     = img->actual_height_;
        img->blurred_version_->actual_width_      = img->actual_width_;
        img->blurred_version_->is_empty_          = img->is_empty_;
        img->blurred_version_->is_font_           = img->is_font_;
        img->blurred_version_->liquid_type_       = img->liquid_type_;
        img->blurred_version_->offset_x_          = img->offset_x_;
        img->blurred_version_->offset_y_          = img->offset_y_;
        img->blurred_version_->opacity_           = img->opacity_;
        img->blurred_version_->height_ratio_      = img->height_ratio_;
        img->blurred_version_->width_ratio_       = img->width_ratio_;
        img->blurred_version_->scale_x_           = img->scale_x_;
        img->blurred_version_->scale_y_           = img->scale_y_;
        img->blurred_version_->source_            = img->source_;
        img->blurred_version_->source_palette_    = img->source_palette_;
        img->blurred_version_->source_type_       = img->source_type_;
        img->blurred_version_->total_height_      = img->total_height_;
        img->blurred_version_->total_width_       = img->total_width_;
        img->blurred_version_->animation_.current = img->blurred_version_;
        img->blurred_version_->animation_.next    = nullptr;
        img->blurred_version_->animation_.count   = 0;
        img->blurred_version_->animation_.speed   = 0;
        img->blurred_version_->grayscale_         = img->grayscale_;
        if (img->blur_sigma_ > 0.0f)
        {
            img->blurred_version_->blur_sigma_ = img->blur_sigma_;
        }
        else
        {
            img->blurred_version_->blur_sigma_ = -1.0f;
        }
    }
}

static Image *NewImage(int width, int height, int opacity = kOpacityUnknown)
{
    Image *rim = new Image;

    rim->actual_width_  = width;
    rim->actual_height_ = height;
    rim->total_width_   = MakeValidTextureSize(width);
    rim->total_height_  = MakeValidTextureSize(height);
    rim->width_ratio_   = (float)width / (float)rim->total_width_ * 0.0625;
    rim->height_ratio_  = (float)height / (float)rim->total_height_ * 0.0625;
    rim->offset_x_ = rim->offset_y_ = 0;
    rim->scale_x_ = rim->scale_y_ = 1.0f;
    rim->opacity_                 = opacity;
    rim->is_empty_                = false;
    rim->is_font_                 = false;

    // set initial animation info
    rim->animation_.current = rim;
    rim->animation_.next    = nullptr;
    rim->animation_.count = rim->animation_.speed = 0;

    rim->liquid_type_ = kLiquidImageNone;

    rim->swirled_game_tic_ = 0;

    return rim;
}

static Image *CreateDummyImage(const char *name, RGBAColor fg, RGBAColor bg)
{
    Image *rim;

    rim = NewImage(kDummyImageSize, kDummyImageSize, (bg == kTransparentPixelIndex) ? kOpacityMasked : kOpacitySolid);

    rim->name_ = name;

    rim->source_type_    = kImageSourceDummy;
    rim->source_palette_ = -1;

    rim->source_.dummy.fg = fg;
    rim->source_.dummy.bg = bg;

    return rim;
}

Image *AddPackImageSmart(const char *name, ImageSource type, const char *packfile_name, std::list<Image *> &container,
                         const Image *replaces)
{
    /* used for Graphics, Sprites and TX/HI stuff */
    epi::File *f = OpenFileFromPack(packfile_name);
    EPI_ASSERT(f);
    int packfile_len = f->GetLength();

    // determine format and size information
    uint8_t header[32];
    memset(header, 255, sizeof(header));

    f->Read(header, sizeof(header));
    f->Seek(0, epi::File::kSeekpointStart);

    int width = 0, height = 0, bpp = 0;
    int offset_x = 0, offset_y = 0;

    bool is_patch = false;
    bool solid    = false;

    int  header_len = HMM_MIN((int)sizeof(header), packfile_len);
    auto fmt        = DetectImageFormat(header, header_len, packfile_len);

    if (fmt == kImageOther)
    {
        // close it
        delete f;

        LogWarning("Unsupported image format in '%s'\n", packfile_name);
        return nullptr;
    }
    else if (fmt == kImageUnknown)
    {
        // close it
        delete f;

        // check for Heretic images, which are raw 320x200
        if (packfile_len == 320 * 200 && type == kImageSourceGraphic)
        {
            width  = 320;
            height = 200;
            solid  = true;
            type   = kImageSourceRawBlock;
        }
        // check for AUTOPAGE images, which are raw 320x158
        else if (packfile_len == 320 * 158 && type == kImageSourceGraphic)
        {
            width  = 320;
            height = 158;
            solid  = true;
            type   = kImageSourceRawBlock;
        }
        // check for flats
        else if ((packfile_len == 64 * 64 || packfile_len == 64 * 65) && type == kImageSourceGraphic)
        {
            width  = 64;
            height = 64;
            solid  = true;
            type   = kImageSourceFlat;
        }
        else
        {
            LogWarning("Graphic '%s' does not seem to be a graphic.\n", name);
            return nullptr;
        }
    }
    else if (fmt == kImageDoom)
    {
        // close it
        delete f;

        const Patch *pat = (Patch *)header;

        width    = AlignedLittleEndianS16(pat->width);
        height   = AlignedLittleEndianS16(pat->height);
        offset_x = AlignedLittleEndianS16(pat->left_offset);
        offset_y = AlignedLittleEndianS16(pat->top_offset);

        is_patch = true;
    }
    else // PNG, TGA or JPEG
    {
        if (!GetImageInfo(f, &width, &height, &bpp) || width <= 0 || height <= 0)
        {
            LogWarning("Error scanning image in '%s'\n", packfile_name);
            return nullptr;
        }

        solid = (bpp == 3);

        // close it
        delete f;
    }

    // create new image
    Image *rim = NewImage(width, height, solid ? kOpacitySolid : kOpacityUnknown);

    rim->offset_x_ = offset_x;
    rim->offset_y_ = offset_y;

    rim->name_ = name;

    FlatDefinition *current_flatdef = flatdefs.Find(rim->name_.c_str());

    if (current_flatdef && !current_flatdef->liquid_.empty())
    {
        if (epi::StringCaseCompareASCII(current_flatdef->liquid_, "THIN") == 0)
            rim->liquid_type_ = kLiquidImageThin;
        else if (epi::StringCaseCompareASCII(current_flatdef->liquid_, "THICK") == 0)
            rim->liquid_type_ = kLiquidImageThick;
    }

    rim->source_type_                  = type;
    int pn_len                         = strlen(packfile_name);
    rim->source_.graphic.packfile_name = (char *)calloc(pn_len + 1, 1);
    epi::CStringCopyMax(rim->source_.graphic.packfile_name, packfile_name, pn_len);
    rim->source_.graphic.is_patch     = is_patch;
    rim->source_.graphic.user_defined = false; // This should only get set to true with DDFIMAGE specified DOOM
                                               // format images
    rim->source_palette_ = -1;

    if (replaces)
    {
        rim->scale_x_ = replaces->actual_width_ / (float)width;
        rim->scale_y_ = replaces->actual_height_ / (float)height;

        if (!is_patch && replaces->source_type_ == kImageSourceSprite)
        {
            rim->offset_x_ = replaces->offset_x_;
            rim->offset_y_ = replaces->offset_y_;
        }
    }

    container.push_back(rim);

    return rim;
}

static Image *AddImage_Smart(const char *name, ImageSource type, int lump, std::list<Image *> &container,
                             const Image *replaces = nullptr)
{
    /* used for Graphics, Sprites and TX/HI stuff */

    int lump_len = GetLumpLength(lump);

    epi::File *f = LoadLumpAsFile(lump);
    EPI_ASSERT(f);

    // determine format and size information
    uint8_t header[32];
    memset(header, 255, sizeof(header));

    f->Read(header, sizeof(header));
    f->Seek(0, epi::File::kSeekpointStart);

    int width = 0, height = 0, bpp = 0;
    int offset_x = 0, offset_y = 0;

    bool is_patch = false;
    bool solid    = false;

    int  header_len = HMM_MIN((int)sizeof(header), lump_len);
    auto fmt        = DetectImageFormat(header, header_len, lump_len);

    if (fmt == kImageOther)
    {
        // close it
        delete f;

        LogWarning("Unsupported image format in '%s' lump\n", GetLumpNameFromIndex(lump));
        return nullptr;
    }
    else if (fmt == kImageUnknown)
    {
        // close it
        delete f;

        // check for Heretic images, which are raw 320x200
        if (lump_len == 320 * 200 && type == kImageSourceGraphic)
        {
            width  = 320;
            height = 200;
            solid  = true;
            type   = kImageSourceRawBlock;
        }
        // check for AUTOPAGE images, which are raw 320x158
        else if (lump_len == 320 * 158 && type == kImageSourceGraphic)
        {
            width  = 320;
            height = 158;
            solid  = true;
            type   = kImageSourceRawBlock;
        }
        // check for flats
        else if ((lump_len == 64 * 64 || lump_len == 64 * 65) && type == kImageSourceGraphic)
        {
            width  = 64;
            height = 64;
            solid  = true;
            type   = kImageSourceFlat;
        }
        else
        {
            LogWarning("Graphic '%s' does not seem to be a graphic.\n", name);
            return nullptr;
        }
    }
    else if (fmt == kImageDoom)
    {
        // close it
        delete f;

        const Patch *pat = (Patch *)header;

        width    = AlignedLittleEndianS16(pat->width);
        height   = AlignedLittleEndianS16(pat->height);
        offset_x = AlignedLittleEndianS16(pat->left_offset);
        offset_y = AlignedLittleEndianS16(pat->top_offset);

        is_patch = true;
    }
    else // PNG, TGA or JPEG
    {
        if (!GetImageInfo(f, &width, &height, &bpp) || width <= 0 || height <= 0)
        {
            LogWarning("Error scanning image in '%s' lump\n", GetLumpNameFromIndex(lump));
            return nullptr;
        }

        solid = (bpp == 3);

        // close it
        delete f;
    }

    // create new image
    Image *rim = NewImage(width, height, solid ? kOpacitySolid : kOpacityUnknown);

    rim->offset_x_ = offset_x;
    rim->offset_y_ = offset_y;

    rim->name_ = name;

    FlatDefinition *current_flatdef = flatdefs.Find(rim->name_.c_str());

    if (current_flatdef && !current_flatdef->liquid_.empty())
    {
        if (epi::StringCaseCompareASCII(current_flatdef->liquid_, "THIN") == 0)
            rim->liquid_type_ = kLiquidImageThin;
        else if (epi::StringCaseCompareASCII(current_flatdef->liquid_, "THICK") == 0)
            rim->liquid_type_ = kLiquidImageThick;
    }

    rim->source_type_                 = type;
    rim->source_.graphic.lump         = lump;
    rim->source_.graphic.is_patch     = is_patch;
    rim->source_.graphic.user_defined = false; // This should only get set to true with DDFIMAGE specified DOOM
                                               // format images
    rim->source_palette_ = GetPaletteForLump(lump);

    if (replaces)
    {
        rim->scale_x_ = replaces->actual_width_ / (float)width;
        rim->scale_y_ = replaces->actual_height_ / (float)height;

        if (!is_patch && replaces->source_type_ == kImageSourceSprite)
        {
            rim->offset_x_ = replaces->offset_x_;
            rim->offset_y_ = replaces->offset_y_;
        }
    }

    container.push_back(rim);

    return rim;
}

static Image *AddImageTexture(const char *name, TextureDefinition *tdef)
{
    Image *rim;

    rim = NewImage(tdef->width, tdef->height);

    rim->name_ = name;

    if (tdef->scale_x)
        rim->scale_x_ = 8.0 / tdef->scale_x;
    if (tdef->scale_y)
        rim->scale_y_ = 8.0 / tdef->scale_y;

    rim->source_type_         = kImageSourceTexture;
    rim->source_.texture.tdef = tdef;
    rim->source_palette_      = tdef->palette_lump;

    real_textures.push_back(rim);

    return rim;
}

static Image *AddImageFlat(const char *name, int lump)
{
    Image *rim;
    int    len, size;

    len = GetLumpLength(lump);

    switch (len)
    {
    case 64 * 64:
        size = 64;
        break;

    // support for odd-size Heretic flats
    case 64 * 65:
        size = 64;
        break;

    // -- EDGE feature: bigger than normal flats --
    case 128 * 128:
        size = 128;
        break;
    case 256 * 256:
        size = 256;
        break;
    case 512 * 512:
        size = 512;
        break;
    case 1024 * 1024:
        size = 1024;
        break;

    default:
        return nullptr;
    }

    rim = NewImage(size, size, kOpacitySolid);

    rim->name_ = name;

    rim->source_type_      = kImageSourceFlat;
    rim->source_.flat.lump = lump;
    rim->source_palette_   = GetPaletteForLump(lump);

    FlatDefinition *current_flatdef = flatdefs.Find(rim->name_.c_str());

    if (current_flatdef && !current_flatdef->liquid_.empty())
    {
        if (epi::StringCaseCompareASCII(current_flatdef->liquid_, "THIN") == 0)
            rim->liquid_type_ = kLiquidImageThin;
        else if (epi::StringCaseCompareASCII(current_flatdef->liquid_, "THICK") == 0)
            rim->liquid_type_ = kLiquidImageThick;
    }

    real_flats.push_back(rim);

    return rim;
}

static Image *AddImage_DOOM(ImageDefinition *def, bool user_defined = false)
{
    const char *name      = def->name_.c_str();
    const char *lump_name = def->info_.c_str();

    Image *rim = nullptr;

    if (def->type_ == kImageDataPackage)
    {
        switch (def->belong_)
        {
        case kImageNamespaceGraphic:
            rim = AddPackImageSmart(name, kImageSourceGraphic, lump_name, real_graphics);
            break;
        case kImageNamespaceTexture:
            rim = AddPackImageSmart(name, kImageSourceTexture, lump_name, real_textures);
            break;
        case kImageNamespaceFlat:
            rim = AddPackImageSmart(name, kImageSourceFlat, lump_name, real_flats);
            break;
        case kImageNamespaceSprite:
            rim = AddPackImageSmart(name, kImageSourceSprite, lump_name, real_sprites);
            break;

        default:
            FatalError("INTERNAL ERROR: Bad belong value: %d\n", def->belong_);
        }
    }
    else
    {
        switch (def->belong_)
        {
        case kImageNamespaceGraphic:
            rim = AddImage_Smart(name, kImageSourceGraphic, GetLumpNumberForName(lump_name), real_graphics);
            break;
        case kImageNamespaceTexture:
            rim = AddImage_Smart(name, kImageSourceTexture, GetLumpNumberForName(lump_name), real_textures);
            break;
        case kImageNamespaceFlat:
            rim = AddImage_Smart(name, kImageSourceFlat, GetLumpNumberForName(lump_name), real_flats);
            break;
        case kImageNamespaceSprite:
            rim = AddImage_Smart(name, kImageSourceSprite, GetLumpNumberForName(lump_name), real_sprites);
            break;

        default:
            FatalError("INTERNAL ERROR: Bad belong value: %d\n", def->belong_);
        }
    }

    if (rim == nullptr)
    {
        LogWarning("Unable to add image lump: %s\n", lump_name);
        return nullptr;
    }

    rim->offset_x_ += def->x_offset_;
    rim->offset_y_ += def->y_offset_;

    rim->scale_x_ = def->scale_ * def->aspect_;
    rim->scale_y_ = def->scale_;

    rim->is_font_ = def->is_font_;

    rim->hsv_rotation_   = def->hsv_rotation_;
    rim->hsv_saturation_ = def->hsv_saturation_;
    rim->hsv_value_      = def->hsv_value_;
    rim->blur_sigma_     = def->blur_factor_;

    rim->source_.graphic.special = kImageSpecialNone;

    if (user_defined)
    {
        rim->source_.graphic.user_defined = true;
        rim->source_.graphic.special      = def->special_;
    }

    if (def->special_ & kImageSpecialCrosshair)
    {
        float dy = (200.0f - rim->actual_height_ * rim->scale_y_) / 2.0f; // - WEAPONTOP;
        rim->offset_y_ += int(dy / rim->scale_y_);
    }

    if (def->special_ & kImageSpecialGrayscale)
        rim->grayscale_ = true;

    return rim;
}

static Image *AddImageUser(ImageDefinition *def)
{
    int  width = 0, height = 0, bpp = 0;
    bool solid = false;

    if (def->type_ == kImageDataLump && def->format_ == kLumpImageFormatDoom)
        return AddImage_DOOM(def, true);

    switch (def->type_)
    {
    case kImageDataColor:
        width  = 8;
        height = 8;
        bpp    = 3;
        solid  = true;
        break;

    case kImageDataLump:
    case kImageDataFile:
    case kImageDataPackage: {
        const char *filename = def->info_.c_str();

        epi::File *f = OpenUserFileOrLump(def);
        if (f == nullptr)
        {
            LogWarning("Unable to open image %s: %s\n", (def->type_ == kImageDataLump) ? "lump" : "file", filename);
            return nullptr;
        }

        int file_size = f->GetLength();

        // determine format and size information.
        // for FILE and PACK get format from filename, but note that when
        // it is wrong (like a PNG called "foo.jpeg"), it can still work.
        ImageFormat fmt = kImageUnknown;

        if (def->type_ == kImageDataLump)
        {
            uint8_t header[32];
            memset(header, 255, sizeof(header));

            f->Read(header, sizeof(header));
            f->Seek(0, epi::File::kSeekpointStart);

            int header_len = HMM_MIN((int)sizeof(header), file_size);
            fmt            = DetectImageFormat(header, header_len, file_size);
        }
        else
            fmt = ImageFormatFromFilename(def->info_);

        // when a lump uses DOOM patch format, use the other method.
        // for lumps, assume kImageUnknown is a mis-detection of DOOM patch
        // and hope for the best.
        if (fmt == kImageDoom || fmt == kImageUnknown)
        {
            delete f; // close file

            if (fmt == kImageDoom)
                return AddImage_DOOM(def, true);

            LogWarning("Unknown image format in: %s\n", filename);
            return nullptr;
        }

        if (fmt == kImageOther)
        {
            delete f;
            LogWarning("Unsupported image format in: %s\n", filename);
            return nullptr;
        }

        if (!GetImageInfo(f, &width, &height, &bpp))
        {
            delete f;
            LogWarning("Error occurred scanning image: %s\n", filename);
            return nullptr;
        }

        // close it
        delete f;

        solid = (bpp == 3);
    }
    break;

    default:
        FatalError("AddImageUser: Coding error, unknown type %d\n", def->type_);
        return nullptr; /* NOT REACHED */
    }

    Image *rim = NewImage(width, height, solid ? kOpacitySolid : kOpacityUnknown);

    rim->name_ = def->name_;

    rim->offset_x_ = def->x_offset_;
    rim->offset_y_ = def->y_offset_;

    rim->scale_x_ = def->scale_ * def->aspect_;
    rim->scale_y_ = def->scale_;

    rim->source_type_     = kImageSourceUser;
    rim->source_.user.def = def;

    rim->is_font_ = def->is_font_;

    rim->hsv_rotation_   = def->hsv_rotation_;
    rim->hsv_saturation_ = def->hsv_saturation_;
    rim->hsv_value_      = def->hsv_value_;
    rim->blur_sigma_     = def->blur_factor_;

    if (def->special_ & kImageSpecialCrosshair)
    {
        float dy = (200.0f - rim->actual_height_ * rim->scale_y_) / 2.0f; // - WEAPONTOP;
        rim->offset_y_ += int(dy / rim->scale_y_);
    }

    if (def->special_ & kImageSpecialGrayscale)
        rim->grayscale_ = true;

    switch (def->belong_)
    {
    case kImageNamespaceGraphic:
        real_graphics.push_back(rim);
        break;
    case kImageNamespaceTexture:
        real_textures.push_back(rim);
        break;
    case kImageNamespaceFlat:
        real_flats.push_back(rim);
        break;
    case kImageNamespaceSprite:
        real_sprites.push_back(rim);
        break;

    default:
        FatalError("INTERNAL ERROR: Bad belong value: %d\n", def->belong_);
    }

    if (def->special_ & kImageSpecialPrecache)
        ImagePrecache(rim);

    return rim;
}

//
// Used to fill in the image array with flats from the WAD.  The set
// of lumps is those that occurred between F_START and F_END in each
// existing wad file, with duplicates set to -1.
//
// NOTE: should only be called once, as it assumes none of the flats
// in the list have names colliding with existing flat images.
//
void CreateFlats(std::vector<int> &lumps)
{
    for (size_t i = 0; i < lumps.size(); i++)
    {
        if (lumps[i] >= 0)
        {
            const char *name = GetLumpNameFromIndex(lumps[i]);
            AddImageFlat(name, lumps[i]);
        }
    }
}

//
// Used to fill in the image array with textures from the WAD.  The
// list of texture definitions comes from each TEXTURE1/2 lump in each
// existing wad file, with duplicates set to nullptr.
//
// NOTE: should only be called once, as it assumes none of the
// textures in the list have names colliding with existing texture
// images.
//
void CreateTextures(struct TextureDefinition **defs, int number)
{
    int i;

    EPI_ASSERT(defs);

    for (i = 0; i < number; i++)
    {
        if (defs[i] == nullptr)
            continue;

        AddImageTexture(defs[i]->name, defs[i]);
    }
}

//
// Used to fill in the image array with sprites from the WAD.  The
// lumps come from those occurring between S_START and S_END markers
// in each existing wad.
//
// NOTE: it is assumed that each new sprite is unique i.e. the name
// does not collide with any existing sprite image.
//
const Image *CreateSprite(const char *name, int lump, bool is_weapon)
{
    EPI_ASSERT(lump >= 0);

    Image *rim = AddImage_Smart(name, kImageSourceSprite, lump, real_sprites);
    if (!rim)
        return nullptr;

    // adjust sprite offsets so that (0,0) is normal
    if (is_weapon)
    {
        rim->offset_x_ += (320.0f / 2.0f - rim->actual_width_ / 2.0f); // loss of accuracy
        rim->offset_y_ += (200.0f - 32.0f - rim->actual_height_);
    }
    else
    {
        // rim->offset_x_ -= rim->actual_width_ / 2;   // loss of accuracy
        rim->offset_x_ -= ((float)rim->actual_width_) / 2.0f; // Lobo 2023: dancing eye fix
        rim->offset_y_ -= rim->actual_height_;
    }

    return rim;
}

const Image *CreatePackSprite(std::string packname, PackFile *pack, bool is_weapon)
{
    EPI_ASSERT(pack);

    Image *rim = AddPackImageSmart(epi::GetStem(packname).c_str(), kImageSourceSprite, packname.c_str(), real_sprites);
    if (!rim)
        return nullptr;

    // adjust sprite offsets so that (0,0) is normal
    if (is_weapon)
    {
        rim->offset_x_ += (320.0f / 2.0f - rim->actual_width_ / 2.0f);
        rim->offset_y_ += (200.0f - 32.0f - rim->actual_height_);
    }
    else
    {
        rim->offset_x_ -= ((float)rim->actual_width_) / 2.0f; // Lobo 2023: dancing eye fix
        rim->offset_y_ -= rim->actual_height_;
    }

    return rim;
}

//
// Add the images defined in IMAGES.DDF.
//
void CreateUserImages(void)
{
    LogPrint("Adding DDFIMAGE definitions...\n");

    for (auto def : imagedefs)
    {
        if (def == nullptr)
            continue;

        if (def->belong_ != kImageNamespacePatch)
            AddImageUser(def);
    }
}

void ImageAddTxHx(int lump, const char *name, bool hires)
{
    if (hires)
    {
        const Image *rim = ImageContainerLookup(real_textures, name, -2);
        if (rim && rim->source_type_ != kImageSourceUser)
        {
            AddImage_Smart(name, kImageSourceTXHI, lump, real_textures, rim);
            return;
        }

        rim = ImageContainerLookup(real_flats, name, -2);
        if (rim && rim->source_type_ != kImageSourceUser)
        {
            AddImage_Smart(name, kImageSourceTXHI, lump, real_flats, rim);
            return;
        }

        rim = ImageContainerLookup(real_sprites, name, -2);
        if (rim && rim->source_type_ != kImageSourceUser)
        {
            AddImage_Smart(name, kImageSourceTXHI, lump, real_sprites, rim);
            return;
        }

        // we do it this way to force the original graphic to be loaded
        rim = ImageLookup(name, kImageNamespaceGraphic, kImageLookupExact | kImageLookupNull);

        if (rim && rim->source_type_ != kImageSourceUser)
        {
            AddImage_Smart(name, kImageSourceTXHI, lump, real_graphics, rim);
            return;
        }

        LogDebug("HIRES replacement '%s' has no counterpart.\n", name);
    }

    TX_names.push_back(name);

    AddImage_Smart(name, kImageSourceTXHI, lump, real_textures);
}

//
// Only used during sprite initialisation.  The returned array of
// images is guaranteed to be sorted by name.
//
// Use delete[] to free the returned array.
//
const Image **GetUserSprites(int *count)
{
    // count number of user sprites
    (*count) = 0;

    std::list<Image *>::iterator it;

    for (it = real_sprites.begin(); it != real_sprites.end(); it++)
    {
        Image *rim = *it;

        if (rim->source_type_ == kImageSourceUser || rim->source_.graphic.user_defined)
            (*count) += 1;
    }

    if (*count == 0)
    {
        LogDebug("GetUserSprites(count = %d)\n", *count);
        return nullptr;
    }

    const Image **array = new const Image *[*count];
    int           pos   = 0;

    for (it = real_sprites.begin(); it != real_sprites.end(); it++)
    {
        Image *rim = *it;

        if (rim->source_type_ == kImageSourceUser || rim->source_.graphic.user_defined)
            array[pos++] = rim;
    }

#define EDGE_CMP(a, b) (strcmp(a->name_.c_str(), b->name_.c_str()) < 0)
    EDGE_QSORT(const Image *, array, (*count), 10);
#undef EDGE_CMP

    return array;
}

//----------------------------------------------------------------------------
//
//  IMAGE LOADING / UNLOADING
//

// TODO: make methods of image_c class
static bool IM_ShouldClamp(const Image *rim)
{
    switch (rim->source_type_)
    {
    case kImageSourceGraphic:
    case kImageSourceRawBlock:
    case kImageSourceSprite:
        return true;

    case kImageSourceUser:
        switch (rim->source_.user.def->belong_)
        {
        case kImageNamespaceGraphic:
        case kImageNamespaceSprite:
            return true;

        default:
            return false;
        }

    default:
        return false;
    }
}

static bool IM_ShouldMipmap(Image *rim)
{
    // the "SKY" check here is a hack...
    if (epi::StringPrefixCaseCompareASCII(rim->name_, "SKY") == 0)
        return false;

    if (image_mipmapping == 0)
        return false;

    switch (rim->source_type_)
    {
    case kImageSourceTexture:
    case kImageSourceFlat:
    case kImageSourceTXHI:
        return true;

    case kImageSourceUser:
        switch (rim->source_.user.def->belong_)
        {
        case kImageNamespaceTexture:
        case kImageNamespaceFlat:
            return true;

        default:
            return false;
        }

    default:
        return false;
    }
}

static bool IM_ShouldSmooth(Image *rim)
{
    if (!AlmostEquals(rim->blur_sigma_, 0.0f))
        return true;

    return image_smoothing ? true : false;
}

static bool IM_ShouldHQ2X(Image *rim)
{
    // Note: no need to check kImageSourceUser, since those images are
    //       always PNG or JPEG (etc) and never palettised, hence
    //       the HQ2x scaling would never apply.

    if (hq2x_scaling == 0)
        return false;

    if (hq2x_scaling >= 3)
        return true;

    switch (rim->source_type_)
    {
    case kImageSourceGraphic:
    case kImageSourceRawBlock:
        // UI elements
        return true;
#if 0
		case kImageSourceTexture:
			// the "SKY" check here is a hack...
			if (epi::StringPrefixCaseCompareASCII(rim->name_, "SKY") == 0)
				return true;
			break;
#endif
    case kImageSourceSprite:
        if (hq2x_scaling >= 2)
            return true;
        break;

    default:
        break;
    }

    return false;
}

static int IM_PixelLimit(Image *rim)
{
    if (detail_level == 0)
        return (1 << 18);
    else if (detail_level == 1)
        return (1 << 20);
    else
        return (1 << 22);
}

static GLuint LoadImageOGL(Image *rim, const Colormap *trans, bool do_whiten)
{
    bool clamp  = IM_ShouldClamp(rim);
    bool mip    = IM_ShouldMipmap(rim);
    bool smooth = IM_ShouldSmooth(rim);

    int max_pix = IM_PixelLimit(rim);

    if (rim->source_type_ == kImageSourceUser)
    {
        if (rim->source_.user.def->special_ & kImageSpecialClamp)
            clamp = true;

        if (rim->source_.user.def->special_ & kImageSpecialMip)
            mip = true;
        else if (rim->source_.user.def->special_ & kImageSpecialNoMip)
            mip = false;

        if (rim->source_.user.def->special_ & kImageSpecialSmooth)
            smooth = true;
        else if (rim->source_.user.def->special_ & kImageSpecialNoSmooth)
            smooth = false;
    }
    else if (rim->source_type_ == kImageSourceGraphic && rim->source_.graphic.user_defined)
    {
        if (rim->source_.graphic.special & kImageSpecialClamp)
            clamp = true;

        if (rim->source_.graphic.special & kImageSpecialMip)
            mip = true;
        else if (rim->source_.graphic.special & kImageSpecialNoMip)
            mip = false;

        if (rim->source_.graphic.special & kImageSpecialSmooth)
            smooth = true;
        else if (rim->source_.graphic.special & kImageSpecialNoSmooth)
            smooth = false;
    }

    const uint8_t *what_palette    = (const uint8_t *)&playpal_data[0];
    bool           what_pal_cached = false;

    static uint8_t trans_pal[256 * 3];

    if (trans != nullptr)
    {
        // Note: we don't care about source_palette here. It's likely that
        // the translation table itself would not match the other palette,
        // and so we would still end up with messed up colours.

        TranslatePalette(trans_pal, what_palette, trans);
        what_palette = trans_pal;
    }
    else if (rim->source_palette_ >= 0)
    {
        what_palette    = (const uint8_t *)LoadLumpIntoMemory(rim->source_palette_);
        what_pal_cached = true;
    }

    ImageData *tmp_img = ReadAsEpiBlock(rim);

    if (rim->liquid_type_ > kLiquidImageNone &&
        (swirling_flats == kLiquidSwirlSmmu || swirling_flats == kLiquidSwirlSmmuSlosh))
    {
        rim->swirled_game_tic_ = hud_tic;
        tmp_img->Swirl(rim->swirled_game_tic_,
                       rim->liquid_type_); // Using leveltime disabled swirl
                                           // for intermission screens
    }

    if (rim->opacity_ == kOpacityUnknown)
        rim->opacity_ = DetermineOpacity(tmp_img, &rim->is_empty_);

    if ((tmp_img->depth_ == 1) && IM_ShouldHQ2X(rim))
    {
        bool solid = (rim->opacity_ == kOpacitySolid);

        HQ2xPaletteSetup(what_palette, solid ? -1 : kTransparentPixelIndex);

        ImageData *scaled_img = ImageHQ2x(tmp_img, solid, false /* invert */);

        if (rim->is_font_)
        {
            scaled_img->RemoveBackground();
            rim->opacity_ = DetermineOpacity(tmp_img, &rim->is_empty_);
        }

        if (rim->blur_sigma_ > 0.0f)
        {
            ImageData *blurred_img = ImageBlur(scaled_img, rim->blur_sigma_);
            delete scaled_img;
            scaled_img = blurred_img;
        }

        delete tmp_img;
        tmp_img = scaled_img;
    }
    else if (tmp_img->depth_ == 1)
    {
        ImageData *rgb_img = RGBFromPalettised(tmp_img, what_palette, rim->opacity_);

        if (rim->is_font_)
        {
            rgb_img->RemoveBackground();
            rim->opacity_ = DetermineOpacity(tmp_img, &rim->is_empty_);
        }

        if (rim->blur_sigma_ > 0.0f)
        {
            ImageData *blurred_img = ImageBlur(rgb_img, rim->blur_sigma_);
            delete rgb_img;
            rgb_img = blurred_img;
        }

        delete tmp_img;
        tmp_img = rgb_img;
    }
    else if (tmp_img->depth_ >= 3)
    {
        if (rim->is_font_)
        {
            tmp_img->RemoveBackground();
            rim->opacity_ = DetermineOpacity(tmp_img, &rim->is_empty_);
        }
        if (rim->blur_sigma_ > 0.0f)
        {
            ImageData *blurred_img = ImageBlur(tmp_img, rim->blur_sigma_);
            delete tmp_img;
            tmp_img = blurred_img;
        }
        if (trans != nullptr)
            PaletteRemapRGBA(tmp_img, what_palette, (const uint8_t *)&playpal_data[0]);
    }

    if (rim->hsv_rotation_ || rim->hsv_saturation_ > -1 || rim->hsv_value_)
        tmp_img->SetHsv(rim->hsv_rotation_, rim->hsv_saturation_, rim->hsv_value_);

    if (do_whiten)
        tmp_img->Whiten();

    GLuint tex_id =
        UploadTexture(tmp_img,
                      (clamp ? kUploadClamp : 0) | (mip ? kUploadMipMap : 0) | (smooth ? kUploadSmooth : 0) |
                          ((rim->opacity_ == kOpacityMasked) ? kUploadThresh : 0),
                      max_pix);

    delete tmp_img;

    if (what_pal_cached)
        delete[] what_palette;

    return tex_id;
}

//----------------------------------------------------------------------------
//  IMAGE LOOKUP
//----------------------------------------------------------------------------

//
// BackupTexture
//
static const Image *BackupTexture(const char *tex_name, int flags)
{
    const Image *rim;

    if (!(flags & kImageLookupExact))
    {
        // backup plan: try a flat with the same name
        rim = ImageContainerLookup(real_flats, tex_name);
        if (rim)
            return rim;

        // backup backup plan: try a graphic with the same name
        rim = ImageContainerLookup(real_graphics, tex_name);
        if (rim)
            return rim;

        // backup backup backup plan: see if it's a graphic in the P/PP_START
        // P/PP_END namespace and make/return an image if valid
        int checkfile = CheckDataFileIndexForName(tex_name);
        int checklump = CheckLumpNumberForName(tex_name);
        if (checkfile > -1 && checklump > -1)
        {
            for (auto patch_lump : *GetPatchListForWAD(checkfile))
            {
                if (patch_lump == checklump)
                {
                    rim = AddImage_Smart(tex_name, kImageSourceGraphic, patch_lump, real_graphics);
                    if (rim)
                        return rim;
                }
            }
        }
    }

    if (flags & kImageLookupNull)
        return nullptr;

    WarningOrError("Unknown texture found in level: '%s'\n", tex_name);

    Image *dummy;

    if (epi::StringPrefixCaseCompareASCII(tex_name, "SKY") == 0)
        dummy = CreateDummyImage(tex_name, 0x0000AA, 0x55AADD);
    else
        dummy = CreateDummyImage(tex_name, 0xAA5511, 0x663300);

    // keep dummy texture so that future lookups will succeed
    real_textures.push_back(dummy);
    return dummy;
}

void CreateFallbackTexture()
{
    real_textures.push_back(CreateDummyImage("EDGETEX", 0xAA5511, 0x663300));
}

//
// BackupFlat
//

static const Image *BackupFlat(const char *flat_name, int flags)
{
    const Image *rim;

    // backup plan 1: if lump exists and is right size, add it.
    if (!(flags & kImageLookupNoNew))
    {
        int i = CheckLumpNumberForName(flat_name);

        if (i >= 0)
        {
            rim = AddImageFlat(flat_name, i);
            if (rim)
                return rim;
        }
    }

    // backup plan 2: Texture with the same name ?
    if (!(flags & kImageLookupExact))
    {
        rim = ImageContainerLookup(real_textures, flat_name);
        if (rim)
            return rim;
    }

    if (flags & kImageLookupNull)
        return nullptr;

    WarningOrError("Unknown flat found in level: '%s'\n", flat_name);

    Image *dummy = CreateDummyImage(flat_name, 0x11AA11, 0x115511);

    // keep dummy flat so that future lookups will succeed
    real_flats.push_back(dummy);
    return dummy;
}

void CreateFallbackFlat()
{
    real_flats.push_back(CreateDummyImage("EDGEFLAT", 0x11AA11, 0x115511));
}

//
// BackupGraphic
//
static const Image *BackupGraphic(const char *gfx_name, int flags)
{
    const Image *rim;

    // backup plan 1: look for sprites and heretic-background
    if ((flags & (kImageLookupExact | kImageLookupFont)) == 0)
    {
        rim = ImageContainerLookup(real_graphics, gfx_name, kImageSourceRawBlock);
        if (rim)
            return rim;

        rim = ImageContainerLookup(real_sprites, gfx_name);
        if (rim)
            return rim;
    }

    // not already loaded ?  Check if lump exists in wad, if so add it.
    if (!(flags & kImageLookupNoNew))
    {
        int i = CheckGraphicLumpNumberForName(gfx_name);

        if (i >= 0)
        {
            rim = AddImage_Smart(gfx_name, kImageSourceGraphic, i, real_graphics);
            if (rim)
                return rim;
        }
    }

    if (flags & kImageLookupNull)
        return nullptr;

    DebugOrError("Unknown graphic: '%s'\n", gfx_name);

    Image *dummy;

    if (flags & kImageLookupFont)
        dummy = CreateDummyImage(gfx_name, 0xFFFFFF, kTransparentPixelIndex);
    else
        dummy = CreateDummyImage(gfx_name, 0xFF0000, kTransparentPixelIndex);

    // keep dummy graphic so that future lookups will succeed
    real_graphics.push_back(dummy);
    return dummy;
}

static const Image *BackupSprite(const char *spr_name, int flags)
{
    if (flags & kImageLookupNull)
        return nullptr;

    return ImageForDummySprite();
}

const Image *ImageLookup(const char *name, ImageNamespace type, int flags)
{
    //
    // Note: search must be case insensitive.
    //

    // "NoTexture" marker.
    if (!name || !name[0] || name[0] == '-')
        return nullptr;

    // "Sky" marker.
    if (type == kImageNamespaceFlat &&
        (epi::StringCaseCompareASCII(name, "F_SKY1") == 0 || epi::StringCaseCompareASCII(name, "F_SKY") == 0))
    {
        return sky_flat_image;
    }

    // compatibility hack (first texture in IWAD is a dummy)
    if (type == kImageNamespaceTexture &&
        ((epi::StringCaseCompareASCII(name, "AASTINKY") == 0) || (epi::StringCaseCompareASCII(name, "AASHITTY") == 0) ||
         (epi::StringCaseCompareASCII(name, "BADPATCH") == 0) || (epi::StringCaseCompareASCII(name, "ABADONE") == 0)))
    {
        return nullptr;
    }

    const Image *rim;

    if (type == kImageNamespaceTexture)
    {
        rim = ImageContainerLookup(real_textures, name);
        return rim ? rim : BackupTexture(name, flags);
    }
    if (type == kImageNamespaceFlat)
    {
        rim = ImageContainerLookup(real_flats, name);
        return rim ? rim : BackupFlat(name, flags);
    }
    if (type == kImageNamespaceSprite)
    {
        rim = ImageContainerLookup(real_sprites, name);
        return rim ? rim : BackupSprite(name, flags);
    }

    /* kImageNamespaceGraphic */

    rim = ImageContainerLookup(real_graphics, name);
    return rim ? rim : BackupGraphic(name, flags);
}

const Image *ImageForDummySprite(void)
{
    return dummy_sprite;
}

const Image *ImageForDummySkin(void)
{
    return dummy_skin;
}

const Image *ImageForHomDetect(void)
{
    return dummy_hom[(hud_tic & 0x10) ? 1 : 0];
}

const Image *ImageForFogWall(RGBAColor fog_color)
{
    std::string fogname = epi::StringFormat("FOGWALL_%d", fog_color);
    Image      *fogwall = (Image *)ImageLookup(fogname.c_str(), kImageNamespaceGraphic, kImageLookupNull);
    if (fogwall)
        return fogwall;
    ImageDefinition *fogdef = new ImageDefinition;
    fogdef->colour_         = fog_color;
    fogdef->name_           = fogname;
    fogdef->type_           = kImageDataColor;
    fogdef->belong_         = kImageNamespaceGraphic;
    fogwall                 = AddImageUser(fogdef);
    return fogwall;
}

const Image *ImageParseSaveString(char type, const char *name)
{
    // Used by the savegame code.

    // this name represents the sky (historical reasons)
    if (type == 'd' && epi::StringCaseCompareASCII(name, "DUMMY__2") == 0)
    {
        return sky_flat_image;
    }

    switch (type)
    {
    case 'K':
        return sky_flat_image;

    case 'F':
        return ImageLookup(name, kImageNamespaceFlat);

    case 'P':
        return ImageLookup(name, kImageNamespaceGraphic);

    case 'S':
        return ImageLookup(name, kImageNamespaceSprite);

    default:
        LogWarning("ImageParseSaveString: unknown type '%c'\n", type);
        /* FALL THROUGH */

    case 'd': /* dummy */
    case 'T':
        return ImageLookup(name, kImageNamespaceTexture);
    }
}

void ImageMakeSaveString(const Image *image, char *type, char *namebuf)
{
    // Used by the savegame code

    if (image == sky_flat_image)
    {
        *type = 'K';
        strcpy(namebuf, "F_SKY1");
        return;
    }

    const Image *rim = (const Image *)image;

    strcpy(namebuf, rim->name_.c_str());

    /* handle User images (convert to a more general type) */
    if (rim->source_type_ == kImageSourceUser)
    {
        switch (rim->source_.user.def->belong_)
        {
        case kImageNamespaceTexture:
            (*type) = 'T';
            return;
        case kImageNamespaceFlat:
            (*type) = 'F';
            return;
        case kImageNamespaceSprite:
            (*type) = 'S';
            return;

        default: /* kImageNamespaceGraphic */
            (*type) = 'P';
            return;
        }
    }

    switch (rim->source_type_)
    {
    case kImageSourceRawBlock:
    case kImageSourceGraphic:
        (*type) = 'P';
        return;

    case kImageSourceTXHI:
    case kImageSourceTexture:
        (*type) = 'T';
        return;

    case kImageSourceFlat:
        (*type) = 'F';
        return;

    case kImageSourceSprite:
        (*type) = 'S';
        return;

    case kImageSourceDummy:
        (*type) = 'd';
        return;

    default:
        FatalError("ImageMakeSaveString: bad type %d\n", rim->source_type_);
        break;
    }
}

//----------------------------------------------------------------------------
//
//  IMAGE USAGE
//

static CachedImage *ImageCacheOGL(Image *rim, const Colormap *trans, bool do_whiten)
{
    // check if image + translation is already cached

    int free_slot = -1;

    CachedImage *rc = nullptr;

    for (int i = 0; i < (int)rim->cache_.size(); i++)
    {
        rc = rim->cache_[i];

        if (!rc)
        {
            free_slot = i;
            continue;
        }

        if (do_whiten && rc->is_whitened)
            break;

        if (rc->translation_map == trans)
        {
            if (do_whiten)
            {
                if (rc->is_whitened)
                    break;
            }
            else if (!rc->is_whitened)
                break;
        }

        rc = nullptr;
    }

    if (!rc)
    {
        // add entry into cache
        rc = new CachedImage;

        rc->parent          = rim;
        rc->translation_map = trans;
        rc->hue             = kRGBANoValue;
        rc->texture_id      = 0;
        rc->is_whitened     = do_whiten ? true : false;

        image_cache.push_back(rc);

        if (free_slot >= 0)
            rim->cache_[free_slot] = rc;
        else
            rim->cache_.push_back(rc);
    }

    EPI_ASSERT(rc);

    if (rim->liquid_type_ > kLiquidImageNone &&
        (swirling_flats == kLiquidSwirlSmmu || swirling_flats == kLiquidSwirlSmmuSlosh))
    {
        if (!erraticism_active && !time_stop_active && rim->swirled_game_tic_ != hud_tic)
        {
            if (rc->texture_id != 0)
            {
                global_render_state->DeleteTexture(&rc->texture_id);
                rc->texture_id = 0;
            }
        }
    }

    if (rc->texture_id == 0)
    {
        // load image into cache
        rc->texture_id = LoadImageOGL(rim, trans, do_whiten);
    }

    return rc;
}

//
// The top-level routine for caching in an image.  Mainly just a
// switch to more specialised routines.
//
GLuint ImageCache(const Image *image, bool anim, const Colormap *trans, bool do_whiten)
{
    // Intentional Const Override
    Image *rim = (Image *)image;

    // handle animations
    if (anim)
    {
        if (rim->liquid_type_ == kLiquidImageNone || swirling_flats == kLiquidSwirlVanilla)
            rim = rim->animation_.current;
    }

    if (rim->grayscale_)
        do_whiten = true;

    CachedImage *rc = ImageCacheOGL(rim, trans, do_whiten);

    EPI_ASSERT(rc->parent);

    return rc->texture_id;
}

void ImagePrecache(const Image *image)
{
    ImageCache(image, false);

    // Intentional Const Override
    Image *rim = (Image *)image;

    // pre-cache alternative images for switches too
    if (rim->name_.size() >= 4 && (epi::StringPrefixCaseCompareASCII(rim->name_, "SW1") == 0 ||
                                   epi::StringPrefixCaseCompareASCII(rim->name_, "SW2") == 0))
    {
        std::string alt_name = rim->name_;

        alt_name[2] = (alt_name[2] == '1') ? '2' : '1';

        Image *alt = ImageContainerLookup(real_textures, alt_name.c_str());

        if (alt)
            ImageCache(alt, false);
    }
}

//----------------------------------------------------------------------------

static void W_CreateDummyImages(void)
{
    dummy_sprite = CreateDummyImage("DUMMY_SPRITE", 0xFFFF00, kTransparentPixelIndex);
    dummy_skin   = CreateDummyImage("DUMMY_SKIN", 0xFF77FF, 0x993399);

    sky_flat_image = CreateDummyImage("DUMMY_SKY", 0x0000AA, 0x55AADD);

    dummy_hom[0] = CreateDummyImage("DUMMY_HOM1", 0xFF3333, 0x000000);
    dummy_hom[1] = CreateDummyImage("DUMMY_HOM2", 0x000000, 0xFF3333);

    // make the dummy sprite easier to see
    {
        // Intentional Const Override
        Image *dsp = (Image *)dummy_sprite;

        dsp->scale_x_ = 3.0f;
        dsp->scale_y_ = 3.0f;
    }
}

//
// Initialises the image system.
//
bool InitializeImages(void)
{
    // check options
    if (FindArgument("nosmoothing") > 0)
        image_smoothing = 0;
    else if (FindArgument("smoothing") > 0)
        image_smoothing = 1;

    if (FindArgument("hqscale") > 0 || FindArgument("hqall") > 0)
        hq2x_scaling = 3;
    else if (FindArgument("nohqscale") > 0)
        hq2x_scaling = 0;

    W_CreateDummyImages();

    return true;
}

//
// Animate all the images.
//
void AnimationTicker(void)
{
    do_Animate(real_graphics);
    if (game_state < kGameStateLevel)
    {
        do_Animate(real_textures);
        do_Animate(real_flats);
    }
    else if (!time_stop_active && !erraticism_active)
    {
        do_Animate(real_textures);
        do_Animate(real_flats);
    }
}

void DeleteAllImages(void)
{
    std::list<CachedImage *>::iterator CI;

    for (CI = image_cache.begin(); CI != image_cache.end(); CI++)
    {
        CachedImage *rc = *CI;
        EPI_ASSERT(rc);

        if (rc->texture_id != 0)
        {
            global_render_state->DeleteTexture(&rc->texture_id);
            rc->texture_id = 0;
        }
    }

    DeleteSkyTextures();
    DeleteColourmapTextures();
}

//
// AnimateImageSet
//
// Sets up the images so they will animate properly.  Array is
// allowed to contain nullptr entries.
//
// NOTE: modifies the input array of images.
//
void AnimateImageSet(const Image **images, int number, int speed)
{
    int    i, total;
    Image *rim, *other;

    EPI_ASSERT(images);
    EPI_ASSERT(speed > 0);

    // ignore images that are already animating
    for (i = 0, total = 0; i < number; i++)
    {
        // Intentional Const Override
        rim = (Image *)images[i];

        if (!rim)
            continue;

        if (rim->animation_.speed > 0)
        {
            Image *dupe_image           = new Image;
            dupe_image->name_           = rim->name_;
            dupe_image->actual_height_  = rim->actual_height_;
            dupe_image->actual_width_   = rim->actual_width_;
            dupe_image->cache_          = rim->cache_;
            dupe_image->is_empty_       = rim->is_empty_;
            dupe_image->is_font_        = rim->is_font_;
            dupe_image->liquid_type_    = rim->liquid_type_;
            dupe_image->offset_x_       = rim->offset_x_;
            dupe_image->offset_y_       = rim->offset_y_;
            dupe_image->opacity_        = rim->opacity_;
            dupe_image->height_ratio_   = rim->height_ratio_;
            dupe_image->width_ratio_    = rim->width_ratio_;
            dupe_image->scale_x_        = rim->scale_x_;
            dupe_image->scale_y_        = rim->scale_y_;
            dupe_image->source_         = rim->source_;
            dupe_image->source_palette_ = rim->source_palette_;
            dupe_image->source_type_    = rim->source_type_;
            dupe_image->total_height_   = rim->total_height_;
            dupe_image->total_width_    = rim->total_width_;
            rim                         = dupe_image;
        }

        images[total++] = rim;
    }

    // anything left to animate ?
    if (total < 2)
        return;

    for (i = 0; i < total; i++)
    {
        // Intentional Const Override
        rim   = (Image *)images[i];
        other = (Image *)images[(i + 1) % total];

        rim->animation_.next  = other;
        rim->animation_.speed = rim->animation_.count = speed;
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
