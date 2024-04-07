//----------------------------------------------------------------------------
//  EDGE Game Handling Code
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

#pragma once

#include "dm_defs.h"
#include "e_event.h"
#include "e_player.h"

extern bool pistol_starts;
extern int  random_seed;
extern int  exit_time; // for savegame code
extern int  key_show_players;

// -KM- 1998/11/25 Added support for finales before levels
enum GameAction
{
    kGameActionNothing = 0,
    kGameActionNewGame,
    kGameActionLoadLevel,
    kGameActionLoadGame,
    kGameActionSaveGame,
    kGameActionIntermission,
    kGameActionFinale,
    kGameActionEndGame
};

extern GameAction game_action;

//  Game action variables:
//    kGameActionNewGame     : defer_params
//    kGameActionLoadGame    : defer_load_slot
//    kGameActionSaveGame    : defer_save_slot, defer_save_description
//
//    kGameActionLoadLevel   : current_map, players, game_skill+dm+level_flags
//    ETC kGameActionIntermission: current_map, next_map, players,
//    intermission_stats ETC kGameActionFinale      : next_map, players

class NewGameParameters
{
  public:
    SkillLevel skill_;
    int        deathmatch_;

    const MapDefinition *map_;
    // gamedef_c is implied (== map->episode)

    int random_seed_;
    int total_players_;

    PlayerFlag players_[kMaximumPlayers];

    GameFlags *flags_; // can be nullptr

    bool level_skip_ = false;

  public:
    NewGameParameters();
    NewGameParameters(const NewGameParameters &src);
    ~NewGameParameters();

  public:
    /* methods */

    void SinglePlayer(int num_bots = 0);
    // setup for single player (no netgame) and possibly some bots.

    void CopyFlags(const GameFlags *F);
};

//
// Called by the Startup code & MenuResponder; A normal game
// is started by calling the beginning map. The level jump
// cheat can get us anywhere.
//
// -ACB- 1998/08/10 New DDF Structure, Use map reference name.
//
void DeferredNewGame(NewGameParameters &params);

// Can be called by the startup code or MenuResponder,
// calls LevelSetup or W_EnterWorld.
void DeferredLoadGame(int slot);
void DeferredSaveGame(int slot, const char *description);
void DeferredScreenShot(void);
void DeferredEndGame(void);

bool MapExists(const MapDefinition *map);

// -KM- 1998/11/25 Added Time param
void ExitLevel(int time);
void ExitLevelSecret(int time);
void ExitToLevel(char *name, int time, bool skip_all);
void ExitToHub(const char *map_name, int tag);
void ExitToHub(int map_number, int tag);

void DoBigGameStuff(void);
void GameTicker(void);
bool GameResponder(InputEvent *ev);

bool CheckWhenAppear(AppearsFlag appear);

extern const MapDefinition *current_map;
extern const MapDefinition *next_map;

MapDefinition *LookupMap(const char *refname);

void DoLoadLevel(void);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
