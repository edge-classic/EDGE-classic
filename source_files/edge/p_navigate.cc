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
#include "dm_defs.h"
#include "dm_state.h"
#include "m_random.h"
#include "p_local.h"
#include "p_mobj.h"
#include "p_navigate.h"
#include "r_misc.h"

// DDF
#include "main.h"
#include "thing.h"


static int NAV_EvaluateBigItem(const mobj_t *mo)
{
	int score = 0;
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
				// ignore small amounts (e.g. potions)
				if (B->amount >= 100)
					score = std::max(score, 40);
				else if (B->amount >= 25)
					score = std::max(score, 10);
				break;

			case BENEFIT_Armour:
				// ignore green armor and tiny amounts.
				// also handle the items which give both health and armor
				if (B->sub.type >= ARMOUR_Blue && B->amount >= 50)
					score = std::max(score, 30);
				break;

			default:
				break;
		}
	}

	return score;
}


void NAV_AnalyseLevel()
{
	// TODO
}


void NAV_FreeLevel()
{
	// TODO
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
