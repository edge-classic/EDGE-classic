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

#include <float.h>

#include "AlmostEquals.h"
#include "bot_think.h"
#include "coal.h"  // for coal::vm_c
#include "colormap.h"
#include "dm_state.h"
#include "e_input.h"
#include "g_game.h"
#include "i_system.h"
#include "m_random.h"
#include "n_network.h"
#include "p_blockmap.h"
#include "p_local.h"
#include "r_misc.h"
#include "rad_trig.h"
#include "s_blit.h"
#include "s_sound.h"
#include "script/compat/lua_compat.h"
#include "vm_coal.h"

extern coal::vm_c *ui_vm;
extern void        VM_SetVector(coal::vm_c *vm, const char *mod_name,
                                const char *var_name, double val_1, double val_2,
                                double val_3);

extern ConsoleVariable double_framerate;

EDGE_DEFINE_CONSOLE_VARIABLE(erraticism, "0", kConsoleVariableFlagArchive)

EDGE_DEFINE_CONSOLE_VARIABLE(view_bobbing, "0", kConsoleVariableFlagArchive)

float room_area;

static constexpr float   kMaximumBob       = 16.0f;
static constexpr uint8_t kZoomAngleDivisor = 4;

static SoundEffect *sfx_jpidle;
static SoundEffect *sfx_jpmove;
static SoundEffect *sfx_jprise;
static SoundEffect *sfx_jpdown;
static SoundEffect *sfx_jpflow;

// Test for "measuring" size of room
static bool P_RoomPath(PathIntercept *in, void *dataptr)
{
    HMM_Vec2 *blocker = (HMM_Vec2 *)dataptr;

    if (in->line)
    {
        Line *ld = in->line;

        if (ld->back_sector && ld->front_sector)
        {
            if ((IS_SKY(ld->back_sector->ceiling) &&
                 !IS_SKY(ld->front_sector->ceiling)) ||
                (!IS_SKY(ld->back_sector->ceiling) &&
                 IS_SKY(ld->front_sector->ceiling)))
            {
                blocker->X = (ld->vertex_1->X + ld->vertex_2->X) / 2;
                blocker->Y = (ld->vertex_1->Y + ld->vertex_2->Y) / 2;
                return false;
            }
        }

        if (ld->blocked)
        {
            blocker->X = (ld->vertex_1->X + ld->vertex_2->X) / 2;
            blocker->Y = (ld->vertex_1->Y + ld->vertex_2->Y) / 2;
            return false;
        }
    }
    return true;
}

static void UpdatePowerups(player_t *player);

static void CalcHeight(player_t *player, bool extra_tic)
{
    bool      onground  = player->mo->z <= player->mo->floor_z_;
    float     sink_mult = 1.0f;
    Sector *cur_sec   = player->mo->subsector_->sector;
    if (!cur_sec->extrafloor_used && !cur_sec->height_sector && onground)
        sink_mult -= cur_sec->sink_depth;

    if (erraticism.d_ && level_time_elapsed > 0 &&
        (!player->cmd.forward_move && !player->cmd.side_move) &&
        ((AlmostEquals(player->mo->height_, player->mo->info_->height_) ||
          AlmostEquals(player->mo->height_,
                       player->mo->info_->crouchheight_)) &&
         (AlmostEquals(player->deltaviewheight, 0.0f) || sink_mult < 1.0f)))
        return;

    if (player->mo->height_ <
        (player->mo->info_->height_ + player->mo->info_->crouchheight_) / 2.0f)
        player->mo->extended_flags_ |= kExtendedFlagCrouching;
    else
        player->mo->extended_flags_ &= ~kExtendedFlagCrouching;

    player->std_viewheight =
        player->mo->height_ * player->mo->info_->viewheight_;

    if (sink_mult < 1.0f)
        player->deltaviewheight =
            HMM_MAX(player->deltaviewheight - 1.0f, -1.0f);

    // calculate the walking / running height adjustment.

    float bob_z = 0;

    // Regular movement bobbing
    // (needs to be calculated for gun swing even if not on ground).
    // -AJA- Moved up here, to prevent weapon jumps when running down
    // stairs.

    if (erraticism.d_)
        player->bob = 12.0f;
    else
        player->bob = (player->mo->momentum_.X * player->mo->momentum_.X +
                       player->mo->momentum_.Y * player->mo->momentum_.Y) /
                      8;

    if (player->bob > kMaximumBob) player->bob = kMaximumBob;

    // ----CALCULATE BOB EFFECT----
    if (player->playerstate == PST_LIVE && onground)
    {
        BAMAngle angle = kBAMAngle90 / 5 * level_time_elapsed;

        bob_z =
            player->bob / 2 * player->mo->info_->bobbing_ * epi::BAMSin(angle);
    }

    // ----CALCULATE VIEWHEIGHT----
    if (player->playerstate == PST_LIVE)
    {
        player->viewheight +=
            player->deltaviewheight * (double_framerate.d_ ? 0.5 : 1);

        if (player->viewheight > player->std_viewheight)
        {
            player->viewheight      = player->std_viewheight;
            player->deltaviewheight = 0;
        }
        else if (sink_mult < 1.0f &&
                 !(player->mo->extended_flags_ & kExtendedFlagCrouching) &&
                 player->viewheight < player->std_viewheight * sink_mult)
        {
            player->viewheight = player->std_viewheight * sink_mult;
            if (player->deltaviewheight <= 0) player->deltaviewheight = 0.01f;
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
            if (!extra_tic || !double_framerate.d_)
                player->deltaviewheight += 0.24162f;
        }
    }

    //----CALCULATE FREEFALL EFFECT, WITH SOUND EFFECTS (code based on HEXEN)
    //  CORBIN, on:
    //  6/6/2011 - Fix this so RTS does NOT interfere with fracunits (it does in
    //  Hypertension's E1M1 starting script)! 6/7/2011 - Ajaped said to remove
    //  FRACUNIT...seeya oldness.

    // if ((player->mo->momentum_.z <= -35.0)&&(player->mo->momentum_.z >=
    // -40.0))
    if ((player->mo->momentum_.Z <= -35.0) &&
        (player->mo->momentum_.Z >= -36.0))
        if (player->mo->info_->falling_sound_)
        {
            int sfx_cat;

            if (player == players[consoleplayer]) { sfx_cat = kCategoryPlayer; }
            else { sfx_cat = kCategoryOpponent; }
            StartSoundEffect(player->mo->info_->falling_sound_, sfx_cat, player->mo);
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

    if (double_framerate.d_) bob_z *= 0.5;

    if (view_bobbing.d_ > 1) bob_z = 0;

    player->view_z = player->viewheight + bob_z;
}

void P_PlayerJump(player_t *pl, float dz, int wait)
{
    pl->mo->momentum_.Z += dz;

    if (pl->jumpwait < wait) pl->jumpwait = wait;

    // enter the JUMP states (if present)
    int jump_st = P_MobjFindLabel(pl->mo, "JUMP");
    if (jump_st != 0) P_SetMobjStateDeferred(pl->mo, jump_st, 0);

    // -AJA- 1999/09/11: New JUMP_SOUND for ddf.
    if (pl->mo->info_->jump_sound_)
    {
        int sfx_cat;

        if (pl == players[consoleplayer])
            sfx_cat = kCategoryPlayer;
        else
            sfx_cat = kCategoryOpponent;

        StartSoundEffect(pl->mo->info_->jump_sound_, sfx_cat, pl->mo);
    }
}

static void MovePlayer(player_t *player, bool extra_tic)
{
    EventTicCommand *cmd;
    MapObject       *mo = player->mo;

    bool onground = player->mo->z <= player->mo->floor_z_;
    bool onladder = player->mo->on_ladder_ >= 0;

    bool swimming = player->swimming;
    bool flying   = (player->powers[kPowerTypeJetpack] > 0) && !swimming;
    bool jumping  = (player->jumpwait > 0);
    bool crouching =
        (player->mo->extended_flags_ & kExtendedFlagCrouching) ? true : false;

    float dx, dy;
    float eh, ev;

    float base_xy_speed;
    float base_z_speed;

    float F_vec[3], U_vec[3], S_vec[3];

    cmd = &player->cmd;

    if (player->zoom_fov > 0) cmd->angle_turn /= kZoomAngleDivisor;

    player->mo->angle_ -= (BAMAngle)(cmd->angle_turn << 16);

    // EDGE Feature: Vertical Look (Mlook)
    //
    // -ACB- 1998/07/02 New Code used, rerouted via Ticcmd
    // -ACB- 1998/07/27 Used defines for look limits.
    //
    if (level_flags.mlook)
    {
        if (player->zoom_fov > 0) cmd->mouselook_turn /= kZoomAngleDivisor;

        BAMAngle V =
            player->mo->vertical_angle_ + (BAMAngle)(cmd->mouselook_turn << 16);

        if (V < kBAMAngle180 && V > MLOOK_LIMIT)
            V = MLOOK_LIMIT;
        else if (V >= kBAMAngle180 && V < (kBAMAngle360 - MLOOK_LIMIT))
            V = (kBAMAngle360 - MLOOK_LIMIT);

        player->mo->vertical_angle_ = V;
    }
    else { player->mo->vertical_angle_ = 0; }

    // EDGE Feature: Vertical Centering
    //
    // -ACB- 1998/07/02 Re-routed via Ticcmd
    //
    if (cmd->extended_buttons & kExtendedButtonCodeCenter)
        player->mo->vertical_angle_ = 0;

    // compute XY and Z speeds, taking swimming (etc) into account
    // (we try to swim in view direction -- assumes no gravity).

    base_xy_speed = player->mo->speed_ / (double_framerate.d_ ? 64.0f : 32.0f);
    base_z_speed  = player->mo->speed_ / (double_framerate.d_ ? 57.0f : 64.0f);

    // Do not let the player control movement if not onground.
    // -MH- 1998/06/18  unless he has the JetPack!

    if (!(onground || onladder || swimming || flying)) base_xy_speed /= 16.0f;

    if (!(onladder || swimming || flying)) base_z_speed /= 16.0f;

    // move slower when crouching
    if (crouching) base_xy_speed *= CROUCH_SLOWDOWN;

    dx = epi::BAMCos(player->mo->angle_);
    dy = epi::BAMSin(player->mo->angle_);

    eh = 1;
    ev = 0;

    if (swimming || flying || onladder)
    {
        float slope = epi::BAMTan(player->mo->vertical_angle_);

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

    player->mo->momentum_.X += F_vec[0] * cmd->forward_move +
                               S_vec[0] * cmd->side_move +
                               U_vec[0] * cmd->upward_move;

    player->mo->momentum_.Y += F_vec[1] * cmd->forward_move +
                               S_vec[1] * cmd->side_move +
                               U_vec[1] * cmd->upward_move;

    if (flying || swimming || !onground || onladder)
    {
        player->mo->momentum_.Z += F_vec[2] * cmd->forward_move +
                                   S_vec[2] * cmd->side_move +
                                   U_vec[2] * cmd->upward_move;
    }

    if (flying && !swimming)
    {
        int sfx_cat;

        if (player == players[consoleplayer])
            sfx_cat = kCategoryPlayer;
        else
            sfx_cat = kCategoryOpponent;

        if (player->powers[kPowerTypeJetpack] <= (5 * kTicRate))
        {
            if ((level_time_elapsed & 10) == 0)
                StartSoundEffect(sfx_jpflow, sfx_cat, player->mo);  // fuel low
        }
        else if (cmd->upward_move > 0)
            StartSoundEffect(sfx_jprise, sfx_cat, player->mo);
        else if (cmd->upward_move < 0)
            StartSoundEffect(sfx_jpdown, sfx_cat, player->mo);
        else if (cmd->forward_move || cmd->side_move)
            StartSoundEffect((onground ? sfx_jpidle : sfx_jpmove), sfx_cat,
                      player->mo);
        else
            StartSoundEffect(sfx_jpidle, sfx_cat, player->mo);
    }

    if (player->mo->state_ == &states[player->mo->info_->idle_state_])
    {
        if (!jumping && !flying && (onground || swimming) &&
            (cmd->forward_move || cmd->side_move))
        {
            // enter the CHASE (i.e. walking) states
            if (player->mo->info_->chase_state_)
                P_SetMobjStateDeferred(player->mo,
                                       player->mo->info_->chase_state_, 0);
        }
    }

    // EDGE Feature: Jump Code
    //
    // -ACB- 1998/08/09 Check that jumping is allowed in the current_map
    //                  Make player pause before jumping again

    if (!extra_tic || !double_framerate.d_)
    {
        if (level_flags.jump && mo->info_->jumpheight_ > 0 &&
            (cmd->upward_move > 4))
        {
            if (!jumping && !crouching && !swimming && !flying && onground &&
                !onladder)
            {
                P_PlayerJump(player,
                             player->mo->info_->jumpheight_ /
                                 (double_framerate.d_ ? 1.25f : 1.4f),
                             player->mo->info_->jump_delay_);
            }
        }
    }

    // EDGE Feature: Crouching

    if (level_flags.crouch && mo->info_->crouchheight_ > 0 &&
        (player->cmd.upward_move < -4) && !player->wet_feet && !jumping &&
        onground)
    // NB: no ladder check, onground is sufficient
    {
        if (mo->height_ > mo->info_->crouchheight_)
        {
            mo->height_ =
                HMM_MAX(mo->height_ - 2.0f / (double_framerate.d_ ? 2.0 : 1.0),
                        mo->info_->crouchheight_);
            mo->player_->deltaviewheight = -1.0f;
        }
    }
    else  // STAND UP
    {
        if (mo->height_ < mo->info_->height_)
        {
            float new_height =
                HMM_MIN(mo->height_ + 2 / (double_framerate.d_ ? 2 : 1),
                        mo->info_->height_);

            // prevent standing up inside a solid area
            if ((mo->flags_ & kMapObjectFlagNoClip) ||
                mo->z + new_height <= mo->ceiling_z_)
            {
                mo->height_                  = new_height;
                mo->player_->deltaviewheight = 1.0f;
            }
        }
    }

    // EDGE Feature: Zooming
    //
    if (cmd->extended_buttons & kExtendedButtonCodeZoom)
    {
        int fov = 0;

        if (player->zoom_fov == 0)
        {
            if (!(player->ready_wp < 0 || player->pending_wp >= 0))
                fov = player->weapons[player->ready_wp].info->zoom_fov_;

            if (fov == int(kBAMAngle360)) fov = 0;
        }

        player->zoom_fov = fov;
    }
}

static void DeathThink(player_t *player, bool extra_tic)
{
    int subtract = extra_tic ? 0 : 1;

    if (!double_framerate.d_) subtract = 1;

    // fall on your face when dying.

    float dx, dy, dz;

    BAMAngle angle;
    BAMAngle delta, delta_s;
    float    slope;

    // -AJA- 1999/12/07: don't die mid-air.
    player->powers[kPowerTypeJetpack] = 0;

    if (!extra_tic) MovePlayerSprites(player);

    // fall to the ground
    if (player->viewheight > player->std_viewheight)
        player->viewheight -= 1.0f / (double_framerate.d_ ? 2.0 : 1.0);
    else if (player->viewheight < player->std_viewheight)
        player->viewheight = player->std_viewheight;

    player->deltaviewheight = 0.0f;
    player->kick_offset     = 0.0f;

    CalcHeight(player, extra_tic);

    if (player->attacker && player->attacker != player->mo)
    {
        dx = player->attacker->x - player->mo->x;
        dy = player->attacker->y - player->mo->y;
        dz = (player->attacker->z + player->attacker->height_ / 2) -
             (player->mo->z + player->viewheight);

        angle = RendererPointToAngle(0, 0, dx, dy);
        delta = angle - player->mo->angle_;

        slope   = ApproximateSlope(dx, dy, dz);
        slope   = HMM_MIN(1.7f, HMM_MAX(-1.7f, slope));
        delta_s = epi::BAMFromATan(slope) - player->mo->vertical_angle_;

        if ((delta <= kBAMAngle1 / 2 ||
             delta >= (BAMAngle)(0 - kBAMAngle1 / 2)) &&
            (delta_s <= kBAMAngle1 / 2 ||
             delta_s >= (BAMAngle)(0 - kBAMAngle1 / 2)))
        {
            // Looking at killer, so fade damage flash down.
            player->mo->angle_          = angle;
            player->mo->vertical_angle_ = epi::BAMFromATan(slope);

            if (player->damagecount > 0) player->damagecount -= subtract;
        }
        else
        {
            unsigned int factor = double_framerate.d_ ? 2 : 1;
            if (delta < kBAMAngle180)
                delta /= (5 * factor);
            else
                delta = (BAMAngle)(0 - (BAMAngle)(0 - delta) / (5 * factor));

            if (delta > kBAMAngle5 / factor &&
                delta < (BAMAngle)(0 - kBAMAngle5 / factor))
                delta = (delta < kBAMAngle180)
                            ? kBAMAngle5 / factor
                            : (BAMAngle)(0 - kBAMAngle5 / factor);

            if (delta_s < kBAMAngle180)
                delta_s /= (5 * factor);
            else
                delta_s =
                    (BAMAngle)(0 - (BAMAngle)(0 - delta_s) / (5 * factor));

            if (delta_s > (kBAMAngle5 / (factor * 2)) &&
                delta_s < (BAMAngle)(0 - kBAMAngle5 / (factor * 2)))
                delta_s = (delta_s < kBAMAngle180)
                              ? (kBAMAngle5 / (factor * 2))
                              : (BAMAngle)(0 - kBAMAngle5 / (factor * 2));

            player->mo->angle_ += delta;
            player->mo->vertical_angle_ += delta_s;

            if (player->damagecount && (level_time_elapsed % 3) == 0)
                player->damagecount -= subtract;
        }
    }
    else if (player->damagecount > 0)
        player->damagecount--;

    // -AJA- 1999/08/07: Fade out armor points too.
    if (player->bonuscount) player->bonuscount -= subtract;

    UpdatePowerups(player);

    // lose the zoom when dead
    player->zoom_fov = 0;

    if (deathmatch >= 3 &&
        player->mo->move_count_ > player->mo->info_->respawntime_)
        return;

    if (player->cmd.buttons & kButtonCodeUse) player->playerstate = PST_REBORN;
}

static void UpdatePowerups(player_t *player)
{
    float limit = FLT_MAX;
    int   pw;

    if (player->playerstate == PST_DEAD) limit = 1;  // kTicRate * 5;

    for (pw = 0; pw < kTotalPowerTypes; pw++)
    {
        if (player->powers[pw] <
            0)  // -ACB- 2004/02/04 Negative values last a level
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

    if (player->powers[kPowerTypePartInvis] >= 128 ||
        fmod(player->powers[kPowerTypePartInvis], 16) >= 8)
        player->mo->flags_ |= kMapObjectFlagFuzzy;
    else
        player->mo->flags_ &= ~kMapObjectFlagFuzzy;

    // Handling colormaps.
    //
    // -AJA- 1999/07/10: Updated for colmap.ddf.
    //
    // !!! FIXME: overlap here with stuff in rgl_fx.cpp.

    player->effect_colourmap = nullptr;
    player->effect_left      = 0;

    if (player->powers[kPowerTypeInvulnerable] > 0)
    {
        float s = player->powers[kPowerTypeInvulnerable];

        // -ACB- FIXME!!! Catch lookup failure!
        player->effect_colourmap = colormaps.Lookup("ALLWHITE");
        player->effect_left = (s <= 0) ? 0 : HMM_MIN(int(s), EFFECT_MAX_TIME);
    }
    else if (player->powers[kPowerTypeInfrared] > 0)
    {
        float s = player->powers[kPowerTypeInfrared];

        player->effect_left = (s <= 0) ? 0 : HMM_MIN(int(s), EFFECT_MAX_TIME);
    }
    else if (player->powers[kPowerTypeNightVision] >
             0)  // -ACB- 1998/07/15 NightVision Code
    {
        float s = player->powers[kPowerTypeNightVision];

        // -ACB- FIXME!!! Catch lookup failure!
        player->effect_colourmap = colormaps.Lookup("ALLGREEN");
        player->effect_left = (s <= 0) ? 0 : HMM_MIN(int(s), EFFECT_MAX_TIME);
    }
    else if (player->powers[kPowerTypeBerserk] >
             0)  // Lobo 2021: Un-Hardcode Berserk colour tint
    {
        float s = player->powers[kPowerTypeBerserk];

        player->effect_colourmap = colormaps.Lookup("BERSERK");
        player->effect_left = (s <= 0) ? 0 : HMM_MIN(int(s), EFFECT_MAX_TIME);
    }
}

// Does the thinking of the console player, i.e. read from input
void P_ConsolePlayerBuilder(const player_t *pl, void *data,
                            EventTicCommand *dest)
{
    EventBuildTicCommand(dest);

    dest->player_index = pl->pnum;
}

bool P_PlayerSwitchWeapon(player_t *player, WeaponDefinition *choice)
{
    int pw_index;

    // see if player owns this kind of weapon
    for (pw_index = 0; pw_index < kMaximumWeapons; pw_index++)
    {
        if (!player->weapons[pw_index].owned) continue;

        if (player->weapons[pw_index].info == choice) break;
    }

    if (pw_index == kMaximumWeapons) return false;

    // ignore this choice if it the same as the current weapon
    if (player->ready_wp >= 0 &&
        choice == player->weapons[player->ready_wp].info)
    {
        return false;
    }

    player->pending_wp = (weapon_selection_e)pw_index;

    return true;
}

void P_DumpMobjsTemp(void)
{
    MapObject *mo;

    int index = 0;

    LogWarning("MOBJs:\n");

    for (mo = map_object_list_head; mo; mo = mo->next_, index++)
    {
        LogWarning(
            " %4d: %p next:%p prev:%p [%s] at (%1.0f,%1.0f,%1.0f) states=%d > "
            "%d tics=%d\n",
            index, mo, mo->next_, mo->previous_, mo->info_->name_.c_str(),
            mo->x, mo->y, mo->z, (int)(mo->state_ ? mo->state_ - states : -1),
            (int)(mo->next_state_ ? mo->next_state_ - states : -1), mo->tics_);
    }

    LogWarning("END OF MOBJs\n");
}

bool P_PlayerThink(player_t *player, bool extra_tic)
{
    EventTicCommand *cmd = &player->cmd;

    SYS_ASSERT(player->mo);

    bool should_think = true;

    if (player->attacker && player->attacker->IsRemoved())
    {
        P_DumpMobjsTemp();
        FatalError("INTERNAL ERROR: player has a removed attacker. \n");
    }

    if (player->damagecount <= 0) player->damage_pain = 0;

    // fixme: do this in the cheat code
    if (player->cheats & CF_NOCLIP)
        player->mo->flags_ |= kMapObjectFlagNoClip;
    else
        player->mo->flags_ &= ~kMapObjectFlagNoClip;

    // chain saw run forward
    if (extra_tic || !double_framerate.d_)
    {
        if (player->mo->flags_ & kMapObjectFlagJustAttacked)
        {
            cmd->angle_turn   = 0;
            cmd->forward_move = 64;
            cmd->side_move    = 0;
            player->mo->flags_ &= ~kMapObjectFlagJustAttacked;
        }
    }

    if (player->playerstate == PST_DEAD)
    {
        DeathThink(player, extra_tic);
        if (player->mo->region_properties_->special &&
            player->mo->region_properties_->special->e_exit_ != kExitTypeNone)
        {
            ExitType do_exit = player->mo->region_properties_->special->e_exit_;

            player->mo->subsector_->sector->properties.special = nullptr;

            if (do_exit == kExitTypeSecret)
                GameSecretExitLevel(1);
            else
                GameExitLevel(1);
        }
        return true;
    }

    int subtract = extra_tic ? 0 : 1;
    if (!double_framerate.d_) subtract = 1;

    // Move/Look around.  Reactiontime is used to prevent movement for a
    // bit after a teleport.

    if (player->mo->reaction_time_) player->mo->reaction_time_ -= subtract;

    if (player->mo->reaction_time_ == 0) MovePlayer(player, extra_tic);

    CalcHeight(player, extra_tic);

    if (erraticism.d_)
    {
        bool      sinking = false;
        Sector *cur_sec = player->mo->subsector_->sector;
        if (!cur_sec->extrafloor_used && !cur_sec->height_sector &&
            cur_sec->sink_depth > 0 && player->mo->z <= player->mo->floor_z_)
            sinking = true;
        if (cmd->forward_move == 0 && cmd->side_move == 0 &&
            !player->swimming && cmd->upward_move <= 0 &&
            !(cmd->buttons &
              (kButtonCodeAttack | kButtonCodeUse | kButtonCodeChangeWeapon |
               kExtendedButtonCodeSecondAttack | kExtendedButtonCodeReload |
               kExtendedButtonCodeAction1 | kExtendedButtonCodeAction2 |
               kExtendedButtonCodeInventoryUse |
               kExtendedButtonCodeThirdAttack |
               kExtendedButtonCodeFourthAttack)) &&
            ((AlmostEquals(player->mo->height_, player->mo->info_->height_) ||
              AlmostEquals(player->mo->height_,
                           player->mo->info_->crouchheight_)) &&
             (AlmostEquals(player->deltaviewheight, 0.0f) || sinking)))
        {
            should_think = false;
            if (!player->mo->momentum_.Z)
            {
                player->mo->momentum_.X = 0;
                player->mo->momentum_.Y = 0;
            }
        }
    }

    // Reset environmental FX in case player has left sector in which they apply
    // - Dasho
    vacuum_sound_effects       = false;
    submerged_sound_effects    = false;
    outdoor_reverb   = false;
    ddf_reverb       = false;
    ddf_reverb_type  = 0;
    ddf_reverb_delay = 0;
    ddf_reverb_ratio = 0;

    if (player->mo->region_properties_->special ||
        player->mo->subsector_->sector->extrafloor_used > 0 ||
        player->underwater || player->swimming || player->airless)
    {
        PlayerInSpecialSector(player, player->mo->subsector_->sector,
                              should_think);
    }

    if (IS_SKY(player->mo->subsector_->sector->ceiling)) outdoor_reverb = true;

    // Check for weapon change.
    if (cmd->buttons & kButtonCodeChangeWeapon)
    {
        // The actual changing of the weapon is done when the weapon
        // psprite can do it (read: not in the middle of an attack).

        int key = (cmd->buttons & kButtonCodeWeaponMask) >>
                  kButtonCodeWeaponMaskShift;

        if (key == kButtonCodeNextWeapon) { CycleWeapon(player, +1); }
        else if (key == kButtonCodePreviousWeapon) { CycleWeapon(player, -1); }
        else /* numeric key */ { DesireWeaponChange(player, key); }
    }

    // check for use
    if (cmd->buttons & kButtonCodeUse)
    {
        if (!player->usedown)
        {
            P_UseLines(player);
            player->usedown = true;
        }
    }
    else { player->usedown = false; }

    player->actiondown[0] =
        (cmd->extended_buttons & kExtendedButtonCodeAction1) ? true : false;
    player->actiondown[1] =
        (cmd->extended_buttons & kExtendedButtonCodeAction2) ? true : false;

    if (LUA_UseLuaHud())
        LUA_SetVector3(
            LUA_GetGlobalVM(), "player", "inventory_event_handler",
            HMM_Vec3{
                {cmd->extended_buttons & kExtendedButtonCodeInventoryPrevious
                     ? 1.0f
                     : 0.0f,
                 cmd->extended_buttons & kExtendedButtonCodeInventoryUse ? 1.0f
                                                                         : 0.0f,
                 cmd->extended_buttons & kExtendedButtonCodeInventoryNext
                     ? 1.0f
                     : 0.0f}});
    else
        VM_SetVector(
            ui_vm, "player", "inventory_event_handler",
            cmd->extended_buttons & kExtendedButtonCodeInventoryPrevious ? 1
                                                                         : 0,
            cmd->extended_buttons & kExtendedButtonCodeInventoryUse ? 1 : 0,
            cmd->extended_buttons & kExtendedButtonCodeInventoryNext ? 1 : 0);

    // FIXME separate code more cleanly
    if (extra_tic && double_framerate.d_) return should_think;

    // decrement jumpwait counter
    if (player->jumpwait > 0) player->jumpwait--;

    if (player->splashwait > 0) player->splashwait--;

    // cycle psprites
    MovePlayerSprites(player);

    // Counters, time dependend power ups.

    UpdatePowerups(player);

    if (player->damagecount > 0) player->damagecount--;

    if (player->bonuscount > 0) player->bonuscount--;

    if (player->grin_count > 0) player->grin_count--;

    if (player->attackdown[0] || player->attackdown[1])
        player->attackdown_count++;
    else
        player->attackdown_count = 0;

    player->kick_offset /= 1.6f;

    if (players[consoleplayer] == player && dynamic_reverb)
    {
        // Approximate "room size" determination for reverb system - Dasho
        HMM_Vec2 room_checker;
        float    line_lengths = 0;
        float    player_x     = player->mo->x;
        float    player_y     = player->mo->y;
        PathTraverse(player_x, player_y, player_x, 32768.0f, kPathAddLines,
                     P_RoomPath, &room_checker);
        line_lengths += abs(room_checker.Y - player_y);
        PathTraverse(player_x, player_y, 32768.0f + player_x,
                     32768.0f + player_y, kPathAddLines, P_RoomPath,
                     &room_checker);
        line_lengths +=
            RendererPointToDistance(player_x, player_y, room_checker.X, room_checker.Y);
        PathTraverse(player_x, player_y, -32768.0f + player_x,
                     32768.0f + player_y, kPathAddLines, P_RoomPath,
                     &room_checker);
        line_lengths +=
            RendererPointToDistance(player_x, player_y, room_checker.X, room_checker.Y);
        PathTraverse(player_x, player_y, player_x, -32768.0f, kPathAddLines,
                     P_RoomPath, &room_checker);
        line_lengths += abs(player_y - room_checker.Y);
        PathTraverse(player_x, player_y, -32768.0f + player_x,
                     -32768.0f + player_y, kPathAddLines, P_RoomPath,
                     &room_checker);
        line_lengths +=
            RendererPointToDistance(player_x, player_y, room_checker.X, room_checker.Y);
        PathTraverse(player_x, player_y, 32768.0f + player_x,
                     -32768.0f + player_y, kPathAddLines, P_RoomPath,
                     &room_checker);
        line_lengths +=
            RendererPointToDistance(player_x, player_y, room_checker.X, room_checker.Y);
        PathTraverse(player_x, player_y, -32768.0f, player_y, kPathAddLines,
                     P_RoomPath, &room_checker);
        line_lengths += abs(player_x - room_checker.X);
        PathTraverse(player_x, player_y, 32768.0f, player_y, kPathAddLines,
                     P_RoomPath, &room_checker);
        line_lengths += abs(room_checker.X - player_x);
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
    if (is_bot) numbots++;

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

    if (is_bot) P_BotCreate(p, false);

    if (!sfx_jpidle)
    {
        sfx_jpidle = sfxdefs.GetEffect("JPIDLE");
        sfx_jpmove = sfxdefs.GetEffect("JPMOVE");
        sfx_jprise = sfxdefs.GetEffect("JPRISE");
        sfx_jpdown = sfxdefs.GetEffect("JPDOWN");
        sfx_jpflow = sfxdefs.GetEffect("JPFLOW");
    }
}

void DestroyAllPlayers(void)
{
    for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
    {
        if (!players[pnum]) continue;

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

void UpdateAvailWeapons(player_t *p)
{
    // Must be called as soon as the player has received or lost
    // a weapon.  Updates the status bar icons.

    int key;

    for (key = 0; key < kTotalWeaponKeys; key++) p->avail_weapons[key] = false;

    for (int i = 0; i < kMaximumWeapons; i++)
    {
        if (!p->weapons[i].owned) continue;

        SYS_ASSERT(p->weapons[i].info);

        key = p->weapons[i].info->bind_key_;

        // update the status bar icons
        if (0 <= key && key <= 9) p->avail_weapons[key] = true;
    }
}

void UpdateTotalArmour(player_t *p)
{
    int i;

    p->totalarmour = 0;

    for (i = 0; i < kTotalArmourTypes; i++)
    {
        p->totalarmour += p->armours[i];

        // forget the association once fully depleted
        if (p->armours[i] <= 0) p->armour_types[i] = nullptr;
    }

    if (p->totalarmour > 999.0f) p->totalarmour = 999.0f;
}

bool AddWeapon(player_t *player, WeaponDefinition *info, int *index)
{
    // Returns true if player did not already have the weapon.
    // If successful and 'index' is non-nullptr, the new index is
    // stored there.

    int slot         = -1;
    int upgrade_slot = -1;

    // cannot own weapons if sprites are missing
    if (!CheckWeaponSprite(info))
    {
        LogWarning("WEAPON %s has no sprites and will not be added!\n",
                   info->name_.c_str());
        return false;
    }

    for (int i = 0; i < kMaximumWeapons; i++)
    {
        WeaponDefinition *cur_info = player->weapons[i].info;

        // skip weapons that are being removed
        if (player->weapons[i].flags & kPlayerWeaponRemoving) continue;

        // find free slot
        if (!player->weapons[i].owned)
        {
            if (slot < 0) slot = i;
            continue;
        }

        // check if already own this weapon
        if (cur_info == info) return false;

        // don't downgrade any UPGRADED weapons
        if (DDF_WeaponIsUpgrade(cur_info, info)) return false;

        // check for weapon upgrades
        if (cur_info == info->upgrade_weap_)
        {
            upgrade_slot = i;
            continue;
        }
    }

    if (slot < 0) return false;

    if (index) (*index) = slot;

    LogDebug("AddWeapon: [%s] @ %d\n", info->name_.c_str(), slot);

    player->weapons[slot].owned        = true;
    player->weapons[slot].info         = info;
    player->weapons[slot].flags        = kPlayerWeaponNoFlag;
    player->weapons[slot].clip_size[0] = 0;
    player->weapons[slot].clip_size[1] = 0;
    player->weapons[slot].model_skin   = info->model_skin_;

    UpdateAvailWeapons(player);

    // for NoAmmo+Clip weapons, always begin with a full clip
    for (int ATK = 0; ATK < 2; ATK++)
    {
        if (info->clip_size_[ATK] > 0 &&
            info->ammo_[ATK] == kAmmunitionTypeNoAmmo)
            player->weapons[slot].clip_size[ATK] = info->clip_size_[ATK];
    }

    // initial weapons should get a full clip
    if (info->autogive_)
        TryFillNewWeapon(player, slot, kAmmunitionTypeDontCare, nullptr);

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
            player->weapons[upgrade_slot].flags =
                (PlayerWeaponFlag)(player->weapons[upgrade_slot].flags |
                                   kPlayerWeaponRemoving);
            player->pending_wp = (weapon_selection_e)slot;
        }
        else
            player->weapons[upgrade_slot].info = nullptr;

        if (player->pending_wp == upgrade_slot)
            player->pending_wp = (weapon_selection_e)slot;
    }

    return true;
}

bool RemoveWeapon(player_t *player, WeaponDefinition *info)
{
    // returns true if player had the weapon.

    int slot;

    for (slot = 0; slot < kMaximumWeapons; slot++)
    {
        if (!player->weapons[slot].owned) continue;

        // Note: no need to check kPlayerWeaponRemoving

        if (player->weapons[slot].info == info) break;
    }

    if (slot >= kMaximumWeapons) return false;

    LogDebug("RemoveWeapon: [%s] @ %d\n", info->name_.c_str(), slot);

    player->weapons[slot].owned = false;

    UpdateAvailWeapons(player);

    // fix the key choices
    for (int w = 0; w <= 9; w++)
        if (player->key_choices[w] == slot) player->key_choices[w] = WPSEL_None;

    // handle the case of already holding the weapon.  We mark the
    // weapon as being removed (the flag is cleared once lowered).

    if (player->ready_wp == slot)
    {
        player->weapons[slot].flags =
            (PlayerWeaponFlag)(player->weapons[slot].flags |
                               kPlayerWeaponRemoving);

        if (player->pending_wp == WPSEL_NoChange) DropWeapon(player);
    }
    else
        player->weapons[slot].info = nullptr;

    if (player->pending_wp == slot)
        SelectNewWeapon(player, -100, kAmmunitionTypeDontCare);

    SYS_ASSERT(player->pending_wp != slot);

    return true;
}

void P_GiveInitialBenefits(player_t *p, const MapObjectDefinition *info)
{
    // Give the player the initial benefits when they start a game
    // (or restart after dying).  Sets up: ammo, ammo-limits, health,
    // armour, keys and weapons.

    p->ready_wp   = WPSEL_None;
    p->pending_wp = WPSEL_NoChange;

    int i;

    for (i = 0; i < kTotalWeaponKeys; i++) p->key_choices[i] = WPSEL_None;

    // clear out ammo & ammo-limits
    for (i = 0; i < kTotalAmmunitionTypes; i++)
    {
        p->ammo[i].num = p->ammo[i].max = 0;
    }

    // clear out inventory & inventory-limits
    for (i = 0; i < kTotalInventoryTypes; i++)
    {
        p->inventory[i].num = p->inventory[i].max = 0;
    }

    // clear out counter & counter-limits
    for (i = 0; i < kTotalCounterTypes; i++)
    {
        p->counters[i].num = p->counters[i].max = 0;
    }

    // set health and armour
    p->health       = info->spawn_health_;
    p->air_in_lungs = info->lung_capacity_;
    p->underwater   = false;
    p->airless      = false;

    for (i = 0; i < kTotalArmourTypes; i++)
    {
        p->armours[i]      = 0;
        p->armour_types[i] = nullptr;
    }

    p->totalarmour = 0;
    p->cards       = kDoorKeyNone;

    // give all initial benefits
    GiveBenefitList(p, nullptr, info->initial_benefits_, false);

    // give all free weapons.  Needs to be after ammo, so that
    // clip weapons can get their clips filled.
    for (WeaponDefinition *w : weapondefs)
    {
        if (!w->autogive_) continue;

        int pw_index;

        AddWeapon(p, w, &pw_index);
    }

    // refresh to remove all stuff from status bar
    UpdateAvailWeapons(p);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
