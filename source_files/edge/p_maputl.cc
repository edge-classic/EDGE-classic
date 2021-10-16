//----------------------------------------------------------------------------
//  EDGE Movement, Collision & Blockmap utility functions
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
// DESCRIPTION:
//   Movement/collision utility functions,
//   as used by function in p_map.c. 
//   BLOCKMAP Iterator functions,
//   and some PIT_* functions to use for iteration.
//   Gap/extrafloor utility functions.
//   Touch Node code.
//
// TODO HERE:
//   + make gap routines I_Error if overflow limit.
//

#include "i_defs.h"

#include <float.h>

#include <vector>
#include <algorithm>

#include "dm_data.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "m_bbox.h"
#include "p_local.h"
#include "p_spec.h"
#include "r_state.h"
#include "z_zone.h"

//
// P_ApproxDistance
//
// Gives an estimation of distance (not exact)
//
float P_ApproxDistance(float dx, float dy)
{
	dx = fabs(dx);
	dy = fabs(dy);

	return (dy > dx) ? dy + dx/2 : dx + dy/2;
}

float P_ApproxDistance(float dx, float dy, float dz)
{
	dx = fabs(dx);
	dy = fabs(dy);
	dz = fabs(dz);

	float dxy = (dy > dx) ? dy + dx/2 : dx + dy/2;

	return (dz > dxy) ? dz + dxy/2 : dxy + dz/2;
}

//
// P_ApproxSlope
//
// Gives an estimation of slope (not exact)
//
// -AJA- 1999/09/11: written.
//
float P_ApproxSlope(float dx, float dy, float dz)
{
	float dist = P_ApproxDistance(dx, dy);

	// kludge to prevent overflow or division by zero.
	if (dist < 1.0f / 32.0f)
		dist = 1.0f / 32.0f;

	return dz / dist;
}

void P_ComputeIntersection(divline_t *div,
		float x1, float y1, float x2, float y2,
		float *ix, float *iy)
{
	if (div->dx == 0)
	{
		*ix = div->x;
		*iy = y1 + (y2 - y1) * (div->x - x1) / (x2 - x1);
	}
	else if (div->dy == 0)
	{
		*iy = div->y;
		*ix = x1 + (x2 - x1) * (div->y - y1) / (y2 - y1);
	}
	else
	{
		// perpendicular distances (unnormalised)
		float p1 = (x1 - div->x) * div->dy - (y1 - div->y) * div->dx;
		float p2 = (x2 - div->x) * div->dy - (y2 - div->y) * div->dx;

		*ix = x1 + (x2 - x1) * p1 / (p1 - p2);
		*iy = y1 + (y2 - y1) * p1 / (p1 - p2);
	}
}

//
// P_PointOnDivlineSide
//
// Tests which side of the line the given point lies on.
// Returns 0 (front/right) or 1 (back/left).  If the point lies
// directly on the line, result is undefined (either 0 or 1).
//
int P_PointOnDivlineSide(float x, float y, divline_t *div)
{
	float dx, dy;
	float left, right;

	if (div->dx == 0)
		return ((x <= div->x) ^ (div->dy > 0)) ? 0 : 1;

	if (div->dy == 0)
		return ((y <= div->y) ^ (div->dx < 0)) ? 0 : 1;

	dx = x - div->x;
	dy = y - div->y;

	// try to quickly decide by looking at sign bits
	if ((div->dy < 0) ^ (div->dx < 0) ^ (dx < 0) ^ (dy < 0))
	{
		// left is negative
		if ((div->dy < 0) ^ (dx < 0))
			return 1;

		return 0;
	}

	left  = dx * div->dy;
	right = dy * div->dx;

	return (right < left) ? 0 : 1;
}

//
// P_PointOnDivlineThick
//
// Tests which side of the line the given point is on.   The thickness
// parameter determines when the point is considered "on" the line.
// Returns 0 (front/right), 1 (back/left), or 2 (on).
//
int P_PointOnDivlineThick(float x, float y, divline_t *div,
						  float div_len, float thickness)
{
	float dx, dy;
	float left, right;

	if (div->dx == 0)
	{
		if (fabs(x - div->x) <= thickness)
			return 2;

		return ((x < div->x) ^ (div->dy > 0)) ? 0 : 1;
	}

	if (div->dy == 0)
	{
		if (fabs(y - div->y) <= thickness)
			return 2;

		return ((y < div->y) ^ (div->dx < 0)) ? 0 : 1;
	}

	dx = x - div->x;
	dy = y - div->y;

	// need divline's length here to compute proper distances
	left  = (dx * div->dy) / div_len;
	right = (dy * div->dx) / div_len;

	if (fabs(left - right) < thickness)
		return 2;

	return (right < left) ? 0 : 1;
}

//
// P_BoxOnLineSide
//
// Considers the line to be infinite
// Returns side 0 or 1, -1 if box crosses the line.
//
int P_BoxOnLineSide(const float * tmbox, line_t * ld)
{
	int p1 = 0;
	int p2 = 0;

	divline_t div;

	div.x = ld->v1->x;
	div.y = ld->v1->y;
	div.dx = ld->dx;
	div.dy = ld->dy;

	switch (ld->slopetype)
	{
	case ST_HORIZONTAL:
		p1 = tmbox[BOXTOP] > ld->v1->y;
		p2 = tmbox[BOXBOTTOM] > ld->v1->y;
		if (ld->dx < 0)
		{
			p1 ^= 1;
			p2 ^= 1;
		}
		break;

	case ST_VERTICAL:
		p1 = tmbox[BOXRIGHT] < ld->v1->x;
		p2 = tmbox[BOXLEFT] < ld->v1->x;
		if (ld->dy < 0)
		{
			p1 ^= 1;
			p2 ^= 1;
		}
		break;

	case ST_POSITIVE:
		p1 = P_PointOnDivlineSide(tmbox[BOXLEFT], tmbox[BOXTOP], &div);
		p2 = P_PointOnDivlineSide(tmbox[BOXRIGHT], tmbox[BOXBOTTOM], &div);
		break;

	case ST_NEGATIVE:
		p1 = P_PointOnDivlineSide(tmbox[BOXRIGHT], tmbox[BOXTOP], &div);
		p2 = P_PointOnDivlineSide(tmbox[BOXLEFT], tmbox[BOXBOTTOM], &div);
		break;
	}

	if (p1 == p2)
		return p1;

	return -1;
}

//
// P_BoxOnDivLineSide
//
// Considers the line to be infinite
// Returns side 0 or 1, -1 if box crosses the line.
//
int P_BoxOnDivLineSide(const float * tmbox, divline_t *div)
{
	int p1 = 0;
	int p2 = 0;

	if (div->dy == 0)
	{
		p1 = tmbox[BOXTOP] > div->y;
		p2 = tmbox[BOXBOTTOM] > div->y;

		if (div->dx < 0)
		{
			p1 ^= 1;
			p2 ^= 1;
		}
	}
	else if (div->dx == 0)
	{
		p1 = tmbox[BOXRIGHT] < div->x;
		p2 = tmbox[BOXLEFT] < div->x;

		if (div->dy < 0)
		{
			p1 ^= 1;
			p2 ^= 1;
		}
	}
	else if (div->dy / div->dx > 0)  // optimise ?
	{
		p1 = P_PointOnDivlineSide(tmbox[BOXLEFT], tmbox[BOXTOP], div);
		p2 = P_PointOnDivlineSide(tmbox[BOXRIGHT], tmbox[BOXBOTTOM], div);
	}
	else
	{
		p1 = P_PointOnDivlineSide(tmbox[BOXRIGHT], tmbox[BOXTOP], div);
		p2 = P_PointOnDivlineSide(tmbox[BOXLEFT], tmbox[BOXBOTTOM], div);
	}

	if (p1 == p2)
		return p1;

	return -1;
}

int P_ThingOnLineSide(const mobj_t *mo, line_t * ld)
{
	float bbox[4];

	bbox[BOXLEFT]   = mo->x - mo->radius;
	bbox[BOXRIGHT]  = mo->x + mo->radius;
	bbox[BOXBOTTOM] = mo->y - mo->radius;
	bbox[BOXTOP]    = mo->y + mo->radius;

	return P_BoxOnLineSide(bbox, ld);
}


//------------------------------------------------------------------------
//
//  GAP UTILITY FUNCTIONS
//

static int GAP_RemoveSolid(vgap_t * dest, int d_num, float z1, float z2)
{
	int d;
	int new_num = 0;
	vgap_t new_gaps[100];

#ifdef DEVELOPERS
	if (z1 > z2)
		I_Error("RemoveSolid: z1 > z2");
#endif

	for (d = 0; d < d_num; d++)
	{
		if (dest[d].c <= dest[d].f)
			continue;  // ignore empty gaps.

		if (z1 <= dest[d].f && z2 >= dest[d].c)
			continue;  // completely blocks it.

		if (z1 >= dest[d].c || z2 <= dest[d].f)
		{
			// no intersection.

			new_gaps[new_num].f = dest[d].f;
			new_gaps[new_num].c = dest[d].c;
			new_num++;
			continue;
		}

		// partial intersections.

		if (z1 > dest[d].f)
		{
			new_gaps[new_num].f = dest[d].f;
			new_gaps[new_num].c = z1;
			new_num++;
		}

		if (z2 < dest[d].c)
		{
			new_gaps[new_num].f = z2;
			new_gaps[new_num].c = dest[d].c;
			new_num++;
		}
	}

	Z_MoveData(dest, new_gaps, vgap_t, new_num);

	return new_num;
}

static int GAP_Construct(vgap_t * gaps, sector_t * sec, mobj_t * thing)
{
	extrafloor_t * ef;

	int num = 1;

	// early out for FUBAR sectors
	if (sec->f_h >= sec->c_h)
		return 0;

	gaps[0].f = sec->f_h;
	gaps[0].c = sec->c_h;

	for (ef = sec->bottom_ef; ef; ef = ef->higher)
	{
		num = GAP_RemoveSolid(gaps, num, ef->bottom_h, ef->top_h);
	}

	// -- handle WATER WALKERS --

	if (!thing || !(thing->extendedflags & EF_WATERWALKER))
		return num;

	for (ef = sec->bottom_liq; ef; ef = ef->higher)
	{
		if (ef->ef_info && (ef->ef_info->type & EXFL_Water))
		{
			num = GAP_RemoveSolid(gaps, num, ef->bottom_h, ef->top_h);
		}
	}

	return num;
}

static int GAP_SightConstruct(vgap_t * gaps, sector_t * sec)
{
	extrafloor_t * ef;

	int num = 1;

	// early out for closed or FUBAR sectors
	if (sec->c_h <= sec->f_h)
		return 0;

	gaps[0].f = sec->f_h;
	gaps[0].c = sec->c_h;

	for (ef = sec->bottom_ef; ef; ef = ef->higher)
	{
		if (!ef->ef_info || !(ef->ef_info->type & EXFL_SeeThrough))
			num = GAP_RemoveSolid(gaps, num, ef->bottom_h, ef->top_h);
	}

	for (ef = sec->bottom_liq; ef; ef = ef->higher)
	{
		if (!ef->ef_info || !(ef->ef_info->type & EXFL_SeeThrough))
			num = GAP_RemoveSolid(gaps, num, ef->bottom_h, ef->top_h);
	}

	return num;
}

static int GAP_Restrict(vgap_t * dest, int d_num, vgap_t * src, int s_num)
{
	int d, s;
	float f, c;

	int new_num = 0;
	vgap_t new_gaps[32];

	for (s = 0; s < s_num; s++)
	{
		// ignore empty gaps.
		if (src[s].c <= src[s].f)
			continue;

		for (d = 0; d < d_num; d++)
		{
			// ignore empty gaps.
			if (dest[d].c <= dest[d].f)
				continue;

			f = MAX(src[s].f, dest[d].f);
			c = MIN(src[s].c, dest[d].c);

			if (f < c)
			{
				new_gaps[new_num].c = c;
				new_gaps[new_num].f = f;
				new_num++;
			}
		}
	}

	Z_MoveData(dest, new_gaps, vgap_t, new_num);

	return new_num;
}

#if 0  // DEBUG ONLY
static void GAP_Dump(vgap_t *gaps, int num)
{
	int j;

	for (j=0; j < num; j++)
		L_WriteDebug("  GAP %d/%d  %1.1f .. %1.1f\n", j+1, num,
		gaps[j].f, gaps[j].c);
}
#endif

//
// P_FindThingGap
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
int P_FindThingGap(vgap_t * gaps, int gap_num, float z1, float z2)
{
	int i;
	float dist;

	int fit_num = 0;
	int fit_last = -1;

	int fit_closest = -1;
	float fit_mindist = FLT_MAX;

	int nofit_closest = -1;
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
		if (z1 >= gaps[i].f && z2 <= gaps[i].c)
		{ // [1]
			return i;
		}

		dist = ABS(z1 - gaps[i].f);

		if (z2 - z1 <= gaps[i].c - gaps[i].f)
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
// P_ComputeThingGap
//
// Determine the initial floorz and ceilingz that a thing placed at a
// particular Z would have.  Returns the nominal Z height.  Some special
// values of Z are recognised: ONFLOORZ & ONCEILINGZ.
//
float P_ComputeThingGap(mobj_t * thing, sector_t * sec, float z,
						float * f, float * c)
{
	vgap_t temp_gaps[100];
	int temp_num;

	temp_num = GAP_Construct(temp_gaps, sec, thing);

	if (z == ONFLOORZ)
		z = sec->f_h;

	if (z == ONCEILINGZ)
		z = sec->c_h - thing->height;

	temp_num = P_FindThingGap(temp_gaps, temp_num, z, z + thing->height);

	if (temp_num < 0)
	{
		// thing is stuck in a closed door.
		*f = *c = sec->f_h;
		return *f;
	}

	*f = temp_gaps[temp_num].f;
	*c = temp_gaps[temp_num].c;

	return z;
}

//
// P_ComputeGaps
//
// Determine the gaps between the front & back sectors of the line,
// taking into account any extra floors.
//
// -AJA- 1999/07/19: This replaces P_LineOpening.
//
void P_ComputeGaps(line_t * ld)
{
	sector_t *front = ld->frontsector;
	sector_t *back  = ld->backsector;

	int temp_num;
	vgap_t temp_gaps[100];

	ld->blocked = true;
	ld->gap_num = 0;

	if (!front || !back)
	{
		// single sided line
		return;
	}

	// NOTE: this check is rather lax.  It mirrors the check in original
	// Doom r_bsp.c, in order for transparent doors to work properly.
	// In particular, the blocked flag can be clear even when one of the
	// sectors is closed (has ceiling <= floor).

	if (back->c_h <= front->f_h || front->c_h <= back->f_h)
	{
		// closed door.

		// -AJA- MUNDO HACK for slopes!!!!
		if (front->f_slope || back->f_slope ||
		    front->c_slope || back->c_slope)
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
		slider_move_t *smov = ld->slider_move;

		if (! smov)
			return;

		// these semantics copied from XDoom
		if (smov->direction > 0 && smov->opening < smov->target * 0.5f)
			return;

		if (smov->direction < 0 && smov->opening < smov->target * 0.75f)
			return;
	}

	// handle normal gaps ("movement" gaps)

	ld->gap_num = GAP_Construct(ld->gaps, front, NULL);
	temp_num = GAP_Construct(temp_gaps, back, NULL);

	ld->gap_num = GAP_Restrict(ld->gaps, ld->gap_num, temp_gaps, temp_num);
}

//
// P_DumpExtraFloors
//
#ifdef DEVELOPERS
void P_DumpExtraFloors(const sector_t *sec)
{
	const extrafloor_t *ef;

	L_WriteDebug("EXTRAFLOORS IN Sector %d  (%d used, %d max)\n",
		(int)(sec - sectors), sec->exfloor_used, sec->exfloor_max);

	L_WriteDebug("  Basic height: %1.1f .. %1.1f\n", sec->f_h, sec->c_h);

	for (ef = sec->bottom_ef; ef; ef = ef->higher)
	{
		L_WriteDebug("  Solid %s: %1.1f .. %1.1f\n",
			(ef->ef_info->type & EXFL_Thick) ? "Thick" : "Thin",
			ef->bottom_h, ef->top_h);
	}

	for (ef = sec->bottom_liq; ef; ef = ef->higher)
	{
		L_WriteDebug("  Liquid %s: %1.1f .. %1.1f\n",
			(ef->ef_info->type & EXFL_Thick) ? "Thick" : "Thin",
			ef->bottom_h, ef->top_h);
	}
}
#endif

//
// P_ExtraFloorFits
//
// Check if a solid extrafloor fits.
//
exfloor_fit_e P_ExtraFloorFits(sector_t *sec, float z1, float z2)
{
	extrafloor_t *ef;

	if (z2 > sec->c_h)
		return EXFIT_StuckInCeiling;

	if (z1 < sec->f_h)
		return EXFIT_StuckInFloor;

	for (ef=sec->bottom_ef; ef && ef->higher; ef=ef->higher)
	{
		float bottom = ef->bottom_h;
		float top = ef->top_h;

		SYS_ASSERT(top >= bottom);

		// here is another solid extrafloor, check for overlap
		if (z2 > bottom && z1 < top)
			return EXFIT_StuckInExtraFloor;
	}

	return EXFIT_Ok;
}


void P_AddExtraFloor(sector_t *sec, line_t *line)
{
	sector_t *ctrl = line->frontsector;
	const extrafloordef_c *ef_info;

	surface_t *top, *bottom;
	extrafloor_t *newbie, *cur;

	bool liquid;
	exfloor_fit_e errcode;

	SYS_ASSERT(line->special);
	SYS_ASSERT(line->special->ef.type & EXFL_Present);

	ef_info = &line->special->ef;

	//
	// -- create new extrafloor --
	// 

	SYS_ASSERT(sec->exfloor_used <= sec->exfloor_max);

	if (sec->exfloor_used == sec->exfloor_max)
		I_Error("INTERNAL ERROR: extrafloor overflow in sector %d\n",
		(int)(sec - sectors));

	newbie = sec->exfloor_first + sec->exfloor_used;
	sec->exfloor_used++;

	Z_Clear(newbie, extrafloor_t, 1);

	bottom = &ctrl->floor;
	top = (ef_info->type & EXFL_Thick) ? &ctrl->ceil : bottom;

	// Handle the BOOMTEX flag (Boom compatibility)
	if (ef_info->type & EXFL_BoomTex)
	{
		bottom = &ctrl->ceil;
		top = &sec->floor;
	}

	newbie->bottom_h = ctrl->f_h;
	newbie->top_h = (ef_info->type & EXFL_Thick) ? ctrl->c_h : newbie->bottom_h;

	if (newbie->top_h < newbie->bottom_h)
		I_Error("Bad Extrafloor in sector #%d: "
		"z range is %1.0f / %1.0f\n", (int)(sec - sectors),
		newbie->bottom_h, newbie->top_h);

	newbie->sector = sec;
	newbie->top = top;
	newbie->bottom = bottom;

	newbie->p = &ctrl->props;
	newbie->ef_info = ef_info;
	newbie->ef_line = line;

	// Insert into the dummy's linked list
	newbie->ctrl_next = ctrl->control_floors;
	ctrl->control_floors = newbie;

	//
	// -- handle liquid extrafloors --
	//

	liquid = (ef_info->type & EXFL_Liquid)?true:false;

	if (liquid)
	{
		// find place to link into.  cur will be the next higher liquid,
		// or NULL if this is the highest.

		for (cur=sec->bottom_liq; cur; cur=cur->higher)
		{
			if (cur->bottom_h > newbie->bottom_h)
				break;
		}

		newbie->higher = cur;
		newbie->lower = cur ? cur->lower : sec->top_liq;

		if (newbie->higher)
			newbie->higher->lower = newbie;
		else
			sec->top_liq = newbie;

		if (newbie->lower)
			newbie->lower->higher = newbie;
		else
			sec->bottom_liq = newbie;

		return;
	}

	//
	// -- handle solid extrafloors --
	//

	// check if fits
	errcode = P_ExtraFloorFits(sec, newbie->bottom_h, newbie->top_h);

	switch (errcode)
	{
		case EXFIT_Ok:
			break;

		case EXFIT_StuckInCeiling:
			I_Error("Extrafloor with z range of %1.0f / %1.0f is stuck "
				"in sector #%d's ceiling.\n",
				newbie->bottom_h, newbie->top_h, (int)(sec - sectors));

		case EXFIT_StuckInFloor:
			I_Error("Extrafloor with z range of %1.0f / %1.0f is stuck "
				"in sector #%d's floor.\n",
				newbie->bottom_h, newbie->top_h, (int)(sec - sectors));

		default:
			I_Error("Extrafloor with z range of %1.0f / %1.0f is stuck "
				"in sector #%d in another extrafloor.\n",
				newbie->bottom_h, newbie->top_h, (int)(sec - sectors));
	}

	// find place to link into.  cur will be the next higher extrafloor,
	// or NULL if this is the highest.

	for (cur=sec->bottom_ef; cur; cur=cur->higher)
	{
		if (cur->bottom_h > newbie->bottom_h)
			break;
	}

	newbie->higher = cur;
	newbie->lower = cur ? cur->lower : sec->top_ef;

	if (newbie->higher)
		newbie->higher->lower = newbie;
	else
		sec->top_ef = newbie;

	if (newbie->lower)
		newbie->lower->higher = newbie;
	else
		sec->bottom_ef = newbie;
}


void P_FloodExtraFloors(sector_t *sector)
{
	extrafloor_t *S, *L, *C;

	region_properties_t *props;
	region_properties_t *flood_p = NULL, *last_p = NULL;

	sector->p = &sector->props;

	// traverse downwards, stagger both lists
	S = sector->top_ef;
	L = sector->top_liq;

	while (S || L)
	{
		if (!L || (S && S->bottom_h > L->bottom_h))
		{
			C = S;  S = S->lower;
		}
		else
		{
			C = L;  L = L->lower;
		}

		SYS_ASSERT(C);

		props = &C->ef_line->frontsector->props;

		if (C->ef_info->type & EXFL_Flooder)
		{
			C->p = last_p = flood_p = props;

			if ((C->ef_info->type & EXFL_Liquid) && C->bottom_h >= sector->c_h)
				sector->p = flood_p;

			continue;
		}

		if (C->ef_info->type & EXFL_NoShade)
		{
			if (! last_p)
				last_p = props;

			C->p = last_p;
			continue;
		}

		C->p = last_p = flood_p ? flood_p : props;
	}
}


void P_RecomputeGapsAroundSector(sector_t *sec)
{
	int i;

	for (i=0; i < sec->linecount; i++)
	{
		P_ComputeGaps(sec->lines[i]);
	}

	// now do the sight gaps...

	if (sec->c_h <= sec->f_h)
	{
		sec->sight_gap_num = 0;
		return;
	}

	sec->sight_gap_num = GAP_SightConstruct(sec->sight_gaps, sec);
}


static inline bool PST_CheckBBox(float *bspcoord, float *test)
{
	return (test[BOXRIGHT]  < bspcoord[BOXLEFT] ||
			test[BOXLEFT]   > bspcoord[BOXRIGHT] ||
			test[BOXTOP]    < bspcoord[BOXBOTTOM] ||
			test[BOXBOTTOM] > bspcoord[BOXTOP]) ? false : true;
}

static bool TraverseSubsec(unsigned int bspnum, float *bbox,
								bool (*func)(mobj_t *mo))
{
	subsector_t *sub;
	node_t *node;
	mobj_t *obj;

	// just a normal node ?
	if (! (bspnum & NF_V5_SUBSECTOR))
	{
		node = nodes + bspnum;

		// recursively check the children nodes
		// OPTIMISE: check against partition lines instead of bboxes.

		if (PST_CheckBBox(node->bbox[0], bbox))
		{
			if (! TraverseSubsec(node->children[0], bbox, func))
				return false;
		}

		if (PST_CheckBBox(node->bbox[1], bbox))
		{
			if (! TraverseSubsec(node->children[1], bbox, func))
				return false;
		}

		return true;
	}

	// the sharp end: check all things in the subsector

	sub = subsectors + (bspnum & ~NF_V5_SUBSECTOR);

	for (obj=sub->thinglist; obj; obj=obj->snext)
	{
		if (! (* func)(obj))
			return false;
	}

	return true;
}

//
// P_SubsecThingIterator
//
// Iterate over all things that touch a certain rectangle on the map,
// using the BSP tree.
//
// If any function returns false, then this routine returns false and
// nothing else is checked.  Otherwise true is returned.
//
bool P_SubsecThingIterator(float *bbox,
								bool (*func)(mobj_t *mo))
{
	return TraverseSubsec(root_node, bbox, func);
}

static float checkempty_bbox[4];
static line_t *checkempty_line;

static bool PST_CheckThingArea(mobj_t *mo)
{
	if (mo->x + mo->radius < checkempty_bbox[BOXLEFT] ||
		mo->x - mo->radius > checkempty_bbox[BOXRIGHT] ||
		mo->y + mo->radius < checkempty_bbox[BOXBOTTOM] ||
		mo->y - mo->radius > checkempty_bbox[BOXTOP])
	{
		// keep looking
		return true;
	}

	// ignore corpses and pickup items
	if (! (mo->flags & MF_SOLID) && (mo->flags & MF_CORPSE))
		return true;

	if (mo->flags & MF_SPECIAL)
		return true;

	// we've found a thing in that area: we can stop now
	return false;
}

static bool PST_CheckThingLine(mobj_t *mo)
{
	float bbox[4];
	int side;

	bbox[BOXLEFT]   = mo->x - mo->radius;
	bbox[BOXRIGHT]  = mo->x + mo->radius;
	bbox[BOXBOTTOM] = mo->y - mo->radius;
	bbox[BOXTOP]    = mo->y + mo->radius;

	// found a thing on the line ?
	side = P_BoxOnLineSide(bbox, checkempty_line);

	if (side != -1)
		return true;

	// ignore corpses and pickup items
	if (! (mo->flags & MF_SOLID) && (mo->flags & MF_CORPSE))
		return true;

	if (mo->flags & MF_SPECIAL)
		return true;

	return false;
}

//
// P_ThingsInArea
//
// Checks if there are any things contained within the given rectangle
// on the 2D map.
//
bool P_ThingsInArea(float *bbox)
{
	checkempty_bbox[BOXLEFT]   = bbox[BOXLEFT];
	checkempty_bbox[BOXRIGHT]  = bbox[BOXRIGHT];
	checkempty_bbox[BOXBOTTOM] = bbox[BOXBOTTOM];
	checkempty_bbox[BOXTOP]    = bbox[BOXTOP];

	return ! P_SubsecThingIterator(bbox, PST_CheckThingArea);
}

//
// P_ThingsOnLine
//
// Checks if there are any things touching the given line.
//
bool P_ThingsOnLine(line_t *ld)
{
	checkempty_line = ld;

	return ! P_SubsecThingIterator(ld->bbox, PST_CheckThingLine);
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
