//------------------------------------------------------------------------
//  Basic image storage
//------------------------------------------------------------------------
// 
//  Copyright (c) 2003-2008  The EDGE Team.
// 
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//------------------------------------------------------------------------

#ifndef __EPI_IMAGEDATA_H__
#define __EPI_IMAGEDATA_H__

namespace epi
{

class image_data_c
{
public:
	short width;
	short height;

	// Bytes Per Pixel, determines image mode:
	// 1 = palettised
	// 3 = format is RGB
	// 4 = format is RGBA
	short bpp;

	// for image loading, these will be the actual image size
	short used_w;
	short used_h;

	u8_t *pixels;

public:
	image_data_c(int _w, int _h, int _bpp = 3);
	~image_data_c();

	void Clear(u8_t val = 0);

	inline u8_t *PixelAt(int x, int y) const
	{
		// Note: DOES NOT CHECK COORDS

		return pixels + (y * width + x) * bpp;
	}

	inline void CopyPixel(int sx, int sy, int dx, int dy)
	{
		u8_t *src  = PixelAt(sx, sy);
		u8_t *dest = PixelAt(dx, dy);

		for (int i = 0; i < bpp; i++)
			*dest++ = *src++;
	}

	// convert all RGB(A) pixels to a greyscale equivalent.
	void Whiten();
	
	// turn the image up-side-down.
	void Invert();

	// shrink an image to a smaller image.
	// The old size and the new size must be powers of two.
	// For RGB(A) images the pixel values are averaged.
	// Palettised images are not averaged, instead the bottom
	// left pixel in each group becomes the final pixel.
	void Shrink(int new_w, int new_h);

	// like Shrink(), but for RGBA images the source alpha is
	// used as a weighting factor for the shrunken color, hence
	// purely transparent pixels never affect the final color
	// of a pixel group.
	void ShrinkMasked(int new_w, int new_h);

	// scale the image up to a larger size.
	// The old size and the new size must be powers of two.
	void Grow(int new_w, int new_h);

	// convert an RGBA image to RGB.  Partially transparent colors
	// (alpha < 255) are blended with black.
	void RemoveAlpha();

	// Set uniform alpha value for all pixels in an image
	// If RGB, will convert to RGBA
	void SetAlpha(int alphaness);
	
	// test each alpha value in the RGBA image against the threshold:
	// lesser values become 0, and greater-or-equal values become 255.
	void ThresholdAlpha(u8_t alpha = 128);

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

	// For the IMAGE DDFFONT type, determines the width of a character
	// by finding the row with the largest distance between the first
	// and last non-background-colored pixel
	int ImageCharacterWidth(int x1, int y1, int x2, int y2);

	// compute the average Hue of the RGB(A) image, storing the
	// result in the 'hue' array (r, g, b).  The average intensity
	// will be stored in 'ity' when given.
	void AverageHue(u8_t *hue, u8_t *ity = NULL);

	// compute the average color of the RGB image, storing the
	// result in the 'rgb' array (r, g, b).
	void AverageColor(u8_t *rgb);

	// compute the average color of the RGB image, storing the
	// result in the 'rgb' array (r, g, b).
	void AverageTopBorderColor(u8_t *rgb);

	// compute the average color of the RGB image, storing the
	// result in the 'rgb' array (r, g, b).
	void AverageBottomBorderColor(u8_t *rgb);

	// compute the lightest color in the RGB image, storing the
	// result in the 'rgb' array (r, g, b).
	void LightestColor(u8_t *rgb);

	// compute the darkest color in the RGB image, storing the
	// result in the 'rgb' array (r, g, b).
	void DarkestColor(u8_t *rgb);

	// SMMU-style swirling
	void Swirl(int leveltime, int thickness);

	// fill the margins of non-power-of-two images with a copy of the
	// left and/or top parts of the image.  This doesn't make it tile
	// properly, but it looks better than having areas of black.
	void FillMarginX(int actual_w);
	void FillMarginY(int actual_h);
};

} // namespace epi

#endif  /* __EPI_IMAGEDATA_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
