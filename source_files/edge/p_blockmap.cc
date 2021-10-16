//----------------------------------------------------------------------------
//  EDGE Blockmap utility functions
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2009  The EDGE Team.
// 
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
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

#include "i_defs.h"
#include "i_defs_gl.h"  // needed for r_shader.h

#include <float.h>

#include <vector>
#include <algorithm>

#include "dm_data.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "m_bbox.h"
#include "p_local.h"
#include "p_spec.h"
#include "r_shader.h"
#include "r_state.h"
#include "z_zone.h"


// FIXME: have a proper API
extern abstract_shader_c *MakeDLightShader(mobj_t *mo);
extern abstract_shader_c *MakePlaneGlow(mobj_t *mo);


#define MAXRADIUS  128.0

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
int bmap_width;
int bmap_height;  // size in mapblocks

// origin of block map
float bmap_orgx;
float bmap_orgy;

typedef std::list<line_t *> linedef_set_t;

static linedef_set_t **bmap_lines = NULL;

// for thing chains
mobj_t **bmap_things = NULL;

// for dynamic lights
int dlmap_width;
int dlmap_height;

mobj_t **dlmap_things = NULL;


void P_CreateThingBlockMap(void)
{
	bmap_things  = new mobj_t* [bmap_width * bmap_height];

	Z_Clear(bmap_things,  mobj_t*, bmap_width * bmap_height);

	// compute size of dynamic light blockmap
	dlmap_width  = (bmap_width  * BLOCKMAP_UNIT + LIGHTMAP_UNIT-1) / LIGHTMAP_UNIT;
	dlmap_height = (bmap_height * BLOCKMAP_UNIT + LIGHTMAP_UNIT-1) / LIGHTMAP_UNIT;

	I_Debugf("Blockmap size: %dx%d --> Lightmap size: %dx%x\n",
			 bmap_width, bmap_height, dlmap_width, dlmap_height);

	dlmap_things = new mobj_t* [dlmap_width * dlmap_height];

	Z_Clear(dlmap_things, mobj_t*, dlmap_width * dlmap_height);
}

void P_DestroyBlockMap(void)
{
	delete[] bmap_lines;    bmap_lines  = NULL;
	delete[] bmap_things;   bmap_things = NULL;

	delete[] dlmap_things;  dlmap_things = NULL;
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
touch_node_t *free_touch_nodes;

static inline touch_node_t *TouchNodeAlloc(void)
{
	touch_node_t *tn;

#ifdef DEVELOPERS
	touchstat_alloc++;
#endif

	if (free_touch_nodes)
	{
		tn = free_touch_nodes;
		free_touch_nodes = tn->mo_next;
	}
	else
	{
		tn = Z_New(touch_node_t, 1);
	}

	return tn;
}

static inline void TouchNodeFree(touch_node_t *tn)
{
#ifdef DEVELOPERS
	touchstat_free++;
#endif

	// PREV field is ignored in quick-alloc list
	tn->mo_next = free_touch_nodes;
	free_touch_nodes = tn;
}

static inline void TouchNodeLinkIntoSector(touch_node_t *tn, sector_t *sec)
{
	tn->sec = sec;

	tn->sec_next = sec->touch_things;
	tn->sec_prev = NULL;

	if (tn->sec_next)
		tn->sec_next->sec_prev = tn;

	sec->touch_things = tn;
}

static inline void TouchNodeLinkIntoThing(touch_node_t *tn, mobj_t *mo)
{
	tn->mo = mo;

	tn->mo_next = mo->touch_sectors;
	tn->mo_prev = NULL;

	if (tn->mo_next)
		tn->mo_next->mo_prev = tn;

	mo->touch_sectors = tn;
}

static inline void TouchNodeUnlinkFromSector(touch_node_t *tn)
{
	if (tn->sec_next)
		tn->sec_next->sec_prev = tn->sec_prev;

	if (tn->sec_prev)
		tn->sec_prev->sec_next = tn->sec_next;
	else
		tn->sec->touch_things = tn->sec_next;
}

static inline void TouchNodeUnlinkFromThing(touch_node_t *tn)
{
	if (tn->mo_next)
		tn->mo_next->mo_prev = tn->mo_prev;

	if (tn->mo_prev)
		tn->mo_prev->mo_next = tn->mo_next;
	else
		tn->mo->touch_sectors = tn->mo_next;
}

typedef struct setposbsp_s
{
	mobj_t *thing;

	float bbox[4];
}
setposbsp_t;

//
// SetPositionBSP
//
static void SetPositionBSP(setposbsp_t *info, int nodenum)
{
	touch_node_t *tn;
	sector_t *sec;
	subsector_t *sub;
	seg_t *seg;

	while (! (nodenum & NF_V5_SUBSECTOR))
	{
		node_t *nd = nodes + nodenum;

		int side = P_BoxOnDivLineSide(info->bbox, &nd->div);

		// if box touches partition line, we must traverse both sides
		if (side == -1)
		{
			SetPositionBSP(info, nd->children[0]);
			side = 1;
		}

		SYS_ASSERT(side == 0 || side == 1);

		nodenum = nd->children[side];
	}

	// reached a leaf of the BSP.  Need to check BBOX against all
	// linedef segs.  This is because we can get false positives, since
	// we don't actually split the thing's BBOX when it intersects with
	// a partition line.

	sub = subsectors + (nodenum & ~NF_V5_SUBSECTOR);

	for (seg=sub->segs; seg; seg=seg->sub_next)
	{
		divline_t div;

		if (seg->miniseg)
			continue;

		div.x = seg->v1->x;
		div.y = seg->v1->y;
		div.dx = seg->v2->x - div.x;
		div.dy = seg->v2->y - div.y;

		if (P_BoxOnDivLineSide(info->bbox, &div) == 1)
			return;
	}

	// Perform linkage...

	sec = sub->sector;

#ifdef DEVELOPERS
	touchstat_miss++;
#endif

	for (tn = info->thing->touch_sectors; tn; tn = tn->mo_next)
	{
		if (! tn->mo)
		{
			// found unused touch node.  We reuse it.
			tn->mo = info->thing;

			if (tn->sec != sec)
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

		SYS_ASSERT(tn->mo == info->thing);

		// sector already present ?
		if (tn->sec == sec)
			return;
	}

	// need to allocate a new touch node
	tn = TouchNodeAlloc();

	TouchNodeLinkIntoThing(tn, info->thing);
	TouchNodeLinkIntoSector(tn, sec);
}

//
// P_UnsetThingPosition
//
// Unlinks a thing from block map and subsector.
// On each position change, BLOCKMAP and other
// lookups maintaining lists ot things inside
// these structures need to be updated.
//
// -ES- 1999/12/04 Better error checking: Clear prev/next fields.
// This catches errors which can occur if the position is unset twice.
//
void P_UnsetThingPosition(mobj_t * mo)
{
	int blockx;
	int blocky;
	int bnum;

	touch_node_t *tn;

	// unlink from subsector
	if (!(mo->flags & MF_NOSECTOR))
	{
		// (inert things don't need to be in subsector list)

		if (mo->snext)
		{
			SYS_ASSERT(mo->snext->sprev == mo);

			mo->snext->sprev = mo->sprev;
		}

		if (mo->sprev)
		{
			SYS_ASSERT(mo->sprev->snext == mo);

			mo->sprev->snext = mo->snext;
		}
		else
		{
			SYS_ASSERT(mo->subsector->thinglist == mo);

			mo->subsector->thinglist = mo->snext;
		}

		mo->snext = NULL;
		mo->sprev = NULL;
	}

	// unlink from touching list.
	// NOTE: lazy unlinking -- see notes in r_defs.h
	//
	for (tn = mo->touch_sectors; tn; tn = tn->mo_next)
	{
		tn->mo = NULL;
	}

	// unlink from blockmap
	if (!(mo->flags & MF_NOBLOCKMAP))
	{
		// inert things don't need to be in blockmap
		if (mo->bnext)
		{
			SYS_ASSERT(mo->bnext->bprev == mo);

			mo->bnext->bprev = mo->bprev;
		}

		if (mo->bprev)
		{
			SYS_ASSERT(mo->bprev->bnext == mo);

			mo->bprev->bnext = mo->bnext;
		}
		else
		{
			blockx = BLOCKMAP_GET_X(mo->x);
			blocky = BLOCKMAP_GET_Y(mo->y);

			if (blockx >= 0 && blockx < bmap_width &&
				blocky >= 0 && blocky < bmap_height)
			{
				bnum = blocky * bmap_width + blockx;

				SYS_ASSERT(bmap_things[bnum] == mo);

				bmap_things[bnum] = mo->bnext;
			}
		}

		mo->bprev = NULL;
		mo->bnext = NULL;
	}

	// unlink from dynamic light blockmap
	if (mo->info && (mo->info->dlight[0].type != DLITE_None) &&
		(mo->info->glow_type == GLOW_None))
	{
		if (mo->dlnext)
		{
			SYS_ASSERT(mo->dlnext->dlprev == mo);

			mo->dlnext->dlprev = mo->dlprev;
		}

		if (mo->dlprev)
		{
			SYS_ASSERT(mo->dlprev->dlnext == mo);

			mo->dlprev->dlnext = mo->dlnext;
		}
		else
		{
			blockx = LIGHTMAP_GET_X(mo->x);
			blocky = LIGHTMAP_GET_Y(mo->y);

			if (blockx >= 0 && blockx < dlmap_width &&
				blocky >= 0 && blocky < dlmap_height)
			{
				bnum = blocky * dlmap_width + blockx;

				SYS_ASSERT(dlmap_things[bnum] == mo);
				dlmap_things[bnum] = mo->dlnext;
			}
		}

		mo->dlprev = NULL;
		mo->dlnext = NULL;
	}

	// unlink from sector glow list
	if (mo->info && (mo->info->dlight[0].type != DLITE_None) &&
		(mo->info->glow_type != GLOW_None))
	{
		sector_t *sec = mo->subsector->sector;

		if (mo->dlnext)
		{
			SYS_ASSERT(mo->dlnext->dlprev == mo);

			mo->dlnext->dlprev = mo->dlprev;
		}

		if (mo->dlprev)
		{
			SYS_ASSERT(mo->dlprev->dlnext == mo);

			mo->dlprev->dlnext = mo->dlnext;
		}
		else
		{
			SYS_ASSERT(sec->glow_things == mo);

			sec->glow_things = mo->dlnext;
		}

		mo->dlprev = NULL;
		mo->dlnext = NULL;
	}
}

//
// P_UnsetThingFinally
// 
// Call when the thing is about to be removed for good.
// 
void P_UnsetThingFinally(mobj_t * mo)
{
	touch_node_t *tn;

	P_UnsetThingPosition(mo);

	// clear out touch nodes

	while (mo->touch_sectors)
	{
		tn = mo->touch_sectors;
		mo->touch_sectors = tn->mo_next;

		TouchNodeUnlinkFromSector(tn);
		TouchNodeFree(tn);
	}
}

//
// P_SetThingPosition
//
// Links a thing into both a block and a subsector
// based on it's x y.
//
void P_SetThingPosition(mobj_t * mo)
{
	subsector_t *ss;
	int blockx;
	int blocky;
	int bnum;

	setposbsp_t pos;
	touch_node_t *tn;

	// -ES- 1999/12/04 The position must be unset before it's set again.
	if (mo->snext || mo->sprev || mo->bnext || mo->bprev)
		I_Error("INTERNAL ERROR: Double P_SetThingPosition call.");

	SYS_ASSERT(! (mo->dlnext || mo->dlprev));

	// link into subsector
	ss = R_PointInSubsector(mo->x, mo->y);
	mo->subsector = ss;

	// determine properties
	mo->props = R_PointGetProps(ss, mo->z + mo->height/2);

	if (! (mo->flags & MF_NOSECTOR))
	{
		mo->snext = ss->thinglist;
		mo->sprev = NULL;

		if (ss->thinglist)
			ss->thinglist->sprev = mo;

		ss->thinglist = mo;
	}

	// link into touching list

#ifdef DEVELOPERS
	touchstat_moves++;
#endif

	pos.thing = mo;
	pos.bbox[BOXLEFT]   = mo->x - mo->radius;
	pos.bbox[BOXRIGHT]  = mo->x + mo->radius;
	pos.bbox[BOXBOTTOM] = mo->y - mo->radius;
	pos.bbox[BOXTOP]    = mo->y + mo->radius;

	SetPositionBSP(&pos, root_node);

	// handle any left-over unused touch nodes

	for (tn = mo->touch_sectors; tn && tn->mo; tn = tn->mo_next)
	{ /* nothing here */ }

	if (tn)
	{
		if (tn->mo_prev)
			tn->mo_prev->mo_next = NULL;
		else
			mo->touch_sectors = NULL;

		while (tn)
		{
			touch_node_t *cur = tn;
			tn = tn->mo_next;

			SYS_ASSERT(! cur->mo);

			TouchNodeUnlinkFromSector(cur);
			TouchNodeFree(cur);
		}
	}

#if 0  // PROFILING
	{
		static int last_time = 0;

		if ((leveltime - last_time) > 5*TICRATE)
		{
			L_WriteDebug("TOUCHSTATS: Mv=%d Ht=%d Ms=%d Al=%d Fr=%d\n",
				touchstat_moves, touchstat_hit, touchstat_miss,
				touchstat_alloc, touchstat_free);

			touchstat_moves = touchstat_hit = touchstat_miss =
				touchstat_alloc = touchstat_free = 0;

			last_time = leveltime;
		}
	}
#endif

	// link into blockmap
	if (!(mo->flags & MF_NOBLOCKMAP))
	{
		blockx = BLOCKMAP_GET_X(mo->x);
		blocky = BLOCKMAP_GET_Y(mo->y);

		if (blockx >= 0 && blockx < bmap_width &&
			blocky >= 0 && blocky < bmap_height)
		{
			bnum = blocky * bmap_width + blockx;

			mo->bprev = NULL;
			mo->bnext = bmap_things[bnum];

			if (bmap_things[bnum])
				(bmap_things[bnum])->bprev = mo;

			bmap_things[bnum] = mo;
		}
		else
		{
			// thing is off the map
			mo->bnext = mo->bprev = NULL;
		}
	}

	// link into dynamic light blockmap
	if (mo->info && (mo->info->dlight[0].type != DLITE_None) &&
		(mo->info->glow_type == GLOW_None))
	{
		blockx = LIGHTMAP_GET_X(mo->x);
		blocky = LIGHTMAP_GET_Y(mo->y);

		if (blockx >= 0 && blockx < dlmap_width &&
			blocky >= 0 && blocky < dlmap_height)
		{
			bnum = blocky * dlmap_width + blockx;

			mo->dlprev = NULL;
			mo->dlnext = dlmap_things[bnum];

			if (dlmap_things[bnum])
				(dlmap_things[bnum])->dlprev = mo;

			dlmap_things[bnum] = mo;
		}
		else
		{
			// thing is off the map
			mo->dlnext = mo->dlprev = NULL;
		}
	}

	// link into sector glow list
	if (mo->info && (mo->info->dlight[0].type != DLITE_None) &&
		(mo->info->glow_type != GLOW_None))
	{
		sector_t *sec = mo->subsector->sector;

		mo->dlprev = NULL;
		mo->dlnext = sec->glow_things;

		sec->glow_things = mo;
	}
}

//
// P_ChangeThingPosition
//
// New routine to "atomicly" move a thing.  Apart from object
// construction and destruction, this routine should always be called
// when moving a thing, rather than fiddling with the coordinates
// directly (or even P_UnsetThingPos/P_SetThingPos pairs).
// 
void P_ChangeThingPosition(mobj_t * mo, float x, float y, float z)
{
	P_UnsetThingPosition(mo);
	{
		mo->x = x;
		mo->y = y;
		mo->z = z;
	}
	P_SetThingPosition(mo);
}

//
// P_FreeSectorTouchNodes
// 
void P_FreeSectorTouchNodes(sector_t *sec)
{
	touch_node_t *tn;

	for (tn = sec->touch_things; tn; tn = tn->sec_next)
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
// P_BlockLinesIterator
//
// The validcount flags are used to avoid checking lines
// that are marked in multiple mapblocks,
// so increment validcount before the first call
// to P_BlockLinesIterator, then make one or more calls
// to it.
//
bool P_BlockLinesIterator(float x1, float y1, float x2, float y2,
		                  bool(* func)(line_t *, void *), void *data)
{
	validcount++;

	int lx = BLOCKMAP_GET_X(x1);
	int ly = BLOCKMAP_GET_Y(y1);
	int hx = BLOCKMAP_GET_X(x2);
	int hy = BLOCKMAP_GET_Y(y2);

	lx = MAX(0, lx);  hx = MIN(bmap_width-1,  hx);
	ly = MAX(0, ly);  hy = MIN(bmap_height-1, hy);

	for (int by = ly; by <= hy; by++)
	for (int bx = lx; bx <= hx; bx++)
	{
		linedef_set_t *lset = bmap_lines[by * bmap_width + bx];

		if (! lset)
			continue;

		linedef_set_t::iterator LI;
		for (LI = lset->begin(); LI != lset->end(); LI++)
		{
			line_t *ld = *LI;

			// has line already been checked ?
			if (ld->validcount == validcount)
				continue;

			ld->validcount = validcount;

			// check whether line touches the given bbox
			if (ld->bbox[BOXRIGHT] <= x1 || ld->bbox[BOXLEFT]   >= x2 ||
				ld->bbox[BOXTOP]   <= y1 || ld->bbox[BOXBOTTOM] >= y2)
			{
				continue;
			}

			if (! func(ld, data))
				return false;
		}
	}

	// everything was checked
	return true;
}


bool P_BlockThingsIterator(float x1, float y1, float x2, float y2,
		                   bool (*func)(mobj_t *, void*), void *data)
{
	// need to expand the source by one block because large
	// things (radius limited to BLOCKMAP_UNIT) can overlap
	// into adjacent blocks.

	int lx = BLOCKMAP_GET_X(x1) - 1;
	int ly = BLOCKMAP_GET_Y(y1) - 1;
	int hx = BLOCKMAP_GET_X(x2) + 1;
	int hy = BLOCKMAP_GET_Y(y2) + 1;

	lx = MAX(0, lx);  hx = MIN(bmap_width-1,  hx);
	ly = MAX(0, ly);  hy = MIN(bmap_height-1, hy);

	for (int by = ly; by <= hy; by++)
	for (int bx = lx; bx <= hx; bx++)
	{
		for (mobj_t *mo = bmap_things[by * bmap_width + bx]; mo; mo = mo->bnext)
		{
			// check whether thing touches the given bbox
			float r = mo->radius;

			if (mo->x + r <= x1 || mo->x - r >= x2 ||
			    mo->y + r <= y1 || mo->y - r >= y2)
				continue;

			if (! func(mo, data))
				return false;
		}
	}

	return true;
}


void P_DynamicLightIterator(float x1, float y1, float z1,
		                    float x2, float y2, float z2,
		                    void (*func)(mobj_t *, void *), void *data)
{
	int lx = LIGHTMAP_GET_X(x1) - 1;
	int ly = LIGHTMAP_GET_Y(y1) - 1;
	int hx = LIGHTMAP_GET_X(x2) + 1;
	int hy = LIGHTMAP_GET_Y(y2) + 1;

	lx = MAX(0, lx);  hx = MIN(dlmap_width-1,  hx);
	ly = MAX(0, ly);  hy = MIN(dlmap_height-1, hy);

	for (int by = ly; by <= hy; by++)
	for (int bx = lx; bx <= hx; bx++)
	{
		for (mobj_t *mo = dlmap_things[by * dlmap_width + bx]; mo; mo = mo->dlnext)
		{
			SYS_ASSERT(mo->state);

			// skip "off" lights
			if (mo->state->bright <= 0 || mo->dlight.r <= 0)
				continue;

			// check whether radius touches the given bbox
			float r = mo->dlight.r;

			if (mo->x + r <= x1 || mo->x - r >= x2 ||
			    mo->y + r <= y1 || mo->y - r >= y2 ||
				mo->z + r <= z1 || mo->z - r >= z2)
				continue;
			
			// create shader if necessary
			if (! mo->dlight.shader)
				  mo->dlight.shader = MakeDLightShader(mo);

//			mo->dlight.shader->CheckReset();

			func(mo, data);
		}
	}
}


void P_SectorGlowIterator(sector_t *sec,
	                   	  float x1, float y1, float z1,
	                   	  float x2, float y2, float z2,
		                  void (*func)(mobj_t *, void *), void *data)
{
	for (mobj_t *mo = sec->glow_things; mo; mo = mo->dlnext)
	{
		SYS_ASSERT(mo->state);

		// skip "off" lights
		if (mo->state->bright <= 0 || mo->dlight.r <= 0)
			continue;

		// check whether radius touches the given bbox
		float r = mo->dlight.r;

		if (mo->info->glow_type == GLOW_Floor && sec->f_h + r <= z1)
			continue;
		
		if (mo->info->glow_type == GLOW_Ceiling && sec->c_h - r >= z1)
			continue;
		
		// create shader if necessary
		if (! mo->dlight.shader)
			  mo->dlight.shader = MakePlaneGlow(mo);

//		mo->dlight.shader->CheckReset();

		func(mo, data);
	}
}


//--------------------------------------------------------------------------
//
// INTERCEPT ROUTINES
//

static std::vector<intercept_t> intercepts;

divline_t trace;


float P_InterceptVector(divline_t * v2, divline_t * v1)
{
	// Returns the fractional intercept point along the first divline.
	// This is only called by the addthings and addlines traversers.

	float den = (v1->dy * v2->dx) - (v1->dx * v2->dy);

	if (den == 0)
		return 0;  // parallel

	float num = (v1->x - v2->x) * v1->dy + (v2->y - v1->y) * v1->dx;

	return num / den;
}


static inline void PIT_AddLineIntercept(line_t * ld)
{
	// Looks for lines in the given block
	// that intercept the given trace
	// to add to the intercepts list.
	//
	// A line is crossed if its endpoints
	// are on opposite sides of the trace.
	// Returns true if earlyout and a solid line hit.

	// has line already been checked ?
	if (ld->validcount == validcount)
		return;

	ld->validcount = validcount;

	int s1;
	int s2;
	float frac;
	divline_t div;

	div.x = ld->v1->x;
	div.y = ld->v1->y;
	div.dx = ld->dx;
	div.dy = ld->dy;

	// avoid precision problems with two routines
	if (trace.dx > 16 || trace.dy > 16 || trace.dx < -16 || trace.dy < -16)
	{
		s1 = P_PointOnDivlineSide(ld->v1->x, ld->v1->y, &trace);
		s2 = P_PointOnDivlineSide(ld->v2->x, ld->v2->y, &trace);
	}
	else
	{
		s1 = P_PointOnDivlineSide(trace.x, trace.y, &div);
		s2 = P_PointOnDivlineSide(trace.x + trace.dx, trace.y + trace.dy, &div);
	}

	// line isn't crossed ?
	if (s1 == s2)
		return;

	// hit the line

	frac = P_InterceptVector(&trace, &div);

	// out of range?
	if (frac < 0 || frac > 1)
		return;

	// Intercept is a simple struct that can be memcpy()'d: Load
	// up a structure and get into the array
	intercept_t in;

	in.frac  = frac;
	in.thing = NULL;
	in.line  = ld;

	intercepts.push_back(in);
}

static inline void PIT_AddThingIntercept(mobj_t * thing)
{
	float x1;
	float y1;
	float x2;
	float y2;

	int s1;
	int s2;

	bool tracepositive;

	divline_t div;

	float frac;

	tracepositive = (trace.dx >= 0) == (trace.dy >= 0);

	// check a corner to corner crossection for hit
	if (tracepositive)
	{
		x1 = thing->x - thing->radius;
		y1 = thing->y + thing->radius;

		x2 = thing->x + thing->radius;
		y2 = thing->y - thing->radius;
	}
	else
	{
		x1 = thing->x - thing->radius;
		y1 = thing->y - thing->radius;

		x2 = thing->x + thing->radius;
		y2 = thing->y + thing->radius;
	}

	s1 = P_PointOnDivlineSide(x1, y1, &trace);
	s2 = P_PointOnDivlineSide(x2, y2, &trace);

	// line isn't crossed ?
	if (s1 == s2)
		return;

	div.x = x1;
	div.y = y1;
	div.dx = x2 - x1;
	div.dy = y2 - y1;

	frac = P_InterceptVector(&trace, &div);

	// out of range?
	if (frac < 0 || frac > 1)
		return;

	// Intercept is a simple struct that can be memcpy()'d: Load
	// up a structure and get into the array
	intercept_t in;

	in.frac  = frac;
	in.thing = thing;
	in.line  = NULL;

	intercepts.push_back(in);
}


struct Compare_Intercept_pred
{
	inline bool operator() (const intercept_t& A, const intercept_t& B) const
	{
		return A.frac < B.frac;
	}
};

//
// P_PathTraverse
//
// Traces a line from x1,y1 to x2,y2,
// calling the traverser function for each.
// Returns true if the traverser function returns true
// for all lines.
//
bool P_PathTraverse(float x1, float y1, float x2, float y2, int flags,
		            bool (* func)(intercept_t *, void *), void *data)
{
	validcount++;

	intercepts.clear();

	// don't side exactly on a line
	if (fmod(x1 - bmap_orgx, BLOCKMAP_UNIT) == 0)
		x1 += 0.1f;

	if (fmod(y1 - bmap_orgy, BLOCKMAP_UNIT) == 0)
		y1 += 0.1f;

	trace.x = x1;
	trace.y = y1;
	trace.dx = x2 - x1;
	trace.dy = y2 - y1;

	x1 -= bmap_orgx;
	y1 -= bmap_orgy;
	x2 -= bmap_orgx;
	y2 -= bmap_orgy;

	int bx1 = (int)(x1 / BLOCKMAP_UNIT);
	int by1 = (int)(y1 / BLOCKMAP_UNIT);
	int bx2 = (int)(x2 / BLOCKMAP_UNIT);
	int by2 = (int)(y2 / BLOCKMAP_UNIT);

	int bx_step;
	int by_step;

	float xstep;
	float ystep;

	float partial;

	double tmp;


	if (bx2 > bx1 && (x2 - x1) > 0.001)
	{
		bx_step = 1;
		partial = 1.0f - modf(x1 / BLOCKMAP_UNIT, &tmp);

		ystep = (y2 - y1) / fabs(x2 - x1);
	}
	else if (bx2 < bx1 && (x2 - x1) < -0.001)
	{
		bx_step = -1;
		partial = modf(x1 / BLOCKMAP_UNIT, &tmp);

		ystep = (y2 - y1) / fabs(x2 - x1);
	}
	else
	{
		bx_step = 0;
		partial = 1.0f;
		ystep = 256.0f;
	}

	float yintercept = y1 / BLOCKMAP_UNIT + partial * ystep;

	if (by2 > by1 && (y2 - y1) > 0.001)
	{
		by_step = 1;
		partial = 1.0f - modf(y1 / BLOCKMAP_UNIT, &tmp);

		xstep = (x2 - x1) / fabs(y2 - y1);
	}
	else if (by2 < by1 && (y2 - y1) < -0.001)
	{
		by_step = -1;
		partial = modf(y1 / BLOCKMAP_UNIT, &tmp);

		xstep = (x2 - x1) / fabs(y2 - y1);
	}
	else
	{
		by_step = 0;
		partial = 1.0f;
		xstep = 256.0f;
	}

	float xintercept = x1 / BLOCKMAP_UNIT + partial * xstep;

	// Step through map blocks.
	// Count is present to prevent a round off error
	// from skipping the break.
	int bx = bx1;
	int by = by1;

	for (int count = 0; count < 64; count++)
	{
		if (0 <= bx && bx < bmap_width &&
			0 <= by && by < bmap_height)
		{
			if (flags & PT_ADDLINES)
			{
				linedef_set_t *lset = bmap_lines[by * bmap_width + bx];
				
				if (lset)
				{
					linedef_set_t::iterator LI;
					for (LI = lset->begin(); LI != lset->end(); LI++)
					{
						PIT_AddLineIntercept(*LI);
					}
				}
			}

			if (flags & PT_ADDTHINGS)
			{
				for (mobj_t *mo = bmap_things[by * bmap_width + bx]; mo; mo = mo->bnext)
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

	std::sort(intercepts.begin(), intercepts.end(),
			  Compare_Intercept_pred());

	std::vector<intercept_t>::iterator I;

	for (I = intercepts.begin(); I != intercepts.end(); I++)
	{
		if (! func(& *I, data))
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

static void BlockAdd(int bnum, line_t *ld)
{
	if (! bmap_lines[bnum])
		bmap_lines[bnum] = new linedef_set_t;

	bmap_lines[bnum]->push_back(ld);

	// blk_total_lines++;
}

static void BlockAddLine(int line_num)
{
	int i, j;
	int x0, y0;
	int x1, y1;

	line_t *ld = lines + line_num;

	int blocknum;

	int y_sign;
	int x_dist, y_dist;

	float slope;

	x0 = (int)(ld->v1->x - bmap_orgx);
	y0 = (int)(ld->v1->y - bmap_orgy);
	x1 = (int)(ld->v2->x - bmap_orgx);
	y1 = (int)(ld->v2->y - bmap_orgy);

	// swap endpoints if horizontally backward
	if (x1 < x0)
	{
		int temp;

		temp = x0; x0 = x1; x1 = temp;
		temp = y0; y0 = y1; y1 = temp;
	}

	SYS_ASSERT(0 <= x0 && (x0 / BLOCKMAP_UNIT) < bmap_width);
	SYS_ASSERT(0 <= y0 && (y0 / BLOCKMAP_UNIT) < bmap_height);
	SYS_ASSERT(0 <= x1 && (x1 / BLOCKMAP_UNIT) < bmap_width);
	SYS_ASSERT(0 <= y1 && (y1 / BLOCKMAP_UNIT) < bmap_height);

	// check if this line spans multiple blocks.

	x_dist = ABS((x1 / BLOCKMAP_UNIT) - (x0 / BLOCKMAP_UNIT));
	y_dist = ABS((y1 / BLOCKMAP_UNIT) - (y0 / BLOCKMAP_UNIT));

	y_sign = (y1 >= y0) ? 1 : -1;

	// handle the simple cases: same column or same row

	blocknum = (y0 / BLOCKMAP_UNIT) * bmap_width + (x0 / BLOCKMAP_UNIT);

	if (y_dist == 0)
	{
		for (i=0; i <= x_dist; i++, blocknum++)
			BlockAdd(blocknum, ld);

		return;
	}

	if (x_dist == 0)
	{
		for (i=0; i <= y_dist; i++, blocknum += y_sign * bmap_width)
			BlockAdd(blocknum, ld);

		return;
	}

	// -AJA- 2000/12/09: rewrote the general case

	SYS_ASSERT(x1 > x0);

	slope = (float)(y1 - y0) / (float)(x1 - x0);

	// handle each column of blocks in turn
	for (i=0; i <= x_dist; i++)
	{
		// compute intersection of column with line
		int sx = (i==0)      ? x0 : (128 * (x0 / 128 + i));
		int ex = (i==x_dist) ? x1 : (128 * (x0 / 128 + i) + 127);

		int sy = y0 + (int)(slope * (sx - x0));
		int ey = y0 + (int)(slope * (ex - x0));

		SYS_ASSERT(sx <= ex);

		y_dist = ABS((ey / 128) - (sy / 128));

		for (j=0; j <= y_dist; j++)
		{
			blocknum = (sy / 128 + j * y_sign) * bmap_width + (sx / 128);

			BlockAdd(blocknum, ld);
		}
	}
}


void P_GenerateBlockMap(int min_x, int min_y, int max_x, int max_y)
{
	bmap_orgx = min_x - 8;
	bmap_orgy = min_y - 8;
	bmap_width  = BLOCKMAP_GET_X(max_x) + 1;
	bmap_height = BLOCKMAP_GET_Y(max_y) + 1;

	int btotal = bmap_width * bmap_height;

	L_WriteDebug("GenerateBlockmap: MAP (%d,%d) -> (%d,%d)\n",
		min_x, min_y, max_x, max_y);
	L_WriteDebug("GenerateBlockmap: BLOCKS %d x %d  TOTAL %d\n",
		bmap_width, bmap_height, btotal);

	// setup blk_cur_lines array.  Initially all pointers are NULL, when
	// any lines get added then the dynamic array is created.

	bmap_lines = new linedef_set_t* [btotal];

	Z_Clear(bmap_lines, linedef_set_t *, btotal);

	// process each linedef of the map
	for (int i=0; i < numlines; i++)
		BlockAddLine(i);

	// L_WriteDebug("GenerateBlockmap: TOTAL DATA=%d\n", blk_total_lines);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
