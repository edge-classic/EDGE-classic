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


bool bot_t::MeleeWeapon() const
{
	int wp_num = pl->ready_wp;

	if (pl->pending_wp >= 0)
		wp_num = pl->pending_wp;

	return pl->weapons[wp_num].info->ammo[0] == AM_NoAmmo;
}


/* TODO sort all this out

static float NAV_EvaluateHealth(const mobj_t *mo)
{
	for (const benefit_t *B = mo->info->pickup_benefits ; B != NULL ; B = B->next)
	{
		if (B->type == BENEFIT_Health)
			return B->amount;
	}

	return -1;
}

*/


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
	// The following must be true to justify that you attack a target:
	// - target may not be yourself or your support obj.
	// - target must either want to attack you, or be on a different side
	// - target may not have the same supportobj as you.
	// - You must be able to see and shoot the target.

	if (0 == (mo->flags & MF_SHOOTABLE) || mo->health <= 0)
		return -1;

	// occasionally shoot barrels
	if (IsBarrel(mo))
		return (C_Random() % 100 < 10) ? +1 : -1;

	if (0 == (mo->extendedflags & EF_MONSTER) && ! mo->player)
		return -1;

	if (mo->player && mo->player == pl)
		return -1;

	if (pl->mo->supportobj == mo)
		return -1;

	if (COOP_MATCH() && mo->player)
		return -1;

	if (COOP_MATCH() && mo->supportobj && mo->supportobj->player)
		return -1;

	// EXTERMINATE !!

	return 1.0;
}


float bot_t::EvalItem(const mobj_t *mo)
{
	// FIXME EvalThing
	return -1;
}


int bot_t::EvaluateWeapon(int w_num) const
{
	// this evaluates weapons owned by the bot (NOT ones in the map).

	playerweapon_t *wp = pl->weapons + w_num;

	// don't have this weapon
	if (! wp->owned)
		return -9999;

	weapondef_c *weapon = wp->info;
	SYS_ASSERT(weapon);

	atkdef_c *attack = weapon->attack[0];
	if (!attack)
		return -9999;

	// have enough ammo?
	if (weapon->ammo[0] != AM_NoAmmo)
	{
		if (pl->ammo[weapon->ammo[0]].num < weapon->ammopershot[0])
			return -9999;
	}

	float value = 64 * attack->damage.nominal;

	switch (attack->attackstyle)
	{
		case ATK_SHOT:
			value *= attack->count;
			break;

		case ATK_CLOSECOMBAT:
			if (pl->powers[PW_Berserk])
				value /= 10;
			else
				value /= 20;
			break;

		case ATK_PROJECTILE:
		case ATK_SMARTPROJECTILE:
			value += 256 * attack->atk_mobj->explode_damage.nominal;
			value *= attack->atk_mobj->speed / 20;
			break;

		default:
			// for everything else, no change
			break;
	}

	value -= weapon->ammopershot[0] * 8;

	if (w_num == pl->ready_wp || w_num == pl->pending_wp)
		value += 1024;

	value += (M_Random() - 128) * 16;

	return (int)value;
}

//----------------------------------------------------------------------------

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


int bot_t::CalcConfidence()
{
	const mobj_t *mo = pl->mo;

	if (pl->powers[PW_Invulnerable] > 0)
		return +1;

	// got enough health?
	if (mo->health < mo->info->spawnhealth * 0.33)
		return -1;

	if (mo->health < mo->info->spawnhealth * 0.66)
		return 0;

	if (MeleeWeapon())
		return 0;

	// got enough ammo?
	ammotype_e ammo = pl->weapons[pl->ready_wp].info->ammo[0];

	if (pl->ammo[ammo].num < pl->ammo[ammo].max / 2)
		return 0;

	return 1;
}


void bot_t::PainResponse()
{
	// oneself?
	if (pl->attacker == pl->mo)
		return;

	// ignore friendly fire -- shit happens
	if (COOP_MATCH() && pl->attacker->player)
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


/* OLD STUFF, REMOVE SOON
static bool PTR_BotLook(intercept_t * in, void *dataptr)
{
	SYS_ASSERT(mo);

	if (! (mo->flags & MF_SPECIAL))
		return true;  // has to be able to be got

	if (mo->health <= 0)
		return true;  // already been picked up

	int score = 0;  //--  looking_bot->EvaluateItem(mo);

	if (score <= 0)
		return true;

	if (!lkbot_target || score > lkbot_score)
	{
		lkbot_target = mo;
		lkbot_score  = score;
	}

	return false;  // found something
}
*/


// Finds items for the bot to get.

/* TODO merge logic into new system

bool bot_t::LookForItems()
{
	mobj_t *best_item  = NULL;
	int     best_score = 0;

	int search = 6;

	// If we are confident, hunt more than gather.
	if (confidence > 0)
		search = 1;

	// Find some stuff!
	for (int i = -search; i <= search; i++)
	{
		angle_t diff = (int)(ANG45/4) * i;

		LineOfSight(angle + diff);

		if (lkbot_target)
		{
			if (!best_item || lkbot_score > best_score)
			{
				best_item  = lkbot_target;
				best_score = lkbot_score;
			}
		}
	}

	if (best_item)
	{
#if (DEBUG > 0)
I_Printf("BOT %d: WANT item %s, score %d\n",
	pl->pnum, best_item->info->name.c_str(), best_score);
#endif

		pl->mo->SetTarget(best_item);
		return true;
	}

	return false;
}


// Based on P_LookForTargets from p_enemy.c
bool bot_t::LookForEnemies()
{
	mobj_t *we = pl->mo;

	for (mobj_t *them = mobjlisthead ; them != NULL ; them = them->next)
	{
		if (! (them->flags & MF_SHOOTABLE))
			continue;

		if (them == we)
			continue;

		// only target monsters or players (not barrels)
		if (! (them->extendedflags & EF_MONSTER) && ! them->player)
			continue;

		bool same_side = ((them->side & we->side) != 0);

		if (them->player && same_side &&
			! we->supportobj && them->supportobj != we)
		{
			if (them->supportobj && P_CheckSight(we, them->supportobj))
			{
				we->SetSupportObj(them->supportobj);
				return true;
			}
			else if (P_CheckSight(we, them))
			{
				we->SetSupportObj(them->supportobj);
				return true;
			}
		}

		// The following must be true to justify that you attack a target:
		// 1. The target may not be yourself or your support obj.
		// 2. The target must either want to attack you, or be on a different side
		// 3. The target may not have the same supportobj as you.
		// 4. You must be able to see and shoot the target.

		if (!same_side || (them->target && them->target == we))
		{
			if (P_CheckSight(we, them))
			{
				pl->mo->SetTarget(them);
#if (DEBUG > 0)
I_Printf("BOT %d: Targeting Agent: %s\n", bot->pl->pnum, them->info->name.c_str());
#endif
				return true;
			}
		}
	}

	return false;
}
*/


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


void bot_t::LookForEnemies()
{
	// TODO check sight of existing target
	//      [ if too many checks, lose patience ]

	if (pl->mo->target == NULL)
	{
		mobj_t * enemy = NAV_FindEnemy(this, 768);

		if (enemy != NULL)
		{
			// sight check, since enemy may be on other side of a wall
			if (P_CheckSight(pl->mo, enemy))
				pl->mo->SetTarget(enemy);
		}
	}
}


void bot_t::LookAround()
{
	look_time--;

	if (look_time == 10 || look_time == 20)
		LookForLeader();

	if (look_time & 1)
	{
		LookForEnemies();
		return;
	}

	if (look_time >= 0)
		return;

	// look for items every second or so
	look_time = 30 + C_Random() % 10;

	// FIXME decide what we want most (e.g. health, etc).

	mobj_t * item = NULL;
	bot_path_c * path = NAV_FindThing(this, 1024, item);

	// FIXME
	if (path != NULL)
		delete path;
}


void bot_t::SelectWeapon()
{
	int best = pl->ready_wp;
	int best_val = -9999;

	for (int i=0 ; i < MAXWEAPONS ; i++)
	{
		int val = EvaluateWeapon(i);

		if (val > best_val)
		{
			best = i;
			best_val = val;
		}
	}

	if (best != pl->ready_wp && best != pl->pending_wp)
	{
		cmd.new_weapon = best;
	}
}


void bot_t::Move()
{
	cmd.move_speed = MOVE_SPEED;
	cmd.move_angle = angle + strafedir;
}


void bot_t::MoveToward(const position_c& pos)
{
	cmd.move_speed = MOVE_SPEED;
	cmd.move_angle = R_PointToAngle(pl->mo->x, pl->mo->y, pos.x, pos.y);
}


void bot_t::TurnToward(angle_t want_angle, float want_slope)
{
	// horizontal (yaw) angle
	angle_t delta = want_angle - pl->mo->angle;

	if (delta < ANG180)
		delta = delta / 8;
	else
		delta = ANG_MAX - (ANG_MAX - delta) / 8;

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


void bot_t::TurnToward(const mobj_t *mo)
{
	float dx = mo->x - pl->mo->x;
	float dy = mo->y - pl->mo->y;
	float dz = mo->z - pl->mo->z;

	angle_t want_angle = R_PointToAngle(0, 0, dx, dy);
	float   want_slope = P_ApproxSlope(dx, dy, dz);

	TurnToward(want_angle, want_slope);
}


void bot_t::WeaveToward(const position_c& pos)
{
	// usually try to move directly toward a wanted position.
	// but if something gets in our way, we try to "weave" around it,
	// by sometimes going diagonally left and sometimes right.

	if (hit_obstacle && weave == 0)
	{
		weave = (C_Random() & 1) ? -1 : +1;
		weave_time = 30 + C_Random() % 30;
	}

	if (weave_time-- < 0)
	{
		if (! hit_obstacle)
			weave = 0;
		else
			weave = -weave;

		weave_time = 30 + C_Random() % 60;
	}

	MoveToward(pos);

	if (weave < 0) cmd.move_angle -= ANG5 * 12;
	if (weave > 0) cmd.move_angle += ANG5 * 12;
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
		TurnToward(mo->target);

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
	if (hit_obstacle || move_count < 0)
	{
		NewChaseDir(!hit_obstacle);

		move_count = 10 + (M_Random() & 31);
		strafedir = 0;
	}

	Move();
}


void bot_t::Think_Fight()
{
	mobj_t *mo = pl->mo;

	// Check if we can see the target
	bool seetarget = false;
	if (mo->target)
		seetarget = P_CheckSight(mo, mo->target);

	// Select a suitable weapon
	if (confidence <= 0)
	{
		weapon_count--;
		if (weapon_count < 0)
		{
			SelectWeapon();
			weapon_count = 30 + (M_Random() & 31) * 4;
		}
	}

	patience--;
	move_count--;

	// Look for enemies
	// If we aren't confident, gather more than fight.
	if (!seetarget && patience < 0 && confidence >= 0)
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


void bot_t::WeaveBehindLeader(const mobj_t *leader)
{
	// pick a position behind the leader, and a bit to the side

	position_c pos = { leader->x, leader->y, leader->z };

	float dx = M_Cos(leader->angle);
	float dy = M_Sin(leader->angle);

	pos.x -= dx * 64.0;
	pos.y -= dy * 64.0;

	if (pl->pnum & 1)
	{
		dx = -dx;
		dy = -dy;
	}

	pos.x += dy * 32.0;
	pos.y += dx * 32.0;

	WeaveToward(pos);
}


void bot_t::Think_Help()
{
	mobj_t *leader = pl->mo->supportobj;

	if (path != NULL)
	{
		if (roam_count-- < 0)
			DeletePath();
		else
			FollowPath();

		return;
	}

	position_c pos = { leader->x, leader->y, leader->z };

	float dist = R_PointToDist(pl->mo->x, pl->mo->y, pos.x, pos.y);

	// check if we are close to the leader, and can see them
	bool near_them = false;

	if (dist < 512.0 && fabs(pl->mo->z - pos.z) <= 24.0)
	{
		near_them = P_CheckSight(pl->mo, leader);
	}

	if (near_them)
	{
		DeletePath();
		WeaveBehindLeader(leader);
		return;
	}

	// leader is too far away, find a path...
	// FIXME only do this every second or so!!

	path = NAV_FindPath(pl->mo, leader, 0);
	if (path != NULL)
	{
		roam_goal  = pos;
		roam_count = 30 * TICRATE;
		return;
	}

	// IDEA: try again a few times, then teleport somewhere nearby

	// TODO
	// Meander()

fprintf(stderr, "cannot find leader: %d\n", gametic);
}


/* USEFUL??
{
	// for thin subsectors, do a check with expanded bbox
	// FIXME make a utility of NAV_XXX
	float x1 = dest->bbox[BOXLEFT];
	float x2 = dest->bbox[BOXRIGHT];
	float y1 = dest->bbox[BOXBOTTOM];
	float y2 = dest->bbox[BOXTOP];

	bool x_narrow = (x2 - x1) < 24;
	bool y_narrow = (y2 - y1) < 24;

	if (x_narrow || y_narrow)
	{
		// have to be at same height, or higher
		if (pl->mo->z > dest->sector->f_h - 0.5)
		{
			float w = (x2 - x1);
			float h = (y2 - y1);

			float mx = (x1 + x2) * 0.5;
			float my = (y1 + y2) * 0.5;

			if (x_narrow) w = 24;
			if (y_narrow) h = 24;

			w *= 0.5;
			h *= 0.5;

			if ((mx - w <= pl->mo->x) && (pl->mo->x <= mx + w) &&
				(my - h <= pl->mo->y) && (pl->mo->y <= my + h))
			{
				reached = true;
			}
		}
	}
*/


bool bot_t::FollowPath()
{
	// returns TRUE when reached the end.

	SYS_ASSERT(path != NULL);
	SYS_ASSERT(! path->finished());

	// have we reached the next node?
	while (path->reached_dest(pl->mo))
	{
		path->along += 1;

		if (path->finished())
		{
			DeletePath();
			return true;
		}

		path_point = path->cur_dest();
	}

	// determine looking angle

	/* FIXME !!!
	{
		//-- int dest_id = (d2 < 0) ? d1 : d2;
		//-- const subsector_t *dest = &subsectors[dest_id];

		float dest_x = (dest->bbox[BOXLEFT] + dest->bbox[BOXRIGHT])  * 0.5;
		float dest_y = (dest->bbox[BOXTOP]  + dest->bbox[BOXBOTTOM]) * 0.5;

		float dx = dest_x - pl->mo->x;
		float dy = dest_y - pl->mo->y;
		float dz = dest->sector->f_h - pl->mo->z;

		angle_t want_angle = R_PointToAngle(0, 0, dx, dy);
		float   want_slope = P_ApproxSlope(dx, dy, dz);

		TurnToward(want_angle, want_slope);
	}
	*/

	strafedir = 0;

	WeaveToward(path_point);

	return false;  // not finished
}


void bot_t::Think_Roam()
{
	if (roam_count-- < 0)
	{
		roam_count = TICRATE;

		if (path != NULL)
			DeletePath();

		if (! NAV_NextRoamPoint(roam_goal))
			return;

		//-- subsector_t *dest = R_PointInSubsector(roam_goal.x, roam_goal.y);

		path = NAV_FindPath(pl->mo, &roam_goal, 0);
		if (path == NULL)
		{
			// try again soon
			return;
		}

		path_point = path->cur_dest();

		// after this amount of time, give up and try another place
		// [ FIXME check if needed, PERHAPS detect lack of progress ]
		roam_count = 20 * TICRATE;
	}

	if (path == NULL)
	{
		// TODO meander
		return;
	}

	if (! FollowPath())
		return;

	// arrived at the spot!
	// TODO look for other nearby items

	fprintf(stderr, "ARRIVED !! %d\n", gametic);
	roam_count = 0;
}


void bot_t::Think_GetItem()
{
	// item gone?  (either we picked it up, or someone else did)
	if (pl->mo->tracer == NULL)
	{
		task = TASK_None;
		return;
	}

	// took too long? (e.g. we got stuck)
	if (roam_count-- < 0)
	{
		DeletePath();
		task = TASK_None;
		return;
	}

	// if we are being chased, look at them, shoot sometimes
	if (pl->mo->target)
	{
		TurnToward(pl->mo->target);

		// FIXME shoot
	}
	else
	{
		TurnToward(pl->mo->tracer);
	}

	// follow the path previously found
	if (path != NULL)
	{
		FollowPath();
		return;
	}

	// move toward the item's location
	WeaveToward(pl->mo->tracer);

	// TODO : REVIEW FOLLOWING LOGIC

/*
	// Got a special (item) target?
	if (mo->target->flags & MF_SPECIAL)
	{
		// have we stopped needed it? (maybe it is old DM and we
		// picked up the weapon).
//--		if (EvaluateItem(mo->target) <= 0)
//--		{
//--			pl->mo->SetTarget(NULL);
//--			return;
//--		}

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
		else if (move_count < 0)
		{
			// Move in the direction of the item.
			angle = R_PointToAngle(mo->x, mo->y, mo->target->x, mo->target->y);
			strafedir = 0;

			move_count = 10 + (M_Random() & 31);
		}

		Move();
		return;
	}
*/
}


void bot_t::Think_OpenDoor()
{
	// TODO
}


void bot_t::Think_UseLift()
{
	// TODO
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
	cmd.new_weapon = -1;

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

	confidence = CalcConfidence();

	// hurt by somebody?
	if (pl->attacker != NULL)
	{
		PainResponse();
	}

	LookAround();

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
	dead_count++;

	// respawn after a random interval, at least one second
	if (dead_count > 30)
	{
		dead_count = 0;

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

	if (cmd.second_attack)
		dest->extbuttons |= EBT_SECONDATK;

	if (cmd.use)
		dest->buttons |= BT_USE;

	if (cmd.jump)
		dest->upwardmove = 0x20;

	if (cmd.new_weapon != -1)
		dest->buttons |= (cmd.new_weapon << BT_WEAPONSHIFT) & BT_WEAPONMASK;

	dest->player_idx = pl->pnum;

	dest->angleturn = (mo->angle - look_angle) >> 16;
	dest->mlookturn = (M_ATan(look_slope) - mo->vertangle) >> 16;

	if (cmd.move_speed != 0)
	{
		// get angle relative the player.
		angle_t a = cmd.move_angle - look_angle;

		float fwd  = M_Cos(a) * cmd.move_speed;
		float side = M_Sin(a) * cmd.move_speed;

		dest->forwardmove =  (int)fwd;
		dest->sidemove    = -(int)side;
	}
}


void bot_t::Respawn()
{
	// TODO in a few tics, do a NAV_FindThing to find a weapon

	task = TASK_None;

	roam_count = C_Random() % 8;
	look_time  = C_Random() % 8;

	hit_obstacle = false;

	NAV_NextRoamPoint(roam_goal);

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
