//----------------------------------------------------------------------------
//  EDGE Player Definition
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

#ifndef __E_PLAYER_H__
#define __E_PLAYER_H__

// The player data structure depends on a number
// of other structs: items (internal inventory),
// animation states (closely tied to the sprites
// used to represent them, unfortunately).
#include "p_weapon.h"

// In addition, the player is just a special
// case of the generic moving object/actor.
#include "p_mobj.h"

// Finally, for odd reasons, the player input
// is buffered within the player data struct,
// as commands per game tick.
#include "e_ticcmd.h"

#include "colormap.h"

// Networking and tick handling related.
#define BACKUPTICS  12

#define MAX_PLAYNAME  32

#define EFFECT_MAX_TIME  (5 * TICRATE)

// The maximum number of players, multiplayer/networking.
#define MAXPLAYERS  16

#define PLAYER_STOPSPEED  1.0

class net_node_c;

// Pointer to each player in the game.
extern struct player_s *players[MAXPLAYERS];
extern int numplayers;
extern int numbots;

// Player taking events, and displaying.
extern int consoleplayer;
extern int displayplayer;

//
// Player states.
//
typedef enum
{
	// Playing or camping.
	PST_LIVE,

	// Dead on the ground, view follows killer.
	PST_DEAD,

	// Waiting to be respawned in the level.
	PST_REBORN
}
playerstate_e;

//
// Player flags
typedef enum
{
	PFL_Zero = 0,

	PFL_Console = 0x0001,
	PFL_Display = 0x0002,
	PFL_Bot     = 0x0004,
	PFL_Network = 0x0008,
	PFL_Demo    = 0x0010,

	// this not used in player_t, only in newgame_params_c
	PFL_NOPLAYER = 0xFFFF
}
playerflag_e;

//
// Player internal flags, for cheats and debug.
//
typedef enum
{
	// No clipping, walk through barriers.
	CF_NOCLIP = 1,

	// No damage, no health loss.
	CF_GODMODE = 2,
}
cheat_t;

typedef struct
{
	// amount of ammo available
	int num;

	// maximum ammo carryable
	int max;
}
playerammo_t;

typedef struct
{
	// amount of stock available
	int num;

	// maximum stock carryable
	int max;
}
playerinv_t;

typedef struct
{
	// current counter value
	int num;

	// max counter value
	int max;
}
playercounter_t;

typedef enum
{
	// (for pending_wp only) no change is occuring
	WPSEL_NoChange = -2,

	// absolutely no weapon at all
	WPSEL_None = -1
}
weapon_selection_e;

//
// Extended player object info: player_t
//
typedef struct player_s
{
	// player number.  Starts at 0.
	int pnum;

	// actions to perform.  Comes either from the local computer or over
	// the network in multiplayer mode.
	ticcmd_t cmd;

	playerstate_e playerstate;

	// miscellaneous flags
	int playerflags;

	// map object that this player controls.  Will be NULL outside of a
	// level (e.g. on the intermission screen).
	mobj_t *mo;

	// player's name
	char playername[MAX_PLAYNAME];

	// a measure of how fast we are actually moving, based on how far
	// the player thing moves on the 2D map.
	float actual_speed;

	// Determine POV, including viewpoint bobbing during movement.
	// Focal origin above r.z
	// will be FLO_UNUSED until the first think.
	float viewz;

	// Base height above floor for viewz.  Tracks `std_viewheight' but
	// is different when squatting (i.e. after a fall).
	float viewheight;

	// Bob/squat speed.
	float deltaviewheight;

	// standard viewheight, usually 75% of height.
	float std_viewheight;

	// bounded/scaled total momentum.
	float bob;

	// Kick offset for vertangle (in mobj_t)
	float kick_offset;

	// when > 0, the player has activated zoom 
	int zoom_fov;

	// This is only used between levels,
	// mo->health is used during levels.
	float health;

	// Armour points for each type
	float armours[NUMARMOUR];
	const mobjtype_c *armour_types[NUMARMOUR];
	float totalarmour;  // needed for status bar

	// Power ups. invinc and invis are tic counters.
	float powers[NUMPOWERS];

	// bitflag of powerups to be kept (esp. BERSERK)
	int keep_powers;

	// Set of keys held
	keys_e cards;

	// weapons, either an index into the player->weapons[] array, or one
	// of the WPSEL_* values.
	weapon_selection_e ready_wp;
	weapon_selection_e pending_wp;

	// -AJA- 1999/08/11: Now uses playerweapon_t.
	playerweapon_t weapons[MAXWEAPONS];

	// current weapon choice for each key (1..9 and 0)
	weapon_selection_e key_choices[10];

	// for status bar: which numbers to light up
	int avail_weapons[10];

	// ammunition, one for each ammotype_e (except AM_NoAmmo)
	playerammo_t ammo[NUMAMMO];

	// inventory stock, one for each invtype_e
	playerinv_t inventory[NUMINV];

	// counters, one for each countertype_e
	playercounter_t counters[NUMCOUNTER];

	// True if button down last tic.
	bool attackdown[2];
	bool usedown;
	bool actiondown[2];

	// Bit flags, for cheats and debug.
	// See cheat_t, above.
	int cheats;

	// Refired shots are less accurate.
	int refire;

	// Frags, kills of other players.
	int frags;
	int totalfrags;

	// For intermission stats.
	int killcount;
	int itemcount;
	int secretcount;
	int leveltime;

	// For screen flashing (red or bright).
	int damagecount;
	int bonuscount;

	// Who did damage (NULL for floors/ceilings).
	mobj_t *attacker;

	// how much damage was done (used for status bar)
	float damage_pain;

	// So gun flashes light up the screen.
	int extralight;
	bool flash;

	// -AJA- 1999/07/10: changed for colmap.ddf.
	const colourmap_c *effect_colourmap;
	int effect_left;  // tics remaining, maxed to EFFECT_MAX_TIME

	// Overlay view sprites (gun, etc).
	pspdef_t psprites[NUMPSPRITES];

	// Current PSP for action
	int action_psp;

	// Implements a wait counter to prevent use jumping again
	// -ACB- 1998/08/09
	int jumpwait;

	// counter used to determine when to enter weapon idle states
	int idlewait;

	int splashwait;

	// breathing support.  In air-less sectors, this is decremented on
	// each tic.  When it reaches zero, the player starts choking (which
	// hurts), and player drowns when health drops to zero.
	int air_in_lungs;
	bool underwater;
	bool swimming;
	bool wet_feet;

	// how many tics to grin :-)
	int grin_count;

	// how many tics player has been attacking (for rampage face)
	int attackdown_count;

	// status bar: used to choose which face to show
	int face_index;
	int face_count;

	// -AJA- 1999/08/10: This field is the state number which is
	// remembered for WEAPON_NOFIRE_RETURN when the player lets go of
	// the button.  Holds -1 if not fired or after changing weapons.
	int remember_atk[2];

	// last frame for weapon models
	int weapon_last_frame;

	ticcmd_t in_cmds[BACKUPTICS];

	int in_tic;  /* tic number of next input command expected */

	// node is NULL for players and bots on the same computer,
	// otherwise is the networking info for the remote computer.
	net_node_c *node;

	// This function will be called to initialise the ticcmd_t.
	void (*builder)(const struct player_s *, void *data, ticcmd_t *dest);
	void *build_data;

public:
	void Reborn();

	inline bool isBot() const
	{
		return (playerflags & PFL_Bot) ? true:false;
	}
}
player_t;

// Player ticcmd builders
void P_ConsolePlayerBuilder(const player_t *p, void *data, ticcmd_t *dest);
void P_BotPlayerBuilder(const player_t *p, void *data, ticcmd_t *dest);

void G_ClearBodyQueue(void);
void G_DeathMatchSpawnPlayer(player_t *p);
void G_CoopSpawnPlayer(player_t *p);
void G_HubSpawnPlayer(player_t *p, int tag);
void G_SpawnVoodooDolls(player_t *p);

void G_SetConsolePlayer(int pnum);
void G_SetDisplayPlayer(int pnum);
void G_ToggleDisplayPlayer(void);

void G_PlayerReborn(player_t *player, const mobjtype_c *info);
void G_PlayerFinishLevel(player_t *p, bool keep_cards);
void G_MarkPlayerAvatars(void);
void G_RemoveOldAvatars(void);

bool G_CheckConditions(mobj_t *mo, condition_check_t *cond);

void G_ClearPlayerStarts(void);

void G_AddDeathmatchStart(const spawnpoint_t& point);
void G_AddCoopStart(const spawnpoint_t& point);
void G_AddHubStart(const spawnpoint_t& point);
void G_AddVoodooDoll(const spawnpoint_t& point);

spawnpoint_t *G_FindCoopPlayer(int pnum);


#endif // __E_PLAYER_H__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
