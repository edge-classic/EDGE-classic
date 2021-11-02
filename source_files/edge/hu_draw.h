//----------------------------------------------------------------------------
//  EDGE Video Context
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
//
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------

#ifndef __HUD_DRAW_H__
#define __HUD_DRAW_H__

#include "r_image.h"

// We are referring to patches.
#include "hu_font.h"


void HUD_SetCoordSys(int width, int height);

void HUD_SetFont(font_c *font = NULL);
void HUD_SetScale(float scale = 1.0f);
void HUD_SetTextColor(rgbcol_t color = RGB_NO_VALUE);
void HUD_SetAlpha(float alpha = 1.0f);

void HUD_SetAlignment(int xa = -1, int ya = -1);
// xa is -1 for left, 0 for centred, +1 for right
// ya is -1 for top,  0 for centred, +! for bottom

void HUD_Reset();
// resets the coord sys to 320x200, and resets all properties


void HL_Init(void);

void HUD_FrameSetup(void);


void HUD_PushScissor(float x1, float y1, float x2, float y2, bool expand=false);
void HUD_PopScissor();
// manage the current clip rectangle.  The first push enables the
// scissor test, subsequent pushes merely shrink the area, and the
// last pop disables the scissor test.

bool HUD_ScissorTest(float x1, float y1, float x2, float y2);
// returns true unless the given line/box is completely outside the
// scissor rectangles.


void HUD_RawImage(float hx1, float hy1, float hx2, float hy2,
                  const image_c *image, 
				  float tx1, float ty1, float tx2, float ty2,
				  float alpha = 1.0f, rgbcol_t text_col = RGB_NO_VALUE,
				  const colourmap_c *palremap = NULL);
 
void HUD_SolidBox(float x1, float y1, float x2, float y2, rgbcol_t col);
// Draw a solid colour box (possibly translucent) in the given
// rectangle.

void HUD_SolidLine(float x1, float y1, float x2, float y2, rgbcol_t col,
                   bool thick=false, bool smooth=true, float dx=0, float dy=0);
// Draw a solid colour line (possibly translucent) between the two
// end points.  Coordinates are inclusive.  Drawing will be clipped
// to the current scissor rectangle.  The dx/dy fields are used by
// the automap code to reduce the wobblies.

void HUD_ThinBox(float x1, float y1, float x2, float y2, rgbcol_t col);
// Draw a thin outline of a box.

void HUD_GradientBox(float x1, float y1, float x2, float y2, rgbcol_t *cols);
// Like HUD_SolidBox but the colors of each corner (TL, BL, TR, BR) can
// be specified individually.

void HUD_DrawImage(float x, float y, const image_c *image);
void HUD_DrawImageTitleWS(const image_c *image);
void HUD_StretchImage(float x, float y, float w, float h, const image_c *image);
void HUD_TileImage(float x, float y, float w, float h, const image_c *image,
				   float offset_x = 0.0f, float offset_y = 0.0f);


float HUD_FontWidth(void);
float HUD_FontHeight(void);

float HUD_StringWidth(const char *str);
float HUD_StringHeight(const char *str);

void HUD_DrawChar(float left_x, float top_y, const image_c *img);

void HUD_DrawText(float x, float y, const char *str);
// draw a text string with the current font, current color (etc).


void HUD_RenderWorld(float x1, float y1, float x2, float y2, mobj_t *camera);
// render a view of the world using the given camera object.

void HUD_GetCastPosition(float *x, float *y, float *scale_x, float *scale_y);

#endif /* __HUD_DRAW_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
