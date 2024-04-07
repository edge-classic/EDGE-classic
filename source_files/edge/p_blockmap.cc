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

#include <float.h>

#include <algorithm>
#include <unordered_set>
#include <vector>

#include "AlmostEquals.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "edge_profiling.h"
#include "epi.h"
#include "epi_doomdefs.h"
#include "i_defs_gl.h" // needed for r_shader.h
#include "i_system.h"
#include "m_bbox.h"
#include "p_local.h"
#include "p_spec.h"
#include "r_gldefs.h"
#include "r_misc.h"
#include "r_shader.h"
#include "r_state.h"

// FIXME: have a proper API
extern AbstractShader *MakeDLightShader(MapObject *mo);
extern AbstractShader *MakePlaneGlow(MapObject *mo);
extern AbstractShader *MakeWallGlow(MapObject *mo);

extern unsigned int root_node;

// BLOCKMAP
//
// Created from axis aligned bounding box
// of the map, a rectangular array of
// blocks of size ...
// Used to speed up collision detection
// by spatial subdivision in 2D.
//
// Blockmap size.
// 23-6-98 KM Promotion of short * to int *
int blockmap_width  = 0;
int blockmap_height = 0; // size in mapblocks

// origin of block map
float blockmap_origin_x;
float blockmap_origin_y;

static std::list<Line *> **blockmap_lines = nullptr;

// for thing chains
MapObject **blockmap_things = nullptr;

// for dynamic lights
static int dynamic_light_blockmap_width;
static int dynamic_light_blockmap_height;

MapObject **dynamic_light_blockmap_things = nullptr;

extern std::unordered_set<AbstractShader *> seen_dynamic_lights;
extern ConsoleVariable                      draw_culling;

EDGE_DEFINE_CONSOLE_VARIABLE(max_dynamic_lights, "0", kConsoleVariableFlagArchive)

void CreateThingBlockmap(void)
{
    blockmap_things = new MapObject *[blockmap_width * blockmap_height];

    EPI_CLEAR_MEMORY(blockmap_things, MapObject *, blockmap_width * blockmap_height);

    // compute size of dynamic light blockmap
    dynamic_light_blockmap_width  = (blockmap_width * kBlockmapUnitSize + kLightmapUnitSize - 1) / kLightmapUnitSize;
    dynamic_light_blockmap_height = (blockmap_height * kBlockmapUnitSize + kLightmapUnitSize - 1) / kLightmapUnitSize;

    LogDebug("Blockmap size: %dx%d --> Lightmap size: %dx%x\n", blockmap_width, blockmap_height,
             dynamic_light_blockmap_width, dynamic_light_blockmap_height);

    dynamic_light_blockmap_things = new MapObject *[dynamic_light_blockmap_width * dynamic_light_blockmap_height];

    EPI_CLEAR_MEMORY(dynamic_light_blockmap_things, MapObject *,
                     dynamic_light_blockmap_width * dynamic_light_blockmap_height);
}

void DestroyBlockmap(void)
{
    if (blockmap_lines)
    {
        for (int i = 0; i < blockmap_width * blockmap_height; i++)
        {
            if (blockmap_lines[i])
            {
                delete blockmap_lines[i];
            }
        }
    }

    delete[] blockmap_lines;
    blockmap_lines = nullptr;
    delete[] blockmap_things;
    blockmap_things = nullptr;

    delete[] dynamic_light_blockmap_things;
    dynamic_light_blockmap_things = nullptr;

    blockmap_width = blockmap_height = 0;
}

//--------------------------------------------------------------------------
//
//  THING POSITION SETTING
//

// stats
#ifdef DEVELOPERS
int touchstat_moves;
int touchstat_hit;
int touchstat_miss;
int touchstat_alloc;
int touchstat_free;
#endif

// quick-alloc list
// FIXME: incorporate into FlushCaches
TouchNode *free_touch_nodes;

static inline TouchNode *TouchNodeAlloc(void)
{
    TouchNode *tn;

#ifdef DEVELOPERS
    touchstat_alloc++;
#endif

    if (free_touch_nodes)
    {
        tn               = free_touch_nodes;
        free_touch_nodes = tn->map_object_next;
    }
    else
    {
        tn = new TouchNode;
    }

    return tn;
}

static inline void TouchNodeFree(TouchNode *tn)
{
#ifdef DEVELOPERS
    touchstat_free++;
#endif

    // PREV field is ignored in quick-alloc list
    tn->map_object_next = free_touch_nodes;
    free_touch_nodes    = tn;
}

static inline void TouchNodeLinkIntoSector(TouchNode *tn, Sector *sec)
{
    tn->sector = sec;

    tn->sector_next     = sec->touch_things;
    tn->sector_previous = nullptr;

    if (tn->sector_next)
        tn->sector_next->sector_previous = tn;

    sec->touch_things = tn;
}

static inline void TouchNodeLinkIntoThing(TouchNode *tn, MapObject *mo)
{
    tn->map_object = mo;

    tn->map_object_next     = mo->touch_sectors_;
    tn->map_object_previous = nullptr;

    if (tn->map_object_next)
        tn->map_object_next->map_object_previous = tn;

    mo->touch_sectors_ = tn;
}

static inline void TouchNodeUnlinkFromSector(TouchNode *tn)
{
    if (tn->sector_next)
        tn->sector_next->sector_previous = tn->sector_previous;

    if (tn->sector_previous)
        tn->sector_previous->sector_next = tn->sector_next;
    else
        tn->sector->touch_things = tn->sector_next;
}

static inline void TouchNodeUnlinkFromThing(TouchNode *tn)
{
    if (tn->map_object_next)
        tn->map_object_next->map_object_previous = tn->map_object_previous;

    if (tn->map_object_previous)
        tn->map_object_previous->map_object_next = tn->map_object_next;
    else
        tn->map_object->touch_sectors_ = tn->map_object_next;
}

struct BspThingPosition
{
    MapObject *thing;
    float      bbox[4];
};

//
// SetPositionBSP
//
static void SetPositionBSP(BspThingPosition *info, int nodenum)
{
    TouchNode *tn;
    Sector    *sec;
    Subsector *sub;
    Seg       *seg;

    while (!(nodenum & kLeafSubsector))
    {
        BspNode *nd = level_nodes + nodenum;

        int side = BoxOnDividingLineSide(info->bbox, &nd->divider);

        // if box touches partition line, we must traverse both sides
        if (side == -1)
        {
            SetPositionBSP(info, nd->children[0]);
            side = 1;
        }

        EPI_ASSERT(side == 0 || side == 1);

        nodenum = nd->children[side];
    }

    // reached a leaf of the BSP.  Need to check BBOX against all
    // linedef segs.  This is because we can get false positives, since
    // we don't actually split the thing's BBOX when it intersects with
    // a partition line.

    sub = level_subsectors + (nodenum & ~kLeafSubsector);

    for (seg = sub->segs; seg; seg = seg->subsector_next)
    {
        DividingLine div;

        if (seg->miniseg)
            continue;

        div.x       = seg->vertex_1->X;
        div.y       = seg->vertex_1->Y;
        div.delta_x = seg->vertex_2->X - div.x;
        div.delta_y = seg->vertex_2->Y - div.y;

        if (BoxOnDividingLineSide(info->bbox, &div) == 1)
            return;
    }

    // Perform linkage...

    sec = sub->sector;

#ifdef DEVELOPERS
    touchstat_miss++;
#endif

    for (tn = info->thing->touch_sectors_; tn; tn = tn->map_object_next)
    {
        if (!tn->map_object)
        {
            // found unused touch node.  We reuse it.
            tn->map_object = info->thing;

            if (tn->sector != sec)
            {
                TouchNodeUnlinkFromSector(tn);
                TouchNodeLinkIntoSector(tn, sec);
            }
#ifdef DEVELOPERS
            else
            {
                touchstat_miss--;
                touchstat_hit++;
            }
#endif

            return;
        }

        EPI_ASSERT(tn->map_object == info->thing);

        // sector already present ?
        if (tn->sector == sec)
            return;
    }

    // need to allocate a new touch node
    tn = TouchNodeAlloc();

    TouchNodeLinkIntoThing(tn, info->thing);
    TouchNodeLinkIntoSector(tn, sec);
}

//
// UnsetThingPosition
//
// Unlinks a thing from block map and subsector.
// On each position change, BLOCKMAP and other
// lookups maintaining lists ot things inside
// these structures need to be updated.
//
// -ES- 1999/12/04 Better error checking: Clear prev/next fields.
// This catches errors which can occur if the position is unset twice.
//
void UnsetThingPosition(MapObject *mo)
{
    int blockx;
    int blocky;
    int bnum;

    TouchNode *tn;

    // unlink from subsector
    if (!(mo->flags_ & kMapObjectFlagNoSector))
    {
        // (inert things don't need to be in subsector list)

        if (mo->subsector_next_)
        {
            if (mo->subsector_next_->subsector_previous_)
            {
                EPI_ASSERT(mo->subsector_next_->subsector_previous_ == mo);

                mo->subsector_next_->subsector_previous_ = mo->subsector_previous_;
            }
        }

        if (mo->subsector_previous_)
        {
            if (mo->subsector_previous_->subsector_next_)
            {
                EPI_ASSERT(mo->subsector_previous_->subsector_next_ == mo);

                mo->subsector_previous_->subsector_next_ = mo->subsector_next_;
            }
        }
        else
        {
            if (mo->subsector_->thing_list)
            {
                EPI_ASSERT(mo->subsector_->thing_list == mo);

                mo->subsector_->thing_list = mo->subsector_next_;
            }
        }

        mo->subsector_next_     = nullptr;
        mo->subsector_previous_ = nullptr;
    }

    // unlink from touching list.
    // NOTE: lazy unlinking -- see notes in r_defs.h
    //
    for (tn = mo->touch_sectors_; tn; tn = tn->map_object_next)
    {
        tn->map_object = nullptr;
    }

    // unlink from blockmap
    if (!(mo->flags_ & kMapObjectFlagNoBlockmap))
    {
        // inert things don't need to be in blockmap
        if (mo->blockmap_next_)
        {
            if (mo->blockmap_next_->blockmap_previous_)
            {
                EPI_ASSERT(mo->blockmap_next_->blockmap_previous_ == mo);

                mo->blockmap_next_->blockmap_previous_ = mo->blockmap_previous_;
            }
        }

        if (mo->blockmap_previous_)
        {
            if (mo->blockmap_previous_->blockmap_next_)
            {
                EPI_ASSERT(mo->blockmap_previous_->blockmap_next_ == mo);

                mo->blockmap_previous_->blockmap_next_ = mo->blockmap_next_;
            }
        }
        else
        {
            blockx = BlockmapGetX(mo->x);
            blocky = BlockmapGetY(mo->y);

            if (blockx >= 0 && blockx < blockmap_width && blocky >= 0 && blocky < blockmap_height)
            {
                bnum = blocky * blockmap_width + blockx;

                EPI_ASSERT(blockmap_things[bnum] == mo);

                blockmap_things[bnum] = mo->blockmap_next_;
            }
        }

        mo->blockmap_previous_ = nullptr;
        mo->blockmap_next_     = nullptr;
    }

    // unlink from dynamic light blockmap
    if (mo->info_ && (mo->info_->dlight_[0].type_ != kDynamicLightTypeNone) &&
        (mo->info_->glow_type_ == kSectorGlowTypeNone))
    {
        if (mo->dynamic_light_next_)
        {
            if (mo->dynamic_light_next_->dynamic_light_previous_)
            {
                EPI_ASSERT(mo->dynamic_light_next_->dynamic_light_previous_ == mo);

                mo->dynamic_light_next_->dynamic_light_previous_ = mo->dynamic_light_previous_;
            }
        }

        if (mo->dynamic_light_previous_)
        {
            if (mo->dynamic_light_previous_->dynamic_light_next_)
            {
                EPI_ASSERT(mo->dynamic_light_previous_->dynamic_light_next_ == mo);

                mo->dynamic_light_previous_->dynamic_light_next_ = mo->dynamic_light_next_;
            }
        }
        else
        {
            blockx = LightmapGetX(mo->x);
            blocky = LightmapGetY(mo->y);

            if (blockx >= 0 && blockx < dynamic_light_blockmap_width && blocky >= 0 &&
                blocky < dynamic_light_blockmap_height)
            {
                bnum = blocky * dynamic_light_blockmap_width + blockx;

                EPI_ASSERT(dynamic_light_blockmap_things[bnum] == mo);
                dynamic_light_blockmap_things[bnum] = mo->dynamic_light_next_;
            }
        }

        mo->dynamic_light_previous_ = nullptr;
        mo->dynamic_light_next_     = nullptr;
    }

    // unlink from sector glow list
    if (mo->info_ && (mo->info_->dlight_[0].type_ != kDynamicLightTypeNone) &&
        (mo->info_->glow_type_ != kSectorGlowTypeNone))
    {
        Sector *sec = mo->subsector_->sector;

        if (mo->dynamic_light_next_)
        {
            if (mo->dynamic_light_next_->dynamic_light_previous_)
            {
                EPI_ASSERT(mo->dynamic_light_next_->dynamic_light_previous_ == mo);

                mo->dynamic_light_next_->dynamic_light_previous_ = mo->dynamic_light_previous_;
            }
        }

        if (mo->dynamic_light_previous_)
        {
            if (mo->dynamic_light_previous_->dynamic_light_next_)
            {
                EPI_ASSERT(mo->dynamic_light_previous_->dynamic_light_next_ == mo);

                mo->dynamic_light_previous_->dynamic_light_next_ = mo->dynamic_light_next_;
            }
        }
        else
        {
            if (sec->glow_things)
            {
                EPI_ASSERT(sec->glow_things == mo);

                sec->glow_things = mo->dynamic_light_next_;
            }
        }

        mo->dynamic_light_previous_ = nullptr;
        mo->dynamic_light_next_     = nullptr;
    }
}

//
// UnsetThingFinal
//
// Call when the thing is about to be removed for good.
//
void UnsetThingFinal(MapObject *mo)
{
    TouchNode *tn;

    UnsetThingPosition(mo);

    // clear out touch nodes

    while (mo->touch_sectors_)
    {
        tn                 = mo->touch_sectors_;
        mo->touch_sectors_ = tn->map_object_next;

        TouchNodeUnlinkFromSector(tn);
        TouchNodeFree(tn);
    }
}

//
// SetThingPosition
//
// Links a thing into both a block and a subsector
// based on it's x y.
//
void SetThingPosition(MapObject *mo)
{
    Subsector *ss;
    int        blockx;
    int        blocky;
    int        bnum;

    BspThingPosition pos;
    TouchNode       *tn;

    // -ES- 1999/12/04 The position must be unset before it's set again.
    if (mo->subsector_next_ || mo->subsector_previous_ || mo->blockmap_next_ || mo->blockmap_previous_)
        FatalError("INTERNAL ERROR: Double SetThingPosition call.");

    EPI_ASSERT(!(mo->dynamic_light_next_ || mo->dynamic_light_previous_));

    // link into subsector
    ss             = PointInSubsector(mo->x, mo->y);
    mo->subsector_ = ss;

    // determine properties
    mo->region_properties_ = GetPointProperties(ss, mo->z + mo->height_ / 2);

    if (!(mo->flags_ & kMapObjectFlagNoSector))
    {
        mo->subsector_next_     = ss->thing_list;
        mo->subsector_previous_ = nullptr;

        if (ss->thing_list)
            ss->thing_list->subsector_previous_ = mo;

        ss->thing_list = mo;
    }

    // link into touching list

#ifdef DEVELOPERS
    touchstat_moves++;
#endif

    pos.thing                    = mo;
    pos.bbox[kBoundingBoxLeft]   = mo->x - mo->radius_;
    pos.bbox[kBoundingBoxRight]  = mo->x + mo->radius_;
    pos.bbox[kBoundingBoxBottom] = mo->y - mo->radius_;
    pos.bbox[kBoundingBoxTop]    = mo->y + mo->radius_;

    SetPositionBSP(&pos, root_node);

    // handle any left-over unused touch nodes

    for (tn = mo->touch_sectors_; tn && tn->map_object; tn = tn->map_object_next)
    { /* nothing here */
    }

    if (tn)
    {
        if (tn->map_object_previous)
            tn->map_object_previous->map_object_next = nullptr;
        else
            mo->touch_sectors_ = nullptr;

        while (tn)
        {
            TouchNode *cur = tn;
            tn             = tn->map_object_next;

            EPI_ASSERT(!cur->map_object);

            TouchNodeUnlinkFromSector(cur);
            TouchNodeFree(cur);
        }
    }

    // link into blockmap
    if (!(mo->flags_ & kMapObjectFlagNoBlockmap))
    {
        blockx = BlockmapGetX(mo->x);
        blocky = BlockmapGetY(mo->y);

        if (blockx >= 0 && blockx < blockmap_width && blocky >= 0 && blocky < blockmap_height)
        {
            bnum = blocky * blockmap_width + blockx;

            mo->blockmap_previous_ = nullptr;
            mo->blockmap_next_     = blockmap_things[bnum];

            if (blockmap_things[bnum])
                (blockmap_things[bnum])->blockmap_previous_ = mo;

            blockmap_things[bnum] = mo;
        }
        else
        {
            // thing is off the map
            mo->blockmap_next_ = mo->blockmap_previous_ = nullptr;
        }
    }

    // link into dynamic light blockmap
    if (mo->info_ && (mo->info_->dlight_[0].type_ != kDynamicLightTypeNone) &&
        (mo->info_->glow_type_ == kSectorGlowTypeNone))
    {
        blockx = LightmapGetX(mo->x);
        blocky = LightmapGetY(mo->y);

        if (blockx >= 0 && blockx < dynamic_light_blockmap_width && blocky >= 0 &&
            blocky < dynamic_light_blockmap_height)
        {
            bnum = blocky * dynamic_light_blockmap_width + blockx;

            mo->dynamic_light_previous_ = nullptr;
            mo->dynamic_light_next_     = dynamic_light_blockmap_things[bnum];

            if (dynamic_light_blockmap_things[bnum])
                (dynamic_light_blockmap_things[bnum])->dynamic_light_previous_ = mo;

            dynamic_light_blockmap_things[bnum] = mo;
        }
        else
        {
            // thing is off the map
            mo->dynamic_light_next_ = mo->dynamic_light_previous_ = nullptr;
        }
    }

    // link into sector glow list
    if (mo->info_ && (mo->info_->dlight_[0].type_ != kDynamicLightTypeNone) &&
        (mo->info_->glow_type_ != kSectorGlowTypeNone))
    {
        Sector *sec = mo->subsector_->sector;

        mo->dynamic_light_previous_ = nullptr;
        mo->dynamic_light_next_     = sec->glow_things;

        if (sec->glow_things)
            sec->glow_things->dynamic_light_previous_ = mo;

        sec->glow_things = mo;
    }
}

//
// ChangeThingPosition
//
// New routine to "atomicly" move a thing.  Apart from object
// construction and destruction, this routine should always be called
// when moving a thing, rather than fiddling with the coordinates
// directly (or even P_UnsetThingPos/P_SetThingPos pairs).
//
void ChangeThingPosition(MapObject *mo, float x, float y, float z)
{
    UnsetThingPosition(mo);
    {
        mo->x = x;
        mo->y = y;
        mo->z = z;
    }
    SetThingPosition(mo);
}

//
// FreeSectorTouchNodes
//
void FreeSectorTouchNodes(Sector *sec)
{
    TouchNode *tn;

    for (tn = sec->touch_things; tn; tn = tn->sector_next)
        TouchNodeFree(tn);
}

//--------------------------------------------------------------------------
//
// BLOCK MAP ITERATORS
//
// For each line/thing in the given mapblock,
// call the passed PIT_* function.
// If the function returns false,
// exit with false without checking anything else.
//

//
// BlockmapLineIterator
//
// The valid_count flags are used to avoid checking lines
// that are marked in multiple mapblocks,
// so increment valid_count before the first call
// to BlockmapLineIterator, then make one or more calls
// to it.
//
bool BlockmapLineIterator(float x1, float y1, float x2, float y2, bool (*func)(Line *, void *), void *data)
{
    valid_count++;

    int lx = BlockmapGetX(x1);
    int ly = BlockmapGetY(y1);
    int hx = BlockmapGetX(x2);
    int hy = BlockmapGetY(y2);

    lx = HMM_MAX(0, lx);
    hx = HMM_MIN(blockmap_width - 1, hx);
    ly = HMM_MAX(0, ly);
    hy = HMM_MIN(blockmap_height - 1, hy);

    for (int by = ly; by <= hy; by++)
        for (int bx = lx; bx <= hx; bx++)
        {
            std::list<Line *> *lset = blockmap_lines[by * blockmap_width + bx];

            if (!lset)
                continue;

            std::list<Line *>::iterator LI;
            for (LI = lset->begin(); LI != lset->end(); LI++)
            {
                Line *ld = *LI;

                // has line already been checked ?
                if (ld->valid_count == valid_count)
                    continue;

                ld->valid_count = valid_count;

                // check whether line touches the given bbox
                if (ld->bounding_box[kBoundingBoxRight] <= x1 || ld->bounding_box[kBoundingBoxLeft] >= x2 ||
                    ld->bounding_box[kBoundingBoxTop] <= y1 || ld->bounding_box[kBoundingBoxBottom] >= y2)
                {
                    continue;
                }

                if (!func(ld, data))
                    return false;
            }
        }

    // everything was checked
    return true;
}

bool BlockmapThingIterator(float x1, float y1, float x2, float y2, bool (*func)(MapObject *, void *), void *data)
{
    // need to expand the source by one block because large
    // things (radius limited to kBlockmapUnitSize) can overlap
    // into adjacent blocks.

    int lx = BlockmapGetX(x1) - 1;
    int ly = BlockmapGetY(y1) - 1;
    int hx = BlockmapGetX(x2) + 1;
    int hy = BlockmapGetY(y2) + 1;

    lx = HMM_MAX(0, lx);
    hx = HMM_MIN(blockmap_width - 1, hx);
    ly = HMM_MAX(0, ly);
    hy = HMM_MIN(blockmap_height - 1, hy);

    for (int by = ly; by <= hy; by++)
        for (int bx = lx; bx <= hx; bx++)
        {
            for (MapObject *mo = blockmap_things[by * blockmap_width + bx]; mo; mo = mo->blockmap_next_)
            {
                // check whether thing touches the given bbox
                float r = mo->radius_;

                if (mo->x + r <= x1 || mo->x - r >= x2 || mo->y + r <= y1 || mo->y - r >= y2)
                    continue;

                if (!func(mo, data))
                    return false;
            }
        }

    return true;
}

void DynamicLightIterator(float x1, float y1, float z1, float x2, float y2, float z2, void (*func)(MapObject *, void *),
                          void *data)
{
    EDGE_ZoneScoped;
    ec_frame_stats.draw_light_iterator++;

    int lx = LightmapGetX(x1) - 1;
    int ly = LightmapGetY(y1) - 1;
    int hx = LightmapGetX(x2) + 1;
    int hy = LightmapGetY(y2) + 1;

    lx = HMM_MAX(0, lx);
    hx = HMM_MIN(dynamic_light_blockmap_width - 1, hx);
    ly = HMM_MAX(0, ly);
    hy = HMM_MIN(dynamic_light_blockmap_height - 1, hy);

    for (int by = ly; by <= hy; by++)
        for (int bx = lx; bx <= hx; bx++)
        {
            for (MapObject *mo = dynamic_light_blockmap_things[by * dynamic_light_blockmap_width + bx]; mo;
                 mo            = mo->dynamic_light_next_)
            {
                EPI_ASSERT(mo->state_);

                // skip "off" lights
                if (mo->state_->bright <= 0 || mo->dynamic_light_.r <= 0)
                    continue;

                if (draw_culling.d_ && PointToDistance(view_x, view_y, mo->x, mo->y) > renderer_far_clip.f_)
                    continue;

                // check whether radius touches the given bbox
                float r = mo->dynamic_light_.r;

                if (mo->x + r <= x1 || mo->x - r >= x2 || mo->y + r <= y1 || mo->y - r >= y2 || mo->z + r <= z1 ||
                    mo->z - r >= z2)
                    continue;

                // create shader if necessary
                if (!mo->dynamic_light_.shader)
                    mo->dynamic_light_.shader = MakeDLightShader(mo);

                if (max_dynamic_lights.d_ > 0 && seen_dynamic_lights.count(mo->dynamic_light_.shader) == 0)
                {
                    if ((int)seen_dynamic_lights.size() >= max_dynamic_lights.d_ * 20)
                        continue;
                    else
                    {
                        seen_dynamic_lights.insert(mo->dynamic_light_.shader);
                    }
                }

                //			mo->dynamic_light_.shader->CheckReset();

                func(mo, data);
            }
        }
}

void SectorGlowIterator(Sector *sec, float x1, float y1, float z1, float x2, float y2, float z2,
                        void (*func)(MapObject *, void *), void *data)
{
    EDGE_ZoneScoped;
    ec_frame_stats.draw_sector_glow_iterator++;

    for (MapObject *mo = sec->glow_things; mo; mo = mo->dynamic_light_next_)
    {
        EPI_ASSERT(mo->state_);

        // skip "off" lights
        if (mo->state_->bright <= 0 || mo->dynamic_light_.r <= 0)
            continue;

        if (draw_culling.d_ && PointToDistance(view_x, view_y, mo->x, mo->y) > renderer_far_clip.f_)
            continue;

        // check whether radius touches the given bbox
        float r = mo->dynamic_light_.r;

        if (mo->info_->glow_type_ == kSectorGlowTypeFloor && sec->floor_height + r <= z1)
            continue;

        if (mo->info_->glow_type_ == kSectorGlowTypeCeiling && sec->ceiling_height - r >= z1)
            continue;

        // create shader if necessary
        if (!mo->dynamic_light_.shader)
        {
            if (mo->info_->glow_type_ == kSectorGlowTypeWall)
            {
                if (mo->dynamic_light_.bad_wall_glow)
                    continue;
                else if (!mo->dynamic_light_.glow_wall)
                {
                    // Use first line that the dlight mobj touches
                    // Ideally it is only touching one line
                    for (int i = 0; i < sec->line_count; i++)
                    {
                        if (ThingOnLineSide(mo, sec->lines[i]) == -1)
                        {
                            mo->dynamic_light_.glow_wall = sec->lines[i];
                            break;
                        }
                    }
                    if (mo->dynamic_light_.glow_wall)
                    {
                        mo->dynamic_light_.shader = MakeWallGlow(mo);
                    }
                    else // skip useless repeated checks
                    {
                        mo->dynamic_light_.bad_wall_glow = true;
                        continue;
                    }
                }
                else
                    mo->dynamic_light_.shader = MakeWallGlow(mo);
            }
            else
                mo->dynamic_light_.shader = MakePlaneGlow(mo);
        }

        //		mo->dynamic_light_.shader->CheckReset();

        func(mo, data);
    }
}

//--------------------------------------------------------------------------
//
// INTERCEPT ROUTINES
//

static std::vector<PathIntercept> intercepts;

DividingLine trace;

float PathInterceptVector(DividingLine *v2, DividingLine *v1)
{
    // Returns the fractional intercept point along the first divline.
    // This is only called by the addthings and addlines traversers.

    float den = (v1->delta_y * v2->delta_x) - (v1->delta_x * v2->delta_y);

    if (AlmostEquals(den, 0.0f))
        return 0; // parallel

    float num = (v1->x - v2->x) * v1->delta_y + (v2->y - v1->y) * v1->delta_x;

    return num / den;
}

static inline void PIT_AddLineIntercept(Line *ld)
{
    // Looks for lines in the given block
    // that intercept the given trace
    // to add to the intercepts list.
    //
    // A line is crossed if its endpoints
    // are on opposite sides of the trace.
    // Returns true if earlyout and a solid line hit.

    // has line already been checked ?
    if (ld->valid_count == valid_count)
        return;

    ld->valid_count = valid_count;

    int          s1;
    int          s2;
    float        along;
    DividingLine div;

    div.x       = ld->vertex_1->X;
    div.y       = ld->vertex_1->Y;
    div.delta_x = ld->delta_x;
    div.delta_y = ld->delta_y;

    // avoid precision problems with two routines
    if (trace.delta_x > 16 || trace.delta_y > 16 || trace.delta_x < -16 || trace.delta_y < -16)
    {
        s1 = PointOnDividingLineSide(ld->vertex_1->X, ld->vertex_1->Y, &trace);
        s2 = PointOnDividingLineSide(ld->vertex_2->X, ld->vertex_2->Y, &trace);
    }
    else
    {
        s1 = PointOnDividingLineSide(trace.x, trace.y, &div);
        s2 = PointOnDividingLineSide(trace.x + trace.delta_x, trace.y + trace.delta_y, &div);
    }

    // line isn't crossed ?
    if (s1 == s2)
        return;

    // hit the line

    along = PathInterceptVector(&trace, &div);

    // out of range?
    if (along < 0 || along > 1)
        return;

    // Intercept is a simple struct that can be memcpy()'d: Load
    // up a structure and get into the array
    PathIntercept in;

    in.along = along;
    in.thing = nullptr;
    in.line  = ld;

    intercepts.push_back(in);
}

static inline void PIT_AddThingIntercept(MapObject *thing)
{
    float x1;
    float y1;
    float x2;
    float y2;

    int s1;
    int s2;

    bool tracepositive;

    DividingLine div;

    float along;

    tracepositive = (trace.delta_x >= 0) == (trace.delta_y >= 0);

    // check a corner to corner crossection for hit
    if (tracepositive)
    {
        x1 = thing->x - thing->radius_;
        y1 = thing->y + thing->radius_;

        x2 = thing->x + thing->radius_;
        y2 = thing->y - thing->radius_;
    }
    else
    {
        x1 = thing->x - thing->radius_;
        y1 = thing->y - thing->radius_;

        x2 = thing->x + thing->radius_;
        y2 = thing->y + thing->radius_;
    }

    s1 = PointOnDividingLineSide(x1, y1, &trace);
    s2 = PointOnDividingLineSide(x2, y2, &trace);

    // line isn't crossed ?
    if (s1 == s2)
        return;

    div.x       = x1;
    div.y       = y1;
    div.delta_x = x2 - x1;
    div.delta_y = y2 - y1;

    along = PathInterceptVector(&trace, &div);

    // out of range?
    if (along < 0 || along > 1)
        return;

    // Intercept is a simple struct that can be memcpy()'d: Load
    // up a structure and get into the array
    PathIntercept in;

    in.along = along;
    in.thing = thing;
    in.line  = nullptr;

    intercepts.push_back(in);
}

struct Compare_Intercept_pred
{
    inline bool operator()(const PathIntercept &A, const PathIntercept &B) const
    {
        return A.along < B.along;
    }
};

//
// PathTraverse
//
// Traces a line from x1,y1 to x2,y2,
// calling the traverser function for each.
// Returns true if the traverser function returns true
// for all lines.
//
bool PathTraverse(float x1, float y1, float x2, float y2, int flags, bool (*func)(PathIntercept *, void *), void *data)
{
    valid_count++;

    intercepts.clear();

    // don't side exactly on a line
    if (AlmostEquals(fmod(x1 - blockmap_origin_x, kBlockmapUnitSize), 0.0))
        x1 += 0.1f;

    if (AlmostEquals(fmod(y1 - blockmap_origin_y, kBlockmapUnitSize), 0.0))
        y1 += 0.1f;

    trace.x       = x1;
    trace.y       = y1;
    trace.delta_x = x2 - x1;
    trace.delta_y = y2 - y1;

    x1 -= blockmap_origin_x;
    y1 -= blockmap_origin_y;
    x2 -= blockmap_origin_x;
    y2 -= blockmap_origin_y;

    int bx1 = (int)(x1 / kBlockmapUnitSize);
    int by1 = (int)(y1 / kBlockmapUnitSize);
    int bx2 = (int)(x2 / kBlockmapUnitSize);
    int by2 = (int)(y2 / kBlockmapUnitSize);

    int bx_step;
    int by_step;

    float xstep;
    float ystep;

    float partial;

    double tmp;

    if (bx2 > bx1 && (x2 - x1) > 0.001)
    {
        bx_step = 1;
        partial = 1.0f - modf(x1 / kBlockmapUnitSize, &tmp);

        ystep = (y2 - y1) / fabs(x2 - x1);
    }
    else if (bx2 < bx1 && (x2 - x1) < -0.001)
    {
        bx_step = -1;
        partial = modf(x1 / kBlockmapUnitSize, &tmp);

        ystep = (y2 - y1) / fabs(x2 - x1);
    }
    else
    {
        bx_step = 0;
        partial = 1.0f;
        ystep   = 256.0f;
    }

    float yintercept = y1 / kBlockmapUnitSize + partial * ystep;

    if (by2 > by1 && (y2 - y1) > 0.001)
    {
        by_step = 1;
        partial = 1.0f - modf(y1 / kBlockmapUnitSize, &tmp);

        xstep = (x2 - x1) / fabs(y2 - y1);
    }
    else if (by2 < by1 && (y2 - y1) < -0.001)
    {
        by_step = -1;
        partial = modf(y1 / kBlockmapUnitSize, &tmp);

        xstep = (x2 - x1) / fabs(y2 - y1);
    }
    else
    {
        by_step = 0;
        partial = 1.0f;
        xstep   = 256.0f;
    }

    float xintercept = x1 / kBlockmapUnitSize + partial * xstep;

    // Step through map blocks.
    // Count is present to prevent a round off error
    // from skipping the break.
    int bx = bx1;
    int by = by1;

    for (int count = 0; count < 64; count++)
    {
        if (0 <= bx && bx < blockmap_width && 0 <= by && by < blockmap_height)
        {
            if (flags & kPathAddLines)
            {
                std::list<Line *> *lset = blockmap_lines[by * blockmap_width + bx];

                if (lset)
                {
                    std::list<Line *>::iterator LI;
                    for (LI = lset->begin(); LI != lset->end(); LI++)
                    {
                        PIT_AddLineIntercept(*LI);
                    }
                }
            }

            if (flags & kPathAddThings)
            {
                for (MapObject *mo = blockmap_things[by * blockmap_width + bx]; mo; mo = mo->blockmap_next_)
                {
                    PIT_AddThingIntercept(mo);
                }
            }
        }

        if (bx == bx2 && by == by2)
            break;

        if (by == (int)yintercept)
        {
            yintercept += ystep;
            bx += bx_step;
        }
        else if (bx == (int)xintercept)
        {
            xintercept += xstep;
            by += by_step;
        }
    }

    // go through the sorted list

    if (intercepts.size() == 0)
        return true;

    std::sort(intercepts.begin(), intercepts.end(), Compare_Intercept_pred());

    std::vector<PathIntercept>::iterator I;

    for (I = intercepts.begin(); I != intercepts.end(); I++)
    {
        if (!func(&*I, data))
        {
            // don't bother going further
            return false;
        }
    }

    // everything was traversed
    return true;
}

//--------------------------------------------------------------------------
//
//  BLOCKMAP GENERATION
//

static void BlockAdd(int bnum, Line *ld)
{
    if (!blockmap_lines[bnum])
        blockmap_lines[bnum] = new std::list<Line *>;

    blockmap_lines[bnum]->push_back(ld);

    // blk_total_lines++;
}

static void BlockAddLine(int line_num)
{
    int i, j;
    int x0, y0;
    int x1, y1;

    Line *ld = level_lines + line_num;

    int blocknum;

    int y_sign;
    int x_dist, y_dist;

    float slope;

    x0 = (int)(ld->vertex_1->X - blockmap_origin_x);
    y0 = (int)(ld->vertex_1->Y - blockmap_origin_y);
    x1 = (int)(ld->vertex_2->X - blockmap_origin_x);
    y1 = (int)(ld->vertex_2->Y - blockmap_origin_y);

    // swap endpoints if horizontally backward
    if (x1 < x0)
    {
        int temp;

        temp = x0;
        x0   = x1;
        x1   = temp;
        temp = y0;
        y0   = y1;
        y1   = temp;
    }

    EPI_ASSERT(0 <= x0 && (x0 / kBlockmapUnitSize) < blockmap_width);
    EPI_ASSERT(0 <= y0 && (y0 / kBlockmapUnitSize) < blockmap_height);
    EPI_ASSERT(0 <= x1 && (x1 / kBlockmapUnitSize) < blockmap_width);
    EPI_ASSERT(0 <= y1 && (y1 / kBlockmapUnitSize) < blockmap_height);

    // check if this line spans multiple blocks.

    x_dist = HMM_ABS((x1 / kBlockmapUnitSize) - (x0 / kBlockmapUnitSize));
    y_dist = HMM_ABS((y1 / kBlockmapUnitSize) - (y0 / kBlockmapUnitSize));

    y_sign = (y1 >= y0) ? 1 : -1;

    // handle the simple cases: same column or same row

    blocknum = (y0 / kBlockmapUnitSize) * blockmap_width + (x0 / kBlockmapUnitSize);

    if (y_dist == 0)
    {
        for (i = 0; i <= x_dist; i++, blocknum++)
            BlockAdd(blocknum, ld);

        return;
    }

    if (x_dist == 0)
    {
        for (i = 0; i <= y_dist; i++, blocknum += y_sign * blockmap_width)
            BlockAdd(blocknum, ld);

        return;
    }

    // -AJA- 2000/12/09: rewrote the general case

    EPI_ASSERT(x1 > x0);

    slope = (float)(y1 - y0) / (float)(x1 - x0);

    // handle each column of blocks in turn
    for (i = 0; i <= x_dist; i++)
    {
        // compute intersection of column with line
        int sx = (i == 0) ? x0 : (128 * (x0 / 128 + i));
        int ex = (i == x_dist) ? x1 : (128 * (x0 / 128 + i) + 127);

        int sy = y0 + (int)(slope * (sx - x0));
        int ey = y0 + (int)(slope * (ex - x0));

        EPI_ASSERT(sx <= ex);

        y_dist = HMM_ABS((ey / 128) - (sy / 128));

        for (j = 0; j <= y_dist; j++)
        {
            blocknum = (sy / 128 + j * y_sign) * blockmap_width + (sx / 128);

            BlockAdd(blocknum, ld);
        }
    }
}

void GenerateBlockmap(int min_x, int min_y, int max_x, int max_y)
{
    blockmap_origin_x = min_x - 8;
    blockmap_origin_y = min_y - 8;
    blockmap_width    = BlockmapGetX(max_x) + 1;
    blockmap_height   = BlockmapGetY(max_y) + 1;

    int btotal = blockmap_width * blockmap_height;

    LogDebug("GenerateBlockmap: MAP (%d,%d) -> (%d,%d)\n", min_x, min_y, max_x, max_y);
    LogDebug("GenerateBlockmap: BLOCKS %d x %d  TOTAL %d\n", blockmap_width, blockmap_height, btotal);

    // setup blk_cur_lines array.  Initially all pointers are nullptr, when
    // any lines get added then the dynamic array is created.

    blockmap_lines = new std::list<Line *> *[btotal];

    EPI_CLEAR_MEMORY(blockmap_lines, std::list<Line *> *, btotal);

    // process each linedef of the map
    for (int i = 0; i < total_level_lines; i++)
        BlockAddLine(i);

    // LogDebug("GenerateBlockmap: TOTAL DATA=%d\n", blk_total_lines);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
