//------------------------------------------------------------------------
//  Image Handling
//------------------------------------------------------------------------
//
//  Copyright (c) 2003-2008  The EDGE Team.
//  Migrated to use stb_image in 2021 - Dashodanger
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

#include "image_funcs.h"

#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_TGA
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace epi
{

// Header check backported from EDGE 2.x - Dasho
bool PNG_IsDataPNG(const byte *data, int length)
{
    static byte png_sig[4] = { 0x89, 0x50, 0x4E, 0x47 };
	if (length < 4)
		return false;

	return memcmp(data, png_sig, 4) == 0;
}

image_data_c *Image_Load(file_c *f, int read_flags, int format)
{
	int width;
	int height;
	int channels;
	int desired_channels;
	byte *raw_image = f->LoadIntoMemory();

	if (format == 1)
		desired_channels = 3;
	else
		desired_channels = 4;

	unsigned char *decoded_img = stbi_load_from_memory(raw_image, f->GetLength(), &width, &height, &channels, desired_channels);

	if (!decoded_img)
		return NULL;

	int tot_W = width;
	int tot_H = height;

	if (read_flags & IRF_Round_POW2)
	{
		tot_W = 1; while (tot_W < (int)width)  tot_W <<= 1;
		tot_H = 1; while (tot_H < (int)height) tot_H <<= 1;
	}

	image_data_c *img = new image_data_c(tot_W, tot_H, desired_channels);

	img->used_w = width;
	img->used_h = height;

	if (img->used_w != tot_W || img->used_h != tot_H)
		img->Clear();

	int total_pixels = 0;

	for (int y = height - 1; y > -1; y--)
	{
		for (int x = 0; x < width; x++)
		{
			memcpy(img->PixelAt(x, y), decoded_img + (total_pixels * desired_channels), desired_channels);
			total_pixels++;
		}
	}

	delete[] raw_image;

	stbi_image_free(decoded_img);

	return img;
}

bool Image_GetInfo(file_c *f, int *width, int *height, bool *solid, int format)
{
	int channels = 0;
	byte *raw_image = f->LoadIntoMemory();

	int result = stbi_info_from_memory(raw_image, f->GetLength(), width, height, &channels);

	if (format == 1)
		*solid = true;
	else
		*solid = false;

	delete[] raw_image;

	return result;
}

//------------------------------------------------------------------------

bool JPEG_Save(const char *fn, image_data_c *img, int quality)
{
	SYS_ASSERT(img->bpp == 3);

	img->Invert();

	int result = stbi_write_jpg(fn, img->used_w, img->used_h, img->bpp, img->pixels, quality);

	return result;
}

bool PNG_Save(const char *fn, image_data_c *img, int compress)
{
	img->Invert();

	stbi_write_png_compression_level = compress;

	int result = stbi_write_png(fn, img->used_w, img->used_h, img->bpp, img->pixels, 0);

	return result;	
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
