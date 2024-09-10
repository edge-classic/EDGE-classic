//----------------------------------------------------------------------------
//  DDF Commaon/Shared Types
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
//
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------

#pragma once

#include <string>
#include <vector>

#include "epi_bam.h"
#include "epi_bitset.h"
#include "epi_color.h"

class MapObjectDefinition;
class WeaponDefinition;

constexpr uint8_t kLookupCacheSize = 211; // Why this number? - Dasho

class MobjStringReference
{
  public:
    MobjStringReference() : name_(), def_(nullptr)
    {
    }
    MobjStringReference(const char *s) : name_(s), def_(nullptr)
    {
    }
    MobjStringReference(const MobjStringReference &rhs) : name_(rhs.name_), def_(nullptr)
    {
    }
    ~MobjStringReference() {};

  private:
    std::string name_;

    const MapObjectDefinition *def_;

  public:
    const char *GetName() const
    {
        return name_.c_str();
    }

    const MapObjectDefinition *GetRef();
    // Note: this returns nullptr if not found, in which case you should
    // produce an error, since future calls will do the search again.

    MobjStringReference &operator=(MobjStringReference &rhs)
    {
        if (&rhs != this)
        {
            name_ = rhs.name_;
            def_  = nullptr;
        }

        return *this;
    }
};

struct StateRange
{
    int first, last;
};

enum BenefitType
{
    kBenefitTypeNone = 0,
    kBenefitTypeAmmo,
    kBenefitTypeAmmoLimit,
    kBenefitTypeWeapon,
    kBenefitTypeKey,
    kBenefitTypeHealth,
    kBenefitTypeArmour,
    kBenefitTypePowerup,
    kBenefitTypeInventory,
    kBenefitTypeInventoryLimit,
    kBenefitTypeCounter,
    kBenefitTypeCounterLimit
};

struct Benefit
{
    // next in linked list
    struct Benefit *next;

    // type of benefit (ammo, ammo-limit, weapon, key, health, armour,
    // powerup, inventory, or inventory-limit).
    BenefitType type;

    // sub-type (specific type of ammo, weapon, key, powerup, or inventory). For
    // armour this is the class, for health it is unused.
    union {
        int               type;
        WeaponDefinition *weap;
    } sub;

    // amount of benefit (e.g. quantity of ammo or health).  For weapons
    // and keys, this is a boolean value: 1 to give, 0 to ignore.  For
    // powerups, it is number of seconds the powerup lasts.
    float amount;

    // for health, armour and powerups, don't make the new value go
    // higher than this (if it is already higher, prefer not to pickup
    // the object).
    float limit;
};

class LabelOffset
{
  public:
    LabelOffset();
    LabelOffset(LabelOffset &rhs);
    ~LabelOffset();

  private:
    void Copy(LabelOffset &src);

  public:
    void         Default();
    LabelOffset &operator=(LabelOffset &rhs);

    std::string label_;
    int         offset_;
};

class DamageClass
{
  public:
    DamageClass();
    DamageClass(DamageClass &rhs);
    ~DamageClass();

    enum DamageClassDefault
    {
        kDamageClassDefaultAttack,
        kDamageClassDefaultMobj,
        kDamageClassDefaultMobjChoke,
        kDamageClassDefaultSector,
        kDamageClassDefaultNumtypes
    };

  private:
    void Copy(DamageClass &src);

  public:
    void         Default(DamageClassDefault def);
    DamageClass &operator=(DamageClass &rhs);

    // nominal damage amount (required)
    float nominal_;

    // used for DAMAGE.MAX: when this is > 0, the damage is random
    // between nominal and linear_max, where each value has equal
    // probability.
    float linear_max_;

    // used for DAMAGE.ERROR: when this is > 0, the damage is the
    // nominal value +/- this error amount, with a bell-shaped
    // distribution (values near the nominal are much more likely than
    // values at the outer extreme).
    float error_;

    // delay (in terms of tics) between damage application, e.g. 34
    // would be once every second.  Only used for slime/crush damage.
    int delay_;

    // death message, names an entry in LANGUAGES.LDF
    std::string obituary_;

    // override labels for various states, if the object being damaged
    // has such a state then it is used instead of the normal ones
    // (PAIN, DEATH, OVERKILL).  Defaults to nullptr.
    LabelOffset pain_, death_, overkill_;

    // this flag says that the damage is unaffected by the player's
    // armour -- and vice versa.
    bool no_armour_;

    // Color of the flash when player is hit by this damage type
    RGBAColor damage_flash_colour_;

    // Apply damange unconditionally
    bool bypass_all_;
    // Damage is always health+1 with no resistances applied
    bool instakill_;
    // Apply to all players
    bool all_players_;
    // Apply damage unless one of these benefits is in effect
    Benefit *damage_unless_;
    // Apply damage if one of these benefits is in effect
    Benefit *damage_if_;
    // Apply to (grounded) monsters instead (MBF21)
    bool grounded_monsters_;
};

enum AttackStyle
{
    kAttackStyleNone = 0,
    kAttackStyleProjectile,
    kAttackStyleSpawner,
    kAttackStyleDoubleSpawner, // Lobo 2021: doom64 pain elemental
    kAttackStyleTripleSpawner,
    kAttackStyleSpreader,
    kAttackStyleRandomSpread,
    kAttackStyleShot,
    kAttackStyleTracker,
    kAttackStyleCloseCombat,
    kAttackStyleShootToSpot,
    kAttackStyleSkullFly,
    kAttackStyleSmartProjectile,
    kAttackStyleSpray,
    kAttackStyleDualAttack, // Dasho 2023: Execute two independent atkdefs with
                            // one command
    kAttackStylePsychic,    // Dasho 2023: Beta Lost Soul attack
    kTotalAttackStyles
};

enum AttackFlags
{
    kAttackFlagNone             = 0,
    kAttackFlagSmokingTracer    = (1 << 0),
    kAttackFlagKillFailedSpawn  = (1 << 1),
    kAttackFlagPrestepSpawn     = (1 << 2),
    kAttackFlagSpawnTelefrags   = (1 << 3),
    kAttackFlagNeedSight        = (1 << 4),
    kAttackFlagFaceTarget       = (1 << 5),
    kAttackFlagPlayer           = (1 << 6),
    kAttackFlagForceAim         = (1 << 7),
    kAttackFlagAngledSpawn      = (1 << 8),
    kAttackFlagNoTriggerLines   = (1 << 9),
    kAttackFlagSilentToMonsters = (1 << 10),
    kAttackFlagNoTarget         = (1 << 11),
    kAttackFlagVampire          = (1 << 12),
    // MBF21 stuff
    kAttackFlagInheritTracerFromTarget = (1 << 13)
};

class AttackDefinition
{
  public:
    AttackDefinition();
    ~AttackDefinition();

  public:
    void Default();
    void CopyDetail(AttackDefinition &src);

    // Member vars
    std::string name_;

    AttackStyle         attackstyle_;
    AttackFlags         flags_;
    struct SoundEffect *initsound_;
    struct SoundEffect *sound_;
    float               accuracy_slope_;
    BAMAngle            accuracy_angle_;
    float               xoffset_;
    float               yoffset_;
    BAMAngle            angle_offset_; // -AJA- 1999/09/10.
    float               slope_offset_; //
    BAMAngle            trace_angle_;  // -AJA- 2005/02/08.
    float               assault_speed_;
    float               height_;
    float               range_;
    int                 count_;
    int                 tooclose_;
    float               berserk_mul_; // -AJA- 2005/08/06.
    DamageClass         damage_;

    // class of the attack.
    BitSet attack_class_;

    // object init state.  The integer value only becomes valid after
    // DDFAttackCleanUp() has been called.
    int         objinitstate_;
    std::string objinitstate_ref_;

    float notracechance_;
    float keepfirechance_;

    // the MOBJ that is integrated with this attack, or nullptr
    const MapObjectDefinition *atk_mobj_;
    std::string                atk_mobj_ref_;

    // spawned object (for spawners).  The mobjdef pointer only becomes
    // valid after DDFAttackCleanUp().  Can be nullptr.
    const MapObjectDefinition *spawnedobj_;
    std::string                spawnedobj_ref_;
    int                        spawn_limit_;

    // puff object.  The mobjdef pointer only becomes valid after
    // DDFAttackCleanUp() has been called.  Can be nullptr.
    const MapObjectDefinition *puff_;
    std::string                puff_ref_;

    // blood object. The mobjdef pointer only becomes valid after
    // DDFAttackCleanUp() has been called.  Can be nullptr.
    // If defined, this will override any blood that a thing would normally
    // spawn after being hit by this attack
    const MapObjectDefinition *blood_;
    std::string                blood_ref_;

    // For DUALATTACK type only
    AttackDefinition *dualattack1_;
    AttackDefinition *dualattack2_;

  private:
    // disable copy construct and assignment operator
    explicit AttackDefinition(AttackDefinition &rhs)
    {
        (void)rhs;
    }
    AttackDefinition &operator=(AttackDefinition &rhs)
    {
        (void)rhs;
        return *this;
    }
};

enum AmmunitionType
{
    kAmmunitionTypeDontCare = -2, // Only used for SelectNewWeapon()
    kAmmunitionTypeNoAmmo   = -1, // Unlimited for chainsaw / fist.
    kAmmunitionTypeBullet   = 0,  // Pistol / chaingun ammo.
    kAmmunitionTypeShell,         // Shotgun / double barreled shotgun.
    kAmmunitionTypeRocket,        // Missile launcher.
    kAmmunitionTypeCell,          // Plasma rifle, BFG.
    // New ammo types
    kAmmunitionTypePellet,
    kAmmunitionTypeNail,
    kAmmunitionTypeGrenade,
    kAmmunitionTypeGas,
    kAmmunitionType9,
    kAmmunitionType10,
    kAmmunitionType11,
    kAmmunitionType12,
    kAmmunitionType13,
    kAmmunitionType14,
    kAmmunitionType15,
    kAmmunitionType16,
    kAmmunitionType17,
    kAmmunitionType18,
    kAmmunitionType19,
    kAmmunitionType20,
    kAmmunitionType21,
    kAmmunitionType22,
    kAmmunitionType23,
    kAmmunitionType24,
    kAmmunitionType25,
    kAmmunitionType26,
    kAmmunitionType27,
    kAmmunitionType28,
    kAmmunitionType29,
    kAmmunitionType30,
    kAmmunitionType31,
    kAmmunitionType32,
    kAmmunitionType33,
    kAmmunitionType34,
    kAmmunitionType35,
    kAmmunitionType36,
    kAmmunitionType37,
    kAmmunitionType38,
    kAmmunitionType39,
    kAmmunitionType40,
    kAmmunitionType41,
    kAmmunitionType42,
    kAmmunitionType43,
    kAmmunitionType44,
    kAmmunitionType45,
    kAmmunitionType46,
    kAmmunitionType47,
    kAmmunitionType48,
    kAmmunitionType49,
    kAmmunitionType50,
    kAmmunitionType51,
    kAmmunitionType52,
    kAmmunitionType53,
    kAmmunitionType54,
    kAmmunitionType55,
    kAmmunitionType56,
    kAmmunitionType57,
    kAmmunitionType58,
    kAmmunitionType59,
    kAmmunitionType60,
    kAmmunitionType61,
    kAmmunitionType62,
    kAmmunitionType63,
    kAmmunitionType64,
    kAmmunitionType65,
    kAmmunitionType66,
    kAmmunitionType67,
    kAmmunitionType68,
    kAmmunitionType69,
    kAmmunitionType70,
    kAmmunitionType71,
    kAmmunitionType72,
    kAmmunitionType73,
    kAmmunitionType74,
    kAmmunitionType75,
    kAmmunitionType76,
    kAmmunitionType77,
    kAmmunitionType78,
    kAmmunitionType79,
    kAmmunitionType80,
    kAmmunitionType81,
    kAmmunitionType82,
    kAmmunitionType83,
    kAmmunitionType84,
    kAmmunitionType85,
    kAmmunitionType86,
    kAmmunitionType87,
    kAmmunitionType88,
    kAmmunitionType89,
    kAmmunitionType90,
    kAmmunitionType91,
    kAmmunitionType92,
    kAmmunitionType93,
    kAmmunitionType94,
    kAmmunitionType95,
    kAmmunitionType96,
    kAmmunitionType97,
    kAmmunitionType98,
    kAmmunitionType99,
    kTotalAmmunitionTypes // Total count (99)
};

// -AJA- 2000/01/12: Weapon special flags
enum WeaponFlag
{
    WeaponFlagNone             = 0,
    WeaponFlagSilentToMonsters = (1 << 0), // monsters cannot hear this weapon
    WeaponFlagAnimated         = (1 << 1), // raise/lower states are animated
    WeaponFlagSwitchAway       = (1 << 4), // select new weapon when we run out of ammo
    // reload flags:
    WeaponFlagReloadWhileTrigger = (1 << 8),  // allow reload while holding trigger
    WeaponFlagFreshReload        = (1 << 9),  // automatically reload when new ammo is avail
    WeaponFlagManualReload       = (1 << 10), // enables the manual reload key
    WeaponFlagPartialReload      = (1 << 11), // manual reload: allow partial refill
    // MBF21 flags:
    WeaponFlagNoAutoFire = (1 << 12), // Do not fire if switched to while trigger is held
};

constexpr WeaponFlag kDefaultWeaponFlags = (WeaponFlag)(WeaponFlagReloadWhileTrigger | WeaponFlagManualReload |
                                                        WeaponFlagSwitchAway | WeaponFlagPartialReload);

class WeaponDefinition
{
  public:
    WeaponDefinition();
    ~WeaponDefinition();

  public:
    void Default(void);
    void CopyDetail(WeaponDefinition &src);

    // Weapon's name, etc...
    std::string name_;

    AttackDefinition *attack_[4];   // Attack type used.

    AmmunitionType ammo_[4];        // Type of ammo this weapon uses.
    int            ammopershot_[4]; // Ammo used per shot.
    int            clip_size_[4];   // Amount of shots in a clip (if <= 1, non-clip weapon)
    bool           autofire_[4];    // If true, this is an automatic else it's semiauto

    float kick_;                    // Amount of kick this weapon gives

    // range of states used
    std::vector<StateRange> state_grp_;

    int up_state_;                   // State to use when raising the weapon
    int down_state_;                 // State to use when lowering the weapon (if changing
                                     // weapon)
    int ready_state_;                // State that the weapon is ready to fire in
    int empty_state_;                // State when weapon is empty.  Usually zero
    int idle_state_;                 // State to use when polishing weapon

    int attack_state_[4];            // State showing the weapon 'firing'
    int reload_state_[4];            // State showing the weapon being reloaded
    int discard_state_[4];           // State showing the weapon discarding a clip
    int warmup_state_[4];            // State showing the weapon warming up
    int flash_state_[4];             // State showing the muzzle flash

    int crosshair_;                  // Crosshair states
    int zoom_state_;                 // State showing viewfinder when zoomed.  Can be zero

    bool no_cheat_;                  // Not given for cheats (Note: set by #CLEARALL)

    bool autogive_;                  // The player gets this weapon on spawn.  (Fist + Pistol)
    bool feedback_;                  // This weapon gives feedback on hit (chainsaw)

    WeaponDefinition *upgrade_weap_; // This weapon upgrades a previous one.

    // This affects how it will be selected if out of ammo.  Also
    // determines the cycling order when on the same key.  Dangerous
    // weapons are not auto-selected when out of ammo.
    int  priority_;
    bool dangerous_;

    // Attack type for the WEAPON_EJECT code pointer.

    AttackDefinition *eject_attack_;

    // Sounds.
    // Played at the start of every readystate
    struct SoundEffect *idle_;

    // Played while the trigger is held (chainsaw)
    struct SoundEffect *engaged_;

    // Played while the trigger is held and it is pointed at a target.
    struct SoundEffect *hit_;

    // Played when the weapon is selected
    struct SoundEffect *start_;

    // Misc sounds
    struct SoundEffect *sound1_;
    struct SoundEffect *sound2_;
    struct SoundEffect *sound3_;

    // This close combat weapon should not push the target away (chainsaw)
    bool nothrust_;

    // which number key this weapon is bound to, or -1 for none
    int bind_key_;

    // -AJA- 2000/01/12: weapon special flags
    WeaponFlag specials_[4];

    // -AJA- 2000/03/18: when > 0, this weapon can zoom
    int zoom_fov_;

    // Dasho - When > 0, this weapon can zoom and will use this value instead of
    // zoom_fov
    float zoom_factor_;

    // -AJA- 2000/05/23: weapon loses accuracy when refired.
    bool refire_inacc_;

    // -AJA- 2000/10/20: show current clip in status bar (not total)
    bool show_clip_;

    // -AJA- 2007/11/12: clip is shared between 1st/2nd attacks.
    bool shared_clip_;

    // controls for weapon bob (up & down) and sway (left & right).
    // Given as percentages in DDF.
    float bobbing_;
    float swaying_;

    // -AJA- 2004/11/15: idle states (polish weapon, crack knuckles)
    int   idle_wait_;
    float idle_chance_;

    int   model_skin_; // -AJA- 2007/10/16: MD2 model support
    float model_aspect_;
    float model_bias_;
    float model_forward_;
    float model_side_;
    int   model_rotate_;

    // Lobo 2022: render order is Crosshair, Flash, Weapon
    //  instead of Weapon, Flash, CrossHair
    bool render_invert_;

    // Lobo 2022: sprite Y offset, mainly for Heretic weapons
    float y_adjust_;

    // Lobo 2023:  Video menu option "Crosshair size" is ignored for this
    // weapons custom crosshair
    bool ignore_crosshair_scaling_;

  public:
    inline int KeyPri(int idx) const // next/prev order value
    {
        int key = 1 + HMM_MAX(-1, HMM_MIN(10, bind_key_));
        int pri = 1 + HMM_MAX(-1, HMM_MIN(900, priority_));

        return (pri * 20 + key) * 100 + idx;
    }

  private:
    // disable copy construct and assignment operator
    explicit WeaponDefinition(WeaponDefinition &rhs)
    {
        (void)rhs;
    }
    WeaponDefinition &operator=(WeaponDefinition &rhs)
    {
        (void)rhs;
        return *this;
    }
};

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
