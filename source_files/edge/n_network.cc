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
// it is an invariant that gametic <= maketic (since we cannot run
// a step of the physics without a ticcmd for all player).

int gametic;
int maketic;

static int last_update_tic;
static int last_tryrun_tic;

extern gameflags_t default_gameflags;


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

///!!!	N_StartupReliableLink (base_port+0);
///!!!	N_StartupBroadcastLink(base_port+1);
}


// TEMP CRAP
bool N_OpenBroadcastSocket(bool is_host)
{ return true; }
void N_CloseBroadcastSocket(void)
{ }
void N_SendBroadcastDiscovery(void)
{ }


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


// process input and create player (and robot) ticcmds.
// returns false if couldn't hold any more.
static bool N_BuildTiccmds(void)
{
	I_ControlGetEvents();
	E_ProcessEvents();

	if (numplayers == 0)
	{
		E_UpdateKeyState();
		return false;
	}

	if (maketic >= gametic + BACKUPTICS)
	{
		// players cannot hold any more ticcmds

		E_UpdateKeyState();
		return false;
	}

	// build ticcmds
	for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
	{
		player_t *p = players[pnum];
		if (! p) continue;
		if (! p->builder) continue;

///     L_WriteDebug("N_BuildTiccmds: pnum %d netgame %c\n", pnum, netgame ? 'Y' : 'n');

		ticcmd_t *cmd = &p->in_cmds[maketic % BACKUPTICS];

		p->builder(p, p->build_data, cmd);
	}

	E_UpdateKeyState();

	TransmitStuff(maketic);

	maketic++;
	return true;
}

int N_NetUpdate()
{
	int nowtime = I_GetTime();

	if (singletics)  // singletic update is syncronous
		return nowtime;

	int newtics = nowtime - last_update_tic;
	last_update_tic = nowtime;

	if (newtics > 0)
	{
		// build and send new ticcmds for local players
		int t;
		for (t = 0; t < newtics; t++)
		{
			if (! N_BuildTiccmds())
				break;
		}

		if (t != newtics && numplayers > 0)
			L_WriteDebug("N_NetUpdate: lost tics: %d\n", newtics - t);
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
L_WriteDebug("N_TryRunTics: now %d last_tryrun %d --> real %d\n",
nowtime, nowtime - realtics, realtics);
#endif

	// simpler handling when no game in progress
	if (numplayers == 0)
	{
		while (realtics <= 0)
		{
			DoDelay();
			nowtime = N_NetUpdate();
			realtics = nowtime - last_tryrun_tic;
			last_tryrun_tic = nowtime;
		}

		// this limit is rather arbitrary
		if (realtics > TICRATE/3)
			realtics = TICRATE/3;

		return realtics;
	}

	int lowtic = maketic;
	int availabletics = lowtic - gametic;

	// this shouldn't happen, since we can only run a gametic when
	// the ticcmds for _all_ players have arrived (hence lowtic >= gametic);
	SYS_ASSERT(availabletics >= 0);

	// decide how many tics to run
	int counts;

	if (realtics + 1 < availabletics)
		counts = realtics + 1;
	else
		counts = MIN(realtics, availabletics);

#ifdef DEBUG_TICS
	L_WriteDebug("=== lowtic %d gametic %d | real %d avail %d raw-counts %d\n",
		lowtic, gametic, realtics, availabletics, counts);
#endif

	if (counts < 1)
		counts = 1;

	// wait for new tics if needed
	while (lowtic < gametic + counts)
	{
		DoDelay();
		N_NetUpdate();

		lowtic = maketic;

		SYS_ASSERT(lowtic >= gametic);
	}

	return counts;
}

void N_ResetTics(void)
{
	maketic = gametic = 0;

	last_update_tic = last_tryrun_tic = I_GetTime();
}

void N_QuitNetGame(void)
{
	// !!!! FIXME: N_QuitNetGame
}


void N_TiccmdTicker(void)
{
	// gametic <= maketic is a system-wide invariant.  However,
	// new levels are loaded during G_Ticker(), resetting them
	// both to zero, hence if we increment gametic here, we
	// break the invariant.  The following is a workaround.
	// TODO: this smells like a hack -- fix it properly!
	if (gametic >= maketic)
	{ 
		if (! (gametic == 0 && maketic == 0) && !netgame)
			I_Printf("WARNING: G_TiccmdTicker: gametic >= maketic (%d >= %d)\n", gametic, maketic);
		return;
	}

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
