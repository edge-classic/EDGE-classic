//----------------------------------------------------------------------------
//  EDGE New SaveGame Handling (Players)
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
// See the file "docs/save_sys.txt" for a complete description of the
// new savegame system.
//
// This file handles:
//   player_t        [PLAY]
//   playerweapon_t  [WEAP]
//   playerammo_t    [AMMO]
//   playerinv_t     [INVY]
//   playercounter_t [CNTR]
//   psprite_t       [PSPR]
//

#include "bot_think.h"
#include "epi.h"
#include "main.h"
#include "str_util.h"
#include "sv_chunk.h"
#include "sv_main.h"

// forward decls.
int   SaveGamePlayerCountElems(void);
int   SaveGamePlayerGetIndex(Player *elem);
void *SaveGamePlayerFindByIndex(int index);
void  SaveGamePlayerCreateElems(int num_elems);
void  SaveGamePlayerFinaliseElems(void);

bool SR_PlayerGetCounter(void *storage, int index, void *extra);
bool SR_PlayerGetInv(void *storage, int index, void *extra);
bool SR_PlayerGetAmmo(void *storage, int index, void *extra);
bool SR_PlayerGetWeapon(void *storage, int index, void *extra);
bool SR_PlayerGetPSprite(void *storage, int index, void *extra);
bool SR_PlayerGetName(void *storage, int index, void *extra);
bool SR_PlayerGetState(void *storage, int index, void *extra);
bool SR_WeaponGetInfo(void *storage, int index, void *extra);

void SR_PlayerPutCounter(void *storage, int index, void *extra);
void SR_PlayerPutInv(void *storage, int index, void *extra);
void SR_PlayerPutAmmo(void *storage, int index, void *extra);
void SR_PlayerPutWeapon(void *storage, int index, void *extra);
void SR_PlayerPutPSprite(void *storage, int index, void *extra);
void SR_PlayerPutName(void *storage, int index, void *extra);
void SR_PlayerPutState(void *storage, int index, void *extra);
void SR_WeaponPutInfo(void *storage, int index, void *extra);

extern bool SaveGameMapObjectGetType(void *storage, int index, void *extra);
extern void SaveGameMapObjectPutType(void *storage, int index, void *extra);

//----------------------------------------------------------------------------
//
//  PLAYER STRUCTURE AND ARRAY
//
static Player dummy_player;

static SaveField sv_fields_player[] = {
    EDGE_SAVE_FIELD(dummy_player, player_number_, "pnum", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_player, player_state_, "playerstate", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_player, player_flags_, "playerflags", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_player, player_name_[0], "playername", 1, kSaveFieldString, 0, nullptr, SR_PlayerGetName,
                    SR_PlayerPutName),
    EDGE_SAVE_FIELD(dummy_player, map_object_, "mo", 1, kSaveFieldIndex, 4, "mobjs", SaveGameGetMapObject,
                    SaveGamePutMapObject),
    EDGE_SAVE_FIELD(dummy_player, view_z_, "viewz", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_player, view_height_, "viewheight", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_player, delta_view_height_, "deltaviewheight", 1, kSaveFieldNumeric, 4, nullptr,
                    SaveGameGetFloat, SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_player, standard_view_height_, "std_viewheight", 1, kSaveFieldNumeric, 4, nullptr,
                    SaveGameGetFloat, SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_player, zoom_field_of_view_, "zoom_fov", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_player, actual_speed_, "actual_speed", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_player, health_, "health", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_player, armours_[0], "armours", kTotalArmourTypes, kSaveFieldNumeric, 4, nullptr,
                    SaveGameGetFloat, SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_player, armour_types_[0], "armour_types", kTotalArmourTypes, kSaveFieldString, 0, nullptr,
                    SaveGameMapObjectGetType, SaveGameMapObjectPutType),
    EDGE_SAVE_FIELD(dummy_player, powers_[0], "powers", kTotalPowerTypes, kSaveFieldNumeric, 4, nullptr,
                    SaveGameGetFloat, SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_player, keep_powers_, "keep_powers", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_player, cards_, "cards_ke", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_player, frags_, "frags", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_player, total_frags_, "totalfrags", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_player, ready_weapon_, "ready_wp", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_player, pending_weapon_, "pending_wp", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_player, weapons_[0], "weapons", kMaximumWeapons, kSaveFieldStruct, 0, "playerweapon_t",
                    SR_PlayerGetWeapon, SR_PlayerPutWeapon),
    EDGE_SAVE_FIELD(dummy_player, ammo_[0], "ammo", kTotalAmmunitionTypes, kSaveFieldStruct, 0, "playerammo_t",
                    SR_PlayerGetAmmo, SR_PlayerPutAmmo),
    EDGE_SAVE_FIELD(dummy_player, inventory_[0], "inventory", kTotalInventoryTypes, kSaveFieldStruct, 0, "playerinv_t",
                    SR_PlayerGetInv, SR_PlayerPutInv),
    EDGE_SAVE_FIELD(dummy_player, counters_[0], "counters", kTotalCounterTypes, kSaveFieldStruct, 0, "playercounter_t",
                    SR_PlayerGetCounter, SR_PlayerPutCounter),
    EDGE_SAVE_FIELD(dummy_player, cheats_, "cheats", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_player, refire_, "refire", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_player, kill_count_, "killcount", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_player, item_count_, "itemcount", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_player, secret_count_, "secretcount", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_player, jump_wait_, "jumpwait", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_player, idle_wait_, "idlewait", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_player, air_in_lungs_, "air_in_lungs", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_player, underwater_, "underwater", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetBoolean,
                    SaveGamePutBoolean),
    EDGE_SAVE_FIELD(dummy_player, airless_, "airless", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetBoolean,
                    SaveGamePutBoolean),
    EDGE_SAVE_FIELD(dummy_player, flash_, "flash_b", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetBoolean,
                    SaveGamePutBoolean),
    EDGE_SAVE_FIELD(dummy_player, player_sprites_[0], "psprites", kTotalPlayerSpriteTypes, kSaveFieldStruct, 0,
                    "psprite_t", SR_PlayerGetPSprite, SR_PlayerPutPSprite),

    // FIXME: swimming & wet_feet ???

    // NOT HERE:
    //   in_game: only in-game players are saved.
    //   key_choices: depends on DDF too much, and not important.
    //   remember_atk[]: ditto.
    //   next,prev: links are regenerated.
    //   avail_weapons, totalarmour: regenerated.
    //   attacker: not very important

    {0, nullptr, 0, {kSaveFieldInvalid, 0, nullptr}, nullptr, nullptr, nullptr}};

SaveStruct sv_struct_player = {
    nullptr,                     // link in list
    "player_t",                  // structure name
    "play",                      // start marker
    sv_fields_player,            // field descriptions
    (const char *)&dummy_player, // dummy base
    true,                        // define_me
    nullptr                      // pointer to known struct
};

SaveArray sv_array_player = {
    nullptr,                     // link in list
    "players",                   // array name
    &sv_struct_player,           // array type
    true,                        // define_me
    false,                       // allow_hub

    SaveGamePlayerCountElems,    // count routine
    SaveGamePlayerFindByIndex,   // index routine
    SaveGamePlayerCreateElems,   // creation routine
    SaveGamePlayerFinaliseElems, // finalisation routine

    nullptr,                     // pointer to known array
    0                            // loaded size
};

//----------------------------------------------------------------------------
//
//  WEAPON STRUCTURE
//
static PlayerWeapon dummy_weapon;

static SaveField sv_fields_playerweapon[] = {
    EDGE_SAVE_FIELD(dummy_weapon, info, "info", 1, kSaveFieldString, 0, nullptr, SR_WeaponGetInfo, SR_WeaponPutInfo),
    EDGE_SAVE_FIELD(dummy_weapon, owned, "owned", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetBoolean,
                    SaveGamePutBoolean),
    EDGE_SAVE_FIELD(dummy_weapon, flags, "flags", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_weapon, clip_size[0], "clip_size", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_weapon, clip_size[1], "sa_clip_size", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_weapon, clip_size[2], "ta_clip_size", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_weapon, clip_size[3], "fa_clip_size", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_weapon, model_skin, "model_skin", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),

    {0, nullptr, 0, {kSaveFieldInvalid, 0, nullptr}, nullptr, nullptr, nullptr}};

SaveStruct sv_struct_playerweapon = {
    nullptr,                     // link in list
    "playerweapon_t",            // structure name
    "weap",                      // start marker
    sv_fields_playerweapon,      // field descriptions
    (const char *)&dummy_weapon, // dummy base
    true,                        // define_me
    nullptr                      // pointer to known struct
};

static PlayerStock dummy_stock;  // this works for the following 3 (counter/inv/ammo)

//----------------------------------------------------------------------------
//
//  COUNTER STRUCTURE
//

static SaveField sv_fields_playercounter[] = {
    EDGE_SAVE_FIELD(dummy_stock, count, "num", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_stock, maximum, "max", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),

    {0, nullptr, 0, {kSaveFieldInvalid, 0, nullptr}, nullptr, nullptr, nullptr}};

SaveStruct sv_struct_playercounter = {
    nullptr,                    // link in list
    "playercounter_t",          // structure name
    "cntr",                     // start marker
    sv_fields_playercounter,    // field descriptions
    (const char *)&dummy_stock, // dummy base
    true,                       // define_me
    nullptr                     // pointer to known struct
};

//----------------------------------------------------------------------------
//
//  INVENTORY STRUCTURE
//

static SaveField sv_fields_playerinv[] = {EDGE_SAVE_FIELD(dummy_stock, count, "num", 1, kSaveFieldNumeric, 4, nullptr,
                                                          SaveGameGetInteger, SaveGamePutInteger),
                                          EDGE_SAVE_FIELD(dummy_stock, maximum, "max", 1, kSaveFieldNumeric, 4, nullptr,
                                                          SaveGameGetInteger, SaveGamePutInteger),

                                          {0, nullptr, 0, {kSaveFieldInvalid, 0, nullptr}, nullptr, nullptr, nullptr}};

SaveStruct sv_struct_playerinv = {
    nullptr,                    // link in list
    "playerinv_t",              // structure name
    "invy",                     // start marker
    sv_fields_playerinv,        // field descriptions
    (const char *)&dummy_stock, // dummy base
    true,                       // define_me
    nullptr                     // pointer to known struct
};

//----------------------------------------------------------------------------
//
//  AMMO STRUCTURE
//

static SaveField sv_fields_playerammo[] = {EDGE_SAVE_FIELD(dummy_stock, count, "num", 1, kSaveFieldNumeric, 4, nullptr,
                                                           SaveGameGetInteger, SaveGamePutInteger),
                                           EDGE_SAVE_FIELD(dummy_stock, maximum, "max", 1, kSaveFieldNumeric, 4,
                                                           nullptr, SaveGameGetInteger, SaveGamePutInteger),

                                           {0, nullptr, 0, {kSaveFieldInvalid, 0, nullptr}, nullptr, nullptr, nullptr}};

SaveStruct sv_struct_playerammo = {
    nullptr,                    // link in list
    "playerammo_t",             // structure name
    "ammo",                     // start marker
    sv_fields_playerammo,       // field descriptions
    (const char *)&dummy_stock, // dummy base
    true,                       // define_me
    nullptr                     // pointer to known struct
};

//----------------------------------------------------------------------------
//
//  PSPRITE STRUCTURE
//
static PlayerSprite dummy_player_sprite;

static SaveField sv_fields_psprite[] = {
    EDGE_SAVE_FIELD(dummy_player_sprite, state, "state", 1, kSaveFieldString, 0, nullptr, SR_PlayerGetState,
                    SR_PlayerPutState),
    EDGE_SAVE_FIELD(dummy_player_sprite, next_state, "next_state", 1, kSaveFieldString, 0, nullptr, SR_PlayerGetState,
                    SR_PlayerPutState),
    EDGE_SAVE_FIELD(dummy_player_sprite, tics, "tics", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetInteger,
                    SaveGamePutInteger),
    EDGE_SAVE_FIELD(dummy_player_sprite, visibility, "visibility", 1, kSaveFieldNumeric, 4, nullptr, SaveGameGetFloat,
                    SaveGamePutFloat),
    EDGE_SAVE_FIELD(dummy_player_sprite, target_visibility, "vis_target", 1, kSaveFieldNumeric, 4, nullptr,
                    SaveGameGetFloat, SaveGamePutFloat),

    // NOT HERE:
    //   sx, sy: they can be regenerated.

    {0, nullptr, 0, {kSaveFieldInvalid, 0, nullptr}, nullptr, nullptr, nullptr}};

SaveStruct sv_struct_psprite = {
    nullptr,                            // link in list
    "pspdef_t",                         // structure name
    "pspr",                             // start marker
    sv_fields_psprite,                  // field descriptions
    (const char *)&dummy_player_sprite, // dummy base
    true,                               // define_me
    nullptr                             // pointer to known struct
};

//----------------------------------------------------------------------------

int SaveGamePlayerCountElems(void)
{
    int count = 0;

    for (int pnum = 0; pnum < kMaximumPlayers; pnum++)
    {
        Player *p = players[pnum];
        if (!p)
            continue;

        count++;
    }

    EPI_ASSERT(count > 0);

    return count;
}

void *SaveGamePlayerFindByIndex(int index)
{
    if (index >= total_players)
        FatalError("LOADGAME: Invalid player index: %d\n", index);

    for (int pnum = 0; pnum < kMaximumPlayers; pnum++)
    {
        Player *p = players[pnum];
        if (!p)
            continue;

        if (index == 0)
            return p;

        index--;
    }

    FatalError("Internal error in SaveGamePlayerFindByIndex: index not found.\n");
    return nullptr;
}

int SaveGamePlayerGetIndex(Player *elem)
{
    int index = 0;

    for (int pnum = 0; pnum < kMaximumPlayers; pnum++)
    {
        Player *p = players[pnum];
        if (!p)
            continue;

        if (p == elem)
            return index;

        index++; // only count non-nullptr pointers
    }

    FatalError("Internal error in SaveGamePlayerGetIndex: No such PlayerPtr: %p\n", elem);
    return 0;
}

void SaveGamePlayerCreateElems(int num_elems)
{
    LogDebug("SaveGamePlayerCreateElems...\n");

    // free existing players (sets all pointers to nullptr)
    DestroyAllPlayers();

    if (num_elems > kMaximumPlayers)
        FatalError("LOADGAME: too many players (%d)\n", num_elems);

    total_players = num_elems;
    total_bots    = 0;

    for (int pnum = 0; pnum < num_elems; pnum++)
    {
        Player *p = new Player;

        EPI_CLEAR_MEMORY(p, Player, 1);

        // Note: while loading, we don't follow the normal principle
        //       where players[p->player_number_] == p.  This is fixed in the
        //       finalisation function.
        players[pnum] = p;

        // initialise defaults

        p->player_number_ = -1; // checked during finalisation.
        sprintf(p->player_name_, "Player%d", 1 + p->player_number_);

        p->remember_attack_state_[0] = -1;
        p->remember_attack_state_[1] = -1;
        p->remember_attack_state_[2] = -1;
        p->remember_attack_state_[3] = -1;
        p->weapon_last_frame_        = -1;

        for (int j = 0; j < kTotalPlayerSpriteTypes; j++)
        {
            p->player_sprites_[j].screen_x = 0;
            p->player_sprites_[j].screen_y = 0;
        }

        for (int k = 0; k < kTotalWeaponKeys; k++)
            p->key_choices_[k] = KWeaponSelectionNone;

        for (int w = 0; w < kMaximumWeapons; w++)
            p->weapons_[w].model_skin = 1;
    }
}

void SaveGamePlayerFinaliseElems(void)
{
    int first = -1;

    console_player = -1;
    display_player = -1;

    Player *temp[kMaximumPlayers];

    for (int i = 0; i < kMaximumPlayers; i++)
    {
        temp[i]    = players[i];
        players[i] = nullptr;
    }

    for (int pnum = 0; pnum < kMaximumPlayers; pnum++)
    {
        Player *p = temp[pnum];
        if (!p)
            continue;

        if (p->player_number_ < 0)
            FatalError("LOADGAME: player did not load (index %d) !\n", pnum);

        if (p->player_number_ >= kMaximumPlayers)
            FatalError("LOADGAME: player with bad index (%d) !\n", p->player_number_);

        if (!p->map_object_)
            FatalError("LOADGAME: Player %d has no mobj !\n", p->player_number_);

        if (players[p->player_number_])
            FatalError("LOADGAME: Two players with same number !\n");

        players[p->player_number_] = p;

        if (first < 0)
            first = p->player_number_;

        if (p->player_flags_ & kPlayerFlagConsole)
            console_player = p->player_number_;

        if (p->player_flags_ & kPlayerFlagDisplay)
            display_player = p->player_number_;

        if (p->player_flags_ & kPlayerFlagBot)
        {
            total_bots++;
            P_BotCreate(p, true);
        }
        else
            p->Builder = ConsolePlayerBuilder;

        UpdateAvailWeapons(p);
        UpdateTotalArmour(p);
    }

    if (first < 0)
        FatalError("LOADGAME: No players !!\n");

    if (console_player < 0)
        SetConsolePlayer(first);

    if (display_player < 0)
        SetDisplayPlayer(console_player);
}

//----------------------------------------------------------------------------

//
// SR_PlayerGetCounter
//
bool SR_PlayerGetCounter(void *storage, int index, void *extra)
{
    PlayerStock *dest = (PlayerStock *)storage + index;

    if (sv_struct_playercounter.counterpart)
        return SaveGameStructLoad(dest, sv_struct_playercounter.counterpart);

    return true; // presumably
}

//
// SR_PlayerPutCounter
//
void SR_PlayerPutCounter(void *storage, int index, void *extra)
{
    PlayerStock *src = (PlayerStock *)storage + index;

    SaveGameStructSave(src, &sv_struct_playercounter);
}

//
// SR_PlayerGetInv
//
bool SR_PlayerGetInv(void *storage, int index, void *extra)
{
    PlayerStock *dest = (PlayerStock *)storage + index;

    if (sv_struct_playerinv.counterpart)
        return SaveGameStructLoad(dest, sv_struct_playerinv.counterpart);

    return true; // presumably
}

//
// SR_PlayerPutInv
//
void SR_PlayerPutInv(void *storage, int index, void *extra)
{
    PlayerStock *src = (PlayerStock *)storage + index;

    SaveGameStructSave(src, &sv_struct_playerinv);
}

//
// SR_PlayerGetAmmo
//
bool SR_PlayerGetAmmo(void *storage, int index, void *extra)
{
    PlayerStock *dest = (PlayerStock *)storage + index;

    if (sv_struct_playerammo.counterpart)
        return SaveGameStructLoad(dest, sv_struct_playerammo.counterpart);

    return true; // presumably
}

//
// SR_PlayerPutAmmo
//
void SR_PlayerPutAmmo(void *storage, int index, void *extra)
{
    PlayerStock *src = (PlayerStock *)storage + index;

    SaveGameStructSave(src, &sv_struct_playerammo);
}

//
// SR_PlayerGetWeapon
//
bool SR_PlayerGetWeapon(void *storage, int index, void *extra)
{
    PlayerWeapon *dest = (PlayerWeapon *)storage + index;

    if (sv_struct_playerweapon.counterpart)
        return SaveGameStructLoad(dest, sv_struct_playerweapon.counterpart);

    return true; // presumably
}

//
// SR_PlayerPutWeapon
//
void SR_PlayerPutWeapon(void *storage, int index, void *extra)
{
    PlayerWeapon *src = (PlayerWeapon *)storage + index;

    SaveGameStructSave(src, &sv_struct_playerweapon);
}

//
// SR_PlayerGetPSprite
//
bool SR_PlayerGetPSprite(void *storage, int index, void *extra)
{
    PlayerSprite *dest = (PlayerSprite *)storage + index;

    //!!! FIXME: should skip if no counterpart
    if (sv_struct_psprite.counterpart)
        return SaveGameStructLoad(dest, sv_struct_psprite.counterpart);

    return true; // presumably
}

//
// SR_PlayerPutPSprite
//
void SR_PlayerPutPSprite(void *storage, int index, void *extra)
{
    PlayerSprite *src = (PlayerSprite *)storage + index;

    SaveGameStructSave(src, &sv_struct_psprite);
}

//
// SR_PlayerGetName
//
bool SR_PlayerGetName(void *storage, int index, void *extra)
{
    char       *dest = (char *)storage;
    const char *str;

    EPI_ASSERT(index == 0);

    str = SaveChunkGetString();
    epi::CStringCopyMax(dest, str, kPlayerNameCharacterLimit - 1);
    SaveChunkFreeString(str);

    return true;
}

//
// SR_PlayerPutName
//
void SR_PlayerPutName(void *storage, int index, void *extra)
{
    char *src = (char *)storage;

    EPI_ASSERT(index == 0);

    SaveChunkPutString(src);
}

//
// SR_WeaponGetInfo
//
bool SR_WeaponGetInfo(void *storage, int index, void *extra)
{
    WeaponDefinition **dest = (WeaponDefinition **)storage + index;
    const char        *name;

    name = SaveChunkGetString();

    *dest = name ? weapondefs.Lookup(name) : nullptr;
    SaveChunkFreeString(name);

    return true;
}

//
// SR_WeaponPutInfo
//
void SR_WeaponPutInfo(void *storage, int index, void *extra)
{
    WeaponDefinition *info = ((WeaponDefinition **)storage)[index];

    SaveChunkPutString(info ? info->name_.c_str() : nullptr);
}

//----------------------------------------------------------------------------

//
// SR_PlayerGetState
//
bool SR_PlayerGetState(void *storage, int index, void *extra)
{
    State **dest = (State **)storage + index;

    char  buffer[256];
    char *base_p, *off_p;
    int   base, offset;

    const char             *swizzle;
    const WeaponDefinition *actual;

    swizzle = SaveChunkGetString();

    if (!swizzle)
    {
        *dest = nullptr;
        return true;
    }

    epi::CStringCopyMax(buffer, swizzle, 256 - 1);
    SaveChunkFreeString(swizzle);

    // separate string at `:' characters

    base_p = strchr(buffer, ':');

    if (base_p == nullptr || base_p[0] == 0)
        FatalError("Corrupt savegame: bad weapon state 1: `%s'\n", buffer);

    *base_p++ = 0;

    off_p = strchr(base_p, ':');

    if (off_p == nullptr || off_p[0] == 0)
        FatalError("Corrupt savegame: bad weapon state 2: `%s'\n", base_p);

    *off_p++ = 0;

    // find weapon that contains the state
    // Traverses backwards in case #CLEARALL was used.
    actual = nullptr;

    actual = weapondefs.Lookup(buffer);
    if (!actual)
        FatalError("LOADGAME: no such weapon %s for state %s:%s\n", buffer, base_p, off_p);

    // find base state
    offset = strtol(off_p, nullptr, 0) - 1;

    base = DDF_StateFindLabel(actual->state_grp_, base_p, true /* quiet */);

    if (!base)
    {
        LogWarning("LOADGAME: no such label `%s' for weapon state.\n", base_p);

        offset = 0;
        base   = actual->ready_state_;
    }

    *dest = states + base + offset;

    return true;
}

//
// SR_PlayerPutState
//
// The format of the string is:
//
//    WEAPON:BASE:OFFSET
//
// where WEAPON refers the ddf weapon containing the state.  BASE is
// the nearest labelled state (e.g. "SPAWN"), or "*" as offset from
// the weapon's first state (unlikely to be needed).  OFFSET is the
// integer offset from the base state, which BTW starts at 1 (like in
// ddf).
//
// Alternatively, the string can be nullptr, which means the state
// pointer should be nullptr.
//
void SR_PlayerPutState(void *storage, int index, void *extra)
{
    State *S = ((State **)storage)[index];

    if (S == nullptr)
    {
        SaveChunkPutString(nullptr);
        return;
    }

    // get state number, check if valid
    int s_num = S - states;

    if (s_num < 0 || s_num >= num_states)
    {
        LogWarning("SAVEGAME: weapon is in invalid state %d\n", s_num);
        s_num = weapondefs[0]->state_grp_[0].first;
    }

    // find the weapon that this state belongs to.
    // Traverses backwards in case #CLEARALL was used.
    const WeaponDefinition *actual = nullptr;

    for (auto iter = weapondefs.rbegin(); iter != weapondefs.rend(); iter++)
    {
        actual = *iter;

        if (DDF_StateGroupHasState(actual->state_grp_, s_num))
            break;
    }

    if (!actual)
    {
        LogWarning("SAVEGAME: weapon state %d cannot be found !!\n", s_num);
        actual = weapondefs[0];
        s_num  = actual->state_grp_[0].first;
    }

    // find the nearest base state
    int base = s_num;

    while (!states[base].label && DDF_StateGroupHasState(actual->state_grp_, base - 1))
    {
        base--;
    }

    std::string buf(epi::StringFormat("%s:%s:%d", actual->name_.c_str(), states[base].label ? states[base].label : "*",
                                      1 + s_num - base));

    SaveChunkPutString(buf.c_str());
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
