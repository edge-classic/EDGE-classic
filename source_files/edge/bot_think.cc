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

#include "i_defs.h"

#include "bot_think.h"
#include "bot_nav.h"
#include "con_main.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "g_game.h"
#include "m_bbox.h"
#include "m_random.h"
#include "p_local.h"
#include "p_weapon.h"
#include "p_action.h"
#include "rad_trig.h"
#include "r_state.h"
#include "s_sound.h"
#include "w_wad.h"

#define DEBUG  0


// this ranges from 0 (EASY) to 2 (HARD)
DEF_CVAR(bot_skill, "1", CVAR_ARCHIVE)


static int strafe_chances[3] = { 128, 192, 256 };
static int attack_chances[3] = {  32,  64, 160 };

#define MOVE_SPEED  45


//----------------------------------------------------------------------------
//  EVALUATING ITEMS, MONSTERS, WEAPONS
//----------------------------------------------------------------------------

bool bot_t::HasWeapon(const weapondef_c *info) const
{
	for (int i=0 ; i < MAXWEAPONS ; i++)
		if (pl->weapons[i].owned && pl->weapons[i].info == info)
			return true;

	return false;
}


bool bot_t::CanGetArmour(const benefit_t *be, int extendedflags) const
{
	// this matches the logic in GiveArmour() in p_inter.cc

	armour_type_e a_class = (armour_type_e) be->sub.type;

	float amount  = be->amount;

	if (extendedflags & EF_SIMPLEARMOUR)
	{
		float slack = be->limit - pl->armours[a_class];

		if (amount > slack)
			amount = slack;

		return (amount > 0);
	}

	float slack = be->limit - pl->totalarmour;
	float upgrade = 0;

	if (slack < 0)
		return false;

	for (int cl = a_class - 1 ; cl >= 0 ; cl--)
		upgrade += pl->armours[cl];

	if (upgrade > amount)
		upgrade = amount;

	slack += upgrade;

	if (amount > slack)
		amount = slack;

	return ! (amount == 0 && upgrade == 0);
}


bool bot_t::MeleeWeapon() const
{
	int wp_num = pl->ready_wp;

	if (pl->pending_wp >= 0)
		wp_num = pl->pending_wp;

	return pl->weapons[wp_num].info->ammo[0] == AM_NoAmmo;
}


bool bot_t::IsBarrel(const mobj_t *mo)
{
	if (mo->player)
		return false;

	if (0 == (mo->extendedflags & EF_MONSTER))
		return false;

	return true;
}


float bot_t::EvalEnemy(const mobj_t *mo)
{
	// returns -1 to ignore, +1 to attack.
	// [ higher values are not possible, so no way to prioritize enemies ]

	// The following must be true to justify that you attack a target:
	// - target may not be yourself or your support obj.
	// - target must either want to attack you, or be on a different side
	// - target may not have the same supportobj as you.
	// - You must be able to see and shoot the target.

	if (0 == (mo->flags & MF_SHOOTABLE) || mo->health <= 0)
		return -1;

	// occasionally shoot barrels
	if (IsBarrel(mo))
		return (C_Random() % 100 < 20) ? +1 : -1;

	if (0 == (mo->extendedflags & EF_MONSTER) && ! mo->player)
		return -1;

	if (mo->player && mo->player == pl)
		return -1;

	if (pl->mo->supportobj == mo)
		return -1;

	if (! DEATHMATCH() && mo->player)
		return -1;

	if (! DEATHMATCH() && mo->supportobj && mo->supportobj->player)
		return -1;

	// EXTERMINATE !!

	return 1.0;
}


float bot_t::EvalItem(const mobj_t *mo)
{
	// determine if an item is worth getting.
	// this depends on our current inventory, whether the game mode is COOP
	// or DEATHMATCH, and whether we are fighting or not.

	if (0 == (mo->flags & MF_SPECIAL))
		return -1;

	bool fighting = (pl->mo->target != NULL);

	// do we *really* need some health?
	bool want_health = (mo->health < 90);
	bool need_health = (mo->health < 45);

	// handle weapons first (due to deathmatch rules)
	for (const benefit_t *B = mo->info->pickup_benefits ; B != NULL ; B = B->next)
	{
		if (B->type == BENEFIT_Weapon)
		{
			if (! HasWeapon(B->sub.weap))
				return NAV_EvaluateBigItem(mo);

			// try to get ammo from a dropped weapon
			if (mo->flags & MF_DROPPED)
				continue;

			// cannot get the ammo from a placed weapon except in altdeath
			if (deathmatch != 2)
				return -1;
		}

		// ignore powerups, backpacks and armor in COOP.
		// [ leave them for the human players ]
		if (! DEATHMATCH())
		{
			switch (B->type)
			{
				case BENEFIT_Powerup:
				case BENEFIT_Armour:
				case BENEFIT_AmmoLimit:
					return -1;

				default: break;
			}
		}
	}

	for (const benefit_t *B = mo->info->pickup_benefits ; B != NULL ; B = B->next)
	{
		switch (B->type)
		{
			case BENEFIT_Key:
				// have it already?
				if (pl->cards & (keys_e) B->sub.type)
					continue;

				return 90;

			case BENEFIT_Powerup:
				return NAV_EvaluateBigItem(mo);

			case BENEFIT_Armour:
				// ignore when fighting
				if (fighting)
					return -1;

				if (! CanGetArmour(B, mo->extendedflags))
					continue;

				return NAV_EvaluateBigItem(mo);

			case BENEFIT_Health:
			{
				// cannot get it?
				if (pl->health >= B->limit)
					return -1;

				// ignore potions unless really desperate
				if (B->amount < 2.5)
				{
					if (pl->health > 19)
						return -1;

					return 2;
				}

				// don't grab health when fighting unless we NEED it
				if (! (need_health || (want_health && ! fighting)))
					return -1;

				if (need_health)
					return 120;
				else if (B->amount > 55)
					return 40;
				else
					return 30;
			}

			case BENEFIT_Ammo:
			{
				if (B->sub.type == AM_NoAmmo)
					continue;

				int ammo = B->sub.type;
				int max  = pl->ammo[ammo].max;

				// in COOP mode, leave some ammo for others
				if (! DEATHMATCH())
					max = max / 4;

				if (pl->ammo[ammo].num >= max)
					continue;

				if (pl->ammo[ammo].num == 0)
					return 35;
				else if (fighting)
					// ignore unneeded ammo when fighting
					continue;
				else
					return 10;
			}

			case BENEFIT_Inventory:
				// TODO : heretic stuff
				continue;

			default:
				continue;
		}
	}

	return -1;
}


float bot_t::EvaluateWeapon(int w_num, int& key) const
{
	// this evaluates weapons owned by the bot (NOT ones in the map).
	// returns -1 when not actually usable (e.g. no ammo).

	playerweapon_t *wp = &pl->weapons[w_num];

	// don't have this weapon
	if (! wp->owned)
		return -1;

	weapondef_c *weapon = wp->info;
	SYS_ASSERT(weapon);

	atkdef_c *attack = weapon->attack[0];
	if (!attack)
		return -1;

	key = weapon->bind_key;

	// have enough ammo?
	if (weapon->ammo[0] != AM_NoAmmo)
	{
		if (pl->ammo[weapon->ammo[0]].num < weapon->ammopershot[0])
			return -1;
	}

	float score = 10.0f * weapon->priority;

	// prefer smaller weapons for smaller monsters.
	// when not fighting, prefer biggest non-dangerous weapon.
	if (pl->mo->target == NULL)
	{
		if (! weapon->dangerous)
			score += 1000.0f;
	}
	else if (pl->mo->target->info->spawnhealth > 250)
	{
		if (weapon->priority > 5)
			score += 1000.0f;
	}
	else
	{
		if (2 <= weapon->priority && weapon->priority <= 5)
			score += 1000.0f;
	}

	// small preference for the current weapon (break ties)
	if (w_num == pl->ready_wp)
		score += 2.0f;

	// ultimate tie breaker (when two weapons have same priority)
	score += (float)w_num / 32.0f;

	return score;
}

//----------------------------------------------------------------------------

float bot_t::DistTo(position_c pos) const
{
	float dx = fabs(pos.x - pl->mo->x);
	float dy = fabs(pos.y - pl->mo->y);

	return hypotf(dx, dy);
}


void bot_t::NewChaseDir(bool move_ok)
{
	mobj_t *mo = pl->mo;

	// FIXME: This is not very intelligent...
	int r = M_Random();

	if (mo->target && (r % 3 == 0))
	{
		angle = R_PointToAngle(mo->x, mo->y, mo->target->x, mo->target->y);
	}
	else if (mo->supportobj && (r % 3 == 1))
	{
		angle = R_PointToAngle(mo->x, mo->y, mo->supportobj->x, mo->supportobj->y);
	}
	else if (move_ok)
	{
		angle_t diff = r - M_Random();

		angle += (diff << 21);
	}
	else
		angle = (r << 24);
}


void bot_t::PainResponse()
{
	// oneself?
	if (pl->attacker == pl->mo)
		return;

	// ignore friendly fire -- shit happens
	if (! DEATHMATCH() && pl->attacker->player)
		return;

	if (pl->attacker->health <= 0)
	{
		pl->attacker = NULL;
		return;
	}

	// TODO only update 'target' if threat is greater than current target

	if (pl->mo->target == NULL)
		pl->mo->SetTarget(pl->attacker);
}


void bot_t::LookForLeader()
{
	if (DEATHMATCH())
		return;

	if (pl->mo->supportobj != NULL)
		return;

	for (int i = 0 ; i < MAXPLAYERS ; i++)
	{
		player_t * p2 = players[i];

		if (p2 == NULL || p2->isBot())
			continue;

		// when multiple humans, make it random who is picked
		if (C_Random() % 100 < 90)
			continue;

		pl->mo->SetSupportObj(p2->mo);
	}
}


void bot_t::LookForEnemies(float radius)
{
	// TODO check sight of existing target
	//      [ if too many checks, lose patience ]

//	return; //!!!!

	if (pl->mo->target == NULL)
	{
		mobj_t * enemy = NAV_FindEnemy(this, radius);

		if (enemy != NULL)
		{
			// sight check, since enemy may be on other side of a wall
			if (P_CheckSight(pl->mo, enemy))
				pl->mo->SetTarget(enemy);
		}
	}
}


void bot_t::LookForItems(float radius)
{
	mobj_t * item = NULL;
	bot_path_c * item_path = NAV_FindThing(this, radius, item);

	if (item_path == NULL)
		return;

	// GET IT !!

	pl->mo->SetTracer(item);

	DeletePath();

	task = TASK_GetItem;
	path = item_path;
	item_time = TICRATE;

	EstimateTravelTime();
}


void bot_t::LookAround()
{
	look_time--;

	if ((look_time & 3) == 2)
		LookForLeader();

	if (look_time & 1)
	{
		LookForEnemies(768);
		return;
	}

	if (look_time >= 0)
		return;

	// look for items every second or so
	look_time = 20 + C_Random() % 20;

	LookForItems(1024);
}


void bot_t::SelectWeapon()
{
	// reconsider every second or so
	weapon_time = 20 + C_Random() % 20;

	// allow any weapon change to complete first
	if (pl->pending_wp != WPSEL_NoChange)
		return;

	int   best       = pl->ready_wp;
	int   best_key   = -1;
	float best_score =  0;

	for (int i = 0 ; i < MAXWEAPONS ; i++)
	{
		int   key   = -1;
		float score = EvaluateWeapon(i, key);

		if (score > best_score)
		{
			best       = i;
			best_key   = key;
			best_score = score;
		}
	}

	if (best != pl->ready_wp)
	{
		cmd.weapon = best_key;
	}
}


void bot_t::Move()
{
	cmd.speed = MOVE_SPEED;
	cmd.direction = angle + strafedir;
}


void bot_t::MoveToward(const position_c& pos)
{
	cmd.speed = MOVE_SPEED;
	cmd.direction = R_PointToAngle(pl->mo->x, pl->mo->y, pos.x, pos.y);
}


void bot_t::WalkToward(const position_c& pos)
{
	cmd.speed = MOVE_SPEED * 0.5;
	cmd.direction = R_PointToAngle(pl->mo->x, pl->mo->y, pos.x, pos.y);
}


void bot_t::TurnToward(angle_t want_angle, float want_slope, bool fast)
{
	// horizontal (yaw) angle
	angle_t delta = want_angle - pl->mo->angle;

	if (delta < ANG180)
		delta = delta / (fast ? 3 : 8);
	else
		delta = ANG_MAX - (ANG_MAX - delta) / (fast ? 3 : 8);

	look_angle  = pl->mo->angle + delta;

	// vertical (pitch or mlook) angle
	if (want_slope < -2.0) want_slope = -2.0;
	if (want_slope > +2.0) want_slope = +2.0;

	float diff = want_slope - M_Tan(pl->mo->vertangle);

	if (fabs(diff) < 0.025)
		look_slope = want_slope;
	else if (diff < 0)
		look_slope -= 0.02;
	else
		look_slope += 0.02;
}


void bot_t::TurnToward(const mobj_t *mo, bool fast)
{
	float dx = mo->x - pl->mo->x;
	float dy = mo->y - pl->mo->y;
	float dz = mo->z - pl->mo->z;

	angle_t want_angle = R_PointToAngle(0, 0, dx, dy);
	float   want_slope = P_ApproxSlope(dx, dy, dz);

	TurnToward(want_angle, want_slope, fast);
}


void bot_t::WeaveToward(const position_c& pos)
{
	// usually try to move directly toward a wanted position.
	// but if something gets in our way, we try to "weave" around it,
	// by sometimes going diagonally left and sometimes right.

	float dist = DistTo(pos);

	if (weave_time-- < 0)
	{
		weave_time = 10 + C_Random() % 10;

		bool neg = weave < 0;

		if (hit_obstacle)
			weave = neg ? +2 : -2;
		else if (dist > 192.0)
			weave = neg ? +1 : -1;
		else
			weave = 0;
	}

	MoveToward(pos);

	if (weave == -2) cmd.direction -= ANG5 * 12;
	if (weave == -1) cmd.direction -= ANG5 * 3;
	if (weave == +1) cmd.direction += ANG5 * 3;
	if (weave == +2) cmd.direction += ANG5 * 12;

	// TODO : REVIEW FOLLOWING LOGIC
/*
		// If there is a wall or something in the way, pick a new direction.
		if (!move_ok || move_count < 0)
		{
			if (seetarget && move_ok)
				cmd.face_target = true;

			angle = R_PointToAngle(mo->x, mo->y, mo->target->x, mo->target->y);
			strafedir = 0;
			move_count = 10 + (C_Random() & 31);

			if (!move_ok)
			{
				int r = M_Random();

				if (r < 10)
					angle = M_Random() << 24;
				else if (r < 60)
					strafedir = ANG90;
				else if (r < 110)
					strafedir = ANG270;
			}
		}
	}
*/
}


void bot_t::WeaveToward(const mobj_t *mo)
{
	position_c pos { mo->x, mo->y, mo->z };

	WeaveToward(pos);
}


void bot_t::DetectObstacle()
{
	mobj_t *mo = pl->mo;

	float dx = last_x - mo->x;
	float dy = last_y - mo->y;

	last_x = mo->x;
	last_y = mo->y;

	hit_obstacle = (dx * dx + dy * dy) < 0.2;
}


void bot_t::Chase(bool seetarget, bool move_ok)
{
	int skill = CLAMP(0, bot_skill.d, 2);

	mobj_t *mo = pl->mo;

#if (DEBUG > 1)
		I_Printf("BOT %d: Chase %s dist %1.1f angle %1.0f | %s\n",
			bot->pl->pnum, mo->target->info->name.c_str(),
			P_ApproxDistance(mo->x - mo->target->x, mo->y - mo->target->y),
			ANG_2_FLOAT(bot->angle), move_ok ? "move_ok" : "NO_MOVE");
#endif

	if (seetarget)
	{
		// face the target
		TurnToward(mo->target, true);

		// Shoot it,
		cmd.attack = M_Random() < attack_chances[skill];

		if (move_count < 0)
		{
			move_count = 20 + (M_Random() & 63);
			strafedir = 0;

			if (MeleeWeapon())
			{
				// run directly toward target
			}
			else if (M_Random() < strafe_chances[skill])
			{
				// strafe it.
				strafedir = (M_Random()%5 - 2) * (int)ANG45;
			}
		}
	}

	// chase towards target
	if (move_count < 0 || !move_ok)
	{
		NewChaseDir(move_ok);

		move_count = 10 + (M_Random() & 31);
		strafedir = 0;
	}

	Move();
}


void bot_t::Meander()
{
	// TODO : the follow is utter rubbish

	/*
	if (hit_obstacle || move_count < 0)
	{
		NewChaseDir(!hit_obstacle);

		look_angle = angle;
		look_slope = 0.0;

		move_count = 10 + (M_Random() & 31);
		strafedir = 0;
	}

	Move();
	*/
}


void bot_t::Think_Fight()
{
	mobj_t *mo = pl->mo;

fprintf(stderr, "FIGHT %d\n", gametic);

	// Check if we can see the target
	bool seetarget = false;
	if (mo->target)
		seetarget = P_CheckSight(mo, mo->target);

	patience--;
	move_count--;

	// Look for enemies
	// If we aren't confident, gather more than fight.
	if (!seetarget && patience < 0)
	{
		seetarget = false; //!!!  LookForEnemies();

		if (mo->target)
			patience = 20 + (M_Random() & 31) * 4;
	}

	// Can't see a target || don't have a suitable weapon to take it out with?
	if (!seetarget && patience < 0)
	{
		seetarget = false; //!!!  LookForItems();

		if (mo->target)
			patience = 30 + (M_Random() & 31) * 8;
	}

	SYS_ASSERT(mo->target != NULL);

	Chase(seetarget || true, !hit_obstacle);  // FIXME !!!
}


void bot_t::WeaveNearLeader(const mobj_t *leader)
{
	// pick a position some distance away, so that a human player
	// can get out of a narrow item closet (etc).

	float dx = pl->mo->x - leader->x;
	float dy = pl->mo->y - leader->y;

	float dlen = std::max(1.0f, hypotf(dx, dy));

	dx = dx * 96.0f / dlen;
	dy = dy * 96.0f / dlen;

	position_c pos { leader->x + dx, leader->y + dy, leader->z };

	TurnToward(leader, false);
	WeaveToward(pos);
}


void bot_t::PathToLeader()
{
	mobj_t *leader = pl->mo->supportobj;
	SYS_ASSERT(leader);

	DeletePath();

	path = NAV_FindPath(pl->mo, leader, 0);

	if (path != NULL)
	{
		EstimateTravelTime();
	}
}


void bot_t::EstimateTravelTime()
{
	// estimate time to travel one segment of a path.
	// overestimates by quite a bit, to account for obstacles.

	float dist = DistTo(path->cur_dest());
	float tics = dist * 1.5f / 10.0f + 6.0f * TICRATE;

	travel_time = (int)tics;
}


void bot_t::Think_Help()
{
	mobj_t *leader = pl->mo->supportobj;

	// check if we are close to the leader, and can see them
	bool cur_near = false;

	position_c pos = { leader->x, leader->y, leader->z };
	float dist = DistTo(pos);

	// allow a bit of "hysteresis"
	float check_dist = near_leader ? 224.0 : 160.0;

	if (dist < check_dist && fabs(pl->mo->z - pos.z) <= 24.0)
	{
		cur_near = P_CheckSight(pl->mo, leader);
	}

	if (near_leader != cur_near)
	{
		near_leader = cur_near;

		DeletePath();

		if (! cur_near)
		{
			// wait a bit then find a path
			path_wait = 10 + C_Random() % 10;
		}
	}

	if (cur_near)
	{
		WeaveNearLeader(leader);
		return;
	}

	if (path != NULL)
	{
		switch (FollowPath())
		{
			case FOLLOW_OK:
				return;

			case FOLLOW_Done:
				DeletePath();
				path_wait = 4 + C_Random() % 4;
				break;

			case FOLLOW_Failed:
				DeletePath();
				path_wait = 30 + C_Random() % 10;
				break;
		}
	}

	// we are waiting until we can establish a path

	if (path_wait-- < 0)
	{
		PathToLeader();
		path_wait = 30 + C_Random() % 10;
	}

	// if somewhat close, attempt to follow player
	if (dist < 512.0 && fabs(pl->mo->z - pos.z) <= 24.0)
		WeaveNearLeader(leader);
	else
		Meander();
}


bot_follow_path_e  bot_t::FollowPath()
{
	// returns a FOLLOW_XXX enum constant.

	SYS_ASSERT(path != NULL);
	SYS_ASSERT(! path->finished());

	// handle doors and lifts
	int flags = path->nodes[path->along].flags;

	if (flags & PNODE_Door)
	{
		task = TASK_OpenDoor;
		door_stage = TKDOOR_Approach;
		door_seg   = path->nodes[path->along].seg;
		door_time  = 5 * TICRATE;
		SYS_ASSERT(door_seg != NULL);
		return FOLLOW_OK;
	}
	else if (flags & PNODE_Lift)
	{
		task = TASK_UseLift;
		lift_stage = TKLIFT_Approach;
		lift_seg   = path->nodes[path->along].seg;
		lift_time  = 5 * TICRATE;
		SYS_ASSERT(lift_seg != NULL);
		return FOLLOW_OK;
	}
	else if (flags & PNODE_Lift)
	{
		// TODO: TASK_Telport which attempts not to telefrag / be telefragged
	}

	// have we reached the next node?
	if (path->reached_dest(pl->mo))
	{
		path->along += 1;

		if (path->finished())
		{
			return FOLLOW_Done;
		}

		EstimateTravelTime();
	}

	if (travel_time-- < 0)
	{
		return FOLLOW_Failed;
	}

	// determine looking angle
	{
		position_c dest = path->cur_dest();

		if (path->along+1 < path->nodes.size())
			dest = path->nodes[path->along+1].pos;

		float dx = dest.x - pl->mo->x;
		float dy = dest.y - pl->mo->y;
		float dz = dest.z - pl->mo->z;

		angle_t want_angle = R_PointToAngle(0, 0, dx, dy);
		float   want_slope = P_ApproxSlope(dx, dy, dz);

		TurnToward(want_angle, want_slope, false);
	}

	strafedir = 0;

	WeaveToward(path->cur_dest());

	return FOLLOW_OK;
}


void bot_t::Think_Roam()
{
	if (path != NULL)
	{
		switch (FollowPath())
		{
			case FOLLOW_OK:
				return;

			case FOLLOW_Done:
				// arrived at the spot!
				// TODO look for other nearby items

				DeletePath();
				path_wait = 4 + C_Random() % 4;
				break;

			case FOLLOW_Failed:
				DeletePath();
				path_wait = 30 + C_Random() % 10;
				break;
		}
	}

	if (path_wait-- < 0)
	{
		path_wait = 30 + C_Random() % 10;

		if (! NAV_NextRoamPoint(roam_goal))
		{
			roam_goal = position_c { 0, 0, 0 };
			return;
		}

		path = NAV_FindPath(pl->mo, &roam_goal, 0);

		// if no path found, try again soon
		if (path == NULL)
		{
			roam_goal = position_c { 0, 0, 0 };
			return;
		}

		EstimateTravelTime();
	}

	Meander();
}


void bot_t::FinishGetItem()
{
	task = TASK_None;
	pl->mo->SetTracer(NULL);

	DeletePath();
	path_wait = 4 + C_Random() % 4;

	// when fighting, look furthe for more items
	if (pl->mo->target != NULL)
	{
		LookForItems(1024);
		return;
	}

	// otherwise collect nearby items
	LookForItems(256);

	if (task == TASK_GetItem)
		return;

	// continue to follow player
	if (pl->mo->supportobj != NULL)
		return;

	// otherwise we were roaming about, so re-establish path
	if (! (roam_goal.x == 0 && roam_goal.y == 0 && roam_goal.z == 0))
	{
		path = NAV_FindPath(pl->mo, &roam_goal, 0);

		// if no path found, try again soon
		if (path == NULL)
		{
			roam_goal = position_c { 0, 0, 0 };
			return;
		}

		EstimateTravelTime();
	}
}


void bot_t::Think_GetItem()
{
	// item gone?  (either we picked it up, or someone else did)
	if (pl->mo->tracer == NULL)
	{
		FinishGetItem();
		return;
	}

	// if we are being chased, look at them, shoot sometimes
	if (pl->mo->target)
	{
		TurnToward(pl->mo->target, true);

		// FIXME shoot
	}
	else
	{
		TurnToward(pl->mo->tracer, false);
	}

	// follow the path previously found
	if (path != NULL)
	{
		switch (FollowPath())
		{
			case FOLLOW_OK:
				return;

			case FOLLOW_Done:
				DeletePath();
				item_time = TICRATE;
				break;

			case FOLLOW_Failed:
				// took too long? (e.g. we got stuck)
				FinishGetItem();
				return;
		}
	}

	// detect not picking up the item
	if (item_time-- < 0)
	{
		FinishGetItem();
		return;
	}

	// move toward the item's location
	WeaveToward(pl->mo->tracer);
}


void bot_t::FinishDoorOrLift(bool ok)
{
	task = TASK_None;

	if (ok)
	{
		path->along += 1;
	}
	else
	{
		DeletePath();
		roam_goal = position_c { 0, 0, 0 };
	}
}


void bot_t::Think_OpenDoor()
{
	switch (door_stage)
	{
		case TKDOOR_Approach:
		{
			if (door_time-- < 0)
			{
				FinishDoorOrLift(false);
				return;
			}

			float dist   = DistTo(path->cur_dest());
			angle_t ang  = path->nodes[path->along].seg->angle + ANG90;
			angle_t diff = ang - pl->mo->angle;

			if (diff > ANG180)
				diff = ANG_MAX - diff;

			if (diff < ANG5 && dist < (USERANGE - 16))
			{
				door_stage = TKDOOR_Use;
				door_time  = TICRATE * 5;
				return;
			}

			TurnToward(ang, 0.0, false);
			WeaveToward(path->cur_dest());
			return;
		}

		case TKDOOR_Use:
		{
			if (door_time-- < 0)
			{
				FinishDoorOrLift(false);
				return;
			}

			// if closing, try to re-open
			const sector_t *sector = door_seg->back_sub->sector;
			const plane_move_t *pm = sector->ceil_move;

			if (pm != NULL && pm->direction < 0)
			{
				if (door_time & 1)
					cmd.use = true;
				return;
			}

			// already open?
			if (sector->c_h > sector->f_h + 56.0f)
			{
				FinishDoorOrLift(true);
				return;
			}

			// door is opening, so don't interfere
			if (pm != NULL)
				return;

			if (door_time & 1)
				cmd.use = true;

			return;
		}
	}
}


void bot_t::Think_UseLift()
{
	switch (lift_stage)
	{
		case TKDOOR_Approach:
		{
			if (lift_time-- < 0)
			{
				FinishDoorOrLift(false);
				return;
			}

			float dist   = DistTo(path->cur_dest());
			angle_t ang  = path->nodes[path->along].seg->angle + ANG90;
			angle_t diff = ang - pl->mo->angle;

			if (diff > ANG180)
				diff = ANG_MAX - diff;

			if (diff < ANG5 && dist < (USERANGE - 16))
			{
				lift_stage = TKLIFT_Use;
				lift_time  = TICRATE * 5;
				return;
			}

			TurnToward(ang, 0.0, false);
			WeaveToward(path->cur_dest());
			return;
		}

		case TKLIFT_Use:
		{
			if (lift_time-- < 0)
			{
				FinishDoorOrLift(false);
				return;
			}

			// if lift is raising, try to re-lower
			const sector_t *sector = lift_seg->back_sub->sector;
			const plane_move_t *pm = sector->floor_move;

			if (pm != NULL && pm->direction > 0)
			{
				if (lift_time & 1)
					cmd.use = true;
				return;
			}

			// already lowered?
			if (sector->f_h < lift_seg->front_sub->sector->f_h + 24.0f)
			{
				// navigation code added a place to stand
				path->along += 1;

				// TODO compute time it will take for lift to go fully up
				lift_stage = TKLIFT_Ride;
				lift_time  = TICRATE * 10;
				return;
			}

			// lift is lowering, so don't interfere
			if (pm != NULL)
				return;

			// try to activate it
			if (lift_time & 1)
				cmd.use = true;

			return;
		}

		case TKLIFT_Ride:
			if (lift_time-- < 0)
			{
				FinishDoorOrLift(false);
				return;
			}

			WalkToward(path->cur_dest());

			const sector_t *lift_sec = lift_seg->back_sub->sector;

			if (lift_sec->floor_move != NULL)
			{
				// if lift went down again, don't time out
				if (lift_sec->floor_move->direction <= 0)
					lift_time = 10 * TICRATE;

				return;
			}

			// reached the top?
			bool ok = pl->mo->z > (lift_sec->f_h - 0.5);

			FinishDoorOrLift(ok);
			return;
	}
}


void bot_t::DeletePath()
{
	if (path != NULL)
	{
		delete path;
		path = NULL;
	}
}

//----------------------------------------------------------------------------

void bot_t::Think()
{
	SYS_ASSERT(pl != NULL);
	SYS_ASSERT(pl->mo != NULL);

	// initialize the botcmd_t
	memset(&cmd, 0, sizeof(botcmd_t));
	cmd.weapon = -1;

	// do nothing when game is paused
	if (paused)
		return;

	mobj_t *mo = pl->mo;

	// dead?
	if (mo->health <= 0)
	{
		DeathThink();
		return;
	}

	// forget target (etc) if they died
	if (mo->target && mo->target->health <= 0)
		mo->SetTarget(NULL);

	if (mo->supportobj && mo->supportobj->health <= 0)
		mo->SetSupportObj(NULL);

	// hurt by somebody?
	if (pl->attacker != NULL)
	{
		PainResponse();
	}

	DetectObstacle();

	// doing a task?
	switch (task)
	{
		case TASK_GetItem:
			Think_GetItem();
			return;

		case TASK_OpenDoor:
			Think_OpenDoor();
			return;

		case TASK_UseLift:
			Think_UseLift();
			return;

		default:
			break;
	}

	LookAround();

	if (weapon_time-- < 0)
		SelectWeapon();

	// if we have a target enemy, fight it or flee it
	if (pl->mo->target != NULL)
	{
		Think_Fight();
		return;
	}

	// if we have a leader (in co-op), follow them
	if (pl->mo->supportobj != NULL)
	{
		Think_Help();
		return;
	}

	// in deathmatch, go to the roaming goal.
	// otherwise just meander around.

	Think_Roam();
}


void bot_t::DeathThink()
{
	dead_time++;

	// respawn after a random interval, at least one second
	if (dead_time > 30)
	{
		dead_time = 0;

		if (C_Random() % 100 < 35)
			cmd.use = true;
	}
}


void bot_t::ConvertTiccmd(ticcmd_t *dest)
{
	// we assume caller has cleared the ticcmd_t to zero.

	mobj_t *mo = pl->mo;

	if (cmd.attack)
		dest->buttons |= BT_ATTACK;

	if (cmd.attack2)
		dest->extbuttons |= EBT_SECONDATK;

	if (cmd.use)
		dest->buttons |= BT_USE;

	if (cmd.jump)
		dest->upwardmove = 0x20;

	if (cmd.weapon != -1)
	{
		dest->buttons |= BT_CHANGE;
		dest->buttons |= (cmd.weapon << BT_WEAPONSHIFT) & BT_WEAPONMASK;
	}

	dest->player_idx = pl->pnum;

	dest->angleturn = (mo->angle - look_angle) >> 16;
	dest->mlookturn = (M_ATan(look_slope) - mo->vertangle) >> 16;

	if (cmd.speed != 0)
	{
		// get angle relative the player.
		angle_t a = cmd.direction - look_angle;

		float fwd  = M_Cos(a) * cmd.speed;
		float side = M_Sin(a) * cmd.speed;

		dest->forwardmove =  (int)fwd;
		dest->sidemove    = -(int)side;
	}
}


void bot_t::Respawn()
{
	// TODO in a few tics, do a NAV_FindThing to find a weapon

	task = TASK_None;

	path_wait   = C_Random() % 8;
	look_time   = C_Random() % 8;
	weapon_time = C_Random() % 8;

	hit_obstacle = false;
	near_leader  = false;
	roam_goal    = position_c { 0, 0, 0 };

	DeletePath();
}


void bot_t::EndLevel()
{
	DeletePath();
}

//----------------------------------------------------------------------------

//
// Converts the player (which should be empty, i.e. neither a network
// or console player) to a bot.  Recreate is true for bot players
// loaded from a savegame.
//
void P_BotCreate(player_t *p, bool recreate)
{
	bot_t *bot = new bot_t;

	bot->pl = p;

	p->builder = P_BotPlayerBuilder;
	p->build_data = (void *)bot;
	p->playerflags |= PFL_Bot;

	if (! recreate)
		sprintf(p->playername, "Bot%d", p->pnum + 1);
}


void P_BotPlayerBuilder(const player_t *p, void *data, ticcmd_t *cmd)
{
	memset(cmd, 0, sizeof(ticcmd_t));

	if (gamestate != GS_LEVEL)
		return;

	bot_t *bot = (bot_t *)data;
	SYS_ASSERT(bot);

	bot->Think();
	bot->ConvertTiccmd(cmd);
}


void BOT_BeginLevel(void)
{
	if (numbots > 0)
		NAV_AnalyseLevel();
}


//
// Done at level shutdown, right after all mobjs have been removed.
// Erases anything level specific from the bot structs.
//
void BOT_EndLevel(void)
{
	for (int i = 0 ; i < MAXPLAYERS ; i++)
	{
		player_t *pl = players[i];
		if (pl != NULL && pl->isBot())
		{
			bot_t *bot = (bot_t *)pl->build_data;
			SYS_ASSERT(bot);

			bot->EndLevel();
		}
	}

	NAV_FreeLevel();
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
