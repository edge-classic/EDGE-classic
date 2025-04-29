//----------------------------------------------------------------------------
//  EDGE OpenGL Rendering (BSP Traversal)
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

#include <math.h>

#include <unordered_map>
#include <unordered_set>

#include "AlmostEquals.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "edge_profiling.h"
#include "epi.h"
#include "epi_doomdefs.h"
#include "g_game.h"
#include "i_defs_gl.h"
#include "m_bbox.h"
#include "n_network.h" // NetworkUpdate
#include "p_local.h"
#include "p_tick.h"
#include "r_backend.h"
#include "r_colormap.h"
#include "r_defs.h"
#include "r_effects.h"
#include "r_gldefs.h"
#include "r_image.h"
#include "r_mirror.h"
#include "r_misc.h"
#include "r_modes.h"
#include "r_occlude.h"
#include "r_render.h"
#include "r_shader.h"
#include "r_sky.h"
#include "r_things.h"
#include "r_units.h"

#ifdef EDGE_SOKOL
#ifndef EDGE_WEB
#define BSP_MULTITHREAD
#endif

static void BSPQueueDrawSubsector(DrawSubsector *subsector);
static void BSPQueueSkyWall(Seg *seg, float h1, float h2);
static void BSPQueueSkyPlane(Subsector *sub, float h);
static void BSPQueueRenderBatch(RenderBatch *batch);

static RenderBatch *current_batch = nullptr;

#endif

#ifdef BSP_MULTITHREAD
#include "thread.h"

constexpr int32_t kMaxRenderBatch = 65536 / 4;

struct BSPThread
{
    thread_ptr_t        thread_;
    thread_signal_t     signal_start_;
    thread_atomic_int_t traverse_finished_;

    thread_queue_t      queue_;
    RenderBatch        *render_queue_[kMaxRenderBatch];
    thread_atomic_int_t exit_flag_;
};

static struct BSPThread bsp_thread;

#else

std::list<DrawSubsector *> draw_subsector_list;

#endif

MirrorSet bsp_mirror_set(kMirrorSetBSP);

EDGE_DEFINE_CONSOLE_VARIABLE(debug_hall_of_mirrors, "0", kConsoleVariableFlagCheat)

extern ConsoleVariable draw_culling;

unsigned int root_node;

// -ES- 1999/03/20 Different right & left side clip angles, for asymmetric FOVs.
BAMAngle clip_left, clip_right;
BAMAngle clip_scope;

MapObject *view_camera_map_object;

static int check_coordinates[12][4] = {{kBoundingBoxRight, kBoundingBoxTop, kBoundingBoxLeft, kBoundingBoxBottom},
                                       {kBoundingBoxRight, kBoundingBoxTop, kBoundingBoxLeft, kBoundingBoxTop},
                                       {kBoundingBoxRight, kBoundingBoxBottom, kBoundingBoxLeft, kBoundingBoxTop},
                                       {0},
                                       {kBoundingBoxLeft, kBoundingBoxTop, kBoundingBoxLeft, kBoundingBoxBottom},
                                       {0},
                                       {kBoundingBoxRight, kBoundingBoxBottom, kBoundingBoxRight, kBoundingBoxTop},
                                       {0},
                                       {kBoundingBoxLeft, kBoundingBoxTop, kBoundingBoxRight, kBoundingBoxBottom},
                                       {kBoundingBoxLeft, kBoundingBoxBottom, kBoundingBoxRight, kBoundingBoxBottom},
                                       {kBoundingBoxLeft, kBoundingBoxBottom, kBoundingBoxRight, kBoundingBoxTop}};

ViewHeightZone view_height_zone;

// common stuff

static Subsector *bsp_current_subsector;

static void BSPWalkMirror(DrawSubsector *dsub, Seg *seg, BAMAngle left, BAMAngle right, bool is_portal)
{
    DrawMirror *mir = GetDrawMirror();
    mir->seg        = seg;
    mir->draw_subsectors.clear();

    mir->left      = view_angle + left;
    mir->right     = view_angle + right;
    mir->is_portal = is_portal;

    dsub->mirrors.push_back(mir);

    // push mirror (translation matrix)
    bsp_mirror_set.Push(mir);

    Subsector *save_sub = bsp_current_subsector;

    BAMAngle save_clip_L = clip_left;
    BAMAngle save_clip_R = clip_right;
    BAMAngle save_scope  = clip_scope;

    clip_left  = left;
    clip_right = right;
    clip_scope = left - right;

    // perform another BSP walk
    BSPWalkNode(root_node);

    bsp_current_subsector = save_sub;

    clip_left  = save_clip_L;
    clip_right = save_clip_R;
    clip_scope = save_scope;

    // pop mirror
    bsp_mirror_set.Pop();
}

//
// BSPWalkSeg
//
// Visit a single seg of the subsector, and for one-sided lines update
// the 1D occlusion buffer.
//
static void BSPWalkSeg(DrawSubsector *dsub, Seg *seg)
{
    EDGE_ZoneScoped;

    // ignore segs sitting on current mirror
    if (bsp_mirror_set.SegOnPortal(seg))
        return;

    float sx1 = seg->vertex_1->X;
    float sy1 = seg->vertex_1->Y;

    float sx2 = seg->vertex_2->X;
    float sy2 = seg->vertex_2->Y;

    // when there are active mirror planes, segs not only need to
    // be flipped across them but also clipped across them.

    int32_t active_mirrors = bsp_mirror_set.TotalActive();
    if (active_mirrors > 0)
    {
        for (int i = active_mirrors - 1; i >= 0; i--)
        {
            bsp_mirror_set.Transform(i, sx1, sy1);
            bsp_mirror_set.Transform(i, sx2, sy2);

            if (!bsp_mirror_set.IsPortal(i))
            {
                float tmp_x = sx1;
                sx1         = sx2;
                sx2         = tmp_x;
                float tmp_y = sy1;
                sy1         = sy2;
                sy2         = tmp_y;
            }

            Seg *clipper = bsp_mirror_set.GetSeg(i);

            DividingLine div;

            div.x       = clipper->vertex_1->X;
            div.y       = clipper->vertex_1->Y;
            div.delta_x = clipper->vertex_2->X - div.x;
            div.delta_y = clipper->vertex_2->Y - div.y;

            int s1 = PointOnDividingLineSide(sx1, sy1, &div);
            int s2 = PointOnDividingLineSide(sx2, sy2, &div);

            // seg lies completely in front of clipper?
            if (s1 == 0 && s2 == 0)
                return;

            if (s1 != s2)
            {
                // seg crosses clipper, need to split it
                float ix, iy;

                ComputeIntersection(&div, sx1, sy1, sx2, sy2, &ix, &iy);

                if (s2 == 0)
                    sx2 = ix, sy2 = iy;
                else
                    sx1 = ix, sy1 = iy;
            }
        }
    }

    bool precise = active_mirrors > 0;
    if (!precise && seg->linedef)
    {
        precise = (seg->linedef->flags & kLineFlagMirror) || (seg->linedef->portal_pair);
    }

    BAMAngle angle_L = PointToAngle(view_x, view_y, sx1, sy1, precise);
    BAMAngle angle_R = PointToAngle(view_x, view_y, sx2, sy2, precise);

    // Clip to view edges.

    BAMAngle span = angle_L - angle_R;

    // back side ?
    if (span >= kBAMAngle180)
        return;

    angle_L -= view_angle;
    angle_R -= view_angle;

    if (clip_scope != kBAMAngle180)
    {
        BAMAngle tspan1 = angle_L - clip_right;
        BAMAngle tspan2 = clip_left - angle_R;

        if (tspan1 > clip_scope)
        {
            // Totally off the left edge?
            if (tspan2 >= kBAMAngle180)
                return;

            angle_L = clip_left;
        }

        if (tspan2 > clip_scope)
        {
            // Totally off the left edge?
            if (tspan1 >= kBAMAngle180)
                return;

            angle_R = clip_right;
        }

        span = angle_L - angle_R;
    }

    // The seg is in the view range,
    // but not necessarily visible.

#if 1
    // check if visible
    if (span > (kBAMAngle1 / 4) && OcclusionTest(angle_R, angle_L))
    {
        return;
    }
#endif

    dsub->visible = true;

    if (seg->miniseg || span == 0)
        return;

    if (active_mirrors < kMaximumMirrors)
    {
        if (seg->linedef->flags & kLineFlagMirror)
        {
            BSPWalkMirror(dsub, seg, angle_L, angle_R, false);
            OcclusionSet(angle_R, angle_L);
            return;
        }
        else if (seg->linedef->portal_pair)
        {
            BSPWalkMirror(dsub, seg, angle_L, angle_R, true);
            OcclusionSet(angle_R, angle_L);
            return;
        }
    }

    DrawSeg *dseg = GetDrawSeg();
    dseg->seg     = seg;

    dsub->segs.push_back(dseg);

    Sector *fsector = seg->front_subsector->sector;
    Sector *bsector = nullptr;

    if (seg->back_subsector)
        bsector = seg->back_subsector->sector;

    // only 1 sided walls affect the 1D occlusion buffer

    if (seg->linedef->blocked)
        OcclusionSet(angle_R, angle_L);

    // --- handle sky (using the depth buffer) ---
    float             f_fh    = 0;
    float             f_ch    = 0;
    float             b_fh    = 0;
    float             b_ch    = 0;
    const MapSurface *f_floor = nullptr;
    const MapSurface *f_ceil  = nullptr;
    const MapSurface *b_floor = nullptr;
    const MapSurface *b_ceil  = nullptr;

    if (!fsector->height_sector)
    {
        f_fh    = fsector->interpolated_floor_height;
        f_floor = &fsector->floor;
        f_ch    = fsector->interpolated_ceiling_height;
        f_ceil  = &fsector->ceiling;
    }
    else
    {
        if (view_height_zone == kHeightZoneA && view_z > fsector->height_sector->interpolated_ceiling_height)
        {
            f_fh    = fsector->height_sector->interpolated_ceiling_height;
            f_ch    = fsector->interpolated_ceiling_height;
            f_floor = &fsector->height_sector->floor;
            f_ceil  = &fsector->height_sector->ceiling;
        }
        else if (view_height_zone == kHeightZoneC && view_z < fsector->height_sector->interpolated_floor_height)
        {
            f_fh    = fsector->interpolated_floor_height;
            f_ch    = fsector->height_sector->interpolated_floor_height;
            f_floor = &fsector->height_sector->floor;
            f_ceil  = &fsector->height_sector->ceiling;
        }
        else
        {
            f_fh    = fsector->height_sector->interpolated_floor_height;
            f_ch    = fsector->height_sector->interpolated_ceiling_height;
            f_floor = &fsector->floor;
            f_ceil  = &fsector->ceiling;
        }
    }

    if (bsector)
    {
        if (!bsector->height_sector)
        {
            b_fh    = bsector->interpolated_floor_height;
            b_floor = &bsector->floor;
            b_ch    = bsector->interpolated_ceiling_height;
            b_ceil  = &bsector->ceiling;
        }
        else
        {
            if (view_height_zone == kHeightZoneA && view_z > bsector->height_sector->interpolated_ceiling_height)
            {
                b_fh    = bsector->height_sector->interpolated_ceiling_height;
                b_ch    = bsector->interpolated_ceiling_height;
                b_floor = &bsector->height_sector->floor;
                b_ceil  = &bsector->height_sector->ceiling;
            }
            else if (view_height_zone == kHeightZoneC && view_z < bsector->height_sector->interpolated_floor_height)
            {
                b_fh    = bsector->interpolated_floor_height;
                b_ch    = bsector->height_sector->interpolated_floor_height;
                b_floor = &bsector->height_sector->floor;
                b_ceil  = &bsector->height_sector->ceiling;
            }
            else
            {
                b_fh    = bsector->height_sector->interpolated_floor_height;
                b_ch    = bsector->height_sector->interpolated_ceiling_height;
                b_floor = &bsector->floor;
                b_ceil  = &bsector->ceiling;
            }
        }
    }

    if (bsector && EDGE_IMAGE_IS_SKY(*f_floor) && EDGE_IMAGE_IS_SKY(*b_floor) && seg->sidedef->bottom.image == nullptr)
    {
        if (f_fh < b_fh)
        {
#ifdef EDGE_SOKOL
            BSPQueueSkyWall(seg, f_fh, b_fh);
#else
            RenderSkyWall(seg, f_fh, b_fh);
#endif
        }
    }

    if (EDGE_IMAGE_IS_SKY(*f_ceil))
    {
        if (f_ch < fsector->sky_height && (!bsector || !EDGE_IMAGE_IS_SKY(*b_ceil) || b_fh >= f_ch))
        {
#ifdef EDGE_SOKOL
            BSPQueueSkyWall(seg, f_ch, fsector->sky_height);
#else
            RenderSkyWall(seg, f_ch, fsector->sky_height);
#endif
        }
        else if (bsector && EDGE_IMAGE_IS_SKY(*b_ceil))
        {
            float max_f = HMM_MAX(f_fh, b_fh);

            if (b_ch <= max_f && max_f < fsector->sky_height)
            {
#ifdef EDGE_SOKOL
                BSPQueueSkyWall(seg, max_f, fsector->sky_height);
#else
                RenderSkyWall(seg, max_f, fsector->sky_height);
#endif
            }
        }
    }
    // -AJA- 2004/08/29: Emulate Sky-Flooding TRICK
    else if (!debug_hall_of_mirrors.d_ && bsector && EDGE_IMAGE_IS_SKY(*b_ceil) && seg->sidedef->top.image == nullptr &&
             b_ch < f_ch)
    {
#ifdef EDGE_SOKOL
        BSPQueueSkyWall(seg, b_ch, f_ch);
#else
        RenderSkyWall(seg, b_ch, f_ch);
#endif
    }
}

//
// BSPCheckBBox
//
// Checks BSP node/subtree bounding box.
// Returns true if some part of the bbox might be visible.
//
// Placed here to be close to BSPWalkSeg(), which has similiar angle
// clipping stuff in it.
//
static bool BSPCheckBBox(const float *bspcoord)
{
    EDGE_ZoneScoped;

    if (bsp_mirror_set.TotalActive() > 0)
    {
        // a flipped bbox may no longer be axis aligned, hence we
        // need to find the bounding area of the transformed box.
        static float new_bbox[4];

        BoundingBoxClear(new_bbox);

        for (int p = 0; p < 4; p++)
        {
            float tx = bspcoord[(p & 1) ? kBoundingBoxLeft : kBoundingBoxRight];
            float ty = bspcoord[(p & 2) ? kBoundingBoxBottom : kBoundingBoxTop];

            bsp_mirror_set.Coordinate(tx, ty);

            BoundingBoxAddPoint(new_bbox, tx, ty);
        }

        bspcoord = new_bbox;
    }

    int boxx, boxy;

    // Find the corners of the box
    // that define the edges from current viewpoint.
    if (view_x <= bspcoord[kBoundingBoxLeft])
        boxx = 0;
    else if (view_x < bspcoord[kBoundingBoxRight])
        boxx = 1;
    else
        boxx = 2;

    if (view_y >= bspcoord[kBoundingBoxTop])
        boxy = 0;
    else if (view_y > bspcoord[kBoundingBoxBottom])
        boxy = 1;
    else
        boxy = 2;

    int boxpos = (boxy << 2) + boxx;

    if (boxpos == 5)
        return true;

    float x1 = bspcoord[check_coordinates[boxpos][0]];
    float y1 = bspcoord[check_coordinates[boxpos][1]];
    float x2 = bspcoord[check_coordinates[boxpos][2]];
    float y2 = bspcoord[check_coordinates[boxpos][3]];

    // check clip list for an open space
    BAMAngle angle_L = PointToAngle(view_x, view_y, x1, y1);
    BAMAngle angle_R = PointToAngle(view_x, view_y, x2, y2);

    BAMAngle span = angle_L - angle_R;

    // Sitting on a line?
    if (span >= kBAMAngle180)
        return true;

    angle_L -= view_angle;
    angle_R -= view_angle;

    if (clip_scope != kBAMAngle180)
    {
        BAMAngle tspan1 = angle_L - clip_right;
        BAMAngle tspan2 = clip_left - angle_R;

        if (tspan1 > clip_scope)
        {
            // Totally off the left edge?
            if (tspan2 >= kBAMAngle180)
                return false;

            angle_L = clip_left;
        }

        if (tspan2 > clip_scope)
        {
            // Totally off the right edge?
            if (tspan1 >= kBAMAngle180)
                return false;

            angle_R = clip_right;
        }

        if (angle_L == angle_R)
            return false;

        if (draw_culling.d_)
        {
            float closest = 1000000.0f;
            float check   = PointToSegDistance({{x1, y1}}, {{x2, y1}}, {{view_x, view_y}});
            if (check < closest)
                closest = check;
            check = PointToSegDistance({{x1, y1}}, {{x1, y2}}, {{view_x, view_y}});
            if (check < closest)
                closest = check;
            check = PointToSegDistance({{x2, y1}}, {{x2, y2}}, {{view_x, view_y}});
            if (check < closest)
                closest = check;
            check = PointToSegDistance({{x1, y2}}, {{x2, y2}}, {{view_x, view_y}});
            if (check < closest)
                closest = check;

            if (closest > (renderer_far_clip.f_ + 500.0f))
                return false;
        }
    }

    return !OcclusionTest(angle_R, angle_L);
}

static inline void AddNewDrawFloor(DrawSubsector *dsub, Extrafloor *ef, float floor_height, float ceiling_height,
                                   float top_h, MapSurface *floor, MapSurface *ceil, RegionProperties *props)
{
    DrawFloor *dfloor;

    dfloor = GetDrawFloor();

    dfloor->is_highest      = false;
    dfloor->is_lowest       = false;
    dfloor->render_next     = nullptr;
    dfloor->render_previous = nullptr;
    dfloor->floor           = nullptr;
    dfloor->ceiling         = nullptr;
    dfloor->extrafloor      = nullptr;
    dfloor->properties      = nullptr;
    dfloor->things          = nullptr;

    dfloor->floor_height   = floor_height;
    dfloor->ceiling_height = ceiling_height;
    dfloor->top_height     = top_h;
    dfloor->floor          = floor;
    dfloor->ceiling        = ceil;
    dfloor->extrafloor     = ef;
    dfloor->properties     = props;

    // link it in, height order

    dsub->floors.push_back(dfloor);

    // link it in, rendering order (very important)

    if (dsub->render_floors == nullptr || floor_height > view_z)
    {
        // add to head
        dfloor->render_next     = dsub->render_floors;
        dfloor->render_previous = nullptr;

        if (dsub->render_floors)
            dsub->render_floors->render_previous = dfloor;

        dsub->render_floors = dfloor;
    }
    else
    {
        // add to tail
        DrawFloor *tail;

        for (tail = dsub->render_floors; tail->render_next; tail = tail->render_next)
        { /* nothing here */
        }

        dfloor->render_next     = nullptr;
        dfloor->render_previous = tail;

        tail->render_next = dfloor;
    }
}

//
// BSPWalkSubsector
//
// Visit a subsector, and collect information, such as where the
// walls, planes (ceilings & floors) and things need to be drawn.
//
static void BSPWalkSubsector(int num)
{
    EDGE_ZoneScoped;

    Subsector *sub    = &level_subsectors[num];
    Sector    *sector = sub->sector;

    // store subsector in a global var for other functions to use
    bsp_current_subsector = sub;

#if (DEBUG >= 1)
    LogDebug("\nVISITING SUBSEC %d (sector %d)\n\n", num, sub->sector - level_sectors);
#endif

    DrawSubsector *K = GetDrawSub();
    K->subsector     = sub;
    K->visible       = false;
    K->sorted        = false;
    K->render_floors = nullptr;

    K->floors.clear();
    K->segs.clear();
    K->mirrors.clear();

    // --- handle sky (using the depth buffer) ---

    if (!sector->height_sector)
    {
        if (EDGE_IMAGE_IS_SKY(sub->sector->floor) && view_z > sub->sector->interpolated_floor_height)
        {
#ifdef EDGE_SOKOL
            BSPQueueSkyPlane(sub, sub->sector->interpolated_floor_height);
#else
            RenderSkyPlane(sub, sub->sector->interpolated_floor_height);
#endif
        }

        if (EDGE_IMAGE_IS_SKY(sub->sector->ceiling) && view_z < sub->sector->sky_height)
        {
#ifdef EDGE_SOKOL
            BSPQueueSkyPlane(sub, sub->sector->sky_height);
#else
            RenderSkyPlane(sub, sub->sector->sky_height);
#endif
        }
    }

    float floor_h = sector->interpolated_floor_height;
    float ceil_h  = sector->interpolated_ceiling_height;

    MapSurface *floor_s = &sector->floor;
    MapSurface *ceil_s  = &sector->ceiling;

    RegionProperties *props = sector->active_properties;

    // Boom compatibility -- deep water FX
    if (sector->height_sector != nullptr)
    {
        if (view_height_zone == kHeightZoneA && view_z > sector->height_sector->interpolated_ceiling_height)
        {
            floor_h = sector->height_sector->interpolated_ceiling_height;
            ceil_h  = sector->interpolated_ceiling_height;
            floor_s = &sector->height_sector->floor;
            ceil_s  = &sector->height_sector->ceiling;
            props   = sector->height_sector->active_properties;
        }
        else if (view_height_zone == kHeightZoneC && view_z < sector->height_sector->interpolated_floor_height)
        {
            floor_h = sector->interpolated_floor_height;
            ceil_h  = sector->height_sector->interpolated_floor_height;
            floor_s = &sector->height_sector->floor;
            ceil_s  = &sector->height_sector->ceiling;
            props   = sector->height_sector->active_properties;
        }
        else
        {
            floor_h = sector->height_sector->interpolated_floor_height;
            ceil_h  = sector->height_sector->interpolated_ceiling_height;
        }
        if (EDGE_IMAGE_IS_SKY(*floor_s) && view_z > floor_h)
        {
#ifdef EDGE_SOKOL
            BSPQueueSkyPlane(sub, floor_h);
#else
            RenderSkyPlane(sub, floor_h);
#endif
        }
        if (EDGE_IMAGE_IS_SKY(*ceil_s) && view_z < sub->sector->sky_height)
        {
#ifdef EDGE_SOKOL
            BSPQueueSkyPlane(sub, sub->sector->sky_height);
#else
            RenderSkyPlane(sub, sub->sector->sky_height);
#endif
        }
    }
    // -AJA- 2004/04/22: emulate the Deep-Water TRICK
    else if (sub->deep_water_reference != nullptr)
    {
        floor_h = sub->deep_water_reference->interpolated_floor_height;
        floor_s = &sub->deep_water_reference->floor;

        ceil_h = sub->deep_water_reference->interpolated_ceiling_height;
        ceil_s = &sub->deep_water_reference->ceiling;
    }

    // the OLD method of Boom deep water (the BOOMTEX flag)
    Extrafloor *boom_ef = sector->bottom_liquid ? sector->bottom_liquid : sector->bottom_extrafloor;
    if (boom_ef && (boom_ef->extrafloor_definition->type_ & kExtraFloorTypeBoomTex))
        floor_s = &boom_ef->extrafloor_line->front_sector->floor;

    // add in each extrafloor, traversing strictly upwards

    Extrafloor *S = sector->bottom_extrafloor;
    Extrafloor *L = sector->bottom_liquid;

    while (S || L)
    {
        Extrafloor *C = nullptr;

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
        if (C->bottom_height < floor_h || C->bottom_height > sector->interpolated_ceiling_height)
            continue;

        AddNewDrawFloor(K, C, floor_h, C->bottom_height, C->top_height, floor_s, C->bottom, C->properties);

        floor_s = C->top;
        floor_h = C->top_height;
    }

    AddNewDrawFloor(K, nullptr, floor_h, ceil_h, ceil_h, floor_s, ceil_s, props);

    K->floors[0]->is_lowest                     = true;
    K->floors[K->floors.size() - 1]->is_highest = true;

    // handle each sprite in the subsector.  Must be done before walls,
    // since the wall code will update the 1D occlusion buffer.

    if (draw_culling.d_)
    {
        bool skip = true;

        for (Seg *seg = sub->segs; seg; seg = seg->subsector_next)
        {
            if (bsp_mirror_set.SegOnPortal(seg))
                continue;

            float sx1 = seg->vertex_1->X;
            float sy1 = seg->vertex_1->Y;

            float sx2 = seg->vertex_2->X;
            float sy2 = seg->vertex_2->Y;

            if (PointToSegDistance({{sx1, sy1}}, {{sx2, sy2}}, {{view_x, view_y}}) <= (renderer_far_clip.f_ + 500.0f))
            {
                skip = false;
                break;
            }
        }

        if (!skip)
        {
            for (MapObject *mo = sub->thing_list; mo; mo = mo->subsector_next_)
            {
                BSPWalkThing(K, mo);
            }
            // clip 1D occlusion buffer.
            for (Seg *seg = sub->segs; seg; seg = seg->subsector_next)
            {
                BSPWalkSeg(K, seg);
            }

            // add drawsub to list (closest -> furthest)
            int32_t active_mirrors = bsp_mirror_set.TotalActive();
            if (active_mirrors > 0)
                bsp_mirror_set.PushSubsector(active_mirrors - 1, K);
            else
            {
#ifdef EDGE_SOKOL
                BSPQueueDrawSubsector(K);
#else
                draw_subsector_list.push_back(K);
#endif
            }
        }
    }
    else
    {
        for (MapObject *mo = sub->thing_list; mo; mo = mo->subsector_next_)
        {
            BSPWalkThing(K, mo);
        }
        // clip 1D occlusion buffer.
        for (Seg *seg = sub->segs; seg; seg = seg->subsector_next)
        {
            BSPWalkSeg(K, seg);
        }

        // add drawsub to list (closest -> furthest)
        int32_t active_mirrors = bsp_mirror_set.TotalActive();
        if (active_mirrors > 0)
            bsp_mirror_set.PushSubsector(active_mirrors - 1, K);
        else
        {
#ifdef EDGE_SOKOL
            BSPQueueDrawSubsector(K);
#else
            draw_subsector_list.push_back(K);
#endif
        }
    }
}

//
// BspWalkNode
//
// Walks all subsectors below a given node, traversing subtree
// recursively, collecting information.  Just call with BSP root.
//
void BSPWalkNode(unsigned int bspnum)
{
    EDGE_ZoneScoped;

    BSPNode *node;
    int      side;

    // Found a subsector?
    if (bspnum & kLeafSubsector)
    {
        BSPWalkSubsector(bspnum & (~kLeafSubsector));
        return;
    }

    node = &level_nodes[bspnum];

    // Decide which side the view point is on.

    DividingLine nd_div;

    nd_div.x       = node->divider.x;
    nd_div.y       = node->divider.y;
    nd_div.delta_x = node->divider.x + node->divider.delta_x;
    nd_div.delta_y = node->divider.y + node->divider.delta_y;

    bsp_mirror_set.Coordinate(nd_div.x, nd_div.y);
    bsp_mirror_set.Coordinate(nd_div.delta_x, nd_div.delta_y);

    if (bsp_mirror_set.Reflective())
    {
        float tx       = nd_div.x;
        nd_div.x       = nd_div.delta_x;
        nd_div.delta_x = tx;
        float ty       = nd_div.y;
        nd_div.y       = nd_div.delta_y;
        nd_div.delta_y = ty;
    }

    nd_div.delta_x -= nd_div.x;
    nd_div.delta_y -= nd_div.y;

    side = PointOnDividingLineSide(view_x, view_y, &nd_div);

    // Recursively divide front space.
    if (BSPCheckBBox(node->bounding_boxes[side]))
        BSPWalkNode(node->children[side]);

    // Recursively divide back space.
    if (BSPCheckBBox(node->bounding_boxes[side ^ 1]))
        BSPWalkNode(node->children[side ^ 1]);
}

#ifdef EDGE_SOKOL

#ifdef BSP_MULTITHREAD

static int32_t BSPTraverseProc(void *thread_data)
{
    EPI_UNUSED(thread_data);

    while (thread_atomic_int_load(&bsp_thread.exit_flag_) == 0)
    {
        if (thread_signal_wait(&bsp_thread.signal_start_, THREAD_SIGNAL_WAIT_INFINITE))
        {
            if (thread_atomic_int_load(&bsp_thread.exit_flag_))
            {
                break;
            }

            current_batch = nullptr;

            // walk the bsp tree
            BSPWalkNode(root_node);

            if (current_batch && current_batch->num_items_)
            {
                BSPQueueRenderBatch(current_batch);
            }

            thread_atomic_int_store(&bsp_thread.traverse_finished_, 1);
        }
    }

    return 0;
}

static RenderBatch render_batches[kMaxRenderBatch];
static uint32_t    render_batch_counter = 0;

void BSPQueueRenderBatch(RenderBatch *batch)
{
    thread_queue_produce(&bsp_thread.queue_, batch, 100);
}

// TODO: This isn't really a ring buffer
static RenderBatch *GetRenderBatch()
{
    RenderBatch *batch = &render_batches[render_batch_counter++];
    EPI_CLEAR_MEMORY(batch, RenderBatch, 1);
    render_batch_counter %= kMaxRenderBatch;
    return batch;
}

static RenderItem *GetRenderItem()
{
    if (!current_batch || current_batch->num_items_ == kRenderItemBatchSize)
    {
        if (current_batch)
        {
            BSPQueueRenderBatch(current_batch);
        }

        current_batch = GetRenderBatch();
    }

    return &current_batch->items_[current_batch->num_items_++];
}

void BSPQueueSkyWall(Seg *seg, float h1, float h2)
{
    RenderItem *item = GetRenderItem();

    item->type_    = kRenderSkyWall;
    item->height1_ = h1;
    item->height2_ = h2;
    item->wallSeg_ = seg;
}

void BSPQueueSkyPlane(Subsector *sub, float h)
{
    RenderItem *item = GetRenderItem();

    item->type_      = kRenderSkyPlane;
    item->height1_   = h;
    item->wallPlane_ = sub;
}

void BSPQueueDrawSubsector(DrawSubsector *subsector)
{
    subsector->solid = true;
    RenderItem *item = GetRenderItem();
    item->type_      = kRenderSubsector;
    item->subsector_ = subsector;
}

RenderBatch *BSPReadRenderBatch()
{
    RenderBatch *batch = (RenderBatch *)thread_queue_consume(&bsp_thread.queue_, 0);
    return batch;
}

static bool traverse_stop_signalled;

void BSPTraverse()
{
    traverse_stop_signalled = false;
    thread_atomic_int_store(&bsp_thread.traverse_finished_, 0);
    thread_signal_raise(&bsp_thread.signal_start_);
}

bool BSPTraversing()
{
    if (!traverse_stop_signalled)
    {
        traverse_stop_signalled = !!thread_atomic_int_load(&bsp_thread.traverse_finished_);
    }

    if (!thread_queue_count(&bsp_thread.queue_) && traverse_stop_signalled)
    {
        return false;
    }

    return true;
}

void BSPStartThread()
{
    thread_atomic_int_store(&bsp_thread.exit_flag_, 0);
    thread_atomic_int_store(&bsp_thread.traverse_finished_, 1);
    thread_signal_init(&bsp_thread.signal_start_);
    thread_queue_init(&bsp_thread.queue_, kMaxRenderBatch, (void **)bsp_thread.render_queue_, 0);
    bsp_thread.thread_ = thread_create(BSPTraverseProc, nullptr, THREAD_STACK_SIZE_DEFAULT);
}
void BSPStopThread()
{
    thread_atomic_int_store(&bsp_thread.exit_flag_, 1);
    thread_signal_raise(&bsp_thread.signal_start_);
    thread_join(bsp_thread.thread_);
    thread_signal_term(&bsp_thread.signal_start_);
}

#else

constexpr uint32_t  kRenderBatchMax      = 65536 * 2;
static uint32_t     render_batch_counter = 0;
static uint32_t     render_batch_travese = 0;
static RenderBatch *render_batches       = nullptr;

static RenderBatch *GetRenderBatch()
{
    if (render_batch_counter >= kRenderBatchMax)
    {
        FatalError("GetRenderBatch: Exceeded max render batches");
    }

    return &render_batches[render_batch_counter++];
}

static RenderItem *GetRenderItem()
{
    if (!current_batch || current_batch->num_items_ == kRenderItemBatchSize)
    {
        current_batch = GetRenderBatch();
    }

    return &current_batch->items_[current_batch->num_items_++];
}

void BSPQueueSkyWall(Seg *seg, float h1, float h2)
{
    RenderItem *item = GetRenderItem();

    item->type_    = kRenderSkyWall;
    item->height1_ = h1;
    item->height2_ = h2;
    item->wallSeg_ = seg;
}

void BSPQueueSkyPlane(Subsector *sub, float h)
{
    RenderItem *item = GetRenderItem();

    item->type_      = kRenderSkyPlane;
    item->height1_   = h;
    item->wallPlane_ = sub;
}

void BSPQueueDrawSubsector(DrawSubsector *subsector)
{
    subsector->solid = true;
    RenderItem *item = GetRenderItem();
    item->type_      = kRenderSubsector;
    item->subsector_ = subsector;
}

void BSPStartThread()
{
    if (render_batches)
    {
        FatalError("BSPStartThread: Render Batches is not null");
    }

    render_batches = (RenderBatch *)malloc(sizeof(RenderBatch) * kRenderBatchMax);
}
void BSPStopThread()
{
    if (render_batches)
    {
        free(render_batches);
        render_batches = nullptr;
    }
}

void BSPTraverse()
{
    current_batch        = nullptr;
    render_batch_counter = 0;
    render_batch_travese = 0;
    EPI_CLEAR_MEMORY(render_batches, RenderBatch, kRenderBatchMax);

    // walk the bsp tree
    BspWalkNode(root_node);
}

bool BSPTraversing()
{
    if (render_batch_counter == render_batch_travese)
    {
        return false;
    }

    return true;
}

RenderBatch *BSPReadRenderBatch()
{
    return &render_batches[render_batch_travese++];
}

#endif
#endif