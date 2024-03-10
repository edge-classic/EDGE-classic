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
#include "sv_chunk.h"
#include "sv_main.h"

// EPI
#include "str_util.h"

// DDF
#include "main.h"

// forward decls.
int   SaveGamePlayerCountElems(void);
int   SaveGamePlayerGetIndex(player_t *elem);
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

static SaveField sv_fields_player[] = {
    {offsetof(player_t, pnum),
     "pnum",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetInteger,
     SaveGamePutInteger,
     nullptr},
    {offsetof(player_t, playerstate),
     "playerstate",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetInteger,
     SaveGamePutInteger,
     nullptr},
    {offsetof(player_t, playerflags),
     "playerflags",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetInteger,
     SaveGamePutInteger,
     nullptr},
    {offsetof(player_t, playername[0]),
     "playername",
     1,
     {kSaveFieldString, 0, nullptr},
     SR_PlayerGetName,
     SR_PlayerPutName,
     nullptr},
    {offsetof(player_t, mo),
     "mo",
     1,
     {kSaveFieldIndex, 4, "mobjs"},
     SaveGameGetMapObject,
     SaveGamePutMapObject,
     nullptr},
    {offsetof(player_t, view_z),
     "viewz",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetFloat,
     SaveGamePutFloat,
     nullptr},
    {offsetof(player_t, viewheight),
     "viewheight",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetFloat,
     SaveGamePutFloat,
     nullptr},
    {offsetof(player_t, deltaviewheight),
     "deltaviewheight",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetFloat,
     SaveGamePutFloat,
     nullptr},
    {offsetof(player_t, std_viewheight),
     "std_viewheight",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetFloat,
     SaveGamePutFloat,
     nullptr},
    {offsetof(player_t, zoom_fov),
     "zoom_fov",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetInteger,
     SaveGamePutInteger,
     nullptr},
    {offsetof(player_t, actual_speed),
     "actual_speed",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetFloat,
     SaveGamePutFloat,
     nullptr},
    {offsetof(player_t, health),
     "health",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetFloat,
     SaveGamePutFloat,
     nullptr},
    {offsetof(player_t, armours[0]),
     "armours",
     kTotalArmourTypes,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetFloat,
     SaveGamePutFloat,
     nullptr},
    {offsetof(player_t, armour_types[0]),
     "armour_types",
     kTotalArmourTypes,
     {kSaveFieldString, 0, nullptr},
     SaveGameMapObjectGetType,
     SaveGameMapObjectPutType,
     nullptr},
    {offsetof(player_t, powers[0]),
     "powers",
     kTotalPowerTypes,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetFloat,
     SaveGamePutFloat,
     nullptr},
    {offsetof(player_t, keep_powers),
     "keep_powers",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetInteger,
     SaveGamePutInteger,
     nullptr},
    {offsetof(player_t, cards),
     "cards_ke",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetInteger,
     SaveGamePutInteger,
     nullptr},
    {offsetof(player_t, frags),
     "frags",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetInteger,
     SaveGamePutInteger,
     nullptr},
    {offsetof(player_t, totalfrags),
     "totalfrags",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetInteger,
     SaveGamePutInteger,
     nullptr},
    {offsetof(player_t, ready_wp),
     "ready_wp",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetInteger,
     SaveGamePutInteger,
     nullptr},
    {offsetof(player_t, pending_wp),
     "pending_wp",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetInteger,
     SaveGamePutInteger,
     nullptr},
    {offsetof(player_t, weapons[0]),
     "weapons",
     kMaximumWeapons,
     {kSaveFieldStruct, 0, "playerweapon_t"},
     SR_PlayerGetWeapon,
     SR_PlayerPutWeapon,
     nullptr},
    {offsetof(player_t, ammo[0]),
     "ammo",
     kTotalAmmunitionTypes,
     {kSaveFieldStruct, 0, "playerammo_t"},
     SR_PlayerGetAmmo,
     SR_PlayerPutAmmo,
     nullptr},
    {offsetof(player_t, inventory[0]),
     "inventory",
     kTotalInventoryTypes,
     {kSaveFieldStruct, 0, "playerinv_t"},
     SR_PlayerGetInv,
     SR_PlayerPutInv,
     nullptr},
    {offsetof(player_t, counters[0]),
     "counters",
     kTotalCounterTypes,
     {kSaveFieldStruct, 0, "playercounter_t"},
     SR_PlayerGetCounter,
     SR_PlayerPutCounter,
     nullptr},
    {offsetof(player_t, cheats),
     "cheats",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetInteger,
     SaveGamePutInteger,
     nullptr},
    {offsetof(player_t, refire),
     "refire",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetInteger,
     SaveGamePutInteger,
     nullptr},
    {offsetof(player_t, killcount),
     "killcount",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetInteger,
     SaveGamePutInteger,
     nullptr},
    {offsetof(player_t, itemcount),
     "itemcount",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetInteger,
     SaveGamePutInteger,
     nullptr},
    {offsetof(player_t, secretcount),
     "secretcount",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetInteger,
     SaveGamePutInteger,
     nullptr},
    {offsetof(player_t, jumpwait),
     "jumpwait",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetInteger,
     SaveGamePutInteger,
     nullptr},
    {offsetof(player_t, idlewait),
     "idlewait",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetInteger,
     SaveGamePutInteger,
     nullptr},
    {offsetof(player_t, air_in_lungs),
     "air_in_lungs",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetInteger,
     SaveGamePutInteger,
     nullptr},
    {offsetof(player_t, underwater),
     "underwater",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetBoolean,
     SaveGamePutBoolean,
     nullptr},
    {offsetof(player_t, airless),
     "airless",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetBoolean,
     SaveGamePutBoolean,
     nullptr},
    {offsetof(player_t, flash),
     "flash_b",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetBoolean,
     SaveGamePutBoolean,
     nullptr},
    {offsetof(player_t, psprites[0]),
     "psprites",
     kTotalPlayerSpriteTypes,
     {kSaveFieldStruct, 0, "psprite_t"},
     SR_PlayerGetPSprite,
     SR_PlayerPutPSprite,
     nullptr},

    // FIXME: swimming & wet_feet ???

    // NOT HERE:
    //   in_game: only in-game players are saved.
    //   key_choices: depends on DDF too much, and not important.
    //   remember_atk[]: ditto.
    //   next,prev: links are regenerated.
    //   avail_weapons, totalarmour: regenerated.
    //   attacker: not very important

    {0,
     nullptr,
     0,
     {kSaveFieldInvalid, 0, nullptr},
     nullptr,
     nullptr,
     nullptr}};

SaveStruct sv_struct_player = {
    nullptr,           // link in list
    "player_t",        // structure name
    "play",            // start marker
    sv_fields_player,  // field descriptions
    true,              // define_me
    nullptr            // pointer to known struct
};

SaveArray sv_array_player = {
    nullptr,            // link in list
    "players",          // array name
    &sv_struct_player,  // array type
    true,               // define_me
    false,              // allow_hub

    SaveGamePlayerCountElems,     // count routine
    SaveGamePlayerFindByIndex,    // index routine
    SaveGamePlayerCreateElems,    // creation routine
    SaveGamePlayerFinaliseElems,  // finalisation routine

    nullptr,  // pointer to known array
    0         // loaded size
};

//----------------------------------------------------------------------------
//
//  WEAPON STRUCTURE
//

static SaveField sv_fields_playerweapon[] = {
    {offsetof(PlayerWeapon, info),
     "info",
     1,
     {kSaveFieldString, 0, nullptr},
     SR_WeaponGetInfo,
     SR_WeaponPutInfo,
     nullptr},
    {offsetof(PlayerWeapon, owned),
     "owned",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetBoolean,
     SaveGamePutBoolean,
     nullptr},
    {offsetof(PlayerWeapon, flags),
     "flags",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetInteger,
     SaveGamePutInteger,
     nullptr},
    {offsetof(PlayerWeapon, clip_size[0]),
     "clip_size",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetInteger,
     SaveGamePutInteger,
     nullptr},
    {offsetof(PlayerWeapon, clip_size[1]),
     "sa_clip_size",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetInteger,
     SaveGamePutInteger,
     nullptr},
    {offsetof(PlayerWeapon, clip_size[2]),
     "ta_clip_size",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetInteger,
     SaveGamePutInteger,
     nullptr},
    {offsetof(PlayerWeapon, clip_size[3]),
     "fa_clip_size",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetInteger,
     SaveGamePutInteger,
     nullptr},
    {offsetof(PlayerWeapon, model_skin),
     "model_skin",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetInteger,
     SaveGamePutInteger,
     nullptr},

    {0,
     nullptr,
     0,
     {kSaveFieldInvalid, 0, nullptr},
     nullptr,
     nullptr,
     nullptr}};

SaveStruct sv_struct_playerweapon = {
    nullptr,                 // link in list
    "playerweapon_t",        // structure name
    "weap",                  // start marker
    sv_fields_playerweapon,  // field descriptions
    true,                    // define_me
    nullptr                  // pointer to known struct
};

//----------------------------------------------------------------------------
//
//  COUNTER STRUCTURE
//

static SaveField sv_fields_playercounter[] = {{offsetof(playercounter_t, num),
                                               "num",
                                               1,
                                               {kSaveFieldNumeric, 4, nullptr},
                                               SaveGameGetInteger,
                                               SaveGamePutInteger,
                                               nullptr},
                                              {offsetof(playercounter_t, max),
                                               "max",
                                               1,
                                               {kSaveFieldNumeric, 4, nullptr},
                                               SaveGameGetInteger,
                                               SaveGamePutInteger,
                                               nullptr},

                                              {0,
                                               nullptr,
                                               0,
                                               {kSaveFieldInvalid, 0, nullptr},
                                               nullptr,
                                               nullptr,
                                               nullptr}};

SaveStruct sv_struct_playercounter = {
    nullptr,                  // link in list
    "playercounter_t",        // structure name
    "cntr",                   // start marker
    sv_fields_playercounter,  // field descriptions
    true,                     // define_me
    nullptr                   // pointer to known struct
};

//----------------------------------------------------------------------------
//
//  INVENTORY STRUCTURE
//

static SaveField sv_fields_playerinv[] = {{offsetof(playerinv_t, num),
                                           "num",
                                           1,
                                           {kSaveFieldNumeric, 4, nullptr},
                                           SaveGameGetInteger,
                                           SaveGamePutInteger,
                                           nullptr},
                                          {offsetof(playerinv_t, max),
                                           "max",
                                           1,
                                           {kSaveFieldNumeric, 4, nullptr},
                                           SaveGameGetInteger,
                                           SaveGamePutInteger,
                                           nullptr},

                                          {0,
                                           nullptr,
                                           0,
                                           {kSaveFieldInvalid, 0, nullptr},
                                           nullptr,
                                           nullptr,
                                           nullptr}};

SaveStruct sv_struct_playerinv = {
    nullptr,              // link in list
    "playerinv_t",        // structure name
    "invy",               // start marker
    sv_fields_playerinv,  // field descriptions
    true,                 // define_me
    nullptr               // pointer to known struct
};

//----------------------------------------------------------------------------
//
//  AMMO STRUCTURE
//

static SaveField sv_fields_playerammo[] = {{offsetof(playerammo_t, num),
                                            "num",
                                            1,
                                            {kSaveFieldNumeric, 4, nullptr},
                                            SaveGameGetInteger,
                                            SaveGamePutInteger,
                                            nullptr},
                                           {offsetof(playerammo_t, max),
                                            "max",
                                            1,
                                            {kSaveFieldNumeric, 4, nullptr},
                                            SaveGameGetInteger,
                                            SaveGamePutInteger,
                                            nullptr},

                                           {0,
                                            nullptr,
                                            0,
                                            {kSaveFieldInvalid, 0, nullptr},
                                            nullptr,
                                            nullptr,
                                            nullptr}};

SaveStruct sv_struct_playerammo = {
    nullptr,               // link in list
    "playerammo_t",        // structure name
    "ammo",                // start marker
    sv_fields_playerammo,  // field descriptions
    true,                  // define_me
    nullptr                // pointer to known struct
};

//----------------------------------------------------------------------------
//
//  PSPRITE STRUCTURE
//

static SaveField sv_fields_psprite[] = {
    {offsetof(PlayerSprite, state),
     "state",
     1,
     {kSaveFieldString, 0, nullptr},
     SR_PlayerGetState,
     SR_PlayerPutState,
     nullptr},
    {offsetof(PlayerSprite, next_state),
     "next_state",
     1,
     {kSaveFieldString, 0, nullptr},
     SR_PlayerGetState,
     SR_PlayerPutState,
     nullptr},
    {offsetof(PlayerSprite, tics),
     "tics",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetInteger,
     SaveGamePutInteger,
     nullptr},
    {offsetof(PlayerSprite, visibility),
     "visibility",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetFloat,
     SaveGamePutFloat,
     nullptr},
    {offsetof(PlayerSprite, target_visibility),
     "vis_target",
     1,
     {kSaveFieldNumeric, 4, nullptr},
     SaveGameGetFloat,
     SaveGamePutFloat,
     nullptr},

    // NOT HERE:
    //   sx, sy: they can be regenerated.

    {0,
     nullptr,
     0,
     {kSaveFieldInvalid, 0, nullptr},
     nullptr,
     nullptr,
     nullptr}};

SaveStruct sv_struct_psprite = {
    nullptr,            // link in list
    "pspdef_t",         // structure name
    "pspr",             // start marker
    sv_fields_psprite,  // field descriptions
    true,               // define_me
    nullptr             // pointer to known struct
};

//----------------------------------------------------------------------------

int SaveGamePlayerCountElems(void)
{
    int count = 0;

    for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
    {
        player_t *p = players[pnum];
        if (!p) continue;

        if (p->node) continue;

        count++;
    }

    SYS_ASSERT(count > 0);

    return count;
}

void *SaveGamePlayerFindByIndex(int index)
{
    if (index >= numplayers)
        FatalError("LOADGAME: Invalid player index: %d\n", index);

    for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
    {
        player_t *p = players[pnum];
        if (!p) continue;

        if (p->node) continue;

        if (index == 0) return p;

        index--;
    }

    FatalError(
        "Internal error in SaveGamePlayerFindByIndex: index not found.\n");
    return nullptr;
}

int SaveGamePlayerGetIndex(player_t *elem)
{
    int index = 0;

    for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
    {
        player_t *p = players[pnum];
        if (!p) continue;

        if (p->node) continue;

        if (p == elem) return index;

        index++;  // only count non-nullptr pointers
    }

    FatalError(
        "Internal error in SaveGamePlayerGetIndex: No such PlayerPtr: %p\n",
        elem);
    return 0;
}

void SaveGamePlayerCreateElems(int num_elems)
{
    LogDebug("SaveGamePlayerCreateElems...\n");

    // free existing players (sets all pointers to nullptr)
    DestroyAllPlayers();

    if (num_elems > MAXPLAYERS)
        FatalError("LOADGAME: too many players (%d)\n", num_elems);

    numplayers = num_elems;
    numbots    = 0;

    for (int pnum = 0; pnum < num_elems; pnum++)
    {
        player_t *p = new player_t;

        Z_Clear(p, player_t, 1);

        // Note: while loading, we don't follow the normal principle
        //       where players[p->pnum] == p.  This is fixed in the
        //       finalisation function.
        players[pnum] = p;

        // initialise defaults

        p->pnum = -1;  // checked during finalisation.
        sprintf(p->playername, "Player%d", 1 + p->pnum);

        p->remember_atk[0]   = -1;
        p->remember_atk[1]   = -1;
        p->remember_atk[2]   = -1;
        p->remember_atk[3]   = -1;
        p->weapon_last_frame = -1;

        for (int j = 0; j < kTotalPlayerSpriteTypes; j++)
        {
            p->psprites[j].screen_x = 0;
            p->psprites[j].screen_y = 0;
        }

        for (int k = 0; k < kTotalWeaponKeys; k++)
            p->key_choices[k] = WPSEL_None;

        for (int w = 0; w < kMaximumWeapons; w++) p->weapons[w].model_skin = 1;
    }
}

void SaveGamePlayerFinaliseElems(void)
{
    int first = -1;

    consoleplayer = -1;
    displayplayer = -1;

    player_t *temp[MAXPLAYERS];

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        temp[i]    = players[i];
        players[i] = nullptr;
    }

    for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
    {
        player_t *p = temp[pnum];
        if (!p) continue;

        if (p->pnum < 0)
            FatalError("LOADGAME: player did not load (index %d) !\n", pnum);

        if (p->pnum >= MAXPLAYERS)
            FatalError("LOADGAME: player with bad index (%d) !\n", p->pnum);

        if (!p->mo) FatalError("LOADGAME: Player %d has no mobj !\n", p->pnum);

        if (players[p->pnum])
            FatalError("LOADGAME: Two players with same number !\n");

        players[p->pnum] = p;

        if (first < 0) first = p->pnum;

        if (p->playerflags & PFL_Console) consoleplayer = p->pnum;

        if (p->playerflags & PFL_Display) displayplayer = p->pnum;

        if (p->playerflags & PFL_Bot)
        {
            numbots++;
            P_BotCreate(p, true);
        }
        else
            p->builder = P_ConsolePlayerBuilder;

        UpdateAvailWeapons(p);
        UpdateTotalArmour(p);
    }

    if (first < 0) FatalError("LOADGAME: No players !!\n");

    if (consoleplayer < 0) GameSetConsolePlayer(first);

    if (displayplayer < 0) GameSetDisplayPlayer(consoleplayer);
}

//----------------------------------------------------------------------------

//
// SR_PlayerGetCounter
//
bool SR_PlayerGetCounter(void *storage, int index, void *extra)
{
    playercounter_t *dest = (playercounter_t *)storage + index;

    if (sv_struct_playercounter.counterpart)
        return SaveGameStructLoad(dest, sv_struct_playercounter.counterpart);

    return true;  // presumably
}

//
// SR_PlayerPutCounter
//
void SR_PlayerPutCounter(void *storage, int index, void *extra)
{
    playercounter_t *src = (playercounter_t *)storage + index;

    SaveGameStructSave(src, &sv_struct_playercounter);
}

//
// SR_PlayerGetInv
//
bool SR_PlayerGetInv(void *storage, int index, void *extra)
{
    playerinv_t *dest = (playerinv_t *)storage + index;

    if (sv_struct_playerinv.counterpart)
        return SaveGameStructLoad(dest, sv_struct_playerinv.counterpart);

    return true;  // presumably
}

//
// SR_PlayerPutInv
//
void SR_PlayerPutInv(void *storage, int index, void *extra)
{
    playerinv_t *src = (playerinv_t *)storage + index;

    SaveGameStructSave(src, &sv_struct_playerinv);
}

//
// SR_PlayerGetAmmo
//
bool SR_PlayerGetAmmo(void *storage, int index, void *extra)
{
    playerammo_t *dest = (playerammo_t *)storage + index;

    if (sv_struct_playerammo.counterpart)
        return SaveGameStructLoad(dest, sv_struct_playerammo.counterpart);

    return true;  // presumably
}

//
// SR_PlayerPutAmmo
//
void SR_PlayerPutAmmo(void *storage, int index, void *extra)
{
    playerammo_t *src = (playerammo_t *)storage + index;

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

    return true;  // presumably
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

    return true;  // presumably
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

    SYS_ASSERT(index == 0);

    str = SaveChunkGetString();
    epi::CStringCopyMax(dest, str, MAX_PLAYNAME - 1);
    SaveChunkFreeString(str);

    return true;
}

//
// SR_PlayerPutName
//
void SR_PlayerPutName(void *storage, int index, void *extra)
{
    char *src = (char *)storage;

    SYS_ASSERT(index == 0);

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
        FatalError("LOADGAME: no such weapon %s for state %s:%s\n", buffer,
                   base_p, off_p);

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

        if (DDF_StateGroupHasState(actual->state_grp_, s_num)) break;
    }

    if (!actual)
    {
        LogWarning("SAVEGAME: weapon state %d cannot be found !!\n", s_num);
        actual = weapondefs[0];
        s_num  = actual->state_grp_[0].first;
    }

    // find the nearest base state
    int base = s_num;

    while (!states[base].label &&
           DDF_StateGroupHasState(actual->state_grp_, base - 1))
    {
        base--;
    }

    std::string buf(epi::StringFormat(
        "%s:%s:%d", actual->name_.c_str(),
        states[base].label ? states[base].label : "*", 1 + s_num - base));

    SaveChunkPutString(buf.c_str());
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
