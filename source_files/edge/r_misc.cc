//----------------------------------------------------------------------------
//  EDGE Main Rendering Organisation Code
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
//
// -KM- 1998/09/27 Dynamic Colourmaps
//

#include "r_misc.h"

#include <math.h>

#include "AlmostEquals.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "e_main.h"
#include "epi.h"
#include "epi_doomdefs.h"
#include "i_defs_gl.h"
#include "m_misc.h"
#include "n_network.h"
#include "p_local.h"
#include "p_mobj.h"
#include "r_colormap.h"
#include "r_defs.h"
#include "r_draw.h"
#include "r_gldefs.h"
#include "r_modes.h"
#include "r_units.h"

EDGE_DEFINE_CONSOLE_VARIABLE(field_of_view, "90", kConsoleVariableFlagArchive)

extern unsigned int root_node;

int view_window_x;
int view_window_y;
int view_window_width;
int view_window_height;

BAMAngle view_angle          = 0;
BAMAngle view_vertical_angle = 0;

HMM_Vec3 view_forward;
HMM_Vec3 view_up;
HMM_Vec3 view_right;

BAMAngle normal_field_of_view, zoomed_field_of_view;
bool     view_is_zoomed = false;

// increment every time a check is made
int valid_count = 1;

// just for profiling purposes
int render_frame_count;
int line_count;

Subsector        *view_subsector;
RegionProperties *view_properties;

float view_x;
float view_y;
float view_z;

float view_cosine;
float view_sine;

Player *view_player;

MapObject *background_camera_map_object = nullptr;

//
// precalculated math tables
//

int reduce_flash = 0;

int invulnerability_effect;

float sine_table[kSineTableSize];

void FreeBSP(void);

//
// To get a global angle from cartesian coordinates,
// the coordinates are flipped until they are in
// the first octant of the coordinate system, then
// the y (<=x) is scaled and divided by x to get a
// tangent (slope) value which is looked up in the
// tantoangle[] table.

static float ApproximateAtan2(float y, float x)
{
    // http://pubs.opengroup.org/onlinepubs/009695399/functions/atan2.html
    // Volkan SALMA

    const float ONEQTR_PI = HMM_PI / 4.0;
    const float THRQTR_PI = 3.0 * HMM_PI / 4.0;
    float       r, angle;
    float       abs_y = fabs(y) + 1e-10f; // kludge to prevent 0/0 condition
    if (x < 0.0f)
    {
        r     = (x + abs_y) / (abs_y - x);
        angle = THRQTR_PI;
    }
    else
    {
        r     = (x - abs_y) / (x + abs_y);
        angle = ONEQTR_PI;
    }
    angle += (0.1963f * r * r - 0.9817f) * r;
    if (y < 0.0f)
        return (-angle); // negate if in quad III or IV
    else
        return (angle);
}
//
BAMAngle PointToAngle(float x1, float y1, float x, float y, bool precise)
{
    x -= x1;
    y -= y1;

    if (precise)
    {
        return (AlmostEquals(x, 0.0f) && AlmostEquals(y, 0.0f)) ? 0 : epi::BAMFromDegrees(atan2(y, x) * (180 / HMM_PI));
    }

    return epi::BAMFromDegrees(ApproximateAtan2(y, x) * (180 / HMM_PI));
}

float PointToDistance(float x1, float y1, float x2, float y2)
{
    BAMAngle angle;
    float    dx;
    float    dy;
    float    temp;
    float    dist;

    dx = (float)fabs(x2 - x1);
    dy = (float)fabs(y2 - y1);

    if (AlmostEquals(dx, 0.0f))
        return dy;
    else if (AlmostEquals(dy, 0.0f))
        return dx;

    if (dy > dx)
    {
        temp = dx;
        dx   = dy;
        dy   = temp;
    }

    angle = epi::BAMFromATan(dy / dx) + kBAMAngle90;

    // use as cosine
    dist = dx / epi::BAMSin(angle);

    return dist;
}

//
// Called once at startup, to initialise some rendering stuff.
//
void RendererStartup(void)
{
    if (language["RefreshDaemon"])
        LogPrint("%s", language["RefreshDaemon"]);
    else
        LogPrint("Unknown Refresh Daemon");

    for (int i = 0; i < kSineTableSize; i++)
    {
        sine_table[i] = HMM_SINF(HMM_AngleDeg(i * 360.0f / ((float)(kSineTableMask))));
    }

    render_frame_count = 0;
}

//
// Called at shutdown
//
void RendererShutdown(void)
{
    FreeBSP();
}

Subsector *PointInSubsector(float x, float y)
{
    BspNode     *node;
    int          side;
    unsigned int nodenum;

    nodenum = root_node;

    while (!(nodenum & kLeafSubsector))
    {
        node    = &level_nodes[nodenum];
        side    = PointOnDividingLineSide(x, y, &node->divider);
        nodenum = node->children[side];
    }

    return &level_subsectors[nodenum & ~kLeafSubsector];
}

RegionProperties *GetPointProperties(Subsector *sub, float z)
{
    Extrafloor *S, *L, *C;
    float       floor_h;

    // traverse extrafloors upwards

    floor_h = sub->sector->floor_height;

    S = sub->sector->bottom_extrafloor;
    L = sub->sector->bottom_liquid;

    while (S || L)
    {
        if (!L || (S && S->bottom_height < L->bottom_height))
        {
            C = S;
            S = S->higher;
        }
        else
        {
            C = L;
            L = L->higher;
        }

        EPI_ASSERT(C);

        // ignore liquids in the middle of THICK solids, or below real
        // floor or above real ceiling
        //
        if (C->bottom_height < floor_h || C->bottom_height > sub->sector->ceiling_height)
            continue;

        if (z < C->top_height)
            return C->properties;

        floor_h = C->top_height;
    }

    // extrafloors were exhausted, must be top area
    return sub->sector->active_properties;
}

//----------------------------------------------------------------------------

// large buffers for cache coherency vs allocating each on heap
static constexpr uint16_t kDefaultDrawThings     = 32768;
static constexpr uint16_t kDefaultDrawFloors     = 32768;
static constexpr uint32_t kDefaultDrawSegs       = 65536;
static constexpr uint32_t kDefaultDrawSubsectors = 65536;
static constexpr uint16_t kDefaultDrawMirrors    = 512;

static std::vector<DrawThing>     draw_things;
static std::vector<DrawFloor>     draw_floors;
static std::vector<DrawSeg>       draw_segs;
static std::vector<DrawSubsector> draw_subsectors;
static std::vector<DrawMirror>    draw_mirrors;

static int draw_thing_position;
static int draw_floor_position;
static int draw_seg_position;
static int draw_subsector_position;
static int draw_mirror_position;

//
// AllocateDrawStructs
//
// One-time initialisation routine.
//
void AllocateDrawStructs(void)
{
    draw_things.resize(kDefaultDrawThings);
    draw_floors.resize(kDefaultDrawFloors);
    draw_segs.resize(kDefaultDrawSegs);
    draw_subsectors.resize(kDefaultDrawSubsectors);
    draw_mirrors.resize(kDefaultDrawMirrors);
}

// bsp clear function

void ClearBSP(void)
{
    draw_thing_position     = 0;
    draw_floor_position     = 0;
    draw_seg_position       = 0;
    draw_subsector_position = 0;
    draw_mirror_position    = 0;
}

void FreeBSP(void)
{
    draw_things.clear();
    draw_floors.clear();
    draw_segs.clear();
    draw_subsectors.clear();
    draw_mirrors.clear();

    ClearBSP();
}

DrawThing *GetDrawThing()
{
    if (draw_thing_position == draw_things.size())
        draw_things.push_back(DrawThing());

    return &draw_things[draw_thing_position++];
}

DrawFloor *GetDrawFloor()
{
    if (draw_floor_position == draw_floors.size())
        draw_floors.push_back(DrawFloor());

    return &draw_floors[draw_floor_position++];
}

DrawSeg *GetDrawSeg()
{
    if (draw_seg_position == draw_segs.size())
        draw_segs.push_back(DrawSeg());

    return &draw_segs[draw_seg_position++];
}

DrawSubsector *GetDrawSub()
{
    if (draw_subsector_position == draw_subsectors.size())
        draw_subsectors.push_back(DrawSubsector());

    return &draw_subsectors[draw_subsector_position++];
}

DrawMirror *GetDrawMirror()
{
    if (draw_mirror_position == draw_mirrors.size())
        draw_mirrors.push_back(DrawMirror());

    return &draw_mirrors[draw_mirror_position++];
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
