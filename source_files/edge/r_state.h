//----------------------------------------------------------------------------
//  EDGE Refresh internal state variables
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

#include <string.h>

#include <unordered_map>

// Need data structure definitions.

#include "AlmostEquals.h"
#include "edge_profiling.h"
#include "i_defs_gl.h"
#include "i_system.h"
#include "m_math.h"
#include "r_defs.h"
#include "w_flat.h"

//
// Lookup tables for map data.
//
extern int     total_level_vertexes;
extern Vertex *level_vertexes;

extern int     total_level_sectors;
extern Sector *level_sectors;

extern int        total_level_subsectors;
extern Subsector *level_subsectors;

extern int         total_level_extrafloors;
extern Extrafloor *level_extrafloors;

extern int      total_level_nodes;
extern BspNode *level_nodes;

extern int   total_level_lines;
extern Line *level_lines;

extern int   total_level_sides;
extern Side *level_sides;

extern std::unordered_map<GLuint, GLint> texture_clamp_s;
extern std::unordered_map<GLuint, GLint> texture_clamp_t;

//
// POV data.
//
extern float view_x;
extern float view_y;
extern float view_z;

extern BAMAngle view_angle;

// -ES- 1999/03/20 Added these.
// Angles that are used for linedef clipping.
// Nearly the same as leftangle/rightangle, but slightly rounded to fit
// view_angletox lookups, and converted to BAM format.

// angles used for clipping
extern BAMAngle clip_left, clip_right;

// the scope of the clipped area (clip_left-clip_right).
// kBAMAngle180 disables polar clipping
extern BAMAngle clip_scope;

// the most extreme angles of the view
extern float view_x_slope, view_y_slope;

extern ECFrameStats ec_frame_stats;

enum RenderUsage
{
    kRenderUsageImmutable = 0,
    kRenderUsageDynamic,
    kRenderUsageStream
};

class RenderState
{
  public:
    virtual void Enable(GLenum cap, bool enabled = true) = 0;

    virtual void Disable(GLenum cap) = 0;

    virtual void DepthMask(bool enable) = 0;

    virtual void DepthFunction(GLenum func) = 0;

    virtual void ColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) = 0;

    virtual void CullFace(GLenum mode) = 0;

    virtual void AlphaFunction(GLenum func, GLfloat ref) = 0;

    virtual void ActiveTexture(GLenum activeTexture) = 0;

    virtual void BindTexture(GLuint textureid) = 0;

    virtual void ClipPlane(GLenum plane, GLdouble *equation) = 0;

    virtual void PolygonOffset(GLfloat factor, GLfloat units) = 0;

    virtual void Clear(GLbitfield mask) = 0;

    virtual void ClearColor(RGBAColor color) = 0;

    virtual void FogMode(GLint fogMode) = 0;

    virtual void FogColor(RGBAColor color) = 0;

    virtual void FogStart(GLfloat start) = 0;

    virtual void FogEnd(GLfloat end) = 0;

    virtual void FogDensity(GLfloat density) = 0;

    virtual void GLColor(RGBAColor color) = 0;

    virtual void BlendFunction(GLenum sfactor, GLenum dfactor) = 0;

    virtual void TextureEnvironmentMode(GLint param) = 0;

    virtual void TextureEnvironmentCombineRGB(GLint param) = 0;

    virtual void TextureEnvironmentSource0RGB(GLint param) = 0;

    virtual void TextureMinFilter(GLint param) = 0;

    virtual void TextureMagFilter(GLint param) = 0;

    virtual void TextureWrapS(GLint param) = 0;

    virtual void TextureWrapT(GLint param) = 0;

    virtual void MultiTexCoord(GLuint tex, const HMM_Vec2 *coords) = 0;

    virtual void Hint(GLenum target, GLenum mode) = 0;

    virtual void LineWidth(float width) = 0;

    virtual void DeleteTexture(const GLuint *tex_id) = 0;

    virtual void FrontFace(GLenum wind) = 0;

    virtual void ShadeModel(GLenum model) = 0;

    virtual void Scissor(GLint x, GLint y, GLsizei width, GLsizei height)  = 0;

    virtual void GenTextures(GLsizei n, GLuint *textures) = 0;

    virtual void FinishTextures(GLsizei n, GLuint *textures) = 0;

    virtual void TexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels, RenderUsage usage = kRenderUsageImmutable) = 0;

    virtual void PixelStorei(GLenum pname, GLint param) = 0;

    virtual void ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels) = 0;

    virtual void PixelZoom(GLfloat xfactor, GLfloat yfactor) = 0;

    virtual void Flush() = 0;

    virtual void SetPipeline(uint32_t flags) = 0;
};

extern RenderState *render_state;

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
