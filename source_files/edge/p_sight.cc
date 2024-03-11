//----------------------------------------------------------------------------
//  EDGE Sight Code
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
//  -AJA- 2001/07/24: New sight code.
//
//  Works like this: First we do what the original DOOM source did,
//  traverse the BSP to find lines that intersecting the LOS ray.  We
//  keep the top/bottom slope optimisation too.
//
//  The difference is that we remember where abouts the intercepts
//  occur, and if the basic LOS check succeeds (e.g. no one-sided
//  lines blocking view) then we use the intercept list to check for
//  extrafloors that block the view.
//

#include <math.h>

#include <vector>

#include "AlmostEquals.h"
#include "dm_data.h"
#include "dm_defs.h"
#include "dm_structs.h"
#include "m_bbox.h"
#include "p_local.h"
#include "r_misc.h"
#include "r_state.h"

#define DEBUG_SIGHT 0

extern unsigned int root_node;

struct LineOfSight
{
    // source position (dx/dy is vector to dest)
    DividingLine    source;
    float        source_z;
    Subsector *source_subsector;

    // dest position
    HMM_Vec2     destination;
    float        destination_z;
    Subsector *destination_subsector;

    // angle from src->dest, for fast seg check
    BAMAngle angle;

    // slopes from source to top/bottom of destination.  They will be
    // updated when one or two-sided lines are encountered.  If they
    // close up completely, then no other heights need to be checked.
    //
    // NOTE: the values are not real slopes, the distance from src to
    //       dest is the implied denominator.
    //
    float top_slope;
    float bottom_slope;

    // bounding box on LOS line (idea pinched from PrBOOM).
    float bounding_box[4];

    // true if one of the sectors contained extrafloors
    bool saw_extrafloors;

    // true if one of the sectors contained vertex slopes
    bool saw_vertex_slopes;
};

static LineOfSight sight_check;

// intercepts found during first pass

struct WallIntercept
{
    // fractional distance, 0.0 -> 1.0
    float along;

    // sector that faces the source from this intercept point
    Sector *sector;
};

// intercept array
static std::vector<WallIntercept> wall_intercepts;

static inline void AddSightIntercept(float frac, Sector *sec)
{
    WallIntercept WI;

    WI.along  = frac;
    WI.sector = sec;

    wall_intercepts.push_back(WI);
}

//
// CrossSubsector
//
// Returns false if LOS is blocked by the given subsector, otherwise
// true.  Note: extrafloors are not checked here.
//
static bool CrossSubsector(Subsector *sub)
{
    Seg  *seg;
    Line *ld;

    int s1, s2;

    Sector *front;
    Sector *back;
    DividingLine divl;

    float frac;
    float slope;

    // check lines
    for (seg = sub->segs; seg != nullptr; seg = seg->subsector_next)
    {
        if (seg->miniseg) continue;

        // ignore segs that face away from the source.  We only want to
        // process linedefs on the _far_ side of each subsector.
        //
        if ((BAMAngle)(seg->angle - sight_check.angle) < kBAMAngle180) continue;

        ld = seg->linedef;

        // line already checked ? (e.g. multiple segs on it)
        if (ld->valid_count == valid_count) continue;

        ld->valid_count = valid_count;

        // line outside of bbox ?
        if (ld->bounding_box[kBoundingBoxLeft] >
                sight_check.bounding_box[kBoundingBoxRight] ||
            ld->bounding_box[kBoundingBoxRight] <
                sight_check.bounding_box[kBoundingBoxLeft] ||
            ld->bounding_box[kBoundingBoxBottom] >
                sight_check.bounding_box[kBoundingBoxTop] ||
            ld->bounding_box[kBoundingBoxTop] <
                sight_check.bounding_box[kBoundingBoxBottom])
            continue;

        // does linedef cross LOS ?
        s1 = PointOnDividingLineSide(ld->vertex_1->X, ld->vertex_1->Y, &sight_check.source);
        s2 = PointOnDividingLineSide(ld->vertex_2->X, ld->vertex_2->Y, &sight_check.source);

        if (s1 == s2) continue;

        // linedef crosses LOS (extended to infinity), now check if the
        // cross point lies within the finite LOS range.
        //
        divl.x  = ld->vertex_1->X;
        divl.y  = ld->vertex_1->Y;
        divl.delta_x = ld->delta_x;
        divl.delta_y = ld->delta_y;

        s1 = PointOnDividingLineSide(sight_check.source.x, sight_check.source.y,
                                     &divl);
        s2 = PointOnDividingLineSide(sight_check.destination.X,
                                     sight_check.destination.Y, &divl);

        if (s1 == s2) continue;

        // stop because it is not two sided anyway
        if (!(ld->flags & MLF_TwoSided) || ld->blocked) { return false; }

        // line explicitly blocks sight ?  (XDoom compatibility)
        if (ld->flags & MLF_SightBlock) return false;

        // -AJA- 2001/11/11: closed Sliding door ?
        if (ld->slide_door && !ld->slide_door->s_.see_through_ &&
            !ld->slider_move)
        {
            return false;
        }

        front = seg->front_sector;
        back  = seg->back_sector;

        SYS_ASSERT(back);

        // compute intercept vector (fraction from 0 to 1)
        {
            float num, den;

            den = divl.delta_y * sight_check.source.delta_x -
                  divl.delta_x * sight_check.source.delta_y;

            // parallel ?
            // -AJA- probably can't happen due to the above Divline checks
            if (fabs(den) < 0.0001) continue;

            num = (divl.x - sight_check.source.x) * divl.delta_y +
                  (sight_check.source.y - divl.y) * divl.delta_x;

            frac = num / den;

            // too close to source ?
            if (frac < 0.0001f) continue;
        }

        if (!AlmostEquals(front->floor_height, back->floor_height))
        {
            float openbottom =
                HMM_MAX(ld->front_sector->floor_height, ld->back_sector->floor_height);
            slope = (openbottom - sight_check.source_z) / frac;
            if (slope > sight_check.bottom_slope)
                sight_check.bottom_slope = slope;
        }

        if (!AlmostEquals(front->ceiling_height, back->ceiling_height))
        {
            float opentop = HMM_MIN(ld->front_sector->ceiling_height, ld->back_sector->ceiling_height);
            slope         = (opentop - sight_check.source_z) / frac;
            if (slope < sight_check.top_slope) sight_check.top_slope = slope;
        }

        // did our slope range close up ?
        if (sight_check.top_slope <= sight_check.bottom_slope) return false;

        // shouldn't be any more matching linedefs
        AddSightIntercept(frac, front);
        return true;
    }

    // LOS ray went completely passed the subsector
    return true;
}

//
// CheckSightBSP
//
// Returns false if LOS is blocked by the given node, otherwise true.
// Note: extrafloors are not checked here.
//
static bool CheckSightBSP(unsigned int bspnum)
{
    while (!(bspnum & NF_V5_SUBSECTOR))
    {
        BspNode *node = level_nodes + bspnum;
        int     s1, s2;

#if (DEBUG_SIGHT >= 2)
        LogDebug("CheckSightBSP: node %d (%1.1f,%1.1f) + (%1.1f,%1.1f)\n",
                 bspnum, node->div.x, node->div.y, node->div.delta_x, node->div.delta_y);
#endif

        // decide which side the src and dest points are on
        s1 = PointOnDividingLineSide(sight_check.source.x, sight_check.source.y,
                                     &node->divider);
        s2 = PointOnDividingLineSide(sight_check.destination.X,
                                     sight_check.destination.Y, &node->divider);

#if (DEBUG_SIGHT >= 2)
        LogDebug("  Sides: %d %d\n", s1, s2);
#endif

        // If sides are different, we must recursively check both.
        // NOTE WELL: we do the source side first, so that subsectors are
        // visited in the correct order (closest -> furthest away).

        if (s1 != s2)
        {
            if (!CheckSightBSP(node->children[s1])) return false;
        }

        bspnum = node->children[s2];
    }

    bspnum &= ~NF_V5_SUBSECTOR;

    SYS_ASSERT(bspnum < (unsigned int)total_level_subsectors);

    {
        Subsector *sub = level_subsectors + bspnum;

#if (DEBUG_SIGHT >= 2)
        LogDebug("  Subsec %d  SEC %d\n", bspnum, sub->sector - sectors);
#endif

        if (sub->sector->extrafloor_used > 0) sight_check.saw_extrafloors = true;

        if (sub->sector->floor_vertex_slope || sub->sector->ceiling_vertex_slope)
            sight_check.saw_vertex_slopes = true;

        // when target subsector is reached, there are no more lines to
        // check, since we only check lines on the _far_ side of the
        // subsector and the target object is inside its subsector.

        if (sub != sight_check.destination_subsector)
            return CrossSubsector(sub);

        AddSightIntercept(1.0f, sub->sector);
    }

    return true;
}

//
// CheckSightIntercepts
//
// Returns false if LOS is blocked by extrafloors, otherwise true.
//
static bool CheckSightIntercepts(float slope)
{
    int       i, j;
    Sector *sec;

    float last_h = sight_check.source_z;
    float cur_h;

#if (DEBUG_SIGHT >= 1)
    LogDebug("INTERCEPTS  slope %1.0f\n", slope);
#endif

    for (i = 0; i < (int)wall_intercepts.size(); i++, last_h = cur_h)
    {
        bool blocked = true;

        cur_h = sight_check.source_z + slope * wall_intercepts[i].along;

#if (DEBUG_SIGHT >= 1)
        LogDebug("  %d/%d  FRAC %1.4f  SEC %d  H=%1.4f/%1.4f\n", i + 1,
                 wall_intercepts.size(), wall_intercepts[i].along,
                 wall_intercepts[i].sector - sectors, last_h, cur_h);
#endif

        // check all the sight gaps.
        sec = wall_intercepts[i].sector;

        for (j = 0; j < sec->sight_gap_number; j++)
        {
            float z1 = sec->sight_gaps[j].floor;
            float z2 = sec->sight_gaps[j].ceiling;

#if (DEBUG_SIGHT >= 3)
            LogDebug("    SIGHT GAP [%d] = %1.1f .. %1.1f\n", j, z1, z2);
#endif

            if (z1 <= last_h && last_h <= z2 && z1 <= cur_h && cur_h <= z2)
            {
                blocked = false;
                break;
            }
        }

        if (blocked) return false;
    }

    return true;
}

//
// CheckSightSameSubsector
//
// When the subsector is the same, we only need to check whether a
// non-SeeThrough extrafloor gets in the way.
//
static bool CheckSightSameSubsector(MapObject *src, MapObject *dest)
{
    int       j;
    Sector *sec;

    float lower_z;
    float upper_z;

    if (sight_check.source_z < dest->z)
    {
        lower_z = sight_check.source_z;
        upper_z = dest->z;
    }
    else if (sight_check.source_z > dest->z + dest->height_)
    {
        lower_z = dest->z + dest->height_;
        upper_z = sight_check.source_z;
    }
    else { return true; }

    // check all the sight gaps.
    sec = src->subsector_->sector;

    for (j = 0; j < sec->sight_gap_number; j++)
    {
        float z1 = sec->sight_gaps[j].floor;
        float z2 = sec->sight_gaps[j].ceiling;

        if (z1 <= lower_z && upper_z <= z2) return true;
    }

    return false;
}

bool P_CheckSight(MapObject *src, MapObject *dest)
{
    // -ACB- 1998/07/20 t2 is Invisible, t1 cannot possibly see it.
    if (AlmostEquals(dest->visibility_, 0.0f)) return false;

    int n, num_div;

    float dest_heights[5];
    float dist_a;

    // First check for trivial rejection.

    SYS_ASSERT(src->subsector_);
    SYS_ASSERT(dest->subsector_);

    // An unobstructed LOS is possible.
    // Now look from eyes of t1 to any part of t2.

    valid_count++;

    // The "eyes" of a thing is 75% of its height.
    SYS_ASSERT(src->info_);
    sight_check.source_z = src->z + src->height_ * src->info_->viewheight_;

    sight_check.source.x         = src->x;
    sight_check.source.y         = src->y;
    sight_check.source.delta_x        = dest->x - src->x;
    sight_check.source.delta_y        = dest->y - src->y;
    sight_check.source_subsector = src->subsector_;

    sight_check.destination.X         = dest->x;
    sight_check.destination.Y         = dest->y;
    sight_check.destination_subsector = dest->subsector_;

    sight_check.bottom_slope = dest->z - sight_check.source_z;
    sight_check.top_slope    = sight_check.bottom_slope + dest->height_;

    // destination out of object's DDF slope range ?
    dist_a = ApproximateDistance(sight_check.source.delta_x, sight_check.source.delta_y);

    if (src->info_->sight_distance_ > -1)  // if we have sight_distance set
    {
        if (src->info_->sight_distance_ < dist_a)
            return false;  // too far away for this thing to see
    }

#if (DEBUG_SIGHT >= 1)
    LogDebug("\n");
    LogDebug("P_CheckSight:\n");
    LogDebug("  Src: [%s] @ (%1.0f,%1.0f) in sub %d SEC %d\n", src->info->name,
             sight_check.source.x, sight_check.source.y,
             sight_check.source_subsector - subsectors,
             sight_check.source_subsector->sector - sectors);
    LogDebug("  Dest: [%s] @ (%1.0f,%1.0f) in sub %d SEC %d\n",
             dest->info->name, sight_check.destination.x,
             sight_check.destination.y,
             sight_check.destination_subsector - subsectors,
             sight_check.destination_subsector->sector - sectors);
    LogDebug("  Angle: %1.0f\n", ANG_2_FLOAT(sight_check.angle));
#endif

    if (sight_check.top_slope < dist_a * -src->info_->sight_slope_)
        return false;

    if (sight_check.bottom_slope > dist_a * src->info_->sight_slope_)
        return false;

    // -AJA- handle the case where no linedefs are crossed
    if (src->subsector_ == dest->subsector_)
    {
        return CheckSightSameSubsector(src, dest);
    }

    sight_check.angle =
        RendererPointToAngle(sight_check.source.x, sight_check.source.y,
                       sight_check.destination.X, sight_check.destination.Y);

    sight_check.bounding_box[kBoundingBoxLeft] =
        HMM_MIN(sight_check.source.x, sight_check.destination.X);
    sight_check.bounding_box[kBoundingBoxRight] =
        HMM_MAX(sight_check.source.x, sight_check.destination.X);
    sight_check.bounding_box[kBoundingBoxBottom] =
        HMM_MIN(sight_check.source.y, sight_check.destination.Y);
    sight_check.bounding_box[kBoundingBoxTop] =
        HMM_MAX(sight_check.source.y, sight_check.destination.Y);

    wall_intercepts.clear();  // FIXME

    sight_check.saw_extrafloors   = false;
    sight_check.saw_vertex_slopes = false;

    // initial pass -- check for basic blockage & create intercepts
    if (!CheckSightBSP(root_node)) return false;

    // no extrafloors or vertslopes encountered ?  Then the checks made by
    // CheckSightBSP are sufficient.  (-AJA- double check this)
    //
    if (!sight_check.saw_extrafloors && !sight_check.saw_vertex_slopes)
        return true;

    // Leveraging the existing hitscan attack code is easier than trying to
    // wrangle this stuff
    if (sight_check.saw_vertex_slopes)
    {
        float objslope;
        P_AimLineAttack(src, sight_check.angle, 64000, &objslope);
        P_LineAttack(src, sight_check.angle, 64000, objslope, 0, nullptr,
                     nullptr);
        bool slope_sight_good = dest->slope_sight_hit_;
        if (slope_sight_good)
        {
            dest->slope_sight_hit_ = false;  // reset for future sight checks
            return true;
        }
        else
            return false;
    }

    // Enter the HackMan...  The new sight code only tests LOS to one
    // destination height.  (The old code kept track of angles -- but
    // this approach is not well suited for extrafloors).  The number of
    // points we test depends on the destination: 5 for players, 3 for
    // monsters, 1 for everything else.

    if (dest->player_)
    {
        num_div         = 5;
        dest_heights[0] = dest->z;
        dest_heights[1] = dest->z + dest->height_ * 0.25f;
        dest_heights[2] = dest->z + dest->height_ * 0.50f;
        dest_heights[3] = dest->z + dest->height_ * 0.75f;
        dest_heights[4] = dest->z + dest->height_;
    }
    else if (dest->extended_flags_ & kExtendedFlagMonster)
    {
        num_div         = 3;
        dest_heights[0] = dest->z;
        dest_heights[1] = dest->z + dest->height_ * 0.5f;
        dest_heights[2] = dest->z + dest->height_;
    }
    else
    {
        num_div         = 1;
        dest_heights[0] = dest->z + dest->height_ * 0.5f;
    }

    // use intercepts to check extrafloor heights
    //
    for (n = 0; n < num_div; n++)
    {
        float slope = dest_heights[n] - sight_check.source_z;

        if (slope > sight_check.top_slope || slope < sight_check.bottom_slope)
            continue;

        if (CheckSightIntercepts(slope)) return true;
    }

    return false;
}

bool CheckSightToPoint(MapObject *src, float x, float y, float z)
{
    Subsector *dest_sub = RendererPointInSubsector(x, y);

    if (dest_sub == src->subsector_) return true;

    valid_count++;

    sight_check.source.x  = src->x;
    sight_check.source.y  = src->y;
    sight_check.source_z  = src->z + src->height_ * src->info_->viewheight_;
    sight_check.source.delta_x = x - src->x;
    sight_check.source.delta_y = y - src->y;
    sight_check.source_subsector = src->subsector_;

    sight_check.destination.X         = x;
    sight_check.destination.Y         = y;
    sight_check.destination_z         = z;
    sight_check.destination_subsector = dest_sub;

    sight_check.bottom_slope = z - 1.0f - sight_check.source_z;
    sight_check.top_slope    = z + 1.0f - sight_check.source_z;

    sight_check.angle =
        RendererPointToAngle(sight_check.source.x, sight_check.source.y,
                       sight_check.destination.X, sight_check.destination.Y);

    sight_check.bounding_box[kBoundingBoxLeft] =
        HMM_MIN(sight_check.source.x, sight_check.destination.X);
    sight_check.bounding_box[kBoundingBoxRight] =
        HMM_MAX(sight_check.source.x, sight_check.destination.X);
    sight_check.bounding_box[kBoundingBoxBottom] =
        HMM_MIN(sight_check.source.y, sight_check.destination.Y);
    sight_check.bounding_box[kBoundingBoxTop] =
        HMM_MAX(sight_check.source.y, sight_check.destination.Y);

    wall_intercepts.clear();

    sight_check.saw_extrafloors = false;

    if (!CheckSightBSP(root_node)) return false;

#if 1
    if (!sight_check.saw_extrafloors) return true;
#endif

    float slope = z - sight_check.source_z;

    if (slope > sight_check.top_slope || slope < sight_check.bottom_slope)
        return false;

    return CheckSightIntercepts(slope);
}

//
// P_CheckSightApproxVert
//
// Quickly check that object t1 can vertically see object t2.  Only
// takes extrafloors into account.  Mainly used so that archviles
// don't resurrect monsters that are completely out of view in another
// vertical region.  Returns true if sight possible, false otherwise.
//
bool P_CheckSightApproxVert(MapObject *src, MapObject *dest)
{
    SYS_ASSERT(src->info_);

    sight_check.source_z = src->z + src->height_ * src->info_->viewheight_;

    return CheckSightSameSubsector(src, dest);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
