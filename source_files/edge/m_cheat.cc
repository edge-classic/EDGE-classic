//----------------------------------------------------------------------------
//  EDGE Cheat Sequence Checking
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
// -KM- 1998/07/21 Moved the cheat sequence here from st_stuff.c
//                 ST_Responder in st_stuff.c calls cht_Responder to check for
//                 cheat codes.  Also added NO_NIGHTMARE_CHEATS #define.
//                 if defined, there can be no cheating in nightmare. :-)
//                 Made all the cheat codes non global.
//
// -ACB- 1998/07/30 Naming Convention stuff, all procedures m_*.*.
//                  Added Touching Mobj "Cheat".
//
// -ACB- 1999/09/19 CD Audio cheats removed.
//

#include "m_cheat.h"

#include "con_main.h"
#include "ddf_main.h"
#include "dm_state.h"
#include "dstrings.h"
#include "epi.h"
#include "g_game.h"
#include "i_system.h"
#include "m_menu.h"
#include "p_local.h"
#include "p_mobj.h"
#include "s_music.h"
#include "s_sound.h"
#include "stb_sprintf.h"
#include "w_wad.h"

extern ConsoleVariable debug_fps;
extern ConsoleVariable debug_position;
extern ConsoleVariable fliplevels;

static CheatSequence cheat_powerup[9] = {
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0}, // -MH- 1998/06/17  added "give jetpack"
            // cheat
    {0, 0}  // -ACB- 1998/07/15  added "give nightvision" cheat
};

static CheatSequence cheat_music           = {0, 0};
static CheatSequence cheat_my_position     = {0, 0};
static CheatSequence cheat_show_stats      = {0, 0};
static CheatSequence cheat_choppers        = {0, 0};
static CheatSequence cheat_change_level    = {0, 0};
static CheatSequence cheat_kill_all        = {0, 0};
static CheatSequence cheat_suicide         = {0, 0};
static CheatSequence cheat_loaded          = {0, 0};
static CheatSequence cheat_take_all        = {0, 0};
static CheatSequence cheat_god             = {0, 0};
static CheatSequence cheat_ammo            = {0, 0};
static CheatSequence cheat_ammo_no_keys    = {0, 0};
static CheatSequence cheat_keys            = {0, 0};
static CheatSequence cheat_no_clipping     = {0, 0};
static CheatSequence cheat_no_clipping2    = {0, 0};
static CheatSequence cheat_hall_of_mirrors = {0, 0};
static CheatSequence cheat_flip_level      = {0, 0};

static CheatSequence cheat_give_weapon[11] = {
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
};

//
// CheckCheatSequence
//
// Called in CheatResponder module, which handles the input.
// Returns a 1 if the cheat was successful, 0 if failed.
//
int CheckCheatSequence(CheatSequence *cht, char key)
{
    int rc = 0;

    if (!cht->p)
        cht->p = cht->sequence; // initialise if first time

    if ((unsigned char)key == (unsigned char)*cht->p)
        cht->p++;
    else
        cht->p = cht->sequence;

    if (*cht->p == 0)
    { // end of sequence character

        cht->p = cht->sequence;
        rc     = 1;
    }

    return rc;
}

void M_ChangeLevelCheat(const char *string)
{
    // User pressed <ESC>
    if (!string)
        return;

    // NOTE WELL: following assumes single player

    NewGameParameters params;

    params.skill_      = game_skill;
    params.deathmatch_ = deathmatch;

    params.map_ = LookupMap(string);
    if (!params.map_)
    {
        ConsoleMessage(kConsoleHUDCenter, "%s", language["ImpossibleChange"]);
        return;
    }

    EPI_ASSERT(MapExists(params.map_));
    EPI_ASSERT(params.map_->episode_);

    params.random_seed_ = PureRandomNumber();

    params.SinglePlayer(total_bots);

    params.level_skip_ = true;

    DeferredNewGame(params);

    ConsoleMessage(kConsoleHUDCenter, "%s", language["LevelChange"]);
}

//
// M_ChangeMusicCheat
//
static void M_ChangeMusicCheat(const char *string)
{
    int entry_num;

    // User pressed <ESC>
    if (!string)
        return;

    entry_num = atoi(string);

    if (!entry_num)
        return;

    ChangeMusic(entry_num, true);
    ConsoleMessage(kConsoleHUDCenter, "%s", language["MusChange"]);
}

static void CheatGiveWeapons(Player *pl, int key = -2)
{
    for (auto info : weapondefs)
    {
        if (info && !info->no_cheat_ && (key < 0 || info->bind_key_ == key))
        {
            AddWeapon(pl, info, nullptr);
        }
    }

    if (key < 0)
    {
        for (int slot = 0; slot < kMaximumWeapons; slot++)
        {
            if (pl->weapons_[slot].info)
                FillWeapon(pl, slot);
        }
    }

    UpdateAvailWeapons(pl);
}

bool CheatResponder(InputEvent *ev)
{
#ifdef NOCHEATS
    return false;
#endif

    int     i;
    Player *pl = players[console_player];

    // disable cheats while in RTS menu
    if (rts_menu_active)
        return false;

    // if a user keypress...
    if (ev->type != kInputEventKeyDown)
        return false;

    char key = (char)ev->value.key.sym;

    // no cheating in bot deathmatch or if disallowed in levels.ddf
    if (!level_flags.cheats || deathmatch)
        return false;

    // 'dqd' cheat for toggleable god mode
    if (CheckCheatSequence(&cheat_god, key))
    {
        pl->cheats_ ^= kCheatingGodMode;
        if (pl->cheats_ & kCheatingGodMode)
        {
            if (pl->map_object_)
            {
                pl->health_ = pl->map_object_->health_ = pl->map_object_->spawn_health_;
            }
            ConsoleMessage(kConsoleHUDCenter, "%s", language["GodModeOn"]);
        }
        else
            ConsoleMessage(kConsoleHUDCenter, "%s", language["GodModeOff"]);
    }

    // 'fa' cheat for killer fucking arsenal
    //
    // -ACB- 1998/06/26 removed backpack from this as backpack is variable
    //
    else if (CheckCheatSequence(&cheat_ammo_no_keys, key))
    {
        // Dasho - Original code made blue armor additive to lesser armour (green),
        // instead of replacing it. This should be a bit more robust and also
        // account for the best armor (including EDGE-specific types) that the player
        // already had
        if (pl->armours_[kArmourTypeRed] > 0)
        {
            pl->armours_[kArmourTypeRed]    = kMaximumArmor;
            pl->armours_[kArmourTypeYellow] = 0;
            pl->armours_[kArmourTypePurple] = 0;
            pl->armours_[kArmourTypeBlue]   = 0;
            pl->armours_[kArmourTypeGreen]  = 0;
        }
        else if (pl->armours_[kArmourTypeYellow] > 0)
        {
            pl->armours_[kArmourTypeYellow] = kMaximumArmor;
            pl->armours_[kArmourTypePurple] = 0;
            pl->armours_[kArmourTypeBlue]   = 0;
            pl->armours_[kArmourTypeGreen]  = 0;
        }
        else if (pl->armours_[kArmourTypePurple] > 0)
        {
            pl->armours_[kArmourTypePurple] = kMaximumArmor;
            pl->armours_[kArmourTypeBlue]   = 0;
            pl->armours_[kArmourTypeGreen]  = 0;
        }
        else
        {
            pl->armours_[kArmourTypeBlue]  = kMaximumArmor;
            pl->armours_[kArmourTypeGreen] = 0;
        }

        UpdateTotalArmour(pl);

        for (i = 0; i < kTotalAmmunitionTypes; i++)
            pl->ammo_[i].count = pl->ammo_[i].maximum;

        CheatGiveWeapons(pl);

        ConsoleMessage(kConsoleHUDCenter, "%s", language["AmmoAdded"]);
    }

    // 'kfa' cheat for key full ammo
    //
    // -ACB- 1998/06/26 removed backpack from this as backpack is variable
    //
    else if (CheckCheatSequence(&cheat_ammo, key))
    {
        // Dasho - Original code made blue armor additive to lesser armour (green),
        // instead of replacing it. This should be a bit more robust and also
        // account for the best armor (including EDGE-specific types) that the player
        // already had
        if (pl->armours_[kArmourTypeRed] > 0)
        {
            pl->armours_[kArmourTypeRed]    = kMaximumArmor;
            pl->armours_[kArmourTypeYellow] = 0;
            pl->armours_[kArmourTypePurple] = 0;
            pl->armours_[kArmourTypeBlue]   = 0;
            pl->armours_[kArmourTypeGreen]  = 0;
        }
        else if (pl->armours_[kArmourTypeYellow] > 0)
        {
            pl->armours_[kArmourTypeYellow] = kMaximumArmor;
            pl->armours_[kArmourTypePurple] = 0;
            pl->armours_[kArmourTypeBlue]   = 0;
            pl->armours_[kArmourTypeGreen]  = 0;
        }
        else if (pl->armours_[kArmourTypePurple] > 0)
        {
            pl->armours_[kArmourTypePurple] = kMaximumArmor;
            pl->armours_[kArmourTypeBlue]   = 0;
            pl->armours_[kArmourTypeGreen]  = 0;
        }
        else
        {
            pl->armours_[kArmourTypeBlue]  = kMaximumArmor;
            pl->armours_[kArmourTypeGreen] = 0;
        }

        UpdateTotalArmour(pl);

        for (i = 0; i < kTotalAmmunitionTypes; i++)
            pl->ammo_[i].count = pl->ammo_[i].maximum;

        pl->cards_ = kDoorKeyBitmask;

        CheatGiveWeapons(pl);

        ConsoleMessage(kConsoleHUDCenter, "%s", language["VeryHappyAmmo"]);
    }
    else if (CheckCheatSequence(&cheat_keys, key))
    {
        pl->cards_ = kDoorKeyBitmask;

        ConsoleMessage(kConsoleHUDCenter, "%s", language["UnlockCheat"]);
    }
    else if (CheckCheatSequence(&cheat_loaded, key))
    {
        for (i = 0; i < kTotalAmmunitionTypes; i++)
            pl->ammo_[i].count = pl->ammo_[i].maximum;

        ConsoleMessage(kConsoleHUDCenter, "%s", language["LoadedCheat"]);
    }
    else if (CheckCheatSequence(&cheat_take_all, key))
    {
        for (WeaponDefinition *weap : weapondefs)
        {
            RemoveWeapon(pl, weap);
        }
        GiveInitialBenefits(pl, pl->map_object_->info_);
        pl->ready_weapon_ = KWeaponSelectionNone;
        SelectNewWeapon(pl, -100, kAmmunitionTypeDontCare);
        ConsoleMessage(kConsoleHUDCenter, "%s", language["StuffRemoval"]);
    }
    else if (CheckCheatSequence(&cheat_suicide, key))
    {
        TelefragMapObject(pl->map_object_, pl->map_object_, nullptr);

        // -ACB- 1998/08/26 Suicide language reference
        ConsoleMessage(kConsoleHUDCenter, "%s", language["SuicideCheat"]);
    }
    // -ACB- 1998/08/27 Used Mobj linked-list code, much cleaner.
    else if (CheckCheatSequence(&cheat_kill_all, key))
    {
        int killcount = 0;

        MapObject *mo;
        MapObject *next;

        for (mo = map_object_list_head; mo; mo = next)
        {
            next = mo->next_;

            if ((mo->extended_flags_ & kExtendedFlagMonster) && (mo->health_ > 0))
            {
                TelefragMapObject(mo, nullptr, nullptr);
                killcount++;
            }
        }

        ConsoleMessage(kConsoleHUDCenter, "%d %s", killcount, language["MonstersKilled"]);
    }
    // Simplified, accepting both "noclip" and "idspispopd".
    // no clipping mode cheat
    else if (CheckCheatSequence(&cheat_no_clipping, key) || CheckCheatSequence(&cheat_no_clipping2, key))
    {
        pl->cheats_ ^= kCheatingNoClip;

        if (pl->cheats_ & kCheatingNoClip)
            ConsoleMessage(kConsoleHUDCenter, "%s", language["ClipOn"]);
        else
            ConsoleMessage(kConsoleHUDCenter, "%s", language["ClipOff"]);
    }
    else if (CheckCheatSequence(&cheat_hall_of_mirrors, key))
    {
        debug_hall_of_mirrors = debug_hall_of_mirrors.d_ ? 0 : 1;

        if (debug_hall_of_mirrors.d_)
            ConsoleMessage(kConsoleHUDCenter, "%s", language["HomDetectOn"]);
        else
            ConsoleMessage(kConsoleHUDCenter, "%s", language["HomDetectOff"]);
    }

    // 'behold?' power-up cheats
    for (i = 0; i < 9; i++)
    {
        if (CheckCheatSequence(&cheat_powerup[i], key))
        {
            if (!pl->powers_[i])
                pl->powers_[i] = 60 * kTicRate;
            else
                pl->powers_[i] = 0;

            if (i == kPowerTypeBerserk)
                pl->keep_powers_ |= (1 << kPowerTypeBerserk);

            ConsoleMessage(kConsoleHUDCenter, "%s", language["BeholdUsed"]);
        }
    }

    // 'give#' power-up cheats
    for (i = 0; i < 10; i++)
    {
        if (!CheckCheatSequence(&cheat_give_weapon[i + 1], key))
            continue;

        CheatGiveWeapons(pl, i);
    }

    // 'choppers' invulnerability & chainsaw
    if (CheckCheatSequence(&cheat_choppers, key))
    {
        WeaponDefinition *w = weapondefs.Lookup("CHAINSAW");
        if (w)
        {
            AddWeapon(pl, w, nullptr);
            pl->powers_[kPowerTypeInvulnerable] = 1;
            ConsoleMessage(kConsoleHUDCenter, "%s", language["CHOPPERSNote"]);
        }
    }

    // 'mypos' for player position
    else if (CheckCheatSequence(&cheat_my_position, key))
    {
        if (debug_position.d_ <= 0)
            debug_position = 1;
        else
            debug_position = 0;
    }

    if (CheckCheatSequence(&cheat_change_level, key))
    {
        // 'clev' change-level cheat
        StartMenuMessageInput(language["LevelQ"], M_ChangeLevelCheat);
    }
    else if (CheckCheatSequence(&cheat_music, key))
    {
        // 'mus' cheat for changing music
        StartMenuMessageInput(language["MusicQ"], M_ChangeMusicCheat);
    }
    else if (CheckCheatSequence(&cheat_flip_level, key))
    {
        if (fliplevels.d_)
            fliplevels = 0;
        else
            fliplevels = 1;
    }
    else if (CheckCheatSequence(&cheat_show_stats, key))
    {
        debug_fps      = debug_fps.d_ ? 0 : 1;
        debug_position = debug_fps.d_;
    }

    return false;
}

// -KM- 1999/01/31 Loads cheats from languages file.
// -ES- 1999/08/26 Removed M_ConvertCheat stuff, the cheat terminator is
//      now just 0.
void CheatInitialize(void)
{
    int  i;
    char temp[16];

    // Now what?
    cheat_music.sequence           = language["idmus"];
    cheat_god.sequence             = language["iddqd"];
    cheat_ammo.sequence            = language["idkfa"];
    cheat_ammo_no_keys.sequence    = language["idfa"];
    cheat_no_clipping.sequence     = language["idspispopd"];
    cheat_no_clipping2.sequence    = language["idclip"];
    cheat_hall_of_mirrors.sequence = language["idhom"];

    for (i = 0; i < 9; i++)
    {
        stbsp_sprintf(temp, "idbehold%d", i + 1);
        cheat_powerup[i].sequence = language[temp];
    }

    cheat_choppers.sequence     = language["idchoppers"];
    cheat_change_level.sequence = language["idclev"];
    cheat_my_position.sequence  = language["idmypos"];

    // new cheats
    cheat_kill_all.sequence   = language["idkillall"];
    cheat_show_stats.sequence = language["idinfo"];
    cheat_suicide.sequence    = language["idsuicide"];
    cheat_keys.sequence       = language["idunlock"];
    cheat_loaded.sequence     = language["idloaded"];
    cheat_take_all.sequence   = language["idtakeall"];
    cheat_flip_level.sequence = language["flipme"];

    for (i = 0; i < 11; i++)
    {
        stbsp_sprintf(temp, "idgive%d", i);
        cheat_give_weapon[i].sequence = language[temp];
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
