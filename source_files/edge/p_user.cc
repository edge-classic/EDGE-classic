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
#include <math.h>

#include "AlmostEquals.h"
#include "bot_think.h"
#ifdef EDGE_CLASSIC
#include "coal.h"
#endif
#include "ddf_colormap.h"
#include "ddf_reverb.h"
#include "dm_state.h"
#include "e_input.h"
#include "g_game.h"
#include "i_sound.h"
#include "i_system.h"
#include "m_random.h"
#include "n_network.h"
#include "p_blockmap.h"
#include "p_local.h"
#include "r_misc.h"
#include "rad_trig.h"
#include "s_blit.h"
#include "s_music.h"
#include "s_sound.h"
#include "script/compat/lua_compat.h"
#include "stb_sprintf.h"
#ifdef EDGE_CLASSIC
#include "vm_coal.h"

extern coal::VM *ui_vm;
extern void      COALSetVector(coal::VM *vm, const char *mod_name, const char *var_name, double val_1, double val_2,
                               double val_3);
#endif

EDGE_DEFINE_CONSOLE_VARIABLE(erraticism, "0", kConsoleVariableFlagArchive)

EDGE_DEFINE_CONSOLE_VARIABLE(view_bobbing, "0", kConsoleVariableFlagArchive)

static constexpr float   kMaximumBob       = 16.0f;
static constexpr uint8_t kZoomAngleDivisor = 4;
static const BAMAngle    kMouseLookLimit   = epi::BAMFromDegrees(75);
static constexpr float   kCrouchSlowdown   = 0.5f;

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
            if ((EDGE_IMAGE_IS_SKY(ld->back_sector->ceiling) && !EDGE_IMAGE_IS_SKY(ld->front_sector->ceiling)) ||
                (!EDGE_IMAGE_IS_SKY(ld->back_sector->ceiling) && EDGE_IMAGE_IS_SKY(ld->front_sector->ceiling)))
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

static void UpdatePowerups(Player *player);

static void CalcHeight(Player *player)
{
    bool    onground  = player->map_object_->z <= player->map_object_->floor_z_;
    float   sink_mult = 1.0f;
    Sector *cur_sec   = player->map_object_->subsector_->sector;
    if (!cur_sec->extrafloor_used && !cur_sec->height_sector && onground)
        sink_mult -= cur_sec->sink_depth;

    if (erraticism.d_ && level_time_elapsed > 0 && (!player->command_.forward_move && !player->command_.side_move) &&
        ((AlmostEquals(player->map_object_->height_, player->map_object_->info_->height_) ||
          AlmostEquals(player->map_object_->height_, player->map_object_->info_->crouchheight_)) &&
         (AlmostEquals(player->delta_view_height_, 0.0f) || sink_mult < 1.0f)))
        return;

    if (player->map_object_->height_ <
        (player->map_object_->info_->height_ + player->map_object_->info_->crouchheight_) / 2.0f)
        player->map_object_->extended_flags_ |= kExtendedFlagCrouching;
    else
        player->map_object_->extended_flags_ &= ~kExtendedFlagCrouching;

    player->standard_view_height_ = player->map_object_->height_ * player->map_object_->info_->viewheight_;

    if (sink_mult < 1.0f)
        player->delta_view_height_ = HMM_MAX(player->delta_view_height_ - 1.0f, -1.0f);

    // calculate the walking / running height adjustment.

    float bob_z = 0;

    // Regular movement bobbing
    // (needs to be calculated for gun swing even if not on ground).
    // -AJA- Moved up here, to prevent weapon jumps when running down
    // stairs.

    if (erraticism.d_)
        player->bob_factor_ = 12.0f;
    else
        player->bob_factor_ = (player->map_object_->momentum_.X * player->map_object_->momentum_.X +
                               player->map_object_->momentum_.Y * player->map_object_->momentum_.Y) /
                              8;

    if (player->bob_factor_ > kMaximumBob)
        player->bob_factor_ = kMaximumBob;

    // ----CALCULATE BOB EFFECT----
    if (player->player_state_ == kPlayerAlive && onground)
    {
        BAMAngle angle = kBAMAngle90 / 5 * level_time_elapsed;

        bob_z = player->bob_factor_ / 2 * player->map_object_->info_->bobbing_ * epi::BAMSin(angle);
    }

    // ----CALCULATE VIEWHEIGHT----
    if (player->player_state_ == kPlayerAlive)
    {
        player->view_height_ += player->delta_view_height_;

        if (player->view_height_ > player->standard_view_height_)
        {
            player->view_height_       = player->standard_view_height_;
            player->delta_view_height_ = 0;
        }
        else if (sink_mult < 1.0f && player->view_height_ < player->standard_view_height_ * sink_mult)
        {
            player->view_height_ = player->standard_view_height_ * sink_mult;
            if (player->delta_view_height_ <= 0)
                player->delta_view_height_ = 0.01f;
        }
        else
        {
            float thresh = player->standard_view_height_ / 2;
            if (sink_mult < 1.0f)
                thresh = HMM_MIN(thresh, player->standard_view_height_ * sink_mult);
            if (player->view_height_ < thresh)
            {
                player->view_height_ = thresh;
                if (player->delta_view_height_ <= 0)
                    player->delta_view_height_ = 0.01f;
            }
        }

        if (!AlmostEquals(player->delta_view_height_, 0.0f))
        {
            // use a weird number to minimise chance of hitting
            // zero when delta_view_height_ goes neg -> positive.
            player->delta_view_height_ += 0.24162f;
        }
    }

    //----CALCULATE FREEFALL EFFECT, WITH SOUND EFFECTS (code based on HEXEN)
    //  CORBIN, on:
    //  6/6/2011 - Fix this so RTS does NOT interfere with fracunits (it does in
    //  Hypertension's E1M1 starting script)! 6/7/2011 - Ajaped said to remove
    //  FRACUNIT...seeya oldness.

    if (player->map_object_->info_->falling_sound_ && player->health_ > 0)
    {
        if ((player->map_object_->momentum_.Z <= -35.0) && (player->map_object_->momentum_.Z >= -36.0))
        {
            if (!AlmostEquals(player->map_object_->floor_z_, -32768.0f))
            {
                int sfx_cat;

                if (player == players[console_player])
                {
                    sfx_cat = kCategoryPlayer;
                }
                else
                {
                    sfx_cat = kCategoryOpponent;
                }
                StartSoundEffect(player->map_object_->info_->falling_sound_, sfx_cat, player->map_object_);
            }
        }
    }

    // don't apply bobbing when jumping, but have a smooth
    // transition at the end of the jump.
    if (player->jump_wait_ > 0)
    {
        if (player->jump_wait_ >= 6)
            bob_z = 0;
        else
            bob_z *= (6 - player->jump_wait_) / 6.0;
    }

    if (view_bobbing.d_ > 1)
        bob_z = 0;

    player->view_z_ = player->view_height_ + bob_z;
}

void PlayerJump(Player *pl, float dz, int wait)
{
    pl->map_object_->momentum_.Z += dz;

    if (pl->jump_wait_ < wait)
        pl->jump_wait_ = wait;

    // enter the JUMP states (if present)
    int jump_st = MapObjectFindLabel(pl->map_object_, "JUMP");
    if (jump_st != 0)
        MapObjectSetStateDeferred(pl->map_object_, jump_st, 0);

    // -AJA- 1999/09/11: New JUMP_SOUND for ddf.
    if (pl->map_object_->info_->jump_sound_)
    {
        int sfx_cat;

        if (pl == players[console_player])
            sfx_cat = kCategoryPlayer;
        else
            sfx_cat = kCategoryOpponent;

        StartSoundEffect(pl->map_object_->info_->jump_sound_, sfx_cat, pl->map_object_);
    }
}

static void MovePlayer(Player *player)
{
    EventTicCommand *cmd;
    MapObject       *mo = player->map_object_;

    bool onground = player->map_object_->z <= player->map_object_->floor_z_;
    bool onladder = player->map_object_->on_ladder_ >= 0;

    bool swimming  = player->swimming_;
    bool flying    = (player->powers_[kPowerTypeJetpack] > 0) && !swimming;
    bool jumping   = (player->jump_wait_ > 0);
    bool crouching = (player->map_object_->extended_flags_ & kExtendedFlagCrouching) ? true : false;

    float dx, dy;
    float eh, ev;

    float base_xy_speed;
    float base_z_speed;

    float F_vec[3], U_vec[3], S_vec[3];

    cmd = &player->command_;

    if (player->zoom_field_of_view_ > 0)
        cmd->angle_turn /= kZoomAngleDivisor;

    player->map_object_->angle_ -= (BAMAngle)(cmd->angle_turn << 16);

    // EDGE Feature: Vertical Look (Mlook)
    //
    // -ACB- 1998/07/02 New Code used, rerouted via Ticcmd
    // -ACB- 1998/07/27 Used defines for look limits.
    //
    if (level_flags.mouselook)
    {
        if (player->zoom_field_of_view_ > 0)
            cmd->mouselook_turn /= kZoomAngleDivisor;

        BAMAngle V = player->map_object_->vertical_angle_ + (BAMAngle)(cmd->mouselook_turn << 16);

        if (V < kBAMAngle180 && V > kMouseLookLimit)
            V = kMouseLookLimit;
        else if (V >= kBAMAngle180 && V < (kBAMAngle360 - kMouseLookLimit))
            V = (kBAMAngle360 - kMouseLookLimit);

        player->map_object_->vertical_angle_ = V;
    }
    else
    {
        player->map_object_->vertical_angle_ = 0;
    }

    // EDGE Feature: Vertical Centering
    //
    // -ACB- 1998/07/02 Re-routed via Ticcmd
    //
    if (cmd->extended_buttons & kExtendedButtonCodeCenter)
        player->map_object_->vertical_angle_ = 0;

    // compute XY and Z speeds, taking swimming (etc) into account
    // (we try to swim in view direction -- assumes no gravity).

    base_xy_speed = player->map_object_->speed_ / 32.0f;
    base_z_speed  = player->map_object_->speed_ / 64.0f;

    // Do not let the player control movement if not onground.
    // -MH- 1998/06/18  unless he has the JetPack!

    if (!(onground || onladder || swimming || flying))
        base_xy_speed /= 16.0f;

    if (!(onladder || swimming || flying))
        base_z_speed /= 16.0f;

    // move slower when crouching
    if (crouching)
        base_xy_speed *= kCrouchSlowdown;

    dx = epi::BAMCos(player->map_object_->angle_);
    dy = epi::BAMSin(player->map_object_->angle_);

    eh = 1;
    ev = 0;

    if (swimming || flying || onladder)
    {
        float slope = epi::BAMTan(player->map_object_->vertical_angle_);

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

    float fric;
    float factor;

    if (mo->flags_ & kMapObjectFlagNoClip)
    {
        fric   = kFrictionDefault;
        factor = 1.0f;
    }
    else
    {
        fric           = -1.0f;
        factor         = -1.0f;
        Sector *sector = player->map_object_->subsector_->sector;

        for (TouchNode *tn = mo->touch_sectors_; tn; tn = tn->map_object_next)
        {
            if (tn->sector)
            {
                float sec_fh =
                    (tn->sector->floor_vertex_slope && sector == tn->sector) ? mo->floor_z_ : tn->sector->floor_height;
                if (mo->z > sec_fh)
                    continue;
                if (fric < 0.0f || tn->sector->properties.friction < fric)
                {
                    fric   = tn->sector->properties.friction;
                    factor = tn->sector->properties.movefactor;
                }
            }
        }

        if (fric < 0.0f || AlmostEquals(fric, kFrictionDefault))
            fric = kFrictionDefault;
        else if (fric > kFrictionDefault)
            fric *= factor;
        else
        {
            float velocity = mo->player_->actual_speed_;
            if (velocity > kFootingFactor)
                factor *= 8;
            else if (velocity > kFootingFactor / 2)
                factor *= 4;
            else if (velocity > kFootingFactor / 4)
                factor *= 2;
            fric *= factor;
        }
    }

    fric = HMM_Clamp(0.0f, fric, 1.0f);

    player->map_object_->momentum_.X +=
        (F_vec[0] * cmd->forward_move + S_vec[0] * cmd->side_move + U_vec[0] * cmd->upward_move) * fric;

    player->map_object_->momentum_.Y +=
        (F_vec[1] * cmd->forward_move + S_vec[1] * cmd->side_move + U_vec[1] * cmd->upward_move) * fric;

    if (flying || swimming || !onground || onladder)
    {
        player->map_object_->momentum_.Z +=
            F_vec[2] * cmd->forward_move + S_vec[2] * cmd->side_move + U_vec[2] * cmd->upward_move;
    }

    if (flying && !swimming)
    {
        int sfx_cat;

        if (player == players[console_player])
            sfx_cat = kCategoryPlayer;
        else
            sfx_cat = kCategoryOpponent;

        if (player->powers_[kPowerTypeJetpack] <= (5 * kTicRate))
        {
            if ((level_time_elapsed & 10) == 0)
                StartSoundEffect(sfx_jpflow, sfx_cat,
                                 player->map_object_); // fuel low
        }
        else if (cmd->upward_move > 0)
            StartSoundEffect(sfx_jprise, sfx_cat, player->map_object_);
        else if (cmd->upward_move < 0)
            StartSoundEffect(sfx_jpdown, sfx_cat, player->map_object_);
        else if (cmd->forward_move || cmd->side_move)
            StartSoundEffect((onground ? sfx_jpidle : sfx_jpmove), sfx_cat, player->map_object_);
        else
            StartSoundEffect(sfx_jpidle, sfx_cat, player->map_object_);
    }

    if (player->map_object_->state_ == &states[player->map_object_->info_->idle_state_])
    {
        if (!jumping && !flying && (onground || swimming) && (cmd->forward_move || cmd->side_move))
        {
            // enter the CHASE (i.e. walking) states
            if (player->map_object_->info_->chase_state_)
                MapObjectSetStateDeferred(player->map_object_, player->map_object_->info_->chase_state_, 0);
        }
    }

    // EDGE Feature: Jump Code
    //
    // -ACB- 1998/08/09 Check that jumping is allowed in the current_map
    //                  Make player pause before jumping again

    if (level_flags.jump && mo->info_->jumpheight_ > 0 && (cmd->upward_move > 4))
    {
        if (!jumping && !crouching && !swimming && !flying && onground && !onladder)
        {
            PlayerJump(player, player->map_object_->info_->jumpheight_ / 1.4f, player->map_object_->info_->jump_delay_);
        }
    }

    // EDGE Feature: Crouching

    if (level_flags.crouch && mo->info_->crouchheight_ > 0 && (player->command_.upward_move < -4) &&
        !player->wet_feet_ && !jumping && onground)
    // NB: no ladder check, onground is sufficient
    {
        if (mo->height_ > mo->info_->crouchheight_)
        {
            mo->height_                     = HMM_MAX(mo->height_ - 2.0f, mo->info_->crouchheight_);
            mo->player_->delta_view_height_ = -1.0f;
        }
    }
    else // STAND UP
    {
        if (mo->height_ < mo->info_->height_)
        {
            float new_height = HMM_MIN(mo->height_ + 2, mo->info_->height_);

            // prevent standing up inside a solid area
            if ((mo->flags_ & kMapObjectFlagNoClip) || mo->z + new_height <= mo->ceiling_z_)
            {
                mo->height_                     = new_height;
                mo->player_->delta_view_height_ = 1.0f;
            }
        }
    }

    // EDGE Feature: Zooming
    //
    if (cmd->extended_buttons & kExtendedButtonCodeZoom)
    {
        int fov = 0;

        if (player->zoom_field_of_view_ == 0)
        {
            if (!(player->ready_weapon_ < 0 || player->pending_weapon_ >= 0))
                fov = player->weapons_[player->ready_weapon_].info->zoom_fov_;

            if (fov == int(kBAMAngle360))
                fov = 0;
        }

        player->zoom_field_of_view_ = fov;
    }
}

static void DeathThink(Player *player)
{
    // fall on your face when dying.

    float dx, dy, dz;

    BAMAngle angle;
    BAMAngle delta, delta_s;
    float    slope;

    // -AJA- 1999/12/07: don't die mid-air.
    player->powers_[kPowerTypeJetpack] = 0;

    MovePlayerSprites(player);

    // fall to the ground
    if (player->view_height_ > player->standard_view_height_)
        player->view_height_ -= 1.0f;
    else if (player->view_height_ < player->standard_view_height_)
        player->view_height_ = player->standard_view_height_;

    player->delta_view_height_ = 0.0f;
    player->kick_offset_       = 0.0f;

    CalcHeight(player);

    if (player->attacker_ && player->attacker_ != player->map_object_)
    {
        dx = player->attacker_->x - player->map_object_->x;
        dy = player->attacker_->y - player->map_object_->y;
        dz = (player->attacker_->z + player->attacker_->height_ / 2) - (player->map_object_->z + player->view_height_);

        angle = PointToAngle(0, 0, dx, dy);
        delta = angle - player->map_object_->angle_;

        slope   = ApproximateSlope(dx, dy, dz);
        slope   = HMM_MIN(1.7f, HMM_MAX(-1.7f, slope));
        delta_s = epi::BAMFromATan(slope) - player->map_object_->vertical_angle_;

        if ((delta <= kBAMAngle1 / 2 || delta >= (BAMAngle)(0 - kBAMAngle1 / 2)) &&
            (delta_s <= kBAMAngle1 / 2 || delta_s >= (BAMAngle)(0 - kBAMAngle1 / 2)))
        {
            // Looking at killer, so fade damage flash down.
            player->map_object_->angle_          = angle;
            player->map_object_->vertical_angle_ = epi::BAMFromATan(slope);

            if (player->damage_count_ > 0)
                player->damage_count_--;
        }
        else
        {
            if (delta < kBAMAngle180)
                delta /= 5;
            else
                delta = (BAMAngle)(0 - (BAMAngle)(0 - delta) / 5);

            if (delta > kBAMAngle5 && delta < (BAMAngle)(0 - kBAMAngle5))
                delta = (delta < kBAMAngle180) ? kBAMAngle5 : (BAMAngle)(0 - kBAMAngle5);

            if (delta_s < kBAMAngle180)
                delta_s /= 5;
            else
                delta_s = (BAMAngle)(0 - (BAMAngle)(0 - delta_s) / 5);

            if (delta_s > (kBAMAngle5 / 2) && delta_s < (BAMAngle)(0 - kBAMAngle5 / 2))
                delta_s = (delta_s < kBAMAngle180) ? (kBAMAngle5 / 2) : (BAMAngle)(0 - kBAMAngle5 / 2);

            player->map_object_->angle_ += delta;
            player->map_object_->vertical_angle_ += delta_s;

            if (player->damage_count_ && (level_time_elapsed % 3) == 0)
                player->damage_count_--;
        }
    }
    else if (player->damage_count_ > 0)
        player->damage_count_--;

    // -AJA- 1999/08/07: Fade out armor points too.
    if (player->bonus_count_)
        player->bonus_count_--;

    UpdatePowerups(player);

    // lose the zoom when dead
    player->zoom_field_of_view_ = 0;

    if (deathmatch >= 3 && player->map_object_->move_count_ > player->map_object_->info_->respawntime_)
        return;

    if (player->command_.buttons & kButtonCodeUse)
        player->player_state_ = kPlayerAwaitingRespawn;
}

static void UpdatePowerups(Player *player)
{
    float limit = FLT_MAX;
    int   pw;

    if (player->player_state_ == kPlayerDead)
        limit = 1; // kTicRate * 5;

    for (pw = 0; pw < kTotalPowerTypes; pw++)
    {
        if (player->powers_[pw] < 0) // -ACB- 2004/02/04 Negative values last a level
            continue;

        float &qty_r = player->powers_[pw];

        if (qty_r > limit)
            qty_r = limit;
        else if (qty_r > 1)
            qty_r -= 1;
        else if (qty_r > 0)
        {
            if (player->keep_powers_ & (1 << pw))
                qty_r = -1;
            else
                qty_r = 0;
        }
    }

    if (player->powers_[kPowerTypePartInvisTranslucent] > 0)
    {
        if (player->powers_[kPowerTypePartInvisTranslucent] >= 128 ||
            fmod(player->powers_[kPowerTypePartInvisTranslucent], 16) >= 8)
            player->map_object_->flags_ |= kMapObjectFlagFuzzy;
        else
            player->map_object_->flags_ &= ~kMapObjectFlagFuzzy;
    }
    else
    {
        if (player->powers_[kPowerTypePartInvis] >= 128 || fmod(player->powers_[kPowerTypePartInvis], 16) >= 8)
            player->map_object_->flags_ |= kMapObjectFlagFuzzy;
        else
            player->map_object_->flags_ &= ~kMapObjectFlagFuzzy;
    }

    // Handling colormaps.
    //
    // -AJA- 1999/07/10: Updated for colmap.ddf.
    //
    // !!! FIXME: overlap here with stuff in rgl_fx.cpp.

    player->effect_colourmap_ = nullptr;
    player->effect_left_      = 0;

    if (player->powers_[kPowerTypeInvulnerable] > 0)
    {
        float s = player->powers_[kPowerTypeInvulnerable];

        // -ACB- FIXME!!! Catch lookup failure!
        player->effect_colourmap_ = colormaps.Lookup("ALLWHITE");
        player->effect_left_      = (s <= 0) ? 0 : HMM_MIN(int(s), kMaximumEffectTime);
    }
    else if (player->powers_[kPowerTypeInfrared] > 0)
    {
        float s = player->powers_[kPowerTypeInfrared];

        player->effect_left_ = (s <= 0) ? 0 : HMM_MIN(int(s), kMaximumEffectTime);
    }
    else if (player->powers_[kPowerTypeNightVision] > 0) // -ACB- 1998/07/15 NightVision Code
    {
        float s = player->powers_[kPowerTypeNightVision];

        // -ACB- FIXME!!! Catch lookup failure!
        player->effect_colourmap_ = colormaps.Lookup("ALLGREEN");
        player->effect_left_      = (s <= 0) ? 0 : HMM_MIN(int(s), kMaximumEffectTime);
    }
    else if (player->powers_[kPowerTypeBerserk] > 0) // Lobo 2021: Un-Hardcode Berserk colour tint
    {
        float s = player->powers_[kPowerTypeBerserk];

        player->effect_colourmap_ = colormaps.Lookup("BERSERK");
        player->effect_left_      = (s <= 0) ? 0 : HMM_MIN(int(s), kMaximumEffectTime);
    }
}

// Does the thinking of the console player, i.e. read from input
void ConsolePlayerBuilder(const Player *pl, void *data, EventTicCommand *dest)
{
    EPI_UNUSED(data);
    BuildEventTicCommand(dest);

    dest->player_index = pl->player_number_;
}

bool PlayerSwitchWeapon(Player *player, WeaponDefinition *choice)
{
    int pw_index;

    // see if player owns this kind of weapon
    for (pw_index = 0; pw_index < kMaximumWeapons; pw_index++)
    {
        if (!player->weapons_[pw_index].owned)
            continue;

        if (player->weapons_[pw_index].info == choice)
            break;
    }

    if (pw_index == kMaximumWeapons)
        return false;

    // ignore this choice if it the same as the current weapon
    if (player->ready_weapon_ >= 0 && choice == player->weapons_[player->ready_weapon_].info)
    {
        return false;
    }

    player->pending_weapon_ = (WeaponSelection)pw_index;

    return true;
}

void P_DumpMobjsTemp(void)
{
    MapObject *mo;

    int index = 0;

    LogWarning("MOBJs:\n");

    for (mo = map_object_list_head; mo; mo = mo->next_, index++)
    {
        LogWarning(" %4d: %p next:%p prev:%p [%s] at (%1.0f,%1.0f,%1.0f) states=%d > "
                   "%d tics=%d\n",
                   index, mo, mo->next_, mo->previous_, mo->info_->name_.c_str(), mo->x, mo->y, mo->z,
                   (int)(mo->state_ ? mo->state_ - states : -1), (int)(mo->next_state_ ? mo->next_state_ - states : -1),
                   mo->tics_);
    }

    LogWarning("END OF MOBJs\n");
}

bool PlayerThink(Player *player)
{
    EventTicCommand *cmd = &player->command_;

    EPI_ASSERT(player->map_object_);

    player->map_object_->interpolate_        = true;
    player->map_object_->old_x_              = player->map_object_->x;
    player->map_object_->old_y_              = player->map_object_->y;
    player->map_object_->old_z_              = player->map_object_->z;
    player->map_object_->old_angle_          = player->map_object_->angle_;
    player->map_object_->old_vertical_angle_ = player->map_object_->vertical_angle_;

    player->old_view_z_ = player->view_z_;

    bool should_think = true;

    if (player->attacker_ && player->attacker_->IsRemoved())
    {
        P_DumpMobjsTemp();
        FatalError("INTERNAL ERROR: player has a removed attacker. \n");
    }

    if (player->damage_count_ <= 0)
        player->damage_pain_ = 0;

    // fixme: do this in the cheat code
    if (player->cheats_ & kCheatingNoClip)
        player->map_object_->flags_ |= kMapObjectFlagNoClip;
    else
        player->map_object_->flags_ &= ~kMapObjectFlagNoClip;

    // chain saw run forward
    if (player->map_object_->flags_ & kMapObjectFlagJustAttacked)
    {
        cmd->angle_turn   = 0;
        cmd->forward_move = 64;
        cmd->side_move    = 0;
        player->map_object_->flags_ &= ~kMapObjectFlagJustAttacked;
    }

    if (player->player_state_ == kPlayerDead)
    {
        DeathThink(player);
        if (player->map_object_->region_properties_->special &&
            player->map_object_->region_properties_->special->e_exit_ != kExitTypeNone)
        {
            ExitType do_exit = player->map_object_->region_properties_->special->e_exit_;

            player->map_object_->subsector_->sector->properties.special = nullptr;

            if (do_exit == kExitTypeSecret)
                ExitLevelSecret(1);
            else
                ExitLevel(1);
        }
        return true;
    }

    // Move/Look around.  Reactiontime is used to prevent movement for a
    // bit after a teleport.

    if (player->map_object_->reaction_time_)
        player->map_object_->reaction_time_--;

    if (player->map_object_->reaction_time_ == 0)
        MovePlayer(player);

    CalcHeight(player);

    if (erraticism.d_)
    {
        bool    sinking = false;
        Sector *cur_sec = player->map_object_->subsector_->sector;
        if (!cur_sec->extrafloor_used && !cur_sec->height_sector && cur_sec->sink_depth > 0 &&
            player->map_object_->z <= player->map_object_->floor_z_)
            sinking = true;
        if (cmd->forward_move == 0 && cmd->side_move == 0 && !player->swimming_ && cmd->upward_move <= 0 &&
            !(cmd->buttons &
              (kButtonCodeAttack | kButtonCodeUse | kButtonCodeChangeWeapon | kExtendedButtonCodeSecondAttack |
               kExtendedButtonCodeReload | kExtendedButtonCodeAction1 | kExtendedButtonCodeAction2 |
               kExtendedButtonCodeInventoryUse | kExtendedButtonCodeThirdAttack | kExtendedButtonCodeFourthAttack)) &&
            ((AlmostEquals(player->map_object_->height_, player->map_object_->info_->height_) ||
              AlmostEquals(player->map_object_->height_, player->map_object_->info_->crouchheight_)) &&
             (AlmostEquals(player->delta_view_height_, 0.0f) || sinking)))
        {
            should_think = false;
            if (!player->map_object_->momentum_.Z)
            {
                player->map_object_->momentum_.X = 0;
                player->map_object_->momentum_.Y = 0;
            }
        }
    }

    // Reset environmental FX in case player has left sector in which they apply
    // - Dasho
    vacuum_sound_effects    = false;
    submerged_sound_effects = false;

    if (player->map_object_->region_properties_->special ||
        player->map_object_->subsector_->sector->extrafloor_used > 0 || player->underwater_ || player->swimming_ ||
        player->airless_)
    {
        PlayerInSpecialSector(player, player->map_object_->subsector_->sector, should_think);
    }

    // Check for weapon change.
    if (cmd->buttons & kButtonCodeChangeWeapon)
    {
        // The actual changing of the weapon is done when the weapon
        // psprite can do it (read: not in the middle of an attack).

        int key = (cmd->buttons & kButtonCodeWeaponMask) >> kButtonCodeWeaponMaskShift;

        if (key == kButtonCodeNextWeapon)
        {
            CycleWeapon(player, +1);
        }
        else if (key == kButtonCodePreviousWeapon)
        {
            CycleWeapon(player, -1);
        }
        else /* numeric key */
        {
            DesireWeaponChange(player, key);
        }
    }

    // check for use
    if (cmd->buttons & kButtonCodeUse)
    {
        if (!player->use_button_down_)
        {
            UseLines(player);
            player->use_button_down_ = true;
        }
    }
    else
    {
        player->use_button_down_ = false;
    }

    player->action_button_down_[0] = (cmd->extended_buttons & kExtendedButtonCodeAction1) ? true : false;
    player->action_button_down_[1] = (cmd->extended_buttons & kExtendedButtonCodeAction2) ? true : false;
#ifdef EDGE_CLASSIC
    if (LuaUseLuaHUD())
        LuaSetVector3(LuaGetGlobalVM(), "player", "inventory_event_handler",
                      HMM_Vec3{{cmd->extended_buttons & kExtendedButtonCodeInventoryPrevious ? 1.0f : 0.0f,
                                cmd->extended_buttons & kExtendedButtonCodeInventoryUse ? 1.0f : 0.0f,
                                cmd->extended_buttons & kExtendedButtonCodeInventoryNext ? 1.0f : 0.0f}});
    else
        COALSetVector(ui_vm, "player", "inventory_event_handler",
                      cmd->extended_buttons & kExtendedButtonCodeInventoryPrevious ? 1 : 0,
                      cmd->extended_buttons & kExtendedButtonCodeInventoryUse ? 1 : 0,
                      cmd->extended_buttons & kExtendedButtonCodeInventoryNext ? 1 : 0);
#else
    LuaSetVector3(LuaGetGlobalVM(), "player", "inventory_event_handler",
                  HMM_Vec3{{cmd->extended_buttons & kExtendedButtonCodeInventoryPrevious ? 1.0f : 0.0f,
                            cmd->extended_buttons & kExtendedButtonCodeInventoryUse ? 1.0f : 0.0f,
                            cmd->extended_buttons & kExtendedButtonCodeInventoryNext ? 1.0f : 0.0f}});
#endif
    // decrement jump_wait_ counter
    if (player->jump_wait_ > 0)
        player->jump_wait_--;

    if (player->splash_wait_ > 0)
        player->splash_wait_--;

    // cycle psprites
    MovePlayerSprites(player);

    // Counters, time dependend power ups.

    UpdatePowerups(player);

    if (player->damage_count_ > 0)
        player->damage_count_--;

    if (player->bonus_count_ > 0)
        player->bonus_count_--;

    if (player->grin_count_ > 0)
        player->grin_count_--;

    if (player->action_button_down_[0] || player->action_button_down_[1])
        player->attack_sustained_count_++;
    else
        player->attack_sustained_count_ = 0;

    player->kick_offset_ /= 1.6f;

    // Adjust reverb node parameters if applicable
    if (players[console_player] == player)
    {
        if (pc_speaker_mode)
            sector_reverb = false;
        else if (player->map_object_->subsector_->sector->sound_reverb)
        {
            sector_reverb = true;
            player->map_object_->subsector_->sector->sound_reverb->ApplyReverb(&reverb_node);
        }
        else if (dynamic_reverb.d_)
        {
            sector_reverb = false;
            HMM_Vec2 room_checker;
            float    room_check = 0;
            float    player_x   = player->map_object_->x;
            float    player_y   = player->map_object_->y;
            PathTraverse(player_x, player_y, player_x, 32768.0f, kPathAddLines, P_RoomPath, &room_checker);
            room_check += abs(room_checker.Y - player_y);
            PathTraverse(player_x, player_y, 32768.0f + player_x, 32768.0f + player_y, kPathAddLines, P_RoomPath,
                         &room_checker);
            room_check += PointToDistance(player_x, player_y, room_checker.X, room_checker.Y);
            PathTraverse(player_x, player_y, -32768.0f + player_x, 32768.0f + player_y, kPathAddLines, P_RoomPath,
                         &room_checker);
            room_check += PointToDistance(player_x, player_y, room_checker.X, room_checker.Y);
            PathTraverse(player_x, player_y, player_x, -32768.0f, kPathAddLines, P_RoomPath, &room_checker);
            room_check += abs(player_y - room_checker.Y);
            PathTraverse(player_x, player_y, -32768.0f + player_x, -32768.0f + player_y, kPathAddLines, P_RoomPath,
                         &room_checker);
            room_check += PointToDistance(player_x, player_y, room_checker.X, room_checker.Y);
            PathTraverse(player_x, player_y, 32768.0f + player_x, -32768.0f + player_y, kPathAddLines, P_RoomPath,
                         &room_checker);
            room_check += PointToDistance(player_x, player_y, room_checker.X, room_checker.Y);
            PathTraverse(player_x, player_y, -32768.0f, player_y, kPathAddLines, P_RoomPath, &room_checker);
            room_check += abs(player_x - room_checker.X);
            PathTraverse(player_x, player_y, 32768.0f, player_y, kPathAddLines, P_RoomPath, &room_checker);
            room_check += abs(room_checker.X - player_x);
            room_check *= 0.125f;
            if (EDGE_IMAGE_IS_SKY(player->map_object_->subsector_->sector->ceiling))
            {
                if (dynamic_reverb.d_ == 1) // Headphones
                    ddf::ReverbDefinition::kOutdoorWeak.ApplyReverb(&reverb_node);
                else                        // Speakers
                    ddf::ReverbDefinition::kOutdoorStrong.ApplyReverb(&reverb_node);
                if (room_check < 700)
                {
                    float new_room_size;
                    if (room_check > 350)
                        new_room_size = 0.3f;
                    else
                        new_room_size = 0.2f;
                    ma_freeverb_update_verb(&reverb_node, &new_room_size, NULL, NULL, NULL, NULL, NULL);
                }
            }
            else
            {
                if (dynamic_reverb.d_ == 1) // Headphones
                    ddf::ReverbDefinition::kIndoorWeak.ApplyReverb(&reverb_node);
                else                        // Speakers
                    ddf::ReverbDefinition::kIndoorStrong.ApplyReverb(&reverb_node);
                if (room_check < 700)
                {
                    float new_room_size;
                    if (room_check > 350)
                        new_room_size = 0.2f;
                    else
                        new_room_size = 0.1f;
                    ma_freeverb_update_verb(&reverb_node, &new_room_size, NULL, NULL, NULL, NULL, NULL);
                }
            }
        }
        else
            sector_reverb = false; // keep sound from being hooked up to the reverb node
    }

    return should_think;
}

void CreatePlayer(int pnum, bool is_bot)
{
    EPI_ASSERT(0 <= pnum && pnum < kMaximumPlayers);
    EPI_ASSERT(players[pnum] == nullptr);

    Player *p = new Player;

    EPI_CLEAR_MEMORY(p, Player, 1);

    p->player_number_ = pnum;
    p->player_state_  = kPlayerDead;

    players[pnum] = p;

    total_players++;
    if (is_bot)
        total_bots++;

    // determine name
    char namebuf[32];
    stbsp_sprintf(namebuf, "Player%dName", pnum + 1);

    if (language.IsValidRef(namebuf))
    {
        strncpy(p->player_name_, language[namebuf], kPlayerNameCharacterLimit - 1);
        p->player_name_[kPlayerNameCharacterLimit - 1] = '\0';
    }
    else
    {
        // -ES- Default to player##
        stbsp_sprintf(p->player_name_, "Player%d", pnum + 1);
    }

    if (is_bot)
        CreateBotPlayer(p, false);

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
    for (int pnum = 0; pnum < kMaximumPlayers; pnum++)
    {
        if (!players[pnum])
            continue;

        delete players[pnum];

        players[pnum] = nullptr;
    }

    total_players = 0;
    total_bots    = 0;

    console_player = -1;
    display_player = -1;

    sfx_jpidle = sfx_jpmove = sfx_jprise = nullptr;
    sfx_jpdown = sfx_jpflow = nullptr;
}

void UpdateAvailWeapons(Player *p)
{
    // Must be called as soon as the player has received or lost
    // a weapon.  Updates the status bar icons.

    int key;

    for (key = 0; key < kTotalWeaponKeys; key++)
        p->available_weapons_[key] = false;

    for (int i = 0; i < kMaximumWeapons; i++)
    {
        if (!p->weapons_[i].owned)
            continue;

        EPI_ASSERT(p->weapons_[i].info);

        key = p->weapons_[i].info->bind_key_;

        // update the status bar icons
        if (0 <= key && key <= 9)
            p->available_weapons_[key] = true;
    }
}

void UpdateTotalArmour(Player *p)
{
    int i;

    p->total_armour_ = 0;

    for (i = 0; i < kTotalArmourTypes; i++)
    {
        p->total_armour_ += p->armours_[i];

        // forget the association once fully depleted
        if (p->armours_[i] <= 0)
            p->armour_types_[i] = nullptr;
    }

    if (p->total_armour_ > 999.0f)
        p->total_armour_ = 999.0f;
}

bool AddWeapon(Player *player, WeaponDefinition *info, int *index)
{
    // Returns true if player did not already have the weapon.
    // If successful and 'index' is non-nullptr, the new index is
    // stored there.

    int slot         = -1;
    int upgrade_slot = -1;

    // cannot own weapons if sprites are missing
    if (!CheckWeaponSprite(info))
    {
        LogWarning("WEAPON %s has no sprites and will not be added!\n", info->name_.c_str());
        return false;
    }

    for (int i = 0; i < kMaximumWeapons; i++)
    {
        WeaponDefinition *cur_info = player->weapons_[i].info;

        // skip weapons that are being removed
        if (player->weapons_[i].flags & kPlayerWeaponRemoving)
            continue;

        // find free slot
        if (!player->weapons_[i].owned)
        {
            if (slot < 0)
                slot = i;
            continue;
        }

        // check if already own this weapon
        if (cur_info == info)
            return false;

        // don't downgrade any UPGRADED weapons
        if (DDFWeaponIsUpgrade(cur_info, info))
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

    LogDebug("AddWeapon: [%s] @ %d\n", info->name_.c_str(), slot);

    player->weapons_[slot].owned        = true;
    player->weapons_[slot].info         = info;
    player->weapons_[slot].flags        = kPlayerWeaponNoFlag;
    player->weapons_[slot].clip_size[0] = 0;
    player->weapons_[slot].clip_size[1] = 0;
    player->weapons_[slot].clip_size[2] = 0;
    player->weapons_[slot].clip_size[3] = 0;
    player->weapons_[slot].model_skin   = info->model_skin_;

    UpdateAvailWeapons(player);

    // for NoAmmo+Clip weapons, always begin with a full clip
    for (int ATK = 0; ATK < 4; ATK++)
    {
        if (info->clip_size_[ATK] > 0 && info->ammo_[ATK] == kAmmunitionTypeNoAmmo)
            player->weapons_[slot].clip_size[ATK] = info->clip_size_[ATK];
    }

    // initial weapons should get a full clip
    if (info->autogive_)
        TryFillNewWeapon(player, slot, kAmmunitionTypeDontCare, nullptr);

    if (upgrade_slot >= 0)
    {
        player->weapons_[upgrade_slot].owned = false;

        // check and update key_choices[]
        for (int w = 0; w <= 9; w++)
            if (player->key_choices_[w] == upgrade_slot)
                player->key_choices_[w] = (WeaponSelection)slot;

        // handle the case of holding the weapon which is being upgraded
        // by the new one.  We mark the old weapon for removal.

        if (player->ready_weapon_ == upgrade_slot)
        {
            player->weapons_[upgrade_slot].flags =
                (PlayerWeaponFlag)(player->weapons_[upgrade_slot].flags | kPlayerWeaponRemoving);
            player->pending_weapon_ = (WeaponSelection)slot;
        }
        else
            player->weapons_[upgrade_slot].info = nullptr;

        if (player->pending_weapon_ == upgrade_slot)
            player->pending_weapon_ = (WeaponSelection)slot;
    }

    return true;
}

bool RemoveWeapon(Player *player, WeaponDefinition *info)
{
    // returns true if player had the weapon.

    int slot;

    for (slot = 0; slot < kMaximumWeapons; slot++)
    {
        if (!player->weapons_[slot].owned)
            continue;

        // Note: no need to check kPlayerWeaponRemoving

        if (player->weapons_[slot].info == info)
            break;
    }

    if (slot >= kMaximumWeapons)
        return false;

    LogDebug("RemoveWeapon: [%s] @ %d\n", info->name_.c_str(), slot);

    player->weapons_[slot].owned = false;

    UpdateAvailWeapons(player);

    // fix the key choices
    for (int w = 0; w <= 9; w++)
        if (player->key_choices_[w] == slot)
            player->key_choices_[w] = KWeaponSelectionNone;

    // handle the case of already holding the weapon.  We mark the
    // weapon as being removed (the flag is cleared once lowered).

    if (player->ready_weapon_ == slot)
    {
        player->weapons_[slot].flags = (PlayerWeaponFlag)(player->weapons_[slot].flags | kPlayerWeaponRemoving);

        if (player->pending_weapon_ == KWeaponSelectionNoChange)
            DropWeapon(player);
    }
    else
        player->weapons_[slot].info = nullptr;

    if (player->pending_weapon_ == slot)
        SelectNewWeapon(player, -100, kAmmunitionTypeDontCare);

    EPI_ASSERT(player->pending_weapon_ != slot);

    return true;
}

void GiveInitialBenefits(Player *p, const MapObjectDefinition *info)
{
    // Give the player the initial benefits when they start a game
    // (or restart after dying).  Sets up: ammo, ammo-limits, health,
    // armour, keys and weapons.

    p->ready_weapon_   = KWeaponSelectionNone;
    p->pending_weapon_ = KWeaponSelectionNoChange;

    int i;

    for (i = 0; i < kTotalWeaponKeys; i++)
        p->key_choices_[i] = KWeaponSelectionNone;

    // clear out ammo & ammo-limits
    for (i = 0; i < kTotalAmmunitionTypes; i++)
    {
        p->ammo_[i].count = p->ammo_[i].maximum = 0;
    }

    // clear out inventory & inventory-limits
    for (i = 0; i < kTotalInventoryTypes; i++)
    {
        p->inventory_[i].count = p->inventory_[i].maximum = 0;
    }

    // clear out counter & counter-limits
    for (i = 0; i < kTotalCounterTypes; i++)
    {
        p->counters_[i].count = p->counters_[i].maximum = 0;
    }

    // set health and armour
    p->health_       = info->spawn_health_;
    p->air_in_lungs_ = info->lung_capacity_;
    p->underwater_   = false;
    p->airless_      = false;

    for (i = 0; i < kTotalArmourTypes; i++)
    {
        p->armours_[i]      = 0;
        p->armour_types_[i] = nullptr;
    }

    p->total_armour_ = 0;
    p->cards_        = kDoorKeyNone;

    // give all initial benefits
    GiveBenefitList(p, nullptr, info->initial_benefits_, false);

    // give all free weapons.  Needs to be after ammo, so that
    // clip weapons can get their clips filled.
    for (WeaponDefinition *w : weapondefs)
    {
        if (!w->autogive_)
            continue;

        int pw_index;

        AddWeapon(p, w, &pw_index);
    }

    // refresh to remove all stuff from status bar
    UpdateAvailWeapons(p);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
