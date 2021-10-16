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


extern int bot_skill;  // 0 to 2


// This describes what action the bot wants to do.
// It will be translated to a ticcmd_t by P_BotPlayerBuilder.

typedef struct botcmd_s
{
	int move_speed;
	angle_t move_angle;

	// Object which we want to face, NULL if none
	struct mobj_s *face_mobj;

	// The weapon we want to use. -1 if the current one is fine.
	int new_weapon;

	bool attack;
	bool second_attack;
	bool use;
	bool jump;
} 
botcmd_t;


typedef struct bot_s
{
	struct player_s *pl;

	int confidence;
	int patience;

	angle_t angle;

	int weapon_count;
	int move_count;
	int use_count;

	// last position, to check if we actually moved
	float last_x, last_y;

	angle_t strafedir;

	botcmd_t cmd;
}
bot_t;

void P_BotCreate(struct player_s *pl, bool recreate);

void BOT_BeginLevel(void);
void BOT_EndLevel(void);

#endif // __P_BOT_H__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
