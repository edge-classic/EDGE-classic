//----------------------------------------------------------------------------
//  EDGE Game Handling Code
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2023  The EDGE Team.
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
//
// -MH- 1998/07/02 Added key_flyup and key_flydown variables (no logic yet)
// -MH- 1998/08/18 Flyup and flydown logic
//

#include "i_defs.h"

#include "endianess.h"

#include "dm_defs.h"
#include "dm_state.h"
#include "e_player.h"
#include "g_game.h"
#include "hu_stuff.h"
#include "m_argv.h"
#include "m_random.h"
#include "bot_think.h"
#include "p_local.h"

#include "str_util.h"

#include "vm_coal.h" // For VM_EndLevel()


//
// PLAYER ARRAY
//
// Main rule is that players[p->num] == p (for all players p).
// The array only holds players "in game", the remaining fields
// are NULL.  There may be NULL entries in-between valid entries
// (e.g. player #2 left the game, so players[2] becomes NULL).
// This means that numplayers is NOT an index to last entry + 1.
//
// The consoleplayer and displayplayer variables must be valid
// indices at all times.
//
player_t *players[MAXPLAYERS];
int numplayers;
int numbots;

int consoleplayer = -1; // player taking events
int displayplayer = -1; // view being displayed 

#define MAX_BODIES   50

static mobj_t *bodyqueue[MAX_BODIES];
static int bodyqueue_size = 0;

// Maintain single and multi player starting spots.
static std::vector<spawnpoint_t> dm_starts;
static std::vector<spawnpoint_t> coop_starts;
static std::vector<spawnpoint_t> voodoo_dolls;
static std::vector<spawnpoint_t> hub_starts;


static void P_SpawnPlayer(player_t *p, const spawnpoint_t *point, bool is_hub);


void G_ClearPlayerStarts(void)
{
	dm_starts.clear();
	coop_starts.clear();
	voodoo_dolls.clear();
	hub_starts.clear();
}


void G_ClearBodyQueue(void)
{
	memset(bodyqueue, 0, sizeof(bodyqueue));

	bodyqueue_size = 0;
}


void G_AddBodyToQueue(mobj_t *mo)
{
	// flush an old corpse if needed 
	if (bodyqueue_size >= MAX_BODIES)
	{
		mobj_t *rotten = bodyqueue[bodyqueue_size % MAX_BODIES];
		rotten->refcount--;
		
		P_RemoveMobj(rotten);
	}

	// prevent accidental re-use
	mo->refcount++;

	bodyqueue[bodyqueue_size % MAX_BODIES] = mo;
	bodyqueue_size++;
}


//
// Called when a player completes a level.
// For HUB changes, we keep powerups and keycards
//
void G_PlayerFinishLevel(player_t *p, bool keep_cards)
{
	if (! keep_cards)
	{
		for (int i = 0; i < NUMPOWERS; i++)
			p->powers[i] = 0;

		p->keep_powers = 0;

		p->cards = KF_NONE;

		p->mo->flags &= ~MF_FUZZY;  // cancel invisibility 
	}

	p->extralight = 0;  // cancel gun flashes 

	// cancel colourmap effects
	p->effect_colourmap = NULL;

	// no palette changes 
	p->damagecount = 0;
	p->damage_pain = 0;
	p->bonuscount  = 0;
	p->grin_count  = 0;
	
	//Lobo 2023: uncomment if still getting 
	// "INTERNAL ERROR: player has a removed attacker"
	p->attacker = NULL; 
	

	VM_EndLevel();
}

//
// player_s::Reborn
//
// Called after a player dies. 
// Almost everything is cleared and initialised.
//
void player_s::Reborn()
{
	I_Debugf("player_s::Reborn\n");

	playerstate = PST_LIVE;

	mo = NULL;
	health = 0;

	memset(armours, 0, sizeof(armours));
	memset(armour_types, 0, sizeof(armour_types));
	memset(powers, 0, sizeof(powers));

	keep_powers = 0;
	totalarmour = 0;
	cards = KF_NONE;

	ready_wp = WPSEL_None;
	pending_wp = WPSEL_NoChange;

	memset(weapons, 0, sizeof(weapons));
	memset(avail_weapons, 0, sizeof(avail_weapons));
	memset(ammo, 0, sizeof(ammo));
	memset(inventory, 0, sizeof(inventory));
	memset(counters, 0, sizeof(counters));

	for (int w=0; w <= 9; w++)
		key_choices[w] = WPSEL_None;

	cheats = 0;
	refire = 0;
	bob = 0;
	kick_offset = 0;
	zoom_fov = 0;
	bonuscount = 0;
	damagecount = 0;
	damage_pain = 0;
	extralight = 0;
	flash = false;

	attacker = NULL;

	effect_colourmap = NULL;
	effect_left = 0;

	memset(psprites, 0, sizeof(psprites));
	
	jumpwait = 0;
	idlewait = 0;
	splashwait = 0;
	air_in_lungs = 0;
	underwater = false;
	swimming = false;
	wet_feet = false;

	grin_count = 0;
	attackdown_count = 0;
	face_index = 0;
	face_count = 0;

	remember_atk[0] = -1;
	remember_atk[1] = -1;
	remember_atk[2] = -1;
	remember_atk[3] = -1;
	weapon_last_frame = -1;
}

//
// Returns false if the player cannot be respawned at the given spot
// because something is occupying it.
//
static bool G_CheckSpot(player_t *player, const spawnpoint_t *point)
{
	float x = point->x;
	float y = point->y;
	float z = point->z;

	if (!player->mo)
	{
		// first spawn of level, before corpses
		for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
		{
			player_t *p = players[pnum];

			if (!p || !p->mo || p == player)
				continue;

			if (fabs(p->mo->x - x) < 8.0f &&
				fabs(p->mo->y - y) < 8.0f)
				return false;
		}

		P_SpawnPlayer(player, point, false);
		return true; // OK
	}

	if (!P_CheckAbsPosition(player->mo, x, y, z))
		return false;

	G_AddBodyToQueue(player->mo);

	// spawn a teleport fog 
	// (temp fix for teleport effect)
	x += 20 * M_Cos(point->angle);
	y += 20 * M_Sin(point->angle);
	P_MobjCreateObject(x, y, z, mobjtypes.Lookup("TELEPORT_FLASH"));

	P_SpawnPlayer(player, point, false);
	return true; // OK
}


//
// G_SetConsolePlayer
//
// Note: we don't rely on current value being valid, hence can use
//       these functions during initialisation.
//
void G_SetConsolePlayer(int pnum)
{
	consoleplayer = pnum;

	SYS_ASSERT(players[consoleplayer]);

	for (int i = 0; i < MAXPLAYERS; i++)
		if (players[i])
			players[i]->playerflags &= ~PFL_Console;
	
	players[pnum]->playerflags |= PFL_Console;

	if (argv::Find("testbot") > 0)
	{
		P_BotCreate(players[pnum], false);
	}
	else
	{
		players[pnum]->builder = P_ConsolePlayerBuilder;
		players[pnum]->build_data = NULL;
	}
}

//
// G_SetDisplayPlayer
//
void G_SetDisplayPlayer(int pnum)
{
	displayplayer = pnum;

	SYS_ASSERT(players[displayplayer]);

	for (int i = 0; i < MAXPLAYERS; i++)
		if (players[i])
			players[i]->playerflags &= ~PFL_Display;
	
	players[pnum]->playerflags |= PFL_Display;
}

void G_ToggleDisplayPlayer(void)
{
	for (int i = 1; i <= MAXPLAYERS; i++)
	{
		int pnum = (displayplayer + i) % MAXPLAYERS;

		if (players[pnum])
		{
			G_SetDisplayPlayer(pnum);
			break;
		}
	}
}

//
// P_SpawnPlayer
//
// Called when a player is spawned on the level.
// Most of the player structure stays unchanged between levels.
//
// -KM- 1998/12/21 Cleaned this up a bit.
// -KM- 1999/01/31 Removed all those nasty cases for doomednum (1/4001)
//
static void P_SpawnPlayer(player_t *p, const spawnpoint_t *point, bool is_hub)
{
	// -KM- 1998/11/25 This is in preparation for skins.  The creatures.ddf
	//   will hold player start objects, sprite will be taken for skin.
	// -AJA- 2004/04/14: Use DDF entry from level thing.

	if (point->info == NULL)
		I_Error("P_SpawnPlayer: No such item type!");

	const mobjtype_c *info = point->info;

	L_WriteDebug("* P_SpawnPlayer %d @ %1.0f,%1.0f\n",
			point->info->playernum, point->x, point->y);
		
	if (info->playernum <= 0)
		info = mobjtypes.LookupPlayer(p->pnum + 1);

	if (p->playerstate == PST_REBORN)
	{
		p->Reborn();

		P_GiveInitialBenefits(p, info);
	}

	mobj_t *mobj = P_MobjCreateObject(point->x, point->y, point->z, info);

	mobj->angle = point->angle;
	mobj->vertangle = point->vertangle;
	mobj->player = p;
	mobj->health = p->health;

	p->mo = mobj;
	p->playerstate = PST_LIVE;
	p->refire = 0;
	p->damagecount = 0;
	p->damage_pain = 0;
	p->bonuscount = 0;
	p->extralight = 0;
	p->effect_colourmap = NULL;
	p->std_viewheight = mobj->height * PERCENT_2_FLOAT(info->viewheight);
	p->viewheight = p->std_viewheight;
	p->zoom_fov = 0;
	p->jumpwait = 0;

	// don't do anything immediately 
	p->attackdown[0] = p->attackdown[1] = false;
	p->usedown = false;
	p->actiondown[0] = p->actiondown[1] = false;

	// setup gun psprite
	if (! is_hub || !SP_MATCH())
		P_SetupPsprites(p);

	// give all cards in death match mode
	if (DEATHMATCH())
		p->cards = KF_MASK;

	// -AJA- in COOP, all players are on the same side
	if (COOP_MATCH())
		mobj->side = ~0;

	// Don't get stuck spawned in things: telefrag them.
	P_TeleportMove(mobj, mobj->x, mobj->y, mobj->z);

	if (COOP_MATCH() && ! level_flags.team_damage)
		mobj->hyperflags |= HF_SIDEIMMUNE;

	if (p->isBot())
	{
		bot_t *bot = (bot_t *)p->build_data;
		SYS_ASSERT(bot);

		bot->Respawn();
	}
}


static void P_SpawnVoodooDoll(player_t *p, const spawnpoint_t *point)
{
	const mobjtype_c *info = point->info;

	SYS_ASSERT(info);
	SYS_ASSERT(info->playernum > 0);

	L_WriteDebug("* P_SpawnVoodooDoll %d @ %1.0f,%1.0f\n",
			p->pnum+1, point->x, point->y);
		
	mobj_t *mobj = P_MobjCreateObject(point->x, point->y, point->z, info);

	mobj->angle = point->angle;
	mobj->vertangle = point->vertangle;
	mobj->player = p;
	mobj->health = p->health;

	mobj->is_voodoo = true;

	if (COOP_MATCH())
		mobj->side = ~0;
}

//
// G_DeathMatchSpawnPlayer 
//
// Spawns a player at one of the random deathmatch spots.
// Called at level load and each death.
//
void G_DeathMatchSpawnPlayer(player_t *p)
{
	if (p->pnum >= (int)dm_starts.size())
		I_Warning("Few deathmatch spots, %d recommended.\n", p->pnum + 1);

	int begin = P_Random();

	if (dm_starts.size() > 0)
	{
		for (int j = 0; j < (int)dm_starts.size(); j++)
		{
			int i = (begin + j) % (int)dm_starts.size();

			if (G_CheckSpot(p, &dm_starts[i]))
				return;
		}
	}

	// no good spot, so the player will probably get stuck
	if (coop_starts.size() > 0)
	{
		for (int j = 0; j < (int)coop_starts.size(); j++)
		{
			int i = (begin + j) % (int)coop_starts.size();

			if (G_CheckSpot(p, &coop_starts[i]))
				return;
		}
	}

	I_Error("No usable DM start found!");
}

//
// G_CoopSpawnPlayer 
//
// Spawns a player at one of the single player spots.
// Called at level load and each death.
//
void G_CoopSpawnPlayer(player_t *p)
{
	spawnpoint_t *point = G_FindCoopPlayer(p->pnum+1);

	if (point && G_CheckSpot(p, point))
		return;

	I_Warning("Player %d start is invalid.\n", p->pnum+1);

	int begin = p->pnum;

	// try to spawn at one of the other players spots
	for (int j = 0; j < (int)coop_starts.size(); j++)
	{
		int i = (begin + j) % (int)coop_starts.size();

		if (G_CheckSpot(p, &coop_starts[i]))
			return;
	}

	I_Error("No usable player start found!\n");
}


static spawnpoint_t *G_FindHubPlayer(int pnum, int tag)
{
	int count = 0;

	for (int i = 0; i < (int)hub_starts.size(); i++)
	{
		spawnpoint_t *point = &hub_starts[i];
		SYS_ASSERT(point->info);

		if (point->tag != tag)
			continue;

		count++;

		if (point->info->playernum == pnum)
			return point;
	}

	if (count == 0)
		I_Error("Missing hub starts with tag %d\n", tag);
	else
		I_Error("No usable hub start for player %d (tag %d)\n", pnum+1, tag);

	return NULL;  /* NOT REACHED */
}

void G_HubSpawnPlayer(player_t *p, int tag)
{
	SYS_ASSERT(! p->mo);

	spawnpoint_t *point = G_FindHubPlayer(p->pnum+1, tag);

	// assume player will fit (too bad otherwise)
	P_SpawnPlayer(p, point, true);
}


void G_SpawnVoodooDolls(player_t *p)
{
	for (int i = 0; i < (int)voodoo_dolls.size(); i++)
	{
		spawnpoint_t *point = &voodoo_dolls[i];

		if (point->info->playernum != p->pnum + 1)
			continue;

		P_SpawnVoodooDoll(p, point);
	}
}


// number of wanted dogs (1-3)
DEF_CVAR(dogs, "0", CVAR_ARCHIVE)

void G_SpawnHelper(int pnum)
{
	if (pnum == 0)
		return;

	if (pnum > dogs.d)
		return;

	spawnpoint_t *point = G_FindCoopPlayer(pnum+1);
	if (point == NULL)
		return;

	const mobjtype_c *info = mobjtypes.Lookup(888);
	if (info == NULL)
		return;

	mobj_t *mo = P_MobjCreateObject(point->x, point->y, point->z, info);

	mo->angle = point->angle;
	mo->spawnpoint = *point;

	mo->side = ~0;
}


bool G_CheckConditions(mobj_t *mo, condition_check_t *cond)
{
	player_t *p = mo->player;
	bool temp;

	for (; cond; cond = cond->next)
	{
		int i_amount = (int)(cond->amount + 0.5f);

		switch (cond->cond_type)
		{
			case COND_Health:
				if (cond->exact)
					return (mo->health == cond->amount);
				
				temp = (mo->health >= cond->amount);

				if ((!cond->negate && !temp) || (cond->negate && temp))
					return false;

				break;

			case COND_Armour:
				if (!p)
					return false;

				if (cond->exact)
				{
					if (cond->sub.type == ARMOUR_Total)
						return (p->totalarmour == i_amount);
					else
						return (p->armours[cond->sub.type] == i_amount);
				}

				if (cond->sub.type == ARMOUR_Total)
					temp = (p->totalarmour >= i_amount);
				else
					temp = (p->armours[cond->sub.type] >= i_amount);

				if ((!cond->negate && !temp) || (cond->negate && temp))
					return false;

				break;

			case COND_Key:
				if (!p)
					return false;

				temp = ((p->cards & cond->sub.type) != 0);

				if ((!cond->negate && !temp) || (cond->negate && temp))
					return false;

				break;

			case COND_Weapon:
				if (!p)
					return false;

				temp = false;

				for (int i=0; i < MAXWEAPONS; i++)
				{
					if (p->weapons[i].owned &&
						p->weapons[i].info == cond->sub.weap)
					{
						temp = true;
						break;
					}
				}

				if ((!cond->negate && !temp) || (cond->negate && temp))
					return false;

				break;

			case COND_Powerup:
				if (!p)
					return false;

				if (cond->exact)
					return (p->powers[cond->sub.type] == cond->amount);

				temp = (p->powers[cond->sub.type] > cond->amount);

				if ((!cond->negate && !temp) || (cond->negate && temp))
					return false;

				break;

			case COND_Ammo:
				if (!p)
					return false;

				if (cond->exact)
					return (p->ammo[cond->sub.type].num == i_amount);

				temp = (p->ammo[cond->sub.type].num >= i_amount);

				if ((!cond->negate && !temp) || (cond->negate && temp))
					return false;

				break;
				
			case COND_Inventory:
				if (!p)
					return false;

				if (cond->exact)
					return (p->inventory[cond->sub.type].num == i_amount);

				temp = (p->inventory[cond->sub.type].num >= i_amount);

				if ((!cond->negate && !temp) || (cond->negate && temp))
					return false;

				break;

			case COND_Counter:
				if (!p)
					return false;

				if (cond->exact)
					return (p->counters[cond->sub.type].num == i_amount);

				temp = (p->counters[cond->sub.type].num >= i_amount);

				if ((!cond->negate && !temp) || (cond->negate && temp))
					return false;

				break;

			case COND_Jumping:
				if (!p)
					return false;

				temp = (p->jumpwait > 0);

				if ((!cond->negate && !temp) || (cond->negate && temp))
					return false;

				break;

			case COND_Crouching:
				if (!p)
					return false;

				temp = (mo->extendedflags & EF_CROUCHING) ? true : false;

				if ((!cond->negate && !temp) || (cond->negate && temp))
					return false;

				break;

			case COND_Swimming:
				if (!p)
					return false;

				temp = p->swimming;

				if ((!cond->negate && !temp) || (cond->negate && temp))
					return false;

				break;

			case COND_Attacking:
				if (!p)
					return false;

				temp = p->attackdown[0] || p->attackdown[1];

				if ((!cond->negate && !temp) || (cond->negate && temp))
					return false;

				break;

			case COND_Rampaging:
				if (!p)
					return false;

				temp = (p->attackdown_count >= 70);

				if ((!cond->negate && !temp) || (cond->negate && temp))
					return false;

				break;

			case COND_Using:
				if (!p)
					return false;

				temp = p->usedown;

				if ((!cond->negate && !temp) || (cond->negate && temp))
					return false;

				break;

			case COND_Action1:
				if (!p)
					return false;

				temp = p->actiondown[0];

				if ((!cond->negate && !temp) || (cond->negate && temp))
					return false;

				break;

			case COND_Action2:
				if (!p)
					return false;

				temp = p->actiondown[1];

				if ((!cond->negate && !temp) || (cond->negate && temp))
					return false;

				break;

			case COND_Walking:
				if (!p)
					return false;

				temp = (p->actual_speed > PLAYER_STOPSPEED) && (p->mo->z <= p->mo->floorz);

				if ((!cond->negate && !temp) || (cond->negate && temp))
					return false;

				break;

			case COND_NONE:
			default:
				// unknown condition -- play it safe and succeed
				break;
		}
	}

	// all conditions succeeded
	return true;
}

void G_AddDeathmatchStart(const spawnpoint_t& point)
{
	dm_starts.push_back(point);
}

void G_AddHubStart(const spawnpoint_t& point)
{
	hub_starts.push_back(point);
}

void G_AddCoopStart(const spawnpoint_t& point)
{
	coop_starts.push_back(point);
}

void G_AddVoodooDoll(const spawnpoint_t& point)
{
	voodoo_dolls.push_back(point);
}

spawnpoint_t *G_FindCoopPlayer(int pnum)
{
	for (int i = 0; i < (int)coop_starts.size(); i++)
	{
		spawnpoint_t *point = &coop_starts[i];
		SYS_ASSERT(point->info);

		if (point->info->playernum == pnum)
			return point;
	}

	return NULL;  // not found
}


void G_MarkPlayerAvatars(void)
{
	for (int i = 0; i < MAXPLAYERS; i++)
	{
		player_t *p = players[i];

		if (p && p->mo)
			p->mo->hyperflags |= HF_OLD_AVATAR;
	}
}

void G_RemoveOldAvatars(void)
{
	mobj_t *mo;
	mobj_t *next;

	// first fix up any references
	for (mo = mobjlisthead; mo; mo = next)
	{
		next = mo->next;

		// update any mobj_t pointer which referred to the old avatar
		// (the one which was saved in the savegame) to refer to the
		// new avatar (the one spawned after loading).

		if (mo->target && (mo->target->hyperflags & HF_OLD_AVATAR))
		{
			SYS_ASSERT(mo->target->player);
			SYS_ASSERT(mo->target->player->mo);

// I_Debugf("Updating avatar reference: %p --> %p\n", mo->target, mo->target->player->mo);

			mo->SetTarget(mo->target->player->mo);		
		}

		if (mo->source && (mo->source->hyperflags & HF_OLD_AVATAR))
		{
			mo->SetSource(mo->source->player->mo);		
		}

		if (mo->supportobj && (mo->supportobj->hyperflags & HF_OLD_AVATAR))
		{
			mo->SetSupportObj(mo->supportobj->player->mo);		
		}

		// the other three fields don't matter (tracer, above_mo, below_mo)
		// because the will be nulled by the P_RemoveMobj() below.
	}

	// now actually remove the old avatars
	for (mo = mobjlisthead; mo; mo = next)
	{
		next = mo->next;

		if (mo->hyperflags & HF_OLD_AVATAR)
		{
			I_Debugf("Removing old avatar: %p\n", mo);

			P_RemoveMobj(mo);
		}
	}
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
