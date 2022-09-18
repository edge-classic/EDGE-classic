//----------------------------------------------------------------------------
//  EDGE Networking (New)
//----------------------------------------------------------------------------
// 
//  Copyright (c) 2004-2009  The EDGE Team.
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

#include "i_defs.h"
#include "i_net.h"

#include <limits.h>
#include <stdlib.h>

#include "types.h"
#include "endianess.h"

#include "n_bcast.h"
#include "n_reliable.h"
#include "n_network.h"

#include "dm_state.h"
#include "e_input.h"
#include "e_main.h"
#include "g_game.h"
#include "e_player.h"
#include "m_argv.h"
#include "m_random.h"

#include "coal.h" // for coal::vm_c

extern coal::vm_c *ui_vm;

extern void VM_SetFloat(coal::vm_c *vm, const char *mod_name, const char *var_name, double value);

// #define DEBUG_TICS 1

// only true if packets are exchanged with a server
bool netgame = false;

int base_port;

DEF_CVAR(m_busywait, "1", CVAR_ARCHIVE)


// gametic is the tic about to (or currently being) run.
// maketic is the tic that hasn't had control made for it yet.
//
// NOTE: it is a system-wide INVARIANT that gametic <= maketic, since we
// cannot run a step of the physics without a ticcmd for all players).

int gametic;
int maketic;

static int last_update_tic;  // last time N_NetUpdate  was called
static int last_tryrun_tic;  // last time N_TryRunTics was called


//----------------------------------------------------------------------------
//  TIC HANDLING
//----------------------------------------------------------------------------

void N_InitNetwork(void)
{
	srand(I_PureRandom());

	N_ResetTics();

	if (nonet)
		return;

	base_port = MP_EDGE_PORT;

	const char *str = M_GetParm("-port");
	if (str)
		base_port = atoi(str);

	I_Printf("Network: base port is %d\n", base_port);

//??	N_StartupReliableLink (base_port+0);
//??	N_StartupBroadcastLink(base_port+1);
}


static void DoDelay()
{
	if (m_busywait.d)
		return;

	// -AJA- This can make everything a bit "jerky" :-(
	I_Sleep(5 /* millis */);
}


static void ReceivePackets()
{
	// TODO receive stuff from other computers
}


static void TransmitStuff(int tic)
{
	// TODO send stuff to other computers
}


static bool N_BuildTiccmds(void)
{
	// process input and create player (and robot) ticcmds.
	// returns false if couldn't hold any more.
	// this is the only allowed to increase `maketics`.

	I_ControlGetEvents();
	E_ProcessEvents();

	if (numplayers == 0)
	{
		E_UpdateKeyState();
		return false;
	}

	if (maketic >= gametic + BACKUPTICS)
	{
		// players cannot hold any more ticcmds!

		E_UpdateKeyState();
		return false;
	}

	// build ticcmds
	for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
	{
		player_t *p = players[pnum];
		if (! p) continue;
		if (! p->builder) continue;

		int buf = maketic % BACKUPTICS;

		p->builder(p, p->build_data, &p->in_cmds[buf]);
	}

	E_UpdateKeyState();

	TransmitStuff(maketic);

	maketic++;
	return true;
}


int N_NetUpdate()
{
	// if enough time has elapsed, process input events and build one
	// or more ticcmds for the local players.

	int nowtime = I_GetTime();

	// singletic update is syncronous
	if (singletics)
		return nowtime;

	int newtics = nowtime - last_update_tic;
	last_update_tic = nowtime;

	if (newtics > 0)
	{
		// build and send new ticcmds for local players.
		// this will advance `maketic`, unless buffers are full.
		int t;
		for (t = 0; t < newtics; t++)
		{
			if (! N_BuildTiccmds())
				break;
		}

		if (t != newtics && numplayers > 0)
			I_Debugf("N_NetUpdate: lost tics: %d\n", newtics - t);
	}

	ReceivePackets();

	return nowtime;
}


int N_TryRunTics()
{
	if (singletics)
	{
		N_BuildTiccmds();
		return 1;
	}

	int nowtime = N_NetUpdate();
	int realtics = nowtime - last_tryrun_tic;
	last_tryrun_tic = nowtime;

#ifdef DEBUG_TICS
	I_Debugf("N_TryRunTics: now %d last_tryrun %d --> real %d\n",
		nowtime, nowtime - realtics, realtics);
#endif

	// we require at least ONE real tic
	while (realtics <= 0)
	{
		DoDelay();
		nowtime = N_NetUpdate();
		realtics = nowtime - last_tryrun_tic;
		last_tryrun_tic = nowtime;
	}

	// we can never generate more ticcmds that this
	if (realtics > BACKUPTICS-1)
		realtics = BACKUPTICS-1;

	// simpler handling when no game in progress
	if (numplayers == 0)
	{
		return realtics;
	}

	SYS_ASSERT(gametic <= maketic);

	// if there is not enough ticcmds to run, build them now.
	// we don't need to process events here -- this is very quick.
	while (maketic < gametic + realtics)
	{
		N_BuildTiccmds();
	}

	return realtics;
}


void N_ResetTics(void)
{
	maketic = gametic = 0;

	last_update_tic = last_tryrun_tic = I_GetTime();
}


void N_QuitNetGame(void)
{
	// TODO send a quit message to all peers

	// wait a bit
	if (false)  // have_peers
		I_Sleep(250);

	// TODO close open sockets
}


void N_TiccmdTicker(void)
{
	// this is called from G_Ticker, and is the only place allowed to
	// advance `gametic` (allowing the game simulation to advance).

	// all we do here is grab the ticcmd for the active players
	// (from ones built earler in N_BuildTiccmds).

	// gametic <= maketic is a system-wide invariant.  However, new levels
	// levels are loaded during G_Ticker(), which resets them both to zero,
	// hence we need to handle that particular case here.
	SYS_ASSERT(gametic <= maketic);

	if (gametic == maketic)
		return;

	int buf = gametic % BACKUPTICS;

	for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
	{
		player_t *p = players[pnum];
		if (! p) continue;

		memcpy(&p->cmd, p->in_cmds + buf, sizeof(ticcmd_t));
	}

	VM_SetFloat(ui_vm, "sys", "gametic", gametic);

	gametic++;
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
