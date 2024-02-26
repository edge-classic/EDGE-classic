//------------------------------------------------------------------------
//  EDGE Image Filtering/Scaling
//------------------------------------------------------------------------
//
//  Copyright (c) 2007-2024 The EDGE Team.
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
//  Blur is based on "C++ implementation of a fast Gaussian blur algorithm by
//    Ivan Kutskir - Integer Version"
//
//  Copyright (C) 2017 Basile Fraboni
//  Copyright (C) 2014 Ivan Kutskir
//  All Rights Reserved
//  You may use, distribute and modify this code under the
//  terms of the MIT license. For further details please refer
//  to : https://mit-license.org/
//
//----------------------------------------------------------------------------
//
//  HQ2x is based heavily on the code (C) 2003 Maxim Stepin, which is
//  under the GNU LGPL (Lesser General Public License).
//
//  For more information, see: http://hiend3d.com/hq2x.html
//
//----------------------------------------------------------------------------

#pragma once

#include "im_data.h"

ImageData *ImageBlur(ImageData *image, float sigma);

void Hq2xPaletteSetup(const uint8_t *palette, int transparent_pixel);
// initialises look-up tables based on the given palette.
// The 'trans_pixel' gives a pixel index which is fully
// transparent, or none when -1.

ImageData *ImageHq2x(ImageData *image, bool solid, bool invert = false);
// converts a single palettised image into an RGB or RGBA
// image (depending on the solid parameter).  The Setup()
// method must be called sometime prior to calling this
// function, and this determines the palette of the input
// image.

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
