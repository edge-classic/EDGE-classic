//----------------------------------------------------------------------------
//  EDGE Movement, Collision & Blockmap utility functions
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
// DESCRIPTION:
//   Movement/collision utility functions,
//   as used by function in p_map.c.
//   BLOCKMAP Iterator functions,
//   and some PIT_* functions to use for iteration.
//   Gap/extrafloor utility functions.
//   Touch Node code.
//
// TODO HERE:
//   + make gap routines FatalError if overflow limit.
//

#include <float.h>

#include <algorithm>
#include <vector>

#include "AlmostEquals.h"
#include "common_doomdefs.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "epi.h"
#include "m_bbox.h"
#include "p_local.h"
#include "p_spec.h"
#include "r_state.h"

extern unsigned int root_node;

//
// ApproximateDistance
//
// Gives an estimation of distance (not exact)
//
float ApproximateDistance(float dx, float dy)
{
    dx = fabs(dx);
    dy = fabs(dy);

    return (dy > dx) ? dy + dx / 2 : dx + dy / 2;
}

float ApproximateDistance(float dx, float dy, float dz)
{
    dx = fabs(dx);
    dy = fabs(dy);
    dz = fabs(dz);

    float dxy = (dy > dx) ? dy + dx / 2 : dx + dy / 2;

    return (dz > dxy) ? dz + dxy / 2 : dxy + dz / 2;
}

//
// ApproximateSlope
//
// Gives an estimation of slope (not exact)
//
// -AJA- 1999/09/11: written.
//
float ApproximateSlope(float dx, float dy, float dz)
{
    float dist = ApproximateDistance(dx, dy);

    // kludge to prevent overflow or division by zero.
    if (dist < 1.0f / 32.0f)
        dist = 1.0f / 32.0f;

    return dz / dist;
}

void ComputeIntersection(DividingLine *div, float x1, float y1, float x2, float y2, float *ix, float *iy)
{
    if (AlmostEquals(div->delta_x, 0.0f))
    {
        *ix = div->x;
        *iy = y1 + (y2 - y1) * (div->x - x1) / (x2 - x1);
    }
    else if (AlmostEquals(div->delta_y, 0.0f))
    {
        *iy = div->y;
        *ix = x1 + (x2 - x1) * (div->y - y1) / (y2 - y1);
    }
    else
    {
        // perpendicular distances (unnormalised)
        float p1 = (x1 - div->x) * div->delta_y - (y1 - div->y) * div->delta_x;
        float p2 = (x2 - div->x) * div->delta_y - (y2 - div->y) * div->delta_x;

        *ix = x1 + (x2 - x1) * p1 / (p1 - p2);
        *iy = y1 + (y2 - y1) * p1 / (p1 - p2);
    }
}

//
// PointOnDividingLineSide
//
// Tests which side of the line the given point lies on.
// Returns 0 (front/right) or 1 (back/left).  If the point lies
// directly on the line, result is undefined (either 0 or 1).
//
int PointOnDividingLineSide(float x, float y, DividingLine *div)
{
    float dx, dy;
    float left, right;

    if (AlmostEquals(div->delta_x, 0.0f))
        return ((x <= div->x) ^ (div->delta_y > 0)) ? 0 : 1;

    if (AlmostEquals(div->delta_y, 0.0f))
        return ((y <= div->y) ^ (div->delta_x < 0)) ? 0 : 1;

    dx = x - div->x;
    dy = y - div->y;

    // try to quickly decide by looking at sign bits
    if ((div->delta_y < 0) ^ (div->delta_x < 0) ^ (dx < 0) ^ (dy < 0))
    {
        // left is negative
        if ((div->delta_y < 0) ^ (dx < 0))
            return 1;

        return 0;
    }

    left  = dx * div->delta_y;
    right = dy * div->delta_x;

    return (right < left) ? 0 : 1;
}

//
// PointOnDividingLineThick
//
// Tests which side of the line the given point is on.   The thickness
// parameter determines when the point is considered "on" the line.
// Returns 0 (front/right), 1 (back/left), or 2 (on).
//
int PointOnDividingLineThick(float x, float y, DividingLine *div, float div_len, float thickness)
{
    float dx, dy;
    float left, right;

    if (AlmostEquals(div->delta_x, 0.0f))
    {
        if (fabs(x - div->x) <= thickness)
            return 2;

        return ((x < div->x) ^ (div->delta_y > 0)) ? 0 : 1;
    }

    if (AlmostEquals(div->delta_y, 0.0f))
    {
        if (fabs(y - div->y) <= thickness)
            return 2;

        return ((y < div->y) ^ (div->delta_x < 0)) ? 0 : 1;
    }

    dx = x - div->x;
    dy = y - div->y;

    // need divline's length here to compute proper distances
    left  = (dx * div->delta_y) / div_len;
    right = (dy * div->delta_x) / div_len;

    if (fabs(left - right) < thickness)
        return 2;

    return (right < left) ? 0 : 1;
}

//
// BoxOnLineSide
//
// Considers the line to be infinite
// Returns side 0 or 1, -1 if box crosses the line.
//
int BoxOnLineSide(const float *tmbox, Line *ld)
{
    int p1 = 0;
    int p2 = 0;

    DividingLine div;

    div.x       = ld->vertex_1->X;
    div.y       = ld->vertex_1->Y;
    div.delta_x = ld->delta_x;
    div.delta_y = ld->delta_y;

    switch (ld->slope_type)
    {
    case kLineClipHorizontal:
        p1 = tmbox[kBoundingBoxTop] > ld->vertex_1->Y;
        p2 = tmbox[kBoundingBoxBottom] > ld->vertex_1->Y;
        if (ld->delta_x < 0)
        {
            p1 ^= 1;
            p2 ^= 1;
        }
        break;

    case kLineClipVertical:
        p1 = tmbox[kBoundingBoxRight] < ld->vertex_1->X;
        p2 = tmbox[kBoundingBoxLeft] < ld->vertex_1->X;
        if (ld->delta_y < 0)
        {
            p1 ^= 1;
            p2 ^= 1;
        }
        break;

    case kLineClipPositive:
        p1 = PointOnDividingLineSide(tmbox[kBoundingBoxLeft], tmbox[kBoundingBoxTop], &div);
        p2 = PointOnDividingLineSide(tmbox[kBoundingBoxRight], tmbox[kBoundingBoxBottom], &div);
        break;

    case kLineClipNegative:
        p1 = PointOnDividingLineSide(tmbox[kBoundingBoxRight], tmbox[kBoundingBoxTop], &div);
        p2 = PointOnDividingLineSide(tmbox[kBoundingBoxLeft], tmbox[kBoundingBoxBottom], &div);
        break;
    }

    if (p1 == p2)
        return p1;

    return -1;
}

//
// BoxOnDividingLineSide
//
// Considers the line to be infinite
// Returns side 0 or 1, -1 if box crosses the line.
//
int BoxOnDividingLineSide(const float *tmbox, DividingLine *div)
{
    int p1 = 0;
    int p2 = 0;

    if (AlmostEquals(div->delta_y, 0.0f))
    {
        p1 = tmbox[kBoundingBoxTop] > div->y;
        p2 = tmbox[kBoundingBoxBottom] > div->y;

        if (div->delta_x < 0)
        {
            p1 ^= 1;
            p2 ^= 1;
        }
    }
    else if (AlmostEquals(div->delta_x, 0.0f))
    {
        p1 = tmbox[kBoundingBoxRight] < div->x;
        p2 = tmbox[kBoundingBoxLeft] < div->x;

        if (div->delta_y < 0)
        {
            p1 ^= 1;
            p2 ^= 1;
        }
    }
    else if (div->delta_y / div->delta_x > 0) // optimise ?
    {
        p1 = PointOnDividingLineSide(tmbox[kBoundingBoxLeft], tmbox[kBoundingBoxTop], div);
        p2 = PointOnDividingLineSide(tmbox[kBoundingBoxRight], tmbox[kBoundingBoxBottom], div);
    }
    else
    {
        p1 = PointOnDividingLineSide(tmbox[kBoundingBoxRight], tmbox[kBoundingBoxTop], div);
        p2 = PointOnDividingLineSide(tmbox[kBoundingBoxLeft], tmbox[kBoundingBoxBottom], div);
    }

    if (p1 == p2)
        return p1;

    return -1;
}

int ThingOnLineSide(const MapObject *mo, Line *ld)
{
    float bbox[4];

    bbox[kBoundingBoxLeft]   = mo->x - mo->radius_;
    bbox[kBoundingBoxRight]  = mo->x + mo->radius_;
    bbox[kBoundingBoxBottom] = mo->y - mo->radius_;
    bbox[kBoundingBoxTop]    = mo->y + mo->radius_;

    return BoxOnLineSide(bbox, ld);
}

//------------------------------------------------------------------------
//
//  GAP UTILITY FUNCTIONS
//

static int GapRemoveSolid(VerticalGap *dest, int d_num, float z1, float z2)
{
    int         d;
    int         new_num = 0;
    VerticalGap new_gaps[100];

#ifdef DEVELOPERS
    if (z1 > z2)
        FatalError("RemoveSolid: z1 > z2");
#endif

    for (d = 0; d < d_num; d++)
    {
        if (dest[d].ceiling <= dest[d].floor)
            continue; // ignore empty gaps.

        if (z1 <= dest[d].floor && z2 >= dest[d].ceiling)
            continue; // completely blocks it.

        if (z1 >= dest[d].ceiling || z2 <= dest[d].floor)
        {
            // no intersection.

            new_gaps[new_num].floor   = dest[d].floor;
            new_gaps[new_num].ceiling = dest[d].ceiling;
            new_num++;
            continue;
        }

        // partial intersections.

        if (z1 > dest[d].floor)
        {
            new_gaps[new_num].floor   = dest[d].floor;
            new_gaps[new_num].ceiling = z1;
            new_num++;
        }

        if (z2 < dest[d].ceiling)
        {
            new_gaps[new_num].floor   = z2;
            new_gaps[new_num].ceiling = dest[d].ceiling;
            new_num++;
        }
    }

    memmove(dest, new_gaps, new_num * sizeof(VerticalGap));

    return new_num;
}

static int GapConstruct(VerticalGap *gaps, Sector *sec, MapObject *thing, float floor_slope_z = 0,
                        float ceiling_slope_z = 0)
{
    Extrafloor *ef;

    int num = 1;

    // early out for FUBAR sectors
    if (sec->floor_height >= sec->ceiling_height)
        return 0;

    gaps[0].floor   = sec->floor_height + floor_slope_z;
    gaps[0].ceiling = sec->ceiling_height - ceiling_slope_z;

    for (ef = sec->bottom_extrafloor; ef; ef = ef->higher)
    {
        num = GapRemoveSolid(gaps, num, ef->bottom_height, ef->top_height);
    }

    // -- handle WATER WALKERS --

    if (!thing || !(thing->extended_flags_ & kExtendedFlagWaterWalker))
        return num;

    for (ef = sec->bottom_liquid; ef; ef = ef->higher)
    {
        if (ef->extrafloor_definition && (ef->extrafloor_definition->type_ & kExtraFloorTypeWater))
        {
            num = GapRemoveSolid(gaps, num, ef->bottom_height, ef->top_height);
        }
    }

    return num;
}

static int GapSightConstruct(VerticalGap *gaps, Sector *sec)
{
    Extrafloor *ef;

    int num = 1;

    // early out for closed or FUBAR sectors
    if (sec->ceiling_height <= sec->floor_height)
        return 0;

    gaps[0].floor   = sec->floor_height;
    gaps[0].ceiling = sec->ceiling_height;

    for (ef = sec->bottom_extrafloor; ef; ef = ef->higher)
    {
        if (!ef->extrafloor_definition || !(ef->extrafloor_definition->type_ & kExtraFloorTypeSeeThrough))
            num = GapRemoveSolid(gaps, num, ef->bottom_height, ef->top_height);
    }

    for (ef = sec->bottom_liquid; ef; ef = ef->higher)
    {
        if (!ef->extrafloor_definition || !(ef->extrafloor_definition->type_ & kExtraFloorTypeSeeThrough))
            num = GapRemoveSolid(gaps, num, ef->bottom_height, ef->top_height);
    }

    return num;
}

static int GapRestrict(VerticalGap *dest, int d_num, VerticalGap *src, int s_num)
{
    int   d, s;
    float f, c;

    int         new_num = 0;
    VerticalGap new_gaps[100];

    for (s = 0; s < s_num; s++)
    {
        // ignore empty gaps.
        if (src[s].ceiling <= src[s].floor)
            continue;

        for (d = 0; d < d_num; d++)
        {
            // ignore empty gaps.
            if (dest[d].ceiling <= dest[d].floor)
                continue;

            f = HMM_MAX(src[s].floor, dest[d].floor);
            c = HMM_MIN(src[s].ceiling, dest[d].ceiling);

            if (f < c)
            {
                new_gaps[new_num].ceiling = c;
                new_gaps[new_num].floor   = f;
                new_num++;
            }
        }
    }

    memmove(dest, new_gaps, new_num * sizeof(VerticalGap));

    return new_num;
}

//
// FindThingGap
//
// Find the best gap that the thing could fit in, given a certain Z
// position (z1 is foot, z2 is head).  Assuming at least two gaps exist,
// the best gap is chosen as follows:
//
// 1. if the thing fits in one of the gaps without moving vertically,
//    then choose that gap.
//
// 2. if there is only *one* gap which the thing could fit in, then
//    choose that gap.
//
// 3. if there is multiple gaps which the thing could fit in, choose
//    the gap whose floor is closest to the thing's current Z.
//
// 4. if there is no gaps which the thing could fit in, do the same.
//
// Returns the gap number, or -1 if there are no gaps at all.
//
int FindThingGap(VerticalGap *gaps, int gap_num, float z1, float z2)
{
    int   i;
    float dist;

    int fit_num  = 0;
    int fit_last = -1;

    int   fit_closest = -1;
    float fit_mindist = FLT_MAX;

    int   nofit_closest = -1;
    float nofit_mindist = FLT_MAX;

    // check for trivial gaps...

    if (gap_num == 0)
    {
        return -1;
    }
    else if (gap_num == 1)
    {
        return 0;
    }

    // There are 2 or more gaps.  Now it gets interesting :-)

    for (i = 0; i < gap_num; i++)
    {
        if (z1 >= gaps[i].floor && z2 <= gaps[i].ceiling)
        { // [1]
            return i;
        }

        dist = HMM_ABS(z1 - gaps[i].floor);

        if (z2 - z1 <= gaps[i].ceiling - gaps[i].floor)
        { // [2]
            fit_num++;

            fit_last = i;
            if (dist < fit_mindist)
            { // [3]
                fit_mindist = dist;
                fit_closest = i;
            }
        }
        else
        {
            if (dist < nofit_mindist)
            { // [4]
                nofit_mindist = dist;
                nofit_closest = i;
            }
        }
    }

    if (fit_num == 1)
        return fit_last;
    else if (fit_num > 1)
        return fit_closest;
    else
        return nofit_closest;
}

//
// ComputeThingGap
//
// Determine the initial floorz and ceilingz that a thing placed at a
// particular Z would have.  Returns the nominal Z height.  Some special
// values of Z are recognised: kOnFloorZ & kOnCeilingZ.
//
float ComputeThingGap(MapObject *thing, Sector *sec, float z, float *f, float *c, float floor_slope_z,
                      float ceiling_slope_z)
{
    VerticalGap temp_gaps[100];
    int         temp_num;

    temp_num = GapConstruct(temp_gaps, sec, thing, floor_slope_z, ceiling_slope_z);

    if (AlmostEquals(z, kOnFloorZ))
        z = sec->floor_height;

    if (AlmostEquals(z, kOnCeilingZ))
        z = sec->ceiling_height - thing->height_;

    temp_num = FindThingGap(temp_gaps, temp_num, z, z + thing->height_);

    if (temp_num < 0)
    {
        // thing is stuck in a closed door.
        *f = *c = sec->floor_height;
        return *f;
    }

    *f = temp_gaps[temp_num].floor;
    *c = temp_gaps[temp_num].ceiling;

    return z;
}

//
// ComputeGaps
//
// Determine the gaps between the front & back sectors of the line,
// taking into account any extra floors.
//
// -AJA- 1999/07/19: This replaces P_LineOpening.
//
void ComputeGaps(Line *ld)
{
    Sector *front = ld->front_sector;
    Sector *back  = ld->back_sector;

    int         temp_num;
    VerticalGap temp_gaps[100];

    ld->blocked    = true;
    ld->gap_number = 0;

    if (!front || !back)
    {
        // single sided line
        return;
    }

    // NOTE: this check is rather lax.  It mirrors the check in original
    // Doom r_bsp.c, in order for transparent doors to work properly.
    // In particular, the blocked flag can be clear even when one of the
    // sectors is closed (has ceiling <= floor).

    if (back->ceiling_height <= front->floor_height || front->ceiling_height <= back->floor_height)
    {
        // closed door.

        // -AJA- MUNDO HACK for slopes!!!!
        if (front->floor_slope || back->floor_slope || front->ceiling_slope || back->ceiling_slope)
        {
            ld->blocked = false;
        }

        return;
    }

    // FIXME: strictly speaking this is not correct, as the front or
    // back sector may be filled up with thick opaque extrafloors.
    ld->blocked = false;

    // handle horizontal sliders
    if (ld->slide_door)
    {
        SlidingDoorMover *smov = ld->slider_move;

        if (!smov)
            return;

        // these semantics copied from XDoom
        if (smov->direction > 0 && smov->opening < smov->target * 0.5f)
            return;

        if (smov->direction < 0 && smov->opening < smov->target * 0.75f)
            return;
    }

    // handle normal gaps ("movement" gaps)

    ld->gap_number = GapConstruct(ld->gaps, front, nullptr);
    temp_num       = GapConstruct(temp_gaps, back, nullptr);

    ld->gap_number = GapRestrict(ld->gaps, ld->gap_number, temp_gaps, temp_num);
}

//
// DumpExtraFloors
//
#ifdef DEVELOPERS
void DumpExtraFloors(const sector_t *sec)
{
    const extrafloor_t *ef;

    LogDebug("EXTRAFLOORS IN Sector %d  (%d used, %d max)\n", (int)(sec - sectors), sec->extrafloor_used,
             sec->extrafloor_maximum);

    LogDebug("  Basic height: %1.1f .. %1.1f\n", sec->floor_height, sec->ceiling_height);

    for (ef = sec->bottom_extrafloor; ef; ef = ef->higher)
    {
        LogDebug("  Solid %s: %1.1f .. %1.1f\n",
                 (ef->extrafloor_definition->type & kExtraFloorTypeThick) ? "Thick" : "Thin", ef->bottom_height,
                 ef->top_height);
    }

    for (ef = sec->bottom_liquid; ef; ef = ef->higher)
    {
        LogDebug("  Liquid %s: %1.1f .. %1.1f\n",
                 (ef->extrafloor_definition->type & kExtraFloorTypeThick) ? "Thick" : "Thin", ef->bottom_height,
                 ef->top_height);
    }
}
#endif

//
// CheckExtrafloorFit
//
// Check if a solid extrafloor fits.
//
ExtrafloorFit CheckExtrafloorFit(Sector *sec, float z1, float z2)
{
    Extrafloor *ef;

    if (z2 > sec->ceiling_height)
        return kFitStuckInCeiling;

    if (z1 < sec->floor_height)
        return kFitStuckInFloor;

    for (ef = sec->bottom_extrafloor; ef && ef->higher; ef = ef->higher)
    {
        float bottom = ef->bottom_height;
        float top    = ef->top_height;

        EPI_ASSERT(top >= bottom);

        // here is another solid extrafloor, check for overlap
        if (z2 > bottom && z1 < top)
            return kFitStuckInExtraFloor;
    }

    return kFitOk;
}

void AddExtraFloor(Sector *sec, Line *line)
{
    Sector                     *ctrl = line->front_sector;
    const ExtraFloorDefinition *ef_info;

    MapSurface *top, *bottom;
    Extrafloor *newbie, *cur;

    bool          liquid;
    ExtrafloorFit errcode;

    EPI_ASSERT(line->special);
    EPI_ASSERT(line->special->ef_.type_ & kExtraFloorTypePresent);

    ef_info = &line->special->ef_;

    //
    // -- create new extrafloor --
    //

    EPI_ASSERT(sec->extrafloor_used <= sec->extrafloor_maximum);

    if (sec->extrafloor_used == sec->extrafloor_maximum)
        FatalError("INTERNAL ERROR: extrafloor overflow in sector %d\n", (int)(sec - level_sectors));

    newbie = sec->extrafloor_first + sec->extrafloor_used;
    sec->extrafloor_used++;

    EPI_CLEAR_MEMORY(newbie, Extrafloor, 1);

    bottom = &ctrl->floor;
    top    = (ef_info->type_ & kExtraFloorTypeThick) ? &ctrl->ceiling : bottom;

    // Handle the BOOMTEX flag (Boom compatibility)
    if (ef_info->type_ & kExtraFloorTypeBoomTex)
    {
        bottom = &ctrl->ceiling;
        top    = &sec->floor;
    }

    newbie->bottom_height = ctrl->floor_height;
    newbie->top_height    = (ef_info->type_ & kExtraFloorTypeThick) ? ctrl->ceiling_height : newbie->bottom_height;

    if (newbie->top_height < newbie->bottom_height)
        FatalError("Bad Extrafloor in sector #%d: "
                   "z range is %1.0f / %1.0f\n",
                   (int)(sec - level_sectors), newbie->bottom_height, newbie->top_height);

    newbie->sector = sec;
    newbie->top    = top;
    newbie->bottom = bottom;

    newbie->properties            = &ctrl->properties;
    newbie->extrafloor_definition = ef_info;
    newbie->extrafloor_line       = line;

    // Insert into the dummy's linked list
    newbie->control_sector_next = ctrl->control_floors;
    ctrl->control_floors        = newbie;

    //
    // -- handle liquid extrafloors --
    //

    liquid = (ef_info->type_ & kExtraFloorTypeLiquid) ? true : false;

    if (liquid)
    {
        // find place to link into.  cur will be the next higher liquid,
        // or nullptr if this is the highest.

        for (cur = sec->bottom_liquid; cur; cur = cur->higher)
        {
            if (cur->bottom_height > newbie->bottom_height)
                break;
        }

        newbie->higher = cur;
        newbie->lower  = cur ? cur->lower : sec->top_liquid;

        if (newbie->higher)
            newbie->higher->lower = newbie;
        else
            sec->top_liquid = newbie;

        if (newbie->lower)
            newbie->lower->higher = newbie;
        else
            sec->bottom_liquid = newbie;

        return;
    }

    //
    // -- handle solid extrafloors --
    //

    // check if fits
    errcode = CheckExtrafloorFit(sec, newbie->bottom_height, newbie->top_height);

    switch (errcode)
    {
    case kFitOk:
        break;

    case kFitStuckInCeiling:
        LogWarning("Extrafloor with z range of %1.0f / %1.0f is stuck "
                   "in sector #%d's ceiling.\n",
                   newbie->bottom_height, newbie->top_height, (int)(sec - level_sectors));

    case kFitStuckInFloor:
        LogWarning("Extrafloor with z range of %1.0f / %1.0f is stuck "
                   "in sector #%d's floor.\n",
                   newbie->bottom_height, newbie->top_height, (int)(sec - level_sectors));

    default:
        LogWarning("Extrafloor with z range of %1.0f / %1.0f is stuck "
                   "in sector #%d in another extrafloor.\n",
                   newbie->bottom_height, newbie->top_height, (int)(sec - level_sectors));
    }

    // find place to link into.  cur will be the next higher extrafloor,
    // or nullptr if this is the highest.

    for (cur = sec->bottom_extrafloor; cur; cur = cur->higher)
    {
        if (cur->bottom_height > newbie->bottom_height)
            break;
    }

    newbie->higher = cur;
    newbie->lower  = cur ? cur->lower : sec->top_extrafloor;

    if (newbie->higher)
        newbie->higher->lower = newbie;
    else
        sec->top_extrafloor = newbie;

    if (newbie->lower)
        newbie->lower->higher = newbie;
    else
        sec->bottom_extrafloor = newbie;
}

void FloodExtraFloors(Sector *sector)
{
    Extrafloor *S, *L, *C;

    RegionProperties *props;
    RegionProperties *flood_p = nullptr, *last_p = nullptr;

    sector->active_properties = &sector->properties;

    // traverse downwards, stagger both lists
    S = sector->top_extrafloor;
    L = sector->top_liquid;

    while (S || L)
    {
        if (!L || (S && S->bottom_height > L->bottom_height))
        {
            C = S;
            S = S->lower;
        }
        else
        {
            C = L;
            L = L->lower;
        }

        EPI_ASSERT(C);

        props = &C->extrafloor_line->front_sector->properties;

        if (C->extrafloor_definition->type_ & kExtraFloorTypeFlooder)
        {
            C->properties = last_p = flood_p = props;

            if ((C->extrafloor_definition->type_ & kExtraFloorTypeLiquid) && C->bottom_height >= sector->ceiling_height)
                sector->active_properties = flood_p;

            continue;
        }

        if (C->extrafloor_definition->type_ & kExtraFloorTypeNoShade)
        {
            if (!last_p)
                last_p = props;

            C->properties = last_p;
            continue;
        }

        C->properties = last_p = flood_p ? flood_p : props;
    }
}

void RecomputeGapsAroundSector(Sector *sec)
{
    int i;

    for (i = 0; i < sec->line_count; i++)
    {
        ComputeGaps(sec->lines[i]);
    }

    // now do the sight gaps...

    if (sec->ceiling_height <= sec->floor_height)
    {
        sec->sight_gap_number = 0;
        return;
    }

    sec->sight_gap_number = GapSightConstruct(sec->sight_gaps, sec);
}

static inline bool CheckBoundingBoxOverlap(float *bspcoord, float *test)
{
    return (test[kBoundingBoxRight] < bspcoord[kBoundingBoxLeft] ||
            test[kBoundingBoxLeft] > bspcoord[kBoundingBoxRight] ||
            test[kBoundingBoxTop] < bspcoord[kBoundingBoxBottom] ||
            test[kBoundingBoxBottom] > bspcoord[kBoundingBoxTop])
               ? false
               : true;
}

static bool TraverseSubsector(unsigned int bspnum, float *bbox, bool (*func)(MapObject *mo))
{
    Subsector *sub;
    BspNode   *node;
    MapObject *obj;

    // just a normal node ?
    if (!(bspnum & kLeafSubsector))
    {
        node = level_nodes + bspnum;

        // recursively check the children nodes
        // OPTIMISE: check against partition lines instead of bboxes.

        if (CheckBoundingBoxOverlap(node->bounding_boxes[0], bbox))
        {
            if (!TraverseSubsector(node->children[0], bbox, func))
                return false;
        }

        if (CheckBoundingBoxOverlap(node->bounding_boxes[1], bbox))
        {
            if (!TraverseSubsector(node->children[1], bbox, func))
                return false;
        }

        return true;
    }

    // the sharp end: check all things in the subsector

    sub = level_subsectors + (bspnum & ~kLeafSubsector);

    for (obj = sub->thing_list; obj; obj = obj->subsector_next_)
    {
        if (!(*func)(obj))
            return false;
    }

    return true;
}

//
// SubsectorThingIterator
//
// Iterate over all things that touch a certain rectangle on the map,
// using the BSP tree.
//
// If any function returns false, then this routine returns false and
// nothing else is checked.  Otherwise true is returned.
//
bool SubsectorThingIterator(float *bbox, bool (*func)(MapObject *mo))
{
    return TraverseSubsector(root_node, bbox, func);
}

static float checkempty_bbox[4];
static Line *checkempty_line;

static bool CheckThingInArea(MapObject *mo)
{
    if (mo->x + mo->radius_ < checkempty_bbox[kBoundingBoxLeft] ||
        mo->x - mo->radius_ > checkempty_bbox[kBoundingBoxRight] ||
        mo->y + mo->radius_ < checkempty_bbox[kBoundingBoxBottom] ||
        mo->y - mo->radius_ > checkempty_bbox[kBoundingBoxTop])
    {
        // keep looking
        return true;
    }

    // ignore corpses and pickup items
    if (!(mo->flags_ & kMapObjectFlagSolid) && (mo->flags_ & kMapObjectFlagCorpse))
        return true;

    if (mo->flags_ & kMapObjectFlagSpecial)
        return true;

    // we've found a thing in that area: we can stop now
    return false;
}

static bool CheckThingOnLine(MapObject *mo)
{
    float bbox[4];
    int   side;

    bbox[kBoundingBoxLeft]   = mo->x - mo->radius_;
    bbox[kBoundingBoxRight]  = mo->x + mo->radius_;
    bbox[kBoundingBoxBottom] = mo->y - mo->radius_;
    bbox[kBoundingBoxTop]    = mo->y + mo->radius_;

    // found a thing on the line ?
    side = BoxOnLineSide(bbox, checkempty_line);

    if (side != -1)
        return true;

    // ignore corpses and pickup items
    if (!(mo->flags_ & kMapObjectFlagSolid) && (mo->flags_ & kMapObjectFlagCorpse))
        return true;

    if (mo->flags_ & kMapObjectFlagSpecial)
        return true;

    return false;
}

//
// CheckAreaForThings
//
// Checks if there are any things contained within the given rectangle
// on the 2D map.
//
bool CheckAreaForThings(float *bbox)
{
    checkempty_bbox[kBoundingBoxLeft]   = bbox[kBoundingBoxLeft];
    checkempty_bbox[kBoundingBoxRight]  = bbox[kBoundingBoxRight];
    checkempty_bbox[kBoundingBoxBottom] = bbox[kBoundingBoxBottom];
    checkempty_bbox[kBoundingBoxTop]    = bbox[kBoundingBoxTop];

    return !SubsectorThingIterator(bbox, CheckThingInArea);
}

//
// CheckSliderPathForThings
//
//
bool CheckSliderPathForThings(Line *ld)
{
    Line *temp_line = new Line;
    memcpy(temp_line, ld, sizeof(Line));
    temp_line->bounding_box[kBoundingBoxLeft] -= 32;
    temp_line->bounding_box[kBoundingBoxRight] += 32;
    temp_line->bounding_box[kBoundingBoxBottom] -= 32;
    temp_line->bounding_box[kBoundingBoxTop] += 32;

    checkempty_line = temp_line;

    bool slider_check = SubsectorThingIterator(temp_line->bounding_box, CheckThingOnLine);

    delete temp_line;
    temp_line = nullptr;

    return !slider_check;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
