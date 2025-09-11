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

#pragma once

#include <stdint.h>

#include "epi_color.h"

class ImageData
{
  public:
    int16_t width_;
    int16_t height_;

    // Bytes Per Pixel, determines image mode:
    // 1 = palettised
    // 3 = format is RGB
    // 4 = format is RGBA
    int16_t depth_;

    // in case offset/scaling from a parent image_c need to be stored (atlases)
    float offset_x_;
    float offset_y_;
    float scale_x_;
    float scale_y_;

    uint8_t *pixels_;

  public:
    ImageData(int width, int height, int depth = 3);
    ~ImageData();

    void Clear(uint8_t val = 0);

    inline uint8_t *PixelAt(int x, int y) const
    {
        // Note: DOES NOT CHECK COORDS

        return pixels_ + (y * width_ + x) * depth_;
    }

    inline void CopyPixel(int sx, int sy, int dx, int dy)
    {
        uint8_t *src  = PixelAt(sx, sy);
        uint8_t *dest = PixelAt(dx, dy);

        for (int i = 0; i < depth_; i++)
            *dest++ = *src++;
    }

    // convert all RGB(A) pixels to a greyscale equivalent.
    void Whiten();

    // turn the image up-side-down.
    void Invert();

    // horizontally flip the image
    void Flip();

    // shrink an image to a smaller image.
    // The old size and the new size must be powers of two.
    // For RGB(A) images the pixel values are averaged.
    // Palettised images are not averaged, instead the bottom
    // left pixel in each group becomes the final pixel.
    void Shrink(int new_width, int new_height);

    // like Shrink(), but for RGBA images the source alpha is
    // used as a weighting factor for the shrunken color, hence
    // purely transparent pixels never affect the final color
    // of a pixel group.
    void ShrinkMasked(int new_width, int new_height);

    // scale the image up to a larger size.
    // The old size and the new size must be powers of two.
    void Grow(int new_width, int new_height);

    // convert an RGBA image to RGB.  Partially transparent colors
    // (alpha < 255) are blended with black.
    void RemoveAlpha();

    // Set uniform alpha value for all pixels in an image
    // If RGB, will convert to RGBA
    void SetAlpha(int alpha);

    // test each alpha value in the RGBA image against the threshold:
    // lesser values become 0, and greater-or-equal values become 255.
    void ThresholdAlpha(uint8_t alpha = 128);

    // mirror the already-drawn corner (lowest x/y values) into the
    // other three corners.  When width or height is odd, the middle
    // column/row must already be drawn.
    void FourWaySymmetry();

    // Intended for font spritesheets; will turn the background color
    // (as determined by the first pixel of the image) transparent, if the
    // background is not already transparent
    void RemoveBackground();

    // mirror the already-drawn half corner (1/8th of the image)
    // into the rest of the image.  The source corner has lowest x/y
    // values, and the triangle piece is where y <= x, including the
    // pixels along the diagonal where (x == y).
    // NOTE: the image must be SQUARE (width == height).
    void EightWaySymmetry();

    // Determine the bounds of the image data that actually contain
    // non-backgroundpixels, based on the provided color
    void DetermineRealBounds(uint16_t *bottom, uint16_t *left, uint16_t *right, uint16_t *top,
                             RGBAColor background_color, int from_x = -1, int to_x = 1000000, int from_y = -1,
                             int to_y = 1000000);

    // compute the average Hue of the RGB(A) image, storing the
    // result in the 'hue' array (r, g, b).  The average intensity
    // will be stored in 'intensity' when given.
    RGBAColor AverageHue(int from_x = -1, int to_x = 1000000, int from_y = -1, int to_y = 1000000);

    // compute the average color of the RGB image, based on modal average
    RGBAColor AverageColor(int from_x = -1, int to_x = 1000000, int from_y = -1, int to_y = 1000000);

    // compute the lightest color in the RGB image
    RGBAColor LightestColor(int from_x = -1, int to_x = 1000000, int from_y = -1, int to_y = 1000000);

    // compute the darkest color in the RGB image
    RGBAColor DarkestColor(int from_x = -1, int to_x = 1000000, int from_y = -1, int to_y = 1000000);

    // SMMU-style swirling
    void Swirl(int level_time, int thickness);

    // Change various HSV color values if needed
    void SetHSV(int rotation, int saturation, int value);
};

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
