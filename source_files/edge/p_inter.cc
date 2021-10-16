//----------------------------------------------------------------------------
//  EDGE Interactions (picking up items etc..) Code
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

#include "am_map.h"
#include "con_main.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "e_input.h"
#include "dstrings.h"
#include "m_random.h"
#include "p_local.h"
#include "rad_trig.h"
#include "s_sound.h"

#define BONUS_ADD    6
#define BONUS_LIMIT  100

#define DAMAGE_ADD_MIN  3
#define DAMAGE_LIMIT  100


bool var_obituaries = true;


typedef struct
{
	benefit_t *list;  // full list of benefits
	bool lose_em;     // lose stuff if true

	player_t *player; // player picking it up
	mobj_t *special;  // object to pick up
	bool dropped;     // object was dropped by a monster

	int new_weap;     // index (for player) of a new weapon, -1 = none
	int new_ammo;     // ammotype of new ammo, -1 = none

	bool got_it;      // player actually got the benefit
	bool keep_it;     // don't remove the thing from map
	bool silent;      // don't make sound/flash/effects
	bool no_ammo;     // skip ammo
}
pickup_info_t;


bool P_CheckForBenefit(benefit_t *list, int kind)
{
	for (benefit_t *be = list; be != NULL; be = be->next)
	{
		if (be->type == kind)
			return true;
	}

	return false;
}


//
// P_GiveAmmo
//
// Returns false if the ammo can't be picked up at all
//
// -ACB- 1998/06/19 DDF Change: Number passed is the exact amount of ammo given.
// -KM- 1998/11/25 Handles weapon change from priority.
//
static void GiveAmmo(pickup_info_t *pu, benefit_t *be)
{
	if (pu->no_ammo)
		return;

	int ammo  = be->sub.type;  
	int num   = I_ROUND(be->amount);

	// -AJA- in old deathmatch, weapons give 2.5 times more ammo
	if (deathmatch == 1 && P_CheckForBenefit(pu->list, BENEFIT_Weapon) &&
		pu->special && !pu->dropped)
	{
		num = I_ROUND(be->amount * 2.5);
	}

	if (ammo == AM_NoAmmo || num <= 0)
		return;

	if (ammo < 0 || ammo >= NUMAMMO)
		I_Error("GiveAmmo: bad type %i", ammo);

	if (pu->lose_em)
	{
		if (pu->player->ammo[ammo].num == 0)
			return;

		pu->player->ammo[ammo].num -= num;

		if (pu->player->ammo[ammo].num < 0)
			pu->player->ammo[ammo].num = 0;

		pu->got_it = true;
		return;
	}

	// In Nightmare you need the extra ammo, in "baby" you are given double
	if (pu->special)
	{
		if ((gameskill == sk_baby) || (gameskill == sk_nightmare))
			num <<= 1;
	}

	bool did_pickup = false;

	// for newly acquired weapons (in the same benefit list) which have
	// a clip, try to "bundle" this ammo inside that clip.  
	if (pu->new_weap >= 0)
	{
		did_pickup = P_TryFillNewWeapon(pu->player, pu->new_weap,
			(ammotype_e)ammo, &num);

		if (num == 0)
		{
			pu->got_it = true;
			return;
		}
	}

	// divide by two _here_, which means that the ammo for filling
	// clip weapons is not affected by the MF_DROPPED flag.
	if (num > 1 && pu->dropped)
		num /= 2;

	if (pu->player->ammo[ammo].num == pu->player->ammo[ammo].max)
	{
		if (did_pickup)
			pu->got_it = true;
		return;
	}

	// if there is some fresh ammo, we should change weapons 
	if (pu->player->ammo[ammo].num == 0)
		pu->new_ammo = ammo;

	pu->player->ammo[ammo].num += num;

	if (pu->player->ammo[ammo].num > pu->player->ammo[ammo].max)
		pu->player->ammo[ammo].num = pu->player->ammo[ammo].max;

	pu->got_it = true;
}

//
// GiveAmmoLimit
//
static void GiveAmmoLimit(pickup_info_t *pu, benefit_t *be)
{
	int ammo  = be->sub.type;  
	int limit = I_ROUND(be->amount);

	if (ammo == AM_NoAmmo)
		return;

	if (ammo < 0 || ammo >= NUMAMMO)
		I_Error("GiveAmmoLimit: bad type %i", ammo);

	if ((!pu->lose_em && limit < pu->player->ammo[ammo].max) ||
		( pu->lose_em && limit > pu->player->ammo[ammo].max))
	{
		return;
	}

	pu->player->ammo[ammo].max = limit;

	// new limit could be lower...
	if (pu->player->ammo[ammo].num > pu->player->ammo[ammo].max)
		pu->player->ammo[ammo].num = pu->player->ammo[ammo].max;

	pu->got_it = true;
}

//
// GiveWeapon
//
// The weapon thing may have a MF_DROPPED flag or'ed in.
//
// -AJA- 2000/03/02: Reworked for new benefit_t stuff.
//
static void GiveWeapon(pickup_info_t *pu, benefit_t *be)
{
	weapondef_c *info = be->sub.weap;
	int pw_index;

	SYS_ASSERT(info);

	if (pu->lose_em)
	{
		if (P_RemoveWeapon(pu->player, info))
			pu->got_it = true;
		return;
	}

	// special handling for CO-OP and OLD DeathMatch
	if (numplayers > 1 && deathmatch != 2 &&
		pu->special && ! pu->dropped)
	{
		if (! P_AddWeapon(pu->player, info, &pw_index))
		{
			pu->no_ammo = true;
			return;
		}

		pu->new_weap = pw_index;
		pu->keep_it = true;
		pu->got_it = true;
		return;
	}

	if (! P_AddWeapon(pu->player, info, &pw_index))
		return;

	pu->new_weap = pw_index;
	pu->got_it = true;
}

//
// GiveHealth
//
// Returns false if not health is not needed,
//
// New Procedure: -ACB- 1998/06/21
//
static void GiveHealth(pickup_info_t *pu, benefit_t *be)
{
	if (pu->lose_em)
	{
		P_DamageMobj(pu->player->mo, pu->special, NULL, be->amount, NULL);
		pu->got_it = true;
		return;
	}

	if (pu->player->health >= be->limit)
		return;

	pu->player->health += be->amount;

	if (pu->player->health > be->limit)
		pu->player->health = be->limit;

	pu->player->mo->health = pu->player->health;

	pu->got_it = true;
}

//
// GiveArmour
//
// Returns false if the new armour would not benefit
//
static void GiveArmour(pickup_info_t *pu, benefit_t *be)
{
	armour_type_e a_class = (armour_type_e)be->sub.type;

	SYS_ASSERT(0 <= a_class && a_class < NUMARMOUR);

	if (pu->lose_em)
	{
		if (pu->player->armours[a_class] == 0)
			return;

		pu->player->armours[a_class] -= be->amount;
		if (pu->player->armours[a_class] < 0)
			pu->player->armours[a_class] = 0;

		P_UpdateTotalArmour(pu->player);

		pu->got_it = true;
		return;
	}

	float amount  = be->amount;
	float upgrade = 0;

	if (! pu->special || (pu->special->extendedflags & EF_SIMPLEARMOUR))
	{
		float slack = be->limit - pu->player->armours[a_class];

		if (amount > slack)
			amount = slack;
		
		if (amount <= 0)
			return;
	}
	else /* Doom emulation */
	{
		float slack = be->limit - pu->player->totalarmour;

		if (slack < 0)
			return;

		// we try to Upgrade any lower class armour with this armour.
		for (int cl = a_class - 1; cl >= 0; cl--)
		{
			upgrade += pu->player->armours[cl];
		}

		// cannot upgrade more than the specified amount
		if (upgrade > amount)
			upgrade = amount;

		slack += upgrade;

		if (amount > slack)
			amount = slack;
		
		SYS_ASSERT(amount  >= 0);
		SYS_ASSERT(upgrade >= 0);

		if (amount == 0 && upgrade == 0)
			return;
	}

	pu->player->armours[a_class] += amount;

	// -AJA- 2007/08/22: armor associations
	if (pu->special && pu->special->info->armour_protect >= 0)
	{
		pu->player->armour_types[a_class] = pu->special->info;
	}

	if (upgrade > 0)
	{
		for (int cl = a_class - 1; (cl >= 0) && (upgrade > 0); cl--)
		{
			if (pu->player->armours[cl] >= upgrade)
			{
				pu->player->armours[cl] -= upgrade;
				break;
			}
			else if (pu->player->armours[cl] > 0)
			{
				upgrade -= pu->player->armours[cl];
				pu->player->armours[cl] = 0;
			}
		}
	}

	P_UpdateTotalArmour(pu->player);

	pu->got_it = true;
}

//
// GiveKey
//
static void GiveKey(pickup_info_t *pu, benefit_t *be)
{
	keys_e key = (keys_e)be->sub.type;

	if (pu->lose_em)
	{
		if (! (pu->player->cards & key))
			return;

		pu->player->cards = (keys_e)(pu->player->cards & ~key);
	}
	else
	{
		if (pu->player->cards & key)
			return;

		pu->player->cards = (keys_e)(pu->player->cards | key);
	}

	// -AJA- leave keys in Co-op games
	if (COOP_MATCH())
		pu->keep_it = true;

	pu->got_it = true;
}

//
// GivePower
//
// DDF Change: duration is now passed as a parameter, for the berserker
//             the value is the health given, extendedflags also passed.
//
// The code was changes to a switch instead of a series of if's, also
// included is the use of limit, which gives a maxmium amount of protection
// for this item. -ACB- 1998/06/20
//
static void GivePower(pickup_info_t *pu, benefit_t *be)
{ 
	// -ACB- 1998/06/20 - calculate duration in seconds
	float duration = be->amount * TICRATE;
	float limit    = be->limit  * TICRATE;

	if (pu->lose_em)
	{
		if (pu->player->powers[be->sub.type] == 0)
			return;

		pu->player->powers[be->sub.type] -= duration;

		if (pu->player->powers[be->sub.type] < 0)
			pu->player->powers[be->sub.type] = 0;

		pu->got_it = true;
		return;
	}

	if (pu->player->powers[be->sub.type] >= limit)
		return;

	pu->player->powers[be->sub.type] += duration;

	if (pu->player->powers[be->sub.type] > limit)
		pu->player->powers[be->sub.type] = limit;

	// special handling for scuba...
	if (be->sub.type == PW_Scuba)
	{
		pu->player->air_in_lungs = pu->player->mo->info->lung_capacity;
	}

	pu->got_it = true;
}

void DoGiveBenefitList(pickup_info_t *pu)
{
	// handle weapons first, since this affects ammo handling

	for (benefit_t *be = pu->list; be; be = be->next)
	{
		if (be->type == BENEFIT_Weapon && be->amount >= 0.0)
			GiveWeapon(pu, be);
	}

	for (benefit_t *be = pu->list; be; be = be->next)
	{
		// Put the checking in for neg amounts at benefit level. Powerups can be neg 
		// if they last all level. -ACB- 2004/02/04

		switch (be->type)
		{
			case BENEFIT_None:
			case BENEFIT_Weapon:
				break;

			case BENEFIT_Ammo:
				if (be->amount >= 0.0)
					GiveAmmo(pu, be);
				break;

			case BENEFIT_AmmoLimit:
				if (be->amount >= 0.0)
					GiveAmmoLimit(pu, be);
				break;

			case BENEFIT_Key:
				if (be->amount >= 0.0)
					GiveKey(pu, be);
				break;

			case BENEFIT_Health:
				if (be->amount >= 0.0)
					GiveHealth(pu, be);
				break;

			case BENEFIT_Armour:
				if (be->amount >= 0.0)
					GiveArmour(pu, be);
				break;

			case BENEFIT_Powerup:
				GivePower(pu, be);
				break;

			default:
				break;
		}
	}
}

//
// P_GiveBenefitList
//
// Give all the benefits in the list to the player.  `special' is the
// special object that all these benefits came from, or NULL if they
// came from the initial_benefits list.  When `lose_em' is true, the
// benefits should be taken away instead.  Returns true if _any_
// benefit was picked up (or lost), or false if none of them were.
//
bool P_GiveBenefitList(player_t *player, mobj_t * special, 
					   benefit_t *list, bool lose_em)
{
	pickup_info_t info;

	info.list    = list;
	info.lose_em = lose_em;

	info.player  = player;
	info.special = special;
	info.dropped = false;

	info.new_weap = -1;
	info.new_ammo = -1;

	info.got_it  = false;
	info.keep_it = false;
	info.silent  = false;
	info.no_ammo = false;

	DoGiveBenefitList(&info);

	return info.got_it;
}

//
// RunPickupEffects
//
static void RunPickupEffects(player_t *player, mobj_t *special, 
		pickup_effect_c *list)
{
	for (; list; list=list->next)
	{
		switch (list->type)
		{
			case PUFX_SwitchWeapon:
				P_PlayerSwitchWeapon(player, list->sub.weap);
				break;

			case PUFX_KeepPowerup:
				player->keep_powers |= (1 << list->sub.type);
				break;

			case PUFX_PowerupEffect:
				// FIXME
				break;

			case PUFX_ScreenEffect:
				// FIXME
				break;
			
			default: break;
		}
	}
}

//
// P_TouchSpecialThing
//
// -KM- 1999/01/31 Things that give you item bonus are always
//  picked up.  Picked up object is set to death frame instead
//  of removed so that effects can happen.
//
void P_TouchSpecialThing(mobj_t * special, mobj_t * toucher)
{
	float delta = special->z - toucher->z;

	// out of reach
	if (delta > toucher->height || delta < -special->height)
		return;

	if (! toucher->player)
		return;

	// Dead thing touching. Can happen with a sliding player corpse.
	if (toucher->health <= 0)
		return;

	// -KM- 1998/09/27 Sounds.ddf
	sfx_t *sound = special->info->activesound;

	pickup_info_t info;

	info.player  = toucher->player;
	info.special = special;
	info.dropped = (special && (special->flags & MF_DROPPED)) ? true : false;

	info.new_weap = -1; // the most recently added weapon (must be new)
	info.new_ammo = -1; // got fresh ammo (old count was zero).

	info.got_it  = false;
	info.keep_it = false;
	info.silent  = false;
	info.no_ammo = false;

	// First handle lost benefits
	info.list    = special->info->lose_benefits;
	info.lose_em = true;
	DoGiveBenefitList(&info);

	// Run through the list of all pickup benefits...
	info.list    = special->info->pickup_benefits;
	info.lose_em = false;
	DoGiveBenefitList(&info);

	if (special->flags & MF_COUNTITEM)
	{
		info.player->itemcount++;
		info.got_it = true;
	}
	else if (special->hyperflags & HF_FORCEPICKUP)
	{
		info.got_it  = true;
		info.keep_it = false;
	}

	if (! info.got_it)
		return;

	if (! info.keep_it)
	{
		special->health = 0;
		P_KillMobj(info.player->mo, special, NULL);
	}

	// do all the special effects, lights & sound etc...
	if (! info.silent)
	{
		info.player->bonuscount += BONUS_ADD;
		if (info.player->bonuscount > BONUS_LIMIT)
			info.player->bonuscount = BONUS_LIMIT;

		if (special->info->pickup_message &&
			language.IsValidRef(special->info->pickup_message))
		{
			CON_PlayerMessage(info.player->pnum, "%s",
				language[special->info->pickup_message]);
		}

		if (sound)
        {
            int sfx_cat;

            if (info.player == players[consoleplayer])
                sfx_cat = SNCAT_Player;
            else
                sfx_cat = SNCAT_Opponent;

			S_StartFX(sound, sfx_cat, info.player->mo);
        }

		if (info.new_weap >= 0 || info.new_ammo >= 0)
			P_TrySwitchNewWeapon(info.player, info.new_weap, (ammotype_e)info.new_ammo);
	}

	RunPickupEffects(info.player, special, special->info->pickup_effects);
}


// FIXME: move this into utility code
static std::string PatternSubst(const char *format, const std::vector<std::string> & keywords)
{
	std::string result;

	while (*format)
	{
		const char *pos = strchr(format, '%');

		if (! pos)
		{
			result = result + std::string(format);
			break;
		}

		if (pos > format)
			result = result + std::string(format, pos - format);

		pos++;

		char key[4];
		
		key[0] = *pos++;
		key[1] = 0;

		if (! key[0])
			break;

		if (isalpha(key[0]))
		{
			for (int i = 0; i+1 < (int)keywords.size(); i += 2)
			{
				if (strcmp(key, keywords[i].c_str()) == 0)
				{
					result = result + keywords[i+1];
					break;
				}
			}
		}
		else if (key[0] == '%')
		{
			result = result + std::string("%");
		}
		else
		{
			result = result + std::string("%");
			result = result + std::string(key);
		}

		format = pos;
	}

	return result;
}

static void DoObituary(const char *format, mobj_t *victim, mobj_t *killer)
{
	std::vector<std::string> keywords;

	keywords.push_back("o");
	keywords.push_back("the player");

	keywords.push_back("k");
	keywords.push_back("a foe");

	std::string msg = PatternSubst(format, keywords);

	CON_PlayerMessage(victim->player->pnum, "%s", msg.c_str());
}

void P_ObituaryMessage(mobj_t * victim, mobj_t * killer, const damage_c *damtype)
{
	if (! var_obituaries)
		return;

	if (damtype && !damtype->obituary.empty())
	{
		const char *ref = damtype->obituary.c_str();

		if (language.IsValidRef(ref))
		{
			DoObituary(language[ref], victim, killer);
			return;
		}

		I_Debugf("Missing obituary entry in LDF: '%s'\n", ref);
	}

	if (killer)
		DoObituary("%o was killed.", victim, killer);
	else
		DoObituary("%o died.", victim, killer);
}


//
// P_KillMobj
//
// Altered to reflect the fact that the dropped item is a pointer to
// mobjtype_c, uses new procedure: P_MobjCreateObject.
//
// Note: Damtype can be NULL here.
//
// -ACB- 1998/08/01
//
// -AJA- 1999/09/12: Now uses P_SetMobjStateDeferred, since this
//       routine can be called by TryMove/PIT_CheckRelThing/etc.
//
void P_KillMobj(mobj_t * source, mobj_t * target, const damage_c *damtype,
				bool weak_spot)
{
	// -AJA- 2006/09/10: Voodoo doll handling for coop
	if (target->player && target->player->mo != target)
	{
		P_KillMobj(source, target->player->mo, damtype, weak_spot);
		target->player = NULL;
	}

	target->flags &= ~(MF_SPECIAL | MF_SHOOTABLE | MF_FLOAT | 
		MF_SKULLFLY | MF_TOUCHY);
	target->extendedflags &= ~(EF_BOUNCE | EF_USABLE | EF_CLIMBABLE);

	if (!(target->extendedflags & EF_NOGRAVKILL))
		target->flags &= ~MF_NOGRAVITY;

	target->flags |= MF_CORPSE | MF_DROPOFF;
	target->height /= 4;

	RAD_MonsterIsDead(target);

	if (source && source->player)
	{
		// count for intermission
		if (target->flags & MF_COUNTKILL)
			source->player->killcount++;

		if (target->player)
		{
			// Killed a team mate?
			if (target->side & source->side)
			{
				source->player->frags--;
				source->player->totalfrags--;
			}
			else
			{
				source->player->frags++;
				source->player->totalfrags++;
			}
		}
	}
	else if (SP_MATCH() && (target->flags & MF_COUNTKILL))
	{
		// count all monster deaths,
		// even those caused by other monsters
		players[consoleplayer]->killcount++;
	}

	if (target->player)
	{
		P_ObituaryMessage(target, source, damtype);

		// count environment kills against you
		if (!source)
		{
			target->player->frags--;
			target->player->totalfrags--;
		}

		target->flags &= ~MF_SOLID;
		target->player->playerstate = PST_DEAD;
		target->player->std_viewheight = MIN(DEATHVIEWHEIGHT,
			target->height / 3);
		target->player->actual_speed = 0;

		P_DropWeapon(target->player);

		// don't die in auto map, switch view prior to dying
		if (target->player == players[consoleplayer] && automapactive)
			AM_Stop();

		// don't immediately restart when USE key was pressed
		if (target->player == players[consoleplayer])
			E_ClearInput();
	}

	statenum_t state = S_NULL;
	bool overkill = (target->health < -target->info->spawnhealth);

	if (weak_spot)
	{
		state = P_MobjFindLabel(target, "WEAKDEATH");
		if (state == S_NULL)
			overkill = true;
	}

	if (state == S_NULL && overkill && damtype && damtype->overkill.label)
	{
		state = P_MobjFindLabel(target, damtype->overkill.label);
		if (state != S_NULL)
			state += damtype->overkill.offset;
	}

	if (state == S_NULL && overkill && target->info->overkill_state)
		state = target->info->overkill_state;

	if (state == S_NULL && damtype && damtype->death.label)
	{
		state = P_MobjFindLabel(target, damtype->death.label);
		if (state != S_NULL)
			state += damtype->death.offset;
	}

	if (state == S_NULL)
		state = target->info->death_state;

	P_SetMobjStateDeferred(target, state, P_Random() & 3);

	// Drop stuff. This determines the kind of object spawned
	// during the death frame of a thing.
	const mobjtype_c *item = target->info->dropitem;
	if (item)
	{
		mobj_t *mo = P_MobjCreateObject(target->x, target->y,
			target->floorz, item);

		// -ES- 1998/07/18 NULL check to prevent crashing
		if (mo)
			mo->flags |= MF_DROPPED;
	}
}

//
// P_ThrustMobj
//
// Like P_DamageMobj, but only pushes the target object around
// (doesn't inflict any damage).  Parameters are:
//
// * target    - mobj to be thrust.
// * inflictor - mobj causing the thrusting.
// * thrust    - amount of thrust done (same values as damage).  Can
//               be negative to "pull" instead of push.
//
// -AJA- 1999/11/06: Wrote this routine.
//
void P_ThrustMobj(mobj_t * target, mobj_t * inflictor, float thrust)
{
	// check for immunity against the attack
	if (target->hyperflags & HF_INVULNERABLE)
		return;

	if (inflictor && inflictor->currentattack && BITSET_EMPTY ==
		(inflictor->currentattack->attack_class & ~target->info->immunity))
	{
		return;
	}

	float dx = target->x - inflictor->x;
	float dy = target->y - inflictor->y;

	// don't thrust if at the same location (no angle)
	if (fabs(dx) < 1.0f && fabs(dy) < 1.0f)
		return;

	angle_t angle = R_PointToAngle(0, 0, dx, dy);

	// -ACB- 2000/03/11 Div-by-zero check...
	SYS_ASSERT(0 != target->info->mass);

	float push = 12.0f * thrust / target->info->mass;

	// limit thrust to reasonable values
	if (push < -40.0f) push = -40.0f;
	if (push >  40.0f) push =  40.0f;

	target->mom.x += push * M_Cos(angle);
	target->mom.y += push * M_Sin(angle);

	if (level_flags.true3dgameplay)
	{
		float dz = MO_MIDZ(target) - MO_MIDZ(inflictor);
		float slope = P_ApproxSlope(dx, dy, dz);

		target->mom.z += push * slope / 2;
	}
}

//
// P_DamageMobj
//
// Damages both enemies and players, decreases the amount of health
// an mobj has and "kills" an mobj in the event of health being 0 or
// less, the parameters are:
//
// * Target    - mobj to be damaged.
// * Inflictor - mobj which is causing the damage.
// * Source    - mobj who is responsible for doing the damage. Can be NULL
// * Amount    - amount of damage done.
// * Damtype   - type of damage (for override states).  Can be NULL
//
// Both source and inflictor can be NULL, slime damage and barrel
// explosions etc....
//
// -AJA- 1999/09/12: Now uses P_SetMobjStateDeferred, since this
//       routine can be called by TryMove/PIT_CheckRelThing/etc.
//
void P_DamageMobj(mobj_t * target, mobj_t * inflictor, mobj_t * source,
				  float damage, const damage_c * damtype, bool weak_spot)
{
	if (!(target->flags & MF_SHOOTABLE))
		return;

	if (target->health <= 0)
		return;

	// check for immunity against the attack
	if (target->hyperflags & HF_INVULNERABLE)
		return;

	if (!weak_spot && inflictor && inflictor->currentattack && BITSET_EMPTY ==
		(inflictor->currentattack->attack_class & ~target->info->immunity))
	{
		return;
	}

	// check for partial resistance against the attack
	if (!weak_spot && damage >= 0.1f && inflictor && inflictor->currentattack &&
		BITSET_EMPTY == (inflictor->currentattack->attack_class & ~target->info->resistance))
	{
		damage = MAX(0.1f, damage * target->info->resist_multiply);
	}

	// -ACB- 1998/07/12 Use Visibility Enum
	// A Damaged Stealth Creature becomes more visible
	if (target->flags & MF_STEALTH)
		target->vis_target = VISIBLE;

	if (target->flags & MF_SKULLFLY)
	{
		target->mom.x = target->mom.y = target->mom.z = 0;
		target->flags &= ~MF_SKULLFLY;
	}

	player_t *player = target->player;

	// Some close combat weapons should not
	// inflict thrust and push the victim out of reach,
	// thus kick away unless using the chainsaw.

	if (inflictor && !(target->flags & MF_NOCLIP) &&
		!(source && source->player && source->player->ready_wp >= 0 &&
		source->player->weapons[source->player->ready_wp].info->nothrust))
	{
		// make fall forwards sometimes
		if (damage < 40 && damage > target->health &&
			target->z - inflictor->z > 64 && (P_Random() & 1))
		{
			P_ThrustMobj(target, inflictor, -damage * 4);
		}
		else
			P_ThrustMobj(target, inflictor, damage);
	}

	// player specific
	if (player)
	{
		int i;

		// ignore damage in GOD mode, or with INVUL powerup
		if ((player->cheats & CF_GODMODE) || player->powers[PW_Invulnerable] > 0)
			return;

		// take half damage in trainer mode
		if (gameskill == sk_baby)
			damage /= 2.0f;

		// preliminary check: immunity and resistance
		for (i = NUMARMOUR-1; i >= ARMOUR_Green; i--)
		{
			if (damtype && damtype->no_armour)
				continue;

			if (player->armours[i] <= 0)
				continue;

			const mobjtype_c *arm_info = player->armour_types[i];

			if (!arm_info || !inflictor || !inflictor->currentattack)
				continue;

			// this armor does not provide any protection for this attack
			if (BITSET_EMPTY != (inflictor->currentattack->attack_class & ~arm_info->armour_class))
				continue;

			if (BITSET_EMPTY == (inflictor->currentattack->attack_class & ~arm_info->immunity))
				return; /* immune : we can go home early! */

			if (damage > 0.1f && BITSET_EMPTY == (inflictor->currentattack->attack_class & ~arm_info->resistance))
			{
				damage = MAX(0.1f, damage * arm_info->resist_multiply);
			}
		}

		// check which armour can take some damage
		for (i=NUMARMOUR-1; i >= ARMOUR_Green; i--)
		{
			if (damtype && damtype->no_armour)
				continue;

			if (player->armours[i] <= 0)
				continue;

			const mobjtype_c *arm_info = player->armour_types[i];

			// this armor does not provide any protection for this attack
			if (arm_info && inflictor && inflictor->currentattack &&
				BITSET_EMPTY != (inflictor->currentattack->attack_class & ~arm_info->armour_class))
			{
				continue;
			}

			float saved = 0;

			if (arm_info)
				saved = damage * PERCENT_2_FLOAT(arm_info->armour_protect);
			else
			{
				switch (i)
				{
					case ARMOUR_Green:  saved = damage * 0.33; break;
					case ARMOUR_Blue:   saved = damage * 0.50; break;
					case ARMOUR_Purple: saved = damage * 0.66; break;
					case ARMOUR_Yellow: saved = damage * 0.75; break;
					case ARMOUR_Red:    saved = damage * 0.90; break;

					default: 
						I_Error("INTERNAL ERROR in P_DamageMobj: bad armour %d\n", i);
				}
			}

			if (player->armours[i] <= saved)
			{
				// armour is used up
				saved = player->armours[i];
			}

			damage -= saved;

			if (arm_info)
				saved *= PERCENT_2_FLOAT(arm_info->armour_deplete);

			player->armours[i] -= saved;

			// don't apply inner armour unless outer is finished
			if (player->armours[i] > 0)
				break;

			player->armours[i] = 0;
		}

		P_UpdateTotalArmour(player);

		player->attacker = source;

		// add damage after armour / invuln detection
		if (damage > 0)
		{
			player->damagecount += (int)MAX(damage, DAMAGE_ADD_MIN);
			player->damage_pain += damage;
		}

		// teleport stomp does 10k points...
		if (player->damagecount > DAMAGE_LIMIT)
			player->damagecount = DAMAGE_LIMIT;
	}

	// do the damage
	target->health -= damage;

	if (player)
	{
		// mirror mobj health here for Dave
		player->health = MAX(0, target->health);
	}

	// -AJA- 2007/11/06: vampire mode!
	if (source && source != target &&
		source->health < source->info->spawnhealth &&
		((source->hyperflags & HF_VAMPIRE) ||
		 (inflictor && inflictor->currentattack &&
		  (inflictor->currentattack->flags & AF_Vampire))))
	{
		float qty = (target->player ? 0.5 : 0.25) * damage;

		source->health = MIN(source->health + qty, source->info->spawnhealth);

		if (source->player)
			source->player->health = source->health;
	}

	if (target->health <= 0)
	{
		P_KillMobj(source, target, damtype, weak_spot);
		return;
	}

	// enter pain states
	float pain_chance;

	if (target->flags & MF_SKULLFLY)
		pain_chance = 0;
	else if (weak_spot && target->info->weak.painchance >= 0)
		pain_chance = target->info->weak.painchance;
	else if (target->info->resist_painchance >= 0 &&
			 inflictor && inflictor->currentattack && BITSET_EMPTY ==
			 (inflictor->currentattack->attack_class & ~target->info->resistance))
		pain_chance = target->info->resist_painchance;
	else
		pain_chance = target->info->painchance;

	if (pain_chance > 0 && P_RandomTest(pain_chance))
	{
		// setup to hit back
		target->flags |= MF_JUSTHIT;

		statenum_t state = S_NULL;

		if (weak_spot)
			state = P_MobjFindLabel(target, "WEAKPAIN");

		if (state == S_NULL && damtype && damtype->pain.label)
		{
			state = P_MobjFindLabel(target, damtype->pain.label);
			if (state != S_NULL)
				state += damtype->pain.offset;
		}

		if (state == S_NULL)
			state = target->info->pain_state;

		if (state != S_NULL)
			P_SetMobjStateDeferred(target, state, 0);
	}

	// we're awake now...
	target->reactiontime = 0;

	bool ultra_loyal = (source && (target->hyperflags & HF_ULTRALOYAL) &&
		(source->side & target->side) != 0);

	if ((!target->threshold || target->extendedflags & EF_NOGRUDGE) &&
		source && source != target && (!(source->extendedflags & EF_NEVERTARGET)) &&
		! ultra_loyal)
	{
		// if not intent on another player, chase after this one
		target->SetTarget(source);
		target->threshold = BASETHRESHOLD;

		if (target->state == &states[target->info->idle_state] &&
			target->info->chase_state)
		{
			P_SetMobjStateDeferred(target, target->info->chase_state, 0);
		}
	}
}


//
// For killing monsters and players when something teleports on top
// of them.  Even the invulnerability powerup doesn't stop it.  Also
// used for the kill-all cheat.  Inflictor and damtype can be NULL.
//
void P_TelefragMobj(mobj_t * target, mobj_t * inflictor, const damage_c * damtype)
{
	if (target->health <= 0)
		return;
	
	target->health = -1000;

	if (target->flags & MF_STEALTH)
		target->vis_target = VISIBLE;

	if (target->flags & MF_SKULLFLY)
	{
		target->mom.x = target->mom.y = target->mom.z = 0;
		target->flags &= ~MF_SKULLFLY;
	}

	if (target->player)
	{
		target->player->attacker = inflictor;
		target->player->damagecount = DAMAGE_LIMIT;
		target->player->damage_pain = target->info->spawnhealth;
	}

	P_KillMobj(inflictor, target, damtype);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
