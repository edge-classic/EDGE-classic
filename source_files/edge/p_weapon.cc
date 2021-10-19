//----------------------------------------------------------------------------
//  EDGE Weapon (player sprites) Action Code
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
// -KM- 1998/11/25 Added/Changed stuff for weapons.ddf
//

#include "i_defs.h"
#include "p_weapon.h"

#include "e_event.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "m_misc.h"
#include "m_random.h"
#include "p_action.h"
#include "p_local.h"
#include "rad_trig.h"
#include "s_sound.h"
#include "w_sprite.h"
#include "w_wad.h"

static sound_category_e WeapSfxCat(player_t *p)
{
	if (p == players[consoleplayer])
		return SNCAT_Weapon;
        
	return SNCAT_Opponent;
}


static void P_SetPsprite(player_t * p, int position, int stnum, weapondef_c *info = NULL)
{
	pspdef_t *psp = &p->psprites[position];

	if (stnum == S_NULL)
	{
		// object removed itself
		psp->state = psp->next_state = NULL;
		return;
	}

	// state is old? -- Mundo hack for DDF inheritance
	if (info && stnum < info->state_grp.back().first)
	{
		state_t *st = &states[stnum];

		if (st->label)
		{
			statenum_t new_state = DDF_StateFindLabel(info->state_grp, st->label, true /* quiet */);
			if (new_state != S_NULL)
				stnum = new_state;
		}
	}

	state_t *st = &states[stnum];

	// model interpolation stuff
	if (psp->state &&
		(st->flags & SFF_Model) && (psp->state->flags & SFF_Model) &&
		(st->sprite == psp->state->sprite) && st->tics > 1)
	{
		p->weapon_last_frame = psp->state->frame;
	}
	else
		p->weapon_last_frame = -1;

	psp->state = st;
	psp->tics  = st->tics;
	psp->next_state = (st->nextstate == S_NULL) ? NULL : 
		(states + st->nextstate);

	// call action routine

	p->action_psp = position;

	if (st->action)
		(* st->action)(p->mo);
}


//
// P_SetPspriteDeferred
//
// -AJA- 2004/11/05: This is preferred method, doesn't run any actions,
//       which (ideally) should only happen during P_MovePsprites().
//
void P_SetPspriteDeferred(player_t * p, int position, int stnum)
{
	pspdef_t *psp = &p->psprites[position];

	if (stnum == S_NULL || psp->state == NULL)
	{
		P_SetPsprite(p, position, stnum);
		return;
	}

	psp->tics = 0;
	psp->next_state = (states + stnum);
}


//
// P_CheckWeaponSprite
//
// returns true if the sprite(s) for the weapon exist.  Prevents being
// able to e.g. select the super shotgun when playing with a DOOM 1
// IWAD (and cheating).
//
// -KM- 1998/12/16 Added check to make sure sprites exist.
// -AJA- 2000: Made into a separate routine.
//
bool P_CheckWeaponSprite(weapondef_c *info)
{
	if (info->up_state == S_NULL)
		return false;

	return W_CheckSpritesExist(info->state_grp);
}

static bool ButtonDown(player_t *p, int ATK)
{
	if (ATK == 0)
		return (p->cmd.buttons & BT_ATTACK);
	else
		return (p->cmd.extbuttons & EBT_SECONDATK);
}

static bool WeaponCanFire(player_t *p, int idx, int ATK)
{
	weapondef_c *info = p->weapons[idx].info;

	if (info->shared_clip)
		ATK = 0;

	// the order here is important, to allow NoAmmo+Clip weapons.
	if (info->clip_size[ATK] > 0)
		return (info->ammopershot[ATK] <= p->weapons[idx].clip_size[ATK]);

	if (info->ammo[ATK] == AM_NoAmmo)
		return true;

	return (info->ammopershot[ATK] <= p->ammo[info->ammo[ATK]].num);
}

static bool WeaponCanReload(player_t *p, int idx, int ATK, bool allow_top_up)
{
	weapondef_c *info = p->weapons[idx].info;

	bool can_fire = WeaponCanFire(p, idx, ATK);

	if (info->shared_clip)
		ATK = 0;

	if (! (info->specials[ATK] & WPSP_Partial))
	{
		allow_top_up = false;
	}

	// for non-clip weapon, can reload whenever enough ammo is avail.
	if (info->clip_size[ATK] == 0)
		return can_fire;

	// clip check (cannot reload if clip is full)
	if (p->weapons[idx].clip_size[ATK] == info->clip_size[ATK])
		return false;

	// for clip weapons, cannot reload until clip is empty.
	if (can_fire && !allow_top_up)
		return false;

	// for NoAmmo+Clip weapons, can always refill it
	if (info->ammo[ATK] == AM_NoAmmo)
		return true;

	// ammo check...
	int total = p->ammo[info->ammo[ATK]].num;

	if (info->specials[ATK] & WPSP_Partial)
	{
		return (info->ammopershot[ATK] <= total);
	}

	return (info->clip_size[ATK] - p->weapons[idx].clip_size[ATK] <= total);
}


#if 0  // OLD FUNCTION
static bool WeaponCanPartialReload(player_t *p, int idx, int ATK)
{
	weapondef_c *info = p->weapons[idx].info;

	// for non-clip weapons, same as WeaponCanReload()
	if (info->clip_size[ATK] == 0)
		return WeaponCanFire(p, idx, ATK);

	// ammo check
	return (info->ammo[ATK] == AM_NoAmmo) ||
		   (info->clip_size[ATK] - p->weapons[idx].clip_size[ATK] <=
			p->ammo[info->ammo[ATK]].num);
}
#endif


static bool WeaponCouldAutoFire(player_t *p, int idx, int ATK)
{
	// Returns true when weapon will either fire or reload
	// (assuming the button is held down).

	weapondef_c *info = p->weapons[idx].info;

	if (! info->attack_state[ATK])
		return false;

	if (info->shared_clip)
		ATK = 0;

	if (info->ammo[ATK] == AM_NoAmmo)
		return true;

	int total = p->ammo[info->ammo[ATK]].num;

	if (info->clip_size[ATK] == 0)
		return (info->ammopershot[ATK] <= total);

	// for clip weapons, either need a non-empty clip or enough
	// ammo to fill the clip (which is able to be filled without the
	// manual reload key).
	if (info->ammopershot[ATK] <= p->weapons[idx].clip_size[ATK] ||
		(info->clip_size[ATK] <= total &&
		 (info->specials[ATK] & (WPSP_Trigger | WPSP_Fresh))))
	{
		return true;
	}

	return false;
}


static void GotoDownState(player_t *p)
{
	weapondef_c *info = p->weapons[p->ready_wp].info;

	int newstate = info->down_state;

	P_SetPspriteDeferred(p, ps_weapon, newstate);
	P_SetPsprite(p, ps_crosshair, S_NULL);
}

static void GotoReadyState(player_t *p)
{
	weapondef_c *info = p->weapons[p->ready_wp].info;

	int newstate = info->ready_state;

	P_SetPspriteDeferred(p, ps_weapon, newstate);
	P_SetPspriteDeferred(p, ps_crosshair, info->crosshair);
}

static void GotoEmptyState(player_t *p)
{
	weapondef_c *info = p->weapons[p->ready_wp].info;
	
	int newstate = info->empty_state;

	P_SetPspriteDeferred(p, ps_weapon, newstate);
	P_SetPsprite(p, ps_crosshair, S_NULL);
}

static void GotoAttackState(player_t * p, int ATK, bool can_warmup)
{
	weapondef_c *info = p->weapons[p->ready_wp].info;

	int newstate = info->attack_state[ATK];

	if (p->remember_atk[ATK] >= 0)
	{
		newstate = p->remember_atk[ATK];
		p->remember_atk[ATK] = -1;
	}
	else if (can_warmup && info->warmup_state[ATK])
	{
		newstate = info->warmup_state[ATK];
	}

	if (newstate)
	{
		P_SetPspriteDeferred(p, ps_weapon, newstate);
		p->idlewait = 0;
	}
}

static void ReloadWeapon(player_t *p, int idx, int ATK)
{
	weapondef_c *info = p->weapons[idx].info;

	if (info->clip_size[ATK] == 0)
		return;

	// for NoAmmo+Clip weapons, can always refill it
	if (info->ammo[ATK] == AM_NoAmmo)
	{
		p->weapons[idx].clip_size[ATK] = info->clip_size[ATK];
		return;
	}

	int qty = info->clip_size[ATK] - p->weapons[idx].clip_size[ATK];

	if (qty > p->ammo[info->ammo[ATK]].num)
		qty = p->ammo[info->ammo[ATK]].num;

	SYS_ASSERT(qty > 0);

	p->weapons[idx].reload_count[ATK] = qty;
	p->weapons[idx].clip_size[ATK] += qty;
	p->ammo[info->ammo[ATK]].num   -= qty;
}

static void GotoReloadState(player_t *p, int ATK)
{
	weapondef_c *info = p->weapons[p->ready_wp].info;

	if (info->shared_clip)
		ATK = 0;

	ReloadWeapon(p, p->ready_wp, ATK);

	// second attack will fall-back to using normal reload states.
	if (ATK == 1 && ! info->reload_state[ATK])
		ATK = 0;

	if (info->reload_state[ATK])
	{
		P_SetPspriteDeferred(p, ps_weapon, info->reload_state[ATK]);
		p->idlewait = 0;
	}

	// if player has reload states, use 'em baby
	if (p->mo->info->reload_state)
		P_SetMobjStateDeferred(p->mo, p->mo->info->reload_state, 0);
}

//
// SwitchAway
//
// Not enough ammo to shoot, selects the next weapon to use.
// In some cases we prefer to reload the weapon (if we can).
// The NO_SWITCH special prevents the switch, enter empty or ready
// states instead.
//
static void SwitchAway(player_t * p, int ATK, int reload)
{
	weapondef_c *info = p->weapons[p->ready_wp].info;

	if (reload && WeaponCanReload(p, p->ready_wp, ATK, false))
		GotoReloadState(p, ATK);
	else if (info->specials[ATK] & WPSP_SwitchAway)
		P_SelectNewWeapon(p, -100, AM_DontCare);
	else if (info->empty_state && ! WeaponCouldAutoFire(p, p->ready_wp, 0))
		GotoEmptyState(p);
	else 
		GotoReadyState(p);
}

//
// P_BringUpWeapon
//
// Starts bringing the pending weapon up
// from the bottom of the screen.
//
static void P_BringUpWeapon(player_t * p)
{
	weapon_selection_e sel = p->pending_wp;

	SYS_ASSERT(sel != WPSEL_NoChange);

	p->ready_wp = sel;

	p->pending_wp = WPSEL_NoChange;
	p->psprites[ps_weapon].sy = WEAPONBOTTOM - WEAPONTOP;

	p->remember_atk[0] = -1;
	p->remember_atk[1] = -1;
	p->idlewait = 0;
	p->weapon_last_frame = -1;

	if (sel == WPSEL_None)
	{
		p->attackdown[0] = false;
		p->attackdown[1] = false;

		P_SetPsprite(p, ps_weapon, S_NULL);
		P_SetPsprite(p, ps_flash, S_NULL);
		P_SetPsprite(p, ps_crosshair, S_NULL);

		p->zoom_fov = 0;
		return;
	}

	weapondef_c *info = p->weapons[sel].info;

	// update current key choice
	if (info->bind_key >= 0)
		p->key_choices[info->bind_key] = sel;

	if (info->specials[0] & WPSP_Animated)
		p->psprites[ps_weapon].sy = 0;

	if (p->zoom_fov > 0)
	{
		if (info->zoom_fov > 0)
			p->zoom_fov = info->zoom_fov;
		else if (level_flags.limit_zoom)
			p->zoom_fov = 0;
		else
			p->zoom_fov = r_zoomfov.d;
	}

	if (info->start)
		S_StartFX(info->start, WeapSfxCat(p), p->mo);

	P_SetPspriteDeferred(p, ps_weapon, info->up_state);
	P_SetPsprite(p, ps_flash,  S_NULL);
	P_SetPsprite(p, ps_crosshair, S_NULL);

	p->refire = info->refire_inacc ? 0 : 1;
}


void P_DesireWeaponChange(player_t * p, int key)
{
	// optimisation: don't keep calculating this over and over
	// while the user holds down the same number key.
	if (p->pending_wp >= 0)
	{
		weapondef_c *info = p->weapons[p->pending_wp].info;

		SYS_ASSERT(info);

		if (info->bind_key == key)
			return;
	}

#if 0  // OLD CODE
	int i, j;
	weaponkey_c *wk = &weaponkey[key];

	for (i=j=player->key_choices[key]; i < (j + wk->numchoices); i++)
	{
		weapondef_c *choice = wk->choices[i % wk->numchoices];

		if (! P_PlayerSwitchWeapon(player, choice))
			continue;

		player->key_choices[key] = i % wk->numchoices;
		break;
	}
#endif

	// NEW CODE

	weapondef_c *ready_info = NULL;
	if (p->ready_wp >= 0)
		ready_info = p->weapons[p->ready_wp].info;

	int base_pri = 0;

	if (p->ready_wp >= 0)
		base_pri = p->weapons[p->ready_wp].info->KeyPri(p->ready_wp);

	int close_idx = -1;
	int close_pri = 99999999;
	int wrap_idx  = -1;
	int wrap_pri  = close_pri;

	for (int i = 0; i < MAXWEAPONS; i++)
	{
		if (i == p->ready_wp)
			continue;

		if (! p->weapons[i].owned)
			continue;

		weapondef_c *info = p->weapons[i].info;

		if (info->bind_key != key)
			continue;

		if (! P_CheckWeaponSprite(info))
			continue;

		// when key & priority are the same, use the index value
		// to break the deadlock.
		int new_pri = info->KeyPri(i);

		// if the key is different, choose last weapon used on that key
		if (ready_info && ready_info->bind_key != key)
		{
			if (p->key_choices[key] >= 0)
			{
				p->pending_wp = p->key_choices[key];
				return;
			}

			// if no last weapon, choose HIGHEST priority
			if (ready_info && ready_info->bind_key != key)
			{
				if (close_idx < 0 || new_pri > close_pri)
					close_idx = i, close_pri = new_pri;
			}
		}
		else  // on same key, use sequence logic
		{
			if (new_pri > base_pri && new_pri < close_pri)
				close_idx = i, close_pri = new_pri;

			if (new_pri < wrap_pri)
				wrap_idx = i, wrap_pri = new_pri;
		}
	}

	if (close_idx >= 0)
		p->pending_wp = (weapon_selection_e) close_idx;
	else if (wrap_idx >= 0)
		p->pending_wp = (weapon_selection_e) wrap_idx;
}


//
// P_NextPrevWeapon
//
// Select the next (or previous) weapon which can be fired.
// The 'dir' parameter is +1 for next (i.e. higher key number)
// and -1 for previous (lower key number).  When no such
// weapon exists, nothing happens.
//
// -AJA- 2005/02/17: added this.
//
void P_NextPrevWeapon(player_t * p, int dir)
{
	if (p->pending_wp != WPSEL_NoChange)
		return;

	int base_pri = 0;

	if (p->ready_wp >= 0)
		base_pri = p->weapons[p->ready_wp].info->KeyPri(p->ready_wp);

	int close_idx = -1;
	int close_pri = dir * 99999999;
	int wrap_idx  = -1;
	int wrap_pri  = close_pri;

	for (int i = 0; i < MAXWEAPONS; i++)
	{
		if (i == p->ready_wp)
			continue;

		if (! p->weapons[i].owned)
			continue;

		weapondef_c *info = p->weapons[i].info;

		if (info->bind_key < 0)
			continue;

		if (! WeaponCouldAutoFire(p, i, 0))
			continue;

		if (! P_CheckWeaponSprite(info))
			continue;

		// when key & priority are the same, use the index value
		// to break the deadlock.
		int new_pri = info->KeyPri(i);

		if (dir > 0)
		{
			if (new_pri > base_pri && new_pri < close_pri)
				close_idx = i, close_pri = new_pri;

			if (new_pri < wrap_pri)
				wrap_idx = i, wrap_pri = new_pri;
		}
		else  /* dir < 0 */
		{
			if (new_pri < base_pri && new_pri > close_pri)
				close_idx = i, close_pri = new_pri;

			if (new_pri > wrap_pri)
				wrap_idx = i, wrap_pri = new_pri;
		}
	}

	if (close_idx >= 0)
		p->pending_wp = (weapon_selection_e) close_idx;
	else if (wrap_idx >= 0)
		p->pending_wp = (weapon_selection_e) wrap_idx;
}


//
// P_SelectNewWeapon
//
// Out of ammo, pick a weapon to change to.
// Preferences are set here.
//
// The `ammo' parameter is normally AM_DontCare, meaning that the user
// ran out of ammo while firing.  Otherwise it is some ammo just
// picked up by the player.
//
// This routine deliberately ignores second attacks.
//
void P_SelectNewWeapon(player_t * p, int priority, ammotype_e ammo)
{
	int key = -1;
	weapondef_c *info;

	for (int i = 0; i < MAXWEAPONS; i++)
	{
		info = p->weapons[i].info;

		if (! p->weapons[i].owned)
			continue;

		if (info->dangerous || info->priority < priority)
			continue;

		if (ammo != AM_DontCare && info->ammo[0] != ammo)
			continue;

		if (! WeaponCouldAutoFire(p, i, 0))
			continue;

		if (! P_CheckWeaponSprite(info))
			continue;

		p->pending_wp = (weapon_selection_e) i;
		priority = info->priority;
		key = info->bind_key;
	}

	// all out of choices ?
	if (priority < 0)
	{
		p->pending_wp = (ammo == AM_DontCare) ? WPSEL_None : WPSEL_NoChange;
		return;
	}

	if (p->pending_wp == p->ready_wp)
	{
		p->pending_wp = WPSEL_NoChange;
		return;
	}
}


void P_TrySwitchNewWeapon(player_t *p, int new_weap, ammotype_e new_ammo)
{
	// be cheeky... :-)
	if (new_weap >= 0)
		p->grin_count = GRIN_TIME;

	if (p->pending_wp != WPSEL_NoChange)
		return;

	if (! level_flags.weapon_switch && p->ready_wp != WPSEL_None &&
		(WeaponCouldAutoFire(p, p->ready_wp, 0) ||
		 WeaponCouldAutoFire(p, p->ready_wp, 1)))
	{
		return;
	}

	if (new_weap >= 0)
	{
		if ( WeaponCouldAutoFire(p, new_weap, 0))
			p->pending_wp = (weapon_selection_e) new_weap;
		return;
	}

	SYS_ASSERT(new_ammo >= 0);
	
	// We were down to zero ammo, so select a new weapon.
	// Choose the next highest priority weapon than the current one.
	// Don't override any weapon change already underway.
	// Don't change weapon if NO_SWITCH is true.

	int priority = -100;

	if (p->ready_wp >= 0)
	{
		weapondef_c *w = p->weapons[p->ready_wp].info;

		if (! (w->specials[0] & WPSP_SwitchAway))
			return;

		priority = w->priority;
	}

	P_SelectNewWeapon(p, priority, new_ammo);
}


bool P_TryFillNewWeapon(player_t *p, int idx, ammotype_e ammo, int *qty)
{
	// When ammo is AM_DontCare, uses any ammo the player has (qty parameter
	// ignored).  Returns true if uses any of the ammo.

	bool result = false;

	weapondef_c *info = p->weapons[idx].info;

	for (int ATK = 0; ATK < 2; ATK++)
	{
		if (! info->attack_state[ATK])
			continue;

		// note: NoAmmo+Clip weapons are handled in P_AddWeapon
		if (info->ammo[ATK] == AM_NoAmmo || info->clip_size[ATK] == 0)
			continue;

		if (ammo != AM_DontCare && info->ammo[ATK] != ammo)
			continue;

		if (ammo == AM_DontCare)
			qty = &p->ammo[info->ammo[ATK]].num;

		SYS_ASSERT(qty);

		if (info->clip_size[ATK] <= *qty)
		{
			p->weapons[idx].clip_size[ATK] = info->clip_size[ATK];
			*qty -= info->clip_size[ATK];

			result = true;
		}
	}

	return result;
}


void P_FillWeapon(player_t *p, int slot)
{
	weapondef_c *info = p->weapons[slot].info;

	for (int ATK = 0; ATK < 2; ATK++)
	{
		if (! info->attack_state[ATK])
			continue;

		if (info->ammo[ATK] == AM_NoAmmo)
		{
			if (info->clip_size[ATK] > 0)
				p->weapons[slot].clip_size[ATK] = info->clip_size[ATK];

			continue;
		}

		p->weapons[slot].clip_size[ATK] = info->clip_size[ATK];
	}
}
	

void P_DropWeapon(player_t * p)
{
	// Player died, so put the weapon away.

	p->remember_atk[0] = -1;
	p->remember_atk[1] = -1;

	if (p->ready_wp != WPSEL_None)
		GotoDownState(p);
}


void P_SetupPsprites(player_t * p)
{
	// --- Called at start of level for each player ---

	// remove all psprites
	for (int i = 0; i < NUMPSPRITES; i++)
	{
		pspdef_t *psp = &p->psprites[i];

		psp->state = NULL;
		psp->next_state = NULL;
		psp->sx = psp->sy = 0;
		psp->visibility = psp->vis_target = VISIBLE;
	}

	// choose highest priority FREE weapon as the default
	if (p->ready_wp == WPSEL_None)
		P_SelectNewWeapon(p, -100, AM_DontCare);
	else
		p->pending_wp = p->ready_wp;

	P_BringUpWeapon(p);
}


#define MAX_PSP_LOOP  10

void P_MovePsprites(player_t * p)
{
	// --- Called every tic by player thinking routine ---

	// check if player has NO weapon but wants to change
	if (p->ready_wp == WPSEL_None && p->pending_wp != WPSEL_NoChange)
	{
		P_BringUpWeapon(p);
	}

	pspdef_t *psp = &p->psprites[0];

	for (int i = 0; i < NUMPSPRITES; i++, psp++)
	{
		// a null state means not active
		if (! psp->state)
			continue;

		for (int loop_count=0; loop_count < MAX_PSP_LOOP; loop_count++)
		{
			// drop tic count and possibly change state
			// Note: a -1 tic count never changes.
			if (psp->tics < 0)
				break;

			psp->tics--;

			if (psp->tics > 0)
				break;

			weapondef_c *info = NULL;
			if (p->ready_wp >= 0)
				info = p->weapons[p->ready_wp].info;

			P_SetPsprite(p, i, psp->next_state ?
					(psp->next_state - states) : S_NULL, info);

			if (psp->tics != 0)
				break;
		}

		// handle translucency fades
		psp->visibility = (34 * psp->visibility + psp->vis_target) / 35;
	}

	p->psprites[ps_flash].sx = p->psprites[ps_weapon].sx;
	p->psprites[ps_flash].sy = p->psprites[ps_weapon].sy;

	p->idlewait++;
}



//----------------------------------------------------------------------------
//  ACTION HANDLERS
//----------------------------------------------------------------------------


static void BobWeapon(player_t *p, weapondef_c *info)
{
	bool hasjetpack = p->powers[PW_Jetpack] > 0;
	pspdef_t *psp = &p->psprites[p->action_psp];

	float new_sx = 0;
	float new_sy = 0;
	
	// bob the weapon based on movement speed
	if (! hasjetpack)
	{
		angle_t angle = (128 * leveltime) << 19;
		new_sx = p->bob * PERCENT_2_FLOAT(info->swaying) * M_Cos(angle);

		angle &= (ANG180 - 1);
		new_sy = p->bob * PERCENT_2_FLOAT(info->bobbing) * M_Sin(angle);
	}

	psp->sx = new_sx;
	psp->sy = new_sy;

#if 0  // won't really work with low framerates
	// try to prevent noticeable jumps
	if (fabs(new_sx - psp->sx) <= MAX_BOB_DX)
		psp->sx = new_sx;
	else
		psp->sx += (new_sx > psp->sx) ? MAX_BOB_DX : -MAX_BOB_DX;

	if (fabs(new_sy - psp->sy) <= MAX_BOB_DY)
		psp->sy = new_sy;
	else
		psp->sy += (new_sy > psp->sy) ? MAX_BOB_DY : -MAX_BOB_DY;
#endif
}


//
// A_WeaponReady
//
// The player can fire the weapon
// or change to another weapon at this time.
// Follows after getting weapon up,
// or after previous attack/fire sequence.
//
void A_WeaponReady(mobj_t * mo)
{
	player_t *p = mo->player;
	pspdef_t *psp = &p->psprites[p->action_psp];

	SYS_ASSERT(p->ready_wp != WPSEL_None);

	weapondef_c *info = p->weapons[p->ready_wp].info;

	// check for change if player is dead, put the weapon away
	if (p->pending_wp != WPSEL_NoChange || p->health <= 0)
	{
		// change weapon (pending weapon should already be validated)
		GotoDownState(p);
		return;
	}

	// check for emptiness.  The ready_state check is needed since this
	// code is also used by the EMPTY action (prevent looping).
	if (info->empty_state && ! WeaponCouldAutoFire(p, p->ready_wp, 0) &&
		psp->state == &states[info->ready_state])
	{
		// don't use Deferred here, since we don't want the weapon to
		// display the ready sprite (even only briefly).
		P_SetPsprite(p, ps_weapon, info->empty_state, info);
		return;
	}

	if (info->idle && (psp->state == &states[info->ready_state] ||
		(info->empty_state && psp->state == &states[info->empty_state])))
	{
		S_StartFX(info->idle, WeapSfxCat(p), mo);
	}

	bool fire_0 = ButtonDown(p, 0);
	bool fire_1 = ButtonDown(p, 1);

	if (fire_0 != fire_1)
	{
		for (int ATK = 0; ATK < 2; ATK++)
		{
			if (! ButtonDown(p, ATK))
				continue;

			if (! info->attack_state[ATK])
				continue;

			// check for fire: the missile launcher and bfg do not auto fire
			if (!p->attackdown[ATK] || info->autofire[ATK])
			{
				p->attackdown[ATK] = true;
				p->flash = false;

				if (WeaponCanFire(p, p->ready_wp, ATK))
					GotoAttackState(p, ATK, true);
				else
					SwitchAway(p, ATK, info->specials[ATK] & WPSP_Trigger);

				return;  // leave now
			}
		}
	}

	// reset memory of held buttons (must be done right here)
	if (! fire_0) p->attackdown[0] = false;
	if (! fire_1) p->attackdown[1] = false;

	// give that weapon a polish, soldier!
	if (info->idle_state && p->idlewait >= info->idle_wait)
	{
		if (M_RandomTest(info->idle_chance))
		{
			p->idlewait = 0;
			P_SetPspriteDeferred(p, ps_weapon, info->idle_state);
		}
		else
		{
			// wait another (idle_wait / 10) seconds before trying again
			p->idlewait = info->idle_wait * 9 / 10;
		}
	}

	// handle manual reload and fresh-ammo reload
	if (! fire_0 && ! fire_1)
	{
		for (int ATK = 0; ATK < 2; ATK++)
		{
			if (! info->attack_state[ATK])
				continue;

			if ((info->specials[ATK] & WPSP_Fresh) &&
				(info->clip_size[ATK] > 0) &&
				! WeaponCanFire(p, p->ready_wp, ATK) &&
				WeaponCanReload(p, p->ready_wp, ATK, true))
			{
				GotoReloadState(p, ATK);
				break;
			}

			if ((p->cmd.extbuttons & EBT_RELOAD) &&
				 (info->clip_size[ATK] > 0) &&
				 (info->specials[ATK] & WPSP_Manual) &&
				  info->reload_state[ATK])
			{
				bool reload = WeaponCanReload(p, p->ready_wp, ATK, true);

				// for discarding, we require a non-empty clip
				if (reload && info->discard_state[ATK] &&
					WeaponCanFire(p, p->ready_wp, ATK))
				{
					p->weapons[p->ready_wp].clip_size[ATK] = 0;
					P_SetPspriteDeferred(p, ps_weapon, info->discard_state[ATK]);
					break;
				}
				else if (reload)
				{
					GotoReloadState(p, ATK);
					break;
				}
			}
		}  // for (ATK)

	}  // (! fire_0 && ! fire_1)

	BobWeapon(p, info);
}


void A_WeaponEmpty(mobj_t * mo)
{
	A_WeaponReady(mo);
}

//
// A_ReFire
//
// The player can re-fire the weapon without lowering it entirely.
//
// -AJA- 1999/08/10: Reworked for multiple attacks.
//
static void DoReFire(mobj_t * mo, int ATK)
{
	player_t *p = mo->player;

	if (p->pending_wp >= 0 || p->health <= 0)
	{
		GotoDownState(p);
		return;
	}

	weapondef_c *info = p->weapons[p->ready_wp].info;

	p->remember_atk[ATK] = -1;

	// check for fire
	// (if a weaponchange is pending, let it go through instead)

	if (ButtonDown(p, ATK))
	{
		// -KM- 1999/01/31 Check for semiautomatic weapons.
		if (!p->attackdown[ATK] || info->autofire[ATK])
		{
			p->refire++;
			p->flash = false;

			if (WeaponCanFire(p, p->ready_wp, ATK))
				GotoAttackState(p, ATK, false);
			else
				SwitchAway(p, ATK, info->specials[ATK] & WPSP_Trigger);
			return;
		}
	}

	p->refire = info->refire_inacc ? 0 : 1;

	if (! WeaponCouldAutoFire(p, p->ready_wp, ATK))
		SwitchAway(p, ATK, 0);
}

void A_ReFire  (mobj_t * mo) { DoReFire(mo, 0); }
void A_ReFireSA(mobj_t * mo) { DoReFire(mo, 1); }

//
// A_NoFire
//
// If the player is still holding the fire button, continue, otherwise
// return to the weapon ready states.
//
// -AJA- 1999/08/18: written.
//
static void DoNoFire(mobj_t * mo, int ATK, bool does_return)
{
	player_t *p = mo->player;
	pspdef_t *psp = &p->psprites[p->action_psp];

	if (p->pending_wp >= 0 || p->health <= 0)
	{
		GotoDownState(p);
		return;
	}

	weapondef_c *info = p->weapons[p->ready_wp].info;

	p->remember_atk[ATK] = -1;

	// check for fire
	//  (if a weaponchange is pending, let it go through instead)

	if (ButtonDown(p, ATK))
	{
		// -KM- 1999/01/31 Check for semiautomatic weapons.
		if (!p->attackdown[ATK] || info->autofire[ATK])
		{
			p->refire++;
			p->flash = false;

			if (! WeaponCanFire(p, p->ready_wp, ATK))
				SwitchAway(p, ATK, info->specials[ATK] & WPSP_Trigger);
			return;
		}
	}

	p->refire = info->refire_inacc ? 0 : 1;
	p->remember_atk[ATK] = does_return ? psp->state->nextstate : -1;

	if (WeaponCouldAutoFire(p, p->ready_wp, ATK))
		GotoReadyState(p);
	else
		SwitchAway(p, ATK, 0);
}

void A_NoFire  (mobj_t * mo)       { DoNoFire(mo, 0, false); }
void A_NoFireSA(mobj_t * mo)       { DoNoFire(mo, 1, false); }
void A_NoFireReturn  (mobj_t * mo) { DoNoFire(mo, 0, true);  }
void A_NoFireReturnSA(mobj_t * mo) { DoNoFire(mo, 1, true);  }


void A_WeaponKick(mobj_t * mo)
{
	player_t *p = mo->player;
	pspdef_t *psp = &p->psprites[p->action_psp];

	float kick = 0.05f;

	if (! level_flags.kicking)
		return;

	if (psp->state && psp->state->action_par)
		kick = ((float *) psp->state->action_par)[0];

	p->deltaviewheight -= kick;
	p->kick_offset = kick;
}

//
// A_CheckReload
//
// Check whether the player has used up the clip quantity of ammo.
// If so, must reload.
//
// For weapons with a clip, only reloads when clip_size is 0 (and
// enough ammo available to fill it).  For non-clip weapons, reloads
// when enough ammo exists in the "ammo bucket" (for NO_AMMO weapons,
// it always reloads).
//
// -KM- 1999/01/31 Check clip size.
// -AJA- 1999/08/11: Reworked for new playerweapon_t field.
//
static void DoCheckReload(mobj_t * mo, int ATK)
{
	player_t *p = mo->player;

	if (p->pending_wp >= 0 || p->health <= 0)
	{
		GotoDownState(p);
		return;
	}

//	SYS_ASSERT(p->ready_wp >= 0);
//
//	weapondef_c *info = p->weapons[p->ready_wp].info;

	if (WeaponCanReload(p, p->ready_wp, ATK, false))
		GotoReloadState(p, ATK);
	else if (! WeaponCanFire(p, p->ready_wp, ATK))
		SwitchAway(p, ATK, 0);
}

void A_CheckReload  (mobj_t * mo) { DoCheckReload(mo, 0); }
void A_CheckReloadSA(mobj_t * mo) { DoCheckReload(mo, 1); }


void A_Lower(mobj_t * mo)
{
	// Lowers current weapon, and changes weapon at bottom

	player_t *p = mo->player;
	pspdef_t *psp = &p->psprites[p->action_psp];

	weapondef_c *info = p->weapons[p->ready_wp].info;

	if (p->zoom_fov > 0)
	{
		// In `LimitZoom' mode, disable any current zoom
		if (level_flags.limit_zoom)
			p->zoom_fov = 0;
		else
			p->zoom_fov = r_zoomfov.d;
	}

	psp->sy += LOWERSPEED;

	// Is already down.
	if (! (info->specials[0] & WPSP_Animated))
		if (psp->sy < WEAPONBOTTOM-WEAPONTOP)
			return;

	psp->sy = WEAPONBOTTOM - WEAPONTOP;

	// Player is dead, don't bring weapon back up.
	if (p->playerstate == PST_DEAD || p->health <= 0)
	{
		p->ready_wp   = WPSEL_None;
		p->pending_wp = WPSEL_NoChange;

		P_SetPsprite(p, ps_weapon, S_NULL);
		return;
	}

	// handle weapons that were removed/upgraded while in use
	if (p->weapons[p->ready_wp].flags & PLWEP_Removing)
	{
		p->weapons[p->ready_wp].flags &= ~PLWEP_Removing;
		p->weapons[p->ready_wp].info = NULL;

		// this should not happen, but handle it just in case
		if (p->pending_wp == p->ready_wp)
			p->pending_wp = WPSEL_NoChange;

		p->ready_wp = WPSEL_None;
	}

	// The old weapon has been lowered off the screen,
	// so change the weapon and start raising it

	if (p->pending_wp == WPSEL_NoChange)
	{
		p->ready_wp = WPSEL_None;
		P_SelectNewWeapon(p, -100, AM_DontCare);
	}

	P_BringUpWeapon(p);
}


void A_Raise(mobj_t * mo)
{
	player_t *p = mo->player;
	pspdef_t *psp = &p->psprites[p->action_psp];

	weapondef_c *info = p->weapons[p->ready_wp].info;

	psp->sy -= RAISESPEED;

	if (psp->sy > 0)
		return;

	psp->sy = 0;

	// The weapon has been raised all the way,
	//  so change to the ready state.
	if (info->empty_state && ! WeaponCouldAutoFire(p, p->ready_wp, 0))
		GotoEmptyState(p);
	else
		GotoReadyState(p);
}


void A_SetCrosshair(mobj_t * mo)
{
	player_t *p = mo->player;
	pspdef_t *psp = &p->psprites[p->action_psp];

	if (psp->state->jumpstate == S_NULL)
		return;  // show warning ??

	P_SetPspriteDeferred(p, ps_crosshair, psp->state->jumpstate);
}

void A_TargetJump(mobj_t * mo)
{
	player_t *p = mo->player;
	pspdef_t *psp = &p->psprites[p->action_psp];

	if (psp->state->jumpstate == S_NULL)
		return;  // show warning ?? error ???

	atkdef_c *attack = p->weapons[p->ready_wp].info->attack[0];

	if (! attack)
		return;

	mobj_t *obj = P_MapTargetAutoAim(mo, mo->angle, attack->range, true);

	if (! obj)
		return;

	P_SetPspriteDeferred(p, ps_crosshair, psp->state->jumpstate);
}

void A_FriendJump(mobj_t * mo)
{
	player_t *p = mo->player;
	pspdef_t *psp = &p->psprites[p->action_psp];

	if (psp->state->jumpstate == S_NULL)
		return;  // show warning ?? error ???

	atkdef_c *attack = p->weapons[p->ready_wp].info->attack[0];

	if (! attack)
		return;

	mobj_t *obj = P_MapTargetAutoAim(mo, mo->angle, attack->range, true);

	if (! obj)
		return;

	if ((obj->side & mo->side) == 0 || obj->target == mo)
		return;

	P_SetPspriteDeferred(p, ps_crosshair, psp->state->jumpstate);
}


static void DoGunFlash(mobj_t * mo, int ATK)
{
	player_t *p = mo->player;
	
	SYS_ASSERT(p->ready_wp >= 0);

	weapondef_c *info = p->weapons[p->ready_wp].info;

	if (!p->flash)
	{
		p->flash = true;

		P_SetPspriteDeferred(p, ps_flash, info->flash_state[ATK]);

#if 0  // the SHOOT actions already do this...
		if (mo->info->missile_state)
			P_SetMobjStateDeferred(mo, mo->info->missile_state, 0);
#endif
	}
}

void A_GunFlash  (mobj_t * mo) { DoGunFlash(mo, 0); }
void A_GunFlashSA(mobj_t * mo) { DoGunFlash(mo, 1); }


static void DoWeaponShoot(mobj_t * mo, int ATK)
{
	player_t *p = mo->player;
	pspdef_t *psp = &p->psprites[p->action_psp];

	SYS_ASSERT(p->ready_wp >= 0);

	weapondef_c *info = p->weapons[p->ready_wp].info;
	atkdef_c *attack = info->attack[ATK];

	// -AJA- 1999/08/10: Multiple attack support.
	if (psp->state && psp->state->action_par)
		attack = (atkdef_c *) psp->state->action_par;

	if (! attack)
		I_Error("Weapon [%s] missing attack for %s action.\n",
			info->name.c_str(), ATK ? "SECSHOOT" : "SHOOT");

	// Some do not need ammunition anyway.
	// Return if current ammunition sufficient.
	if (! WeaponCanFire(p, p->ready_wp, ATK))
		return;

	int ATK_orig = ATK;
	if (info->shared_clip)
		ATK = 0;

	ammotype_e ammo = info->ammo[ATK];

	// Minimal amount for one shot varies.
	int count = info->ammopershot[ATK];

	if (info->clip_size[ATK] > 0)
	{
		p->weapons[p->ready_wp].clip_size[ATK] -= count;
		SYS_ASSERT(p->weapons[p->ready_wp].clip_size[ATK] >= 0);
	}
	else if (ammo != AM_NoAmmo)
	{
		p->ammo[ammo].num -= count;
		SYS_ASSERT(p->ammo[ammo].num >= 0);
	}

	P_PlayerAttack(mo, attack);

	if (level_flags.kicking && ATK == 0)
	{
		p->deltaviewheight -= info->kick;
		p->kick_offset = info->kick;
	}

	if (mo->target)
	{
		if (info->hit)
			S_StartFX(info->hit, WeapSfxCat(p), mo);

		if (info->feedback)
			mo->flags |= MF_JUSTATTACKED;
	}
	else
	{
		if (info->engaged)
			S_StartFX(info->engaged, WeapSfxCat(p), mo);
	}

	// show the player making the shot/attack...
	if (attack && attack->attackstyle == ATK_CLOSECOMBAT &&
			mo->info->melee_state)
	{
		P_SetMobjStateDeferred(mo, mo->info->melee_state, 0);
	}
	else if (mo->info->missile_state)
	{
		P_SetMobjStateDeferred(mo, mo->info->missile_state, 0);
	}

	ATK = ATK_orig;

	if (info->flash_state[ATK] && !p->flash)
	{
		p->flash = true;
		P_SetPspriteDeferred(p, ps_flash, info->flash_state[ATK]);
	}

	// wake up monsters
	if (! (info->specials[ATK] & WPSP_SilentToMon) &&
		! (attack->flags & AF_SilentToMon))
	{
		P_NoiseAlert(p);
	}

	p->idlewait = 0;
}

void A_WeaponShoot  (mobj_t * mo) { DoWeaponShoot(mo, 0); }
void A_WeaponShootSA(mobj_t * mo) { DoWeaponShoot(mo, 1); }


//
// Used for ejecting shells (or other effects).
//
// -AJA- 1999/09/10: written.
//
void A_WeaponEject(mobj_t * mo)
{
	player_t *p = mo->player;
	pspdef_t *psp = &p->psprites[p->action_psp];

	weapondef_c *info = p->weapons[p->ready_wp].info;
	atkdef_c *attack = info->eject_attack;

	if (psp->state && psp->state->action_par)
		attack = (atkdef_c *) psp->state->action_par;

	if (! attack)
		I_Error("Weapon [%s] missing attack for EJECT action.\n",
			info->name.c_str());

	P_PlayerAttack(mo, attack);
}


void A_WeaponPlaySound(mobj_t * mo)
{
	// Generate an arbitrary sound from this weapon.

	player_t *p = mo->player;
	pspdef_t *psp = &p->psprites[p->action_psp];

	sfx_t *sound = NULL;

	if (psp->state && psp->state->action_par)
		sound = (sfx_t *) psp->state->action_par;

	if (! sound)
	{
		M_WarnError("A_WeaponPlaySound: missing sound name !\n");
		return;
	}

	S_StartFX(sound, WeapSfxCat(p), mo);
}


void A_WeaponKillSound(mobj_t * mo)
{
	// kill any current sound from this weapon

	S_StopFX(mo);
}


void A_SFXWeapon1(mobj_t * mo)
{
	player_t *p = mo->player;
	S_StartFX(p->weapons[p->ready_wp].info->sound1, WeapSfxCat(p), mo);
}

void A_SFXWeapon2(mobj_t * mo)
{
	player_t *p = mo->player;
	S_StartFX(p->weapons[p->ready_wp].info->sound2, WeapSfxCat(p), mo);
}

void A_SFXWeapon3(mobj_t * mo)
{
	player_t *p = mo->player;
	S_StartFX(p->weapons[p->ready_wp].info->sound3, WeapSfxCat(p), mo);
}

//
// These three routines make a flash of light when a weapon fires.
//
void A_Light0(mobj_t * mo) { mo->player->extralight = 0; }
void A_Light1(mobj_t * mo) { mo->player->extralight = 1; }
void A_Light2(mobj_t * mo) { mo->player->extralight = 2; }


void A_WeaponJump(mobj_t * mo)
{
	player_t *p = mo->player;
	pspdef_t *psp = &p->psprites[p->action_psp];

	weapondef_c *info = p->weapons[p->ready_wp].info;

	act_jump_info_t *jump;

	if (!psp->state || !psp->state->action_par)
	{
		M_WarnError("JUMP used in weapon [%s] without a label !\n",
				info->name.c_str());
		return;
	}

	jump = (act_jump_info_t *) psp->state->action_par;

	SYS_ASSERT(jump->chance >= 0);
	SYS_ASSERT(jump->chance <= 1);

	if (P_RandomTest(jump->chance))
	{
		psp->next_state = (psp->state->jumpstate == S_NULL) ? NULL :
			(states + psp->state->jumpstate);
	}
}

void A_WeaponDJNE(mobj_t * mo)
{
	player_t *p = mo->player;
	pspdef_t *psp = &p->psprites[p->action_psp];

	weapondef_c *info = p->weapons[p->ready_wp].info;

	act_jump_info_t *jump;

	if (!psp->state || !psp->state->action_par)
	{
		M_WarnError("DJNE used in weapon [%s] without a label !\n",
				info->name.c_str());
		return;
	}

	jump = (act_jump_info_t *) psp->state->action_par;

	SYS_ASSERT(jump->chance >= 0);
	SYS_ASSERT(jump->chance <= 1);
	int ATK = jump->chance > 0 ? 1 : 0;

	if (--p->weapons[p->ready_wp].reload_count[ATK] > 0)
	{
		psp->next_state = (psp->state->jumpstate == S_NULL) ? NULL :
			(states + psp->state->jumpstate);
	}
}



void A_WeaponTransSet(mobj_t * mo)
{
	player_t *p = mo->player;
	pspdef_t *psp = &p->psprites[p->action_psp];
	float value = VISIBLE;

	if (psp->state && psp->state->action_par)
	{
		value = ((percent_t *) psp->state->action_par)[0];
		value = MAX(0.0f, MIN(1.0f, value));
	}

	psp->visibility = psp->vis_target = value;
}


void A_WeaponTransFade(mobj_t * mo)
{
	player_t *p = mo->player;
	pspdef_t *psp = &p->psprites[p->action_psp];

	float value = INVISIBLE;

	if (psp->state && psp->state->action_par)
	{
		value = ((percent_t *) psp->state->action_par)[0];
		value = MAX(0.0f, MIN(1.0f, value));
	}

	psp->vis_target = value;
}


void A_WeaponEnableRadTrig(mobj_t *mo)
{
	player_t *p = mo->player;
	pspdef_t *psp = &p->psprites[p->action_psp];

	if (psp->state && psp->state->action_par)
	{
		int tag = *(int *)psp->state->action_par;
		RAD_EnableByTag(mo, tag, false);
	}
}

void A_WeaponDisableRadTrig(mobj_t *mo)
{
	player_t *p = mo->player;
	pspdef_t *psp = &p->psprites[p->action_psp];

	if (psp->state && psp->state->action_par)
	{
		int tag = *(int *)psp->state->action_par;
		RAD_EnableByTag(mo, tag, true);
	}
}


void A_WeaponSetSkin(mobj_t * mo)
{
	player_t *p = mo->player;
	pspdef_t *psp = &p->psprites[p->action_psp];

	SYS_ASSERT(p->ready_wp >= 0);
	weapondef_c *info = p->weapons[p->ready_wp].info;

	const state_t *st = psp->state;

	if (st && st->action_par)
	{
		int skin = ((int *)st->action_par)[0];

		if (skin < 0 || skin > 9)
			I_Error("Weapon [%s]: Bad skin number %d in SET_SKIN action.\n",
					info->name.c_str(), skin);

		p->weapons[p->ready_wp].model_skin = skin;
	}
}


void A_WeaponUnzoom(mobj_t * mo)
{
	player_t *p = mo->player;

	p->zoom_fov = 0;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
