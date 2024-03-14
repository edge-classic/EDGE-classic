//----------------------------------------------------------------------------
//  EDGE OpenGL Rendering (Definitions)
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

#include <list>
#include <vector>

#include "con_var.h"
#include "image.h"
#include "main.h"
#include "r_defs.h"

extern ConsoleVariable renderer_dumb_sky;
extern ConsoleVariable renderer_dumb_clamp;

//
//  RendererMAIN
//

extern int maximum_texture_size;

void RendererInit(void);
void RendererSoftInit(void);
void RendererSetupMatrices2D(void);
void RendererSetupMatricesWorld2D(void);
void RendererSetupMatrices3d(void);

//
//  RendererBSP
//

extern int render_view_extra_light;

extern float render_view_red_multiplier;
extern float render_view_green_multiplier;
extern float render_view_blue_multiplier;

extern const Colormap *render_view_effect_colormap;

extern ConsoleVariable renderer_near_clip;
extern ConsoleVariable renderer_far_clip;

inline float FastApproximateDistance(float delta_x, float delta_y)
{
    return ((delta_x) + (delta_y)-0.5f * HMM_MIN((delta_x), (delta_y)));
}

//----------------------------------------------------------------------------

struct DrawFloor;

struct DrawSubsector;

enum VerticalClipMode
{
    kVerticalClipNever = 0,
    kVerticalClipSoft  = 1,  // only clip at translucent water
    kVerticalClipHard  = 2,  // vertically clip sprites at all solid surfaces
};

//
// DrawThing
//
// Stores the info about a single visible sprite in a subsector.
//
struct DrawThing
{
    // link for list
    DrawThing *next;
    DrawThing *previous;

    // actual map object
    MapObject *map_object;

    bool is_model;

    float map_x, map_y, map_z;  // map_z only used for models

    // vertical extent of sprite (world coords)
    float top;
    float bottom;

    int y_clipping;

    // sprite image to use
    const Image *image;
    bool         flip;

    // translated coords
    float translated_z;

    // colourmap/lighting
    RegionProperties *properties;

    // world offsets for GL
    float left_delta_x, left_delta_y;
    float right_delta_x, right_delta_y;
    float original_top, original_bottom;

    // Rendering order
    DrawThing *render_left, *render_right, *render_previous, *render_next;
};

//
// DrawFloor
//
// Stores all the information needed to draw a single on-screen
// floor of a subsector.
//
struct DrawFloor
{
    short is_lowest;
    short is_highest;

    // link for list, rendering order
    DrawFloor *render_next, *render_previous;

    // heights for this floor
    float floor_height, ceiling_height, top_height;

    MapSurface *floor, *ceiling;

    Extrafloor *extrafloor;

    // properties used herein
    RegionProperties *properties;

    // list of things
    // (not sorted until RendererDrawFloor is called).
    DrawThing *things;
};

struct DrawMirror
{
    Seg *seg = nullptr;

    BAMAngle left, right;

    bool is_portal = false;

    std::list<DrawSubsector *> draw_subsectors;
};

struct DrawSeg  // HOPEFULLY this can go away
{
    Seg *seg;
};

struct DrawSubsector
{
    Subsector *subsector = nullptr;

    // floors, sorted in height order (lowest to highest).
    std::vector<DrawFloor *> floors;

    // link list of floors, render order (furthest to closest)
    DrawFloor *render_floors;

    std::list<DrawSeg *> segs;

    std::list<DrawMirror *> mirrors;

    bool visible;
    bool sorted;
};

extern int detail_level;
extern int use_dynamic_lights;
extern int sprite_kludge;

const Image *RendererGetOtherSprite(int sprite, int frame, bool *flip);

//
//  RendererUTIL
//

void RendererInitialize(void);
void RendererClearBsp(void);

DrawThing     *RendererGetDrawThing();
DrawFloor     *RendererGetDrawFloor();
DrawSeg       *RendererGetDrawSeg();
DrawSubsector *RendererGetDrawSub();
DrawMirror    *RendererGetDrawMirror();

//
//  MIRRORS
//

extern int total_active_mirrors;

void MirrorCoordinate(float &x, float &y);
void MirrorHeight(float &z);
void MirrorAngle(BAMAngle &ang);

bool  MirrorReflective(void);
float MirrorXYScale(void);
float MirrorZScale(void);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
