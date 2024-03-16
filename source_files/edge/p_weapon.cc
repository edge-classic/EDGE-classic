//----------------------------------------------------------------------------
//  EDGE Weapon (player sprites) Action Code
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
//
// -KM- 1998/11/25 Added/Changed stuff for weapons.ddf
//

#include "p_weapon.h"

#include "dm_defs.h"
#include "dm_state.h"
#include "e_event.h"
#include "epi.h"
#include "m_misc.h"
#include "m_random.h"
#include "p_action.h"
#include "p_local.h"
#include "rad_trig.h"
#include "s_sound.h"
#include "w_sprite.h"
#include "w_wad.h"

extern ConsoleVariable view_bobbing;
extern ConsoleVariable erraticism;

static constexpr uint8_t kMaximumPlayerSpriteLoop = 10;

static constexpr float   kWeaponSwapSpeed = 6.0f;
static constexpr uint8_t kWeaponBottom    = 128;
static constexpr uint8_t kWeaponTop       = 32;
static constexpr uint8_t kGrinTime        = (kTicRate * 2);

static void BobWeapon(Player *p, WeaponDefinition *info);

static SoundCategory WeaponSoundEffectCategory(Player *p)
{
    if (p == players[console_player])
        return kCategoryWeapon;

    return kCategoryOpponent;
}

static void SetPlayerSprite(Player *p, int position, int stnum, WeaponDefinition *info = nullptr)
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
            int new_state = DDF_StateFindLabel(info->state_grp_, st->label, true /* quiet */);
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
void SetPlayerSpriteDeferred(Player *p, int position, int stnum)
{
    PlayerSprite *psp = &p->player_sprites_[position];

    if (stnum == 0 || psp->state == nullptr)
    {
        SetPlayerSprite(p, position, stnum);
        return;
    }

    psp->tics       = 0;
    psp->next_state = (states + stnum);
}

//
// CheckWeaponSprite
//
// returns true if the sprite(s) for the weapon exist.  Prevents being
// able to e.g. select the super shotgun when playing with a DOOM 1
// IWAD (and cheating).
//
// -KM- 1998/12/16 Added check to make sure sprites exist.
// -AJA- 2000: Made into a separate routine.
//
bool CheckWeaponSprite(WeaponDefinition *info)
{
    if (info->up_state_ == 0)
        return false;

    return CheckSpritesExist(info->state_grp_);
}

static bool ButtonDown(Player *p, int ATK)
{
    /*
        if (ATK == 0)
            return (p->command_.buttons & kButtonCodeAttack);
        else
            return (p->command_.extended_buttons &
       kExtendedButtonCodeSecondAttack);

    */

    uint16_t tempbuttons = 0;
    switch (ATK)
    {
    case 0:
        tempbuttons = p->command_.buttons & kButtonCodeAttack;
        break;
    case 1:
        tempbuttons = p->command_.extended_buttons & kExtendedButtonCodeSecondAttack;
        break;
    case 2:
        tempbuttons = p->command_.extended_buttons & kExtendedButtonCodeThirdAttack;
        break;
    case 3:
        tempbuttons = p->command_.extended_buttons & kExtendedButtonCodeFourthAttack;
        break;
    default:
        // should never happen
        break;
    }

    return tempbuttons;
}

static bool WeaponCanFire(Player *p, int idx, int ATK)
{
    WeaponDefinition *info = p->weapons_[idx].info;

    if (info->shared_clip_)
        ATK = 0;

    // the order here is important, to allow NoAmmo+Clip weapons.
    if (info->clip_size_[ATK] > 0)
        return (info->ammopershot_[ATK] <= p->weapons_[idx].clip_size[ATK]);

    if (info->ammo_[ATK] == kAmmunitionTypeNoAmmo)
        return true;

    return (info->ammopershot_[ATK] <= p->ammo_[info->ammo_[ATK]].count);
}

static bool WeaponCanReload(Player *p, int idx, int ATK, bool allow_top_up)
{
    WeaponDefinition *info = p->weapons_[idx].info;

    bool can_fire = WeaponCanFire(p, idx, ATK);

    if (info->shared_clip_)
        ATK = 0;

    if (!(info->specials_[ATK] & WeaponFlagPartialReload))
    {
        allow_top_up = false;
    }

    // for non-clip weapon, can reload whenever enough ammo is avail.
    if (info->clip_size_[ATK] == 0)
        return can_fire;

    // clip check (cannot reload if clip is full)
    if (p->weapons_[idx].clip_size[ATK] == info->clip_size_[ATK])
        return false;

    // for clip weapons, cannot reload until clip is empty.
    if (can_fire && !allow_top_up)
        return false;

    // for NoAmmo+Clip weapons, can always refill it
    if (info->ammo_[ATK] == kAmmunitionTypeNoAmmo)
        return true;

    // ammo check...
    int total = p->ammo_[info->ammo_[ATK]].count;

    if (info->specials_[ATK] & WeaponFlagPartialReload)
    {
        return (info->ammopershot_[ATK] <= total);
    }

    return (info->clip_size_[ATK] - p->weapons_[idx].clip_size[ATK] <= total);
}

static bool WeaponCouldAutoFire(Player *p, int idx, int ATK)
{
    // Returns true when weapon will either fire or reload
    // (assuming the button is held down).

    WeaponDefinition *info = p->weapons_[idx].info;

    if (!info->attack_state_[ATK])
        return false;

    // MBF21 NOAUTOFIRE flag
    if (info->specials_[ATK] & WeaponFlagNoAutoFire)
        return false;

    if (info->shared_clip_)
        ATK = 0;

    if (info->ammo_[ATK] == kAmmunitionTypeNoAmmo)
        return true;

    int total = p->ammo_[info->ammo_[ATK]].count;

    if (info->clip_size_[ATK] == 0)
        return (info->ammopershot_[ATK] <= total);

    // for clip weapons, either need a non-empty clip or enough
    // ammo to fill the clip (which is able to be filled without the
    // manual reload key).
    if (info->ammopershot_[ATK] <= p->weapons_[idx].clip_size[ATK] ||
        (info->clip_size_[ATK] <= total &&
         (info->specials_[ATK] & (WeaponFlagReloadWhileTrigger | WeaponFlagFreshReload))))
    {
        return true;
    }

    return false;
}

static void GotoDownState(Player *p)
{
    WeaponDefinition *info = p->weapons_[p->ready_weapon_].info;

    int newstate = info->down_state_;

    SetPlayerSpriteDeferred(p, kPlayerSpriteWeapon, newstate);
    SetPlayerSprite(p, kPlayerSpriteCrosshair, info->crosshair_);
}

static void GotoReadyState(Player *p)
{
    WeaponDefinition *info = p->weapons_[p->ready_weapon_].info;

    int newstate = info->ready_state_;

    SetPlayerSpriteDeferred(p, kPlayerSpriteWeapon, newstate);
    SetPlayerSpriteDeferred(p, kPlayerSpriteCrosshair, info->crosshair_);
}

static void GotoEmptyState(Player *p)
{
    WeaponDefinition *info = p->weapons_[p->ready_weapon_].info;

    int newstate = info->empty_state_;

    SetPlayerSpriteDeferred(p, kPlayerSpriteWeapon, newstate);
    SetPlayerSprite(p, kPlayerSpriteCrosshair, 0);
}

static void GotoAttackState(Player *p, int ATK, bool can_warmup)
{
    WeaponDefinition *info = p->weapons_[p->ready_weapon_].info;

    int newstate = info->attack_state_[ATK];

    if (p->remember_attack_state_[ATK] >= 0)
    {
        newstate                       = p->remember_attack_state_[ATK];
        p->remember_attack_state_[ATK] = -1;
    }
    else if (can_warmup && info->warmup_state_[ATK])
    {
        newstate = info->warmup_state_[ATK];
    }

    if (newstate)
    {
        SetPlayerSpriteDeferred(p, kPlayerSpriteWeapon, newstate);
        p->idle_wait_ = 0;
    }
}

static void ReloadWeapon(Player *p, int idx, int ATK)
{
    WeaponDefinition *info = p->weapons_[idx].info;

    if (info->clip_size_[ATK] == 0)
        return;

    // for NoAmmo+Clip weapons, can always refill it
    if (info->ammo_[ATK] == kAmmunitionTypeNoAmmo)
    {
        p->weapons_[idx].clip_size[ATK] = info->clip_size_[ATK];
        return;
    }

    int qty = info->clip_size_[ATK] - p->weapons_[idx].clip_size[ATK];

    if (qty > p->ammo_[info->ammo_[ATK]].count)
        qty = p->ammo_[info->ammo_[ATK]].count;

    EPI_ASSERT(qty > 0);

    p->weapons_[idx].reload_count[ATK] = qty;
    p->weapons_[idx].clip_size[ATK] += qty;
    p->ammo_[info->ammo_[ATK]].count -= qty;
}

static void GotoReloadState(Player *p, int ATK)
{
    WeaponDefinition *info = p->weapons_[p->ready_weapon_].info;

    if (info->shared_clip_)
        ATK = 0;

    ReloadWeapon(p, p->ready_weapon_, ATK);

    // second attack will fall-back to using normal reload states.
    if (ATK == 1 && !info->reload_state_[ATK])
        ATK = 0;

    // third attack will fall-back to using normal reload states.
    if (ATK == 2 && !info->reload_state_[ATK])
        ATK = 0;

    // fourth attack will fall-back to using normal reload states.
    if (ATK == 3 && !info->reload_state_[ATK])
        ATK = 0;

    if (info->reload_state_[ATK])
    {
        SetPlayerSpriteDeferred(p, kPlayerSpriteWeapon, info->reload_state_[ATK]);
        p->idle_wait_ = 0;
    }

    // if player has reload states, use 'em baby
    if (p->map_object_->info_->reload_state_)
        MapObjectSetStateDeferred(p->map_object_, p->map_object_->info_->reload_state_, 0);
}

//
// SwitchAway
//
// Not enough ammo to shoot, selects the next weapon to use.
// In some cases we prefer to reload the weapon (if we can).
// The NO_SWITCH special prevents the switch, enter empty or ready
// states instead.
//
static void SwitchAway(Player *p, int ATK, int reload)
{
    WeaponDefinition *info = p->weapons_[p->ready_weapon_].info;

    if (reload && WeaponCanReload(p, p->ready_weapon_, ATK, false))
        GotoReloadState(p, ATK);
    else if (info->specials_[ATK] & WeaponFlagSwitchAway)
        SelectNewWeapon(p, -100, kAmmunitionTypeDontCare);
    else if (info->empty_state_ && !WeaponCouldAutoFire(p, p->ready_weapon_, 0))
        GotoEmptyState(p);
    else
        GotoReadyState(p);
}

//
// BringUpWeapon
//
// Starts bringing the pending weapon up
// from the bottom of the screen.
//
static void BringUpWeapon(Player *p)
{
    WeaponSelection sel = p->pending_weapon_;

    EPI_ASSERT(sel != KWeaponSelectionNoChange);

    p->ready_weapon_ = sel;

    p->pending_weapon_                               = KWeaponSelectionNoChange;
    p->player_sprites_[kPlayerSpriteWeapon].screen_y = kWeaponBottom - kWeaponTop;

    p->remember_attack_state_[0] = -1;
    p->remember_attack_state_[1] = -1;
    p->remember_attack_state_[2] = -1;
    p->remember_attack_state_[3] = -1;
    p->idle_wait_                = 0;
    p->weapon_last_frame_        = -1;

    if (sel == KWeaponSelectionNone)
    {
        p->attack_button_down_[0] = false;
        p->attack_button_down_[1] = false;
        p->attack_button_down_[2] = false;
        p->attack_button_down_[3] = false;

        SetPlayerSprite(p, kPlayerSpriteWeapon, 0);
        SetPlayerSprite(p, kPlayerSpriteFlash, 0);
        SetPlayerSprite(p, kPlayerSpriteCrosshair, 0);

        p->zoom_field_of_view_ = 0;
        return;
    }

    WeaponDefinition *info = p->weapons_[sel].info;

    // update current key choice
    if (info->bind_key_ >= 0)
        p->key_choices_[info->bind_key_] = sel;

    if (info->specials_[0] & WeaponFlagAnimated)
        p->player_sprites_[kPlayerSpriteWeapon].screen_y = 0;

    if (p->zoom_field_of_view_ > 0)
    {
        if (info->zoom_fov_ < int(kBAMAngle360))
            p->zoom_field_of_view_ = info->zoom_fov_;
        else
            p->zoom_field_of_view_ = 0;
    }

    if (info->start_)
        StartSoundEffect(info->start_, WeaponSoundEffectCategory(p), p->map_object_);

    SetPlayerSpriteDeferred(p, kPlayerSpriteWeapon, info->up_state_);
    SetPlayerSprite(p, kPlayerSpriteFlash, 0);
    SetPlayerSprite(p, kPlayerSpriteCrosshair, info->crosshair_);

    p->refire_ = info->refire_inacc_ ? 0 : 1;
}

void DesireWeaponChange(Player *p, int key)
{
    // optimisation: don't keep calculating this over and over
    // while the user holds down the same number key.
    if (p->pending_weapon_ >= 0)
    {
        WeaponDefinition *info = p->weapons_[p->pending_weapon_].info;

        EPI_ASSERT(info);

        if (info->bind_key_ == key)
            return;
    }

    // NEW CODE

    WeaponDefinition *ready_info = nullptr;
    if (p->ready_weapon_ >= 0)
        ready_info = p->weapons_[p->ready_weapon_].info;

    int base_pri = 0;

    if (p->ready_weapon_ >= 0)
        base_pri = p->weapons_[p->ready_weapon_].info->KeyPri(p->ready_weapon_);

    int close_idx = -1;
    int close_pri = 99999999;
    int wrap_idx  = -1;
    int wrap_pri  = close_pri;

    for (int i = 0; i < kMaximumWeapons; i++)
    {
        if (i == p->ready_weapon_)
            continue;

        if (!p->weapons_[i].owned)
            continue;

        WeaponDefinition *info = p->weapons_[i].info;

        if (info->bind_key_ != key)
            continue;

        if (!CheckWeaponSprite(info))
            continue;

        // when key & priority are the same, use the index value
        // to break the deadlock.
        int new_pri = info->KeyPri(i);

        // if the key is different, choose last weapon used on that key
        if (ready_info && ready_info->bind_key_ != key)
        {
            if (p->key_choices_[key] >= 0)
            {
                p->pending_weapon_ = p->key_choices_[key];
                return;
            }

            // if no last weapon, choose HIGHEST priority
            if (ready_info && ready_info->bind_key_ != key)
            {
                if (close_idx < 0 || new_pri > close_pri)
                    close_idx = i, close_pri = new_pri;
            }
        }
        else // on same key, use sequence logic
        {
            if (new_pri > base_pri && new_pri < close_pri)
                close_idx = i, close_pri = new_pri;

            if (new_pri < wrap_pri)
                wrap_idx = i, wrap_pri = new_pri;
        }
    }

    if (close_idx >= 0)
        p->pending_weapon_ = (WeaponSelection)close_idx;
    else if (wrap_idx >= 0)
        p->pending_weapon_ = (WeaponSelection)wrap_idx;
}

//
// CycleWeapon
//
// Select the next (or previous) weapon which can be fired.
// The 'dir' parameter is +1 for next (i.e. higher key number)
// and -1 for previous (lower key number).  When no such
// weapon exists, nothing happens.
//
// -AJA- 2005/02/17: added this.
//
void CycleWeapon(Player *p, int dir)
{
    if (p->pending_weapon_ != KWeaponSelectionNoChange)
        return;

    int base_pri = 0;

    if (p->ready_weapon_ >= 0)
        base_pri = p->weapons_[p->ready_weapon_].info->KeyPri(p->ready_weapon_);

    int close_idx = -1;
    int close_pri = dir * 99999999;
    int wrap_idx  = -1;
    int wrap_pri  = close_pri;

    for (int i = 0; i < kMaximumWeapons; i++)
    {
        if (i == p->ready_weapon_)
            continue;

        if (!p->weapons_[i].owned)
            continue;

        WeaponDefinition *info = p->weapons_[i].info;

        if (info->bind_key_ < 0)
            continue;

        if (!WeaponCouldAutoFire(p, i, 0))
            continue;

        if (!CheckWeaponSprite(info))
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
        else /* dir < 0 */
        {
            if (new_pri < base_pri && new_pri > close_pri)
                close_idx = i, close_pri = new_pri;

            if (new_pri > wrap_pri)
                wrap_idx = i, wrap_pri = new_pri;
        }
    }

    if (close_idx >= 0)
        p->pending_weapon_ = (WeaponSelection)close_idx;
    else if (wrap_idx >= 0)
        p->pending_weapon_ = (WeaponSelection)wrap_idx;
}

//
// SelectNewWeapon
//
// Out of ammo, pick a weapon to change to.
// Preferences are set here.
//
// The `ammo' parameter is normally kAmmunitionTypeDontCare, meaning that the
// user ran out of ammo while firing.  Otherwise it is some ammo just picked up
// by the player.
//
// This routine deliberately ignores second attacks.
//
void SelectNewWeapon(Player *p, int priority, AmmunitionType ammo)
{
    // int key = -1; - Seems to be unused - Dasho
    WeaponDefinition *info;

    for (int i = 0; i < kMaximumWeapons; i++)
    {
        info = p->weapons_[i].info;

        if (!p->weapons_[i].owned)
            continue;

        if (info->dangerous_ || info->priority_ < priority)
            continue;

        if (ammo != kAmmunitionTypeDontCare && info->ammo_[0] != ammo)
            continue;

        if (!WeaponCouldAutoFire(p, i, 0))
            continue;

        if (!CheckWeaponSprite(info))
            continue;

        p->pending_weapon_ = (WeaponSelection)i;
        priority           = info->priority_;
        // key = info->bind_key;
    }

    // all out of choices ?
    if (priority < 0)
    {
        p->pending_weapon_ = (ammo == kAmmunitionTypeDontCare) ? KWeaponSelectionNone : KWeaponSelectionNoChange;
        return;
    }

    if (p->pending_weapon_ == p->ready_weapon_)
    {
        p->pending_weapon_ = KWeaponSelectionNoChange;
        return;
    }
}

void TrySwitchNewWeapon(Player *p, int new_weap, AmmunitionType new_ammo)
{
    // be cheeky... :-)
    if (new_weap >= 0)
        p->grin_count_ = kGrinTime;

    if (p->pending_weapon_ != KWeaponSelectionNoChange)
        return;

    if (!level_flags.weapon_switch && p->ready_weapon_ != KWeaponSelectionNone &&
        (WeaponCouldAutoFire(p, p->ready_weapon_, 0) || WeaponCouldAutoFire(p, p->ready_weapon_, 1) ||
         WeaponCouldAutoFire(p, p->ready_weapon_, 2) || WeaponCouldAutoFire(p, p->ready_weapon_, 3)))
    {
        return;
    }

    if (new_weap >= 0)
    {
        if (WeaponCouldAutoFire(p, new_weap, 0))
            p->pending_weapon_ = (WeaponSelection)new_weap;
        return;
    }

    EPI_ASSERT(new_ammo >= 0);

    // We were down to zero ammo, so select a new weapon.
    // Choose the next highest priority weapon than the current one.
    // Don't override any weapon change already underway.
    // Don't change weapon if NO_SWITCH is true.

    int priority = -100;

    if (p->ready_weapon_ >= 0)
    {
        WeaponDefinition *w = p->weapons_[p->ready_weapon_].info;

        if (!(w->specials_[0] & WeaponFlagSwitchAway))
            return;

        priority = w->priority_;
    }

    SelectNewWeapon(p, priority, new_ammo);
}

bool TryFillNewWeapon(Player *p, int idx, AmmunitionType ammo, int *qty)
{
    // When ammo is kAmmunitionTypeDontCare, uses any ammo the player has (qty
    // parameter ignored).  Returns true if uses any of the ammo.

    bool result = false;

    WeaponDefinition *info = p->weapons_[idx].info;

    for (int ATK = 0; ATK < 4; ATK++)
    {
        if (!info->attack_state_[ATK])
            continue;

        // note: NoAmmo+Clip weapons are handled in AddWeapon
        if (info->ammo_[ATK] == kAmmunitionTypeNoAmmo || info->clip_size_[ATK] == 0)
            continue;

        if (ammo != kAmmunitionTypeDontCare && info->ammo_[ATK] != ammo)
            continue;

        if (ammo == kAmmunitionTypeDontCare)
            qty = &p->ammo_[info->ammo_[ATK]].count;

        EPI_ASSERT(qty);

        if (info->clip_size_[ATK] <= *qty)
        {
            p->weapons_[idx].clip_size[ATK] = info->clip_size_[ATK];
            *qty -= info->clip_size_[ATK];

            result = true;
        }
    }

    return result;
}

void FillWeapon(Player *p, int slot)
{
    WeaponDefinition *info = p->weapons_[slot].info;

    for (int ATK = 0; ATK < 4; ATK++)
    {
        if (!info->attack_state_[ATK])
            continue;

        if (info->ammo_[ATK] == kAmmunitionTypeNoAmmo)
        {
            if (info->clip_size_[ATK] > 0)
                p->weapons_[slot].clip_size[ATK] = info->clip_size_[ATK];

            continue;
        }

        p->weapons_[slot].clip_size[ATK] = info->clip_size_[ATK];
    }
}

void DropWeapon(Player *p)
{
    // Player died, so put the weapon away.

    p->remember_attack_state_[0] = -1;
    p->remember_attack_state_[1] = -1;
    p->remember_attack_state_[2] = -1;
    p->remember_attack_state_[3] = -1;

    if (p->ready_weapon_ != KWeaponSelectionNone)
        GotoDownState(p);
}

void SetupPlayerSprites(Player *p)
{
    // --- Called at start of level for each player ---

    // remove all player_sprites_
    for (int i = 0; i < kTotalPlayerSpriteTypes; i++)
    {
        PlayerSprite *psp = &p->player_sprites_[i];

        psp->state      = nullptr;
        psp->next_state = nullptr;
        psp->screen_x = psp->screen_y = 0;
        psp->visibility = psp->target_visibility = 1.0f;
    }

    // choose highest priority FREE weapon as the default
    if (p->ready_weapon_ == KWeaponSelectionNone)
        SelectNewWeapon(p, -100, kAmmunitionTypeDontCare);
    else
        p->pending_weapon_ = p->ready_weapon_;

    BringUpWeapon(p);
}

void MovePlayerSprites(Player *p)
{
    // --- Called every tic by player thinking routine ---

    // check if player has NO weapon but wants to change
    if (p->ready_weapon_ == KWeaponSelectionNone && p->pending_weapon_ != KWeaponSelectionNoChange)
    {
        BringUpWeapon(p);
    }

    PlayerSprite *psp = &p->player_sprites_[0];

    for (int i = 0; i < kTotalPlayerSpriteTypes; i++, psp++)
    {
        // a null state means not active
        if (!psp->state)
            continue;

        for (int loop_count = 0; loop_count < kMaximumPlayerSpriteLoop; loop_count++)
        {
            // drop tic count and possibly change state
            // Note: a -1 tic count never changes.
            if (psp->tics < 0)
                break;

            psp->tics--;

            if (psp->tics > 0)
            {
                if (psp->state->action == A_WeaponReady)
                {
                    BobWeapon(p, p->weapons_[p->ready_weapon_].info);
                }
                break;
            }

            WeaponDefinition *info = nullptr;
            if (p->ready_weapon_ >= 0)
                info = p->weapons_[p->ready_weapon_].info;

            SetPlayerSprite(p, i, psp->next_state ? (psp->next_state - states) : 0, info);

            if (psp->tics != 0)
                break;
        }

        // handle translucency fades
        psp->visibility = (34 * psp->visibility + psp->target_visibility) / 35;
    }

    p->player_sprites_[kPlayerSpriteFlash].screen_x = p->player_sprites_[kPlayerSpriteWeapon].screen_x;
    p->player_sprites_[kPlayerSpriteFlash].screen_y = p->player_sprites_[kPlayerSpriteWeapon].screen_y;

    p->idle_wait_++;
}

//----------------------------------------------------------------------------
//  ACTION HANDLERS
//----------------------------------------------------------------------------

static void BobWeapon(Player *p, WeaponDefinition *info)
{
    if (view_bobbing.d_ == 1 || view_bobbing.d_ == 3 ||
        (erraticism.d_ && (!p->command_.forward_move && !p->command_.side_move)))
        return;

    PlayerSprite *psp = &p->player_sprites_[p->action_player_sprite_];

    float new_sx = p->map_object_->momentum_.Z ? psp->screen_x : 0;
    float new_sy = p->map_object_->momentum_.Z ? psp->screen_y : 0;

    // bob the weapon based on movement speed
    if (p->powers_[kPowerTypeJetpack] <= 0) // Don't bob when using jetpack
    {
        BAMAngle angle = (128 * (erraticism.d_ ? p->erraticism_bob_ticker_++ : level_time_elapsed)) << 19;
        new_sx         = p->bob_factor_ * info->swaying_ * epi::BAMCos(angle);

        angle &= (kBAMAngle180 - 1);
        new_sy = p->bob_factor_ * info->bobbing_ * epi::BAMSin(angle);
    }

    psp->screen_x = new_sx;
    psp->screen_y = new_sy;
}

//
// A_WeaponReady
//
// The player can fire the weapon
// or change to another weapon at this time.
// Follows after getting weapon up,
// or after previous attack/fire sequence.
//
void A_WeaponReady(MapObject *mo)
{
    Player       *p   = mo->player_;
    PlayerSprite *psp = &p->player_sprites_[p->action_player_sprite_];

    EPI_ASSERT(p->ready_weapon_ != KWeaponSelectionNone);

    WeaponDefinition *info = p->weapons_[p->ready_weapon_].info;

    // check for change if player is dead, put the weapon away
    if (p->pending_weapon_ != KWeaponSelectionNoChange || p->health_ <= 0)
    {
        // change weapon (pending weapon should already be validated)
        GotoDownState(p);
        return;
    }

    // check for emptiness.  The ready_state check is needed since this
    // code is also used by the EMPTY action (prevent looping).
    if (info->empty_state_ && !WeaponCouldAutoFire(p, p->ready_weapon_, 0) && psp->state == &states[info->ready_state_])
    {
        // don't use Deferred here, since we don't want the weapon to
        // display the ready sprite (even only briefly).
        SetPlayerSprite(p, kPlayerSpriteWeapon, info->empty_state_, info);
        return;
    }

    if (info->idle_ && (psp->state == &states[info->ready_state_] ||
                        (info->empty_state_ && psp->state == &states[info->empty_state_])))
    {
        StartSoundEffect(info->idle_, WeaponSoundEffectCategory(p), mo);
    }

    bool fire_0 = ButtonDown(p, 0);
    bool fire_1 = ButtonDown(p, 1);
    bool fire_2 = ButtonDown(p, 2);
    bool fire_3 = ButtonDown(p, 3);

    // if (fire_0 != fire_1)
    if (fire_0 || fire_1 || fire_2 || fire_3)
    {
        for (int ATK = 0; ATK < 4; ATK++)
        {
            if (!ButtonDown(p, ATK))
                continue;

            if (!info->attack_state_[ATK])
                continue;

            // check for fire: the missile launcher and bfg do not auto fire
            if (!p->attack_button_down_[ATK] || info->autofire_[ATK])
            {
                p->attack_button_down_[ATK] = true;
                p->flash_                   = false;

                if (WeaponCanFire(p, p->ready_weapon_, ATK))
                    GotoAttackState(p, ATK, true);
                else
                    SwitchAway(p, ATK, info->specials_[ATK] & WeaponFlagReloadWhileTrigger);

                return; // leave now
            }
        }
    }

    // reset memory of held buttons (must be done right here)
    if (!fire_0)
        p->attack_button_down_[0] = false;
    if (!fire_1)
        p->attack_button_down_[1] = false;
    if (!fire_2)
        p->attack_button_down_[2] = false;
    if (!fire_3)
        p->attack_button_down_[3] = false;

    // give that weapon a polish, soldier!
    if (info->idle_state_ && p->idle_wait_ >= info->idle_wait_)
    {
        if (RandomByteTest(info->idle_chance_))
        {
            p->idle_wait_ = 0;
            SetPlayerSpriteDeferred(p, kPlayerSpriteWeapon, info->idle_state_);
        }
        else
        {
            // wait another (idle_wait / 10) seconds before trying again
            p->idle_wait_ = info->idle_wait_ * 9 / 10;
        }
    }

    // handle manual reload and fresh-ammo reload
    if (!fire_0 && !fire_1 && !fire_2 && !fire_3)
    {
        for (int ATK = 0; ATK < 4; ATK++)
        {
            if (!info->attack_state_[ATK])
                continue;

            if ((info->specials_[ATK] & WeaponFlagFreshReload) && (info->clip_size_[ATK] > 0) &&
                !WeaponCanFire(p, p->ready_weapon_, ATK) && WeaponCanReload(p, p->ready_weapon_, ATK, true))
            {
                GotoReloadState(p, ATK);
                break;
            }

            if ((p->command_.extended_buttons & kExtendedButtonCodeReload) && (info->clip_size_[ATK] > 0) &&
                (info->specials_[ATK] & WeaponFlagManualReload) && info->reload_state_[ATK])
            {
                bool reload = WeaponCanReload(p, p->ready_weapon_, ATK, true);

                // for discarding, we require a non-empty clip
                if (reload && info->discard_state_[ATK] && WeaponCanFire(p, p->ready_weapon_, ATK))
                {
                    p->weapons_[p->ready_weapon_].clip_size[ATK] = 0;
                    SetPlayerSpriteDeferred(p, kPlayerSpriteWeapon, info->discard_state_[ATK]);
                    break;
                }
                else if (reload)
                {
                    GotoReloadState(p, ATK);
                    break;
                }
            }
        } // for (ATK)

    }     // (! fire_0 && ! fire_1)

    BobWeapon(p, info);
}

void A_WeaponEmpty(MapObject *mo)
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
static void DoReFire(MapObject *mo, int ATK)
{
    Player *p = mo->player_;

    if (p->pending_weapon_ >= 0 || p->health_ <= 0)
    {
        GotoDownState(p);
        return;
    }

    WeaponDefinition *info = p->weapons_[p->ready_weapon_].info;

    p->remember_attack_state_[ATK] = -1;

    // check for fire
    // (if a weaponchange is pending, let it go through instead)

    if (ButtonDown(p, ATK))
    {
        // -KM- 1999/01/31 Check for semiautomatic weapons.
        if (!p->attack_button_down_[ATK] || info->autofire_[ATK])
        {
            p->refire_++;
            p->flash_ = false;

            if (WeaponCanFire(p, p->ready_weapon_, ATK))
                GotoAttackState(p, ATK, false);
            else
                SwitchAway(p, ATK, info->specials_[ATK] & WeaponFlagReloadWhileTrigger);
            return;
        }
    }

    p->refire_ = info->refire_inacc_ ? 0 : 1;

    if (!WeaponCouldAutoFire(p, p->ready_weapon_, ATK))
        SwitchAway(p, ATK, 0);
}

void A_ReFire(MapObject *mo)
{
    DoReFire(mo, 0);
}
void A_ReFireSA(MapObject *mo)
{
    DoReFire(mo, 1);
}
void A_ReFireTA(MapObject *mo)
{
    DoReFire(mo, 2);
}
void A_ReFireFA(MapObject *mo)
{
    DoReFire(mo, 3);
}

//
// A_ReFireTo
//
// The player can re-fire the weapon without lowering it entirely.
// Unlike A_ReFire, this can re-fire to an arbitrary state
//
static void DoReFireTo(MapObject *mo, int ATK)
{
    Player       *p   = mo->player_;
    PlayerSprite *psp = &p->player_sprites_[p->action_player_sprite_];

    if (p->pending_weapon_ >= 0 || p->health_ <= 0)
    {
        GotoDownState(p);
        return;
    }

    if (psp->state->jumpstate == 0)
        return; // show warning ??

    WeaponDefinition *info = p->weapons_[p->ready_weapon_].info;

    p->remember_attack_state_[ATK] = -1;

    // check for fire
    // (if a weaponchange is pending, let it go through instead)

    if (ButtonDown(p, ATK))
    {
        // -KM- 1999/01/31 Check for semiautomatic weapons.
        if (!p->attack_button_down_[ATK] || info->autofire_[ATK])
        {
            p->refire_++;
            p->flash_ = false;

            if (WeaponCanFire(p, p->ready_weapon_, ATK))
                SetPlayerSpriteDeferred(p, kPlayerSpriteWeapon, psp->state->jumpstate);
            // do the crosshair too?
            else
                SwitchAway(p, ATK, info->specials_[ATK] & WeaponFlagReloadWhileTrigger);
            return;
        }
    }

    p->refire_ = info->refire_inacc_ ? 0 : 1;

    if (!WeaponCouldAutoFire(p, p->ready_weapon_, ATK))
        SwitchAway(p, ATK, 0);
}

void A_ReFireTo(MapObject *mo)
{
    DoReFireTo(mo, 0);
}
void A_ReFireToSA(MapObject *mo)
{
    DoReFireTo(mo, 1);
}
void A_ReFireToTA(MapObject *mo)
{
    DoReFireTo(mo, 2);
}
void A_ReFireToFA(MapObject *mo)
{
    DoReFireTo(mo, 3);
}

//
// A_NoFire
//
// If the player is still holding the fire button, continue, otherwise
// return to the weapon ready states.
//
// -AJA- 1999/08/18: written.
//
static void DoNoFire(MapObject *mo, int ATK, bool does_return)
{
    Player       *p   = mo->player_;
    PlayerSprite *psp = &p->player_sprites_[p->action_player_sprite_];

    if (p->pending_weapon_ >= 0 || p->health_ <= 0)
    {
        GotoDownState(p);
        return;
    }

    WeaponDefinition *info = p->weapons_[p->ready_weapon_].info;

    p->remember_attack_state_[ATK] = -1;

    // check for fire
    //  (if a weaponchange is pending, let it go through instead)

    if (ButtonDown(p, ATK))
    {
        // -KM- 1999/01/31 Check for semiautomatic weapons.
        if (!p->attack_button_down_[ATK] || info->autofire_[ATK])
        {
            p->refire_++;
            p->flash_ = false;

            if (!WeaponCanFire(p, p->ready_weapon_, ATK))
                SwitchAway(p, ATK, info->specials_[ATK] & WeaponFlagReloadWhileTrigger);
            return;
        }
    }

    p->refire_                     = info->refire_inacc_ ? 0 : 1;
    p->remember_attack_state_[ATK] = does_return ? psp->state->nextstate : -1;

    if (WeaponCouldAutoFire(p, p->ready_weapon_, ATK))
        GotoReadyState(p);
    else
        SwitchAway(p, ATK, 0);
}

void A_NoFire(MapObject *mo)
{
    DoNoFire(mo, 0, false);
}
void A_NoFireSA(MapObject *mo)
{
    DoNoFire(mo, 1, false);
}
void A_NoFireTA(MapObject *mo)
{
    DoNoFire(mo, 2, false);
}
void A_NoFireFA(MapObject *mo)
{
    DoNoFire(mo, 3, false);
}
void A_NoFireReturn(MapObject *mo)
{
    DoNoFire(mo, 0, true);
}
void A_NoFireReturnSA(MapObject *mo)
{
    DoNoFire(mo, 1, true);
}
void A_NoFireReturnTA(MapObject *mo)
{
    DoNoFire(mo, 2, true);
}
void A_NoFireReturnFA(MapObject *mo)
{
    DoNoFire(mo, 3, true);
}

void A_WeaponKick(MapObject *mo)
{
    Player       *p   = mo->player_;
    PlayerSprite *psp = &p->player_sprites_[p->action_player_sprite_];

    float kick = 0.05f;

    if (!level_flags.kicking || erraticism.d_)
        return;

    if (psp->state && psp->state->action_par)
        kick = ((float *)psp->state->action_par)[0];

    p->delta_view_height_ -= kick;
    p->kick_offset_ = kick;
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
static void DoCheckReload(MapObject *mo, int ATK)
{
    Player *p = mo->player_;

    if (p->pending_weapon_ >= 0 || p->health_ <= 0)
    {
        GotoDownState(p);
        return;
    }

    //	EPI_ASSERT(p->ready_weapon_ >= 0);
    //
    //	WeaponDefinition *info = p->weapons_[p->ready_weapon_].info;

    if (WeaponCanReload(p, p->ready_weapon_, ATK, false))
        GotoReloadState(p, ATK);
    else if (!WeaponCanFire(p, p->ready_weapon_, ATK))
        SwitchAway(p, ATK, 0);
}

void A_CheckReload(MapObject *mo)
{
    DoCheckReload(mo, 0);
}
void A_CheckReloadSA(MapObject *mo)
{
    DoCheckReload(mo, 1);
}
void A_CheckReloadTA(MapObject *mo)
{
    DoCheckReload(mo, 2);
}
void A_CheckReloadFA(MapObject *mo)
{
    DoCheckReload(mo, 3);
}

void A_Lower(MapObject *mo)
{
    // Lowers current weapon, and changes weapon at bottom

    Player       *p   = mo->player_;
    PlayerSprite *psp = &p->player_sprites_[p->action_player_sprite_];

    WeaponDefinition *info = p->weapons_[p->ready_weapon_].info;

    if (p->zoom_field_of_view_ > 0)
        p->zoom_field_of_view_ = 0;

    psp->screen_y += kWeaponSwapSpeed;

    // Is already down.
    if (!(info->specials_[0] & WeaponFlagAnimated))
        if (psp->screen_y < kWeaponBottom - kWeaponTop)
            return;

    psp->screen_y = kWeaponBottom - kWeaponTop;

    // Player is dead, don't bring weapon back up.
    if (p->player_state_ == kPlayerDead || p->health_ <= 0)
    {
        p->ready_weapon_   = KWeaponSelectionNone;
        p->pending_weapon_ = KWeaponSelectionNoChange;

        SetPlayerSprite(p, kPlayerSpriteWeapon, 0);
        return;
    }

    // handle weapons that were removed/upgraded while in use
    if (p->weapons_[p->ready_weapon_].flags & kPlayerWeaponRemoving)
    {
        p->weapons_[p->ready_weapon_].flags =
            (PlayerWeaponFlag)(p->weapons_[p->ready_weapon_].flags & ~kPlayerWeaponRemoving);
        p->weapons_[p->ready_weapon_].info = nullptr;

        // this should not happen, but handle it just in case
        if (p->pending_weapon_ == p->ready_weapon_)
            p->pending_weapon_ = KWeaponSelectionNoChange;

        p->ready_weapon_ = KWeaponSelectionNone;
    }

    // The old weapon has been lowered off the screen,
    // so change the weapon and start raising it

    if (p->pending_weapon_ == KWeaponSelectionNoChange)
    {
        p->ready_weapon_ = KWeaponSelectionNone;
        SelectNewWeapon(p, -100, kAmmunitionTypeDontCare);
    }

    BringUpWeapon(p);
}

void A_Raise(MapObject *mo)
{
    Player       *p   = mo->player_;
    PlayerSprite *psp = &p->player_sprites_[p->action_player_sprite_];

    WeaponDefinition *info = p->weapons_[p->ready_weapon_].info;

    psp->screen_y -= kWeaponSwapSpeed;

    if (psp->screen_y > 0)
        return;

    psp->screen_y = 0;

    // The weapon has been raised all the way,
    //  so change to the ready state.
    if (info->empty_state_ && !WeaponCouldAutoFire(p, p->ready_weapon_, 0))
        GotoEmptyState(p);
    else
        GotoReadyState(p);
}

void A_SetCrosshair(MapObject *mo)
{
    Player       *p   = mo->player_;
    PlayerSprite *psp = &p->player_sprites_[p->action_player_sprite_];

    if (psp->state->jumpstate == 0)
        return; // show warning ??

    SetPlayerSpriteDeferred(p, kPlayerSpriteCrosshair, psp->state->jumpstate);
}

void A_TargetJump(MapObject *mo)
{
    Player       *p   = mo->player_;
    PlayerSprite *psp = &p->player_sprites_[p->action_player_sprite_];

    if (psp->state->jumpstate == 0)
        return; // show warning ?? error ???

    if (p->ready_weapon_ == KWeaponSelectionNone)
        return;

    AttackDefinition *attack = p->weapons_[p->ready_weapon_].info->attack_[0];

    if (!attack)
        return;

    MapObject *obj = MapTargetAutoAim(mo, mo->angle_, attack->range_, true);

    if (!obj)
        return;

    SetPlayerSpriteDeferred(p, kPlayerSpriteCrosshair, psp->state->jumpstate);
}

void A_FriendJump(MapObject *mo)
{
    Player       *p   = mo->player_;
    PlayerSprite *psp = &p->player_sprites_[p->action_player_sprite_];

    if (psp->state->jumpstate == 0)
        return; // show warning ?? error ???

    if (p->ready_weapon_ == KWeaponSelectionNone)
        return;

    AttackDefinition *attack = p->weapons_[p->ready_weapon_].info->attack_[0];

    if (!attack)
        return;

    MapObject *obj = MapTargetAutoAim(mo, mo->angle_, attack->range_, true);

    if (!obj)
        return;

    if ((obj->side_ & mo->side_) == 0 || obj->target_ == mo)
        return;

    SetPlayerSpriteDeferred(p, kPlayerSpriteCrosshair, psp->state->jumpstate);
}

static void DoGunFlash(MapObject *mo, int ATK)
{
    Player *p = mo->player_;

    EPI_ASSERT(p->ready_weapon_ >= 0);

    WeaponDefinition *info = p->weapons_[p->ready_weapon_].info;

    if (!p->flash_)
    {
        p->flash_ = true;

        SetPlayerSpriteDeferred(p, kPlayerSpriteFlash, info->flash_state_[ATK]);
    }
}

void A_GunFlash(MapObject *mo)
{
    DoGunFlash(mo, 0);
}
void A_GunFlashSA(MapObject *mo)
{
    DoGunFlash(mo, 1);
}
void A_GunFlashTA(MapObject *mo)
{
    DoGunFlash(mo, 2);
}
void A_GunFlashFA(MapObject *mo)
{
    DoGunFlash(mo, 3);
}

static void DoWeaponShoot(MapObject *mo, int ATK)
{
    Player       *p   = mo->player_;
    PlayerSprite *psp = &p->player_sprites_[p->action_player_sprite_];

    EPI_ASSERT(p->ready_weapon_ >= 0);

    WeaponDefinition *info   = p->weapons_[p->ready_weapon_].info;
    AttackDefinition *attack = info->attack_[ATK];

    // -AJA- 1999/08/10: Multiple attack support.
    if (psp->state && psp->state->action_par)
        attack = (AttackDefinition *)psp->state->action_par;

    if (!attack)
        FatalError("Weapon [%s] missing attack for %s action.\n", info->name_.c_str(), ATK ? "XXXSHOOT" : "SHOOT");

    // Some do not need ammunition anyway.
    // Return if current ammunition sufficient.
    if (!WeaponCanFire(p, p->ready_weapon_, ATK))
        return;

    int ATK_orig = ATK;
    if (info->shared_clip_)
        ATK = 0;

    AmmunitionType ammo = info->ammo_[ATK];

    // Minimal amount for one shot varies.
    int count = info->ammopershot_[ATK];

    if (info->clip_size_[ATK] > 0)
    {
        p->weapons_[p->ready_weapon_].clip_size[ATK] -= count;
        EPI_ASSERT(p->weapons_[p->ready_weapon_].clip_size[ATK] >= 0);
    }
    else if (ammo != kAmmunitionTypeNoAmmo)
    {
        p->ammo_[ammo].count -= count;
        EPI_ASSERT(p->ammo_[ammo].count >= 0);
    }

    PlayerAttack(mo, attack);

    if (level_flags.kicking && ATK == 0 && !erraticism.d_)
    {
        p->delta_view_height_ -= info->kick_;
        p->kick_offset_ = info->kick_;
    }

    if (mo->target_)
    {
        if (info->hit_)
            StartSoundEffect(info->hit_, WeaponSoundEffectCategory(p), mo);

        if (info->feedback_)
            mo->flags_ |= kMapObjectFlagJustAttacked;
    }
    else
    {
        if (info->engaged_)
            StartSoundEffect(info->engaged_, WeaponSoundEffectCategory(p), mo);
    }

    // show the player making the shot/attack...
    if (attack && attack->attackstyle_ == kAttackStyleCloseCombat && mo->info_->melee_state_)
    {
        MapObjectSetStateDeferred(mo, mo->info_->melee_state_, 0);
    }
    else if (mo->info_->missile_state_)
    {
        MapObjectSetStateDeferred(mo, mo->info_->missile_state_, 0);
    }

    ATK = ATK_orig;

    if (info->flash_state_[ATK] && !p->flash_)
    {
        p->flash_ = true;
        SetPlayerSpriteDeferred(p, kPlayerSpriteFlash, info->flash_state_[ATK]);
    }

    // wake up monsters
    if (!(info->specials_[ATK] & WeaponFlagSilentToMonsters) && !(attack->flags_ & kAttackFlagSilentToMonsters))
    {
        NoiseAlert(p);
    }

    p->idle_wait_ = 0;
}

void A_WeaponShoot(MapObject *mo)
{
    DoWeaponShoot(mo, 0);
}
void A_WeaponShootSA(MapObject *mo)
{
    DoWeaponShoot(mo, 1);
}
void A_WeaponShootTA(MapObject *mo)
{
    DoWeaponShoot(mo, 2);
}
void A_WeaponShootFA(MapObject *mo)
{
    DoWeaponShoot(mo, 3);
}

//
// Used for ejecting shells (or other effects).
//
// -AJA- 1999/09/10: written.
//
void A_WeaponEject(MapObject *mo)
{
    Player       *p   = mo->player_;
    PlayerSprite *psp = &p->player_sprites_[p->action_player_sprite_];

    WeaponDefinition *info   = p->weapons_[p->ready_weapon_].info;
    AttackDefinition *attack = info->eject_attack_;

    if (psp->state && psp->state->action_par)
        attack = (AttackDefinition *)psp->state->action_par;

    if (!attack)
        FatalError("Weapon [%s] missing attack for EJECT action.\n", info->name_.c_str());

    PlayerAttack(mo, attack);
}

void A_WeaponPlaySound(MapObject *mo)
{
    // Generate an arbitrary sound from this weapon.

    Player       *p   = mo->player_;
    PlayerSprite *psp = &p->player_sprites_[p->action_player_sprite_];

    SoundEffect *sound = nullptr;

    if (psp->state && psp->state->action_par)
        sound = (SoundEffect *)psp->state->action_par;

    if (!sound)
    {
        PrintWarningOrError("A_WeaponPlaySound: missing sound name !\n");
        return;
    }

    StartSoundEffect(sound, WeaponSoundEffectCategory(p), mo);
}

void A_WeaponKillSound(MapObject *mo)
{
    // kill any current sound from this weapon

    StopSoundEffect(mo);
}

void A_SFXWeapon1(MapObject *mo)
{
    Player *p = mo->player_;
    StartSoundEffect(p->weapons_[p->ready_weapon_].info->sound1_, WeaponSoundEffectCategory(p), mo);
}

void A_SFXWeapon2(MapObject *mo)
{
    Player *p = mo->player_;
    StartSoundEffect(p->weapons_[p->ready_weapon_].info->sound2_, WeaponSoundEffectCategory(p), mo);
}

void A_SFXWeapon3(MapObject *mo)
{
    Player *p = mo->player_;
    StartSoundEffect(p->weapons_[p->ready_weapon_].info->sound3_, WeaponSoundEffectCategory(p), mo);
}

//
// These three routines make a flash of light when a weapon fires.
//
void A_Light0(MapObject *mo)
{
    mo->player_->extra_light_ = 0;
}
void A_Light1(MapObject *mo)
{
    if (!reduce_flash)
        mo->player_->extra_light_ = 1;
    else
        mo->player_->extra_light_ = 0;
}
void A_Light2(MapObject *mo)
{
    if (!reduce_flash)
        mo->player_->extra_light_ = 2;
    else
        mo->player_->extra_light_ = 0;
}

void A_WeaponJump(MapObject *mo)
{
    Player       *p   = mo->player_;
    PlayerSprite *psp = &p->player_sprites_[p->action_player_sprite_];

    WeaponDefinition *info = p->weapons_[p->ready_weapon_].info;

    JumpActionInfo *jump;

    if (!psp->state || !psp->state->action_par)
    {
        PrintWarningOrError("JUMP used in weapon [%s] without a label !\n", info->name_.c_str());
        return;
    }

    jump = (JumpActionInfo *)psp->state->action_par;

    EPI_ASSERT(jump->chance >= 0);
    EPI_ASSERT(jump->chance <= 1);

    if (RandomByteTestDeterministic(jump->chance))
    {
        psp->next_state = (psp->state->jumpstate == 0) ? nullptr : (states + psp->state->jumpstate);
    }
}

// Lobo: what the hell is this function for?
void A_WeaponDJNE(MapObject *mo)
{
    Player       *p   = mo->player_;
    PlayerSprite *psp = &p->player_sprites_[p->action_player_sprite_];

    WeaponDefinition *info = p->weapons_[p->ready_weapon_].info;

    JumpActionInfo *jump;

    if (!psp->state || !psp->state->action_par)
    {
        PrintWarningOrError("DJNE used in weapon [%s] without a label !\n", info->name_.c_str());
        return;
    }

    jump = (JumpActionInfo *)psp->state->action_par;

    EPI_ASSERT(jump->chance >= 0);
    EPI_ASSERT(jump->chance <= 1);

    int ATK = jump->chance > 0 ? 1 : 0; // Lobo: fixme for 3rd and 4th attack?

    if (--p->weapons_[p->ready_weapon_].reload_count[ATK] > 0)
    {
        psp->next_state = (psp->state->jumpstate == 0) ? nullptr : (states + psp->state->jumpstate);
    }
}

void A_WeaponTransSet(MapObject *mo)
{
    Player       *p     = mo->player_;
    PlayerSprite *psp   = &p->player_sprites_[p->action_player_sprite_];
    float         value = 1.0f;

    if (psp->state && psp->state->action_par)
    {
        value = ((float *)psp->state->action_par)[0];
        value = HMM_MAX(0.0f, HMM_MIN(1.0f, value));
    }

    psp->visibility = psp->target_visibility = value;
}

void A_WeaponTransFade(MapObject *mo)
{
    Player       *p   = mo->player_;
    PlayerSprite *psp = &p->player_sprites_[p->action_player_sprite_];

    float value = 0.0f;

    if (psp->state && psp->state->action_par)
    {
        value = ((float *)psp->state->action_par)[0];
        value = HMM_MAX(0.0f, HMM_MIN(1.0f, value));
    }

    psp->target_visibility = value;
}

void A_WeaponEnableRadTrig(MapObject *mo)
{
    Player       *p   = mo->player_;
    PlayerSprite *psp = &p->player_sprites_[p->action_player_sprite_];

    if (psp->state && psp->state->action_par)
    {
        int tag = *(int *)psp->state->action_par;
        ScriptEnableByTag(mo, tag, false, (TriggerScriptTag)psp->state->rts_tag_type);
    }
}

void A_WeaponDisableRadTrig(MapObject *mo)
{
    Player       *p   = mo->player_;
    PlayerSprite *psp = &p->player_sprites_[p->action_player_sprite_];

    if (psp->state && psp->state->action_par)
    {
        int tag = *(int *)psp->state->action_par;
        ScriptEnableByTag(mo, tag, true, (TriggerScriptTag)psp->state->rts_tag_type);
    }
}

void A_WeaponSetSkin(MapObject *mo)
{
    Player       *p   = mo->player_;
    PlayerSprite *psp = &p->player_sprites_[p->action_player_sprite_];

    EPI_ASSERT(p->ready_weapon_ >= 0);
    WeaponDefinition *info = p->weapons_[p->ready_weapon_].info;

    const State *st = psp->state;

    if (st && st->action_par)
    {
        int skin = ((int *)st->action_par)[0];

        if (skin < 0 || skin > 9)
            FatalError("Weapon [%s]: Bad skin number %d in SET_SKIN action.\n", info->name_.c_str(), skin);

        p->weapons_[p->ready_weapon_].model_skin = skin;
    }
}

void A_WeaponUnzoom(MapObject *mo)
{
    Player *p = mo->player_;

    p->zoom_field_of_view_ = 0;
}

// Handle potential New clip size being smaller than old
void FixWeaponClip(Player *p, int slot)
{
    WeaponDefinition *info = p->weapons_[slot].info;

    for (int ATK = 0; ATK < 4; ATK++)
    {
        if (!info->attack_state_[ATK])
            continue;

        if (info->ammo_[ATK] == kAmmunitionTypeNoAmmo) // infinite ammo?
        {
            if (info->clip_size_[ATK] > 0)             // and has a clip?
            {
                // Current ammo bigger than new clipsize?
                // If so, reduce ammo to new clip size
                if (p->weapons_[slot].clip_size[ATK] > info->clip_size_[ATK])
                    p->weapons_[slot].clip_size[ATK] = info->clip_size_[ATK];
            }

            continue;
        }

        // Current ammo bigger than new clipsize?
        // If so, reduce ammo to new clip size
        if (p->weapons_[slot].clip_size[ATK] > info->clip_size_[ATK])
            p->weapons_[slot].clip_size[ATK] = info->clip_size_[ATK];
    }
}

void A_WeaponBecome(MapObject *mo)
{
    Player       *p   = mo->player_;
    PlayerSprite *psp = &p->player_sprites_[p->action_player_sprite_];

    WeaponDefinition *oldWep = p->weapons_[p->ready_weapon_].info;

    if (!psp->state || !psp->state->action_par)
    {
        FatalError("BECOME used in weapon [%s] without a label !\n", oldWep->name_.c_str());
        return; /* NOT REACHED */
    }

    WeaponBecomeActionInfo *become = (WeaponBecomeActionInfo *)psp->state->action_par;

    if (!become->info_)
    {
        become->info_ = weapondefs.Lookup(become->info_ref_.c_str());
        EPI_ASSERT(become->info_); // lookup should be OK (fatal error if not found)
    }

    WeaponDefinition *newWep = weapondefs.Lookup(become->info_ref_.c_str());

    p->weapons_[p->ready_weapon_].info = newWep; // here it BECOMES()

    int state = DDF_StateFindLabel(newWep->state_grp_, become->start_.label_.c_str(), true /* quiet */);
    if (state == 0)
        FatalError("BECOME action: frame '%s' in [%s] not found!\n", become->start_.label_.c_str(),
                   newWep->name_.c_str());

    state += become->start_.offset_;
    SetPlayerSpriteDeferred(p, kPlayerSpriteWeapon,
                            state);  // refresh the sprite

    FixWeaponClip(p,
                  p->ready_weapon_); // handle the potential clip_size difference

    UpdateAvailWeapons(p);

    // SetPlayerSpriteDeferred(p,kPlayerSpriteWeapon,p->weapons_[p->ready_weapon_].info->ready_state);
}

void A_WeaponZoom(MapObject *mo)
{
    Player *p = mo->player_;

    int fov = p->zoom_field_of_view_;

    if (p->zoom_field_of_view_ == 0) // only zoom if we're not already
    {
        if (!(p->ready_weapon_ < 0 || p->pending_weapon_ >= 0))
            fov = p->weapons_[p->ready_weapon_].info->zoom_fov_;

        if (fov == int(kBAMAngle360))
            fov = 0;
    }

    p->zoom_field_of_view_ = fov;
}

void WA_MoveFwd(MapObject *mo)
{
    Player       *p   = mo->player_;
    PlayerSprite *psp = &p->player_sprites_[p->action_player_sprite_];

    const State *st = psp->state;

    if (st && st->action_par)
    {
        float amount = *(float *)st->action_par;

        float dx = epi::BAMCos(mo->angle_);
        float dy = epi::BAMSin(mo->angle_);

        mo->momentum_.X += dx * amount;
        mo->momentum_.Y += dy * amount;
    }
}

void WA_MoveRight(MapObject *mo)
{
    Player       *p   = mo->player_;
    PlayerSprite *psp = &p->player_sprites_[p->action_player_sprite_];

    const State *st = psp->state;

    if (st && st->action_par)
    {
        float amount = *(float *)st->action_par;

        float dx = epi::BAMCos(mo->angle_ - kBAMAngle90);
        float dy = epi::BAMSin(mo->angle_ - kBAMAngle90);

        mo->momentum_.X += dx * amount;
        mo->momentum_.Y += dy * amount;
    }
}

void WA_MoveUp(MapObject *mo)
{
    Player       *p   = mo->player_;
    PlayerSprite *psp = &p->player_sprites_[p->action_player_sprite_];

    const State *st = psp->state;

    if (st && st->action_par)
        mo->momentum_.Z += *(float *)st->action_par;
}

void WA_TurnDir(MapObject *mo)
{
    Player       *p   = mo->player_;
    PlayerSprite *psp = &p->player_sprites_[p->action_player_sprite_];

    const State *st = psp->state;

    if (st && st->action_par)
        mo->angle_ += *(BAMAngle *)st->action_par;
}

void WA_TurnRandom(MapObject *mo)
{
    Player       *p   = mo->player_;
    PlayerSprite *psp = &p->player_sprites_[p->action_player_sprite_];

    const State *st            = psp->state;
    int          turn          = 359;
    int          random_angle  = 0;
    int          current_angle = (int)epi::DegreesFromBAM(mo->angle_);

    if (current_angle >= 360)
        current_angle -= 360;

    if (current_angle < 0)
        current_angle += 360;

    if (st && st->action_par)
    {
        turn = *(int *)st->action_par;
    }

    // We want a random number between 0 and our parameter
    if (turn < 0) // between -x and 0
        random_angle = turn + (0 - turn) * (RandomShort() / double(0x10000));
    else          // between 0 and x
        random_angle = 0 + (turn - 0) * (RandomShort() / double(0x10000));

    turn       = current_angle + random_angle;
    mo->angle_ = epi::BAMFromDegrees(turn);
}

void WA_MlookTurn(MapObject *mo)
{
    Player       *p   = mo->player_;
    PlayerSprite *psp = &p->player_sprites_[p->action_player_sprite_];

    const State *st = psp->state;

    if (st && st->action_par)
        mo->vertical_angle_ += epi::BAMFromATan(*(float *)st->action_par);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
