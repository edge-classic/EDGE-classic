//----------------------------------------------------------------------------
//  EDGE: DeathBots
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

#ifndef __P_BOT_H__
#define __P_BOT_H__

#include "types.h"
#include "p_mobj.h"
#include "e_ticcmd.h"

class weapondef_c;
class bot_path_c;


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


/* ???
// what kind of thing we are looking for
enum bot_find_thing_e
{
	FIND_Enemy = 0,  // an enemy (or barrel)
	FIND_Goal,       // a big item (the roam goal)
	FIND_Other,      // anything useful or needed
};
*/


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

	int weapon_count = 0;
	int move_count   = 0;
	int roam_count   = 0;
	int dead_count   = 0;
	int look_time    = 0;
	int weave_time   = 0;

	// 0 = go straight, 1 = left, 2 = right
	int weave = 0;

	// last position, to check if we actually moved
	float last_x = 0;
	float last_y = 0;
	bool  hit_obstacle;

	// pathing info.
	// used for DM roaming, COOP follow-the-leader, and getting items
	position_c roam_goal  { 0, 0, 0 };
	position_c path_point { 0, 0, 0 };
	bot_path_c * path = NULL;

	botcmd_t cmd;

public:
	void Think();
	void DeathThink();
	void ConvertTiccmd(ticcmd_t *dest);
	void Respawn();
	void EndLevel();

private:
	int  CalcConfidence();
	int  EvaluateWeapon(int w_num) const;
	void SelectWeapon();
	bool HasWeapon(const weapondef_c *info) const;
	bool MeleeWeapon() const;

	void NewChaseDir(bool move_ok);
	void Chase(bool seetarget, bool move_ok);
	void Move();
	void MoveToward(const position_c& pos);
	void WeaveToward(const position_c& pos);
	void WeaveToward(const mobj_t *mo);
	void WeaveBehindLeader(const mobj_t *leader);

	void DetectObstacle();
	bool FollowPath();
	void Meander();

	void LookAround();
	void LookForEnemies();
	void LookForLeader();
	void PathToLeader();

	bool IsBarrel(const mobj_t *mo);

	void Think_Roam();
	void Think_Help();
	void Think_Fight();
	void Think_GetItem();
	void Think_OpenDoor();
	void Think_UseLift();

	void PainResponse();

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
