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

constexpr uint16_t kDummyClamp = 789;
constexpr uint8_t kMaximumPolygonVertices = 64;
constexpr uint16_t kMaximumLocalVertices = 65535;

// a single vertex to pass to the GPU
struct RendererVertex
{
    RGBAColor rgba;
    HMM_Vec3 position;
    HMM_Vec2 texture_coordinates[2];
    HMM_Vec3 normal;
};

extern RGBAColor culling_fog_color;

void StartUnitBatch(bool sort_em);
void FinishUnitBatch(void);
void RenderCurrentUnits(void);

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

    kBlendingNoFog     = (1 << 8),  // force disable fog (including culling fog)

    kBlendingRepeatX   = (1 << 9),  // force texture to repeat on X axis
    kBlendingRepeatY   = (1 << 10), // force texture to repeat on Y axis

    kBlendingGEqual    = (1 << 11), // drop fragments when alpha >= 1.0f - color.a
                                    // Dasho - This is super specific and only 
                                    // used by the "pixelfade" wipe :/

    kBlendingInvert    = (1 << 12), // color inversion (simple invuln fx)
    kBlendingNegativeGamma = (1 << 13),
    kBlendingPositiveGamma = (1 << 14)
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

RendererVertex *BeginRenderUnit(GLuint shape, int max_vert, GLuint env1, GLuint tex1, GLuint env2, GLuint tex2,
                                int pass, int blending, RGBAColor fog_color = kRGBANoValue, float fog_density = 0);
void            EndRenderUnit(int actual_vert);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
