//------------------------------------------------------------------------
//  Targa Image Handling
//------------------------------------------------------------------------
//
//  Copyright (c) 2006-2008  The EDGE Team.
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
//
//  Based on IMG_tga.c in the SDL_image library.
//  SDL_image is Copyright (C) 1997-2006 Sam Lantinga.
//
//------------------------------------------------------------------------

#include "epi.h"

#include "image_tga.h"


// -AJA- according to the documentation I see, colormaps in TGA
//       files are BGR(A) format (consistent with non-cmap files).
//
//       However it doesn't work with my test case (saved from
//       the GIMP).  Needs more testing with other programs.
#define SWAP_FOR_INDEXED  0


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

typedef enum
{
	TGA_TYPE_INDEXED = 1,
    TGA_TYPE_RGB = 2,
    TGA_TYPE_BW = 3,

    TGA_TYPE_RLE_INDEXED = 9,
    TGA_TYPE_RLE_RGB = 10,
    TGA_TYPE_RLE_BW = 11
}
tga_type_e;

typedef enum
{
	TGA_FLAG_TOP_DOWN = 0x20,
}
tga_flag_e;


static inline void SwapPixels(u8_t *pix, short bpp, int num)
{
	for (; num > 0; num--, pix += bpp)
	{
	  	u8_t temp = pix[0]; pix[0] = pix[2]; pix[2] = temp;
	}
}

static bool ReadPixels(image_data_c *img, file_c *f, int& x, int& y, int total)
{
	while (total > 0)
	{
		if (y >= img->used_h)
		{
			I_Debugf("TGA_Load: uncompressed too much!\n");
			return false;
		}

		int w = img->used_w - x;

		if (w > total)
			w = total;
		
		SYS_ASSERT(w > 0);
		SYS_ASSERT(x + w <= img->used_w);

		u8_t *dest = img->PixelAt(x, y);

		int size = w * img->bpp;

		if (size != (int)f->Read(dest, size))
			return false;

		// handle BGRA format (swap R and B!)
		SwapPixels(dest, img->bpp, w);

		x     += w;
		total -= w;

		if (x >= img->used_w)
		{
			x  = 0;
			y += 1;
		}
	}

	return true;
}

static bool ReadPixels_ColMap(image_data_c *img, file_c *f, int& x, int& y,
						      int total, const u8_t *palette)
{
	u8_t buffer[256];
		
	while (total > 0)
	{
		if (y >= img->used_h)
		{
			I_Debugf("TGA_Load: uncompressed too much!\n");
			return false;
		}

		int w = img->used_w - x;

		if (w > total) w = total;
		if (w > 256)   w = 256;

		SYS_ASSERT(w > 0);
		SYS_ASSERT(x + w <= img->used_w);

		u8_t *dest = img->PixelAt(x, y);

		if (w != (int)f->Read(buffer, w))
			return false;

		// FIXME: make sure color indices are in range!

		for (int p=0; p < w; p++, dest += img->bpp)
			memcpy(dest, palette + (buffer[p] * 4), img->bpp);

		x     += w;
		total -= w;

		if (x >= img->used_w)
		{
			x  = 0;
			y += 1;
		}
	}

	return true;
}


static bool DuplicatePixels(image_data_c *img, int& x, int& y, int total,
			const u8_t *src)
{
	while (total > 0)
	{
		if (y >= img->used_h)
		{
			I_Debugf("TGA_Load: uncompressed too much!\n");
			return false;
		}

		int w = img->used_w - x;

		if (w > total)
			w = total;
		
		SYS_ASSERT(w > 0);
		SYS_ASSERT(x + w <= img->used_w);

		u8_t *dest  = img->PixelAt(x, y);
		u8_t *d_end = dest + img->bpp * w;

		for (; dest < d_end; dest += img->bpp)
		{
			memcpy(dest, src, img->bpp);
		}

		x     += w;
		total -= w;

		if (x >= img->used_w)
		{
			x  = 0;
			y += 1;
		}
	}

	return true;
}


static bool DecodeRGB(image_data_c *img, file_c *f, tga_header_t& header)
{
	int x = 0;
	int y = 0;

	return ReadPixels(img, f, x, y, img->used_w * img->used_h);
}

static bool DecodeRLE_RGB(image_data_c *img, file_c *f, tga_header_t& header)
{
	int x = 0;
	int y = 0;

	while (y < img->used_h)
	{
		/* read the next "packet" */

		u8_t count;

		if (1 != f->Read(&count, 1))
			return false;

		if ((count & 0x80) == 0) // raw packet
		{
			if (! ReadPixels(img, f, x, y, count + 1))
				return false;
		}
		else // run-length packet
		{
			const u8_t *src = img->PixelAt(x, y);

			if (! ReadPixels(img, f, x, y, 1))
				return false;

			if (! DuplicatePixels(img, x, y, count & 0x7F, src))
				return false;
		}
	}

	return true;
}


static u8_t * ReadPalette(file_c *f, tga_header_t &header)
{
	int num_colors = header.cmap_len[1] * 256 + header.cmap_len[0];

	if (num_colors <= 0 || num_colors > 256)
		I_Error("TGA_Load: illegal number of colors in image: %d\n", num_colors);

	if (header.pixel_bits != 8)
		I_Error("TGA_Load: %d bit color-mapped pixels not supported.\n", (int)header.pixel_bits);

	u8_t *palette = new u8_t[num_colors * 4];

	memset(palette, 0xFF, num_colors * 4);

	int bpp = (header.cmap_bits == 32) ? 4 : 3;

	for (int i=0; i < num_colors; i++)
	{
		if (bpp != (int)f->Read(&palette[i*4], bpp))
		{
			I_Debugf("TGA_Load: error reading palette.\n");
			return NULL;
		}
	}

#if (SWAP_FOR_INDEXED)
	SwapPixels(palette, bpp, num_colors);
#endif

	return palette;
}


static bool DecodeIndexed(image_data_c *img, file_c *f, tga_header_t& header)
{
	u8_t *palette = ReadPalette(f, header);
	if (! palette)
		return false;

	int x = 0;
	int y = 0;

	return ReadPixels_ColMap(img, f, x, y, img->used_w * img->used_h, palette);
}

static bool DecodeRLE_Indexed(image_data_c *img, file_c *f, tga_header_t& header)
{
	u8_t *palette = ReadPalette(f, header);
	if (! palette)
		return false;

	int x = 0;
	int y = 0;

	while (y < img->used_h)
	{
		/* read the next "packet" */

		u8_t count;

		if (1 != f->Read(&count, 1))
			return false;

		if ((count & 0x80) == 0) // raw packet
		{
			if (! ReadPixels_ColMap(img, f, x, y, count + 1, palette))
				return false;
		}
		else // run-length packet
		{
			const u8_t *src = img->PixelAt(x, y);

			if (! ReadPixels_ColMap(img, f, x, y, 1, palette))
				return false;

			if (! DuplicatePixels(img, x, y, count & 0x7F, src))
				return false;
		}
	}

	return true;
}


image_data_c *TGA_Load(file_c *f, int read_flags)
{
	tga_header_t header;

	size_t nbytes = f->Read((u8_t*) &header, sizeof(header));

	if (nbytes < sizeof(header))
		return NULL;

	if (header.type != TGA_TYPE_INDEXED &&
        header.type != TGA_TYPE_RGB &&
	    header.type != TGA_TYPE_RLE_INDEXED &&
		header.type != TGA_TYPE_RLE_RGB)
		return NULL;

	// FIXME: awww, this is heavy handed!
	if (header.type == TGA_TYPE_INDEXED ||
		header.type == TGA_TYPE_RLE_INDEXED)
	{
		if (header.cmap_bits < 24)
			I_Error("TGA_Load: 16 bpp color-mapped formats not yet supported.\n");
	}
	else
	{
		if (header.pixel_bits < 24)
			I_Error("TGA_Load: 16 bpp RGB formats not yet supported.\n");
	}

	int width  = GET_U16_FIELD(header, width);
	int height = GET_U16_FIELD(header, height);

	if (width  < 4 || width  > 4096 || 
		height < 4 || height > 4096)
		return NULL;

	int tot_W = width;
	int tot_H = height;

	if (read_flags & IRF_Round_POW2)
	{
		tot_W = 1; while (tot_W < (int)width)  tot_W <<= 1;
		tot_H = 1; while (tot_H < (int)height) tot_H <<= 1;
	}


	// skip the info field
	if (header.info_len > 0)
	{
		if (! f->Seek(header.info_len, file_c::SEEKPOINT_CURRENT))
			return NULL;
	}

	int new_bpp;
	if (header.type == TGA_TYPE_INDEXED || header.type == TGA_TYPE_RLE_INDEXED)
		new_bpp = (header.cmap_bits == 32) ? 4 : 3;
	else
		new_bpp = (header.pixel_bits == 32) ? 4 : 3;

#if 1 // DEBUG
	fprintf(stderr, "WIDTH  = %d (%d)\n", width, tot_W);
	fprintf(stderr, "HEIGHT = %d (%d)\n", height, tot_H);
	fprintf(stderr, "BPP = %d\n", new_bpp);
#endif

	image_data_c *img = new image_data_c(tot_W, tot_H, new_bpp);

	img->used_w = width;
	img->used_h = height;

	if (width != tot_W || height != tot_H)
		img->Clear();

	/* read image pixels */

	bool result;

	switch (header.type)
	{
		case TGA_TYPE_RLE_RGB:
			result = DecodeRLE_RGB(img, f, header);
			break;

		case TGA_TYPE_RGB:
			result = DecodeRGB(img, f, header);
			break;

		case TGA_TYPE_INDEXED:
			result = DecodeIndexed(img, f, header);
			break;

		case TGA_TYPE_RLE_INDEXED:
			result = DecodeRLE_Indexed(img, f, header);
			break;

		default:
			I_Error("TGA_Load: INTERNAL ERROR (type?)\n");
			return NULL; /* NOT REACHED */
	}

	if (! result)
		I_Printf("TGA_Load: WARNING: failure decoding image!\n");

	if (header.flags & TGA_FLAG_TOP_DOWN)
		img->Invert();

	return img;
}

bool TGA_GetInfo(file_c *f, int *width, int *height, bool *solid)
{
	tga_header_t header;

	size_t nbytes = f->Read((u8_t*) &header, sizeof(header));

	if (nbytes < sizeof(header))
		return false;

	*width  = GET_U16_FIELD(header, width);
	*height = GET_U16_FIELD(header, height);

	if (header.type == TGA_TYPE_INDEXED || header.type == TGA_TYPE_RLE_INDEXED)
		*solid = (header.cmap_bits != 32); 
	else
		*solid = (header.pixel_bits != 32);

	// *x_offset = GET_S16_FIELD(header, x_origin);
	// *y_offset = GET_S16_FIELD(header, y_origin);

	return true;
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
