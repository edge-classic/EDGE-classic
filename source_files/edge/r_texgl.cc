//----------------------------------------------------------------------------
//  EDGE Texture Upload
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

#include "r_texgl.h"

#include <limits.h>

#include <unordered_map>

#include "e_main.h"
#include "e_search.h"
#include "epi.h"
#include "i_defs_gl.h"
#include "i_system.h"
#include "im_data.h"
#include "m_argv.h"
#include "m_misc.h"
#include "p_local.h"
#include "r_colormap.h"
#include "r_gldefs.h"
#include "r_image.h"
#include "r_sky.h"
#include "w_texture.h"
#include "w_wad.h"

// clamp cache used by runits to avoid an extremely expensive gl tex param
// lookup
extern std::unordered_map<GLuint, GLint> texture_clamp;

int MakeValidTextureSize(int value)
{
    EPI_ASSERT(value > 0);

    if (value <= 1)
        return 1;
    if (value <= 2)
        return 2;
    if (value <= 4)
        return 4;
    if (value <= 8)
        return 8;
    if (value <= 16)
        return 16;
    if (value <= 32)
        return 32;
    if (value <= 64)
        return 64;
    if (value <= 128)
        return 128;
    if (value <= 256)
        return 256;
    if (value <= 512)
        return 512;
    if (value <= 1024)
        return 1024;
    if (value <= 2048)
        return 2048;
    if (value <= 4096)
        return 4096;

    FatalError("Texture size (%d) too large !\n", value);
    return -1; /* NOT REACHED */
}

ImageData *RgbFromPalettised(ImageData *src, const uint8_t *palette, int opacity)
{
    if (src->depth_ == 1)
    {
        int        bpp     = (opacity == kOpacitySolid) ? 3 : 4;
        ImageData *dest    = new ImageData(src->width_, src->height_, bpp);
        dest->used_width_  = src->used_width_;
        dest->used_height_ = src->used_height_;
        for (int y = 0; y < src->height_; y++)
            for (int x = 0; x < src->width_; x++)
            {
                uint8_t src_pix = src->PixelAt(x, y)[0];

                uint8_t *dest_pix = dest->PixelAt(x, y);

                if (src_pix == kTransparentPixelIndex)
                {
                    dest_pix[0] = dest_pix[1] = dest_pix[2] = 0;

                    if (bpp == 4)
                        dest_pix[3] = 0;
                }
                else
                {
                    dest_pix[0] = palette[src_pix * 3 + 0];
                    dest_pix[1] = palette[src_pix * 3 + 1];
                    dest_pix[2] = palette[src_pix * 3 + 2];

                    if (bpp == 4)
                        dest_pix[3] = 255;
                }
            }
        return dest;
    }
    else
        return src;
}

GLuint RendererUploadTexture(ImageData *img, int flags, int max_pix)
{
    /* Send the texture data to the GL, and returns the texture ID
     * assigned to it.
     */

    EPI_ASSERT(img->depth_ == 3 || img->depth_ == 4);

    bool clamp  = (flags & kUploadClamp) ? true : false;
    bool nomip  = (flags & kUploadMipMap) ? false : true;
    bool smooth = (flags & kUploadSmooth) ? true : false;

    int total_w = img->width_;
    int total_h = img->height_;

    int new_w, new_h;

    // scale down, if necessary, to fix the maximum size
    for (new_w = total_w; new_w > maximum_texture_size; new_w /= 2)
    { /* nothing here */
    }

    for (new_h = total_h; new_h > maximum_texture_size; new_h /= 2)
    { /* nothing here */
    }

    while (new_w * new_h > max_pix)
    {
        if (new_h >= new_w)
            new_h /= 2;
        else
            new_w /= 2;
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    GLuint id;

    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);

    GLint tmode = GL_REPEAT;

    if (clamp)
        tmode = renderer_dumb_clamp.d_ ? GL_CLAMP : GL_CLAMP_TO_EDGE;

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, tmode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, tmode);

    texture_clamp.emplace(id, tmode);

    // magnification mode
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, smooth ? GL_LINEAR : GL_NEAREST);

    // minification mode
    int mip_level = HMM_Clamp(0, detail_level, 2);

    // special logic for mid-masked textures.  The kUploadThresh flag
    // guarantees that each texture level has simple alpha (0 or 255),
    // but we must also disable Trilinear Mipmapping because it will
    // produce partial alpha values when interpolating between mips.
    if (flags & kUploadThresh)
        mip_level = HMM_Clamp(0, mip_level, 1);

    static GLuint minif_modes[2 * 3] = { GL_NEAREST, GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST_MIPMAP_LINEAR,

                                         GL_LINEAR,  GL_LINEAR_MIPMAP_NEAREST,  GL_LINEAR_MIPMAP_LINEAR };

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minif_modes[(smooth ? 3 : 0) + (nomip ? 0 : mip_level)]);

    for (int mip = 0;; mip++)
    {
        if (img->width_ != new_w || img->height_ != new_h)
        {
            img->ShrinkMasked(new_w, new_h);

            if (flags & kUploadThresh)
                img->ThresholdAlpha((mip & 1) ? 96 : 144);
        }

        glTexImage2D(GL_TEXTURE_2D, mip, (img->depth_ == 3) ? GL_RGB : GL_RGBA, new_w, new_h, 0 /* border */,
                     (img->depth_ == 3) ? GL_RGB : GL_RGBA, GL_UNSIGNED_BYTE, img->PixelAt(0, 0));

        // stop if mipmapping disabled or we have reached the end
        if (nomip || !detail_level || (new_w == 1 && new_h == 1))
            break;

        new_w = HMM_MAX(1, new_w / 2);
        new_h = HMM_MAX(1, new_h / 2);

        // -AJA- 2003/12/05: workaround for Radeon 7500 driver bug, which
        //       incorrectly draws the 1x1 mip texture as black.
#ifndef _WIN32
        if (new_w == 1 && new_h == 1)
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mip);
#endif
    }

    return id;
}

//----------------------------------------------------------------------------

void PaletteRemapRgba(ImageData *img, const uint8_t *new_pal, const uint8_t *old_pal)
{
    const int max_prev = 16;

    // cache of previously looked-up colours (in pairs)
    uint8_t previous[max_prev * 6];
    int     num_prev = 0;

    for (int y = 0; y < img->height_; y++)
        for (int x = 0; x < img->width_; x++)
        {
            uint8_t *cur = img->PixelAt(x, y);

            // skip completely transparent pixels
            if (img->depth_ == 4 && cur[3] == 0)
                continue;

            // optimisation: if colour matches previous one, don't need
            // to compute the remapping again.
            int i;
            for (i = 0; i < num_prev; i++)
            {
                if (previous[i * 6 + 0] == cur[0] && previous[i * 6 + 1] == cur[1] && previous[i * 6 + 2] == cur[2])
                {
                    break;
                }
            }

            if (i < num_prev)
            {
                // move to front (Most Recently Used)
                if (i != 0)
                {
                    uint8_t tmp[6];

                    memcpy(tmp, previous, 6);
                    memcpy(previous, previous + i * 6, 6);
                    memcpy(previous + i * 6, tmp, 6);
                }

                cur[0] = previous[3];
                cur[1] = previous[4];
                cur[2] = previous[5];

                continue;
            }

            if (num_prev < max_prev)
            {
                memmove(previous + 6, previous, num_prev * 6);
                num_prev++;
            }

            // most recent lookup is at the head
            previous[0] = cur[0];
            previous[1] = cur[1];
            previous[2] = cur[2];

            int best      = 0;
            int best_dist = (1 << 30);

            int R = int(cur[0]);
            int G = int(cur[1]);
            int B = int(cur[2]);

            for (int p = 0; p < 256; p++)
            {
                int dR = int(old_pal[p * 3 + 0]) - R;
                int dG = int(old_pal[p * 3 + 1]) - G;
                int dB = int(old_pal[p * 3 + 2]) - B;

                int dist = dR * dR + dG * dG + dB * dB;

                if (dist < best_dist)
                {
                    best_dist = dist;
                    best      = p;
                }
            }

            // if this colour is not affected by the colourmap, then
            // keep the original colour (which has more precision).
            if (old_pal[best * 3 + 0] != new_pal[best * 3 + 0] || old_pal[best * 3 + 1] != new_pal[best * 3 + 1] ||
                old_pal[best * 3 + 2] != new_pal[best * 3 + 2])
            {
                cur[0] = new_pal[best * 3 + 0];
                cur[1] = new_pal[best * 3 + 1];
                cur[2] = new_pal[best * 3 + 2];
            }

            previous[3] = cur[0];
            previous[4] = cur[1];
            previous[5] = cur[2];
        }
}

int DetermineOpacity(ImageData *img, bool *is_empty_)
{
    if (img->depth_ == 3)
    {
        *is_empty_ = false;
        return kOpacitySolid;
    }

    if (img->depth_ == 1)
    {
        ImageOpacity opacity = kOpacitySolid;
        bool         empty   = true;

        for (int y = 0; y < img->used_height_; y++)
            for (int x = 0; x < img->used_width_; x++)
            {
                uint8_t pix = img->PixelAt(x, y)[0];

                if (pix == kTransparentPixelIndex)
                    opacity = kOpacityMasked;
                else
                    empty = false;
            }

        *is_empty_ = empty;
        return opacity;
    }
    else
    {
        EPI_ASSERT(img->depth_ == 4);

        ImageOpacity opacity   = kOpacitySolid;
        bool         is_masked = false;
        bool         empty     = true;

        for (int y = 0; y < img->used_height_; y++)
            for (int x = 0; x < img->used_width_; x++)
            {
                uint8_t alpha = img->PixelAt(x, y)[3];

                if (alpha == 0)
                    is_masked = true;
                else if (alpha != 255)
                {
                    empty   = false;
                    opacity = kOpacityComplex;
                }
                else
                    empty = false;
            }

        *is_empty_ = empty;
        if (opacity == kOpacityComplex)
            return kOpacityComplex;
        else
        {
            if (is_masked)
                return kOpacityMasked;
            else
                return kOpacitySolid;
        }
    }
}

void BlackenClearAreas(ImageData *img)
{
    // makes sure that any totally transparent pixel (alpha == 0)
    // has a colour of black.

    uint8_t *dest = img->pixels_;

    int count = img->width_ * img->height_;

    if (img->depth_ == 1)
    {
        for (; count > 0; count--, dest++)
        {
            if (*dest == kTransparentPixelIndex)
                *dest = playpal_black;
        }
    }
    else if (img->depth_ == 4)
    {
        for (; count > 0; count--, dest += 4)
        {
            if (dest[3] == 0)
            {
                dest[0] = dest[1] = dest[2] = 0;
            }
        }
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
