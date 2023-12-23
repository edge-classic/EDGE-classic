//----------------------------------------------------------------------------
//  EDGE Networking (New)
//----------------------------------------------------------------------------
//
//  Copyright (c) 2004-2023  The EDGE Team.
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

#include "i_defs.h"

#include <limits.h>
#include <stdlib.h>

#include "types.h"
#include "endianess.h"

#include "n_network.h"

#include "dm_state.h"
#include "e_input.h"
#include "e_main.h"
#include "g_game.h"
#include "e_player.h"
#include "m_argv.h"
#include "m_random.h"

#include "str_util.h"

#include "coal.h"
#include "vm_coal.h" // for coal::vm_c
#include "script/compat/lua_compat.h"
#include "edge_profiling.h"

extern coal::vm_c *ui_vm;
extern void        VM_SetFloat(coal::vm_c *vm, const char *mod_name, const char *var_name, double value);

// #define DEBUG_TICS 1

// only true if packets are exchanged with a server
bool netgame = false;

// 70Hz
DEF_CVAR(r_doubleframes, "1", CVAR_ARCHIVE)
DEF_CVAR(n_busywait, "1", CVAR_ROM)

#if !defined(__MINGW32__) && (defined(WIN32) || defined(_WIN32) || defined(_WIN64))
HANDLE windows_timer = NULL;
#endif

// gametic is the tic about to (or currently being) run.
// maketic is the tic that hasn't had control made for it yet.
//
// NOTE 1: it is a system-wide INVARIANT that gametic <= maketic, since
//         we cannot run a physics step without a ticcmd for each player.
//
// NOTE 2: maketic - gametic is number of buffered (un-run) ticcmds,
//         and it must be <= BACKUPTICS (the maximum buffered ticcmds).

int gametic;
int maketic;

static int last_update_tic; // last time N_NetUpdate  was called
static int last_tryrun_tic; // last time N_TryRunTics was called

//----------------------------------------------------------------------------
//  TIC HANDLING
//----------------------------------------------------------------------------

void N_InitNetwork(void)
{
    srand(I_PureRandom());

    N_ResetTics();

#if !defined(__MINGW32__) && (defined(WIN32) || defined(_WIN32) || defined(_WIN64))
    windows_timer = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (windows_timer != NULL)
    {
        n_busywait = 0;
    }
#endif
}

void N_Shutdown(void)
{
#if !defined(__MINGW32__) && (defined(WIN32) || defined(_WIN32) || defined(_WIN64))
    if (windows_timer)
    {
        CloseHandle(windows_timer);
        windows_timer = NULL;
    }
#endif
}

static void PreInput()
{
    // process input
    I_ControlGetEvents();
    E_ProcessEvents();
}

static void PostInput()
{
    E_UpdateKeyState();
}

static bool N_BuildTiccmds(void)
{
    // create player (and robot) ticcmds.
    // returns false if players cannot hold any more ticcmds.
    // NOTE: this is the only place allowed to bump `maketics`.

    if (numplayers == 0)
        return false;

    if (maketic >= gametic + BACKUPTICS)
        return false;

    for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
    {
        player_t *p = players[pnum];
        if (!p)
            continue;
        if (!p->builder)
            continue;
#if 0
		I_Debugf("N_BuildTiccmds: pnum %d netgame %c\n", pnum, netgame ? 'Y' : 'n');
#endif
        int buf = maketic % BACKUPTICS;

        p->builder(p, p->build_data, &p->in_cmds[buf]);
    }

    maketic++;
    return true;
}

void N_GrabTiccmds(void)
{
    // this is called from G_Ticker, and is the only place allowed to
    // bump `gametic` (allowing the game simulation to advance).
    //
    // all we actually do here is grab the ticcmd for each local player
    // (i.e. ones created earler in N_BuildTiccmds).

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
        if (!p)
            continue;

        memcpy(&p->cmd, p->in_cmds + buf, sizeof(ticcmd_t));
    }
    if (LUA_UseLuaHud())
        LUA_SetFloat(LUA_GetGlobalVM(), "sys", "gametic", gametic / (r_doubleframes.d ? 2 : 1));
    else
        VM_SetFloat(ui_vm, "sys", "gametic", gametic / (r_doubleframes.d ? 2 : 1));

    gametic++;
}

//----------------------------------------------------------------------------

int N_NetUpdate()
{
    // if enough time has elapsed, process input events and build one
    // or more ticcmds for the local players.

    int nowtime = I_GetTime();

    // singletic update is syncronous
    if (singletics)
        return nowtime;

    int newtics     = nowtime - last_update_tic;
    last_update_tic = nowtime;

    if (newtics > 0)
    {
        PreInput();

        // build and send new ticcmds for local players.
        // N_BuildTiccmds returns false when buffers are full.

        for (; newtics > 0; newtics--)
            if (!N_BuildTiccmds())
                break;

        PostInput();

#if 0
		if (newtics > 0 && numplayers > 0)
			I_Debugf("N_NetUpdate: lost tics: %d\n", newtics);
#endif
    }

    return nowtime;
}

int N_TryRunTics()
{
    EDGE_ZoneScoped;

    if (singletics)
    {
        PreInput();
        N_BuildTiccmds();
        PostInput();
        return 1;
    }

    int nowtime     = N_NetUpdate();
    int realtics    = nowtime - last_tryrun_tic;
    last_tryrun_tic = nowtime;

#ifdef DEBUG_TICS
    I_Debugf("N_TryRunTics: now %d last_tryrun %d --> real %d\n", nowtime, nowtime - realtics, realtics);
#endif

    // simpler handling when no game in progress
    if (numplayers == 0)
    {
        while (realtics <= 0)
        {
            nowtime         = N_NetUpdate();
            realtics        = nowtime - last_tryrun_tic;
            last_tryrun_tic = nowtime;

            if (!n_busywait.d && realtics <= 0)
            {
                I_Sleep(5);
            }
        }

        // this limit is rather arbitrary
        if (realtics > TICRATE / 3)
            realtics = TICRATE / 3;

        return realtics;
    }

    SYS_ASSERT(gametic <= maketic);

    // decide how many tics to run...
    int tics = maketic - gametic;

    // -AJA- been staring at this all day, still can't explain it.
    //       my best guess is that we *usually* need an extra tic so that
    //       the ticcmd queue cannot "run away" and we never catch up.
    if (tics > realtics + 1)
        tics = realtics + 1;
    else
        tics = std::max(std::min(tics, realtics), 1);

#ifdef DEBUG_TICS
    I_Debugf("=== maketic %d gametic %d | real %d using %d\n", maketic, gametic, realtics, tics);
#endif

    // wait for new tics if needed
    while (maketic < gametic + tics)
    {
        N_NetUpdate();

        if (!n_busywait.d && (maketic < gametic + tics))
        {
            I_Sleep(5);
        }
    }

    return tics;
}

void N_ResetTics(void)
{
    maketic = gametic = 0;

    last_update_tic = last_tryrun_tic = I_GetTime();
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
