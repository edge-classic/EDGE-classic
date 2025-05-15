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
#include "r_units.h"

// X coordinates of left and right edges of screen.
// updated by calls to HUDSetCoordinateSystem() or HUDReset().
extern float                    hud_x_left;
extern float                    hud_x_right;
extern float                    hud_x_middle;
extern float                    hud_visible_top;
extern float                    hud_visible_bottom;
extern std::vector<std::string> hud_overlays;

void  HUDSetCoordinateSystem(int width, int height);
float HUDToRealCoordinatesX(float x);
float HUDToRealCoordinatesY(float y);

void  HUDSetFont(Font *font = nullptr);
void  HUDSetScale(float scale = 1.0f);
void  HUDSetTextColor(RGBAColor color = kRGBANoValue);
void  HUDSetAlpha(float alpha = 1.0f);
float HUDGetAlpha(void);

// xa is -1 for left, 0 for centred, +1 for right
// ya is -1 for top,  0 for centred, +! for bottom
void HUDSetAlignment(int xa = -1, int ya = -1);

// resets the coord sys to 320x200, and resets all properties
void HUDReset();

void HUDFrameSetup(void);

// manage the current clip rectangle.  The first push enables the
// scissor test, subsequent pushes merely shrink the area, and the
// last pop disables the scissor test.
void HUDPushScissor(float x1, float y1, float x2, float y2, bool expand = false);
void HUDPopScissor();

void HUDRawImage(float hx1, float hy1, float hx2, float hy2, const Image *image, float tx1, float ty1, float tx2,
                 float ty2, float alpha = 1.0f, RGBAColor text_col = kRGBANoValue, float sx = 0.0, float sy = 0.0,
                 bool font_draw = false);

// Draw a solid colour box (possibly translucent) in the given
// rectangle.
void HUDSolidBox(float x1, float y1, float x2, float y2, RGBAColor col);

// Draw a solid colour line (possibly translucent) between the two
// end points.  Coordinates are inclusive.  Drawing will be clipped
// to the current scissor rectangle.
void HUDSolidLine(float x1, float y1, float x2, float y2, RGBAColor col);

// Draw a thin outline of a box.
void HUDThinBox(float x1, float y1, float x2, float y2, RGBAColor col, float thickness = 0.0f,
                BlendingMode special_blend = kBlendingNone);

// Like HUDSolidBox but the colors of each corner (TL, BL, TR, BR) can
// be specified individually.
void HUDGradientBox(float x1, float y1, float x2, float y2, RGBAColor *cols);

void HUDDrawImage(float x, float y, const Image *image, const Colormap *colmap = nullptr);
void HUDDrawImageNoOffset(float x, float y, const Image *image);
void HUDScrollImage(float x, float y, const Image *image, float sx, float sy);
void HUDScrollImageNoOffset(float x, float y, const Image *image, float sx, float sy);
void HUDDrawImageTitleWS(const Image *image);
void HUDStretchImage(float x, float y, float w, float h, const Image *image, float sx, float sy,
                     const Colormap *colmap = nullptr);
void HUDStretchImageNoOffset(float x, float y, float w, float h, const Image *image, float sx, float sy);
void HUDTileImage(float x, float y, float w, float h, const Image *image, float offset_x = 0.0f, float offset_y = 0.0f);

// Functions for when we want to draw without having an image_c
void HUDStretchFromImageData(float x, float y, float w, float h, const ImageData *img, unsigned int tex_id,
                             ImageOpacity opacity);

extern int hud_tic;

float HUDFontWidth(void);
float HUDFontHeight(void);

float HUDFontWidthNew(float size = 0);

float HUDStringWidth(const char *str);
float HUDStringHeight(const char *str);

float HUDStringWidthNew(const char *str, float size = 0);

void HUDDrawChar(float left_x, float top_y, const Image *img, char ch, float size = 0);

// draw a text string with the current font, current color (etc).
void HUDDrawText(float x, float y, const char *str, float size = 0);

// Draw the ENDOOM/Quit screen
void HUDDrawQuitScreen();

// render a view of the world using the given camera object.
void HUDRenderWorld(float x, float y, float w, float h, MapObject *camera, int flags);

// render the automap
void HUDRenderAutomap(float x, float y, float w, float h, MapObject *focus, int flags);

void HUDGetCastPosition(float *x, float *y, float *scale_x, float *scale_y);

float HUDGetImageWidth(const Image *img);
float HUDGetImageHeight(const Image *img);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
