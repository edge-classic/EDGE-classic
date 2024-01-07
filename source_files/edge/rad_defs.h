//----------------------------------------------------------------------------
//  Radius Trigger Main definitions
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

#ifndef __RAD_MAIN_H__
#define __RAD_MAIN_H__

#include "main.h"

#include "r_defs.h"
#include "hu_stuff.h"

#include "math_crc.h"

struct rts_state_s;
struct rad_script_s;
struct rad_trigger_s;

typedef enum
{
    RTS_TAG_NUMBER = 0,
    RTS_TAG_HASH
} s_tagtype_e;

typedef struct s_weapon_s
{
    char *name = nullptr;
} s_weapon_t;

typedef struct s_tip_s
{
    // tip text or graphic.  Two of these must be NULL.
    const char *tip_text    = nullptr;
    char       *tip_ldf     = nullptr;
    char       *tip_graphic = nullptr;

    // display time, in ticks
    int display_time = 0;

    // play the TINK sound ?
    bool playsound = false;

    // graphic scaling (1.0 is normal, higher is bigger).
    float gfx_scale = 0;
} s_tip_t;

typedef struct s_tip_prop_s
{
    // new slot number, or < 0 for no change.
    int slot_num = 0;

    // tip position (as a percentage, 0-255), < 0 for no change
    percent_t x_pos = 0;
    percent_t y_pos = 0;

    // left justify.  Can be 1, 0, or < 0 for no change.
    int left_just = 0;

    // tip color, or NULL for no change
    const char *color_name = nullptr;

    // translucency value (normally 1.0), or < 0 for no change
    percent_t translucency = 0;

    // time (in tics) to reach target.
    int time = 0;
} s_tip_prop_t;

typedef struct s_show_menu_s
{
    bool use_ldf = false;

    char *title      = nullptr;
    char *options[9] = {nullptr};
} s_show_menu_t;

typedef struct s_menu_style_s
{
    char *style = nullptr;
} s_menu_style_t;

typedef struct s_jump_on_s
{
    // int vartype;  /* only MENU currently supported */

    char *labels[9] = {nullptr};
} s_jump_on_t;

// SpawnThing Function
typedef struct s_thing_s
{
    // spawn coordinates.  z can be ONFLOORZ or ONCEILINGZ.
    float x = 0;
    float y = 0;
    float z = 0;

    bam_angle angle = 0;
    float   slope = 0;
    int     tag   = 0;

    when_appear_e appear = WNAP_None;

    // -AJA- 1999/09/11: since the RSCRIPT lump can be loaded before
    //       DDF* lumps, we can't store a pointer to a mobjtype_c here
    //       (and the mobjtypes can move about with later additions).

    // thing's DDF name, or if NULL, then thing's mapnumber.
    char *thing_name = nullptr;
    int   thing_type = 0;

    bool ambush       = false;
    bool spawn_effect = false;
} s_thing_t;

// Radius Damage Player Trigger
typedef struct s_damagep_s
{
    float damage_amount = 0;
} s_damagep_t;

// Radius Heal Player Trigger
typedef struct s_healp_s
{
    float limit       = 0;
    float heal_amount = 0;
} s_healp_t;

// Radius GiveArmour Player Trigger
typedef struct s_armour_s
{
    armour_type_e type          = ARMOUR_Green;
    float         limit         = 0;
    float         armour_amount = 0;
} s_armour_t;

// Radius Give/Lose Benefit
typedef struct s_benefit_s
{
    benefit_t *benefit = nullptr;
    bool       lose_it = false; // or use_it :)
} s_benefit_t;

// Radius Damage Monster Trigger
typedef struct s_damage_monsters_s
{
    // type of monster to damage: DDF name, or if NULL, then the
    // monster's mapnumber, or if -1 then ANY monster can be damaged.
    char *thing_name = nullptr;
    int   thing_type = 0;
    int   thing_tag  = 0;

    // how much damage to do
    float damage_amount = 0;
} s_damage_monsters_t;

// Set Skill
typedef struct s_skill_s
{
    skill_t skill        = sk_baby;
    bool    respawn      = false;
    bool    fastmonsters = false;
} s_skill_t;

// Go to map
typedef struct s_gotomap_s
{
    char *map_name = nullptr;

    bool skip_all = false;
    bool is_hub   = false;

    int tag = 0;
} s_gotomap_t;

// Play Sound function
typedef enum
{
    PSOUND_Normal = 0,
    PSOUND_BossMan
} s_sound_kind_e;

typedef struct s_sound_s
{
    int kind = 0;

    // sound location.  z can be ONFLOORZ.
    float x = 0;
    float y = 0;
    float z = 0;

    struct sfx_s *sfx = nullptr;
} s_sound_t;

// Change Music function
typedef struct s_music_s
{
    // playlist entry number
    int playnum = 0;

    // whether to loop or not
    bool looping = false;
} s_music_t;

// Play Movie function
typedef struct s_movie_s
{
    // lump or packfile name
    std::string movie;
} s_movie_t;

// Sector Vertical movement
typedef struct s_movesector_s
{
    // tag to apply to.  When tag == 0, use the exact sector number
    // (deprecated, but kept for backwards compat).
    int tag    = 0;
    int secnum = 0;

    // Ceiling or Floor
    bool is_ceiling = false;

    // when true, add the value to current height.  Otherwise set it.
    bool relative = false;

    float value = 0;
} s_movesector_t;

// Sector Light change
typedef struct s_lightsector_s
{
    // tag to apply to.  When tag == 0, use the exact sector number
    // (deprecated, but kept for backwards compat).
    int tag    = 0;
    int secnum = 0;

    // when true, add the value to current light.  Otherwise set it.
    bool relative = false;

    float value = 0;
} s_lightsector_t;

// Sector Fog change
typedef struct s_fogsector_s
{
    // tag to apply to
    int tag = 0;

    // when true, add the value to current density.  Otherwise set it.
    bool relative = true;

    // when true, leave color or density untouched regardless of this struct's values
    bool leave_color   = false;
    bool leave_density = false;

    const char *colmap_color = nullptr;

    float density;
} s_fogsector_t;

// Enable/Disable
typedef struct s_enabler_s
{
    // script to enable/disable.  If script_name is NULL, then `tag' is
    // the tag number to enable/disable.
    char    *script_name = nullptr;
    uint32_t tag[2]      = {0, 0};

    // true to disable, false to enable
    bool new_disabled = false;
} s_enabler_t;

// ActivateLine
typedef struct s_lineactivator_s
{
    // line type
    int typenum = 0;

    // sector tag
    int tag = 0;
} s_lineactivator_t;

// UnblockLines
typedef struct s_lineunblocker_s
{
    // line tag
    int tag = 0;
} s_lineunblocker_t;

// Jump
typedef struct s_jump_s
{
    // label name
    char *label = nullptr;

    // state to jump to.  Initially NULL, it is looked up when needed
    // (since the label may be a future reference, we can't rely on
    // looking it up at parse time).
    struct rts_state_s *cache_state = nullptr;

    // chance that the jump is taken.
    percent_t random_chance = 0;
} s_jump_t;

// Exit
typedef struct s_exit_s
{
    // exit time, in tics
    int exittime = 0;

    bool is_secret = false;
} s_exit_t;

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
} changetex_type_e;

typedef struct s_changetex_s
{
    // what to change
    changetex_type_e what = CHTEX_RightUpper;

    // texture/flat name
    char texname[10] = {0};

    // tags used to find lines/sectors to change.  The `tag' value must
    // match sector.tag for sector changers and line.tag for line
    // changers.  The `subtag' value, if not 0, acts as a restriction:
    // for sector changers, a line in the sector must match subtag, and
    // for line changers, the sector on the given side must match the
    // subtag.  Both are ignored for sky changers.
    int tag    = 0;
    int subtag = 0;
} s_changetex_t;

// Thing Event
typedef struct s_thing_event_s
{
    // DDF type name of thing to cause the event.  If NULL, then the
    // thing map number is used instead.
    const char *thing_name = nullptr;
    int         thing_type = 0;
    int         thing_tag  = 0;

    // label to jump to
    const char *label  = nullptr;
    int         offset = 0;
} s_thing_event_t;

// Weapon Event
typedef struct s_weapon_event_s
{
    // DDF type name of weapon to cause the event.
    const char *weapon_name = nullptr;

    // label to jump to
    const char *label  = nullptr;
    int         offset = 0;
} s_weapon_event_t;

typedef struct s_weapon_replace_s
{
    const char *old_weapon = nullptr;
    const char *new_weapon = nullptr;
} s_weapon_replace_t;

typedef struct s_thing_replace_s
{
    const char *old_thing_name = nullptr;
    const char *new_thing_name = nullptr;
    int         old_thing_type = -1;
    int         new_thing_type = -1;
} s_thing_replace_t;

// A single RTS action, not unlike the ones for DDF things.
//
typedef struct rts_state_s
{
    // link in list of states
    struct rts_state_s *next = nullptr;
    struct rts_state_s *prev = nullptr;

    // duration in tics
    int tics = 0;

    // routine to be performed
    void (*action)(struct rad_trigger_s *trig, void *param) = nullptr;

    // parameter for routine, or NULL
    void *param = nullptr;

    // state's label, or NULL
    char *label = nullptr;
} rts_state_t;

// Destination path name
typedef struct rts_path_s
{
    // next in list, or NULL
    struct rts_path_s *next = nullptr;

    const char *name = nullptr;

    // cached pointer to script
    struct rad_script_s *cached_scr = nullptr;
} rts_path_t;

// ONDEATH info
typedef struct s_ondeath_s
{
    // next in link (order is unimportant)
    struct s_ondeath_s *next = nullptr;

    // thing's DDF name, or if NULL, then thing's mapnumber.
    char *thing_name = nullptr;
    int   thing_type = 0;

    // threshhold: number of things still alive before the trigger can
    // activate.  Defaults to zero (i.e. all of them must be dead).
    int threshhold = 0;

    // mobjdef pointer, computed the first time this ONDEATH condition
    // is tested.
    const mobjtype_c *cached_info = nullptr;
} s_ondeath_t;

// ONHEIGHT info
typedef struct s_onheight_s
{
    // next in link (order is unimportant)
    struct s_onheight_s *next = nullptr;

    // Ceiling/Floor
    bool is_ceil = false;

    // height range, trigger won't activate until sector's floor is
    // within this range (inclusive).
    float z1 = 0;
    float z2 = 0;

    // sector number, < 0 means use the trigger's location
    int sec_num = 0;

    // sector pointer, computed the first time this ONHEIGHT condition
    // is tested.
    sector_t *cached_sector = nullptr;
} s_onheight_t;

// WAIT_UNTIL_DEAD info
typedef struct s_wait_until_dead_s
{
    // tag number to give the monsters which we'll wait on
    int tag = 0;

    // the DDF names of the monsters to wait for
    const char *mon_names[10] = {nullptr};
} s_wait_until_dead_t;

// Trigger Definition (Made up of actions)
// Start_Map & Radius_Trigger Declaration

typedef struct rad_script_s
{
    // link in list
    struct rad_script_s *next = nullptr;
    struct rad_script_s *prev = nullptr;

    // Which map
    char *mapid = nullptr;

    // When appears
    when_appear_e appear = WNAP_None;

    int min_players = 0;
    int max_players = 0;

    // Map Coordinates
    float x = 0;
    float y = 0;
    float z = 0;

    // Trigger size
    float rad_x = 0;
    float rad_y = 0;
    float rad_z = 0;

    // Sector Tag - Will ignore above X/Y coords and size if > 0 
    int sector_tag = 0;

    // Sector Index - Will ignore above X/Y coords and size if >= 0 and Tag is also 0
    int sector_index = -1;

    // Script name (or NULL)
    char *script_name = nullptr;

    // Script tag (or 0 for none)
    uint32_t tag[2] = {0, 0};

    // ABSOLUTE mode: minimum players needed to trigger, -1 for ALL
    int absolute_req_players = 0;

    // Initially disabled ?
    bool tagged_disabled = false;

    // Check for use.
    bool tagged_use = false;

    // Continues working ?
    bool tagged_independent = false;

    // Requires no player intervention ?
    bool tagged_immediate = false;

    // Tagged_Repeat info (normal if repeat_count < 0)
    int repeat_count = 0;
    int repeat_delay = 0;

    // Optional conditions...
    s_ondeath_t       *boss_trig   = nullptr;
    s_onheight_t      *height_trig = nullptr;
    condition_check_t *cond_trig   = nullptr;

    // Path info
    rts_path_t *next_in_path    = nullptr;
    int         next_path_total = 0;

    const char *path_event_label  = nullptr;
    int         path_event_offset = 0;

    // Set of states
    rts_state_t *first_state = nullptr;
    rts_state_t *last_state  = nullptr;

    // CRC of the important parts of this RTS script.
    epi::crc32_c crc;
} rad_script_t;

#define REPEAT_FOREVER 0

// Dynamic Trigger info.
// Goes away when trigger is finished.
typedef struct rad_trigger_s
{
    // link in list
    struct rad_trigger_s *next = nullptr;
    struct rad_trigger_s *prev = nullptr;

    // link for triggers with the same tag
    struct rad_trigger_s *tag_next = nullptr;
    struct rad_trigger_s *tag_prev = nullptr;

    // parent info of trigger
    rad_script_t *info = nullptr;

    // is it disabled ?
    bool disabled = false;

    // has it been activated yet?
    bool activated = false;

    // players who activated it (bit field)
    int acti_players = 0;

    // repeat info
    int repeats_left = 0;
    int repeat_delay = 0;

    // current state info
    rts_state_t *state     = nullptr;
    int          wait_tics = 0;

    // current tip slot (each tip slot works independently).
    int tip_slot = 0;

    // menu style name, or NULL if not set
    const char *menu_style_name = nullptr;

    // result of last SHOW_MENU (1 to 9, or 0 when cancelled)
    int menu_result = 0;

    // Sound handle
    position_c sfx_origin = {0, 0, 0};

    // used for WAIT_UNTIL_DEAD, normally zero
    int wud_tag   = 0;
    int wud_count = 0;

    // prevent repeating scripts from clogging the console
    const char *last_con_message = nullptr;
} rad_trigger_t;

//
// Tip Displayer info
//
#define MAXTIPSLOT 45

#define TIP_LINE_MAX 10

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
    const char    *tip_text;
    const image_c *tip_graphic;

    // play a sound ?
    bool playsound;

    // scaling info (so far only for Tip_Graphic)
    float scale;

    // current colour
    rgbcol_t color;

    // fading fields
    int   fade_time;
    float fade_target;
} drawtip_t;

extern drawtip_t tip_slots[MAXTIPSLOT];

#endif /*__RAD_MAIN_H__*/

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
