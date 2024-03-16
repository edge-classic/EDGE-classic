//----------------------------------------------------------------------------
//  EDGE Player Definition
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

#pragma once

#include <stdint.h>

#include "colormap.h"
#include "e_ticcmd.h"
#include "p_mobj.h"
#include "p_weapon.h"

constexpr uint8_t kBackupTics = 12;

constexpr uint8_t kPlayerNameCharacterLimit = 32;

constexpr uint16_t kMaximumEffectTime = (5 * kTicRate);

// The maximum number of players, multiplayer/networking.
constexpr uint8_t kMaximumPlayers = 16;

constexpr float kPlayerStopSpeed = 1.0f;

// Pointer to each player in the game.
extern class Player *players[kMaximumPlayers];
extern int           total_players;
extern int           total_bots;

// Player taking events, and displaying.
extern int console_player;
extern int display_player;

//
// Player states.
//
enum PlayerState
{
    // Playing or camping.
    kPlayerAlive,

    // Dead on the ground, view follows killer.
    kPlayerDead,

    // Waiting to be respawned in the level.
    kPlayerAwaitingRespawn
};

//
// Player flags
enum PlayerFlag
{
    kPlayerFlagNone = 0,

    kPlayerFlagConsole = 0x0001,
    kPlayerFlagDisplay = 0x0002,
    kPlayerFlagBot     = 0x0004,
    kPlayerFlagNetwork = 0x0008,

    // this not used in player_t, only in newgame_params_c
    kPlayerFlagNoPlayer = 0xFFFF
};

//
// Player internal flags, for cheats and debug.
//
enum CheatFlag
{
    // No clipping, walk through barriers.
    kCheatingNoClip = 1,

    // No damage, no health loss.
    kCheatingGodMode = 2,
};

// Consolidated struct for ammo/inventory/counters,
// basically a thing that has a finite quantity
// and some kind of upper limit to how many
// a player can have
struct PlayerStock
{
    int count;
    int maximum;
};

enum WeaponSelection
{
    // (for pending_wp only) no change is occuring
    KWeaponSelectionNoChange = -2,

    // absolutely no weapon at all
    KWeaponSelectionNone = -1
};

//
// Extended player object info: player_t
//
class Player
{
  public:
    // player number.  Starts at 0.
    int player_number_;

    // actions to perform.  Comes either from the local computer or over
    // the network in multiplayer mode.
    EventTicCommand command_;

    PlayerState player_state_;

    // miscellaneous flags
    int player_flags_;

    // map object that this player controls.  Will be nullptr outside of a
    // level (e.g. on the intermission screen).
    MapObject *map_object_;

    // player's name
    char player_name_[kPlayerNameCharacterLimit];

    // a measure of how fast we are actually moving, based on how far
    // the player thing moves on the 2D map.
    float actual_speed_;

    // Determine POV, including viewpoint bobbing during movement.
    // Focal origin above r.z
    // will be kFloatUnused until the first think.
    float view_z_;

    // Base height above floor for view_z.  Tracks `std_viewheight' but
    // is different when squatting (i.e. after a fall).
    float view_height_;

    // Bob/squat speed.
    float delta_view_height_;

    // standard viewheight, usually 75% of height.
    float standard_view_height_;

    // bounded/scaled total momentum.
    float bob_factor_;
    int   erraticism_bob_ticker_ = 0; // Erraticism bob timer to prevent weapon bob jumps

    // Kick offset for vertangle (in mobj_t)
    float kick_offset_;

    // when > 0, the player has activated zoom
    int zoom_field_of_view_;

    // This is only used between levels,
    // mo->health_ is used during levels.
    float health_;

    // Armour points for each type
    float                      armours_[kTotalArmourTypes];
    const MapObjectDefinition *armour_types_[kTotalArmourTypes];
    float                      total_armour_; // needed for status bar

    // Power ups. invinc and invis are tic counters.
    float powers_[kTotalPowerTypes];

    // bitflag of powerups to be kept (esp. BERSERK)
    int keep_powers_;

    // Set of keys held
    DoorKeyType cards_;

    // weapons, either an index into the player->weapons_[] array, or one
    // of the KWeaponSelection* values.
    WeaponSelection ready_weapon_;
    WeaponSelection pending_weapon_;

    // -AJA- 1999/08/11: Now uses playerweapon_t.
    PlayerWeapon weapons_[kMaximumWeapons];

    // current weapon choice for each key (1..9 and 0)
    WeaponSelection key_choices_[10];

    // for status bar: which numbers to light up
    int available_weapons_[10];

    // ammunition, one for each AmmunitionType (except AM_NoAmmo)
    PlayerStock ammo_[kTotalAmmunitionTypes];

    // inventory stock, one for each InventoryType
    PlayerStock inventory_[kTotalInventoryTypes];

    // counters, one for each CounterType
    PlayerStock counters_[kTotalCounterTypes];

    // True if button down last tic.
    bool attack_button_down_[4];
    bool use_button_down_;
    bool action_button_down_[2];

    // Bit flags, for cheats and debug.
    // See cheat_t, above.
    int cheats_;

    // Refired shots are less accurate.
    int refire_;

    // Frags, kills of other players.
    int frags_;
    int total_frags_;

    // For intermission stats.
    int kill_count_;
    int item_count_;
    int secret_count_;
    int level_time_;

    // For screen flashing (red or bright).
    int damage_count_;
    int bonus_count_;

    // Who did damage (nullptr for floors/ceilings).
    MapObject *attacker_;

    // how much damage was done (used for status bar)
    float damage_pain_;

    // damage flash colour of last damage type inflicted
    RGBAColor last_damage_colour_;

    // So gun flashes light up the screen.
    int  extra_light_;
    bool flash_;

    // -AJA- 1999/07/10: changed for colmap.ddf.
    const Colormap *effect_colourmap_;
    int             effect_left_; // tics remaining, maxed to kMaximumEffectTime

    // Overlay view sprites (gun, etc).
    PlayerSprite player_sprites_[kTotalPlayerSpriteTypes];

    // Current PSP for action
    int action_player_sprite_;

    // Implements a wait counter to prevent use jumping again
    // -ACB- 1998/08/09
    int jump_wait_;

    // counter used to determine when to enter weapon idle states
    int idle_wait_;

    int splash_wait_;

    // breathing support.  In air-less sectors, this is decremented on
    // each tic.  When it reaches zero, the player starts choking (which
    // hurts), and player drowns when health drops to zero.
    int  air_in_lungs_;
    bool underwater_;
    bool airless_;
    bool swimming_;
    bool wet_feet_;

    // how many tics to grin :-)
    int grin_count_;

    // how many tics player has been attacking (for rampage face)
    int attack_sustained_count_;

    // status bar: used to choose which face to show
    int face_index_;
    int face_count_;

    // -AJA- 1999/08/10: This field is the state number which is
    // remembered for WEAPON_NOFIRE_RETURN when the player lets go of
    // the button.  Holds -1 if not fired or after changing weapons.
    int remember_attack_state_[4];

    // last frame for weapon models
    int weapon_last_frame_;

    EventTicCommand input_commands_[kBackupTics];

    int in_tic_; /* tic number of next input command expected */

    // This function will be called to initialise the ticcmd_t.
    void (*Builder)(const class Player *, void *data, EventTicCommand *dest);
    void *build_data_;

  public:
    void Reborn();

    bool IsBot() const
    {
        return (player_flags_ & kPlayerFlagBot) != 0;
    }
};

// Player ticcmd builders
void ConsolePlayerBuilder(const Player *p, void *data, EventTicCommand *dest);
void BotPlayerBuilder(const Player *p, void *data, EventTicCommand *dest);

void ClearBodyQueue(void);
void DeathMatchSpawnPlayer(Player *p);
void CoopSpawnPlayer(Player *p);
void GameHubSpawnPlayer(Player *p, int tag);
void SpawnVoodooDolls(Player *p);
void SpawnHelper(int pnum);

void SetConsolePlayer(int pnum);
void SetDisplayPlayer(int pnum);
void ToggleDisplayPlayer(void);

void PlayerFinishLevel(Player *p, bool keep_cards);
void MarkPlayerAvatars(void);
void RemoveOldAvatars(void);

bool GameCheckConditions(MapObject *mo, ConditionCheck *cond);

void ClearPlayerStarts(void);

void AddDeathmatchStart(const SpawnPoint &point);
void AddCoopStart(const SpawnPoint &point);
void AddHubStart(const SpawnPoint &point);
void AddVoodooDoll(const SpawnPoint &point);

SpawnPoint *FindCoopPlayer(int pnum);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
