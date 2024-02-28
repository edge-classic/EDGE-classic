//----------------------------------------------------------------------------
//  EDGE Sector Forces (wind / current / points)
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



#include <vector>

#include "dm_defs.h"
#include "dm_state.h"
#include "m_random.h"
#include "p_local.h"
#include "r_state.h"

#define PUSH_FACTOR 64.0f // should be 128 ??

extern ConsoleVariable double_framerate;

std::vector<force_t *> active_forces;

static force_t *tm_force; // for PIT_PushThing

static void WindCurrentForce(force_t *f, MapObject *mo)
{
    float z1 = mo->z;
    float z2 = z1 + mo->height_;

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
    else // Current
    {
        if (z1 > (ef ? ef->bottom_h : sec->f_h) + 2.0f)
            return;

        if (z2 < (ef ? ef->bottom_h : sec->c_h))
            qty = 1.0f;
    }

    mo->momentum_.X += qty * f->mag.X;
    mo->momentum_.Y += qty * f->mag.Y;
}

static bool PIT_PushThing(MapObject *mo, void *dataptr)
{
    if (!(mo->hyper_flags_ & kHyperFlagPushable))
        return true;

    if (mo->flags_ & kMapObjectFlagNoClip)
        return true;

    float dx = mo->x - tm_force->point.X;
    float dy = mo->y - tm_force->point.Y;

    float d_unit = P_ApproxDistance(dx, dy);
    float dist   = d_unit * 2.0f / tm_force->radius;

    if (dist >= 2.0f)
        return true;

    // don't apply the force through walls
    if (!P_CheckSightToPoint(mo, tm_force->point.X, tm_force->point.Y, tm_force->point.Z))
        return true;

    float speed;

    if (dist >= 1.0f)
        speed = (2.0f - dist);
    else
        speed = 1.0 / HMM_MAX(0.05f, dist);

    // the speed factor is squared, giving similar results to BOOM.
    // NOTE: magnitude is negative for PULL mode.
    speed = tm_force->magnitude * speed * speed;

    mo->momentum_.X += speed * (dx / d_unit);
    mo->momentum_.Y += speed * (dy / d_unit);

    return true;
}

//
// GENERALISED FORCE
//
static void DoForce(force_t *f)
{
    sector_t *sec = f->sector;

    if (sec->props.type & MSF_Push)
    {
        if (f->is_point)
        {
            tm_force = f;

            float x = f->point.X;
            float y = f->point.Y;
            float r = f->radius;

            BlockmapThingIterator(x - r, y - r, x + r, y + r, PIT_PushThing);
        }
        else // wind/current
        {
            touch_node_t *nd;

            for (nd = sec->touch_things; nd; nd = nd->sec_next)
                if (nd->mo->hyper_flags_ & kHyperFlagPushable)
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
        for (MapObject *mo = sub->thinglist; mo; mo = mo->subsector_next_)
            if (mo->hyper_flags_ & kHyperFlagPointForce)
            {
                force_t *f = P_NewForce();

                f->is_point  = true;
                f->point.X   = mo->x;
                f->point.Y   = mo->y;
                f->point.Z   = mo->z + 28.0f;
                f->radius    = length * 2.0f;
                f->magnitude = length * mo->info_->speed_ / PUSH_FACTOR / 24.0f;
                f->sector    = sec;
            }
}

void P_AddSectorForce(sector_t *sec, bool is_wind, float x_mag, float y_mag)
{
    force_t *f = P_NewForce();

    f->is_point = false;
    f->is_wind  = is_wind;

    f->mag.X  = x_mag / PUSH_FACTOR;
    f->mag.Y  = y_mag / PUSH_FACTOR;
    f->sector = sec;
}

//
// P_RunForces
//
// Executes all force effects for the current tic.
//
void P_RunForces(bool extra_tic)
{
    // TODO: review what needs updating here for 70 Hz
    if (extra_tic && double_framerate.d_)
        return;

    std::vector<force_t *>::iterator FI;

    for (FI = active_forces.begin(); FI != active_forces.end(); FI++)
    {
        DoForce(*FI);
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
