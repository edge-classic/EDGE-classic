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

#pragma once

#include "hu_stuff.h"
#include "main.h"
#include "math_crc.h"
#include "r_defs.h"

struct TriggerScriptState;
struct TriggerScript;
struct TriggerScriptTrigger;

enum TriggerScriptTag
{
    kTriggerTagNumber = 0,
    kTriggerTagHash
};

struct ScriptWeaponParameter
{
    char *name = nullptr;
};

struct ScriptTip
{
    // tip text or graphic.  Two of these must be nullptr.
    const char *tip_text    = nullptr;
    char       *tip_ldf     = nullptr;
    char       *tip_graphic = nullptr;

    // display time, in ticks
    int display_time = 0;

    // play the TINK sound ?
    bool playsound = false;

    // graphic scaling (1.0 is normal, higher is bigger).
    float gfx_scale = 0;
};

struct ScriptTipProperties
{
    // new slot number, or < 0 for no change.
    int slot_num = 0;

    // tip position (as a percentage, 0-255), < 0 for no change
    float x_pos = 0;
    float y_pos = 0;

    // left justify.  Can be 1, 0, or < 0 for no change.
    int left_just = 0;

    // tip color, or nullptr for no change
    const char *color_name = nullptr;

    // translucency value (normally 1.0), or < 0 for no change
    float translucency = 0;

    // time (in tics) to reach target.
    int time = 0;
};

struct ScriptShowMenuParameter
{
    bool use_ldf = false;

    char *title      = nullptr;
    char *options[9] = {nullptr};
};

struct ScriptMenuStyle
{
    char *style = nullptr;
};

struct ScriptJumpOnParameter
{
    // int vartype;  /* only MENU currently supported */

    char *labels[9] = {nullptr};
};

// SpawnThing Function
struct ScriptThingParameter
{
    // spawn coordinates.  z can be kOnFloorZ or kOnCeilingZ.
    float x = 0;
    float y = 0;
    float z = 0;

    BAMAngle angle = 0;
    float    slope = 0;
    int      tag   = 0;

    AppearsFlag appear = kAppearsWhenNone;

    // -AJA- 1999/09/11: since the RSCRIPT lump can be loaded before
    //       DDF* lumps, we can't store a pointer to a MapObjectDefinition here
    //       (and the mobjtypes can move about with later additions).

    // thing's DDF name, or if nullptr, then thing's mapnumber.
    char *thing_name = nullptr;
    int   thing_type = 0;

    bool ambush       = false;
    bool spawn_effect = false;
};

// Radius Damage Player Trigger
struct ScriptDamagePlayerParameter
{
    float damage_amount = 0;
};

// Radius Heal Player Trigger
struct ScriptHealParameter
{
    float limit       = 0;
    float heal_amount = 0;
};

// Radius GiveArmour Player Trigger
struct ScriptArmourParameter
{
    ArmourType type          = kArmourTypeGreen;
    float      limit         = 0;
    float      armour_amount = 0;
};

// Radius Give/Lose Benefit
struct ScriptBenefitParameter
{
    Benefit *benefit = nullptr;
    bool     lose_it = false;  // or use_it :)
};

// Radius Damage Monster Trigger
struct ScriptDamangeMonstersParameter
{
    // type of monster to damage: DDF name, or if nullptr, then the
    // monster's mapnumber, or if -1 then ANY monster can be damaged.
    char *thing_name = nullptr;
    int   thing_type = 0;
    int   thing_tag  = 0;

    // how much damage to do
    float damage_amount = 0;
};

// Set Skill
struct ScriptSkillParameter
{
    SkillLevel skill        = kSkillBaby;
    bool       respawn      = false;
    bool       fastmonsters = false;
};

// Go to map
struct ScriptGoToMapParameter
{
    char *map_name = nullptr;

    bool skip_all = false;
    bool is_hub   = false;

    int tag = 0;
};

// Play Sound function
enum ScriptSoundKind
{
    kScriptSoundNormal = 0,
    kScriptSoundBossMan
};

struct ScriptSoundParameter
{
    int kind = 0;

    // sound location.  z can be kOnFloorZ.
    float x = 0;
    float y = 0;
    float z = 0;

    struct SoundEffect *sfx = nullptr;
};

// Change Music function
struct ScriptMusicParameter
{
    // playlist entry number
    int playnum = 0;

    // whether to loop or not
    bool looping = false;
};

// Play Movie function
struct ScriptMovieParameter
{
    // lump or packfile name
    std::string movie;
};

// Sector Vertical movement
struct ScriptMoveSectorParameter
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
};

// Sector Light change
struct ScriptSectorLightParameter
{
    // tag to apply to.  When tag == 0, use the exact sector number
    // (deprecated, but kept for backwards compat).
    int tag    = 0;
    int secnum = 0;

    // when true, add the value to current light.  Otherwise set it.
    bool relative = false;

    float value = 0;
};

// Sector Fog change
struct ScriptFogSectorParameter
{
    // tag to apply to
    int tag = 0;

    // when true, add the value to current density.  Otherwise set it.
    bool relative = true;

    // when true, leave color or density untouched regardless of this struct's
    // values
    bool leave_color   = false;
    bool leave_density = false;

    const char *colmap_color = nullptr;

    float density;
};

// Enable/Disable
struct ScriptEnablerParameter
{
    // script to enable/disable.  If script_name is nullptr, then `tag' is
    // the tag number to enable/disable.
    char    *script_name = nullptr;
    uint32_t tag[2]      = {0, 0};

    // true to disable, false to enable
    bool new_disabled = false;
};

// ActivateLine
struct ScriptActivateLineParameter
{
    // line type
    int typenum = 0;

    // sector tag
    int tag = 0;
};

// UnblockLines
struct ScriptLineBlockParameter
{
    // line tag
    int tag = 0;
};

// Jump
struct ScriptJumpParameter
{
    // label name
    char *label = nullptr;

    // state to jump to.  Initially nullptr, it is looked up when needed
    // (since the label may be a future reference, we can't rely on
    // looking it up at parse time).
    struct TriggerScriptState *cache_state = nullptr;

    // chance that the jump is taken.
    float random_chance = 0;
};

// Exit
struct ScriptExitParameter
{
    // exit time, in tics
    int exit_time = 0;

    bool is_secret = false;
};

// Texture changing on lines/sectors
enum ScriptChangeTexturetureType
{
    // right side of the line
    kChangeTextureRightUpper  = 0,
    kChangeTextureRightMiddle = 1,
    kChangeTextureRightLower  = 2,

    // left side of the line
    kChangeTextureLeftUpper  = 3,
    kChangeTextureLeftMiddle = 4,
    kChangeTextureLeftLower  = 5,

    // the sky texture
    kChangeTextureSky = 6,

    // sector floor or ceiling
    kChangeTextureFloor   = 7,
    kChangeTextureCeiling = 8,
};

struct ScriptChangeTexturetureParameter
{
    // what to change
    ScriptChangeTexturetureType what = kChangeTextureRightUpper;

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
};

// Thing Event
struct ScriptThingEventParameter
{
    // DDF type name of thing to cause the event.  If nullptr, then the
    // thing map number is used instead.
    const char *thing_name = nullptr;
    int         thing_type = 0;
    int         thing_tag  = 0;

    // label to jump to
    const char *label  = nullptr;
    int         offset = 0;
};

// Weapon Event
struct ScriptWeaponEventParameter
{
    // DDF type name of weapon to cause the event.
    const char *weapon_name = nullptr;

    // label to jump to
    const char *label  = nullptr;
    int         offset = 0;
};

struct ScriptWeaponReplaceParameter
{
    const char *old_weapon = nullptr;
    const char *new_weapon = nullptr;
};

struct ScriptThingReplaceParameter
{
    const char *old_thing_name = nullptr;
    const char *new_thing_name = nullptr;
    int         old_thing_type = -1;
    int         new_thing_type = -1;
};

// A single RTS action, not unlike the ones for DDF things.
//
struct TriggerScriptState
{
    // link in list of states
    TriggerScriptState *next = nullptr;
    TriggerScriptState *prev = nullptr;

    // duration in tics
    int tics = 0;

    // routine to be performed
    void (*action)(struct TriggerScriptTrigger *trig, void *param) = nullptr;

    // parameter for routine, or nullptr
    void *param = nullptr;

    // state's label, or nullptr
    char *label = nullptr;
};

// Destination path name
struct TriggerScriptPath
{
    // next in list, or nullptr
    TriggerScriptPath *next = nullptr;

    const char *name = nullptr;

    // cached pointer to script
    struct TriggerScript *cached_scr = nullptr;
};

// ONDEATH info
struct ScriptOnDeathParameter
{
    // next in link (order is unimportant)
    struct ScriptOnDeathParameter *next = nullptr;

    // thing's DDF name, or if nullptr, then thing's mapnumber.
    char *thing_name = nullptr;
    int   thing_type = 0;

    // threshhold: number of things still alive before the trigger can
    // activate.  Defaults to zero (i.e. all of them must be dead).
    int threshhold = 0;

    // mobjdef pointer, computed the first time this ONDEATH condition
    // is tested.
    const MapObjectDefinition *cached_info = nullptr;
};

// ONHEIGHT info
struct ScriptOnHeightParameter
{
    // next in link (order is unimportant)
    struct ScriptOnHeightParameter *next = nullptr;

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
    Sector *cached_sector = nullptr;
};

// WAIT_UNTIL_DEAD info
struct ScriptWaitUntilDeadParameter
{
    // tag number to give the monsters which we'll wait on
    int tag = 0;

    // the DDF names of the monsters to wait for
    const char *mon_names[10] = {nullptr};
};

// Trigger Definition (Made up of actions)
// Start_Map & Radius_Trigger Declaration

struct TriggerScript
{
    // link in list
    TriggerScript *next = nullptr;
    TriggerScript *prev = nullptr;

    // Which map
    char *mapid = nullptr;

    // When appears
    AppearsFlag appear = kAppearsWhenNone;

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

    // Sector Index - Will ignore above X/Y coords and size if >= 0 and Tag is
    // also 0
    int sector_index = -1;

    // Script name (or nullptr)
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
    ScriptOnDeathParameter  *boss_trig   = nullptr;
    ScriptOnHeightParameter *height_trig = nullptr;
    ConditionCheck          *cond_trig   = nullptr;

    // Path info
    TriggerScriptPath *next_in_path    = nullptr;
    int                next_path_total = 0;

    const char *path_event_label  = nullptr;
    int         path_event_offset = 0;

    // Set of states
    TriggerScriptState *first_state = nullptr;
    TriggerScriptState *last_state  = nullptr;

    // CRC of the important parts of this RTS script.
    epi::CRC32 crc;
};

// Dynamic Trigger info.
// Goes away when trigger is finished.
struct TriggerScriptTrigger
{
    // link in list
    TriggerScriptTrigger *next = nullptr;
    TriggerScriptTrigger *prev = nullptr;

    // link for triggers with the same tag
    TriggerScriptTrigger *tag_next     = nullptr;
    TriggerScriptTrigger *tag_previous = nullptr;

    // parent info of trigger
    TriggerScript *info = nullptr;

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
    TriggerScriptState *state     = nullptr;
    int                 wait_tics = 0;

    // current tip slot (each tip slot works independently).
    int tip_slot = 0;

    // menu style name, or nullptr if not set
    const char *menu_style_name = nullptr;

    // result of last SHOW_MENU (1 to 9, or 0 when cancelled)
    int menu_result = 0;

    // Sound handle
    Position sound_effects_origin = {0, 0, 0};

    // used for WAIT_UNTIL_DEAD, normally zero
    int wud_tag   = 0;
    int wud_count = 0;

    // prevent repeating scripts from clogging the console
    const char *last_con_message = nullptr;
};

//
// Tip Displayer info
//
constexpr uint8_t kMaximumTipSlots = 45;

struct ScriptDrawTip
{
    // current properties
    ScriptTipProperties p;

    // display time.  When < 0, this slot is not in use (and all of the
    // fields below this one are unused).
    int delay;

    // do we need to recompute some stuff (e.g. colmap) ?
    bool dirty;

    // tip text DOH!
    const char  *tip_text;
    const Image *tip_graphic;

    // play a sound ?
    bool playsound;

    // scaling info (so far only for Tip_Graphic)
    float scale;

    // current colour
    RGBAColor color;

    // fading fields
    int   fade_time;
    float fade_target;
};

extern ScriptDrawTip tip_slots[kMaximumTipSlots];

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
