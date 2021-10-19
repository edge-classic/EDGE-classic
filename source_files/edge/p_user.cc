//----------------------------------------------------------------------------
//  EDGE Player User Code
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

#include <float.h>

#include "colormap.h"

#include "dm_state.h"
#include "e_input.h"
#include "m_random.h"
#include "n_network.h"
#include "p_bot.h"
#include "p_local.h"
#include "rad_trig.h"
#include "s_sound.h"
#include "z_zone.h"


static void P_UpdatePowerups(player_t *player);

#define MAXBOB  16.0f

#define ZOOM_ANGLE_DIV  4

static sfx_t * sfx_jpidle;
static sfx_t * sfx_jpmove;
static sfx_t * sfx_jprise;
static sfx_t * sfx_jpdown;
static sfx_t * sfx_jpflow;

static void CalcHeight(player_t * player)
{
	bool onground = player->mo->z <= player->mo->floorz;

	if (player->mo->height < (player->mo->info->height + player->mo->info->crouchheight) / 2.0f)
		player->mo->extendedflags |= EF_CROUCHING;
	else
		player->mo->extendedflags &= ~EF_CROUCHING;

	player->std_viewheight = player->mo->height * PERCENT_2_FLOAT(player->mo->info->viewheight);

	// calculate the walking / running height adjustment.

	float bob_z = 0;

	// Regular movement bobbing 
	// (needs to be calculated for gun swing even if not on ground).  
	// -AJA- Moved up here, to prevent weapon jumps when running down
	// stairs.

	player->bob = (player->mo->mom.x * player->mo->mom.x
		+ player->mo->mom.y * player->mo->mom.y) / 8;

	if (player->bob > MAXBOB)
		player->bob = MAXBOB;

	// ----CALCULATE BOB EFFECT----
	if (player->playerstate == PST_LIVE && onground)
	{
		angle_t angle = ANG90 / 5 * leveltime;

		bob_z = player->bob / 2 * player->mo->info->bobbing * M_Sin(angle);
	}

	// ----CALCULATE VIEWHEIGHT----
	if (player->playerstate == PST_LIVE)
	{
		player->viewheight += player->deltaviewheight;

		if (player->viewheight > player->std_viewheight)
		{
			player->viewheight = player->std_viewheight;
			player->deltaviewheight = 0;
		}
		else if (player->viewheight < player->std_viewheight / 2)
		{
			player->viewheight = player->std_viewheight / 2;

			if (player->deltaviewheight <= 0)
				player->deltaviewheight = 0.01f;
		}

		if (player->deltaviewheight != 0)
		{
			// use a weird number to minimise chance of hitting
			// zero when deltaviewheight goes neg -> positive.
			player->deltaviewheight += 0.24162f;
		}
	}


	//----CALCULATE FREEFALL EFFECT, WITH SOUND EFFECTS (code based on HEXEN)
	//  CORBIN, on:
	//  6/6/2011 - Fix this so RTS does NOT interfere with fracunits (it does in Hypertension's E1M1 starting script)!
    //  6/7/2011 - Ajaped said to remove FRACUNIT...seeya oldness.

	//if ((player->mo->mom.z <= -35.0)&&(player->mo->mom.z >= -40.0))
	if ((player->mo->mom.z <= -35.0)&&(player->mo->mom.z >= -36.0))
	if (player->mo->info->falling_sound)
	{
	    int sfx_cat;

		if (player == players[consoleplayer]) {
			sfx_cat = SNCAT_Player;
		} else {
			sfx_cat = SNCAT_Opponent;
		}
		S_StartFX(player->mo->info->falling_sound, sfx_cat, player->mo);
	}


	// don't apply bobbing when jumping, but have a smooth
	// transition at the end of the jump.
	if (player->jumpwait > 0)
	{
		if (player->jumpwait >= 6)
			bob_z = 0;
		else
			bob_z *= (6 - player->jumpwait) / 6.0;
	}

	player->viewz = player->viewheight + bob_z;

#if 0  // DEBUG
I_Debugf("Jump:%d bob_z:%1.2f  z:%1.2f  height:%1.2f delta:%1.2f --> viewz:%1.3f\n",
		 player->jumpwait, bob_z, player->mo->z,
		 player->viewheight, player->deltaviewheight,
		 player->mo->z + player->viewz);
#endif
}


void P_PlayerJump(player_t *pl, float dz, int wait)
{
	pl->mo->mom.z += pl->mo->info->jumpheight / 1.4f;

	if (pl->jumpwait < wait)
		pl->jumpwait = wait;

	// enter the JUMP states (if present)
	statenum_t jump_st = P_MobjFindLabel(pl->mo, "JUMP");
	if (jump_st != S_NULL)
		P_SetMobjStateDeferred(pl->mo, jump_st, 0);

	// -AJA- 1999/09/11: New JUMP_SOUND for ddf.
	if (pl->mo->info->jump_sound)
	{
		int sfx_cat;

		if (pl == players[consoleplayer])
			sfx_cat = SNCAT_Player;
		else
			sfx_cat = SNCAT_Opponent;

		S_StartFX(pl->mo->info->jump_sound, sfx_cat, pl->mo);
	}
}


static void MovePlayer(player_t * player)
{
	ticcmd_t *cmd;
	mobj_t *mo = player->mo;

	bool onground = player->mo->z <= player->mo->floorz;
	bool onladder = player->mo->on_ladder >= 0;

	bool swimming  = player->swimming;
	bool flying    = (player->powers[PW_Jetpack] > 0) && ! swimming;
	bool jumping   = (player->jumpwait > 0);
	bool crouching = (player->mo->extendedflags & EF_CROUCHING) ? true : false;

	float dx, dy;
	float eh, ev;

	float base_xy_speed;
	float base_z_speed;

	float F_vec[3], U_vec[3], S_vec[3];

	cmd = &player->cmd;

	if (player->zoom_fov > 0)
		cmd->angleturn /= ZOOM_ANGLE_DIV;

	player->mo->angle -= (angle_t)(cmd->angleturn << 16);

	// EDGE Feature: Vertical Look (Mlook)
	//
	// -ACB- 1998/07/02 New Code used, rerouted via Ticcmd
	// -ACB- 1998/07/27 Used defines for look limits.
	//
	if (level_flags.mlook)
	{
		if (player->zoom_fov > 0)
			cmd->mlookturn /= ZOOM_ANGLE_DIV;

		angle_t V = player->mo->vertangle + (angle_t)(cmd->mlookturn << 16);

		if (V < ANG180 && V > MLOOK_LIMIT)
			V = MLOOK_LIMIT;
		else if (V >= ANG180 && V < (ANG_MAX - MLOOK_LIMIT))
			V = (ANG_MAX - MLOOK_LIMIT);

		player->mo->vertangle = V;
	}
	else
	{
		player->mo->vertangle = 0;
	}

	// EDGE Feature: Vertical Centering
	//
	// -ACB- 1998/07/02 Re-routed via Ticcmd
	//
	if (cmd->extbuttons & EBT_CENTER)
		player->mo->vertangle = 0;

	// compute XY and Z speeds, taking swimming (etc) into account
	// (we try to swim in view direction -- assumes no gravity).

	base_xy_speed = player->mo->speed / 32.0f;
	base_z_speed  = player->mo->speed / 64.0f;

	// Do not let the player control movement if not onground.
	// -MH- 1998/06/18  unless he has the JetPack!

	if (! (onground || onladder || swimming || flying))
		base_xy_speed /= 16.0f;

	if (! (onladder || swimming || flying))
		base_z_speed /= 16.0f;

	// move slower when crouching
	if (crouching)
		base_xy_speed *= CROUCH_SLOWDOWN;

	dx = M_Cos(player->mo->angle);
	dy = M_Sin(player->mo->angle);

	eh = 1;
	ev = 0;

	if (swimming || flying)
	{
		float slope = M_Tan(player->mo->vertangle);

		float hyp = (float)sqrt((double)(1.0f + slope * slope));

		eh = 1.0f  / hyp;
		ev = slope / hyp;
	}

	// compute movement vectors

	F_vec[0] = eh * dx * base_xy_speed;
	F_vec[1] = eh * dy * base_xy_speed;
	F_vec[2] = ev * base_z_speed;

	S_vec[0] =  dy * base_xy_speed;
	S_vec[1] = -dx * base_xy_speed;
	S_vec[2] =  0;

	U_vec[0] = -ev * dx * base_xy_speed;
	U_vec[1] = -ev * dy * base_xy_speed;
	U_vec[2] =  eh * base_z_speed;

	player->mo->mom.x += F_vec[0] * cmd->forwardmove + S_vec[0] *
		cmd->sidemove + U_vec[0] * cmd->upwardmove;

	player->mo->mom.y += F_vec[1] * cmd->forwardmove + S_vec[1] *
		cmd->sidemove + U_vec[1] * cmd->upwardmove;

	if (flying || swimming || !onground || onladder)
	{
		player->mo->mom.z += F_vec[2] * cmd->forwardmove + S_vec[2] *
			cmd->sidemove + U_vec[2] * cmd->upwardmove;
	}

	if (flying && !swimming)
	{
        int sfx_cat;

        if (player == players[consoleplayer])
            sfx_cat = SNCAT_Player;
        else
            sfx_cat = SNCAT_Opponent;

		if (player->powers[PW_Jetpack] <= (5 * TICRATE))
		{
			if ((leveltime & 10) == 0)
				S_StartFX(sfx_jpflow, sfx_cat, player->mo);  // fuel low
		}
		else if (cmd->upwardmove > 0)
			S_StartFX(sfx_jprise, sfx_cat, player->mo);
		else if (cmd->upwardmove < 0)
			S_StartFX(sfx_jpdown, sfx_cat, player->mo);
		else if (cmd->forwardmove || cmd->sidemove)
			S_StartFX((onground ? sfx_jpidle : sfx_jpmove), sfx_cat, player->mo);
		else
			S_StartFX(sfx_jpidle, sfx_cat, player->mo);
	}

	if (player->mo->state == &states[player->mo->info->idle_state])
	{
		if (!jumping && !flying && (onground || swimming) &&
		    (cmd->forwardmove || cmd->sidemove))
		{
			// enter the CHASE (i.e. walking) states
			if (player->mo->info->chase_state)
				P_SetMobjStateDeferred(player->mo, player->mo->info->chase_state, 0);
		}
	}

	// EDGE Feature: Jump Code
	//
	// -ACB- 1998/08/09 Check that jumping is allowed in the currmap
	//                  Make player pause before jumping again

	if (level_flags.jump && mo->info->jumpheight > 0 &&
	    (cmd->upwardmove > 4))
	{
		if (!jumping && !crouching && !swimming && !flying && onground && !onladder)
		{
			P_PlayerJump(player, player->mo->info->jumpheight / 1.4f,
			             player->mo->info->jump_delay);
		}
	}

	// EDGE Feature: Crouching

	if (level_flags.crouch && mo->info->crouchheight > 0 &&
		(player->cmd.upwardmove < -4) &&
		!player->wet_feet && !jumping && onground)
		// NB: no ladder check, onground is sufficient
	{
		if (mo->height > mo->info->crouchheight)
		{
			mo->height = MAX(mo->height - 2.0f, mo->info->crouchheight);

			// update any things near the player
			P_ChangeThingSize(mo);

			mo->player->deltaviewheight = -1.0f;
		}
	}
	else // STAND UP
	{
		if (mo->height < mo->info->height)
		{
			// prevent standing up inside a solid area
			if ((mo->flags & MF_NOCLIP) || mo->z+mo->height+2 <= mo->ceilingz)
			{
				mo->height = MIN(mo->height + 2, mo->info->height);

				// update any things near the player
				P_ChangeThingSize(mo);

				mo->player->deltaviewheight = 1.0f;
			}
		}
	}

	// EDGE Feature: Zooming
	//
	if (cmd->extbuttons & EBT_ZOOM)
	{
		int fov = 0;

		if (player->zoom_fov == 0)
		{
			if (! (player->ready_wp < 0 || player->pending_wp >= 0))
				fov = player->weapons[player->ready_wp].info->zoom_fov;

			// In `LimitZoom' mode, only allow zooming if weapon supports it
			if (fov <= 0 && !level_flags.limit_zoom)
				fov = r_zoomfov.d;
		}

		player->zoom_fov = fov;
	}
}


static void DeathThink(player_t * player)
{
	// fall on your face when dying.

	float dx, dy, dz;

	angle_t angle;
	angle_t delta, delta_s;
	float slope;

	// -AJA- 1999/12/07: don't die mid-air.
	player->powers[PW_Jetpack] = 0;

	P_MovePsprites(player);

	// fall to the ground
	if (player->viewheight > player->std_viewheight)
		player->viewheight -= 1.0f;
	else if (player->viewheight < player->std_viewheight)
		player->viewheight = player->std_viewheight;

	player->deltaviewheight = 0.0f;
	player->kick_offset = 0.0f;

	CalcHeight(player);

	if (player->attacker && player->attacker != player->mo)
	{
		dx = player->attacker->x - player->mo->x;
		dy = player->attacker->y - player->mo->y;
		dz = (player->attacker->z + player->attacker->height/2) - 
			(player->mo->z + player->viewheight);

		angle = R_PointToAngle(0, 0, dx, dy);
		delta = angle - player->mo->angle;

		slope = P_ApproxSlope(dx, dy, dz);
		slope = MIN(1.7f, MAX(-1.7f, slope));
		delta_s = M_ATan(slope) - player->mo->vertangle;

		if ((delta <= ANG1/2 || delta >= (angle_t)(0 - ANG1/2)) &&
			(delta_s <= ANG1/2 || delta_s >= (angle_t)(0 - ANG1/2)))
		{
			// Looking at killer, so fade damage flash down.
			player->mo->angle = angle;
			player->mo->vertangle = M_ATan(slope);

			if (player->damagecount > 0)
				player->damagecount--;
		}
		else 
		{
			if (delta < ANG180)
				delta /= 5;
			else
				delta = (angle_t)(0 - (angle_t)(0 - delta) / 5);
			
			if (delta > ANG5 && delta < (angle_t)(0 - ANG5))
				delta = (delta < ANG180) ? ANG5 : (angle_t)(0 - ANG5);

			if (delta_s < ANG180)
				delta_s /= 5;
			else
				delta_s = (angle_t)(0 - (angle_t)(0 - delta_s) / 5);
			
			if (delta_s > (ANG5/2) && delta_s < (angle_t)(0 - ANG5/2))
				delta_s = (delta_s < ANG180) ? (ANG5/2) : (angle_t)(0 - ANG5/2);

			player->mo->angle += delta;
			player->mo->vertangle += delta_s;

			if (player->damagecount && (leveltime % 3) == 0)
				player->damagecount--;
		}
	}
	else if (player->damagecount > 0)
		player->damagecount--;

	// -AJA- 1999/08/07: Fade out armor points too.
	if (player->bonuscount)
		player->bonuscount--;

	P_UpdatePowerups(player);

	// lose the zoom when dead
	player->zoom_fov = 0;

	if (deathmatch >= 3 && player->mo->movecount > player->mo->info->respawntime)
		return;

	if (player->cmd.buttons & BT_USE)
		player->playerstate = PST_REBORN;
}

static void P_UpdatePowerups(player_t *player)
{
	float limit = FLT_MAX;
	int pw;

	if (player->playerstate == PST_DEAD)
		limit = 1;  // TICRATE * 5;

	for (pw = 0; pw < NUMPOWERS; pw++)
	{
		if (player->powers[pw] < 0)  // -ACB- 2004/02/04 Negative values last a level
			continue;

		float& qty_r = player->powers[pw];

		if (qty_r > limit)
			qty_r = limit;
		else if (qty_r > 1)
			qty_r -= 1;
		else if (qty_r > 0)
		{
			if (player->keep_powers & (1 << pw))
				qty_r = -1;
			else
				qty_r = 0;
		}
	}

	if (player->powers[PW_PartInvis] >= 128 ||
		fmod(player->powers[PW_PartInvis], 16) >= 8)
		player->mo->flags |=  MF_FUZZY;
	else
		player->mo->flags &= ~MF_FUZZY;

	// Handling colourmaps.
	//
	// -AJA- 1999/07/10: Updated for colmap.ddf.
	//
	// !!! FIXME: overlap here with stuff in rgl_fx.cpp.

	player->effect_colourmap = NULL;
	player->effect_left = 0;

	if (player->powers[PW_Invulnerable] > 0)
	{
		float s = player->powers[PW_Invulnerable];

		// -ACB- FIXME!!! Catch lookup failure!
		player->effect_colourmap = colourmaps.Lookup("ALLWHITE");
		player->effect_left = (s <= 0) ? 0 : MIN(int(s), EFFECT_MAX_TIME);
	}
	else if (player->powers[PW_Infrared] > 0)
	{
		float s = player->powers[PW_Infrared];

		player->effect_left = (s <= 0) ? 0 : MIN(int(s), EFFECT_MAX_TIME);
	}
	else if (player->powers[PW_NightVision] > 0)	// -ACB- 1998/07/15 NightVision Code
	{
		float s = player->powers[PW_NightVision];

		// -ACB- FIXME!!! Catch lookup failure!
		player->effect_colourmap = colourmaps.Lookup("ALLGREEN");
		player->effect_left = (s <= 0) ? 0 : MIN(int(s), EFFECT_MAX_TIME);
	}
}


// Does the thinking of the console player, i.e. read from input
void P_ConsolePlayerBuilder(const player_t *pl, void *data, ticcmd_t *dest)
{
	E_BuildTiccmd(dest);

	dest->player_idx = pl->pnum;
}

static u16_t MakeConsistency(const player_t *pl)
{
	u16_t result = 0;

	if (pl->mo)
	{
		result = (int)(pl->mo->x - pl->mo->y + pl->mo->z);
	}

	return (result ^= (u16_t)P_ReadRandomState());
}


bool P_PlayerSwitchWeapon(player_t *player, weapondef_c *choice)
{
	int pw_index;

	// see if player owns this kind of weapon
	for (pw_index=0; pw_index < MAXWEAPONS; pw_index++)
	{
		if (! player->weapons[pw_index].owned)
			continue;

		if (player->weapons[pw_index].info == choice)
			break;
	}

	if (pw_index == MAXWEAPONS)
		return false;

	// ignore this choice if it the same as the current weapon
	if (player->ready_wp >= 0 && choice ==
		player->weapons[player->ready_wp].info)
	{
		return false;
	}

	player->pending_wp = (weapon_selection_e) pw_index;

	return true;
}


void P_PlayerThink(player_t * player)
{
	ticcmd_t *cmd;

	SYS_ASSERT(player->mo);

#if 0  // DEBUG ONLY
	{
		touch_node_t *tn;
		L_WriteDebug("Player %d Touch List:\n", player->pnum);
		for (tn = mo->touch_sectors; tn; tn=tn->mo_next)
		{
			L_WriteDebug("  SEC %d  Other = %s\n", tn->sec - sectors,
				tn->sec_next ? tn->sec_next->mo->info->name :
			tn->sec_prev ? tn->sec_prev->mo->info->name : "(None)");

			SYS_ASSERT(tn->mo == mo);
			if (tn->mo_next)
			{
				SYS_ASSERT(tn->mo_next->mo_prev == tn);
			}
		}
	}
#endif

	if (player->attacker && player->attacker->isRemoved())
		player->attacker = NULL;

	if (player->damagecount <= 0)
		player->damage_pain = 0;

	player->consistency[gametic % (MP_SAVETICS*2)] = MakeConsistency(player);

	// fixme: do this in the cheat code
	if (player->cheats & CF_NOCLIP)
		player->mo->flags |= MF_NOCLIP;
	else
		player->mo->flags &= ~MF_NOCLIP;

	// chain saw run forward
	cmd = &player->cmd;
	if (player->mo->flags & MF_JUSTATTACKED)
	{
		cmd->angleturn = 0;
		cmd->forwardmove = 64;
		cmd->sidemove = 0;
		player->mo->flags &= ~MF_JUSTATTACKED;
	}

	if (player->playerstate == PST_DEAD)
	{
		DeathThink(player);
		return;
	}

	// Move/Look around.  Reactiontime is used to prevent movement for a
	// bit after a teleport.

	if (player->mo->reactiontime)
		player->mo->reactiontime--;
	else
		MovePlayer(player);

	CalcHeight(player);

	if (player->mo->props->special ||
		player->mo->subsector->sector->exfloor_used > 0 ||
		player->underwater)
	{
		P_PlayerInSpecialSector(player, player->mo->subsector->sector);
	}

	// Check for weapon change.
	if (cmd->buttons & BT_CHANGE)
	{
		// The actual changing of the weapon is done when the weapon
		// psprite can do it (read: not in the middle of an attack).

		int key = (cmd->buttons & BT_WEAPONMASK) >> BT_WEAPONSHIFT;

		if (key == BT_NEXT_WEAPON)
		{
			P_NextPrevWeapon(player, +1);
		}
		else if (key == BT_PREV_WEAPON)
		{
			P_NextPrevWeapon(player, -1);
		}
		else /* numeric key */
		{
			P_DesireWeaponChange(player, key);
		}
	}

	// check for use
	if (cmd->buttons & BT_USE)
	{
		if (!player->usedown)
		{
			P_UseLines(player);
			player->usedown = true;
		}
	}
	else
	{
		player->usedown = false;
	}

	player->actiondown[0] = (cmd->extbuttons & EBT_ACTION1) ? true : false;
	player->actiondown[1] = (cmd->extbuttons & EBT_ACTION2) ? true : false;

	// decrement jumpwait counter
	if (player->jumpwait > 0)
		player->jumpwait--;

	if (player->splashwait > 0)
		player->splashwait--;

	// cycle psprites
	P_MovePsprites(player);

	// Counters, time dependend power ups.

	P_UpdatePowerups(player);

	if (player->damagecount > 0)
		player->damagecount--;

	if (player->bonuscount > 0)
		player->bonuscount--;

	if (player->grin_count > 0)
		player->grin_count--;

	if (player->attackdown[0] || player->attackdown[1])
		player->attackdown_count++;
	else
		player->attackdown_count = 0;

	player->kick_offset /= 1.6f;

}


void P_CreatePlayer(int pnum, bool is_bot)
{
	SYS_ASSERT(0 <= pnum && pnum < MAXPLAYERS);

	SYS_ASSERT_MSG(! players[pnum], ("P_CreatePlayer: %d already there", pnum));

	player_t *p = new player_t;

	Z_Clear(p, player_t, 1);

	p->pnum = pnum;
	p->playerstate = PST_DEAD;

	players[pnum] = p;

	numplayers++;
	if (is_bot)
		numbots++;

	// determine name
	char namebuf[32];
	sprintf(namebuf, "Player%dName", pnum + 1);

	if (language.IsValidRef(namebuf))
	{
		strncpy(p->playername, language[namebuf], MAX_PLAYNAME-1);
		p->playername[MAX_PLAYNAME-1] = '\0';
	}
	else
	{
		// -ES- Default to player##
		sprintf(p->playername, "Player%d", pnum + 1);
	}

	if (is_bot)
		P_BotCreate(p, false);

	memset(p->consistency, -1, sizeof(p->consistency));

	if (! sfx_jpidle)
	{
		sfx_jpidle = sfxdefs.GetEffect("JPIDLE");
		sfx_jpmove = sfxdefs.GetEffect("JPMOVE");
		sfx_jprise = sfxdefs.GetEffect("JPRISE");
		sfx_jpdown = sfxdefs.GetEffect("JPDOWN");
		sfx_jpflow = sfxdefs.GetEffect("JPFLOW");
	}
}


void P_DestroyAllPlayers(void)
{
	for (int pnum=0; pnum < MAXPLAYERS; pnum++)
	{
		if (! players[pnum])
			continue;

		delete players[pnum];

		players[pnum] = NULL;
	}

	numplayers = 0;
	numbots = 0;

	consoleplayer = -1;
	displayplayer = -1;

	sfx_jpidle = sfx_jpmove = sfx_jprise = NULL;
	sfx_jpdown = sfx_jpflow = NULL;
}


void P_UpdateAvailWeapons(player_t *p)
{
	// Must be called as soon as the player has received or lost
	// a weapon.  Updates the status bar icons.

	int key;

	for (key = 0; key < WEAPON_KEYS; key++)
		p->avail_weapons[key] = false;

	for (int i = 0; i < MAXWEAPONS; i++)
	{
		if (! p->weapons[i].owned)
			continue;

		SYS_ASSERT(p->weapons[i].info);

		key = p->weapons[i].info->bind_key;

		// update the status bar icons
		if (0 <= key && key <= 9)
			p->avail_weapons[key] = true;
	}
}


void P_UpdateTotalArmour(player_t *p)
{
	int i;

	p->totalarmour = 0;

	for (i=0; i < NUMARMOUR; i++)
	{
		p->totalarmour += p->armours[i];

		// forget the association once fully depleted
		if (p->armours[i] <= 0)
			p->armour_types[i] = NULL;
	}

	if (p->totalarmour > 999.0f)
		p->totalarmour = 999.0f;
}


bool P_AddWeapon(player_t *player, weapondef_c *info, int *index)
{
	// Returns true if player did not already have the weapon.
	// If successful and 'index' is non-NULL, the new index is
	// stored there.

	int slot = -1;
	int upgrade_slot = -1;

	// cannot own weapons if sprites are missing
	if (! P_CheckWeaponSprite(info))
		return false;

	for (int i=0; i < MAXWEAPONS; i++)
	{
		weapondef_c *cur_info = player->weapons[i].info;

		// skip weapons that are being removed
		if (player->weapons[i].flags & PLWEP_Removing)
			continue;

		// find free slot
		if (! player->weapons[i].owned)
		{
			if (slot < 0)
				slot = i;
			continue;
		}

		// check if already own this weapon
		if (cur_info == info)
			return false;

		// don't downgrade any UPGRADED weapons
		if (DDF_WeaponIsUpgrade(cur_info, info))
			return false;

		// check for weapon upgrades
		if (cur_info == info->upgrade_weap)
		{
			upgrade_slot = i;
			continue;
		}
	}

	if (slot < 0)
		return false;

	if (index)
		(*index) = slot;

	L_WriteDebug("P_AddWeapon: [%s] @ %d\n", info->name.c_str(), slot);

	player->weapons[slot].owned = true;
	player->weapons[slot].info  = info;
	player->weapons[slot].flags = 0;
	player->weapons[slot].clip_size[0] = 0;
	player->weapons[slot].clip_size[1] = 0;
	player->weapons[slot].model_skin = info->model_skin;

	P_UpdateAvailWeapons(player);

	// for NoAmmo+Clip weapons, always begin with a full clip
	for (int ATK = 0; ATK < 2; ATK++)
	{
		if (info->clip_size[ATK] > 0 && info->ammo[ATK] == AM_NoAmmo)
			player->weapons[slot].clip_size[ATK] = info->clip_size[ATK];
	}

	// initial weapons should get a full clip
	if (info->autogive)
		P_TryFillNewWeapon(player, slot, AM_DontCare, NULL);

	if (upgrade_slot >= 0)
	{
		player->weapons[upgrade_slot].owned = false;

		// check and update key_choices[]
		for (int w=0; w <= 9; w++)
			if (player->key_choices[w] == upgrade_slot)
				player->key_choices[w] = (weapon_selection_e)slot;

		// handle the case of holding the weapon which is being upgraded
		// by the new one.  We mark the old weapon for removal.

		if (player->ready_wp == upgrade_slot)
		{
			player->weapons[upgrade_slot].flags |= PLWEP_Removing;
			player->pending_wp = (weapon_selection_e) slot;
		}
		else
			player->weapons[upgrade_slot].info = NULL;

		if (player->pending_wp == upgrade_slot)
			player->pending_wp = (weapon_selection_e) slot;
	}

	return true;
}


bool P_RemoveWeapon(player_t *player, weapondef_c *info)
{
	// returns true if player had the weapon.

	int slot;

	for (slot=0; slot < MAXWEAPONS; slot++)
	{
		if (! player->weapons[slot].owned)
			continue;

		// Note: no need to check PLWEP_Removing

		if (player->weapons[slot].info == info)
			break;
	}

	if (slot >= MAXWEAPONS)
		return false;

	L_WriteDebug("P_RemoveWeapon: [%s] @ %d\n", info->name.c_str(), slot);

	player->weapons[slot].owned = false;

	P_UpdateAvailWeapons(player);

	// fix the key choices
	for (int w=0; w <= 9; w++)
		if (player->key_choices[w] == slot)
			player->key_choices[w] = WPSEL_None;

	// handle the case of already holding the weapon.  We mark the
	// weapon as being removed (the flag is cleared once lowered).

	if (player->ready_wp == slot)
	{
		player->weapons[slot].flags |= PLWEP_Removing;

		if (player->pending_wp == WPSEL_NoChange)
			P_DropWeapon(player);
	}
	else
		player->weapons[slot].info = NULL;

	if (player->pending_wp == slot)
		P_SelectNewWeapon(player, -100, AM_DontCare);

	SYS_ASSERT(player->pending_wp != slot);

	return true;
}


void P_GiveInitialBenefits(player_t *p, const mobjtype_c *info)
{
	// Give the player the initial benefits when they start a game
	// (or restart after dying).  Sets up: ammo, ammo-limits, health,
	// armour, keys and weapons.

	epi::array_iterator_c it;
	weapondef_c *w;
	
	p->ready_wp   = WPSEL_None;
	p->pending_wp = WPSEL_NoChange;

	int i;

	for (i=0; i < WEAPON_KEYS; i++)
		p->key_choices[i] = WPSEL_None;

	// clear out ammo & ammo-limits
	for (i=0; i < NUMAMMO; i++)
	{
		p->ammo[i].num = p->ammo[i].max = 0;
	}

	// set health and armour
	p->health = info->spawnhealth;
	p->air_in_lungs = info->lung_capacity;
	p->underwater = false;

	for (i = 0; i < NUMARMOUR; i++)
	{
		p->armours[i] = 0;
		p->armour_types[i] = NULL;
	}

	p->totalarmour = 0;
	p->cards = KF_NONE;

	// give all initial benefits
	P_GiveBenefitList(p, NULL, info->initial_benefits, false);

	// give all free weapons.  Needs to be after ammo, so that
	// clip weapons can get their clips filled.
	for (it=weapondefs.GetIterator(0); it.IsValid(); it++)
	{
		w = ITERATOR_TO_TYPE(it, weapondef_c*);
		if (!w->autogive)
			continue;

		int pw_index;

		P_AddWeapon(p, w, &pw_index);
	}

	// refresh to remove all stuff from status bar
	P_UpdateAvailWeapons(p);
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
