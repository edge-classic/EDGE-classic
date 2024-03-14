//----------------------------------------------------------------------------
//  EDGE Game Handling Code
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
// -MH- 1998/07/02 Added key_fly_up and key_fly_down variables (no logic yet)
// -MH- 1998/08/18 Flyup and flydown logic
//

#include "e_player.h"

#include <string.h>

#include "bot_think.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "endianess.h"
#include "g_game.h"
#include "hu_stuff.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_random.h"
#include "p_local.h"
#include "script/compat/lua_compat.h"
#include "sokol_color.h"
#include "str_util.h"
#include "vm_coal.h"  // For CoalEndLevel()

//
// PLAYER ARRAY
//
// Main rule is that players[p->num] == p (for all players p).
// The array only holds players "in game", the remaining fields
// are nullptr.  There may be nullptr entries in-between valid entries
// (e.g. player #2 left the game, so players[2] becomes nullptr).
// This means that numplayers is NOT an index to last entry + 1.
//
// The consoleplayer and displayplayer variables must be valid
// indices at all times.
//
Player *players[kMaximumPlayers];
int     total_players;
int     total_bots;

int console_player = -1;  // player taking events
int display_player = -1;  // view being displayed

static constexpr uint8_t kMaximumBodies = 50;

static MapObject *body_queue[kMaximumBodies];
static int        body_queue_size = 0;

// Maintain single and multi player starting spots.
static std::vector<SpawnPoint> deathmatch_starts;
static std::vector<SpawnPoint> coop_starts;
static std::vector<SpawnPoint> voodoo_dolls;
static std::vector<SpawnPoint> hub_starts;

static void P_SpawnPlayer(Player *p, const SpawnPoint *point, bool is_hub);

void ClearPlayerStarts(void)
{
    deathmatch_starts.clear();
    coop_starts.clear();
    voodoo_dolls.clear();
    hub_starts.clear();
}

void ClearBodyQueue(void)
{
    memset(body_queue, 0, sizeof(body_queue));

    body_queue_size = 0;
}

void GameAddBodyToQueue(MapObject *mo)
{
    // flush an old corpse if needed
    if (body_queue_size >= kMaximumBodies)
    {
        MapObject *rotten = body_queue[body_queue_size % kMaximumBodies];
        rotten->reference_count_--;

        RemoveMapObject(rotten);
    }

    // prevent accidental re-use
    mo->reference_count_++;

    body_queue[body_queue_size % kMaximumBodies] = mo;
    body_queue_size++;
}

//
// Called when a player completes a level.
// For HUB changes, we keep powerups and keycards
//
void PlayerFinishLevel(Player *p, bool keep_cards)
{
    if (!keep_cards)
    {
        for (int i = 0; i < kTotalPowerTypes; i++) p->powers_[i] = 0;

        p->keep_powers_ = 0;

        p->cards_ = kDoorKeyNone;

        p->map_object_->flags_ &= ~kMapObjectFlagFuzzy;  // cancel invisibility
    }

    p->extra_light_ = 0;  // cancel gun flashes

    // cancel colourmap effects
    p->effect_colourmap_ = nullptr;

    // no palette changes
    p->damage_count_       = 0;
    p->damage_pain_        = 0;
    p->bonus_count_        = 0;
    p->grin_count_         = 0;
    p->last_damage_colour_ = SG_RED_RGBA32;

    // Lobo 2023: uncomment if still getting
    //  "INTERNAL ERROR: player has a removed attacker"
    p->attacker_ = nullptr;

    if (LuaUseLuaHud())
        LuaEndLevel();
    else
        CoalEndLevel();
}

//
// Player::Reborn
//
// Called after a player dies.
// Almost everything is cleared and initialised.
//
void Player::Reborn()
{
    LogDebug("Player::Reborn\n");

    player_state_ = kPlayerAlive;

    map_object_ = nullptr;
    health_     = 0;

    memset(armours_, 0, sizeof(armours_));
    memset(armour_types_, 0, sizeof(armour_types_));
    memset(powers_, 0, sizeof(powers_));

    keep_powers_  = 0;
    total_armour_ = 0;
    cards_        = kDoorKeyNone;

    ready_weapon_   = KWeaponSelectionNone;
    pending_weapon_ = KWeaponSelectionNoChange;

    memset(weapons_, 0, sizeof(weapons_));
    memset(available_weapons_, 0, sizeof(available_weapons_));
    memset(ammo_, 0, sizeof(ammo_));
    memset(inventory_, 0, sizeof(inventory_));
    memset(counters_, 0, sizeof(counters_));

    for (int w = 0; w <= 9; w++) key_choices_[w] = KWeaponSelectionNone;

    cheats_             = 0;
    refire_             = 0;
    bob_factor_         = 0;
    kick_offset_        = 0;
    zoom_field_of_view_ = 0;
    bonus_count_        = 0;
    damage_count_       = 0;
    damage_pain_        = 0;
    extra_light_        = 0;
    flash_              = false;
    last_damage_colour_ = SG_RED_RGBA32;

    attacker_ = nullptr;

    effect_colourmap_ = nullptr;
    effect_left_      = 0;

    memset(player_sprites_, 0, sizeof(player_sprites_));

    jump_wait_    = 0;
    idle_wait_    = 0;
    splash_wait_  = 0;
    air_in_lungs_ = 0;
    underwater_   = false;
    airless_      = false;
    swimming_     = false;
    wet_feet_     = false;

    grin_count_             = 0;
    attack_sustained_count_ = 0;
    face_index_             = 0;
    face_count_             = 0;

    remember_attack_state_[0] = -1;
    remember_attack_state_[1] = -1;
    remember_attack_state_[2] = -1;
    remember_attack_state_[3] = -1;
    weapon_last_frame_        = -1;
}

//
// Returns false if the player cannot be respawned at the given spot
// because something is occupying it.
//
static bool GameCheckSpot(Player *player, const SpawnPoint *point)
{
    float x = point->x;
    float y = point->y;
    float z = point->z;

    if (!player->map_object_)
    {
        // first spawn of level, before corpses
        for (int player_number_ = 0; player_number_ < kMaximumPlayers;
             player_number_++)
        {
            Player *p = players[player_number_];

            if (!p || !p->map_object_ || p == player) continue;

            if (fabs(p->map_object_->x - x) < 8.0f &&
                fabs(p->map_object_->y - y) < 8.0f)
                return false;
        }

        P_SpawnPlayer(player, point, false);
        return true;  // OK
    }

    if (!CheckAbsolutePosition(player->map_object_, x, y, z)) return false;

    GameAddBodyToQueue(player->map_object_);

    // spawn a teleport fog
    // (temp fix for teleport effect)
    x += 20 * epi::BAMCos(point->angle);
    y += 20 * epi::BAMSin(point->angle);
    CreateMapObject(x, y, z, mobjtypes.Lookup("TELEPORT_FLASH"));

    P_SpawnPlayer(player, point, false);
    return true;  // OK
}

//
// GameSetConsolePlayer
//
// Note: we don't rely on current value being valid, hence can use
//       these functions during initialisation.
//
void SetConsolePlayer(int player_number_)
{
    console_player = player_number_;

    EPI_ASSERT(players[console_player]);

    for (int i = 0; i < kMaximumPlayers; i++)
        if (players[i]) players[i]->player_flags_ &= ~kPlayerFlagConsole;

    players[player_number_]->player_flags_ |= kPlayerFlagConsole;

    if (ArgumentFind("testbot") > 0)
    {
        P_BotCreate(players[player_number_], false);
    }
    else
    {
        players[player_number_]->Builder     = ConsolePlayerBuilder;
        players[player_number_]->build_data_ = nullptr;
    }
}

//
// GameSetDisplayPlayer
//
void SetDisplayPlayer(int player_number_)
{
    display_player = player_number_;

    EPI_ASSERT(players[display_player]);

    for (int i = 0; i < kMaximumPlayers; i++)
        if (players[i]) players[i]->player_flags_ &= ~kPlayerFlagDisplay;

    players[player_number_]->player_flags_ |= kPlayerFlagDisplay;
}

void ToggleDisplayPlayer(void)
{
    for (int i = 1; i <= kMaximumPlayers; i++)
    {
        int player_number_ = (display_player + i) % kMaximumPlayers;

        if (players[player_number_])
        {
            SetDisplayPlayer(player_number_);
            break;
        }
    }
}

//
// P_SpawnPlayer
//
// Called when a player is spawned on the level.
// Most of the player structure stays unchanged between levels.
//
// -KM- 1998/12/21 Cleaned this up a bit.
// -KM- 1999/01/31 Removed all those nasty cases for doomednum (1/4001)
//
static void P_SpawnPlayer(Player *p, const SpawnPoint *point, bool is_hub)
{
    // -KM- 1998/11/25 This is in preparation for skins.  The creatures.ddf
    //   will hold player start objects, sprite will be taken for skin.
    // -AJA- 2004/04/14: Use DDF entry from level thing.

    if (point->info == nullptr) FatalError("P_SpawnPlayer: No such item type!");

    const MapObjectDefinition *info = point->info;

    LogDebug("* P_SpawnPlayer %d @ %1.0f,%1.0f\n", point->info->playernum_,
             point->x, point->y);

    if (info->playernum_ <= 0)
        info = mobjtypes.LookupPlayer(p->player_number_ + 1);

    if (p->player_state_ == kPlayerAwaitingRespawn)
    {
        p->Reborn();

        GiveInitialBenefits(p, info);
    }

    MapObject *mobj = CreateMapObject(point->x, point->y, point->z, info);

    mobj->angle_          = point->angle;
    mobj->vertical_angle_ = point->vertical_angle;
    mobj->player_         = p;
    mobj->health_         = p->health_;

    p->map_object_           = mobj;
    p->player_state_         = kPlayerAlive;
    p->refire_               = 0;
    p->damage_count_         = 0;
    p->damage_pain_          = 0;
    p->bonus_count_          = 0;
    p->extra_light_          = 0;
    p->effect_colourmap_     = nullptr;
    p->standard_view_height_ = mobj->height_ * info->viewheight_;
    p->view_height_          = p->standard_view_height_;
    p->zoom_field_of_view_   = 0;
    p->jump_wait_            = 0;

    // don't do anything immediately
    p->attack_button_down_[0] = p->attack_button_down_[1] = false;
    p->attack_button_down_[2] = p->attack_button_down_[3] = false;
    p->use_button_down_                                   = false;
    p->action_button_down_[0] = p->action_button_down_[1] = false;

    // setup gun psprite
    if (!is_hub || !InSinglePlayerMatch()) SetupPlayerSprites(p);

    // give all cards in death match mode
    if (InDeathmatch()) p->cards_ = kDoorKeyBitmask;

    // -AJA- in COOP, all players are on the same side
    if (InCooperativeMatch()) mobj->side_ = ~0;

    // Don't get stuck spawned in things: telefrag them.

    /* Dasho 2023.10.09 - Ran into a map where having the player stuck inside
    a thing next to it with a sufficiently large radius was an intentional
    mechanic (The All-Ghosts Forest). Telefragging in this scenario seems
    to diverge from reasonably 'correct' behavior compared to ports with good
    vanilla/Boom compat, so I'm commenting this out. I had to do this previously
    for voodoo dolls because it would break certain maps. */

    // TeleportMove(mobj, mobj->x, mobj->y, mobj->z);

    if (InCooperativeMatch() && !level_flags.team_damage)
        mobj->hyper_flags_ |= kHyperFlagFriendlyFireImmune;

    if (p->IsBot())
    {
        DeathBot *bot = (DeathBot *)p->build_data_;
        EPI_ASSERT(bot);

        bot->Respawn();
    }
}

static void P_SpawnVoodooDoll(Player *p, const SpawnPoint *point)
{
    const MapObjectDefinition *info = point->info;

    EPI_ASSERT(info);
    EPI_ASSERT(info->playernum_ > 0);

    LogDebug("* P_SpawnVoodooDoll %d @ %1.0f,%1.0f\n", p->player_number_ + 1,
             point->x, point->y);

    MapObject *mobj = CreateMapObject(point->x, point->y, point->z, info);

    mobj->angle_          = point->angle;
    mobj->vertical_angle_ = point->vertical_angle;
    mobj->player_         = p;
    mobj->health_         = p->health_;

    mobj->is_voodoo_ = true;

    if (InCooperativeMatch()) mobj->side_ = ~0;
}

//
// GameDeathMatchSpawnPlayer
//
// Spawns a player at one of the random deathmatch spots.
// Called at level load and each death.
//
void DeathMatchSpawnPlayer(Player *p)
{
    if (p->player_number_ >= (int)deathmatch_starts.size())
        LogWarning("Few deathmatch spots, %d recommended.\n",
                   p->player_number_ + 1);

    int begin = RandomByteDeterministic();

    if (deathmatch_starts.size() > 0)
    {
        for (int j = 0; j < (int)deathmatch_starts.size(); j++)
        {
            int i = (begin + j) % (int)deathmatch_starts.size();

            if (GameCheckSpot(p, &deathmatch_starts[i])) return;
        }
    }

    // no good spot, so the player will probably get stuck
    if (coop_starts.size() > 0)
    {
        for (int j = 0; j < (int)coop_starts.size(); j++)
        {
            int i = (begin + j) % (int)coop_starts.size();

            if (GameCheckSpot(p, &coop_starts[i])) return;
        }
    }

    FatalError("No usable DM start found!");
}

//
// GameCoopSpawnPlayer
//
// Spawns a player at one of the single player spots.
// Called at level load and each death.
//
void CoopSpawnPlayer(Player *p)
{
    SpawnPoint *point = FindCoopPlayer(p->player_number_ + 1);

    if (point && GameCheckSpot(p, point)) return;

    LogWarning("Player %d start is invalid.\n", p->player_number_ + 1);

    int begin = p->player_number_;

    // try to spawn at one of the other players spots
    for (int j = 0; j < (int)coop_starts.size(); j++)
    {
        int i = (begin + j) % (int)coop_starts.size();

        if (GameCheckSpot(p, &coop_starts[i])) return;
    }

    FatalError("No usable player start found!\n");
}

static SpawnPoint *GameFindHubPlayer(int player_number_, int tag)
{
    int count = 0;

    for (int i = 0; i < (int)hub_starts.size(); i++)
    {
        SpawnPoint *point = &hub_starts[i];
        EPI_ASSERT(point->info);

        if (point->tag != tag) continue;

        count++;

        if (point->info->playernum_ == player_number_) return point;
    }

    if (count == 0)
        FatalError("Missing hub starts with tag %d\n", tag);
    else
        FatalError("No usable hub start for player %d (tag %d)\n",
                   player_number_ + 1, tag);

    return nullptr; /* NOT REACHED */
}

void GameHubSpawnPlayer(Player *p, int tag)
{
    EPI_ASSERT(!p->map_object_);

    SpawnPoint *point = GameFindHubPlayer(p->player_number_ + 1, tag);

    // assume player will fit (too bad otherwise)
    P_SpawnPlayer(p, point, true);
}

void SpawnVoodooDolls(Player *p)
{
    for (int i = 0; i < (int)voodoo_dolls.size(); i++)
    {
        SpawnPoint *point = &voodoo_dolls[i];

        if (point->info->playernum_ != p->player_number_ + 1) continue;

        P_SpawnVoodooDoll(p, point);
    }
}

// number of wanted dogs (1-3)
EDGE_DEFINE_CONSOLE_VARIABLE(dogs, "0", kConsoleVariableFlagArchive)

void SpawnHelper(int player_number_)
{
    if (player_number_ == 0) return;

    if (player_number_ > dogs.d_) return;

    SpawnPoint *point = FindCoopPlayer(player_number_ + 1);
    if (point == nullptr) return;

    const MapObjectDefinition *info = mobjtypes.Lookup(888);
    if (info == nullptr) return;

    MapObject *mo = CreateMapObject(point->x, point->y, point->z, info);

    mo->angle_      = point->angle;
    mo->spawnpoint_ = *point;

    mo->side_ = ~0;
}

bool GameCheckConditions(MapObject *mo, ConditionCheck *cond)
{
    Player *p = mo->player_;
    bool    temp;

    for (; cond; cond = cond->next)
    {
        int i_amount = (int)(cond->amount + 0.5f);

        switch (cond->cond_type)
        {
            case kConditionCheckTypeHealth:
                if (cond->exact) return (mo->health_ == cond->amount);

                temp = (mo->health_ >= cond->amount);

                if ((!cond->negate && !temp) || (cond->negate && temp))
                    return false;

                break;

            case kConditionCheckTypeArmour:
                if (!p) return false;

                if (cond->exact)
                {
                    if (cond->sub.type == kTotalArmourTypes)
                        return (p->total_armour_ == i_amount);
                    else
                        return (p->armours_[cond->sub.type] == i_amount);
                }

                if (cond->sub.type == kTotalArmourTypes)
                    temp = (p->total_armour_ >= i_amount);
                else
                    temp = (p->armours_[cond->sub.type] >= i_amount);

                if ((!cond->negate && !temp) || (cond->negate && temp))
                    return false;

                break;

            case kConditionCheckTypeKey:
                if (!p) return false;

                temp = ((p->cards_ & cond->sub.type) != 0);

                if ((!cond->negate && !temp) || (cond->negate && temp))
                    return false;

                break;

            case kConditionCheckTypeWeapon:
                if (!p) return false;

                temp = false;

                for (int i = 0; i < kMaximumWeapons; i++)
                {
                    if (p->weapons_[i].owned &&
                        p->weapons_[i].info == cond->sub.weap)
                    {
                        temp = true;
                        break;
                    }
                }

                if ((!cond->negate && !temp) || (cond->negate && temp))
                    return false;

                break;

            case kConditionCheckTypePowerup:
                if (!p) return false;

                if (cond->exact)
                    return (p->powers_[cond->sub.type] == cond->amount);

                temp = (p->powers_[cond->sub.type] > cond->amount);

                if ((!cond->negate && !temp) || (cond->negate && temp))
                    return false;

                break;

            case kConditionCheckTypeAmmo:
                if (!p) return false;

                if (cond->exact)
                    return (p->ammo_[cond->sub.type].count == i_amount);

                temp = (p->ammo_[cond->sub.type].count >= i_amount);

                if ((!cond->negate && !temp) || (cond->negate && temp))
                    return false;

                break;

            case kConditionCheckTypeInventory:
                if (!p) return false;

                if (cond->exact)
                    return (p->inventory_[cond->sub.type].count == i_amount);

                temp = (p->inventory_[cond->sub.type].count >= i_amount);

                if ((!cond->negate && !temp) || (cond->negate && temp))
                    return false;

                break;

            case kConditionCheckTypeCounter:
                if (!p) return false;

                if (cond->exact)
                    return (p->counters_[cond->sub.type].count == i_amount);

                temp = (p->counters_[cond->sub.type].count >= i_amount);

                if ((!cond->negate && !temp) || (cond->negate && temp))
                    return false;

                break;

            case kConditionCheckTypeJumping:
                if (!p) return false;

                temp = (p->jump_wait_ > 0);

                if ((!cond->negate && !temp) || (cond->negate && temp))
                    return false;

                break;

            case kConditionCheckTypeCrouching:
                if (!p) return false;

                temp = (mo->extended_flags_ & kExtendedFlagCrouching) ? true
                                                                      : false;

                if ((!cond->negate && !temp) || (cond->negate && temp))
                    return false;

                break;

            case kConditionCheckTypeSwimming:
                if (!p) return false;

                temp = p->swimming_;

                if ((!cond->negate && !temp) || (cond->negate && temp))
                    return false;

                break;

            case kConditionCheckTypeAttacking:
                if (!p) return false;

                temp =
                    (p->attack_button_down_[0] || p->attack_button_down_[1] ||
                     p->attack_button_down_[2] || p->attack_button_down_[3]);

                if ((!cond->negate && !temp) || (cond->negate && temp))
                    return false;

                break;

            case kConditionCheckTypeRampaging:
                if (!p) return false;

                temp = (p->attack_sustained_count_ >= 70);

                if ((!cond->negate && !temp) || (cond->negate && temp))
                    return false;

                break;

            case kConditionCheckTypeUsing:
                if (!p) return false;

                temp = p->use_button_down_;

                if ((!cond->negate && !temp) || (cond->negate && temp))
                    return false;

                break;

            case kConditionCheckTypeAction1:
                if (!p) return false;

                temp = p->action_button_down_[0];

                if ((!cond->negate && !temp) || (cond->negate && temp))
                    return false;

                break;

            case kConditionCheckTypeAction2:
                if (!p) return false;

                temp = p->action_button_down_[1];

                if ((!cond->negate && !temp) || (cond->negate && temp))
                    return false;

                break;

            case kConditionCheckTypeWalking:
                if (!p) return false;

                temp = (p->actual_speed_ > kPlayerStopSpeed) &&
                       (p->map_object_->z <= p->map_object_->floor_z_);

                if ((!cond->negate && !temp) || (cond->negate && temp))
                    return false;

                break;

            case kConditionCheckTypeNone:
            default:
                // unknown condition -- play it safe and succeed
                break;
        }
    }

    // all conditions succeeded
    return true;
}

void AddDeathmatchStart(const SpawnPoint &point)
{
    deathmatch_starts.push_back(point);
}

void AddHubStart(const SpawnPoint &point) { hub_starts.push_back(point); }

void AddCoopStart(const SpawnPoint &point) { coop_starts.push_back(point); }

void AddVoodooDoll(const SpawnPoint &point) { voodoo_dolls.push_back(point); }

SpawnPoint *FindCoopPlayer(int player_number_)
{
    for (int i = 0; i < (int)coop_starts.size(); i++)
    {
        SpawnPoint *point = &coop_starts[i];
        EPI_ASSERT(point->info);

        if (point->info->playernum_ == player_number_) return point;
    }

    return nullptr;  // not found
}

void MarkPlayerAvatars(void)
{
    for (int i = 0; i < kMaximumPlayers; i++)
    {
        Player *p = players[i];

        if (p && p->map_object_)
            p->map_object_->hyper_flags_ |= kHyperFlagRememberOldAvatars;
    }
}

void RemoveOldAvatars(void)
{
    MapObject *mo;
    MapObject *next;

    // first fix up any references
    for (mo = map_object_list_head; mo; mo = next)
    {
        next = mo->next_;

        // update any mobj_t pointer which referred to the old avatar
        // (the one which was saved in the savegame) to refer to the
        // new avatar (the one spawned after loading).

        if (mo->target_ &&
            (mo->target_->hyper_flags_ & kHyperFlagRememberOldAvatars))
        {
            EPI_ASSERT(mo->target_->player_);
            EPI_ASSERT(mo->target_->player_->map_object_);

            // LogDebug("Updating avatar reference: %p --> %p\n", mo->target_,
            // mo->target_->player_->map_object_);

            mo->SetTarget(mo->target_->player_->map_object_);
        }

        if (mo->source_ &&
            (mo->source_->hyper_flags_ & kHyperFlagRememberOldAvatars))
        {
            mo->SetSource(mo->source_->player_->map_object_);
        }

        if (mo->support_object_ &&
            (mo->support_object_->hyper_flags_ & kHyperFlagRememberOldAvatars))
        {
            mo->SetSupportObject(mo->support_object_->player_->map_object_);
        }

        // the other three fields don't matter (tracer, above_object_,
        // below_object_) because the will be nulled by the P_RemoveMobj()
        // below.
    }

    // now actually remove the old avatars
    for (mo = map_object_list_head; mo; mo = next)
    {
        next = mo->next_;

        if (mo->hyper_flags_ & kHyperFlagRememberOldAvatars)
        {
            LogDebug("Removing old avatar: %p\n", mo);

            RemoveMapObject(mo);
        }
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
