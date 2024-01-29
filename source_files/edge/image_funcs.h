//------------------------------------------------------------------------
//  Image Handling
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

#ifndef __EPI_IMAGE_FUNCS_H__
#define __EPI_IMAGE_FUNCS_H__

#include "file.h"
#include "image_data.h"
#include <unordered_map>

enum ImageFormat
{
    kUnknownImage = 0,
    kPNGImage,
    kTGAImage,
    kJPEGImage,
    kDoomImage,
    kOtherImage // e.g. gif, dds, bmp
};

class image_rect_c
{
    public:
    // Normalized atlas x/y/width/height for texcoords
        float tx;
        float ty;
        float tw;
        float th;
    // Actual sub-image information
        short iw;
        short ih;
        float off_x;
        float off_y;
};

class image_atlas_c
{
	public:
		image_data_c *data;
		std::unordered_map<int, image_rect_c> rects;

  public:
    image_atlas_c(int _w, int _h);
    ~image_atlas_c();
};

// determine image format from the first 32 bytes (or so) of the file.
// the file_size is the total size of the file or lump, and helps to
// distinguish DOOM patch format from other things.
ImageFormat Image_DetectFormat(uint8_t *header, int header_len, int file_size);

// determine image format from the filename (by its extension).
ImageFormat Image_FilenameToFormat(const std::string &filename);

// loads the given image, which must be PNG, TGA or JPEG format.
// Returns NULL if something went wrong.  The result image will be RGB
// or RGBA (never paletted).  The image size (width and height) will be
// rounded to the next power-of-two.
image_data_c *Image_Load(epi::file_c *f);

// given a collection of loaded images, pack and return the image data
// for an atlas containing all of them. Does not assume that the incoming
// data pointers should be deleted/freed. Images at a BPP of 3 will be
// converted to BPP 4 with 255 for their pixel alpha values.
// The integer keys for the associated image data can vary based on need, but are
// generally a way of tracking which part of the atlas you are trying to retrieve
image_atlas_c *Image_Pack(const std::unordered_map<int, image_data_c *> &im_pack_data);

// reads the principle information from the image header.
// (should be much faster than loading the whole image).
// The image must be PNG, TGA or JPEG format, it cannot be used
// with DOOM patches.  Returns false if something went wrong.
//
// NOTE: size returned here is the real size, and may be different
// from the image returned by Load() which rounds to power-of-two.
bool Image_GetInfo(epi::file_c *f, int *width, int *height, int *bpp);

// saves the image (in JPEG format) to the given file.  Returns false if
// something went wrong.  The image _MUST_ be RGB (bpp == 3).
bool JPEG_Save(std::string fn, image_data_c *img);

// saves the image (in PNG format) to the given file.
// Returns false if failed to save (e.g. file already exists).
// The image _MUST_ be RGB or RGBA.
bool PNG_Save(std::string fn, image_data_c *img);

#endif /* __EPI_IMAGE_JPEG_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
