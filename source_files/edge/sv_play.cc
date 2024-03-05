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

#undef SF
#define SF SVFIELD

// forward decls.
int   SV_PlayerCountElems(void);
int   SV_PlayerFindElem(player_t *elem);
void *SV_PlayerGetElem(int index);
void  SV_PlayerCreateElems(int num_elems);
void  SV_PlayerFinaliseElems(void);

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

extern bool SR_MobjGetType(void *storage, int index, void *extra);
extern void SR_MobjPutType(void *storage, int index, void *extra);

//----------------------------------------------------------------------------
//
//  PLAYER STRUCTURE AND ARRAY
//
static player_t sv_dummy_player;

#define SV_F_BASE sv_dummy_player

static savefield_t sv_fields_player[] = {
    SF(pnum, "pnum", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(playerstate, "playerstate", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(playerflags, "playerflags", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(playername[0], "playername", 1, SVT_STRING, SR_PlayerGetName, SR_PlayerPutName),
    SF(mo, "mo", 1, SVT_INDEX("mobjs"), SR_MobjGetMobj, SR_MobjPutMobj),
    SF(view_z, "view_z", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(viewheight, "viewheight", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(deltaviewheight, "deltaviewheight", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(std_viewheight, "std_viewheight", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(zoom_fov, "zoom_fov", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(actual_speed, "actual_speed", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(health, "health", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(armours[0], "armours", kTotalArmourTypes, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(armour_types[0], "armour_types", kTotalArmourTypes, SVT_STRING, SR_MobjGetType, SR_MobjPutType),
    SF(powers[0], "powers", kTotalPowerTypes, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(keep_powers, "keep_powers", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(cards, "cards_ke", 1, SVT_ENUM, SR_GetEnum, SR_PutEnum), SF(frags, "frags", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(totalfrags, "totalfrags", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(ready_wp, "ready_wp", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(pending_wp, "pending_wp", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(weapons[0], "weapons", kMaximumWeapons, SVT_STRUCT("playerweapon_t"), SR_PlayerGetWeapon, SR_PlayerPutWeapon),
    SF(ammo[0], "ammo", kTotalAmmunitionTypes, SVT_STRUCT("playerammo_t"), SR_PlayerGetAmmo, SR_PlayerPutAmmo),
    SF(inventory[0], "inventory", kTotalInventoryTypes, SVT_STRUCT("playerinv_t"), SR_PlayerGetInv, SR_PlayerPutInv),
    SF(counters[0], "counters", kTotalCounterTypes, SVT_STRUCT("playercounter_t"), SR_PlayerGetCounter, SR_PlayerPutCounter),
    SF(cheats, "cheats", 1, SVT_INT, SR_GetInt, SR_PutInt), SF(refire, "refire", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(killcount, "killcount", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(itemcount, "itemcount", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(secretcount, "secretcount", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(jumpwait, "jumpwait", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(idlewait, "idlewait", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(air_in_lungs, "air_in_lungs", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(underwater, "underwater", 1, SVT_BOOLEAN, SR_GetBoolean, SR_PutBoolean),
    SF(airless, "airless", 1, SVT_BOOLEAN, SR_GetBoolean, SR_PutBoolean),
    SF(flash, "flash_b", 1, SVT_BOOLEAN, SR_GetBoolean, SR_PutBoolean),
    SF(psprites[0], "psprites", kTotalPlayerSpriteTypes, SVT_STRUCT("psprite_t"), SR_PlayerGetPSprite, SR_PlayerPutPSprite),

    // FIXME: swimming & wet_feet ???

    // NOT HERE:
    //   in_game: only in-game players are saved.
    //   key_choices: depends on DDF too much, and not important.
    //   remember_atk[]: ditto.
    //   next,prev: links are regenerated.
    //   avail_weapons, totalarmour: regenerated.
    //   attacker: not very important

    SVFIELD_END};

savestruct_t sv_struct_player = {
    nullptr,             // link in list
    "player_t",       // structure name
    "play",           // start marker
    sv_fields_player, // field descriptions
    SVDUMMY,          // dummy base
    true,             // define_me
    nullptr              // pointer to known struct
};

#undef SV_F_BASE

savearray_t sv_array_player = {
    nullptr,              // link in list
    "players",         // array name
    &sv_struct_player, // array type
    true,              // define_me
    false,             // allow_hub

    SV_PlayerCountElems,    // count routine
    SV_PlayerGetElem,       // index routine
    SV_PlayerCreateElems,   // creation routine
    SV_PlayerFinaliseElems, // finalisation routine

    nullptr, // pointer to known array
    0     // loaded size
};

//----------------------------------------------------------------------------
//
//  WEAPON STRUCTURE
//
static PlayerWeapon sv_dummy_playerweapon;

#define SV_F_BASE sv_dummy_playerweapon

static savefield_t sv_fields_playerweapon[] = {SF(info, "info", 1, SVT_STRING, SR_WeaponGetInfo, SR_WeaponPutInfo),
                                               SF(owned, "owned", 1, SVT_BOOLEAN, SR_GetBoolean, SR_PutBoolean),
                                               SF(flags, "flags", 1, SVT_INT, SR_GetInt, SR_PutInt),
                                               SF(clip_size[0], "clip_size", 1, SVT_INT, SR_GetInt, SR_PutInt),
                                               SF(clip_size[1], "sa_clip_size", 1, SVT_INT, SR_GetInt, SR_PutInt),
                                               SF(clip_size[2], "ta_clip_size", 1, SVT_INT, SR_GetInt, SR_PutInt),
                                               SF(clip_size[3], "fa_clip_size", 1, SVT_INT, SR_GetInt, SR_PutInt),
                                               SF(model_skin, "model_skin", 1, SVT_INT, SR_GetInt, SR_PutInt),

                                               SVFIELD_END};

savestruct_t sv_struct_playerweapon = {
    nullptr,                   // link in list
    "playerweapon_t",       // structure name
    "weap",                 // start marker
    sv_fields_playerweapon, // field descriptions
    SVDUMMY,                // dummy base
    true,                   // define_me
    nullptr                    // pointer to known struct
};

#undef SV_F_BASE

//----------------------------------------------------------------------------
//
//  COUNTER STRUCTURE
//
static playercounter_t sv_dummy_playercounter;

#define SV_F_BASE sv_dummy_playercounter

static savefield_t sv_fields_playercounter[] = {SF(num, "num", 1, SVT_INT, SR_GetInt, SR_PutInt),
                                                SF(max, "max", 1, SVT_INT, SR_GetInt, SR_PutInt),

                                                SVFIELD_END};

savestruct_t sv_struct_playercounter = {
    nullptr,                    // link in list
    "playercounter_t",       // structure name
    "cntr",                  // start marker
    sv_fields_playercounter, // field descriptions
    SVDUMMY,                 // dummy base
    true,                    // define_me
    nullptr                     // pointer to known struct
};

#undef SV_F_BASE

//----------------------------------------------------------------------------
//
//  INVENTORY STRUCTURE
//
static playerinv_t sv_dummy_playerinv;

#define SV_F_BASE sv_dummy_playerinv

static savefield_t sv_fields_playerinv[] = {SF(num, "num", 1, SVT_INT, SR_GetInt, SR_PutInt),
                                            SF(max, "max", 1, SVT_INT, SR_GetInt, SR_PutInt),

                                            SVFIELD_END};

savestruct_t sv_struct_playerinv = {
    nullptr,                // link in list
    "playerinv_t",       // structure name
    "invy",              // start marker
    sv_fields_playerinv, // field descriptions
    SVDUMMY,             // dummy base
    true,                // define_me
    nullptr                 // pointer to known struct
};

#undef SV_F_BASE

//----------------------------------------------------------------------------
//
//  AMMO STRUCTURE
//
static playerammo_t sv_dummy_playerammo;

#define SV_F_BASE sv_dummy_playerammo

static savefield_t sv_fields_playerammo[] = {SF(num, "num", 1, SVT_INT, SR_GetInt, SR_PutInt),
                                             SF(max, "max", 1, SVT_INT, SR_GetInt, SR_PutInt),

                                             SVFIELD_END};

savestruct_t sv_struct_playerammo = {
    nullptr,                 // link in list
    "playerammo_t",       // structure name
    "ammo",               // start marker
    sv_fields_playerammo, // field descriptions
    SVDUMMY,              // dummy base
    true,                 // define_me
    nullptr                  // pointer to known struct
};

#undef SV_F_BASE

//----------------------------------------------------------------------------
//
//  PSPRITE STRUCTURE
//
static PlayerSprite sv_dummy_psprite;

#define SV_F_BASE sv_dummy_psprite

static savefield_t sv_fields_psprite[] = {
    SF(state, "state", 1, SVT_STRING, SR_PlayerGetState, SR_PlayerPutState),
    SF(next_state, "next_state", 1, SVT_STRING, SR_PlayerGetState, SR_PlayerPutState),
    SF(tics, "tics", 1, SVT_INT, SR_GetInt, SR_PutInt),
    SF(visibility, "visibility", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),
    SF(target_visibility, "vis_target", 1, SVT_FLOAT, SR_GetFloat, SR_PutFloat),

    // NOT HERE:
    //   sx, sy: they can be regenerated.

    SVFIELD_END};

savestruct_t sv_struct_psprite = {
    nullptr,              // link in list
    "pspdef_t",        // structure name
    "pspr",            // start marker
    sv_fields_psprite, // field descriptions
    SVDUMMY,           // dummy base
    true,              // define_me
    nullptr               // pointer to known struct
};

#undef SV_F_BASE

//----------------------------------------------------------------------------

int SV_PlayerCountElems(void)
{
    int count = 0;

    for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
    {
        player_t *p = players[pnum];
        if (!p)
            continue;

        if (p->node)
            continue;

        count++;
    }

    SYS_ASSERT(count > 0);

    return count;
}

void *SV_PlayerGetElem(int index)
{
    if (index >= numplayers)
        FatalError("LOADGAME: Invalid player index: %d\n", index);

    for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
    {
        player_t *p = players[pnum];
        if (!p)
            continue;

        if (p->node)
            continue;

        if (index == 0)
            return p;

        index--;
    }

    FatalError("Internal error in SV_PlayerGetElem: index not found.\n");
    return nullptr;
}

int SV_PlayerFindElem(player_t *elem)
{
    int index = 0;

    for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
    {
        player_t *p = players[pnum];
        if (!p)
            continue;

        if (p->node)
            continue;

        if (p == elem)
            return index;

        index++; // only count non-nullptr pointers
    }

    FatalError("Internal error in SV_PlayerFindElem: No such PlayerPtr: %p\n", elem);
    return 0;
}

void SV_PlayerCreateElems(int num_elems)
{
    LogDebug("SV_PlayerCreateElems...\n");

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

        p->pnum = -1; // checked during finalisation.
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

        for (int w = 0; w < kMaximumWeapons; w++)
            p->weapons[w].model_skin = 1;
    }
}

void SV_PlayerFinaliseElems(void)
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
        if (!p)
            continue;

        if (p->pnum < 0)
            FatalError("LOADGAME: player did not load (index %d) !\n", pnum);

        if (p->pnum >= MAXPLAYERS)
            FatalError("LOADGAME: player with bad index (%d) !\n", p->pnum);

        if (!p->mo)
            FatalError("LOADGAME: Player %d has no mobj !\n", p->pnum);

        if (players[p->pnum])
            FatalError("LOADGAME: Two players with same number !\n");

        players[p->pnum] = p;

        if (first < 0)
            first = p->pnum;

        if (p->playerflags & PFL_Console)
            consoleplayer = p->pnum;

        if (p->playerflags & PFL_Display)
            displayplayer = p->pnum;

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

    if (first < 0)
        FatalError("LOADGAME: No players !!\n");

    if (consoleplayer < 0)
        GameSetConsolePlayer(first);

    if (displayplayer < 0)
        GameSetDisplayPlayer(consoleplayer);
}

//----------------------------------------------------------------------------

//
// SR_PlayerGetCounter
//
bool SR_PlayerGetCounter(void *storage, int index, void *extra)
{
    playercounter_t *dest = (playercounter_t *)storage + index;

    if (sv_struct_playercounter.counterpart)
        return SV_LoadStruct(dest, sv_struct_playercounter.counterpart);

    return true; // presumably
}

//
// SR_PlayerPutCounter
//
void SR_PlayerPutCounter(void *storage, int index, void *extra)
{
    playercounter_t *src = (playercounter_t *)storage + index;

    SV_SaveStruct(src, &sv_struct_playercounter);
}

//
// SR_PlayerGetInv
//
bool SR_PlayerGetInv(void *storage, int index, void *extra)
{
    playerinv_t *dest = (playerinv_t *)storage + index;

    if (sv_struct_playerinv.counterpart)
        return SV_LoadStruct(dest, sv_struct_playerinv.counterpart);

    return true; // presumably
}

//
// SR_PlayerPutInv
//
void SR_PlayerPutInv(void *storage, int index, void *extra)
{
    playerinv_t *src = (playerinv_t *)storage + index;

    SV_SaveStruct(src, &sv_struct_playerinv);
}

//
// SR_PlayerGetAmmo
//
bool SR_PlayerGetAmmo(void *storage, int index, void *extra)
{
    playerammo_t *dest = (playerammo_t *)storage + index;

    if (sv_struct_playerammo.counterpart)
        return SV_LoadStruct(dest, sv_struct_playerammo.counterpart);

    return true; // presumably
}

//
// SR_PlayerPutAmmo
//
void SR_PlayerPutAmmo(void *storage, int index, void *extra)
{
    playerammo_t *src = (playerammo_t *)storage + index;

    SV_SaveStruct(src, &sv_struct_playerammo);
}

//
// SR_PlayerGetWeapon
//
bool SR_PlayerGetWeapon(void *storage, int index, void *extra)
{
    PlayerWeapon *dest = (PlayerWeapon *)storage + index;

    if (sv_struct_playerweapon.counterpart)
        return SV_LoadStruct(dest, sv_struct_playerweapon.counterpart);

    return true; // presumably
}

//
// SR_PlayerPutWeapon
//
void SR_PlayerPutWeapon(void *storage, int index, void *extra)
{
    PlayerWeapon *src = (PlayerWeapon *)storage + index;

    SV_SaveStruct(src, &sv_struct_playerweapon);
}

//
// SR_PlayerGetPSprite
//
bool SR_PlayerGetPSprite(void *storage, int index, void *extra)
{
    PlayerSprite *dest = (PlayerSprite *)storage + index;

    //!!! FIXME: should skip if no counterpart
    if (sv_struct_psprite.counterpart)
        return SV_LoadStruct(dest, sv_struct_psprite.counterpart);

    return true; // presumably
}

//
// SR_PlayerPutPSprite
//
void SR_PlayerPutPSprite(void *storage, int index, void *extra)
{
    PlayerSprite *src = (PlayerSprite *)storage + index;

    SV_SaveStruct(src, &sv_struct_psprite);
}

//
// SR_PlayerGetName
//
bool SR_PlayerGetName(void *storage, int index, void *extra)
{
    char       *dest = (char *)storage;
    const char *str;

    SYS_ASSERT(index == 0);

    str = SV_GetString();
    epi::CStringCopyMax(dest, str, MAX_PLAYNAME - 1);
    SV_FreeString(str);

    return true;
}

//
// SR_PlayerPutName
//
void SR_PlayerPutName(void *storage, int index, void *extra)
{
    char *src = (char *)storage;

    SYS_ASSERT(index == 0);

    SV_PutString(src);
}

//
// SR_WeaponGetInfo
//
bool SR_WeaponGetInfo(void *storage, int index, void *extra)
{
    WeaponDefinition **dest = (WeaponDefinition **)storage + index;
    const char   *name;

    name = SV_GetString();

    *dest = name ? weapondefs.Lookup(name) : nullptr;
    SV_FreeString(name);

    return true;
}

//
// SR_WeaponPutInfo
//
void SR_WeaponPutInfo(void *storage, int index, void *extra)
{
    WeaponDefinition *info = ((WeaponDefinition **)storage)[index];

    SV_PutString(info ? info->name_.c_str() : nullptr);
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

    const char        *swizzle;
    const WeaponDefinition *actual;

    swizzle = SV_GetString();

    if (!swizzle)
    {
        *dest = nullptr;
        return true;
    }

    epi::CStringCopyMax(buffer, swizzle, 256 - 1);
    SV_FreeString(swizzle);

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
        SV_PutString(nullptr);
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

    SV_PutString(buf.c_str());
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
