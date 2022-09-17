//----------------------------------------------------------------------------
//  EDGE Player Handling
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2009  The EDGE Team.
// 
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
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

#include "i_defs.h"

#include <time.h>
#include <limits.h>

#include "endianess.h"
#include "path.h"
#include "str_format.h"
#include "filesystem.h"

#include "con_main.h"
#include "dstrings.h"
#include "e_input.h"
#include "e_main.h"
#include "f_finale.h"
#include "g_game.h"
#include "m_cheat.h"
#include "m_menu.h"
#include "m_random.h"
#include "n_network.h"
#include "p_bot.h"
#include "p_setup.h"
#include "p_tick.h"
#include "rad_trig.h"
#include "am_map.h"
#include "r_sky.h"
#include "r_modes.h"
#include "s_sound.h"
#include "s_music.h"
#include "sv_chunk.h"
#include "sv_main.h"
#include "r_colormap.h"
#include "version.h"
#include "w_wad.h"
#include "f_interm.h"
#include "vm_coal.h"


gamestate_e gamestate = GS_NOTHING;

gameaction_e gameaction = ga_nothing;

bool paused = false;
bool pistol_starts = false;

int key_pause;

// for comparative timing purposes 
bool nodrawers;
bool noblit;

// if true, load all graphics at start 
bool precache = true;

int starttime;

// -KM- 1998/11/25 Exit time is the time when the level will actually finish
// after hitting the exit switch/killing the boss.  So that you see the
// switch change or the boss die.

int exittime = INT_MAX;
bool exit_skipall = false;  // -AJA- temporary (maybe become "exit_mode")
int exit_hub_tag  = 0;

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

skill_t gameskill = sk_medium;

// -ACB- 2004/05/25 We need to store our current/next mapdefs
const mapdef_c *currmap = NULL;
const mapdef_c *nextmap = NULL;

int curr_hub_tag = 0;  // affects where players are spawned
const mapdef_c *curr_hub_first;  // first map in group of hubs

// -KM- 1998/12/16 These flags hold everything needed about a level
gameflags_t level_flags;


//--------------------------------------------

static int defer_load_slot;
static int defer_save_slot;
static char defer_save_desc[32];

// deferred stuff...
static newgame_params_c *defer_params = NULL;


static void G_DoNewGame(void);
static void G_DoLoadGame(void);
static void G_DoCompleted(void);
static void G_DoSaveGame(void);
static void G_DoEndGame(void);

static void InitNew(newgame_params_c& params);
static void RespawnPlayer(player_t *p);
static void SpawnInitialPlayers(void);

static bool G_LoadGameFromFile(const char *filename, bool is_hub = false);
static bool G_SaveGameToFile(const char *filename, const char *description);


void LoadLevel_Bits(void)
{
	if (currmap == NULL)
		I_Error("G_DoLoadLevel: No Current Map selected");

	// Set the sky map.
	//
	// First thing, we have a dummy sky texture name, a flat. The data is
	// in the WAD only because we look for an actual index, instead of simply
	// setting one.
	//
	// -ACB- 1998/08/09 Reference current map for sky name.

	sky_image = W_ImageLookup(currmap->sky.c_str(), INS_Texture);

	gamestate = GS_NOTHING; //FIXME: needed ???

	// -AJA- FIXME: this background camera stuff is a mess
	background_camera_mo = NULL;

	for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
	{
		player_t *p = players[pnum];
		if (! p) continue;

		if (p->playerstate == PST_DEAD ||
			(currmap->force_on & MPF_ResetPlayer) || pistol_starts)
		{
			p->playerstate = PST_REBORN;
		}

		p->frags = 0;
	}

	// -KM- 1998/12/16 Make map flags actually do stuff.
	// -AJA- 2000/02/02: Made it more generic.

#define HANDLE_FLAG(var, specflag)  \
	if (currmap->force_on & (specflag))  \
	(var) = true;  \
	else if (currmap->force_off & (specflag))  \
	(var) = false;

	HANDLE_FLAG(level_flags.jump, MPF_Jumping);
	HANDLE_FLAG(level_flags.crouch, MPF_Crouching);
	HANDLE_FLAG(level_flags.mlook, MPF_Mlook);
	HANDLE_FLAG(level_flags.itemrespawn, MPF_ItemRespawn);
	HANDLE_FLAG(level_flags.fastparm, MPF_FastParm);
	HANDLE_FLAG(level_flags.true3dgameplay, MPF_True3D);
	HANDLE_FLAG(level_flags.more_blood, MPF_MoreBlood);
	HANDLE_FLAG(level_flags.cheats, MPF_Cheats);
	HANDLE_FLAG(level_flags.respawn, MPF_Respawn);
	HANDLE_FLAG(level_flags.res_respawn, MPF_ResRespawn);
	HANDLE_FLAG(level_flags.have_extra, MPF_Extras);
	HANDLE_FLAG(level_flags.limit_zoom, MPF_LimitZoom);
	HANDLE_FLAG(level_flags.kicking, MPF_Kicking);
	HANDLE_FLAG(level_flags.weapon_switch, MPF_WeaponSwitch);
	HANDLE_FLAG(level_flags.pass_missile, MPF_PassMissile);
	HANDLE_FLAG(level_flags.team_damage, MPF_TeamDamage);

#undef HANDLE_FLAG

	if (currmap->force_on & MPF_BoomCompat)
		level_flags.edge_compat = false;
	else if (currmap->force_off & MPF_BoomCompat)
		level_flags.edge_compat = true;

	if (currmap->force_on & MPF_AutoAim)
	{
		if (currmap->force_on & MPF_AutoAimMlook)
			level_flags.autoaim = AA_MLOOK;
		else
			level_flags.autoaim = AA_ON;
	}
	else if (currmap->force_off & MPF_AutoAim)
		level_flags.autoaim = AA_OFF;

	//
	// Note: It should be noted that only the gameskill is
	// passed as the level is already defined in currmap,
	// The method for changing currmap, is using by
	// G_DeferredNewGame.
	//
	// -ACB- 1998/08/09 New P_SetupLevel
	// -KM- 1998/11/25 P_SetupLevel accepts the autotag
	//
	RAD_ClearTriggers();
	RAD_FinishMenu(0);

	wi_stats.kills = wi_stats.items = wi_stats.secret = 0;

	for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
	{
		player_t *p = players[pnum];
		if (! p) continue;

		p->killcount = p->secretcount = p->itemcount = 0;
		p->mo = NULL;
	}

	// Initial height of PointOfView will be set by player think.
	players[consoleplayer]->viewz = FLO_UNUSED;

	leveltime = 0;

	P_SetupLevel();

	RAD_SpawnTriggers(currmap->name.c_str());

	starttime = I_GetTime();
	exittime = INT_MAX;
	exit_skipall = false;
	exit_hub_tag = 0;

	BOT_BeginLevel();

	gamestate = GS_LEVEL;

	CON_SetVisible(vs_notvisible);

	// clear cmd building stuff
	E_ClearInput();

	paused = false;
}

//
// REQUIRED STATE:
//   (a) currmap
//   (b) curr_hub_tag
//   (c) players[], numplayers (etc)
//   (d) gameskill + deathmatch
//   (e) level_flags
//
//   ??  exittime
//
void G_DoLoadLevel(void)
{
	HU_Start();

	if (curr_hub_tag == 0)
		SV_ClearSlot("current");

	if (curr_hub_tag > 0)
	{
		// HUB system: check for loading a previously visited map
		const char *mapname = SV_MapName(currmap);

		std::string fn(SV_FileName("current", mapname));

		if (epi::FS_Access(fn.c_str(), epi::file_c::ACCESS_READ))
		{
			I_Printf("Loading HUB...\n");

			if (! G_LoadGameFromFile(fn.c_str(), true))
				I_Error("LOAD-HUB failed with filename: %s\n", fn.c_str());

			SpawnInitialPlayers();

			// Need to investigate if VM_BeginLevel() needs to go here too now - Dasho

			G_RemoveOldAvatars();

			P_HubFastForward();
			return;
		}
	}

	LoadLevel_Bits();

	SpawnInitialPlayers();

	VM_BeginLevel();
}


//
// G_Responder  
//
// Get info needed to make ticcmd_ts for the players.
// 
bool G_Responder(event_t * ev)
{
	// any other key pops up menu
	if (gameaction == ga_nothing && (gamestate == GS_TITLESCREEN))
	{
		if (ev->type == ev_keydown)
		{
			M_StartControlPanel();
			S_StartFX(sfx_swtchn, SNCAT_UI);
			return true;
		}

		return false;
	}

	if (ev->type == ev_keydown && E_MatchesKey(key_show_players, ev->value.key.sym))
	{
		if (gamestate == GS_LEVEL) //!!!! && !DEATHMATCH())
		{
			G_ToggleDisplayPlayer();
			return true;
		}
	}

	if (!netgame && ev->type == ev_keydown && E_MatchesKey(key_pause, ev->value.key.sym))
	{
		paused = !paused;

		if (paused)
		{
			S_PauseMusic();
			S_PauseSound();
			I_GrabCursor(false);
		}
		else
		{
			S_ResumeMusic();
			S_ResumeSound();
			I_GrabCursor(true);
		}

		// explicit as probably killed the initial effect
		S_StartFX(sfx_swtchn, SNCAT_UI);
		return true;
	}

	if (gamestate == GS_LEVEL)
	{
		if (RAD_Responder(ev))
			return true;  // RTS system ate it

		if (AM_Responder(ev))
			return true;  // automap ate it 

		if (HU_Responder(ev))
			return true;  // chat ate the event

		if (M_CheatResponder(ev))
			return true;  // cheat code at it
	}

	if (gamestate == GS_FINALE)
	{
		if (F_Responder(ev))
			return true;  // finale ate the event 
	}

	return INP_Responder(ev);
}


static void CheckPlayersReborn(void)
{
	for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
	{
		player_t *p = players[pnum];

		if (!p || p->playerstate != PST_REBORN)
			continue;

		if (SP_MATCH())
		{
			// reload the level
			E_ForceWipe();
			gameaction = ga_loadlevel;

			// -AJA- if we are on a HUB map, then we must go all the
			//       way back to the beginning.
			if (curr_hub_first)
			{
				currmap = curr_hub_first;
				curr_hub_tag = 0;
				curr_hub_first = NULL;
			}
			return;
		}

		RespawnPlayer(p);
	}
}


void G_BigStuff(void)
{
	// do things to change the game state
	while (gameaction != ga_nothing)
	{
		gameaction_e action = gameaction;
		gameaction = ga_nothing;

		switch (action)
		{
			case ga_newgame:
				G_DoNewGame();
				break;

			case ga_loadlevel:
				G_DoLoadLevel();
				break;

			case ga_loadgame:
				G_DoLoadGame();
				break;

			case ga_savegame:
				G_DoSaveGame();
				break;

			case ga_intermission:
				G_DoCompleted();
				break;

			case ga_finale:
				SYS_ASSERT(nextmap);
				currmap = nextmap;
				curr_hub_tag = 0;
				curr_hub_first = NULL;
				F_StartFinale(&currmap->f_pre, ga_loadlevel);
				break;

			case ga_endgame:
				G_DoEndGame();
				break;

			default:
				I_Error("G_BigStuff: Unknown gameaction %d", gameaction);
				break;
		}
	}
}

void G_Ticker(void)
{
	// ANIMATE FLATS AND TEXTURES GLOBALLY
	W_UpdateImageAnims();

	// do main actions
	switch (gamestate)
	{
		case GS_TITLESCREEN:
			E_TitleTicker();
			break;

		case GS_LEVEL:
			// get commands, check consistency,
			// and build new consistency check.
			N_TiccmdTicker();

			P_Ticker();
			AM_Ticker();
			HU_Ticker();
			RAD_Ticker();

			// do player reborns if needed
			CheckPlayersReborn();
			break;

		case GS_INTERMISSION:
			N_TiccmdTicker();
			WI_Ticker();
			break;

		case GS_FINALE:
			N_TiccmdTicker();
			F_Ticker();
			break;

		default:
			break;
	}
}


static void RespawnPlayer(player_t *p)
{
	// first disassociate the corpse (if any)
	if (p->mo)
		p->mo->player = NULL;
	
	p->mo = NULL;

	// spawn at random spot if in death match 
	if (DEATHMATCH())
		G_DeathMatchSpawnPlayer(p);
	else if (curr_hub_tag > 0)
		G_HubSpawnPlayer(p, curr_hub_tag);
	else
		G_CoopSpawnPlayer(p); // respawn at the start
}

static void SpawnInitialPlayers(void)
{
	L_WriteDebug("Deathmatch %d\n", deathmatch);

	// spawn the active players
	for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
	{
		player_t *p = players[pnum];
		if (! p) continue;

		RespawnPlayer(p);

		if (!DEATHMATCH())
			G_SpawnVoodooDolls(p);
	}

	// check for missing player start.
	if (players[consoleplayer]->mo == NULL)
		I_Error("Missing player start !\n");

	G_SetDisplayPlayer(consoleplayer); // view the guy you are playing
}

void G_DeferredScreenShot(void)
{
	m_screenshot_required = true;
}

// -KM- 1998/11/25 Added time param which is the time to wait before
//  actually exiting level.
void G_ExitLevel(int time)
{
	nextmap = G_LookupMap(currmap->nextmapname.c_str());
	exittime = leveltime + time;
	exit_skipall = false;
	exit_hub_tag = 0;
}

// -ACB- 1998/08/08 We don't have support for the german edition
//                  removed the check for map31.
void G_SecretExitLevel(int time)
{
	nextmap = G_LookupMap(currmap->secretmapname.c_str());
	exittime = leveltime + time;
	exit_skipall = false;
	exit_hub_tag = 0;
}

void G_ExitToLevel(char *name, int time, bool skip_all)
{
	nextmap = G_LookupMap(name);
	exittime = leveltime + time;
	exit_skipall = skip_all;
	exit_hub_tag = 0;
}

void G_ExitToHub(const char *map_name, int tag)
{
	if (tag <= 0)
		I_Error("Hub exit line/command: bad tag %d\n", tag);

	nextmap = G_LookupMap(map_name);
	if (! nextmap)
		I_Error("G_ExitToHub: No such map %s !\n", map_name);

	exittime = leveltime + 5;
	exit_skipall = true;
	exit_hub_tag = tag;
}

void G_ExitToHub(int map_number, int tag)
{
	SYS_ASSERT(currmap);

	char name_buf[32];

	// bit hackish: decided whether to use MAP## or E#M#
	if (currmap->name[0] == 'E')
	{
		sprintf(name_buf, "E%dM%d", 1+(map_number/10), map_number%10);
	}
	else
		sprintf(name_buf, "MAP%02d", map_number);

	G_ExitToHub(name_buf, tag);
}

// 
// REQUIRED STATE:
//   (a) currmap, nextmap
//   (b) players[]
//   (c) leveltime
//   (d) exit_skipall
//   (d) exit_hub_tag
//   (e) wi_stats.kills (etc)
// 
static void G_DoCompleted(void)
{
	SYS_ASSERT(currmap);

	E_ForceWipe();

	exittime = INT_MAX;

	for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
	{
		player_t *p = players[pnum];
		if (! p) continue;

		p->leveltime = leveltime;

		// take away cards and stuff
		G_PlayerFinishLevel(p, exit_hub_tag > 0);
	}

	if (automapactive)
		AM_Stop();

	if (rts_menuactive)
		RAD_FinishMenu(0);

	BOT_EndLevel();

	automapactive = false;

	// handle "no stat" levels
	if (currmap->wistyle == WISTYLE_None || exit_skipall)
	{
		if (exit_skipall && nextmap)
		{
			if (exit_hub_tag <= 0)
				curr_hub_first = NULL;
			else
			{
				// save current map for HUB system
				I_Printf("Saving HUB...\n");

				// remember avatars of players, so we can remove them
				// when we return to this level.
				G_MarkPlayerAvatars();

				const char *mapname = SV_MapName(currmap);

				std::string fn(SV_FileName("current", mapname));

				if (! G_SaveGameToFile(fn.c_str(), "__HUB_SAVE__"))
					I_Error("SAVE-HUB failed with filename: %s\n", fn.c_str());

				if (! curr_hub_first)
					curr_hub_first = currmap;
			}

			currmap = nextmap;
			curr_hub_tag = exit_hub_tag;
			
			gameaction = ga_loadlevel;
		}
		else
		{
			F_StartFinale(&currmap->f_end, nextmap ? ga_finale : ga_nothing);
		}

		return;
	}

	wi_stats.cur  = currmap;
	wi_stats.next = nextmap;

	gamestate = GS_INTERMISSION;

	WI_Start();
}


void G_DeferredLoadGame(int slot)
{
	// Can be called by the startup code or the menu task. 

	defer_load_slot = slot;
	gameaction = ga_loadgame;
}


static bool G_LoadGameFromFile(const char *filename, bool is_hub)
{
	if (! SV_OpenReadFile(filename))
	{
		I_Printf("LOAD-GAME: cannot open %s\n", filename);
		return false;
	}
	
	int version;

	if (! SV_VerifyHeader(&version) || ! SV_VerifyContents())
	{
		I_Printf("LOAD-GAME: Savegame is corrupt !\n");
		SV_CloseReadFile();
		return false;
	}

	SV_BeginLoad(is_hub);

	saveglobals_t *globs = SV_LoadGLOB();

	if (!globs)
		I_Error("LOAD-GAME: Bad savegame file (no GLOB)\n");

	// --- pull info from global structure ---

	if (is_hub)
	{
		currmap = G_LookupMap(globs->level);
		if (! currmap)
			I_Error("LOAD-HUB: No such map %s !  Check WADS\n", globs->level);

		G_SetDisplayPlayer(consoleplayer);
		automapactive = false;

		N_ResetTics();
	}
	else
	{
		newgame_params_c params;

		params.map = G_LookupMap(globs->level);
		if (! params.map)
			I_Error("LOAD-GAME: No such map %s !  Check WADS\n", globs->level);

		SYS_ASSERT(params.map->episode);

		params.skill      = (skill_t) globs->skill;
		params.deathmatch = (globs->netgame >= 2) ? (globs->netgame - 1) : 0;

		params.random_seed = globs->p_random;

		// this player is a dummy one, replaced during actual load
		params.SinglePlayer(0);

		params.CopyFlags(&globs->flags);
		
		InitNew(params);

		curr_hub_tag = globs->hub_tag;
		curr_hub_first = globs->hub_first ? G_LookupMap(globs->hub_first) : NULL;
	}

	LoadLevel_Bits();

	// -- Check LEVEL consistency (crc) --
	//
	// FIXME: ideally we shouldn't bomb out, just display an error box

	if (globs->mapsector.count != numsectors ||
		globs->mapsector.crc != mapsector_CRC.crc ||
		globs->mapline.count != numlines ||
		globs->mapline.crc != mapline_CRC.crc ||
		globs->mapthing.count != mapthing_NUM ||
		globs->mapthing.crc != mapthing_CRC.crc)
	{
		SV_CloseReadFile();

		I_Error("LOAD-GAME: Level data does not match !  Check WADs\n");
	}

	if (! is_hub)
	{
		leveltime = globs->level_time;
		exittime  = globs->exit_time;

		wi_stats.kills  = globs->total_kills;
		wi_stats.items  = globs->total_items;
		wi_stats.secret = globs->total_secrets;
	}

	if (globs->sky_image)  // backwards compat (sky_image added 2003/12/19)
		sky_image = globs->sky_image;

	// clear line/sector lookup caches, in case level_flags.edge_compat
	// has changed.
	DDF_BoomClearGenTypes();

	if (SV_LoadEverything() && SV_GetError() == 0)
	{
		/* all went well */ 
	}
	else
	{
		// something went horribly wrong...
		// FIXME (oneday) : show message & go back to title screen

		I_Error("Bad Save Game !\n");
	}

	SV_FreeGLOB(globs);

	SV_FinishLoad();
	SV_CloseReadFile();

	return true; //OK
}

//
// REQUIRED STATE:
//   (a) defer_load_slot
//
//   ?? nothing else ??
//
static void G_DoLoadGame(void)
{
	E_ForceWipe();

	const char *dir_name = SV_SlotName(defer_load_slot);
	I_Debugf("G_DoLoadGame : %s\n", dir_name);

	SV_ClearSlot("current");
	SV_CopySlot(dir_name, "current");

	std::string fn(SV_FileName("current", "head"));

	if (! G_LoadGameFromFile(fn.c_str()))
	{
		// !!! FIXME: what to do?
	}

	HU_Start();

	V_SetPalette(PALETTE_NORMAL, 0);

	VM_LoadGame(); // Stub for now
}

//
// G_DeferredSaveGame
//
// Called by the menu task.
// Description is a 24 byte text string 
//
void G_DeferredSaveGame(int slot, const char *description)
{
	defer_save_slot = slot;
	strcpy(defer_save_desc, description);

	gameaction = ga_savegame;
}

static bool G_SaveGameToFile(const char *filename, const char *description)
{
	time_t cur_time;
	char timebuf[100];

	epi::FS_Delete(filename);

	if (! SV_OpenWriteFile(filename, (EDGEVERHEX << 8) | EDGEPATCH))
	{
		I_Printf("Unable to create savegame file: %s\n", filename);
		return false; /* NOT REACHED */
	}

	saveglobals_t *globs = SV_NewGLOB();

	// --- fill in global structure ---

	globs->game  = SV_DupString(currmap->episode_name.c_str());
	globs->level = SV_DupString(currmap->name.c_str());
	globs->flags = level_flags;
	globs->hub_tag = curr_hub_tag;
	globs->hub_first = curr_hub_first ? SV_DupString(curr_hub_first->name.c_str()) : NULL;

	globs->skill = gameskill;
	globs->netgame = netgame ? (1+deathmatch) : 0;
	globs->p_random = P_ReadRandomState();

	globs->console_player = consoleplayer; // NB: not used

	globs->level_time = leveltime;
	globs->exit_time  = exittime;

	globs->total_kills   = wi_stats.kills;
	globs->total_items   = wi_stats.items;
	globs->total_secrets = wi_stats.secret;

	globs->sky_image = sky_image;

	time(&cur_time);
	strftime(timebuf, 99, "%I:%M %p  %d/%b/%Y", localtime(&cur_time));

	if (timebuf[0] == '0' && isdigit(timebuf[1]))
		timebuf[0] = ' ';

	globs->description = SV_DupString(description);
	globs->desc_date   = SV_DupString(timebuf);

	globs->mapsector.count = numsectors;
	globs->mapsector.crc = mapsector_CRC.crc;
	globs->mapline.count = numlines;
	globs->mapline.crc = mapline_CRC.crc;
	globs->mapthing.count = mapthing_NUM;
	globs->mapthing.crc = mapthing_CRC.crc;

	SV_BeginSave();

	SV_SaveGLOB(globs);
	SV_SaveEverything();

	SV_FreeGLOB(globs);

	SV_FinishSave();
	SV_CloseWriteFile();

	return true; //OK
}

static void G_DoSaveGame(void)
{
	VM_SaveGame(); //Stub for now; eventually things like determining if saving is allowed, etc

	std::string fn(SV_FileName("current", "head"));

	if (G_SaveGameToFile(fn.c_str(), defer_save_desc))
	{
		const char *dir_name = SV_SlotName(defer_save_slot);

		SV_ClearSlot(dir_name);
		SV_CopySlot("current", dir_name);

		CON_Printf("%s", language["GameSaved"]);
	}
	else
	{
		// !!! FIXME: what to do?
	}

	defer_save_desc[0] = 0;
}

//
// G_InitNew Stuff
//
// Can be called by the startup code or the menu task.
// consoleplayer, displayplayer, players[] are setup.
//

//---> newgame_params_c class

newgame_params_c::newgame_params_c() :
	skill(sk_medium), deathmatch(0),
	map(NULL), random_seed(0),
	total_players(0), flags(NULL)
{
	for (int i = 0; i < MAXPLAYERS; i++)
	{
		players[i] = PFL_NOPLAYER;
		nodes[i]   = NULL;
	}
}

newgame_params_c::newgame_params_c(const newgame_params_c& src)
{
	skill = src.skill;
	deathmatch = src.deathmatch;

	map = src.map;

	random_seed   = src.random_seed;
	total_players = src.total_players;

	for (int i = 0; i < MAXPLAYERS; i++)
	{
		players[i] = src.players[i];
		nodes[i] = src.nodes[i];
	}

	flags = NULL;

	if (src.flags)
		CopyFlags(src.flags);
}

newgame_params_c::~newgame_params_c()
{
	if (flags)
		delete flags;
}

void newgame_params_c::SinglePlayer(int num_bots)
{
	total_players = 1 + num_bots;
	players[0] = PFL_Zero;  // i.e. !BOT and !NETWORK
	nodes[0]   = NULL;

	for (int pnum = 1; pnum <= num_bots; pnum++)
	{
		players[pnum] = PFL_Bot;
		nodes[pnum]   = NULL;
	}
}

void newgame_params_c::CopyFlags(const gameflags_t *F)
{
	if (flags)
		delete flags;

	flags = new gameflags_t;

	memcpy(flags, F, sizeof(gameflags_t));
}

//
// This is the procedure that changes the currmap
// at the start of the game and outside the normal
// progression of the game. All thats needed is the
// skill and the name (The name in the DDF File itself).
//
void G_DeferredNewGame(newgame_params_c& params)
{
	SYS_ASSERT(params.map);

	defer_params = new newgame_params_c(params);

	if (params.level_skip)
		defer_params->level_skip = true;

	gameaction = ga_newgame;
}

bool G_MapExists(const mapdef_c *map)
{
	return (W_CheckNumForName(map->lump.c_str()) >= 0);
}


//
// REQUIRED STATE:
//   (a) defer_params
//
static void G_DoNewGame(void)
{
	SYS_ASSERT(defer_params);

	E_ForceWipe();

	SV_ClearSlot("current");
	quickSaveSlot = -1;

	InitNew(*defer_params);

	bool skip_pre = defer_params->level_skip;

	delete defer_params;
	defer_params = NULL;

	VM_NewGame();

	// -AJA- 2003/10/09: support for pre-level briefing screen on first map.
	//       FIXME: kludgy. All this game logic desperately needs rethinking.
	if (skip_pre)
		gameaction = ga_loadlevel;
	else
		F_StartFinale(&currmap->f_pre, ga_loadlevel);
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
static void InitNew(newgame_params_c& params)
{
	// --- create players ---

	P_DestroyAllPlayers();

	for (int pnum = 0; pnum < MAXPLAYERS; pnum++)
	{
		if (params.players[pnum] == PFL_NOPLAYER)
			continue;

		P_CreatePlayer(pnum, (params.players[pnum] & PFL_Bot) ? true : false);

		if (consoleplayer < 0 && ! (params.players[pnum] & PFL_Bot) &&
			! (params.players[pnum] & PFL_Network))
		{
			G_SetConsolePlayer(pnum);
		}

		players[pnum]->node = params.nodes[pnum];
	}

	if (numplayers != params.total_players)
		I_Error("Internal Error: InitNew: player miscount (%d != %d)\n",
			numplayers, params.total_players);

	if (consoleplayer < 0)
		I_Error("Internal Error: InitNew: no local players!\n");

	G_SetDisplayPlayer(consoleplayer);

	if (paused)
	{
		paused = false;
		S_ResumeMusic(); // -ACB- 1999/10/07 New Music API
		S_ResumeSound();  
	}

	currmap = params.map;
	curr_hub_tag = 0;
	curr_hub_first = NULL;

	if (params.skill > sk_nightmare)
		params.skill = sk_nightmare;

	P_WriteRandomState(params.random_seed);

	automapactive = false;

	gameskill = params.skill;
	deathmatch = params.deathmatch;

// L_WriteDebug("G_InitNew: Deathmatch %d Skill %d\n", params.deathmatch, (int)params.skill);

	// copy global flags into the level-specific flags
	if (params.flags)
		level_flags = *params.flags;
	else
		level_flags = global_flags;

	if (params.skill == sk_nightmare)
	{
		level_flags.fastparm = true;
		level_flags.respawn = true;
	}

	N_ResetTics();
}

void G_DeferredEndGame(void)
{
	if (gamestate == GS_LEVEL || gamestate == GS_INTERMISSION ||
	    gamestate == GS_FINALE)
	{
		gameaction = ga_endgame;
	}
}

// 
// REQUIRED STATE:
//    ?? nothing ??
// 
static void G_DoEndGame(void)
{
	E_ForceWipe();

	P_DestroyAllPlayers();

	SV_ClearSlot("current");

	if (gamestate == GS_LEVEL)
	{
		BOT_EndLevel();

		// FIXME: P_ShutdownLevel()
	}

	gamestate = GS_NOTHING;

	V_SetPalette(PALETTE_NORMAL, 0);

	S_StopMusic();

	E_PickLoadingScreen();

	E_StartTitle();
}


bool G_CheckWhenAppear(when_appear_e appear)
{
	if (! (appear & (1 << gameskill)))
		return false;

	if (SP_MATCH() && !(appear & WNAP_Single))
		return false;

	if (COOP_MATCH() && !(appear & WNAP_Coop))
		return false;

	if (DEATHMATCH() && !(appear & WNAP_DeathMatch))
		return false;

	return true;
}


mapdef_c *G_LookupMap(const char *refname)
{
	mapdef_c *m = mapdefs.Lookup(refname);

	if (m && G_MapExists(m))
		return m;

	// -AJA- handle numbers (like original DOOM)
	if (strlen(refname) <= 2 && isdigit(refname[0]) &&
		(!refname[1] || isdigit(refname[1])))
	{
		int num = atoi(refname);
		char new_ref[20];

		// try MAP## first
		sprintf(new_ref, "MAP%02d", num);

		m = mapdefs.Lookup(new_ref);
		if (m && G_MapExists(m))
			return m;

		// otherwise try E#M#
		if (1 <= num && num <= 9) num = num + 10;
		sprintf(new_ref, "E%dM%d", num/10, num%10);

		m = mapdefs.Lookup(new_ref);
		if (m && G_MapExists(m))
			return m;
	}

	return NULL;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
