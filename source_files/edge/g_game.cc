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

extern ConsoleVariable double_framerate;

GameState game_state = kGameStateNothing;

GameAction game_action = kGameActionNothing;

bool paused        = false;
bool pistol_starts = false;

int key_pause;

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

SkillLevel game_skill = kSkillMedium;

// -ACB- 2004/05/25 We need to store our current/next mapdefs
const MapDefinition *current_map = nullptr;
const MapDefinition *next_map    = nullptr;

int                  current_hub_tag = 0;  // affects where players are spawned
const MapDefinition *current_hub_first;    // first map in group of hubs

// -KM- 1998/12/16 These flags hold everything needed about a level
GameFlags level_flags;

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
static void RespawnPlayer(Player *p);
static void SpawnInitialPlayers(void);

static bool GameLoadGameFromFile(std::string filename, bool is_hub = false);
static bool GameSaveGameToFile(std::string filename, const char *description);

void LoadLevel_Bits(void)
{
    if (current_map == nullptr)
        FatalError("GameDoLoadLevel: No Current Map selected");

#ifdef EDGE_WEB
    PauseAudioDevice();
#endif

    // Set the sky map.
    //
    // First thing, we have a dummy sky texture name, a flat. The data is
    // in the WAD only because we look for an actual index, instead of simply
    // setting one.
    //
    // -ACB- 1998/08/09 Reference current map for sky name.

    sky_image =
        ImageLookup(current_map->sky_.c_str(), kImageNamespaceTexture);

    game_state = kGameStateNothing;  // FIXME: needed ???

    // -AJA- FIXME: this background camera stuff is a mess
    background_camera_map_object = nullptr;

    for (int pnum = 0; pnum < kMaximumPlayers; pnum++)
    {
        Player *p = players[pnum];
        if (!p) continue;

        if (p->player_state_ == kPlayerDead ||
            (current_map->force_on_ & kMapFlagResetPlayer) || pistol_starts)
        {
            p->player_state_ = kPlayerAwaitingRespawn;
        }

        p->frags_ = 0;
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
    HANDLE_FLAG(level_flags.mouselook, kMapFlagMlook);
    HANDLE_FLAG(level_flags.items_respawn, kMapFlagItemRespawn);
    HANDLE_FLAG(level_flags.fast_monsters, kMapFlagFastParm);
    HANDLE_FLAG(level_flags.true_3d_gameplay, kMapFlagTrue3D);
    HANDLE_FLAG(level_flags.more_blood, kMapFlagMoreBlood);
    HANDLE_FLAG(level_flags.cheats, kMapFlagCheats);
    HANDLE_FLAG(level_flags.enemies_respawn, kMapFlagRespawn);
    HANDLE_FLAG(level_flags.enemy_respawn_mode, kMapFlagResRespawn);
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
            level_flags.autoaim = kAutoAimMouselook;
        else
            level_flags.autoaim = kAutoAimOn;
    }
    else if (current_map->force_off_ & kMapFlagAutoAim)
        level_flags.autoaim = kAutoAimOff;

    //
    // Note: It should be noted that only the game_skill is
    // passed as the level is already defined in current_map,
    // The method for changing current_map, is using by
    // GameDeferredNewGame.
    //
    // -ACB- 1998/08/09 New LevelSetup
    // -KM- 1998/11/25 LevelSetup accepts the autotag
    //
    ClearScriptTriggers();
    ScriptMenuFinish(0);

    intermission_stats.kills       = intermission_stats.items =
        intermission_stats.secrets = 0;

    for (int pnum = 0; pnum < kMaximumPlayers; pnum++)
    {
        Player *p = players[pnum];
        if (!p) continue;

        p->kill_count_ = p->secret_count_ = p->item_count_ = 0;
        p->map_object_                                        = nullptr;
    }

    // Initial height of PointOfView will be set by player think.
    players[console_player]->view_z_ = kFloatUnused;

    level_time_elapsed = 0;

    LevelSetup();

    SpawnScriptTriggers(current_map->name_.c_str());

    exit_time     = INT_MAX;
    exit_skip_all = false;
    exit_hub_tag  = 0;

    BotBeginLevel();

    game_state = kGameStateLevel;

    ConsoleSetVisible(kConsoleVisibilityNotVisible);

    // clear cmd building stuff
    EventClearInput();

#ifdef EDGE_WEB
    ResumeAudioDevice();
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

    if (current_hub_tag == 0) SaveClearSlot("current");

    if (current_hub_tag > 0)
    {
        // HUB system: check for loading a previously visited map
        const char *mapname = SaveMapName(current_map);

        std::string fn(SaveFilename("current", mapname));

        if (epi::TestFileAccess(fn))
        {
            LogPrint("Loading HUB...\n");

            if (!GameLoadGameFromFile(fn, true))
                FatalError("LOAD-HUB failed with filename: %s\n", fn.c_str());

            SpawnInitialPlayers();

            // Need to investigate if CoalBeginLevel() needs to go here too now -
            // Dasho

            RemoveOldAvatars();

            HubFastForward();
            return;
        }
    }

    LoadLevel_Bits();

    SpawnInitialPlayers();
    if (LuaUseLuaHud())
        LuaBeginLevel();
    else
        CoalBeginLevel();
}

//
// GameResponder
//
// Get info needed to make ticcmd_ts for the players.
//
bool GameResponder(InputEvent *ev)
{
    // any other key pops up menu
    if (game_action == kGameActionNothing && (game_state == kGameStateTitleScreen))
    {
        if (ev->type == kInputEventKeyDown)
        {
            MenuStartControlPanel();
            StartSoundEffect(sound_effect_swtchn, kCategoryUi);
            return true;
        }

        return false;
    }

    if (ev->type == kInputEventKeyDown &&
        EventMatchesKey(key_show_players, ev->value.key.sym))
    {
        if (game_state == kGameStateLevel)  //!!!! && !InDeathmatch())
        {
            ToggleDisplayPlayer();
            return true;
        }
    }

    if (!network_game && ev->type == kInputEventKeyDown &&
        EventMatchesKey(key_pause, ev->value.key.sym))
    {
        paused = !paused;

        if (paused)
        {
            PauseMusic();
            PauseSound();
            GrabCursor(false);
        }
        else
        {
            ResumeMusic();
            ResumeSound();
            GrabCursor(true);
        }

        // explicit as probably killed the initial effect
        StartSoundEffect(sound_effect_swtchn, kCategoryUi);
        return true;
    }

    if (game_state == kGameStateLevel)
    {
        if (ScriptResponder(ev)) return true;  // RTS system ate it

        if (AutomapResponder(ev)) return true;  // automap ate it

        if (CheatResponder(ev)) return true;  // cheat code at it
    }

    if (game_state == kGameStateFinale)
    {
        if (FinaleResponder(ev)) return true;  // finale ate the event
    }

    return EventInputResponderResponder(ev);
}

static void CheckPlayersReborn(void)
{
    for (int pnum = 0; pnum < kMaximumPlayers; pnum++)
    {
        Player *p = players[pnum];

        if (!p || p->player_state_ != kPlayerAwaitingRespawn) continue;

        if (InSinglePlayerMatch())
        {
            // reload the level
            ForceWipe();
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

    if (extra_tic && double_framerate.d_)
    {
        switch (game_state)
        {
            case kGameStateLevel:
                // get commands
                NetworkGrabTicCommands();

                //!!!  MapObjectTicker();
                MapObjectTicker(true);
                break;

            case kGameStateIntermission:
            case kGameStateFinale:
                NetworkGrabTicCommands();
                break;
            default:
                break;
        }
        // ANIMATE FLATS AND TEXTURES GLOBALLY
        AnimationTicker();
        return;
    }

    // ANIMATE FLATS AND TEXTURES GLOBALLY
    AnimationTicker();

    // do main actions
    switch (game_state)
    {
        case kGameStateTitleScreen:
            TitleTicker();
            break;

        case kGameStateLevel:
            // get commands
            NetworkGrabTicCommands();

            MapObjectTicker(false);
            AutomapTicker();
            HudTicker();
            ScriptTicker();

            // do player reborns if needed
            CheckPlayersReborn();
            break;

        case kGameStateIntermission:
            NetworkGrabTicCommands();
            IntermissionTicker();
            break;

        case kGameStateFinale:
            NetworkGrabTicCommands();
            FinaleTicker();
            break;

        default:
            break;
    }
}

static void RespawnPlayer(Player *p)
{
    // first disassociate the corpse (if any)
    if (p->map_object_) p->map_object_->player_ = nullptr;

    p->map_object_ = nullptr;

    // spawn at random spot if in death match
    if (InDeathmatch())
        DeathMatchSpawnPlayer(p);
    else if (current_hub_tag > 0)
        GameHubSpawnPlayer(p, current_hub_tag);
    else
        CoopSpawnPlayer(p);  // respawn at the start
}

static void SpawnInitialPlayers(void)
{
    LogDebug("Deathmatch %d\n", deathmatch);

    // spawn the active players
    for (int pnum = 0; pnum < kMaximumPlayers; pnum++)
    {
        Player *p = players[pnum];
        if (p == nullptr)
        {
            // no real player, maybe spawn a helper dog?
            SpawnHelper(pnum);
            continue;
        }

        RespawnPlayer(p);

        if (!InDeathmatch()) SpawnVoodooDolls(p);
    }

    // check for missing player start.
    if (players[console_player]->map_object_ == nullptr)
        FatalError("Missing player start !\n");

    SetDisplayPlayer(console_player);  // view the guy you are playing
}

void GameDeferredScreenShot(void) { m_screenshot_required = true; }

// -KM- 1998/11/25 Added time param which is the time to wait before
//  actually exiting level.
void GameExitLevel(int time)
{
    next_map      = GameLookupMap(current_map->next_mapname_.c_str());
    exit_time     = level_time_elapsed + time;
    exit_skip_all = false;
    exit_hub_tag  = 0;
}

// -ACB- 1998/08/08 We don't have support for the german edition
//                  removed the check for map31.
void GameSecretExitLevel(int time)
{
    next_map      = GameLookupMap(current_map->secretmapname_.c_str());
    exit_time     = level_time_elapsed + time;
    exit_skip_all = false;
    exit_hub_tag  = 0;
}

void GameExitToLevel(char *name, int time, bool skip_all)
{
    next_map      = GameLookupMap(name);
    exit_time     = level_time_elapsed + time;
    exit_skip_all = skip_all;
    exit_hub_tag  = 0;
}

void GameExitToHub(const char *map_name, int tag)
{
    if (tag <= 0) FatalError("Hub exit line/command: bad tag %d\n", tag);

    next_map = GameLookupMap(map_name);
    if (!next_map) FatalError("GameExitToHub: No such map %s !\n", map_name);

    exit_time     = level_time_elapsed + 5;
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
//   (c) level_time_elapsed
//   (d) exit_skip_all
//   (d) exit_hub_tag
//   (e) intermission_stats.kills (etc)
//
static void GameDoCompleted(void)
{
    SYS_ASSERT(current_map);

    ForceWipe();

    exit_time = INT_MAX;

    for (int pnum = 0; pnum < kMaximumPlayers; pnum++)
    {
        Player *p = players[pnum];
        if (!p) continue;

        p->level_time_ = level_time_elapsed;

        // take away cards and stuff
        PlayerFinishLevel(p, exit_hub_tag > 0);
    }

    if (automap_active) AutomapStop();

    if (rts_menu_active) ScriptMenuFinish(0);

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
                MarkPlayerAvatars();

                const char *mapname = SaveMapName(current_map);

                std::string fn(SaveFilename("current", mapname));

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

    game_state = kGameStateIntermission;

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
    if (!SaveFileOpenRead(filename))
    {
        LogPrint("LOAD-GAME: cannot open %s\n", filename.c_str());
        return false;
    }

    int version;

    if (!SaveFileVerifyHeader(&version) || !SaveFileVerifyContents())
    {
        LogPrint("LOAD-GAME: Savegame is corrupt !\n");
        SaveFileCloseRead();
        return false;
    }

    BeginSaveGameLoad(is_hub);

    SaveGlobals *globs = SaveGlobalsLoad();

    if (!globs) FatalError("LOAD-GAME: Bad savegame file (no GLOB)\n");

    // --- pull info from global structure ---

    if (is_hub)
    {
        current_map = GameLookupMap(globs->level);
        if (!current_map)
            FatalError("LOAD-HUB: No such map %s !  Check WADS\n", globs->level);

        SetDisplayPlayer(console_player);
        automap_active = false;

        NetworkResetTics();
    }
    else
    {
        NewGameParameters params;

        params.map_ = GameLookupMap(globs->level);
        if (!params.map_)
            FatalError("LOAD-GAME: No such map %s !  Check WADS\n", globs->level);

        SYS_ASSERT(params.map_->episode_);

        params.skill_      = (SkillLevel)globs->skill;
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

    if (globs->mapsector.count != total_level_sectors ||
        globs->mapsector.crc != map_sectors_crc.GetCRC() ||
        globs->mapline.count != total_level_lines ||
        globs->mapline.crc != map_lines_crc.GetCRC() ||
        globs->mapthing.count != total_map_things ||
        globs->mapthing.crc != map_things_crc.GetCRC())
    {
        SaveFileCloseRead();

        FatalError("LOAD-GAME: Level data does not match !  Check WADs\n");
    }

    if (!is_hub)
    {
        level_time_elapsed = globs->level_time;
        exit_time = globs->exit_time;

        intermission_stats.kills   = globs->total_kills;
        intermission_stats.items   = globs->total_items;
        intermission_stats.secrets = globs->total_secrets;
    }

    if (globs->sky_image)  // backwards compat (sky_image added 2003/12/19)
        sky_image = globs->sky_image;

    // clear line/sector lookup caches
    DDF_BoomClearGenTypes();

    if (LoadAllSaveChunks() && SaveGetError() == 0)
    { /* all went well */
    }
    else
    {
        // something went horribly wrong...
        // FIXME (oneday) : show message & go back to title screen

        FatalError("Bad Save Game !\n");
    }

    SaveGlobalsFree(globs);

    FinishSaveGameLoad();
    SaveFileCloseRead();

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
    ForceWipe();

    const char *dir_name = SaveSlotName(defer_load_slot);
    LogDebug("GameDoLoadGame : %s\n", dir_name);

    SaveClearSlot("current");
    SaveCopySlot(dir_name, "current");

    std::string fn(SaveFilename("current", "head"));

    if (!GameLoadGameFromFile(fn))
    {
        // !!! FIXME: what to do?
    }

    HudStart();

    SetPalette(kPaletteNormal, 0);

    if (LuaUseLuaHud())
        LuaLoadGame();
    else
        CoalLoadGame();
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

    if (!SaveFileOpenWrite(filename, 0xEC))
    {
        LogPrint("Unable to create savegame file: %s\n", filename.c_str());
        return false; /* NOT REACHED */
    }

#ifdef EDGE_WEB
    PauseAudioDevice();
#endif

    SaveGlobals *globs = SaveGlobalsNew();

    // --- fill in global structure ---

    // globs->game  = SV_DupString(game_base.c_str());
    globs->game      = SaveChunkCopyString(current_map->episode_name_.c_str());
    globs->level     = SaveChunkCopyString(current_map->name_.c_str());
    globs->flags     = level_flags;
    globs->hub_tag   = current_hub_tag;
    globs->hub_first = current_hub_first
                           ? SaveChunkCopyString(current_hub_first->name_.c_str())
                           : nullptr;

    globs->skill    = game_skill;
    globs->netgame  = network_game ? (1 + deathmatch) : 0;
    globs->p_random = RandomStateRead();

    globs->console_player = console_player;  // NB: not used

    globs->level_time = level_time_elapsed;
    globs->exit_time  = exit_time;

    globs->total_kills   = intermission_stats.kills;
    globs->total_items   = intermission_stats.items;
    globs->total_secrets = intermission_stats.secrets;

    globs->sky_image = sky_image;

    time(&cur_time);
    strftime(timebuf, 99, "%H:%M  %Y-%m-%d", localtime(&cur_time));

    globs->description = SaveChunkCopyString(description);
    globs->desc_date   = SaveChunkCopyString(timebuf);

    globs->mapsector.count = total_level_sectors;
    globs->mapsector.crc   = map_sectors_crc.GetCRC();
    globs->mapline.count   = total_level_lines;
    globs->mapline.crc     = map_lines_crc.GetCRC();
    globs->mapthing.count  = total_map_things;
    globs->mapthing.crc    = map_things_crc.GetCRC();

    BeginSaveGameSave();

    SaveGlobalsSave(globs);
    SaveAllSaveChunks();

    SaveGlobalsFree(globs);

    FinishSaveGameSave();
    SaveFileCloseWrite();

    epi::SyncFilesystem();

#ifdef EDGE_WEB
    ResumeAudioDevice();
#endif

    return true;  // OK
}

static void GameDoSaveGame(void)
{
    if (LuaUseLuaHud())
        LuaSaveGame();
    else
        CoalSaveGame();

    std::string fn(SaveFilename("current", "head"));

    if (GameSaveGameToFile(fn, defer_save_description))
    {
        const char *dir_name = SaveSlotName(defer_save_slot);

        SaveClearSlot(dir_name);
        SaveCopySlot("current", dir_name);

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
    : skill_(kSkillMedium),
      deathmatch_(0),
      map_(nullptr),
      random_seed_(0),
      total_players_(0),
      flags_(nullptr)
{
    for (int i = 0; i < kMaximumPlayers; i++)
    {
        players_[i] = kPlayerFlagNoPlayer;
    }
}

NewGameParameters::NewGameParameters(const NewGameParameters &src)
{
    skill_      = src.skill_;
    deathmatch_ = src.deathmatch_;

    map_ = src.map_;

    random_seed_   = src.random_seed_;
    total_players_ = src.total_players_;

    for (int i = 0; i < kMaximumPlayers; i++)
    {
        players_[i] = src.players_[i];
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
    players_[0]    = kPlayerFlagNone;  // i.e. !BOT and !NETWORK

    for (int pnum = 1; pnum <= num_bots; pnum++)
    {
        players_[pnum] = kPlayerFlagBot;
    }
}

void NewGameParameters::CopyFlags(const GameFlags *F)
{
    if (flags_) delete flags_;

    flags_ = new GameFlags;

    memcpy(flags_, F, sizeof(GameFlags));
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
    return (CheckLumpNumberForName(map->lump_.c_str()) >= 0);
}

//
// REQUIRED STATE:
//   (a) defer_params
//
static void GameDoNewGame(void)
{
    SYS_ASSERT(defer_params);

    ForceWipe();

    SaveClearSlot("current");
    quicksave_slot = -1;

    InitNew(*defer_params);

    bool skip_pre = defer_params->level_skip_;

    delete defer_params;
    defer_params = nullptr;

    if (LuaUseLuaHud())
        LuaNewGame();
    else
        CoalNewGame();

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

    DestroyAllPlayers();

    for (int pnum = 0; pnum < kMaximumPlayers; pnum++)
    {
        if (params.players_[pnum] == kPlayerFlagNoPlayer) continue;

        CreatePlayer(pnum, (params.players_[pnum] & kPlayerFlagBot) ? true : false);

        if (console_player < 0 && !(params.players_[pnum] & kPlayerFlagBot) &&
            !(params.players_[pnum] & kPlayerFlagNetwork))
        {
            SetConsolePlayer(pnum);
        }
    }

    if (total_players != params.total_players_)
        FatalError("Internal Error: InitNew: player miscount (%d != %d)\n",
                total_players, params.total_players_);

    if (console_player < 0)
        FatalError("Internal Error: InitNew: no local players!\n");

    SetDisplayPlayer(console_player);

    if (paused)
    {
        paused = false;
        ResumeMusic();  // -ACB- 1999/10/07 New Music API
        ResumeSound();
    }

    current_map       = params.map_;
    current_hub_tag   = 0;
    current_hub_first = nullptr;

    if (params.skill_ > kSkillNightmare) params.skill_ = kSkillNightmare;

    RandomStateWrite(params.random_seed_);

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

    if (params.skill_ == kSkillNightmare)
    {
        level_flags.fast_monsters = true;
        level_flags.enemies_respawn  = true;
    }

    NetworkResetTics();
}

void GameDeferredEndGame(void)
{
    if (game_state == kGameStateLevel || game_state == kGameStateIntermission ||
        game_state == kGameStateFinale)
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
    ForceWipe();

    DestroyAllPlayers();

    SaveClearSlot("current");

    if (game_state == kGameStateLevel)
    {
        BotEndLevel();

        // FIXME: LevelShutdownLevel()
    }

    game_state = kGameStateNothing;

    SetPalette(kPaletteNormal, 0);

    StopMusic();

    PickLoadingScreen();

    StartTitle();
}

bool GameCheckWhenAppear(AppearsFlag appear)
{
    if (!(appear & (1 << game_skill))) return false;

    if (InSinglePlayerMatch() && !(appear & kAppearsWhenSingle)) return false;

    if (InCooperativeMatch() && !(appear & kAppearsWhenCoop)) return false;

    if (InDeathmatch() && !(appear & kAppearsWhenDeathMatch)) return false;

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
