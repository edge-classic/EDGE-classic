//------------------------------------------------------------------------
//  MOBJ Definitions
//------------------------------------------------------------------------
//
//  DEH_EDGE  Copyright (C) 2004-2005  The EDGE Team
//
//  This program is under the GNU General Public License.
//  It comes WITHOUT ANY WARRANTY of any kind.
//  See COPYING.txt for the full details.
//
//------------------------------------------------------------------------

#ifndef __DEH_MOBJ_HDR__
#define __DEH_MOBJ_HDR__

namespace Deh_Edge
{

#define FRACUNIT  65536

//
// Misc. mobj flags
//
typedef enum
{
	// Call P_SpecialThing when touched.
	MF_SPECIAL = 1,

	// Blocks.
	MF_SOLID = 2,

	// Can be hit.
	MF_SHOOTABLE = 4,

	// Don't use the sector links (invisible but touchable).
	MF_NOSECTOR = 8,

	// Don't use the blocklinks (inert but displayable)
	MF_NOBLOCKMAP = 16,

	// Not to be activated by sound, deaf monster.
	MF_AMBUSH = 32,

	// Will try to attack right back.
	MF_JUSTHIT = 64,

	// Will take at least one step before attacking.
	MF_JUSTATTACKED = 128,

	// On level spawning (initial position),
	//  hang from ceiling instead of stand on floor.
	MF_SPAWNCEILING = 256,

	// Don't apply gravity (every tic),
	//  that is, object will float, keeping current height
	//  or changing it actively.
	MF_NOGRAVITY = 512,

	// Movement flags.
	// This allows jumps from high places.
	MF_DROPOFF = 0x400,

	// For players, will pick up items.
	MF_PICKUP = 0x800,

	// Player cheat. ???
	MF_NOCLIP = 0x1000,

	// Player: keep info about sliding along walls.
	MF_SLIDE = 0x2000,

	// Allow moves to any height, no gravity.
	// For active floaters, e.g. cacodemons, pain elementals.
	MF_FLOAT = 0x4000,

	// Don't cross lines
	//   ??? or look at heights on teleport.
	MF_TELEPORT = 0x8000,

	// Don't hit same species, explode on block.
	// Player missiles as well as fireballs of various kinds.
	MF_MISSILE = 0x10000,

	// Dropped by a demon, not level spawned.
	// E.g. ammo clips dropped by dying former humans.
	MF_DROPPED = 0x20000,

	// Use fuzzy draw (shadow demons or spectres),
	//  temporary player invisibility powerup.
	MF_SHADOW = 0x40000,

	// Flag: don't bleed when shot (use puff),
	//  barrels and shootable furniture shall not bleed.
	MF_NOBLOOD = 0x80000,

	// Don't stop moving halfway off a step,
	//  that is, have dead bodies slide down all the way.
	MF_CORPSE = 0x100000,

	// Floating to a height for a move, ???
	//  don't auto float to target's height.
	MF_INFLOAT = 0x200000,

	// On kill, count this enemy object
	//  towards intermission kill total.
	// Happy gathering.
	MF_COUNTKILL = 0x400000,

	// On picking up, count this item object
	//  towards intermission item total.
	MF_COUNTITEM = 0x800000,

	// Special handling: skull in flight.
	// Neither a cacodemon nor a missile.
	MF_SKULLFLY = 0x1000000,

	// Don't spawn this object
	//  in death match mode (e.g. key cards).
	MF_NOTDMATCH = 0x2000000,

	// Player sprites in multiplayer modes are modified
	//  using an internal color lookup table for re-indexing.
	// If 0x4 0x8 or 0xc,
	//  use a translation table for player colormaps
	MF_TRANSLATION1 = 0x4000000,
	MF_TRANSLATION2 = 0x8000000,

	// ---- BOOM and MBF flags ----

	// Stealth Mode - Creatures that dissappear and reappear.
	MF_STEALTH = 0x10000000,

	// Translucent sprite?
	MF_TRANSLUCENT = 0x40000000,

	MF_TOUCHY  = 0x20000000     // Should be: (0x1 << 32)

// FIXME place these in `flags2` or similar
#define MF_BOUNCES  MF_JUSTHIT  // Should be: (0x2 << 32)
#define MF_FRIEND   MF_INFLOAT  // Should be: (0x4 << 32)
}
mobjflag_t;

//
// MBF21 mobj flags
//
typedef enum
{
	// Lower gravity (1/8)
	MBF21_LOGRAV = 1,

	// Short missile range (archvile)
	MBF21_SHORTMRANGE = 2,

	// Other things ignore its attacks (archvile?)
	MBF21_DMGIGNORED = 4,

	// Doesn't take splash damage (cyberdemon, mastermind)
	MBF21_NORADIUSDMG = 8,

	// Thing causes splash damage even if the target shouldn't (WTF?)
	MBF21_FORCERADIUSDMG = 16,

	// Higher missile attack prob (cyberdemon)
	MBF21_HIGHERMPROB = 32,

	// Use half distance for missile attack prob (cyberdemon, mastermind, revvie, lost soul)
	MBF21_RANGEHALF = 64,

	// Has no targeting threshold (archvile)
	MBF21_NOTHRESHOLD = 128,

	// Has long melee range (revvie)
	MBF21_LONGMELEE = 256,

	// Full volume see/death sound and splash immunity
	MBF21_BOSS = 512,

	// Triggers tag 666 when all are dead (mancubus)
	MBF21_MAP07BOSS1 = 0x400,

	// Triggers tag 667 when all are dead (arachnotron)
	MBF21_MAP07BOSS2 = 0x800,

	// E1M8 boss (baron)
	MBF21_E1M8BOSS = 0x1000,

	// E2M8 boss (cyberdemon)
	MBF21_E2M8BOSS = 0x2000,

	// E3M8 boss (mastermind)
	MBF21_E3M8BOSS = 0x4000,

	// E4M6 boss (cyberdemon)
	MBF21_E4M6BOSS = 0x8000,

	// E4M8 boss (mastermind)
	MBF21_E4M8BOSS = 0x10000,

	// Ripper projectile (does not disappear on impact)
	MBF21_RIP = 0x20000,

	// Full volume see/death sounds
	MBF21_FULLVOLSOUNDS = 0x40000,
}
mobjmbf21flag_t;

#define MF_TRANSLATION  (MF_TRANSLATION1 | MF_TRANSLATION2)
#define ALL_BEX_FLAGS  \
	(MF_STEALTH | MF_TRANSLUCENT | MF_TOUCHY | MF_BOUNCES | MF_FRIEND)


typedef enum
{
	MT_PLAYER,
	MT_POSSESSED, MT_SHOTGUY, MT_VILE, MT_FIRE, MT_UNDEAD, MT_TRACER,
	MT_SMOKE, MT_FATSO, MT_FATSHOT, MT_CHAINGUY, MT_TROOP, MT_SERGEANT,
	MT_SHADOWS, MT_HEAD, MT_BRUISER, MT_BRUISERSHOT, MT_KNIGHT, MT_SKULL,
	MT_SPIDER, MT_BABY, MT_CYBORG, MT_PAIN, MT_WOLFSS, MT_KEEN,
	MT_BOSSBRAIN, MT_BOSSSPIT, MT_BOSSTARGET, MT_SPAWNSHOT, MT_SPAWNFIRE,
	MT_BARREL, MT_TROOPSHOT, MT_HEADSHOT, MT_ROCKET, MT_PLASMA, MT_BFG,
	MT_ARACHPLAZ, MT_PUFF, MT_BLOOD, MT_TFOG, MT_IFOG, MT_TELEPORTMAN,
	MT_EXTRABFG, MT_MISC0, MT_MISC1, MT_MISC2, MT_MISC3, MT_MISC4, MT_MISC5,
	MT_MISC6, MT_MISC7, MT_MISC8, MT_MISC9, MT_MISC10, MT_MISC11, MT_MISC12,
	MT_INV, MT_MISC13, MT_INS, MT_MISC14, MT_MISC15, MT_MISC16, MT_MEGA,
	MT_CLIP, MT_MISC17, MT_MISC18, MT_MISC19, MT_MISC20, MT_MISC21, MT_MISC22,
	MT_MISC23, MT_MISC24, MT_MISC25, MT_CHAINGUN, MT_MISC26, MT_MISC27,
	MT_MISC28, MT_SHOTGUN, MT_SUPERSHOTGUN, MT_MISC29, MT_MISC30, MT_MISC31,

	MT_MISC32, MT_MISC33, MT_MISC34, MT_MISC35, MT_MISC36, MT_MISC37,
	MT_MISC38, MT_MISC39, MT_MISC40, MT_MISC41, MT_MISC42, MT_MISC43,
	MT_MISC44, MT_MISC45, MT_MISC46, MT_MISC47, MT_MISC48, MT_MISC49,
	MT_MISC50, MT_MISC51, MT_MISC52, MT_MISC53, MT_MISC54, MT_MISC55,
	MT_MISC56, MT_MISC57, MT_MISC58, MT_MISC59, MT_MISC60, MT_MISC61,
	MT_MISC62, MT_MISC63, MT_MISC64, MT_MISC65, MT_MISC66, MT_MISC67,
	MT_MISC68, MT_MISC69, MT_MISC70, MT_MISC71, MT_MISC72, MT_MISC73,
	MT_MISC74, MT_MISC75, MT_MISC76, MT_MISC77, MT_MISC78, MT_MISC79,
	MT_MISC80, MT_MISC81, MT_MISC82, MT_MISC83, MT_MISC84, MT_MISC85,
	MT_MISC86,

	NUMMOBJTYPES,

	// BOOM and MBF things:
	MT_PUSH = NUMMOBJTYPES,
	MT_PULL,
	MT_DOGS,

	MT_PLASMA1,
	MT_PLASMA2,
	MT_SCEPTRE,
	MT_BIBLE,

	// other source port stuff:
	MT_MUSICSOURCE,
	MT_GIBDTH,

	// Note: there is a gap here of five mobjtypes.
	// There used to be 12 `MT_STEALTHXXX` monsters, but DEHEXTRA
	// spoiled them :-(

	NUMMOBJTYPES_COMPAT,

	// DEHEXTRA : 150..249
	MT_EXTRA00 = 150,
	MT_EXTRA99 = 249,

	NUMMOBJTYPES_DEHEXTRA
}
mobjtype_t;


typedef struct
{
    const char *name;

    int	doomednum;
    int	spawnstate;
    int	spawnhealth;
    int	seestate;
    int	seesound;
    int	reactiontime;
    int	attacksound;
    int	painstate;
    int	painchance;
    int	painsound;
    int	meleestate;
    int	missilestate;
    int	deathstate;
    int	xdeathstate;
    int	deathsound;
    int	speed;
    int	radius;
    int	height;
    int	mass;
    int	damage;
    int	activesound;
    int	flags;
	int mbf21_flags;
	int infight_group;
	int proj_group;
	int splash_group;
	int rip_sound;
	int fast_speed;
	int melee_range;
    int	raisestate;
}
mobjinfo_t;


}  // Deh_Edge

#endif /* __DEH_MOBJ_HDR__ */
