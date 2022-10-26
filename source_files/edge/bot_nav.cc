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

#include "bot_nav.h"
#include "bot_think.h"
#include "con_main.h"
#include "dm_data.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "m_bbox.h"
#include "m_random.h"
#include "p_local.h"
#include "p_mobj.h"
#include "r_defs.h"
#include "r_misc.h"
#include "r_state.h"

// DDF
#include "main.h"
#include "thing.h"

#include <algorithm>

class big_item_c
{
public:
	float x = 0;
	float y = 0;
	float z = 0;
	float score = 0;
};

static std::vector<big_item_c> big_items;


float NAV_EvaluateBigItem(const mobj_t *mo)
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
					default:              return -1;
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

	return -1;
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
		if (score < 0)
			continue;

		big_items.push_back(big_item_c { mo->x, mo->y, mo->z + 8.0f, score });
	}

	// FIXME : if < 4 or so, try small items and/or player spawn points
}


bool NAV_NextRoamPoint(position_c& out)
{
	if (big_items.empty())
		return false;

	for (int loop = 0 ; loop < 100 ; loop++)
	{
		int idx = C_Random() % (int)big_items.size();

		const big_item_c& item = big_items[idx];

		float dx = fabs(item.x - out.x);
		float dy = fabs(item.y - out.y);

		// too close to last goal?
		if (dx < 200 && dy < 200)
			continue;

		out.x = item.x;
		out.y = item.y;
		out.z = item.z;

		return true;
	}

	return false;
}


//----------------------------------------------------------------------------
//  A* PATHING ALGORITHM
//----------------------------------------------------------------------------

// NOTE: for the A* algorithm, we use *time* in seconds for measuring
//       the cost of travelling between two nodes.

// player travel speed when running, in map units per second.
#define RUNNING_SPEED  500.0


class nav_area_c
{
public:
	int id         = 0;
	int first_link = 0;
	int num_links  = 0;

	// info for A* path finding...

	bool open  = false;  // in the OPEN set?
	int parent = -1;     // parent nav_area_c / subsector_t
	float G    =  0;     // cost of this node (from start node)
	float H    =  0;     // estimated cost to reach end node

	nav_area_c(int _id) : id(_id)
	{ }

	~nav_area_c()
	{ }
};


class nav_link_c
{
public:
	int   dest_id = -1;
	float length  =  0;
};


// there is a one-to-one correspondence from a subsector_t to a
// nav_area_c in this vector.
static std::vector<nav_area_c> nav_areas;
static std::vector<nav_link_c> nav_links;

static position_c nav_finish_mid;


static nav_area_c * NavArea(const subsector_t *sub)
{
	if (sub == NULL)
		return NULL;

	int id = (int)(sub - subsectors);

	SYS_ASSERT(id >= 0 && id < numsubsectors);

	return &nav_areas[id];
}


static position_c NAV_CalcMiddle(const subsector_t *sub)
{
	position_c result;

	result.x = (sub->bbox[BOXLEFT] + sub->bbox[BOXRIGHT])  * 0.5;
	result.y = (sub->bbox[BOXTOP]  + sub->bbox[BOXBOTTOM]) * 0.5;
	result.z = sub->sector->f_h;

	return result;
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
			// no link for a one-sided wall
			if (seg->back_sub == NULL)
				continue;

			int dest_id = (int)(seg->back_sub - subsectors);

			// ignore player-blocking lines
			if (! seg->miniseg)
				if (0 != (seg->linedef->flags & (MLF_Blocking | MLF_BlockPlayers)))
					continue;

			// NOTE: a big height difference is allowed here, it is checked
			//       during play (since we need to allow lowering floors etc).

			// WISH: check if link is blocked by obstacle things

			// compute length of link
			auto p1 = NAV_CalcMiddle(&sub);
			auto p2 = NAV_CalcMiddle(seg->back_sub);

			float length = R_PointToDist(p1.x, p1.y, p2.x, p2.y);

			nav_links.push_back(nav_link_c { dest_id, length });
			area.num_links += 1;

			//DEBUG
			// fprintf(stderr, "link area %d --> %d\n", area.id, dest_id);
		}
	}
}


static float NAV_TraverseLinkCost(int cur, const nav_link_c& link, bool allow_doors)
{
	const sector_t *s1 = subsectors[cur].sector;
	const sector_t *s2 = subsectors[link.dest_id].sector;

	// too big a step up?   FIXME check for a manual lift
	if (s2->f_h > s1->f_h + 24.0f)
		return -1;

	// not enough vertical space?   FIXME check for a manual door
	float high_f = std::max(s1->f_h, s2->f_h);
	float  low_c = std::min(s1->c_h, s2->c_h);

	if (low_c - high_f < 56.0f)
		return -1;

	// TODO if a drop-off, compute time to fall

	return link.length / RUNNING_SPEED;
}


static float NAV_EstimateH(const subsector_t * cur_sub)
{
	auto p = NAV_CalcMiddle(cur_sub);

	float dist = R_PointToDist(p.x, p.y, nav_finish_mid.x, nav_finish_mid.y);

	float time = dist / RUNNING_SPEED;

	// over-estimate, to account for height changes, obstacles etc
	return time * 1.25f;
}


static int NAV_LowestOpenF()
{
	// return index of the nav_area_c which is in the OPEN set and has the
	// lowest F value, where F = G + H.  returns -1 if OPEN set is empty.

	// this is a brute force search -- consider OPTIMISING it...

	int result = -1;
	float best_F = 9e18;

	for (int i = 0 ; i < (int)nav_areas.size() ; i++)
	{
		const nav_area_c& area = nav_areas[i];

		if (area.open)
		{
			float F = area.G + area.H;
			if (F < best_F)
			{
				best_F = F;
				result = i;
			}
		}
	}

	return result;
}


static void NAV_TryOpenArea(int idx, int parent, float cost)
{
	nav_area_c& area = nav_areas[idx];

	if (cost < area.G)
	{
		area.open   = true;
		area.parent = parent;
		area.G      = cost;

		if (area.H == 0)
			area.H = NAV_EstimateH(&subsectors[idx]);
	}
}


static bot_path_c * NAV_StorePath(int start, int finish)
{
	bot_path_c *path = new bot_path_c;

	for (;;)
	{
		path->subs.push_back(finish);

		if (finish == start)
		{
			std::reverse(path->subs.begin(), path->subs.end());
			return path;
		}

		finish = nav_areas[finish].parent;
		SYS_ASSERT(finish >= 0);
	}
}


bot_path_c * NAV_FindPath(subsector_t *start, subsector_t *finish, int flags)
{
	// tries to find a path from start to finish (subsectors).
	// if successful, returns a path (containing indices of subsectors).
	// otherwise returns NULL.
	//
	// the path may include manual lifts and doors, but more complicated
	// things (e.g. a door activated by a nearby switch) will fail.

	SYS_ASSERT(start);
	SYS_ASSERT(finish);

	int start_id  = (int)(start  - subsectors);
	int finish_id = (int)(finish - subsectors);

	if (start == finish)
	{
		return NAV_StorePath(start_id, finish_id);
	}

	// get coordinate of finish subsec
	nav_finish_mid = NAV_CalcMiddle(finish);

	// prepare all nodes
	for (nav_area_c& area : nav_areas)
	{
		area.open   = false;
		area.G      = 9e19;
		area.H      = 0.0;
		area.parent = -1;
	}

	NAV_TryOpenArea(start_id, -1, 0);

	for (;;)
	{
		int cur = NAV_LowestOpenF();

		// no path at all?
		if (cur < 0)
			return NULL;

		// reached the destination?
		if (&subsectors[cur] == finish)
		{
			return NAV_StorePath(start_id, finish_id);
		}

		// move current node to CLOSED set
		nav_area_c& area = nav_areas[cur];
		area.open = false;

		// visit each neighbor node
		for (int k = 0 ; k < area.num_links ; k++)
		{
			const nav_link_c& link = nav_links[area.first_link + k];

			float cost = NAV_TraverseLinkCost(cur, link, true);
			if (cost < 0)
				continue;

			// we need the total traversal time
			cost += area.G;

			// update neighbor if this path is a better one
			NAV_TryOpenArea(link.dest_id, cur, cost);
		}
	}
}

//----------------------------------------------------------------------------

static void NAV_ItemsInSubsector(subsector_t *sub, bot_t *bot, position_c& pos, float radius,
	int sub_id, int& best_id, float& best_score, mobj_t*& best_mo)
{
	for (mobj_t *mo = sub->thinglist ; mo != NULL ; mo = mo->snext)
	{
		float score = bot->EvalItem(mo);
		if (score < 0)
			continue;

		float dist = R_PointToDist(pos.x, pos.y, mo->x, mo->y);
		if (dist > radius)
			continue;

		// very close things get a boost
		if (dist < radius * 0.25f)
			score = score * 2.0f;

		// randomize the score -- to break ties
		score += (float)C_Random() / 65535.0f;

		if (score > best_score)
		{
			best_id    = sub_id;
			best_score = score;
			best_mo    = mo;
		}
	}
}


bot_path_c * NAV_FindThing(bot_t *bot, float radius, mobj_t*& best)
{
	// find an item to pickup or enemy to fight.
	// each nearby thing (limited roughly by `radius') will be passed to the
	// EvalThing() method of the bot.  returns NULL if nothing was found.

	position_c pos { bot->pl->mo->x, bot->pl->mo->y, bot->pl->mo->z };

	subsector_t *start = R_PointInSubsector(pos.x, pos.y);
	int start_id = (int)(start - subsectors);

	// the best thing so far...
	best = NULL;
	float best_score = 0;
	int   best_id = -1;

	// prepare all nodes
	for (nav_area_c& area : nav_areas)
	{
		area.open   = false;
		area.G      = 9e19;
		area.H      = 1.0;  // a constant gives a Djikstra search
		area.parent = -1;
	}

	NAV_TryOpenArea(start_id, -1, 0);

	for (;;)
	{
		int cur = NAV_LowestOpenF();

		// no areas left to visit?
		if (cur < 0)
		{
			if (best == NULL)
				return NULL;

			return NAV_StorePath(start_id, best_id);
		}

		// move current node to CLOSED set
		nav_area_c& area = nav_areas[cur];
		area.open = false;

		// visit the things
		NAV_ItemsInSubsector(&subsectors[cur], bot, pos, radius, cur, best_id, best_score, best);

		// visit each neighbor node
		for (int k = 0 ; k < area.num_links ; k++)
		{
			// check link is passable
			const nav_link_c& link = nav_links[area.first_link + k];

			float cost = NAV_TraverseLinkCost(cur, link, false);
			if (cost < 0)
				continue;

			// we need the total traversal time
			cost += area.G;

			if (cost > (radius * 1.4) / RUNNING_SPEED)
				continue;

			// update neighbor if this path is a better one
			NAV_TryOpenArea(link.dest_id, cur, cost);
		}
	}
}

//----------------------------------------------------------------------------

static void NAV_EnemiesInSubsector(const subsector_t *sub, bot_t *bot, float radius, mobj_t*& best_mo, float& best_score)
{
	for (mobj_t *mo = sub->thinglist ; mo != NULL ; mo = mo->snext)
	{
		if (bot->EvalEnemy(mo) < 0)
			continue;

		float dx = fabs(bot->pl->mo->x - mo->x);
		float dy = fabs(bot->pl->mo->y - mo->y);

		if (dx > radius || dy > radius)
			continue;

		// pick one of the monsters at random
		float score = (float)C_Random() / 65535.0f;

		if (score > best_score)
		{
			best_mo    = mo;
			best_score = score;
		}
	}
}


static void NAV_EnemiesInNode(unsigned int bspnum, bot_t *bot, float radius, mobj_t*& best_mo, float& best_score)
{
	SYS_ASSERT(bspnum >= 0);

	if (bspnum & NF_V5_SUBSECTOR)
	{
		bspnum &= ~NF_V5_SUBSECTOR;
		NAV_EnemiesInSubsector(&subsectors[bspnum], bot, radius, best_mo, best_score);
		return;
	}

	const node_t *node = &nodes[bspnum];

	position_c pos { bot->pl->mo->x, bot->pl->mo->y, bot->pl->mo->z };

	for (int c = 0 ; c < 2 ; c++)
	{
		// reject children outside of the bounds
		if (node->bbox[c][BOXLEFT]   > pos.x + radius) continue;
		if (node->bbox[c][BOXRIGHT]  < pos.x - radius) continue;
		if (node->bbox[c][BOXBOTTOM] > pos.y + radius) continue;
		if (node->bbox[c][BOXTOP]    < pos.y - radius) continue;

		NAV_EnemiesInNode(node->children[c], bot, radius, best_mo, best_score);
	}
}


mobj_t * NAV_FindEnemy(bot_t *bot, float radius)
{
	// find an enemy to fight, or NULL if none found.
	// caller is responsible to do a sight checks.
	// radius is the size of a square box (not a circle).

	mobj_t *best_mo  = NULL;
	float best_score = 0;

	NAV_EnemiesInNode(root_node, bot, radius, best_mo, best_score);

	return best_mo;
}

//----------------------------------------------------------------------------
// DEBUGGING
//----------------------------------------------------------------------------

#if 0

static void NAV_PotionUpPath(bot_path_c * path, subsector_t *start)
{
	const mobjtype_c *type = mobjtypes.Lookup(2014);
	SYS_ASSERT(type);

	for (int idx : path->subs)
	{
		subsector_t *sub = &subsectors[idx];

		position_c p1 = NAV_CalcMiddle(start);
		position_c p2 = NAV_CalcMiddle(sub);

		float length = R_PointToDist(p1.x, p1.y, p2.x, p2.y);

		for (float k = 0 ; k < length ; k += 12.0)
		{
			float x = p1.x + (p2.x - p1.x) * k / length;
			float y = p1.y + (p2.y - p1.y) * k / length;

			P_MobjCreateObject(x, y, ONFLOORZ, type);
		}

		start = sub;
	}
}

static void NAV_DebugFindPath(subsector_t *start, subsector_t *finish)
{
	int start_id  = (int)(start  - subsectors);
	int finish_id = (int)(finish - subsectors);

	if (nav_areas.empty())
		NAV_AnalyseLevel();

	bot_path_c *path = NAV_FindPath(start, finish, 0);

	if (path == NULL)
	{
		CON_Printf("No path from %d --> %d\n", start_id, finish_id);
		return;
	}

	CON_Printf("Path from %d --> %d has %d subsectors\n", start_id, finish_id, (int)path->subs.size());

	NAV_PotionUpPath(path, start);
}

void NAV_DebugFindPath(float x1, float y1, float x2, float y2)
{
	auto start  = R_PointInSubsector(x1, y1);
	auto finish = R_PointInSubsector(x2, y2);

	NAV_DebugFindPath(start, finish);
}

#endif

//----------------------------------------------------------------------------

position_c bot_path_c::calc_target() const
{
	SYS_ASSERT(along < subs.size());

	if (along > 0)
	{
		const subsector_t *src  = &subsectors[subs[along - 1]];
		const subsector_t *dest = &subsectors[subs[along]];

		for (const seg_t *seg = src->segs ; seg != NULL ; seg=seg->sub_next)
		{
			if (seg->back_sub == dest)
			{
				// middle of the adjoining seg
				position_c pos;

				pos.x = (seg->v1->x + seg->v2->x) * 0.5f;
				pos.y = (seg->v1->y + seg->v2->y) * 0.5f;
				pos.z = dest->sector->f_h;

				// compute normal of seg
				/*
				angle_t normal = seg->angle - ANG90;
				float nx = M_Cos(normal);
				float ny = M_Sin(normal);

				pos.x += nx * 30.0f;
				pos.y += ny * 30.0f;
				*/

				return pos;
			}
		}
	}

	return NAV_CalcMiddle(&subsectors[subs[along]]);
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
