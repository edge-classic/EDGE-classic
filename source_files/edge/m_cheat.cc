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
#include "dm_state.h"
#include "dstrings.h"
#include "g_game.h"
#include "i_system.h"
#include "m_menu.h"
#include "main.h"
#include "p_local.h"
#include "p_mobj.h"
#include "s_music.h"
#include "s_sound.h"
#include "w_wad.h"

extern ConsoleVariable debug_fps;
extern ConsoleVariable debug_position;

static CheatSequence cheat_powerup[9] = {
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
    {0, 0}, {0, 0}, {0, 0},  // -MH- 1998/06/17  added "give jetpack" cheat
    {0, 0}                   // -ACB- 1998/07/15  added "give nightvision" cheat
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

static CheatSequence cheat_give_weapon[11] = {
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
};

//
// CheatCheckSequence
//
// Called in CheatResponder module, which handles the input.
// Returns a 1 if the cheat was successful, 0 if failed.
//
int CheatCheckSequence(CheatSequence *cht, char key)
{
    int rc = 0;

    if (!cht->p) cht->p = cht->sequence;  // initialise if first time

    if ((unsigned char)key == (unsigned char)*cht->p)
        cht->p++;
    else
        cht->p = cht->sequence;

    if (*cht->p == 0)
    {  // end of sequence character

        cht->p = cht->sequence;
        rc     = 1;
    }

    return rc;
}

void M_ChangeLevelCheat(const char *string)
{
    // User pressed <ESC>
    if (!string) return;

    // NOTE WELL: following assumes single player

    NewGameParameters params;

    params.skill_      = game_skill;
    params.deathmatch_ = deathmatch;

    params.map_ = GameLookupMap(string);
    if (!params.map_)
    {
        ConsoleMessageLDF("ImpossibleChange");
        return;
    }

    SYS_ASSERT(GameMapExists(params.map_));
    SYS_ASSERT(params.map_->episode_);

    params.random_seed_ = PureRandomNumber();

    params.SinglePlayer(numbots);

    params.level_skip_ = true;

    GameDeferredNewGame(params);

    ConsoleMessageLDF("LevelChange");
}

//
// M_ChangeMusicCheat
//
static void M_ChangeMusicCheat(const char *string)
{
    int entry_num;

    // User pressed <ESC>
    if (!string) return;

    entry_num = atoi(string);

    if (!entry_num) return;

    S_ChangeMusic(entry_num, true);
    ConsoleMessageLDF("MusChange");
}

static void CheatGiveWeapons(player_t *pl, int key = -2)
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
            if (pl->weapons[slot].info) FillWeapon(pl, slot);
        }
    }

    UpdateAvailWeapons(pl);
}

bool CheatResponder(InputEvent *ev)
{
#ifdef NOCHEATS
    return false;
#endif

    int       i;
    player_t *pl = players[consoleplayer];

    // disable cheats while in RTS menu
    if (rts_menu_active) return false;

    // if a user keypress...
    if (ev->type != kInputEventKeyDown) return false;

    char key = (char)ev->value.key.sym;

    // no cheating in bot deathmatch or if disallowed in levels.ddf
    if (!level_flags.cheats || deathmatch) return false;

    // 'dqd' cheat for toggleable god mode
    if (CheatCheckSequence(&cheat_god, key))
    {
        pl->cheats ^= CF_GODMODE;
        if (pl->cheats & CF_GODMODE)
        {
            if (pl->mo) { pl->health = pl->mo->health_ = pl->mo->spawn_health_; }
            ConsoleMessageLDF("GodModeOn");
        }
        else
            ConsoleMessageLDF("GodModeOff");
    }

    // 'fa' cheat for killer fucking arsenal
    //
    // -ACB- 1998/06/26 removed backpack from this as backpack is variable
    //
    else if (CheatCheckSequence(&cheat_ammo_no_keys, key))
    {
        pl->armours[CHEATARMOURTYPE] = CHEATARMOUR;

        UpdateTotalArmour(pl);

        for (i = 0; i < kTotalAmmunitionTypes; i++)
            pl->ammo[i].num = pl->ammo[i].max;

        CheatGiveWeapons(pl);

        ConsoleMessageLDF("AmmoAdded");
    }

    // 'kfa' cheat for key full ammo
    //
    // -ACB- 1998/06/26 removed backpack from this as backpack is variable
    //
    else if (CheatCheckSequence(&cheat_ammo, key))
    {
        pl->armours[CHEATARMOURTYPE] = CHEATARMOUR;

        UpdateTotalArmour(pl);

        for (i = 0; i < kTotalAmmunitionTypes; i++)
            pl->ammo[i].num = pl->ammo[i].max;

        pl->cards = kDoorKeyBitmask;

        CheatGiveWeapons(pl);

        ConsoleMessageLDF("VeryHappyAmmo");
    }
    else if (CheatCheckSequence(&cheat_keys, key))
    {
        pl->cards = kDoorKeyBitmask;

        ConsoleMessageLDF("UnlockCheat");
    }
    else if (CheatCheckSequence(&cheat_loaded, key))
    {
        for (i = 0; i < kTotalAmmunitionTypes; i++)
            pl->ammo[i].num = pl->ammo[i].max;

        ConsoleMessageLDF("LoadedCheat");
    }
#if 0  // FIXME: this crashes ?
	else if (CheatCheckSequence(&cheat_take_all, key))
	{
		P_GiveInitialBenefits(pl, pl->mo->info_);

		// -ACB- 1998/08/26 Stuff removed language reference
		ConsoleMessageLDF("StuffRemoval");
	}
#endif
    else if (CheatCheckSequence(&cheat_suicide, key))
    {
        TelefragMapObject(pl->mo, pl->mo, nullptr);

        // -ACB- 1998/08/26 Suicide language reference
        ConsoleMessageLDF("SuicideCheat");
    }
    // -ACB- 1998/08/27 Used Mobj linked-list code, much cleaner.
    else if (CheatCheckSequence(&cheat_kill_all, key))
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

        ConsoleMessageLDF("MonstersKilled", killcount);
    }
    // Simplified, accepting both "noclip" and "idspispopd".
    // no clipping mode cheat
    else if (CheatCheckSequence(&cheat_no_clipping, key) ||
             CheatCheckSequence(&cheat_no_clipping2, key))
    {
        pl->cheats ^= CF_NOCLIP;

        if (pl->cheats & CF_NOCLIP)
            ConsoleMessageLDF("ClipOn");
        else
            ConsoleMessageLDF("ClipOff");
    }
    else if (CheatCheckSequence(&cheat_hall_of_mirrors, key))
    {
        debug_hall_of_mirrors = debug_hall_of_mirrors.d_ ? 0 : 1;

        if (debug_hall_of_mirrors.d_)
            ConsoleMessageLDF("HomDetectOn");
        else
            ConsoleMessageLDF("HomDetectOff");
    }

    // 'behold?' power-up cheats
    for (i = 0; i < 9; i++)
    {
        if (CheatCheckSequence(&cheat_powerup[i], key))
        {
            if (!pl->powers[i])
                pl->powers[i] = 60 * kTicRate;
            else
                pl->powers[i] = 0;

            if (i == kPowerTypeBerserk)
                pl->keep_powers |= (1 << kPowerTypeBerserk);

            ConsoleMessageLDF("BeholdUsed");
        }
    }

    // 'give#' power-up cheats
    for (i = 0; i < 10; i++)
    {
        if (!CheatCheckSequence(&cheat_give_weapon[i + 1], key)) continue;

        CheatGiveWeapons(pl, i);
    }

    // 'choppers' invulnerability & chainsaw
    if (CheatCheckSequence(&cheat_choppers, key))
    {
        WeaponDefinition *w = weapondefs.Lookup("CHAINSAW");
        if (w)
        {
            AddWeapon(pl, w, nullptr);
            pl->powers[kPowerTypeInvulnerable] = 1;
            ConsoleMessageLDF("CHOPPERSNote");
        }
    }

    // 'mypos' for player position
    else if (CheatCheckSequence(&cheat_my_position, key))
    {
        ConsoleMessage("ang=%f;x,y=(%f,%f)", epi::DegreesFromBAM(pl->mo->angle_),
                       pl->mo->x, pl->mo->y);
    }

    if (CheatCheckSequence(&cheat_change_level, key))
    {
        // 'clev' change-level cheat
        MenuStartMessageInput(language["LevelQ"], M_ChangeLevelCheat);
    }
    else if (CheatCheckSequence(&cheat_music, key))
    {
        // 'mus' cheat for changing music
        MenuStartMessageInput(language["MusicQ"], M_ChangeMusicCheat);
    }
    else if (CheatCheckSequence(&cheat_show_stats, key))
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
        sprintf(temp, "idbehold%d", i + 1);
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

    for (i = 0; i < 11; i++)
    {
        sprintf(temp, "idgive%d", i);
        cheat_give_weapon[i].sequence = language[temp];
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
