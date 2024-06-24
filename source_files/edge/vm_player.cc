//------------------------------------------------------------------------
//  COAL Play Simulation Interface
//------------------------------------------------------------------------
//
//  Copyright (c) 2006-2024 The EDGE Team.
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
//------------------------------------------------------------------------

#include "AlmostEquals.h"
#include "coal.h"
#include "ddf_flat.h" // DDFFLAT - Dasho
#include "ddf_types.h"
#include "dm_state.h"
#include "e_main.h"
#include "e_player.h"
#include "epi.h"
#include "f_interm.h" //Lobo: need this to get access to intermission_stats
#include "g_game.h"
#include "hu_draw.h"
#include "hu_font.h"
#include "p_local.h"
#include "p_mobj.h"
#include "r_image.h"
#include "r_misc.h"
#include "r_modes.h"
#include "r_sky.h"
#include "r_state.h"
#include "rad_trig.h" //Lobo: need this to access RTS
#include "s_sound.h"  // play_footstep() - Dasho
#include "vm_coal.h"

extern coal::VM *ui_vm;

extern void COALSetFloat(coal::VM *vm, const char *mod_name, const char *var_name, double value);
extern void COALCallFunction(coal::VM *vm, const char *name);

Player *ui_player_who = nullptr;

//------------------------------------------------------------------------
//  PLAYER MODULE
//------------------------------------------------------------------------

// player.num_players()
//
static void PL_num_players(coal::VM *vm, int argc)
{
    vm->ReturnFloat(total_players);
}

// player.set_who(index)
//
static void PL_set_who(coal::VM *vm, int argc)
{
    int index = (int)*vm->AccessParam(0);

    if (index < 0 || index >= total_players)
        FatalError("player.set_who: bad index value: %d (numplayers=%d)\n", index, total_players);

    if (index == 0)
    {
        ui_player_who = players[console_player];
        return;
    }

    int who = display_player;

    for (; index > 1; index--)
    {
        do
        {
            who = (who + 1) % kMaximumPlayers;
        } while (players[who] == nullptr);
    }

    ui_player_who = players[who];
}

// player.is_bot()
//
static void PL_is_bot(coal::VM *vm, int argc)
{
    vm->ReturnFloat((ui_player_who->player_flags_ & kPlayerFlagBot) ? 1 : 0);
}

// player.get_name()
//
static void PL_get_name(coal::VM *vm, int argc)
{
    vm->ReturnString(ui_player_who->player_name_);
}

// player.get_pos()
//
static void PL_get_pos(coal::VM *vm, int argc)
{
    double v[3];

    v[0] = ui_player_who->map_object_->x;
    v[1] = ui_player_who->map_object_->y;
    v[2] = ui_player_who->map_object_->z;

    vm->ReturnVector(v);
}

// player.get_angle()
//
static void PL_get_angle(coal::VM *vm, int argc)
{
    float value = epi::DegreesFromBAM(ui_player_who->map_object_->angle_);

    if (value > 360.0f)
        value -= 360.0f;
    if (value < 0)
        value += 360.0f;

    vm->ReturnFloat(value);
}

// player.get_mlook()
//
static void PL_get_mlook(coal::VM *vm, int argc)
{
    float value = epi::DegreesFromBAM(ui_player_who->map_object_->vertical_angle_);

    if (value > 180.0f)
        value -= 360.0f;

    vm->ReturnFloat(value);
}

// player.health()
//
static void PL_health(coal::VM *vm, int argc)
{
    float health = ui_player_who->health_;
    if (health < 1 && health > 0)
        vm->ReturnFloat(1);
    else if (health > 99 && health < 100)
        vm->ReturnFloat(99);
    else
        vm->ReturnFloat((int)health);
}

// player.armor(type)
//
static void PL_armor(coal::VM *vm, int argc)
{
    int kind = (int)*vm->AccessParam(0);

    if (kind < 1 || kind > kTotalArmourTypes)
        FatalError("player.armor: bad armor index: %d\n", kind);

    kind--;
    // vm->ReturnFloat(floor(ui_player_who->armours_[kind] + 0.99));

    float a = ui_player_who->armours_[kind];
    if (a < 98)
        a += 0.99f;

    vm->ReturnFloat(floor(a));
}

// player.total_armor(type)
//
static void PL_total_armor(coal::VM *vm, int argc)
{
    // vm->ReturnFloat(floor(ui_player_who->totalarmour + 0.99));

    float a = ui_player_who->total_armour_;
    if (a < 98)
        a += 0.99f;

    vm->ReturnFloat(floor(a));
}

// player.frags()
//
static void PL_frags(coal::VM *vm, int argc)
{
    vm->ReturnFloat(ui_player_who->frags_);
}

// player.under_water()
//
static void PL_under_water(coal::VM *vm, int argc)
{
    vm->ReturnFloat(ui_player_who->underwater_ ? 1 : 0);
}

// player.on_ground()
//
static void PL_on_ground(coal::VM *vm, int argc)
{
    // not a 3D floor?
    if (ui_player_who->map_object_->subsector_->sector->extrafloor_used == 0)
    {
        // on the edge above water/lava/etc? Handles edge walker case
        if (!AlmostEquals(ui_player_who->map_object_->floor_z_,
                          ui_player_who->map_object_->subsector_->sector->floor_height) &&
            !ui_player_who->map_object_->subsector_->sector->floor_vertex_slope)
            vm->ReturnFloat(0);
        else
        {
            // touching the floor? Handles jumping or flying
            if (ui_player_who->map_object_->z <= ui_player_who->map_object_->floor_z_)
                vm->ReturnFloat(1);
            else
                vm->ReturnFloat(0);
        }
    }
    else
    {
        if (ui_player_who->map_object_->z <= ui_player_who->map_object_->floor_z_)
            vm->ReturnFloat(1);
        else
            vm->ReturnFloat(0);
    }
}

// player.is_swimming()
//
static void PL_is_swimming(coal::VM *vm, int argc)
{
    vm->ReturnFloat(ui_player_who->swimming_ ? 1 : 0);
}

// player.is_jumping()
//
static void PL_is_jumping(coal::VM *vm, int argc)
{
    vm->ReturnFloat((ui_player_who->jump_wait_ > 0) ? 1 : 0);
}

// player.is_crouching()
//
static void PL_is_crouching(coal::VM *vm, int argc)
{
    vm->ReturnFloat((ui_player_who->map_object_->extended_flags_ & kExtendedFlagCrouching) ? 1 : 0);
}

// player.is_attacking()
//
static void PL_is_attacking(coal::VM *vm, int argc)
{
    vm->ReturnFloat((ui_player_who->attack_button_down_[0] || ui_player_who->attack_button_down_[1] ||
                     ui_player_who->attack_button_down_[2] || ui_player_who->attack_button_down_[3])
                        ? 1
                        : 0);
}

// player.is_rampaging()
//
static void PL_is_rampaging(coal::VM *vm, int argc)
{
    vm->ReturnFloat((ui_player_who->attack_sustained_count_ >= 70) ? 1 : 0);
}

// player.is_grinning()
//
static void PL_is_grinning(coal::VM *vm, int argc)
{
    vm->ReturnFloat((ui_player_who->grin_count_ > 0) ? 1 : 0);
}

// player.is_using()
//
static void PL_is_using(coal::VM *vm, int argc)
{
    vm->ReturnFloat(ui_player_who->use_button_down_ ? 1 : 0);
}

// player.is_zoomed()
//
static void PL_is_zoomed(coal::VM *vm, int argc)
{
    vm->ReturnFloat(view_is_zoomed ? 1 : 0);
}

// player.is_action1()
//
static void PL_is_action1(coal::VM *vm, int argc)
{
    vm->ReturnFloat(ui_player_who->action_button_down_[0] ? 1 : 0);
}

// player.is_action2()
//
static void PL_is_action2(coal::VM *vm, int argc)
{
    vm->ReturnFloat(ui_player_who->action_button_down_[1] ? 1 : 0);
}

// player.move_speed()
//
static void PL_move_speed(coal::VM *vm, int argc)
{
    vm->ReturnFloat(ui_player_who->actual_speed_);
}

// player.air_in_lungs()
//
static void PL_air_in_lungs(coal::VM *vm, int argc)
{
    if (ui_player_who->air_in_lungs_ <= 0)
    {
        vm->ReturnFloat(0);
        return;
    }

    float value = ui_player_who->air_in_lungs_ * 100.0f / ui_player_who->map_object_->info_->lung_capacity_;

    value = HMM_Clamp(0.0f, value, 100.0f);

    vm->ReturnFloat(value);
}

// player.has_key(key)
//
static void PL_has_key(coal::VM *vm, int argc)
{
    int key = (int)*vm->AccessParam(0);

    if (key < 1 || key > 16)
        FatalError("player.has_key: bad key number: %d\n", key);

    key--;

    int value = (ui_player_who->cards_ & (1 << key)) ? 1 : 0;

    vm->ReturnFloat(value);
}

// player.has_power(power)
//
static void PL_has_power(coal::VM *vm, int argc)
{
    int power = (int)*vm->AccessParam(0);

    if (power < 1 || power > kTotalPowerTypes)
        FatalError("player.has_power: bad powerup number: %d\n", power);

    power--;

    int value = (ui_player_who->powers_[power] > 0) ? 1 : 0;

    // special check for GOD mode
    if (power == kPowerTypeInvulnerable && (ui_player_who->cheats_ & kCheatingGodMode))
        value = 1;

    vm->ReturnFloat(value);
}

// player.power_left(power)
//
static void PL_power_left(coal::VM *vm, int argc)
{
    int power = (int)*vm->AccessParam(0);

    if (power < 1 || power > kTotalPowerTypes)
        FatalError("player.power_left: bad powerup number: %d\n", power);

    power--;

    float value = ui_player_who->powers_[power];

    if (value > 0)
        value /= kTicRate;

    vm->ReturnFloat(value);
}

// player.has_weapon_slot(slot)
//
static void PL_has_weapon_slot(coal::VM *vm, int argc)
{
    int slot = (int)*vm->AccessParam(0);

    if (slot < 0 || slot > 9)
        FatalError("player.has_weapon_slot: bad slot number: %d\n", slot);

    int value = ui_player_who->available_weapons_[slot] ? 1 : 0;

    vm->ReturnFloat(value);
}

// player.cur_weapon_slot()
//
static void PL_cur_weapon_slot(coal::VM *vm, int argc)
{
    int slot;

    if (ui_player_who->ready_weapon_ < 0)
        slot = -1;
    else
        slot = ui_player_who->weapons_[ui_player_who->ready_weapon_].info->bind_key_;

    vm->ReturnFloat(slot);
}

// player.has_weapon(name)
//
static void PL_has_weapon(coal::VM *vm, int argc)
{
    const char *name = vm->AccessParamString(0);

    for (int j = 0; j < kMaximumWeapons; j++)
    {
        PlayerWeapon *pw = &ui_player_who->weapons_[j];

        if (pw->owned && !(pw->flags & kPlayerWeaponRemoving) && DDFCompareName(name, pw->info->name_.c_str()) == 0)
        {
            vm->ReturnFloat(1);
            return;
        }
    }

    vm->ReturnFloat(0);
}

// player.cur_weapon()
//
static void PL_cur_weapon(coal::VM *vm, int argc)
{
    if (ui_player_who->pending_weapon_ >= 0)
    {
        vm->ReturnString("change");
        return;
    }

    if (ui_player_who->ready_weapon_ < 0)
    {
        vm->ReturnString("none");
        return;
    }

    WeaponDefinition *info = ui_player_who->weapons_[ui_player_who->ready_weapon_].info;

    vm->ReturnString(info->name_.c_str());
}

static void COAL_SetPlayerSprite(Player *p, int position, int stnum, WeaponDefinition *info = nullptr)
{
    PlayerSprite *psp = &p->player_sprites_[position];

    if (stnum == 0)
    {
        // object removed itself
        psp->state = psp->next_state = nullptr;
        return;
    }

    // state is old? -- Mundo hack for DDF inheritance
    if (info && stnum < info->state_grp_.back().first)
    {
        State *st = &states[stnum];

        if (st->label)
        {
            int new_state = DDFStateFindLabel(info->state_grp_, st->label, true /* quiet */);
            if (new_state != 0)
                stnum = new_state;
        }
    }

    State *st = &states[stnum];

    // model interpolation stuff
    if (psp->state && (st->flags & kStateFrameFlagModel) && (psp->state->flags & kStateFrameFlagModel) &&
        (st->sprite == psp->state->sprite) && st->tics > 1)
    {
        p->weapon_last_frame_ = psp->state->frame;
    }
    else
        p->weapon_last_frame_ = -1;

    psp->state      = st;
    psp->tics       = st->tics;
    psp->next_state = (st->nextstate == 0) ? nullptr : (states + st->nextstate);

    // call action routine

    p->action_player_sprite_ = position;

    if (st->action)
        (*st->action)(p->map_object_);
}

//
// SetPlayerSpriteDeferred
//
// -AJA- 2004/11/05: This is preferred method, doesn't run any actions,
//       which (ideally) should only happen during MovePlayerSprites().
//
static void COAL_SetPlayerSpriteDeferred(Player *p, int position, int stnum)
{
    PlayerSprite *psp = &p->player_sprites_[position];

    if (stnum == 0 || psp->state == nullptr)
    {
        COAL_SetPlayerSprite(p, position, stnum);
        return;
    }

    psp->tics       = 0;
    psp->next_state = (states + stnum);
}

// player.weapon_state(weapon name, weapon state)
//
static void PL_weapon_state(coal::VM *vm, int argc)
{
    const char *weapon_name  = vm->AccessParamString(0);
    const char *weapon_state = vm->AccessParamString(1);

    if (ui_player_who->pending_weapon_ >= 0)
    {
        vm->ReturnFloat(0);
        return;
    }

    if (ui_player_who->ready_weapon_ < 0)
    {
        vm->ReturnFloat(0);
        return;
    }

    // WeaponDefinition *info =
    // ui_player_who->weapons_[ui_player_who->ready_weapon_].info;
    WeaponDefinition *oldWep = weapondefs.Lookup(weapon_name);
    if (!oldWep)
    {
        FatalError("player.weapon_state: Unknown weapon name '%s'.\n", weapon_name);
    }

    int pw_index;

    // see if player owns this kind of weapon
    for (pw_index = 0; pw_index < kMaximumWeapons; pw_index++)
    {
        if (!ui_player_who->weapons_[pw_index].owned)
            continue;

        if (ui_player_who->weapons_[pw_index].info == oldWep)
            break;
    }

    if (pw_index == kMaximumWeapons) // we dont have the weapon
    {
        vm->ReturnFloat(0);
        return;
    }

    ui_player_who->ready_weapon_ = (WeaponSelection)pw_index; // insta-switch to it

    int state = DDFStateFindLabel(oldWep->state_grp_, weapon_state, true /* quiet */);
    if (state == 0)
        FatalError("player.weapon_state: frame '%s' in [%s] not found!\n", weapon_state, weapon_name);
    // state += 1;

    COAL_SetPlayerSpriteDeferred(ui_player_who, kPlayerSpriteWeapon,
                                 state); // refresh the sprite

    vm->ReturnFloat(1);
}

// player.ammo(type)
//
static void PL_ammo(coal::VM *vm, int argc)
{
    int ammo = (int)*vm->AccessParam(0);

    if (ammo < 1 || ammo > kTotalAmmunitionTypes)
        FatalError("player.ammo: bad ammo number: %d\n", ammo);

    ammo--;

    vm->ReturnFloat(ui_player_who->ammo_[ammo].count);
}

// player.ammomax(type)
//
static void PL_ammomax(coal::VM *vm, int argc)
{
    int ammo = (int)*vm->AccessParam(0);

    if (ammo < 1 || ammo > kTotalAmmunitionTypes)
        FatalError("player.ammomax: bad ammo number: %d\n", ammo);

    ammo--;

    vm->ReturnFloat(ui_player_who->ammo_[ammo].maximum);
}

// player.inventory(type)
//
static void PL_inventory(coal::VM *vm, int argc)
{
    int inv = (int)*vm->AccessParam(0);

    if (inv < 1 || inv > kTotalInventoryTypes)
        FatalError("player.inventory: bad inv number: %d\n", inv);

    inv--;

    vm->ReturnFloat(ui_player_who->inventory_[inv].count);
}

// player.inventorymax(type)
//
static void PL_inventorymax(coal::VM *vm, int argc)
{
    int inv = (int)*vm->AccessParam(0);

    if (inv < 1 || inv > kTotalInventoryTypes)
        FatalError("player.inventorymax: bad inv number: %d\n", inv);

    inv--;

    vm->ReturnFloat(ui_player_who->inventory_[inv].maximum);
}

// player.counter(type)
//
static void PL_counter(coal::VM *vm, int argc)
{
    int cntr = (int)*vm->AccessParam(0);

    if (cntr < 1 || cntr > kTotalCounterTypes)
        FatalError("player.counter: bad counter number: %d\n", cntr);

    cntr--;

    vm->ReturnFloat(ui_player_who->counters_[cntr].count);
}

// player.counter_max(type)
//
static void PL_counter_max(coal::VM *vm, int argc)
{
    int cntr = (int)*vm->AccessParam(0);

    if (cntr < 1 || cntr > kTotalCounterTypes)
        FatalError("player.counter_max: bad counter number: %d\n", cntr);

    cntr--;

    vm->ReturnFloat(ui_player_who->counters_[cntr].maximum);
}

// player.set_counter(type, value)
//
static void PL_set_counter(coal::VM *vm, int argc)
{
    if (argc != 2)
        FatalError("player.set_counter: wrong number of arguments given\n");

    int cntr = (int)*vm->AccessParam(0);
    int amt  = (int)*vm->AccessParam(1);

    if (cntr < 1 || cntr > kTotalCounterTypes)
        FatalError("player.set_counter: bad counter number: %d\n", cntr);

    cntr--;

    if (amt < 0)
        FatalError("player.set_counter: target amount cannot be negative!\n");

    if (amt > ui_player_who->counters_[cntr].maximum)
        FatalError("player.set_counter: target amount %d exceeds limit for counter "
                   "number %d\n",
                   amt, cntr);

    ui_player_who->counters_[cntr].count = amt;
}

// player.main_ammo(clip)
//
static void PL_main_ammo(coal::VM *vm, int argc)
{
    int value = 0;

    if (ui_player_who->ready_weapon_ >= 0)
    {
        PlayerWeapon *pw = &ui_player_who->weapons_[ui_player_who->ready_weapon_];

        if (pw->info->ammo_[0] != kAmmunitionTypeNoAmmo)
        {
            if (pw->info->show_clip_)
            {
                EPI_ASSERT(pw->info->ammopershot_[0] > 0);

                value = pw->clip_size[0] / pw->info->ammopershot_[0];
            }
            else
            {
                value = ui_player_who->ammo_[pw->info->ammo_[0]].count;

                if (pw->info->clip_size_[0] > 0)
                    value += pw->clip_size[0];
            }
        }
    }

    vm->ReturnFloat(value);
}

// player.ammo_type(ATK)
//
static void PL_ammo_type(coal::VM *vm, int argc)
{
    int ATK = (int)*vm->AccessParam(0);

    if (ATK < 1 || ATK > 2)
        FatalError("player.ammo_type: bad attack number: %d\n", ATK);

    ATK--;

    int value = 0;

    if (ui_player_who->ready_weapon_ >= 0)
    {
        PlayerWeapon *pw = &ui_player_who->weapons_[ui_player_who->ready_weapon_];

        value = 1 + (int)pw->info->ammo_[ATK];
    }

    vm->ReturnFloat(value);
}

// player.ammo_pershot(ATK)
//
static void PL_ammo_pershot(coal::VM *vm, int argc)
{
    int ATK = (int)*vm->AccessParam(0);

    if (ATK < 1 || ATK > 2)
        FatalError("player.ammo_pershot: bad attack number: %d\n", ATK);

    ATK--;

    int value = 0;

    if (ui_player_who->ready_weapon_ >= 0)
    {
        PlayerWeapon *pw = &ui_player_who->weapons_[ui_player_who->ready_weapon_];

        value = pw->info->ammopershot_[ATK];
    }

    vm->ReturnFloat(value);
}

// player.clip_ammo(ATK)
//
static void PL_clip_ammo(coal::VM *vm, int argc)
{
    int ATK = (int)*vm->AccessParam(0);

    if (ATK < 1 || ATK > 2)
        FatalError("player.clip_ammo: bad attack number: %d\n", ATK);

    ATK--;

    int value = 0;

    if (ui_player_who->ready_weapon_ >= 0)
    {
        PlayerWeapon *pw = &ui_player_who->weapons_[ui_player_who->ready_weapon_];

        value = pw->clip_size[ATK];
    }

    vm->ReturnFloat(value);
}

// player.clip_size(ATK)
//
static void PL_clip_size(coal::VM *vm, int argc)
{
    int ATK = (int)*vm->AccessParam(0);

    if (ATK < 1 || ATK > 2)
        FatalError("player.clip_size: bad attack number: %d\n", ATK);

    ATK--;

    int value = 0;

    if (ui_player_who->ready_weapon_ >= 0)
    {
        PlayerWeapon *pw = &ui_player_who->weapons_[ui_player_who->ready_weapon_];

        value = pw->info->clip_size_[ATK];
    }

    vm->ReturnFloat(value);
}

// player.clip_is_shared()
//
static void PL_clip_is_shared(coal::VM *vm, int argc)
{
    int value = 0;

    if (ui_player_who->ready_weapon_ >= 0)
    {
        PlayerWeapon *pw = &ui_player_who->weapons_[ui_player_who->ready_weapon_];

        if (pw->info->shared_clip_)
            value = 1;
    }

    vm->ReturnFloat(value);
}

// player.hurt_by()
//
static void PL_hurt_by(coal::VM *vm, int argc)
{
    if (ui_player_who->damage_count_ <= 0)
    {
        vm->ReturnString("");
        return;
    }

    // getting hurt because of your own damn stupidity
    if (ui_player_who->attacker_ == ui_player_who->map_object_)
        vm->ReturnString("self");
    else if (ui_player_who->attacker_ && (ui_player_who->attacker_->side_ & ui_player_who->map_object_->side_))
        vm->ReturnString("friend");
    else if (ui_player_who->attacker_)
        vm->ReturnString("enemy");
    else
        vm->ReturnString("other");
}

// player.hurt_mon()
//
static void PL_hurt_mon(coal::VM *vm, int argc)
{
    if (ui_player_who->damage_count_ > 0 && ui_player_who->attacker_ &&
        ui_player_who->attacker_ != ui_player_who->map_object_)
    {
        vm->ReturnString(ui_player_who->attacker_->info_->name_.c_str());
        return;
    }

    vm->ReturnString("");
}

// player.hurt_pain()
//
static void PL_hurt_pain(coal::VM *vm, int argc)
{
    vm->ReturnFloat(ui_player_who->damage_pain_);
}

// player.hurt_dir()
//
static void PL_hurt_dir(coal::VM *vm, int argc)
{
    int dir = 0;

    if (ui_player_who->attacker_ && ui_player_who->attacker_ != ui_player_who->map_object_)
    {
        MapObject *badguy = ui_player_who->attacker_;
        MapObject *pmo    = ui_player_who->map_object_;

        BAMAngle diff = PointToAngle(pmo->x, pmo->y, badguy->x, badguy->y) - pmo->angle_;

        if (diff >= kBAMAngle45 && diff <= kBAMAngle135)
        {
            dir = -1;
        }
        else if (diff >= kBAMAngle225 && diff <= kBAMAngle315)
        {
            dir = +1;
        }
    }

    vm->ReturnFloat(dir);
}

// player.hurt_angle()
//
static void PL_hurt_angle(coal::VM *vm, int argc)
{
    float value = 0;

    if (ui_player_who->attacker_ && ui_player_who->attacker_ != ui_player_who->map_object_)
    {
        MapObject *badguy = ui_player_who->attacker_;
        MapObject *pmo    = ui_player_who->map_object_;

        BAMAngle real_a = PointToAngle(pmo->x, pmo->y, badguy->x, badguy->y);

        value = epi::DegreesFromBAM(real_a);

        if (value > 360.0f)
            value -= 360.0f;

        if (value < 0)
            value += 360.0f;
    }

    vm->ReturnFloat(value);
}

// player.kills()
// Lobo: November 2021
static void PL_kills(coal::VM *vm, int argc)
{
    vm->ReturnFloat(ui_player_who->kill_count_);
}

// player.secrets()
// Lobo: November 2021
static void PL_secrets(coal::VM *vm, int argc)
{
    vm->ReturnFloat(ui_player_who->secret_count_);
}

// player.items()
// Lobo: November 2021
static void PL_items(coal::VM *vm, int argc)
{
    vm->ReturnFloat(ui_player_who->item_count_);
}

// player.map_enemies()
// Lobo: November 2021
static void PL_map_enemies(coal::VM *vm, int argc)
{
    vm->ReturnFloat(intermission_stats.kills);
}

// player.map_secrets()
// Lobo: November 2021
static void PL_map_secrets(coal::VM *vm, int argc)
{
    vm->ReturnFloat(intermission_stats.secrets);
}

// player.map_items()
// Lobo: November 2021
static void PL_map_items(coal::VM *vm, int argc)
{
    vm->ReturnFloat(intermission_stats.items);
}

// player.floor_flat()
// Lobo: November 2021
static void PL_floor_flat(coal::VM *vm, int argc)
{
    // If no 3D floors, just return the flat
    if (ui_player_who->map_object_->subsector_->sector->extrafloor_used == 0)
    {
        vm->ReturnString(ui_player_who->map_object_->subsector_->sector->floor.image->name_.c_str());
    }
    else
    {
        // Start from the lowest exfloor and check if the player is standing on
        // it, then return the control sector's flat
        float       player_floor_height = ui_player_who->map_object_->floor_z_;
        Extrafloor *floor_checker       = ui_player_who->map_object_->subsector_->sector->bottom_extrafloor;
        for (Extrafloor *ef = floor_checker; ef; ef = ef->higher)
        {
            if (player_floor_height + 1 > ef->top_height)
            {
                vm->ReturnString(ef->top->image->name_.c_str());
                return;
            }
        }
        // Fallback if nothing else satisfies these conditions
        vm->ReturnString(ui_player_who->map_object_->subsector_->sector->floor.image->name_.c_str());
    }
}

// player.sector_tag()
// Lobo: November 2021
static void PL_sector_tag(coal::VM *vm, int argc)
{
    vm->ReturnFloat(ui_player_who->map_object_->subsector_->sector->tag);
}

// player.play_footstep(flat name)
// Dasho: January 2022
// Now uses the new DDFFLAT construct
static void PL_play_footstep(coal::VM *vm, int argc)
{
    const char *flat = vm->AccessParamString(0);
    if (!flat)
        FatalError("player.play_footstep: No flat name given!\n");

    FlatDefinition *current_flatdef = flatdefs.Find(flat);

    if (!current_flatdef)
    {
        vm->ReturnFloat(0);
        return;
    }

    if (!current_flatdef->footstep_)
    {
        vm->ReturnFloat(0);
        return;
    }
    else
    {
        // Probably need to add check to see if the sfx is valid - Dasho
        StartSoundEffect(current_flatdef->footstep_);
        vm->ReturnFloat(1);
    }
}

// player.use_inventory(type)
//
static void PL_use_inventory(coal::VM *vm, int argc)
{
    double     *num         = vm->AccessParam(0);
    std::string script_name = "INVENTORY";
    int         inv         = 0;
    if (!num)
        FatalError("player.use_inventory: can't parse inventory number!\n");
    else
        inv = (int)*num;

    if (inv < 1 || inv > kTotalInventoryTypes)
        FatalError("player.use_inventory: bad inventory number: %d\n", inv);

    if (inv < 10)
        script_name.append("0").append(std::to_string(inv));
    else
        script_name.append(std::to_string(inv));

    inv--;

    //******
    // If the same inventory script is already running then
    // don't start the same one again
    if (!CheckActiveScriptByTag(nullptr, script_name.c_str()))
    {
        if (ui_player_who->inventory_[inv].count > 0)
        {
            ui_player_who->inventory_[inv].count -= 1;
            ScriptEnableByTag(nullptr, script_name.c_str(), false);
        }
    }
}

// player.rts_enable_tagged(tag)
//
static void PL_rts_enable_tagged(coal::VM *vm, int argc)
{
    std::string name = vm->AccessParamString(0);

    if (!name.empty())
        ScriptEnableByTag(nullptr, name.c_str(), false);
}

// AuxStringReplaceAll("Our_String", std::string("_"), std::string(" "));
//
static std::string AuxStringReplaceAll(std::string str, const std::string &from, const std::string &to)
{
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos)
    {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
    return str;
}

// GetMobjBenefits(mobj);
//
static std::string GetMobjBenefits(MapObject *obj, bool KillBenefits = false)
{
    std::string temp_string;
    temp_string.clear();
    Benefit *list;
    int      temp_num = 0;

    if (KillBenefits)
        list = obj->info_->kill_benefits_;
    else
        list = obj->info_->pickup_benefits_;

    for (; list != nullptr; list = list->next)
    {
        switch (list->type)
        {
        case kBenefitTypeWeapon:
            // If it's a weapon all bets are off: we'll want to parse
            // it differently, not here.
            temp_string = "WEAPON=1";
            break;

        case kBenefitTypeAmmo:
            temp_string += "AMMO";
            if ((list->sub.type + 1) < 10)
                temp_string += "0";
            temp_string += std::to_string((int)list->sub.type + 1);
            temp_string += "=" + std::to_string((int)list->amount);
            break;

        case kBenefitTypeHealth: // only benefit without a sub.type so just
                                 // give it 01
            temp_string += "HEALTH01=" + std::to_string((int)list->amount);
            break;

        case kBenefitTypeArmour:
            temp_string += "ARMOUR" + std::to_string((int)list->sub.type + 1);
            temp_string += "=" + std::to_string((int)list->amount);
            break;

        case kBenefitTypeInventory:
            temp_string += "INVENTORY";
            if ((list->sub.type + 1) < 10)
                temp_string += "0";
            temp_string += std::to_string((int)list->sub.type + 1);
            temp_string += "=" + std::to_string((int)list->amount);
            break;

        case kBenefitTypeCounter:
            temp_string += "COUNTER";
            if ((list->sub.type + 1) < 10)
                temp_string += "0";
            temp_string += std::to_string((int)list->sub.type + 1);
            temp_string += "=" + std::to_string((int)list->amount);
            break;

        case kBenefitTypeKey:
            temp_string += "KEY";
            temp_num = log2((int)list->sub.type);
            temp_num++;
            temp_string += std::to_string(temp_num);
            break;

        case kBenefitTypePowerup:
            temp_string += "POWERUP" + std::to_string((int)list->sub.type + 1);
            break;

        default:
            break;
        }
    }
    return temp_string;
}

// GetQueryInfoFromMobj(mobj, whatinfo)
//
static std::string GetQueryInfoFromMobj(MapObject *obj, int whatinfo)
{
    int         temp_num = 0;
    std::string temp_string;
    temp_string.clear();

    switch (whatinfo)
    {
    case 1: // name
        if (obj)
        {
            // try CAST_TITLE first
            temp_string = language[obj->info_->cast_title_];

            if (temp_string.empty()) // fallback to DDFTHING entry name
            {
                temp_string = obj->info_->name_;
                temp_string = AuxStringReplaceAll(temp_string, std::string("_"), std::string(" "));
            }
        }
        break;

    case 2: // current health
        if (obj)
        {
            temp_num    = obj->health_;
            temp_string = std::to_string(temp_num);
        }
        break;

    case 3: // spawn health
        if (obj)
        {
            temp_num    = obj->spawn_health_;
            temp_string = std::to_string(temp_num);
        }
        break;

    case 4: // pickup_benefits
        if (obj)
        {
            temp_string = GetMobjBenefits(obj, false);
        }
        break;

    case 5: // kill_benefits
        if (obj)
        {
            temp_string = GetMobjBenefits(obj, true);
        }
        break;
    }

    if (temp_string.empty())
        return ("");

    return (temp_string.c_str());
}

// GetQueryInfoFromWeapon(mobj, whatinfo, [secattackinfo])
//
static std::string GetQueryInfoFromWeapon(MapObject *obj, int whatinfo, bool secattackinfo = false)
{
    int         temp_num = 0;
    std::string temp_string;
    temp_string.clear();

    if (!obj->info_->pickup_benefits_)
        return "";
    if (!obj->info_->pickup_benefits_->sub.weap)
        return "";
    if (obj->info_->pickup_benefits_->type != kBenefitTypeWeapon)
        return "";

    WeaponDefinition *objWep = obj->info_->pickup_benefits_->sub.weap;
    if (!objWep)
        return "";

    int attacknum = 0; // default to primary attack
    if (secattackinfo)
        attacknum = 1;

    AttackDefinition *objAtck = objWep->attack_[attacknum];
    if (!objAtck && whatinfo > 2)
        return ""; // no attack to get info about (only should happen with
                   // secondary attacks)

    const DamageClass *damtype;

    float temp_num2;

    switch (whatinfo)
    {
    case 1: // name
        temp_string = objWep->name_;
        temp_string = AuxStringReplaceAll(temp_string, std::string("_"), std::string(" "));
        break;

    case 2: // ZOOM_FACTOR
        temp_num2   = 90.0f / objWep->zoom_fov_;
        temp_string = std::to_string(temp_num2);
        break;

    case 3: // AMMOTYPE
        temp_num    = (objWep->ammo_[attacknum]) + 1;
        temp_string = std::to_string(temp_num);
        break;

    case 4: // AMMOPERSHOT
        temp_num    = objWep->ammopershot_[attacknum];
        temp_string = std::to_string(temp_num);
        break;

    case 5: // CLIPSIZE
        temp_num    = objWep->clip_size_[attacknum];
        temp_string = std::to_string(temp_num);
        break;

    case 6: // DAMAGE Nominal
        damtype     = &objAtck->damage_;
        temp_num    = damtype->nominal_;
        temp_string = std::to_string(temp_num);
        break;

    case 7: // DAMAGE Max
        damtype     = &objAtck->damage_;
        temp_num    = damtype->linear_max_;
        temp_string = std::to_string(temp_num);
        break;

    case 8: // Range
        temp_num    = objAtck->range_;
        temp_string = std::to_string(temp_num);
        break;

    case 9: // AUTOMATIC
        if (objWep->autofire_[attacknum])
            temp_string = "1";
        else
            temp_string = "0";
        break;
    }

    if (temp_string.empty())
        return ("");

    return (temp_string.c_str());
}

// player.query_object(maxdistance,whatinfo)
//
static void PL_query_object(coal::VM *vm, int argc)
{
    double *maxd        = vm->AccessParam(0);
    double *whati       = vm->AccessParam(1);
    int     whatinfo    = 1;
    int     maxdistance = 512;

    if (argc != 2)
        FatalError("player.query_object: wrong number of arguments given\n");

    if (!whati)
        FatalError("player.query_object: can't parse WhatInfo!\n");
    else
        whatinfo = (int)*whati;

    if (!maxd)
        FatalError("player.query_object: can't parse MaxDistance!\n");
    else
        maxdistance = (int)*maxd;

    if (whatinfo < 1 || whatinfo > 5)
        FatalError("player.query_object: bad whatInfo number: %d\n", whatinfo);

    MapObject *obj = GetMapTargetAimInfo(ui_player_who->map_object_, ui_player_who->map_object_->angle_, maxdistance);
    if (!obj)
    {
        vm->ReturnString("");
        return;
    }

    std::string temp_string;
    temp_string.clear();

    temp_string = GetQueryInfoFromMobj(obj, whatinfo);

    if (temp_string.empty())
        vm->ReturnString("");
    else
        vm->ReturnString(temp_string.c_str());
}

// mapobject.query_tagged(thing tag, whatinfo)
//
static void MO_query_tagged(coal::VM *vm, int argc)
{
    if (argc != 2)
        FatalError("mapobject.query_tagged: wrong number of arguments given\n");

    double *argTag   = vm->AccessParam(0);
    double *argInfo  = vm->AccessParam(1);
    int     whattag  = 1;
    int     whatinfo = 1;

    whattag  = (int)*argTag;
    whatinfo = (int)*argInfo;

    MapObject *mo;

    std::string temp_value;
    temp_value.clear();

    for (mo = map_object_list_head; mo; mo = mo->next_)
    {
        if (mo->tag_ == whattag)
        {
            temp_value = GetQueryInfoFromMobj(mo, whatinfo);
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
static void MO_count(coal::VM *vm, int argc)
{
    double *num     = vm->AccessParam(0);
    int     thingid = 0;

    if (!num)
        FatalError("mapobjects.count: can't parse thing id/type!\n");
    else
        thingid = (int)*num;

    MapObject *mo;

    double thingcount = 0;

    for (mo = map_object_list_head; mo; mo = mo->next_)
    {
        if (mo->info_->number_ == thingid && mo->health_ > 0)
            thingcount++;
    }

    vm->ReturnFloat(thingcount);
}

// player.query_weapon(maxdistance,whatinfo,[SecAttack])
//
static void PL_query_weapon(coal::VM *vm, int argc)
{
    double *maxd      = vm->AccessParam(0);
    double *whati     = vm->AccessParam(1);
    double *secattack = vm->AccessParam(2);

    int whatinfo      = 1;
    int maxdistance   = 512;
    int secattackinfo = 0;

    if (!maxd)
        FatalError("player.query_weapon: can't parse MaxDistance!\n");
    else
        maxdistance = (int)*maxd;

    if (!whati)
        FatalError("player.query_weapon: can't parse WhatInfo!\n");
    else
        whatinfo = (int)*whati;

    if (secattack)
        secattackinfo = (int)*secattack;

    if (whatinfo < 1 || whatinfo > 9)
        FatalError("player.query_weapon: bad whatInfo number: %d\n", whatinfo);

    if (secattackinfo < 0 || secattackinfo > 1)
        FatalError("player.query_weapon: bad secAttackInfo number: %d\n", whatinfo);

    MapObject *obj = GetMapTargetAimInfo(ui_player_who->map_object_, ui_player_who->map_object_->angle_, maxdistance);
    if (!obj)
    {
        vm->ReturnString("");
        return;
    }

    std::string temp_string;
    temp_string.clear();

    if (secattackinfo == 1)
        temp_string = GetQueryInfoFromWeapon(obj, whatinfo, true);
    else
        temp_string = GetQueryInfoFromWeapon(obj, whatinfo);

    if (temp_string.empty())
        vm->ReturnString("");
    else
        vm->ReturnString(temp_string.c_str());
}

// player.sector_light()
// Lobo: May 2023
static void PL_sector_light(coal::VM *vm, int argc)
{
    vm->ReturnFloat(ui_player_who->map_object_->subsector_->sector->properties.light_level);
}

// player.sector_floor_height()
// Lobo: May 2023
static void PL_sector_floor_height(coal::VM *vm, int argc)
{
    // If no 3D floors, just return the current sector floor height
    if (ui_player_who->map_object_->subsector_->sector->extrafloor_used == 0)
    {
        vm->ReturnFloat(ui_player_who->map_object_->subsector_->sector->floor_height);
    }
    else
    {
        // Start from the lowest exfloor and check if the player is standing on
        // it,
        //  then return the control sector floor height
        float       CurrentFloor        = 0;
        float       player_floor_height = ui_player_who->map_object_->floor_z_;
        Extrafloor *floor_checker       = ui_player_who->map_object_->subsector_->sector->bottom_extrafloor;
        for (Extrafloor *ef = floor_checker; ef; ef = ef->higher)
        {
            if (CurrentFloor > ef->top_height)
            {
                vm->ReturnFloat(ef->top_height);
                return;
            }

            if (player_floor_height + 1 > ef->top_height)
            {
                CurrentFloor = ef->top_height;
            }
        }
        vm->ReturnFloat(CurrentFloor);
    }
}

// player.sector_ceiling_height()
// Lobo: May 2023
static void PL_sector_ceiling_height(coal::VM *vm, int argc)
{
    // If no 3D floors, just return the current sector ceiling height
    if (ui_player_who->map_object_->subsector_->sector->extrafloor_used == 0)
    {
        vm->ReturnFloat(ui_player_who->map_object_->subsector_->sector->ceiling_height);
    }
    else
    {
        // Start from the lowest exfloor and check if the player is standing on
        // it,
        //   then return the control sector ceiling height
        float       HighestCeiling      = 0;
        float       player_floor_height = ui_player_who->map_object_->floor_z_;
        Extrafloor *floor_checker       = ui_player_who->map_object_->subsector_->sector->bottom_extrafloor;
        for (Extrafloor *ef = floor_checker; ef; ef = ef->higher)
        {
            if (player_floor_height + 1 > ef->top_height)
            {
                HighestCeiling = ef->top_height;
            }
            if (HighestCeiling < ef->top_height)
            {
                vm->ReturnFloat(ef->bottom_height);
                return;
            }
        }
        // Fallback if nothing else satisfies these conditions
        vm->ReturnFloat(ui_player_who->map_object_->subsector_->sector->ceiling_height);
    }
}

// player.is_outside()
// Lobo: May 2023
static void PL_is_outside(coal::VM *vm, int argc)
{
    // Doesn't account for extrafloors by design. Reasoning is that usually
    //  extrafloors will be platforms, not roofs...
    if (ui_player_who->map_object_->subsector_->sector->ceiling.image != sky_flat_image) // is it outdoors?
        vm->ReturnFloat(0);
    else
        vm->ReturnFloat(1);
}

//------------------------------------------------------------------------

void COALRegisterPlaysim()
{
    ui_vm->AddNativeFunction("player.num_players", PL_num_players);
    ui_vm->AddNativeFunction("player.set_who", PL_set_who);
    ui_vm->AddNativeFunction("player.is_bot", PL_is_bot);
    ui_vm->AddNativeFunction("player.get_name", PL_get_name);
    ui_vm->AddNativeFunction("player.get_pos", PL_get_pos);
    ui_vm->AddNativeFunction("player.get_angle", PL_get_angle);
    ui_vm->AddNativeFunction("player.get_mlook", PL_get_mlook);

    ui_vm->AddNativeFunction("player.health", PL_health);
    ui_vm->AddNativeFunction("player.armor", PL_armor);
    ui_vm->AddNativeFunction("player.total_armor", PL_total_armor);
    ui_vm->AddNativeFunction("player.ammo", PL_ammo);
    ui_vm->AddNativeFunction("player.ammomax", PL_ammomax);
    ui_vm->AddNativeFunction("player.frags", PL_frags);

    ui_vm->AddNativeFunction("player.is_swimming", PL_is_swimming);
    ui_vm->AddNativeFunction("player.is_jumping", PL_is_jumping);
    ui_vm->AddNativeFunction("player.is_crouching", PL_is_crouching);
    ui_vm->AddNativeFunction("player.is_using", PL_is_using);
    ui_vm->AddNativeFunction("player.is_action1", PL_is_action1);
    ui_vm->AddNativeFunction("player.is_action2", PL_is_action2);
    ui_vm->AddNativeFunction("player.is_attacking", PL_is_attacking);
    ui_vm->AddNativeFunction("player.is_rampaging", PL_is_rampaging);
    ui_vm->AddNativeFunction("player.is_grinning", PL_is_grinning);

    ui_vm->AddNativeFunction("player.under_water", PL_under_water);
    ui_vm->AddNativeFunction("player.on_ground", PL_on_ground);
    ui_vm->AddNativeFunction("player.move_speed", PL_move_speed);
    ui_vm->AddNativeFunction("player.air_in_lungs", PL_air_in_lungs);

    ui_vm->AddNativeFunction("player.has_key", PL_has_key);
    ui_vm->AddNativeFunction("player.has_power", PL_has_power);
    ui_vm->AddNativeFunction("player.power_left", PL_power_left);
    ui_vm->AddNativeFunction("player.has_weapon", PL_has_weapon);
    ui_vm->AddNativeFunction("player.has_weapon_slot", PL_has_weapon_slot);
    ui_vm->AddNativeFunction("player.cur_weapon", PL_cur_weapon);
    ui_vm->AddNativeFunction("player.cur_weapon_slot", PL_cur_weapon_slot);

    ui_vm->AddNativeFunction("player.main_ammo", PL_main_ammo);
    ui_vm->AddNativeFunction("player.ammo_type", PL_ammo_type);
    ui_vm->AddNativeFunction("player.ammo_pershot", PL_ammo_pershot);
    ui_vm->AddNativeFunction("player.clip_ammo", PL_clip_ammo);
    ui_vm->AddNativeFunction("player.clip_size", PL_clip_size);
    ui_vm->AddNativeFunction("player.clip_is_shared", PL_clip_is_shared);

    ui_vm->AddNativeFunction("player.hurt_by", PL_hurt_by);
    ui_vm->AddNativeFunction("player.hurt_mon", PL_hurt_mon);
    ui_vm->AddNativeFunction("player.hurt_pain", PL_hurt_pain);
    ui_vm->AddNativeFunction("player.hurt_dir", PL_hurt_dir);
    ui_vm->AddNativeFunction("player.hurt_angle", PL_hurt_angle);

    ui_vm->AddNativeFunction("player.kills", PL_kills);
    ui_vm->AddNativeFunction("player.secrets", PL_secrets);
    ui_vm->AddNativeFunction("player.items", PL_items);
    ui_vm->AddNativeFunction("player.map_enemies", PL_map_enemies);
    ui_vm->AddNativeFunction("player.map_secrets", PL_map_secrets);
    ui_vm->AddNativeFunction("player.map_items", PL_map_items);
    ui_vm->AddNativeFunction("player.floor_flat", PL_floor_flat);
    ui_vm->AddNativeFunction("player.sector_tag", PL_sector_tag);

    ui_vm->AddNativeFunction("player.play_footstep", PL_play_footstep);

    ui_vm->AddNativeFunction("player.use_inventory", PL_use_inventory);
    ui_vm->AddNativeFunction("player.inventory", PL_inventory);
    ui_vm->AddNativeFunction("player.inventorymax", PL_inventorymax);

    ui_vm->AddNativeFunction("player.rts_enable_tagged", PL_rts_enable_tagged);

    ui_vm->AddNativeFunction("player.counter", PL_counter);
    ui_vm->AddNativeFunction("player.counter_max", PL_counter_max);
    ui_vm->AddNativeFunction("player.set_counter", PL_set_counter);

    ui_vm->AddNativeFunction("player.query_object", PL_query_object);
    ui_vm->AddNativeFunction("player.query_weapon", PL_query_weapon);
    ui_vm->AddNativeFunction("mapobject.query_tagged", MO_query_tagged);
    ui_vm->AddNativeFunction("mapobject.count", MO_count);

    ui_vm->AddNativeFunction("player.is_zoomed", PL_is_zoomed);
    ui_vm->AddNativeFunction("player.weapon_state", PL_weapon_state);

    ui_vm->AddNativeFunction("player.sector_light", PL_sector_light);
    ui_vm->AddNativeFunction("player.sector_floor_height", PL_sector_floor_height);
    ui_vm->AddNativeFunction("player.sector_ceiling_height", PL_sector_ceiling_height);
    ui_vm->AddNativeFunction("player.is_outside", PL_is_outside);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
