//----------------------------------------------------------------------------
//  EDGE OpenGL Texture Upload
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2009  The EDGE Team.
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
//----------------------------------------------------------------------------

#ifndef __RGL_TEXGL_H__
#define __RGL_TEXGL_H__

#include "image_data.h"

typedef enum
{
	UPL_NONE = 0,

	UPL_Smooth   = (1 << 0),
	UPL_Clamp    = (1 << 1),
	UPL_MipMap   = (1 << 2),
	UPL_Thresh   = (1 << 3), // threshhold alpha (to 0 or 255)
}
upload_texture_flag_e;

GLuint R_UploadTexture(epi::image_data_c *img,
		 int flags = UPL_NONE, int max_pix = (1<<30));

epi::image_data_c *R_PalettisedToRGB(epi::image_data_c *src,
									 const byte *palette, int opacity);

void R_PaletteRemapRGBA(epi::image_data_c *img,
		const byte *new_pal, const byte *old_pal);

int R_DetermineOpacity(epi::image_data_c *img, bool *is_empty);

void R_BlackenClearAreas(epi::image_data_c *img);

void R_DumpImage(epi::image_data_c *img);

#endif /* __RGL_TEXGL_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
