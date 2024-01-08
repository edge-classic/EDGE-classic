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

#include "endianess.h"
#include "file.h"
#include "filesystem.h"

#include "image_data.h"
#include "image_hq2x.h"
#include "image_funcs.h"

#include "dm_state.h"
#include "e_search.h"
#include "e_main.h"
#include "m_argv.h"
#include "m_misc.h"
#include "p_local.h"
#include "r_defs.h"
#include "r_image.h"
#include "r_gldefs.h"
#include "r_sky.h"
#include "r_colormap.h"
#include "r_texgl.h"
#include "w_files.h"
#include "w_texture.h"
#include "w_wad.h"

// posts are runs of non masked source pixels
typedef struct
{
    // -1 is the last post in a column
    uint8_t topdelta;

    // length data bytes follows
    uint8_t length; // length data bytes follows
} post_t;

// column_t is a list of 0 or more post_t, (byte)-1 terminated
typedef post_t column_t;

#define TRANS_REPLACE pal_black

// Dummy image, for when texture/flat/graphic is unknown.  Row major
// order.  Could be packed, but why bother ?
static uint8_t dummy_graphic[DUMMY_X * DUMMY_Y] = {
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

#define PIXEL_RED(pix) (what_palette[pix * 3 + 0])
#define PIXEL_GRN(pix) (what_palette[pix * 3 + 1])
#define PIXEL_BLU(pix) (what_palette[pix * 3 + 2])

#define GAMMA_RED(pix) GAMMA_CONV(PIXEL_RED(pix))
#define GAMMA_GRN(pix) GAMMA_CONV(PIXEL_GRN(pix))
#define GAMMA_BLU(pix) GAMMA_CONV(PIXEL_BLU(pix))

static void DrawColumnIntoEpiBlock(image_c *rim, epi::image_data_c *img, const column_t *patchcol, int x, int y)
{
    SYS_ASSERT(patchcol);

    int w1 = rim->actual_w;
    int h1 = rim->actual_h;
    int w2 = rim->total_w;

    // clip horizontally
    if (x < 0 || x >= w1)
        return;

    int top = -1;

    while (patchcol->topdelta != 0xFF)
    {
        int delta = patchcol->topdelta;
        int count = patchcol->length;

        const uint8_t *src  = (const uint8_t *)patchcol + 3;
        uint8_t       *dest = img->pixels + x;

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

            if (*src == TRANS_PIXEL)
                dest[(h1 - 1 - y2) * w2] = TRANS_REPLACE;
            else
                dest[(h1 - 1 - y2) * w2] = *src;
        }

        // jump to next column
        patchcol = (const column_t *)((const uint8_t *)patchcol + patchcol->length + 4);
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
static epi::image_data_c *ReadFlatAsEpiBlock(image_c *rim)
{
    SYS_ASSERT(rim->source_type == IMSRC_Flat || rim->source_type == IMSRC_Raw320x200);

    int tw = MAX(rim->total_w, 1);
    int th = MAX(rim->total_h, 1);

    int w = rim->actual_w;
    int h = rim->actual_h;

    epi::image_data_c *img = new epi::image_data_c(tw, th, 1);

    uint8_t *dest = img->pixels;

#ifdef MAKE_TEXTURES_WHITE
    img->Clear(pal_white);
    return img;
#endif

    // clear initial image to black
    img->Clear(pal_black);

    // read in pixels
    const uint8_t *src = (const uint8_t *)W_LoadLump(rim->source.flat.lump);

    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
           uint8_t src_pix = src[y * w + x];

            uint8_t *dest_pix = &dest[(h - 1 - y) * tw + x];

            // make sure TRANS_PIXEL values (which do not occur naturally in
            // Doom images) are properly remapped.
            if (src_pix == TRANS_PIXEL)
                dest_pix[0] = TRANS_REPLACE;
            else
                dest_pix[0] = src_pix;
        }

    delete[] src;

    // CW: Textures MUST tile! If actual size not total size, manually tile
    // [ AJA: this does not make them tile, just fills in the black gaps ]
    img->FillMarginX(rim->actual_w);
    img->FillMarginY(rim->actual_h);

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
static epi::image_data_c *ReadTextureAsEpiBlock(image_c *rim)
{
    SYS_ASSERT(rim->source_type == IMSRC_Texture);

    texturedef_t *tdef = rim->source.texture.tdef;
    SYS_ASSERT(tdef);

    int tw = rim->total_w;
    int th = rim->total_h;

    epi::image_data_c *img = new epi::image_data_c(tw, th, 1);

#ifdef MAKE_TEXTURES_WHITE
    img->Clear(pal_white);
    return img;
#endif

    // Clear initial pixels to either totally transparent, or totally
    // black (if we know the image should be solid).
    //
    //---- If the image turns
    //---- out to be solid instead of transparent, the transparent pixels
    //---- will be blackened.

    if (rim->opacity == OPAC_Solid)
        img->Clear(pal_black);
    else
        img->Clear(TRANS_PIXEL);

    int         i;
    texpatch_t *patch;

    // Composite the columns into the block.
    for (i = 0, patch = tdef->patches; i < tdef->patchcount; i++, patch++)
    {
        const patch_t *realpatch = (const patch_t *)W_LoadLump(patch->patch);

        int realsize = W_LumpLength(patch->patch);

        int x1 = patch->originx;
        int y1 = patch->originy;
        int x2 = x1 + EPI_LE_S16(realpatch->width);

        int x = MAX(0, x1);

        x2 = MIN(tdef->width, x2);

        for (; x < x2; x++)
        {
            int offset = EPI_LE_S32(realpatch->columnofs[x - x1]);

            if (offset < 0 || offset >= realsize)
                I_Error("Bad image offset 0x%08x in image [%s]\n", offset, rim->name.c_str());

            const column_t *patchcol = (const column_t *)((const uint8_t *)realpatch + offset);

            DrawColumnIntoEpiBlock(rim, img, patchcol, x, y1);
        }

        delete[] realpatch;
    }

    // CW: Textures MUST tile! If actual size not total size, manually tile
    // [ AJA: this does not make them tile, just fills in the black gaps ]
    img->FillMarginX(rim->actual_w);
    img->FillMarginY(rim->actual_h);

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
static epi::image_data_c *ReadPatchAsEpiBlock(image_c *rim)
{
    SYS_ASSERT(rim->source_type == IMSRC_Graphic || rim->source_type == IMSRC_Sprite ||
               rim->source_type == IMSRC_TX_HI);

    int         lump          = rim->source.graphic.lump;
    const char *packfile_name = rim->source.graphic.packfile_name;

    // handle PNG/JPEG/TGA images
    if (!rim->source.graphic.is_patch)
    {
        epi::file_c *f;

        if (packfile_name)
            f = W_OpenPackFile(packfile_name);
        else
            f = W_OpenLump(lump);

        epi::image_data_c *img = epi::Image_Load(f);

        // close it
        delete f;

        if (!img)
            I_Error("Error loading image in lump: %s\n", packfile_name ? packfile_name : W_GetLumpName(lump));

        return img;
    }

    int tw = rim->total_w;
    int th = rim->total_h;

    epi::image_data_c *img = new epi::image_data_c(tw, th, 1);

    // Clear initial pixels to either totally transparent, or totally
    // black (if we know the image should be solid).
    //
    //---- If the image turns
    //---- out to be solid instead of transparent, the transparent pixels
    //---- will be blackened.

    if (rim->opacity == OPAC_Solid)
        img->Clear(pal_black);
    else
        img->Clear(TRANS_PIXEL);

    // Composite the columns into the block.
    const patch_t *realpatch = nullptr;
    int            realsize  = 0;

    if (packfile_name)
    {
        epi::file_c *f = W_OpenPackFile(packfile_name);
        if (f)
        {
            realpatch = (const patch_t *)f->LoadIntoMemory();
            realsize  = f->GetLength();
        }
        else
            I_Error("ReadPatchAsEpiBlock: Failed to load %s!\n", packfile_name);
        delete f;
    }
    else
    {
        realpatch = (const patch_t *)W_LoadLump(lump);
        realsize  = W_LumpLength(lump);
    }

    SYS_ASSERT(realpatch);
    SYS_ASSERT(rim->actual_w == EPI_LE_S16(realpatch->width));
    SYS_ASSERT(rim->actual_h == EPI_LE_S16(realpatch->height));

    // 2023.11.07 - These were previously left as total_w/h, which accounts
    // for power-of-two sizing and was messing up patch font atlas generation.
    // Not sure if there are any bad side effects yet - Dasho
    img->used_w = rim->actual_w;
    img->used_h = rim->actual_h;

    for (int x = 0; x < rim->actual_w; x++)
    {
        int offset = EPI_LE_S32(realpatch->columnofs[x]);

        if (offset < 0 || offset >= realsize)
            I_Error("Bad image offset 0x%08x in image [%s]\n", offset, rim->name.c_str());

        const column_t *patchcol = (const column_t *)((const uint8_t *)realpatch + offset);

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
static epi::image_data_c *ReadDummyAsEpiBlock(image_c *rim)
{
    SYS_ASSERT(rim->source_type == IMSRC_Dummy);
    SYS_ASSERT(rim->actual_w == rim->total_w);
    SYS_ASSERT(rim->actual_h == rim->total_h);
    SYS_ASSERT(rim->total_w == DUMMY_X);
    SYS_ASSERT(rim->total_h == DUMMY_Y);

    epi::image_data_c *img = new epi::image_data_c(DUMMY_X, DUMMY_Y, 4);

    // copy pixels
    for (int y = 0; y < DUMMY_Y; y++)
        for (int x = 0; x < DUMMY_X; x++)
        {
            uint8_t *dest_pix = img->PixelAt(x, y);

            if (dummy_graphic[(DUMMY_Y - 1 - y) * DUMMY_X + x])
            {
                *dest_pix++ = (rim->source.dummy.fg & 0xFF0000) >> 16;
                *dest_pix++ = (rim->source.dummy.fg & 0x00FF00) >> 8;
                *dest_pix++ = (rim->source.dummy.fg & 0x0000FF);
                *dest_pix++ = 255;
            }
            else if (rim->source.dummy.bg == TRANS_PIXEL)
            {
                *dest_pix++ = 0;
                *dest_pix++ = 0;
                *dest_pix++ = 0;
                *dest_pix++ = 0;
            }
            else
            {
                *dest_pix++ = (rim->source.dummy.bg & 0xFF0000) >> 16;
                *dest_pix++ = (rim->source.dummy.bg & 0x00FF00) >> 8;
                *dest_pix++ = (rim->source.dummy.bg & 0x0000FF);
                *dest_pix++ = 255;
            }
        }

    return img;
}

static epi::image_data_c *CreateUserColourImage(image_c *rim, imagedef_c *def)
{
    int tw = MAX(rim->total_w, 1);
    int th = MAX(rim->total_h, 1);

    epi::image_data_c *img = new epi::image_data_c(tw, th, 3);

    uint8_t *dest = img->pixels;

    for (int y = 0; y < img->height; y++)
        for (int x = 0; x < img->width; x++)
        {
            *dest++ = epi::RGBA_Red(def->colour);
            *dest++ = epi::RGBA_Green(def->colour);
            *dest++ = epi::RGBA_Blue(def->colour);
        }

    return img;
}

epi::file_c *OpenUserFileOrLump(imagedef_c *def)
{
    switch (def->type)
    {
    case IMGDT_File:
        // -AJA- 2005/01/15: filenames in DDF relative to APPDIR
        return M_OpenComposedEPIFile(game_dir.c_str(), def->info.c_str());

    case IMGDT_Package:
        return W_OpenPackFile(def->info);

    case IMGDT_Lump: {
        int lump = W_CheckNumForName(def->info.c_str());
        if (lump < 0)
            return NULL;

        return W_OpenLump(lump);
    }

    default:
        return NULL;
    }
}

static epi::image_data_c *CreateUserFileImage(image_c *rim, imagedef_c *def)
{
    epi::file_c *f = OpenUserFileOrLump(def);

    if (!f)
        I_Error("Missing image file: %s\n", def->info.c_str());

    epi::image_data_c *img = epi::Image_Load(f);

    // close it
    delete f;

    if (!img)
        I_Error("Error occurred loading image file: %s\n", def->info.c_str());

    /* Lobo 2022: info overload. Shut up.
    #if 1  // DEBUGGING
        L_WriteDebug("CREATE IMAGE [%s] %dx%d < %dx%d opac=%d --> %p %dx%d bpp %d\n",
        rim->name,
        rim->actual_w, rim->actual_h,
        rim->total_w, rim->total_h,
        rim->opacity,
        img, img->width, img->height, img->bpp);
    #endif
    */
    rim->opacity = R_DetermineOpacity(img, &rim->is_empty);

    if (def->is_font)
        return img;

    if (def->fix_trans == FIXTRN_Blacken)
        R_BlackenClearAreas(img);

    SYS_ASSERT(rim->total_w == img->width);
    SYS_ASSERT(rim->total_h == img->height);

    // CW: Textures MUST tile! If actual size not total size, manually tile
    // [ AJA: this does not make them tile, just fills in the black gaps ]
    if (rim->opacity == OPAC_Solid)
    {
        img->FillMarginX(rim->actual_w);
        img->FillMarginY(rim->actual_h);
    }

    return img;
}

//
// ReadUserAsEpiBlock
//
// Loads or Creates the user defined image.
// Doesn't do any mipmapping (this is too "raw" if you follow).
//
static epi::image_data_c *ReadUserAsEpiBlock(image_c *rim)
{
    SYS_ASSERT(rim->source_type == IMSRC_User);

    // clear initial image to black / transparent
    /// ALREADY DONE: memset(dest, pal_black, tw * th * bpp);

    imagedef_c *def = rim->source.user.def;

    switch (def->type)
    {
    case IMGDT_Colour:
        return CreateUserColourImage(rim, def);

    case IMGDT_File:
    case IMGDT_Lump:
    case IMGDT_Package:
        return CreateUserFileImage(rim, def);

    default:
        I_Error("ReadUserAsEpiBlock: Coding error, unknown type %d\n", def->type);
    }

    return NULL; /* NOT REACHED */
}

//
// ReadAsEpiBlock
//
// Read the image from the wad into an image_data_c class.
// The image returned is normally palettised (bpp == 1), and the
// palette must be determined from rim->source_palette.  Mainly
// just a switch to more specialised image readers.
//
// Never returns NULL.
//
epi::image_data_c *ReadAsEpiBlock(image_c *rim)
{
    switch (rim->source_type)
    {
    case IMSRC_Flat:
    case IMSRC_Raw320x200:
        return ReadFlatAsEpiBlock(rim);

    case IMSRC_Texture:
        return ReadTextureAsEpiBlock(rim);

    case IMSRC_Graphic:
    case IMSRC_Sprite:
    case IMSRC_TX_HI:
        return ReadPatchAsEpiBlock(rim);

    case IMSRC_Dummy:
        return ReadDummyAsEpiBlock(rim);

    case IMSRC_User:
        return ReadUserAsEpiBlock(rim);

    default:
        I_Error("ReadAsBlock: unknown source_type %d !\n", rim->source_type);
        return NULL;
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
