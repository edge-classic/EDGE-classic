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
#include "r_misc.h"
#include "r_modes.h"
#include "r_occlude.h"
#include "r_render.h"
#include "r_shader.h"
#include "r_sky.h"
#include "r_things.h"
#include "r_units.h"

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

Subsector *current_subsector;
Seg       *current_seg;

std::list<DrawSubsector *> draw_subsector_list;

static constexpr uint8_t kMaximumEdgeVertices = 20;

static void UpdateSectorInterpolation(Sector *sector)
{
    if (uncapped_frames.d_ && !time_stop_active && !paused && !erraticism_active && !menu_active && !rts_menu_active)
    {
        // Interpolate between current and last floor/ceiling position.
        if (!AlmostEquals(sector->floor_height, sector->old_floor_height))
            sector->interpolated_floor_height =
                HMM_Lerp(sector->old_floor_height, fractional_tic, sector->floor_height);
        else
            sector->interpolated_floor_height = sector->floor_height;
        if (!AlmostEquals(sector->ceiling_height, sector->old_ceiling_height))
            sector->interpolated_ceiling_height =
                HMM_Lerp(sector->old_ceiling_height, fractional_tic, sector->ceiling_height);
        else
            sector->interpolated_ceiling_height = sector->ceiling_height;
    }
    else
    {
        sector->interpolated_floor_height   = sector->floor_height;
        sector->interpolated_ceiling_height = sector->ceiling_height;
    }
}

static void BSPWalkMirror(DrawSubsector *dsub, Seg *seg, BAMAngle left, BAMAngle right, bool is_portal)
{
    DrawMirror *mir = GetDrawMirror();
    mir->seg        = seg;
    mir->draw_subsectors.clear();

    mir->left      = view_angle + left;
    mir->right     = view_angle + right;
    mir->is_portal = is_portal;

    dsub->mirrors.push_back(mir);

#if defined(EDGE_GL_ES2)
    // GL4ES mirror fix for renderlist
    gl4es_flush();
#endif

    // push mirror (translation matrix)
    MirrorPush(mir);

    Subsector *save_sub = current_subsector;

    BAMAngle save_clip_L = clip_left;
    BAMAngle save_clip_R = clip_right;
    BAMAngle save_scope  = clip_scope;

    clip_left  = left;
    clip_right = right;
    clip_scope = left - right;

    // perform another BSP walk
    BspWalkNode(root_node);

    current_subsector = save_sub;

    clip_left  = save_clip_L;
    clip_right = save_clip_R;
    clip_scope = save_scope;

    // pop mirror
    MirrorPop();

#if defined(EDGE_GL_ES2)
    // GL4ES mirror fix for renderlist
    gl4es_flush();
#endif
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
    if (MirrorSegOnPortal(seg))
        return;

    float sx1 = seg->vertex_1->X;
    float sy1 = seg->vertex_1->Y;

    float sx2 = seg->vertex_2->X;
    float sy2 = seg->vertex_2->Y;

    // when there are active mirror planes, segs not only need to
    // be flipped across them but also clipped across them.

    int32_t active_mirrors = MirrorTotalActive();
    if (active_mirrors > 0)
    {
        for (int i = active_mirrors - 1; i >= 0; i--)
        {
            MirrorTransform(i, sx1, sy1);
            MirrorTransform(i, sx2, sy2);

            if (!MirrorIsPortal(i))
            {
                float tmp_x = sx1;
                sx1         = sx2;
                sx2         = tmp_x;
                float tmp_y = sy1;
                sy1         = sy2;
                sy2         = tmp_y;
            }

            Seg *clipper = MirrorSeg(i);

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

    if (bsector)
        UpdateSectorInterpolation(bsector);

    // --- handle sky (using the depth buffer) ---
    float       f_fh    = 0;
    float       f_ch    = 0;
    float       b_fh    = 0;
    float       b_ch    = 0;
    MapSurface *f_floor = nullptr;
    MapSurface *f_ceil  = nullptr;
    MapSurface *b_floor = nullptr;
    MapSurface *b_ceil  = nullptr;

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
            QueueSkyWall(seg, f_fh, b_fh);
        }
    }

    if (EDGE_IMAGE_IS_SKY(*f_ceil))
    {
        if (f_ch < fsector->sky_height && (!bsector || !EDGE_IMAGE_IS_SKY(*b_ceil) || b_fh >= f_ch))
        {
            QueueSkyWall(seg, f_ch, fsector->sky_height);
        }
        else if (bsector && EDGE_IMAGE_IS_SKY(*b_ceil))
        {
            float max_f = HMM_MAX(f_fh, b_fh);

            if (b_ch <= max_f && max_f < fsector->sky_height)
            {
                QueueSkyWall(seg, max_f, fsector->sky_height);
            }
        }
    }
    // -AJA- 2004/08/29: Emulate Sky-Flooding TRICK
    else if (!debug_hall_of_mirrors.d_ && bsector && EDGE_IMAGE_IS_SKY(*b_ceil) && seg->sidedef->top.image == nullptr &&
             b_ch < f_ch)
    {
        QueueSkyWall(seg, b_ch, f_ch);
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
static bool BSPCheckBBox(float *bspcoord)
{
    EDGE_ZoneScoped;

    if (MirrorTotalActive() > 0)
    {
        // a flipped bbox may no longer be axis aligned, hence we
        // need to find the bounding area of the transformed box.
        static float new_bbox[4];

        BoundingBoxClear(new_bbox);

        for (int p = 0; p < 4; p++)
        {
            float tx = bspcoord[(p & 1) ? kBoundingBoxLeft : kBoundingBoxRight];
            float ty = bspcoord[(p & 2) ? kBoundingBoxBottom : kBoundingBoxTop];

            MirrorCoordinate(tx, ty);

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
    current_subsector = sub;

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

    UpdateSectorInterpolation(sector);

    // --- handle sky (using the depth buffer) ---

    if (!sector->height_sector)
    {
        if (EDGE_IMAGE_IS_SKY(sub->sector->floor) && view_z > sub->sector->interpolated_floor_height)
        {
            QueueSkyPlane(sub, sub->sector->interpolated_floor_height);
        }

        if (EDGE_IMAGE_IS_SKY(sub->sector->ceiling) && view_z < sub->sector->sky_height)
        {
            QueueSkyPlane(sub, sub->sector->sky_height);
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
            QueueSkyPlane(sub, floor_h);
        }
        if (EDGE_IMAGE_IS_SKY(*ceil_s) && view_z < sub->sector->sky_height)
        {
            QueueSkyPlane(sub, sub->sector->sky_height);
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
            if (MirrorSegOnPortal(seg))
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
            int32_t active_mirrors = MirrorTotalActive();
            if (active_mirrors > 0)
                MirrorPushSubsector(active_mirrors - 1, K);
            else
                draw_subsector_list.push_back(K);
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
        int32_t active_mirrors = MirrorTotalActive();
        if (active_mirrors > 0)
            MirrorPushSubsector(active_mirrors - 1, K);
        else
            draw_subsector_list.push_back(K);
    }
}

//
// BspWalkNode
//
// Walks all subsectors below a given node, traversing subtree
// recursively, collecting information.  Just call with BSP root.
//
void BspWalkNode(unsigned int bspnum)
{
    EDGE_ZoneScoped;

    BspNode *node;
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

    MirrorCoordinate(nd_div.x, nd_div.y);
    MirrorCoordinate(nd_div.delta_x, nd_div.delta_y);

    if (MirrorReflective())
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
        BspWalkNode(node->children[side]);

    // Recursively divide back space.
    if (BSPCheckBBox(node->bounding_boxes[side ^ 1]))
        BspWalkNode(node->children[side ^ 1]);
}