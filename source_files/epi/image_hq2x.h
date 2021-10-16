//------------------------------------------------------------------------
//  HQ2X : High-Quality 2x Graphics Resizing
//------------------------------------------------------------------------
// 
//  Copyright (c) 2007  The EDGE Team.
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
//  Based heavily on the code (C) 2003 Maxim Stepin, which is
//  under the GNU LGPL (Lesser General Public License).
//
//  For more information, see: http://hiend3d.com/hq2x.html
//
//------------------------------------------------------------------------

#ifndef __EPI_IMAGE_HQ2X_H__
#define __EPI_IMAGE_HQ2X_H__

#include "image_data.h"

namespace epi
{
	namespace Hq2x
	{
		/* ------ Functions ------------------------------------- */

		void Setup(const byte *palette, int trans_pixel);
		// initialises look-up tables based on the given palette.
		// The 'trans_pixel' gives a pixel index which is fully
		// transparent, or none when -1.

		image_data_c *Convert(image_data_c *img, bool solid, bool invert = false);
		// converts a single palettised image into an RGB or RGBA
		// image (depending on the solid parameter).  The Setup()
		// method must be called sometime prior to calling this
		// function, and this determines the palette of the input
		// image.
	}

}  // namespace epi

#endif  /* __EPI_IMAGE_HQ2X_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
