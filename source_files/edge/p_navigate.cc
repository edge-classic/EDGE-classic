//----------------------------------------------------------------------------
//  EDGE Navigation System
//----------------------------------------------------------------------------
//
//  Copyright (c) 2022  The EDGE Team.
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

#include "i_defs.h"

#include "con_main.h"
#include "dm_data.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "m_random.h"
#include "p_bot.h"
#include "p_local.h"
#include "p_mobj.h"
#include "p_navigate.h"
#include "r_defs.h"
#include "r_misc.h"
#include "r_state.h"

// DDF
#include "main.h"
#include "thing.h"


class big_item_c
{
public:
	float x = 0;
	float y = 0;
	float z = 0;
	float score = 0;
};

static std::vector<big_item_c> big_items;


static float NAV_EvaluateBigItem(const mobj_t *mo)
{
	ammotype_e ammotype;

	for (const benefit_t *B = mo->info->pickup_benefits ; B != NULL ; B = B->next)
	{
		switch (B->type)
		{
			case BENEFIT_Weapon:
				// crude guess of powerfulness based on ammo
				ammotype = B->sub.weap->ammo[0];

				switch (ammotype)
				{
					case AM_NoAmmo: return 25;
					case AM_Bullet: return 50;
					case AM_Shell:  return 60;
					case AM_Rocket: return 70;
					case AM_Cell:   return 80;
					default:        return 65;
				}
				break;

			case BENEFIT_Powerup:
				// powerups are rare in DM, these are most useful for a bot
				switch (B->sub.type)
				{
					case PW_Invulnerable: return 100;
					case PW_PartInvis:    return 15;
					default:              return 0;
				}
				break;

			case BENEFIT_Ammo:
				// ignored here
				break;

			case BENEFIT_Health:
				// ignore small amounts (e.g. potions, stimpacks)
				if (B->amount >= 100)
					return 40;
				break;

			case BENEFIT_Armour:
				// ignore small amounts (e.g. helmets)
				if (B->amount >= 50)
					return 20;
				break;

			default:
				break;
		}
	}

	return 0;
}


static void NAV_CollectBigItems()
{
	// collect the location of all the significant pickups on the map.
	// the main purpose of this is allowing the bots to roam, since big
	// items (e.g. weapons) tend to be well distributed around a map.
	// it will also be useful for finding a weapon after spawning.

	for (const mobj_t *mo = mobjlisthead ; mo != NULL ; mo = mo->next)
	{
		if ((mo->flags & MF_SPECIAL) == 0)
			continue;

		float score = NAV_EvaluateBigItem(mo);
		if (score <= 0)
			continue;

		big_items.push_back(big_item_c { mo->x, mo->y, mo->z + 8.0f, score });
	}

	// FIXME : if < 4 or so, try small items and/or player spawn points
}


position_c NAV_NextRoamPoint(bot_t *bot)
{
	// TODO ensure it is not different to the last few chosen points

	if (big_items.empty())
	{
		return position_c { 0, 0, 0 };
	}

	int idx = C_Random() % (int)big_items.size();

	const big_item_c& item = big_items[idx];

	return position_c { item.x, item.y, item.z };
}

//----------------------------------------------------------------------------
//  A* PATHING ALGORITHM
//----------------------------------------------------------------------------

// NOTE: for the A* algorithm, we use *time* in seconds for measuring
//       the cost of travelling between two nodes.

// player travel speed when running, in map units per second.
#define RUNNING_SPEED  500.0


enum astar_state_e
{
	AST_Unseen = 0,   // not visited yet
	AST_Open,         // in the open set
	AST_Closed,       // in the closed set
};


class nav_area_c
{
public:
	int id         = 0;
	int first_link = 0;
	int num_links  = 0;

	// info for A* path finding...

	astar_state_e  state = AST_Unseen;
	nav_area_c  * parent = NULL;
	double G = 0;  // cost of this node (from start node)
	double H = 0;  // estimated cost to reach end node

	nav_area_c(int _id) : id(_id)
	{ }

	~nav_area_c()
	{ }
};


class nav_link_c
{
public:
	nav_area_c * dest = NULL;
	float length = 0;
};


// there is a one-to-one correspondence from a subsector_t to a
// nav_area_c in this vector.
static std::vector<nav_area_c> nav_areas;
static std::vector<nav_link_c> nav_links;


static nav_area_c * NavArea(const subsector_t *sub)
{
	if (sub == NULL)
		return NULL;

	int id = (int)(sub - subsectors);

	SYS_ASSERT(id >= 0 && id < numsubsectors);

	return &nav_areas[id];
}


static void NAV_CreateLinks()
{
	for (int i = 0 ; i < numsubsectors ; i++)
	{
		nav_areas.push_back(nav_area_c(i));
	}

	for (int i = 0 ; i < numsubsectors ; i++)
	{
		const subsector_t& sub = subsectors[i];

		nav_area_c &area = nav_areas[i];
		area.first_link = (int)nav_links.size();

		for (const seg_t *seg = sub.segs ; seg != NULL ; seg=seg->sub_next)
		{
			nav_area_c *dest = NavArea(seg->back_sub);

			// no link for a one-sided wall
			if (dest == NULL)
				continue;

			// ignore player-blocking lines
			if (! seg->miniseg)
				if (0 != (seg->linedef->flags & (MLF_Blocking | MLF_BlockPlayers)))
					continue;

			// NOTE: a big height difference is allowed here, it is checked
			//       during play (and we need to allow lowering floors etc).

			// FIXME compute length !!
			float length = 64.0;

			// WISH: check if link is blocked by obstacles

			nav_links.push_back(nav_link_c { dest, length });
			area.num_links += 1;

			//DEBUG
			// fprintf(stderr, "link area %d --> %d\n", area.id, dest->id);
		}
	}
}


static int NAV_LowestOpenF()
{
	// return index of the nav_area_c which is in the OPEN set and has the
	// lowest F value, where F = G + H.  returns -1 if OPEN set is empty.

	// this is a brute force search -- consider OPTIMISING it...

	int result = -1;
	double best_F = 9e99;

	for (int i = 0 ; i < (int)nav_areas.size() ; i++)
	{
		const nav_area_c& area = nav_areas[i];

		if (area.state == AST_Open)
		{
			double F = area.G + area.H;
			if (F < best_F)
			{
				best_F = F;
				result = i;
			}
		}
	}

	return result;
}


static void NAV_OpenNode(nav_area_c *area, nav_area_c *parent, double G, double H)
{
	area->state  = AST_Open;
	area->parent = parent;
	area->G      = G;
	area->H      = H;
}


static void NAV_StorePath()
{
	// TODO
}



static bool NAV_FindPath(std::vector<subsector_t *> path, subsector_t *start, subsector_t *finish, int flags)
{
	// tries to find a path from start to finish (subsectors).
	// if successful, returns true and 'path' vector will contain all the
	// subsectors along the path (including finish but excluding start).
	// the path may include manual lifts and doors, but more complicated
	// things (a door activated by a nearby switch) will fail.

	SYS_ASSERT(start);
	SYS_ASSERT(finish);

	// prepare all nodes
	for (nav_area_c& area : nav_areas)
	{
		area.state = AST_Unseen;
	}

	NAV_OpenNode(NavArea(start), NULL, 0, 999 /* FIXME: H */);

	for (;;)
	{
		int cur = NAV_LowestOpenF();

		// no path at all?
		if (cur < 0)
			return false;

		if (&subsectors[cur] == finish)
		{
			NAV_StorePath();
			return true;
		}

		nav_area_c& area = nav_areas[cur];
		area.state = AST_Closed;

		// visit each neighbor node
		for (int k = 0 ; k < area.num_links ; k++)
		{
			const nav_link_c& link = nav_links[area.first_link + k];

			// TODO
		}
	}

	return false;
}

//----------------------------------------------------------------------------

void NAV_AnalyseLevel()
{
	NAV_FreeLevel();

	NAV_CollectBigItems();
	NAV_CreateLinks();
}


void NAV_FreeLevel()
{
	big_items.clear();
	nav_areas.clear();
	nav_links.clear();
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
