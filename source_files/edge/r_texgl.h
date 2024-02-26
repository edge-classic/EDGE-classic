//----------------------------------------------------------------------------
//  EDGE OpenGL Texture Upload
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2024 The EDGE Team.
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

#ifndef __RGL_TEXGL_H__
#define __RGL_TEXGL_H__

#include "im_data.h"

typedef enum
{
    UPL_NONE = 0,

    UPL_Smooth = (1 << 0),
    UPL_Clamp  = (1 << 1),
    UPL_MipMap = (1 << 2),
    UPL_Thresh = (1 << 3), // threshhold alpha (to 0 or 255)
} upload_texture_flag_e;

GLuint R_UploadTexture(ImageData *img, int flags = UPL_NONE, int max_pix = (1 << 30));

ImageData *R_PalettisedToRGB(ImageData *src, const uint8_t *palette, int opacity);

void R_PaletteRemapRGBA(ImageData *img, const uint8_t *new_pal, const uint8_t *old_pal);

int R_DetermineOpacity(ImageData *img, bool *is_empty);

void R_BlackenClearAreas(ImageData *img);

void R_DumpImage(ImageData *img);

#endif /* __RGL_TEXGL_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
