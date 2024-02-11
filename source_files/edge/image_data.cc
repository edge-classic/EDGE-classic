//------------------------------------------------------------------------
//  Basic image storage
//----------------------------------------------------------------------------
//
//  Copyright (c) 2003-2024 The EDGE Team.
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

#include <string.h>

#include "epi.h"

#include "image_data.h"

#include "tables.h"

#include "math_color.h"
#include "HandmadeMath.h"
#include <unordered_map>

image_data_c::image_data_c(int _w, int _h, int _bpp) : width(_w), height(_h), bpp(_bpp), used_w(_w), used_h(_h)
{
    pixels = new uint8_t[width * height * bpp];
    offset_x = offset_y = 0;
    scale_x = scale_y = 1.0f;
}

image_data_c::~image_data_c()
{
    delete[] pixels;

    pixels = nullptr;
    width = height = 0;
}

void image_data_c::Clear(uint8_t val)
{
    memset(pixels, val, width * height * bpp);
}

void image_data_c::Whiten()
{
    SYS_ASSERT(bpp >= 3);

    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
        {
            uint8_t *src = PixelAt(x, y);

            int ity = HMM_MAX(src[0], HMM_MAX(src[1], src[2]));

            // soften the above equation, take average into account
            ity = (ity * 196 + src[0] * 20 + src[1] * 20 + src[2] * 20) >> 8;

            src[0] = src[1] = src[2] = ity;
        }
}

void image_data_c::Invert()
{
    int line_size = used_w * bpp;

    uint8_t *line_data = new uint8_t[line_size + 1];

    for (int y = 0; y < used_h / 2; y++)
    {
        int y2 = used_h - 1 - y;

        memcpy(line_data, PixelAt(0, y), line_size);
        memcpy(PixelAt(0, y), PixelAt(0, y2), line_size);
        memcpy(PixelAt(0, y2), line_data, line_size);
    }

    delete[] line_data;
}

void image_data_c::Shrink(int new_w, int new_h)
{
    SYS_ASSERT(new_w <= width && new_h <= height);

    int step_x = width / new_w;
    int step_y = height / new_h;
    int total  = step_x * step_y;

    if (bpp == 1)
    {
        for (int dy = 0; dy < new_h; dy++)
            for (int dx = 0; dx < new_w; dx++)
            {
                uint8_t *dest_pix = pixels + (dy * new_w + dx) * 3;

                int sx = dx * step_x;
                int sy = dy * step_y;

                const uint8_t *src_pix = PixelAt(sx, sy);

                *dest_pix = *src_pix;
            }
    }
    else if (bpp == 3)
    {
        for (int dy = 0; dy < new_h; dy++)
            for (int dx = 0; dx < new_w; dx++)
            {
                uint8_t *dest_pix = pixels + (dy * new_w + dx) * 3;

                int sx = dx * step_x;
                int sy = dy * step_y;

                int r = 0, g = 0, b = 0;

                // compute average colour of block
                for (int x = 0; x < step_x; x++)
                    for (int y = 0; y < step_y; y++)
                    {
                        const uint8_t *src_pix = PixelAt(sx + x, sy + y);

                        r += src_pix[0];
                        g += src_pix[1];
                        b += src_pix[2];
                    }

                dest_pix[0] = r / total;
                dest_pix[1] = g / total;
                dest_pix[2] = b / total;
            }
    }
    else /* bpp == 4 */
    {
        for (int dy = 0; dy < new_h; dy++)
            for (int dx = 0; dx < new_w; dx++)
            {
                uint8_t *dest_pix = pixels + (dy * new_w + dx) * 4;

                int sx = dx * step_x;
                int sy = dy * step_y;

                int r = 0, g = 0, b = 0, a = 0;

                // compute average colour of block
                for (int x = 0; x < step_x; x++)
                    for (int y = 0; y < step_y; y++)
                    {
                        const uint8_t *src_pix = PixelAt(sx + x, sy + y);

                        r += src_pix[0];
                        g += src_pix[1];
                        b += src_pix[2];
                        a += src_pix[3];
                    }

                dest_pix[0] = r / total;
                dest_pix[1] = g / total;
                dest_pix[2] = b / total;
                dest_pix[3] = a / total;
            }
    }

    used_w = HMM_MAX(1, used_w * new_w / width);
    used_h = HMM_MAX(1, used_h * new_h / height);

    width  = new_w;
    height = new_h;
}

void image_data_c::ShrinkMasked(int new_w, int new_h)
{
    if (bpp != 4)
    {
        Shrink(new_w, new_h);
        return;
    }

    SYS_ASSERT(new_w <= width && new_h <= height);

    int step_x = width / new_w;
    int step_y = height / new_h;
    int total  = step_x * step_y;

    for (int dy = 0; dy < new_h; dy++)
        for (int dx = 0; dx < new_w; dx++)
        {
            uint8_t *dest_pix = pixels + (dy * new_w + dx) * 4;

            int sx = dx * step_x;
            int sy = dy * step_y;

            int r = 0, g = 0, b = 0, a = 0;

            // compute average colour of block
            for (int x = 0; x < step_x; x++)
                for (int y = 0; y < step_y; y++)
                {
                    const uint8_t *src_pix = PixelAt(sx + x, sy + y);

                    int weight = src_pix[3];

                    r += src_pix[0] * weight;
                    g += src_pix[1] * weight;
                    b += src_pix[2] * weight;

                    a += weight;
                }

            if (a == 0)
            {
                dest_pix[0] = 0;
                dest_pix[1] = 0;
                dest_pix[2] = 0;
                dest_pix[3] = 0;
            }
            else
            {
                dest_pix[0] = r / a;
                dest_pix[1] = g / a;
                dest_pix[2] = b / a;
                dest_pix[3] = a / total;
            }
        }

    used_w = HMM_MAX(1, used_w * new_w / width);
    used_h = HMM_MAX(1, used_h * new_h / height);

    width  = new_w;
    height = new_h;
}

void image_data_c::Grow(int new_w, int new_h)
{
    SYS_ASSERT(new_w >= width && new_h >= height);

    uint8_t *new_pixels = new uint8_t[new_w * new_h * bpp];

    for (int dy = 0; dy < new_h; dy++)
        for (int dx = 0; dx < new_w; dx++)
        {
            int sx = dx * width / new_w;
            int sy = dy * height / new_h;

            const uint8_t *src = PixelAt(sx, sy);

            uint8_t *dest = new_pixels + (dy * new_w + dx) * bpp;

            for (int i = 0; i < bpp; i++)
                *dest++ = *src++;
        }

    delete[] pixels;

    used_w = used_w * new_w / width;
    used_h = used_h * new_h / height;

    pixels = new_pixels;
    width  = new_w;
    height = new_h;
}

void image_data_c::RemoveAlpha()
{
    if (bpp != 4)
        return;

    uint8_t *src   = pixels;
    uint8_t *s_end = src + (width * height * bpp);
    uint8_t *dest  = pixels;

    for (; src < s_end; src += 4)
    {
        // blend alpha with BLACK

        *dest++ = (int)src[0] * (int)src[3] / 255;
        *dest++ = (int)src[1] * (int)src[3] / 255;
        *dest++ = (int)src[2] * (int)src[3] / 255;
    }

    bpp = 3;
}

void image_data_c::SetAlpha(int alphaness)
{
    if (bpp < 3)
        return;

    if (bpp == 3)
    {
        uint8_t *new_pixels = new uint8_t[width * height * 4];
        uint8_t *src        = pixels;
        uint8_t *s_end      = src + (width * height * 3);
        uint8_t *dest       = new_pixels;
        for (; src < s_end; src += 3)
        {
            *dest++ = src[0];
            *dest++ = src[1];
            *dest++ = src[2];
            *dest++ = alphaness;
        }
        delete[] pixels;
        pixels = new_pixels;
        bpp    = 4;
    }
    else
    {
        for (int i = 3; i < width * height * 4; i += 4)
        {
            pixels[i] = alphaness;
        }
    }
}

void image_data_c::ThresholdAlpha(uint8_t alpha)
{
    if (bpp != 4)
        return;

    uint8_t *src   = pixels;
    uint8_t *s_end = src + (width * height * bpp);

    for (; src < s_end; src += 4)
    {
        src[3] = (src[3] < alpha) ? 0 : 255;
    }
}

void image_data_c::FourWaySymmetry()
{
    int w2 = (width + 1) / 2;
    int h2 = (height + 1) / 2;

    for (int y = 0; y < h2; y++)
        for (int x = 0; x < w2; x++)
        {
            int ix = width - 1 - x;
            int iy = height - 1 - y;

            CopyPixel(x, y, ix, y);
            CopyPixel(x, y, x, iy);
            CopyPixel(x, y, ix, iy);
        }
}

void image_data_c::RemoveBackground()
{
    if (bpp < 3)
        return;

    if (bpp == 3)
    {
        uint8_t *new_pixels = new uint8_t[width * height * 4];
        uint8_t *src        = pixels;
        uint8_t *s_end      = src + (width * height * 3);
        uint8_t *dest       = new_pixels;
        for (; src < s_end; src += 3)
        {
            *dest++ = src[0];
            *dest++ = src[1];
            *dest++ = src[2];
            *dest++ = (src[0] == pixels[0] && src[1] == pixels[1] && src[2] == pixels[2]) ? 0 : 255;
        }
        delete[] pixels;
        pixels = new_pixels;
        bpp    = 4;
    }
    else
    {
        // If first pixel is fully transparent, assume that image background is already transparent
        if (pixels[3] == 0)
            return;

        for (int i = 4; i < width * height * 4; i += 4)
        {
            if (pixels[i] == pixels[0] && pixels[i + 1] == pixels[1] && pixels[i + 2] == pixels[2])
                pixels[i + 3] = 0;
        }
    }
}

void image_data_c::EightWaySymmetry()
{
    SYS_ASSERT(width == height);

    int hw = (width + 1) / 2;

    for (int y = 0; y < hw; y++)
        for (int x = y + 1; x < hw; x++)
        {
            CopyPixel(x, y, y, x);
        }

    FourWaySymmetry();
}

int image_data_c::ImageCharacterWidth(int x1, int y1, int x2, int y2)
{
    uint8_t *src         = pixels;
    int   last_last   = x1;
    int   first_first = x2;
    for (int i = y1; i < y2; i++)
    {
        bool found_first = false;
        bool found_last  = false;
        int  first       = 0;
        int  last        = 0;
        for (int j = x1; j < x2; j++)
        {
            uint8_t *checker = PixelAt(j, i);
            if (src[0] != checker[0] || src[1] != checker[1] || src[2] != checker[2])
            {
                if (!found_first)
                {
                    first       = j;
                    found_first = true;
                }
                else
                {
                    last       = j;
                    found_last = true;
                }
            }
        }
        if (found_first && first >= x1 && first < first_first)
            first_first = first;
        if (found_last && last <= x2 && last > last_last)
            last_last = last;
    }
    return HMM_MAX(last_last - first_first, 0) + 3; // Some padding on each side of the letter
}

void image_data_c::AverageHue(uint8_t *hue, uint8_t *ity, int from_x, int to_x, int from_y, int to_y)
{
    // make sure we don't overflow
    SYS_ASSERT(used_w * used_h <= 2048 * 2048);

    int r_sum = 0;
    int g_sum = 0;
    int b_sum = 0;
    int i_sum = 0;

    int weight = 0;

    // Sanity checking; at a minimum sample a 1x1 portion of the image
    from_x = HMM_Clamp(0, from_x, used_w - 1);
    to_x   = HMM_Clamp(1, to_x, used_h);
    from_y = HMM_Clamp(0, from_y, used_h - 1);
    to_y   = HMM_Clamp(1, to_y, used_h);

    for (int y = from_y; y < to_y; y++)
    {
        const uint8_t *src = PixelAt(0, y);

        for (int x = from_x; x < to_x; x++, src += bpp)
        {
            int r = src[0];
            int g = src[1];
            int b = src[2];
            int a = (bpp == 4) ? src[3] : 255;

            int v = HMM_MAX(r, HMM_MAX(g, b));

            i_sum += (v * (1 + a)) >> 9;

            // brighten color
            if (v > 0)
            {
                r = r * 255 / v;
                g = g * 255 / v;
                b = b * 255 / v;

                v = 255;
            }

            // compute weighting (based on saturation)
            if (v > 0)
            {
                int m = HMM_MIN(r, HMM_MIN(g, b));

                v = 4 + 12 * (v - m) / v;
            }

            // take alpha into account
            v = (v * (1 + a)) >> 8;

            r_sum += (r * v) >> 3;
            g_sum += (g * v) >> 3;
            b_sum += (b * v) >> 3;

            weight += v;
        }
    }

    weight = (weight + 7) >> 3;

    if (weight > 0)
    {
        hue[0] = r_sum / weight;
        hue[1] = g_sum / weight;
        hue[2] = b_sum / weight;
    }
    else
    {
        hue[0] = 0;
        hue[1] = 0;
        hue[2] = 0;
    }

    if (ity)
    {
        weight = (used_w * used_h + 1) / 2;

        *ity = i_sum / weight;
    }
}

RGBAColor image_data_c::AverageColor(int from_x, int to_x, int from_y, int to_y)
{
    // make sure we don't overflow
    SYS_ASSERT(used_w * used_h <= 2048 * 2048);

    std::unordered_map<RGBAColor, unsigned int> seen_colors;

    // Sanity checking; at a minimum sample a 1x1 portion of the image
    from_x = HMM_Clamp(0, from_x, used_w - 1);
    to_x   = HMM_Clamp(1, to_x, used_h);
    from_y = HMM_Clamp(0, from_y, used_h - 1);
    to_y   = HMM_Clamp(1, to_y, used_h);

    for (int y = from_y; y < to_y; y++)
    {
        const uint8_t *src = PixelAt(0, y);

        for (int x = from_x; x < to_x; x++, src += bpp)
        {
            if (bpp == 4 && src[3] == 0)
                continue;
            RGBAColor color = epi::MakeRGBA(src[0], src[1], src[2]);
            auto         res   = seen_colors.try_emplace(color, 0);
            // If color already seen, increment the hit counter
            if (!res.second)
                res.first->second++;
        }
    }

    unsigned int highest_count = 0;
    RGBAColor average_color = SG_BLACK_RGBA32;
    for (auto color : seen_colors)
    {
        if (color.second > highest_count)
            highest_count = color.second;
    }

    // If multiple colors were seen "the most", just use the last one spotted
    for (auto color : seen_colors)
    {
        if (color.second == highest_count)
        {
            average_color = color.first;
        }
    }

    return average_color;
}

RGBAColor image_data_c::LightestColor(int from_x, int to_x, int from_y, int to_y)
{
    // make sure we don't overflow
    SYS_ASSERT(used_w * used_h <= 2048 * 2048);

    int lightest_total = 0;
    int lightest_r     = 0;
    int lightest_g     = 0;
    int lightest_b     = 0;

    // Sanity checking; at a minimum sample a 1x1 portion of the image
    from_x = HMM_Clamp(0, from_x, used_w - 1);
    to_x   = HMM_Clamp(1, to_x, used_h);
    from_y = HMM_Clamp(0, from_y, used_h - 1);
    to_y   = HMM_Clamp(1, to_y, used_h);

    for (int y = from_y; y < to_y; y++)
    {
        const uint8_t *src = PixelAt(0, y);

        for (int x = from_x; x < to_x; x++, src += bpp)
        {
            if (bpp == 4 && src[3] == 0)
                continue;
            int current_total = src[0] + src[1] + src[2];
            if (current_total > lightest_total)
            {
                lightest_r     = src[0];
                lightest_g     = src[1];
                lightest_b     = src[2];
                lightest_total = current_total;
            }
        }
    }

    return epi::MakeRGBA(lightest_r, lightest_g, lightest_b);
}

RGBAColor image_data_c::DarkestColor(int from_x, int to_x, int from_y, int to_y)
{
    // make sure we don't overflow
    SYS_ASSERT(used_w * used_h <= 2048 * 2048);

    int darkest_total = 765;
    int darkest_r     = 0;
    int darkest_g     = 0;
    int darkest_b     = 0;

    // Sanity checking; at a minimum sample a 1x1 portion of the image
    from_x = HMM_Clamp(0, from_x, used_w - 1);
    to_x   = HMM_Clamp(1, to_x, used_h);
    from_y = HMM_Clamp(0, from_y, used_h - 1);
    to_y   = HMM_Clamp(1, to_y, used_h);

    for (int y = from_y; y < to_y; y++)
    {
        const uint8_t *src = PixelAt(0, y);

        for (int x = from_x; x < to_x; x++, src += bpp)
        {
            if (bpp == 4 && src[3] == 0)
                continue;
            int current_total = src[0] + src[1] + src[2];
            if (current_total < darkest_total)
            {
                darkest_r     = src[0];
                darkest_g     = src[1];
                darkest_b     = src[2];
                darkest_total = current_total;
            }
        }
    }

    return epi::MakeRGBA(darkest_r, darkest_g, darkest_b);
}

void image_data_c::Swirl(int leveltime, int thickness)
{
    const int swirlfactor  = 8192 / 64;
    const int swirlfactor2 = 8192 / 32;
    const int amp          = 2;
    int       speed;

    if (thickness == 1) // Thin liquid
    {
        speed = 40;
    }
    else
    {
        speed = 10;
    }

    uint8_t *new_pixels = new uint8_t[width * height * bpp];

    int x, y;

    // SMMU swirling algorithm
    for (x = 0; x < width; x++)
    {
        for (y = 0; y < height; y++)
        {
            int x1, y1;
            int sinvalue, sinvalue2;

            sinvalue  = (y * swirlfactor + leveltime * speed * 5 + 900) & 8191;
            sinvalue2 = (x * swirlfactor2 + leveltime * speed * 4 + 300) & 8191;
            x1        = x + width + height + ((finesine[sinvalue] * amp) >> FRACBITS) +
                 ((finesine[sinvalue2] * amp) >> FRACBITS);

            sinvalue  = (x * swirlfactor + leveltime * speed * 3 + 700) & 8191;
            sinvalue2 = (y * swirlfactor2 + leveltime * speed * 4 + 1200) & 8191;
            y1        = y + width + height + ((finesine[sinvalue] * amp) >> FRACBITS) +
                 ((finesine[sinvalue2] * amp) >> FRACBITS);

            x1 &= width - 1;
            y1 &= height - 1;

            uint8_t *src  = pixels + (y1 * width + x1) * bpp;
            uint8_t *dest = new_pixels + (y * width + x) * bpp;

            memcpy(dest, src, bpp);
        }
    }
    delete[] pixels;
    pixels = new_pixels;
}

void image_data_c::FillMarginX(int actual_w)
{
    if (actual_w >= width)
        return;

    for (int x = 0; x < (width - actual_w); x++)
    {
        for (int y = 0; y < height; y++)
        {
            memcpy(pixels + (y * width + x + actual_w) * bpp, pixels + (y * width + x) * bpp, bpp);
        }
    }
}

void image_data_c::FillMarginY(int actual_h)
{
    if (actual_h >= height)
        return;

    for (int y = 0; y < (height - actual_h); y++)
    {
        memcpy(pixels + (y + actual_h) * width * bpp, pixels + y * width * bpp, width * bpp);
    }
}

void image_data_c::SetHSV(int rotation, int saturation, int value)
{
    SYS_ASSERT(bpp >= 3);

    rotation   = HMM_Clamp(-1800, rotation, 1800);
    saturation = HMM_Clamp(-1, saturation, 255);

    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
        {
            uint8_t *src = PixelAt(x, y);

            RGBAColor col = epi::MakeRGBA(src[0], src[1], src[2], bpp == 4 ? src[3] : 255);

            epi::HSVColor hue(col);

            if (rotation)
                hue.Rotate(rotation);

            if (saturation > -1)
                hue.SetSaturation(saturation);

            if (value)
                hue.SetValue(HMM_Clamp(0, hue.v_+value, 255));

            col = hue.ToRGBA();

            src[0] = epi::GetRGBARed(col);
            src[1] = epi::GetRGBAGreen(col);
            src[2] = epi::GetRGBABlue(col);
        }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
