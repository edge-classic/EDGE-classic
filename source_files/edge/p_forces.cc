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
#include "epi.h"
#include "epi_doomdefs.h"
#include "m_random.h"
#include "p_local.h"
#include "r_state.h"

constexpr float kPushFactor = 64.0f; // should be 128 ?? (why? - Dasho)

std::vector<Force *> active_forces;

static Force *current_force; // for PushThingCallback

static void WindCurrentForce(Force *f, MapObject *mo)
{
    float z1 = mo->z;
    float z2 = z1 + mo->height_;

    Sector *sec = f->sector;

    // NOTE: assumes that BOOM's [242] linetype was used
    Extrafloor *ef = sec->bottom_liquid ? sec->bottom_liquid : sec->bottom_extrafloor;

    float qty = 0.5f;

    if (f->is_wind)
    {
        if (ef && z2 < ef->bottom_height)
            return;

        if (z1 > (ef ? ef->bottom_height : sec->floor_height) + 2.0f)
            qty = 1.0f;
    }
    else // Current
    {
        if (z1 > (ef ? ef->bottom_height : sec->floor_height) + 2.0f)
            return;

        if (z2 < (ef ? ef->bottom_height : sec->ceiling_height))
            qty = 1.0f;
    }

    mo->momentum_.X += qty * f->direction.X;
    mo->momentum_.Y += qty * f->direction.Y;
}

static bool PushThingCallback(MapObject *mo, void *dataptr)
{
    EPI_UNUSED(dataptr);
    if (!(mo->hyper_flags_ & kHyperFlagPushable))
        return true;

    if (mo->flags_ & kMapObjectFlagNoClip)
        return true;

    float dx = mo->x - current_force->point.X;
    float dy = mo->y - current_force->point.Y;

    float d_unit = ApproximateDistance(dx, dy);
    float dist   = d_unit * 2.0f / current_force->radius;

    if (dist >= 2.0f)
        return true;

    // don't apply the force through walls
    if (!CheckSightToPoint(mo, current_force->point.X, current_force->point.Y, current_force->point.Z))
        return true;

    float speed;

    if (dist >= 1.0f)
        speed = (2.0f - dist);
    else
        speed = 1.0 / HMM_MAX(0.05f, dist);

    // the speed factor is squared, giving similar results to BOOM.
    // NOTE: magnitude is negative for PULL mode.
    speed = current_force->magnitude * speed * speed;

    mo->momentum_.X += speed * (dx / d_unit);
    mo->momentum_.Y += speed * (dy / d_unit);

    return true;
}

//
// GENERALISED FORCE
//
static void DoForce(Force *f)
{
    Sector *sec = f->sector;

    if (sec->properties.type & kBoomSectorFlagPush)
    {
        if (f->is_point)
        {
            current_force = f;

            float x = f->point.X;
            float y = f->point.Y;
            float r = f->radius;

            BlockmapThingIterator(x - r, y - r, x + r, y + r, PushThingCallback);
        }
        else // wind/current
        {
            TouchNode *nd;

            for (nd = sec->touch_things; nd; nd = nd->sector_next)
                if (nd->map_object->hyper_flags_ & kHyperFlagPushable)
                    WindCurrentForce(f, nd->map_object);
        }
    }
}

void DestroyAllForces(void)
{
    std::vector<Force *>::iterator FI;

    for (FI = active_forces.begin(); FI != active_forces.end(); FI++)
        delete (*FI);

    active_forces.clear();
}

//
// Allocate and link in the force.
//
static Force *NewForce(void)
{
    Force *f = new Force;

    EPI_CLEAR_MEMORY(f, Force, 1);

    active_forces.push_back(f);
    return f;
}

void AddPointForce(Sector *sec, float length)
{
    // search for the point objects
    for (Subsector *sub = sec->subsectors; sub; sub = sub->sector_next)
        for (MapObject *mo = sub->thing_list; mo; mo = mo->subsector_next_)
            if (mo->hyper_flags_ & kHyperFlagPointForce)
            {
                Force *f = NewForce();

                f->is_point  = true;
                f->point.X   = mo->x;
                f->point.Y   = mo->y;
                f->point.Z   = mo->z + 28.0f;
                f->radius    = length * 2.0f;
                f->magnitude = length * mo->info_->speed_ / kPushFactor / 24.0f;
                f->sector    = sec;
            }
}

void AddSectorForce(Sector *sec, bool is_wind, float x_mag, float y_mag)
{
    Force *f = NewForce();

    f->is_point = false;
    f->is_wind  = is_wind;

    f->direction.X = x_mag / kPushFactor;
    f->direction.Y = y_mag / kPushFactor;
    f->sector      = sec;
}

//
// RunForces
//
// Executes all force effects for the current tic.
//
void RunForces()
{
    std::vector<Force *>::iterator FI;

    for (FI = active_forces.begin(); FI != active_forces.end(); FI++)
    {
        DoForce(*FI);
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
