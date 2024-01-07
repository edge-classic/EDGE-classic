//------------------------------------------------------------------------
//  BLUR: Fast Gaussian Blurring
//------------------------------------------------------------------------
//
//  Copyright (c) 2023-2024 The EDGE Team.
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
//------------------------------------------------------------------------
//  Based on "C++ implementation of a fast Gaussian blur algorithm by
//    Ivan Kutskir - Integer Version"
//
//  Copyright (C) 2017 Basile Fraboni
//  Copyright (C) 2014 Ivan Kutskir
//  All Rights Reserved
//  You may use, distribute and modify this code under the
//  terms of the MIT license. For further details please refer
//  to : https://mit-license.org/
//
//------------------------------------------------------------------------

#ifndef __EPI_IMAGE_BLUR_H__
#define __EPI_IMAGE_BLUR_H__

#include "image_data.h"

namespace epi
{
namespace Blur
{
/* ------ Functions ------------------------------------- */

image_data_c *Blur(image_data_c *img, float sigma);
} // namespace Blur

} // namespace epi

#endif /* __EPI_IMAGE_BLUR_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
