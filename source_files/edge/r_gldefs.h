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
#include "ddf_image.h"
#include "ddf_main.h"
#include "r_defs.h"

extern ConsoleVariable renderer_dumb_sky;
extern ConsoleVariable renderer_dumb_clamp;

//
//  RendererBSP
//

extern int render_view_extra_light;

extern float render_view_red_multiplier;
extern float render_view_green_multiplier;
extern float render_view_blue_multiplier;

extern const Colormap *render_view_effect_colormap;

extern ConsoleVariable renderer_far_clip;
extern ConsoleVariable renderer_near_clip;

inline float FastApproximateDistance(float delta_x, float delta_y)
{
    return ((delta_x) + (delta_y)-0.5f * HMM_MIN((delta_x), (delta_y)));
}

//----------------------------------------------------------------------------

struct DrawFloor;

struct DrawSubsector;

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

    float map_x, map_y, map_z; // map_z only used for models

    // vertical extent of sprite (world coords)
    float top;
    float bottom;

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
    // (not sorted until RenderFloor is called).
    DrawThing *things;
};

struct DrawMirror
{
    Seg *seg = nullptr;

    BAMAngle left, right;

    bool is_portal = false;

    std::list<DrawSubsector *> draw_subsectors;
};

struct DrawSeg // HOPEFULLY this can go away
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
    bool solid;
};

extern int detail_level;
extern int use_dynamic_lights;
extern int sprite_kludge;

const Image *GetOtherSprite(int sprite, int frame, bool *flip);

//
//  RendererUTIL
//

void AllocateDrawStructs(void);
void ClearBSP(void);

DrawThing     *GetDrawThing();
DrawFloor     *GetDrawFloor();
DrawSeg       *GetDrawSeg();
DrawSubsector *GetDrawSub();
DrawMirror    *GetDrawMirror();

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
