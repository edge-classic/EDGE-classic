//----------------------------------------------------------------------------
//  EDGE Player Handling
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

#include "g_game.h"

#include <limits.h>
#include <string.h>
#include <time.h>

#include "am_map.h"
#include "bot_think.h"
#include "con_main.h"
#include "dstrings.h"
#include "dm_state.h"
#include "e_input.h"
#include "e_main.h"
#include "endianess.h"
#include "f_finale.h"
#include "f_interm.h"
#include "filesystem.h"
#include "i_system.h"
#include "m_cheat.h"
#include "m_menu.h"
#include "m_random.h"
#include "n_network.h"
#include "p_setup.h"
#include "p_tick.h"
#include "r_colormap.h"
#include "r_misc.h"
#include "r_modes.h"
#include "r_sky.h"
#include "rad_trig.h"
#include "s_music.h"
#include "s_sound.h"
#include "script/compat/lua_compat.h"
#include "str_compare.h"
#include "str_util.h"
#include "sv_chunk.h"
#include "sv_main.h"
#include "version.h"
#include "vm_coal.h"
#include "w_wad.h"

extern ConsoleVariable r_doubleframes;

game_state_e game_state = GS_NOTHING;

GameAction game_action = kGameActionNothing;

bool paused        = false;
bool pistol_starts = false;

int key_pause;

// for comparative timing purposes
bool nodrawers;
bool noblit;

// if true, load all graphics at start
bool precache = true;

// -KM- 1998/11/25 Exit time is the time when the level will actually finish
// after hitting the exit switch/killing the boss.  So that you see the
// switch change or the boss die.

int  exit_time     = INT_MAX;
bool exit_skip_all = false;  // -AJA- temporary (maybe become "exit_mode")
int  exit_hub_tag  = 0;

int key_show_players;

// GAMEPLAY MODES:
//
//   numplayers  deathmatch   mode
//   --------------------------------------
//     <= 1         0         single player
//     >  1         0         coop
//     -            1         deathmatch
//     -            2         altdeath

int deathmatch;

skill_t game_skill = sk_medium;

// -ACB- 2004/05/25 We need to store our current/next mapdefs
const MapDefinition *current_map = nullptr;
const MapDefinition *next_map    = nullptr;

int                  current_hub_tag = 0;  // affects where players are spawned
const MapDefinition *current_hub_first;    // first map in group of hubs

// -KM- 1998/12/16 These flags hold everything needed about a level
gameflags_t level_flags;

//--------------------------------------------

static int  defer_load_slot;
static int  defer_save_slot;
static char defer_save_description[32];

// deferred stuff...
static NewGameParameters *defer_params = nullptr;

static void GameDoNewGame(void);
static void GameDoLoadGame(void);
static void GameDoCompleted(void);
static void GameDoSaveGame(void);
static void GameDoEndGame(void);

static void InitNew(NewGameParameters &params);
static void RespawnPlayer(player_t *p);
static void SpawnInitialPlayers(void);

static bool GameLoadGameFromFile(std::string filename, bool is_hub = false);
static bool GameSaveGameToFile(std::string filename, const char *description);

void LoadLevel_Bits(void)
{
    if (current_map == nullptr)
        FatalError("GameDoLoadLevel: No Current Map selected");

#ifdef EDGE_WEB
    S_PauseAudioDevice();
#endif

    // Set the sky map.
    //
    // First thing, we have a dummy sky texture name, a flat. The data is
    // in the WAD only because we look for an actual index, instead of simply
    // setting one.
    //
    // -ACB- 1998/08/09 Reference current map for sky name.

    sky_image =
        W_ImageLookup(current_map->sky_.c_str(), kImageNamespaceTexture);

    game_state = GS_NOTHING;  // FIXME: needed ???

    // -AJA- FIXME: this background camera stuff is a mess
    background_camera_mo = nullptr;

    for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
    {
        player_t *p = players[pnum];
        if (!p) continue;

        if (p->playerstate == PST_DEAD ||
            (current_map->force_on_ & kMapFlagResetPlayer) || pistol_starts)
        {
            p->playerstate = PST_REBORN;
        }

        p->frags = 0;
    }

    // -KM- 1998/12/16 Make map flags actually do stuff.
    // -AJA- 2000/02/02: Made it more generic.

#define HANDLE_FLAG(var, specflag)                 \
    if (current_map->force_on_ & (specflag))       \
        (var) = true;                              \
    else if (current_map->force_off_ & (specflag)) \
        (var) = false;

    HANDLE_FLAG(level_flags.jump, kMapFlagJumping);
    HANDLE_FLAG(level_flags.crouch, kMapFlagCrouching);
    HANDLE_FLAG(level_flags.mlook, kMapFlagMlook);
    HANDLE_FLAG(level_flags.itemrespawn, kMapFlagItemRespawn);
    HANDLE_FLAG(level_flags.fastparm, kMapFlagFastParm);
    HANDLE_FLAG(level_flags.true3dgameplay, kMapFlagTrue3D);
    HANDLE_FLAG(level_flags.more_blood, kMapFlagMoreBlood);
    HANDLE_FLAG(level_flags.cheats, kMapFlagCheats);
    HANDLE_FLAG(level_flags.respawn, kMapFlagRespawn);
    HANDLE_FLAG(level_flags.res_respawn, kMapFlagResRespawn);
    HANDLE_FLAG(level_flags.have_extra, kMapFlagExtras);
    HANDLE_FLAG(level_flags.limit_zoom, kMapFlagLimitZoom);
    HANDLE_FLAG(level_flags.kicking, kMapFlagKicking);
    HANDLE_FLAG(level_flags.weapon_switch, kMapFlagWeaponSwitch);
    HANDLE_FLAG(level_flags.pass_missile, kMapFlagPassMissile);
    HANDLE_FLAG(level_flags.team_damage, kMapFlagTeamDamage);

#undef HANDLE_FLAG

    if (current_map->force_on_ & kMapFlagAutoAim)
    {
        if (current_map->force_on_ & kMapFlagAutoAimMlook)
            level_flags.autoaim = AA_MLOOK;
        else
            level_flags.autoaim = AA_ON;
    }
    else if (current_map->force_off_ & kMapFlagAutoAim)
        level_flags.autoaim = AA_OFF;

    //
    // Note: It should be noted that only the game_skill is
    // passed as the level is already defined in current_map,
    // The method for changing current_map, is using by
    // GameDeferredNewGame.
    //
    // -ACB- 1998/08/09 New P_SetupLevel
    // -KM- 1998/11/25 P_SetupLevel accepts the autotag
    //
    RAD_ClearTriggers();
    RAD_FinishMenu(0);

    intermission_stats.kills       = intermission_stats.items =
        intermission_stats.secrets = 0;

    for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
    {
        player_t *p = players[pnum];
        if (!p) continue;

        p->killcount = p->secretcount = p->itemcount = 0;
        p->mo                                        = nullptr;
    }

    // Initial height of PointOfView will be set by player think.
    players[consoleplayer]->viewz = kFloatUnused;

    leveltime = 0;

    P_SetupLevel();

    RAD_SpawnTriggers(current_map->name_.c_str());

    exit_time     = INT_MAX;
    exit_skip_all = false;
    exit_hub_tag  = 0;

    BotBeginLevel();

    game_state = GS_LEVEL;

    ConsoleSetVisible(kConsoleVisibilityNotVisible);

    // clear cmd building stuff
    EventClearInput();

#ifdef EDGE_WEB
    S_ResumeAudioDevice();
#endif

    paused = false;
}

//
// REQUIRED STATE:
//   (a) current_map
//   (b) current_hub_tag
//   (c) players[], numplayers (etc)
//   (d) game_skill + deathmatch
//   (e) level_flags
//
//   ??  exit_time
//
void GameDoLoadLevel(void)
{
    HudStart();

    if (current_hub_tag == 0) SV_ClearSlot("current");

    if (current_hub_tag > 0)
    {
        // HUB system: check for loading a previously visited map
        const char *mapname = SV_MapName(current_map);

        std::string fn(SV_FileName("current", mapname));

        if (epi::TestFileAccess(fn))
        {
            LogPrint("Loading HUB...\n");

            if (!GameLoadGameFromFile(fn, true))
                FatalError("LOAD-HUB failed with filename: %s\n", fn.c_str());

            SpawnInitialPlayers();

            // Need to investigate if VM_BeginLevel() needs to go here too now -
            // Dasho

            GameRemoveOldAvatars();

            P_HubFastForward();
            return;
        }
    }

    LoadLevel_Bits();

    SpawnInitialPlayers();
    if (LUA_UseLuaHud())
        LUA_BeginLevel();
    else
        VM_BeginLevel();
}

//
// GameResponder
//
// Get info needed to make ticcmd_ts for the players.
//
bool GameResponder(InputEvent *ev)
{
    // any other key pops up menu
    if (game_action == kGameActionNothing && (game_state == GS_TITLESCREEN))
    {
        if (ev->type == kInputEventKeyDown)
        {
            M_StartControlPanel();
            S_StartFX(sfx_swtchn, SNCAT_UI);
            return true;
        }

        return false;
    }

    if (ev->type == kInputEventKeyDown &&
        EventMatchesKey(key_show_players, ev->value.key.sym))
    {
        if (game_state == GS_LEVEL)  //!!!! && !DEATHMATCH())
        {
            GameToggleDisplayPlayer();
            return true;
        }
    }

    if (!netgame && ev->type == kInputEventKeyDown &&
        EventMatchesKey(key_pause, ev->value.key.sym))
    {
        paused = !paused;

        if (paused)
        {
            S_PauseMusic();
            S_PauseSound();
            GrabCursor(false);
        }
        else
        {
            S_ResumeMusic();
            S_ResumeSound();
            GrabCursor(true);
        }

        // explicit as probably killed the initial effect
        S_StartFX(sfx_swtchn, SNCAT_UI);
        return true;
    }

    if (game_state == GS_LEVEL)
    {
        if (RAD_Responder(ev)) return true;  // RTS system ate it

        if (AutomapResponder(ev)) return true;  // automap ate it

        if (CheatResponder(ev)) return true;  // cheat code at it
    }

    if (game_state == GS_FINALE)
    {
        if (FinaleResponder(ev)) return true;  // finale ate the event
    }

    return EventInputResponderResponder(ev);
}

static void CheckPlayersReborn(void)
{
    for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
    {
        player_t *p = players[pnum];

        if (!p || p->playerstate != PST_REBORN) continue;

        if (SP_MATCH())
        {
            // reload the level
            E_ForceWipe();
            game_action = kGameActionLoadLevel;

            // -AJA- if we are on a HUB map, then we must go all the
            //       way back to the beginning.
            if (current_hub_first)
            {
                current_map       = current_hub_first;
                current_hub_tag   = 0;
                current_hub_first = nullptr;
            }
            return;
        }

        RespawnPlayer(p);
    }
}

void GameBigStuff(void)
{
    // do things to change the game state
    while (game_action != kGameActionNothing)
    {
        GameAction action = game_action;
        game_action       = kGameActionNothing;

        switch (action)
        {
            case kGameActionNewGame:
                GameDoNewGame();
                break;

            case kGameActionLoadLevel:
                GameDoLoadLevel();
                break;

            case kGameActionLoadGame:
                GameDoLoadGame();
                break;

            case kGameActionSaveGame:
                GameDoSaveGame();
                break;

            case kGameActionIntermission:
                GameDoCompleted();
                break;

            case kGameActionFinale:
                SYS_ASSERT(next_map);
                current_map       = next_map;
                current_hub_tag   = 0;
                current_hub_first = nullptr;
                FinaleStart(&current_map->f_pre_, kGameActionLoadLevel);
                break;

            case kGameActionEndGame:
                GameDoEndGame();
                break;

            default:
                FatalError("GameBigStuff: Unknown game_action %d", game_action);
                break;
        }
    }
}

void GameTicker(void)
{
    bool extra_tic = (game_tic & 1) == 1;

    if (extra_tic && r_doubleframes.d_)
    {
        switch (game_state)
        {
            case GS_LEVEL:
                // get commands
                N_GrabTiccmds();

                //!!!  P_Ticker();
                P_Ticker(true);
                break;

            case GS_INTERMISSION:
            case GS_FINALE:
                N_GrabTiccmds();
                break;
            default:
                break;
        }
        // ANIMATE FLATS AND TEXTURES GLOBALLY
        W_UpdateImageAnims();
        return;
    }

    // ANIMATE FLATS AND TEXTURES GLOBALLY
    W_UpdateImageAnims();

    // do main actions
    switch (game_state)
    {
        case GS_TITLESCREEN:
            E_TitleTicker();
            break;

        case GS_LEVEL:
            // get commands
            N_GrabTiccmds();

            P_Ticker(false);
            AutomapTicker();
            HudTicker();
            RAD_Ticker();

            // do player reborns if needed
            CheckPlayersReborn();
            break;

        case GS_INTERMISSION:
            N_GrabTiccmds();
            IntermissionTicker();
            break;

        case GS_FINALE:
            N_GrabTiccmds();
            FinaleTicker();
            break;

        default:
            break;
    }
}

static void RespawnPlayer(player_t *p)
{
    // first disassociate the corpse (if any)
    if (p->mo) p->mo->player = nullptr;

    p->mo = nullptr;

    // spawn at random spot if in death match
    if (DEATHMATCH())
        GameDeathMatchSpawnPlayer(p);
    else if (current_hub_tag > 0)
        GameHubSpawnPlayer(p, current_hub_tag);
    else
        GameCoopSpawnPlayer(p);  // respawn at the start
}

static void SpawnInitialPlayers(void)
{
    LogDebug("Deathmatch %d\n", deathmatch);

    // spawn the active players
    for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
    {
        player_t *p = players[pnum];
        if (p == nullptr)
        {
            // no real player, maybe spawn a helper dog?
            GameSpawnHelper(pnum);
            continue;
        }

        RespawnPlayer(p);

        if (!DEATHMATCH()) GameSpawnVoodooDolls(p);
    }

    // check for missing player start.
    if (players[consoleplayer]->mo == nullptr)
        FatalError("Missing player start !\n");

    GameSetDisplayPlayer(consoleplayer);  // view the guy you are playing
}

void GameDeferredScreenShot(void) { m_screenshot_required = true; }

// -KM- 1998/11/25 Added time param which is the time to wait before
//  actually exiting level.
void GameExitLevel(int time)
{
    next_map      = GameLookupMap(current_map->next_mapname_.c_str());
    exit_time     = leveltime + time;
    exit_skip_all = false;
    exit_hub_tag  = 0;
}

// -ACB- 1998/08/08 We don't have support for the german edition
//                  removed the check for map31.
void GameSecretExitLevel(int time)
{
    next_map      = GameLookupMap(current_map->secretmapname_.c_str());
    exit_time     = leveltime + time;
    exit_skip_all = false;
    exit_hub_tag  = 0;
}

void GameExitToLevel(char *name, int time, bool skip_all)
{
    next_map      = GameLookupMap(name);
    exit_time     = leveltime + time;
    exit_skip_all = skip_all;
    exit_hub_tag  = 0;
}

void GameExitToHub(const char *map_name, int tag)
{
    if (tag <= 0) FatalError("Hub exit line/command: bad tag %d\n", tag);

    next_map = GameLookupMap(map_name);
    if (!next_map) FatalError("GameExitToHub: No such map %s !\n", map_name);

    exit_time     = leveltime + 5;
    exit_skip_all = true;
    exit_hub_tag  = tag;
}

void GameExitToHub(int map_number, int tag)
{
    SYS_ASSERT(current_map);

    char name_buf[32];

    // bit hackish: decided whether to use MAP## or E#M#
    if (current_map->name_[0] == 'E')
    {
        sprintf(name_buf, "E%dM%d", 1 + (map_number / 10), map_number % 10);
    }
    else
        sprintf(name_buf, "MAP%02d", map_number);

    GameExitToHub(name_buf, tag);
}

//
// REQUIRED STATE:
//   (a) current_map, next_map
//   (b) players[]
//   (c) leveltime
//   (d) exit_skip_all
//   (d) exit_hub_tag
//   (e) intermission_stats.kills (etc)
//
static void GameDoCompleted(void)
{
    SYS_ASSERT(current_map);

    E_ForceWipe();

    exit_time = INT_MAX;

    for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
    {
        player_t *p = players[pnum];
        if (!p) continue;

        p->leveltime = leveltime;

        // take away cards and stuff
        GamePlayerFinishLevel(p, exit_hub_tag > 0);
    }

    if (automap_active) AutomapStop();

    if (rts_menuactive) RAD_FinishMenu(0);

    BotEndLevel();

    automap_active = false;

    // handle "no stat" levels
    if (current_map->wistyle_ == kIntermissionStyleNone || exit_skip_all)
    {
        if (exit_skip_all && next_map)
        {
            if (exit_hub_tag <= 0)
                current_hub_first = nullptr;
            else
            {
                // save current map for HUB system
                LogPrint("Saving HUB...\n");

                // remember avatars of players, so we can remove them
                // when we return to this level.
                GameMarkPlayerAvatars();

                const char *mapname = SV_MapName(current_map);

                std::string fn(SV_FileName("current", mapname));

                if (!GameSaveGameToFile(fn, "__HUB_SAVE__"))
                    FatalError("SAVE-HUB failed with filename: %s\n", fn.c_str());

                if (!current_hub_first) current_hub_first = current_map;
            }

            current_map     = next_map;
            current_hub_tag = exit_hub_tag;

            game_action = kGameActionLoadLevel;
        }
        else
        {
            FinaleStart(&current_map->f_end_,
                        next_map ? kGameActionFinale : kGameActionNothing);
        }

        return;
    }

    intermission_stats.current_level = current_map;
    intermission_stats.next_level    = next_map;

    game_state = GS_INTERMISSION;

    IntermissionStart();
}

void GameDeferredLoadGame(int slot)
{
    // Can be called by the startup code or the menu task.

    defer_load_slot = slot;
    game_action     = kGameActionLoadGame;
}

static bool GameLoadGameFromFile(std::string filename, bool is_hub)
{
    if (!SV_OpenReadFile(filename))
    {
        LogPrint("LOAD-GAME: cannot open %s\n", filename.c_str());
        return false;
    }

    int version;

    if (!SV_VerifyHeader(&version) || !SV_VerifyContents())
    {
        LogPrint("LOAD-GAME: Savegame is corrupt !\n");
        SV_CloseReadFile();
        return false;
    }

    SV_BeginLoad(is_hub);

    saveglobals_t *globs = SV_LoadGLOB();

    if (!globs) FatalError("LOAD-GAME: Bad savegame file (no GLOB)\n");

    // --- pull info from global structure ---

    if (is_hub)
    {
        current_map = GameLookupMap(globs->level);
        if (!current_map)
            FatalError("LOAD-HUB: No such map %s !  Check WADS\n", globs->level);

        GameSetDisplayPlayer(consoleplayer);
        automap_active = false;

        N_ResetTics();
    }
    else
    {
        NewGameParameters params;

        params.map_ = GameLookupMap(globs->level);
        if (!params.map_)
            FatalError("LOAD-GAME: No such map %s !  Check WADS\n", globs->level);

        SYS_ASSERT(params.map_->episode_);

        params.skill_      = (skill_t)globs->skill;
        params.deathmatch_ = (globs->netgame >= 2) ? (globs->netgame - 1) : 0;

        params.random_seed_ = globs->p_random;

        // this player is a dummy one, replaced during actual load
        params.SinglePlayer(0);

        params.CopyFlags(&globs->flags);

        InitNew(params);

        current_hub_tag = globs->hub_tag;
        current_hub_first =
            globs->hub_first ? GameLookupMap(globs->hub_first) : nullptr;
    }

    LoadLevel_Bits();

    // -- Check LEVEL consistency (crc) --

    if (globs->mapsector.count != numsectors ||
        globs->mapsector.crc != mapsector_CRC.GetCRC() ||
        globs->mapline.count != numlines ||
        globs->mapline.crc != mapline_CRC.GetCRC() ||
        globs->mapthing.count != mapthing_NUM ||
        globs->mapthing.crc != mapthing_CRC.GetCRC())
    {
        SV_CloseReadFile();

        FatalError("LOAD-GAME: Level data does not match !  Check WADs\n");
    }

    if (!is_hub)
    {
        leveltime = globs->level_time;
        exit_time = globs->exit_time;

        intermission_stats.kills   = globs->total_kills;
        intermission_stats.items   = globs->total_items;
        intermission_stats.secrets = globs->total_secrets;
    }

    if (globs->sky_image)  // backwards compat (sky_image added 2003/12/19)
        sky_image = globs->sky_image;

    // clear line/sector lookup caches
    DDF_BoomClearGenTypes();

    if (SV_LoadEverything() && SV_GetError() == 0)
    { /* all went well */
    }
    else
    {
        // something went horribly wrong...
        // FIXME (oneday) : show message & go back to title screen

        FatalError("Bad Save Game !\n");
    }

    SV_FreeGLOB(globs);

    SV_FinishLoad();
    SV_CloseReadFile();

    return true;  // OK
}

//
// REQUIRED STATE:
//   (a) defer_load_slot
//
//   ?? nothing else ??
//
static void GameDoLoadGame(void)
{
    E_ForceWipe();

    const char *dir_name = SV_SlotName(defer_load_slot);
    LogDebug("GameDoLoadGame : %s\n", dir_name);

    SV_ClearSlot("current");
    SV_CopySlot(dir_name, "current");

    std::string fn(SV_FileName("current", "head"));

    if (!GameLoadGameFromFile(fn))
    {
        // !!! FIXME: what to do?
    }

    HudStart();

    V_SetPalette(PALETTE_NORMAL, 0);

    if (LUA_UseLuaHud())
        LUA_LoadGame();
    else
        VM_LoadGame();
}

//
// GameDeferredSaveGame
//
// Called by the menu task.
// Description is a 24 byte text string
//
void GameDeferredSaveGame(int slot, const char *description)
{
    defer_save_slot = slot;
    strcpy(defer_save_description, description);

    game_action = kGameActionSaveGame;
}

static bool GameSaveGameToFile(std::string filename, const char *description)
{
    time_t cur_time;
    char   timebuf[100];

    epi::FileDelete(filename);

    if (!SV_OpenWriteFile(filename, 0xEC))
    {
        LogPrint("Unable to create savegame file: %s\n", filename.c_str());
        return false; /* NOT REACHED */
    }

#ifdef EDGE_WEB
    S_PauseAudioDevice();
#endif

    saveglobals_t *globs = SV_NewGLOB();

    // --- fill in global structure ---

    // globs->game  = SV_DupString(game_base.c_str());
    globs->game      = SV_DupString(current_map->episode_name_.c_str());
    globs->level     = SV_DupString(current_map->name_.c_str());
    globs->flags     = level_flags;
    globs->hub_tag   = current_hub_tag;
    globs->hub_first = current_hub_first
                           ? SV_DupString(current_hub_first->name_.c_str())
                           : nullptr;

    globs->skill    = game_skill;
    globs->netgame  = netgame ? (1 + deathmatch) : 0;
    globs->p_random = P_ReadRandomState();

    globs->console_player = consoleplayer;  // NB: not used

    globs->level_time = leveltime;
    globs->exit_time  = exit_time;

    globs->total_kills   = intermission_stats.kills;
    globs->total_items   = intermission_stats.items;
    globs->total_secrets = intermission_stats.secrets;

    globs->sky_image = sky_image;

    time(&cur_time);
    strftime(timebuf, 99, "%H:%M  %Y-%m-%d", localtime(&cur_time));

    globs->description = SV_DupString(description);
    globs->desc_date   = SV_DupString(timebuf);

    globs->mapsector.count = numsectors;
    globs->mapsector.crc   = mapsector_CRC.GetCRC();
    globs->mapline.count   = numlines;
    globs->mapline.crc     = mapline_CRC.GetCRC();
    globs->mapthing.count  = mapthing_NUM;
    globs->mapthing.crc    = mapthing_CRC.GetCRC();

    SV_BeginSave();

    SV_SaveGLOB(globs);
    SV_SaveEverything();

    SV_FreeGLOB(globs);

    SV_FinishSave();
    SV_CloseWriteFile();

    epi::SyncFilesystem();

#ifdef EDGE_WEB
    S_ResumeAudioDevice();
#endif

    return true;  // OK
}

static void GameDoSaveGame(void)
{
    if (LUA_UseLuaHud())
        LUA_SaveGame();
    else
        VM_SaveGame();

    std::string fn(SV_FileName("current", "head"));

    if (GameSaveGameToFile(fn, defer_save_description))
    {
        const char *dir_name = SV_SlotName(defer_save_slot);

        SV_ClearSlot(dir_name);
        SV_CopySlot("current", dir_name);

        ConsolePrint("%s", language["GameSaved"]);
    }
    else
    {
        // !!! FIXME: what to do?
    }

    defer_save_description[0] = 0;
}

//
// GameInitNew Stuff
//
// Can be called by the startup code or the menu task.
// consoleplayer, displayplayer, players[] are setup.
//

//---> newgame_params_c class

NewGameParameters::NewGameParameters()
    : skill_(sk_medium),
      deathmatch_(0),
      map_(nullptr),
      random_seed_(0),
      total_players_(0),
      flags_(nullptr)
{
    for (int i = 0; i < MAXPLAYERS; i++)
    {
        players_[i] = PFL_NOPLAYER;
        nodes_[i]   = nullptr;
    }
}

NewGameParameters::NewGameParameters(const NewGameParameters &src)
{
    skill_      = src.skill_;
    deathmatch_ = src.deathmatch_;

    map_ = src.map_;

    random_seed_   = src.random_seed_;
    total_players_ = src.total_players_;

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        players_[i] = src.players_[i];
        nodes_[i]   = src.nodes_[i];
    }

    flags_ = nullptr;

    if (src.flags_) CopyFlags(src.flags_);
}

NewGameParameters::~NewGameParameters()
{
    if (flags_) delete flags_;
}

void NewGameParameters::SinglePlayer(int num_bots)
{
    total_players_ = 1 + num_bots;
    players_[0]    = PFL_Zero;  // i.e. !BOT and !NETWORK
    nodes_[0]      = nullptr;

    for (int pnum = 1; pnum <= num_bots; pnum++)
    {
        players_[pnum] = PFL_Bot;
        nodes_[pnum]   = nullptr;
    }
}

void NewGameParameters::CopyFlags(const gameflags_t *F)
{
    if (flags_) delete flags_;

    flags_ = new gameflags_t;

    memcpy(flags_, F, sizeof(gameflags_t));
}

//
// This is the procedure that changes the current_map
// at the start of the game and outside the normal
// progression of the game. All thats needed is the
// skill and the name (The name in the DDF File itself).
//
void GameDeferredNewGame(NewGameParameters &params)
{
    SYS_ASSERT(params.map_);

    defer_params = new NewGameParameters(params);

    if (params.level_skip_) defer_params->level_skip_ = true;

    game_action = kGameActionNewGame;
}

bool GameMapExists(const MapDefinition *map)
{
    return (W_CheckNumForName(map->lump_.c_str()) >= 0);
}

//
// REQUIRED STATE:
//   (a) defer_params
//
static void GameDoNewGame(void)
{
    SYS_ASSERT(defer_params);

    E_ForceWipe();

    SV_ClearSlot("current");
    quickSaveSlot = -1;

    InitNew(*defer_params);

    bool skip_pre = defer_params->level_skip_;

    delete defer_params;
    defer_params = nullptr;

    if (LUA_UseLuaHud())
        LUA_NewGame();
    else
        VM_NewGame();

    // -AJA- 2003/10/09: support for pre-level briefing screen on first map.
    //       FIXME: kludgy. All this game logic desperately needs rethinking.
    if (skip_pre)
        game_action = kGameActionLoadLevel;
    else
        FinaleStart(&current_map->f_pre_, kGameActionLoadLevel);
}

//
// InitNew
//
// -ACB- 1998/07/12 Removed Lost Soul/Spectre Ability stuff
// -ACB- 1998/08/10 Inits new game without the need for gamemap or episode.
// -ACB- 1998/09/06 Removed remarked code.
// -KM- 1998/12/21 Added mapdef param so no need for defered init new
//   which was conflicting with net games.
//
// REQUIRED STATE:
//   ?? nothing ??
//
static void InitNew(NewGameParameters &params)
{
    // --- create players ---

    P_DestroyAllPlayers();

    for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
    {
        if (params.players_[pnum] == PFL_NOPLAYER) continue;

        P_CreatePlayer(pnum, (params.players_[pnum] & PFL_Bot) ? true : false);

        if (consoleplayer < 0 && !(params.players_[pnum] & PFL_Bot) &&
            !(params.players_[pnum] & PFL_Network))
        {
            GameSetConsolePlayer(pnum);
        }

        players[pnum]->node = params.nodes_[pnum];
    }

    if (numplayers != params.total_players_)
        FatalError("Internal Error: InitNew: player miscount (%d != %d)\n",
                numplayers, params.total_players_);

    if (consoleplayer < 0)
        FatalError("Internal Error: InitNew: no local players!\n");

    GameSetDisplayPlayer(consoleplayer);

    if (paused)
    {
        paused = false;
        S_ResumeMusic();  // -ACB- 1999/10/07 New Music API
        S_ResumeSound();
    }

    current_map       = params.map_;
    current_hub_tag   = 0;
    current_hub_first = nullptr;

    if (params.skill_ > sk_nightmare) params.skill_ = sk_nightmare;

    P_WriteRandomState(params.random_seed_);

    automap_active = false;

    game_skill = params.skill_;
    deathmatch = params.deathmatch_;

    // LogDebug("GameInitNew: Deathmatch %d Skill %d\n", params.deathmatch,
    // (int)params.skill);

    // copy global flags into the level-specific flags
    if (params.flags_)
        level_flags = *params.flags_;
    else
        level_flags = global_flags;

    if (params.skill_ == sk_nightmare)
    {
        level_flags.fastparm = true;
        level_flags.respawn  = true;
    }

    N_ResetTics();
}

void GameDeferredEndGame(void)
{
    if (game_state == GS_LEVEL || game_state == GS_INTERMISSION ||
        game_state == GS_FINALE)
    {
        game_action = kGameActionEndGame;
    }
}

//
// REQUIRED STATE:
//    ?? nothing ??
//
static void GameDoEndGame(void)
{
    E_ForceWipe();

    P_DestroyAllPlayers();

    SV_ClearSlot("current");

    if (game_state == GS_LEVEL)
    {
        BotEndLevel();

        // FIXME: P_ShutdownLevel()
    }

    game_state = GS_NOTHING;

    V_SetPalette(PALETTE_NORMAL, 0);

    S_StopMusic();

    E_PickLoadingScreen();

    E_StartTitle();
}

bool GameCheckWhenAppear(AppearsFlag appear)
{
    if (!(appear & (1 << game_skill))) return false;

    if (SP_MATCH() && !(appear & kAppearsWhenSingle)) return false;

    if (COOP_MATCH() && !(appear & kAppearsWhenCoop)) return false;

    if (DEATHMATCH() && !(appear & kAppearsWhenDeathMatch)) return false;

    return true;
}

MapDefinition *GameLookupMap(const char *refname)
{
    MapDefinition *m = mapdefs.Lookup(refname);

    if (m && GameMapExists(m)) return m;

    // -AJA- handle numbers (like original DOOM)
    if (strlen(refname) <= 2 && epi::IsDigitASCII(refname[0]) &&
        (!refname[1] || epi::IsDigitASCII(refname[1])))
    {
        int num = atoi(refname);
        // first try map names ending in ## (single digit treated as 0#)
        std::string map_check = epi::StringFormat("%02d", num);
        for (int i = mapdefs.size() - 1; i >= 0; i--)
        {
            if (mapdefs[i]->name_.size() >= 2)
            {
                if (epi::StringCaseCompareASCII(
                        map_check, mapdefs[i]->name_.substr(
                                       mapdefs[i]->name_.size() - 2)) == 0 &&
                    GameMapExists(mapdefs[i]) && mapdefs[i]->episode_)
                    return mapdefs[i];
            }
        }

        // otherwise try X#X# (episodic) style names
        if (1 <= num && num <= 9) num = num + 10;
        map_check = epi::StringFormat("E%dM%d", num / 10, num % 10);
        for (int i = mapdefs.size() - 1; i >= 0; i--)
        {
            if (mapdefs[i]->name_.size() == 4)
            {
                if (mapdefs[i]->name_[1] == map_check[1] &&
                    mapdefs[i]->name_[3] == map_check[3] &&
                    GameMapExists(mapdefs[i]) && mapdefs[i]->episode_)
                    return mapdefs[i];
            }
        }
    }

    return nullptr;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
