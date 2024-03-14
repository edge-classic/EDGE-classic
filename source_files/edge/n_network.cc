//----------------------------------------------------------------------------
//  EDGE Networking (New)
//----------------------------------------------------------------------------
//
//  Copyright (c) 2004-2024 The EDGE Team.
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

#include "n_network.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "coal.h"
#include "dm_state.h"
#include "e_input.h"
#include "e_main.h"
#include "e_player.h"
#include "edge_profiling.h"
#include "endianess.h"
#include "epi_windows.h"
#include "g_game.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_random.h"
#include "script/compat/lua_compat.h"
#include "str_util.h"
#include "types.h"
#include "vm_coal.h"  // for coal::vm_c

extern coal::vm_c *ui_vm;
extern void        CoalSetFloat(coal::vm_c *vm, const char *mod_name,
                                const char *var_name, double value);

// only true if packets are exchanged with a server
bool network_game = false;

// 70Hz
EDGE_DEFINE_CONSOLE_VARIABLE(double_framerate, "1", kConsoleVariableFlagArchive)
EDGE_DEFINE_CONSOLE_VARIABLE(
    busy_wait, "1",
    kConsoleVariableFlagReadOnly)  // Not sure what to rename this yet - Dasho

#if !defined(__MINGW32__) && \
    (defined(WIN32) || defined(_WIN32) || defined(_WIN64))
HANDLE windows_timer = nullptr;
#endif

// game_tic is the tic about to (or currently being) run.
// make_tic is the tic that hasn't had control made for it yet.
//
// NOTE 1: it is a system-wide INVARIANT that game_tic <= make_tic, since
//         we cannot run a physics step without a ticcmd for each player.
//
// NOTE 2: make_tic - game_tic is number of buffered (un-run) ticcmds,
//         and it must be <= kBackupTics (the maximum buffered ticcmds).

int game_tic;
int make_tic;

static int last_update_tic;   // last time NetworkUpdate  was called
static int last_try_run_tic;  // last time NetworkTryRunTicCommands was called

//----------------------------------------------------------------------------
//  TIC HANDLING
//----------------------------------------------------------------------------

void NetworkInitialize(void)
{
    srand(PureRandomNumber());

    NetworkResetTics();

#if !defined(__MINGW32__) && \
    (defined(WIN32) || defined(_WIN32) || defined(_WIN64))
    windows_timer = CreateWaitableTimerExW(
        nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
        TIMER_ALL_ACCESS);
    if (windows_timer != nullptr) { busy_wait = 0; }
#endif
}

void NetworkShutdown(void)
{
#if !defined(__MINGW32__) && \
    (defined(WIN32) || defined(_WIN32) || defined(_WIN64))
    if (windows_timer)
    {
        CloseHandle(windows_timer);
        windows_timer = nullptr;
    }
#endif
}

static void PreInput()
{
    // process input
    ControlGetEvents();
    EventProcessEvents();
}

static void PostInput() { EventUpdateKeyState(); }

static bool NetworkBuildTicCommands(void)
{
    // create player (and robot) ticcmds.
    // returns false if players cannot hold any more ticcmds.
    // NOTE: this is the only place allowed to bump `make_tics`.

    if (total_players == 0) return false;

    if (make_tic >= game_tic + kBackupTics) return false;

    for (int pnum = 0; pnum < kMaximumPlayers; pnum++)
    {
        Player *p = players[pnum];
        if (!p) continue;
        if (!p->Builder) continue;

        int buf = make_tic % kBackupTics;

        p->Builder(p, p->build_data_, &p->input_commands_[buf]);
    }

    make_tic++;
    return true;
}

void NetworkGrabTicCommands(void)
{
    // this is called from G_Ticker, and is the only place allowed to
    // bump `game_tic` (allowing the game simulation to advance).
    //
    // all we actually do here is grab the ticcmd for each local player
    // (i.e. ones created earler in NetworkBuildTicCommands).

    // game_tic <= make_tic is a system-wide invariant.  However, new levels
    // levels are loaded during G_Ticker(), which resets them both to zero,
    // hence we need to handle that particular case here.
    EPI_ASSERT(game_tic <= make_tic);

    if (game_tic == make_tic) return;

    int buf = game_tic % kBackupTics;

    for (int pnum = 0; pnum < kMaximumPlayers; pnum++)
    {
        Player *p = players[pnum];
        if (!p) continue;

        memcpy(&p->command_, p->input_commands_ + buf, sizeof(EventTicCommand));
    }
    if (LuaUseLuaHud())
        LuaSetFloat(LuaGetGlobalVM(), "sys", "gametic",
                    game_tic / (double_framerate.d_ ? 2 : 1));
    else
        CoalSetFloat(ui_vm, "sys", "gametic",
                     game_tic / (double_framerate.d_ ? 2 : 1));

    game_tic++;
}

//----------------------------------------------------------------------------

int NetworkUpdate()
{
    // if enough time has elapsed, process input events and build one
    // or more ticcmds for the local players.

    int now_time = GetTime();

    // singletic update is syncronous
    if (single_tics) return now_time;

    int new_tics    = now_time - last_update_tic;
    last_update_tic = now_time;

    if (new_tics > 0)
    {
        PreInput();

        // build and send new ticcmds for local players.
        // NetworkBuildTicCommands returns false when buffers are full.

        for (; new_tics > 0; new_tics--)
            if (!NetworkBuildTicCommands()) break;

        PostInput();
    }

    return now_time;
}

int NetworkTryRunTicCommands()
{
    EDGE_ZoneScoped;

    if (single_tics)
    {
        PreInput();
        NetworkBuildTicCommands();
        PostInput();
        return 1;
    }

    int now_time     = NetworkUpdate();
    int real_tics    = now_time - last_try_run_tic;
    last_try_run_tic = now_time;

#ifdef EDGE_DEBUG_TICS
    LogDebug("NetworkTryRunTicCommands: now %d last_try_run %d --> real %d\n",
             now_time, now_time - real_tics, real_tics);
#endif

    // simpler handling when no game in progress
    if (total_players == 0)
    {
        while (real_tics <= 0)
        {
            now_time         = NetworkUpdate();
            real_tics        = now_time - last_try_run_tic;
            last_try_run_tic = now_time;

            if (!busy_wait.d_ && real_tics <= 0) { SleepForMilliseconds(5); }
        }

        // this limit is rather arbitrary
        if (real_tics > kTicRate / 3) real_tics = kTicRate / 3;

        return real_tics;
    }

    EPI_ASSERT(game_tic <= make_tic);

    // decide how many tics to run...
    int tics = make_tic - game_tic;

    // -AJA- been staring at this all day, still can't explain it.
    //       my best guess is that we *usually* need an extra tic so that
    //       the ticcmd queue cannot "run away" and we never catch up.
    if (tics > real_tics + 1)
        tics = real_tics + 1;
    else
        tics = HMM_MAX(HMM_MIN(tics, real_tics), 1);

#ifdef EDGE_DEBUG_TICS
    LogDebug("=== make_tic %d game_tic %d | real %d using %d\n", make_tic,
             game_tic, real_tics, tics);
#endif

    // wait for new tics if needed
    while (make_tic < game_tic + tics)
    {
        NetworkUpdate();

        if (!busy_wait.d_ && (make_tic < game_tic + tics))
        {
            SleepForMilliseconds(5);
        }
    }

    return tics;
}

void NetworkResetTics(void)
{
    make_tic = game_tic = 0;

    last_update_tic = last_try_run_tic = GetTime();
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
