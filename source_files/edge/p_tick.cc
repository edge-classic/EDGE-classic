//----------------------------------------------------------------------------
//  EDGE Thinker & Ticker Code
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

int leveltime;

bool fast_forward_active;

//
// P_Ticker
//
void P_Ticker(void)
{
	if (paused)
		return;

	// pause if in menu and at least one tic has been run
	if (!netgame && (menuactive || rts_menuactive) &&
		players[consoleplayer]->viewz != FLO_UNUSED)
	{
		return;
	}

	for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
		if (players[pnum])
			P_PlayerThink(players[pnum]);

	RAD_RunTriggers();

	P_RunForces();
	P_RunMobjThinkers();
	P_RunLights();
	P_RunActivePlanes();
	P_RunActiveSliders();
	P_RunAmbientSFX();

	P_UpdateSpecials();
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
		P_Ticker();

	fast_forward_active = false;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
