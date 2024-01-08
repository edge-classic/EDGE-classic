//----------------------------------------------------------------------------
//  EDGE Video Context
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
//
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------

#ifndef __R_DRAW_H__
#define __R_DRAW_H__

#include "main.h"
#include "r_image.h"

// Move to somewhere appropriate later -ACB- 2004/08/19
void RGL_DrawImage(float x, float y, float w, float h, const image_c *image, float tx1, float ty1, float tx2, float ty2,
                   const colourmap_c *textmap = NULL, float alpha = 1.0f, const colourmap_c *palremap = NULL);

void RGL_ReadScreen(int x, int y, int w, int h, uint8_t *rgb_buffer);

// This routine should inform the lower level system(s) that the
// screen has changed size/depth.  New size/depth is given.  Must be
// called before any rendering has occurred (e.g. just before
// I_StartFrame).
void RGL_NewScreenSize(int width, int height, int bits);

#endif /* __R_DRAW_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
