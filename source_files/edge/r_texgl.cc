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
#include "r_backend.h"
#include "r_colormap.h"
#include "r_gldefs.h"
#include "r_image.h"
#include "r_sky.h"
#include "w_texture.h"
#include "w_wad.h"

// clamp cache used by runits to avoid extremely expensive gl tex param
// lookups
extern std::unordered_map<GLuint, GLint> texture_clamp_s;
extern std::unordered_map<GLuint, GLint> texture_clamp_t;

ImageData *RGBFromPalettised(ImageData *src, const uint8_t *palette, int opacity)
{
    if (src->depth_ == 1)
    {
        int        bpp  = (opacity == kOpacitySolid) ? 3 : 4;
        ImageData *dest = new ImageData(src->width_, src->height_, bpp);
        dest->width_    = src->width_;
        dest->height_   = src->height_;
        for (int y = 0; y < src->height_; y++)
            for (int x = 0; x < src->width_; x++)
            {
                uint8_t src_pix = *src->PixelAt(x, y);

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

GLuint UploadTexture(ImageData *img, int flags, int max_pix)
{
    /* Send the texture data to the GL, and returns the texture ID
     * assigned to it.
     */

#ifdef EDGE_SOKOL
    // Only OpenGL supports RGB format for textures, so promote to RGBA
    if (img->depth_ == 3)
    {
        img->SetAlpha(255);
    }
#endif

    EPI_ASSERT(img->depth_ == 3 || img->depth_ == 4);

    bool clamp  = (flags & kUploadClamp) ? true : false;
    bool nomip  = (flags & kUploadMipMap) ? false : true;
    bool smooth = (flags & kUploadSmooth) ? true : false;

    int total_w = img->width_;
    int total_h = img->height_;

    int new_w, new_h;

    // scale down, if necessary, to fix the maximum size
    for (new_w = total_w; new_w > render_backend->GetMaxTextureSize(); new_w /= 2)
    { /* nothing here */
    }

    for (new_h = total_h; new_h > render_backend->GetMaxTextureSize(); new_h /= 2)
    { /* nothing here */
    }

    while (new_w * new_h > max_pix)
    {
        if (new_h >= new_w)
            new_h /= 2;
        else
            new_w /= 2;
    }

    render_state->PixelStorei(GL_UNPACK_ALIGNMENT, 1);

    GLuint id;

    render_state->GenTextures(1, &id);
    render_state->BindTexture(id);

    GLint tmode = GL_REPEAT;

    if (clamp)
        tmode = renderer_dumb_clamp.d_ ? GL_CLAMP : GL_CLAMP_TO_EDGE;

    render_state->TextureWrapS(tmode);
    render_state->TextureWrapT(tmode);

    texture_clamp_s.emplace(id, tmode);
    texture_clamp_t.emplace(id, tmode);

    // magnification mode
    render_state->TextureMagFilter(smooth ? GL_LINEAR : GL_NEAREST);

    // minification mode
    int mip_level = HMM_Clamp(0, image_mipmapping, 2);

    // special logic for mid-masked textures.  The kUploadThresh flag
    // guarantees that each texture level has simple alpha (0 or 255),
    // but we must also disable Trilinear Mipmapping because it will
    // produce partial alpha values when interpolating between mips.
    if (flags & kUploadThresh)
        mip_level = HMM_Clamp(0, mip_level, 1);

    static GLuint minif_modes[2 * 3] = {GL_NEAREST, GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST_MIPMAP_LINEAR,

                                        GL_LINEAR,  GL_LINEAR_MIPMAP_NEAREST,  GL_LINEAR_MIPMAP_LINEAR};

    render_state->TextureMinFilter(minif_modes[(smooth ? 3 : 0) + (nomip ? 0 : mip_level)]);

    for (int mip = 0;; mip++)
    {
        if (img->width_ != new_w || img->height_ != new_h)
        {
            img->ShrinkMasked(new_w, new_h);

            if (flags & kUploadThresh)
                img->ThresholdAlpha((mip & 1) ? 96 : 144);
        }

        render_state->TexImage2D(GL_TEXTURE_2D, mip, img->depth_ == 3 ? GL_RGB : GL_RGBA, new_w, new_h, 0,
                                 img->depth_ == 3 ? GL_RGB : GL_RGBA, GL_UNSIGNED_BYTE, img->PixelAt(0, 0));

        // stop if mipmapping disabled or we have reached the end
        if (nomip || !image_mipmapping || (new_w == 1 && new_h == 1))
            break;

        new_w = HMM_MAX(1, new_w / 2);
        new_h = HMM_MAX(1, new_h / 2);
    }

    render_state->FinishTextures(1, &id);

    return id;
}

//----------------------------------------------------------------------------

void PaletteRemapRGBA(const ImageData *img, const uint8_t *new_pal, const uint8_t *old_pal)
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

int DetermineOpacity(const ImageData *img, bool *is_empty_)
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

        for (int y = 0; y < img->height_; y++)
            for (int x = 0; x < img->width_; x++)
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

        for (int y = 0; y < img->height_; y++)
            for (int x = 0; x < img->width_; x++)
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
