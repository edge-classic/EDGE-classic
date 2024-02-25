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

#ifndef __N_NETWORK_H__
#define __N_NETWORK_H__

extern bool netgame;
extern int game_tic;

void N_InitNetwork(void);
void N_Shutdown(void);

// Create any new ticcmds and broadcast to other players.
// returns value of GetTime().
int N_NetUpdate();

// returns number of ticks to run (always > 0).
int N_TryRunTics();

// restart tic counters (maketic, game_tic) at zero.
void N_ResetTics(void);

void N_GrabTiccmds(void);

#endif /* __N_NETWORK_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
