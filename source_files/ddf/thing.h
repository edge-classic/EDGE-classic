//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Map Objects)
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

#include "colormap.h"
#include "math_bitset.h"
#include "states.h"
#include "types.h"

inline float DynamicLightCompatibilityRadius(float x)
{
    return 10.0f * HMM_SQRTF(x);
}

//
// Misc. mobj flags
//
enum MapObjectFlag
{
    // Call P_TouchSpecialThing when touched.
    kMapObjectFlagSpecial = (1 << 0),
    // Blocks.
    kMapObjectFlagSolid = (1 << 1),
    // Can be hit.
    kMapObjectFlagShootable = (1 << 2),
    // Don't use the sector links (invisible but touchable).
    kMapObjectFlagNoSector = (1 << 3),
    // Don't use the blocklinks (inert but displayable)
    kMapObjectFlagNoBlockmap = (1 << 4),
    // Not to be activated by sound, deaf monster.
    kMapObjectFlagAmbush = (1 << 5),
    // Will try to attack right back.
    kMapObjectFlagJustHit = (1 << 6),
    // Will take at least one step before attacking.
    kMapObjectFlagJustAttacked = (1 << 7),
    // On level spawning (initial position),
    // hang from ceiling instead of stand on floor.
    kMapObjectFlagSpawnCeiling = (1 << 8),
    // Don't apply gravity (every tic), that is, object will float,
    // keeping current height or changing it actively.
    kMapObjectFlagNoGravity = (1 << 9),
    // Movement flags. This allows jumps from high places.
    kMapObjectFlagDropOff = (1 << 10),
    // For players, will pick up items.
    kMapObjectFlagPickup = (1 << 11),
    // Object is not checked when moving, no clipping is used.
    kMapObjectFlagNoClip = (1 << 12),
    // Player: keep info about sliding along walls.
    kMapObjectFlagSlide = (1 << 13),
    // Allow moves to any height, no gravity.
    // For active floaters, e.g. cacodemons, pain elementals.
    kMapObjectFlagFloat = (1 << 14),
    // Instantly cross lines, whatever the height differences may be
    // (e.g. go from the bottom of a cliff to the top).
    // Note: nothing to do with teleporters.
    kMapObjectFlagTeleport = (1 << 15),
    // Don't hit same species, explode on block.
    // Player missiles as well as fireballs of various kinds.
    kMapObjectFlagMissile = (1 << 16),
    // Dropped by a demon, not level spawned.
    // E.g. ammo clips dropped by dying former humans.
    kMapObjectFlagDropped = (1 << 17),
    // Use fuzzy draw (shadow demons or spectres),
    // temporary player invisibility powerup.
    kMapObjectFlagFuzzy = (1 << 18),
    // Flag: don't bleed when shot (use puff),
    // barrels and shootable furniture shall not bleed.
    kMapObjectFlagNoBlood = (1 << 19),
    // Don't stop moving halfway off a step,
    // that is, have dead bodies slide down all the way.
    kMapObjectFlagCorpse = (1 << 20),
    // Floating to a height for a move, ???
    // don't auto float to target's height.
    kMapObjectFlagInFloat = (1 << 21),
    // On kill, count this enemy object
    // towards intermission kill total.
    // Happy gathering.
    kMapObjectFlagCountKill = (1 << 22),
    // On picking up, count this item object
    // towards intermission item total.
    kMapObjectFlagCountItem = (1 << 23),
    // Special handling: skull in flight.
    // Neither a cacodemon nor a missile.
    kMapObjectFlagSkullFly = (1 << 24),
    // Don't spawn this object
    // in death match mode (e.g. key cards).
    kMapObjectFlagNotDeathmatch = (1 << 25),
    // Monster grows (in)visible at certain times.
    kMapObjectFlagStealth = (1 << 26),
    // For projectiles: keep momentum of projectile source
    kMapObjectFlagPreserveMomentum = (1 << 27),
    // Object reacts to being touched (often violently :->)
    kMapObjectFlagTouchy = (1 << 28),
};

enum ExtendedFlag
{
    // -AJA- 2004/07/22: ignore certain types of damage
    // (previously was the EF_BOSSMAN flag).
    kExtendedFlagExplodeImmune = (1 << 0),
    // Used when varying visibility levels
    kExtendedFlagLessVisible = (1 << 1),
    // This thing does not respawn
    kExtendedFlagNoRespawn = (1 << 2),
    // On death, gravity either affects it if it wasn't affected before,
    // or now doesn't affect it if it was (best guess from looking at
    // the code - Dasho)
    kExtendedFlagNoGravityOnKill = (1 << 3),
    // This thing is not loyal to its own type, fights its own
    kExtendedFlagDisloyalToOwnType = (1 << 4),
    // This thing can be hurt by another thing with same attack
    kExtendedFlagOwnAttackHurts = (1 << 5),
    // Used for tracing (homing) projectiles, its the first time
    // this projectile has been checked for tracing if set.
    kExtendedFlagFirstTracerCheck = (1 << 6),
    // Simplified checks when picking up armour
    // i.e., does not take into account other armour classes
    kExtendedFlagSimpleArmour = (1 << 7),
    // double the chance of object using range attack
    kExtendedFlagTriggerHappy = (1 << 8),
    // not targeted by other monsters for damaging them
    kExtendedFlagNeverTarget = (1 << 9),
    // Normally most monsters will follow a target which caused them
    // damage for a length of time, even if another object inflicted
    // pain upon them; with this enabled, they will not hold the grudge
    // and switch targets to the other object that has caused them the
    // more recent pain.
    kExtendedFlagNoGrudge = (1 << 10),
    // NO LONGER USED (1 << 11),  // was: DUMMYMOBJ
    // Archvile cannot resurrect this monster
    kExtendedFlagCannotResurrect = (1 << 12),
    // Object bounces
    kExtendedFlagBounce = (1 << 13),
    // Thing walks along the edge near large dropoffs.
    kExtendedFlagEdgeWalker = (1 << 14),
    // Monster falls with gravity when walks over cliff.
    kExtendedFlagGravityFall = (1 << 15),
    // Thing can be climbed on-top-of or over.
    kExtendedFlagClimbable = (1 << 16),
    // Thing won't penetrate WATER extra floors.
    kExtendedFlagWaterWalker = (1 << 17),
    // Thing is a monster.
    kExtendedFlagMonster = (1 << 18),
    // Thing can cross blocking lines.
    kExtendedFlagCrossBlockingLines = (1 << 19),
    // Thing is never affected by friction
    kExtendedFlagNoFriction = (1 << 20),
    // Thing is optional, won't exist when -noextra is used.
    kExtendedFlagExtra = (1 << 21),
    // Just bounced, won't enter bounce states until BOUNCE_REARM.
    kExtendedFlagJustBounced = (1 << 22),
    // Thing can be "used" (like linedefs) with the spacebar.  Thing
    // will then enter its TOUCH_STATES (when they exist).
    kExtendedFlagUsable = (1 << 23),
    // Thing will block bullets and missiles.  -AJA- 2000/09/29
    kExtendedFlagBlockShots = (1 << 24),
    // Player is currently crouching.  -AJA- 2000/10/19
    kExtendedFlagCrouching = (1 << 25),
    // Missile can tunnel through enemies.  -AJA- 2000/10/23
    kExtendedFlagTunnel = (1 << 26),
    // NO LONGER USED (1 << 27),  // was: DLIGHT
    // Thing has been gibbed.
    kExtendedFlagGibbed = (1 << 28),
    // -AJA- 2004/07/22: play the monster sounds at full volume
    // (separated out from the BOSSMAN flag).
    kExtendedFlagAlwaysLoud = (1 << 29),
};

enum HyperFlag
{
    // -AJA- 2004/08/25: always pick up this item
    kHyperFlagForcePickup = (1 << 0),
    // -AJA- 2004/09/02: immune from friendly fire
    kHyperFlagFriendlyFireImmune = (1 << 1),
    // -AJA- 2005/05/15: friendly fire passes through you
    kHyperFlagFriendlyFirePassesThrough = (1 << 2),
    // -AJA- 2004/09/02: don't retaliate if hurt by friendly fire
    kHyperFlagUltraLoyal = (1 << 3),
    // -AJA- 2005/05/14: don't update the Z buffer (particles).
    kHyperFlagNoZBufferUpdate = (1 << 4),
    // -AJA- 2005/05/15: the sprite hovers up and down
    kHyperFlagHover = (1 << 5),
    // -AJA- 2006/08/17: object can be pushed (wind/current/point)
    kHyperFlagPushable = (1 << 6),
    // -AJA- 2006/08/17: used by MT_PUSH and MT_PULL objects
    kHyperFlagPointForce = (1 << 7),
    // -AJA- 2006/10/19: scenery items don't block missiles
    kHyperFlagMissilesPassThrough = (1 << 8),
    // -AJA- 2007/11/03: invulnerable flag
    kHyperFlagInvulnerable = (1 << 9),
    // -AJA- 2007/11/06: gain health when causing damage
    kHyperFlagVampire = (1 << 10),
    // -AJA- 2008/01/11: compatibility for quadratic dlights
    kHyperFlagQuadraticDynamicLight = (1 << 11),
    // -AJA- 2009/10/15: HUB system: remember old avatars
    kHyperFlagRememberOldAvatars = (1 << 12),
    // -AJA- 2009/10/22: never autoaim at this monster/thing
    kHyperFlagNoAutoaim = (1 << 13),
    // -AJA- 2010/06/13: used for RTS command of same name
    kHyperFlagWaitUntilDead = (1 << 14),
    // -AJA- 2010/12/23: force models to tilt by viewangle
    kHyperFlagForceModelTilt = (1 << 15),
    // -Lobo- 2021/10/24: immortal flag
    kHyperFlagImmortal = (1 << 16),
    // -Lobo- 2021/11/18: floorclip flag
    kHyperFlagFloorClip = (1 << 17),
    // -Lobo- 2022/05/30: this thing cannot trigger lines
    kHyperFlagNoTriggerLines = (1 << 18),
    // -Lobo- 2022/07/07: this thing can be shoved/pushed
    kHyperFlagShoveable = (1 << 19),
    // -Lobo- 2022/07/07: this thing doesn't cause splashes
    kHyperFlagNoSplash = (1 << 20),
    // -AJA- 2022/10/04: used by DEH_EDGE to workaround issues
    kHyperFlagDehackedCompatibility = (1 << 21),
    // -Lobo- 2023/10/19: this thing will not be affected by thrust forces
    kHyperFlagImmovable = (1 << 22),
    // Dasho 2023/12/05: this thing is a MUSINFO Music Changer thing
    // This flag is present because we cannot assume a thing is a
    // music changer just because it has an ID of 14100-14164
    kHyperFlagMusicChanger = (1 << 23),
};

// MBF21 flags not already covered by extended/hyper flags
enum MBF21Flag
{
    // Gravity affects this thing as if it were 1/8 of the normal value
    kMBF21FlagLowGravity        = (1 << 0),
    kMBF21FlagShortMissileRange = (1 << 1),
    kMBF21FlagForceRadiusDamage = (1 << 4),
    kMBF21FlagLongMeleeRange    = (1 << 8),
};

constexpr uint8_t kTotalEffectsSlots = 30;

// ------------------------------------------------------------------
// ------------------------BENEFIT TYPES-----------------------------
// ------------------------------------------------------------------

enum InventoryType
{
    kInventoryType01,
    kInventoryType02,
    kInventoryType03,
    kInventoryType04,
    kInventoryType05,
    kInventoryType06,
    kInventoryType07,
    kInventoryType08,
    kInventoryType09,
    kInventoryType10,
    kInventoryType11,
    kInventoryType12,
    kInventoryType13,
    kInventoryType14,
    kInventoryType15,
    kInventoryType16,
    kInventoryType17,
    kInventoryType18,
    kInventoryType19,
    kInventoryType20,
    kInventoryType21,
    kInventoryType22,
    kInventoryType23,
    kInventoryType24,
    kInventoryType25,
    kInventoryType26,
    kInventoryType27,
    kInventoryType28,
    kInventoryType29,
    kInventoryType30,
    kInventoryType31,
    kInventoryType32,
    kInventoryType33,
    kInventoryType34,
    kInventoryType35,
    kInventoryType36,
    kInventoryType37,
    kInventoryType38,
    kInventoryType39,
    kInventoryType40,
    kInventoryType41,
    kInventoryType42,
    kInventoryType43,
    kInventoryType44,
    kInventoryType45,
    kInventoryType46,
    kInventoryType47,
    kInventoryType48,
    kInventoryType49,
    kInventoryType50,
    kInventoryType51,
    kInventoryType52,
    kInventoryType53,
    kInventoryType54,
    kInventoryType55,
    kInventoryType56,
    kInventoryType57,
    kInventoryType58,
    kInventoryType59,
    kInventoryType60,
    kInventoryType61,
    kInventoryType62,
    kInventoryType63,
    kInventoryType64,
    kInventoryType65,
    kInventoryType66,
    kInventoryType67,
    kInventoryType68,
    kInventoryType69,
    kInventoryType70,
    kInventoryType71,
    kInventoryType72,
    kInventoryType73,
    kInventoryType74,
    kInventoryType75,
    kInventoryType76,
    kInventoryType77,
    kInventoryType78,
    kInventoryType79,
    kInventoryType80,
    kInventoryType81,
    kInventoryType82,
    kInventoryType83,
    kInventoryType84,
    kInventoryType85,
    kInventoryType86,
    kInventoryType87,
    kInventoryType88,
    kInventoryType89,
    kInventoryType90,
    kInventoryType91,
    kInventoryType92,
    kInventoryType93,
    kInventoryType94,
    kInventoryType95,
    kInventoryType96,
    kInventoryType97,
    kInventoryType98,
    kInventoryType99,
    kTotalInventoryTypes  // Total count (99)
};

enum CounterType
{
    kCounterTypeLives = 0,   // Arbitrarily named Lives counter
    kCounterTypeScore,       // Arbitrarily named Score counter
    kCounterTypeMoney,       // Arbitrarily named Money
    kCounterTypeExperience,  // Arbitrarily named EXP counter
    kCounterType05,
    kCounterType06,
    kCounterType07,
    kCounterType08,
    kCounterType09,
    kCounterType10,
    kCounterType11,
    kCounterType12,
    kCounterType13,
    kCounterType14,
    kCounterType15,
    kCounterType16,
    kCounterType17,
    kCounterType18,
    kCounterType19,
    kCounterType20,
    kCounterType21,
    kCounterType22,
    kCounterType23,
    kCounterType24,
    kCounterType25,
    kCounterType26,
    kCounterType27,
    kCounterType28,
    kCounterType29,
    kCounterType30,
    kCounterType31,
    kCounterType32,
    kCounterType33,
    kCounterType34,
    kCounterType35,
    kCounterType36,
    kCounterType37,
    kCounterType38,
    kCounterType39,
    kCounterType40,
    kCounterType41,
    kCounterType42,
    kCounterType43,
    kCounterType44,
    kCounterType45,
    kCounterType46,
    kCounterType47,
    kCounterType48,
    kCounterType49,
    kCounterType50,
    kCounterType51,
    kCounterType52,
    kCounterType53,
    kCounterType54,
    kCounterType55,
    kCounterType56,
    kCounterType57,
    kCounterType58,
    kCounterType59,
    kCounterType60,
    kCounterType61,
    kCounterType62,
    kCounterType63,
    kCounterType64,
    kCounterType65,
    kCounterType66,
    kCounterType67,
    kCounterType68,
    kCounterType69,
    kCounterType70,
    kCounterType71,
    kCounterType72,
    kCounterType73,
    kCounterType74,
    kCounterType75,
    kCounterType76,
    kCounterType77,
    kCounterType78,
    kCounterType79,
    kCounterType80,
    kCounterType81,
    kCounterType82,
    kCounterType83,
    kCounterType84,
    kCounterType85,
    kCounterType86,
    kCounterType87,
    kCounterType88,
    kCounterType89,
    kCounterType90,
    kCounterType91,
    kCounterType92,
    kCounterType93,
    kCounterType94,
    kCounterType95,
    kCounterType96,
    kCounterType97,
    kCounterType98,
    kCounterType99,
    kTotalCounterTypes  // Total count (99)
};

enum ArmourType
{
    // weak armour, saves 33% of damage
    kArmourTypeGreen = 0,
    // better armour, saves 50% of damage
    kArmourTypeBlue,
    // -AJA- 2007/08/22: another one, saves 66% of damage  (not in Doom)
    kArmourTypePurple,
    // good armour, saves 75% of damage  (not in Doom)
    kArmourTypeYellow,
    // the best armour, saves 90% of damage  (not in Doom)
    kArmourTypeRed,
    kTotalArmourTypes
};

// Power up artifacts.
//
// -MH- 1998/06/17  Jet Pack Added
// -ACB- 1998/07/15 NightVision Added

enum PowerType
{
    kPowerTypeInvulnerable = 0,
    kPowerTypeBerserk,
    kPowerTypePartInvis,
    kPowerTypeAcidSuit,
    kPowerTypeAllMap,
    kPowerTypeInfrared,
    // extra powerups (not in Doom)
    kPowerTypeJetpack,  // -MH- 1998/06/18  jetpack "fuel" counter
    kPowerTypeNightVision,
    kPowerTypeScuba,
    kPowerTypeTimeStop,
    kPowerTypeUnused10,
    kPowerTypeUnused11,
    kPowerTypeUnused12,
    kPowerTypeUnused13,
    kPowerTypeUnused14,
    kPowerTypeUnused15,
    // -AJA- Note: Savegame code relies on kTotalPowerTypes == 16.
    kTotalPowerTypes
};

enum PickupEffectType
{
    kPickupEffectTypeNone = 0,
    kPickupEffectTypePowerupEffect,
    kPickupEffectTypeScreenEffect,
    kPickupEffectTypeSwitchWeapon,
    kPickupEffectTypeKeepPowerup
};

class PickupEffect
{
   public:
    PickupEffect(PickupEffectType type, int sub, int slot, float time);
    PickupEffect(PickupEffectType type, WeaponDefinition *weap, int slot,
                 float time);
    ~PickupEffect() {}

    // next in linked list
    PickupEffect *next_;

    // type and optional sub-type
    PickupEffectType type_;

    union
    {
        int               type;
        WeaponDefinition *weap;
    } sub_;

    // which slot to use
    int slot_;

    // how long for the effect to last (in seconds).
    float time_;
};

// -ACB- 2003/05/15: Made enum external to structure, caused different issues
// with gcc and VC.NET compile
enum ConditionCheckType
{
    // dummy condition, used if parsing failed
    kConditionCheckTypeNone = 0,
    // object must have health
    kConditionCheckTypeHealth,
    // player must have armour (subtype is ARMOUR_* value)
    kConditionCheckTypeArmour,
    // player must have a key (subtype is KF_* value).
    kConditionCheckTypeKey,
    // player must have a weapon (subtype is slot number).
    kConditionCheckTypeWeapon,
    // player must have a powerup (subtype is kPowerType* value).
    kConditionCheckTypePowerup,
    // player must have ammo (subtype is AM_* value)
    kConditionCheckTypeAmmo,
    // player must have inventory (subtype is INVENTORY* value)
    kConditionCheckTypeInventory,
    // player must have inventory (subtype is COUNTER* value)
    kConditionCheckTypeCounter,
    // player must be jumping
    kConditionCheckTypeJumping,
    // player must be crouching
    kConditionCheckTypeCrouching,
    // object must be swimming (i.e. in water)
    kConditionCheckTypeSwimming,
    // player must be attacking (holding fire down)
    kConditionCheckTypeAttacking,
    // player must be rampaging (holding fire a long time)
    kConditionCheckTypeRampaging,
    // player must be using (holding space down)
    kConditionCheckTypeUsing,
    // player must be pressing an Action key
    kConditionCheckTypeAction1,
    kConditionCheckTypeAction2,
    // player must be walking
    kConditionCheckTypeWalking
};

struct ConditionCheck
{
    // next in linked list (order is unimportant)
    struct ConditionCheck *next = nullptr;

    // negate the condition
    bool negate = false;

    // condition is looking for an exact value (not "greater than" or "smaller
    // than")
    bool exact = false;

    ConditionCheckType cond_type = kConditionCheckTypeNone;

    // sub-type (specific type of ammo, weapon, key, powerup, inventory).  Not
    // used for health, jumping, crouching, etc.
    union
    {
        int               type;
        WeaponDefinition *weap;
    } sub;

    // required amount of health, armour, ammo, inventory or "counter",   Not
    // used for weapon, key, powerup, jumping, crouching, etc.
    float amount = 0;
};

// ------------------------------------------------------------------
// --------------------MOVING OBJECT INFORMATION---------------------
// ------------------------------------------------------------------

//
// -ACB- 2003/05/15: Moved this outside of DamageClass. GCC and VC.NET have
// different- and conflicting - issues with structs in structs
//
// override labels for various states, if the object being damaged
// has such a state then it is used instead of the normal ones
// (PAIN, DEATH, OVERKILL).  Defaults to nullptr.
//

enum SectorGlowType
{
    kSectorGlowTypeNone    = 0,
    kSectorGlowTypeFloor   = 1,
    kSectorGlowTypeCeiling = 2,
    kSectorGlowTypeWall    = 3
};

enum SpriteYAlignment
{
    SpriteYAlignmentBottomUp = 0,
    SpriteYAlignmentMiddle   = 1,
    SpriteYAlignmentTopDown  = 2,
};

enum DynamicLightType
{
    // dynamic lighting disabled
    kDynamicLightTypeNone,
    // light texture is modulated with wall texture
    kDynamicLightTypeModulate,
    // light texture is simply added to wall
    kDynamicLightTypeAdd,
    // backwards compatibility cruft
    kDynamicLightTypeCompatibilityLinear,
    kDynamicLightTypeCompatibilityQuadratic,
};

class DynamicLightDefinition
{
   public:
    DynamicLightDefinition();
    DynamicLightDefinition(DynamicLightDefinition &rhs);
    ~DynamicLightDefinition(){};

   private:
    void Copy(DynamicLightDefinition &src);

   public:
    void                    Default(void);
    DynamicLightDefinition &operator=(DynamicLightDefinition &rhs);

    DynamicLightType type_;
    std::string      shape_;  // IMAGES.DDF reference
    float            radius_;
    RGBAColor        colour_;
    float            height_;
    bool             leaky_;

    void *cache_data_;
};

class WeaknessDefinition
{
   public:
    WeaknessDefinition();
    WeaknessDefinition(WeaknessDefinition &rhs);
    ~WeaknessDefinition(){};

   private:
    void Copy(WeaknessDefinition &src);

   public:
    void                Default(void);
    WeaknessDefinition &operator=(WeaknessDefinition &rhs);

    float    height_[2];
    BAMAngle angle_[2];
    BitSet   classes_;
    float    multiply_;
    float    painchance_;
};

// mobjdef class
class MapObjectDefinition
{
   public:
    // DDF Id
    std::string name_;

    int number_;

    // range of states used
    std::vector<StateRange> state_grp_;

    int spawn_state_;
    int idle_state_;
    int chase_state_;
    int pain_state_;
    int missile_state_;
    int melee_state_;
    int death_state_;
    int overkill_state_;
    int raise_state_;
    int res_state_;
    int meander_state_;
    int morph_state_;
    int bounce_state_;
    int touch_state_;
    int gib_state_;
    int reload_state_;

    int   reaction_time_;
    float pain_chance_;
    float spawn_health_;
    float speed_;
    float float_speed_;
    float radius_;
    float height_;
    float step_size_;
    float mass_;

    int flags_;
    int extended_flags_;
    int hyper_flags_;
    int mbf21_flags_;

    DamageClass explode_damage_;
    float       explode_radius_;  // normally zero (radius == damage)

    // linked list of losing benefits, or nullptr
    Benefit *lose_benefits_;

    // linked list of pickup benefits, or nullptr
    Benefit *pickup_benefits_;

    // linked list of kill benefits, or nullptr
    Benefit *kill_benefits_;

    // linked list of pickup effects, or nullptr
    PickupEffect *pickup_effects_;

    // pickup message, a reference to languages.ldf
    std::string pickup_message_;

    // linked list of initial benefits for players, or nullptr if none
    Benefit *initial_benefits_;

    int             castorder_;
    std::string     cast_title_;
    int             respawntime_;
    float           translucency_;
    float           minatkchance_;
    const Colormap *palremap_;

    int      jump_delay_;
    float    jumpheight_;
    float    crouchheight_;
    float    viewheight_;
    float    shotheight_;
    float    maxfall_;
    float    fast_;
    float    scale_;
    float    aspect_;
    float    bounce_speed_;
    float    bounce_up_;
    float    sight_slope_;
    BAMAngle sight_angle_;
    float    ride_friction_;
    float    shadow_trans_;

    struct SoundEffect *seesound_;
    struct SoundEffect *attacksound_;
    struct SoundEffect *painsound_;
    struct SoundEffect *deathsound_;
    struct SoundEffect *overkill_sound_;
    struct SoundEffect *activesound_;
    struct SoundEffect *walksound_;
    struct SoundEffect *jump_sound_;
    struct SoundEffect *noway_sound_;
    struct SoundEffect *oof_sound_;
    struct SoundEffect *fallpain_sound_;
    struct SoundEffect *gasp_sound_;
    struct SoundEffect *secretsound_;
    struct SoundEffect *falling_sound_;
    struct SoundEffect *rip_sound_;

    int fuse_;
    int reload_shots_;

    // armour control: armour_protect is how much damage the armour
    // saves when the bullet/fireball hits you (1% to 100%).  Zero
    // disables the association (between color and MapObjectDefinition).
    // The 'erosion' is how much of the saved damage eats up the
    // armour held: 100% is normal, at 0% you never lose it.
    float  armour_protect_;
    float  armour_deplete_;
    BitSet armour_class_;

    BitSet side_;
    int    playernum_;
    int    yalign_;  // -AJA- 2007/08/08: sprite Y alignment in bbox

    int   model_skin_;  // -AJA- 2007/10/16: MD2 model support
    float model_scale_;
    float model_aspect_;
    float model_bias_;
    int   model_rotate_;

    // breathing support: lung_capacity is how many tics we can last
    // underwater.  gasp_start is how long underwater before we gasp
    // when leaving it.  Damage and choking interval is in choke_damage.
    int         lung_capacity_;
    int         gasp_start_;
    DamageClass choke_damage_;

    // controls how much the player bobs when walking.
    float bobbing_;

    // what attack classes we are immune/resistant to (usually none).
    BitSet immunity_;
    BitSet resistance_;
    BitSet ghost_;  // pass through us

    float resist_multiply_;
    float resist_painchance_;

    const AttackDefinition *closecombat_;
    const AttackDefinition *rangeattack_;
    const AttackDefinition *spareattack_;

    DynamicLightDefinition dlight_[2];
    int                    glow_type_;

    // -AJA- 2007/08/21: weakness support (head-shots etc)
    WeaknessDefinition weak_;

    // item to drop (or nullptr).  The mobjdef pointer is only valid after
    // DDF_MobjCleanUp() has been called.
    const MapObjectDefinition *dropitem_;
    std::string                dropitem_ref_;

    // blood object (or nullptr).  The mobjdef pointer is only valid after
    // DDF_MobjCleanUp() has been called.
    const MapObjectDefinition *blood_;
    std::string                blood_ref_;

    // respawn effect object (or nullptr).  The mobjdef pointer is only
    // valid after DDF_MobjCleanUp() has been called.
    const MapObjectDefinition *respawneffect_;
    std::string                respawneffect_ref_;

    // spot type for the `SHOOT_TO_SPOT' attack (or nullptr).  The mobjdef
    // pointer is only valid after DDF_MobjCleanUp() has been called.
    const MapObjectDefinition *spitspot_;
    std::string                spitspot_ref_;

    float sight_distance_;  // lobo 2022: How far this thing can see
    float hear_distance_;   // lobo 2022: How far this thing can hear

    int morphtimeout_;  // lobo 2023: Go to MORPH states when times up

    // DEHEXTRA
    float gib_health_;

    // MBF 21
    int infight_group_;
    int proj_group_;
    int splash_group_;
    int fast_speed_;
    int melee_range_;

   public:
    MapObjectDefinition();
    ~MapObjectDefinition();

   public:
    void Default();
    void CopyDetail(MapObjectDefinition &src);

    void DLightCompatibility(void);

   private:
    // disable copy construct and assignment operator
    explicit MapObjectDefinition(MapObjectDefinition &rhs) { (void)rhs; }
    MapObjectDefinition &operator=(MapObjectDefinition &rhs)
    {
        (void)rhs;
        return *this;
    }
};

class MapObjectDefinitionContainer : public std::vector<MapObjectDefinition *>
{
   public:
    MapObjectDefinitionContainer();
    ~MapObjectDefinitionContainer();

   private:
    MapObjectDefinition *lookup_cache_[kLookupCacheSize];

   public:
    // List Management
    bool MoveToEnd(int idx);

    // Search Functions
    int                        FindFirst(const char *name, int startpos = -1);
    int                        FindLast(const char *name, int startpos = -1);
    const MapObjectDefinition *Lookup(const char *refname);
    const MapObjectDefinition *Lookup(int id);

    // FIXME!!! Move to a more appropriate location
    const MapObjectDefinition *LookupCastMember(int castpos);
    const MapObjectDefinition *LookupPlayer(int playernum);
    const MapObjectDefinition *LookupDoorKey(int theKey);
};

// -------EXTERNALISATIONS-------

extern MapObjectDefinitionContainer mobjtypes;

void DDF_MobjGetBenefit(const char *info, void *storage);

void DDF_ReadThings(const std::string &data);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
