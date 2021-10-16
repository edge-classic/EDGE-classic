//----------------------------------------------------------------------------
//  EDGE Creature Action Code
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
//
// -KM- 1998/09/27 Sounds.ddf stuff
//
// -AJA- 1999/07/21: Replaced some non-critical P_Randoms with M_Random,
//       and removed some X_Random()-X_Random() things.
//

#include "i_defs.h"

#include "dm_data.h"
#include "dm_state.h"
#include "g_game.h"
#include "m_random.h"
#include "p_action.h"
#include "p_local.h"
#include "s_sound.h"
#include "w_wad.h"
#include "z_zone.h"

#include <float.h>

dirtype_e opposite[] =
{
	DI_WEST,
	DI_SOUTHWEST,
	DI_SOUTH,
	DI_SOUTHEAST,
	DI_EAST,
	DI_NORTHEAST,
	DI_NORTH,
	DI_NORTHWEST,
	DI_NODIR
};

dirtype_e diags[] =
{
	DI_NORTHWEST,
	DI_NORTHEAST,
	DI_SOUTHWEST,
	DI_SOUTHEAST
};

// sqrt(2) / 2: The diagonal speed of creatures
#define SQ2 0.7071067812f

float xspeed[8] = {1.0f, SQ2, 0.0f, -SQ2, -1.0f, -SQ2, 0.0f, SQ2};
float yspeed[8] = {0.0f, SQ2, 1.0f, SQ2, 0.0f, -SQ2, -1.0f, -SQ2};

#undef SQ2

//
//  ENEMY THINKING
//
// Enemies are allways spawned
// with targetplayer = -1, threshold = 0
// Most monsters are spawned unaware of all players,
// but some can be made preaware
//

//
// Called by P_NoiseAlert.
// Recursively traverse adjacent sectors,
// sound blocking lines cut off traversal.
//
static void RecursiveSound(sector_t * sec, int soundblocks, int player)
{
	int i;
	line_t *check;
	sector_t *other;

	// has the sound flooded this sector
	if (sec->validcount == validcount && sec->soundtraversed <= soundblocks + 1)
		return;

	// wake up all monsters in this sector
	sec->validcount = validcount;
	sec->soundtraversed = soundblocks + 1;
	sec->sound_player = player;

	for (i = 0; i < sec->linecount; i++)
	{
		check = sec->lines[i];

		if (!(check->flags & MLF_TwoSided))
			continue;

		// -AJA- 1999/07/19: Gaps are now stored in line_t.
		if (check->gap_num == 0)
			continue;  // closed door

		// -AJA- 2001/11/11: handle closed Sliding doors
		if (check->slide_door && ! check->slide_door->s.see_through &&
			! check->slider_move)
		{
			continue;
		}

		if (check->frontsector == sec)
			other = check->backsector;
		else
			other = check->frontsector;

		if (check->flags & MLF_SoundBlock)
		{
			if (!soundblocks)
				RecursiveSound(other, 1, player);
		}
		else
		{
			RecursiveSound(other, soundblocks, player);
		}
	}
}

void P_NoiseAlert(player_t *p)
{
	validcount++;

	RecursiveSound(p->mo->subsector->sector, 0, p->pnum);
}

//
// Move in the current direction,
// returns false if the move is blocked.
//
bool P_Move(mobj_t * actor, bool path)
{
	vec3_t orig_pos;
	
	orig_pos.Set(actor->x, actor->y, actor->z);

	float tryx;
	float tryy;

	if (path)
	{
		tryx = actor->x + actor->speed * M_Cos(actor->angle);
		tryy = actor->y + actor->speed * M_Sin(actor->angle);
	}
	else
	{
		if (actor->movedir == DI_NODIR)
			return false;

		if ((unsigned)actor->movedir >= 8)
			I_Error("Weird actor->movedir!");

		tryx = actor->x + actor->speed * xspeed[actor->movedir];
		tryy = actor->y + actor->speed * yspeed[actor->movedir];
	}

	if (! P_TryMove(actor, tryx, tryy))
	{
		epi::array_iterator_c it;
		line_t* ld;
		
		// open any specials
		if (actor->flags & MF_FLOAT && floatok)
		{
			// must adjust height
			if (actor->z < float_destz)
				actor->z += actor->info->float_speed;
			else
				actor->z -= actor->info->float_speed;

			actor->flags |= MF_INFLOAT;
			// FIXME: position interpolation
			return true;
		}

		if (spechit.GetSize() == 0)
			return false;

		actor->movedir = DI_NODIR;

		// -AJA- 1999/09/10: As Lee Killough points out, this is where
		//       monsters can get stuck in doortracks.  We follow Lee's
		//       method: return true 90% of the time if the blocking line
		//       was the one activated, or false 90% of the time if there
		//       was some other line activated.

		bool any_used = false;
		bool block_used = false;
					
		for (it=spechit.GetTailIterator(); it.IsValid(); it--)
		{
			ld = ITERATOR_TO_TYPE(it, line_t*);
			if (P_UseSpecialLine(actor, ld, 0, -FLT_MAX, FLT_MAX))
			{
				any_used = true;

				if (ld == blockline)
					block_used = true;
			}
		}
		
		return any_used && (P_Random() < 230 ? block_used : !block_used);
	}

	actor->flags &= ~MF_INFLOAT;

	if (!(actor->flags & MF_FLOAT) &&
		!(actor->extendedflags & EF_GRAVFALL))
		actor->z = actor->floorz;

	// -AJA- 2008/01/16: position interpolation
	if ((actor->state->flags & SFF_Model) ||
		(actor->flags & MF_FLOAT))
	{
		actor->lerp_num = CLAMP(2, actor->state->tics, 10);
		actor->lerp_pos = 1;

		actor->lerp_from = orig_pos;
	}

	return true;
}

//
// Attempts to move actor on
// in its current (ob->moveangle) direction.
// If blocked by either a wall or an actor
// returns FALSE
// If move is either clear or blocked only by a door,
// returns TRUE and sets...
// If a door is in the way,
// an OpenDoor call is made to start it opening.
//
static bool TryWalk(mobj_t * actor)
{
	if (!P_Move(actor, false))
		return false;

	actor->movecount = P_Random() & 15;
	return true;
}

// -ACB- 1998/09/06 actor is now an object; different movement choices.
void P_NewChaseDir(mobj_t * object)
{
	float deltax;
	float deltay;
	dirtype_e tdir;

	dirtype_e d[3];
	dirtype_e olddir;
	dirtype_e turnaround;

	olddir = object->movedir;
	turnaround = opposite[olddir];

	//
	// Movement choice: Previously this was calculation to find
	// the distance between object and target: if the object had
	// no target, a fatal error was returned. However it is now
	// possible to have movement without a target. if the object
	// has a target, go for that; else if it has a supporting
	// object aim to go within supporting distance of that; the
	// remaining option is to walk aimlessly: the target destination
	// is always 128 in the old movement direction, think
	// of it like the donkey and the carrot sketch: the donkey will
	// move towards the carrot, but since the carrot is always a
	// set distance away from the donkey, the rather stupid mammal
	// will spend eternity trying to get the carrot and will walk
	// forever.
	//
	// -ACB- 1998/09/06

	if (object->target)
	{
		deltax = object->target->x - object->x;
		deltay = object->target->y - object->y;
	}
	else if (object->supportobj)
	{
		// not too close
		deltax = (object->supportobj->x - object->x) - (object->supportobj->radius * 4);
		deltay = (object->supportobj->y - object->y) - (object->supportobj->radius * 4);
	}
	else
	{
		deltax = 128 * xspeed[olddir];
		deltay = 128 * yspeed[olddir];
	}

	if (deltax > 10)
		d[1] = DI_EAST;
	else if (deltax < -10)
		d[1] = DI_WEST;
	else
		d[1] = DI_NODIR;

	if (deltay < -10)
		d[2] = DI_SOUTH;
	else if (deltay > 10)
		d[2] = DI_NORTH;
	else
		d[2] = DI_NODIR;

	// try direct route
	if (d[1] != DI_NODIR && d[2] != DI_NODIR)
	{
		object->movedir = diags[((deltay < 0) << 1) + (deltax > 0)];
		if (object->movedir != turnaround && TryWalk(object))
			return;
	}

	// try other directions
	if (P_Random() > 200 || fabs(deltay) > fabs(deltax))
	{
		tdir = d[1];
		d[1] = d[2];
		d[2] = tdir;
	}

	if (d[1] == turnaround)
		d[1] = DI_NODIR;

	if (d[2] == turnaround)
		d[2] = DI_NODIR;

	if (d[1] != DI_NODIR)
	{
		object->movedir = d[1];
		if (TryWalk(object))
		{
			// either moved forward or attacked
			return;
		}
	}

	if (d[2] != DI_NODIR)
	{
		object->movedir = d[2];

		if (TryWalk(object))
			return;
	}

	// there is no direct path to the player,
	// so pick another direction.
	if (olddir != DI_NODIR)
	{
		object->movedir = olddir;

		if (TryWalk(object))
			return;
	}

	// randomly determine direction of search
	if (P_Random() & 1)
	{
		for (tdir = DI_EAST; tdir <= DI_SOUTHEAST; tdir = (dirtype_e)((int)tdir+1))
		{
			if (tdir != turnaround)
			{
				object->movedir = tdir;

				if (TryWalk(object))
					return;
			}
		}
	}
	else
	{
		for (tdir = DI_SOUTHEAST; tdir != (dirtype_e)(DI_EAST - 1); tdir = (dirtype_e)((int)tdir-1))
		{
			if (tdir != turnaround)
			{
				object->movedir = tdir;

				if (TryWalk(object))
					return;
			}
		}
	}

	if (turnaround != DI_NODIR)
	{
		object->movedir = turnaround;
		if (TryWalk(object))
			return;
	}

	// cannot move
	object->movedir = DI_NODIR;
}

//
// Range is angle range on either side of eyes, 90 degrees for normal
// view, 180 degrees for total sight in all dirs.
//
// Returns true if a player is targeted.
//
bool P_LookForPlayers(mobj_t * actor, angle_t range)
{
	int c;
	int stop;
	player_t *player;
	angle_t an;
	float dist;

	c = 0;
	stop = (actor->lastlook - 1 + MAXPLAYERS) % MAXPLAYERS;

	for (; actor->lastlook != stop; actor->lastlook = (actor->lastlook + 1) % MAXPLAYERS)
	{
		player = players[actor->lastlook];

		if (!player)
			continue;

		SYS_ASSERT(player->mo);

		// done looking ?
		if (c++ >= 2)
			break;

		// dead ?
		if (player->health <= 0)
			continue;

		// on the same team ?
		if ((actor->side & player->mo->side) != 0)
			continue;

		if (range < ANG180)
		{
			an = R_PointToAngle(actor->x, actor->y, player->mo->x,
				player->mo->y) - actor->angle;

			if (range <= an && an <= (range * -1))
			{
				// behind back.
				// if real close, react anyway
				dist = P_ApproxDistance(player->mo->x - actor->x,
					player->mo->y - actor->y);

				if (dist > MELEERANGE)
					continue;
			}
		}

		// out of sight ?
		if (!P_CheckSight(actor, player->mo))
			continue;

		actor->SetTarget(player->mo);
		return true;
	}

	return false;
}

//
//   BOSS-BRAIN HANDLING
//

//
// -AJA- Savegames: we assume that the spit-spot objects never
//       disappear, or new ones appear.  After all, they have to be
//       there to be the target of the cubes.  This means we don't
//       need to save anything: the set of shoot-spots will be
//       regenerated after the loadgame when the BrainShooter next
//       tries to shoot a cube.

shoot_spot_info_t brain_spots = { -1, NULL };

void P_LookForShootSpots(const mobjtype_c *spot_type)
{
	int i;
	mobj_t *cur;

	brain_spots.number = 0;

	// count them
	for (cur=mobjlisthead; cur != NULL; cur=cur->next)
	{
		if (cur->info == spot_type)
			brain_spots.number++;
	}

	if (brain_spots.number == 0)
	{
		I_Warning("No [%s] objects found for BossBrain shooter.\n",
			spot_type->name.c_str());
		return;
	}

	// create the spots
	brain_spots.targets = new mobj_t* [brain_spots.number];

	for (cur=mobjlisthead, i=0; cur != NULL; cur=cur->next)
	{
		if (cur->info == spot_type)
			brain_spots.targets[i++] = cur;
	}

	SYS_ASSERT(i == brain_spots.number);
}

void P_FreeShootSpots(void)
{
	if (brain_spots.number < 0)
		return;

	if (brain_spots.targets > 0)
	{
		SYS_ASSERT(brain_spots.targets);

		delete[] brain_spots.targets;
	}

	brain_spots.number = -1;
	brain_spots.targets = NULL;
}

static void SpawnDeathMissile(mobj_t *source, float x, float y, float z)
{
	const mobjtype_c *info;
	mobj_t *th;

	info = mobjtypes.Lookup("BRAIN_DEATH_MISSILE");

	th = P_MobjCreateObject(x, y, z, info);
	if (th->info->seesound)
		S_StartFX(th->info->seesound, P_MobjGetSfxCategory(th), th);

	th->SetRealSource(source);

	th->mom.x = (x - source->x) / 50.0f;
	th->mom.y = -0.25f;
	th->mom.z = (z - source->z) / 50.0f;

	th->tics -= M_Random() & 7;

	if (th->tics < 1)
		th->tics = 1;
}

void P_ActBrainScream(mobj_t * bossbrain)
{
	// The brain and his pain...

	float x, y, z;
	float min_x, max_x;

	min_x = bossbrain->x - 280.0f;
	max_x = bossbrain->x + 280.0f;

	for (x = min_x; x < max_x; x += 4)
	{
		y = bossbrain->y - 320.0f;
		z = bossbrain->z + (P_Random() - 180.0f) * 2.0f;

		SpawnDeathMissile(bossbrain, x, y, z);
	}

	if (bossbrain->info->deathsound)
		S_StartFX(bossbrain->info->deathsound, P_MobjGetSfxCategory(bossbrain), bossbrain);
}

void P_ActBrainMissileExplode(mobj_t * mo)
{
	float x, y, z;

	if (! mo->source)
		return;

	x = mo->source->x + (P_Random() - 128.0f) * 4.0f;
	y = mo->source->y - 320.0f;
	z = mo->source->z + (P_Random() - 180.0f) * 2.0f;

	SpawnDeathMissile(mo->source, x, y, z);
}

void P_ActBrainDie(mobj_t * bossbrain)
{
	G_ExitLevel(TICRATE);
}

void P_ActBrainSpit(mobj_t * shooter)
{
	static int easy = 0;

	// when skill is easy, only fire every second cube.

	easy ^= 1;

	if (gameskill <= sk_easy && (!easy))
		return;

	// shoot out a cube
	P_ActRangeAttack(shooter);
}


void P_ActCubeSpawn(mobj_t * cube)
{
	mobj_t *targ;
	mobj_t *newmobj;
	const mobjtype_c *type;
	int r;

	targ = cube->target;

	// -AJA- 2007/07/28: workaround for DeHackEd patches using S_SPAWNFIRE
	if (!targ || !cube->currentattack ||
		cube->currentattack->attackstyle != ATK_SHOOTTOSPOT)
		return;
	
	// Randomly select monster to spawn.
	r = P_Random();

	// Probability distribution (kind of :)),
	// decreasing likelihood.
	if (r < 50)
		type = mobjtypes.Lookup("IMP");
	else if (r < 90)
		type = mobjtypes.Lookup("DEMON");
	else if (r < 120)
		type = mobjtypes.Lookup("SPECTRE");
	else if (r < 130)
		type = mobjtypes.Lookup("PAIN_ELEMENTAL");
	else if (r < 160)
		type = mobjtypes.Lookup("CACODEMON");
	else if (r < 162)
		type = mobjtypes.Lookup("ARCHVILE");
	else if (r < 172)
		type = mobjtypes.Lookup("REVENANT");
	else if (r < 192)
		type = mobjtypes.Lookup("ARACHNOTRON");
	else if (r < 222)
		type = mobjtypes.Lookup("MANCUBUS");
	else if (r < 246)
		type = mobjtypes.Lookup("HELL_KNIGHT");
	else
		type = mobjtypes.Lookup("BARON_OF_HELL");

	newmobj = P_MobjCreateObject(targ->x, targ->y, targ->z, type);

	if (P_LookForPlayers(newmobj, ANG180))
	{
		if (newmobj->info->chase_state)
			P_SetMobjState(newmobj, newmobj->info->chase_state);
		else
			P_SetMobjState(newmobj, newmobj->info->spawn_state);
	}

	// telefrag anything in this spot
	P_TeleportMove(newmobj, newmobj->x, newmobj->y, newmobj->z);
}

void P_ActPlayerScream(mobj_t * mo)
{
	sfx_t *sound;

	sound = mo->info->deathsound;

	if ((mo->health < -50) && (W_CheckNumForName("DSPDIEHI") >= 0))
	{
		// if the player dies and unclipped health is < -50%...

		sound = sfxdefs.GetEffect("PDIEHI");
	}

	S_StartFX(sound, P_MobjGetSfxCategory(mo), mo);
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
