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

#pragma once

#include <unordered_map>

#include "epi_file.h"
#include "im_data.h"

enum ImageFormat
{
    kImageUnknown = 0,
    kImagePNG,
    kImageTGA,
    kImageJPEG,
    kImageDoom,
    kImageOther // e.g. gif, dds, bmp
};

class ImageAtlasRectangle
{
  public:
    // Normalized atlas x/y/width/height for texcoords
    float texture_coordinate_x;
    float texture_coordinate_y;
    float texture_coordinate_width;
    float texture_coordinate_height;
    // Actual sub-image information
    short image_width;
    short image_height;
    float offset_x;
    float offset_y;
};

class ImageAtlas
{
  public:
    ImageData                                   *data_;
    std::unordered_map<int, ImageAtlasRectangle> rectangles_;

  public:
    ImageAtlas(int w, int h);
    ~ImageAtlas();
};

// determine image format from the first 32 bytes (or so) of the file.
// the file_size is the total size of the file or lump, and helps to
// distinguish DOOM patch format from other things.
ImageFormat DetectImageFormat(uint8_t *header, int header_lengh, int file_size);

// determine image format from the filename (by its extension).
ImageFormat ImageFormatFromFilename(const std::string &filename);

// loads the given image, which must be PNG, TGA or JPEG format.
// Returns nullptr if something went wrong.  The result image will be RGB
// or RGBA (never paletted).  The image size (width and height) will be
// rounded to the next power-of-two.
ImageData *LoadImageData(epi::File *file);

// given a collection of loaded images, pack and return the image data
// for an atlas containing all of them. Does not assume that the incoming
// data pointers should be deleted/freed. Images at a BPP of 3 will be
// converted to BPP 4 with 255 for their pixel alpha values.
// The integer keys for the associated image data can vary based on need, but
// are generally a way of tracking which part of the atlas you are trying to
// retrieve
ImageAtlas *PackImages(const std::unordered_map<int, ImageData *> &image_pack_data);

// reads the principle information from the image header.
// (should be much faster than loading the whole image).
// The image must be PNG, TGA or JPEG format, it cannot be used
// with DOOM patches.  Returns false if something went wrong.
//
// NOTE: size returned here is the real size, and may be different
// from the image returned by Load() which rounds to power-of-two.
bool GetImageInfo(epi::File *file, int *width, int *height, int *depth);

// saves the image (in PNG format) to the given file.
// Returns false if failed to save (e.g. file already exists).
// The image _MUST_ be RGB or RGBA.
bool SavePNG(std::string filename, ImageData *image);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
