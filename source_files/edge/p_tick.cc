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

#include "p_tick.h"

#include "AlmostEquals.h"
#include "dm_state.h"
#include "g_game.h"
#include "n_network.h"
#include "p_local.h"
#include "p_spec.h"
#include "rad_trig.h"

int level_time_elapsed;

bool fast_forward_active;

bool erraticism_active = false;

extern ConsoleVariable erraticism;
extern ConsoleVariable double_framerate;

//
// MapObjectTicker
//
void MapObjectTicker(bool extra_tic)
{
    if (paused) return;

    // pause if in menu and at least one tic has been run
    if (!network_game && (menu_active || rts_menu_active) &&
        !AlmostEquals(players[console_player]->view_z_, kFloatUnused))
    {
        return;
    }

    erraticism_active = false;

    if (erraticism.d_)
    {
        bool keep_thinking = PlayerThink(players[console_player], extra_tic);

        if (!keep_thinking)
        {
            erraticism_active = true;
            return;
        }

        for (int pnum = 0; pnum < kMaximumPlayers; pnum++)
        {
            if (players[pnum] && players[pnum] != players[console_player])
                PlayerThink(players[pnum], extra_tic);
        }
    }
    else
    {
        for (int pnum = 0; pnum < kMaximumPlayers; pnum++)
            if (players[pnum]) PlayerThink(players[pnum], extra_tic);
    }

    if (!extra_tic || !double_framerate.d_) RAD_RunTriggers();

    RunForces(extra_tic);
    RunMapObjectThinkers(extra_tic);

    if (!extra_tic || !double_framerate.d_) RunLights();

    RunActivePlanes();
    RunActiveSliders();

    if (!extra_tic || !double_framerate.d_) RunAmbientSounds();

    UpdateSpecials(extra_tic);

    if (extra_tic && double_framerate.d_) return;

    ItemRespawn();

    // for par times
    level_time_elapsed++;

    if (level_time_elapsed >= exit_time && game_action == kGameActionNothing)
    {
        game_action = kGameActionIntermission;
    }
}

void HubFastForward(void)
{
    fast_forward_active = true;

    // close doors
    for (int i = 0; i < kTicRate * 8; i++)
    {
        RunActivePlanes();
        RunActiveSliders();
    }

    for (int k = 0; k < kTicRate / 3; k++) MapObjectTicker(false);

    fast_forward_active = false;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
