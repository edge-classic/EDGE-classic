//----------------------------------------------------------------------------
//  EDGE Blockmap utility functions
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

#include "r_defs.h"

extern int blockmap_width;  // in mapblocks
extern int blockmap_height;

extern float blockmap_origin_x;  // origin of block map
extern float blockmap_origin_y;

constexpr uint8_t  kBlockmapUnitSize = 128;
constexpr uint16_t kLightmapUnitSize = 512;

inline int BlockmapGetX(float raw_x)
{
    return ((int)((raw_x)-blockmap_origin_x) / kBlockmapUnitSize);
}

inline int BlockmapGetY(float raw_y)
{
    return ((int)((raw_y)-blockmap_origin_y) / kBlockmapUnitSize);
}

inline int LightmapGetX(float raw_x)
{
    return ((int)((raw_x)-blockmap_origin_x) / kLightmapUnitSize);
}

inline int LightmapGetY(float raw_y)
{
    return ((int)((raw_y)-blockmap_origin_y) / kLightmapUnitSize);
}

enum PathInterceptFlags
{
    kPathAddLines  = 1,
    kPathAddThings = 2
};

struct PathIntercept
{
    float along;  // along trace line
    // one of these will be nullptr
    MapObject *thing;
    line_t    *line;
};

extern divline_t trace;

/* FUNCTIONS */

void CreateThingBlockmap(void);
void DestroyBlockmap(void);

void SetThingPosition(MapObject *mo);
void UnsetThingPosition(MapObject *mo);
void UnsetThingFinal(MapObject *mo);
void ChangeThingPosition(MapObject *mo, float x, float y, float z);
void FreeSectorTouchNodes(sector_t *sec);

void GenerateBlockmap(int min_x, int min_y, int max_x, int max_y);

bool BlockmapLineIterator(float x1, float y1, float x2, float y2,
                          bool (*func)(line_t *, void *), void *data = nullptr);

bool BlockmapThingIterator(float x1, float y1, float x2, float y2,
                           bool (*func)(MapObject *, void *),
                           void *data = nullptr);

void DynamicLightIterator(float x1, float y1, float z1, float x2, float y2,
                          float z2, void (*func)(MapObject *, void *),
                          void *data = nullptr);

void SectorGlowIterator(sector_t *sec, float x1, float y1, float z1, float x2,
                        float y2, float z2, void (*func)(MapObject *, void *),
                        void *data = nullptr);

float PathInterceptVector(divline_t *v2, divline_t *v1);

bool PathTraverse(float x1, float y1, float x2, float y2, int flags,
                  bool (*func)(PathIntercept *, void *), void *data = nullptr);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
