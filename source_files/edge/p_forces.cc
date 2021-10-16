//----------------------------------------------------------------------------
//  EDGE Sector Forces (wind / current / points)
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
//  Based on code from PrBoom:
//
//  PrBoom a Doom port merged with LxDoom and LSDLDoom
//  based on BOOM, a modified and improved DOOM engine
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
//  Copyright (C) 1999-2000 by
//  Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze
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

#include <vector>

#include "dm_defs.h"
#include "dm_state.h"
#include "m_random.h"
#include "p_local.h"
#include "r_state.h"
#include "z_zone.h"

#define PUSH_FACTOR  64.0f  // should be 128 ??


std::vector<force_t *> active_forces;

static force_t *tm_force;  // for PIT_PushThing


static void WindCurrentForce(force_t *f, mobj_t *mo)
{
	float z1 = mo->z;
	float z2 = z1 + mo->height;

	sector_t *sec = f->sector;

	// NOTE: assumes that BOOM's [242] linetype was used
	extrafloor_t *ef = sec->bottom_liq ? sec->bottom_liq : sec->bottom_ef;

	float qty = 0.5f;

	if (f->is_wind)
	{
		if (ef && z2 < ef->bottom_h)
			return;

		if (z1 > (ef ? ef->bottom_h : sec->f_h) + 2.0f)
			qty = 1.0f;
	}
	else  // Current
	{
		if (z1 > (ef ? ef->bottom_h : sec->f_h) + 2.0f)
			return;

		if (z2 < (ef ? ef->bottom_h : sec->c_h))
			qty = 1.0f;
	}

	mo->mom.x += qty * f->mag.x;
	mo->mom.y += qty * f->mag.y;
}

static bool PIT_PushThing(mobj_t *mo, void *dataptr)
{
	if (! (mo->hyperflags & HF_PUSHABLE))
		return true;

	if (mo->flags & MF_NOCLIP)
		return true;

	float dx = mo->x - tm_force->point.x;
	float dy = mo->y - tm_force->point.y;

	float d_unit = P_ApproxDistance(dx, dy);
	float dist   = d_unit * 2.0f / tm_force->radius;

	if (dist >= 2.0f)
		return true;

	// don't apply the force through walls
	if (! P_CheckSightToPoint(mo, tm_force->point.x, tm_force->point.y, tm_force->point.z))
		return true;

	float speed;
	
	if (dist >= 1.0f)
		speed = (2.0f - dist);
	else
		speed = 1.0 / MAX(0.05f, dist);

	// the speed factor is squared, giving similar results to BOOM.
	// NOTE: magnitude is negative for PULL mode.
	speed = tm_force->magnitude * speed * speed;

	mo->mom.x += speed * (dx / d_unit);
	mo->mom.y += speed * (dy / d_unit);

	return true;
}

//
// GENERALISED FORCE
//
static void DoForce(force_t * f)
{
	sector_t *sec = f->sector;

    if (sec->props.type & MSF_Push)
	{
		if (f->is_point)
		{
			tm_force = f;

			float x = f->point.x;
			float y = f->point.y;
			float r = f->radius;

			P_BlockThingsIterator(x-r, y-r, x+r, y+r, PIT_PushThing);
		}
		else // wind/current
		{
			touch_node_t *nd;

			for (nd = sec->touch_things; nd; nd = nd->sec_next)
				if (nd->mo->hyperflags & HF_PUSHABLE)
					WindCurrentForce(f, nd->mo);
		}
	}
}


void P_DestroyAllForces(void)
{
	std::vector<force_t *>::iterator FI;

	for (FI = active_forces.begin(); FI != active_forces.end(); FI++)
		delete (*FI);

	active_forces.clear();
}

//
// Allocate and link in the force.
//
static force_t *P_NewForce(void)
{
	force_t *f = new force_t;

	Z_Clear(f, force_t, 1);

	active_forces.push_back(f);
	return f;
}

void P_AddPointForce(sector_t *sec, float length)
{
	// search for the point objects
	for (subsector_t *sub = sec->subsectors; sub; sub = sub->sec_next)
		for (mobj_t *mo = sub->thinglist; mo; mo = mo->snext)
			if (mo->hyperflags & HF_POINT_FORCE)
			{
				force_t *f = P_NewForce();

				f->is_point = true;
				f->point.x = mo->x;
				f->point.y = mo->y;
				f->point.z = mo->z + 28.0f;
				f->radius = length * 2.0f;
				f->magnitude = length * mo->info->speed / PUSH_FACTOR / 24.0f;
				f->sector = sec;
			}
}

void P_AddSectorForce(sector_t *sec, bool is_wind, float x_mag, float y_mag)
{
	force_t *f = P_NewForce();

	f->is_point = false;
	f->is_wind  = is_wind;

	f->mag.x  = x_mag / PUSH_FACTOR;
	f->mag.y  = y_mag / PUSH_FACTOR;
	f->sector = sec;
}

//
// P_RunForces
//
// Executes all force effects for the current tic.
//
void P_RunForces(void)
{
	std::vector<force_t *>::iterator FI;

	for (FI = active_forces.begin(); FI != active_forces.end(); FI++)
	{
		DoForce(*FI);
	}
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
