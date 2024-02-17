//----------------------------------------------------------------------------
//  EDGE Thinker & Ticker Code
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
// -ACB- 1998/09/14 Removed P_AllocateThinker: UNUSED
//
// -AJA- 1999/11/06: Changed from ring (thinkercap) to list.
//
// -ES- 2000/02/14: Removed thinker system.

#include "i_defs.h"
#include "p_tick.h"

#include "dm_state.h"
#include "g_game.h"
#include "n_network.h"
#include "p_local.h"
#include "p_spec.h"
#include "rad_trig.h"

#include "AlmostEquals.h"

int leveltime;

bool fast_forward_active;

bool erraticism_active = false;

extern cvar_c g_erraticism;
extern cvar_c r_doubleframes;

//
// P_Ticker
//
void P_Ticker(bool extra_tic)
{
    if (paused)
        return;

    // pause if in menu and at least one tic has been run
    if (!netgame && (menuactive || rts_menuactive) && !AlmostEquals(players[consoleplayer]->viewz, kFloatUnused))
    {
        return;
    }

    erraticism_active = false;

    if (g_erraticism.d)
    {
        bool keep_thinking = P_PlayerThink(players[consoleplayer], extra_tic);

        if (!keep_thinking)
        {
            erraticism_active = true;
            return;
        }

        for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
        {
            if (players[pnum] && players[pnum] != players[consoleplayer])
                P_PlayerThink(players[pnum], extra_tic);
        }
    }
    else
    {
        for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
            if (players[pnum])
                P_PlayerThink(players[pnum], extra_tic);
    }

    if (!extra_tic || !r_doubleframes.d)
        RAD_RunTriggers();

    P_RunForces(extra_tic);
    P_RunMobjThinkers(extra_tic);

    if (!extra_tic || !r_doubleframes.d)
        P_RunLights();

    P_RunActivePlanes();
    P_RunActiveSliders();

    if (!extra_tic || !r_doubleframes.d)
        P_RunAmbientSFX();

    P_UpdateSpecials(extra_tic);

    if (extra_tic && r_doubleframes.d)
        return;

    P_MobjItemRespawn();

    // for par times
    leveltime++;

    if (leveltime >= exittime && gameaction == ga_nothing)
    {
        gameaction = ga_intermission;
    }
}

void P_HubFastForward(void)
{
    fast_forward_active = true;

    // close doors
    for (int i = 0; i < TICRATE * 8; i++)
    {
        P_RunActivePlanes();
        P_RunActiveSliders();
    }

    for (int k = 0; k < TICRATE / 3; k++)
        P_Ticker(false);

    fast_forward_active = false;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
