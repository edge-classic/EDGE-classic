//----------------------------------------------------------------------------
//  EDGE Sky Handling Code
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

#include "r_image.h"

extern const Image *sky_image;

// true when a custom sky box is present
extern bool custom_skybox;

extern bool need_to_draw_sky;

enum SkyboxFace
{
    kSkyboxNorth = 0,
    kSkyboxEast,
    kSkyboxSouth,
    kSkyboxWest,
    kSkyboxTop,
    kSkyboxBottom
};

void ComputeSkyHeights(void);

void BeginSky(void);
void FinishSky(void);

void RenderSkyPlane(Subsector *sub, float h);
void RenderSkyWall(Seg *seg, float h1, float h2);

int  UpdateSkyboxTextures(void);
void PrecacheSky(void);

void SetupSkyMatrices(void);
void RendererRevertSkyMatrices(void);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
