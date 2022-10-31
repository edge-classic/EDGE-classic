//----------------------------------------------------------------------------
//  EDGE: DeathBots
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2022  The EDGE Team.
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

#ifndef __P_BOT_H__
#define __P_BOT_H__

#include "types.h"
#include "p_mobj.h"
#include "e_ticcmd.h"
#include "r_defs.h"

class weapondef_c;
class bot_path_c;

typedef struct benefit_s benefit_t;


// specific tasks which the bot needs/wants to do.
// these can occur in combination with the behaviors above, e.g. while
// attacking something a bot may still want to pickup some health or
// use a lift.
enum bot_task_e
{
	TASK_None = 0,    // no task right now
	TASK_GetItem,     // pickup a nearby item (in tracer)
	TASK_OpenDoor,    // open a door
	TASK_UseLift,     // lower a lift, ride it to top
};


// stages for TASK_OpenDoor
enum task_open_door_e
{
	TKDOOR_Approach = 0,  // walk to door and face it
	TKDOOR_Use      = 1,  // press USE button, wait for it to open
//--	TKDOOR_Wait     = 2,  // wait for door to open
};


// stages for TASK_UseLift
enum task_use_lift_e
{
	TKLIFT_Approach = 0,  // walk to lift and face it
	TKLIFT_Use      = 1,  // press USE button to lower it
	TKLIFT_Wait     = 2,  // wait for lift to lower
	TKLIFT_Embark   = 3,  // get onto the lift
	TKLIFT_Ride     = 4,  // ride the lift to the top
};


// results of FollowPath()
enum bot_follow_path_e
{
	FOLLOW_OK = 0,    // going okay...
	FOLLOW_Done,      // reached end of path
	FOLLOW_Failed     // got stuck somewhere
};


// This describes what action the bot wants to do.
// It will be translated to a ticcmd_t by P_BotPlayerBuilder.

struct botcmd_t
{
	int move_speed;
	angle_t move_angle;

	// The weapon we want to use. -1 if the current one is fine.
	int new_weapon;

	bool attack;
	bool second_attack;
	bool use;
	bool jump;
};


class bot_t
{
public:
	struct player_s *pl = NULL;

	bot_task_e task = TASK_None;

	// TODO describe or remove
	int confidence = 0;

	int patience = 0;

	angle_t angle = 0;
	angle_t strafedir = 0;

	angle_t look_angle = 0;
	float   look_slope = 0;

	// 0 = go straight, -1 = left, +1 = right
	int weave = 0;
	int weave_time = 0;

	int weapon_count = 0;
	int move_count   = 0;

	int dead_time = 0;
	int look_time = 0;

	// last position, to check if we actually moved
	float last_x = 0;
	float last_y = 0;
	bool  hit_obstacle = false;
	bool  near_leader  = false;

	// -- pathing info --
	// used for DM roaming, COOP follow-the-leader, and getting items.
	// main_goal is final target.  travel_time detects losing the path.
	// path_wait is when we need a path, but are waiting a bit.
	bot_path_c * path = NULL;
	position_c roam_goal { 0, 0, 0 };
	int travel_time  = 0;
	int path_wait    = 0;

	// information for TASK_GetItem (+ the pathing info)
	int item_time = 0;

	// information for TASK_OpenDoor
	int door_stage = 0;
	int door_time  = 0;
	const seg_t * door_seg = NULL;

	// information for TASK_UseLift
	int lift_stage = 0;
	int lift_time  = 0;
	const seg_t * lift_seg = NULL;

	botcmd_t cmd;

public:
	void Think();
	void DeathThink();
	void ConvertTiccmd(ticcmd_t *dest);
	void Respawn();
	void EndLevel();

private:
	float DistTo(position_c pos) const;

	int  CalcConfidence();
	int  EvaluateWeapon(int w_num) const;
	void SelectWeapon();
	bool HasWeapon(const weapondef_c *info) const;
	bool MeleeWeapon() const;
	bool CanGetArmour(const benefit_t *be, int extendedflags) const;

	void NewChaseDir(bool move_ok);
	void Chase(bool seetarget, bool move_ok);
	void Move();
	void MoveToward(const position_c& pos);
	void WeaveToward(const position_c& pos);
	void WeaveToward(const mobj_t *mo);
	void WeaveNearLeader(const mobj_t *leader);

	bot_follow_path_e FollowPath();
	void DetectObstacle();
	void Meander();

	void LookAround();
	void LookForEnemies(float radius);
	void LookForItems(float radius);
	void LookForLeader();
	void PathToLeader();
	void EstimateTravelTime();

	bool IsBarrel(const mobj_t *mo);

	void Think_Roam();
	void Think_Help();
	void Think_Fight();
	void Think_GetItem();
	void Think_OpenDoor();
	void Think_UseLift();

	void PainResponse();
	void FinishGetItem();
	void FinishOpenDoor(bool ok);

	void TurnToward(angle_t angle, float slope, bool fast);
	void TurnToward(const mobj_t *mo, bool fast);
	void DeletePath();

public:
	float EvalItem (const mobj_t *mo);
	float EvalEnemy(const mobj_t *mo);
};

void P_BotCreate(struct player_s *pl, bool recreate);

void BOT_BeginLevel(void);
void BOT_EndLevel(void);

#endif // __P_BOT_H__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
