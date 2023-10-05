//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Main)
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

#ifndef __DDF_MOBJ_H__
#define __DDF_MOBJ_H__

#include "epi.h"
#include "arrays.h"

#include "types.h"
#include "states.h"


// special 'number' value which signifies that the mobjtype_c
// forms part of an ATTACKS.DDF entry.
#define ATTACK__MOBJ  -7777


#define DLIT_COMPAT_RAD(x)  (10.0f * sqrt(x))
#define DLIT_COMPAT_ITY   0.8f


//
// Misc. mobj flags
//
typedef enum
{
	// Call P_TouchSpecialThing when touched.
	MF_SPECIAL = (1 << 0),

	// Blocks.
	MF_SOLID = (1 << 1),

	// Can be hit.
	MF_SHOOTABLE = (1 << 2),

	// Don't use the sector links (invisible but touchable).
	MF_NOSECTOR = (1 << 3),

	// Don't use the blocklinks (inert but displayable)
	MF_NOBLOCKMAP = (1 << 4),

	// Not to be activated by sound, deaf monster.
	MF_AMBUSH = (1 << 5),

	// Will try to attack right back.
	MF_JUSTHIT = (1 << 6),

	// Will take at least one step before attacking.
	MF_JUSTATTACKED = (1 << 7),

	// On level spawning (initial position),
	// hang from ceiling instead of stand on floor.
	MF_SPAWNCEILING = (1 << 8),

	// Don't apply gravity (every tic), that is, object will float,
	// keeping current height or changing it actively.
	MF_NOGRAVITY = (1 << 9),

	// Movement flags. This allows jumps from high places.
	MF_DROPOFF = (1 << 10),

	// For players, will pick up items.
	MF_PICKUP = (1 << 11),

	// Object is not checked when moving, no clipping is used.
	MF_NOCLIP = (1 << 12),

	// Player: keep info about sliding along walls.
	MF_SLIDE = (1 << 13),

	// Allow moves to any height, no gravity.
	// For active floaters, e.g. cacodemons, pain elementals.
	MF_FLOAT = (1 << 14),

	// Instantly cross lines, whatever the height differences may be
	// (e.g. go from the bottom of a cliff to the top).
	// Note: nothing to do with teleporters.
	MF_TELEPORT = (1 << 15),

	// Don't hit same species, explode on block.
	// Player missiles as well as fireballs of various kinds.
	MF_MISSILE = (1 << 16),

	// Dropped by a demon, not level spawned.
	// E.g. ammo clips dropped by dying former humans.
	MF_DROPPED = (1 << 17),

	// Use fuzzy draw (shadow demons or spectres),
	// temporary player invisibility powerup.
	MF_FUZZY = (1 << 18),

	// Flag: don't bleed when shot (use puff),
	// barrels and shootable furniture shall not bleed.
	MF_NOBLOOD = (1 << 19),

	// Don't stop moving halfway off a step,
	// that is, have dead bodies slide down all the way.
	MF_CORPSE = (1 << 20),

	// Floating to a height for a move, ???
	// don't auto float to target's height.
	MF_INFLOAT = (1 << 21),

	// On kill, count this enemy object
	// towards intermission kill total.
	// Happy gathering.
	MF_COUNTKILL = (1 << 22),

	// On picking up, count this item object
	// towards intermission item total.
	MF_COUNTITEM = (1 << 23),

	// Special handling: skull in flight.
	// Neither a cacodemon nor a missile.
	MF_SKULLFLY = (1 << 24),

	// Don't spawn this object
	// in death match mode (e.g. key cards).
	MF_NOTDMATCH = (1 << 25),

	// Monster grows (in)visible at certain times.
	MF_STEALTH = (1 << 26),

	// For projectiles: keep momentum of projectile source
	MF_PRESERVEMOMENTUM = (1 << 27),

	// Object reacts to being touched (often violently :->)
	MF_TOUCHY = (1 << 28),
}
mobjflag_t;

typedef enum
{
	// -AJA- 2004/07/22: ignore certain types of damage
	// (previously was the EF_BOSSMAN flag).
	EF_EXPLODEIMMUNE = (1 << 0),

	// Used when varying visibility levels
	EF_LESSVIS = (1 << 1),

	// This thing does not respawn
	EF_NORESPAWN = (1 << 2),

	// double the chance of object using range attack
	EF_NOGRAVKILL = (1 << 3),

	// This thing is not loyal to its own type, fights its own
	EF_DISLOYALTYPE = (1 << 4),

	// This thing can be hurt by another thing with same attack
	EF_OWNATTACKHURTS = (1 << 5),

	// Used for tracing (homing) projectiles, its the first time
	// this projectile has been checked for tracing if set.
	EF_FIRSTCHECK = (1 << 6),

	// NO LONGER USED (1 << 7),  // was: NOTRACE

	// double the chance of object using range attack
	EF_TRIGGERHAPPY = (1 << 8),

	// not targeted by other monsters for damaging them
	EF_NEVERTARGET = (1 << 9),

	// Normally most monsters will follow a target which caused them
	// damage for a length of time, even if another object inflicted
	// pain upon them; with this enabled, they will not hold the grudge
	// and switch targets to the other object that has caused them the
	// more recent pain.
	EF_NOGRUDGE = (1 << 10),

	// NO LONGER USED (1 << 11),  // was: DUMMYMOBJ

	// Archvile cannot resurrect this monster
	EF_NORESURRECT = (1 << 12),

	// Object bounces
	EF_BOUNCE = (1 << 13),

	// Thing walks along the edge near large dropoffs. 
	EF_EDGEWALKER = (1 << 14),

	// Monster falls with gravity when walks over cliff. 
	EF_GRAVFALL =  (1 << 15),

	// Thing can be climbed on-top-of or over. 
	EF_CLIMBABLE = (1 << 16),

	// Thing won't penetrate WATER extra floors. 
	EF_WATERWALKER = (1 << 17),

	// Thing is a monster. 
	EF_MONSTER = (1 << 18),

	// Thing can cross blocking lines.
	EF_CROSSLINES = (1 << 19),

	// Thing is never affected by friction
	EF_NOFRICTION = (1 << 20),

	// Thing is optional, won't exist when -noextra is used.
	EF_EXTRA = (1 << 21),

	// Just bounced, won't enter bounce states until BOUNCE_REARM.
	EF_JUSTBOUNCED = (1 << 22),

	// Thing can be "used" (like linedefs) with the spacebar.  Thing
	// will then enter its TOUCH_STATES (when they exist).
	EF_USABLE = (1 << 23),

	// Thing will block bullets and missiles.  -AJA- 2000/09/29
	EF_BLOCKSHOTS = (1 << 24),

	// Player is currently crouching.  -AJA- 2000/10/19
	EF_CROUCHING = (1 << 25),

	// Missile can tunnel through enemies.  -AJA- 2000/10/23
	EF_TUNNEL = (1 << 26),

	// NO LONGER USED (1 << 27),  // was: DLIGHT

	// Thing has been gibbed.
	EF_GIBBED = (1 << 28),

	// -AJA- 2004/07/22: play the monster sounds at full volume
	// (separated out from the BOSSMAN flag).
	EF_ALWAYSLOUD = (1 << 29),
}
mobjextendedflag_t;

#define EF_SIMPLEARMOUR  EF_TRIGGERHAPPY

typedef enum
{
	// -AJA- 2004/08/25: always pick up this item
	HF_FORCEPICKUP = (1 << 0),

	// -AJA- 2004/09/02: immune from friendly fire
	HF_SIDEIMMUNE = (1 << 1),

	// -AJA- 2005/05/15: friendly fire passes through you
	HF_SIDEGHOST = (1 << 2),

	// -AJA- 2004/09/02: don't retaliate if hurt by friendly fire
	HF_ULTRALOYAL = (1 << 3),

	// -AJA- 2005/05/14: don't update the Z buffer (particles).
	HF_NOZBUFFER = (1 << 4),

	// -AJA- 2005/05/15: the sprite hovers up and down
	HF_HOVER = (1 << 5),

	// -AJA- 2006/08/17: object can be pushed (wind/current/point)
	HF_PUSHABLE = (1 << 6),

	// -AJA- 2006/08/17: used by MT_PUSH and MT_PULL objects
	HF_POINT_FORCE = (1 << 7),

	// -AJA- 2006/10/19: scenery items don't block missiles
	HF_PASSMISSILE = (1 << 8),

	// -AJA- 2007/11/03: invulnerable flag
	HF_INVULNERABLE = (1 << 9),

	// -AJA- 2007/11/06: gain health when causing damage
	HF_VAMPIRE = (1 << 10),

	// -AJA- 2008/01/11: compatibility for quadratic dlights
	HF_QUADRATIC_COMPAT = (1 << 11),

	// -AJA- 2009/10/15: HUB system: remember old avatars
	HF_OLD_AVATAR = (1 << 12),

	// -AJA- 2009/10/22: never autoaim at this monster/thing
	HF_NO_AUTOAIM = (1 << 13),

	// -AJA- 2010/06/13: used for RTS command of same name
	HF_WAIT_UNTIL_DEAD = (1 << 14),

	// -AJA- 2010/12/23: force models to tilt by viewangle
	HF_TILT = (1 << 15),
	
	// -Lobo- 2021/10/24: immortal flag
	HF_IMMORTAL = (1 << 16),
	
	// -Lobo- 2021/11/18: floorclip flag
	HF_FLOORCLIP = (1 << 17),

	// -Lobo- 2022/05/30: this thing cannot trigger lines
	HF_NOTRIGGERLINES = (1 << 18),

	// -Lobo- 2022/07/07: this thing can be shoved/pushed
	HF_SHOVEABLE = (1 << 19),

	// -Lobo- 2022/07/07: this thing doesn't cause splashes
	HF_NOSPLASH = (1 << 20),

	// -AJA- 2022/10/04: used by DEH_EDGE to workaround issues
	HF_DEHACKED_COMPAT = (1 << 21),
}
mobjhyperflag_t;

// MBF21 flags not already covered by extended/hyper flags
typedef enum
{
	// Gravity affects this thing as if it were 1/8 of the normal value
	MBF21_LOGRAV = (1 << 0),

	MBF21_SHORTMRANGE = (1 << 1),

	MBF21_FORCERADIUSDMG = (1 << 4),

	MBF21_LONGMELEE = (1 << 8),
}
mobjmbf21flag_t;

#define NUM_FX_SLOT  30


// ------------------------------------------------------------------
// ------------------------BENEFIT TYPES-----------------------------
// ------------------------------------------------------------------

typedef enum
{
	BENEFIT_None = 0,
	BENEFIT_Ammo,
	BENEFIT_AmmoLimit,
	BENEFIT_Weapon,
	BENEFIT_Key,
	BENEFIT_Health,
	BENEFIT_Armour,
	BENEFIT_Powerup,
	BENEFIT_Inventory,
	BENEFIT_InventoryLimit,
	BENEFIT_Counter,
	BENEFIT_CounterLimit
}
benefit_type_e;

// Ammunition types defined.
typedef enum
{
	AM_DontCare = -2,  // Only used for P_SelectNewWeapon()
	AM_NoAmmo   = -1,  // Unlimited for chainsaw / fist.
  
	AM_Bullet = 0, // Pistol / chaingun ammo.
	AM_Shell,      // Shotgun / double barreled shotgun.
	AM_Rocket,     // Missile launcher.
	AM_Cell,       // Plasma rifle, BFG.

	// New ammo types
	AM_Pellet,
	AM_Nail,
	AM_Grenade,
	AM_Gas,

	AM_9, AM_10,
	AM_11, AM_12,	AM_13,	AM_14,	AM_15,
	AM_16, AM_17,	AM_18,	AM_19,	AM_20,
	AM_21, AM_22,	AM_23,	AM_24,	AM_25,
	AM_26, AM_27,	AM_28, AM_29,	AM_30,
	AM_31, AM_32,	AM_33, AM_34,	AM_35,
	AM_36, AM_37,	AM_38, AM_39,	AM_40,
	AM_41, AM_42,	AM_43, AM_44,	AM_45,
	AM_46, AM_47,	AM_48, AM_49,	AM_50,
	AM_51, AM_52,	AM_53, AM_54,	AM_55,
	AM_56, AM_57,	AM_58, AM_59,	AM_60,
	AM_61, AM_62,	AM_63, AM_64,	AM_65,
	AM_66, AM_67,	AM_68, AM_69,	AM_70,
	AM_71, AM_72,	AM_73, AM_74,	AM_75,
	AM_76, AM_77,	AM_78, AM_79,	AM_80,
	AM_81, AM_82,	AM_83, AM_84,	AM_85,
	AM_86, AM_87,	AM_88, AM_89,	AM_90,
	AM_91, AM_92,	AM_93, AM_94,	AM_95,
	AM_96, AM_97,	AM_98, AM_99,

	NUMAMMO  // Total count (99)
}
ammotype_e;

// Inventory types defined.
typedef enum
{
	INV_01,	INV_02,	INV_03,	INV_04,	INV_05,
	INV_06,	INV_07,	INV_08,	INV_09,	INV_10,
	INV_11,	INV_12,	INV_13,	INV_14,	INV_15,
	INV_16,	INV_17,	INV_18,	INV_19,	INV_20,
	INV_21,	INV_22,	INV_23,	INV_24,	INV_25,
	INV_26, INV_27,	INV_28, INV_29,	INV_30,
	INV_31, INV_32,	INV_33, INV_34,	INV_35,
	INV_36, INV_37,	INV_38, INV_39,	INV_40,
	INV_41, INV_42,	INV_43, INV_44,	INV_45,
	INV_46, INV_47,	INV_48, INV_49,	INV_50,
	INV_51, INV_52,	INV_53, INV_54,	INV_55,
	INV_56, INV_57,	INV_58, INV_59,	INV_60,
	INV_61, INV_62,	INV_63, INV_64,	INV_65,
	INV_66, INV_67,	INV_68, INV_69,	INV_70,
	INV_71, INV_72,	INV_73, INV_74,	INV_75,
	INV_76, INV_77,	INV_78, INV_79,	INV_80,
	INV_81, INV_82,	INV_83, INV_84,	INV_85,
	INV_86, INV_87,	INV_88, INV_89,	INV_90,
	INV_91, INV_92,	INV_93, INV_94,	INV_95,
	INV_96, INV_97,	INV_98, INV_99,
	NUMINV  // Total count (99)
}
invtype_e;

// Counter types defined.
typedef enum
{
	CT_Lives = 0, // Arbitrarily named Lives counter
	CT_Score,      // Arbitrarily named Score counter
	CT_Money,     // Arbitrarily named Money
	CT_Experience,       // Arbitrarily named EXP counter

	COUNT_05,
	COUNT_06, COUNT_07,	COUNT_08, COUNT_09,	COUNT_10,
	COUNT_11, COUNT_12,	COUNT_13, COUNT_14,	COUNT_15,
	COUNT_16, COUNT_17,	COUNT_18, COUNT_19,	COUNT_20,
	COUNT_21, COUNT_22,	COUNT_23, COUNT_24,	COUNT_25,
	COUNT_26, COUNT_27,	COUNT_28, COUNT_29,	COUNT_30,
	COUNT_31, COUNT_32,	COUNT_33, COUNT_34,	COUNT_35,
	COUNT_36, COUNT_37,	COUNT_38, COUNT_39,	COUNT_40,
	COUNT_41, COUNT_42,	COUNT_43, COUNT_44,	COUNT_45,
	COUNT_46, COUNT_47,	COUNT_48, COUNT_49,	COUNT_50,
	COUNT_51, COUNT_52,	COUNT_53, COUNT_54,	COUNT_55,
	COUNT_56, COUNT_57,	COUNT_58, COUNT_59,	COUNT_60,
	COUNT_61, COUNT_62,	COUNT_63, COUNT_64,	COUNT_65,
	COUNT_66, COUNT_67,	COUNT_68, COUNT_69,	COUNT_70,
	COUNT_71, COUNT_72,	COUNT_73, COUNT_74,	COUNT_75,
	COUNT_76, COUNT_77,	COUNT_78, COUNT_79,	COUNT_80,
	COUNT_81, COUNT_82,	COUNT_83, COUNT_84,	COUNT_85,
	COUNT_86, COUNT_87,	COUNT_88, COUNT_89,	COUNT_90,
	COUNT_91, COUNT_92,	COUNT_93, COUNT_94,	COUNT_95,
	COUNT_96, COUNT_97,	COUNT_98, COUNT_99,
	NUMCOUNTER  // Total count (99)
}
countertype_e;

typedef enum
{
	// weak armour, saves 33% of damage
	ARMOUR_Green = 0,

    // better armour, saves 50% of damage
	ARMOUR_Blue,

	// -AJA- 2007/08/22: another one, saves 66% of damage  (not in Doom)
	ARMOUR_Purple,

	// good armour, saves 75% of damage  (not in Doom)
	ARMOUR_Yellow,

	// the best armour, saves 90% of damage  (not in Doom)
	ARMOUR_Red,

	NUMARMOUR
}
armour_type_e;

#define ARMOUR_Total  (NUMARMOUR+0)  // -AJA- used for conditions

typedef short armour_set_t;  // one bit per armour

// Power up artifacts.
//
// -MH- 1998/06/17  Jet Pack Added
// -ACB- 1998/07/15 NightVision Added

typedef enum
{
	PW_Invulnerable = 0,
	PW_Berserk,
	PW_PartInvis,
	PW_AcidSuit,
	PW_AllMap,
	PW_Infrared,

	// extra powerups (not in Doom)
	PW_Jetpack,     // -MH- 1998/06/18  jetpack "fuel" counter
	PW_NightVision,
	PW_Scuba,

	PW_TimeStop,
	PW_Unused10,
	PW_Unused11,

	PW_Unused12,
	PW_Unused13,
	PW_Unused14,
	PW_Unused15,

	// -AJA- Note: Savegame code relies on NUMPOWERS == 16.
	NUMPOWERS
}
power_type_e;

typedef struct benefit_s
{
	// next in linked list
	struct benefit_s *next;

    // type of benefit (ammo, ammo-limit, weapon, key, health, armour,
    // powerup, inventory, or inventory-limit).
	benefit_type_e type;
  
	// sub-type (specific type of ammo, weapon, key, powerup, or inventory).  For
	// armour this is the class, for health it is unused.
	union
	{
		int type;
		weapondef_c *weap;
	}
	sub;

    // amount of benefit (e.g. quantity of ammo or health).  For weapons
    // and keys, this is a boolean value: 1 to give, 0 to ignore.  For
    // powerups, it is number of seconds the powerup lasts.
	float amount;

	// for health, armour and powerups, don't make the new value go
	// higher than this (if it is already higher, prefer not to pickup
	// the object).
	float limit;
}
benefit_t;

typedef enum
{
	PUFX_None = 0,
	PUFX_PowerupEffect,
	PUFX_ScreenEffect,
	PUFX_SwitchWeapon,
	PUFX_KeepPowerup
}
pickup_effect_type_e;

class pickup_effect_c
{
public:
	pickup_effect_c(pickup_effect_type_e _type, int _sub, int _slot, float _time);
	pickup_effect_c(pickup_effect_type_e _type, weapondef_c *_weap, int _slot, float _time);
	~pickup_effect_c() { }

	// next in linked list
	pickup_effect_c *next;

	// type and optional sub-type
	pickup_effect_type_e type;

	union
	{
		int type;
		weapondef_c *weap;
	}
	sub;

	// which slot to use
	int slot;

    // how long for the effect to last (in seconds).
	float time;
};

// -ACB- 2003/05/15: Made enum external to structure, caused different issues with gcc and VC.NET compile
typedef enum
{
	// dummy condition, used if parsing failed
	COND_NONE = 0,
    
    // object must have health
	COND_Health,

	// player must have armour (subtype is ARMOUR_* value)
	COND_Armour,

	// player must have a key (subtype is KF_* value).
	COND_Key,

	// player must have a weapon (subtype is slot number).
	COND_Weapon,

	// player must have a powerup (subtype is PW_* value).
	COND_Powerup,

	// player must have ammo (subtype is AM_* value)
	COND_Ammo,
	
	// player must have inventory (subtype is INVENTORY* value)
	COND_Inventory,

	// player must have inventory (subtype is COUNTER* value)
	COND_Counter,

	// player must be jumping
	COND_Jumping,

	// player must be crouching
	COND_Crouching,

	// object must be swimming (i.e. in water)
	COND_Swimming,

	// player must be attacking (holding fire down)
	COND_Attacking,

	// player must be rampaging (holding fire a long time)
	COND_Rampaging,

	// player must be using (holding space down)
	COND_Using,

	// player must be pressing an Action key
	COND_Action1,
	COND_Action2,

	// player must be walking
	COND_Walking
}
condition_check_type_e;

typedef struct condition_check_s
{
	// next in linked list (order is unimportant)
	struct condition_check_s *next = nullptr;

	// negate the condition
	bool negate = false;

	// condition is looking for an exact value (not "greater than" or "smaller than")
	bool exact = false;

	// condition typing. -ACB- 2003/05/15: Made an integer to hold condition_check_type_e enumeration
	int cond_type = 0;

	// sub-type (specific type of ammo, weapon, key, powerup, inventory).  Not used
	// for health, jumping, crouching, etc.
	union
	{
		int type;
		weapondef_c *weap;
	}
	sub;

	// required amount of health, armour, ammo, inventory or "counter",   Not used for
	// weapon, key, powerup, jumping, crouching, etc.
	float amount = 0;
}
condition_check_t;

// ------------------------------------------------------------------
// --------------------MOVING OBJECT INFORMATION---------------------
// ------------------------------------------------------------------

//
// -ACB- 2003/05/15: Moved this outside of damage_c. GCC and VC.NET have 
// different- and conflicting - issues with structs in structs
//
// override labels for various states, if the object being damaged
// has such a state then it is used instead of the normal ones
// (PAIN, DEATH, OVERKILL).  Defaults to NULL.
//

class label_offset_c
{
public:
	label_offset_c();
	label_offset_c(label_offset_c &rhs);
	~label_offset_c(); 

private:
	void Copy(label_offset_c &src);

public:
	void Default();
	label_offset_c& operator=(label_offset_c &rhs);

	std::string label;
	int offset;
};

class damage_c
{
public:
	damage_c();
	damage_c(damage_c &rhs);
	~damage_c();
	
	enum default_e
	{
		DEFAULT_Attack,
		DEFAULT_Mobj,
		DEFAULT_MobjChoke,
		DEFAULT_Sector,
		DEFAULT_Numtypes
	};
	
private:
	void Copy(damage_c &src);

public:
	void Default(default_e def);
	damage_c& operator= (damage_c &rhs);

	// nominal damage amount (required)
	float nominal;

	// used for DAMAGE.MAX: when this is > 0, the damage is random
	// between nominal and linear_max, where each value has equal
	// probability.
	float linear_max;
  
	// used for DAMAGE.ERROR: when this is > 0, the damage is the
	// nominal value +/- this error amount, with a bell-shaped
	// distribution (values near the nominal are much more likely than
	// values at the outer extreme).
	float error;

	// delay (in terms of tics) between damage application, e.g. 34
	// would be once every second.  Only used for slime/crush damage.
	int delay;

	// death message, names an entry in LANGUAGES.LDF
	std::string obituary;

	// override labels for various states, if the object being damaged
	// has such a state then it is used instead of the normal ones
	// (PAIN, DEATH, OVERKILL).  Defaults to NULL.
	label_offset_c pain, death, overkill;

	// this flag says that the damage is unaffected by the player's
	// armour -- and vice versa.
	bool no_armour;

	// Apply damange unconditionally
	bool bypass_all;
	// Damage is always health+1 with no resistances applied
	bool instakill;
	// Apply to all players
	bool all_players;
	// Apply damage unless one of these benefits is in effect
	benefit_t *damage_unless;
	// Apply damage if one of these benefits is in effect
	benefit_t *damage_if;
	// Apply to (grounded) monsters instead (MBF21)
	bool grounded_monsters;
};

typedef enum
{
	GLOW_None    = 0,
	GLOW_Floor   = 1,
	GLOW_Ceiling = 2,
	GLOW_Wall = 3
}
glow_sector_type_e;

typedef enum
{
	SPYA_BottomUp = 0,
	SPYA_Middle   = 1,
	SPYA_TopDown  = 2,
}
sprite_y_alignment_e;

// a bitset is a set of named bits, from `A' to `Z'.
typedef int bitset_t;

#define BITSET_EMPTY  0
#define BITSET_FULL   0x7FFFFFFF
#define BITSET_MAKE(ch)  (1 << ((ch) - 'A'))

// dynamic light info

typedef enum
{
	// dynamic lighting disabled
	DLITE_None,

	// light texture is modulated with wall texture
	DLITE_Modulate,

	// light texture is simply added to wall
	DLITE_Add,

	// backwards compatibility cruft
	DLITE_Compat_LIN,
	DLITE_Compat_QUAD,
}
dlight_type_e;

class dlight_info_c
{
public:
	dlight_info_c();
	dlight_info_c(dlight_info_c &rhs);
	~dlight_info_c() {};

private:
	void Copy(dlight_info_c &src);

public:
	void Default(void);
	dlight_info_c& operator=(dlight_info_c &rhs);

	dlight_type_e type;
	std::string shape;  // IMAGES.DDF reference
	float radius;
	rgbcol_t colour;
	percent_t height;
	bool leaky;

	void *cache_data;
};

class weakness_info_c
{
public:
	weakness_info_c();
	weakness_info_c(weakness_info_c &rhs);
	~weakness_info_c() {};

private:
	void Copy(weakness_info_c &src);

public:
	void Default(void);
	weakness_info_c& operator=(weakness_info_c &rhs);

	percent_t height[2];
	angle_t angle[2];
	bitset_t classes;
	float multiply;
	percent_t painchance;
};


// mobjdef class
class mobjtype_c
{
public:
	// DDF Id
	std::string name;

	int number;

	// range of states used
	state_group_t state_grp;
  
	int spawn_state;
	int idle_state;
	int chase_state;
	int pain_state;
	int missile_state;
	int melee_state;
	int death_state;
	int overkill_state;
	int raise_state;
	int res_state;
	int meander_state;
	int morph_state;
	int bounce_state;
	int touch_state;
	int gib_state;
	int reload_state;

	int reactiontime;
	percent_t painchance;
	float spawnhealth;
	float speed;
	float float_speed;
	float radius;
	float height;
	float step_size;
	float mass;

	int flags;
	int extendedflags;
	int hyperflags;
	int mbf21flags;

	damage_c explode_damage;
	float explode_radius;  // normally zero (radius == damage)

	// linked list of losing benefits, or NULL
	benefit_t *lose_benefits;
  
	// linked list of pickup benefits, or NULL
	benefit_t *pickup_benefits;

	// linked list of kill benefits, or NULL
	benefit_t *kill_benefits;

	// linked list of pickup effects, or NULL
	pickup_effect_c *pickup_effects;

	// pickup message, a reference to languages.ldf
	std::string pickup_message;

	// linked list of initial benefits for players, or NULL if none
	benefit_t *initial_benefits;

	int castorder;
	std::string cast_title;
	int respawntime;
	percent_t translucency;
	percent_t minatkchance;
	const colourmap_c *palremap;

	int jump_delay;
	float jumpheight;
	float crouchheight;
	percent_t viewheight;
	percent_t shotheight;
	float maxfall;
	float fast;
	float scale;
	float aspect;
	float bounce_speed;
	float bounce_up;
	float sight_slope;
	angle_t sight_angle;
	float ride_friction;
	percent_t shadow_trans;

	struct sfx_s *seesound;
	struct sfx_s *attacksound;
	struct sfx_s *painsound;
	struct sfx_s *deathsound;
	struct sfx_s *overkill_sound;
	struct sfx_s *activesound;
	struct sfx_s *walksound;
	struct sfx_s *jump_sound;
	struct sfx_s *noway_sound;
	struct sfx_s *oof_sound;
	struct sfx_s *fallpain_sound;
	struct sfx_s *gasp_sound;
	struct sfx_s *secretsound;
	struct sfx_s *falling_sound;
	struct sfx_s *rip_sound;

	int fuse;
	int reload_shots;

	// armour control: armour_protect is how much damage the armour
	// saves when the bullet/fireball hits you (1% to 100%).  Zero
	// disables the association (between color and mobjtype_c).
	// The 'erosion' is how much of the saved damage eats up the
	// armour held: 100% is normal, at 0% you never lose it.
	percent_t armour_protect;
	percent_t armour_deplete;
	bitset_t armour_class;

	bitset_t side;
	int playernum;
	int yalign;     // -AJA- 2007/08/08: sprite Y alignment in bbox

	int model_skin; // -AJA- 2007/10/16: MD2 model support
	float model_scale;
	float model_aspect;
	float model_bias;
	int model_rotate;

	// breathing support: lung_capacity is how many tics we can last
	// underwater.  gasp_start is how long underwater before we gasp
	// when leaving it.  Damage and choking interval is in choke_damage.
	int lung_capacity;
	int gasp_start;
	damage_c choke_damage;

	// controls how much the player bobs when walking.
	percent_t bobbing;

	// what attack classes we are immune/resistant to (usually none).
	bitset_t immunity;
	bitset_t resistance;
	bitset_t ghost;  // pass through us

	float     resist_multiply;
	percent_t resist_painchance;

	const atkdef_c *closecombat;
	const atkdef_c *rangeattack;
	const atkdef_c *spareattack;

	dlight_info_c dlight[2];
	int glow_type;

	// -AJA- 2007/08/21: weakness support (head-shots etc)
	weakness_info_c weak;

	// item to drop (or NULL).  The mobjdef pointer is only valid after
	// DDF_MobjCleanUp() has been called.
	const mobjtype_c *dropitem;
	std::string dropitem_ref;

	// blood object (or NULL).  The mobjdef pointer is only valid after
	// DDF_MobjCleanUp() has been called.
	const mobjtype_c *blood;
	std::string blood_ref;
  
	// respawn effect object (or NULL).  The mobjdef pointer is only
	// valid after DDF_MobjCleanUp() has been called.
	const mobjtype_c *respawneffect;
	std::string respawneffect_ref;
  
	// spot type for the `SHOOT_TO_SPOT' attack (or NULL).  The mobjdef
	// pointer is only valid after DDF_MobjCleanUp() has been called.
	const mobjtype_c *spitspot;
	std::string spitspot_ref;

	float sight_distance; // lobo 2022: How far this thing can see
	float hear_distance; // lobo 2022: How far this thing can hear

	int morphtimeout; // lobo 2023: Go to MORPH states when times up

	// DEHEXTRA
	float gib_health;

	// MBF 21
	int infight_group;
	int proj_group;
	int splash_group;
	int fast_speed;
	int melee_range;

	// cleanup flag for UDMF-derived ad-hoc mobjtypes
	bool adhoc;

public:
	mobjtype_c();
	~mobjtype_c();

public:
	void Default();
	void CopyDetail(mobjtype_c &src);

	void DLightCompatibility(void);

private:
	// disable copy construct and assignment operator
	explicit mobjtype_c(mobjtype_c &rhs) { (void) rhs; }
	mobjtype_c& operator= (mobjtype_c &rhs) { (void) rhs; return *this; }
};


// Our mobjdef container
#define LOOKUP_CACHESIZE 211

class mobjtype_container_c : public epi::array_c
{
public:
	mobjtype_container_c();
	~mobjtype_container_c();

private:
	void CleanupObject(void *obj);

	mobjtype_c* lookup_cache[LOOKUP_CACHESIZE];

public:
	// List Management
	int GetSize() {	return array_entries; } 
	int Insert(mobjtype_c *m) { return InsertObject((void*)&m); }
	mobjtype_c* operator[](int idx) { return *(mobjtype_c**)FetchObject(idx); } 
	bool MoveToEnd(int idx);

	// Search Functions
	int FindFirst(const char *name, int startpos = -1);
	int FindLast(const char *name, int startpos = -1);
	const mobjtype_c *Lookup(const char *refname);
	const mobjtype_c *Lookup(int id);

	// FIXME!!! Move to a more appropriate location
	const mobjtype_c *LookupCastMember(int castpos);
	const mobjtype_c *LookupPlayer(int playernum);
	const mobjtype_c *LookupDoorKey(int theKey);
};


// -------EXTERNALISATIONS-------

extern mobjtype_container_c mobjtypes;

void DDF_MobjGetBenefit(const char *info, void *storage);

void DDF_ReadThings(const std::string& data);

#endif /*__DDF_MOBJ_H__*/

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
