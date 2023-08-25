//----------------------------------------------------------------------------
//  EDGE DDF: Lines and Sectors
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2023  The EDGE Team.
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

#ifndef __DDF_LINE_H__
#define __DDF_LINE_H__

#include "epi.h"
#include "arrays.h"

#include "types.h"

// ------------------------------------------------------------------
// ------------------------LINEDEF TYPES-----------------------------
// ------------------------------------------------------------------

#define FLO_UNUSED  ((float) 3.18081979f)

// Triggers (What the line triggers)
// NOTE TO SELF: Move to an area for linedef/rts/sector stuff
typedef enum
{
	line_none,
	line_shootable,
	line_walkable,
	line_pushable,
	line_manual,   // same as pushable, but ignore any tag
	line_Any
}
trigger_e;

// Triggers (What object types can cause the line to be triggered)
// NOTE TO SELF: Move to an area for linedef/rts/sector stuff
typedef enum
{
	trig_none    = 0,
	trig_player  = 1,
	trig_monster = 2,
	trig_other   = 4,
	trig_nobot   = 8   // -AJA- 2009/10/17
}
trigacttype_e;

// Height Info Reference
// NOTE TO SELF: Move to an area for linedef/rts/sector stuff
typedef enum
{
	REF_Absolute = 0,  // Absolute from current position
	REF_Current,       // Measure from current sector height
	REF_Surrounding,   // Measure from surrounding heights
	REF_LowestLoTexture,
	REF_Trigger,       // Use the triggering linedef

	// additive flags
	REF_MASK    = 0x00FF,
	REF_CEILING = 0x0100,   // otherwise floor
	REF_HIGHEST = 0x0200,   // otherwise lowest
	REF_NEXT    = 0x0400,   // otherwise absolute
	REF_INCLUDE = 0x0800,   // otherwise excludes self
}
heightref_e;

// Movement type
typedef enum
{
	mov_undefined = 0,
	mov_Once,
	mov_MoveWaitReturn,
	mov_Continuous,
	mov_Plat,
	mov_Stairs,
	mov_Stop,
	mov_Toggle,     // -AJA- 2004/10/07: added.
	mov_Elevator,   // -AJA- 2006/11/17: added.
}
movetype_e;

// Security type: requires certain key
// NOTE TO SELF: Move to an area for linedef/rts/sector stuff
typedef enum
{
	KF_NONE = 0,

	// keep card/skull together, for easy SKCK check
	KF_BlueCard    = (1 << 0),
	KF_YellowCard  = (1 << 1),
	KF_RedCard     = (1 << 2),
	KF_GreenCard   = (1 << 3),

	KF_BlueSkull   = (1 << 4),
	KF_YellowSkull = (1 << 5),
	KF_RedSkull    = (1 << 6),
	KF_GreenSkull  = (1 << 7),

	// -AJA- 2001/06/30: ten new keys (these + Green ones)
	KF_GoldKey     = (1 << 8),
	KF_SilverKey   = (1 << 9),
	KF_BrassKey    = (1 << 10),
	KF_CopperKey   = (1 << 11),
	KF_SteelKey    = (1 << 12),
	KF_WoodenKey   = (1 << 13),
	KF_FireKey     = (1 << 14),
	KF_WaterKey    = (1 << 15),

	// this is a special flag value that indicates that _all_ of the
	// keys in the bitfield must be held.  Normally we require _any_ of
	// the keys in the bitfield to be held.
	KF_STRICTLY_ALL = (1 << 16),

	// Boom compatibility: don't care if card or skull
	KF_BOOM_SKCK = (1 << 17),

	// mask of actual key bits
	KF_CARDS  = 0x000F,
	KF_SKULLS = 0x00F0,
	KF_MASK = 0xFFFF
}
keys_e;

#define EXPAND_KEYS(set)  ((set) |  \
    (((set) & KF_CARDS) << 4) | (((set) & KF_SKULLS) >> 4))

// NOTE TO SELF: Move to an area for linedef/rts/sector stuff
typedef enum
{
	EXIT_None = 0,
	EXIT_Normal,
	EXIT_Secret,
	EXIT_Hub
}
exittype_e;

// -AJA- 1999/10/24: Reimplemented when_appear_e type.
// NOTE TO SELF: Move to an area for linedef/rts/sector stuff
typedef enum
{
	WNAP_None        = 0x0000,

	WNAP_SkillLevel1 = 0x0001,
	WNAP_SkillLevel2 = 0x0002,
	WNAP_SkillLevel3 = 0x0004,
	WNAP_SkillLevel4 = 0x0008,
	WNAP_SkillLevel5 = 0x0010,

	WNAP_Single      = 0x0100,
	WNAP_Coop        = 0x0200,
	WNAP_DeathMatch  = 0x0400,

	WNAP_SkillBits   = 0x001F,
	WNAP_NetBits     = 0x0700
}
when_appear_e;

#define DEFAULT_APPEAR  ((when_appear_e)(0xFFFF))

// -AJA- 1999/06/21: extra floor types
// FIXME!!! Move into to extrafloordef_c?
typedef enum
{
	EXFL_None = 0x0000,

	// keeps the value from being zero
	EXFL_Present = 0x0001,

	// floor is thick, has sides.  When clear: surface only
	EXFL_Thick = 0x0002,

	// floor is liquid, i.e. non-solid.  When clear: solid
	EXFL_Liquid = 0x0004,

	// can monsters see through this extrafloor ?
	EXFL_SeeThrough = 0x0010,

	// things with the WATERWALKER tag will not fall through.
	// Also, certain player sounds (pain, death) can be overridden when
	// in a water region.  Scope for other "waterish" effects...
	// 
	EXFL_Water = 0x0020,

	// the region properties will "flood" all lower regions (unless it
	// finds another flooder).
	// 
	EXFL_Flooder = 0x0040,

	// the properties (lighting etc..) below are not transferred from
	// the dummy sector, they'll be the same as the above region.
	// 
	EXFL_NoShade = 0x0080,

	// take the side texture for THICK floors from the upper part of the
	// sidedef where the thick floor is drawn (instead of tagging line).
	// 
	EXFL_SideUpper = 0x0100,

	// like above, but use the lower part.
	EXFL_SideLower = 0x0200,

	// this controls the Y offsets on normal THICK floors.
	EXFL_SideMidY = 0x0800,

	// Boom compatibility flag (for linetype 242)
	EXFL_BoomTex = 0x1000
}
extrafloor_type_e;

#define EF_DEF_THIN    ((extrafloor_type_e)(EXFL_Present | 0))
#define EF_DEF_THICK   ((extrafloor_type_e)(EXFL_Present | EXFL_Thick))
#define EF_DEF_LIQUID  ((extrafloor_type_e)(EXFL_Present | EXFL_Liquid))

// FIXME!! Move into extrafloordef_c
typedef enum
{
	// remove an extra floor
	EFCTL_None = 0,
	EFCTL_Remove
}
extrafloor_control_e;

class extrafloordef_c
{
public:
	extrafloordef_c();
	extrafloordef_c(extrafloordef_c &rhs);
	~extrafloordef_c();

private:
	void Copy(extrafloordef_c &src);

public:
	void Default(void);
	extrafloordef_c& operator=(extrafloordef_c &src);

	extrafloor_type_e type;
	extrafloor_control_e control;
};

// --> Moving plane def (Ceilings, floors and doors)
class movplanedef_c
{
public:
	movplanedef_c();
	movplanedef_c(movplanedef_c &rhs);
	~movplanedef_c();
	
	enum default_e
	{
		DEFAULT_CeilingLine,
		DEFAULT_CeilingSect,
		DEFAULT_DonutFloor,
		DEFAULT_FloorLine,
		DEFAULT_FloorSect,
		DEFAULT_Numtypes
	};
	
private:
	void Copy(movplanedef_c &src);
	
public:
	void Default(default_e def);
	movplanedef_c& operator=(movplanedef_c &rhs);

	// Type of floor: raise/lower/etc
	movetype_e type;

    // True for a ceiling, false for a floor
	bool is_ceiling;

    // How fast the plane moves.
	float speed_up;
	float speed_down;

    // This refers to what the dest. height refers to.
	heightref_e destref;

	// Destination height.
	float dest;

	// -AJA- 2001/05/28: This specifies the other height used.
	heightref_e otherref;
	float other;

    // Floor texture to change to.
	std::string tex;

	// How much crush damage to do (0 for none).
	int crush_damage;

	// PLAT/DOOR Specific: Time to wait before returning.
	int wait;
	int prewait;

    // Up/Down/Stop sfx
	struct sfx_s *sfxstart, *sfxup, *sfxdown, *sfxstop;

	// Scrolling. -AJA- 2000/04/16
	angle_t scroll_angle;
	float scroll_speed;

	// Boom compatibility bits
	bool ignore_texture;
};


// --> Sliding door definition class

// FIXME Move inside sliding_door_c?
typedef enum
{
	// not a slider
	SLIDE_None = 0,

	// door slides left (when looking at the right side)
	SLIDE_Left,

	// door slides right (when looking at the right side)
	SLIDE_Right,

	// door opens from middle
	SLIDE_Center
}
slidetype_e;

// --> Sliding Door Definition

//
// Thin Sliding Doors
//
// -AJA- 2000/08/05: added this.
//
class sliding_door_c
{
public:
	sliding_door_c();
	sliding_door_c(sliding_door_c &rhs);
	~sliding_door_c();
	
private:
	void Copy(sliding_door_c &src);
	
public:
	void Default(void);
	sliding_door_c& operator=(sliding_door_c &rhs);
	
	// type of slider, normally SLIDE_None
	slidetype_e type;

	// how fast it opens/closes
	float speed;

	// time to wait before returning (in tics).  Note: door stays open
	// after the last activation.
	int wait;

	// whether or not the texture can be seen through
	bool see_through;

	// how far it actually opens (usually 100%)
	percent_t distance;

	// sound effects.
	struct sfx_s *sfx_start;
	struct sfx_s *sfx_open;
	struct sfx_s *sfx_close;
	struct sfx_s *sfx_stop;
};

// --> Donut definition class

class donutdef_c
{
public:
	donutdef_c();
	donutdef_c(donutdef_c &rhs);
	~donutdef_c();
	
private:
	void Copy(donutdef_c &src);
	
public:
	void Default(void);
	donutdef_c& operator=(donutdef_c &rhs);
	
	// Do Donut?

	//
	// FIXME! Make the objects that use this require
	//        a pointer/ref. This becomes an
	//        therefore becomes an unnecessary entry
	//
	bool dodonut;

	// FIXME! Strip out the d_ since we're not trying to
	// to differentiate them now?
	 
	// SFX for inner donut parts
	struct sfx_s *d_sfxin, *d_sfxinstop;

	// SFX for outer donut parts
	struct sfx_s *d_sfxout, *d_sfxoutstop;
};

// -AJA- 1999/07/12: teleporter special flags.
// FIXME!! Move into teleport def class?
typedef enum
{
	TELSP_None       = 0,
	
	TELSP_Relative   = 0x0001,  // keep same relative angle
	TELSP_SameHeight = 0x0002,  // keep same height off the floor
	TELSP_SameSpeed  = 0x0004,  // keep same momentum
	TELSP_SameOffset = 0x0008,  // keep same X/Y offset along line

    TELSP_SameAbsDir = 0x0010,  // keep same _absolute_ angle (DEPRECATED)
    TELSP_Rotate     = 0x0020,  // rotate by target angle     (DEPRECATED)

	TELSP_Line       = 0x0100,  // target is a line (not a thing)
	TELSP_Flipped    = 0x0200,  // pretend target was flipped 180 degrees
	TELSP_Silent     = 0x0400   // no fog or sound
}
teleportspecial_e;

// --> Teleport def class

class teleportdef_c
{
public:
	teleportdef_c();
	teleportdef_c(teleportdef_c &rhs);
	~teleportdef_c();
	
private:
	void Copy(teleportdef_c &src);
	
public:
	void Default(void);
	teleportdef_c& operator=(teleportdef_c &rhs);

	// If true, teleport activator
	//
	// FIXME! Make the objects that use this require
	//        a pointer/ref. This
	//        therefore becomes an unnecessary entry
	//
	bool teleport;

  	// effect object spawned when going in...
	const mobjtype_c *inspawnobj;	// FIXME! Do mobjtypes.Lookup()?
	std::string inspawnobj_ref;

  	// effect object spawned when going out...
	const mobjtype_c *outspawnobj;	// FIXME! Do mobjtypes.Lookup()?
	std::string outspawnobj_ref;

  	// Teleport delay
	int delay;

	// Special flags.
	teleportspecial_e special;
};

// Light Specials
typedef enum
{
	LITE_None,

	// set light to new level instantly
	LITE_Set,

	// fade light to new level over time
	LITE_Fade,

	// flicker like a fire
	LITE_FireFlicker,

	// smoothly fade between bright and dark, continously
	LITE_Glow,

	// blink randomly between bright and dark
	LITE_Flash,

	// blink between bright and dark, alternating
	LITE_Strobe
}
lighttype_e;

// --> Light information class
class lightdef_c
{
public:
	lightdef_c();
	lightdef_c(lightdef_c &rhs);
	~lightdef_c();
	
private:
	void Copy(lightdef_c &src);
	
public:
	void Default(void);
	lightdef_c& operator=(lightdef_c &rhs);
	
	lighttype_e type;

	// light level to change to (for SET and FADE)
	int level;

	// chance value for FLASH type
	percent_t chance;
  
	// time remaining dark and bright, in tics
	int darktime;
	int brighttime;

	// synchronisation time, in tics
	int sync;

	// stepping used for FADE and GLOW types
	int step;
};

// --> Ladder definition class (Probably somewhat OTT)
 
class ladderdef_c
{
public:
	ladderdef_c();
	ladderdef_c(ladderdef_c &rhs);
	~ladderdef_c();
	
private:
	void Copy(ladderdef_c &src);

public:
	void Default(void);
	ladderdef_c& operator=(ladderdef_c &rhs);

	// height of ladder itself.  Zero or negative disables.  Bottom of
	// ladder comes from Y_OFFSET on the linedef.
	float height;
};

typedef enum
{
	LINEFX_NONE = 0,

	// make tagged lines (inclusive) 50% translucent
	LINEFX_Translucency = (1 << 0),

	// make tagged walls (inclusive) scroll using vector
	LINEFX_VectorScroll = (1 << 1),

	// make source line scroll using sidedef offsets
	LINEFX_OffsetScroll = (1 << 2),

	// experimental: tagged walls (inclusive) scaling & skewing
	LINEFX_Scale = (1 << 3),
	LINEFX_Skew  = (1 << 4),

	// experimental: transfer properties to tagged walls (incl)
	LINEFX_LightWall = (1 << 5),

	// experimental: make tagged lines (exclusive) non-blocking
	LINEFX_UnblockThings = (1 << 6),

	// experimental: make tagged lines (incl) block bullets/missiles
	LINEFX_BlockShots = (1 << 7),

	// experimental: make tagged lines (incl) block monster sight
	LINEFX_BlockSight = (1 << 8),
	
	// experimental: transfer upper texture to SKY
	LINEFX_SkyTransfer = (1 << 9),

	// make all tagged lines scroll using this sidedef's offsets (MBF21)
	LINEFX_TaggedOffsetScroll = (1 << 10),

	// block land monsters (MBF21)
	LINEFX_BlockGroundedMonsters = (1 << 11),

	// block players (MBF21)
	LINEFX_BlockPlayers = (1 << 12),
}
line_effect_type_e;

typedef enum
{
	SECTFX_None = 0,

	// transfer sector lighting to tagged floors/ceilings
	SECTFX_LightFloor   = (1 << 0),
	SECTFX_LightCeiling = (1 << 1),

	// make tagged floors/ceilings scroll
	SECTFX_ScrollFloor   = (1 << 2),
	SECTFX_ScrollCeiling = (1 << 3),

	// push things on tagged floor
	SECTFX_PushThings = (1 << 4),

	// restore light/scroll/push in tagged floors/ceilings
	SECTFX_ResetFloor   = (1 << 6),
	SECTFX_ResetCeiling = (1 << 7),

	// set floor/ceiling texture scale
	SECTFX_ScaleFloor   = (1 << 8),
	SECTFX_ScaleCeiling = (1 << 9),

	// align floor/ceiling texture to line
	SECTFX_AlignFloor   = (1 << 10),
	SECTFX_AlignCeiling = (1 << 11),

	// set various force parameters
	SECTFX_SetFriction  = (1 << 12),
	SECTFX_WindForce    = (1 << 13),
	SECTFX_CurrentForce = (1 << 14),
	SECTFX_PointForce   = (1 << 15),

	// BOOM's linetype 242 -- deep water effect (etc)
	SECTFX_BoomHeights  = (1 << 16)
}
sector_effect_type_e;

typedef enum
{
	PORTFX_None = 0,

	PORTFX_Standard = (1 << 0),
	PORTFX_Mirror   = (1 << 1),
	PORTFX_Camera   = (1 << 2),
}
portal_effect_type_e;

// -AJA- 2008/03/08: slope types
typedef enum
{
	SLP_NONE = 0,

	SLP_DetailFloor   = (1 << 0),
	SLP_DetailCeiling = (1 << 1),
}
slope_type_e;

// -AJA- 1999/10/12: Generalised scrolling parts of walls.
typedef enum
{
	SCPT_None  = 0,

	SCPT_RightUpper  = 0x0001,
	SCPT_RightMiddle = 0x0002,
	SCPT_RightLower  = 0x0004,

	SCPT_LeftUpper  = 0x0010,
	SCPT_LeftMiddle = 0x0020,
	SCPT_LeftLower  = 0x0040,

	SCPT_LeftRevX   = 0x0100,
	SCPT_LeftRevY   = 0x0200
}
scroll_part_e;

#define SCPT_RIGHT  ((scroll_part_e)(SCPT_RightUpper | SCPT_RightMiddle | SCPT_RightLower))
#define SCPT_LEFT   ((scroll_part_e)(SCPT_LeftUpper | SCPT_LeftMiddle | SCPT_LeftLower))

// -AJA- 1999/12/07: Linedef special flags
typedef enum
{
	LINSP_None = 0,

	// player must be able to vertically reach this linedef to press it
	LINSP_MustReach = (1 << 0),

	// don't change the texture on other linedefs with the same tag
	LINSP_SwitchSeparate = (1 << 1),

	// -AJA- 2007/09/14: for SECTOR_EFFECT with no tag
	LINSP_BackSector = (1 << 2),
}
line_special_e;

// BOOM scroll types (didn't want to eat up flags elsewhere)
typedef enum
{
	ScrollType_None  = 0,
	ScrollType_Displace = (1 << 0),
	ScrollType_Accel = (1 << 1),
}
scroll_type_e;

// --> Line definition type class

class linetype_c
{
public:
	linetype_c();
	~linetype_c();
	
public:
	void Default(void);
	void CopyDetail(linetype_c &src);
	
	// Member vars....
	int number;
	
    // Linedef will change to this.
	int newtrignum;

	// Determines whether line is shootable/walkable/pushable
	trigger_e type;

	// Determines whether line is acted on by monsters/players/projectiles
	trigacttype_e obj;

	// Keys required to use
	keys_e keys;

	// Number of times this line can be triggered. -1 = Any amount
	int count;

	// Floor
	movplanedef_c f;

	// Ceiling
	movplanedef_c c;

	// Donut
	donutdef_c d;

	// Slider
	sliding_door_c s;

	// Ladder -AJA- 2001/03/10
	ladderdef_c ladder;

	// Teleporter
	teleportdef_c t;
	
	//Lobo: item to spawn (or NULL).  The mobjdef pointer is only valid after
	// DDF_MobjCleanUp() has been called.
	const mobjtype_c *effectobject;
	std::string effectobject_ref;
	
	// Handle this line differently
	bool glass;
	
	// line texture to change to.
	std::string brokentex;

	// LIGHT SPECIFIC
	// Things may be added here; start strobing/flashing glowing lights.
	lightdef_c l;

    // EXIT SPECIFIC
	exittype_e e_exit;
	int hub_exit;

	// SCROLLER SPECIFIC
	float s_xspeed;
	float s_yspeed;
	scroll_part_e scroll_parts;

	// -ACB- 1998/09/11 Message handling
	std::string failedmessage;

    // -AJA- 2011/01/14: sound for unusable locked door
	struct sfx_s *failed_sfx;

	// Colourmap changing
	// -AJA- 1999/07/09: Now uses colmap.ddf
	const colourmap_c *use_colourmap;

	// Property Transfers (FLO_UNUSED if unset)
	float gravity;
	float friction;
	float viscosity;
	float drag;

    // Ambient sound transfer
	struct sfx_s *ambient_sfx;

	// Activation sound (overrides the switch sound)
	struct sfx_s *activate_sfx;

	int music;

    // Automatically trigger this line at level start ?
	bool autoline;

	// Activation only possible from right side of line
	bool singlesided;

	// -AJA- 1999/06/21: Extra floor handling
	extrafloordef_c ef;

	// -AJA- 1999/06/30: TRANSLUCENT MID-TEXTURES
	percent_t translucency;

	// -AJA- 1999/10/24: Appearance control.
	when_appear_e appear;

	// -AJA- 1999/12/07: line special flags
	line_special_e special_flags;

	// -AJA- 2000/01/09: enable (if +1) or disable (if -1) all radius
	//       triggers with the same tag as the linedef.
	int trigger_effect;

    // -AJA- 2000/09/28: BOOM compatibility fields (and more !).
	line_effect_type_e line_effect;
	scroll_part_e line_parts;
	scroll_type_e scroll_type;

	sector_effect_type_e sector_effect;
	portal_effect_type_e portal_effect;

	slope_type_e slope_type;

	// -AJA- 2007/07/05: color for effects (e.g. MIRRORs)
	rgbcol_t fx_color;

private:
	// disable copy construct and assignment operator
	explicit linetype_c(linetype_c &rhs) { (void) rhs; }
	linetype_c& operator= (linetype_c &rhs) { (void) rhs; return *this; }
};


// --> Linetype container class

class linetype_container_c : public epi::array_c
{
public:
	linetype_container_c();
	~linetype_container_c();

private:
	void CleanupObject(void *obj);

	linetype_c* lookup_cache[LOOKUP_CACHESIZE];

public:
	// List Management
	int GetSize() {	return array_entries; } 
	int Insert(linetype_c *l) { return InsertObject((void*)&l); }
	linetype_c* Lookup(int num);
	linetype_c* operator[](int idx) { return *(linetype_c**)FetchObject(idx); }
	void Reset();
};

// ------------------------------------------------------------------
// -------------------------SECTOR TYPES-----------------------------
// ------------------------------------------------------------------

// -AJA- 1999/11/25: Sector special flags
typedef enum
{
	SECSP_None = 0x0000,

	// apply damage whenever in whole region (not just touching floor)
	SECSP_WholeRegion = 0x0001,

	// goes with above: damage is proportional to how deep you're in
	// Also affects pushing sectors.
	SECSP_Proportional = 0x0002,

	// push _all_ things, including NOGRAVITY ones
	SECSP_PushAll = 0x0008,

	// the push force is constant, regardless of the mass
	SECSP_PushConstant = 0x0010,

	// breathing support: this sector contains no air.
	SECSP_AirLess = 0x0020,

	// player can swim in this sector
	SECSP_Swimming = 0x0040,

	// sounds will apply underwater effects in this sector
	SECSP_SubmergedSFX = 0x0080,

	// sounds will be heavily muffled in this sector
	SECSP_VacuumSFX = 0x0100,

	// sounds will reverberate/echo in this sector
	SECSP_ReverbSFX = 0x0200
}
sector_flag_e;

class sectortype_c
{
public:
	sectortype_c();
	~sectortype_c();
	
public:
	void Default(void);
	void CopyDetail(sectortype_c &src);

	// Member vars....
	int number;

    // This sector gives you secret count
	bool secret;
	bool crush;

	// Hub entry, player starts are treated differently
	bool hub;

	// Gravity
	float gravity;
	float friction;
	float viscosity;
	float drag;

	// Movement
	movplanedef_c f, c;

	// Lighting
	lightdef_c l;

	// Slime
	damage_c damage;

	// -AJA- 1999/11/25: sector special flags
	sector_flag_e special_flags;

	// Exit.  Also disables god mode.
	exittype_e e_exit;

	// Colourmap changing
	// -AJA- 1999/07/09: Now uses colmap.ddf
	const colourmap_c *use_colourmap;

    // Ambient sound transfer
	struct sfx_s *ambient_sfx;

	// -AJA- 2008/01/20: Splash sounds
	struct sfx_s *splash_sfx;

	// -AJA- 1999/10/24: Appearance control.
	when_appear_e appear;

    // -AJA- 2000/04/16: Pushing (fixed direction).
	float push_speed;
	float push_zspeed;
	angle_t push_angle;

	// Dasho 2022 - Params for user-defined reverb in sectors
	std::string reverb_type;
	float reverb_ratio;
	float reverb_delay;

	float floor_bob;
	float ceiling_bob;

	colourmap_c *fog_cmap;
	rgbcol_t fog_color;
	float fog_density;

private:
	// disable copy construct and assignment operator
	explicit sectortype_c(sectortype_c &rhs) { (void) rhs; }
	sectortype_c& operator= (sectortype_c &rhs) { (void) rhs; return *this; }
};


// --> Sectortype container class

class sectortype_container_c : public epi::array_c
{
public:
	sectortype_container_c();
	~sectortype_container_c();

private:
	void CleanupObject(void *obj);

	sectortype_c* lookup_cache[LOOKUP_CACHESIZE];

public:
	// List Management
	int GetSize() {	return array_entries; } 
	int Insert(sectortype_c *s) { return InsertObject((void*)&s); }
	sectortype_c* Lookup(int num);
	sectortype_c* operator[](int idx) { return *(sectortype_c**)FetchObject(idx); }
	void Reset();
};


/* EXTERNALISATIONS */

extern linetype_container_c linetypes;		// -ACB- 2004/07/05 Implemented
extern sectortype_container_c sectortypes;	// -ACB- 2004/07/05 Implemented

void DDF_ReadLines(const std::string& data);
void DDF_ReadSectors(const std::string& data);

#endif // __DDF_LINE_H__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
