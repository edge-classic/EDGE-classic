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

#include "im_data.h"

#include <string.h>

#include <unordered_map>

#include "HandmadeMath.h"
#include "epi.h"
#include "epi_color.h"
#include "swirl_table.h"

ImageData::ImageData(int width, int height, int depth)
    : width_(width), height_(height), depth_(depth)
{
    pixels_   = new uint8_t[width * height * depth];
    offset_x_ = offset_y_ = 0;
    scale_x_ = scale_y_ = 1.0f;
}

ImageData::~ImageData()
{
    delete[] pixels_;

    pixels_ = nullptr;
    width_ = height_ = 0;
}

void ImageData::Clear(uint8_t value)
{
    memset(pixels_, value, width_ * height_ * depth_);
}

void ImageData::Whiten()
{
    EPI_ASSERT(depth_ >= 3);

    for (int y = 0; y < height_; y++)
        for (int x = 0; x < width_; x++)
        {
            uint8_t *src = PixelAt(x, y);

            int ity = HMM_MAX(src[0], HMM_MAX(src[1], src[2]));

            // soften the above equation, take average into account
            ity = (ity * 196 + src[0] * 20 + src[1] * 20 + src[2] * 20) >> 8;

            src[0] = src[1] = src[2] = ity;
        }
}

void ImageData::Invert()
{
    int line_size = width_ * depth_;

    uint8_t *line_data = new uint8_t[line_size + 1];

    for (int y = 0; y < height_ / 2; y++)
    {
        int y2 = height_ - 1 - y;

        memcpy(line_data, PixelAt(0, y), line_size);
        memcpy(PixelAt(0, y), PixelAt(0, y2), line_size);
        memcpy(PixelAt(0, y2), line_data, line_size);
    }

    delete[] line_data;
}

void ImageData::Flip()
{
    int line_size = width_ * depth_;

    uint8_t *line_data  = new uint8_t[line_size];

    for (int y = 0; y < height_; y++)
    {
        for (int x = 0; x < width_; x++)
        {
            memcpy(line_data + (x * depth_), PixelAt(width_ - x, y), depth_);
        }
        memcpy(PixelAt(0, y), line_data, line_size);
    }

    delete[] line_data;
}

void ImageData::Shrink(int new_w, int new_h)
{
    EPI_ASSERT(new_w <= width_ && new_h <= height_);

    int step_x = width_ / new_w;
    int step_y = height_ / new_h;
    int total  = step_x * step_y;

    if (depth_ == 1)
    {
        for (int dy = 0; dy < new_h; dy++)
            for (int dx = 0; dx < new_w; dx++)
            {
                uint8_t *dest_pix = pixels_ + (dy * new_w + dx) * 3;

                int sx = dx * step_x;
                int sy = dy * step_y;

                const uint8_t *src_pix = PixelAt(sx, sy);

                *dest_pix = *src_pix;
            }
    }
    else if (depth_ == 3)
    {
        for (int dy = 0; dy < new_h; dy++)
            for (int dx = 0; dx < new_w; dx++)
            {
                uint8_t *dest_pix = pixels_ + (dy * new_w + dx) * 3;

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
    else /* depth_ == 4 */
    {
        for (int dy = 0; dy < new_h; dy++)
            for (int dx = 0; dx < new_w; dx++)
            {
                uint8_t *dest_pix = pixels_ + (dy * new_w + dx) * 4;

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

    width_  = HMM_MAX(1, width_ * new_w / width_);
    height_ = HMM_MAX(1, height_ * new_h / height_);

    width_  = new_w;
    height_ = new_h;
}

void ImageData::ShrinkMasked(int new_w, int new_h)
{
    if (depth_ != 4)
    {
        Shrink(new_w, new_h);
        return;
    }

    EPI_ASSERT(new_w <= width_ && new_h <= height_);

    int step_x = width_ / new_w;
    int step_y = height_ / new_h;
    int total  = step_x * step_y;

    for (int dy = 0; dy < new_h; dy++)
        for (int dx = 0; dx < new_w; dx++)
        {
            uint8_t *dest_pix = pixels_ + (dy * new_w + dx) * 4;

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

    width_  = HMM_MAX(1, width_ * new_w / width_);
    height_ = HMM_MAX(1, height_ * new_h / height_);

    width_  = new_w;
    height_ = new_h;
}

void ImageData::Grow(int new_w, int new_h)
{
    EPI_ASSERT(new_w >= width_ && new_h >= height_);

    uint8_t *new_pixels_ = new uint8_t[new_w * new_h * depth_];

    for (int dy = 0; dy < new_h; dy++)
        for (int dx = 0; dx < new_w; dx++)
        {
            int sx = dx * width_ / new_w;
            int sy = dy * height_ / new_h;

            const uint8_t *src = PixelAt(sx, sy);

            uint8_t *dest = new_pixels_ + (dy * new_w + dx) * depth_;

            for (int i = 0; i < depth_; i++)
                *dest++ = *src++;
        }

    delete[] pixels_;

    width_  = width_ * new_w / width_;
    height_ = height_ * new_h / height_;

    pixels_ = new_pixels_;
    width_  = new_w;
    height_ = new_h;
}

void ImageData::RemoveAlpha()
{
    if (depth_ != 4)
        return;

    uint8_t *src   = pixels_;
    uint8_t *s_end = src + (width_ * height_ * depth_);
    uint8_t *dest  = pixels_;

    for (; src < s_end; src += 4)
    {
        // blend alpha with BLACK

        *dest++ = (int)src[0] * (int)src[3] / 255;
        *dest++ = (int)src[1] * (int)src[3] / 255;
        *dest++ = (int)src[2] * (int)src[3] / 255;
    }

    depth_ = 3;
}

void ImageData::SetAlpha(int alphaness)
{
    if (depth_ < 3)
        return;

    if (depth_ == 3)
    {
        uint8_t *new_pixels_ = new uint8_t[width_ * height_ * 4];
        uint8_t *src         = pixels_;
        uint8_t *s_end       = src + (width_ * height_ * 3);
        uint8_t *dest        = new_pixels_;
        for (; src < s_end; src += 3)
        {
            *dest++ = src[0];
            *dest++ = src[1];
            *dest++ = src[2];
            *dest++ = alphaness;
        }
        delete[] pixels_;
        pixels_ = new_pixels_;
        depth_  = 4;
    }
    else
    {
        for (int i = 3; i < width_ * height_ * 4; i += 4)
        {
            pixels_[i] = alphaness;
        }
    }
}

void ImageData::ThresholdAlpha(uint8_t alpha)
{
    if (depth_ != 4)
        return;

    uint8_t *src   = pixels_;
    uint8_t *s_end = src + (width_ * height_ * depth_);

    for (; src < s_end; src += 4)
    {
        src[3] = (src[3] < alpha) ? 0 : 255;
    }
}

void ImageData::FourWaySymmetry()
{
    int w2 = (width_ + 1) / 2;
    int h2 = (height_ + 1) / 2;

    for (int y = 0; y < h2; y++)
        for (int x = 0; x < w2; x++)
        {
            int ix = width_ - 1 - x;
            int iy = height_ - 1 - y;

            CopyPixel(x, y, ix, y);
            CopyPixel(x, y, x, iy);
            CopyPixel(x, y, ix, iy);
        }
}

void ImageData::RemoveBackground()
{
    if (depth_ < 3)
        return;

    if (depth_ == 3)
    {
        uint8_t *new_pixels_ = new uint8_t[width_ * height_ * 4];
        uint8_t *src         = pixels_;
        uint8_t *s_end       = src + (width_ * height_ * 3);
        uint8_t *dest        = new_pixels_;
        for (; src < s_end; src += 3)
        {
            *dest++ = src[0];
            *dest++ = src[1];
            *dest++ = src[2];
            *dest++ = (src[0] == pixels_[0] && src[1] == pixels_[1] && src[2] == pixels_[2]) ? 0 : 255;
        }
        delete[] pixels_;
        pixels_ = new_pixels_;
        depth_  = 4;
    }
    else
    {
        // If first pixel is fully transparent, assume that image background is
        // already transparent
        if (pixels_[3] == 0)
            return;

        for (int i = 4; i < width_ * height_ * 4; i += 4)
        {
            if (pixels_[i] == pixels_[0] && pixels_[i + 1] == pixels_[1] && pixels_[i + 2] == pixels_[2])
                pixels_[i + 3] = 0;
        }
    }
}

void ImageData::EightWaySymmetry()
{
    EPI_ASSERT(width_ == height_);

    int hw = (width_ + 1) / 2;

    for (int y = 0; y < hw; y++)
        for (int x = y + 1; x < hw; x++)
        {
            CopyPixel(x, y, y, x);
        }

    FourWaySymmetry();
}

void ImageData::DetermineRealBounds(uint16_t *bottom, uint16_t *left, uint16_t *right, uint16_t *top,
                                    RGBAColor background_color, int from_x, int to_x, int from_y, int to_y)
{
    from_x = HMM_MAX(0, from_x);
    to_x   = HMM_MIN(to_x, width_ - 1);
    from_y = HMM_MAX(0, from_y);
    to_y   = HMM_MIN(to_y, height_ - 1);

    EPI_ASSERT(bottom || left || right || top);

    if (left)
    {
        *left      = from_x;
        bool found = false;
        while (*left < to_x)
        {
            for (int y = from_y; y < to_y; ++y)
            {
                const uint8_t *src = PixelAt(*left, y);
                if (background_color == kRGBATransparent)
                {
                    if (src[3] != 0)
                    {
                        found = true;
                        break;
                    }
                }
                else
                {
                    if (epi::MakeRGBA(src[0], src[1], src[2]) != background_color)
                    {
                        found = true;
                        break;
                    }
                }
            }
            if (found)
                break;
            ++*left;
        }
    }
    if (bottom)
    {
        *bottom    = from_y;
        bool found = false;
        while (*bottom < to_y)
        {
            for (int x = from_x; x < to_x; ++x)
            {
                const uint8_t *src = PixelAt(x, *bottom);
                if (background_color == kRGBATransparent)
                {
                    if (src[3] != 0)
                    {
                        found = true;
                        break;
                    }
                }
                else
                {
                    if (epi::MakeRGBA(src[0], src[1], src[2]) != background_color)
                    {
                        found = true;
                        break;
                    }
                }
            }
            if (found)
                break;
            ++*bottom;
        }
    }
    if (right)
    {
        *right     = to_x;
        bool found = false;
        while (*right > from_x)
        {
            for (int y = from_y; y < to_y; ++y)
            {
                const uint8_t *src = PixelAt(*right, y);
                if (background_color == kRGBATransparent)
                {
                    if (src[3] != 0)
                    {
                        found = true;
                        break;
                    }
                }
                else
                {
                    if (epi::MakeRGBA(src[0], src[1], src[2]) != background_color)
                    {
                        found = true;
                        break;
                    }
                }
            }
            if (found)
                break;
            --*right;
        }
    }
    if (top)
    {
        *top       = to_y;
        bool found = false;
        while (*top > from_y)
        {
            for (int x = from_x; x < to_x; ++x)
            {
                const uint8_t *src = PixelAt(x, *top);
                if (background_color == kRGBATransparent)
                {
                    if (src[3] != 0)
                    {
                        found = true;
                        break;
                    }
                }
                else
                {
                    if (epi::MakeRGBA(src[0], src[1], src[2]) != background_color)
                    {
                        found = true;
                        break;
                    }
                }
            }
            if (found)
                break;
            --*top;
        }
    }
}

RGBAColor ImageData::AverageHue(int from_x, int to_x, int from_y, int to_y)
{
    // make sure we don't overflow
    EPI_ASSERT(width_ * height_ <= 2048 * 2048);

    int r_sum = 0;
    int g_sum = 0;
    int b_sum = 0;

    int weight = 0;

    uint8_t hue[3] = {0, 0, 0};

    // Sanity checking; at a minimum sample a 1x1 portion of the image
    from_x = HMM_Clamp(0, from_x, width_ - 1);
    to_x   = HMM_Clamp(1, to_x, width_);
    from_y = HMM_Clamp(0, from_y, height_ - 1);
    to_y   = HMM_Clamp(1, to_y, height_);

    for (int y = from_y; y < to_y; y++)
    {
        const uint8_t *src = PixelAt(0, y);

        for (int x = from_x; x < to_x; x++, src += depth_)
        {
            int r = src[0];
            int g = src[1];
            int b = src[2];
            int a = (depth_ == 4) ? src[3] : 255;

            int v = HMM_MAX(r, HMM_MAX(g, b));

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

    return epi::MakeRGBA(hue[0], hue[1], hue[2]);
}

RGBAColor ImageData::AverageColor(int from_x, int to_x, int from_y, int to_y)
{
    // make sure we don't overflow
    EPI_ASSERT(width_ * height_ <= 2048 * 2048);

    std::unordered_map<RGBAColor, unsigned int> seen_colors;

    // Sanity checking; at a minimum sample a 1x1 portion of the image
    from_x = HMM_Clamp(0, from_x, width_ - 1);
    to_x   = HMM_Clamp(1, to_x, width_);
    from_y = HMM_Clamp(0, from_y, height_ - 1);
    to_y   = HMM_Clamp(1, to_y, height_);

    for (int y = from_y; y < to_y; y++)
    {
        const uint8_t *src = PixelAt(0, y);

        for (int x = from_x; x < to_x; x++, src += depth_)
        {
            if (depth_ == 4 && src[3] == 0)
                continue;
            RGBAColor color = epi::MakeRGBA(src[0], src[1], src[2]);
            auto      res   = seen_colors.try_emplace(color, 0);
            // If color already seen, increment the hit counter
            if (!res.second)
                res.first->second++;
        }
    }

    unsigned int highest_count = 0;
    RGBAColor    average_color = kRGBABlack;
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

RGBAColor ImageData::LightestColor(int from_x, int to_x, int from_y, int to_y)
{
    // make sure we don't overflow
    EPI_ASSERT(width_ * height_ <= 2048 * 2048);

    int lightest_total = 0;
    int lightest_r     = 0;
    int lightest_g     = 0;
    int lightest_b     = 0;

    // Sanity checking; at a minimum sample a 1x1 portion of the image
    from_x = HMM_Clamp(0, from_x, width_ - 1);
    to_x   = HMM_Clamp(1, to_x, width_);
    from_y = HMM_Clamp(0, from_y, height_ - 1);
    to_y   = HMM_Clamp(1, to_y, height_);

    for (int y = from_y; y < to_y; y++)
    {
        const uint8_t *src = PixelAt(0, y);

        for (int x = from_x; x < to_x; x++, src += depth_)
        {
            if (depth_ == 4 && src[3] == 0)
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

RGBAColor ImageData::DarkestColor(int from_x, int to_x, int from_y, int to_y)
{
    // make sure we don't overflow
    EPI_ASSERT(width_ * height_ <= 2048 * 2048);

    int darkest_total = 765;
    int darkest_r     = 0;
    int darkest_g     = 0;
    int darkest_b     = 0;

    // Sanity checking; at a minimum sample a 1x1 portion of the image
    from_x = HMM_Clamp(0, from_x, width_ - 1);
    to_x   = HMM_Clamp(1, to_x, width_);
    from_y = HMM_Clamp(0, from_y, height_ - 1);
    to_y   = HMM_Clamp(1, to_y, height_);

    for (int y = from_y; y < to_y; y++)
    {
        const uint8_t *src = PixelAt(0, y);

        for (int x = from_x; x < to_x; x++, src += depth_)
        {
            if (depth_ == 4 && src[3] == 0)
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

void ImageData::Swirl(int leveltime, int thickness)
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

    uint8_t *new_pixels_ = new uint8_t[width_ * height_ * depth_];

    int x, y;

    // SMMU swirling algorithm
    for (x = 0; x < width_; x++)
    {
        for (y = 0; y < height_; y++)
        {
            int x1, y1;
            int sinvalue, sinvalue2;

            sinvalue  = (y * swirlfactor + leveltime * speed * 5 + 900) & 8191;
            sinvalue2 = (x * swirlfactor2 + leveltime * speed * 4 + 300) & 8191;
            x1        = x + width_ + height_ + ((finesine[sinvalue] * amp) >> 16) + ((finesine[sinvalue2] * amp) >> 16);

            sinvalue  = (x * swirlfactor + leveltime * speed * 3 + 700) & 8191;
            sinvalue2 = (y * swirlfactor2 + leveltime * speed * 4 + 1200) & 8191;
            y1        = y + width_ + height_ + ((finesine[sinvalue] * amp) >> 16) + ((finesine[sinvalue2] * amp) >> 16);

            x1 &= width_ - 1;
            y1 &= height_ - 1;

            uint8_t *src  = pixels_ + (y1 * width_ + x1) * depth_;
            uint8_t *dest = new_pixels_ + (y * width_ + x) * depth_;

            memcpy(dest, src, depth_);
        }
    }
    delete[] pixels_;
    pixels_ = new_pixels_;
}

void ImageData::SetHSV(int rotation, int saturation, int value)
{
    EPI_ASSERT(depth_ >= 3);

    rotation   = HMM_Clamp(-1800, rotation, 1800);
    saturation = HMM_Clamp(-1, saturation, 255);

    for (int y = 0; y < height_; y++)
        for (int x = 0; x < width_; x++)
        {
            uint8_t *src = PixelAt(x, y);

            RGBAColor col = epi::MakeRGBA(src[0], src[1], src[2], depth_ == 4 ? src[3] : 255);

            epi::HSVColor hue(col);

            if (rotation)
                hue.Rotate(rotation);

            if (saturation > -1)
                hue.SetSaturation(saturation);

            if (value)
                hue.SetValue(HMM_Clamp(0, hue.v_ + value, 255));

            col = hue.ToRGBA();

            src[0] = epi::GetRGBARed(col);
            src[1] = epi::GetRGBAGreen(col);
            src[2] = epi::GetRGBABlue(col);
        }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
