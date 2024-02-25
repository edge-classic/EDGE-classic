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



#include "am_map.h"
#include "con_main.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "e_input.h"
#include "dstrings.h"
#include "m_random.h"
#include "p_local.h"
#include "rad_trig.h"
#include "r_misc.h"
#include "s_sound.h"
#include "str_util.h"

#include "AlmostEquals.h"

#define BONUS_ADD   6
#define BONUS_LIMIT 100

#define DAMAGE_ADD_MIN 3
#define DAMAGE_LIMIT   100

bool var_obituaries = true;

extern ConsoleVariable g_gore;
extern ConsoleVariable player_deathmatch_damage_resistance;

typedef struct
{
    Benefit *list;    // full list of benefits
    bool       lose_em; // lose stuff if true

    player_t *player;  // player picking it up
    mobj_t   *special; // object to pick up
    bool      dropped; // object was dropped by a monster

    int new_weap; // index (for player) of a new weapon, -1 = none
    int new_ammo; // ammotype of new ammo, -1 = none

    bool got_it;  // player actually got the benefit
    bool keep_it; // don't remove the thing from map
    bool silent;  // don't make sound/flash/effects
    bool no_ammo; // skip ammo
} pickup_info_t;

static bool P_CheckForBenefit(Benefit *list, int kind)
{
    for (Benefit *be = list; be != nullptr; be = be->next)
    {
        if (be->type == kind)
            return true;
    }

    return false;
}

//
// P_GiveCounter
//
// Returns false if the "counter" item can't be picked up at all
//
//
static void GiveCounter(pickup_info_t *pu, Benefit *be)
{
    int cntr = be->sub.type;
    int num  = RoundToInteger(be->amount);

    if (cntr < 0 || cntr >= kTotalCounterTypes)
        FatalError("GiveCounter: bad type %i", cntr);

    if (pu->lose_em)
    {
        if (pu->player->counters[cntr].num == 0)
            return;

        pu->player->counters[cntr].num -= num;

        if (pu->player->counters[cntr].num < 0)
            pu->player->counters[cntr].num = 0;

        pu->got_it = true;
        return;
    }

    if (pu->player->counters[cntr].num == pu->player->counters[cntr].max)
    {
        return;
    }

    pu->player->counters[cntr].num += num;

    if (pu->player->counters[cntr].num > pu->player->counters[cntr].max)
        pu->player->counters[cntr].num = pu->player->counters[cntr].max;

    pu->got_it = true;
}

//
// GiveCounterLimit
//
static void GiveCounterLimit(pickup_info_t *pu, Benefit *be)
{
    int cntr  = be->sub.type;
    int limit = RoundToInteger(be->amount);

    if (cntr < 0 || cntr >= kTotalCounterTypes)
        FatalError("GiveCounterLimit: bad type %i", cntr);

    if ((!pu->lose_em && limit < pu->player->counters[cntr].max) ||
        (pu->lose_em && limit > pu->player->counters[cntr].max))
    {
        return;
    }

    pu->player->counters[cntr].max = limit;

    // new limit could be lower...
    if (pu->player->counters[cntr].num > pu->player->counters[cntr].max)
        pu->player->counters[cntr].num = pu->player->counters[cntr].max;

    pu->got_it = true;
}

//
// P_GiveInventory
//
// Returns false if the inventory item can't be picked up at all
//
//
static void GiveInventory(pickup_info_t *pu, Benefit *be)
{
    int inv = be->sub.type;
    int num = RoundToInteger(be->amount);

    if (inv < 0 || inv >= kTotalInventoryTypes)
        FatalError("GiveInventory: bad type %i", inv);

    if (pu->lose_em)
    {
        if (pu->player->inventory[inv].num == 0)
            return;

        pu->player->inventory[inv].num -= num;

        if (pu->player->inventory[inv].num < 0)
            pu->player->inventory[inv].num = 0;

        pu->got_it = true;
        return;
    }

    if (pu->player->inventory[inv].num == pu->player->inventory[inv].max)
    {
        return;
    }

    pu->player->inventory[inv].num += num;

    if (pu->player->inventory[inv].num > pu->player->inventory[inv].max)
        pu->player->inventory[inv].num = pu->player->inventory[inv].max;

    pu->got_it = true;
}

//
// GiveInventoryLimit
//
static void GiveInventoryLimit(pickup_info_t *pu, Benefit *be)
{
    int inv   = be->sub.type;
    int limit = RoundToInteger(be->amount);

    if (inv < 0 || inv >= kTotalInventoryTypes)
        FatalError("GiveInventoryLimit: bad type %i", inv);

    if ((!pu->lose_em && limit < pu->player->inventory[inv].max) ||
        (pu->lose_em && limit > pu->player->inventory[inv].max))
    {
        return;
    }

    pu->player->inventory[inv].max = limit;

    // new limit could be lower...
    if (pu->player->inventory[inv].num > pu->player->inventory[inv].max)
        pu->player->inventory[inv].num = pu->player->inventory[inv].max;

    pu->got_it = true;
}

//
// P_GiveAmmo
//
// Returns false if the ammo can't be picked up at all
//
// -ACB- 1998/06/19 DDF Change: Number passed is the exact amount of ammo given.
// -KM- 1998/11/25 Handles weapon change from priority.
//
static void GiveAmmo(pickup_info_t *pu, Benefit *be)
{
    if (pu->no_ammo)
        return;

    int ammo = be->sub.type;
    int num  = RoundToInteger(be->amount);

    // -AJA- in old deathmatch, weapons give 2.5 times more ammo
    if (deathmatch == 1 && P_CheckForBenefit(pu->list, kBenefitTypeWeapon) && pu->special && !pu->dropped)
    {
        num = RoundToInteger(be->amount * 2.5);
    }

    if (ammo == kAmmunitionTypeNoAmmo || num <= 0)
        return;

    if (ammo < 0 || ammo >= kTotalAmmunitionTypes)
        FatalError("GiveAmmo: bad type %i", ammo);

    if (pu->lose_em)
    {
        if (pu->player->ammo[ammo].num == 0)
            return;

        pu->player->ammo[ammo].num -= num;

        if (pu->player->ammo[ammo].num < 0)
            pu->player->ammo[ammo].num = 0;

        pu->got_it = true;
        return;
    }

    // In Nightmare you need the extra ammo, in "baby" you are given double
    if (pu->special)
    {
        if ((game_skill == sk_baby) || (game_skill == sk_nightmare))
            num <<= 1;
    }

    bool did_pickup = false;

    // for newly acquired weapons (in the same benefit list) which have
    // a clip, try to "bundle" this ammo inside that clip.
    if (pu->new_weap >= 0)
    {
        did_pickup = P_TryFillNewWeapon(pu->player, pu->new_weap, (AmmunitionType)ammo, &num);

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

    if (pu->player->ammo[ammo].num == pu->player->ammo[ammo].max)
    {
        if (did_pickup)
            pu->got_it = true;
        return;
    }

    // if there is some fresh ammo, we should change weapons
    if (pu->player->ammo[ammo].num == 0)
        pu->new_ammo = ammo;

    pu->player->ammo[ammo].num += num;

    if (pu->player->ammo[ammo].num > pu->player->ammo[ammo].max)
        pu->player->ammo[ammo].num = pu->player->ammo[ammo].max;

    pu->got_it = true;
}

//
// GiveAmmoLimit
//
static void GiveAmmoLimit(pickup_info_t *pu, Benefit *be)
{
    int ammo  = be->sub.type;
    int limit = RoundToInteger(be->amount);

    if (ammo == kAmmunitionTypeNoAmmo)
        return;

    if (ammo < 0 || ammo >= kTotalAmmunitionTypes)
        FatalError("GiveAmmoLimit: bad type %i", ammo);

    if ((!pu->lose_em && limit < pu->player->ammo[ammo].max) || (pu->lose_em && limit > pu->player->ammo[ammo].max))
    {
        return;
    }

    pu->player->ammo[ammo].max = limit;

    // new limit could be lower...
    if (pu->player->ammo[ammo].num > pu->player->ammo[ammo].max)
        pu->player->ammo[ammo].num = pu->player->ammo[ammo].max;

    pu->got_it = true;
}

//
// GiveWeapon
//
// The weapon thing may have a kMapObjectFlagDropped flag or'ed in.
//
// -AJA- 2000/03/02: Reworked for new Benefit stuff.
//
static void GiveWeapon(pickup_info_t *pu, Benefit *be)
{
    WeaponDefinition *info = be->sub.weap;
    int          pw_index;

    SYS_ASSERT(info);

    if (pu->lose_em)
    {
        if (P_RemoveWeapon(pu->player, info))
            pu->got_it = true;
        return;
    }

    // special handling for CO-OP and OLD DeathMatch
    if (numplayers > 1 && deathmatch != 2 && pu->special && !pu->dropped)
    {
        if (!P_AddWeapon(pu->player, info, &pw_index))
        {
            pu->no_ammo = true;
            return;
        }

        pu->new_weap = pw_index;
        pu->keep_it  = true;
        pu->got_it   = true;
        return;
    }

    if (!P_AddWeapon(pu->player, info, &pw_index))
        return;

    pu->new_weap = pw_index;
    pu->got_it   = true;
}

//
// GiveHealth
//
// Returns false if not health is not needed,
//
// New Procedure: -ACB- 1998/06/21
//
static void GiveHealth(pickup_info_t *pu, Benefit *be)
{
    if (pu->lose_em)
    {
        // P_DamageMobj(pu->player->mo, pu->special, nullptr, be->amount, nullptr);
        if (pu->player->health <= 0)
            return;

        pu->player->health -= be->amount;
        pu->player->mo->health = pu->player->health;

        if (pu->player->mo->health <= 0)
        {
            P_KillMobj(nullptr, pu->player->mo);
            // return;
        }

        pu->got_it = true;
        return;
    }

    if (pu->player->health >= be->limit)
        return;

    pu->player->health += be->amount;

    if (pu->player->health > be->limit)
        pu->player->health = be->limit;

    pu->player->mo->health = pu->player->health;

    pu->got_it = true;
}

//
// GiveArmour
//
// Returns false if the new armour would not benefit
//
static void GiveArmour(pickup_info_t *pu, Benefit *be)
{
    ArmourType a_class = (ArmourType)be->sub.type;

    SYS_ASSERT(0 <= a_class && a_class < kTotalArmourTypes);

    if (pu->lose_em)
    {
        if (AlmostEquals(pu->player->armours[a_class], 0.0f))
            return;

        pu->player->armours[a_class] -= be->amount;
        if (pu->player->armours[a_class] < 0)
            pu->player->armours[a_class] = 0;

        P_UpdateTotalArmour(pu->player);

        pu->got_it = true;
        return;
    }

    float amount  = be->amount;
    float upgrade = 0;

    if (!pu->special || (pu->special->extendedflags & kExtendedFlagSimpleArmour))
    {
        float slack = be->limit - pu->player->armours[a_class];

        if (amount > slack)
            amount = slack;

        if (amount <= 0)
            return;
    }
    else /* Doom emulation */
    {
        float slack = be->limit - pu->player->totalarmour;

        if (slack < 0)
            return;

        // we try to Upgrade any lower class armour with this armour.
        for (int cl = a_class - 1; cl >= 0; cl--)
        {
            upgrade += pu->player->armours[cl];
        }

        // cannot upgrade more than the specified amount
        if (upgrade > amount)
            upgrade = amount;

        slack += upgrade;

        if (amount > slack)
            amount = slack;

        SYS_ASSERT(amount >= 0);
        SYS_ASSERT(upgrade >= 0);

        if (AlmostEquals(amount, 0.0f) && AlmostEquals(upgrade, 0.0f))
            return;
    }

    pu->player->armours[a_class] += amount;

    // -AJA- 2007/08/22: armor associations
    if (pu->special && pu->special->info->armour_protect_ >= 0)
    {
        pu->player->armour_types[a_class] = pu->special->info;
    }

    if (upgrade > 0)
    {
        for (int cl = a_class - 1; (cl >= 0) && (upgrade > 0); cl--)
        {
            if (pu->player->armours[cl] >= upgrade)
            {
                pu->player->armours[cl] -= upgrade;
                break;
            }
            else if (pu->player->armours[cl] > 0)
            {
                upgrade -= pu->player->armours[cl];
                pu->player->armours[cl] = 0;
            }
        }
    }

    P_UpdateTotalArmour(pu->player);

    pu->got_it = true;
}

//
// GiveKey
//
static void GiveKey(pickup_info_t *pu, Benefit *be)
{
    DoorKeyType key = (DoorKeyType)be->sub.type;

    if (pu->lose_em)
    {
        if (!(pu->player->cards & key))
            return;

        pu->player->cards = (DoorKeyType)(pu->player->cards & ~key);
    }
    else
    {
        if (pu->player->cards & key)
            return;

        pu->player->cards = (DoorKeyType)(pu->player->cards | key);
    }

    // -AJA- leave keys in Co-op games
    if (COOP_MATCH())
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
static void GivePower(pickup_info_t *pu, Benefit *be)
{
    // -ACB- 1998/06/20 - calculate duration in seconds
    float duration = be->amount * kTicRate;
    float limit    = be->limit * kTicRate;

    if (pu->lose_em)
    {
        if (AlmostEquals(pu->player->powers[be->sub.type], 0.0f))
            return;

        pu->player->powers[be->sub.type] -= duration;

        if (pu->player->powers[be->sub.type] < 0)
            pu->player->powers[be->sub.type] = 0;

        pu->got_it = true;
        return;
    }

    if (pu->player->powers[be->sub.type] >= limit)
        return;

    pu->player->powers[be->sub.type] += duration;

    if (pu->player->powers[be->sub.type] > limit)
        pu->player->powers[be->sub.type] = limit;

    // special handling for scuba...
    if (be->sub.type == kPowerTypeScuba)
    {
        pu->player->air_in_lungs = pu->player->mo->info->lung_capacity_;
    }

    pu->got_it = true;
}

static void DoGiveBenefitList(pickup_info_t *pu)
{
    // handle weapons first, since this affects ammo handling

    for (Benefit *be = pu->list; be; be = be->next)
    {
        if (be->type == kBenefitTypeWeapon && be->amount >= 0.0)
            GiveWeapon(pu, be);
    }

    for (Benefit *be = pu->list; be; be = be->next)
    {
        // Put the checking in for neg amounts at benefit level. Powerups can be neg
        // if they last all level. -ACB- 2004/02/04

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
// P_HasBenefitInList
//
// Check if the player has at least one of the benefits in the provided list.
// Returns true if any of them are present for the player, but does not otherwise
// return any information about which benefits matched or what their amounts are.
//
bool P_HasBenefitInList(player_t *player, Benefit *list)
{
    SYS_ASSERT(player && list);
    for (Benefit *be = list; be; be = be->next)
    {
        switch (be->type)
        {
        case kBenefitTypeNone:
            break;

        case kBenefitTypeWeapon:
            for (int i = 0; i < MAXWEAPONS; i++)
            {
                WeaponDefinition *cur_info = player->weapons[i].info;
                if (cur_info == be->sub.weap)
                    return true;
            }
            break;

        case kBenefitTypeAmmo:
            if (player->ammo[be->sub.type].num > be->amount)
                return true;
            break;

        case kBenefitTypeAmmoLimit:
            if (player->ammo[be->sub.type].max > be->amount)
                return true;
            break;

        case kBenefitTypeKey:
            if (player->cards & (DoorKeyType)be->sub.type)
                return true;
            break;

        case kBenefitTypeHealth:
            if (player->health > be->amount)
                return true;
            break;

        case kBenefitTypeArmour:
            if (player->armours[be->sub.type] > be->amount)
                return true;
            break;

        case kBenefitTypePowerup:
            if (!AlmostEquals(player->powers[be->sub.type], 0.0f))
                return true;
            break;

        case kBenefitTypeInventory:
            if (player->inventory[be->sub.type].num > be->amount)
                return true;
            break;

        case kBenefitTypeInventoryLimit:
            if (player->inventory[be->sub.type].max > be->amount)
                return true;
            break;

        case kBenefitTypeCounter:
            if (player->counters[be->sub.type].num > be->amount)
                return true;
            break;

        case kBenefitTypeCounterLimit:
            if (player->counters[be->sub.type].max > be->amount)
                return true;
            break;

        default:
            break;
        }
    }
    return false;
}

//
// P_GiveBenefitList
//
// Give all the benefits in the list to the player.  `special' is the
// special object that all these benefits came from, or nullptr if they
// came from the initial_benefits list.  When `lose_em' is true, the
// benefits should be taken away instead.  Returns true if _any_
// benefit was picked up (or lost), or false if none of them were.
//
bool P_GiveBenefitList(player_t *player, mobj_t *special, Benefit *list, bool lose_em)
{
    pickup_info_t info;

    info.list    = list;
    info.lose_em = lose_em;

    info.player  = player;
    info.special = special;
    info.dropped = false;

    info.new_weap = -1;
    info.new_ammo = -1;

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
static void RunPickupEffects(player_t *player, mobj_t *special, PickupEffect *list)
{
    for (; list; list = list->next_)
    {
        switch (list->type_)
        {
        case kPickupEffectTypeSwitchWeapon:
            P_PlayerSwitchWeapon(player, list->sub_.weap);
            break;

        case kPickupEffectTypeKeepPowerup:
            player->keep_powers |= (1 << list->sub_.type);
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
// P_TouchSpecialThing
//
// -KM- 1999/01/31 Things that give you item bonus are always
//  picked up.  Picked up object is set to death frame instead
//  of removed so that effects can happen.
//
void P_TouchSpecialThing(mobj_t *special, mobj_t *toucher)
{
    float delta = special->z - toucher->z;

    // out of reach
    if (delta > toucher->height || delta < -special->height)
        return;

    if (!toucher->player)
        return;

    // Dead thing touching. Can happen with a sliding player corpse.
    if (toucher->health <= 0)
        return;

    // VOODOO DOLLS: Do not pick up the item if completely still
    if (toucher->is_voodoo && AlmostEquals(toucher->mom.X, 0.0f) && AlmostEquals(toucher->mom.Y, 0.0f) &&
        AlmostEquals(toucher->mom.Z, 0.0f))
        return;

    // -KM- 1998/09/27 Sounds.ddf
    SoundEffect *sound = special->info->activesound_;

    pickup_info_t info;

    info.player  = toucher->player;
    info.special = special;
    info.dropped = (special && (special->flags & kMapObjectFlagDropped)) ? true : false;

    info.new_weap = -1; // the most recently added weapon (must be new)
    info.new_ammo = -1; // got fresh ammo (old count was zero).

    info.got_it  = false;
    info.keep_it = false;
    info.silent  = false;
    info.no_ammo = false;

    // First handle lost benefits
    info.list    = special->info->lose_benefits_;
    info.lose_em = true;
    DoGiveBenefitList(&info);

    // Run through the list of all pickup benefits...
    info.list    = special->info->pickup_benefits_;
    info.lose_em = false;
    DoGiveBenefitList(&info);

    if (special->flags & kMapObjectFlagCountItem)
    {
        info.player->itemcount++;
        info.got_it = true;
    }
    else if (special->hyperflags & kHyperFlagForcePickup)
    {
        info.got_it  = true;
        info.keep_it = false;
    }

    if (!info.got_it)
        return;

    if (!info.keep_it)
    {
        special->health = 0;
        if (time_stop_active) // Hide pickup after gaining benefit while time stop is still active
            special->visibility = INVISIBLE;
        P_KillMobj(info.player->mo, special, nullptr);
    }

    // do all the special effects, lights & sound etc...
    if (!info.silent)
    {
        info.player->bonuscount += BONUS_ADD;
        if (info.player->bonuscount > BONUS_LIMIT)
            info.player->bonuscount = BONUS_LIMIT;

        if (special->info->pickup_message_ != "" && language.IsValidRef(special->info->pickup_message_.c_str()))
        {
            ConsolePlayerMessage(info.player->pnum, "%s", language[special->info->pickup_message_]);
        }

        if (sound)
        {
            int sfx_cat;

            if (info.player == players[consoleplayer])
                sfx_cat = SNCAT_Player;
            else
                sfx_cat = SNCAT_Opponent;

            S_StartFX(sound, sfx_cat, info.player->mo);
        }

        if (info.new_weap >= 0 || info.new_ammo >= 0)
            P_TrySwitchNewWeapon(info.player, info.new_weap, (AmmunitionType)info.new_ammo);
    }

    RunPickupEffects(info.player, special, special->info->pickup_effects_);
}

// FIXME: move this into utility code
static std::string PatternSubst(const char *format, const std::vector<std::string> &keywords)
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

static void DoObituary(const char *format, mobj_t *victim, mobj_t *killer)
{
    std::vector<std::string> keywords;

    keywords.push_back("o");
    keywords.push_back("the player");

    keywords.push_back("k");
    keywords.push_back("a foe");

    std::string msg = PatternSubst(format, keywords);

    ConsolePlayerMessage(victim->player->pnum, "%s", msg.c_str());
}

void P_ObituaryMessage(mobj_t *victim, mobj_t *killer, const DamageClass *damtype)
{
    if (!var_obituaries)
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
// P_KillMobj
//
// Altered to reflect the fact that the dropped item is a pointer to
// MapObjectDefinition, uses new procedure: P_MobjCreateObject.
//
// Note: Damtype can be nullptr here.
//
// -ACB- 1998/08/01
//
// -AJA- 1999/09/12: Now uses P_SetMobjStateDeferred, since this
//       routine can be called by TryMove/PIT_CheckRelThing/etc.
//
void P_KillMobj(mobj_t *source, mobj_t *target, const DamageClass *damtype, bool weak_spot)
{
    // -AJA- 2006/09/10: Voodoo doll handling for coop
    if (target->player && target->player->mo != target)
    {
        P_KillMobj(source, target->player->mo, damtype, weak_spot);
        target->player = nullptr;
    }

    bool nofog = (target->flags & kMapObjectFlagSpecial);

    target->flags &= ~(kMapObjectFlagSpecial | kMapObjectFlagShootable | kMapObjectFlagFloat | kMapObjectFlagSkullFly | kMapObjectFlagTouchy);
    target->extendedflags &= ~(kExtendedFlagBounce | kExtendedFlagUsable | kExtendedFlagClimbable);

    if (!(target->extendedflags & kExtendedFlagNoGravityOnKill))
        target->flags &= ~kMapObjectFlagNoGravity;

    target->flags |= kMapObjectFlagCorpse | kMapObjectFlagDropOff;
    target->height /= (4 / (target->mbf21flags & kMBF21FlagLowGravity ? 8 : 1));

    RAD_MonsterIsDead(target);

    if (source && source->player)
    {
        // count for intermission
        if (target->flags & kMapObjectFlagCountKill)
            source->player->killcount++;

        if (target->info->kill_benefits_)
        {
            pickup_info_t info;
            info.player  = source->player;
            info.special = nullptr;
            info.dropped = false;

            info.new_weap = -1; // the most recently added weapon (must be new)
            info.new_ammo = -1; // got fresh ammo (old count was zero).

            info.got_it  = false;
            info.keep_it = false;
            info.silent  = false;
            info.no_ammo = false;

            info.list    = target->info->kill_benefits_;
            info.lose_em = false;
            DoGiveBenefitList(&info);
        }

        if (target->player)
        {
            // Killed a team mate?
            if (target->side & source->side)
            {
                source->player->frags--;
                source->player->totalfrags--;
            }
            else
            {
                source->player->frags++;
                source->player->totalfrags++;
            }
        }
    }
    else if (SP_MATCH() && (target->flags & kMapObjectFlagCountKill))
    {
        // count all monster deaths,
        // even those caused by other monsters
        players[consoleplayer]->killcount++;
    }

    if (target->player)
    {
        P_ObituaryMessage(target, source, damtype);

        // count environment kills against you
        if (!source)
        {
            target->player->frags--;
            target->player->totalfrags--;
        }

        target->flags &= ~kMapObjectFlagSolid;
        target->player->playerstate    = PST_DEAD;
        target->player->std_viewheight = HMM_MIN(DEATHVIEWHEIGHT, target->height / 3);
        target->player->actual_speed   = 0;

        P_DropWeapon(target->player);

        // don't die in auto map, switch view prior to dying
        if (target->player == players[consoleplayer] && automap_active)
            AutomapStop();

        // don't immediately restart when USE key was pressed
        if (target->player == players[consoleplayer])
            EventClearInput();
    }

    int state    = 0;
    bool       overkill = false;

    if (target->info->gib_health_ < 0 && target->health < target->info->gib_health_)
        overkill = true;
    else if (target->health < -target->spawnhealth)
        overkill = true;

    if (weak_spot)
    {
        state = P_MobjFindLabel(target, "WEAKDEATH");
        if (state == 0)
            overkill = true;
    }

    if (state == 0 && overkill && damtype && damtype->overkill_.label_ != "")
    {
        state = P_MobjFindLabel(target, damtype->overkill_.label_.c_str());
        if (state != 0)
            state += damtype->overkill_.offset_;
    }

    if (state == 0 && overkill && target->info->overkill_state_)
        state = target->info->overkill_state_;

    if (state == 0 && damtype && damtype->death_.label_ != "")
    {
        state = P_MobjFindLabel(target, damtype->death_.label_.c_str());
        if (state != 0)
            state += damtype->death_.offset_;
    }

    if (state == 0)
        state = target->info->death_state_;

    if (g_gore.d_== 2 &&
        (target->flags & kMapObjectFlagCountKill)) // Hopefully the only things with blood/gore are monsters and not "barrels", etc
    {
        state = 0;
        if (!nofog)
        {
            mobj_t *fog = P_MobjCreateObject(target->x, target->y, target->z, mobjtypes.Lookup("TELEPORT_FLASH"));
            if (fog && fog->info->chase_state_)
                P_SetMobjStateDeferred(fog, fog->info->chase_state_, 0);
        }
    }

    if (target->hyperflags & kHyperFlagDehackedCompatibility)
    {
        P_SetMobjState(target, state);
        target->tics -= Random8BitStateful() & 3;
        if (target->tics < 1)
            target->tics = 1;
    }
    else
    {
        P_SetMobjStateDeferred(target, state, Random8BitStateful() & 3);
    }

    // Drop stuff. This determines the kind of object spawned
    // during the death frame of a thing.
    const MapObjectDefinition *item = target->info->dropitem_;
    if (item)
    {
        mobj_t *mo = P_MobjCreateObject(target->x, target->y, target->floorz, item);

        // -ES- 1998/07/18 nullptr check to prevent crashing
        if (mo)
            mo->flags |= kMapObjectFlagDropped;
    }
}

//
// P_ThrustMobj
//
// Like P_DamageMobj, but only pushes the target object around
// (doesn't inflict any damage).  Parameters are:
//
// * target    - mobj to be thrust.
// * inflictor - mobj causing the thrusting.
// * thrust    - amount of thrust done (same values as damage).  Can
//               be negative to "pull" instead of push.
//
// -AJA- 1999/11/06: Wrote this routine.
//
void P_ThrustMobj(mobj_t *target, mobj_t *inflictor, float thrust)
{
    // check for immunity against the attack
    if (target->hyperflags & kHyperFlagInvulnerable)
        return;

    // check for lead feet ;)
    if (target->hyperflags & kHyperFlagImmovable)
        return;

    if (inflictor && inflictor->currentattack &&
        0 == (inflictor->currentattack->attack_class_ & ~target->info->immunity_))
    {
        return;
    }

    float dx = target->x - inflictor->x;
    float dy = target->y - inflictor->y;

    // don't thrust if at the same location (no angle)
    if (fabs(dx) < 1.0f && fabs(dy) < 1.0f)
        return;

    BAMAngle angle = R_PointToAngle(0, 0, dx, dy);

    // -ACB- 2000/03/11 Div-by-zero check...
    SYS_ASSERT(!AlmostEquals(target->info->mass_, 0.0f));

    float push = 12.0f * thrust / target->info->mass_;

    // limit thrust to reasonable values
    if (push < -40.0f)
        push = -40.0f;
    if (push > 40.0f)
        push = 40.0f;

    target->mom.X += push * epi::BAMCos(angle);
    target->mom.Y += push * epi::BAMSin(angle);

    if (level_flags.true3dgameplay)
    {
        float dz    = MO_MIDZ(target) - MO_MIDZ(inflictor);
        float slope = P_ApproxSlope(dx, dy, dz);

        target->mom.Z += push * slope / 2;
    }
}

//
// P_PushMobj
//
// Like P_DamageMobj, but only pushes the target object around
// (doesn't inflict any damage).  Parameters are:
//
// * target    - mobj to be thrust.
// * inflictor - mobj causing the thrusting.
// * thrust    - amount of thrust done (same values as damage).  Can
//               be negative to "pull" instead of push.
//
// -Lobo- 2022/07/07: Created this routine.
//
void P_PushMobj(mobj_t *target, mobj_t *inflictor, float thrust)
{
    /*
    if(tm_I.mover->mom.x > tm_I.mover->mom.y)
        ThrustSpeed = fabsf(tm_I.mover->mom.x);
    else
        ThrustSpeed = fabsf(tm_I.mover->mom.y);
    */

    float dx = target->x - inflictor->x;
    float dy = target->y - inflictor->y;

    // don't thrust if at the same location (no angle)
    if (fabs(dx) < 1.0f && fabs(dy) < 1.0f)
        return;

    BAMAngle angle = R_PointToAngle(0, 0, dx, dy);

    // -ACB- 2000/03/11 Div-by-zero check...
    SYS_ASSERT(!AlmostEquals(target->info->mass_, 0.0f));

    float push = 12.0f * thrust / target->info->mass_;

    // limit thrust to reasonable values
    if (push < -40.0f)
        push = -40.0f;
    if (push > 40.0f)
        push = 40.0f;

    target->mom.X += push * epi::BAMCos(angle);
    target->mom.Y += push * epi::BAMSin(angle);

    if (level_flags.true3dgameplay)
    {
        float dz    = MO_MIDZ(target) - MO_MIDZ(inflictor);
        float slope = P_ApproxSlope(dx, dy, dz);

        target->mom.Z += push * slope / 2;
    }
}

//
// P_DamageMobj
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
//       routine can be called by TryMove/PIT_CheckRelThing/etc.
//
void P_DamageMobj(mobj_t *target, mobj_t *inflictor, mobj_t *source, float damage, const DamageClass *damtype,
                  bool weak_spot)
{
    if (target->isRemoved())
        return;

    if (!(target->flags & kMapObjectFlagShootable))
        return;

    if (target->health <= 0)
        return;

    // check for immunity against the attack
    if (target->hyperflags & kHyperFlagInvulnerable)
        return;

    if (!weak_spot && inflictor && inflictor->currentattack &&
        0 == (inflictor->currentattack->attack_class_ & ~target->info->immunity_))
    {
        return;
    }

    // sanity check : don't produce references to removed objects
    if (inflictor && inflictor->isRemoved())
        inflictor = nullptr;
    if (source && source->isRemoved())
        source = nullptr;

    // check for immortality
    if (target->hyperflags & kHyperFlagImmortal)
        damage = 0.0f; // do no damage

    // check for partial resistance against the attack
    if (!weak_spot && damage >= 0.1f && inflictor && inflictor->currentattack &&
        0 == (inflictor->currentattack->attack_class_ & ~target->info->resistance_))
    {
        damage = HMM_MAX(0.1f, damage * target->info->resist_multiply_);
    }

    // -ACB- 1998/07/12 Use Visibility Enum
    // A Damaged Stealth Creature becomes more visible
    if (target->flags & kMapObjectFlagStealth)
        target->vis_target = VISIBLE;

    if (target->flags & kMapObjectFlagSkullFly)
    {
        target->mom.X = target->mom.Y = target->mom.Z = 0;
        target->flags &= ~kMapObjectFlagSkullFly;
    }

    player_t *player = target->player;

    // Some close combat weapons should not
    // inflict thrust and push the victim out of reach,
    // thus kick away unless using the chainsaw.

    if (inflictor && !(target->flags & kMapObjectFlagNoClip) &&
        !(source && source->player && source->player->ready_wp >= 0 &&
          source->player->weapons[source->player->ready_wp].info->nothrust_))
    {
        // make fall forwards sometimes
        if (damage < 40 && damage > target->health && target->z - inflictor->z > 64 && (Random8BitStateful() & 1))
        {
            P_ThrustMobj(target, inflictor, -damage * 4);
        }
        else
            P_ThrustMobj(target, inflictor, damage);
    }

    // player specific
    if (player)
    {
        int i;

        // Don't damage player if sector type should only affect grounded monsters
        // Note: flesh this out be be more versatile - Dasho
        if (damtype && damtype->grounded_monsters_)
            return;

        // ignore damage in GOD mode, or with INVUL powerup
        if ((player->cheats & CF_GODMODE) || player->powers[kPowerTypeInvulnerable] > 0)
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
            if (damtype->damage_unless_ && P_HasBenefitInList(player, damtype->damage_unless_))
                unless_damage = false;
            if (damtype->damage_if_ && P_HasBenefitInList(player, damtype->damage_if_))
                if_damage = true;
            if (!unless_damage && !if_damage && !damtype->bypass_all_)
                return;
        }

        // take half damage in trainer mode
        if (game_skill == sk_baby)
            damage /= 2.0f;

        // preliminary check: immunity and resistance
        for (i = kTotalArmourTypes - 1; i >= kArmourTypeGreen; i--)
        {
            if (damtype && damtype->no_armour_)
                continue;

            if (player->armours[i] <= 0)
                continue;

            const MapObjectDefinition *arm_info = player->armour_types[i];

            if (!arm_info || !inflictor || !inflictor->currentattack)
                continue;

            // this armor does not provide any protection for this attack
            if (0 != (inflictor->currentattack->attack_class_ & ~arm_info->armour_class_))
                continue;

            if (0 == (inflictor->currentattack->attack_class_ & ~arm_info->immunity_))
                return; /* immune : we can go home early! */

            if (damage > 0.1f && 0 == (inflictor->currentattack->attack_class_ & ~arm_info->resistance_))
            {
                damage = HMM_MAX(0.1f, damage * arm_info->resist_multiply_);
            }
        }

        // Bot Deathmatch Damange Resistance check
        if (DEATHMATCH() && !player->isBot() && source && source->player && source->player->isBot())
        {
            if (player_deathmatch_damage_resistance.d_< 9)
            {
                float mul = 1.90f - (player_deathmatch_damage_resistance.d_* 0.10f);
                damage *= mul;
            }
            else if (player_deathmatch_damage_resistance.d_> 9)
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

            if (player->armours[i] <= 0)
                continue;

            const MapObjectDefinition *arm_info = player->armour_types[i];

            // this armor does not provide any protection for this attack
            if (arm_info && inflictor && inflictor->currentattack &&
                0 != (inflictor->currentattack->attack_class_ & ~arm_info->armour_class_))
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
                    FatalError("INTERNAL ERROR in P_DamageMobj: bad armour %d\n", i);
                }
            }

            if (player->armours[i] <= saved)
            {
                // armour is used up
                saved = player->armours[i];
            }

            damage -= saved;

            if (arm_info)
                saved *= arm_info->armour_deplete_;

            player->armours[i] -= saved;

            // don't apply inner armour unless outer is finished
            if (player->armours[i] > 0)
                break;

            player->armours[i] = 0;
        }

        P_UpdateTotalArmour(player);

        player->attacker = source;

        // instakill sectors
        if (damtype && damtype->instakill_)
            damage = target->player->health + 1;

        // add damage after armour / invuln detection
        if (damage > 0)
        {
            // Change damage color if new inflicted damage is greater than current processed damage
            if (damage >= player->damagecount)
            {
                if (damtype)
                    player->last_damage_colour = damtype->damage_flash_colour_;
                else
                    player->last_damage_colour = SG_RED_RGBA32;
            }

            player->damagecount += (int)HMM_MAX(damage, DAMAGE_ADD_MIN);
            player->damage_pain += damage;
        }

        // teleport stomp does 10k points...
        if (player->damagecount > DAMAGE_LIMIT)
            player->damagecount = DAMAGE_LIMIT;
    }
    else
    {
        // instakill sectors
        if (damtype && damtype->instakill_)
            damage = target->health + 1;
    }

    // do the damage
    target->health -= damage;

    if (player)
    {
        // mirror mobj health here for Dave
        // player->health = HMM_MAX(0, target->health);

        // Dasho 2023.09.05: The above behavior caused inconsistencies when multiple
        // voodoo dolls were present in a level (i.e., heavily damaging one and then
        // lightly damaging another one that was previously at full health would "heal" the player)
        player->health = HMM_MAX(0, player->health - damage);
    }

    // Lobo 2023: Handle attack flagged with the "PLAYER_ATTACK" special.
    //  This attack will always be treated as originating from the player, even if it's an indirect secondary attack.
    //  This way the player gets his VAMPIRE health and KillBenefits.
    if (inflictor && inflictor->currentattack && (inflictor->currentattack->flags_ & kAttackFlagPlayer))
    {
        player_t *CurrentPlayer;
        CurrentPlayer = players[consoleplayer];

        source = CurrentPlayer->mo;

        if (source && source->isRemoved()) // Sanity check?
            source = nullptr;
    }

    // -AJA- 2007/11/06: vampire mode!
    if (source && source != target && source->health < source->spawnhealth &&
        ((source->hyperflags & kHyperFlagVampire) ||
         (inflictor && inflictor->currentattack && (inflictor->currentattack->flags_ & kAttackFlagVampire))))
    {
        float qty = (target->player ? 0.5 : 0.25) * damage;

        source->health = HMM_MIN(source->health + qty, source->spawnhealth);

        if (source->player)
            source->player->health = HMM_MIN(source->player->health + qty, source->spawnhealth);
    }

    if (target->health <= 0)
    {
        P_KillMobj(source, target, damtype, weak_spot);
        return;
    }

    // enter pain states
    float pain_chance;

    if (target->flags & kMapObjectFlagSkullFly)
        pain_chance = 0;
    else if (weak_spot && target->info->weak_.painchance_ >= 0)
        pain_chance = target->info->weak_.painchance_;
    else if (target->info->resist_painchance_ >= 0 && inflictor && inflictor->currentattack &&
             0 == (inflictor->currentattack->attack_class_ & ~target->info->resistance_))
        pain_chance = target->info->resist_painchance_;
    else
        pain_chance = target->painchance; // Lobo 2023: use dynamic painchance
    // pain_chance = target->info->painchance_;

    if (pain_chance > 0 && Random8BitTestStateful(pain_chance))
    {
        // setup to hit back
        target->flags |= kMapObjectFlagJustHit;

        int state = 0;

        if (weak_spot)
            state = P_MobjFindLabel(target, "WEAKPAIN");

        if (state == 0 && damtype && damtype->pain_.label_ != "")
        {
            state = P_MobjFindLabel(target, damtype->pain_.label_.c_str());
            if (state != 0)
                state += damtype->pain_.offset_;
        }

        if (state == 0)
            state = target->info->pain_state_;

        if (state != 0)
            P_SetMobjStateDeferred(target, state, 0);
    }

    // we're awake now...
    target->reactiontime = 0;

    bool ultra_loyal = (source && (target->hyperflags & kHyperFlagUltraLoyal) && (source->side & target->side) != 0);

    if ((!target->threshold || target->extendedflags & kExtendedFlagNoGrudge) && source && source != target &&
        (!(source->extendedflags & kExtendedFlagNeverTarget)) && !target->player && !ultra_loyal)
    {
        // if not intent on another player, chase after this one
        target->SetTarget(source);
        target->threshold = BASETHRESHOLD;

        if (target->state == &states[target->info->idle_state_] && target->info->chase_state_)
        {
            P_SetMobjStateDeferred(target, target->info->chase_state_, 0);
        }
    }
}

//
// For killing monsters and players when something teleports on top
// of them.  Even the invulnerability powerup doesn't stop it.  Also
// used for the kill-all cheat.  Inflictor and damtype can be nullptr.
//
void P_TelefragMobj(mobj_t *target, mobj_t *inflictor, const DamageClass *damtype)
{
    if (target->health <= 0)
        return;

    target->health = -1000;

    if (target->flags & kMapObjectFlagStealth)
        target->vis_target = VISIBLE;

    if (target->flags & kMapObjectFlagSkullFly)
    {
        target->mom.X = target->mom.Y = target->mom.Z = 0;
        target->flags &= ~kMapObjectFlagSkullFly;
    }

    if (target->player)
    {
        target->player->attacker    = inflictor;
        target->player->damagecount = DAMAGE_LIMIT;
        target->player->damage_pain = target->spawnhealth;
    }

    P_KillMobj(inflictor, target, damtype);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
