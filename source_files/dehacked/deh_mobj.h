//------------------------------------------------------------------------
//  MOBJ Definitions
//------------------------------------------------------------------------
//
//  DEH_EDGE  Copyright (C) 2004-2024 The EDGE Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License (in COPYING.txt) for more details.
//
//------------------------------------------------------------------------

#pragma once

namespace dehacked
{

constexpr int kFracUnit = 65536;

// This file diverges slightly from the style guide with enum member naming
// as these reflect the historical code pointer/state/flag/etc names - Dasho

enum DehackedMapObjectFlag
{
    // Call P_SpecialThing when touched.
    kMF_SPECIAL = 1,

    // Blocks.
    kMF_SOLID = 2,

    // Can be hit.
    kMF_SHOOTABLE = 4,

    // Don't use the sector links (invisible but touchable).
    kMF_NOSECTOR = 8,

    // Don't use the blocklinks (inert but displayable)
    kMF_NOBLOCKMAP = 16,

    // Not to be activated by sound, deaf monster.
    kMF_AMBUSH = 32,

    // Will try to attack right back.
    kMF_JUSTHIT = 64,

    // Will take at least one step before attacking.
    kMF_JUSTATTACKED = 128,

    // On level spawning (initial position),
    //  hang from ceiling instead of stand on floor.
    kMF_SPAWNCEILING = 256,

    // Don't apply gravity (every tic),
    //  that is, object will float, keeping current height
    //  or changing it actively.
    kMF_NOGRAVITY = 512,

    // Movement flags.
    // This allows jumps from high places.
    kMF_DROPOFF = 0x400,

    // For players, will pick up items.
    kMF_PICKUP = 0x800,

    // Player cheat. ???
    kMF_NOCLIP = 0x1000,

    // Player: keep info about sliding along walls.
    kMF_SLIDE = 0x2000,

    // Allow moves to any height, no gravity.
    // For active floaters, e.g. cacodemons, pain elementals.
    kMF_FLOAT = 0x4000,

    // Don't cross lines
    //   ??? or look at heights on teleport.
    kMF_TELEPORT = 0x8000,

    // Don't hit same species, explode on block.
    // Player missiles as well as fireballs of various kinds.
    kMF_MISSILE = 0x10000,

    // Dropped by a demon, not level spawned.
    // E.g. ammo clips dropped by dying former humans.
    kMF_DROPPED = 0x20000,

    // Use fuzzy draw (shadow demons or spectres),
    //  temporary player invisibility powerup.
    kMF_SHADOW = 0x40000,

    // Flag: don't bleed when shot (use puff),
    //  barrels and shootable furniture shall not bleed.
    kMF_NOBLOOD = 0x80000,

    // Don't stop moving halfway off a step,
    //  that is, have dead bodies slide down all the way.
    kMF_CORPSE = 0x100000,

    // Floating to a height for a move, ???
    //  don't auto float to target's height.
    kMF_INFLOAT = 0x200000,

    // On kill, count this enemy object
    //  towards intermission kill total.
    // Happy gathering.
    kMF_COUNTKILL = 0x400000,

    // On picking up, count this item object
    //  towards intermission item total.
    kMF_COUNTITEM = 0x800000,

    // Special handling: skull in flight.
    // Neither a cacodemon nor a missile.
    kMF_SKULLFLY = 0x1000000,

    // Don't spawn this object
    //  in death match mode (e.g. key cards).
    kMF_NOTDMATCH = 0x2000000,

    // Player sprites in multiplayer modes are modified
    //  using an internal color lookup table for re-indexing.
    // If 0x4 0x8 or 0xc,
    //  use a translation table for player colormaps
    kMF_TRANSLATION1 = 0x4000000,
    kMF_TRANSLATION2 = 0x8000000,

    kMF_TOUCHY = 0x10000000,

    kMF_BOUNCES = 0x20000000,

    kMF_FRIEND = 0x40000000,

    // Stealth Mode - Creatures that dissappear and reappear.
    // kMF_STEALTH = 0x10000000, // What to do with this ? - Dasho

    // Translucent sprite?
    kMF_TRANSLUCENT = 0x80000000

// Pre-MBF mappings
#define kMF_UNUSED1 kMF_TRANSLATION2
#define kMF_UNUSED2 kMF_TOUCHY
#define kMF_UNUSED3 kMF_BOUNCES
#define kMF_UNUSED4 kMF_FRIEND
};

//
// MBF21 mobj flags
//
enum DehackedMapObjectFlagMBF21
{
    // Lower gravity (1/8)
    kMBF21_LOGRAV = 1,

    // Short missile range (archvile)
    kMBF21_SHORTMRANGE = 2,

    // Other things ignore its attacks (archvile?)
    kMBF21_DMGIGNORED = 4,

    // Doesn't take splash damage (cyberdemon, mastermind)
    kMBF21_NORADIUSDMG = 8,

    // Thing causes splash damage even if the target shouldn't (WTF?)
    kMBF21_FORCERADIUSDMG = 16,

    // Higher missile attack prob (cyberdemon)
    kMBF21_HIGHERMPROB = 32,

    // Use half distance for missile attack prob (cyberdemon, mastermind,
    // revvie, lost soul)
    kMBF21_RANGEHALF = 64,

    // Has no targeting threshold (archvile)
    kMBF21_NOTHRESHOLD = 128,

    // Has long melee range (revvie)
    kMBF21_LONGMELEE = 256,

    // Full volume see/death sound and splash immunity
    kMBF21_BOSS = 512,

    // Triggers tag 666 when all are dead (mancubus)
    kMBF21_MAP07BOSS1 = 0x400,

    // Triggers tag 667 when all are dead (arachnotron)
    kMBF21_MAP07BOSS2 = 0x800,

    // E1M8 boss (baron)
    kMBF21_E1M8BOSS = 0x1000,

    // E2M8 boss (cyberdemon)
    kMBF21_E2M8BOSS = 0x2000,

    // E3M8 boss (mastermind)
    kMBF21_E3M8BOSS = 0x4000,

    // E4M6 boss (cyberdemon)
    kMBF21_E4M6BOSS = 0x8000,

    // E4M8 boss (mastermind)
    kMBF21_E4M8BOSS = 0x10000,

    // Ripper projectile (does not disappear on impact)
    kMBF21_RIP = 0x20000,

    // Full volume see/death sounds
    kMBF21_FULLVOLSOUNDS = 0x40000,
};

#define kMF_TRANSLATION (kMF_TRANSLATION1 | kMF_TRANSLATION2)
#define DEHACKED_ALL_BEX_FLAGS                                                                                         \
    (kMF_TRANSLUCENT | kMF_TOUCHY | kMF_BOUNCES | kMF_FRIEND) // Also housed kMF_STEALTH, but this is not a BEX flag

enum DehackedMapObjectType
{
    kMT_PLAYER,
    kMT_POSSESSED,
    kMT_SHOTGUY,
    kMT_VILE,
    kMT_FIRE,
    kMT_UNDEAD,
    kMT_TRACER,
    kMT_SMOKE,
    kMT_FATSO,
    kMT_FATSHOT,
    kMT_CHAINGUY,
    kMT_TROOP,
    kMT_SERGEANT,
    kMT_SHADOWS,
    kMT_HEAD,
    kMT_BRUISER,
    kMT_BRUISERSHOT,
    kMT_KNIGHT,
    kMT_SKULL,
    kMT_SPIDER,
    kMT_BABY,
    kMT_CYBORG,
    kMT_PAIN,
    kMT_WOLFSS,
    kMT_KEEN,
    kMT_BOSSBRAIN,
    kMT_BOSSSPIT,
    kMT_BOSSTARGET,
    kMT_SPAWNSHOT,
    kMT_SPAWNFIRE,
    kMT_BARREL,
    kMT_TROOPSHOT,
    kMT_HEADSHOT,
    kMT_ROCKET,
    kMT_PLASMA,
    kMT_BFG,
    kMT_ARACHPLAZ,
    kMT_PUFF,
    kMT_BLOOD,
    kMT_TFOG,
    kMT_IFOG,
    kMT_TELEPORTMAN,
    kMT_EXTRABFG,
    kMT_MISC0,
    kMT_MISC1,
    kMT_MISC2,
    kMT_MISC3,
    kMT_MISC4,
    kMT_MISC5,
    kMT_MISC6,
    kMT_MISC7,
    kMT_MISC8,
    kMT_MISC9,
    kMT_MISC10,
    kMT_MISC11,
    kMT_MISC12,
    kMT_INV,
    kMT_MISC13,
    kMT_INS,
    kMT_MISC14,
    kMT_MISC15,
    kMT_MISC16,
    kMT_MEGA,
    kMT_CLIP,
    kMT_MISC17,
    kMT_MISC18,
    kMT_MISC19,
    kMT_MISC20,
    kMT_MISC21,
    kMT_MISC22,
    kMT_MISC23,
    kMT_MISC24,
    kMT_MISC25,
    kMT_CHAINGUN,
    kMT_MISC26,
    kMT_MISC27,
    kMT_MISC28,
    kMT_SHOTGUN,
    kMT_SUPERSHOTGUN,
    kMT_MISC29,
    kMT_MISC30,
    kMT_MISC31,

    kMT_MISC32,
    kMT_MISC33,
    kMT_MISC34,
    kMT_MISC35,
    kMT_MISC36,
    kMT_MISC37,
    kMT_MISC38,
    kMT_MISC39,
    kMT_MISC40,
    kMT_MISC41,
    kMT_MISC42,
    kMT_MISC43,
    kMT_MISC44,
    kMT_MISC45,
    kMT_MISC46,
    kMT_MISC47,
    kMT_MISC48,
    kMT_MISC49,
    kMT_MISC50,
    kMT_MISC51,
    kMT_MISC52,
    kMT_MISC53,
    kMT_MISC54,
    kMT_MISC55,
    kMT_MISC56,
    kMT_MISC57,
    kMT_MISC58,
    kMT_MISC59,
    kMT_MISC60,
    kMT_MISC61,
    kMT_MISC62,
    kMT_MISC63,
    kMT_MISC64,
    kMT_MISC65,
    kMT_MISC66,
    kMT_MISC67,
    kMT_MISC68,
    kMT_MISC69,
    kMT_MISC70,
    kMT_MISC71,
    kMT_MISC72,
    kMT_MISC73,
    kMT_MISC74,
    kMT_MISC75,
    kMT_MISC76,
    kMT_MISC77,
    kMT_MISC78,
    kMT_MISC79,
    kMT_MISC80,
    kMT_MISC81,
    kMT_MISC82,
    kMT_MISC83,
    kMT_MISC84,
    kMT_MISC85,
    kMT_MISC86,

    kTotalDehackedMapObjectTypes,

    // BOOM and MBF things:
    kMT_PUSH = kTotalDehackedMapObjectTypes,
    kMT_PULL,
    kMT_DOGS,

    kMT_PLASMA1,
    kMT_PLASMA2,
    kMT_SCEPTRE,
    kMT_BIBLE,

    // other source port stuff:
    kMT_MUSICSOURCE,
    kMT_GIBDTH,

    // Note: there is a gap here of five mobjtypes.
    // There used to be 12 `kMT_STEALTHXXX` monsters, but DEHEXTRA
    // spoiled them :-(

    kTotalDehackedMapObjectTypesPortCompatibility,

    // DEHEXTRA : 150..249
    kMT_EXTRA00 = 150,
    kMT_EXTRA99 = 249,

    kTotalDehackedMapObjectTypesDEHEXTRA
};

struct DehackedMapObjectDefinition
{
    const char *name;

    int doomednum;
    int spawnstate;
    int spawnhealth;
    int seestate;
    int seesound;
    int reactiontime;
    int attacksound;
    int painstate;
    int painchance;
    int painsound;
    int meleestate;
    int missilestate;
    int deathstate;
    int xdeathstate;
    int deathsound;
    int speed;
    int radius;
    int height;
    int mass;
    int damage;
    int activesound;
    int flags;
    int mbf21_flags;
    int infight_group;
    int proj_group;
    int splash_group;
    int rip_sound;
    int fast_speed;
    int melee_range;
    int raisestate;
};

} // namespace dehacked