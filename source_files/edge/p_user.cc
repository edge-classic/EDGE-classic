//----------------------------------------------------------------------------
//  EDGE Player User Code
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2024 The EDGE Team.
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

#include "i_defs.h"

#include <float.h>

#include "colormap.h"

#include "dm_state.h"
#include "e_input.h"
#include "g_game.h"
#include "m_random.h"
#include "n_network.h"
#include "bot_think.h"
#include "p_local.h"
#include "rad_trig.h"
#include "s_blit.h"
#include "s_sound.h"

// Room size test - Dasho
#include "p_blockmap.h"

#include "AlmostEquals.h"

#include "coal.h" // for coal::vm_c
#include "vm_coal.h"
#include "script/compat/lua_compat.h"

extern coal::vm_c *ui_vm;
extern void        VM_SetVector(coal::vm_c *vm, const char *mod_name, const char *var_name, double val_1, double val_2,
                                double val_3);

extern cvar_c r_doubleframes;

DEF_CVAR(g_erraticism, "0", CVAR_ARCHIVE)

DEF_CVAR(g_bobbing, "0", CVAR_ARCHIVE)

float room_area;

struct room_measure
{
    float x = 0;
    float y = 0;
};

// Test for "measuring" size of room
static bool P_RoomPath(intercept_t *in, void *dataptr)
{
    room_measure *blocker = (room_measure *)dataptr;

    if (in->line)
    {
        line_t *ld = in->line;

        if (ld->backsector && ld->frontsector)
        {
            if ((IS_SKY(ld->backsector->ceil) && !IS_SKY(ld->frontsector->ceil)) ||
                (!IS_SKY(ld->backsector->ceil) && IS_SKY(ld->frontsector->ceil)))
            {
                blocker->x = (ld->v1->X + ld->v2->X) / 2;
                blocker->y = (ld->v1->Y + ld->v2->Y) / 2;
                return false;
            }
        }

        if (ld->blocked)
        {
            blocker->x = (ld->v1->X + ld->v2->X) / 2;
            blocker->y = (ld->v1->Y + ld->v2->Y) / 2;
            return false;
        }
    }
    return true;
}

static void P_UpdatePowerups(player_t *player);

#define MAXBOB 16.0f

#define ZOOM_ANGLE_DIV 4

static sfx_t *sfx_jpidle;
static sfx_t *sfx_jpmove;
static sfx_t *sfx_jprise;
static sfx_t *sfx_jpdown;
static sfx_t *sfx_jpflow;

static void CalcHeight(player_t *player, bool extra_tic)
{
    bool      onground  = player->mo->z <= player->mo->floorz;
    float     sink_mult = 1.0f;
    sector_t *cur_sec   = player->mo->subsector->sector;
    if (!cur_sec->exfloor_used && !cur_sec->heightsec && onground)
        sink_mult -= cur_sec->sink_depth;

    if (g_erraticism.d && leveltime > 0 && (!player->cmd.forwardmove && !player->cmd.sidemove) &&
        ((AlmostEquals(player->mo->height, player->mo->info->height) ||
          AlmostEquals(player->mo->height, player->mo->info->crouchheight)) &&
         (AlmostEquals(player->deltaviewheight, 0.0f) || sink_mult < 1.0f)))
        return;

    if (player->mo->height < (player->mo->info->height + player->mo->info->crouchheight) / 2.0f)
        player->mo->extendedflags |= EF_CROUCHING;
    else
        player->mo->extendedflags &= ~EF_CROUCHING;

    player->std_viewheight = player->mo->height * player->mo->info->viewheight;

    if (sink_mult < 1.0f)
        player->deltaviewheight = HMM_MAX(player->deltaviewheight - 1.0f, -1.0f);

    // calculate the walking / running height adjustment.

    float bob_z = 0;

    // Regular movement bobbing
    // (needs to be calculated for gun swing even if not on ground).
    // -AJA- Moved up here, to prevent weapon jumps when running down
    // stairs.

    if (g_erraticism.d)
        player->bob = 12.0f;
    else
        player->bob = (player->mo->mom.X * player->mo->mom.X + player->mo->mom.Y * player->mo->mom.Y) / 8;

    if (player->bob > MAXBOB)
        player->bob = MAXBOB;

    // ----CALCULATE BOB EFFECT----
    if (player->playerstate == PST_LIVE && onground)
    {
        BAMAngle angle = kBAMAngle90 / 5 * leveltime;

        bob_z = player->bob / 2 * player->mo->info->bobbing * epi::BAMSin(angle);
    }

    // ----CALCULATE VIEWHEIGHT----
    if (player->playerstate == PST_LIVE)
    {
        player->viewheight += player->deltaviewheight * (r_doubleframes.d ? 0.5 : 1);

        if (player->viewheight > player->std_viewheight)
        {
            player->viewheight      = player->std_viewheight;
            player->deltaviewheight = 0;
        }
        else if (sink_mult < 1.0f && !(player->mo->extendedflags & EF_CROUCHING) &&
                 player->viewheight < player->std_viewheight * sink_mult)
        {
            player->viewheight = player->std_viewheight * sink_mult;
            if (player->deltaviewheight <= 0)
                player->deltaviewheight = 0.01f;
        }
        else
        {
            float thresh = player->std_viewheight / 2;
            if (sink_mult < 1.0f)
                thresh = HMM_MIN(thresh, player->std_viewheight * sink_mult);
            if (player->viewheight < thresh)
            {
                player->viewheight = thresh;
                if (player->deltaviewheight <= 0)
                    player->deltaviewheight = 0.01f;
            }
        }

        if (!AlmostEquals(player->deltaviewheight, 0.0f))
        {
            // use a weird number to minimise chance of hitting
            // zero when deltaviewheight goes neg -> positive.
            if (!extra_tic || !r_doubleframes.d)
                player->deltaviewheight += 0.24162f;
        }
    }

    //----CALCULATE FREEFALL EFFECT, WITH SOUND EFFECTS (code based on HEXEN)
    //  CORBIN, on:
    //  6/6/2011 - Fix this so RTS does NOT interfere with fracunits (it does in Hypertension's E1M1 starting script)!
    //  6/7/2011 - Ajaped said to remove FRACUNIT...seeya oldness.

    // if ((player->mo->mom.z <= -35.0)&&(player->mo->mom.z >= -40.0))
    if ((player->mo->mom.Z <= -35.0) && (player->mo->mom.Z >= -36.0))
        if (player->mo->info->falling_sound)
        {
            int sfx_cat;

            if (player == players[consoleplayer])
            {
                sfx_cat = SNCAT_Player;
            }
            else
            {
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

    if (r_doubleframes.d)
        bob_z *= 0.5;

    if (g_bobbing.d > 1)
        bob_z = 0;

    player->viewz = player->viewheight + bob_z;

#if 0 // DEBUG
I_Debugf("Jump:%d bob_z:%1.2f  z:%1.2f  height:%1.2f delta:%1.2f --> viewz:%1.3f\n",
		 player->jumpwait, bob_z, player->mo->z,
		 player->viewheight, player->deltaviewheight,
		 player->mo->z + player->viewz);
#endif
}

void P_PlayerJump(player_t *pl, float dz, int wait)
{
    pl->mo->mom.Z += dz;

    if (pl->jumpwait < wait)
        pl->jumpwait = wait;

    // enter the JUMP states (if present)
    int jump_st = P_MobjFindLabel(pl->mo, "JUMP");
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

static void MovePlayer(player_t *player, bool extra_tic)
{
    ticcmd_t *cmd;
    mobj_t   *mo = player->mo;

    bool onground = player->mo->z <= player->mo->floorz;
    bool onladder = player->mo->on_ladder >= 0;

    bool swimming  = player->swimming;
    bool flying    = (player->powers[PW_Jetpack] > 0) && !swimming;
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

    player->mo->angle -= (BAMAngle)(cmd->angleturn << 16);

    // EDGE Feature: Vertical Look (Mlook)
    //
    // -ACB- 1998/07/02 New Code used, rerouted via Ticcmd
    // -ACB- 1998/07/27 Used defines for look limits.
    //
    if (level_flags.mlook)
    {
        if (player->zoom_fov > 0)
            cmd->mlookturn /= ZOOM_ANGLE_DIV;

        BAMAngle V = player->mo->vertangle + (BAMAngle)(cmd->mlookturn << 16);

        if (V < kBAMAngle180 && V > MLOOK_LIMIT)
            V = MLOOK_LIMIT;
        else if (V >= kBAMAngle180 && V < (kBAMAngle360 - MLOOK_LIMIT))
            V = (kBAMAngle360 - MLOOK_LIMIT);

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

    base_xy_speed = player->mo->speed / (r_doubleframes.d ? 64.0f : 32.0f);
    base_z_speed  = player->mo->speed / (r_doubleframes.d ? 57.0f : 64.0f);

    // Do not let the player control movement if not onground.
    // -MH- 1998/06/18  unless he has the JetPack!

    if (!(onground || onladder || swimming || flying))
        base_xy_speed /= 16.0f;

    if (!(onladder || swimming || flying))
        base_z_speed /= 16.0f;

    // move slower when crouching
    if (crouching)
        base_xy_speed *= CROUCH_SLOWDOWN;

    dx = epi::BAMCos(player->mo->angle);
    dy = epi::BAMSin(player->mo->angle);

    eh = 1;
    ev = 0;

    if (swimming || flying || onladder)
    {
        float slope = epi::BAMTan(player->mo->vertangle);

        float hyp = (float)sqrt((double)(1.0f + slope * slope));

        eh = 1.0f / hyp;
        ev = slope / hyp;
    }

    // compute movement vectors

    F_vec[0] = eh * dx * base_xy_speed;
    F_vec[1] = eh * dy * base_xy_speed;
    F_vec[2] = ev * base_z_speed;

    S_vec[0] = dy * base_xy_speed;
    S_vec[1] = -dx * base_xy_speed;
    S_vec[2] = 0;

    U_vec[0] = -ev * dx * base_xy_speed;
    U_vec[1] = -ev * dy * base_xy_speed;
    U_vec[2] = eh * base_z_speed;

    player->mo->mom.X += F_vec[0] * cmd->forwardmove + S_vec[0] * cmd->sidemove + U_vec[0] * cmd->upwardmove;

    player->mo->mom.Y += F_vec[1] * cmd->forwardmove + S_vec[1] * cmd->sidemove + U_vec[1] * cmd->upwardmove;

    if (flying || swimming || !onground || onladder)
    {
        player->mo->mom.Z += F_vec[2] * cmd->forwardmove + S_vec[2] * cmd->sidemove + U_vec[2] * cmd->upwardmove;
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
                S_StartFX(sfx_jpflow, sfx_cat, player->mo); // fuel low
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
        if (!jumping && !flying && (onground || swimming) && (cmd->forwardmove || cmd->sidemove))
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

    if (!extra_tic || !r_doubleframes.d)
    {
        if (level_flags.jump && mo->info->jumpheight > 0 && (cmd->upwardmove > 4))
        {
            if (!jumping && !crouching && !swimming && !flying && onground && !onladder)
            {
                P_PlayerJump(player, player->mo->info->jumpheight / (r_doubleframes.d ? 1.25f : 1.4f),
                             player->mo->info->jump_delay);
            }
        }
    }

    // EDGE Feature: Crouching

    if (level_flags.crouch && mo->info->crouchheight > 0 && (player->cmd.upwardmove < -4) && !player->wet_feet &&
        !jumping && onground)
    // NB: no ladder check, onground is sufficient
    {
        if (mo->height > mo->info->crouchheight)
        {
            mo->height = HMM_MAX(mo->height - 2.0f / (r_doubleframes.d ? 2.0 : 1.0), mo->info->crouchheight);
            mo->player->deltaviewheight = -1.0f;
        }
    }
    else // STAND UP
    {
        if (mo->height < mo->info->height)
        {
            float new_height = HMM_MIN(mo->height + 2 / (r_doubleframes.d ? 2 : 1), mo->info->height);

            // prevent standing up inside a solid area
            if ((mo->flags & MF_NOCLIP) || mo->z + new_height <= mo->ceilingz)
            {
                mo->height                  = new_height;
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
            if (!(player->ready_wp < 0 || player->pending_wp >= 0))
                fov = player->weapons[player->ready_wp].info->zoom_fov_;

            if (fov == int(kBAMAngle360))
                fov = 0;
        }

        player->zoom_fov = fov;
    }
}

static void DeathThink(player_t *player, bool extra_tic)
{
    int subtract = extra_tic ? 0 : 1;

    if (!r_doubleframes.d)
        subtract = 1;

    // fall on your face when dying.

    float dx, dy, dz;

    BAMAngle angle;
    BAMAngle delta, delta_s;
    float   slope;

    // -AJA- 1999/12/07: don't die mid-air.
    player->powers[PW_Jetpack] = 0;

    if (!extra_tic)
        P_MovePsprites(player);

    // fall to the ground
    if (player->viewheight > player->std_viewheight)
        player->viewheight -= 1.0f / (r_doubleframes.d ? 2.0 : 1.0);
    else if (player->viewheight < player->std_viewheight)
        player->viewheight = player->std_viewheight;

    player->deltaviewheight = 0.0f;
    player->kick_offset     = 0.0f;

    CalcHeight(player, extra_tic);

    if (player->attacker && player->attacker != player->mo)
    {
        dx = player->attacker->x - player->mo->x;
        dy = player->attacker->y - player->mo->y;
        dz = (player->attacker->z + player->attacker->height / 2) - (player->mo->z + player->viewheight);

        angle = R_PointToAngle(0, 0, dx, dy);
        delta = angle - player->mo->angle;

        slope   = P_ApproxSlope(dx, dy, dz);
        slope   = HMM_MIN(1.7f, HMM_MAX(-1.7f, slope));
        delta_s = epi::BAMFromATan(slope) - player->mo->vertangle;

        if ((delta <= kBAMAngle1 / 2 || delta >= (BAMAngle)(0 - kBAMAngle1 / 2)) &&
            (delta_s <= kBAMAngle1 / 2 || delta_s >= (BAMAngle)(0 - kBAMAngle1 / 2)))
        {
            // Looking at killer, so fade damage flash down.
            player->mo->angle     = angle;
            player->mo->vertangle = epi::BAMFromATan(slope);

            if (player->damagecount > 0)
                player->damagecount -= subtract;
        }
        else
        {
            unsigned int factor = r_doubleframes.d ? 2 : 1;
            if (delta < kBAMAngle180)
                delta /= (5 * factor);
            else
                delta = (BAMAngle)(0 - (BAMAngle)(0 - delta) / (5 * factor));

            if (delta > kBAMAngle5 / factor && delta < (BAMAngle)(0 - kBAMAngle5 / factor))
                delta = (delta < kBAMAngle180) ? kBAMAngle5 / factor : (BAMAngle)(0 - kBAMAngle5 / factor);

            if (delta_s < kBAMAngle180)
                delta_s /= (5 * factor);
            else
                delta_s = (BAMAngle)(0 - (BAMAngle)(0 - delta_s) / (5 * factor));

            if (delta_s > (kBAMAngle5 / (factor * 2)) && delta_s < (BAMAngle)(0 - kBAMAngle5 / (factor * 2)))
                delta_s = (delta_s < kBAMAngle180) ? (kBAMAngle5 / (factor * 2)) : (BAMAngle)(0 - kBAMAngle5 / (factor * 2));

            player->mo->angle += delta;
            player->mo->vertangle += delta_s;

            if (player->damagecount && (leveltime % 3) == 0)
                player->damagecount -= subtract;
        }
    }
    else if (player->damagecount > 0)
        player->damagecount--;

    // -AJA- 1999/08/07: Fade out armor points too.
    if (player->bonuscount)
        player->bonuscount -= subtract;

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
    int   pw;

    if (player->playerstate == PST_DEAD)
        limit = 1; // TICRATE * 5;

    for (pw = 0; pw < NUMPOWERS; pw++)
    {
        if (player->powers[pw] < 0) // -ACB- 2004/02/04 Negative values last a level
            continue;

        float &qty_r = player->powers[pw];

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

    if (player->powers[PW_PartInvis] >= 128 || fmod(player->powers[PW_PartInvis], 16) >= 8)
        player->mo->flags |= MF_FUZZY;
    else
        player->mo->flags &= ~MF_FUZZY;

    // Handling colormaps.
    //
    // -AJA- 1999/07/10: Updated for colmap.ddf.
    //
    // !!! FIXME: overlap here with stuff in rgl_fx.cpp.

    player->effect_colourmap = nullptr;
    player->effect_left      = 0;

    if (player->powers[PW_Invulnerable] > 0)
    {
        float s = player->powers[PW_Invulnerable];

        // -ACB- FIXME!!! Catch lookup failure!
        player->effect_colourmap = colormaps.Lookup("ALLWHITE");
        player->effect_left      = (s <= 0) ? 0 : HMM_MIN(int(s), EFFECT_MAX_TIME);
    }
    else if (player->powers[PW_Infrared] > 0)
    {
        float s = player->powers[PW_Infrared];

        player->effect_left = (s <= 0) ? 0 : HMM_MIN(int(s), EFFECT_MAX_TIME);
    }
    else if (player->powers[PW_NightVision] > 0) // -ACB- 1998/07/15 NightVision Code
    {
        float s = player->powers[PW_NightVision];

        // -ACB- FIXME!!! Catch lookup failure!
        player->effect_colourmap = colormaps.Lookup("ALLGREEN");
        player->effect_left      = (s <= 0) ? 0 : HMM_MIN(int(s), EFFECT_MAX_TIME);
    }
    else if (player->powers[PW_Berserk] > 0) // Lobo 2021: Un-Hardcode Berserk colour tint
    {
        float s = player->powers[PW_Berserk];

        player->effect_colourmap = colormaps.Lookup("BERSERK");
        player->effect_left      = (s <= 0) ? 0 : HMM_MIN(int(s), EFFECT_MAX_TIME);
    }
}

// Does the thinking of the console player, i.e. read from input
void P_ConsolePlayerBuilder(const player_t *pl, void *data, ticcmd_t *dest)
{
    E_BuildTiccmd(dest);

    dest->player_idx = pl->pnum;
}

bool P_PlayerSwitchWeapon(player_t *player, WeaponDefinition *choice)
{
    int pw_index;

    // see if player owns this kind of weapon
    for (pw_index = 0; pw_index < MAXWEAPONS; pw_index++)
    {
        if (!player->weapons[pw_index].owned)
            continue;

        if (player->weapons[pw_index].info == choice)
            break;
    }

    if (pw_index == MAXWEAPONS)
        return false;

    // ignore this choice if it the same as the current weapon
    if (player->ready_wp >= 0 && choice == player->weapons[player->ready_wp].info)
    {
        return false;
    }

    player->pending_wp = (weapon_selection_e)pw_index;

    return true;
}

void P_DumpMobjsTemp(void)
{
    mobj_t *mo;

    int index = 0;

    I_Warning("MOBJs:\n");

    for (mo = mobjlisthead; mo; mo = mo->next, index++)
    {
        I_Warning(" %4d: %p next:%p prev:%p [%s] at (%1.0f,%1.0f,%1.0f) states=%d > %d tics=%d\n", index, mo, mo->next,
                  mo->prev, mo->info->name.c_str(), mo->x, mo->y, mo->z, (int)(mo->state ? mo->state - states : -1),
                  (int)(mo->next_state ? mo->next_state - states : -1), mo->tics);
    }

    I_Warning("END OF MOBJs\n");
}

bool P_PlayerThink(player_t *player, bool extra_tic)
{
    ticcmd_t *cmd = &player->cmd;

    SYS_ASSERT(player->mo);

    bool should_think = true;

#if 0 // DEBUG ONLY
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
    {
        P_DumpMobjsTemp();
        I_Error("INTERNAL ERROR: player has a removed attacker. \n");
    }

    if (player->damagecount <= 0)
        player->damage_pain = 0;

    // fixme: do this in the cheat code
    if (player->cheats & CF_NOCLIP)
        player->mo->flags |= MF_NOCLIP;
    else
        player->mo->flags &= ~MF_NOCLIP;

    // chain saw run forward
    if (extra_tic || !r_doubleframes.d)
    {
        if (player->mo->flags & MF_JUSTATTACKED)
        {
            cmd->angleturn   = 0;
            cmd->forwardmove = 64;
            cmd->sidemove    = 0;
            player->mo->flags &= ~MF_JUSTATTACKED;
        }
    }

    if (player->playerstate == PST_DEAD)
    {
        DeathThink(player, extra_tic);
        if (player->mo->props->special && player->mo->props->special->e_exit_ != kExitTypeNone)
        {
            ExitType do_exit = player->mo->props->special->e_exit_;

            player->mo->subsector->sector->props.special = nullptr;

            if (do_exit == kExitTypeSecret)
                G_SecretExitLevel(1);
            else
                G_ExitLevel(1);
        }
        return true;
    }

    int subtract = extra_tic ? 0 : 1;
    if (!r_doubleframes.d)
        subtract = 1;

    // Move/Look around.  Reactiontime is used to prevent movement for a
    // bit after a teleport.

    if (player->mo->reactiontime)
        player->mo->reactiontime -= subtract;

    if (player->mo->reactiontime == 0)
        MovePlayer(player, extra_tic);

    CalcHeight(player, extra_tic);

    if (g_erraticism.d)
    {
        bool      sinking = false;
        sector_t *cur_sec = player->mo->subsector->sector;
        if (!cur_sec->exfloor_used && !cur_sec->heightsec && cur_sec->sink_depth > 0 &&
            player->mo->z <= player->mo->floorz)
            sinking = true;
        if (cmd->forwardmove == 0 && cmd->sidemove == 0 && !player->swimming && cmd->upwardmove <= 0 &&
            !(cmd->buttons & (BT_ATTACK | BT_USE | BT_CHANGE | EBT_SECONDATK | EBT_RELOAD | EBT_ACTION1 | EBT_ACTION2 |
                              EBT_INVUSE | EBT_THIRDATK | EBT_FOURTHATK)) &&
            ((AlmostEquals(player->mo->height, player->mo->info->height) ||
              AlmostEquals(player->mo->height, player->mo->info->crouchheight)) &&
             (AlmostEquals(player->deltaviewheight, 0.0f) || sinking)))
        {
            should_think = false;
            if (!player->mo->mom.Z)
            {
                player->mo->mom.X = 0;
                player->mo->mom.Y = 0;
            }
        }
    }

    // Reset environmental FX in case player has left sector in which they apply - Dasho
    vacuum_sfx       = false;
    submerged_sfx    = false;
    outdoor_reverb   = false;
    ddf_reverb       = false;
    ddf_reverb_type  = 0;
    ddf_reverb_delay = 0;
    ddf_reverb_ratio = 0;

    if (player->mo->props->special || player->mo->subsector->sector->exfloor_used > 0 || player->underwater ||
        player->swimming || player->airless)
    {
        P_PlayerInSpecialSector(player, player->mo->subsector->sector, should_think);
    }

    if (IS_SKY(player->mo->subsector->sector->ceil))
        outdoor_reverb = true;

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

    if (LUA_UseLuaHud())
        LUA_SetVector3(LUA_GetGlobalVM(), "player", "inventory_event_handler",
                       HMM_Vec3{{cmd->extbuttons & EBT_INVPREV ? 1.0f : 0.0f, cmd->extbuttons & EBT_INVUSE ? 1.0f : 0.0f,
                                   cmd->extbuttons & EBT_INVNEXT ? 1.0f : 0.0f}});
    else
        VM_SetVector(ui_vm, "player", "inventory_event_handler", cmd->extbuttons & EBT_INVPREV ? 1 : 0,
                     cmd->extbuttons & EBT_INVUSE ? 1 : 0, cmd->extbuttons & EBT_INVNEXT ? 1 : 0);

    // FIXME separate code more cleanly
    if (extra_tic && r_doubleframes.d)
        return should_think;

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

    if (players[consoleplayer] == player && dynamic_reverb)
    {
        // Approximate "room size" determination for reverb system - Dasho
        room_measure room_checker;
        float        line_lengths = 0;
        float        player_x     = player->mo->x;
        float        player_y     = player->mo->y;
        P_PathTraverse(player_x, player_y, player_x, 32768.0f, PT_ADDLINES, P_RoomPath, &room_checker);
        line_lengths += abs(room_checker.y - player_y);
        P_PathTraverse(player_x, player_y, 32768.0f + player_x, 32768.0f + player_y, PT_ADDLINES, P_RoomPath,
                       &room_checker);
        line_lengths += R_PointToDist(player_x, player_y, room_checker.x, room_checker.y);
        P_PathTraverse(player_x, player_y, -32768.0f + player_x, 32768.0f + player_y, PT_ADDLINES, P_RoomPath,
                       &room_checker);
        line_lengths += R_PointToDist(player_x, player_y, room_checker.x, room_checker.y);
        P_PathTraverse(player_x, player_y, player_x, -32768.0f, PT_ADDLINES, P_RoomPath, &room_checker);
        line_lengths += abs(player_y - room_checker.y);
        P_PathTraverse(player_x, player_y, -32768.0f + player_x, -32768.0f + player_y, PT_ADDLINES, P_RoomPath,
                       &room_checker);
        line_lengths += R_PointToDist(player_x, player_y, room_checker.x, room_checker.y);
        P_PathTraverse(player_x, player_y, 32768.0f + player_x, -32768.0f + player_y, PT_ADDLINES, P_RoomPath,
                       &room_checker);
        line_lengths += R_PointToDist(player_x, player_y, room_checker.x, room_checker.y);
        P_PathTraverse(player_x, player_y, -32768.0f, player_y, PT_ADDLINES, P_RoomPath, &room_checker);
        line_lengths += abs(player_x - room_checker.x);
        P_PathTraverse(player_x, player_y, 32768.0f, player_y, PT_ADDLINES, P_RoomPath, &room_checker);
        line_lengths += abs(room_checker.x - player_x);
        room_area = line_lengths / 8;
    }

    return should_think;
}

void P_CreatePlayer(int pnum, bool is_bot)
{
    SYS_ASSERT(0 <= pnum && pnum < MAXPLAYERS);
    SYS_ASSERT(players[pnum] == nullptr);

    player_t *p = new player_t;

    Z_Clear(p, player_t, 1);

    p->pnum        = pnum;
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
        strncpy(p->playername, language[namebuf], MAX_PLAYNAME - 1);
        p->playername[MAX_PLAYNAME - 1] = '\0';
    }
    else
    {
        // -ES- Default to player##
        sprintf(p->playername, "Player%d", pnum + 1);
    }

    if (is_bot)
        P_BotCreate(p, false);

    if (!sfx_jpidle)
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
    for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
    {
        if (!players[pnum])
            continue;

        delete players[pnum];

        players[pnum] = nullptr;
    }

    numplayers = 0;
    numbots    = 0;

    consoleplayer = -1;
    displayplayer = -1;

    sfx_jpidle = sfx_jpmove = sfx_jprise = nullptr;
    sfx_jpdown = sfx_jpflow = nullptr;
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
        if (!p->weapons[i].owned)
            continue;

        SYS_ASSERT(p->weapons[i].info);

        key = p->weapons[i].info->bind_key_;

        // update the status bar icons
        if (0 <= key && key <= 9)
            p->avail_weapons[key] = true;
    }
}

void P_UpdateTotalArmour(player_t *p)
{
    int i;

    p->totalarmour = 0;

    for (i = 0; i < NUMARMOUR; i++)
    {
        p->totalarmour += p->armours[i];

        // forget the association once fully depleted
        if (p->armours[i] <= 0)
            p->armour_types[i] = nullptr;
    }

    if (p->totalarmour > 999.0f)
        p->totalarmour = 999.0f;
}

bool P_AddWeapon(player_t *player, WeaponDefinition *info, int *index)
{
    // Returns true if player did not already have the weapon.
    // If successful and 'index' is non-nullptr, the new index is
    // stored there.

    int slot         = -1;
    int upgrade_slot = -1;

    // cannot own weapons if sprites are missing
    if (!P_CheckWeaponSprite(info))
    {
        I_Warning("WEAPON %s has no sprites and will not be added!\n", info->name_.c_str());
        return false;
    }

    for (int i = 0; i < MAXWEAPONS; i++)
    {
        WeaponDefinition *cur_info = player->weapons[i].info;

        // skip weapons that are being removed
        if (player->weapons[i].flags & PLWEP_Removing)
            continue;

        // find free slot
        if (!player->weapons[i].owned)
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
        if (cur_info == info->upgrade_weap_)
        {
            upgrade_slot = i;
            continue;
        }
    }

    if (slot < 0)
        return false;

    if (index)
        (*index) = slot;

    L_WriteDebug("P_AddWeapon: [%s] @ %d\n", info->name_.c_str(), slot);

    player->weapons[slot].owned        = true;
    player->weapons[slot].info         = info;
    player->weapons[slot].flags        = 0;
    player->weapons[slot].clip_size[0] = 0;
    player->weapons[slot].clip_size[1] = 0;
    player->weapons[slot].model_skin   = info->model_skin_;

    P_UpdateAvailWeapons(player);

    // for NoAmmo+Clip weapons, always begin with a full clip
    for (int ATK = 0; ATK < 2; ATK++)
    {
        if (info->clip_size_[ATK] > 0 && info->ammo_[ATK] == kAmmunitionTypeNoAmmo)
            player->weapons[slot].clip_size[ATK] = info->clip_size_[ATK];
    }

    // initial weapons should get a full clip
    if (info->autogive_)
        P_TryFillNewWeapon(player, slot, kAmmunitionTypeDontCare, nullptr);

    if (upgrade_slot >= 0)
    {
        player->weapons[upgrade_slot].owned = false;

        // check and update key_choices[]
        for (int w = 0; w <= 9; w++)
            if (player->key_choices[w] == upgrade_slot)
                player->key_choices[w] = (weapon_selection_e)slot;

        // handle the case of holding the weapon which is being upgraded
        // by the new one.  We mark the old weapon for removal.

        if (player->ready_wp == upgrade_slot)
        {
            player->weapons[upgrade_slot].flags |= PLWEP_Removing;
            player->pending_wp = (weapon_selection_e)slot;
        }
        else
            player->weapons[upgrade_slot].info = nullptr;

        if (player->pending_wp == upgrade_slot)
            player->pending_wp = (weapon_selection_e)slot;
    }

    return true;
}

bool P_RemoveWeapon(player_t *player, WeaponDefinition *info)
{
    // returns true if player had the weapon.

    int slot;

    for (slot = 0; slot < MAXWEAPONS; slot++)
    {
        if (!player->weapons[slot].owned)
            continue;

        // Note: no need to check PLWEP_Removing

        if (player->weapons[slot].info == info)
            break;
    }

    if (slot >= MAXWEAPONS)
        return false;

    L_WriteDebug("P_RemoveWeapon: [%s] @ %d\n", info->name_.c_str(), slot);

    player->weapons[slot].owned = false;

    P_UpdateAvailWeapons(player);

    // fix the key choices
    for (int w = 0; w <= 9; w++)
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
        player->weapons[slot].info = nullptr;

    if (player->pending_wp == slot)
        P_SelectNewWeapon(player, -100, kAmmunitionTypeDontCare);

    SYS_ASSERT(player->pending_wp != slot);

    return true;
}

void P_GiveInitialBenefits(player_t *p, const MobjType *info)
{
    // Give the player the initial benefits when they start a game
    // (or restart after dying).  Sets up: ammo, ammo-limits, health,
    // armour, keys and weapons.

    p->ready_wp   = WPSEL_None;
    p->pending_wp = WPSEL_NoChange;

    int i;

    for (i = 0; i < WEAPON_KEYS; i++)
        p->key_choices[i] = WPSEL_None;

    // clear out ammo & ammo-limits
    for (i = 0; i < kTotalAmmunitionTypes; i++)
    {
        p->ammo[i].num = p->ammo[i].max = 0;
    }

    // clear out inventory & inventory-limits
    for (i = 0; i < NUMINV; i++)
    {
        p->inventory[i].num = p->inventory[i].max = 0;
    }

    // clear out counter & counter-limits
    for (i = 0; i < NUMCOUNTER; i++)
    {
        p->counters[i].num = p->counters[i].max = 0;
    }

    // set health and armour
    p->health       = info->spawnhealth;
    p->air_in_lungs = info->lung_capacity;
    p->underwater   = false;
    p->airless   = false;


    for (i = 0; i < NUMARMOUR; i++)
    {
        p->armours[i]      = 0;
        p->armour_types[i] = nullptr;
    }

    p->totalarmour = 0;
    p->cards       = kDoorKeyNone;

    // give all initial benefits
    P_GiveBenefitList(p, nullptr, info->initial_benefits, false);

    // give all free weapons.  Needs to be after ammo, so that
    // clip weapons can get their clips filled.
    for (WeaponDefinition *w : weapondefs)
    {
        if (!w->autogive_)
            continue;

        int pw_index;

        P_AddWeapon(p, w, &pw_index);
    }

    // refresh to remove all stuff from status bar
    P_UpdateAvailWeapons(p);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
