//------------------------------------------------------------------------
//  COAL Play Simulation Interface
//------------------------------------------------------------------------
//
//  Copyright (c) 2006-2022  The EDGE Team.
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
//------------------------------------------------------------------------

#include "i_defs.h"

#include "coal.h"

#include "types.h"

#include "vm_coal.h"
#include "p_local.h"  
#include "p_mobj.h"
#include "r_state.h"

#include "dm_state.h"
#include "e_main.h"
#include "g_game.h"

#include "e_player.h"
#include "hu_font.h"
#include "hu_draw.h"
#include "r_modes.h"
#include "r_image.h"
#include "r_sky.h"

#include "f_interm.h" //Lobo: need this to get access to wi_stats
#include "rad_trig.h" //Lobo: need this to access RTS

#include <charconv>

#include "flat.h" // DDFFLAT - Dasho
#include "s_sound.h" // play_footstep() - Dasho

extern coal::vm_c *ui_vm;

extern void VM_SetFloat(coal::vm_c *vm, const char *mod_name, const char *var_name, double value);
extern void VM_CallFunction(coal::vm_c *vm, const char *name);

player_t * ui_player_who = NULL;

//------------------------------------------------------------------------
//  PLAYER MODULE
//------------------------------------------------------------------------


// player.num_players()
//
static void PL_num_players(coal::vm_c *vm, int argc)
{
	vm->ReturnFloat(numplayers);
}


// player.set_who(index)
//
static void PL_set_who(coal::vm_c *vm, int argc)
{
	int index = (int) *vm->AccessParam(0);

	if (index < 0 || index >= numplayers)
		I_Error("player.set_who: bad index value: %d (numplayers=%d)\n", index, numplayers);

	if (index == 0)
	{
		ui_player_who = players[consoleplayer];
		return;
	}

	int who = displayplayer;

	for (; index > 1; index--)
	{
		do
		{
			who = (who + 1) % MAXPLAYERS;
		}
		while (players[who] == NULL);
	}

	ui_player_who = players[who];
}


// player.is_bot()
//
static void PL_is_bot(coal::vm_c *vm, int argc)
{
	vm->ReturnFloat((ui_player_who->playerflags & PFL_Bot) ? 1 : 0);
}


// player.get_name()
//
static void PL_get_name(coal::vm_c *vm, int argc)
{
	vm->ReturnString(ui_player_who->playername);
}

// player.get_pos()
//
static void PL_get_pos(coal::vm_c *vm, int argc)
{
	double v[3];

	v[0] = ui_player_who->mo->x;
	v[1] = ui_player_who->mo->y;
	v[2] = ui_player_who->mo->z;

	vm->ReturnVector(v);
}

// player.get_angle()
//
static void PL_get_angle(coal::vm_c *vm, int argc)
{
	float value = ANG_2_FLOAT(ui_player_who->mo->angle);

	if (value > 360.0f) value -= 360.0f;
	if (value < 0)      value += 360.0f;

	vm->ReturnFloat(value);
}

// player.get_mlook()
//
static void PL_get_mlook(coal::vm_c *vm, int argc)
{
	float value = ANG_2_FLOAT(ui_player_who->mo->vertangle);

	if (value > 180.0f) value -= 360.0f;

	vm->ReturnFloat(value);
}


// player.health()
//
static void PL_health(coal::vm_c *vm, int argc)
{
	float h = ui_player_who->health * 100 / ui_player_who->mo->info->spawnhealth;

	vm->ReturnFloat(floor(h + 0.99));
}


// player.armor(type)
//
static void PL_armor(coal::vm_c *vm, int argc)
{
	int kind = (int) *vm->AccessParam(0);

	if (kind < 1 || kind > NUMARMOUR)
		I_Error("player.armor: bad armor index: %d\n", kind);
	
	kind--;

	vm->ReturnFloat(floor(ui_player_who->armours[kind] + 0.99));
}


// player.total_armor(type)
//
static void PL_total_armor(coal::vm_c *vm, int argc)
{
	vm->ReturnFloat(floor(ui_player_who->totalarmour + 0.99));
}


// player.frags()
//
static void PL_frags(coal::vm_c *vm, int argc)
{
	vm->ReturnFloat(ui_player_who->frags);
}


// player.under_water()
//
static void PL_under_water(coal::vm_c *vm, int argc)
{
	vm->ReturnFloat(ui_player_who->underwater ? 1 : 0);
}


// player.on_ground()
//
static void PL_on_ground(coal::vm_c *vm, int argc)
{
	vm->ReturnFloat((ui_player_who->mo->z <= ui_player_who->mo->floorz) ? 1 : 0);
}


// player.is_swimming()
//
static void PL_is_swimming(coal::vm_c *vm, int argc)
{
	vm->ReturnFloat(ui_player_who->swimming ? 1 : 0);
}


// player.is_jumping()
//
static void PL_is_jumping(coal::vm_c *vm, int argc)
{
	vm->ReturnFloat((ui_player_who->jumpwait > 0) ? 1 : 0);
}


// player.is_crouching()
//
static void PL_is_crouching(coal::vm_c *vm, int argc)
{
	vm->ReturnFloat((ui_player_who->mo->extendedflags & EF_CROUCHING) ? 1 : 0);
}


// player.is_attacking()
//
static void PL_is_attacking(coal::vm_c *vm, int argc)
{
	vm->ReturnFloat((ui_player_who->attackdown[0] ||
	                 ui_player_who->attackdown[1]) ? 1 : 0);
}


// player.is_rampaging()
//
static void PL_is_rampaging(coal::vm_c *vm, int argc)
{
	vm->ReturnFloat((ui_player_who->attackdown_count >= 70) ? 1 : 0);
}


// player.is_grinning()
//
static void PL_is_grinning(coal::vm_c *vm, int argc)
{
	vm->ReturnFloat((ui_player_who->grin_count > 0) ? 1 : 0);
}


// player.is_using()
//
static void PL_is_using(coal::vm_c *vm, int argc)
{
	vm->ReturnFloat(ui_player_who->usedown ? 1 : 0);
}


// player.is_action1()
//
static void PL_is_action1(coal::vm_c *vm, int argc)
{
	vm->ReturnFloat(ui_player_who->actiondown[0] ? 1 : 0);
}


// player.is_action2()
//
static void PL_is_action2(coal::vm_c *vm, int argc)
{
	vm->ReturnFloat(ui_player_who->actiondown[1] ? 1 : 0);
}


// player.move_speed()
//
static void PL_move_speed(coal::vm_c *vm, int argc)
{
	vm->ReturnFloat(ui_player_who->actual_speed);
}


// player.air_in_lungs()
//
static void PL_air_in_lungs(coal::vm_c *vm, int argc)
{
	if (ui_player_who->air_in_lungs <= 0)
	{
		vm->ReturnFloat(0);
		return;
	}

	float value = ui_player_who->air_in_lungs * 100.0f /
	              ui_player_who->mo->info->lung_capacity;

	value = CLAMP(0.0f, value, 100.0f);

	vm->ReturnFloat(value);
}


// player.has_key(key)
//
static void PL_has_key(coal::vm_c *vm, int argc)
{
	int key = (int) *vm->AccessParam(0);

	if (key < 1 || key > 16)
		I_Error("player.has_key: bad key number: %d\n", key);

	key--;

	int value = (ui_player_who->cards & (1 << key)) ? 1 : 0;

	vm->ReturnFloat(value);
}


// player.has_power(power)
//
static void PL_has_power(coal::vm_c *vm, int argc)
{
	int power = (int) *vm->AccessParam(0);

	if (power < 1 || power > NUMPOWERS)
		I_Error("player.has_power: bad powerup number: %d\n", power);

	power--;

	int value = (ui_player_who->powers[power] > 0) ? 1 : 0;

	// special check for GOD mode
	if (power == PW_Invulnerable && (ui_player_who->cheats & CF_GODMODE))
		value = 1;

	vm->ReturnFloat(value);
}


// player.power_left(power)
//
static void PL_power_left(coal::vm_c *vm, int argc)
{
	int power = (int) *vm->AccessParam(0);

	if (power < 1 || power > NUMPOWERS)
		I_Error("player.power_left: bad powerup number: %d\n", power);

	power--;

	float value = ui_player_who->powers[power];

	if (value > 0)
		value /= TICRATE;

	vm->ReturnFloat(value);
}


// player.has_weapon_slot(slot)
//
static void PL_has_weapon_slot(coal::vm_c *vm, int argc)
{
	int slot = (int) *vm->AccessParam(0);

	if (slot < 0 || slot > 9)
		I_Error("player.has_weapon_slot: bad slot number: %d\n", slot);

	int value = ui_player_who->avail_weapons[slot] ? 1 : 0;

	vm->ReturnFloat(value);
}


// player.cur_weapon_slot()
// 
static void PL_cur_weapon_slot(coal::vm_c *vm, int argc)
{
	int slot;

	if (ui_player_who->ready_wp < 0)
		slot = -1;
	else
		slot = ui_player_who->weapons[ui_player_who->ready_wp].info->bind_key;

	vm->ReturnFloat(slot);
}


// player.has_weapon(name)
//
static void PL_has_weapon(coal::vm_c *vm, int argc)
{
	const char * name = vm->AccessParamString(0);

	for (int j = 0; j < MAXWEAPONS; j++)
	{
		playerweapon_t *pw = &ui_player_who->weapons[j];

		if (pw->owned && ! (pw->flags & PLWEP_Removing) &&
			DDF_CompareName(name, pw->info->name.c_str()) == 0)
		{
			vm->ReturnFloat(1);
			return;
		}
	}

	vm->ReturnFloat(0);
}

// player.cur_weapon()
// 
static void PL_cur_weapon(coal::vm_c *vm, int argc)
{
	if (ui_player_who->pending_wp >= 0)
	{
		vm->ReturnString("change");
		return;
	}

	if (ui_player_who->ready_wp < 0)
	{
		vm->ReturnString("none");
		return;
	}

	weapondef_c *info = ui_player_who->weapons[ui_player_who->ready_wp].info;

	vm->ReturnString(info->name.c_str());
}


// player.ammo(type)
//
static void PL_ammo(coal::vm_c *vm, int argc)
{
	int ammo = (int) *vm->AccessParam(0);

	if (ammo < 1 || ammo > NUMAMMO)
		I_Error("player.ammo: bad ammo number: %d\n", ammo);

	ammo--;

	vm->ReturnFloat(ui_player_who->ammo[ammo].num);
}


// player.ammomax(type)
//
static void PL_ammomax(coal::vm_c *vm, int argc)
{
	int ammo = (int) *vm->AccessParam(0);

	if (ammo < 1 || ammo > NUMAMMO)
		I_Error("player.ammomax: bad ammo number: %d\n", ammo);

	ammo--;

	vm->ReturnFloat(ui_player_who->ammo[ammo].max);
}

// player.inventory(type)
//
static void PL_inventory(coal::vm_c *vm, int argc)
{
	int inv = (int) *vm->AccessParam(0);

	if (inv < 1 || inv > NUMINV)
		I_Error("player.inventory: bad inv number: %d\n", inv);

	inv--;

	vm->ReturnFloat(ui_player_who->inventory[inv].num);
}


// player.inventorymax(type)
//
static void PL_inventorymax(coal::vm_c *vm, int argc)
{
	int inv = (int) *vm->AccessParam(0);

	if (inv < 1 || inv > NUMINV)
		I_Error("player.inventorymax: bad inv number: %d\n", inv);

	inv--;

	vm->ReturnFloat(ui_player_who->inventory[inv].max);
}

// player.counter(type)
//
static void PL_counter(coal::vm_c *vm, int argc)
{
	int cntr = (int) *vm->AccessParam(0);

	if (cntr < 1 || cntr > NUMCOUNTER)
		I_Error("player.counter: bad counter number: %d\n", cntr);

	cntr--;

	vm->ReturnFloat(ui_player_who->counters[cntr].num);
}


// player.counter_max(type)
//
static void PL_counter_max(coal::vm_c *vm, int argc)
{
	int cntr = (int) *vm->AccessParam(0);

	if (cntr < 1 || cntr > NUMCOUNTER)
		I_Error("player.counter_max: bad counter number: %d\n", cntr);

	cntr--;

	vm->ReturnFloat(ui_player_who->counters[cntr].max);
}

// player.set_counter(type, value)
//
static void PL_set_counter(coal::vm_c *vm, int argc)
{
	if (argc != 2)
		I_Error("player.set_counter: wrong number of arguments given\n");

	int cntr = (int) *vm->AccessParam(0);
	int amt = (int) *vm->AccessParam(1);

	if (cntr < 1 || cntr > NUMCOUNTER)
		I_Error("player.set_counter: bad counter number: %d\n", cntr);

	cntr--;

	if (amt < 0)
		I_Error("player.set_counter: target amount cannot be negative!\n");

	if (amt > ui_player_who->counters[cntr].max)
		I_Error("player.set_counter: target amount %d exceeds limit for counter number %d\n", amt, cntr);

	ui_player_who->counters[cntr].num = amt;
}

// player.main_ammo(clip)
//
static void PL_main_ammo(coal::vm_c *vm, int argc)
{
	int value = 0;

	if (ui_player_who->ready_wp >= 0)
	{
		playerweapon_t *pw = &ui_player_who->weapons[ui_player_who->ready_wp];

		if (pw->info->ammo[0] != AM_NoAmmo)
		{
			if (pw->info->show_clip)
			{
				SYS_ASSERT(pw->info->ammopershot[0] > 0);

				value = pw->clip_size[0] / pw->info->ammopershot[0];
			}
			else
			{
				value = ui_player_who->ammo[pw->info->ammo[0]].num;

				if (pw->info->clip_size[0] > 0)
					value += pw->clip_size[0];
			}
		}
	}

	vm->ReturnFloat(value);
}


// player.ammo_type(ATK)
//
static void PL_ammo_type(coal::vm_c *vm, int argc)
{
	int ATK = (int) *vm->AccessParam(0);

	if (ATK < 1 || ATK > 2)
		I_Error("player.ammo_type: bad attack number: %d\n", ATK);

	ATK--;

	int value = 0;

	if (ui_player_who->ready_wp >= 0)
	{
		playerweapon_t *pw = &ui_player_who->weapons[ui_player_who->ready_wp];
		
		value = 1 + (int) pw->info->ammo[ATK];
	}

	vm->ReturnFloat(value);
}


// player.ammo_pershot(ATK)
//
static void PL_ammo_pershot(coal::vm_c *vm, int argc)
{
	int ATK = (int) *vm->AccessParam(0);

	if (ATK < 1 || ATK > 2)
		I_Error("player.ammo_pershot: bad attack number: %d\n", ATK);

	ATK--;

	int value = 0;

	if (ui_player_who->ready_wp >= 0)
	{
		playerweapon_t *pw = &ui_player_who->weapons[ui_player_who->ready_wp];
		
		value = pw->info->ammopershot[ATK];
	}

	vm->ReturnFloat(value);
}


// player.clip_ammo(ATK)
//
static void PL_clip_ammo(coal::vm_c *vm, int argc)
{
	int ATK = (int) *vm->AccessParam(0);

	if (ATK < 1 || ATK > 2)
		I_Error("player.clip_ammo: bad attack number: %d\n", ATK);

	ATK--;

	int value = 0;

	if (ui_player_who->ready_wp >= 0)
	{
		playerweapon_t *pw = &ui_player_who->weapons[ui_player_who->ready_wp];

		value = pw->clip_size[ATK];
	}

	vm->ReturnFloat(value);
}


// player.clip_size(ATK)
//
static void PL_clip_size(coal::vm_c *vm, int argc)
{
	int ATK = (int) *vm->AccessParam(0);

	if (ATK < 1 || ATK > 2)
		I_Error("player.clip_size: bad attack number: %d\n", ATK);

	ATK--;

	int value = 0;

	if (ui_player_who->ready_wp >= 0)
	{
		playerweapon_t *pw = &ui_player_who->weapons[ui_player_who->ready_wp];

		value = pw->info->clip_size[ATK];
	}

	vm->ReturnFloat(value);
}


// player.clip_is_shared()
//
static void PL_clip_is_shared(coal::vm_c *vm, int argc)
{
	int value = 0;

	if (ui_player_who->ready_wp >= 0)
	{
		playerweapon_t *pw = &ui_player_who->weapons[ui_player_who->ready_wp];

		if (pw->info->shared_clip)
			value = 1;
	}

	vm->ReturnFloat(value);
}


// player.hurt_by()
//
static void PL_hurt_by(coal::vm_c *vm, int argc)
{
	if (ui_player_who->damagecount <= 0)
	{
		vm->ReturnString("");
		return;
	}

	// getting hurt because of your own damn stupidity
	if (ui_player_who->attacker == ui_player_who->mo)
		vm->ReturnString("self");
	else if (ui_player_who->attacker && (ui_player_who->attacker->side & ui_player_who->mo->side))
		vm->ReturnString("friend");
	else if (ui_player_who->attacker)
		vm->ReturnString("enemy");
	else
		vm->ReturnString("other");
}


// player.hurt_mon()
//
static void PL_hurt_mon(coal::vm_c *vm, int argc)
{
	if (ui_player_who->damagecount > 0 &&
		ui_player_who->attacker &&
		ui_player_who->attacker != ui_player_who->mo)
	{
		vm->ReturnString(ui_player_who->attacker->info->name.c_str());
		return;
	}

	vm->ReturnString("");
}


// player.hurt_pain()
//
static void PL_hurt_pain(coal::vm_c *vm, int argc)
{
	vm->ReturnFloat(ui_player_who->damage_pain);
}


// player.hurt_dir()
//
static void PL_hurt_dir(coal::vm_c *vm, int argc)
{
	int dir = 0;

	if (ui_player_who->attacker && ui_player_who->attacker != ui_player_who->mo)
	{
		mobj_t *badguy = ui_player_who->attacker;
		mobj_t *pmo    = ui_player_who->mo;

		angle_t diff = R_PointToAngle(pmo->x, pmo->y, badguy->x, badguy->y) - pmo->angle;

		if (diff >= ANG45 && diff <= ANG135)
		{
			dir = -1;
		}
		else if (diff >= ANG225 && diff <= ANG315)
		{
			dir = +1;
		}
	}

	vm->ReturnFloat(dir);
}


// player.hurt_angle()
//
static void PL_hurt_angle(coal::vm_c *vm, int argc)
{
	float value = 0;

	if (ui_player_who->attacker && ui_player_who->attacker != ui_player_who->mo)
	{
		mobj_t *badguy = ui_player_who->attacker;
		mobj_t *pmo    = ui_player_who->mo;

		angle_t real_a = R_PointToAngle(pmo->x, pmo->y, badguy->x, badguy->y);

		value = ANG_2_FLOAT(real_a);

		if (value > 360.0f)
			value -= 360.0f;

		if (value < 0)
			value += 360.0f;
	}

	vm->ReturnFloat(value);
}


// player.kills()
// Lobo: November 2021
static void PL_kills(coal::vm_c *vm, int argc)
{
	vm->ReturnFloat(ui_player_who->killcount);
}

// player.secrets()
// Lobo: November 2021
static void PL_secrets(coal::vm_c *vm, int argc)
{
	vm->ReturnFloat(ui_player_who->secretcount);
}

// player.items()
// Lobo: November 2021
static void PL_items(coal::vm_c *vm, int argc)
{
	vm->ReturnFloat(ui_player_who->itemcount);
}


// player.map_enemies()
// Lobo: November 2021
static void PL_map_enemies(coal::vm_c *vm, int argc)
{
	vm->ReturnFloat(wi_stats.kills);
}

// player.map_secrets()
// Lobo: November 2021
static void PL_map_secrets(coal::vm_c *vm, int argc)
{
	vm->ReturnFloat(wi_stats.secret);
}

// player.map_items()
// Lobo: November 2021
static void PL_map_items(coal::vm_c *vm, int argc)
{
	vm->ReturnFloat(wi_stats.items);
}


// player.floor_flat()
// Lobo: November 2021
static void PL_floor_flat(coal::vm_c *vm, int argc)
{
	// If no 3D floors, just return the flat
	if (ui_player_who->mo->subsector->sector->exfloor_used == 0)
	{
		vm->ReturnString(ui_player_who->mo->subsector->sector->floor.image->name);
	}
	else
	{
		// Start from the lowest exfloor and check if the player is standing on it, then return the control sector's flat
		float player_floor_height = ui_player_who->mo->floorz;
		extrafloor_t *floor_checker = ui_player_who->mo->subsector->sector->bottom_ef;
		for (extrafloor_t *ef = floor_checker; ef; ef=ef->higher)
		{
			if (player_floor_height + 1 > ef->top_h)
			{
				vm->ReturnString(ef->top->image->name);
				return;
			}
		}
		// Fallback if nothing else satisfies these conditions
		vm->ReturnString(ui_player_who->mo->subsector->sector->floor.image->name);
	}
}

// player.sector_tag()
// Lobo: November 2021
static void PL_sector_tag(coal::vm_c *vm, int argc)
{
	vm->ReturnFloat(ui_player_who->mo->subsector->sector->tag);
}

// player.play_footstep(flat name)
// Dasho: January 2022
// Now uses the new DDFFLAT construct
static void PL_play_footstep(coal::vm_c *vm, int argc)
{
	const char *flat = vm->AccessParamString(0);
	if (!flat)
		I_Error("player.play_footstep: No flat name given!\n");
	
	flatdef_c *current_flatdef = flatdefs.Find(flat);

	if (!current_flatdef)
		return;
	
	if (!current_flatdef->footstep)
		return;
	else
	{
		// Probably need to add check to see if the sfx is valid - Dasho
		S_StartFX(current_flatdef->footstep);
	}
}

// player.use_inventory(type)
//
static void PL_use_inventory(coal::vm_c *vm, int argc)
{
	double *num = vm->AccessParam(0);
	std::string script_name = "INVENTORY";
	int inv;
	if (!num)
		I_Error("player.use_inventory: can't parse inventory number!\n");
	else
		inv = (int)*num;

	if (inv < 1 || inv > NUMINV)
		I_Error("player.use_inventory: bad inventory number: %d\n", inv);

	if (inv < 10)
		script_name.append("0").append(std::to_string(inv));
	else
		script_name.append(std::to_string(inv));

	inv--;

	if (ui_player_who->inventory[inv].num > 0)
	{
		ui_player_who->inventory[inv].num -= 1;
		RAD_EnableByTag(NULL, script_name.c_str(), false);
	}
	
	vm->ReturnFloat(ui_player_who->inventory[inv].num);
}

// player.rts_enable_tagged(tag)
//
static void PL_rts_enable_tagged(coal::vm_c *vm, int argc)
{
	std::string name = vm->AccessParamString(0);

	if (!name.empty())
		RAD_EnableByTag(NULL, name.c_str(), false);
}


// AuxStringReplaceAll("Our_String", std::string("_"), std::string(" "));
//
std::string AuxStringReplaceAll(std::string str, const std::string& from, const std::string& to) 
{
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) 
	{
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
    return str;
}



// GetMobjBenefits(mobj);
//
std::string GetMobjBenefits(mobj_t *obj, bool KillBenefits=false) 
{
	std::string temp_string;
	temp_string.clear();
	benefit_t *list;
	int temp_num=0;

	if(KillBenefits)
		list = obj->info->kill_benefits;
	else
		list = obj->info->pickup_benefits;

    for (; list != NULL; list=list->next)
	{
		switch (list->type)
		{
			case BENEFIT_Weapon:  
				//If it's a weapon all bets are off: we'll want to parse
				//it differently, not here.
				temp_string = "WEAPON=1";
				break;

			case BENEFIT_Ammo:
				temp_string += "AMMO" + std::to_string((int)list->sub.type + 1);
				temp_string += "=" + std::to_string((int)list->amount);
				break;

			case BENEFIT_Health: //only benefit without a sub.type so just give it 01
				temp_string += "HEALTH01=" + std::to_string((int)list->amount);
				break;

			case BENEFIT_Armour:
				temp_string += "ARMOUR" + std::to_string((int)list->sub.type + 1);
				temp_string += "=" + std::to_string((int)list->amount);
				break;
			
			case BENEFIT_Inventory:
				temp_string += "INVENTORY";
				if((list->sub.type + 1) < 10)
					temp_string += "0";
				temp_string += std::to_string((int)list->sub.type + 1);
				temp_string += "=" + std::to_string((int)list->amount);
				break;
			
			case BENEFIT_Counter:
				temp_string += "COUNTER";
				if((list->sub.type + 1) < 10)
					temp_string += "0";
				temp_string += std::to_string((int)list->sub.type + 1);
				temp_string += "=" + std::to_string((int)list->amount);
				break;

			case BENEFIT_Key:
				temp_string += "KEY";
				temp_num = log2((int)list->sub.type);
				temp_num ++;
				temp_string += std::to_string(temp_num);
				break;

			case BENEFIT_Powerup:
				temp_string += "POWERUP" + std::to_string((int)list->sub.type + 1);
				break;
			
			default: break;
		}
	}
	return temp_string;
}

// GetQueryInfoFromMobj(mobj, whatinfo)
//
std::string GetQueryInfoFromMobj(mobj_t *obj, int whatinfo)
{
	int temp_num = 0;
	std::string temp_string;
	temp_string.clear();

	switch (whatinfo)
	{
		case 1:  //name
			if (obj)
			{
				//try CAST_TITLE first
				temp_string = language[obj->info->cast_title];

				if (temp_string.empty()) //fallback to DDFTHING entry name
				{
					temp_string = obj->info->name;
					temp_string = AuxStringReplaceAll(temp_string, std::string("_"), std::string(" "));
				}
			}
			break;

		case 2: //current health
			if (obj)
			{
				temp_num = obj->health;
				temp_string = std::to_string(temp_num);
			}
			break;

		case 3: //spawn health
			if (obj)
			{
				temp_num = obj->info->spawnhealth;
				temp_string = std::to_string(temp_num);
			}
			break;

		case 4:  //pickup_benefits
			if (obj)
			{
				temp_string = GetMobjBenefits(obj, false);
			}
			break;

		case 5: //kill_benefits
			if (obj)
			{
				temp_string = GetMobjBenefits(obj,true);
			}
			break;

	}

	if (temp_string.empty())
		return("");

	return(temp_string.c_str());
	
}

// GetQueryInfoFromWeapon(mobj, whatinfo, [secattackinfo])
//
std::string GetQueryInfoFromWeapon(mobj_t *obj, int whatinfo, bool secattackinfo = false)
{
	int temp_num = 0;
	std::string temp_string;
	temp_string.clear();

	if (!obj->info->pickup_benefits)
		return "";
	if (!obj->info->pickup_benefits->sub.weap)
		return "";	

	weapondef_c *objWep = obj->info->pickup_benefits->sub.weap;
	if (!objWep)
		return "";

	int attacknum = 0; //default to primary attack
	if(secattackinfo)
		attacknum = 1;

	atkdef_c *objAtck = objWep->attack[attacknum];	
	if(!objAtck && whatinfo > 2)
		return ""; //no attack to get info about (only should happen with secondary attacks)

	const damage_c *damtype;

	switch (whatinfo)
	{
		case 1:  //name
			temp_string = objWep->name;
			temp_string = AuxStringReplaceAll(temp_string, std::string("_"), std::string(" "));
			break;
		
		case 2:  //ZOOM_FACTOR
			temp_num = objWep->zoom_factor;
			temp_string = std::to_string(temp_num);
			break;

		case 3: //AMMOTYPE
			temp_num = (objWep->ammo[attacknum]) + 1;
			temp_string = std::to_string(temp_num);
			break;

		case 4: //AMMOPERSHOT
			temp_num = objWep->ammopershot[attacknum];
			temp_string = std::to_string(temp_num);
			break;

		case 5:  //CLIPSIZE
			temp_num = objWep->clip_size[attacknum];
			temp_string = std::to_string(temp_num);
			break;

		case 6: //DAMAGE Nominal
			damtype = &objAtck->damage;
			temp_num = damtype->nominal;
			temp_string = std::to_string(temp_num);
			break;
		
		case 7: //DAMAGE Max
			damtype = &objAtck->damage;
			temp_num = damtype->linear_max;
			temp_string = std::to_string(temp_num);
			break;

		case 8: //Range
			temp_num = objAtck->range;
			temp_string = std::to_string(temp_num);
			break;

		case 9:  //AUTOMATIC
			if (objWep->autofire[attacknum])
				temp_string = "1";
			else
				temp_string = "0";
			break;

	}

	if (temp_string.empty())
		return("");

	return(temp_string.c_str());
	
}

// player.query_object(whatinfo)
//
static void PL_query_object(coal::vm_c *vm, int argc)
{
	double *num = vm->AccessParam(0);
	int whatinfo = 1;

	if (!num)
		I_Error("player.query_object: can't parse WhatInfo!\n");
	else
		whatinfo = (int)*num;

	if (whatinfo < 1 || whatinfo > 5)
		I_Error("player.query_object: bad whatInfo number: %d\n", whatinfo);

	mobj_t *obj = DoMapTargetAutoAim(ui_player_who->mo, ui_player_who->mo->angle, 512, true, true);
	if (!obj)
	{
		vm->ReturnString("");
		return;
	}

	std::string temp_string;
	temp_string.clear();

	temp_string = GetQueryInfoFromMobj(obj,whatinfo);

	if (temp_string.empty())
		vm->ReturnString("");
	else
		vm->ReturnString(temp_string.c_str());
	
}

// mapobject.query_tagged(thing tag, whatinfo)
//
static void MO_query_tagged(coal::vm_c *vm, int argc)
{
	
	if (argc != 2)
		I_Error("mapobject.query_tagged: wrong number of arguments given\n");

	double *argTag = vm->AccessParam(0);
	double *argInfo = vm->AccessParam(1);
	int whattag = 1;
	int whatinfo = 1;

	whattag = (int)*argTag;
	whatinfo = (int)*argInfo;

	mobj_t *mo;

	int index = 0;
	std::string temp_value;
	temp_value.clear();

	for (mo=mobjlisthead; mo; mo=mo->next, index++)
	{
		if (mo->tag == whattag)
		{
			temp_value = GetQueryInfoFromMobj(mo,whatinfo);
			break;
		}	
	}

	if (temp_value.empty())
		vm->ReturnString("");
	else
		vm->ReturnString(temp_value.c_str());
	
}

// mapobject.count(thing type/id)
//
static void MO_count(coal::vm_c *vm, int argc)
{
	double *num = vm->AccessParam(0);
	int thingid = 0;

	if (!num)
		I_Error("mapobjects.count: can't parse thing id/type!\n");
	else
		thingid = (int)*num;


	mobj_t *mo;

	int index = 0;
	double thingcount = 0;

	for (mo=mobjlisthead; mo; mo=mo->next, index++)
	{
		if (mo->info->number == thingid && mo->health > 0)
			thingcount ++;
	}
	
	vm->ReturnFloat(thingcount);
}

// player.query_weapon(whatinfo,[SecAttack])
//
static void PL_query_weapon(coal::vm_c *vm, int argc)
{
	double *num = vm->AccessParam(0);
	double *secattack = vm->AccessParam(1);

	int whatinfo = 1;
	int secattackinfo = 0;

	if (!num)
		I_Error("player.query_weapon: can't parse WhatInfo!\n");
	else
		whatinfo = (int)*num;

	if (secattack)
		secattackinfo = (int)*secattack;

	if (whatinfo < 1 || whatinfo > 9)
		I_Error("player.query_weapon: bad whatInfo number: %d\n", whatinfo);

	if (secattackinfo < 0 || secattackinfo > 1)
		I_Error("player.query_weapon: bad secAttackInfo number: %d\n", whatinfo);

	mobj_t *obj = DoMapTargetAutoAim(ui_player_who->mo, ui_player_who->mo->angle, 512, true, true);
	if (!obj)
	{
		vm->ReturnString("");
		return;
	}

	std::string temp_string;
	temp_string.clear();

	if (secattackinfo == 1)
		temp_string = GetQueryInfoFromWeapon(obj,whatinfo,true);
	else
		temp_string = GetQueryInfoFromWeapon(obj,whatinfo);

	if (temp_string.empty())
		vm->ReturnString("");
	else
		vm->ReturnString(temp_string.c_str());
	
}

//------------------------------------------------------------------------


void VM_RegisterPlaysim()
{
	ui_vm->AddNativeFunction("player.num_players", PL_num_players);
	ui_vm->AddNativeFunction("player.set_who",     PL_set_who);
    ui_vm->AddNativeFunction("player.is_bot",      PL_is_bot);
    ui_vm->AddNativeFunction("player.get_name",    PL_get_name);
    ui_vm->AddNativeFunction("player.get_pos",     PL_get_pos);
    ui_vm->AddNativeFunction("player.get_angle",   PL_get_angle);
    ui_vm->AddNativeFunction("player.get_mlook",   PL_get_mlook);

    ui_vm->AddNativeFunction("player.health",      PL_health);
    ui_vm->AddNativeFunction("player.armor",       PL_armor);
    ui_vm->AddNativeFunction("player.total_armor", PL_total_armor);
    ui_vm->AddNativeFunction("player.ammo",        PL_ammo);
    ui_vm->AddNativeFunction("player.ammomax",     PL_ammomax);
    ui_vm->AddNativeFunction("player.frags",       PL_frags);

    ui_vm->AddNativeFunction("player.is_swimming",     PL_is_swimming);
    ui_vm->AddNativeFunction("player.is_jumping",      PL_is_jumping);
    ui_vm->AddNativeFunction("player.is_crouching",    PL_is_crouching);
    ui_vm->AddNativeFunction("player.is_using",        PL_is_using);
    ui_vm->AddNativeFunction("player.is_action1",      PL_is_action1);
    ui_vm->AddNativeFunction("player.is_action2",      PL_is_action2);
    ui_vm->AddNativeFunction("player.is_attacking",    PL_is_attacking);
    ui_vm->AddNativeFunction("player.is_rampaging",    PL_is_rampaging);
    ui_vm->AddNativeFunction("player.is_grinning",     PL_is_grinning);

    ui_vm->AddNativeFunction("player.under_water",     PL_under_water);
    ui_vm->AddNativeFunction("player.on_ground",       PL_on_ground);
    ui_vm->AddNativeFunction("player.move_speed",      PL_move_speed);
    ui_vm->AddNativeFunction("player.air_in_lungs",    PL_air_in_lungs);

    ui_vm->AddNativeFunction("player.has_key",         PL_has_key);
    ui_vm->AddNativeFunction("player.has_power",       PL_has_power);
    ui_vm->AddNativeFunction("player.power_left",      PL_power_left);
    ui_vm->AddNativeFunction("player.has_weapon",      PL_has_weapon);
    ui_vm->AddNativeFunction("player.has_weapon_slot", PL_has_weapon_slot);
    ui_vm->AddNativeFunction("player.cur_weapon",      PL_cur_weapon);
    ui_vm->AddNativeFunction("player.cur_weapon_slot", PL_cur_weapon_slot);

    ui_vm->AddNativeFunction("player.main_ammo",       PL_main_ammo);
    ui_vm->AddNativeFunction("player.ammo_type",       PL_ammo_type);
    ui_vm->AddNativeFunction("player.ammo_pershot",    PL_ammo_pershot);
    ui_vm->AddNativeFunction("player.clip_ammo",       PL_clip_ammo);
    ui_vm->AddNativeFunction("player.clip_size",       PL_clip_size);
    ui_vm->AddNativeFunction("player.clip_is_shared",  PL_clip_is_shared);

    ui_vm->AddNativeFunction("player.hurt_by",         PL_hurt_by);
    ui_vm->AddNativeFunction("player.hurt_mon",        PL_hurt_mon);
    ui_vm->AddNativeFunction("player.hurt_pain",       PL_hurt_pain);
    ui_vm->AddNativeFunction("player.hurt_dir",        PL_hurt_dir);
    ui_vm->AddNativeFunction("player.hurt_angle",      PL_hurt_angle);

	// Lobo: November 2021
	ui_vm->AddNativeFunction("player.kills",      PL_kills);
	ui_vm->AddNativeFunction("player.secrets",      PL_secrets);
	ui_vm->AddNativeFunction("player.items",      PL_items);
	ui_vm->AddNativeFunction("player.map_enemies",      PL_map_enemies);
	ui_vm->AddNativeFunction("player.map_secrets",      PL_map_secrets);
	ui_vm->AddNativeFunction("player.map_items",      PL_map_items);
	ui_vm->AddNativeFunction("player.floor_flat",      PL_floor_flat);
	ui_vm->AddNativeFunction("player.sector_tag",      PL_sector_tag);

	// Dasho: December 2021
	ui_vm->AddNativeFunction("player.play_footstep", PL_play_footstep);

	ui_vm->AddNativeFunction("player.use_inventory",        PL_use_inventory);
    ui_vm->AddNativeFunction("player.inventory",        PL_inventory);
    ui_vm->AddNativeFunction("player.inventorymax",     PL_inventorymax);

	ui_vm->AddNativeFunction("player.rts_enable_tagged",        PL_rts_enable_tagged);

    ui_vm->AddNativeFunction("player.counter",        PL_counter);
    ui_vm->AddNativeFunction("player.counter_max",     PL_counter_max);
	ui_vm->AddNativeFunction("player.set_counter",     PL_set_counter);

	//Lobo: October 2022
	ui_vm->AddNativeFunction("player.query_object",     PL_query_object);
	ui_vm->AddNativeFunction("player.query_weapon",     PL_query_weapon);

	ui_vm->AddNativeFunction("mapobject.query_tagged",     MO_query_tagged);
    ui_vm->AddNativeFunction("mapobject.count",     MO_count);
	
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
