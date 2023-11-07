//------------------------------------------------------------------------
//  HQ2X : High-Quality 2x Graphics Resizing
//----------------------------------------------------------------------------
//
//  Copyright (c) 2007-2023  The EDGE Team.
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
//  Based heavily on the code (C) 2003 Maxim Stepin, which is
//  under the GNU LGPL (Lesser General Public License).
//
//  For more information, see: http://hiend3d.com/hq2x.html
//
//------------------------------------------------------------------------
//
//  -AJA- Modified to use a palettised input image, allowing the
//        YUV table to be used with full precision (because the
//        intermediate conversion to 16-bit RGB isn't needed).
//
//        Changed the YUV calculation to something more standard.
//        The LerpColor method and support for transparent pixels
//        was copied from the Doomsday engine by Jaakko Ker�nen.
//

#include "epi.h"
#include "image_hq2x.h"

namespace epi
{
namespace Hq2x
{

u32_t PixelRGB[256];
u32_t PixelYUV[256];

const u32_t Amask = 0xFF000000;
const u32_t Ymask = 0x00FF0000;
const u32_t Umask = 0x0000FF00;
const u32_t Vmask = 0x000000FF;

const u32_t trY = 0x00300000;
const u32_t trU = 0x00000700;
const u32_t trV = 0x00000007; // -AJA- changed (was 6)

inline u32_t GET_R(u32_t col)
{
    return (col >> 16) & 0xFF;
}
inline u32_t GET_G(u32_t col)
{
    return (col >> 8) & 0xFF;
}
inline u32_t GET_B(u32_t col)
{
    return (col) & 0xFF;
}
inline u32_t GET_A(u32_t col)
{
    return (col >> 24) & 0xFF;
}

inline void LerpColor(u8_t *dest, u32_t c1, u32_t c2, u32_t c3, u32_t f1, u32_t f2, u32_t f3, u32_t shift)
{
    dest[0] = (GET_R(c1) * f1 + GET_R(c2) * f2 + GET_R(c3) * f3) >> shift;
    dest[1] = (GET_G(c1) * f1 + GET_G(c2) * f2 + GET_G(c3) * f3) >> shift;
    dest[2] = (GET_B(c1) * f1 + GET_B(c2) * f2 + GET_B(c3) * f3) >> shift;
    dest[3] = (GET_A(c1) * f1 + GET_A(c2) * f2 + GET_A(c3) * f3) >> shift;
}

void Interp0(u8_t *dest, u32_t c1)
{
    // *dest = c1
    dest[0] = GET_R(c1);
    dest[1] = GET_G(c1);
    dest[2] = GET_B(c1);
    dest[3] = GET_A(c1);
}

void Interp1(u8_t *dest, u32_t c1, u32_t c2)
{
    // *dest = (c1*3+c2) >> 2;
    LerpColor(dest, c1, c2, 0, 3, 1, 0, 2);
}

void Interp2(u8_t *dest, u32_t c1, u32_t c2, u32_t c3)
{
    // *dest = (c1*2+c2+c3) >> 2;
    LerpColor(dest, c1, c2, c3, 2, 1, 1, 2);
}

void Interp5(u8_t *dest, u32_t c1, u32_t c2)
{
    // *dest = (c1+c2) >> 1;
    LerpColor(dest, c1, c2, 0, 1, 1, 0, 1);
}

void Interp6(u8_t *dest, u32_t c1, u32_t c2, u32_t c3)
{
    // *dest = (c1*5+c2*2+c3)/8;
    LerpColor(dest, c1, c2, c3, 5, 2, 1, 3);
}

void Interp7(u8_t *dest, u32_t c1, u32_t c2, u32_t c3)
{
    // *dest = (c1*6+c2+c3)/8;
    LerpColor(dest, c1, c2, c3, 6, 1, 1, 3);
}

void Interp9(u8_t *dest, u32_t c1, u32_t c2, u32_t c3)
{
    // *dest = (c1*2+(c2+c3)*3)/8;
    LerpColor(dest, c1, c2, c3, 2, 3, 3, 3);
}

void Interp10(u8_t *dest, u32_t c1, u32_t c2, u32_t c3)
{
    // *dest = (c1*14+c2+c3)/16;
    LerpColor(dest, c1, c2, c3, 14, 1, 1, 4);
}

#define PIXEL00_0   Interp0(dest, c[5]);
#define PIXEL00_10  Interp1(dest, c[5], c[1]);
#define PIXEL00_11  Interp1(dest, c[5], c[4]);
#define PIXEL00_12  Interp1(dest, c[5], c[2]);
#define PIXEL00_20  Interp2(dest, c[5], c[4], c[2]);
#define PIXEL00_21  Interp2(dest, c[5], c[1], c[2]);
#define PIXEL00_22  Interp2(dest, c[5], c[1], c[4]);
#define PIXEL00_60  Interp6(dest, c[5], c[2], c[4]);
#define PIXEL00_61  Interp6(dest, c[5], c[4], c[2]);
#define PIXEL00_70  Interp7(dest, c[5], c[4], c[2]);
#define PIXEL00_90  Interp9(dest, c[5], c[4], c[2]);
#define PIXEL00_100 Interp10(dest, c[5], c[4], c[2]);

#define PIXEL01_0   Interp0(dest + 4, c[5]);
#define PIXEL01_10  Interp1(dest + 4, c[5], c[3]);
#define PIXEL01_11  Interp1(dest + 4, c[5], c[2]);
#define PIXEL01_12  Interp1(dest + 4, c[5], c[6]);
#define PIXEL01_20  Interp2(dest + 4, c[5], c[2], c[6]);
#define PIXEL01_21  Interp2(dest + 4, c[5], c[3], c[6]);
#define PIXEL01_22  Interp2(dest + 4, c[5], c[3], c[2]);
#define PIXEL01_60  Interp6(dest + 4, c[5], c[6], c[2]);
#define PIXEL01_61  Interp6(dest + 4, c[5], c[2], c[6]);
#define PIXEL01_70  Interp7(dest + 4, c[5], c[2], c[6]);
#define PIXEL01_90  Interp9(dest + 4, c[5], c[2], c[6]);
#define PIXEL01_100 Interp10(dest + 4, c[5], c[2], c[6]);

#define PIXEL10_0   Interp0(dest + BpL, c[5]);
#define PIXEL10_10  Interp1(dest + BpL, c[5], c[7]);
#define PIXEL10_11  Interp1(dest + BpL, c[5], c[8]);
#define PIXEL10_12  Interp1(dest + BpL, c[5], c[4]);
#define PIXEL10_20  Interp2(dest + BpL, c[5], c[8], c[4]);
#define PIXEL10_21  Interp2(dest + BpL, c[5], c[7], c[4]);
#define PIXEL10_22  Interp2(dest + BpL, c[5], c[7], c[8]);
#define PIXEL10_60  Interp6(dest + BpL, c[5], c[4], c[8]);
#define PIXEL10_61  Interp6(dest + BpL, c[5], c[8], c[4]);
#define PIXEL10_70  Interp7(dest + BpL, c[5], c[8], c[4]);
#define PIXEL10_90  Interp9(dest + BpL, c[5], c[8], c[4]);
#define PIXEL10_100 Interp10(dest + BpL, c[5], c[8], c[4]);

#define PIXEL11_0   Interp0(dest + BpL + 4, c[5]);
#define PIXEL11_10  Interp1(dest + BpL + 4, c[5], c[9]);
#define PIXEL11_11  Interp1(dest + BpL + 4, c[5], c[6]);
#define PIXEL11_12  Interp1(dest + BpL + 4, c[5], c[8]);
#define PIXEL11_20  Interp2(dest + BpL + 4, c[5], c[6], c[8]);
#define PIXEL11_21  Interp2(dest + BpL + 4, c[5], c[9], c[8]);
#define PIXEL11_22  Interp2(dest + BpL + 4, c[5], c[9], c[6]);
#define PIXEL11_60  Interp6(dest + BpL + 4, c[5], c[8], c[6]);
#define PIXEL11_61  Interp6(dest + BpL + 4, c[5], c[6], c[8]);
#define PIXEL11_70  Interp7(dest + BpL + 4, c[5], c[6], c[8]);
#define PIXEL11_90  Interp9(dest + BpL + 4, c[5], c[6], c[8]);
#define PIXEL11_100 Interp10(dest + BpL + 4, c[5], c[6], c[8]);

inline bool Diff(const u8_t p1, const u8_t p2)
{
    u32_t YUV1 = PixelYUV[p1];
    u32_t YUV2 = PixelYUV[p2];

    return (YUV1 & Amask) != (YUV2 & Amask) || std::abs(static_cast<int>((YUV1 & Ymask) - (YUV2 & Ymask))) > trY ||
           std::abs(static_cast<int>((YUV1 & Umask) - (YUV2 & Umask))) > trU ||
           std::abs(static_cast<int>((YUV1 & Vmask) - (YUV2 & Vmask))) > trV;
}

void Setup(const u8_t *palette, int trans_pixel)
{
    for (int c = 0; c < 256; c++)
    {
        int r = palette[c * 3 + 0];
        int g = palette[c * 3 + 1];
        int b = palette[c * 3 + 2];
        int A = 255;

        if (c == trans_pixel)
            r = g = b = A = 0;

        PixelRGB[c] = ((A << 24) + (r << 16) + (g << 8) + b);

        // -AJA- changed to better formulas (based on Wikipedia article)
        int Y = (r * 77 + g * 150 + b * 29) >> 8;
        int u = 128 + ((-r * 38 - g * 74 + b * 111) >> 9);
        int v = 128 + ((r * 157 - g * 132 - b * 26) >> 9);

#if 0 // DEBUGGING
		fprintf(stderr, "[%d] #%02x%02x%02x -> YUV #%02x%02x%02x\n", c, r,g,b, Y,u,v);
#endif
        PixelYUV[c] = ((A << 24) + (Y << 16) + (u << 8) + v);
    }
}

void ConvertLine(int y, int w, int h, bool invert, u8_t *dest, const u8_t *src)
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

    u8_t  p[10]; // palette pixels
    u32_t c[10]; // RGBA pixels

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
            c[k] = PixelRGB[p[k]];

        u8_t pattern = 0;

        if (Diff(p[5], p[1]))
            pattern |= 0x01;
        if (Diff(p[5], p[2]))
            pattern |= 0x02;
        if (Diff(p[5], p[3]))
            pattern |= 0x04;
        if (Diff(p[5], p[4]))
            pattern |= 0x08;
        if (Diff(p[5], p[6]))
            pattern |= 0x10;
        if (Diff(p[5], p[7]))
            pattern |= 0x20;
        if (Diff(p[5], p[8]))
            pattern |= 0x40;
        if (Diff(p[5], p[9]))
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
            PIXEL00_20
            PIXEL01_20
            PIXEL10_20
            PIXEL11_20
            break;
        }
        case 2:
        case 34:
        case 130:
        case 162: {
            PIXEL00_22
            PIXEL01_21
            PIXEL10_20
            PIXEL11_20
            break;
        }
        case 16:
        case 17:
        case 48:
        case 49: {
            PIXEL00_20
            PIXEL01_22
            PIXEL10_20
            PIXEL11_21
            break;
        }
        case 64:
        case 65:
        case 68:
        case 69: {
            PIXEL00_20
            PIXEL01_20
            PIXEL10_21
            PIXEL11_22
            break;
        }
        case 8:
        case 12:
        case 136:
        case 140: {
            PIXEL00_21
            PIXEL01_20
            PIXEL10_22
            PIXEL11_20
            break;
        }
        case 3:
        case 35:
        case 131:
        case 163: {
            PIXEL00_11
            PIXEL01_21
            PIXEL10_20
            PIXEL11_20
            break;
        }
        case 6:
        case 38:
        case 134:
        case 166: {
            PIXEL00_22
            PIXEL01_12
            PIXEL10_20
            PIXEL11_20
            break;
        }
        case 20:
        case 21:
        case 52:
        case 53: {
            PIXEL00_20
            PIXEL01_11
            PIXEL10_20
            PIXEL11_21
            break;
        }
        case 144:
        case 145:
        case 176:
        case 177: {
            PIXEL00_20
            PIXEL01_22
            PIXEL10_20
            PIXEL11_12
            break;
        }
        case 192:
        case 193:
        case 196:
        case 197: {
            PIXEL00_20
            PIXEL01_20
            PIXEL10_21
            PIXEL11_11
            break;
        }
        case 96:
        case 97:
        case 100:
        case 101: {
            PIXEL00_20
            PIXEL01_20
            PIXEL10_12
            PIXEL11_22
            break;
        }
        case 40:
        case 44:
        case 168:
        case 172: {
            PIXEL00_21
            PIXEL01_20
            PIXEL10_11
            PIXEL11_20
            break;
        }
        case 9:
        case 13:
        case 137:
        case 141: {
            PIXEL00_12
            PIXEL01_20
            PIXEL10_22
            PIXEL11_20
            break;
        }
        case 18:
        case 50: {
            PIXEL00_22
            if (Diff(p[2], p[6]))
            {
                PIXEL01_10
            }
            else
            {
                PIXEL01_20
            }
            PIXEL10_20
            PIXEL11_21
            break;
        }
        case 80:
        case 81: {
            PIXEL00_20
            PIXEL01_22
            PIXEL10_21
            if (Diff(p[6], p[8]))
            {
                PIXEL11_10
            }
            else
            {
                PIXEL11_20
            }
            break;
        }
        case 72:
        case 76: {
            PIXEL00_21
            PIXEL01_20
            if (Diff(p[8], p[4]))
            {
                PIXEL10_10
            }
            else
            {
                PIXEL10_20
            }
            PIXEL11_22
            break;
        }
        case 10:
        case 138: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_10
            }
            else
            {
                PIXEL00_20
            }
            PIXEL01_21
            PIXEL10_22
            PIXEL11_20
            break;
        }
        case 66: {
            PIXEL00_22
            PIXEL01_21
            PIXEL10_21
            PIXEL11_22
            break;
        }
        case 24: {
            PIXEL00_21
            PIXEL01_22
            PIXEL10_22
            PIXEL11_21
            break;
        }
        case 7:
        case 39:
        case 135: {
            PIXEL00_11
            PIXEL01_12
            PIXEL10_20
            PIXEL11_20
            break;
        }
        case 148:
        case 149:
        case 180: {
            PIXEL00_20
            PIXEL01_11
            PIXEL10_20
            PIXEL11_12
            break;
        }
        case 224:
        case 228:
        case 225: {
            PIXEL00_20
            PIXEL01_20
            PIXEL10_12
            PIXEL11_11
            break;
        }
        case 41:
        case 169:
        case 45: {
            PIXEL00_12
            PIXEL01_20
            PIXEL10_11
            PIXEL11_20
            break;
        }
        case 22:
        case 54: {
            PIXEL00_22
            if (Diff(p[2], p[6]))
            {
                PIXEL01_0
            }
            else
            {
                PIXEL01_20
            }
            PIXEL10_20
            PIXEL11_21
            break;
        }
        case 208:
        case 209: {
            PIXEL00_20
            PIXEL01_22
            PIXEL10_21
            if (Diff(p[6], p[8]))
            {
                PIXEL11_0
            }
            else
            {
                PIXEL11_20
            }
            break;
        }
        case 104:
        case 108: {
            PIXEL00_21
            PIXEL01_20
            if (Diff(p[8], p[4]))
            {
                PIXEL10_0
            }
            else
            {
                PIXEL10_20
            }
            PIXEL11_22
            break;
        }
        case 11:
        case 139: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_0
            }
            else
            {
                PIXEL00_20
            }
            PIXEL01_21
            PIXEL10_22
            PIXEL11_20
            break;
        }
        case 19:
        case 51: {
            if (Diff(p[2], p[6]))
            {
                PIXEL00_11
                PIXEL01_10
            }
            else
            {
                PIXEL00_60
                PIXEL01_90
            }
            PIXEL10_20
            PIXEL11_21
            break;
        }
        case 146:
        case 178: {
            PIXEL00_22
            if (Diff(p[2], p[6]))
            {
                PIXEL01_10
                PIXEL11_12
            }
            else
            {
                PIXEL01_90
                PIXEL11_61
            }
            PIXEL10_20
            break;
        }
        case 84:
        case 85: {
            PIXEL00_20
            if (Diff(p[6], p[8]))
            {
                PIXEL01_11
                PIXEL11_10
            }
            else
            {
                PIXEL01_60
                PIXEL11_90
            }
            PIXEL10_21
            break;
        }
        case 112:
        case 113: {
            PIXEL00_20
            PIXEL01_22
            if (Diff(p[6], p[8]))
            {
                PIXEL10_12
                PIXEL11_10
            }
            else
            {
                PIXEL10_61
                PIXEL11_90
            }
            break;
        }
        case 200:
        case 204: {
            PIXEL00_21
            PIXEL01_20
            if (Diff(p[8], p[4]))
            {
                PIXEL10_10
                PIXEL11_11
            }
            else
            {
                PIXEL10_90
                PIXEL11_60
            }
            break;
        }
        case 73:
        case 77: {
            if (Diff(p[8], p[4]))
            {
                PIXEL00_12
                PIXEL10_10
            }
            else
            {
                PIXEL00_61
                PIXEL10_90
            }
            PIXEL01_20
            PIXEL11_22
            break;
        }
        case 42:
        case 170: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_10
                PIXEL10_11
            }
            else
            {
                PIXEL00_90
                PIXEL10_60
            }
            PIXEL01_21
            PIXEL11_20
            break;
        }
        case 14:
        case 142: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_10
                PIXEL01_12
            }
            else
            {
                PIXEL00_90
                PIXEL01_61
            }
            PIXEL10_22
            PIXEL11_20
            break;
        }
        case 67: {
            PIXEL00_11
            PIXEL01_21
            PIXEL10_21
            PIXEL11_22
            break;
        }
        case 70: {
            PIXEL00_22
            PIXEL01_12
            PIXEL10_21
            PIXEL11_22
            break;
        }
        case 28: {
            PIXEL00_21
            PIXEL01_11
            PIXEL10_22
            PIXEL11_21
            break;
        }
        case 152: {
            PIXEL00_21
            PIXEL01_22
            PIXEL10_22
            PIXEL11_12
            break;
        }
        case 194: {
            PIXEL00_22
            PIXEL01_21
            PIXEL10_21
            PIXEL11_11
            break;
        }
        case 98: {
            PIXEL00_22
            PIXEL01_21
            PIXEL10_12
            PIXEL11_22
            break;
        }
        case 56: {
            PIXEL00_21
            PIXEL01_22
            PIXEL10_11
            PIXEL11_21
            break;
        }
        case 25: {
            PIXEL00_12
            PIXEL01_22
            PIXEL10_22
            PIXEL11_21
            break;
        }
        case 26:
        case 31: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_0
            }
            else
            {
                PIXEL00_20
            }
            if (Diff(p[2], p[6]))
            {
                PIXEL01_0
            }
            else
            {
                PIXEL01_20
            }
            PIXEL10_22
            PIXEL11_21
            break;
        }
        case 82:
        case 214: {
            PIXEL00_22
            if (Diff(p[2], p[6]))
            {
                PIXEL01_0
            }
            else
            {
                PIXEL01_20
            }
            PIXEL10_21
            if (Diff(p[6], p[8]))
            {
                PIXEL11_0
            }
            else
            {
                PIXEL11_20
            }
            break;
        }
        case 88:
        case 248: {
            PIXEL00_21
            PIXEL01_22
            if (Diff(p[8], p[4]))
            {
                PIXEL10_0
            }
            else
            {
                PIXEL10_20
            }
            if (Diff(p[6], p[8]))
            {
                PIXEL11_0
            }
            else
            {
                PIXEL11_20
            }
            break;
        }
        case 74:
        case 107: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_0
            }
            else
            {
                PIXEL00_20
            }
            PIXEL01_21
            if (Diff(p[8], p[4]))
            {
                PIXEL10_0
            }
            else
            {
                PIXEL10_20
            }
            PIXEL11_22
            break;
        }
        case 27: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_0
            }
            else
            {
                PIXEL00_20
            }
            PIXEL01_10
            PIXEL10_22
            PIXEL11_21
            break;
        }
        case 86: {
            PIXEL00_22
            if (Diff(p[2], p[6]))
            {
                PIXEL01_0
            }
            else
            {
                PIXEL01_20
            }
            PIXEL10_21
            PIXEL11_10
            break;
        }
        case 216: {
            PIXEL00_21
            PIXEL01_22
            PIXEL10_10
            if (Diff(p[6], p[8]))
            {
                PIXEL11_0
            }
            else
            {
                PIXEL11_20
            }
            break;
        }
        case 106: {
            PIXEL00_10
            PIXEL01_21
            if (Diff(p[8], p[4]))
            {
                PIXEL10_0
            }
            else
            {
                PIXEL10_20
            }
            PIXEL11_22
            break;
        }
        case 30: {
            PIXEL00_10
            if (Diff(p[2], p[6]))
            {
                PIXEL01_0
            }
            else
            {
                PIXEL01_20
            }
            PIXEL10_22
            PIXEL11_21
            break;
        }
        case 210: {
            PIXEL00_22
            PIXEL01_10
            PIXEL10_21
            if (Diff(p[6], p[8]))
            {
                PIXEL11_0
            }
            else
            {
                PIXEL11_20
            }
            break;
        }
        case 120: {
            PIXEL00_21
            PIXEL01_22
            if (Diff(p[8], p[4]))
            {
                PIXEL10_0
            }
            else
            {
                PIXEL10_20
            }
            PIXEL11_10
            break;
        }
        case 75: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_0
            }
            else
            {
                PIXEL00_20
            }
            PIXEL01_21
            PIXEL10_10
            PIXEL11_22
            break;
        }
        case 29: {
            PIXEL00_12
            PIXEL01_11
            PIXEL10_22
            PIXEL11_21
            break;
        }
        case 198: {
            PIXEL00_22
            PIXEL01_12
            PIXEL10_21
            PIXEL11_11
            break;
        }
        case 184: {
            PIXEL00_21
            PIXEL01_22
            PIXEL10_11
            PIXEL11_12
            break;
        }
        case 99: {
            PIXEL00_11
            PIXEL01_21
            PIXEL10_12
            PIXEL11_22
            break;
        }
        case 57: {
            PIXEL00_12
            PIXEL01_22
            PIXEL10_11
            PIXEL11_21
            break;
        }
        case 71: {
            PIXEL00_11
            PIXEL01_12
            PIXEL10_21
            PIXEL11_22
            break;
        }
        case 156: {
            PIXEL00_21
            PIXEL01_11
            PIXEL10_22
            PIXEL11_12
            break;
        }
        case 226: {
            PIXEL00_22
            PIXEL01_21
            PIXEL10_12
            PIXEL11_11
            break;
        }
        case 60: {
            PIXEL00_21
            PIXEL01_11
            PIXEL10_11
            PIXEL11_21
            break;
        }
        case 195: {
            PIXEL00_11
            PIXEL01_21
            PIXEL10_21
            PIXEL11_11
            break;
        }
        case 102: {
            PIXEL00_22
            PIXEL01_12
            PIXEL10_12
            PIXEL11_22
            break;
        }
        case 153: {
            PIXEL00_12
            PIXEL01_22
            PIXEL10_22
            PIXEL11_12
            break;
        }
        case 58: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_10
            }
            else
            {
                PIXEL00_70
            }
            if (Diff(p[2], p[6]))
            {
                PIXEL01_10
            }
            else
            {
                PIXEL01_70
            }
            PIXEL10_11
            PIXEL11_21
            break;
        }
        case 83: {
            PIXEL00_11
            if (Diff(p[2], p[6]))
            {
                PIXEL01_10
            }
            else
            {
                PIXEL01_70
            }
            PIXEL10_21
            if (Diff(p[6], p[8]))
            {
                PIXEL11_10
            }
            else
            {
                PIXEL11_70
            }
            break;
        }
        case 92: {
            PIXEL00_21
            PIXEL01_11
            if (Diff(p[8], p[4]))
            {
                PIXEL10_10
            }
            else
            {
                PIXEL10_70
            }
            if (Diff(p[6], p[8]))
            {
                PIXEL11_10
            }
            else
            {
                PIXEL11_70
            }
            break;
        }
        case 202: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_10
            }
            else
            {
                PIXEL00_70
            }
            PIXEL01_21
            if (Diff(p[8], p[4]))
            {
                PIXEL10_10
            }
            else
            {
                PIXEL10_70
            }
            PIXEL11_11
            break;
        }
        case 78: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_10
            }
            else
            {
                PIXEL00_70
            }
            PIXEL01_12
            if (Diff(p[8], p[4]))
            {
                PIXEL10_10
            }
            else
            {
                PIXEL10_70
            }
            PIXEL11_22
            break;
        }
        case 154: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_10
            }
            else
            {
                PIXEL00_70
            }
            if (Diff(p[2], p[6]))
            {
                PIXEL01_10
            }
            else
            {
                PIXEL01_70
            }
            PIXEL10_22
            PIXEL11_12
            break;
        }
        case 114: {
            PIXEL00_22
            if (Diff(p[2], p[6]))
            {
                PIXEL01_10
            }
            else
            {
                PIXEL01_70
            }
            PIXEL10_12
            if (Diff(p[6], p[8]))
            {
                PIXEL11_10
            }
            else
            {
                PIXEL11_70
            }
            break;
        }
        case 89: {
            PIXEL00_12
            PIXEL01_22
            if (Diff(p[8], p[4]))
            {
                PIXEL10_10
            }
            else
            {
                PIXEL10_70
            }
            if (Diff(p[6], p[8]))
            {
                PIXEL11_10
            }
            else
            {
                PIXEL11_70
            }
            break;
        }
        case 90: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_10
            }
            else
            {
                PIXEL00_70
            }
            if (Diff(p[2], p[6]))
            {
                PIXEL01_10
            }
            else
            {
                PIXEL01_70
            }
            if (Diff(p[8], p[4]))
            {
                PIXEL10_10
            }
            else
            {
                PIXEL10_70
            }
            if (Diff(p[6], p[8]))
            {
                PIXEL11_10
            }
            else
            {
                PIXEL11_70
            }
            break;
        }
        case 55:
        case 23: {
            if (Diff(p[2], p[6]))
            {
                PIXEL00_11
                PIXEL01_0
            }
            else
            {
                PIXEL00_60
                PIXEL01_90
            }
            PIXEL10_20
            PIXEL11_21
            break;
        }
        case 182:
        case 150: {
            PIXEL00_22
            if (Diff(p[2], p[6]))
            {
                PIXEL01_0
                PIXEL11_12
            }
            else
            {
                PIXEL01_90
                PIXEL11_61
            }
            PIXEL10_20
            break;
        }
        case 213:
        case 212: {
            PIXEL00_20
            if (Diff(p[6], p[8]))
            {
                PIXEL01_11
                PIXEL11_0
            }
            else
            {
                PIXEL01_60
                PIXEL11_90
            }
            PIXEL10_21
            break;
        }
        case 241:
        case 240: {
            PIXEL00_20
            PIXEL01_22
            if (Diff(p[6], p[8]))
            {
                PIXEL10_12
                PIXEL11_0
            }
            else
            {
                PIXEL10_61
                PIXEL11_90
            }
            break;
        }
        case 236:
        case 232: {
            PIXEL00_21
            PIXEL01_20
            if (Diff(p[8], p[4]))
            {
                PIXEL10_0
                PIXEL11_11
            }
            else
            {
                PIXEL10_90
                PIXEL11_60
            }
            break;
        }
        case 109:
        case 105: {
            if (Diff(p[8], p[4]))
            {
                PIXEL00_12
                PIXEL10_0
            }
            else
            {
                PIXEL00_61
                PIXEL10_90
            }
            PIXEL01_20
            PIXEL11_22
            break;
        }
        case 171:
        case 43: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_0
                PIXEL10_11
            }
            else
            {
                PIXEL00_90
                PIXEL10_60
            }
            PIXEL01_21
            PIXEL11_20
            break;
        }
        case 143:
        case 15: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_0
                PIXEL01_12
            }
            else
            {
                PIXEL00_90
                PIXEL01_61
            }
            PIXEL10_22
            PIXEL11_20
            break;
        }
        case 124: {
            PIXEL00_21
            PIXEL01_11
            if (Diff(p[8], p[4]))
            {
                PIXEL10_0
            }
            else
            {
                PIXEL10_20
            }
            PIXEL11_10
            break;
        }
        case 203: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_0
            }
            else
            {
                PIXEL00_20
            }
            PIXEL01_21
            PIXEL10_10
            PIXEL11_11
            break;
        }
        case 62: {
            PIXEL00_10
            if (Diff(p[2], p[6]))
            {
                PIXEL01_0
            }
            else
            {
                PIXEL01_20
            }
            PIXEL10_11
            PIXEL11_21
            break;
        }
        case 211: {
            PIXEL00_11
            PIXEL01_10
            PIXEL10_21
            if (Diff(p[6], p[8]))
            {
                PIXEL11_0
            }
            else
            {
                PIXEL11_20
            }
            break;
        }
        case 118: {
            PIXEL00_22
            if (Diff(p[2], p[6]))
            {
                PIXEL01_0
            }
            else
            {
                PIXEL01_20
            }
            PIXEL10_12
            PIXEL11_10
            break;
        }
        case 217: {
            PIXEL00_12
            PIXEL01_22
            PIXEL10_10
            if (Diff(p[6], p[8]))
            {
                PIXEL11_0
            }
            else
            {
                PIXEL11_20
            }
            break;
        }
        case 110: {
            PIXEL00_10
            PIXEL01_12
            if (Diff(p[8], p[4]))
            {
                PIXEL10_0
            }
            else
            {
                PIXEL10_20
            }
            PIXEL11_22
            break;
        }
        case 155: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_0
            }
            else
            {
                PIXEL00_20
            }
            PIXEL01_10
            PIXEL10_22
            PIXEL11_12
            break;
        }
        case 188: {
            PIXEL00_21
            PIXEL01_11
            PIXEL10_11
            PIXEL11_12
            break;
        }
        case 185: {
            PIXEL00_12
            PIXEL01_22
            PIXEL10_11
            PIXEL11_12
            break;
        }
        case 61: {
            PIXEL00_12
            PIXEL01_11
            PIXEL10_11
            PIXEL11_21
            break;
        }
        case 157: {
            PIXEL00_12
            PIXEL01_11
            PIXEL10_22
            PIXEL11_12
            break;
        }
        case 103: {
            PIXEL00_11
            PIXEL01_12
            PIXEL10_12
            PIXEL11_22
            break;
        }
        case 227: {
            PIXEL00_11
            PIXEL01_21
            PIXEL10_12
            PIXEL11_11
            break;
        }
        case 230: {
            PIXEL00_22
            PIXEL01_12
            PIXEL10_12
            PIXEL11_11
            break;
        }
        case 199: {
            PIXEL00_11
            PIXEL01_12
            PIXEL10_21
            PIXEL11_11
            break;
        }
        case 220: {
            PIXEL00_21
            PIXEL01_11
            if (Diff(p[8], p[4]))
            {
                PIXEL10_10
            }
            else
            {
                PIXEL10_70
            }
            if (Diff(p[6], p[8]))
            {
                PIXEL11_0
            }
            else
            {
                PIXEL11_20
            }
            break;
        }
        case 158: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_10
            }
            else
            {
                PIXEL00_70
            }
            if (Diff(p[2], p[6]))
            {
                PIXEL01_0
            }
            else
            {
                PIXEL01_20
            }
            PIXEL10_22
            PIXEL11_12
            break;
        }
        case 234: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_10
            }
            else
            {
                PIXEL00_70
            }
            PIXEL01_21
            if (Diff(p[8], p[4]))
            {
                PIXEL10_0
            }
            else
            {
                PIXEL10_20
            }
            PIXEL11_11
            break;
        }
        case 242: {
            PIXEL00_22
            if (Diff(p[2], p[6]))
            {
                PIXEL01_10
            }
            else
            {
                PIXEL01_70
            }
            PIXEL10_12
            if (Diff(p[6], p[8]))
            {
                PIXEL11_0
            }
            else
            {
                PIXEL11_20
            }
            break;
        }
        case 59: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_0
            }
            else
            {
                PIXEL00_20
            }
            if (Diff(p[2], p[6]))
            {
                PIXEL01_10
            }
            else
            {
                PIXEL01_70
            }
            PIXEL10_11
            PIXEL11_21
            break;
        }
        case 121: {
            PIXEL00_12
            PIXEL01_22
            if (Diff(p[8], p[4]))
            {
                PIXEL10_0
            }
            else
            {
                PIXEL10_20
            }
            if (Diff(p[6], p[8]))
            {
                PIXEL11_10
            }
            else
            {
                PIXEL11_70
            }
            break;
        }
        case 87: {
            PIXEL00_11
            if (Diff(p[2], p[6]))
            {
                PIXEL01_0
            }
            else
            {
                PIXEL01_20
            }
            PIXEL10_21
            if (Diff(p[6], p[8]))
            {
                PIXEL11_10
            }
            else
            {
                PIXEL11_70
            }
            break;
        }
        case 79: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_0
            }
            else
            {
                PIXEL00_20
            }
            PIXEL01_12
            if (Diff(p[8], p[4]))
            {
                PIXEL10_10
            }
            else
            {
                PIXEL10_70
            }
            PIXEL11_22
            break;
        }
        case 122: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_10
            }
            else
            {
                PIXEL00_70
            }
            if (Diff(p[2], p[6]))
            {
                PIXEL01_10
            }
            else
            {
                PIXEL01_70
            }
            if (Diff(p[8], p[4]))
            {
                PIXEL10_0
            }
            else
            {
                PIXEL10_20
            }
            if (Diff(p[6], p[8]))
            {
                PIXEL11_10
            }
            else
            {
                PIXEL11_70
            }
            break;
        }
        case 94: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_10
            }
            else
            {
                PIXEL00_70
            }
            if (Diff(p[2], p[6]))
            {
                PIXEL01_0
            }
            else
            {
                PIXEL01_20
            }
            if (Diff(p[8], p[4]))
            {
                PIXEL10_10
            }
            else
            {
                PIXEL10_70
            }
            if (Diff(p[6], p[8]))
            {
                PIXEL11_10
            }
            else
            {
                PIXEL11_70
            }
            break;
        }
        case 218: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_10
            }
            else
            {
                PIXEL00_70
            }
            if (Diff(p[2], p[6]))
            {
                PIXEL01_10
            }
            else
            {
                PIXEL01_70
            }
            if (Diff(p[8], p[4]))
            {
                PIXEL10_10
            }
            else
            {
                PIXEL10_70
            }
            if (Diff(p[6], p[8]))
            {
                PIXEL11_0
            }
            else
            {
                PIXEL11_20
            }
            break;
        }
        case 91: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_0
            }
            else
            {
                PIXEL00_20
            }
            if (Diff(p[2], p[6]))
            {
                PIXEL01_10
            }
            else
            {
                PIXEL01_70
            }
            if (Diff(p[8], p[4]))
            {
                PIXEL10_10
            }
            else
            {
                PIXEL10_70
            }
            if (Diff(p[6], p[8]))
            {
                PIXEL11_10
            }
            else
            {
                PIXEL11_70
            }
            break;
        }
        case 229: {
            PIXEL00_20
            PIXEL01_20
            PIXEL10_12
            PIXEL11_11
            break;
        }
        case 167: {
            PIXEL00_11
            PIXEL01_12
            PIXEL10_20
            PIXEL11_20
            break;
        }
        case 173: {
            PIXEL00_12
            PIXEL01_20
            PIXEL10_11
            PIXEL11_20
            break;
        }
        case 181: {
            PIXEL00_20
            PIXEL01_11
            PIXEL10_20
            PIXEL11_12
            break;
        }
        case 186: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_10
            }
            else
            {
                PIXEL00_70
            }
            if (Diff(p[2], p[6]))
            {
                PIXEL01_10
            }
            else
            {
                PIXEL01_70
            }
            PIXEL10_11
            PIXEL11_12
            break;
        }
        case 115: {
            PIXEL00_11
            if (Diff(p[2], p[6]))
            {
                PIXEL01_10
            }
            else
            {
                PIXEL01_70
            }
            PIXEL10_12
            if (Diff(p[6], p[8]))
            {
                PIXEL11_10
            }
            else
            {
                PIXEL11_70
            }
            break;
        }
        case 93: {
            PIXEL00_12
            PIXEL01_11
            if (Diff(p[8], p[4]))
            {
                PIXEL10_10
            }
            else
            {
                PIXEL10_70
            }
            if (Diff(p[6], p[8]))
            {
                PIXEL11_10
            }
            else
            {
                PIXEL11_70
            }
            break;
        }
        case 206: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_10
            }
            else
            {
                PIXEL00_70
            }
            PIXEL01_12
            if (Diff(p[8], p[4]))
            {
                PIXEL10_10
            }
            else
            {
                PIXEL10_70
            }
            PIXEL11_11
            break;
        }
        case 205:
        case 201: {
            PIXEL00_12
            PIXEL01_20
            if (Diff(p[8], p[4]))
            {
                PIXEL10_10
            }
            else
            {
                PIXEL10_70
            }
            PIXEL11_11
            break;
        }
        case 174:
        case 46: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_10
            }
            else
            {
                PIXEL00_70
            }
            PIXEL01_12
            PIXEL10_11
            PIXEL11_20
            break;
        }
        case 179:
        case 147: {
            PIXEL00_11
            if (Diff(p[2], p[6]))
            {
                PIXEL01_10
            }
            else
            {
                PIXEL01_70
            }
            PIXEL10_20
            PIXEL11_12
            break;
        }
        case 117:
        case 116: {
            PIXEL00_20
            PIXEL01_11
            PIXEL10_12
            if (Diff(p[6], p[8]))
            {
                PIXEL11_10
            }
            else
            {
                PIXEL11_70
            }
            break;
        }
        case 189: {
            PIXEL00_12
            PIXEL01_11
            PIXEL10_11
            PIXEL11_12
            break;
        }
        case 231: {
            PIXEL00_11
            PIXEL01_12
            PIXEL10_12
            PIXEL11_11
            break;
        }
        case 126: {
            PIXEL00_10
            if (Diff(p[2], p[6]))
            {
                PIXEL01_0
            }
            else
            {
                PIXEL01_20
            }
            if (Diff(p[8], p[4]))
            {
                PIXEL10_0
            }
            else
            {
                PIXEL10_20
            }
            PIXEL11_10
            break;
        }
        case 219: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_0
            }
            else
            {
                PIXEL00_20
            }
            PIXEL01_10
            PIXEL10_10
            if (Diff(p[6], p[8]))
            {
                PIXEL11_0
            }
            else
            {
                PIXEL11_20
            }
            break;
        }
        case 125: {
            if (Diff(p[8], p[4]))
            {
                PIXEL00_12
                PIXEL10_0
            }
            else
            {
                PIXEL00_61
                PIXEL10_90
            }
            PIXEL01_11
            PIXEL11_10
            break;
        }
        case 221: {
            PIXEL00_12
            if (Diff(p[6], p[8]))
            {
                PIXEL01_11
                PIXEL11_0
            }
            else
            {
                PIXEL01_60
                PIXEL11_90
            }
            PIXEL10_10
            break;
        }
        case 207: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_0
                PIXEL01_12
            }
            else
            {
                PIXEL00_90
                PIXEL01_61
            }
            PIXEL10_10
            PIXEL11_11
            break;
        }
        case 238: {
            PIXEL00_10
            PIXEL01_12
            if (Diff(p[8], p[4]))
            {
                PIXEL10_0
                PIXEL11_11
            }
            else
            {
                PIXEL10_90
                PIXEL11_60
            }
            break;
        }
        case 190: {
            PIXEL00_10
            if (Diff(p[2], p[6]))
            {
                PIXEL01_0
                PIXEL11_12
            }
            else
            {
                PIXEL01_90
                PIXEL11_61
            }
            PIXEL10_11
            break;
        }
        case 187: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_0
                PIXEL10_11
            }
            else
            {
                PIXEL00_90
                PIXEL10_60
            }
            PIXEL01_10
            PIXEL11_12
            break;
        }
        case 243: {
            PIXEL00_11
            PIXEL01_10
            if (Diff(p[6], p[8]))
            {
                PIXEL10_12
                PIXEL11_0
            }
            else
            {
                PIXEL10_61
                PIXEL11_90
            }
            break;
        }
        case 119: {
            if (Diff(p[2], p[6]))
            {
                PIXEL00_11
                PIXEL01_0
            }
            else
            {
                PIXEL00_60
                PIXEL01_90
            }
            PIXEL10_12
            PIXEL11_10
            break;
        }
        case 237:
        case 233: {
            PIXEL00_12
            PIXEL01_20
            if (Diff(p[8], p[4]))
            {
                PIXEL10_0
            }
            else
            {
                PIXEL10_100
            }
            PIXEL11_11
            break;
        }
        case 175:
        case 47: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_0
            }
            else
            {
                PIXEL00_100
            }
            PIXEL01_12
            PIXEL10_11
            PIXEL11_20
            break;
        }
        case 183:
        case 151: {
            PIXEL00_11
            if (Diff(p[2], p[6]))
            {
                PIXEL01_0
            }
            else
            {
                PIXEL01_100
            }
            PIXEL10_20
            PIXEL11_12
            break;
        }
        case 245:
        case 244: {
            PIXEL00_20
            PIXEL01_11
            PIXEL10_12
            if (Diff(p[6], p[8]))
            {
                PIXEL11_0
            }
            else
            {
                PIXEL11_100
            }
            break;
        }
        case 250: {
            PIXEL00_10
            PIXEL01_10
            if (Diff(p[8], p[4]))
            {
                PIXEL10_0
            }
            else
            {
                PIXEL10_20
            }
            if (Diff(p[6], p[8]))
            {
                PIXEL11_0
            }
            else
            {
                PIXEL11_20
            }
            break;
        }
        case 123: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_0
            }
            else
            {
                PIXEL00_20
            }
            PIXEL01_10
            if (Diff(p[8], p[4]))
            {
                PIXEL10_0
            }
            else
            {
                PIXEL10_20
            }
            PIXEL11_10
            break;
        }
        case 95: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_0
            }
            else
            {
                PIXEL00_20
            }
            if (Diff(p[2], p[6]))
            {
                PIXEL01_0
            }
            else
            {
                PIXEL01_20
            }
            PIXEL10_10
            PIXEL11_10
            break;
        }
        case 222: {
            PIXEL00_10
            if (Diff(p[2], p[6]))
            {
                PIXEL01_0
            }
            else
            {
                PIXEL01_20
            }
            PIXEL10_10
            if (Diff(p[6], p[8]))
            {
                PIXEL11_0
            }
            else
            {
                PIXEL11_20
            }
            break;
        }
        case 252: {
            PIXEL00_21
            PIXEL01_11
            if (Diff(p[8], p[4]))
            {
                PIXEL10_0
            }
            else
            {
                PIXEL10_20
            }
            if (Diff(p[6], p[8]))
            {
                PIXEL11_0
            }
            else
            {
                PIXEL11_100
            }
            break;
        }
        case 249: {
            PIXEL00_12
            PIXEL01_22
            if (Diff(p[8], p[4]))
            {
                PIXEL10_0
            }
            else
            {
                PIXEL10_100
            }
            if (Diff(p[6], p[8]))
            {
                PIXEL11_0
            }
            else
            {
                PIXEL11_20
            }
            break;
        }
        case 235: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_0
            }
            else
            {
                PIXEL00_20
            }
            PIXEL01_21
            if (Diff(p[8], p[4]))
            {
                PIXEL10_0
            }
            else
            {
                PIXEL10_100
            }
            PIXEL11_11
            break;
        }
        case 111: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_0
            }
            else
            {
                PIXEL00_100
            }
            PIXEL01_12
            if (Diff(p[8], p[4]))
            {
                PIXEL10_0
            }
            else
            {
                PIXEL10_20
            }
            PIXEL11_22
            break;
        }
        case 63: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_0
            }
            else
            {
                PIXEL00_100
            }
            if (Diff(p[2], p[6]))
            {
                PIXEL01_0
            }
            else
            {
                PIXEL01_20
            }
            PIXEL10_11
            PIXEL11_21
            break;
        }
        case 159: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_0
            }
            else
            {
                PIXEL00_20
            }
            if (Diff(p[2], p[6]))
            {
                PIXEL01_0
            }
            else
            {
                PIXEL01_100
            }
            PIXEL10_22
            PIXEL11_12
            break;
        }
        case 215: {
            PIXEL00_11
            if (Diff(p[2], p[6]))
            {
                PIXEL01_0
            }
            else
            {
                PIXEL01_100
            }
            PIXEL10_21
            if (Diff(p[6], p[8]))
            {
                PIXEL11_0
            }
            else
            {
                PIXEL11_20
            }
            break;
        }
        case 246: {
            PIXEL00_22
            if (Diff(p[2], p[6]))
            {
                PIXEL01_0
            }
            else
            {
                PIXEL01_20
            }
            PIXEL10_12
            if (Diff(p[6], p[8]))
            {
                PIXEL11_0
            }
            else
            {
                PIXEL11_100
            }
            break;
        }
        case 254: {
            PIXEL00_10
            if (Diff(p[2], p[6]))
            {
                PIXEL01_0
            }
            else
            {
                PIXEL01_20
            }
            if (Diff(p[8], p[4]))
            {
                PIXEL10_0
            }
            else
            {
                PIXEL10_20
            }
            if (Diff(p[6], p[8]))
            {
                PIXEL11_0
            }
            else
            {
                PIXEL11_100
            }
            break;
        }
        case 253: {
            PIXEL00_12
            PIXEL01_11
            if (Diff(p[8], p[4]))
            {
                PIXEL10_0
            }
            else
            {
                PIXEL10_100
            }
            if (Diff(p[6], p[8]))
            {
                PIXEL11_0
            }
            else
            {
                PIXEL11_100
            }
            break;
        }
        case 251: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_0
            }
            else
            {
                PIXEL00_20
            }
            PIXEL01_10
            if (Diff(p[8], p[4]))
            {
                PIXEL10_0
            }
            else
            {
                PIXEL10_100
            }
            if (Diff(p[6], p[8]))
            {
                PIXEL11_0
            }
            else
            {
                PIXEL11_20
            }
            break;
        }
        case 239: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_0
            }
            else
            {
                PIXEL00_100
            }
            PIXEL01_12
            if (Diff(p[8], p[4]))
            {
                PIXEL10_0
            }
            else
            {
                PIXEL10_100
            }
            PIXEL11_11
            break;
        }
        case 127: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_0
            }
            else
            {
                PIXEL00_100
            }
            if (Diff(p[2], p[6]))
            {
                PIXEL01_0
            }
            else
            {
                PIXEL01_20
            }
            if (Diff(p[8], p[4]))
            {
                PIXEL10_0
            }
            else
            {
                PIXEL10_20
            }
            PIXEL11_10
            break;
        }
        case 191: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_0
            }
            else
            {
                PIXEL00_100
            }
            if (Diff(p[2], p[6]))
            {
                PIXEL01_0
            }
            else
            {
                PIXEL01_100
            }
            PIXEL10_11
            PIXEL11_12
            break;
        }
        case 223: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_0
            }
            else
            {
                PIXEL00_20
            }
            if (Diff(p[2], p[6]))
            {
                PIXEL01_0
            }
            else
            {
                PIXEL01_100
            }
            PIXEL10_10
            if (Diff(p[6], p[8]))
            {
                PIXEL11_0
            }
            else
            {
                PIXEL11_20
            }
            break;
        }
        case 247: {
            PIXEL00_11
            if (Diff(p[2], p[6]))
            {
                PIXEL01_0
            }
            else
            {
                PIXEL01_100
            }
            PIXEL10_12
            if (Diff(p[6], p[8]))
            {
                PIXEL11_0
            }
            else
            {
                PIXEL11_100
            }
            break;
        }
        case 255: {
            if (Diff(p[4], p[2]))
            {
                PIXEL00_0
            }
            else
            {
                PIXEL00_100
            }
            if (Diff(p[2], p[6]))
            {
                PIXEL01_0
            }
            else
            {
                PIXEL01_100
            }
            if (Diff(p[8], p[4]))
            {
                PIXEL10_0
            }
            else
            {
                PIXEL10_100
            }
            if (Diff(p[6], p[8]))
            {
                PIXEL11_0
            }
            else
            {
                PIXEL11_100
            }
            break;
        }
        } // switch (pattern)
    }     // for (x)
}

void StripAlpha(u8_t *dest, const u8_t *src, int width)
{
    // we don't care about transparent pixels here (on the assumption
    // that the original image didn't have any).

    const u8_t *src_end = src + width * 8;

    for (; src < src_end; src += 4, dest += 3)
    {
        dest[0] = src[0];
        dest[1] = src[1];
        dest[2] = src[2];
    }
}

image_data_c *Convert(image_data_c *img, bool solid, bool invert)
{
    int w = img->width;
    int h = img->height;

    image_data_c *result = new image_data_c(w * 2, h * 2, solid ? 3 : 4);

    // for solid mode, we must strip off the alpha channel
    u8_t *temp_buffer = NULL;

    if (solid)
        temp_buffer = new u8_t[w * 16]; // two lines worth

    for (int y = 0; y < h; y++)
    {
        int dst_y = invert ? (h - 1 - y) : y;

        u8_t *out_buf = solid ? temp_buffer : result->PixelAt(0, dst_y * 2);

        ConvertLine(y, w, h, invert, out_buf, img->PixelAt(0, y));

        if (solid)
            StripAlpha(result->PixelAt(0, dst_y * 2), temp_buffer, w * 2);
    }

    if (temp_buffer)
        delete[] temp_buffer;

    return result;
}

} // namespace Hq2x
} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
