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
player_t *players[MAXPLAYERS];
int       numplayers;
int       numbots;

int consoleplayer = -1;  // player taking events
int displayplayer = -1;  // view being displayed

#define MAX_BODIES 50

static MapObject *bodyqueue[MAX_BODIES];
static int        bodyqueue_size = 0;

// Maintain single and multi player starting spots.
static std::vector<SpawnPoint> dm_starts;
static std::vector<SpawnPoint> coop_starts;
static std::vector<SpawnPoint> voodoo_dolls;
static std::vector<SpawnPoint> hub_starts;

static void P_SpawnPlayer(player_t *p, const SpawnPoint *point, bool is_hub);

void GameClearPlayerStarts(void)
{
    dm_starts.clear();
    coop_starts.clear();
    voodoo_dolls.clear();
    hub_starts.clear();
}

void GameClearBodyQueue(void)
{
    memset(bodyqueue, 0, sizeof(bodyqueue));

    bodyqueue_size = 0;
}

void GameAddBodyToQueue(MapObject *mo)
{
    // flush an old corpse if needed
    if (bodyqueue_size >= MAX_BODIES)
    {
        MapObject *rotten = bodyqueue[bodyqueue_size % MAX_BODIES];
        rotten->reference_count_--;

        P_RemoveMobj(rotten);
    }

    // prevent accidental re-use
    mo->reference_count_++;

    bodyqueue[bodyqueue_size % MAX_BODIES] = mo;
    bodyqueue_size++;
}

//
// Called when a player completes a level.
// For HUB changes, we keep powerups and keycards
//
void GamePlayerFinishLevel(player_t *p, bool keep_cards)
{
    if (!keep_cards)
    {
        for (int i = 0; i < kTotalPowerTypes; i++) p->powers[i] = 0;

        p->keep_powers = 0;

        p->cards = kDoorKeyNone;

        p->mo->flags_ &= ~kMapObjectFlagFuzzy;  // cancel invisibility
    }

    p->extralight = 0;  // cancel gun flashes

    // cancel colourmap effects
    p->effect_colourmap = nullptr;

    // no palette changes
    p->damagecount        = 0;
    p->damage_pain        = 0;
    p->bonuscount         = 0;
    p->grin_count         = 0;
    p->last_damage_colour = SG_RED_RGBA32;

    // Lobo 2023: uncomment if still getting
    //  "INTERNAL ERROR: player has a removed attacker"
    p->attacker = nullptr;

    if (LuaUseLuaHud())
        LuaEndLevel();
    else
        CoalEndLevel();
}

//
// player_s::Reborn
//
// Called after a player dies.
// Almost everything is cleared and initialised.
//
void player_s::Reborn()
{
    LogDebug("player_s::Reborn\n");

    playerstate = PST_LIVE;

    mo     = nullptr;
    health = 0;

    memset(armours, 0, sizeof(armours));
    memset(armour_types, 0, sizeof(armour_types));
    memset(powers, 0, sizeof(powers));

    keep_powers = 0;
    totalarmour = 0;
    cards       = kDoorKeyNone;

    ready_wp   = WPSEL_None;
    pending_wp = WPSEL_NoChange;

    memset(weapons, 0, sizeof(weapons));
    memset(avail_weapons, 0, sizeof(avail_weapons));
    memset(ammo, 0, sizeof(ammo));
    memset(inventory, 0, sizeof(inventory));
    memset(counters, 0, sizeof(counters));

    for (int w = 0; w <= 9; w++) key_choices[w] = WPSEL_None;

    cheats             = 0;
    refire             = 0;
    bob                = 0;
    kick_offset        = 0;
    zoom_fov           = 0;
    bonuscount         = 0;
    damagecount        = 0;
    damage_pain        = 0;
    extralight         = 0;
    flash              = false;
    last_damage_colour = SG_RED_RGBA32;

    attacker = nullptr;

    effect_colourmap = nullptr;
    effect_left      = 0;

    memset(psprites, 0, sizeof(psprites));

    jumpwait     = 0;
    idlewait     = 0;
    splashwait   = 0;
    air_in_lungs = 0;
    underwater   = false;
    airless      = false;
    swimming     = false;
    wet_feet     = false;

    grin_count       = 0;
    attackdown_count = 0;
    face_index       = 0;
    face_count       = 0;

    remember_atk[0]   = -1;
    remember_atk[1]   = -1;
    remember_atk[2]   = -1;
    remember_atk[3]   = -1;
    weapon_last_frame = -1;
}

//
// Returns false if the player cannot be respawned at the given spot
// because something is occupying it.
//
static bool GameCheckSpot(player_t *player, const SpawnPoint *point)
{
    float x = point->x;
    float y = point->y;
    float z = point->z;

    if (!player->mo)
    {
        // first spawn of level, before corpses
        for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
        {
            player_t *p = players[pnum];

            if (!p || !p->mo || p == player) continue;

            if (fabs(p->mo->x - x) < 8.0f && fabs(p->mo->y - y) < 8.0f)
                return false;
        }

        P_SpawnPlayer(player, point, false);
        return true;  // OK
    }

    if (!CheckAbsolutePosition(player->mo, x, y, z)) return false;

    GameAddBodyToQueue(player->mo);

    // spawn a teleport fog
    // (temp fix for teleport effect)
    x += 20 * epi::BAMCos(point->angle);
    y += 20 * epi::BAMSin(point->angle);
    P_MobjCreateObject(x, y, z, mobjtypes.Lookup("TELEPORT_FLASH"));

    P_SpawnPlayer(player, point, false);
    return true;  // OK
}

//
// GameSetConsolePlayer
//
// Note: we don't rely on current value being valid, hence can use
//       these functions during initialisation.
//
void GameSetConsolePlayer(int pnum)
{
    consoleplayer = pnum;

    SYS_ASSERT(players[consoleplayer]);

    for (int i = 0; i < MAXPLAYERS; i++)
        if (players[i]) players[i]->playerflags &= ~PFL_Console;

    players[pnum]->playerflags |= PFL_Console;

    if (ArgumentFind("testbot") > 0) { P_BotCreate(players[pnum], false); }
    else
    {
        players[pnum]->builder    = P_ConsolePlayerBuilder;
        players[pnum]->build_data = nullptr;
    }
}

//
// GameSetDisplayPlayer
//
void GameSetDisplayPlayer(int pnum)
{
    displayplayer = pnum;

    SYS_ASSERT(players[displayplayer]);

    for (int i = 0; i < MAXPLAYERS; i++)
        if (players[i]) players[i]->playerflags &= ~PFL_Display;

    players[pnum]->playerflags |= PFL_Display;
}

void GameToggleDisplayPlayer(void)
{
    for (int i = 1; i <= MAXPLAYERS; i++)
    {
        int pnum = (displayplayer + i) % MAXPLAYERS;

        if (players[pnum])
        {
            GameSetDisplayPlayer(pnum);
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
static void P_SpawnPlayer(player_t *p, const SpawnPoint *point, bool is_hub)
{
    // -KM- 1998/11/25 This is in preparation for skins.  The creatures.ddf
    //   will hold player start objects, sprite will be taken for skin.
    // -AJA- 2004/04/14: Use DDF entry from level thing.

    if (point->info == nullptr) FatalError("P_SpawnPlayer: No such item type!");

    const MapObjectDefinition *info = point->info;

    LogDebug("* P_SpawnPlayer %d @ %1.0f,%1.0f\n", point->info->playernum_,
             point->x, point->y);

    if (info->playernum_ <= 0) info = mobjtypes.LookupPlayer(p->pnum + 1);

    if (p->playerstate == PST_REBORN)
    {
        p->Reborn();

        P_GiveInitialBenefits(p, info);
    }

    MapObject *mobj = P_MobjCreateObject(point->x, point->y, point->z, info);

    mobj->angle_          = point->angle;
    mobj->vertical_angle_ = point->vertical_angle;
    mobj->player_         = p;
    mobj->health_         = p->health;

    p->mo               = mobj;
    p->playerstate      = PST_LIVE;
    p->refire           = 0;
    p->damagecount      = 0;
    p->damage_pain      = 0;
    p->bonuscount       = 0;
    p->extralight       = 0;
    p->effect_colourmap = nullptr;
    p->std_viewheight   = mobj->height_ * info->viewheight_;
    p->viewheight       = p->std_viewheight;
    p->zoom_fov         = 0;
    p->jumpwait         = 0;

    // don't do anything immediately
    p->attackdown[0] = p->attackdown[1] = false;
    p->usedown                          = false;
    p->actiondown[0] = p->actiondown[1] = false;

    // setup gun psprite
    if (!is_hub || !SP_MATCH()) SetupPlayerSprites(p);

    // give all cards in death match mode
    if (DEATHMATCH()) p->cards = kDoorKeyBitmask;

    // -AJA- in COOP, all players are on the same side
    if (COOP_MATCH()) mobj->side_ = ~0;

    // Don't get stuck spawned in things: telefrag them.

    /* Dasho 2023.10.09 - Ran into a map where having the player stuck inside
    a thing next to it with a sufficiently large radius was an intentional
    mechanic (The All-Ghosts Forest). Telefragging in this scenario seems
    to diverge from reasonably 'correct' behavior compared to ports with good
    vanilla/Boom compat, so I'm commenting this out. I had to do this previously
    for voodoo dolls because it would break certain maps. */

    // TeleportMove(mobj, mobj->x, mobj->y, mobj->z);

    if (COOP_MATCH() && !level_flags.team_damage)
        mobj->hyper_flags_ |= kHyperFlagFriendlyFireImmune;

    if (p->isBot())
    {
        DeathBot *bot = (DeathBot *)p->build_data;
        SYS_ASSERT(bot);

        bot->Respawn();
    }
}

static void P_SpawnVoodooDoll(player_t *p, const SpawnPoint *point)
{
    const MapObjectDefinition *info = point->info;

    SYS_ASSERT(info);
    SYS_ASSERT(info->playernum_ > 0);

    LogDebug("* P_SpawnVoodooDoll %d @ %1.0f,%1.0f\n", p->pnum + 1, point->x,
             point->y);

    MapObject *mobj = P_MobjCreateObject(point->x, point->y, point->z, info);

    mobj->angle_          = point->angle;
    mobj->vertical_angle_ = point->vertical_angle;
    mobj->player_         = p;
    mobj->health_         = p->health;

    mobj->is_voodoo_ = true;

    if (COOP_MATCH()) mobj->side_ = ~0;
}

//
// GameDeathMatchSpawnPlayer
//
// Spawns a player at one of the random deathmatch spots.
// Called at level load and each death.
//
void GameDeathMatchSpawnPlayer(player_t *p)
{
    if (p->pnum >= (int)dm_starts.size())
        LogWarning("Few deathmatch spots, %d recommended.\n", p->pnum + 1);

    int begin = RandomByteDeterministic();

    if (dm_starts.size() > 0)
    {
        for (int j = 0; j < (int)dm_starts.size(); j++)
        {
            int i = (begin + j) % (int)dm_starts.size();

            if (GameCheckSpot(p, &dm_starts[i])) return;
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
void GameCoopSpawnPlayer(player_t *p)
{
    SpawnPoint *point = GameFindCoopPlayer(p->pnum + 1);

    if (point && GameCheckSpot(p, point)) return;

    LogWarning("Player %d start is invalid.\n", p->pnum + 1);

    int begin = p->pnum;

    // try to spawn at one of the other players spots
    for (int j = 0; j < (int)coop_starts.size(); j++)
    {
        int i = (begin + j) % (int)coop_starts.size();

        if (GameCheckSpot(p, &coop_starts[i])) return;
    }

    FatalError("No usable player start found!\n");
}

static SpawnPoint *GameFindHubPlayer(int pnum, int tag)
{
    int count = 0;

    for (int i = 0; i < (int)hub_starts.size(); i++)
    {
        SpawnPoint *point = &hub_starts[i];
        SYS_ASSERT(point->info);

        if (point->tag != tag) continue;

        count++;

        if (point->info->playernum_ == pnum) return point;
    }

    if (count == 0)
        FatalError("Missing hub starts with tag %d\n", tag);
    else
        FatalError("No usable hub start for player %d (tag %d)\n", pnum + 1,
                   tag);

    return nullptr; /* NOT REACHED */
}

void GameHubSpawnPlayer(player_t *p, int tag)
{
    SYS_ASSERT(!p->mo);

    SpawnPoint *point = GameFindHubPlayer(p->pnum + 1, tag);

    // assume player will fit (too bad otherwise)
    P_SpawnPlayer(p, point, true);
}

void GameSpawnVoodooDolls(player_t *p)
{
    for (int i = 0; i < (int)voodoo_dolls.size(); i++)
    {
        SpawnPoint *point = &voodoo_dolls[i];

        if (point->info->playernum_ != p->pnum + 1) continue;

        P_SpawnVoodooDoll(p, point);
    }
}

// number of wanted dogs (1-3)
EDGE_DEFINE_CONSOLE_VARIABLE(dogs, "0", kConsoleVariableFlagArchive)

void GameSpawnHelper(int pnum)
{
    if (pnum == 0) return;

    if (pnum > dogs.d_) return;

    SpawnPoint *point = GameFindCoopPlayer(pnum + 1);
    if (point == nullptr) return;

    const MapObjectDefinition *info = mobjtypes.Lookup(888);
    if (info == nullptr) return;

    MapObject *mo = P_MobjCreateObject(point->x, point->y, point->z, info);

    mo->angle_      = point->angle;
    mo->spawnpoint_ = *point;

    mo->side_ = ~0;
}

bool GameCheckConditions(MapObject *mo, ConditionCheck *cond)
{
    player_t *p = mo->player_;
    bool      temp;

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
                        return (p->totalarmour == i_amount);
                    else
                        return (p->armours[cond->sub.type] == i_amount);
                }

                if (cond->sub.type == kTotalArmourTypes)
                    temp = (p->totalarmour >= i_amount);
                else
                    temp = (p->armours[cond->sub.type] >= i_amount);

                if ((!cond->negate && !temp) || (cond->negate && temp))
                    return false;

                break;

            case kConditionCheckTypeKey:
                if (!p) return false;

                temp = ((p->cards & cond->sub.type) != 0);

                if ((!cond->negate && !temp) || (cond->negate && temp))
                    return false;

                break;

            case kConditionCheckTypeWeapon:
                if (!p) return false;

                temp = false;

                for (int i = 0; i < kMaximumWeapons; i++)
                {
                    if (p->weapons[i].owned &&
                        p->weapons[i].info == cond->sub.weap)
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
                    return (p->powers[cond->sub.type] == cond->amount);

                temp = (p->powers[cond->sub.type] > cond->amount);

                if ((!cond->negate && !temp) || (cond->negate && temp))
                    return false;

                break;

            case kConditionCheckTypeAmmo:
                if (!p) return false;

                if (cond->exact)
                    return (p->ammo[cond->sub.type].num == i_amount);

                temp = (p->ammo[cond->sub.type].num >= i_amount);

                if ((!cond->negate && !temp) || (cond->negate && temp))
                    return false;

                break;

            case kConditionCheckTypeInventory:
                if (!p) return false;

                if (cond->exact)
                    return (p->inventory[cond->sub.type].num == i_amount);

                temp = (p->inventory[cond->sub.type].num >= i_amount);

                if ((!cond->negate && !temp) || (cond->negate && temp))
                    return false;

                break;

            case kConditionCheckTypeCounter:
                if (!p) return false;

                if (cond->exact)
                    return (p->counters[cond->sub.type].num == i_amount);

                temp = (p->counters[cond->sub.type].num >= i_amount);

                if ((!cond->negate && !temp) || (cond->negate && temp))
                    return false;

                break;

            case kConditionCheckTypeJumping:
                if (!p) return false;

                temp = (p->jumpwait > 0);

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

                temp = p->swimming;

                if ((!cond->negate && !temp) || (cond->negate && temp))
                    return false;

                break;

            case kConditionCheckTypeAttacking:
                if (!p) return false;

                temp = p->attackdown[0] || p->attackdown[1];

                if ((!cond->negate && !temp) || (cond->negate && temp))
                    return false;

                break;

            case kConditionCheckTypeRampaging:
                if (!p) return false;

                temp = (p->attackdown_count >= 70);

                if ((!cond->negate && !temp) || (cond->negate && temp))
                    return false;

                break;

            case kConditionCheckTypeUsing:
                if (!p) return false;

                temp = p->usedown;

                if ((!cond->negate && !temp) || (cond->negate && temp))
                    return false;

                break;

            case kConditionCheckTypeAction1:
                if (!p) return false;

                temp = p->actiondown[0];

                if ((!cond->negate && !temp) || (cond->negate && temp))
                    return false;

                break;

            case kConditionCheckTypeAction2:
                if (!p) return false;

                temp = p->actiondown[1];

                if ((!cond->negate && !temp) || (cond->negate && temp))
                    return false;

                break;

            case kConditionCheckTypeWalking:
                if (!p) return false;

                temp = (p->actual_speed > PLAYER_kStopSpeed) &&
                       (p->mo->z <= p->mo->floor_z_);

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

void GameAddDeathmatchStart(const SpawnPoint &point)
{
    dm_starts.push_back(point);
}

void GameAddHubStart(const SpawnPoint &point) { hub_starts.push_back(point); }

void GameAddCoopStart(const SpawnPoint &point)
{
    coop_starts.push_back(point);
}

void GameAddVoodooDoll(const SpawnPoint &point)
{
    voodoo_dolls.push_back(point);
}

SpawnPoint *GameFindCoopPlayer(int pnum)
{
    for (int i = 0; i < (int)coop_starts.size(); i++)
    {
        SpawnPoint *point = &coop_starts[i];
        SYS_ASSERT(point->info);

        if (point->info->playernum_ == pnum) return point;
    }

    return nullptr;  // not found
}

void GameMarkPlayerAvatars(void)
{
    for (int i = 0; i < MAXPLAYERS; i++)
    {
        player_t *p = players[i];

        if (p && p->mo) p->mo->hyper_flags_ |= kHyperFlagRememberOldAvatars;
    }
}

void GameRemoveOldAvatars(void)
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
            SYS_ASSERT(mo->target_->player_);
            SYS_ASSERT(mo->target_->player_->mo);

            // LogDebug("Updating avatar reference: %p --> %p\n", mo->target_,
            // mo->target_->player_->mo);

            mo->SetTarget(mo->target_->player_->mo);
        }

        if (mo->source_ &&
            (mo->source_->hyper_flags_ & kHyperFlagRememberOldAvatars))
        {
            mo->SetSource(mo->source_->player_->mo);
        }

        if (mo->support_object_ &&
            (mo->support_object_->hyper_flags_ & kHyperFlagRememberOldAvatars))
        {
            mo->SetSupportObject(mo->support_object_->player_->mo);
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

            P_RemoveMobj(mo);
        }
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
