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

#include "i_defs.h"
#include "i_defs_gl.h"

#include <limits.h>
#include <list>

#include "endianess.h"
#include "file.h"
#include "filesystem.h"
#include "flat.h"

#include "image_data.h"
#include "image_blur.h"
#include "image_hq2x.h"
#include "image_funcs.h"
#include "str_util.h"

#include "dm_data.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "e_search.h"
#include "e_main.h"
#include "hu_draw.h" // hudtic
#include "m_argv.h"
#include "m_misc.h"
#include "p_local.h"
#include "r_defs.h"
#include "r_gldefs.h"
#include "r_image.h"
#include "r_sky.h"
#include "r_texgl.h"
#include "r_colormap.h"
#include "w_texture.h"
#include "w_epk.h"
#include "w_wad.h"
#include "w_files.h"

swirl_type_e swirling_flats = SWIRL_Vanilla;

// LIGHTING DEBUGGING
// #define MAKE_TEXTURES_WHITE  1

extern image_data_c *ReadAsEpiBlock(image_c *rim);

extern epi::File *OpenUserFileOrLump(imagedef_c *def);

extern cvar_c r_doubleframes;

extern void DeleteSkyTextures(void);
extern void DeleteColourmapTextures(void);

extern bool erraticism_active;

//
// This structure is for "cached" images (i.e. ready to be used for
// rendering), and is the non-opaque version of cached_image_t.  A
// single structure is used for all image modes (Block and OGL).
//
// Note: multiple modes and/or multiple mips of the same image_c can
// indeed be present in the cache list at any one time.
//
typedef struct cached_image_s
{
    // parent image
    image_c *parent;

    // colormap used for translated image, normally NULL
    const colourmap_c *trans_map;

    // general hue of image (skewed towards pure colors)
    RGBAColor hue;

    // texture identifier within GL
    GLuint tex_id;

    bool is_whitened;
} cached_image_t;

image_c *W_ImageDoLookup(real_image_container_c &bucket, const char *name, int source_type
                         /* use -2 to prevent USER override */)
{
    // for a normal lookup, we want USER images to override
    if (source_type == -1)
    {
        image_c *rim = W_ImageDoLookup(bucket, name, IMSRC_User); // recursion
        if (rim)
            return rim;
    }

    real_image_container_c::reverse_iterator it;

    // search backwards, we want newer image to override older ones
    for (it = bucket.rbegin(); it != bucket.rend(); it++)
    {
        image_c *rim = *it;

        if (source_type >= 0 && source_type != (int)rim->source_type)
            continue;

        if (epi::StringCaseCompareASCII(name, rim->name) == 0)
            return rim;
    }

    return NULL; // not found
}

static void do_Animate(real_image_container_c &bucket)
{
    real_image_container_c::iterator it;

    for (it = bucket.begin(); it != bucket.end(); it++)
    {
        image_c *rim = *it;

        if (rim->anim.speed == 0) // not animated ?
            continue;

        if (rim->liquid_type > LIQ_None && swirling_flats > SWIRL_Vanilla)
            continue;

        SYS_ASSERT(rim->anim.count > 0);

        rim->anim.count -= (!r_doubleframes.d || !(hudtic & 1)) ? 1 : 0;

        if (rim->anim.count == 0 && rim->anim.cur->anim.next)
        {
            rim->anim.cur   = rim->anim.cur->anim.next;
            rim->anim.count = rim->anim.speed;
        }
    }
}

#if 0
static void do_DebugDump(real_image_container_c& bucket)
{
	L_WriteDebug("{\n");

	real_image_container_c::iterator it;

	for (it = bucket.begin(); it != bucket.end(); it++)
	{
		image_c *rim = *it;
	
		L_WriteDebug("   [%s] type %d: %dx%d < %dx%d\n",
			rim->name, rim->source_type,
			rim->actual_w, rim->actual_h,
			rim->total_w, rim->total_h);
	}

	L_WriteDebug("}\n");
}
#endif

int var_smoothing = 1;

int hq2x_scaling = 1;

// total set of images
real_image_container_c real_graphics;
real_image_container_c real_textures;
real_image_container_c real_flats;
real_image_container_c real_sprites;

const image_c *skyflatimage;

static const image_c *dummy_sprite;
static const image_c *dummy_skin;
static const image_c *dummy_hom[2];

// image cache (actually a ring structure)
static std::list<cached_image_t *> image_cache;

// tiny ring helpers
static inline void InsertAtTail(cached_image_t *rc)
{
    image_cache.push_back(rc);

#if 0 // OLD WAY
	SYS_ASSERT(rc != &imagecachehead);

	rc->prev =  imagecachehead.prev;
	rc->next = &imagecachehead;

	rc->prev->next = rc;
	rc->next->prev = rc;
#endif
}
static inline void Unlink(cached_image_t *rc)
{
    // FIXME: Unlink
    (void)rc;
#if 0
	SYS_ASSERT(rc != &imagecachehead);

	rc->prev->next = rc->next;
	rc->next->prev = rc->prev;
#endif
}

//----------------------------------------------------------------------------
//
//  IMAGE CREATION
//

image_c::image_c()
    : actual_w(0), actual_h(0), total_w(0), total_h(0), ratio_w(0.0), ratio_h(0.0), source_type(IMSRC_Dummy),
      source_palette(-1), cache()
{
    name = "_UNINIT_";

    memset(&source, 0, sizeof(source));
    memset(&anim, 0, sizeof(anim));
}

image_c::~image_c()
{
    /* TODO: image_c destructor */
}

void W_ImageStoreBlurred(const image_c *image, float sigma)
{
    // const override
    image_c *img = (image_c *)image;
    if (!img->blurred_version)
    {
        img->blurred_version                 = new image_c;
        img->blurred_version->name           = std::string(img->name).append("_BLURRED");
        img->blurred_version->actual_h       = img->actual_h;
        img->blurred_version->actual_w       = img->actual_w;
        img->blurred_version->is_empty       = img->is_empty;
        img->blurred_version->is_font        = img->is_font;
        img->blurred_version->liquid_type    = img->liquid_type;
        img->blurred_version->offset_x       = img->offset_x;
        img->blurred_version->offset_y       = img->offset_y;
        img->blurred_version->opacity        = img->opacity;
        img->blurred_version->ratio_h        = img->ratio_h;
        img->blurred_version->ratio_w        = img->ratio_w;
        img->blurred_version->scale_x        = img->scale_x;
        img->blurred_version->scale_y        = img->scale_y;
        img->blurred_version->source         = img->source;
        img->blurred_version->source_palette = img->source_palette;
        img->blurred_version->source_type    = img->source_type;
        img->blurred_version->total_h        = img->total_h;
        img->blurred_version->total_w        = img->total_w;
        img->blurred_version->anim.cur       = img->blurred_version;
        img->blurred_version->anim.next      = NULL;
        img->blurred_version->anim.count     = 0;
        img->blurred_version->anim.speed     = 0;
        img->blurred_version->blur_sigma     = sigma;
    }
}

static image_c *NewImage(int width, int height, int opacity = OPAC_Unknown)
{
    image_c *rim = new image_c;

    rim->actual_w = width;
    rim->actual_h = height;
    rim->total_w  = W_MakeValidSize(width);
    rim->total_h  = W_MakeValidSize(height);
    rim->ratio_w  = (float)width / (float)rim->total_w * 0.0625;
    rim->ratio_h  = (float)height / (float)rim->total_h * 0.0625;
    rim->offset_x = rim->offset_y = 0;
    rim->scale_x = rim->scale_y = 1.0f;
    rim->opacity                = opacity;
    rim->is_empty               = false;
    rim->is_font                = false;

    // set initial animation info
    rim->anim.cur   = rim;
    rim->anim.next  = NULL;
    rim->anim.count = rim->anim.speed = 0;

    rim->liquid_type = LIQ_None;

    rim->swirled_gametic = 0;

    return rim;
}

static image_c *CreateDummyImage(const char *name, RGBAColor fg, RGBAColor bg)
{
    image_c *rim;

    rim = NewImage(DUMMY_X, DUMMY_Y, (bg == TRANS_PIXEL) ? OPAC_Masked : OPAC_Solid);

    rim->name = name;

    rim->source_type    = IMSRC_Dummy;
    rim->source_palette = -1;

    rim->source.dummy.fg = fg;
    rim->source.dummy.bg = bg;

    return rim;
}

image_c *AddImage_SmartPack(const char *name, image_source_e type, const char *packfile_name,
                            real_image_container_c &container, const image_c *replaces)
{
    /* used for Graphics, Sprites and TX/HI stuff */
    epi::File *f = W_OpenPackFile(packfile_name);
    SYS_ASSERT(f);
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
    auto fmt        = Image_DetectFormat(header, header_len, packfile_len);

    if (fmt == kOtherImage)
    {
        // close it
        delete f;

        I_Warning("Unsupported image format in '%s'\n", packfile_name);
        return NULL;
    }
    else if (fmt == kUnknownImage)
    {
        // close it
        delete f;

        // check for Heretic/Hexen images, which are raw 320x200
        if (packfile_len == 320 * 200 && type == IMSRC_Graphic)
        {
            width  = 320;
            height = 200;
            solid  = true;
            type   = IMSRC_Raw320x200;
        }
        // check for AUTOPAGE images, which are raw 320x158
        else if (packfile_len == 320 * 158 && type == IMSRC_Graphic)
        {
            width  = 320;
            height = 158;
            solid  = true;
            type   = IMSRC_Raw320x200;
        }
        // check for flats
        else if ((packfile_len == 64 * 64 || packfile_len == 64 * 65 || packfile_len == 64 * 128) &&
                 type == IMSRC_Graphic)
        {
            width  = 64;
            height = 64;
            solid  = true;
            type   = IMSRC_Flat;
        }
        else
        {
            I_Warning("Graphic '%s' does not seem to be a graphic.\n", name);
            return nullptr;
        }
    }
    else if (fmt == kDoomImage)
    {
        // close it
        delete f;

        const patch_t *pat = (patch_t *)header;

        width    = AlignedLittleEndianS16(pat->width);
        height   = AlignedLittleEndianS16(pat->height);
        offset_x = AlignedLittleEndianS16(pat->leftoffset);
        offset_y = AlignedLittleEndianS16(pat->topoffset);

        is_patch = true;
    }
    else // PNG, TGA or JPEG
    {
        if (!Image_GetInfo(f, &width, &height, &bpp) || width <= 0 || height <= 0)
        {
            I_Warning("Error scanning image in '%s'\n", packfile_name);
            return NULL;
        }

        solid = (bpp == 3);

        // close it
        delete f;
    }

    // create new image
    image_c *rim = NewImage(width, height, solid ? OPAC_Solid : OPAC_Unknown);

    rim->offset_x = offset_x;
    rim->offset_y = offset_y;

    rim->name = name;

    flatdef_c *current_flatdef = flatdefs.Find(rim->name.c_str());

    if (current_flatdef && !current_flatdef->liquid.empty())
    {
        if (epi::StringCaseCompareASCII(current_flatdef->liquid, "THIN") == 0)
            rim->liquid_type = LIQ_Thin;
        else if (epi::StringCaseCompareASCII(current_flatdef->liquid, "THICK") == 0)
            rim->liquid_type = LIQ_Thick;
    }

    rim->source_type                  = type;
    int pn_len                        = strlen(packfile_name);
    rim->source.graphic.packfile_name = (char *)calloc(pn_len + 1, 1);
    Z_StrNCpy(rim->source.graphic.packfile_name, packfile_name, pn_len);
    rim->source.graphic.is_patch = is_patch;
    rim->source.graphic.user_defined =
        false; // This should only get set to true with DDFIMAGE specified DOOM format images
    // rim->source_palette = W_GetPaletteForLump(lump);
    rim->source_palette = -1;

    if (replaces)
    {
        rim->scale_x = replaces->actual_w / (float)width;
        rim->scale_y = replaces->actual_h / (float)height;

        if (!is_patch && replaces->source_type == IMSRC_Sprite)
        {
            rim->offset_x = replaces->offset_x;
            rim->offset_y = replaces->offset_y;
        }
    }

    container.push_back(rim);

    return rim;
}

static image_c *AddImage_Smart(const char *name, image_source_e type, int lump, real_image_container_c &container,
                               const image_c *replaces = NULL)
{
    /* used for Graphics, Sprites and TX/HI stuff */

    int lump_len = W_LumpLength(lump);

    epi::File *f = W_OpenLump(lump);
    SYS_ASSERT(f);

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
    auto fmt        = Image_DetectFormat(header, header_len, lump_len);

    if (fmt == kOtherImage)
    {
        // close it
        delete f;

        I_Warning("Unsupported image format in '%s' lump\n", W_GetLumpName(lump));
        return NULL;
    }
    else if (fmt == kUnknownImage)
    {
        // close it
        delete f;

        // check for Heretic/Hexen images, which are raw 320x200
        if (lump_len == 320 * 200 && type == IMSRC_Graphic)
        {
            width  = 320;
            height = 200;
            solid  = true;
            type   = IMSRC_Raw320x200;
        }
        // check for AUTOPAGE images, which are raw 320x158
        else if (lump_len == 320 * 158 && type == IMSRC_Graphic)
        {
            width  = 320;
            height = 158;
            solid  = true;
            type   = IMSRC_Raw320x200;
        }
        // check for flats
        else if ((lump_len == 64 * 64 || lump_len == 64 * 65 || lump_len == 64 * 128) && type == IMSRC_Graphic)
        {
            width  = 64;
            height = 64;
            solid  = true;
            type   = IMSRC_Flat;
        }
        else
        {
            I_Warning("Graphic '%s' does not seem to be a graphic.\n", name);
            return nullptr;
        }
    }
    else if (fmt == kDoomImage)
    {
        // close it
        delete f;

        const patch_t *pat = (patch_t *)header;

        width    = AlignedLittleEndianS16(pat->width);
        height   = AlignedLittleEndianS16(pat->height);
        offset_x = AlignedLittleEndianS16(pat->leftoffset);
        offset_y = AlignedLittleEndianS16(pat->topoffset);

        is_patch = true;
    }
    else // PNG, TGA or JPEG
    {
        if (!Image_GetInfo(f, &width, &height, &bpp) || width <= 0 || height <= 0)
        {
            I_Warning("Error scanning image in '%s' lump\n", W_GetLumpName(lump));
            return NULL;
        }

        solid = (bpp == 3);

        // close it
        delete f;
    }

    // create new image
    image_c *rim = NewImage(width, height, solid ? OPAC_Solid : OPAC_Unknown);

    rim->offset_x = offset_x;
    rim->offset_y = offset_y;

    rim->name = name;

    flatdef_c *current_flatdef = flatdefs.Find(rim->name.c_str());

    if (current_flatdef && !current_flatdef->liquid.empty())
    {
        if (epi::StringCaseCompareASCII(current_flatdef->liquid, "THIN") == 0)
            rim->liquid_type = LIQ_Thin;
        else if (epi::StringCaseCompareASCII(current_flatdef->liquid, "THICK") == 0)
            rim->liquid_type = LIQ_Thick;
    }

    rim->source_type             = type;
    rim->source.graphic.lump     = lump;
    rim->source.graphic.is_patch = is_patch;
    rim->source.graphic.user_defined =
        false; // This should only get set to true with DDFIMAGE specified DOOM format images
    rim->source_palette = W_GetPaletteForLump(lump);

    if (replaces)
    {
        rim->scale_x = replaces->actual_w / (float)width;
        rim->scale_y = replaces->actual_h / (float)height;

        if (!is_patch && replaces->source_type == IMSRC_Sprite)
        {
            rim->offset_x = replaces->offset_x;
            rim->offset_y = replaces->offset_y;
        }
    }

    container.push_back(rim);

    return rim;
}

static image_c *AddImageTexture(const char *name, texturedef_t *tdef)
{
    image_c *rim;

    rim = NewImage(tdef->width, tdef->height);

    rim->name = name;

    if (tdef->scale_x)
        rim->scale_x = 8.0 / tdef->scale_x;
    if (tdef->scale_y)
        rim->scale_y = 8.0 / tdef->scale_y;

    rim->source_type         = IMSRC_Texture;
    rim->source.texture.tdef = tdef;
    rim->source_palette      = tdef->palette_lump;

    real_textures.push_back(rim);

    return rim;
}

static image_c *AddImageFlat(const char *name, int lump)
{
    image_c *rim;
    int      len, size;

    len = W_LumpLength(lump);

    switch (len)
    {
    case 64 * 64:
        size = 64;
        break;

    // support for odd-size Heretic flats
    case 64 * 65:
        size = 64;
        break;

    // support for odd-size Hexen flats
    case 64 * 128:
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
        return NULL;
    }

    rim = NewImage(size, size, OPAC_Solid);

    rim->name = name;

    rim->source_type      = IMSRC_Flat;
    rim->source.flat.lump = lump;
    rim->source_palette   = W_GetPaletteForLump(lump);

    flatdef_c *current_flatdef = flatdefs.Find(rim->name.c_str());

    if (current_flatdef && !current_flatdef->liquid.empty())
    {
        if (epi::StringCaseCompareASCII(current_flatdef->liquid, "THIN") == 0)
            rim->liquid_type = LIQ_Thin;
        else if (epi::StringCaseCompareASCII(current_flatdef->liquid, "THICK") == 0)
            rim->liquid_type = LIQ_Thick;
    }

    real_flats.push_back(rim);

    return rim;
}

static image_c *AddImage_DOOM(imagedef_c *def, bool user_defined = false)
{
    const char *name      = def->name.c_str();
    const char *lump_name = def->info.c_str();

    image_c *rim = NULL;

    if (def->type == IMGDT_Package)
    {
        switch (def->belong)
        {
        case INS_Graphic:
            rim = AddImage_SmartPack(name, IMSRC_Graphic, lump_name, real_graphics);
            break;
        case INS_Texture:
            rim = AddImage_SmartPack(name, IMSRC_Texture, lump_name, real_textures);
            break;
        case INS_Flat:
            rim = AddImage_SmartPack(name, IMSRC_Flat, lump_name, real_flats);
            break;
        case INS_Sprite:
            rim = AddImage_SmartPack(name, IMSRC_Sprite, lump_name, real_sprites);
            break;

        default:
            I_Error("INTERNAL ERROR: Bad belong value: %d\n", def->belong);
        }
    }
    else
    {
        switch (def->belong)
        {
        case INS_Graphic:
            rim = AddImage_Smart(name, IMSRC_Graphic, W_GetNumForName(lump_name), real_graphics);
            break;
        case INS_Texture:
            rim = AddImage_Smart(name, IMSRC_Texture, W_GetNumForName(lump_name), real_textures);
            break;
        case INS_Flat:
            rim = AddImage_Smart(name, IMSRC_Flat, W_GetNumForName(lump_name), real_flats);
            break;
        case INS_Sprite:
            rim = AddImage_Smart(name, IMSRC_Sprite, W_GetNumForName(lump_name), real_sprites);
            break;

        default:
            I_Error("INTERNAL ERROR: Bad belong value: %d\n", def->belong);
        }
    }

    if (rim == NULL)
    {
        I_Warning("Unable to add image lump: %s\n", lump_name);
        return NULL;
    }

    rim->offset_x += def->x_offset;
    rim->offset_y += def->y_offset;

    rim->scale_x = def->scale * def->aspect;
    rim->scale_y = def->scale;

    rim->is_font = def->is_font;

    rim->hsv_rotation   = def->hsv_rotation;
    rim->hsv_saturation = def->hsv_saturation;
    rim->hsv_value      = def->hsv_value;
    rim->blur_sigma     = def->blur_factor;

    rim->source.graphic.special = IMGSP_None;

    if (user_defined)
    {
        rim->source.graphic.user_defined = true;
        rim->source.graphic.special      = def->special;
    }

    if (def->special & IMGSP_Crosshair)
    {
        float dy = (200.0f - rim->actual_h * rim->scale_y) / 2.0f; // - WEAPONTOP;
        rim->offset_y += int(dy / rim->scale_y);
    }

    if (def->special & IMGSP_Grayscale)
        rim->grayscale = true;

    return rim;
}

static image_c *AddImageUser(imagedef_c *def)
{
    int  width = 0, height = 0, bpp = 0;
    bool solid = false;

    if (def->type == IMGDT_Lump && def->format == LIF_DOOM)
        return AddImage_DOOM(def, true);

    switch (def->type)
    {
    case IMGDT_Colour:
        width  = 8;
        height = 8;
        bpp    = 3;
        solid  = true;
        break;

    case IMGDT_Lump:
    case IMGDT_File:
    case IMGDT_Package: {
        const char *filename = def->info.c_str();

        epi::File *f = OpenUserFileOrLump(def);
        if (f == NULL)
        {
            I_Warning("Unable to open image %s: %s\n", (def->type == IMGDT_Lump) ? "lump" : "file", filename);
            return NULL;
        }

        int file_size = f->GetLength();

        // determine format and size information.
        // for FILE and PACK get format from filename, but note that when
        // it is wrong (like a PNG called "foo.jpeg"), it can still work.
        ImageFormat fmt = kUnknownImage;

        if (def->type == IMGDT_Lump)
        {
            uint8_t header[32];
            memset(header, 255, sizeof(header));

            f->Read(header, sizeof(header));
            f->Seek(0, epi::File::kSeekpointStart);

            int header_len = HMM_MIN((int)sizeof(header), file_size);
            fmt            = Image_DetectFormat(header, header_len, file_size);
        }
        else
            fmt = Image_FilenameToFormat(def->info);

        // when a lump uses DOOM patch format, use the other method.
        // for lumps, assume kUnknownImage is a mis-detection of DOOM patch
        // and hope for the best.
        if (fmt == kDoomImage || fmt == kUnknownImage)
        {
            delete f; // close file

            if (fmt == kDoomImage)
                return AddImage_DOOM(def, true);

            I_Warning("Unknown image format in: %s\n", filename);
            return NULL;
        }

        if (fmt == kOtherImage)
        {
            delete f;
            I_Warning("Unsupported image format in: %s\n", filename);
            return NULL;
        }

        if (!Image_GetInfo(f, &width, &height, &bpp))
        {
            delete f;
            I_Warning("Error occurred scanning image: %s\n", filename);
            return NULL;
        }

        // close it
        delete f;

        solid = (bpp == 3);
    }
    break;

    default:
        I_Error("AddImageUser: Coding error, unknown type %d\n", def->type);
        return NULL; /* NOT REACHED */
    }

    image_c *rim = NewImage(width, height, solid ? OPAC_Solid : OPAC_Unknown);

    rim->name = def->name;

    rim->offset_x = def->x_offset;
    rim->offset_y = def->y_offset;

    rim->scale_x = def->scale * def->aspect;
    rim->scale_y = def->scale;

    rim->source_type     = IMSRC_User;
    rim->source.user.def = def;

    rim->is_font = def->is_font;

    rim->hsv_rotation   = def->hsv_rotation;
    rim->hsv_saturation = def->hsv_saturation;
    rim->hsv_value      = def->hsv_value;
    rim->blur_sigma     = def->blur_factor;

    if (def->special & IMGSP_Crosshair)
    {
        float dy = (200.0f - rim->actual_h * rim->scale_y) / 2.0f; // - WEAPONTOP;
        rim->offset_y += int(dy / rim->scale_y);
    }

    if (def->special & IMGSP_Grayscale)
        rim->grayscale = true;

    switch (def->belong)
    {
    case INS_Graphic:
        real_graphics.push_back(rim);
        break;
    case INS_Texture:
        real_textures.push_back(rim);
        break;
    case INS_Flat:
        real_flats.push_back(rim);
        break;
    case INS_Sprite:
        real_sprites.push_back(rim);
        break;

    default:
        I_Error("INTERNAL ERROR: Bad belong value: %d\n", def->belong);
    }

    if (def->special & IMGSP_Precache)
        W_ImagePreCache(rim);

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
void W_ImageCreateFlats(std::vector<int> &lumps)
{
    for (size_t i = 0; i < lumps.size(); i++)
    {
        if (lumps[i] >= 0)
        {
            const char *name = W_GetLumpName(lumps[i]);
            AddImageFlat(name, lumps[i]);
        }
    }
}

//
// Used to fill in the image array with textures from the WAD.  The
// list of texture definitions comes from each TEXTURE1/2 lump in each
// existing wad file, with duplicates set to NULL.
//
// NOTE: should only be called once, as it assumes none of the
// textures in the list have names colliding with existing texture
// images.
//
void W_ImageCreateTextures(struct texturedef_s **defs, int number)
{
    int i;

    SYS_ASSERT(defs);

    for (i = 0; i < number; i++)
    {
        if (defs[i] == NULL)
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
const image_c *W_ImageCreateSprite(const char *name, int lump, bool is_weapon)
{
    SYS_ASSERT(lump >= 0);

    image_c *rim = AddImage_Smart(name, IMSRC_Sprite, lump, real_sprites);
    if (!rim)
        return NULL;

    // adjust sprite offsets so that (0,0) is normal
    if (is_weapon)
    {
        rim->offset_x += (320.0f / 2.0f - rim->actual_w / 2.0f); // loss of accuracy
        rim->offset_y += (200.0f - 32.0f - rim->actual_h);
    }
    else
    {
        // rim->offset_x -= rim->actual_w / 2;   // loss of accuracy
        rim->offset_x -= ((float)rim->actual_w) / 2.0f; // Lobo 2023: dancing eye fix
        rim->offset_y -= rim->actual_h;
    }

    return rim;
}

const image_c *W_ImageCreatePackSprite(std::string packname, pack_file_c *pack, bool is_weapon)
{
    SYS_ASSERT(pack);

    image_c *rim = AddImage_SmartPack(epi::GetStem(packname).c_str(), IMSRC_Sprite, packname.c_str(),
                                      real_sprites);
    if (!rim)
        return NULL;

    // adjust sprite offsets so that (0,0) is normal
    if (is_weapon)
    {
        rim->offset_x += (320.0f / 2.0f - rim->actual_w / 2.0f); // loss of accuracy
        rim->offset_y += (200.0f - 32.0f - rim->actual_h);
    }
    else
    {
        // rim->offset_x -= rim->actual_w / 2;   // loss of accuracy
        rim->offset_x -= ((float)rim->actual_w) / 2.0f; // Lobo 2023: dancing eye fix
        rim->offset_y -= rim->actual_h;
    }

    return rim;
}

//
// Add the images defined in IMAGES.DDF.
//
void W_ImageCreateUser(void)
{
    I_Printf("Adding DDFIMAGE definitions...\n");

    for (auto def : imagedefs)
    {
        if (def == NULL)
            continue;

        if (def->belong != INS_Patch)
            AddImageUser(def);
    }

#if 0
	L_WriteDebug("Textures -----------------------------\n");
	do_DebugDump(real_textures);

	L_WriteDebug("Flats ------------------------------\n");
	do_DebugDump(real_flats);

	L_WriteDebug("Sprites ------------------------------\n");
	do_DebugDump(real_sprites);

	L_WriteDebug("Graphics ------------------------------\n");
	do_DebugDump(real_graphics);
#endif
}

void W_ImageAddTX(int lump, const char *name, bool hires)
{
    if (hires)
    {
        const image_c *rim = W_ImageDoLookup(real_textures, name, -2);
        if (rim && rim->source_type != IMSRC_User)
        {
            AddImage_Smart(name, IMSRC_TX_HI, lump, real_textures, rim);
            return;
        }

        rim = W_ImageDoLookup(real_flats, name, -2);
        if (rim && rim->source_type != IMSRC_User)
        {
            AddImage_Smart(name, IMSRC_TX_HI, lump, real_flats, rim);
            return;
        }

        rim = W_ImageDoLookup(real_sprites, name, -2);
        if (rim && rim->source_type != IMSRC_User)
        {
            AddImage_Smart(name, IMSRC_TX_HI, lump, real_sprites, rim);
            return;
        }

        // we do it this way to force the original graphic to be loaded
        rim = W_ImageLookup(name, INS_Graphic, ILF_Exact | ILF_Null);

        if (rim && rim->source_type != IMSRC_User)
        {
            AddImage_Smart(name, IMSRC_TX_HI, lump, real_graphics, rim);
            return;
        }

        I_Warning("HIRES replacement '%s' has no counterpart.\n", name);
    }

    AddImage_Smart(name, IMSRC_TX_HI, lump, real_textures);
}

//
// Only used during sprite initialisation.  The returned array of
// images is guaranteed to be sorted by name.
//
// Use delete[] to free the returned array.
//
const image_c **W_ImageGetUserSprites(int *count)
{
    // count number of user sprites
    (*count) = 0;

    real_image_container_c::iterator it;

    for (it = real_sprites.begin(); it != real_sprites.end(); it++)
    {
        image_c *rim = *it;

        if (rim->source_type == IMSRC_User || rim->source.graphic.user_defined)
            (*count) += 1;
    }

    if (*count == 0)
    {
        L_WriteDebug("W_ImageGetUserSprites(count = %d)\n", *count);
        return NULL;
    }

    const image_c **array = new const image_c *[*count];
    int             pos   = 0;

    for (it = real_sprites.begin(); it != real_sprites.end(); it++)
    {
        image_c *rim = *it;

        if (rim->source_type == IMSRC_User || rim->source.graphic.user_defined)
            array[pos++] = rim;
    }

#define CMP(a, b) (strcmp(W_ImageGetName(a), W_ImageGetName(b)) < 0)
    QSORT(const image_c *, array, (*count), CUTOFF);
#undef CMP

#if 0 // DEBUGGING
	L_WriteDebug("W_ImageGetUserSprites(count = %d)\n", *count);
	L_WriteDebug("{\n");

	for (pos = 0; pos < *count; pos++)
		L_WriteDebug("   %p = [%s] %dx%d\n", array[pos], W_ImageGetName(array[pos]),
			array[pos]->actual_w, array[pos]->actual_h);
		
	L_WriteDebug("}\n");
#endif

    return array;
}

//----------------------------------------------------------------------------
//
//  IMAGE LOADING / UNLOADING
//

// TODO: make methods of image_c class
static bool IM_ShouldClamp(const image_c *rim)
{
    switch (rim->source_type)
    {
    case IMSRC_Graphic:
    case IMSRC_Raw320x200:
    case IMSRC_Sprite:
        return true;

    case IMSRC_User:
        switch (rim->source.user.def->belong)
        {
        case INS_Graphic:
        case INS_Sprite:
            return true;

        default:
            return false;
        }

    default:
        return false;
    }
}

static bool IM_ShouldMipmap(image_c *rim)
{
    // the "SKY" check here is a hack...
    if (epi::StringPrefixCaseCompareASCII(rim->name, "SKY") == 0)
        return false;

    switch (rim->source_type)
    {
    case IMSRC_Texture:
    case IMSRC_Flat:
    case IMSRC_TX_HI:
        return true;

    case IMSRC_User:
        switch (rim->source.user.def->belong)
        {
        case INS_Texture:
        case INS_Flat:
            return true;

        default:
            return false;
        }

    default:
        return false;
    }
}

static bool IM_ShouldSmooth(image_c *rim)
{
    if (rim->blur_sigma > 0.0f)
        return true;

    return var_smoothing ? true : false;
}

static bool IM_ShouldHQ2X(image_c *rim)
{
    // Note: no need to check IMSRC_User, since those images are
    //       always PNG or JPEG (etc) and never palettised, hence
    //       the Hq2x scaling would never apply.

    if (hq2x_scaling == 0)
        return false;

    if (hq2x_scaling >= 3)
        return true;

    switch (rim->source_type)
    {
    case IMSRC_Graphic:
    case IMSRC_Raw320x200:
        // UI elements
        return true;
#if 0
		case IMSRC_Texture:
			// the "SKY" check here is a hack...
			if (epi::StringPrefixCaseCompareASCII(rim->name, "SKY") == 0)
				return true;
			break;
#endif
    case IMSRC_Sprite:
        if (hq2x_scaling >= 2)
            return true;
        break;

    default:
        break;
    }

    return false;
}

static int IM_PixelLimit(image_c *rim)
{
    if (detail_level == 0)
        return (1 << 18);
    else if (detail_level == 1)
        return (1 << 20);
    else
        return (1 << 22);
}

static GLuint LoadImageOGL(image_c *rim, const colourmap_c *trans, bool do_whiten)
{
    bool clamp  = IM_ShouldClamp(rim);
    bool mip    = IM_ShouldMipmap(rim);
    bool smooth = IM_ShouldSmooth(rim);

    int max_pix = IM_PixelLimit(rim);

    if (rim->source_type == IMSRC_User)
    {
        if (rim->source.user.def->special & IMGSP_Clamp)
            clamp = true;

        if (rim->source.user.def->special & IMGSP_Mip)
            mip = true;
        else if (rim->source.user.def->special & IMGSP_NoMip)
            mip = false;

        if (rim->source.user.def->special & IMGSP_Smooth)
            smooth = true;
        else if (rim->source.user.def->special & IMGSP_NoSmooth)
            smooth = false;
    }
    else if (rim->source_type == IMSRC_Graphic && rim->source.graphic.user_defined)
    {
        if (rim->source.graphic.special & IMGSP_Clamp)
            clamp = true;

        if (rim->source.graphic.special & IMGSP_Mip)
            mip = true;
        else if (rim->source.graphic.special & IMGSP_NoMip)
            mip = false;

        if (rim->source.graphic.special & IMGSP_Smooth)
            smooth = true;
        else if (rim->source.graphic.special & IMGSP_NoSmooth)
            smooth = false;
    }

    const uint8_t *what_palette    = (const uint8_t *)&playpal_data[0];
    bool        what_pal_cached = false;

    static uint8_t trans_pal[256 * 3];

    if (trans != NULL)
    {
        // Note: we don't care about source_palette here. It's likely that
        // the translation table itself would not match the other palette,
        // and so we would still end up with messed up colours.

        R_TranslatePalette(trans_pal, what_palette, trans);
        what_palette = trans_pal;
    }
    else if (rim->source_palette >= 0)
    {
        what_palette    = (const uint8_t *)W_LoadLump(rim->source_palette);
        what_pal_cached = true;
    }

    image_data_c *tmp_img = ReadAsEpiBlock(rim);

    if (rim->liquid_type > LIQ_None && (swirling_flats == SWIRL_SMMU || swirling_flats == SWIRL_SMMUSWIRL))
    {
        rim->swirled_gametic = hudtic / (r_doubleframes.d ? 2 : 1);
        tmp_img->Swirl(rim->swirled_gametic,
                       rim->liquid_type); // Using leveltime disabled swirl for intermission screens
    }

    if (rim->opacity == OPAC_Unknown)
        rim->opacity = R_DetermineOpacity(tmp_img, &rim->is_empty);

    if ((tmp_img->bpp == 1) && IM_ShouldHQ2X(rim))
    {
        bool solid = (rim->opacity == OPAC_Solid);

        Hq2x::Setup(what_palette, solid ? -1 : TRANS_PIXEL);

        image_data_c *scaled_img = Hq2x::Convert(tmp_img, solid, false /* invert */);

        if (rim->is_font)
        {
            scaled_img->RemoveBackground();
            rim->opacity = R_DetermineOpacity(tmp_img, &rim->is_empty);
        }

        if (rim->blur_sigma > 0.0f)
        {
            image_data_c *blurred_img = Blur::Blur(scaled_img, rim->blur_sigma);
            delete scaled_img;
            scaled_img = blurred_img;
        }

        delete tmp_img;
        tmp_img = scaled_img;
    }
    else if (tmp_img->bpp == 1)
    {
        image_data_c *rgb_img = R_PalettisedToRGB(tmp_img, what_palette, rim->opacity);

        if (rim->is_font)
        {
            rgb_img->RemoveBackground();
            rim->opacity = R_DetermineOpacity(tmp_img, &rim->is_empty);
        }

        if (rim->blur_sigma > 0.0f)
        {
            image_data_c *blurred_img = Blur::Blur(rgb_img, rim->blur_sigma);
            delete rgb_img;
            rgb_img = blurred_img;
        }

        delete tmp_img;
        tmp_img = rgb_img;
    }
    else if (tmp_img->bpp >= 3)
    {
        if (rim->is_font)
        {
            tmp_img->RemoveBackground();
            rim->opacity = R_DetermineOpacity(tmp_img, &rim->is_empty);
        }
        if (rim->blur_sigma > 0.0f)
        {
            image_data_c *blurred_img = Blur::Blur(tmp_img, rim->blur_sigma);
            delete tmp_img;
            tmp_img = blurred_img;
        }
        if (trans != NULL)
            R_PaletteRemapRGBA(tmp_img, what_palette, (const uint8_t *)&playpal_data[0]);
    }

    if (rim->hsv_rotation || rim->hsv_saturation > -1 || rim->hsv_value)
        tmp_img->SetHSV(rim->hsv_rotation, rim->hsv_saturation, rim->hsv_value);

    if (do_whiten)
        tmp_img->Whiten();

    GLuint tex_id = R_UploadTexture(tmp_img,
                                    (clamp ? UPL_Clamp : 0) | (mip ? UPL_MipMap : 0) | (smooth ? UPL_Smooth : 0) |
                                        ((rim->opacity == OPAC_Masked) ? UPL_Thresh : 0),
                                    max_pix);

    delete tmp_img;

    if (what_pal_cached)
        delete[] what_palette;

    return tex_id;
}

#if 0
static
void UnloadImageOGL(cached_image_t *rc, image_c *rim)
{
	glDeleteTextures(1, &rc->tex_id);

	for (unsigned int i = 0; i < rim->cache.size(); i++)
	{
		if (rim->cache[i] == rc)
		{
			rim->cache[i] = NULL;
			return;
		}
	}

	I_Error("INTERNAL ERROR: UnloadImageOGL: no such RC in cache !\n");
}


//
// UnloadImage
//
// Unloads a cached image from the cache list and frees all resources.
// Mainly just a switch to more specialised image unloaders.
//
static void UnloadImage(cached_image_t *rc)
{
	image_c *rim = rc->parent;

	SYS_ASSERT(rc);
	SYS_ASSERT(rc != &imagecachehead);
	SYS_ASSERT(rim);

	// unlink from the cache list
	Unlink(rc);

	UnloadImageOGL(rc, rim);

	delete rc;
}
#endif

//----------------------------------------------------------------------------
//  IMAGE LOOKUP
//----------------------------------------------------------------------------

//
// BackupTexture
//
static const image_c *BackupTexture(const char *tex_name, int flags)
{
    const image_c *rim;

    if (!(flags & ILF_Exact))
    {
        // backup plan: try a flat with the same name
        rim = W_ImageDoLookup(real_flats, tex_name);
        if (rim)
            return rim;

        // backup backup plan: try a graphic with the same name
        rim = W_ImageDoLookup(real_graphics, tex_name);
        if (rim)
            return rim;

        // backup backup backup plan: see if it's a graphic in the P/PP_START P/PP_END namespace
        // and make/return an image if valid
        int checkfile = W_CheckFileNumForName(tex_name);
        int checklump = W_CheckNumForName(tex_name);
        if (checkfile > -1 && checklump > -1)
        {
            for (auto patch_lump : *W_GetPatchList(checkfile))
            {
                if (patch_lump == checklump)
                {
                    rim = AddImage_Smart(tex_name, IMSRC_Graphic, patch_lump, real_graphics);
                    if (rim)
                        return rim;
                }
            }
        }
    }

    if (flags & ILF_Null)
        return NULL;

    M_WarnError("Unknown texture found in level: '%s'\n", tex_name);

    image_c *dummy;

    if (epi::StringPrefixCaseCompareASCII(tex_name, "SKY") == 0)
        dummy = CreateDummyImage(tex_name, 0x0000AA, 0x55AADD);
    else
        dummy = CreateDummyImage(tex_name, 0xAA5511, 0x663300);

    // keep dummy texture so that future lookups will succeed
    real_textures.push_back(dummy);
    return dummy;
}

void W_MakeEdgeTex()
{
    real_textures.push_back(CreateDummyImage("EDGETEX", 0xAA5511, 0x663300));
}

//
// BackupFlat
//

static const image_c *BackupFlat(const char *flat_name, int flags)
{
    const image_c *rim;

    // backup plan 1: if lump exists and is right size, add it.
    if (!(flags & ILF_NoNew))
    {
        int i = W_CheckNumForName(flat_name);

        if (i >= 0)
        {
            rim = AddImageFlat(flat_name, i);
            if (rim)
                return rim;
        }
    }

    // backup plan 2: Texture with the same name ?
    if (!(flags & ILF_Exact))
    {
        rim = W_ImageDoLookup(real_textures, flat_name);
        if (rim)
            return rim;
    }

    if (flags & ILF_Null)
        return NULL;

    M_WarnError("Unknown flat found in level: '%s'\n", flat_name);

    image_c *dummy = CreateDummyImage(flat_name, 0x11AA11, 0x115511);

    // keep dummy flat so that future lookups will succeed
    real_flats.push_back(dummy);
    return dummy;
}

void W_MakeEdgeFlat()
{
    real_flats.push_back(CreateDummyImage("EDGEFLAT", 0x11AA11, 0x115511));
}

//
// BackupGraphic
//
static const image_c *BackupGraphic(const char *gfx_name, int flags)
{
    const image_c *rim;

    // backup plan 1: look for sprites and heretic-background
    if ((flags & (ILF_Exact | ILF_Font)) == 0)
    {
        rim = W_ImageDoLookup(real_graphics, gfx_name, IMSRC_Raw320x200);
        if (rim)
            return rim;

        rim = W_ImageDoLookup(real_sprites, gfx_name);
        if (rim)
            return rim;
    }

    // not already loaded ?  Check if lump exists in wad, if so add it.
    if (!(flags & ILF_NoNew))
    {
        int i = W_CheckNumForName_GFX(gfx_name);

        if (i >= 0)
        {
            rim = AddImage_Smart(gfx_name, IMSRC_Graphic, i, real_graphics);
            if (rim)
                return rim;
        }
    }

    if (flags & ILF_Null)
        return NULL;

    M_DebugError("Unknown graphic: '%s'\n", gfx_name);

    image_c *dummy;

    if (flags & ILF_Font)
        dummy = CreateDummyImage(gfx_name, 0xFFFFFF, TRANS_PIXEL);
    else
        dummy = CreateDummyImage(gfx_name, 0xFF0000, TRANS_PIXEL);

    // keep dummy graphic so that future lookups will succeed
    real_graphics.push_back(dummy);
    return dummy;
}

static const image_c *BackupSprite(const char *spr_name, int flags)
{
    if (flags & ILF_Null)
        return NULL;

    return W_ImageForDummySprite();
}

const image_c *W_ImageLookup(const char *name, image_namespace_e type, int flags)
{
    //
    // Note: search must be case insensitive.
    //

    // "NoTexture" marker.
    if (!name || !name[0] || name[0] == '-')
        return NULL;

    // "Sky" marker.
    if (type == INS_Flat && (epi::StringCaseCompareASCII(name, "F_SKY1") == 0 || epi::StringCaseCompareASCII(name, "F_SKY") == 0))
    {
        return skyflatimage;
    }

    // compatibility hack (first texture in IWAD is a dummy)
    if (type == INS_Texture && ((epi::StringCaseCompareASCII(name, "AASTINKY") == 0) || (epi::StringCaseCompareASCII(name, "AASHITTY") == 0) ||
                                (epi::StringCaseCompareASCII(name, "BADPATCH") == 0) || (epi::StringCaseCompareASCII(name, "ABADONE") == 0)))
    {
        return NULL;
    }

    const image_c *rim;

    if (type == INS_Texture)
    {
        rim = W_ImageDoLookup(real_textures, name);
        return rim ? rim : BackupTexture(name, flags);
    }
    if (type == INS_Flat)
    {
        rim = W_ImageDoLookup(real_flats, name);
        return rim ? rim : BackupFlat(name, flags);
    }
    if (type == INS_Sprite)
    {
        rim = W_ImageDoLookup(real_sprites, name);
        return rim ? rim : BackupSprite(name, flags);
    }

    /* INS_Graphic */

    rim = W_ImageDoLookup(real_graphics, name);
    return rim ? rim : BackupGraphic(name, flags);
}

const image_c *W_ImageForDummySprite(void)
{
    return dummy_sprite;
}

const image_c *W_ImageForDummySkin(void)
{
    return dummy_skin;
}

const image_c *W_ImageForHOMDetect(void)
{
    return dummy_hom[(framecount & 0x10) ? 1 : 0];
}

const image_c *W_ImageForFogWall(RGBAColor fog_color)
{
    std::string fogname = epi::StringFormat("FOGWALL_%d", fog_color);
    image_c    *fogwall = (image_c *)W_ImageLookup(fogname.c_str(), INS_Graphic, ILF_Null);
    if (fogwall)
        return fogwall;
    imagedef_c *fogdef = new imagedef_c;
    fogdef->colour     = fog_color;
    fogdef->name       = fogname;
    fogdef->type       = IMGDT_Colour;
    fogdef->belong     = INS_Graphic;
    fogwall            = AddImageUser(fogdef);
    return fogwall;
}

const image_c *W_ImageParseSaveString(char type, const char *name)
{
    // Used by the savegame code.

    // this name represents the sky (historical reasons)
    if (type == 'd' && epi::StringCaseCompareASCII(name, "DUMMY__2") == 0)
    {
        return skyflatimage;
    }

    switch (type)
    {
    case 'K':
        return skyflatimage;

    case 'F':
        return W_ImageLookup(name, INS_Flat);

    case 'P':
        return W_ImageLookup(name, INS_Graphic);

    case 'S':
        return W_ImageLookup(name, INS_Sprite);

    default:
        I_Warning("W_ImageParseSaveString: unknown type '%c'\n", type);
        /* FALL THROUGH */

    case 'd': /* dummy */
    case 'T':
        return W_ImageLookup(name, INS_Texture);
    }
}

void W_ImageMakeSaveString(const image_c *image, char *type, char *namebuf)
{
    // Used by the savegame code

    if (image == skyflatimage)
    {
        *type = 'K';
        strcpy(namebuf, "F_SKY1");
        return;
    }

    const image_c *rim = (const image_c *)image;

    strcpy(namebuf, rim->name.c_str());

    /* handle User images (convert to a more general type) */
    if (rim->source_type == IMSRC_User)
    {
        switch (rim->source.user.def->belong)
        {
        case INS_Texture:
            (*type) = 'T';
            return;
        case INS_Flat:
            (*type) = 'F';
            return;
        case INS_Sprite:
            (*type) = 'S';
            return;

        default: /* INS_Graphic */
            (*type) = 'P';
            return;
        }
    }

    switch (rim->source_type)
    {
    case IMSRC_Raw320x200:
    case IMSRC_Graphic:
        (*type) = 'P';
        return;

    case IMSRC_TX_HI:
    case IMSRC_Texture:
        (*type) = 'T';
        return;

    case IMSRC_Flat:
        (*type) = 'F';
        return;

    case IMSRC_Sprite:
        (*type) = 'S';
        return;

    case IMSRC_Dummy:
        (*type) = 'd';
        return;

    default:
        I_Error("W_ImageMakeSaveString: bad type %d\n", rim->source_type);
        break;
    }
}

const char *W_ImageGetName(const image_c *image)
{
    const image_c *rim;

    rim = (const image_c *)image;

    return rim->name.c_str();
}

//----------------------------------------------------------------------------
//
//  IMAGE USAGE
//

static cached_image_t *ImageCacheOGL(image_c *rim, const colourmap_c *trans, bool do_whiten)
{
    // check if image + translation is already cached

    int free_slot = -1;

    cached_image_t *rc = NULL;

    for (int i = 0; i < (int)rim->cache.size(); i++)
    {
        rc = rim->cache[i];

        if (!rc)
        {
            free_slot = i;
            continue;
        }

        if (do_whiten && rc->is_whitened)
            break;

        if (rc->trans_map == trans)
        {
            if (do_whiten)
            {
                if (rc->is_whitened)
                    break;
            }
            else if (!rc->is_whitened)
                break;
        }

        rc = NULL;
    }

    if (!rc)
    {
        // add entry into cache
        rc = new cached_image_t;

        rc->parent      = rim;
        rc->trans_map   = trans;
        rc->hue         = kRGBANoValue;
        rc->tex_id      = 0;
        rc->is_whitened = do_whiten ? true : false;

        InsertAtTail(rc);

        if (free_slot >= 0)
            rim->cache[free_slot] = rc;
        else
            rim->cache.push_back(rc);
    }

    SYS_ASSERT(rc);

    if (rim->liquid_type > LIQ_None && (swirling_flats == SWIRL_SMMU || swirling_flats == SWIRL_SMMUSWIRL))
    {
        if (!erraticism_active && !time_stop_active && rim->swirled_gametic != hudtic / (r_doubleframes.d ? 2 : 1))
        {
            if (rc->tex_id != 0)
            {
                glDeleteTextures(1, &rc->tex_id);
                rc->tex_id = 0;
            }
        }
    }

    if (rc->tex_id == 0)
    {
        // load image into cache
        rc->tex_id = LoadImageOGL(rim, trans, do_whiten);
    }

    return rc;
}

//
// The top-level routine for caching in an image.  Mainly just a
// switch to more specialised routines.
//
GLuint W_ImageCache(const image_c *image, bool anim, const colourmap_c *trans, bool do_whiten)
{
    // Intentional Const Override
    image_c *rim = (image_c *)image;

    // handle animations
    if (anim)
    {
        if (rim->liquid_type == LIQ_None || swirling_flats == SWIRL_Vanilla)
            rim = rim->anim.cur;
    }

    if (rim->grayscale)
        do_whiten = true;

    cached_image_t *rc = ImageCacheOGL(rim, trans, do_whiten);

    SYS_ASSERT(rc->parent);

    return rc->tex_id;
}

#if 0
RGBAColor W_ImageGetHue(const image_c *img)
{
	SYS_ASSERT(c);

	// Intentional Const Override
	cached_image_t *rc = ((cached_image_t *) c) - 1;

	SYS_ASSERT(rc->parent);

	return rc->hue;
}
#endif

void W_ImagePreCache(const image_c *image)
{
    W_ImageCache(image, false);

    // Intentional Const Override
    image_c *rim = (image_c *)image;

    // pre-cache alternative images for switches too
    if (rim->name.size() >= 4 &&
        (epi::StringPrefixCaseCompareASCII(rim->name, "SW1") == 0 || epi::StringPrefixCaseCompareASCII(rim->name, "SW2") == 0))
    {
        std::string alt_name = rim->name;

        alt_name[2] = (alt_name[2] == '1') ? '2' : '1';

        image_c *alt = W_ImageDoLookup(real_textures, alt_name.c_str());

        if (alt)
            W_ImageCache(alt, false);
    }
}

//----------------------------------------------------------------------------

static void W_CreateDummyImages(void)
{
    dummy_sprite = CreateDummyImage("DUMMY_SPRITE", 0xFFFF00, TRANS_PIXEL);
    dummy_skin   = CreateDummyImage("DUMMY_SKIN", 0xFF77FF, 0x993399);

    skyflatimage = CreateDummyImage("DUMMY_SKY", 0x0000AA, 0x55AADD);

    dummy_hom[0] = CreateDummyImage("DUMMY_HOM1", 0xFF3333, 0x000000);
    dummy_hom[1] = CreateDummyImage("DUMMY_HOM2", 0x000000, 0xFF3333);

    // make the dummy sprite easier to see
    {
        // Intentional Const Override
        image_c *dsp = (image_c *)dummy_sprite;

        dsp->scale_x = 3.0f;
        dsp->scale_y = 3.0f;
    }
}

//
// Initialises the image system.
//
bool W_InitImages(void)
{
    // check options
    if (argv::Find("nosmoothing") > 0)
        var_smoothing = 0;
    else if (argv::Find("smoothing") > 0)
        var_smoothing = 1;

    W_CreateDummyImages();

    return true;
}

//
// Animate all the images.
//
void W_UpdateImageAnims(void)
{
    do_Animate(real_graphics);
    if (gamestate < GS_LEVEL)
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

void W_DeleteAllImages(void)
{
    std::list<cached_image_t *>::iterator CI;

    for (CI = image_cache.begin(); CI != image_cache.end(); CI++)
    {
        cached_image_t *rc = *CI;
        SYS_ASSERT(rc);

        if (rc->tex_id != 0)
        {
            glDeleteTextures(1, &rc->tex_id);
            rc->tex_id = 0;
        }
    }

    DeleteSkyTextures();
    DeleteColourmapTextures();
}

//
// W_AnimateImageSet
//
// Sets up the images so they will animate properly.  Array is
// allowed to contain NULL entries.
//
// NOTE: modifies the input array of images.
//
void W_AnimateImageSet(const image_c **images, int number, int speed)
{
    int      i, total;
    image_c *rim, *other;

    SYS_ASSERT(images);
    SYS_ASSERT(speed > 0);

    // ignore images that are already animating
    for (i = 0, total = 0; i < number; i++)
    {
        // Intentional Const Override
        rim = (image_c *)images[i];

        if (!rim)
            continue;

        if (rim->anim.speed > 0)
        {
            // Make new image but keep it out of the lookup list ? - Dasho
            // I don't think image_c class has a CopyDetail function...is it worth it for this one use?
            image_c *dupe_image        = new image_c;
            dupe_image->name           = rim->name;
            dupe_image->actual_h       = rim->actual_h;
            dupe_image->actual_w       = rim->actual_w;
            dupe_image->cache          = rim->cache;
            dupe_image->is_empty       = rim->is_empty;
            dupe_image->is_font        = rim->is_font;
            dupe_image->liquid_type    = rim->liquid_type;
            dupe_image->offset_x       = rim->offset_x;
            dupe_image->offset_y       = rim->offset_y;
            dupe_image->opacity        = rim->opacity;
            dupe_image->ratio_h        = rim->ratio_h;
            dupe_image->ratio_w        = rim->ratio_w;
            dupe_image->scale_x        = rim->scale_x;
            dupe_image->scale_y        = rim->scale_y;
            dupe_image->source         = rim->source;
            dupe_image->source_palette = rim->source_palette;
            dupe_image->source_type    = rim->source_type;
            dupe_image->total_h        = rim->total_h;
            dupe_image->total_w        = rim->total_w;
            rim                        = dupe_image;
        }

        images[total++] = rim;
    }

    // anything left to animate ?
    if (total < 2)
        return;

    for (i = 0; i < total; i++)
    {
        // Intentional Const Override
        rim   = (image_c *)images[i];
        other = (image_c *)images[(i + 1) % total];

        rim->anim.next  = other;
        rim->anim.speed = rim->anim.count = speed;
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
