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


// the current behavior of the bot.
// this is very generic, more specific tasks (like using a lift) are
// not handled by this enumeration.
enum bot_behavior_e
{
	BHV_Roam = 0,  // roaming about, often trying to get somewhere
	BHV_Help,      // helping / following a human (in supportobj)
	BHV_Attack,    // attacking a monster or player (in target)
	BHV_Flee,      // fleeing from a monster or player (in target)
};


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
	TSAK_Teleport,    // use a teleporter
};


// This describes what action the bot wants to do.
// It will be translated to a ticcmd_t by P_BotPlayerBuilder.

struct botcmd_t
{
	int move_speed;
	angle_t move_angle;

	bool face_target;

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

	bot_behavior_e behave = BHV_Roam;
	bot_task_e     task   = TASK_None;

	int confidence = 0;
	int patience   = 0;

	angle_t angle = 0;
	angle_t strafedir = 0;

	angle_t look_angle = 0;
	float   look_slope = 0;

	int weapon_count = 0;
	int move_count   = 0;
	int roam_count   = 0;
	int dead_count   = 0;

	// last position, to check if we actually moved
	float last_x = 0;
	float last_y = 0;

	bot_path_c * path = NULL;
	position_c path_target { 0, 0, 0 };

	botcmd_t cmd;

public:
	void Think();
	void ConvertTiccmd(ticcmd_t *dest);
	void EndLevel();

private:
	bool HasWeapon(const weapondef_c *info) const;
	bool MeleeWeapon() const;

	void NewChaseDir(bool move_ok);
	void Chase(bool seetarget, bool move_ok);
	void Move();
	void MoveToward(const position_c& pos);
	void Roam();

	void Confidence();
	int  EvaluateWeapon(int w_num) const;
	public: int  EvaluateItem(const mobj_t *mo) const; private:
	void SelectWeapon();

	bool LookForEnemies();
	bool LookForItems();
	void LineOfSight(angle_t angle);

	void TurnToward(angle_t angle, float slope);
	void TurnToward(const mobj_t *mo);
};

void P_BotCreate(struct player_s *pl, bool recreate);

void BOT_BeginLevel(void);
void BOT_EndLevel(void);

#endif // __P_BOT_H__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
