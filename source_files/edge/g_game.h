//----------------------------------------------------------------------------
//  EDGE Game Handling Code
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

#ifndef __G_GAME__
#define __G_GAME__

#include "main.h"
#include "dm_defs.h"
#include "e_event.h"
#include "e_player.h"


extern bool pistol_starts;

extern int random_seed;  // for demo code
extern int starttime;    //

extern int exittime;  // for savegame code

extern int key_show_players;

// -KM- 1998/11/25 Added support for finales before levels
typedef enum
{
	ga_nothing = 0,
	ga_newgame,
	ga_loadlevel,
	ga_loadgame,
	ga_savegame,
	ga_playdemo,
	ga_recorddemo,
	ga_intermission,
	ga_finale,
	ga_endgame
}
gameaction_e;

extern gameaction_e gameaction;

//  Game action variables:
//    ga_newgame     : defer_params
//    ga_loadgame    : defer_load_slot
//    ga_savegame    : defer_save_slot, defer_save_desc
//    ga_playdemo    : defer_demoname, timingdemo, singledemo
//    ga_recorddemo  : defer_demoname, defer_demo_parm
// 
//    ga_loadlevel   : currmap, players, gameskill+dm+level_flags ETC
//    ga_intermission: currmap, nextmap, players, wi_stats ETC
//    ga_finale      : nextmap, players


class newgame_params_c
{
public:
	skill_t skill;
	int deathmatch;

	const mapdef_c *map;
	// gamedef_c is implied (== map->episode)

	int random_seed;
	int total_players;

	playerflag_e players[MAXPLAYERS];
	net_node_c *nodes[MAXPLAYERS];

	gameflags_t *flags;  // can be NULL

	bool level_skip = false;

public:
	newgame_params_c();
	newgame_params_c(const newgame_params_c& src);
	~newgame_params_c();

public:
	/* methods */

	void SinglePlayer(int num_bots = 0);
	// setup for single player (no netgame) and possibly some bots.

	void CopyFlags(const gameflags_t *F);
};


//
// Called by the Startup code & M_Responder; A normal game
// is started by calling the beginning map. The level jump
// cheat can get us anywhere.
//
// -ACB- 1998/08/10 New DDF Structure, Use map reference name.
//
void G_DeferredNewGame(newgame_params_c& params);

// Can be called by the startup code or M_Responder,
// calls P_SetupLevel or W_EnterWorld.
void G_DeferredLoadGame(int slot);
void G_DeferredSaveGame(int slot, const char *description);
void G_DeferredScreenShot(void);
void G_DeferredEndGame(void);

bool G_MapExists(const mapdef_c *map);

// -KM- 1998/11/25 Added Time param
void G_ExitLevel(int time);
void G_SecretExitLevel(int time);
void G_ExitToLevel(char *name, int time, bool skip_all);
void G_ExitToHub(const char *map_name, int tag);
void G_ExitToHub(int map_number, int tag);

void G_BigStuff(void);
void G_Ticker(void);
bool G_Responder(event_t * ev);

bool G_CheckWhenAppear(when_appear_e appear);

extern const mapdef_c* currmap;
extern const mapdef_c* nextmap;

mapdef_c* G_LookupMap(const char *refname);

void G_DoLoadLevel(void);         // for demo code
void G_SpawnInitialPlayers(void); //

#endif  /* __G_GAME__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
