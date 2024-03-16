//------------------------------------------------------------------------
//  EDGE Image Filtering/Scaling
//------------------------------------------------------------------------
//
//  Copyright (c) 2007-2024 The EDGE Team.
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
//  Blur is based on "C++ implementation of a fast Gaussian blur algorithm by
//    Ivan Kutskir - Integer Version"
//
//  Copyright (C) 2017 Basile Fraboni
//  Copyright (C) 2014 Ivan Kutskir
//  All Rights Reserved
//  You may use, distribute and modify this code under the
//  terms of the MIT license. For further details please refer
//  to : https://mit-license.org/
//
//----------------------------------------------------------------------------
//
//  HQ2x is based heavily on the code (C) 2003 Maxim Stepin, which is
//  under the GNU LGPL (Lesser General Public License).
//
//  For more information, see: http://hiend3d.com/hq2x.html
//
//----------------------------------------------------------------------------

#include "im_filter.h"

#include <math.h>
#include <string.h>

#include <algorithm>

#include "HandmadeMath.h"
#include "epi.h"

static void SigmaToBox(int boxes[], float sigma, int n)
{
    // ideal filter width
    float wi = sqrt((12 * sigma * sigma / n) + 1);
    int   wl = floor(wi);
    if (wl % 2 == 0)
        wl--;
    int wu = wl + 2;

    float mi = (12 * sigma * sigma - n * wl * wl - 4 * n * wl - 3 * n) / (-4 * wl - 4);
    int   m  = round(mi);

    for (int i = 0; i < n; i++)
        boxes[i] = ((i < m ? wl : wu) - 1) / 2;
}

static void HorizontalBlurRgb(uint8_t *in, uint8_t *out, int w, int h, int c, int r)
{
    float iarr = 1.f / (r + r + 1);
    for (int i = 0; i < h; i++)
    {
        int ti = i * w;
        int li = ti;
        int ri = ti + r;

        int fv[3]  = {in[ti * c + 0], in[ti * c + 1], in[ti * c + 2]};
        int lv[3]  = {in[(ti + w - 1) * c + 0], in[(ti + w - 1) * c + 1], in[(ti + w - 1) * c + 2]};
        int val[3] = {(r + 1) * fv[0], (r + 1) * fv[1], (r + 1) * fv[2]};

        for (int j = 0; j < r; j++)
        {
            val[0] += in[(ti + j) * c + 0];
            val[1] += in[(ti + j) * c + 1];
            val[2] += in[(ti + j) * c + 2];
        }

        for (int j = 0; j <= r; j++, ri++, ti++)
        {
            val[0] += in[ri * c + 0] - fv[0];
            val[1] += in[ri * c + 1] - fv[1];
            val[2] += in[ri * c + 2] - fv[2];
            out[ti * c + 0] = round(val[0] * iarr);
            out[ti * c + 1] = round(val[1] * iarr);
            out[ti * c + 2] = round(val[2] * iarr);
        }

        for (int j = r + 1; j < w - r; j++, ri++, ti++, li++)
        {
            val[0] += in[ri * c + 0] - in[li * c + 0];
            val[1] += in[ri * c + 1] - in[li * c + 1];
            val[2] += in[ri * c + 2] - in[li * c + 2];
            out[ti * c + 0] = round(val[0] * iarr);
            out[ti * c + 1] = round(val[1] * iarr);
            out[ti * c + 2] = round(val[2] * iarr);
        }

        for (int j = w - r; j < w; j++, ti++, li++)
        {
            val[0] += lv[0] - in[li * c + 0];
            val[1] += lv[1] - in[li * c + 1];
            val[2] += lv[2] - in[li * c + 2];
            out[ti * c + 0] = round(val[0] * iarr);
            out[ti * c + 1] = round(val[1] * iarr);
            out[ti * c + 2] = round(val[2] * iarr);
        }
    }
}

static void TotalBlurRgb(uint8_t *in, uint8_t *out, int w, int h, int c, int r)
{
    // radius range on either side of a pixel + the pixel itself
    float iarr = 1.f / (r + r + 1);
    for (int i = 0; i < w; i++)
    {
        int ti = i;
        int li = ti;
        int ri = ti + r * w;

        int fv[3]  = {in[ti * c + 0], in[ti * c + 1], in[ti * c + 2]};
        int lv[3]  = {in[(ti + w * (h - 1)) * c + 0], in[(ti + w * (h - 1)) * c + 1], in[(ti + w * (h - 1)) * c + 2]};
        int val[3] = {(r + 1) * fv[0], (r + 1) * fv[1], (r + 1) * fv[2]};

        for (int j = 0; j < r; j++)
        {
            val[0] += in[(ti + j * w) * c + 0];
            val[1] += in[(ti + j * w) * c + 1];
            val[2] += in[(ti + j * w) * c + 2];
        }

        for (int j = 0; j <= r; j++, ri += w, ti += w)
        {
            val[0] += in[ri * c + 0] - fv[0];
            val[1] += in[ri * c + 1] - fv[1];
            val[2] += in[ri * c + 2] - fv[2];
            out[ti * c + 0] = round(val[0] * iarr);
            out[ti * c + 1] = round(val[1] * iarr);
            out[ti * c + 2] = round(val[2] * iarr);
        }

        for (int j = r + 1; j < h - r; j++, ri += w, ti += w, li += w)
        {
            val[0] += in[ri * c + 0] - in[li * c + 0];
            val[1] += in[ri * c + 1] - in[li * c + 1];
            val[2] += in[ri * c + 2] - in[li * c + 2];
            out[ti * c + 0] = round(val[0] * iarr);
            out[ti * c + 1] = round(val[1] * iarr);
            out[ti * c + 2] = round(val[2] * iarr);
        }

        for (int j = h - r; j < h; j++, ti += w, li += w)
        {
            val[0] += lv[0] - in[li * c + 0];
            val[1] += lv[1] - in[li * c + 1];
            val[2] += lv[2] - in[li * c + 2];
            out[ti * c + 0] = round(val[0] * iarr);
            out[ti * c + 1] = round(val[1] * iarr);
            out[ti * c + 2] = round(val[2] * iarr);
        }
    }
}

static void BoxBlurRgb(uint8_t *&in, uint8_t *&out, int w, int h, int c, int r)
{
    std::swap(in, out);
    HorizontalBlurRgb(out, in, w, h, c, r);
    TotalBlurRgb(in, out, w, h, c, r);
}

ImageData *ImageBlur(ImageData *image, float sigma)
{
    EPI_ASSERT(image->depth_ >= 3);

    int w = image->width_;
    int h = image->height_;
    int c = image->depth_;

    ImageData *result = new ImageData(w, h, c);

    int box;
    SigmaToBox(&box, sigma, 1);
    BoxBlurRgb(image->pixels_, result->pixels_, w, h, c, box);

    return result;
}

static uint32_t pixel_rgb[256];
static uint32_t pixel_yuv[256];

static constexpr uint32_t a_mask = 0xFF000000;
static constexpr uint32_t y_mask = 0x00FF0000;
static constexpr uint32_t u_mask = 0x0000FF00;
static constexpr uint32_t v_mask = 0x000000FF;

// no idea what "tr" stands for - Dasho
static constexpr uint32_t tr_y = 0x00300000;
static constexpr uint32_t tr_u = 0x00000700;
static constexpr uint32_t tr_v = 0x00000007; // -AJA- changed (was 6)

static inline uint32_t Hq2xGetR(uint32_t col)
{
    return (col >> 16) & 0xFF;
}
static inline uint32_t Hq2xGetG(uint32_t col)
{
    return (col >> 8) & 0xFF;
}
static inline uint32_t Hq2xGetB(uint32_t col)
{
    return (col) & 0xFF;
}
static inline uint32_t Hq2xGetA(uint32_t col)
{
    return (col >> 24) & 0xFF;
}

static inline void InterpolateColor(uint8_t *dest, uint32_t c1, uint32_t c2, uint32_t c3, uint32_t f1, uint32_t f2,
                                    uint32_t f3, uint32_t shift)
{
    dest[0] = (Hq2xGetR(c1) * f1 + Hq2xGetR(c2) * f2 + Hq2xGetR(c3) * f3) >> shift;
    dest[1] = (Hq2xGetG(c1) * f1 + Hq2xGetG(c2) * f2 + Hq2xGetG(c3) * f3) >> shift;
    dest[2] = (Hq2xGetB(c1) * f1 + Hq2xGetB(c2) * f2 + Hq2xGetB(c3) * f3) >> shift;
    dest[3] = (Hq2xGetA(c1) * f1 + Hq2xGetA(c2) * f2 + Hq2xGetA(c3) * f3) >> shift;
}

static void Interpolate0(uint8_t *dest, uint32_t c1)
{
    dest[0] = Hq2xGetR(c1);
    dest[1] = Hq2xGetG(c1);
    dest[2] = Hq2xGetB(c1);
    dest[3] = Hq2xGetA(c1);
}

static void Interpolate1(uint8_t *dest, uint32_t c1, uint32_t c2)
{
    InterpolateColor(dest, c1, c2, 0, 3, 1, 0, 2);
}

static void Interpolate2(uint8_t *dest, uint32_t c1, uint32_t c2, uint32_t c3)
{
    InterpolateColor(dest, c1, c2, c3, 2, 1, 1, 2);
}

static void Interpolate6(uint8_t *dest, uint32_t c1, uint32_t c2, uint32_t c3)
{
    InterpolateColor(dest, c1, c2, c3, 5, 2, 1, 3);
}

static void Interpolate7(uint8_t *dest, uint32_t c1, uint32_t c2, uint32_t c3)
{
    InterpolateColor(dest, c1, c2, c3, 6, 1, 1, 3);
}

static void Interpolate9(uint8_t *dest, uint32_t c1, uint32_t c2, uint32_t c3)
{
    InterpolateColor(dest, c1, c2, c3, 2, 3, 3, 3);
}

static void Interpolate10(uint8_t *dest, uint32_t c1, uint32_t c2, uint32_t c3)
{
    InterpolateColor(dest, c1, c2, c3, 14, 1, 1, 4);
}

inline bool YuvDiff(const uint8_t p1, const uint8_t p2)
{
    uint32_t YUV1 = pixel_yuv[p1];
    uint32_t YUV2 = pixel_yuv[p2];

    return (YUV1 & a_mask) != (YUV2 & a_mask) || HMM_ABS((int)((YUV1 & y_mask) - (YUV2 & y_mask))) > tr_y ||
           HMM_ABS((int)((YUV1 & u_mask) - (YUV2 & u_mask))) > tr_u ||
           HMM_ABS((int)((YUV1 & v_mask) - (YUV2 & v_mask))) > tr_v;
}

void Hq2xPaletteSetup(const uint8_t *palette, int transparent_pixel)
{
    for (int c = 0; c < 256; c++)
    {
        int r = palette[c * 3 + 0];
        int g = palette[c * 3 + 1];
        int b = palette[c * 3 + 2];
        int A = 255;

        if (c == transparent_pixel)
            r = g = b = A = 0;

        pixel_rgb[c] = ((A << 24) + (r << 16) + (g << 8) + b);

        // -AJA- changed to better formulas (based on Wikipedia article)
        int Y = (r * 77 + g * 150 + b * 29) >> 8;
        int u = 128 + ((-r * 38 - g * 74 + b * 111) >> 9);
        int v = 128 + ((r * 157 - g * 132 - b * 26) >> 9);

        pixel_yuv[c] = ((A << 24) + (Y << 16) + (u << 8) + v);
    }
}

void ConvertLine(int y, int w, int h, bool invert, uint8_t *dest, const uint8_t *src)
{
    int prevline = (y > 0) ? -w : 0;
    int nextline = (y < h - 1) ? w : 0;

    // Bytes per Line (destination)
    int BpL = (w * 8);

    if (invert)
    {
        dest += BpL;
        BpL = 0 - BpL;
    }

    uint8_t  p[10]; // palette pixels
    uint32_t c[10]; // RGBA pixels

    //   +----+----+----+
    //   |    |    |    |
    //   | p1 | p2 | p3 |
    //   +----+----+----+
    //   |    |    |    |
    //   | p4 | p5 | p6 |
    //   +----+----+----+
    //   |    |    |    |
    //   | p7 | p8 | p9 |
    //   +----+----+----+

    for (int x = 0; x < w; x++, src++, dest += 8 /* RGBA */)
    {
        p[2] = src[prevline];
        p[5] = src[0];
        p[8] = src[nextline];

        if (x > 0)
        {
            p[1] = src[prevline - 1];
            p[4] = src[-1];
            p[7] = src[nextline - 1];
        }
        else
        {
            p[1] = p[2];
            p[4] = p[5];
            p[7] = p[8];
        }

        if (x < w - 1)
        {
            p[3] = src[prevline + 1];
            p[6] = src[1];
            p[9] = src[nextline + 1];
        }
        else
        {
            p[3] = p[2];
            p[6] = p[5];
            p[9] = p[8];
        }

        for (int k = 1; k <= 9; k++)
            c[k] = pixel_rgb[p[k]];

        uint8_t pattern = 0;

        if (YuvDiff(p[5], p[1]))
            pattern |= 0x01;
        if (YuvDiff(p[5], p[2]))
            pattern |= 0x02;
        if (YuvDiff(p[5], p[3]))
            pattern |= 0x04;
        if (YuvDiff(p[5], p[4]))
            pattern |= 0x08;
        if (YuvDiff(p[5], p[6]))
            pattern |= 0x10;
        if (YuvDiff(p[5], p[7]))
            pattern |= 0x20;
        if (YuvDiff(p[5], p[8]))
            pattern |= 0x40;
        if (YuvDiff(p[5], p[9]))
            pattern |= 0x80;

        switch (pattern)
        {
        case 0:
        case 1:
        case 4:
        case 32:
        case 128:
        case 5:
        case 132:
        case 160:
        case 33:
        case 129:
        case 36:
        case 133:
        case 164:
        case 161:
        case 37:
        case 165: {
            Interpolate2(dest, c[5], c[4], c[2]);
            Interpolate2(dest + 4, c[5], c[2], c[6]);
            Interpolate2(dest + BpL, c[5], c[8], c[4]);
            Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            break;
        }
        case 2:
        case 34:
        case 130:
        case 162: {
            Interpolate2(dest, c[5], c[1], c[4]);
            Interpolate2(dest + 4, c[5], c[3], c[6]);
            Interpolate2(dest + BpL, c[5], c[8], c[4]);
            Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            break;
        }
        case 16:
        case 17:
        case 48:
        case 49: {
            Interpolate2(dest, c[5], c[4], c[2]);
            Interpolate2(dest + 4, c[5], c[3], c[2]);
            Interpolate2(dest + BpL, c[5], c[8], c[4]);
            Interpolate2(dest + BpL + 4, c[5], c[9], c[8]);
            break;
        }
        case 64:
        case 65:
        case 68:
        case 69: {
            Interpolate2(dest, c[5], c[4], c[2]);
            Interpolate2(dest + 4, c[5], c[2], c[6]);
            Interpolate2(dest + BpL, c[5], c[7], c[4]);
            Interpolate2(dest + BpL + 4, c[5], c[9], c[6]);
            break;
        }
        case 8:
        case 12:
        case 136:
        case 140: {
            Interpolate2(dest, c[5], c[1], c[2]);
            Interpolate2(dest + 4, c[5], c[2], c[6]);
            Interpolate2(dest + BpL, c[5], c[7], c[8]);
            Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            break;
        }
        case 3:
        case 35:
        case 131:
        case 163: {
            Interpolate1(dest, c[5], c[4]);
            Interpolate2(dest + 4, c[5], c[3], c[6]);
            Interpolate2(dest + BpL, c[5], c[8], c[4]);
            Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            break;
        }
        case 6:
        case 38:
        case 134:
        case 166: {
            Interpolate2(dest, c[5], c[1], c[4]);
            Interpolate1(dest + 4, c[5], c[6]);
            Interpolate2(dest + BpL, c[5], c[8], c[4]);
            Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            break;
        }
        case 20:
        case 21:
        case 52:
        case 53: {
            Interpolate2(dest, c[5], c[4], c[2]);
            Interpolate1(dest + 4, c[5], c[2]);
            Interpolate2(dest + BpL, c[5], c[8], c[4]);
            Interpolate2(dest + BpL + 4, c[5], c[9], c[8]);
            break;
        }
        case 144:
        case 145:
        case 176:
        case 177: {
            Interpolate2(dest, c[5], c[4], c[2]);
            Interpolate2(dest + 4, c[5], c[3], c[2]);
            Interpolate2(dest + BpL, c[5], c[8], c[4]);
            Interpolate1(dest + BpL + 4, c[5], c[8]);
            break;
        }
        case 192:
        case 193:
        case 196:
        case 197: {
            Interpolate2(dest, c[5], c[4], c[2]);
            Interpolate2(dest + 4, c[5], c[2], c[6]);
            Interpolate2(dest + BpL, c[5], c[7], c[4]);
            Interpolate1(dest + BpL + 4, c[5], c[6]);
            break;
        }
        case 96:
        case 97:
        case 100:
        case 101: {
            Interpolate2(dest, c[5], c[4], c[2]);
            Interpolate2(dest + 4, c[5], c[2], c[6]);
            Interpolate1(dest + BpL, c[5], c[4]);
            Interpolate2(dest + BpL + 4, c[5], c[9], c[6]);
            break;
        }
        case 40:
        case 44:
        case 168:
        case 172: {
            Interpolate2(dest, c[5], c[1], c[2]);
            Interpolate2(dest + 4, c[5], c[2], c[6]);
            Interpolate1(dest + BpL, c[5], c[8]);
            Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            break;
        }
        case 9:
        case 13:
        case 137:
        case 141: {
            Interpolate1(dest, c[5], c[2]);
            Interpolate2(dest + 4, c[5], c[2], c[6]);
            Interpolate2(dest + BpL, c[5], c[7], c[8]);
            Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            break;
        }
        case 18:
        case 50: {
            Interpolate2(dest, c[5], c[1], c[4]);
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate1(dest + 4, c[5], c[3]);
            }
            else
            {
                Interpolate2(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate2(dest + BpL, c[5], c[8], c[4]);
            Interpolate2(dest + BpL + 4, c[5], c[9], c[8]);
            break;
        }
        case 80:
        case 81: {
            Interpolate2(dest, c[5], c[4], c[2]);
            Interpolate2(dest + 4, c[5], c[3], c[2]);
            Interpolate2(dest + BpL, c[5], c[7], c[4]);
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate1(dest + BpL + 4, c[5], c[9]);
            }
            else
            {
                Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 72:
        case 76: {
            Interpolate2(dest, c[5], c[1], c[2]);
            Interpolate2(dest + 4, c[5], c[2], c[6]);
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate1(dest + BpL, c[5], c[7]);
            }
            else
            {
                Interpolate2(dest + BpL, c[5], c[8], c[4]);
            }
            Interpolate2(dest + BpL + 4, c[5], c[9], c[6]);
            break;
        }
        case 10:
        case 138: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate1(dest, c[5], c[1]);
            }
            else
            {
                Interpolate2(dest, c[5], c[4], c[2]);
            }
            Interpolate2(dest + 4, c[5], c[3], c[6]);
            Interpolate2(dest + BpL, c[5], c[7], c[8]);
            Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            break;
        }
        case 66: {
            Interpolate2(dest, c[5], c[1], c[4]);
            Interpolate2(dest + 4, c[5], c[3], c[6]);
            Interpolate2(dest + BpL, c[5], c[7], c[4]);
            Interpolate2(dest + BpL + 4, c[5], c[9], c[6]);
            break;
        }
        case 24: {
            Interpolate2(dest, c[5], c[1], c[2]);
            Interpolate2(dest + 4, c[5], c[3], c[2]);
            Interpolate2(dest + BpL, c[5], c[7], c[8]);
            Interpolate2(dest + BpL + 4, c[5], c[9], c[8]);
            break;
        }
        case 7:
        case 39:
        case 135: {
            Interpolate1(dest, c[5], c[4]);
            Interpolate1(dest + 4, c[5], c[6]);
            Interpolate2(dest + BpL, c[5], c[8], c[4]);
            Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            break;
        }
        case 148:
        case 149:
        case 180: {
            Interpolate2(dest, c[5], c[4], c[2]);
            Interpolate1(dest + 4, c[5], c[2]);
            Interpolate2(dest + BpL, c[5], c[8], c[4]);
            Interpolate1(dest + BpL + 4, c[5], c[8]);
            break;
        }
        case 224:
        case 228:
        case 225: {
            Interpolate2(dest, c[5], c[4], c[2]);
            Interpolate2(dest + 4, c[5], c[2], c[6]);
            Interpolate1(dest + BpL, c[5], c[4]);
            Interpolate1(dest + BpL + 4, c[5], c[6]);
            break;
        }
        case 41:
        case 169:
        case 45: {
            Interpolate1(dest, c[5], c[2]);
            Interpolate2(dest + 4, c[5], c[2], c[6]);
            Interpolate1(dest + BpL, c[5], c[8]);
            Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            break;
        }
        case 22:
        case 54: {
            Interpolate2(dest, c[5], c[1], c[4]);
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate0(dest + 4, c[5]);
            }
            else
            {
                Interpolate2(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate2(dest + BpL, c[5], c[8], c[4]);
            Interpolate2(dest + BpL + 4, c[5], c[9], c[8]);
            break;
        }
        case 208:
        case 209: {
            Interpolate2(dest, c[5], c[4], c[2]);
            Interpolate2(dest + 4, c[5], c[3], c[2]);
            Interpolate2(dest + BpL, c[5], c[7], c[4]);
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate0(dest + BpL + 4, c[5]);
            }
            else
            {
                Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 104:
        case 108: {
            Interpolate2(dest, c[5], c[1], c[2]);
            Interpolate2(dest + 4, c[5], c[2], c[6]);
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate0(dest + BpL, c[5]);
            }
            else
            {
                Interpolate2(dest + BpL, c[5], c[8], c[4]);
            }
            Interpolate2(dest + BpL + 4, c[5], c[9], c[6]);
            break;
        }
        case 11:
        case 139: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate0(dest, c[5]);
            }
            else
            {
                Interpolate2(dest, c[5], c[4], c[2]);
            }
            Interpolate2(dest + 4, c[5], c[3], c[6]);
            Interpolate2(dest + BpL, c[5], c[7], c[8]);
            Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            break;
        }
        case 19:
        case 51: {
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate1(dest, c[5], c[4]);
                Interpolate1(dest + 4, c[5], c[3]);
            }
            else
            {
                Interpolate6(dest, c[5], c[2], c[4]);
                Interpolate9(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate2(dest + BpL, c[5], c[8], c[4]);
            Interpolate2(dest + BpL + 4, c[5], c[9], c[8]);
            break;
        }
        case 146:
        case 178: {
            Interpolate2(dest, c[5], c[1], c[4]);
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate1(dest + 4, c[5], c[3]);
                Interpolate1(dest + BpL + 4, c[5], c[8]);
            }
            else
            {
                Interpolate9(dest + 4, c[5], c[2], c[6]);
                Interpolate6(dest + BpL + 4, c[5], c[6], c[8]);
            }
            Interpolate2(dest + BpL, c[5], c[8], c[4]);
            break;
        }
        case 84:
        case 85: {
            Interpolate2(dest, c[5], c[4], c[2]);
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate1(dest + 4, c[5], c[2]);
                Interpolate1(dest + BpL + 4, c[5], c[9]);
            }
            else
            {
                Interpolate6(dest + 4, c[5], c[6], c[2]);
                Interpolate9(dest + BpL + 4, c[5], c[6], c[8]);
            }
            Interpolate2(dest + BpL, c[5], c[7], c[4]);
            break;
        }
        case 112:
        case 113: {
            Interpolate2(dest, c[5], c[4], c[2]);
            Interpolate2(dest + 4, c[5], c[3], c[2]);
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate1(dest + BpL, c[5], c[4]);
                Interpolate1(dest + BpL + 4, c[5], c[9]);
            }
            else
            {
                Interpolate6(dest + BpL, c[5], c[8], c[4]);
                Interpolate9(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 200:
        case 204: {
            Interpolate2(dest, c[5], c[1], c[2]);
            Interpolate2(dest + 4, c[5], c[2], c[6]);
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate1(dest + BpL, c[5], c[7]);
                Interpolate1(dest + BpL + 4, c[5], c[6]);
            }
            else
            {
                Interpolate9(dest + BpL, c[5], c[8], c[4]);
                Interpolate6(dest + BpL + 4, c[5], c[8], c[6]);
            }
            break;
        }
        case 73:
        case 77: {
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate1(dest, c[5], c[2]);
                Interpolate1(dest + BpL, c[5], c[7]);
            }
            else
            {
                Interpolate6(dest, c[5], c[4], c[2]);
                Interpolate9(dest + BpL, c[5], c[8], c[4]);
            }
            Interpolate2(dest + 4, c[5], c[2], c[6]);
            Interpolate2(dest + BpL + 4, c[5], c[9], c[6]);
            break;
        }
        case 42:
        case 170: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate1(dest, c[5], c[1]);
                Interpolate1(dest + BpL, c[5], c[8]);
            }
            else
            {
                Interpolate9(dest, c[5], c[4], c[2]);
                Interpolate6(dest + BpL, c[5], c[4], c[8]);
            }
            Interpolate2(dest + 4, c[5], c[3], c[6]);
            Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            break;
        }
        case 14:
        case 142: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate1(dest, c[5], c[1]);
                Interpolate1(dest + 4, c[5], c[6]);
            }
            else
            {
                Interpolate9(dest, c[5], c[4], c[2]);
                Interpolate6(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate2(dest + BpL, c[5], c[7], c[8]);
            Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            break;
        }
        case 67: {
            Interpolate1(dest, c[5], c[4]);
            Interpolate2(dest + 4, c[5], c[3], c[6]);
            Interpolate2(dest + BpL, c[5], c[7], c[4]);
            Interpolate2(dest + BpL + 4, c[5], c[9], c[6]);
            break;
        }
        case 70: {
            Interpolate2(dest, c[5], c[1], c[4]);
            Interpolate1(dest + 4, c[5], c[6]);
            Interpolate2(dest + BpL, c[5], c[7], c[4]);
            Interpolate2(dest + BpL + 4, c[5], c[9], c[6]);
            break;
        }
        case 28: {
            Interpolate2(dest, c[5], c[1], c[2]);
            Interpolate1(dest + 4, c[5], c[2]);
            Interpolate2(dest + BpL, c[5], c[7], c[8]);
            Interpolate2(dest + BpL + 4, c[5], c[9], c[8]);
            break;
        }
        case 152: {
            Interpolate2(dest, c[5], c[1], c[2]);
            Interpolate2(dest + 4, c[5], c[3], c[2]);
            Interpolate2(dest + BpL, c[5], c[7], c[8]);
            Interpolate1(dest + BpL + 4, c[5], c[8]);
            break;
        }
        case 194: {
            Interpolate2(dest, c[5], c[1], c[4]);
            Interpolate2(dest + 4, c[5], c[3], c[6]);
            Interpolate2(dest + BpL, c[5], c[7], c[4]);
            Interpolate1(dest + BpL + 4, c[5], c[6]);
            break;
        }
        case 98: {
            Interpolate2(dest, c[5], c[1], c[4]);
            Interpolate2(dest + 4, c[5], c[3], c[6]);
            Interpolate1(dest + BpL, c[5], c[4]);
            Interpolate2(dest + BpL + 4, c[5], c[9], c[6]);
            break;
        }
        case 56: {
            Interpolate2(dest, c[5], c[1], c[2]);
            Interpolate2(dest + 4, c[5], c[3], c[2]);
            Interpolate1(dest + BpL, c[5], c[8]);
            Interpolate2(dest + BpL + 4, c[5], c[9], c[8]);
            break;
        }
        case 25: {
            Interpolate1(dest, c[5], c[2]);
            Interpolate2(dest + 4, c[5], c[3], c[2]);
            Interpolate2(dest + BpL, c[5], c[7], c[8]);
            Interpolate2(dest + BpL + 4, c[5], c[9], c[8]);
            break;
        }
        case 26:
        case 31: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate0(dest, c[5]);
            }
            else
            {
                Interpolate2(dest, c[5], c[4], c[2]);
            }
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate0(dest + 4, c[5]);
            }
            else
            {
                Interpolate2(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate2(dest + BpL, c[5], c[7], c[8]);
            Interpolate2(dest + BpL + 4, c[5], c[9], c[8]);
            break;
        }
        case 82:
        case 214: {
            Interpolate2(dest, c[5], c[1], c[4]);
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate0(dest + 4, c[5]);
            }
            else
            {
                Interpolate2(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate2(dest + BpL, c[5], c[7], c[4]);
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate0(dest + BpL + 4, c[5]);
            }
            else
            {
                Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 88:
        case 248: {
            Interpolate2(dest, c[5], c[1], c[2]);
            Interpolate2(dest + 4, c[5], c[3], c[2]);
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate0(dest + BpL, c[5]);
            }
            else
            {
                Interpolate2(dest + BpL, c[5], c[8], c[4]);
            }
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate0(dest + BpL + 4, c[5]);
            }
            else
            {
                Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 74:
        case 107: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate0(dest, c[5]);
            }
            else
            {
                Interpolate2(dest, c[5], c[4], c[2]);
            }
            Interpolate2(dest + 4, c[5], c[3], c[6]);
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate0(dest + BpL, c[5]);
            }
            else
            {
                Interpolate2(dest + BpL, c[5], c[8], c[4]);
            }
            Interpolate2(dest + BpL + 4, c[5], c[9], c[6]);
            break;
        }
        case 27: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate0(dest, c[5]);
            }
            else
            {
                Interpolate2(dest, c[5], c[4], c[2]);
            }
            Interpolate1(dest + 4, c[5], c[3]);
            Interpolate2(dest + BpL, c[5], c[7], c[8]);
            Interpolate2(dest + BpL + 4, c[5], c[9], c[8]);
            break;
        }
        case 86: {
            Interpolate2(dest, c[5], c[1], c[4]);
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate0(dest + 4, c[5]);
            }
            else
            {
                Interpolate2(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate2(dest + BpL, c[5], c[7], c[4]);
            Interpolate1(dest + BpL + 4, c[5], c[9]);
            break;
        }
        case 216: {
            Interpolate2(dest, c[5], c[1], c[2]);
            Interpolate2(dest + 4, c[5], c[3], c[2]);
            Interpolate1(dest + BpL, c[5], c[7]);
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate0(dest + BpL + 4, c[5]);
            }
            else
            {
                Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 106: {
            Interpolate1(dest, c[5], c[1]);
            Interpolate2(dest + 4, c[5], c[3], c[6]);
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate0(dest + BpL, c[5]);
            }
            else
            {
                Interpolate2(dest + BpL, c[5], c[8], c[4]);
            }
            Interpolate2(dest + BpL + 4, c[5], c[9], c[6]);
            break;
        }
        case 30: {
            Interpolate1(dest, c[5], c[1]);
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate0(dest + 4, c[5]);
            }
            else
            {
                Interpolate2(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate2(dest + BpL, c[5], c[7], c[8]);
            Interpolate2(dest + BpL + 4, c[5], c[9], c[8]);
            break;
        }
        case 210: {
            Interpolate2(dest, c[5], c[1], c[4]);
            Interpolate1(dest + 4, c[5], c[3]);
            Interpolate2(dest + BpL, c[5], c[7], c[4]);
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate0(dest + BpL + 4, c[5]);
            }
            else
            {
                Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 120: {
            Interpolate2(dest, c[5], c[1], c[2]);
            Interpolate2(dest + 4, c[5], c[3], c[2]);
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate0(dest + BpL, c[5]);
            }
            else
            {
                Interpolate2(dest + BpL, c[5], c[8], c[4]);
            }
            Interpolate1(dest + BpL + 4, c[5], c[9]);
            break;
        }
        case 75: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate0(dest, c[5]);
            }
            else
            {
                Interpolate2(dest, c[5], c[4], c[2]);
            }
            Interpolate2(dest + 4, c[5], c[3], c[6]);
            Interpolate1(dest + BpL, c[5], c[7]);
            Interpolate2(dest + BpL + 4, c[5], c[9], c[6]);
            break;
        }
        case 29: {
            Interpolate1(dest, c[5], c[2]);
            Interpolate1(dest + 4, c[5], c[2]);
            Interpolate2(dest + BpL, c[5], c[7], c[8]);
            Interpolate2(dest + BpL + 4, c[5], c[9], c[8]);
            break;
        }
        case 198: {
            Interpolate2(dest, c[5], c[1], c[4]);
            Interpolate1(dest + 4, c[5], c[6]);
            Interpolate2(dest + BpL, c[5], c[7], c[4]);
            Interpolate1(dest + BpL + 4, c[5], c[6]);
            break;
        }
        case 184: {
            Interpolate2(dest, c[5], c[1], c[2]);
            Interpolate2(dest + 4, c[5], c[3], c[2]);
            Interpolate1(dest + BpL, c[5], c[8]);
            Interpolate1(dest + BpL + 4, c[5], c[8]);
            break;
        }
        case 99: {
            Interpolate1(dest, c[5], c[4]);
            Interpolate2(dest + 4, c[5], c[3], c[6]);
            Interpolate1(dest + BpL, c[5], c[4]);
            Interpolate2(dest + BpL + 4, c[5], c[9], c[6]);
            break;
        }
        case 57: {
            Interpolate1(dest, c[5], c[2]);
            Interpolate2(dest + 4, c[5], c[3], c[2]);
            Interpolate1(dest + BpL, c[5], c[8]);
            Interpolate2(dest + BpL + 4, c[5], c[9], c[8]);
            break;
        }
        case 71: {
            Interpolate1(dest, c[5], c[4]);
            Interpolate1(dest + 4, c[5], c[6]);
            Interpolate2(dest + BpL, c[5], c[7], c[4]);
            Interpolate2(dest + BpL + 4, c[5], c[9], c[6]);
            break;
        }
        case 156: {
            Interpolate2(dest, c[5], c[1], c[2]);
            Interpolate1(dest + 4, c[5], c[2]);
            Interpolate2(dest + BpL, c[5], c[7], c[8]);
            Interpolate1(dest + BpL + 4, c[5], c[8]);
            break;
        }
        case 226: {
            Interpolate2(dest, c[5], c[1], c[4]);
            Interpolate2(dest + 4, c[5], c[3], c[6]);
            Interpolate1(dest + BpL, c[5], c[4]);
            Interpolate1(dest + BpL + 4, c[5], c[6]);
            break;
        }
        case 60: {
            Interpolate2(dest, c[5], c[1], c[2]);
            Interpolate1(dest + 4, c[5], c[2]);
            Interpolate1(dest + BpL, c[5], c[8]);
            Interpolate2(dest + BpL + 4, c[5], c[9], c[8]);
            break;
        }
        case 195: {
            Interpolate1(dest, c[5], c[4]);
            Interpolate2(dest + 4, c[5], c[3], c[6]);
            Interpolate2(dest + BpL, c[5], c[7], c[4]);
            Interpolate1(dest + BpL + 4, c[5], c[6]);
            break;
        }
        case 102: {
            Interpolate2(dest, c[5], c[1], c[4]);
            Interpolate1(dest + 4, c[5], c[6]);
            Interpolate1(dest + BpL, c[5], c[4]);
            Interpolate2(dest + BpL + 4, c[5], c[9], c[6]);
            break;
        }
        case 153: {
            Interpolate1(dest, c[5], c[2]);
            Interpolate2(dest + 4, c[5], c[3], c[2]);
            Interpolate2(dest + BpL, c[5], c[7], c[8]);
            Interpolate1(dest + BpL + 4, c[5], c[8]);
            break;
        }
        case 58: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate1(dest, c[5], c[1]);
            }
            else
            {
                Interpolate7(dest, c[5], c[4], c[2]);
            }
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate1(dest + 4, c[5], c[3]);
            }
            else
            {
                Interpolate7(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate1(dest + BpL, c[5], c[8]);
            Interpolate2(dest + BpL + 4, c[5], c[9], c[8]);
            break;
        }
        case 83: {
            Interpolate1(dest, c[5], c[4]);
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate1(dest + 4, c[5], c[3]);
            }
            else
            {
                Interpolate7(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate2(dest + BpL, c[5], c[7], c[4]);
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate1(dest + BpL + 4, c[5], c[9]);
            }
            else
            {
                Interpolate7(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 92: {
            Interpolate2(dest, c[5], c[1], c[2]);
            Interpolate1(dest + 4, c[5], c[2]);
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate1(dest + BpL, c[5], c[7]);
            }
            else
            {
                Interpolate7(dest + BpL, c[5], c[8], c[4]);
            }
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate1(dest + BpL + 4, c[5], c[9]);
            }
            else
            {
                Interpolate7(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 202: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate1(dest, c[5], c[1]);
            }
            else
            {
                Interpolate7(dest, c[5], c[4], c[2]);
            }
            Interpolate2(dest + 4, c[5], c[3], c[6]);
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate1(dest + BpL, c[5], c[7]);
            }
            else
            {
                Interpolate7(dest + BpL, c[5], c[8], c[4]);
            }
            Interpolate1(dest + BpL + 4, c[5], c[6]);
            break;
        }
        case 78: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate1(dest, c[5], c[1]);
            }
            else
            {
                Interpolate7(dest, c[5], c[4], c[2]);
            }
            Interpolate1(dest + 4, c[5], c[6]);
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate1(dest + BpL, c[5], c[7]);
            }
            else
            {
                Interpolate7(dest + BpL, c[5], c[8], c[4]);
            }
            Interpolate2(dest + BpL + 4, c[5], c[9], c[6]);
            break;
        }
        case 154: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate1(dest, c[5], c[1]);
            }
            else
            {
                Interpolate7(dest, c[5], c[4], c[2]);
            }
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate1(dest + 4, c[5], c[3]);
            }
            else
            {
                Interpolate7(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate2(dest + BpL, c[5], c[7], c[8]);
            Interpolate1(dest + BpL + 4, c[5], c[8]);
            break;
        }
        case 114: {
            Interpolate2(dest, c[5], c[1], c[4]);
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate1(dest + 4, c[5], c[3]);
            }
            else
            {
                Interpolate7(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate1(dest + BpL, c[5], c[4]);
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate1(dest + BpL + 4, c[5], c[9]);
            }
            else
            {
                Interpolate7(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 89: {
            Interpolate1(dest, c[5], c[2]);
            Interpolate2(dest + 4, c[5], c[3], c[2]);
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate1(dest + BpL, c[5], c[7]);
            }
            else
            {
                Interpolate7(dest + BpL, c[5], c[8], c[4]);
            }
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate1(dest + BpL + 4, c[5], c[9]);
            }
            else
            {
                Interpolate7(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 90: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate1(dest, c[5], c[1]);
            }
            else
            {
                Interpolate7(dest, c[5], c[4], c[2]);
            }
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate1(dest + 4, c[5], c[3]);
            }
            else
            {
                Interpolate7(dest + 4, c[5], c[2], c[6]);
            }
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate1(dest + BpL, c[5], c[7]);
            }
            else
            {
                Interpolate7(dest + BpL, c[5], c[8], c[4]);
            }
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate1(dest + BpL + 4, c[5], c[9]);
            }
            else
            {
                Interpolate7(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 55:
        case 23: {
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate1(dest, c[5], c[4]);
                Interpolate0(dest + 4, c[5]);
            }
            else
            {
                Interpolate6(dest, c[5], c[2], c[4]);
                Interpolate9(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate2(dest + BpL, c[5], c[8], c[4]);
            Interpolate2(dest + BpL + 4, c[5], c[9], c[8]);
            break;
        }
        case 182:
        case 150: {
            Interpolate2(dest, c[5], c[1], c[4]);
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate0(dest + 4, c[5]);
                Interpolate1(dest + BpL + 4, c[5], c[8]);
            }
            else
            {
                Interpolate9(dest + 4, c[5], c[2], c[6]);
                Interpolate6(dest + BpL + 4, c[5], c[6], c[8]);
            }
            Interpolate2(dest + BpL, c[5], c[8], c[4]);
            break;
        }
        case 213:
        case 212: {
            Interpolate2(dest, c[5], c[4], c[2]);
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate1(dest + 4, c[5], c[2]);
                Interpolate0(dest + BpL + 4, c[5]);
            }
            else
            {
                Interpolate6(dest + 4, c[5], c[6], c[2]);
                Interpolate9(dest + BpL + 4, c[5], c[6], c[8]);
            }
            Interpolate2(dest + BpL, c[5], c[7], c[4]);
            break;
        }
        case 241:
        case 240: {
            Interpolate2(dest, c[5], c[4], c[2]);
            Interpolate2(dest + 4, c[5], c[3], c[2]);
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate1(dest + BpL, c[5], c[4]);
                Interpolate0(dest + BpL + 4, c[5]);
            }
            else
            {
                Interpolate6(dest + BpL, c[5], c[8], c[4]);
                Interpolate9(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 236:
        case 232: {
            Interpolate2(dest, c[5], c[1], c[2]);
            Interpolate2(dest + 4, c[5], c[2], c[6]);
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate0(dest + BpL, c[5]);
                Interpolate1(dest + BpL + 4, c[5], c[6]);
            }
            else
            {
                Interpolate9(dest + BpL, c[5], c[8], c[4]);
                Interpolate6(dest + BpL + 4, c[5], c[8], c[6]);
            }
            break;
        }
        case 109:
        case 105: {
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate1(dest, c[5], c[2]);
                Interpolate0(dest + BpL, c[5]);
            }
            else
            {
                Interpolate6(dest, c[5], c[4], c[2]);
                Interpolate9(dest + BpL, c[5], c[8], c[4]);
            }
            Interpolate2(dest + 4, c[5], c[2], c[6]);
            Interpolate2(dest + BpL + 4, c[5], c[9], c[6]);
            break;
        }
        case 171:
        case 43: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate0(dest, c[5]);
                Interpolate1(dest + BpL, c[5], c[8]);
            }
            else
            {
                Interpolate9(dest, c[5], c[4], c[2]);
                Interpolate6(dest + BpL, c[5], c[4], c[8]);
            }
            Interpolate2(dest + 4, c[5], c[3], c[6]);
            Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            break;
        }
        case 143:
        case 15: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate0(dest, c[5]);
                Interpolate1(dest + 4, c[5], c[6]);
            }
            else
            {
                Interpolate9(dest, c[5], c[4], c[2]);
                Interpolate6(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate2(dest + BpL, c[5], c[7], c[8]);
            Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            break;
        }
        case 124: {
            Interpolate2(dest, c[5], c[1], c[2]);
            Interpolate1(dest + 4, c[5], c[2]);
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate0(dest + BpL, c[5]);
            }
            else
            {
                Interpolate2(dest + BpL, c[5], c[8], c[4]);
            }
            Interpolate1(dest + BpL + 4, c[5], c[9]);
            break;
        }
        case 203: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate0(dest, c[5]);
            }
            else
            {
                Interpolate2(dest, c[5], c[4], c[2]);
            }
            Interpolate2(dest + 4, c[5], c[3], c[6]);
            Interpolate1(dest + BpL, c[5], c[7]);
            Interpolate1(dest + BpL + 4, c[5], c[6]);
            break;
        }
        case 62: {
            Interpolate1(dest, c[5], c[1]);
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate0(dest + 4, c[5]);
            }
            else
            {
                Interpolate2(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate1(dest + BpL, c[5], c[8]);
            Interpolate2(dest + BpL + 4, c[5], c[9], c[8]);
            break;
        }
        case 211: {
            Interpolate1(dest, c[5], c[4]);
            Interpolate1(dest + 4, c[5], c[3]);
            Interpolate2(dest + BpL, c[5], c[7], c[4]);
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate0(dest + BpL + 4, c[5]);
            }
            else
            {
                Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 118: {
            Interpolate2(dest, c[5], c[1], c[4]);
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate0(dest + 4, c[5]);
            }
            else
            {
                Interpolate2(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate1(dest + BpL, c[5], c[4]);
            Interpolate1(dest + BpL + 4, c[5], c[9]);
            break;
        }
        case 217: {
            Interpolate1(dest, c[5], c[2]);
            Interpolate2(dest + 4, c[5], c[3], c[2]);
            Interpolate1(dest + BpL, c[5], c[7]);
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate0(dest + BpL + 4, c[5]);
            }
            else
            {
                Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 110: {
            Interpolate1(dest, c[5], c[1]);
            Interpolate1(dest + 4, c[5], c[6]);
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate0(dest + BpL, c[5]);
            }
            else
            {
                Interpolate2(dest + BpL, c[5], c[8], c[4]);
            }
            Interpolate2(dest + BpL + 4, c[5], c[9], c[6]);
            break;
        }
        case 155: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate0(dest, c[5]);
            }
            else
            {
                Interpolate2(dest, c[5], c[4], c[2]);
            }
            Interpolate1(dest + 4, c[5], c[3]);
            Interpolate2(dest + BpL, c[5], c[7], c[8]);
            Interpolate1(dest + BpL + 4, c[5], c[8]);
            break;
        }
        case 188: {
            Interpolate2(dest, c[5], c[1], c[2]);
            Interpolate1(dest + 4, c[5], c[2]);
            Interpolate1(dest + BpL, c[5], c[8]);
            Interpolate1(dest + BpL + 4, c[5], c[8]);
            break;
        }
        case 185: {
            Interpolate1(dest, c[5], c[2]);
            Interpolate2(dest + 4, c[5], c[3], c[2]);
            Interpolate1(dest + BpL, c[5], c[8]);
            Interpolate1(dest + BpL + 4, c[5], c[8]);
            break;
        }
        case 61: {
            Interpolate1(dest, c[5], c[2]);
            Interpolate1(dest + 4, c[5], c[2]);
            Interpolate1(dest + BpL, c[5], c[8]);
            Interpolate2(dest + BpL + 4, c[5], c[9], c[8]);
            break;
        }
        case 157: {
            Interpolate1(dest, c[5], c[2]);
            Interpolate1(dest + 4, c[5], c[2]);
            Interpolate2(dest + BpL, c[5], c[7], c[8]);
            Interpolate1(dest + BpL + 4, c[5], c[8]);
            break;
        }
        case 103: {
            Interpolate1(dest, c[5], c[4]);
            Interpolate1(dest + 4, c[5], c[6]);
            Interpolate1(dest + BpL, c[5], c[4]);
            Interpolate2(dest + BpL + 4, c[5], c[9], c[6]);
            break;
        }
        case 227: {
            Interpolate1(dest, c[5], c[4]);
            Interpolate2(dest + 4, c[5], c[3], c[6]);
            Interpolate1(dest + BpL, c[5], c[4]);
            Interpolate1(dest + BpL + 4, c[5], c[6]);
            break;
        }
        case 230: {
            Interpolate2(dest, c[5], c[1], c[4]);
            Interpolate1(dest + 4, c[5], c[6]);
            Interpolate1(dest + BpL, c[5], c[4]);
            Interpolate1(dest + BpL + 4, c[5], c[6]);
            break;
        }
        case 199: {
            Interpolate1(dest, c[5], c[4]);
            Interpolate1(dest + 4, c[5], c[6]);
            Interpolate2(dest + BpL, c[5], c[7], c[4]);
            Interpolate1(dest + BpL + 4, c[5], c[6]);
            break;
        }
        case 220: {
            Interpolate2(dest, c[5], c[1], c[2]);
            Interpolate1(dest + 4, c[5], c[2]);
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate1(dest + BpL, c[5], c[7]);
            }
            else
            {
                Interpolate7(dest + BpL, c[5], c[8], c[4]);
            }
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate0(dest + BpL + 4, c[5]);
            }
            else
            {
                Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 158: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate1(dest, c[5], c[1]);
            }
            else
            {
                Interpolate7(dest, c[5], c[4], c[2]);
            }
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate0(dest + 4, c[5]);
            }
            else
            {
                Interpolate2(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate2(dest + BpL, c[5], c[7], c[8]);
            Interpolate1(dest + BpL + 4, c[5], c[8]);
            break;
        }
        case 234: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate1(dest, c[5], c[1]);
            }
            else
            {
                Interpolate7(dest, c[5], c[4], c[2]);
            }
            Interpolate2(dest + 4, c[5], c[3], c[6]);
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate0(dest + BpL, c[5]);
            }
            else
            {
                Interpolate2(dest + BpL, c[5], c[8], c[4]);
            }
            Interpolate1(dest + BpL + 4, c[5], c[6]);
            break;
        }
        case 242: {
            Interpolate2(dest, c[5], c[1], c[4]);
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate1(dest + 4, c[5], c[3]);
            }
            else
            {
                Interpolate7(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate1(dest + BpL, c[5], c[4]);
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate0(dest + BpL + 4, c[5]);
            }
            else
            {
                Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 59: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate0(dest, c[5]);
            }
            else
            {
                Interpolate2(dest, c[5], c[4], c[2]);
            }
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate1(dest + 4, c[5], c[3]);
            }
            else
            {
                Interpolate7(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate1(dest + BpL, c[5], c[8]);
            Interpolate2(dest + BpL + 4, c[5], c[9], c[8]);
            break;
        }
        case 121: {
            Interpolate1(dest, c[5], c[2]);
            Interpolate2(dest + 4, c[5], c[3], c[2]);
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate0(dest + BpL, c[5]);
            }
            else
            {
                Interpolate2(dest + BpL, c[5], c[8], c[4]);
            }
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate1(dest + BpL + 4, c[5], c[9]);
            }
            else
            {
                Interpolate7(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 87: {
            Interpolate1(dest, c[5], c[4]);
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate0(dest + 4, c[5]);
            }
            else
            {
                Interpolate2(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate2(dest + BpL, c[5], c[7], c[4]);
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate1(dest + BpL + 4, c[5], c[9]);
            }
            else
            {
                Interpolate7(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 79: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate0(dest, c[5]);
            }
            else
            {
                Interpolate2(dest, c[5], c[4], c[2]);
            }
            Interpolate1(dest + 4, c[5], c[6]);
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate1(dest + BpL, c[5], c[7]);
            }
            else
            {
                Interpolate7(dest + BpL, c[5], c[8], c[4]);
            }
            Interpolate2(dest + BpL + 4, c[5], c[9], c[6]);
            break;
        }
        case 122: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate1(dest, c[5], c[1]);
            }
            else
            {
                Interpolate7(dest, c[5], c[4], c[2]);
            }
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate1(dest + 4, c[5], c[3]);
            }
            else
            {
                Interpolate7(dest + 4, c[5], c[2], c[6]);
            }
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate0(dest + BpL, c[5]);
            }
            else
            {
                Interpolate2(dest + BpL, c[5], c[8], c[4]);
            }
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate1(dest + BpL + 4, c[5], c[9]);
            }
            else
            {
                Interpolate7(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 94: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate1(dest, c[5], c[1]);
            }
            else
            {
                Interpolate7(dest, c[5], c[4], c[2]);
            }
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate0(dest + 4, c[5]);
            }
            else
            {
                Interpolate2(dest + 4, c[5], c[2], c[6]);
            }
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate1(dest + BpL, c[5], c[7]);
            }
            else
            {
                Interpolate7(dest + BpL, c[5], c[8], c[4]);
            }
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate1(dest + BpL + 4, c[5], c[9]);
            }
            else
            {
                Interpolate7(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 218: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate1(dest, c[5], c[1]);
            }
            else
            {
                Interpolate7(dest, c[5], c[4], c[2]);
            }
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate1(dest + 4, c[5], c[3]);
            }
            else
            {
                Interpolate7(dest + 4, c[5], c[2], c[6]);
            }
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate1(dest + BpL, c[5], c[7]);
            }
            else
            {
                Interpolate7(dest + BpL, c[5], c[8], c[4]);
            }
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate0(dest + BpL + 4, c[5]);
            }
            else
            {
                Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 91: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate0(dest, c[5]);
            }
            else
            {
                Interpolate2(dest, c[5], c[4], c[2]);
            }
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate1(dest + 4, c[5], c[3]);
            }
            else
            {
                Interpolate7(dest + 4, c[5], c[2], c[6]);
            }
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate1(dest + BpL, c[5], c[7]);
            }
            else
            {
                Interpolate7(dest + BpL, c[5], c[8], c[4]);
            }
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate1(dest + BpL + 4, c[5], c[9]);
            }
            else
            {
                Interpolate7(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 229: {
            Interpolate2(dest, c[5], c[4], c[2]);
            Interpolate2(dest + 4, c[5], c[2], c[6]);
            Interpolate1(dest + BpL, c[5], c[4]);
            Interpolate1(dest + BpL + 4, c[5], c[6]);
            break;
        }
        case 167: {
            Interpolate1(dest, c[5], c[4]);
            Interpolate1(dest + 4, c[5], c[6]);
            Interpolate2(dest + BpL, c[5], c[8], c[4]);
            Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            break;
        }
        case 173: {
            Interpolate1(dest, c[5], c[2]);
            Interpolate2(dest + 4, c[5], c[2], c[6]);
            Interpolate1(dest + BpL, c[5], c[8]);
            Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            break;
        }
        case 181: {
            Interpolate2(dest, c[5], c[4], c[2]);
            Interpolate1(dest + 4, c[5], c[2]);
            Interpolate2(dest + BpL, c[5], c[8], c[4]);
            Interpolate1(dest + BpL + 4, c[5], c[8]);
            break;
        }
        case 186: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate1(dest, c[5], c[1]);
            }
            else
            {
                Interpolate7(dest, c[5], c[4], c[2]);
            }
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate1(dest + 4, c[5], c[3]);
            }
            else
            {
                Interpolate7(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate1(dest + BpL, c[5], c[8]);
            Interpolate1(dest + BpL + 4, c[5], c[8]);
            break;
        }
        case 115: {
            Interpolate1(dest, c[5], c[4]);
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate1(dest + 4, c[5], c[3]);
            }
            else
            {
                Interpolate7(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate1(dest + BpL, c[5], c[4]);
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate1(dest + BpL + 4, c[5], c[9]);
            }
            else
            {
                Interpolate7(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 93: {
            Interpolate1(dest, c[5], c[2]);
            Interpolate1(dest + 4, c[5], c[2]);
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate1(dest + BpL, c[5], c[7]);
            }
            else
            {
                Interpolate7(dest + BpL, c[5], c[8], c[4]);
            }
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate1(dest + BpL + 4, c[5], c[9]);
            }
            else
            {
                Interpolate7(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 206: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate1(dest, c[5], c[1]);
            }
            else
            {
                Interpolate7(dest, c[5], c[4], c[2]);
            }
            Interpolate1(dest + 4, c[5], c[6]);
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate1(dest + BpL, c[5], c[7]);
            }
            else
            {
                Interpolate7(dest + BpL, c[5], c[8], c[4]);
            }
            Interpolate1(dest + BpL + 4, c[5], c[6]);
            break;
        }
        case 205:
        case 201: {
            Interpolate1(dest, c[5], c[2]);
            Interpolate2(dest + 4, c[5], c[2], c[6]);
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate1(dest + BpL, c[5], c[7]);
            }
            else
            {
                Interpolate7(dest + BpL, c[5], c[8], c[4]);
            }
            Interpolate1(dest + BpL + 4, c[5], c[6]);
            break;
        }
        case 174:
        case 46: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate1(dest, c[5], c[1]);
            }
            else
            {
                Interpolate7(dest, c[5], c[4], c[2]);
            }
            Interpolate1(dest + 4, c[5], c[6]);
            Interpolate1(dest + BpL, c[5], c[8]);
            Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            break;
        }
        case 179:
        case 147: {
            Interpolate1(dest, c[5], c[4]);
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate1(dest + 4, c[5], c[3]);
            }
            else
            {
                Interpolate7(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate2(dest + BpL, c[5], c[8], c[4]);
            Interpolate1(dest + BpL + 4, c[5], c[8]);
            break;
        }
        case 117:
        case 116: {
            Interpolate2(dest, c[5], c[4], c[2]);
            Interpolate1(dest + 4, c[5], c[2]);
            Interpolate1(dest + BpL, c[5], c[4]);
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate1(dest + BpL + 4, c[5], c[9]);
            }
            else
            {
                Interpolate7(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 189: {
            Interpolate1(dest, c[5], c[2]);
            Interpolate1(dest + 4, c[5], c[2]);
            Interpolate1(dest + BpL, c[5], c[8]);
            Interpolate1(dest + BpL + 4, c[5], c[8]);
            break;
        }
        case 231: {
            Interpolate1(dest, c[5], c[4]);
            Interpolate1(dest + 4, c[5], c[6]);
            Interpolate1(dest + BpL, c[5], c[4]);
            Interpolate1(dest + BpL + 4, c[5], c[6]);
            break;
        }
        case 126: {
            Interpolate1(dest, c[5], c[1]);
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate0(dest + 4, c[5]);
            }
            else
            {
                Interpolate2(dest + 4, c[5], c[2], c[6]);
            }
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate0(dest + BpL, c[5]);
            }
            else
            {
                Interpolate2(dest + BpL, c[5], c[8], c[4]);
            }
            Interpolate1(dest + BpL + 4, c[5], c[9]);
            break;
        }
        case 219: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate0(dest, c[5]);
            }
            else
            {
                Interpolate2(dest, c[5], c[4], c[2]);
            }
            Interpolate1(dest + 4, c[5], c[3]);
            Interpolate1(dest + BpL, c[5], c[7]);
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate0(dest + BpL + 4, c[5]);
            }
            else
            {
                Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 125: {
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate1(dest, c[5], c[2]);
                Interpolate0(dest + BpL, c[5]);
            }
            else
            {
                Interpolate6(dest, c[5], c[4], c[2]);
                Interpolate9(dest + BpL, c[5], c[8], c[4]);
            }
            Interpolate1(dest + 4, c[5], c[2]);
            Interpolate1(dest + BpL + 4, c[5], c[9]);
            break;
        }
        case 221: {
            Interpolate1(dest, c[5], c[2]);
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate1(dest + 4, c[5], c[2]);
                Interpolate0(dest + BpL + 4, c[5]);
            }
            else
            {
                Interpolate6(dest + 4, c[5], c[6], c[2]);
                Interpolate9(dest + BpL + 4, c[5], c[6], c[8]);
            }
            Interpolate1(dest + BpL, c[5], c[7]);
            break;
        }
        case 207: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate0(dest, c[5]);
                Interpolate1(dest + 4, c[5], c[6]);
            }
            else
            {
                Interpolate9(dest, c[5], c[4], c[2]);
                Interpolate6(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate1(dest + BpL, c[5], c[7]);
            Interpolate1(dest + BpL + 4, c[5], c[6]);
            break;
        }
        case 238: {
            Interpolate1(dest, c[5], c[1]);
            Interpolate1(dest + 4, c[5], c[6]);
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate0(dest + BpL, c[5]);
                Interpolate1(dest + BpL + 4, c[5], c[6]);
            }
            else
            {
                Interpolate9(dest + BpL, c[5], c[8], c[4]);
                Interpolate6(dest + BpL + 4, c[5], c[8], c[6]);
            }
            break;
        }
        case 190: {
            Interpolate1(dest, c[5], c[1]);
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate0(dest + 4, c[5]);
                Interpolate1(dest + BpL + 4, c[5], c[8]);
            }
            else
            {
                Interpolate9(dest + 4, c[5], c[2], c[6]);
                Interpolate6(dest + BpL + 4, c[5], c[6], c[8]);
            }
            Interpolate1(dest + BpL, c[5], c[8]);
            break;
        }
        case 187: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate0(dest, c[5]);
                Interpolate1(dest + BpL, c[5], c[8]);
            }
            else
            {
                Interpolate9(dest, c[5], c[4], c[2]);
                Interpolate6(dest + BpL, c[5], c[4], c[8]);
            }
            Interpolate1(dest + 4, c[5], c[3]);
            Interpolate1(dest + BpL + 4, c[5], c[8]);
            break;
        }
        case 243: {
            Interpolate1(dest, c[5], c[4]);
            Interpolate1(dest + 4, c[5], c[3]);
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate1(dest + BpL, c[5], c[4]);
                Interpolate0(dest + BpL + 4, c[5]);
            }
            else
            {
                Interpolate6(dest + BpL, c[5], c[8], c[4]);
                Interpolate9(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 119: {
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate1(dest, c[5], c[4]);
                Interpolate0(dest + 4, c[5]);
            }
            else
            {
                Interpolate6(dest, c[5], c[2], c[4]);
                Interpolate9(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate1(dest + BpL, c[5], c[4]);
            Interpolate1(dest + BpL + 4, c[5], c[9]);
            break;
        }
        case 237:
        case 233: {
            Interpolate1(dest, c[5], c[2]);
            Interpolate2(dest + 4, c[5], c[2], c[6]);
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate0(dest + BpL, c[5]);
            }
            else
            {
                Interpolate10(dest + BpL, c[5], c[8], c[4]);
            }
            Interpolate1(dest + BpL + 4, c[5], c[6]);
            break;
        }
        case 175:
        case 47: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate0(dest, c[5]);
            }
            else
            {
                Interpolate10(dest, c[5], c[4], c[2]);
            }
            Interpolate1(dest + 4, c[5], c[6]);
            Interpolate1(dest + BpL, c[5], c[8]);
            Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            break;
        }
        case 183:
        case 151: {
            Interpolate1(dest, c[5], c[4]);
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate0(dest + 4, c[5]);
            }
            else
            {
                Interpolate10(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate2(dest + BpL, c[5], c[8], c[4]);
            Interpolate1(dest + BpL + 4, c[5], c[8]);
            break;
        }
        case 245:
        case 244: {
            Interpolate2(dest, c[5], c[4], c[2]);
            Interpolate1(dest + 4, c[5], c[2]);
            Interpolate1(dest + BpL, c[5], c[4]);
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate0(dest + BpL + 4, c[5]);
            }
            else
            {
                Interpolate10(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 250: {
            Interpolate1(dest, c[5], c[1]);
            Interpolate1(dest + 4, c[5], c[3]);
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate0(dest + BpL, c[5]);
            }
            else
            {
                Interpolate2(dest + BpL, c[5], c[8], c[4]);
            }
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate0(dest + BpL + 4, c[5]);
            }
            else
            {
                Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 123: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate0(dest, c[5]);
            }
            else
            {
                Interpolate2(dest, c[5], c[4], c[2]);
            }
            Interpolate1(dest + 4, c[5], c[3]);
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate0(dest + BpL, c[5]);
            }
            else
            {
                Interpolate2(dest + BpL, c[5], c[8], c[4]);
            }
            Interpolate1(dest + BpL + 4, c[5], c[9]);
            break;
        }
        case 95: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate0(dest, c[5]);
            }
            else
            {
                Interpolate2(dest, c[5], c[4], c[2]);
            }
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate0(dest + 4, c[5]);
            }
            else
            {
                Interpolate2(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate1(dest + BpL, c[5], c[7]);
            Interpolate1(dest + BpL + 4, c[5], c[9]);
            break;
        }
        case 222: {
            Interpolate1(dest, c[5], c[1]);
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate0(dest + 4, c[5]);
            }
            else
            {
                Interpolate2(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate1(dest + BpL, c[5], c[7]);
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate0(dest + BpL + 4, c[5]);
            }
            else
            {
                Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 252: {
            Interpolate2(dest, c[5], c[1], c[2]);
            Interpolate1(dest + 4, c[5], c[2]);
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate0(dest + BpL, c[5]);
            }
            else
            {
                Interpolate2(dest + BpL, c[5], c[8], c[4]);
            }
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate0(dest + BpL + 4, c[5]);
            }
            else
            {
                Interpolate10(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 249: {
            Interpolate1(dest, c[5], c[2]);
            Interpolate2(dest + 4, c[5], c[3], c[2]);
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate0(dest + BpL, c[5]);
            }
            else
            {
                Interpolate10(dest + BpL, c[5], c[8], c[4]);
            }
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate0(dest + BpL + 4, c[5]);
            }
            else
            {
                Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 235: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate0(dest, c[5]);
            }
            else
            {
                Interpolate2(dest, c[5], c[4], c[2]);
            }
            Interpolate2(dest + 4, c[5], c[3], c[6]);
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate0(dest + BpL, c[5]);
            }
            else
            {
                Interpolate10(dest + BpL, c[5], c[8], c[4]);
            }
            Interpolate1(dest + BpL + 4, c[5], c[6]);
            break;
        }
        case 111: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate0(dest, c[5]);
            }
            else
            {
                Interpolate10(dest, c[5], c[4], c[2]);
            }
            Interpolate1(dest + 4, c[5], c[6]);
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate0(dest + BpL, c[5]);
            }
            else
            {
                Interpolate2(dest + BpL, c[5], c[8], c[4]);
            }
            Interpolate2(dest + BpL + 4, c[5], c[9], c[6]);
            break;
        }
        case 63: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate0(dest, c[5]);
            }
            else
            {
                Interpolate10(dest, c[5], c[4], c[2]);
            }
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate0(dest + 4, c[5]);
            }
            else
            {
                Interpolate2(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate1(dest + BpL, c[5], c[8]);
            Interpolate2(dest + BpL + 4, c[5], c[9], c[8]);
            break;
        }
        case 159: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate0(dest, c[5]);
            }
            else
            {
                Interpolate2(dest, c[5], c[4], c[2]);
            }
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate0(dest + 4, c[5]);
            }
            else
            {
                Interpolate10(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate2(dest + BpL, c[5], c[7], c[8]);
            Interpolate1(dest + BpL + 4, c[5], c[8]);
            break;
        }
        case 215: {
            Interpolate1(dest, c[5], c[4]);
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate0(dest + 4, c[5]);
            }
            else
            {
                Interpolate10(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate2(dest + BpL, c[5], c[7], c[4]);
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate0(dest + BpL + 4, c[5]);
            }
            else
            {
                Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 246: {
            Interpolate2(dest, c[5], c[1], c[4]);
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate0(dest + 4, c[5]);
            }
            else
            {
                Interpolate2(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate1(dest + BpL, c[5], c[4]);
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate0(dest + BpL + 4, c[5]);
            }
            else
            {
                Interpolate10(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 254: {
            Interpolate1(dest, c[5], c[1]);
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate0(dest + 4, c[5]);
            }
            else
            {
                Interpolate2(dest + 4, c[5], c[2], c[6]);
            }
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate0(dest + BpL, c[5]);
            }
            else
            {
                Interpolate2(dest + BpL, c[5], c[8], c[4]);
            }
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate0(dest + BpL + 4, c[5]);
            }
            else
            {
                Interpolate10(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 253: {
            Interpolate1(dest, c[5], c[2]);
            Interpolate1(dest + 4, c[5], c[2]);
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate0(dest + BpL, c[5]);
            }
            else
            {
                Interpolate10(dest + BpL, c[5], c[8], c[4]);
            }
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate0(dest + BpL + 4, c[5]);
            }
            else
            {
                Interpolate10(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 251: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate0(dest, c[5]);
            }
            else
            {
                Interpolate2(dest, c[5], c[4], c[2]);
            }
            Interpolate1(dest + 4, c[5], c[3]);
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate0(dest + BpL, c[5]);
            }
            else
            {
                Interpolate10(dest + BpL, c[5], c[8], c[4]);
            }
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate0(dest + BpL + 4, c[5]);
            }
            else
            {
                Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 239: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate0(dest, c[5]);
            }
            else
            {
                Interpolate10(dest, c[5], c[4], c[2]);
            }
            Interpolate1(dest + 4, c[5], c[6]);
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate0(dest + BpL, c[5]);
            }
            else
            {
                Interpolate10(dest + BpL, c[5], c[8], c[4]);
            }
            Interpolate1(dest + BpL + 4, c[5], c[6]);
            break;
        }
        case 127: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate0(dest, c[5]);
            }
            else
            {
                Interpolate10(dest, c[5], c[4], c[2]);
            }
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate0(dest + 4, c[5]);
            }
            else
            {
                Interpolate2(dest + 4, c[5], c[2], c[6]);
            }
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate0(dest + BpL, c[5]);
            }
            else
            {
                Interpolate2(dest + BpL, c[5], c[8], c[4]);
            }
            Interpolate1(dest + BpL + 4, c[5], c[9]);
            break;
        }
        case 191: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate0(dest, c[5]);
            }
            else
            {
                Interpolate10(dest, c[5], c[4], c[2]);
            }
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate0(dest + 4, c[5]);
            }
            else
            {
                Interpolate10(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate1(dest + BpL, c[5], c[8]);
            Interpolate1(dest + BpL + 4, c[5], c[8]);
            break;
        }
        case 223: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate0(dest, c[5]);
            }
            else
            {
                Interpolate2(dest, c[5], c[4], c[2]);
            }
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate0(dest + 4, c[5]);
            }
            else
            {
                Interpolate10(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate1(dest + BpL, c[5], c[7]);
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate0(dest + BpL + 4, c[5]);
            }
            else
            {
                Interpolate2(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 247: {
            Interpolate1(dest, c[5], c[4]);
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate0(dest + 4, c[5]);
            }
            else
            {
                Interpolate10(dest + 4, c[5], c[2], c[6]);
            }
            Interpolate1(dest + BpL, c[5], c[4]);
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate0(dest + BpL + 4, c[5]);
            }
            else
            {
                Interpolate10(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        case 255: {
            if (YuvDiff(p[4], p[2]))
            {
                Interpolate0(dest, c[5]);
            }
            else
            {
                Interpolate10(dest, c[5], c[4], c[2]);
            }
            if (YuvDiff(p[2], p[6]))
            {
                Interpolate0(dest + 4, c[5]);
            }
            else
            {
                Interpolate10(dest + 4, c[5], c[2], c[6]);
            }
            if (YuvDiff(p[8], p[4]))
            {
                Interpolate0(dest + BpL, c[5]);
            }
            else
            {
                Interpolate10(dest + BpL, c[5], c[8], c[4]);
            }
            if (YuvDiff(p[6], p[8]))
            {
                Interpolate0(dest + BpL + 4, c[5]);
            }
            else
            {
                Interpolate10(dest + BpL + 4, c[5], c[6], c[8]);
            }
            break;
        }
        } // switch (pattern)
    }     // for (x)
}

void StripAlpha(uint8_t *dest, const uint8_t *src, int width)
{
    // we don't care about transparent pixels here (on the assumption
    // that the original image didn't have any).

    const uint8_t *src_end = src + width * 8;

    for (; src < src_end; src += 4, dest += 3)
    {
        dest[0] = src[0];
        dest[1] = src[1];
        dest[2] = src[2];
    }
}

ImageData *ImageHq2x(ImageData *image, bool solid, bool invert)
{
    int w = image->width_;
    int h = image->height_;

    ImageData *result = new ImageData(w * 2, h * 2, solid ? 3 : 4);

    // for solid mode, we must strip off the alpha channel
    uint8_t *temp_buffer = nullptr;

    if (solid)
        temp_buffer = new uint8_t[w * 16]; // two lines worth

    for (int y = 0; y < h; y++)
    {
        int dst_y = invert ? (h - 1 - y) : y;

        uint8_t *out_buf = solid ? temp_buffer : result->PixelAt(0, dst_y * 2);

        ConvertLine(y, w, h, invert, out_buf, image->PixelAt(0, y));

        if (solid)
            StripAlpha(result->PixelAt(0, dst_y * 2), temp_buffer, w * 2);
    }

    if (temp_buffer)
        delete[] temp_buffer;

    return result;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
