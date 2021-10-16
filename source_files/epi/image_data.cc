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

#include "epi.h"

#include "image_data.h"


namespace epi
{

image_data_c::image_data_c(int _w, int _h, int _bpp) :
    width(_w), height(_h), bpp(_bpp), flags(IDF_NONE),
    used_w(_w), used_h(_h)
{
	pixels = new u8_t[width * height * bpp];
}

image_data_c::~image_data_c()
{
	delete[] pixels;

	pixels = NULL;
	width = height = 0;
}

void image_data_c::Clear(u8_t val)
{
	memset(pixels, val, width * height * bpp);
}

void image_data_c::Whiten()
{
	SYS_ASSERT(bpp >= 3);

	// TODO: OPTIMISE this

	for (int y = 0; y < height; y++)
	for (int x = 0; x < width;  x++)
	{
		u8_t *src = PixelAt(x, y);

		int ity = MAX(src[0], MAX(src[1], src[2]));

		ity = (ity * 128 + src[0] * 38 + src[1] * 64 + src[2] * 26) >> 8;

		src[0] = src[1] = src[2] = ity; 
	}
}

void image_data_c::Invert()
{
	int line_size = used_w * bpp;

	u8_t *line_data = new u8_t[line_size + 1];

	for (int y=0; y < used_h/2; y++)
	{
		int y2 = used_h - 1 - y;

		memcpy(line_data,      PixelAt(0, y),  line_size);
		memcpy(PixelAt(0, y),  PixelAt(0, y2), line_size);
		memcpy(PixelAt(0, y2), line_data,      line_size);
	}

	delete[] line_data;
}

void image_data_c::Shrink(int new_w, int new_h)
{
	SYS_ASSERT(new_w <= width && new_h <= height);

	int step_x = width  / new_w;
	int step_y = height / new_h;
	int total  = step_x * step_y;

	// TODO: OPTIMISE this

	if (bpp == 1)
	{
		for (int dy=0; dy < new_h; dy++)
		for (int dx=0; dx < new_w; dx++)
		{
			u8_t *dest_pix = pixels + (dy * new_w + dx) * 3;

			int sx = dx * step_x;
			int sy = dy * step_y;

			const u8_t *src_pix = PixelAt(sx, sy);

			*dest_pix = *src_pix;
		}
	}
	else if (bpp == 3)
	{
		for (int dy=0; dy < new_h; dy++)
		for (int dx=0; dx < new_w; dx++)
		{
			u8_t *dest_pix = pixels + (dy * new_w + dx) * 3;

			int sx = dx * step_x;
			int sy = dy * step_y;

			int r=0, g=0, b=0;

			// compute average colour of block
			for (int x=0; x < step_x; x++)
			for (int y=0; y < step_y; y++)
			{
				const u8_t *src_pix = PixelAt(sx+x, sy+y);

				r += src_pix[0];
				g += src_pix[1];
				b += src_pix[2];
			}

			dest_pix[0] = r / total;
			dest_pix[1] = g / total;
			dest_pix[2] = b / total;
		}
	}
	else  /* bpp == 4 */
	{
		for (int dy=0; dy < new_h; dy++)
		for (int dx=0; dx < new_w; dx++)
		{
			u8_t *dest_pix = pixels + (dy * new_w + dx) * 4;

			int sx = dx * step_x;
			int sy = dy * step_y;

			int r=0, g=0, b=0, a=0;

			// compute average colour of block
			for (int x=0; x < step_x; x++)
			for (int y=0; y < step_y; y++)
			{
				const u8_t *src_pix = PixelAt(sx+x, sy+y);

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

	used_w  = MAX(1, used_w * new_w / width);
	used_h  = MAX(1, used_h * new_h / height);

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

	int step_x = width  / new_w;
	int step_y = height / new_h;
	int total  = step_x * step_y;

	// TODO: OPTIMISE this

	for (int dy=0; dy < new_h; dy++)
	for (int dx=0; dx < new_w; dx++)
	{
		u8_t *dest_pix = pixels + (dy * new_w + dx) * 4;

		int sx = dx * step_x;
		int sy = dy * step_y;

		int r=0, g=0, b=0, a=0;

		// compute average colour of block
		for (int x=0; x < step_x; x++)
		for (int y=0; y < step_y; y++)
		{
			const u8_t *src_pix = PixelAt(sx+x, sy+y);

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

	used_w  = MAX(1, used_w * new_w / width);
	used_h  = MAX(1, used_h * new_h / height);

	width  = new_w;
	height = new_h;
}

void image_data_c::Grow(int new_w, int new_h)
{
	SYS_ASSERT(new_w >= width && new_h >= height);

	u8_t *new_pixels = new u8_t[new_w * new_h * bpp];

	for (int dy = 0; dy < new_h; dy++)
	for (int dx = 0; dx < new_w; dx++)
	{
		int sx = dx * width  / new_w;
		int sy = dy * height / new_h;

		const u8_t *src = PixelAt(sx, sy);

		u8_t *dest = new_pixels + (dy * new_w + dx) * bpp;

		for (int i = 0; i < bpp; i++)
			*dest++ = *src++;
	}

	delete[] pixels;

	used_w  = used_w * new_w / width;
	used_h  = used_h * new_h / height;

	pixels  = new_pixels;
	width   = new_w;
	height  = new_h;
}

void image_data_c::RemoveAlpha()
{
	if (bpp != 4)
		return;

	u8_t *src   = pixels;
	u8_t *s_end = src + (width * height * bpp);
	u8_t *dest  = pixels;

	for (; src < s_end; src += 4)
	{
		// blend alpha with BLACK

		*dest++ = (int)src[0] * (int)src[3] / 255;
		*dest++ = (int)src[1] * (int)src[3] / 255;
		*dest++ = (int)src[2] * (int)src[3] / 255;
	}

	bpp = 3;
}

void image_data_c::ThresholdAlpha(u8_t alpha)
{
	if (bpp != 4)
		return;

	u8_t *src   = pixels;
	u8_t *s_end = src + (width * height * bpp);

	for (; src < s_end; src += 4)
	{
		src[3] = (src[3] < alpha) ? 0 : 255;
	}
}

void image_data_c::FourWaySymmetry()
{
	int w2 = (width  + 1) / 2;
	int h2 = (height + 1) / 2;

	for (int y = 0; y < h2; y++)
	for (int x = 0; x < w2; x++)
	{
		int ix = width  - 1 - x;
		int iy = height - 1 - y;

		CopyPixel(x, y, ix,  y);
		CopyPixel(x, y,  x, iy);
		CopyPixel(x, y, ix, iy);
	}
}

void image_data_c::EightWaySymmetry()
{
	SYS_ASSERT(width == height);

	int hw = (width + 1) / 2;

	for (int y = 0;   y < hw; y++)
	for (int x = y+1; x < hw; x++)
	{
		CopyPixel(x, y, y, x);
	}

	FourWaySymmetry();
}

void image_data_c::AverageHue(u8_t *hue, u8_t *ity)
{
	// make sure we don't overflow
	SYS_ASSERT(used_w * used_h <= 2048 * 2048);

	int r_sum = 0;
	int g_sum = 0;
	int b_sum = 0;
	int i_sum = 0;

	int weight = 0;

	for (int y = 0; y < used_h; y++)
	{
		const u8_t *src = PixelAt(0, y);

		for (int x = 0; x < used_w; x++, src += bpp)
		{
			int r = src[0];
			int g = src[1];
			int b = src[2];
			int a = (bpp == 4) ? src[3] : 255;

			int v = MAX(r, MAX(g, b));

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
				int m = MIN(r, MIN(g, b));

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

} // namespace epi


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
