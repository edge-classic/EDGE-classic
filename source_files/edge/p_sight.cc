//----------------------------------------------------------------------------
//  EDGE Sight Code
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

#include "i_defs.h"

#include <math.h>

#include <vector>

#include "dm_data.h"
#include "dm_defs.h"
#include "dm_structs.h"
#include "m_bbox.h"
#include "p_local.h"
#include "r_state.h"
#include "z_zone.h"

#define DEBUG_SIGHT  0


typedef struct sight_info_s
{
	// source position (dx/dy is vector to dest)
	divline_t src;
	float src_z;
	subsector_t *src_sub;

	// dest position
	vec2_t dest;
	float dest_z;
	subsector_t *dest_sub;

	// angle from src->dest, for fast seg check
	angle_t angle;

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
	float bbox[4];

	// true if one of the sectors contained extrafloors
	bool exfloors;
}
sight_info_t;

static sight_info_t sight_I;


// intercepts found during first pass

typedef struct wall_intercept_s
{
	// fractional distance, 0.0 -> 1.0
	float frac;

	// sector that faces the source from this intercept point
	sector_t *sector;
}
wall_intercept_t;

// intercept array
static std::vector<wall_intercept_t> wall_icpts;

// for profiling...
#ifdef DEVELOPERS
int sight_rej_hit;
int sight_rej_miss;
#endif


static inline void AddSightIntercept(float frac, sector_t *sec)
{
	wall_intercept_t WI;

	WI.frac = frac;
	WI.sector = sec;
	
	wall_icpts.push_back(WI);
}

//
// CrossSubsector
//
// Returns false if LOS is blocked by the given subsector, otherwise
// true.  Note: extrafloors are not checked here.
//
static bool CrossSubsector(subsector_t *sub)
{
	seg_t *seg;
	line_t *ld;

	int s1, s2;

	sector_t *front;
	sector_t *back;
	divline_t divl;

	float frac;
	float slope;

	// check lines
	for (seg = sub->segs; seg != NULL; seg = seg->sub_next)
	{
		if (seg->miniseg)
			continue;

		// ignore segs that face away from the source.  We only want to
		// process linedefs on the _far_ side of each subsector.
		//
		if ((angle_t)(seg->angle - sight_I.angle) < ANG180)
			continue;

		ld = seg->linedef;

		// line already checked ? (e.g. multiple segs on it)
		if (ld->validcount == validcount)
			continue;

		ld->validcount = validcount;

		// line outside of bbox ?
		if (ld->bbox[BOXLEFT] > sight_I.bbox[BOXRIGHT] ||
			ld->bbox[BOXRIGHT] < sight_I.bbox[BOXLEFT] ||
			ld->bbox[BOXBOTTOM] > sight_I.bbox[BOXTOP] ||
			ld->bbox[BOXTOP] < sight_I.bbox[BOXBOTTOM])
			continue;

		// does linedef cross LOS ?
		s1 = P_PointOnDivlineSide(ld->v1->x, ld->v1->y, &sight_I.src);
		s2 = P_PointOnDivlineSide(ld->v2->x, ld->v2->y, &sight_I.src);

		if (s1 == s2)
			continue;

		// linedef crosses LOS (extended to infinity), now check if the
		// cross point lies within the finite LOS range.
		// 
		divl.x  = ld->v1->x;
		divl.y  = ld->v1->y;
		divl.dx = ld->dx;
		divl.dy = ld->dy;

		s1 = P_PointOnDivlineSide(sight_I.src.x, sight_I.src.y, &divl);
		s2 = P_PointOnDivlineSide(sight_I.dest.x, sight_I.dest.y, &divl);

		if (s1 == s2)
			continue;

		// stop because it is not two sided anyway
		if (!(ld->flags & MLF_TwoSided) || ld->blocked)
		{
			return false;
		}

		// line explicitly blocks sight ?  (XDoom compatibility)
		if (ld->flags & MLF_SightBlock)
			return false;

		// -AJA- 2001/11/11: closed Sliding door ?
		if (ld->slide_door && ! ld->slide_door->s.see_through &&
			! ld->slider_move)
		{
			return false;
		}

		front = seg->frontsector;
		back = seg->backsector;

		SYS_ASSERT(back);

		// compute intercept vector (fraction from 0 to 1)
		{
			float num, den;

			den = divl.dy * sight_I.src.dx - divl.dx * sight_I.src.dy;

			// parallel ?  
			// -AJA- probably can't happen due to the above Divline checks
			if (fabs(den) < 0.0001)
				continue;

			num = (divl.x - sight_I.src.x) * divl.dy + 
				(sight_I.src.y - divl.y) * divl.dx;

			frac = num / den;

			// too close to source ?
			if (frac < 0.0001f)
				continue;
		}

		if (front->f_h != back->f_h)
		{
			float openbottom = MAX(ld->frontsector->f_h, ld->backsector->f_h);
			slope = (openbottom - sight_I.src_z) / frac;
			if (slope > sight_I.bottom_slope)
				sight_I.bottom_slope = slope;
		}

		if (front->c_h != back->c_h)
		{
			float opentop = MIN(ld->frontsector->c_h, ld->backsector->c_h);
			slope = (opentop - sight_I.src_z) / frac;
			if (slope < sight_I.top_slope)
				sight_I.top_slope = slope;
		}

		// did our slope range close up ?
		if (sight_I.top_slope <= sight_I.bottom_slope)
			return false;

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
	SYS_ASSERT(bspnum >= 0);

	while (! (bspnum & NF_V5_SUBSECTOR))
	{
		node_t *node = nodes + bspnum;
		int s1, s2;

#if (DEBUG_SIGHT >= 2)
		L_WriteDebug("CheckSightBSP: node %d (%1.1f,%1.1f) + (%1.1f,%1.1f)\n",
			bspnum, node->div.x, node->div.y, node->div.dx, node->div.dy);
#endif

		// decide which side the src and dest points are on
		s1 = P_PointOnDivlineSide(sight_I.src.x, sight_I.src.y, &node->div);
		s2 = P_PointOnDivlineSide(sight_I.dest.x, sight_I.dest.y, &node->div);

#if (DEBUG_SIGHT >= 2)
		L_WriteDebug("  Sides: %d %d\n", s1, s2);
#endif

		// If sides are different, we must recursively check both.
		// NOTE WELL: we do the source side first, so that subsectors are
		// visited in the correct order (closest -> furthest away).

		if (s1 != s2)
		{
			if (! CheckSightBSP(node->children[s1]))
				return false;
		}

		bspnum = node->children[s2];
	}

	bspnum &= ~NF_V5_SUBSECTOR;

	SYS_ASSERT(0 <= bspnum && int(bspnum) < numsubsectors);

	{
		subsector_t *sub = subsectors + bspnum;

#if (DEBUG_SIGHT >= 2)
		L_WriteDebug("  Subsec %d  SEC %d\n", bspnum, sub->sector - sectors);
#endif

		if (sub->sector->exfloor_used > 0)
			sight_I.exfloors = true;

		// when target subsector is reached, there are no more lines to
		// check, since we only check lines on the _far_ side of the
		// subsector and the target object is inside its subsector.

		if (sub != sight_I.dest_sub)
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
	int i, j;
	sector_t *sec;

	float last_h = sight_I.src_z;
	float cur_h;

#if (DEBUG_SIGHT >= 1)
	L_WriteDebug("INTERCEPTS  slope %1.0f\n", slope);
#endif

	for (i=0; i < (int)wall_icpts.size(); i++, last_h = cur_h)
	{
		bool blocked = true;

		cur_h = sight_I.src_z + slope * wall_icpts[i].frac;

#if (DEBUG_SIGHT >= 1)
		L_WriteDebug("  %d/%d  FRAC %1.4f  SEC %d  H=%1.4f/%1.4f\n", i+1,
			wall_icpts.size(), wall_icpts[i].frac, 
			wall_icpts[i].sector - sectors, last_h, cur_h);
#endif

		// check all the sight gaps.
		sec = wall_icpts[i].sector;

		for (j=0; j < sec->sight_gap_num; j++)
		{
			float z1 = sec->sight_gaps[j].f;
			float z2 = sec->sight_gaps[j].c;

#if (DEBUG_SIGHT >= 3)
			L_WriteDebug("    SIGHT GAP [%d] = %1.1f .. %1.1f\n", j, z1, z2);
#endif

			if (z1 <= last_h && last_h <= z2 &&
				z1 <= cur_h && cur_h <= z2)
			{
				blocked = false;
				break;
			}
		}

		if (blocked)
			return false;
	}

	return true;
}

//
// CheckSightSameSubsector
//
// When the subsector is the same, we only need to check whether a
// non-SeeThrough extrafloor gets in the way.
// 
static bool CheckSightSameSubsector(mobj_t *src, mobj_t *dest)
{
	int j;
	sector_t *sec;

	float lower_z;
	float upper_z;

	if (sight_I.src_z < dest->z)
	{
		lower_z = sight_I.src_z;
		upper_z = dest->z;
	}
	else if (sight_I.src_z > dest->z + dest->height)
	{
		lower_z = dest->z + dest->height;
		upper_z = sight_I.src_z;
	}
	else
	{
		return true;
	}

	// check all the sight gaps.
	sec = src->subsector->sector;

	for (j=0; j < sec->sight_gap_num; j++)
	{
		float z1 = sec->sight_gaps[j].f;
		float z2 = sec->sight_gaps[j].c;

		if (z1 <= lower_z && upper_z <= z2)
			return true;
	}

	return false;
}

bool P_CheckSight(mobj_t * src, mobj_t * dest)
{
	// -ACB- 1998/07/20 t2 is Invisible, t1 cannot possibly see it.
	if (dest->visibility == INVISIBLE)
		return false;

	int n, num_div;

	float dest_heights[5];
	float dist_a;

	// First check for trivial rejection.

	SYS_ASSERT(src->subsector);
	SYS_ASSERT(dest->subsector);

	// An unobstructed LOS is possible.
	// Now look from eyes of t1 to any part of t2.

	validcount++;

	// The "eyes" of a thing is 75% of its height.
	SYS_ASSERT(src->info);
	sight_I.src_z = src->z + src->height * 
		PERCENT_2_FLOAT(src->info->viewheight);

	sight_I.src.x = src->x;
	sight_I.src.y = src->y;
	sight_I.src.dx = dest->x - src->x;
	sight_I.src.dy = dest->y - src->y;
	sight_I.src_sub = src->subsector;

	sight_I.dest.x = dest->x;
	sight_I.dest.y = dest->y;
	sight_I.dest_sub = dest->subsector;

	sight_I.bottom_slope = dest->z - sight_I.src_z;
	sight_I.top_slope = sight_I.bottom_slope + dest->height;

	// destination out of object's DDF slope range ?
	dist_a = P_ApproxDistance(sight_I.src.dx, sight_I.src.dy);

#if (DEBUG_SIGHT >= 1)
	L_WriteDebug("\n");
	L_WriteDebug("P_CheckSight:\n");
	L_WriteDebug("  Src: [%s] @ (%1.0f,%1.0f) in sub %d SEC %d\n", 
		src->info->name, sight_I.src.x, sight_I.src.y,
		sight_I.src_sub - subsectors, sight_I.src_sub->sector - sectors);
	L_WriteDebug("  Dest: [%s] @ (%1.0f,%1.0f) in sub %d SEC %d\n", 
		dest->info->name, sight_I.dest.x, sight_I.dest.y,
		sight_I.dest_sub - subsectors, sight_I.dest_sub->sector - sectors);
	L_WriteDebug("  Angle: %1.0f\n", ANG_2_FLOAT(sight_I.angle));
#endif

	if (sight_I.top_slope < dist_a * -src->info->sight_slope)
		return false;

	if (sight_I.bottom_slope > dist_a * src->info->sight_slope)
		return false;

	// -AJA- handle the case where no linedefs are crossed
	if (src->subsector == dest->subsector)
	{
		return CheckSightSameSubsector(src, dest);
	}

	sight_I.angle = R_PointToAngle(sight_I.src.x, sight_I.src.y,
		sight_I.dest.x, sight_I.dest.y);

	sight_I.bbox[BOXLEFT]   = MIN(sight_I.src.x, sight_I.dest.x);
	sight_I.bbox[BOXRIGHT]  = MAX(sight_I.src.x, sight_I.dest.x);
	sight_I.bbox[BOXBOTTOM] = MIN(sight_I.src.y, sight_I.dest.y);
	sight_I.bbox[BOXTOP]    = MAX(sight_I.src.y, sight_I.dest.y);

	wall_icpts.clear(); // FIXME

	sight_I.exfloors = false;

	// initial pass -- check for basic blockage & create intercepts
	if (! CheckSightBSP(root_node))
		return false;

	// no extrafloors encountered ?  Then the checks made by
	// CheckSightBSP are sufficient.  (-AJA- double check this)
	//
#if 1
	if (! sight_I.exfloors)
		return true;
#endif

	// Enter the HackMan...  The new sight code only tests LOS to one
	// destination height.  (The old code kept track of angles -- but
	// this approach is not well suited for extrafloors).  The number of
	// points we test depends on the destination: 5 for players, 3 for
	// monsters, 1 for everything else.

	if (dest->player)
	{
		num_div = 5;
		dest_heights[0] = dest->z;
		dest_heights[1] = dest->z + dest->height * 0.25f;
		dest_heights[2] = dest->z + dest->height * 0.50f;
		dest_heights[3] = dest->z + dest->height * 0.75f;
		dest_heights[4] = dest->z + dest->height;
	}
	else if (dest->extendedflags & EF_MONSTER)
	{
		num_div = 3;
		dest_heights[0] = dest->z;
		dest_heights[1] = dest->z + dest->height * 0.5f;
		dest_heights[2] = dest->z + dest->height;
	}
	else
	{
		num_div = 1;
		dest_heights[0] = dest->z + dest->height * 0.5f;
	}

	// use intercepts to check extrafloor heights
	// 
	for (n=0; n < num_div; n++)
	{
		float slope = dest_heights[n] - sight_I.src_z;

		if (slope > sight_I.top_slope || slope < sight_I.bottom_slope)
			continue;

		if (CheckSightIntercepts(slope))
			return true;
	}

	return false;
}

bool P_CheckSightToPoint(mobj_t * src, float x, float y, float z)
{
	subsector_t *dest_sub = R_PointInSubsector(x, y);

	if (dest_sub == src->subsector)
		return true;

	validcount++;

	sight_I.src.x = src->x;
	sight_I.src.y = src->y;
	sight_I.src_z = src->z + src->height * 
		PERCENT_2_FLOAT(src->info->viewheight);
	sight_I.src.dx = x - src->x;
	sight_I.src.dy = y - src->y;
	sight_I.src_sub = src->subsector;

	sight_I.dest.x = x;
	sight_I.dest.y = y;
	sight_I.dest_z = z;
	sight_I.dest_sub = dest_sub;

	sight_I.bottom_slope = z - 1.0f - sight_I.src_z;
	sight_I.top_slope    = z + 1.0f - sight_I.src_z;

	sight_I.angle = R_PointToAngle(sight_I.src.x, sight_I.src.y,
		sight_I.dest.x, sight_I.dest.y);

	sight_I.bbox[BOXLEFT]   = MIN(sight_I.src.x, sight_I.dest.x);
	sight_I.bbox[BOXRIGHT]  = MAX(sight_I.src.x, sight_I.dest.x);
	sight_I.bbox[BOXBOTTOM] = MIN(sight_I.src.y, sight_I.dest.y);
	sight_I.bbox[BOXTOP]    = MAX(sight_I.src.y, sight_I.dest.y);

	wall_icpts.clear();

	sight_I.exfloors = false;

	if (! CheckSightBSP(root_node))
		return false;

#if 1
	if (! sight_I.exfloors)
		return true;
#endif

	float slope = z - sight_I.src_z;

	if (slope > sight_I.top_slope || slope < sight_I.bottom_slope)
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
bool P_CheckSightApproxVert(mobj_t * src, mobj_t * dest)
{
	SYS_ASSERT(src->info);

	sight_I.src_z = src->z + src->height * 
		PERCENT_2_FLOAT(src->info->viewheight);

	return CheckSightSameSubsector(src, dest);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
