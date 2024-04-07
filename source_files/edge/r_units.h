//----------------------------------------------------------------------------
//  EDGE GPU Rendering (Unit system)
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

#include "HandmadeMath.h"
#include "epi_color.h"
#include "i_defs_gl.h"
#include "sokol_color.h"

constexpr uint16_t kDummyClamp = 789;

// a single vertex to pass to the GPU
struct RendererVertex
{
    GLfloat  rgba_color[4];
    HMM_Vec3 position;
    HMM_Vec2 texture_coordinates[2];
    HMM_Vec3 normal;
};

extern sg_color culling_fog_color;

void RendererStartUnits(bool sort_em);
void RendererFinishUnits(void);
void RenderUnits(void);

enum BlendingMode
{
    kBlendingNone = 0,

    kBlendingMasked = (1 << 0),    // drop fragments when alpha == 0
    kBlendingLess   = (1 << 1),    // drop fragments when alpha < color.a
    kBlendingAlpha  = (1 << 2),    // alpha-blend with the framebuffer
    kBlendingAdd    = (1 << 3),    // additive-blend with the framebuffer

    kBlendingCullBack  = (1 << 4), // enable back-face culling
    kBlendingCullFront = (1 << 5), // enable front-face culling
    kBlendingNoZBuffer = (1 << 6), // don't update the Z buffer
    kBlendingClampY    = (1 << 7), // force texture to be Y clamped
};

enum CustomTextureEnvironment
{
    kTextureEnvironmentDisable,
    // the texture unit is disabled (complete pass-through).

    kTextureEnvironmentSkipRGB,
    // causes the RGB of the texture to be skipped, i.e. the
    // output of the texture unit is the same as the input
    // for the RGB components.  The alpha component is treated
    // normally, i.e. passed on to next texture unit.
};

RendererVertex *RendererBeginUnit(GLuint shape, int max_vert, GLuint env1, GLuint tex1, GLuint env2, GLuint tex2,
                                  int pass, int blending, RGBAColor fog_color = kRGBANoValue, float fog_density = 0);
void            RendererEndUnit(int actual_vert);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
