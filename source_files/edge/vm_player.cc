//------------------------------------------------------------------------
//  COAL Play Simulation Interface
//------------------------------------------------------------------------
//
//  Copyright (c) 2006-2009  The EDGE Team.
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


extern coal::vm_c *ui_vm;

extern void VM_SetFloat(coal::vm_c *vm, const char *name, double value);
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
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
