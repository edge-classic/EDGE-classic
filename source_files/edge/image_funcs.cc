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

#include "epi.h"
#include "filesystem.h"
#include "image_funcs.h"
#include "str_util.h"

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"

#define STBI_ONLY_PNG
#define STBI_ONLY_TGA
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STBI_WRITE_NO_STDIO
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

image_atlas_c::image_atlas_c(int _w, int _h)
{
	data = new image_data_c(_w, _h, 4);
	memset(data->pixels, 0, _w * _h * 4);
}

image_atlas_c::~image_atlas_c()
{
	delete data;
	data = nullptr;
}

ImageFormat Image_DetectFormat(uint8_t *header, int header_len, int file_size)
{
    // AJA 2022: based on code I wrote for Eureka...

    if (header_len < 12)
        return kUnknownImage;

    // PNG is clearly marked in the header, so check it first.

    if (header[0] == 0x89 && header[1] == 'P' && header[2] == 'N' && header[3] == 'G' && header[4] == 0x0D &&
        header[5] == 0x0A)
    {
        return kPNGImage;
    }

    // check some other common image formats....

    if (header[0] == 0xFF && header[1] == 0xD8 && header[2] == 0xFF && header[3] >= 0xE0 &&
        ((header[6] == 'J' && header[7] == 'F') || (header[6] == 'E' && header[7] == 'x')))
    {
        return kJPEGImage;
    }

    if (header[0] == 'G' && header[1] == 'I' && header[2] == 'F' && header[3] == '8' && header[4] >= '7' &&
        header[4] <= '9' && header[5] == 'a')
    {
        return kOtherImage; /* GIF */
    }

    if (header[0] == 'D' && header[1] == 'D' && header[2] == 'S' && header[3] == 0x20 && header[4] == 124 &&
        header[5] == 0 && header[6] == 0)
    {
        return kOtherImage; /* DDS (DirectDraw Surface) */
    }

    // TGA (Targa) is not clearly marked, but better than Doom patches,
    // so check it next.

    if (header_len >= 18)
    {
        int width  = (int)header[12] + ((int)header[13] << 8);
        int height = (int)header[14] + ((int)header[15] << 8);

        uint8_t cmap_type = header[1];
        uint8_t img_type  = header[2];
        uint8_t depth     = header[16];

        if (width > 0 && width <= 2048 && height > 0 && height <= 2048 && (cmap_type == 0 || cmap_type == 1) &&
            ((img_type | 8) >= 8 && (img_type | 8) <= 11) &&
            (depth == 8 || depth == 15 || depth == 16 || depth == 24 || depth == 32))
        {
            return kTGAImage;
        }
    }

    // check for DOOM patches last

    {
        int width  = (int)header[0] + (int)(header[1] << 8);
        int height = (int)header[2] + (int)(header[3] << 8);

        int ofs_x = (int)header[4] + (int)((signed char)header[5] * 256);
        int ofs_y = (int)header[6] + (int)((signed char)header[7] * 256);

        if (width > 0 && width <= 4096 && abs(ofs_x) <= 4096 && height > 0 && height <= 1024 && abs(ofs_y) <= 4096 &&
            file_size > width * 4 /* columnofs */)
        {
            return kDoomImage;
        }
    }

    return kUnknownImage; // uh oh!
}

ImageFormat Image_FilenameToFormat(const std::string &filename)
{
    std::string ext = epi::GetExtension(filename);

    epi::StringLowerASCII(ext);

    if (ext == ".png")
        return kPNGImage;

    if (ext == ".tga")
        return kTGAImage;

    if (ext == ".jpg" || ext == ".jpeg")
        return kJPEGImage;

    if (ext == ".lmp") // Kind of a gamble, but whatever
        return kDoomImage;

    if (ext == ".gif" || ext == ".bmp" || ext == ".dds")
        return kOtherImage;

    return kUnknownImage;
}

image_data_c *Image_Load(epi::File *f)
{
    int width  = 0;
    int height = 0;
    int bpp    = 0;

    int   length    = f->GetLength();
    uint8_t *raw_image = f->LoadIntoMemory();

    uint8_t *decoded_img = stbi_load_from_memory(raw_image, length, &width, &height, &bpp, 0);

    // we don't want no grayscale here, force STB to convert
    if (decoded_img != NULL && (bpp == 1 || bpp == 2))
    {
        stbi_image_free(decoded_img);

        // bpp 1 = grayscale, so force RGB
        // bpp 2 = grayscale + alpha, so force RGBA
        int new_bpp = bpp + 2;

        decoded_img = stbi_load_from_memory(raw_image, length, &width, &height, &bpp, new_bpp);

        bpp = new_bpp; // sigh...
    }

    delete[] raw_image;

    if (decoded_img == NULL)
        return NULL;

    int total_w = width;
    int total_h = height;

    // round size up to the nearest power-of-two
    if (true)
    {
        total_w = 1;
        while (total_w < (int)width)
            total_w <<= 1;
        total_h = 1;
        while (total_h < (int)height)
            total_h <<= 1;
    }

    image_data_c *img = new image_data_c(total_w, total_h, bpp);

    img->used_w = width;
    img->used_h = height;

    if (img->used_w != total_w || img->used_h != total_h)
        img->Clear();

    // copy the image data, inverting it at the same time
    for (int y = 0; y < height; y++)
    {
        const uint8_t *source = &decoded_img[(height - 1 - y) * width * bpp];
        memcpy(img->PixelAt(0, y), source, width * bpp);
    }

    stbi_image_free(decoded_img);

    return img;
}

image_atlas_c *Image_Pack(const std::unordered_map<int, image_data_c *> &im_pack_data)
{
	stbrp_node nodes[4096]; // Max OpenGL texture width we allow
	std::vector<stbrp_rect> rects;
	// These should only grow up to the minimum coverage, which is hopefully less than
	// 4096 since stb_rect_pack indicates the number of nodes should be higher
	// than the actual width for best results
	int atlas_w = 1;
	int atlas_h = 1;
	for (auto im : im_pack_data)
	{
		SYS_ASSERT(im.second->bpp >= 3);
		if (im.second->bpp == 3)
			im.second->SetAlpha(255);
		stbrp_rect rect;
		rect.id = im.first;
		rect.w = im.second->used_w + 2;
		rect.h = im.second->used_h + 2;
		if (rect.w > atlas_w)
		{
			atlas_w = 1; 
			while (atlas_w < (int)rect.w)  atlas_w <<= 1;
		}
		if (rect.h > atlas_h)
		{
			atlas_h = 1; 
			while (atlas_h < (int)rect.h)  atlas_h <<= 1;
		}
		rects.push_back(rect);	
	}
	if (atlas_h < atlas_w)
		atlas_h = atlas_w;
	stbrp_context ctx;
  	stbrp_init_target(&ctx, atlas_w, atlas_h, nodes, 4096);
	int packres = stbrp_pack_rects(&ctx, rects.data(), rects.size());
	while (packres != 1)
	{
		atlas_w *= 2;
		if (atlas_h < atlas_w)
			atlas_h = atlas_w;
		if (atlas_w > 4096 || atlas_h > 4096)
			I_Error("Image_Pack: Atlas exceeds maximum allowed texture size (4096x4096)!");
		stbrp_init_target(&ctx, atlas_w, atlas_h, nodes, 4096);
		packres = stbrp_pack_rects(&ctx, rects.data(), rects.size());
	}
	image_atlas_c *atlas = new image_atlas_c(atlas_w, atlas_h);
	// fill atlas image_data_c
	for (size_t i = 0; i < rects.size(); i++)
	{
		int rect_x = rects[i].x+1;
		int rect_y = rects[i].y+1;
		image_data_c *im = im_pack_data.at(rects[i].id);
		for (short x = 0; x < im->used_w; x++)
		{
			for (short y = 0; y < im->used_h; y++)
			{
				memcpy(atlas->data->PixelAt(rect_x + x, rect_y + y), im->PixelAt(x, y), 4);
			}
		}
        image_rect_c atlas_rect;
		atlas_rect.tx = (float)rect_x / atlas_w;
		atlas_rect.ty = (float)rect_y / atlas_h;
		atlas_rect.tw = (float)im->used_w / atlas_w;
		atlas_rect.th = (float)im->used_h / atlas_h;
        atlas_rect.iw = im->used_w * im->scale_x;
        atlas_rect.ih = im->used_h * im->scale_y;
        atlas_rect.off_x = im->offset_x;
        atlas_rect.off_y = im->offset_y;
		atlas->rects.try_emplace(rects[i].id, atlas_rect);
	}
	return atlas;
}

bool Image_GetInfo(epi::File *f, int *width, int *height, int *bpp)
{
    int   length    = f->GetLength();
    uint8_t *raw_image = f->LoadIntoMemory();

    int result = stbi_info_from_memory(raw_image, length, width, height, bpp);

    delete[] raw_image;

    return result != 0;
}

//------------------------------------------------------------------------

static void stbi_file_c_write(void *context, void *data, int size)
{
    SYS_ASSERT(context && data && size);
    epi::File *dest = (epi::File *)context;
    dest->Write(data, size);
}

bool JPEG_Save(std::string fn, image_data_c *img)
{
    SYS_ASSERT(img->bpp == 3);

    epi::File *dest = epi::FileOpen(fn, epi::kFileAccessBinary | epi::kFileAccessWrite);

    if (!dest) return false;

    // zero means failure here
    int result = stbi_write_jpg_to_func(stbi_file_c_write, dest, img->used_w, img->used_h, img->bpp, img->pixels, 95);
    
    delete dest;

    if (result == 0)
    {
        epi::FileDelete(fn);
        return false;
    }
    else
        return true;
}

bool PNG_Save(std::string fn, image_data_c *img)
{
    SYS_ASSERT(img->bpp >= 3);

    epi::File *dest = epi::FileOpen(fn, epi::kFileAccessBinary | epi::kFileAccessWrite);

    if (!dest) return false;

    // zero means failure here
    int result = stbi_write_png_to_func(stbi_file_c_write, dest, img->used_w, img->used_h, img->bpp, img->pixels, 0);
    
    delete dest;

    if (result == 0)
    {
        epi::FileDelete(fn);
        return false;
    }
    else
        return true;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
