//------------------------------------------------------------------------
//  BLUR: Fast Gaussian Blurring
//------------------------------------------------------------------------
// 
//  Copyright (c) 2023  The EDGE Team.
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

#include "epi.h"
#include "image_blur.h"
#include <cmath>
#include <cstring>

namespace epi
{
namespace Blur
{

void std_to_box(int boxes[], float sigma, int n)  
{
    // ideal filter width
    float wi = std::sqrt((12*sigma*sigma/n)+1); 
    int wl = std::floor(wi);  
    if(wl%2==0) wl--;
    int wu = wl+2;
                
    float mi = (12*sigma*sigma - n*wl*wl - 4*n*wl - 3*n)/(-4*wl - 4);
    int m = std::round(mi);
                
    for(int i=0; i<n; i++) 
        boxes[i] = ((i < m ? wl : wu) - 1) / 2;
}

void horizontal_blur_rgb(u8_t * in, u8_t * out, int w, int h, int c, int r) 
{
    float iarr = 1.f / (r+r+1);
    #pragma omp parallel for
    for(int i=0; i<h; i++) 
    {
        int ti = i*w; 
        int li = ti;  
        int ri = ti+r;

        int fv[3] = { in[ti*c+0], in[ti*c+1], in[ti*c+2] };                  
        int lv[3] = { in[(ti+w-1)*c+0], in[(ti+w-1)*c+1], in[(ti+w-1)*c+2] };
        int val[3] = { (r+1)*fv[0], (r+1)*fv[1], (r+1)*fv[2] };              

        for(int j=0; j<r; j++) 
        { 
            val[0] += in[(ti+j)*c+0]; 
            val[1] += in[(ti+j)*c+1]; 
            val[2] += in[(ti+j)*c+2]; 
        }

        for(int j=0; j<=r; j++, ri++, ti++) 
        { 
            val[0] += in[ri*c+0] - fv[0]; 
            val[1] += in[ri*c+1] - fv[1]; 
            val[2] += in[ri*c+2] - fv[2]; 
            out[ti*c+0] = std::round(val[0]*iarr); 
            out[ti*c+1] = std::round(val[1]*iarr); 
            out[ti*c+2] = std::round(val[2]*iarr); 
        }

        for(int j=r+1; j<w-r; j++, ri++, ti++, li++) 
        { 
            val[0] += in[ri*c+0] - in[li*c+0]; 
            val[1] += in[ri*c+1] - in[li*c+1]; 
            val[2] += in[ri*c+2] - in[li*c+2]; 
            out[ti*c+0] = std::round(val[0]*iarr); 
            out[ti*c+1] = std::round(val[1]*iarr); 
            out[ti*c+2] = std::round(val[2]*iarr); 
        }

        for(int j=w-r; j<w; j++, ti++, li++) 
        { 
            val[0] += lv[0] - in[li*c+0]; 
            val[1] += lv[1] - in[li*c+1]; 
            val[2] += lv[2] - in[li*c+2]; 
            out[ti*c+0] = std::round(val[0]*iarr); 
            out[ti*c+1] = std::round(val[1]*iarr); 
            out[ti*c+2] = std::round(val[2]*iarr); 
        }
    }
}

void total_blur_rgb(u8_t * in, u8_t * out, int w, int h, int c, int r) 
{
    // radius range on either side of a pixel + the pixel itself
    float iarr = 1.f / (r+r+1);
    #pragma omp parallel for
    for(int i=0; i<w; i++) 
    {
        int ti = i;
        int li = ti;
        int ri = ti+r*w;

        int fv[3] = {in[ti*c+0], in[ti*c+1], in[ti*c+2] };
        int lv[3] = {in[(ti+w*(h-1))*c+0], in[(ti+w*(h-1))*c+1], in[(ti+w*(h-1))*c+2] };
        int val[3] = {(r+1)*fv[0], (r+1)*fv[1], (r+1)*fv[2] };

        for(int j=0; j<r; j++) 
        { 
            val[0] += in[(ti+j*w)*c+0]; 
            val[1] += in[(ti+j*w)*c+1]; 
            val[2] += in[(ti+j*w)*c+2]; 
        }

        for(int j=0; j<=r; j++, ri+=w, ti+=w) 
        { 
            val[0] += in[ri*c+0] - fv[0]; 
            val[1] += in[ri*c+1] - fv[1]; 
            val[2] += in[ri*c+2] - fv[2]; 
            out[ti*c+0] = std::round(val[0]*iarr); 
            out[ti*c+1] = std::round(val[1]*iarr); 
            out[ti*c+2] = std::round(val[2]*iarr); 
        }

        for(int j=r+1; j<h-r; j++, ri+=w, ti+=w, li+=w) 
        { 
            val[0] += in[ri*c+0] - in[li*c+0]; 
            val[1] += in[ri*c+1] - in[li*c+1]; 
            val[2] += in[ri*c+2] - in[li*c+2]; 
            out[ti*c+0] = std::round(val[0]*iarr); 
            out[ti*c+1] = std::round(val[1]*iarr); 
            out[ti*c+2] = std::round(val[2]*iarr); 
        }
        
        for(int j=h-r; j<h; j++, ti+=w, li+=w) 
        { 
            val[0] += lv[0] - in[li*c+0]; 
            val[1] += lv[1] - in[li*c+1]; 
            val[2] += lv[2] - in[li*c+2]; 
            out[ti*c+0] = std::round(val[0]*iarr); 
            out[ti*c+1] = std::round(val[1]*iarr); 
            out[ti*c+2] = std::round(val[2]*iarr); 
        }
    }
}

void box_blur_rgb(u8_t *& in, u8_t *& out, int w, int h, int c, int r) 
{
    std::swap(in, out);
    horizontal_blur_rgb(out, in, w, h, c, r);
    total_blur_rgb(in, out, w, h, c, r);
    // Note to myself : 
    // here we could go anisotropic with different radiis rx,ry in HBlur and TBlur
}

image_data_c *Blur(image_data_c *img, float sigma)
{
	SYS_ASSERT(img->bpp >= 3);

	int w = img->width;
	int h = img->height;
	int c = img->bpp;

	image_data_c *result = new image_data_c(w, h, c);

    int box;
    std_to_box(&box, sigma, 1);
    box_blur_rgb(img->pixels, result->pixels, w, h, c, box);

	return result;
}

}  // namespace Blur
}  // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
