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

#include <limits.h>

#include "con_main.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "g_game.h"
#include "m_random.h"
#include "p_bot.h"
#include "p_local.h"
#include "p_weapon.h"
#include "p_action.h"
#include "rad_trig.h"
#include "r_state.h"
#include "s_sound.h"
#include "w_wad.h"
#include "z_zone.h"

#define DEBUG  0


int bot_skill = 1;  // range is 0 to 2


static int strafe_chances[3] = { 128, 192, 256 };
static int attack_chances[3] = {  32,  64, 160 };
static int    move_speeds[3] = {  36,  45,  50 };


static bot_t *looking_bot;
static mobj_t *lkbot_target;
static int lkbot_score;

static void BOT_SetTarget(bot_t *bot, mobj_t *target)
{
	bot->pl->mo->SetTarget(target);
}

static bool BOT_HasWeapon(bot_t *bot, benefit_t *benefit)
{
	for (int i=0; i < MAXWEAPONS; i++)
	{
		if (bot->pl->weapons[i].owned &&
			bot->pl->weapons[i].info == benefit->sub.weap)
		{
			return true;
		}
	}

	return false;
}

static bool BOT_MeleeWeapon(bot_t *bot)
{
	int wp_num = bot->pl->ready_wp;

	if (bot->pl->pending_wp >= 0)
		wp_num = bot->pl->pending_wp;

	return bot->pl->weapons[wp_num].info->ammo[0] == AM_NoAmmo;
}

static void BOT_NewChaseDir(bot_t * bot, bool move_ok)
{
	mobj_t *mo = bot->pl->mo;

	// FIXME: This is not very intelligent...
	int r = M_Random();

	if (mo->target && (r % 3 == 0))
	{
		bot->angle = R_PointToAngle(mo->x, mo->y,
			mo->target->x, mo->target->y);
	}
	else if (mo->supportobj && (r % 3 == 1))
	{
		bot->angle = R_PointToAngle(mo->x, mo->y,
			mo->supportobj->x, mo->supportobj->y);
	}
	else if (move_ok)
	{
		angle_t diff = r - M_Random();

		bot->angle += (diff << 21);
	}
	else
		bot->angle = (r << 24);

}

static void BOT_Confidence(bot_t * bot)
{
	const player_t *p = bot->pl;
	const mobj_t *mo = p->mo;

	bot->confidence = 0;

	if (p->powers[PW_Invulnerable])
		bot->confidence = 1;
	else if (mo->health < mo->info->spawnhealth / 3)
		bot->confidence = -1;
	else if (mo->health > mo->info->spawnhealth * 3 / 4 && ! BOT_MeleeWeapon(bot))
	{
        ammotype_e ammo = p->weapons[p->ready_wp].info->ammo[0];

		if (p->ammo[ammo].num > p->ammo[ammo].max / 2)
			bot->confidence = 1;
	}
}

static int BOT_EvaluateWeapon(bot_t *bot, int w_num)
{
	player_t *p = bot->pl;
	playerweapon_t *wp = p->weapons + w_num;

	// don't have this weapon
	if (! wp->owned)
		return INT_MIN;

	weapondef_c *weapon = wp->info;
	SYS_ASSERT(weapon);

	atkdef_c *attack = weapon->attack[0];
	if (!attack)
		return INT_MIN;

	// Don't have enough ammo
	if (weapon->ammo[0] != AM_NoAmmo)
	{
		if (p->ammo[weapon->ammo[0]].num < weapon->ammopershot[0])
			return INT_MIN;
	}

	float value = 64 * attack->damage.nominal;

	switch (attack->attackstyle)
	{
		case ATK_CLOSECOMBAT:
			if (p->powers[PW_Berserk])
				value /= 10;
			else
				value /= 20;
			break;

		case ATK_SMARTPROJECTILE:
			value *= 2;
		case ATK_PROJECTILE:
			value += 256 * attack->atk_mobj->explode_damage.nominal;
			value *= attack->atk_mobj->speed / 20;
			break;

		case ATK_SPAWNER:
			value *= 2;
			break;

		case ATK_TRIPLESPAWNER:
			value *= 6;
			break;

		case ATK_SHOT:
			value *= attack->count;
			break;

		default:  // do nothing ??
			break;
	}

	value -= weapon->ammopershot[0] * 8;

	if (w_num == p->ready_wp || w_num == p->pending_wp)
		value += 1024;

	value += (M_Random() - 128) * 16;

	return (int)value;
}

static int BOT_EvaluateItem(bot_t *bot, mobj_t *mo)
{
	player_t *pl = bot->pl;

	int score = 0;
	int ammo;

	for (benefit_t *list = mo->info->pickup_benefits;
		list != NULL; list=list->next)
	{
		switch (list->type)
		{
			case BENEFIT_Weapon:
				if (! BOT_HasWeapon(bot, list))
					score = score + 50;
				else if (numplayers > 1 && deathmatch != 2 && !(mo->flags & MF_DROPPED))
					// cannot pick up in CO-OP or OLD DeathMatch
					score = -999;
				break;

			case BENEFIT_Ammo:
				ammo = list->sub.type;
				if (ammo != AM_NoAmmo && pl->ammo[ammo].num < pl->ammo[ammo].max)
					score = score + 10;
				break;

			case BENEFIT_Health:
				if (bot->confidence < 0)
					score = score + 40 + (int)list->amount;
				else if (pl->health < list->limit)
					score = score + 20;
				break;

			case BENEFIT_Armour:
				score = score + 5 + (int)list->amount / 10;
				break;

			case BENEFIT_Powerup:
				switch (list->sub.type)
				{
					case PW_Invulnerable: score = 18; break;
					case PW_PartInvis:    score = 14; break;
					case PW_Berserk:      score = 12; break;
					case PW_AcidSuit:     score = 11; break;

					default: break;
				}
				break;
			
			default: break;
		}
	}

	return score;
}

static bool PTR_BotLook(intercept_t * in, void *dataptr)
{
	if (in->line)
	{
		line_t *ld = in->line;

		if (! (ld->flags & MLF_TwoSided))
			return false;  // stop

		// Crosses a two sided line.
		// A two sided line will restrict
		// the possible target ranges.
		// -AJA- 1999/07/19: Gaps are now stored in line_t.

		if (ld->gap_num == 0)
			return false;  // stop

		if (fabs(ld->frontsector->f_h - ld->backsector->f_h) > 24.0f)
			return false;

		return true;  // sight continues
	}

	mobj_t *mo = in->thing;

	SYS_ASSERT(mo);

	if (! (mo->flags & MF_SPECIAL))
		return true;  // has to be able to be got

	if (mo->health <= 0)
		return true;  // already been picked up

	int score = BOT_EvaluateItem(looking_bot, mo);

	if (score <= 0)
		return true;

	if (!lkbot_target || score > lkbot_score)
	{
		lkbot_target = mo;
		lkbot_score  = score;
	}

	return false;  // found something
}

static void BOT_LineOfSight(bot_t *bot, angle_t angle)
{
	looking_bot = bot;

	lkbot_target = NULL;
	lkbot_score  = 0;

	float x1 = bot->pl->mo->x;
	float y1 = bot->pl->mo->y;
	float x2 = x1 + 1024 * M_Cos(angle);
	float y2 = y1 + 1024 * M_Sin(angle);

	P_PathTraverse(x1, y1, x2, y2, PT_ADDLINES | PT_ADDTHINGS, PTR_BotLook);

	looking_bot = NULL;
}

// Finds items for the bot to get.
static bool BOT_LookForItems(bot_t *bot)
{
	mobj_t *best_item  = NULL;
	int     best_score = 0;

	int search = 6;

	// If we are confident, hunt more than gather.
	if (bot->confidence > 0)
		search = 1;

	// Find some stuff!
	for (int i = -search; i <= search; i++)
	{
		angle_t diff = (int)(ANG45/4) * i;

		BOT_LineOfSight(bot, bot->angle + diff);
			
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
		BOT_SetTarget(bot, best_item);

#if (DEBUG > 0)
I_Printf("BOT %d: WANT item %s, score %d\n",
bot->pl->pnum, best_item->info->name.c_str(), best_score);
#endif

		return true;
	}

	return false;
}

// Based on P_LookForTargets from p_enemy.c
static bool BOT_LookForEnemies(bot_t *bot)
{
	mobj_t *we = bot->pl->mo;
	mobj_t *them;

	for (them = mobjlisthead; them; them = them->next)
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
			!we->supportobj && them->supportobj != we)
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
				BOT_SetTarget(bot, them);
#if (DEBUG > 0)
I_Printf("BOT %d: Targeting Agent: %s\n", bot->pl->pnum,
them->info->name.c_str());
#endif
				return true;
			}
		}
	}

	return false;
}

static void BOT_SelectWeapon(bot_t *bot)
{
	int best = bot->pl->ready_wp;
	int best_val = INT_MIN;

	for (int i=0; i < MAXWEAPONS; i++)
	{
		int val = BOT_EvaluateWeapon(bot, i);

		if (val > best_val)
		{
			best = i;
			best_val = val;
		}
	}

	if (best != bot->pl->ready_wp &&
		best != bot->pl->pending_wp)
	{
		bot->cmd.new_weapon = best;
	}
}

static void BOT_Move(bot_t *bot)
{
	bot->cmd.move_speed = move_speeds[bot_skill];
	bot->cmd.move_angle = bot->angle + bot->strafedir;
}

static void BOT_Chase(bot_t *bot, bool seetarget, bool move_ok)
{
	mobj_t *mo = bot->pl->mo;

#if (DEBUG > 1)
		I_Printf("BOT %d: Chase %s dist %1.1f angle %1.0f | %s\n",
			bot->pl->pnum, mo->target->info->name.c_str(),
			P_ApproxDistance(mo->x - mo->target->x, mo->y - mo->target->y),
			ANG_2_FLOAT(bot->angle), move_ok ? "move_ok" : "NO_MOVE");
#endif

	// Got a special (item) target?
	if (mo->target->flags & MF_SPECIAL)
	{
		// have we stopped needed it? (maybe it is old DM and we
		// picked up the weapon).
		if (BOT_EvaluateItem(bot, mo->target) <= 0)
		{
			BOT_SetTarget(bot, NULL);
			return;
		}

		// If there is a wall or something in the way, pick a new direction.
		if (!move_ok || bot->move_count < 0)
		{
			if (seetarget && move_ok)
				bot->cmd.face_mobj = mo->target;

			bot->angle = R_PointToAngle(mo->x, mo->y, mo->target->x, mo->target->y);
			bot->strafedir = 0;
			bot->move_count = 10 + (M_Random() & 31);

			if (!move_ok)
			{
				int r = M_Random();

				if (r < 10)
					bot->angle = M_Random() << 24;
				else if (r < 60)
					bot->strafedir = ANG90;
				else if (r < 110)
					bot->strafedir = ANG270;
			}
		}
		else if (bot->move_count < 0)
		{
			// Move in the direction of the item.
			bot->angle = R_PointToAngle(mo->x, mo->y, mo->target->x, mo->target->y);
			bot->strafedir = 0;

			bot->move_count = 10 + (M_Random() & 31);
		}

		BOT_Move(bot);
		return;
	}

	// Target can be killed.
	if (mo->target->flags & MF_SHOOTABLE)
	{
		// Can see a target,
		if (seetarget)
		{
			// So face it,
			bot->angle = R_PointToAngle(mo->x, mo->y,
				mo->target->x, mo->target->y);

			bot->cmd.face_mobj = mo->target;
			// Shoot it,
			bot->cmd.attack = M_Random() < attack_chances[bot_skill];

			if (bot->move_count < 0)
			{
				bot->move_count = 20 + (M_Random() & 63);
				bot->strafedir = 0;

				if (BOT_MeleeWeapon(bot))
				{
					// run directly toward target
				}
				else if (M_Random() < strafe_chances[bot_skill])
				{
					// strafe it.
					bot->strafedir = (M_Random()%5 - 2) * (int)ANG45;
				}
			}
		}

		// chase towards target
		if (bot->move_count < 0 || !move_ok)
		{
			BOT_NewChaseDir(bot, move_ok);

			bot->move_count = 10 + (M_Random() & 31);
			bot->strafedir = 0;
		}

		BOT_Move(bot);
		return;
	}

	// Target died
	BOT_SetTarget(bot, NULL);
}

static void BOT_Think(bot_t * bot)
{
	SYS_ASSERT(bot->pl);
	SYS_ASSERT(bot->pl->mo);

	mobj_t *mo = bot->pl->mo;

	// press USE button sometimes (open doors, respawn)
	bot->use_count--;
	if (bot->use_count < 0)
	{
		bot->cmd.use = true;
		bot->use_count = 30 + M_Random()/2;
	}

	// Dead?
	if (mo->health <= 0)
		return;

	BOT_Confidence(bot);

	float move_dx = bot->last_x - mo->x;
	float move_dy = bot->last_y - mo->y;

	bot->last_x = mo->x;
	bot->last_y = mo->y;
	
	bool move_ok = (move_dx*move_dx + move_dy*move_dy) > 0.2;

	// Check if we can see the target
	bool seetarget = false;
	if (mo->target)
		seetarget = P_CheckSight(mo, mo->target);
	
	// Select a suitable weapon
	if (bot->confidence <= 0)
	{
		bot->weapon_count--;
		if (bot->weapon_count < 0)
		{
			BOT_SelectWeapon(bot);
			bot->weapon_count = 30 + (M_Random() & 31) * 4;
		}
	}

	bot->patience--;
	bot->move_count--;

	// Look for enemies
	// If we aren't confident, gather more than fight.
	if (!seetarget && bot->patience < 0 && bot->confidence >= 0)
	{
		seetarget = BOT_LookForEnemies(bot);

		if (mo->target)
			bot->patience = 20 + (M_Random() & 31) * 4;
	}

	// Can't see a target || don't have a suitable weapon to take it out with?
	if (!seetarget && bot->patience < 0)
	{
		seetarget = BOT_LookForItems(bot);

		if (mo->target)
			bot->patience = 30 + (M_Random() & 31) * 8;
	}

	if (mo->target)
	{
		BOT_Chase(bot, seetarget, move_ok);
	}
	else
	{
		// Wander around.
		if (!move_ok || bot->move_count < 0)
		{
			BOT_NewChaseDir(bot, move_ok);

			bot->move_count = 10 + (M_Random() & 31);
			bot->strafedir = 0;
		}

		BOT_Move(bot);
	}
}

//
// Reads the botcmd_t, converts it to ticcmd_t and stores the result in dest.
//
static void BOT_ConvertToTiccmd(bot_t *bot, ticcmd_t *dest, botcmd_t *src)
{
	mobj_t *mo = bot->pl->mo;

	if (src->attack)
		dest->buttons |= BT_ATTACK;
	if (src->second_attack)
		dest->extbuttons |= EBT_SECONDATK;
	if (src->use)
		dest->buttons |= BT_USE;
	if (src->new_weapon != -1)
		dest->buttons |= (src->new_weapon << BT_WEAPONSHIFT) & BT_WEAPONMASK;

	dest->player_idx = bot->pl->pnum;

	angle_t new_angle = bot->angle;
	float   new_slope = 0;		

	if (src->face_mobj)
	{
		float dx = src->face_mobj->x - mo->x;
		float dy = src->face_mobj->y - mo->y;
		float dz = src->face_mobj->z - mo->z;

		new_angle = R_PointToAngle(0,0, dx,dy);
		new_slope = P_ApproxSlope(dx, dy, dz);
	}

	dest->angleturn = (mo->angle - new_angle) >> 16;
	dest->mlookturn = (M_ATan(new_slope) - mo->vertangle) >> 16;

	dest->forwardmove = 0;
	dest->sidemove    = 0;
	dest->upwardmove  = 0;

	if (src->move_speed != 0)
	{
		// set a to the angle relative the player.
		angle_t a = src->move_angle - new_angle;

		float fm = M_Cos(a) * src->move_speed;
		float sm = M_Sin(a) * src->move_speed;

		dest->forwardmove = (int)fm;
		dest->sidemove    = (int)sm;
	}

	if (src->jump)
		dest->upwardmove = 0x20;
}


//----------------------------------------------------------------------------


void P_BotPlayerBuilder(const player_t *p, void *data, ticcmd_t *cmd)
{
	Z_Clear(cmd, ticcmd_t, 1);

	if (gamestate != GS_LEVEL)
		return;

	bot_t *bot = (bot_t *)data;
	SYS_ASSERT(bot);

	Z_Clear(&bot->cmd, botcmd_t, 1);
	bot->cmd.new_weapon = -1;

	BOT_Think(bot);
	BOT_ConvertToTiccmd(bot, cmd, &bot->cmd);
}


//
// Converts the player (which should be empty, i.e. neither a network
// or console player) to a bot.  Recreate is true for bot players
// loaded from a savegame.
//
void P_BotCreate(player_t *p, bool recreate)
{
	bot_t *bot = new bot_t;

	Z_Clear(bot, bot_t, 1);

	p->builder = P_BotPlayerBuilder;
	p->build_data = (void *)bot;
	p->playerflags |= PFL_Bot;

	bot->pl = p;

	if (! recreate)
		sprintf(p->playername, "Bot%d", p->pnum + 1);
}


void BOT_BeginLevel(void)
{
}


//
// Done at level shutdown, right after all mobjs have been removed.
// Erases anything level specific from the bot structs.
//
void BOT_EndLevel(void)
{
	// FIXME bot data never freed
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
