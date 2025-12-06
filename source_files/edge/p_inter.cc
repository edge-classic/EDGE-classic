//----------------------------------------------------------------------------
//  EDGE Interactions (picking up items etc..) Code
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

#include "AlmostEquals.h"
#include "am_map.h"
#include "con_main.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "dstrings.h"
#include "e_input.h"
#include "epi.h"
#include "epi_str_util.h"
#include "g_game.h"
#include "m_random.h"
#include "p_local.h"
#include "r_misc.h"
#include "rad_trig.h"
#include "s_sound.h"

static constexpr uint8_t kBonusAddMinimum = 6;
static constexpr uint8_t kBonusLimit      = 100;

static constexpr uint8_t kDamageAddMinimum = 3;
static constexpr uint8_t kDamageLimit      = 100;

// follow a player exlusively for 3 seconds
static constexpr uint8_t kBaseThreshold = 100;

static constexpr float kDeathViewHeight = 6.0f;

bool show_obituaries = true;

extern ConsoleVariable gore_level;
extern ConsoleVariable player_deathmatch_damage_resistance;

struct PickupInfo
{
    Benefit *list;      // full list of benefits
    bool     lose_them; // lose stuff if true

    Player    *player;  // player picking it up
    MapObject *special; // object to pick up
    bool       dropped; // object was dropped by a monster

    int new_weapon;     // index (for player) of a new weapon, -1 = none
    int new_ammo;       // ammotype of new ammo, -1 = none

    bool got_it;        // player actually got the benefit
    bool keep_it;       // don't remove the thing from map
    bool silent;        // don't make sound/flash/effects
    bool no_ammo;       // skip ammo
};

static bool CheckForBenefit(Benefit *list, int kind)
{
    for (Benefit *be = list; be != nullptr; be = be->next)
    {
        if (be->type == kind)
            return true;
    }

    return false;
}

//
// GiveCounter
//
// Returns false if the "counter" item can't be picked up at all
//
//
static void GiveCounter(PickupInfo *pu, Benefit *be)
{
    int cntr = be->sub.type;
    int num  = RoundToInteger(be->amount);

    if (cntr < 0 || cntr >= kTotalCounterTypes)
        FatalError("GiveCounter: bad type %i", cntr);

    if (pu->lose_them)
    {
        if (pu->player->counters_[cntr].count == 0)
            return;

        pu->player->counters_[cntr].count -= num;

        if (pu->player->counters_[cntr].count < 0)
            pu->player->counters_[cntr].count = 0;

        pu->got_it = true;
        return;
    }

    if (pu->player->counters_[cntr].count == pu->player->counters_[cntr].maximum)
    {
        return;
    }

    pu->player->counters_[cntr].count += num;

    if (pu->player->counters_[cntr].count > pu->player->counters_[cntr].maximum)
        pu->player->counters_[cntr].count = pu->player->counters_[cntr].maximum;

    pu->got_it = true;
}

//
// GiveCounterLimit
//
static void GiveCounterLimit(PickupInfo *pu, Benefit *be)
{
    int cntr  = be->sub.type;
    int limit = RoundToInteger(be->amount);

    if (cntr < 0 || cntr >= kTotalCounterTypes)
        FatalError("GiveCounterLimit: bad type %i", cntr);

    if ((!pu->lose_them && limit < pu->player->counters_[cntr].maximum) ||
        (pu->lose_them && limit > pu->player->counters_[cntr].maximum))
    {
        return;
    }

    pu->player->counters_[cntr].maximum = limit;

    // new limit could be lower...
    if (pu->player->counters_[cntr].count > pu->player->counters_[cntr].maximum)
        pu->player->counters_[cntr].count = pu->player->counters_[cntr].maximum;

    pu->got_it = true;
}

//
// GiveInventory
//
// Returns false if the inventory item can't be picked up at all
//
//
static void GiveInventory(PickupInfo *pu, Benefit *be)
{
    int inv = be->sub.type;
    int num = RoundToInteger(be->amount);

    if (inv < 0 || inv >= kTotalInventoryTypes)
        FatalError("GiveInventory: bad type %i", inv);

    if (pu->lose_them)
    {
        if (pu->player->inventory_[inv].count == 0)
            return;

        pu->player->inventory_[inv].count -= num;

        if (pu->player->inventory_[inv].count < 0)
            pu->player->inventory_[inv].count = 0;

        pu->got_it = true;
        return;
    }

    if (pu->player->inventory_[inv].count == pu->player->inventory_[inv].maximum)
    {
        return;
    }

    pu->player->inventory_[inv].count += num;

    if (pu->player->inventory_[inv].count > pu->player->inventory_[inv].maximum)
        pu->player->inventory_[inv].count = pu->player->inventory_[inv].maximum;

    pu->got_it = true;
}

//
// GiveInventoryLimit
//
static void GiveInventoryLimit(PickupInfo *pu, Benefit *be)
{
    int inv   = be->sub.type;
    int limit = RoundToInteger(be->amount);

    if (inv < 0 || inv >= kTotalInventoryTypes)
        FatalError("GiveInventoryLimit: bad type %i", inv);

    if ((!pu->lose_them && limit < pu->player->inventory_[inv].maximum) ||
        (pu->lose_them && limit > pu->player->inventory_[inv].maximum))
    {
        return;
    }

    pu->player->inventory_[inv].maximum = limit;

    // new limit could be lower...
    if (pu->player->inventory_[inv].count > pu->player->inventory_[inv].maximum)
        pu->player->inventory_[inv].count = pu->player->inventory_[inv].maximum;

    pu->got_it = true;
}

//
// GiveAmmo
//
// Returns false if the ammo can't be picked up at all
//
// -ACB- 1998/06/19 DDF Change: Number passed is the exact amount of ammo given.
// -KM- 1998/11/25 Handles weapon change from priority.
//
static void GiveAmmo(PickupInfo *pu, Benefit *be)
{
    if (pu->no_ammo)
        return;

    int ammo = be->sub.type;
    int num  = RoundToInteger(be->amount);

    // -AJA- in old deathmatch, weapons give 2.5 times more ammo
    if (deathmatch == 1 && CheckForBenefit(pu->list, kBenefitTypeWeapon) && pu->special && !pu->dropped)
    {
        num = RoundToInteger(be->amount * 2.5);
    }

    if (ammo == kAmmunitionTypeNoAmmo || num <= 0)
        return;

    if (ammo < 0 || ammo >= kTotalAmmunitionTypes)
        FatalError("GiveAmmo: bad type %i", ammo);

    if (pu->lose_them)
    {
        if (pu->player->ammo_[ammo].count == 0)
            return;

        pu->player->ammo_[ammo].count -= num;

        if (pu->player->ammo_[ammo].count < 0)
            pu->player->ammo_[ammo].count = 0;

        pu->got_it = true;
        return;
    }

    // In Nightmare you need the extra ammo, in "baby" you are given double
    if (pu->special)
    {
        if ((game_skill == kSkillBaby) || (game_skill == kSkillNightmare))
            num <<= 1;
    }

    bool did_pickup = false;

    // for newly acquired weapons (in the same benefit list) which have
    // a clip, try to "bundle" this ammo inside that clip.
    if (pu->new_weapon >= 0)
    {
        did_pickup = TryFillNewWeapon(pu->player, pu->new_weapon, (AmmunitionType)ammo, &num);

        if (num == 0)
        {
            pu->got_it = true;
            return;
        }
    }

    // divide by two _here_, which means that the ammo for filling
    // clip weapons is not affected by the kMapObjectFlagDropped flag.
    if (num > 1 && pu->dropped)
        num /= 2;

    if (pu->player->ammo_[ammo].count == pu->player->ammo_[ammo].maximum)
    {
        if (did_pickup)
            pu->got_it = true;
        return;
    }

    // if there is some fresh ammo, we should change weapons
    if (pu->player->ammo_[ammo].count == 0)
        pu->new_ammo = ammo;

    pu->player->ammo_[ammo].count += num;

    if (pu->player->ammo_[ammo].count > pu->player->ammo_[ammo].maximum)
        pu->player->ammo_[ammo].count = pu->player->ammo_[ammo].maximum;

    pu->got_it = true;
}

//
// GiveAmmoLimit
//
static void GiveAmmoLimit(PickupInfo *pu, Benefit *be)
{
    int ammo  = be->sub.type;
    int limit = RoundToInteger(be->amount);

    if (ammo == kAmmunitionTypeNoAmmo)
        return;

    if (ammo < 0 || ammo >= kTotalAmmunitionTypes)
        FatalError("GiveAmmoLimit: bad type %i", ammo);

    if ((!pu->lose_them && limit < pu->player->ammo_[ammo].maximum) ||
        (pu->lose_them && limit > pu->player->ammo_[ammo].maximum))
    {
        return;
    }

    pu->player->ammo_[ammo].maximum = limit;

    // new limit could be lower...
    if (pu->player->ammo_[ammo].count > pu->player->ammo_[ammo].maximum)
        pu->player->ammo_[ammo].count = pu->player->ammo_[ammo].maximum;

    pu->got_it = true;
}

//
// GiveWeapon
//
// The weapon thing may have a kMapObjectFlagDropped flag or'ed in.
//
// -AJA- 2000/03/02: Reworked for new Benefit stuff.
//
static void GiveWeapon(PickupInfo *pu, Benefit *be)
{
    WeaponDefinition *info = be->sub.weap;
    int               pw_index;

    EPI_ASSERT(info);

    if (pu->lose_them)
    {
        if (RemoveWeapon(pu->player, info))
            pu->got_it = true;
        return;
    }

    // special handling for CO-OP and OLD DeathMatch
    if (total_players > 1 && deathmatch != 2 && pu->special && !pu->dropped)
    {
        if (!AddWeapon(pu->player, info, &pw_index))
        {
            pu->no_ammo = true;
            return;
        }

        pu->new_weapon = pw_index;
        pu->keep_it    = true;
        pu->got_it     = true;
        return;
    }

    if (!AddWeapon(pu->player, info, &pw_index))
        return;

    pu->new_weapon = pw_index;
    pu->got_it     = true;
}

//
// GiveHealth
//
// Returns false if not health is not needed,
//
// New Procedure: -ACB- 1998/06/21
//
static void GiveHealth(PickupInfo *pu, Benefit *be)
{
    if (pu->lose_them)
    {
        // DamageMapObject(pu->player->map_object_, pu->special, nullptr,
        // be->amount, nullptr);
        if (pu->player->health_ <= 0)
            return;

        pu->player->health_ -= be->amount;
        pu->player->map_object_->health_ = pu->player->health_;

        if (pu->player->map_object_->health_ <= 0)
        {
            KillMapObject(nullptr, pu->player->map_object_);
            // return;
        }

        pu->got_it = true;
        return;
    }

    if (pu->player->health_ >= be->limit)
        return;

    pu->player->health_ += be->amount;

    if (pu->player->health_ > be->limit)
        pu->player->health_ = be->limit;

    pu->player->map_object_->health_ = pu->player->health_;

    pu->got_it = true;
}

//
// GiveArmour
//
// Returns false if the new armour would not benefit
//
static void GiveArmour(PickupInfo *pu, Benefit *be)
{
    ArmourType a_class = (ArmourType)be->sub.type;

    EPI_ASSERT(0 <= a_class && a_class < kTotalArmourTypes);

    if (pu->lose_them)
    {
        if (AlmostEquals(pu->player->armours_[a_class], 0.0f))
            return;

        pu->player->armours_[a_class] -= be->amount;
        if (pu->player->armours_[a_class] < 0)
            pu->player->armours_[a_class] = 0;

        UpdateTotalArmour(pu->player);

        pu->got_it = true;
        return;
    }

    float amount  = be->amount;
    float upgrade = 0;

    if (!pu->special || (pu->special->extended_flags_ & kExtendedFlagSimpleArmour))
    {
        float slack = be->limit - pu->player->armours_[a_class];

        if (amount > slack)
            amount = slack;

        if (amount <= 0)
            return;
    }
    else /* Doom emulation */
    {
        float slack = be->limit - pu->player->total_armour_;

        if (slack < 0)
            return;

        // we try to Upgrade any lower class armour with this armour.
        for (int cl = a_class - 1; cl >= 0; cl--)
        {
            upgrade += pu->player->armours_[cl];
        }

        // cannot upgrade more than the specified amount
        if (upgrade > amount)
            upgrade = amount;

        slack += upgrade;

        if (amount > slack)
            amount = slack;

        EPI_ASSERT(amount >= 0);
        EPI_ASSERT(upgrade >= 0);

        if (AlmostEquals(amount, 0.0f) && AlmostEquals(upgrade, 0.0f))
            return;
    }

    pu->player->armours_[a_class] += amount;

    // -AJA- 2007/08/22: armor associations
    if (pu->special && pu->special->info_->armour_protect_ >= 0)
    {
        pu->player->armour_types_[a_class] = pu->special->info_;
    }

    if (upgrade > 0)
    {
        for (int cl = a_class - 1; (cl >= 0) && (upgrade > 0); cl--)
        {
            if (pu->player->armours_[cl] >= upgrade)
            {
                pu->player->armours_[cl] -= upgrade;
                break;
            }
            else if (pu->player->armours_[cl] > 0)
            {
                upgrade -= pu->player->armours_[cl];
                pu->player->armours_[cl] = 0;
            }
        }
    }

    UpdateTotalArmour(pu->player);

    pu->got_it = true;
}

//
// GiveKey
//
static void GiveKey(PickupInfo *pu, Benefit *be)
{
    DoorKeyType key = (DoorKeyType)be->sub.type;

    if (pu->lose_them)
    {
        if (!(pu->player->cards_ & key))
            return;

        pu->player->cards_ = (DoorKeyType)(pu->player->cards_ & ~key);
    }
    else
    {
        if (pu->player->cards_ & key)
            return;

        pu->player->cards_ = (DoorKeyType)(pu->player->cards_ | key);
    }

    // -AJA- leave keys in Co-op games
    if (InCooperativeMatch())
        pu->keep_it = true;

    pu->got_it = true;
}

//
// GivePower
//
// DDF Change: duration is now passed as a parameter, for the berserker
//             the value is the health given, extendedflags also passed.
//
// The code was changes to a switch instead of a series of if's, also
// included is the use of limit, which gives a maxmium amount of protection
// for this item. -ACB- 1998/06/20
//
static void GivePower(PickupInfo *pu, Benefit *be)
{
    // -ACB- 1998/06/20 - calculate duration in seconds
    float duration = be->amount * kTicRate;
    float limit    = be->limit * kTicRate;

    if (pu->lose_them)
    {
        if (AlmostEquals(pu->player->powers_[be->sub.type], 0.0f))
            return;

        pu->player->powers_[be->sub.type] -= duration;

        if (pu->player->powers_[be->sub.type] < 0)
            pu->player->powers_[be->sub.type] = 0;

        pu->got_it = true;
        return;
    }

    if (pu->player->powers_[be->sub.type] >= limit)
        return;

    pu->player->powers_[be->sub.type] += duration;

    if (pu->player->powers_[be->sub.type] > limit)
        pu->player->powers_[be->sub.type] = limit;

    // special handling for scuba...
    if (be->sub.type == kPowerTypeScuba)
        pu->player->air_in_lungs_ = pu->player->map_object_->info_->lung_capacity_;

    // deconflict fuzzy and translucent style partial invis
    if (be->sub.type == kPowerTypePartInvisTranslucent)
        pu->player->powers_[kPowerTypePartInvis] = 0;
    else if (be->sub.type == kPowerTypePartInvis)
        pu->player->powers_[kPowerTypePartInvisTranslucent] = 0;

    pu->got_it = true;
}

static void DoGiveBenefitList(PickupInfo *pu)
{
    // handle weapons first, since this affects ammo handling

    for (Benefit *be = pu->list; be; be = be->next)
    {
        if (be->type == kBenefitTypeWeapon && be->amount >= 0.0)
            GiveWeapon(pu, be);
    }

    for (Benefit *be = pu->list; be; be = be->next)
    {
        // Put the checking in for neg amounts at benefit level. Powerups can be
        // neg if they last all level. -ACB- 2004/02/04

        switch (be->type)
        {
        case kBenefitTypeNone:
        case kBenefitTypeWeapon:
            break;

        case kBenefitTypeAmmo:
            if (be->amount >= 0.0)
                GiveAmmo(pu, be);
            break;

        case kBenefitTypeAmmoLimit:
            if (be->amount >= 0.0)
                GiveAmmoLimit(pu, be);
            break;

        case kBenefitTypeKey:
            if (be->amount >= 0.0)
                GiveKey(pu, be);
            break;

        case kBenefitTypeHealth:
            if (be->amount >= 0.0)
                GiveHealth(pu, be);
            break;

        case kBenefitTypeArmour:
            if (be->amount >= 0.0)
                GiveArmour(pu, be);
            break;

        case kBenefitTypePowerup:
            GivePower(pu, be);
            break;

        case kBenefitTypeInventory:
            GiveInventory(pu, be);
            break;

        case kBenefitTypeInventoryLimit:
            GiveInventoryLimit(pu, be);
            break;

        case kBenefitTypeCounter:
            GiveCounter(pu, be);
            break;

        case kBenefitTypeCounterLimit:
            GiveCounterLimit(pu, be);
            break;

        default:
            break;
        }
    }
}

//
// HasBenefitInList
//
// Check if the player has at least one of the benefits in the provided list.
// Returns true if any of them are present for the player, but does not
// otherwise return any information about which benefits matched or what their
// amounts are.
//
bool HasBenefitInList(Player *player, Benefit *list)
{
    EPI_ASSERT(player && list);
    for (Benefit *be = list; be; be = be->next)
    {
        switch (be->type)
        {
        case kBenefitTypeNone:
            break;

        case kBenefitTypeWeapon:
            for (int i = 0; i < kMaximumWeapons; i++)
            {
                WeaponDefinition *cur_info = player->weapons_[i].info;
                if (cur_info == be->sub.weap)
                    return true;
            }
            break;

        case kBenefitTypeAmmo:
            if (player->ammo_[be->sub.type].count > be->amount)
                return true;
            break;

        case kBenefitTypeAmmoLimit:
            if (player->ammo_[be->sub.type].maximum > be->amount)
                return true;
            break;

        case kBenefitTypeKey:
            if (player->cards_ & (DoorKeyType)be->sub.type)
                return true;
            break;

        case kBenefitTypeHealth:
            if (player->health_ > be->amount)
                return true;
            break;

        case kBenefitTypeArmour:
            if (player->armours_[be->sub.type] > be->amount)
                return true;
            break;

        case kBenefitTypePowerup:
            if (!AlmostEquals(player->powers_[be->sub.type], 0.0f))
                return true;
            break;

        case kBenefitTypeInventory:
            if (player->inventory_[be->sub.type].count > be->amount)
                return true;
            break;

        case kBenefitTypeInventoryLimit:
            if (player->inventory_[be->sub.type].maximum > be->amount)
                return true;
            break;

        case kBenefitTypeCounter:
            if (player->counters_[be->sub.type].count > be->amount)
                return true;
            break;

        case kBenefitTypeCounterLimit:
            if (player->counters_[be->sub.type].maximum > be->amount)
                return true;
            break;

        default:
            break;
        }
    }
    return false;
}

//
// GiveBenefitList
//
// Give all the benefits in the list to the player.  `special' is the
// special object that all these benefits came from, or nullptr if they
// came from the initial_benefits list.  When `lose_them' is true, the
// benefits should be taken away instead.  Returns true if _any_
// benefit was picked up (or lost), or false if none of them were.
//
bool GiveBenefitList(Player *player, MapObject *special, Benefit *list, bool lose_them)
{
    PickupInfo info;

    info.list      = list;
    info.lose_them = lose_them;

    info.player  = player;
    info.special = special;
    info.dropped = false;

    info.new_weapon = -1;
    info.new_ammo   = -1;

    info.got_it  = false;
    info.keep_it = false;
    info.silent  = false;
    info.no_ammo = false;

    DoGiveBenefitList(&info);

    return info.got_it;
}

//
// RunPickupEffects
//
static void RunPickupEffects(Player *player, PickupEffect *list)
{
    for (; list; list = list->next_)
    {
        switch (list->type_)
        {
        case kPickupEffectTypeSwitchWeapon:
            PlayerSwitchWeapon(player, list->sub_.weap);
            break;

        case kPickupEffectTypeKeepPowerup:
            player->keep_powers_ |= (1 << list->sub_.type);
            break;

        case kPickupEffectTypePowerupEffect:
            // FIXME
            break;

        case kPickupEffectTypeScreenEffect:
            // FIXME
            break;

        default:
            break;
        }
    }
}

//
// TouchSpecialThing
//
// -KM- 1999/01/31 Things that give you item bonus are always
//  picked up.  Picked up object is set to death frame instead
//  of removed so that effects can happen.
//
void TouchSpecialThing(MapObject *special, MapObject *toucher)
{
    float delta = special->z - toucher->z;

    // out of reach
    if (delta > toucher->height_ || delta < -special->height_)
        return;

    if (!toucher->player_)
        return;

    // Dead thing touching. Can happen with a sliding player corpse.
    if (toucher->health_ <= 0)
        return;

    // Do not pick up the item if completely still
    if (AlmostEquals(toucher->momentum_.X, 0.0f) && AlmostEquals(toucher->momentum_.Y, 0.0f) &&
        AlmostEquals(toucher->momentum_.Z, 0.0f))
        return;

    // -KM- 1998/09/27 Sounds.ddf
    SoundEffect *sound = special->info_->activesound_;

    PickupInfo info;

    info.player  = toucher->player_;
    info.special = special;
    info.dropped = (special && (special->flags_ & kMapObjectFlagDropped)) ? true : false;

    info.new_weapon = -1; // the most recently added weapon (must be new)
    info.new_ammo   = -1; // got fresh ammo (old count was zero).

    info.got_it  = false;
    info.keep_it = false;
    info.silent  = false;
    info.no_ammo = false;

    // First handle lost benefits
    info.list      = special->info_->lose_benefits_;
    info.lose_them = true;
    DoGiveBenefitList(&info);

    // Run through the list of all pickup benefits...
    info.list      = special->info_->pickup_benefits_;
    info.lose_them = false;
    DoGiveBenefitList(&info);

    if (special->flags_ & kMapObjectFlagCountItem)
    {
        info.player->item_count_++;
        info.got_it = true;
    }
    else if (special->hyper_flags_ & kHyperFlagForcePickup)
    {
        info.got_it  = true;
        info.keep_it = false;
    }

    if (!info.got_it)
        return;

    if (!info.keep_it)
    {
        special->health_ = 0;
        if (time_stop_active) // Hide pickup after gaining benefit while time
                              // stop is still active
            special->visibility_ = 0.0f;
        KillMapObject(info.player->map_object_, special, nullptr);
    }

    // do all the special effects, lights & sound etc...
    if (!info.silent)
    {
        info.player->bonus_count_ += kBonusAddMinimum;
        if (info.player->bonus_count_ > kBonusLimit)
            info.player->bonus_count_ = kBonusLimit;

        if (toucher->player_ == players[display_player] && special->info_->pickup_message_ != "" &&
            language.IsValidRef(special->info_->pickup_message_.c_str()))
        {
            ConsoleMessage(kConsoleHUDTop, "%s", language[special->info_->pickup_message_]);
        }

        if (sound)
        {
            int sfx_cat;

            if (info.player == players[console_player])
                sfx_cat = kCategoryPlayer;
            else
                sfx_cat = kCategoryOpponent;

            StartSoundEffect(sound, sfx_cat, info.player->map_object_);
        }

        if (info.new_weapon >= 0 || info.new_ammo >= 0)
            TrySwitchNewWeapon(info.player, info.new_weapon, (AmmunitionType)info.new_ammo);
    }

    RunPickupEffects(info.player, special->info_->pickup_effects_);
}

// FIXME: move this into utility code
static std::string PatternSubstitution(const char *format, const std::vector<std::string> &keywords)
{
    std::string result;

    while (*format)
    {
        const char *pos = strchr(format, '%');

        if (!pos)
        {
            result = result + std::string(format);
            break;
        }

        if (pos > format)
            result = result + std::string(format, pos - format);

        pos++;

        char key[4];

        key[0] = *pos++;
        key[1] = 0;

        if (!key[0])
            break;

        if (epi::IsAlphaASCII(key[0]))
        {
            for (int i = 0; i + 1 < (int)keywords.size(); i += 2)
            {
                if (strcmp(key, keywords[i].c_str()) == 0)
                {
                    result = result + keywords[i + 1];
                    break;
                }
            }
        }
        else if (key[0] == '%')
        {
            result = result + std::string("%");
        }
        else
        {
            result = result + std::string("%");
            result = result + std::string(key);
        }

        format = pos;
    }

    return result;
}

static void DoObituary(const char *format, MapObject *victim, MapObject *killer)
{
    EPI_UNUSED(killer); // eventually use DDFLANG to actually state the killer - Dasho
    EPI_UNUSED(victim); // and uhhh the victim
    std::vector<std::string> keywords;

    keywords.push_back("o");
    keywords.push_back("the player");

    keywords.push_back("k");
    keywords.push_back("a foe");

    std::string msg = PatternSubstitution(format, keywords);

    if (victim->player_ == players[display_player])
        ConsoleMessage(kConsoleHUDTop, "%s", msg.c_str());
}

void ObituaryMessage(MapObject *victim, MapObject *killer, const DamageClass *damtype)
{
    if (!show_obituaries)
        return;

    if (damtype && !damtype->obituary_.empty())
    {
        const char *ref = damtype->obituary_.c_str();

        if (language.IsValidRef(ref))
        {
            DoObituary(language[ref], victim, killer);
            return;
        }

        LogDebug("Missing obituary entry in LDF: '%s'\n", ref);
    }

    if (killer)
        DoObituary("%o was killed.", victim, killer);
    else
        DoObituary("%o died.", victim, killer);
}

//
// KillMapObject
//
// Altered to reflect the fact that the dropped item is a pointer to
// MapObjectDefinition, uses new procedure: P_MobjCreateObject.
//
// Note: Damtype can be nullptr here.
//
// -ACB- 1998/08/01
//
// -AJA- 1999/09/12: Now uses P_SetMobjStateDeferred, since this
//       routine can be called by TryMove/CheckRelativeThingCallback/etc.
//
void KillMapObject(MapObject *source, MapObject *target, const DamageClass *damtype, bool weak_spot)
{
    // -AJA- 2006/09/10: Voodoo doll handling for coop
    if (target->player_ && target->player_->map_object_ != target)
    {
        KillMapObject(source, target->player_->map_object_, damtype, weak_spot);
        target->player_ = nullptr;
    }

    bool nofog = (target->flags_ & kMapObjectFlagSpecial);

    target->flags_ &= ~(kMapObjectFlagSpecial | kMapObjectFlagShootable | kMapObjectFlagFloat | kMapObjectFlagSkullFly |
                        kMapObjectFlagTouchy);
    target->extended_flags_ &= ~(kExtendedFlagBounce | kExtendedFlagUsable | kExtendedFlagClimbable);

    if (!(target->extended_flags_ & kExtendedFlagNoGravityOnKill))
        target->flags_ &= ~kMapObjectFlagNoGravity;

    target->flags_ |= kMapObjectFlagCorpse | kMapObjectFlagDropOff;
    target->height_ /= (4 / (target->mbf21_flags_ & kMBF21FlagLowGravity ? 8 : 1));

    ScriptUpdateMonsterDeaths(target);

    if (source && source->player_)
    {
        // count for intermission
        if (target->flags_ & kMapObjectFlagCountKill)
            source->player_->kill_count_++;

        if (target->info_->kill_benefits_)
        {
            PickupInfo info;
            info.player  = source->player_;
            info.special = nullptr;
            info.dropped = false;

            info.new_weapon = -1; // the most recently added weapon (must be new)
            info.new_ammo   = -1; // got fresh ammo (old count was zero).

            info.got_it  = false;
            info.keep_it = false;
            info.silent  = false;
            info.no_ammo = false;

            info.list      = target->info_->kill_benefits_;
            info.lose_them = false;
            DoGiveBenefitList(&info);
        }

        if (target->player_)
        {
            // Killed a team mate?
            if (target->side_ & source->side_)
            {
                source->player_->frags_--;
                source->player_->total_frags_--;
            }
            else
            {
                source->player_->frags_++;
                source->player_->total_frags_++;
            }
        }
    }
    else if (InSinglePlayerMatch() && (target->flags_ & kMapObjectFlagCountKill))
    {
        // count all monster deaths,
        // even those caused by other monsters
        players[console_player]->kill_count_++;
    }

    if (target->player_)
    {
        ObituaryMessage(target, source, damtype);

        // count environment kills against you
        if (!source)
        {
            target->player_->frags_--;
            target->player_->total_frags_--;
        }

        target->flags_ &= ~kMapObjectFlagSolid;
        target->player_->player_state_         = kPlayerDead;
        target->player_->standard_view_height_ = HMM_MIN(kDeathViewHeight, target->height_ / 3);
        target->player_->actual_speed_         = 0;

        DropWeapon(target->player_);

        // don't die in auto map, switch view prior to dying
        if (target->player_ == players[console_player] && automap_active)
            AutomapStop();

        // don't immediately restart when USE key was pressed
        if (target->player_ == players[console_player])
            ClearEventInput();
    }

    int  state    = 0;
    bool overkill = false;

    if (target->info_->gib_health_ < 0 && target->health_ < target->info_->gib_health_)
        overkill = true;
    else if (target->health_ < -target->spawn_health_)
        overkill = true;

    if (weak_spot)
    {
        state = MapObjectFindLabel(target, "WEAKDEATH");
        if (state == 0)
            overkill = true;
    }

    if (state == 0 && overkill && damtype && damtype->overkill_.label_ != "")
    {
        state = MapObjectFindLabel(target, damtype->overkill_.label_.c_str());
        if (state != 0)
            state += damtype->overkill_.offset_;
    }

    if (state == 0 && overkill && target->info_->overkill_state_)
        state = target->info_->overkill_state_;

    if (state == 0 && damtype && damtype->death_.label_ != "")
    {
        state = MapObjectFindLabel(target, damtype->death_.label_.c_str());
        if (state != 0)
            state += damtype->death_.offset_;
    }

    if (state == 0)
        state = target->info_->death_state_;

    if (gore_level.d_ == 2 && (target->flags_ & kMapObjectFlagCountKill)) // Hopefully the only things with
                                                                          // blood/gore_level are monsters and not
                                                                          // "barrels", etc
    {
        state = 0;
        if (!nofog)
        {
            MapObject *fog = CreateMapObject(target->x, target->y, target->z, mobjtypes.Lookup("TELEPORT_FLASH"));
            if (fog && fog->info_->chase_state_)
                MapObjectSetStateDeferred(fog, fog->info_->chase_state_, 0);
        }
    }

    if (target->hyper_flags_ & kHyperFlagDehackedCompatibility)
    {
        MapObjectSetState(target, state);
        target->tics_ -= RandomByteDeterministic() & 3;
        if (target->tics_ < 1)
            target->tics_ = 1;
    }
    else
    {
        MapObjectSetStateDeferred(target, state, RandomByteDeterministic() & 3);
    }

    // Drop stuff. This determines the kind of object spawned
    // during the death frame of a thing.
    const MapObjectDefinition *item = target->info_->dropitem_;
    if (item)
    {
        MapObject *mo = CreateMapObject(target->x, target->y, target->floor_z_, item);

        // -ES- 1998/07/18 nullptr check to prevent crashing
        if (mo)
            mo->flags_ |= kMapObjectFlagDropped;
    }
}

//
// ThrustMapObject
//
// Like DamageMapObject, but only pushes the target object around
// (doesn't inflict any damage).  Parameters are:
//
// * target    - mobj to be thrust.
// * inflictor - mobj causing the thrusting.
// * thrust    - amount of thrust done (same values as damage).  Can
//               be negative to "pull" instead of push.
//
// -AJA- 1999/11/06: Wrote this routine.
//
void ThrustMapObject(MapObject *target, MapObject *inflictor, float thrust)
{
    // check for immunity against the attack
    if (target->hyper_flags_ & kHyperFlagInvulnerable)
        return;

    // check for lead feet ;)
    if (target->hyper_flags_ & kHyperFlagImmovable)
        return;

    if (inflictor && inflictor->current_attack_ &&
        0 == (inflictor->current_attack_->attack_class_ & ~target->info_->immunity_))
    {
        return;
    }

    float dx = target->x - inflictor->x;
    float dy = target->y - inflictor->y;

    // don't thrust if at the same location (no angle)
    if (fabs(dx) < 1.0f && fabs(dy) < 1.0f)
        return;

    BAMAngle angle = PointToAngle(0, 0, dx, dy);

    // -ACB- 2000/03/11 Div-by-zero check...
    EPI_ASSERT(!AlmostEquals(target->info_->mass_, 0.0f));

    float push = 12.0f * thrust / target->info_->mass_;

    // limit thrust to reasonable values
    if (push < -40.0f)
        push = -40.0f;
    if (push > 40.0f)
        push = 40.0f;

    target->momentum_.X += push * epi::BAMCos(angle);
    target->momentum_.Y += push * epi::BAMSin(angle);

    if (level_flags.true_3d_gameplay)
    {
        float dz       = MapObjectMidZ(target) - MapObjectMidZ(inflictor);
        float slope    = ApproximateSlope(dx, dy, dz);
        float z_thrust = push * slope / 2;
        // Don't apply downward Z momentum if the target is on the ground
        // (this was screwing up mikoportal/peccaflight levels - Dasho)
        if (z_thrust >= 0.0f || target->z > target->floor_z_)
            target->momentum_.Z += push * slope / 2;
    }
}

//
// PushMapObject
//
// Like DamageMapObject, but only pushes the target object around
// (doesn't inflict any damage).  Parameters are:
//
// * target    - mobj to be thrust.
// * inflictor - mobj causing the thrusting.
// * thrust    - amount of thrust done (same values as damage).  Can
//               be negative to "pull" instead of push.
//
// -Lobo- 2022/07/07: Created this routine.
//
void PushMapObject(MapObject *target, MapObject *inflictor, float thrust)
{
    /*
    if(tm_I.mover->momentum_.x > tm_I.mover->momentum_.y)
        ThrustSpeed = fabsf(tm_I.mover->momentum_.x);
    else
        ThrustSpeed = fabsf(tm_I.mover->momentum_.y);
    */

    float dx = target->x - inflictor->x;
    float dy = target->y - inflictor->y;

    // don't thrust if at the same location (no angle)
    if (fabs(dx) < 1.0f && fabs(dy) < 1.0f)
        return;

    BAMAngle angle = PointToAngle(0, 0, dx, dy);

    // -ACB- 2000/03/11 Div-by-zero check...
    EPI_ASSERT(!AlmostEquals(target->info_->mass_, 0.0f));

    float push = 12.0f * thrust / target->info_->mass_;

    // limit thrust to reasonable values
    if (push < -40.0f)
        push = -40.0f;
    if (push > 40.0f)
        push = 40.0f;

    target->momentum_.X += push * epi::BAMCos(angle);
    target->momentum_.Y += push * epi::BAMSin(angle);

    if (level_flags.true_3d_gameplay)
    {
        float dz    = MapObjectMidZ(target) - MapObjectMidZ(inflictor);
        float slope = ApproximateSlope(dx, dy, dz);

        target->momentum_.Z += push * slope / 2;
    }
}

//
// DamageMapObject
//
// Damages both enemies and players, decreases the amount of health
// an mobj has and "kills" an mobj in the event of health being 0 or
// less, the parameters are:
//
// * Target    - mobj to be damaged.
// * Inflictor - mobj which is causing the damage.
// * Source    - mobj who is responsible for doing the damage. Can be nullptr
// * Amount    - amount of damage done.
// * Damtype   - type of damage (for override states).  Can be nullptr
//
// Both source and inflictor can be nullptr, slime damage and barrel
// explosions etc....
//
// -AJA- 1999/09/12: Now uses P_SetMobjStateDeferred, since this
//       routine can be called by TryMove/CheckRelativeThingCallback/etc.
//
void DamageMapObject(MapObject *target, MapObject *inflictor, MapObject *source, float damage,
                     const DamageClass *damtype, bool weak_spot)
{
    if (target->IsRemoved())
        return;

    if (!(target->flags_ & kMapObjectFlagShootable))
        return;

    if (target->health_ <= 0)
        return;

    // check for immunity against the attack
    if (target->hyper_flags_ & kHyperFlagInvulnerable)
        return;

    if (!weak_spot && inflictor && inflictor->current_attack_ &&
        0 == (inflictor->current_attack_->attack_class_ & ~target->info_->immunity_))
    {
        return;
    }

    // sanity check : don't produce references to removed objects
    if (inflictor && inflictor->IsRemoved())
        inflictor = nullptr;
    if (source && source->IsRemoved())
        source = nullptr;

    // check for immortality
    if (target->hyper_flags_ & kHyperFlagImmortal)
        damage = 0.0f; // do no damage

    // check for partial resistance against the attack
    if (!weak_spot && damage >= 0.1f && inflictor && inflictor->current_attack_ &&
        0 == (inflictor->current_attack_->attack_class_ & ~target->info_->resistance_))
    {
        damage = HMM_MAX(0.05f, damage * target->info_->resist_multiply_);
    }

    // -ACB- 1998/07/12 Use Visibility Enum
    // A Damaged Stealth Creature becomes more visible
    if (target->flags_ & kMapObjectFlagStealth)
        target->target_visibility_ = 1.0f;

    if (target->flags_ & kMapObjectFlagSkullFly)
    {
        target->momentum_.X = target->momentum_.Y = target->momentum_.Z = 0;
        target->flags_ &= ~kMapObjectFlagSkullFly;
    }

    Player *player = target->player_;

    // Some close combat weapons should not
    // inflict thrust and push the victim out of reach,
    // thus kick away unless using the chainsaw.

    if (inflictor && !(target->flags_ & kMapObjectFlagNoClip) &&
        !(source && source->player_ && source->player_->ready_weapon_ >= 0 &&
          source->player_->weapons_[source->player_->ready_weapon_].info->nothrust_))
    {
        // make fall forwards sometimes
        if (damage < 40 && damage > target->health_ && target->z - inflictor->z > 64 && (RandomByteDeterministic() & 1))
        {
            ThrustMapObject(target, inflictor, -damage * 4);
        }
        else
            ThrustMapObject(target, inflictor, damage);
    }

    // player specific
    if (player)
    {
        int i;

        // Don't damage player if sector type shouldn't affect players
        if (damtype && damtype->only_affects_)
        {
            if (!(damtype->only_affects_ & epi::BitSetFromChar('P')))
                return;
        }

        // ignore damage in GOD mode, or with INVUL powerup
        if ((player->cheats_ & kCheatingGodMode) || player->powers_[kPowerTypeInvulnerable] > 0)
        {
            if (!damtype)
                return;
            else if (!damtype->bypass_all_ && !damtype->damage_if_)
                return;
        }

        // Check for DAMAGE_UNLESS/DAMAGE_IF DDF specials
        if (damtype && (damtype->damage_unless_ || damtype->damage_if_))
        {
            bool unless_damage = (damtype->damage_unless_ != nullptr);
            bool if_damage     = false;
            if (damtype->damage_unless_ && HasBenefitInList(player, damtype->damage_unless_))
                unless_damage = false;
            if (damtype->damage_if_ && HasBenefitInList(player, damtype->damage_if_))
                if_damage = true;
            if (!unless_damage && !if_damage && !damtype->bypass_all_)
                return;
        }

        // take half damage in trainer mode
        if (game_skill == kSkillBaby)
            damage /= 2.0f;

        // preliminary check: immunity and resistance
        for (i = kTotalArmourTypes - 1; i >= kArmourTypeGreen; i--)
        {
            if (damtype && damtype->no_armour_)
                continue;

            if (player->armours_[i] <= 0)
                continue;

            const MapObjectDefinition *arm_info = player->armour_types_[i];

            if (!arm_info || !inflictor || !inflictor->current_attack_)
                continue;

            // this armor does not provide any protection for this attack
            if (0 != (inflictor->current_attack_->attack_class_ & ~arm_info->armour_class_))
                continue;

            if (0 == (inflictor->current_attack_->attack_class_ & ~arm_info->immunity_))
                return; /* immune : we can go home early! */

            if (damage > 0.05f && 0 == (inflictor->current_attack_->attack_class_ & ~arm_info->resistance_))
            {
                damage = HMM_MAX(0.05f, damage * arm_info->resist_multiply_);
            }
        }

        // Bot Deathmatch Damange Resistance check
        if (InDeathmatch() && !player->IsBot() && source && source->player_ && source->player_->IsBot())
        {
            if (player_deathmatch_damage_resistance.d_ < 9)
            {
                float mul = 1.90f - (player_deathmatch_damage_resistance.d_ * 0.10f);
                damage *= mul;
            }
            else if (player_deathmatch_damage_resistance.d_ > 9)
            {
                float mul = 0.10f + ((18 - player_deathmatch_damage_resistance.d_) * 0.10f);
                damage    = HMM_MAX(0.1f, damage * mul);
            }
        }

        // check which armour can take some damage
        for (i = kTotalArmourTypes - 1; i >= kArmourTypeGreen; i--)
        {
            if (damtype && damtype->no_armour_)
                continue;

            if (player->armours_[i] <= 0)
                continue;

            const MapObjectDefinition *arm_info = player->armour_types_[i];

            // this armor does not provide any protection for this attack
            if (arm_info && inflictor && inflictor->current_attack_ &&
                0 != (inflictor->current_attack_->attack_class_ & ~arm_info->armour_class_))
            {
                continue;
            }

            float saved = 0;

            if (arm_info)
                saved = damage * arm_info->armour_protect_;
            else
            {
                switch (i)
                {
                case kArmourTypeGreen:
                    saved = damage * 0.33;
                    break;
                case kArmourTypeBlue:
                    saved = damage * 0.50;
                    break;
                case kArmourTypePurple:
                    saved = damage * 0.66;
                    break;
                case kArmourTypeYellow:
                    saved = damage * 0.75;
                    break;
                case kArmourTypeRed:
                    saved = damage * 0.90;
                    break;

                default:
                    FatalError("INTERNAL ERROR in DamageMapObject: bad armour "
                               "%d\n",
                               i);
                }
            }

            if (player->armours_[i] <= saved)
            {
                // armour is used up
                saved = player->armours_[i];
            }

            damage -= saved;

            if (arm_info)
                saved *= arm_info->armour_deplete_;

            player->armours_[i] -= saved;

            // don't apply inner armour unless outer is finished
            if (player->armours_[i] > 0)
                break;

            player->armours_[i] = 0;
        }

        UpdateTotalArmour(player);

        player->attacker_ = source;

        // instakill sectors
        if (damtype && damtype->instakill_)
            damage = target->player_->health_ + 1;

        // add damage after armour / invuln detection
        if (damage > 0)
        {
            // Change damage color if new inflicted damage is greater than
            // current processed damage
            if (damage >= player->damage_count_)
            {
                if (damtype && damtype->damage_flash_colour_ != kRGBANoValue)
                    player->last_damage_colour_ = damtype->damage_flash_colour_;
                else
                    player->last_damage_colour_ = current_map->episode_->default_damage_flash_;
            }

            player->damage_count_ += (int)HMM_MAX(damage, kDamageAddMinimum);
            player->damage_pain_ += damage;
        }

        // teleport stomp does 10k points...
        if (player->damage_count_ > kDamageLimit)
            player->damage_count_ = kDamageLimit;
    }
    else
    {
        // instakill sectors
        if (damtype && damtype->instakill_)
            damage = target->health_ + 1;
    }

    // do the damage
    target->health_ -= damage;

    if (player)
    {
        // mirror mobj health here for Dave
        // player->health_ = HMM_MAX(0, target->health);

        // Dasho 2023.09.05: The above behavior caused inconsistencies when
        // multiple voodoo dolls were present in a level (i.e., heavily damaging
        // one and then lightly damaging another one that was previously at full
        // health would "heal" the player)
        player->health_ = HMM_MAX(0, player->health_ - damage);
    }

    // Lobo 2023: Handle attack flagged with the "PLAYER_ATTACK" special.
    //  This attack will always be treated as originating from the player, even
    //  if it's an indirect secondary attack. This way the player gets his
    //  VAMPIRE health and KillBenefits.
    if (inflictor && inflictor->current_attack_ && (inflictor->current_attack_->flags_ & kAttackFlagPlayer))
    {
        Player *current_player;
        current_player = players[console_player];

        source = current_player->map_object_;

        if (source && source->IsRemoved()) // Sanity check?
            source = nullptr;
    }

    // -AJA- 2007/11/06: vampire mode!
    if (source && source != target && source->health_ < source->spawn_health_ &&
        ((source->hyper_flags_ & kHyperFlagVampire) ||
         (inflictor && inflictor->current_attack_ && (inflictor->current_attack_->flags_ & kAttackFlagVampire))))
    {
        float qty = (target->player_ ? 0.5 : 0.25) * damage;

        source->health_ = HMM_MIN(source->health_ + qty, source->spawn_health_);

        if (source->player_)
            source->player_->health_ = HMM_MIN(source->player_->health_ + qty, source->spawn_health_);
    }

    if (target->health_ <= 0)
    {
        KillMapObject(source, target, damtype, weak_spot);
        return;
    }

    // enter pain states
    float pain_chance;
    bool resistance_spot = false; 

    if (inflictor && inflictor->current_attack_ &&
             0 == (inflictor->current_attack_->attack_class_ & ~target->info_->resistance_))
        resistance_spot = true;

    if (target->flags_ & kMapObjectFlagSkullFly)
        pain_chance = 0;
    else if (weak_spot && target->info_->weak_.painchance_ >= 0)
        pain_chance = target->info_->weak_.painchance_;
    else if (resistance_spot && target->info_->resist_painchance_ >= 0)
        pain_chance = target->info_->resist_painchance_;
    else
        pain_chance = target->pain_chance_; // Lobo 2023: use dynamic painchance
    // pain_chance = target->info->painchance_;

    if (pain_chance > 0 && RandomByteTestDeterministic(pain_chance))
    {
        // setup to hit back
        target->flags_ |= kMapObjectFlagJustHit;

        int state = 0;

        if (weak_spot)
            state = MapObjectFindLabel(target, "WEAKPAIN");

        if (resistance_spot)
            state = MapObjectFindLabel(target, "RESISTANCEPAIN");

        if (state == 0 && damtype && damtype->pain_.label_ != "")
        {
            state = MapObjectFindLabel(target, damtype->pain_.label_.c_str());
            if (state != 0)
                state += damtype->pain_.offset_;
        }

        if (state == 0)
            state = target->info_->pain_state_;

        if (state != 0)
            MapObjectSetStateDeferred(target, state, 0);
    }

    // we're awake now...
    target->reaction_time_ = 0;

    bool ultra_loyal =
        (source && (target->hyper_flags_ & kHyperFlagUltraLoyal) && (source->side_ & target->side_) != 0);

    if ((!target->threshold_ || target->extended_flags_ & kExtendedFlagNoGrudge) && source && source != target &&
        (!(source->extended_flags_ & kExtendedFlagNeverTarget)) && !target->player_ && !ultra_loyal)
    {
        // if not intent on another player, chase after this one
        target->SetTarget(source);
        target->threshold_ = kBaseThreshold;

        if (target->state_ == &states[target->info_->idle_state_] && target->info_->chase_state_)
        {
            MapObjectSetStateDeferred(target, target->info_->chase_state_, 0);
        }
    }
}

//
// For killing monsters and players when something teleports on top
// of them.  Even the invulnerability powerup doesn't stop it.  Also
// used for the kill-all cheat.  Inflictor and damtype can be nullptr.
//
void TelefragMapObject(MapObject *target, MapObject *inflictor, const DamageClass *damtype)
{
    if (target->health_ <= 0)
        return;

    target->health_ = -1000;

    if (target->flags_ & kMapObjectFlagStealth)
        target->target_visibility_ = 1.0f;

    if (target->flags_ & kMapObjectFlagSkullFly)
    {
        target->momentum_.X = target->momentum_.Y = target->momentum_.Z = 0;
        target->flags_ &= ~kMapObjectFlagSkullFly;
    }

    if (target->player_)
    {
        target->player_->attacker_     = inflictor;
        target->player_->damage_count_ = kDamageLimit;
        target->player_->damage_pain_  = target->spawn_health_;
    }

    KillMapObject(inflictor, target, damtype);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
