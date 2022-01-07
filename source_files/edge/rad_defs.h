//----------------------------------------------------------------------------
//  Radius Trigger Main definitions
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

#ifndef __RAD_MAIN_H__
#define __RAD_MAIN_H__

#include "main.h"

#include "r_defs.h"
#include "hu_stuff.h"

#include "math_crc.h"

struct rts_state_s;
struct rad_script_s;
struct rad_trigger_s;

typedef struct s_weapon_s
{
	char *name;
}
s_weapon_t;

typedef struct s_tip_s
{
	// tip text or graphic.  Two of these must be NULL.
	const char *tip_text;
	char *tip_ldf;
	char *tip_graphic;

	// display time, in ticks
	int display_time;

	// play the TINK sound ?
	bool playsound;

	// graphic scaling (1.0 is normal, higher is bigger).
	float gfx_scale;
}
s_tip_t;


typedef struct s_tip_prop_s
{
	// new slot number, or < 0 for no change.
	int slot_num;

	// tip position (as a percentage, 0-255), < 0 for no change
	percent_t x_pos, y_pos;

	// left justify.  Can be 1, 0, or < 0 for no change.
	int left_just;

	// tip color, or NULL for no change
	const char *color_name;

	// translucency value (normally 1.0), or < 0 for no change
	percent_t translucency;

	// time (in tics) to reach target.
	int time;
}
s_tip_prop_t;


typedef struct s_show_menu_s
{
	bool use_ldf;

	char *title;
	char *options[9];
}
s_show_menu_t;

typedef struct s_menu_style_s
{
	char *style;
}
s_menu_style_t;

typedef struct s_jump_on_s
{
	// int vartype;  /* only MENU currently supported */

	char *labels[9];
}
s_jump_on_t;

// SpawnThing Function
typedef struct s_thing_s
{
	// spawn coordinates.  z can be ONFLOORZ or ONCEILINGZ.
	float x;
	float y;
	float z;

	angle_t angle;
	float slope;
	int tag;

	when_appear_e appear;

	// -AJA- 1999/09/11: since the RSCRIPT lump can be loaded before
	//       DDF* lumps, we can't store a pointer to a mobjtype_c here
	//       (and the mobjtypes can move about with later additions).

	// thing's DDF name, or if NULL, then thing's mapnumber.
	char *thing_name;
	int thing_type;

	bool ambush;
	bool spawn_effect;
}
s_thing_t;


// Radius Damage Player Trigger
typedef struct
{
	float damage_amount;
}
s_damagep_t;


// Radius Heal Player Trigger
typedef struct
{
	float limit;
	float heal_amount;
}
s_healp_t;


// Radius GiveArmour Player Trigger
typedef struct
{
	armour_type_e type;
	float limit;
	float armour_amount;
}
s_armour_t;


// Radius Give/Lose Benefit
typedef struct
{
	benefit_t *benefit;
	bool lose_it;  // or use_it :)
}
s_benefit_t;


// Radius Damage Monster Trigger
typedef struct s_damage_monsters_s
{
	// type of monster to damage: DDF name, or if NULL, then the
	// monster's mapnumber, or if -1 then ANY monster can be damaged.
	char *thing_name;
	int thing_type;
	int thing_tag;

	// how much damage to do
	float damage_amount;
}
s_damage_monsters_t;


// Set Skill
typedef struct
{
	skill_t skill;
	bool respawn;
	bool fastmonsters;
}
s_skill_t;


// Go to map
typedef struct
{
	char *map_name;

	bool skip_all;
	bool is_hub;

	int tag;
}
s_gotomap_t;


// Play Sound function
typedef enum
{
	PSOUND_Normal = 0,
	PSOUND_BossMan
}
s_sound_kind_e;

typedef struct s_sound_s
{
	int kind;

	// sound location.  z can be ONFLOORZ.
	float x, y, z;

	struct sfx_s *sfx;
}
s_sound_t;


// Change Music function
typedef struct s_music_s
{
	// playlist entry number
	int playnum;

	// whether to loop or not
	bool looping;
}
s_music_t;


//Sector Vertical movement
typedef struct s_movesector_s
{
	// tag to apply to.  When tag == 0, use the exact sector number
	// (deprecated, but kept for backwards compat).
	int tag;
	int secnum;

	// Ceiling or Floor
	bool is_ceiling;

	// when true, add the value to current height.  Otherwise set it.
	bool relative;

	float value;
}
s_movesector_t;


//Sector Light change
typedef struct s_lightsector_s
{
	// tag to apply to.  When tag == 0, use the exact sector number
	// (deprecated, but kept for backwards compat).
	int tag;
	int secnum;

	// when true, add the value to current light.  Otherwise set it.
	bool relative;

	float value;
}
s_lightsector_t;


// Enable/Disable
typedef struct s_enabler_s
{
	// script to enable/disable.  If script_name is NULL, then `tag' is
	// the tag number to enable/disable.
	char *script_name;
	int tag;

	// true to disable, false to enable
	bool new_disabled;
}
s_enabler_t;


// ActivateLine
typedef struct s_lineactivator_s
{
	// line type
	int typenum;

	// sector tag
	int tag;
}
s_lineactivator_t;


// UnblockLines
typedef struct s_lineunblocker_s
{
	// line tag
	int tag;
}
s_lineunblocker_t;


// Jump
typedef struct s_jump_s
{
	// label name
	char *label;

	// state to jump to.  Initially NULL, it is looked up when needed
	// (since the label may be a future reference, we can't rely on
	// looking it up at parse time).
	struct rts_state_s *cache_state;

	// chance that the jump is taken.
	percent_t random_chance;
}
s_jump_t;


// Exit
typedef struct s_exit_s
{
	// exit time, in tics
	int exittime;

	bool is_secret;
}
s_exit_t;


// Texture changing on lines/sectors
typedef enum
{
	// right side of the line
	CHTEX_RightUpper  = 0,
	CHTEX_RightMiddle = 1,
	CHTEX_RightLower  = 2,

	// left side of the line
	CHTEX_LeftUpper  = 3,
	CHTEX_LeftMiddle = 4,
	CHTEX_LeftLower  = 5,

	// the sky texture
	CHTEX_Sky = 6,

	// sector floor or ceiling
	CHTEX_Floor   = 7,
	CHTEX_Ceiling = 8,
}
changetex_type_e;

typedef struct s_changetex_s
{
	// what to change
	changetex_type_e what;

	// texture/flat name
	char texname[10];

	// tags used to find lines/sectors to change.  The `tag' value must
	// match sector.tag for sector changers and line.tag for line
	// changers.  The `subtag' value, if not 0, acts as a restriction:
	// for sector changers, a line in the sector must match subtag, and
	// for line changers, the sector on the given side must match the
	// subtag.  Both are ignored for sky changers.
	int tag, subtag;
}
s_changetex_t;


// Thing Event
typedef struct s_thing_event_s
{
	// DDF type name of thing to cause the event.  If NULL, then the
	// thing map number is used instead.
	const char *thing_name;
	int thing_type;
	int thing_tag;

	// label to jump to
	const char *label;
	int offset;
}
s_thing_event_t;


// A single RTS action, not unlike the ones for DDF things.
//
typedef struct rts_state_s
{
	// link in list of states
	struct rts_state_s *next;
	struct rts_state_s *prev;

	// duration in tics
	int tics;

	// routine to be performed
	void (*action)(struct rad_trigger_s *trig, void *param);

	// parameter for routine, or NULL
	void *param;

	// state's label, or NULL
	char *label;
}
rts_state_t;


// Destination path name
typedef struct rts_path_s
{
	// next in list, or NULL
	struct rts_path_s *next;

	const char *name;

	// cached pointer to script
	struct rad_script_s *cached_scr;
}
rts_path_t;


// ONDEATH info
typedef struct s_ondeath_s
{
	// next in link (order is unimportant)
	struct s_ondeath_s *next;

	// thing's DDF name, or if NULL, then thing's mapnumber.
	char *thing_name;
	int thing_type;

	// threshhold: number of things still alive before the trigger can
	// activate.  Defaults to zero (i.e. all of them must be dead).
	int threshhold;

	// mobjdef pointer, computed the first time this ONDEATH condition
	// is tested.
	const mobjtype_c *cached_info;
}
s_ondeath_t;


// ONHEIGHT info
typedef struct s_onheight_s
{
	// next in link (order is unimportant)
	struct s_onheight_s *next;

	// Ceiling/Floor
	bool is_ceil;

	// height range, trigger won't activate until sector's floor is
	// within this range (inclusive).
	float z1, z2;

	// sector number, < 0 means use the trigger's location
	int sec_num;

	// sector pointer, computed the first time this ONHEIGHT condition
	// is tested.
	sector_t *cached_sector;
}
s_onheight_t;


// WAIT_UNTIL_DEAD info
typedef struct s_wait_until_dead_s
{
	// tag number to give the monsters which we'll wait on
	int tag;

	// the DDF names of the monsters to wait for
	const char *mon_names[10];
}
s_wait_until_dead_t;


// Trigger Definition (Made up of actions)
// Start_Map & Radius_Trigger Declaration

typedef struct rad_script_s
{
	// link in list
	struct rad_script_s *next;
	struct rad_script_s *prev;

	// Which map
	char *mapid;

	// When appears
	when_appear_e appear;

	int min_players;
	int max_players;

	// Map Coordinates
	float x, y, z;

	// Trigger size
	float rad_x, rad_y, rad_z;

	// Script name (or NULL)
	char *script_name;

	// Script tag (or 0 for none)
	int tag;

	// ABSOLUTE mode: minimum players needed to trigger, -1 for ALL
	int absolute_req_players;

	// Initially disabled ?
	bool tagged_disabled;

	// Check for use.
	bool tagged_use;

	// Continues working ?
	bool tagged_independent;

	// Requires no player intervention ?
	bool tagged_immediate;

	// Tagged_Repeat info (normal if repeat_count < 0)
	int repeat_count;
	int repeat_delay;

	// Optional conditions...
	s_ondeath_t *boss_trig;
	s_onheight_t *height_trig;
	condition_check_t *cond_trig;

	// Path info
	rts_path_t *next_in_path;
	int next_path_total;

	const char *path_event_label;
	int path_event_offset;

	// Set of states
	rts_state_t *first_state;
	rts_state_t *last_state;

	// CRC of the important parts of this RTS script.
	epi::crc32_c crc;
}
rad_script_t;


#define REPEAT_FOREVER  0

// Dynamic Trigger info.
// Goes away when trigger is finished.
typedef struct rad_trigger_s
{
	// link in list
	struct rad_trigger_s *next;
	struct rad_trigger_s *prev;

	// link for triggers with the same tag
	struct rad_trigger_s *tag_next;
	struct rad_trigger_s *tag_prev;

	// parent info of trigger
	rad_script_t *info;

	// is it disabled ?
	bool disabled;

	// has it been activated yet?
	bool activated;

	// players who activated it (bit field)
	int acti_players;

	// repeat info
	int repeats_left;
	int repeat_delay;

	// current state info
	rts_state_t *state;
	int wait_tics;

	// current tip slot (each tip slot works independently).
	int tip_slot;

	// menu style name, or NULL if not set
	const char *menu_style_name;

	// result of last SHOW_MENU (1 to 9, or 0 when cancelled)
	int menu_result;

    // Sound handle
	position_c sfx_origin;

	// used for WAIT_UNTIL_DEAD, normally zero
	int wud_tag;
	int wud_count;

	// prevent repeating scripts from clogging the console
	const char *last_con_message;
}
rad_trigger_t;


//
// Tip Displayer info
//
#define MAXTIPSLOT    45

#define TIP_LINE_MAX  10

typedef struct drawtip_s
{
	// current properties
	s_tip_prop_t p;

	// display time.  When < 0, this slot is not in use (and all of the
	// fields below this one are unused).
	int delay;

	// do we need to recompute some stuff (e.g. colmap) ?
	bool dirty;

	// tip text DOH!
	const char *tip_text;
	const image_c *tip_graphic;

	// play a sound ?
	bool playsound;

	// scaling info (so far only for Tip_Graphic)
	float scale;

	// current colour
	rgbcol_t color;

	// fading fields
	int fade_time;
	float fade_target;
}
drawtip_t;

extern drawtip_t tip_slots[MAXTIPSLOT];

#endif /*__RAD_MAIN_H__*/

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
