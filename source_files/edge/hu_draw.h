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

#pragma once

#include "hu_font.h"

// X coordinates of left and right edges of screen.
// updated by calls to HudSetCoordinateSystem() or HudReset().
extern float                    hud_x_left;
extern float                    hud_x_right;
extern float                    hud_x_middle;
extern float                    hud_visible_top;
extern float                    hud_visible_bottom;
extern std::vector<std::string> hud_overlays;

void HudSetCoordinateSystem(int width, int height);

void  HudSetFont(Font *font = nullptr);
void  HudSetScale(float scale = 1.0f);
void  HudSetTextColor(RGBAColor color = kRGBANoValue);
void  HudSetAlpha(float alpha = 1.0f);
float HudGetAlpha(void);

// xa is -1 for left, 0 for centred, +1 for right
// ya is -1 for top,  0 for centred, +! for bottom
void HudSetAlignment(int xa = -1, int ya = -1);

// resets the coord sys to 320x200, and resets all properties
void HudReset();

void HudFrameSetup(void);

// manage the current clip rectangle.  The first push enables the
// scissor test, subsequent pushes merely shrink the area, and the
// last pop disables the scissor test.
void HudPushScissor(float x1, float y1, float x2, float y2, bool expand = false);
void HudPopScissor();

void HudRawImage(float hx1, float hy1, float hx2, float hy2, const Image *image, float tx1, float ty1, float tx2,
                 float ty2, float alpha = 1.0f, RGBAColor text_col = kRGBANoValue, const Colormap *palremap = nullptr,
                 float sx = 0.0, float sy = 0.0, char ch = -1);

// Draw a solid colour box (possibly translucent) in the given
// rectangle.
void HudSolidBox(float x1, float y1, float x2, float y2, RGBAColor col);

// Draw a solid colour line (possibly translucent) between the two
// end points.  Coordinates are inclusive.  Drawing will be clipped
// to the current scissor rectangle.  The dx/dy fields are used by
// the automap code to reduce the wobblies.
void HudSolidLine(float x1, float y1, float x2, float y2, RGBAColor col, float thickness = 1, bool smooth = true,
                  float dx = 0, float dy = 0);

// Draw a thin outline of a box.
void HudThinBox(float x1, float y1, float x2, float y2, RGBAColor col, float thickness = 0.0f);

// Like HudSolidBox but the colors of each corner (TL, BL, TR, BR) can
// be specified individually.
void HudGradientBox(float x1, float y1, float x2, float y2, RGBAColor *cols);

void HudDrawImage(float x, float y, const Image *image, const Colormap *colmap = nullptr);
void HudDrawImageNoOffset(float x, float y, const Image *image);
void HudScrollImage(float x, float y, const Image *image, float sx, float sy);
void HudScrollImageNoOffset(float x, float y, const Image *image, float sx, float sy);
void HudDrawImageTitleWS(const Image *image);
void HudStretchImage(float x, float y, float w, float h, const Image *image, float sx, float sy,
                     const Colormap *colmap = nullptr);
void HudStretchImageNoOffset(float x, float y, float w, float h, const Image *image, float sx, float sy);
void HudTileImage(float x, float y, float w, float h, const Image *image, float offset_x = 0.0f, float offset_y = 0.0f);

// Functions for when we want to draw without having an image_c
void HudStretchFromImageData(float x, float y, float w, float h, const ImageData *img, unsigned int tex_id,
                             ImageOpacity opacity);

extern int hud_tic;

float HudFontWidth(void);
float HudFontHeight(void);

float HudStringWidth(const char *str);
float HudStringHeight(const char *str);

void HudDrawChar(float left_x, float top_y, const Image *img, char ch, float size = 0);

// draw a text string with the current font, current color (etc).
void HudDrawText(float x, float y, const char *str, float size = 0);

// Draw the ENDOOM/Quit screen
void HudDrawQuitScreen();

// render a view of the world using the given camera object.
void HudRenderWorld(float x, float y, float w, float h, MapObject *camera, int flags);

// render the automap
void HudRenderAutomap(float x, float y, float w, float h, MapObject *focus, int flags);

void HudGetCastPosition(float *x, float *y, float *scale_x, float *scale_y);

float HudGetImageWidth(const Image *img);
float HudGetImageHeight(const Image *img);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
