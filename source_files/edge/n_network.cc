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


static void GetPackets(bool do_delay)
{
	if (! netgame)
	{
		// -AJA- This can make everything a bit "jerky" :-(
		if (do_delay && ! m_busywait.d)
			I_Sleep(10 /* millis */);

		return;
	}

#ifdef USE_HAWKNL

	NLsocket socks[4];  // only one in the group

	int delay = (do_delay && ! m_busywait.d) ? 10 /* millis */ : 0;
	int num = nlPollGroup(sk_group, NL_READ_STATUS, socks, 4, delay);

	if (num < 1)
		return;

	if (! pk.Read(socks[0]))
		return;

	L_WriteDebug("- GOT PACKET [%c%c] len = %d\n", pk.hd().type[0], pk.hd().type[1],
		pk.hd().data_len);

#if 0
	if (! pk.CheckType("Tg"))
		return;

	// FIXME: validate packet, check size

	tic_group_proto_t& tg = pk.tg_p();

	int count = tg.last_player - tg.first_player + 1;

	tg.ByteSwap();
	tg.ByteSwapCmds((1 + bots_each) * count);

	// !!! FIXME: handle tg.count

	player_t *p = players[consoleplayer]; //!!!!

	int got_tic = tg.gametic + tg.offset;

	L_WriteDebug("-- Got TG: counter = %d  in_tic = %d\n", got_tic, p->in_tic);

	if (got_tic != p->in_tic)
	{
		L_WriteDebug("GOT TIC %d != EXPECTED %d\n", got_tic, p->in_tic);

		// !!! FIXME: handle "future" tics (save them, set bit in mask)
		//            (send retransmission request for gametic NOW).
		return;
	}

	ticcmd_t *raw_cmd = tg.tic_cmds;

	for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
	{
		player_t *p = players[pnum];

		if (! p) continue;

		memcpy(p->in_cmds + (got_tic % (MP_SAVETICS*2)), raw_cmd, sizeof(ticcmd_t));

		raw_cmd++;

		p->in_tic++;
	}

//	SYS_ASSERT((raw_cmd - tg.tic_cmds) == (1 + bots_each));

#endif

#endif  // USE_HAWKNL
}

static void DoSendTiccmds(int tic)
{
#ifdef USE_HAWKNL_XX
	pk.Clear();  // FIXME: TESTING ONLY

	pk.SetType("tc");
	pk.hd().flags = 0;
	pk.hd().data_len = sizeof(ticcmd_proto_t) - sizeof(ticcmd_t);
	pk.hd().client = client_id;

    ticcmd_proto_t& tc = pk.tc_p();

	SYS_ASSERT(tic >= gametic);

	tc.gametic = gametic;
	tc.offset  = tic - gametic;
	tc.count   = 1;

	ticcmd_t *raw_cmd = tc.tic_cmds;

	for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
	{
		player_t *p = players[pnum];

		if (! p || ! p->builder) continue;

		memcpy(raw_cmd, p->out_cmds + (tic % (MP_SAVETICS*2)), sizeof(ticcmd_t));

		raw_cmd++;

		pk.hd().data_len += sizeof(ticcmd_t);
	}

	SYS_ASSERT((raw_cmd - tc.tic_cmds) == (1 + bots_each));

    tc.ByteSwap();
    tc.ByteSwapCmds((1 + bots_each) * tc.count);

	L_WriteDebug("Writing ticcmd for tic %d\n", tic);

	if (! pk.Write(socket))
		L_WriteDebug("Failed to write packet (tic %d)\n", tic);
	
	// FIXME: need an 'out_tic' for resends....

#endif  // USE_HAWKNL_XX
}

bool N_BuildTiccmds(void)
{
	I_ControlGetEvents();
	E_ProcessEvents();

	if (numplayers == 0)
	{
		E_UpdateKeyState();
		return false;
	}

	if (maketic >= gametic + MP_SAVETICS)
	{
		E_UpdateKeyState();
		return false;  // can't hold any more
	}

	// build ticcmds
	for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
	{
		player_t *p = players[pnum];
		if (! p) continue;

		if (p->builder)
		{
			ticcmd_t *cmd;

///     L_WriteDebug("N_BuildTiccmds: pnum %d netgame %c\n", pnum, netgame ? 'Y' : 'n');

			if (false) // FIXME: temp hack!!!  if (netgame)
				cmd = &p->out_cmds[maketic % (MP_SAVETICS*2)];
			else
				cmd = &p->in_cmds[maketic % (MP_SAVETICS*2)];

			p->builder(p, p->build_data, cmd);
			
			cmd->consistency = p->consistency[maketic % (MP_SAVETICS*2)];
		}
	}

	E_UpdateKeyState();

	if (netgame)
		DoSendTiccmds(maketic);

	maketic++;
	return true;
}

int N_NetUpdate(bool do_delay)
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

	GetPackets(do_delay);

	return nowtime;
}

int N_TryRunTics(bool *is_fresh)
{
	*is_fresh = true;

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
			nowtime = N_NetUpdate(true);
			realtics = nowtime - last_tryrun_tic;
			last_tryrun_tic = nowtime;
		}

		if (realtics > MP_SAVETICS)
			realtics = MP_SAVETICS;

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
		int wait_tics = N_NetUpdate(true) - last_tryrun_tic;

		lowtic = maketic;

		SYS_ASSERT(lowtic >= gametic);

		// don't stay in here forever -- give the menu a chance to work
		if (wait_tics > TICRATE/2)
		{
			L_WriteDebug("Waited %d tics IN VAIN !\n", wait_tics);
			*is_fresh = false;
			return 3;
		}
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

#define TURBOTHRESHOLD  0x32

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

		// check for turbo cheats
		if (p->cmd.forwardmove > TURBOTHRESHOLD
			&& !(gametic & 31) && (((gametic >> 5) + p->pnum) & 0x1f) == 0)
		{
			// FIXME: something better for turbo cheat
			I_Printf(language["IsTurbo"], p->playername);
		}

		if (netgame)
		{
			if (gametic > BACKUPTICS && p->consistency[buf] != p->cmd.consistency)
			{
// !!!! DEBUG //	I_Warning("Consistency failure on player %d (%i should be %i)",
//					p->pnum + 1, p->cmd.consistency, p->consistency[buf]);
			}
			if (p->mo)
				p->consistency[buf] = (int)p->mo->x;
			else
				p->consistency[buf] = P_ReadRandomState() & 0xff;
		}
	}

	VM_SetFloat(ui_vm, "sys", "gametic", gametic);

	gametic++;
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
