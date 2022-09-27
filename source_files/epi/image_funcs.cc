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
#include "path.h"

#define STBI_ONLY_PNG
#define STBI_ONLY_TGA
#define STBI_ONLY_JPEG

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace epi
{

typedef struct
{
    u8_t info_len;        /* length of info field */
    u8_t has_cmap;        /* 1 if image has colormap, 0 otherwise */
    u8_t type;

    u8_t cmap_start[2];   /* index of first colormap entry */
    u8_t cmap_len[2];     /* number of entries in colormap */
    u8_t cmap_bits;       /* bits per colormap entry */

    u8_t y_origin[2];     /* image origin */
    u8_t x_origin[2];
    u8_t width[2];        /* image size */
    u8_t height[2];

    u8_t pixel_bits;      /* bits/pixel */
    u8_t flags;
}
tga_header_t;

#define GET_U16_FIELD(hdvar, field)  (u16_t)  \
		((hdvar).field[0] + ((hdvar).field[1] << 8))

#define GET_S16_FIELD(hdvar, field)  (s16_t)  \
		((hdvar).field[0] + ((hdvar).field[1] << 8))

// Adapted from stb_image's stbi__tga_test function
bool TGA_IsDataTGA(const byte *data, int length)
{
	if (length < sizeof(tga_header_t))
		return false;

	tga_header_t header;
	int width, height;

	memcpy(&header, data, sizeof(header));

	if (header.has_cmap > 1) return false;

	if (header.has_cmap == 1)
	{
		if (header.type != 1 && header.type != 9) return false;
		if ((header.cmap_bits != 8) && (header.cmap_bits != 15) && (header.cmap_bits != 16)
			&& (header.cmap_bits != 24) && (header.cmap_bits != 32)) return false;
	}
	else
	{
		if ((header.type != 2) && (header.type != 3) && (header.type != 10)
			&& (header.type != 11)) return false;
	}
	height = GET_U16_FIELD(header, height);
	width = GET_U16_FIELD(header, width);
	if (height < 1 || width < 1) return false;
	if ((header.has_cmap == 1) && (header.pixel_bits != 8) && (header.pixel_bits != 16)) return false;
	if ((header.pixel_bits != 8) && (header.pixel_bits != 15) && (header.pixel_bits != 16)
		&& (header.pixel_bits != 24) && (header.pixel_bits != 32)) return false;

	return true;  
}

// PNG Header check backported from EDGE 2.x - Dasho
bool PNG_IsDataPNG(const byte *data, int length)
{
    static byte png_sig[4] = { 0x89, 0x50, 0x4E, 0x47 };
	if (length < 4)
		return false;

	return memcmp(data, png_sig, 4) == 0;
}


image_format_e Image_FilenameToFormat(const std::string& filename)
{
	std::string ext = epi::PATH_GetExtension(filename.c_str());

	if (ext == ".png" || ext == ".PNG")
		return FMT_PNG;

	if (ext == ".tga" || ext == ".TGA")
		return FMT_TGA;

	if (ext == ".jpg" || ext == ".JPG" || ext == ".jpeg" || ext == ".JPEG")
		return FMT_JPEG;

	return FMT_Unknown;
}


image_data_c *Image_Load(file_c *f, int format)
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

	// round size up to the nearest power-of-two
	if (true)
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

bool JPEG_Save(const char *fn, image_data_c *img)
{
	SYS_ASSERT(img->bpp == 3);

	// zero means failure here
	int result = stbi_write_jpg(fn, img->used_w, img->used_h, img->bpp, img->pixels, 95);

	return result != 0;
}

bool PNG_Save(const char *fn, image_data_c *img)
{
	// zero means failure here
	int result = stbi_write_png(fn, img->used_w, img->used_h, img->bpp, img->pixels, 0);

	return result != 0;
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
