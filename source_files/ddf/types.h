//----------------------------------------------------------------------------
//  EDGE Basic Types
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

#ifndef __DDF_TYPE_H__
#define __DDF_TYPE_H__

#include <string>
#include <vector>

#include "math_bam.h"
#include "math_color.h"

class mobjtype_c;
class weapondef_c;

// percentage type.  Ranges from 0.0f - 1.0f
typedef float percent_t;

#define PERCENT_MAKE(val)     ((val) / 100.0f)
#define PERCENT_2_FLOAT(perc) (perc)

// a bitset is a set of named bits, from `A' to `Z'.
typedef int bitset_t;

#define BITSET_EMPTY    0
#define BITSET_FULL     0x7FFFFFFF
#define BITSET_MAKE(ch) (1 << ((ch) - 'A'))

// mobjdef container
#define LOOKUP_CACHESIZE 211

class mobj_strref_c
{
   public:
    mobj_strref_c() : name(), def(nullptr) {}
    mobj_strref_c(const char *s) : name(s), def(nullptr) {}
    mobj_strref_c(const mobj_strref_c &rhs) : name(rhs.name), def(nullptr) {}
    ~mobj_strref_c(){};

   private:
    std::string name;

    const mobjtype_c *def;

   public:
    const char *GetName() const { return name.c_str(); }

    const mobjtype_c *GetRef();
    // Note: this returns nullptr if not found, in which case you should
    // produce an error, since future calls will do the search again.

    mobj_strref_c &operator=(mobj_strref_c &rhs)
    {
        if (&rhs != this)
        {
            name = rhs.name;
            def  = nullptr;
        }

        return *this;
    }
};

typedef struct
{
    int first, last;
} state_range_t;

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
} benefit_type_e;

typedef struct benefit_s
{
    // next in linked list
    struct benefit_s *next;

    // type of benefit (ammo, ammo-limit, weapon, key, health, armour,
    // powerup, inventory, or inventory-limit).
    benefit_type_e type;

    // sub-type (specific type of ammo, weapon, key, powerup, or inventory). For
    // armour this is the class, for health it is unused.
    union
    {
        int          type;
        weapondef_c *weap;
    } sub;

    // amount of benefit (e.g. quantity of ammo or health).  For weapons
    // and keys, this is a boolean value: 1 to give, 0 to ignore.  For
    // powerups, it is number of seconds the powerup lasts.
    float amount;

    // for health, armour and powerups, don't make the new value go
    // higher than this (if it is already higher, prefer not to pickup
    // the object).
    float limit;
} benefit_t;

class label_offset_c
{
   public:
    label_offset_c();
    label_offset_c(label_offset_c &rhs);
    ~label_offset_c();

   private:
    void Copy(label_offset_c &src);

   public:
    void            Default();
    label_offset_c &operator=(label_offset_c &rhs);

    std::string label;
    int         offset;
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
    void      Default(default_e def);
    damage_c &operator=(damage_c &rhs);

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
    // (PAIN, DEATH, OVERKILL).  Defaults to nullptr.
    label_offset_c pain, death, overkill;

    // this flag says that the damage is unaffected by the player's
    // armour -- and vice versa.
    bool no_armour;

    // Color of the flash when player is hit by this damage type
    RGBAColor damage_flash_colour;

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

// FIXME!!! Move enums into attackdef_t
typedef enum
{
    ATK_NONE = 0,
    ATK_PROJECTILE,
    ATK_SPAWNER,
    ATK_DOUBLESPAWNER,  // Lobo 2021: doom64 pain elemental
    ATK_TRIPLESPAWNER,
    ATK_SPREADER,
    ATK_RANDOMSPREAD,
    ATK_SHOT,
    ATK_TRACKER,
    ATK_CLOSECOMBAT,
    ATK_SHOOTTOSPOT,
    ATK_SKULLFLY,
    ATK_SMARTPROJECTILE,
    ATK_SPRAY,
    ATK_DUALATTACK,  // Dasho 2023: Execute two independent atkdefs with one
                     // command
    ATK_PSYCHIC,     // Dasho 2023: Beta Lost Soul attack
    NUMATKCLASS
} attackstyle_e;

typedef enum
{
    AF_None = 0,

    AF_TraceSmoke      = (1 << 0),
    AF_KillFailedSpawn = (1 << 1),
    AF_PrestepSpawn    = (1 << 2),
    AF_SpawnTelefrags  = (1 << 3),

    AF_NeedSight  = (1 << 4),
    AF_FaceTarget = (1 << 5),
    AF_Player     = (1 << 6),
    AF_ForceAim   = (1 << 7),

    AF_AngledSpawn    = (1 << 8),
    AF_NoTriggerLines = (1 << 9),
    AF_SilentToMon    = (1 << 10),
    AF_NoTarget       = (1 << 11),
    AF_Vampire        = (1 << 12),
} attackflags_e;

// attack definition class
class atkdef_c
{
   public:
    atkdef_c();
    ~atkdef_c();

   public:
    void Default();
    void CopyDetail(atkdef_c &src);

    // Member vars
    std::string name;

    attackstyle_e attackstyle;
    attackflags_e flags;
    struct sfx_s *initsound;
    struct sfx_s *sound;
    float         accuracy_slope;
    BAMAngle      accuracy_angle;
    float         xoffset;
    float         yoffset;
    BAMAngle      angle_offset;  // -AJA- 1999/09/10.
    float         slope_offset;  //
    BAMAngle      trace_angle;   // -AJA- 2005/02/08.
    float         assault_speed;
    float         height;
    float         range;
    int           count;
    int           tooclose;
    float         berserk_mul;  // -AJA- 2005/08/06.
    damage_c      damage;

    // class of the attack.
    bitset_t attack_class;

    // object init state.  The integer value only becomes valid after
    // DDF_AttackCleanUp() has been called.
    int         objinitstate;
    std::string objinitstate_ref;

    percent_t notracechance;
    percent_t keepfirechance;

    // the MOBJ that is integrated with this attack, or nullptr
    const mobjtype_c *atk_mobj;

    // spawned object (for spawners).  The mobjdef pointer only becomes
    // valid after DDF_AttackCleanUp().  Can be nullptr.
    const mobjtype_c *spawnedobj;
    std::string       spawnedobj_ref;
    int               spawn_limit;

    // puff object.  The mobjdef pointer only becomes valid after
    // DDF_AttackCleanUp() has been called.  Can be nullptr.
    const mobjtype_c *puff;
    std::string       puff_ref;

    // For DUALATTACK type only
    atkdef_c *dualattack1;
    atkdef_c *dualattack2;

   private:
    // disable copy construct and assignment operator
    explicit atkdef_c(atkdef_c &rhs) { (void)rhs; }
    atkdef_c &operator=(atkdef_c &rhs)
    {
        (void)rhs;
        return *this;
    }
};

// Ammunition types defined.
typedef enum
{
    AM_DontCare = -2,  // Only used for P_SelectNewWeapon()
    AM_NoAmmo   = -1,  // Unlimited for chainsaw / fist.

    AM_Bullet = 0,  // Pistol / chaingun ammo.
    AM_Shell,       // Shotgun / double barreled shotgun.
    AM_Rocket,      // Missile launcher.
    AM_Cell,        // Plasma rifle, BFG.

    // New ammo types
    AM_Pellet,
    AM_Nail,
    AM_Grenade,
    AM_Gas,

    AM_9,
    AM_10,
    AM_11,
    AM_12,
    AM_13,
    AM_14,
    AM_15,
    AM_16,
    AM_17,
    AM_18,
    AM_19,
    AM_20,
    AM_21,
    AM_22,
    AM_23,
    AM_24,
    AM_25,
    AM_26,
    AM_27,
    AM_28,
    AM_29,
    AM_30,
    AM_31,
    AM_32,
    AM_33,
    AM_34,
    AM_35,
    AM_36,
    AM_37,
    AM_38,
    AM_39,
    AM_40,
    AM_41,
    AM_42,
    AM_43,
    AM_44,
    AM_45,
    AM_46,
    AM_47,
    AM_48,
    AM_49,
    AM_50,
    AM_51,
    AM_52,
    AM_53,
    AM_54,
    AM_55,
    AM_56,
    AM_57,
    AM_58,
    AM_59,
    AM_60,
    AM_61,
    AM_62,
    AM_63,
    AM_64,
    AM_65,
    AM_66,
    AM_67,
    AM_68,
    AM_69,
    AM_70,
    AM_71,
    AM_72,
    AM_73,
    AM_74,
    AM_75,
    AM_76,
    AM_77,
    AM_78,
    AM_79,
    AM_80,
    AM_81,
    AM_82,
    AM_83,
    AM_84,
    AM_85,
    AM_86,
    AM_87,
    AM_88,
    AM_89,
    AM_90,
    AM_91,
    AM_92,
    AM_93,
    AM_94,
    AM_95,
    AM_96,
    AM_97,
    AM_98,
    AM_99,

    NUMAMMO  // Total count (99)
} ammotype_e;

// -AJA- 2000/01/12: Weapon special flags
typedef enum
{
    WPSP_None = 0,

    WPSP_SilentToMon = (1 << 0),  // monsters cannot hear this weapon
    WPSP_Animated    = (1 << 1),  // raise/lower states are animated

    WPSP_SwitchAway = (1 << 4),  // select new weapon when we run out of ammo

    // reload flags:
    WPSP_Trigger = (1 << 8),   // allow reload while holding trigger
    WPSP_Fresh   = (1 << 9),   // automatically reload when new ammo is avail
    WPSP_Manual  = (1 << 10),  // enables the manual reload key
    WPSP_Partial = (1 << 11),  // manual reload: allow partial refill

    // MBF21 flags:
    WPSP_NoAutoFire =
        (1 << 12),  // Do not fire if switched to while trigger is held
} weapon_flag_e;

#define DEFAULT_WPSP \
    (weapon_flag_e)(WPSP_Trigger | WPSP_Manual | WPSP_SwitchAway | WPSP_Partial)

class weapondef_c
{
   public:
    weapondef_c();
    ~weapondef_c();

   public:
    void Default(void);
    void CopyDetail(weapondef_c &src);

    // Weapon's name, etc...
    std::string name;

    atkdef_c *attack[4];  // Attack type used.

    ammotype_e ammo[4];         // Type of ammo this weapon uses.
    int        ammopershot[4];  // Ammo used per shot.
    int  clip_size[4];  // Amount of shots in a clip (if <= 1, non-clip weapon)
    bool autofire[4];   // If true, this is an automatic else it's semiauto

    float kick;  // Amount of kick this weapon gives

    // range of states used
    std::vector<state_range_t> state_grp;

    int up_state;     // State to use when raising the weapon
    int down_state;   // State to use when lowering the weapon (if changing
                      // weapon)
    int ready_state;  // State that the weapon is ready to fire in
    int empty_state;  // State when weapon is empty.  Usually zero
    int idle_state;   // State to use when polishing weapon

    int attack_state[4];   // State showing the weapon 'firing'
    int reload_state[4];   // State showing the weapon being reloaded
    int discard_state[4];  // State showing the weapon discarding a clip
    int warmup_state[4];   // State showing the weapon warming up
    int flash_state[4];    // State showing the muzzle flash

    int crosshair;   // Crosshair states
    int zoom_state;  // State showing viewfinder when zoomed.  Can be zero

    bool no_cheat;  // Not given for cheats (Note: set by #CLEARALL)

    bool autogive;  // The player gets this weapon on spawn.  (Fist + Pistol)
    bool feedback;  // This weapon gives feedback on hit (chainsaw)

    weapondef_c *upgrade_weap;  // This weapon upgrades a previous one.

    // This affects how it will be selected if out of ammo.  Also
    // determines the cycling order when on the same key.  Dangerous
    // weapons are not auto-selected when out of ammo.
    int  priority;
    bool dangerous;

    // Attack type for the WEAPON_EJECT code pointer.

    atkdef_c *eject_attack;

    // Sounds.
    // Played at the start of every readystate
    struct sfx_s *idle;

    // Played while the trigger is held (chainsaw)
    struct sfx_s *engaged;

    // Played while the trigger is held and it is pointed at a target.
    struct sfx_s *hit;

    // Played when the weapon is selected
    struct sfx_s *start;

    // Misc sounds
    struct sfx_s *sound1;
    struct sfx_s *sound2;
    struct sfx_s *sound3;

    // This close combat weapon should not push the target away (chainsaw)
    bool nothrust;

    // which number key this weapon is bound to, or -1 for none
    int bind_key;

    // -AJA- 2000/01/12: weapon special flags
    weapon_flag_e specials[4];

    // -AJA- 2000/03/18: when > 0, this weapon can zoom
    int zoom_fov;

    // Dasho - When > 0, this weapon can zoom and will use this value instead of
    // zoom_fov
    float zoom_factor;

    // -AJA- 2000/05/23: weapon loses accuracy when refired.
    bool refire_inacc;

    // -AJA- 2000/10/20: show current clip in status bar (not total)
    bool show_clip;

    // -AJA- 2007/11/12: clip is shared between 1st/2nd attacks.
    bool shared_clip;

    // controls for weapon bob (up & down) and sway (left & right).
    // Given as percentages in DDF.
    percent_t bobbing;
    percent_t swaying;

    // -AJA- 2004/11/15: idle states (polish weapon, crack knuckles)
    int       idle_wait;
    percent_t idle_chance;

    int   model_skin;  // -AJA- 2007/10/16: MD2 model support
    float model_aspect;
    float model_bias;
    float model_forward;
    float model_side;
    int   model_rotate;

    // Lobo 2022: render order is Crosshair, Flash, Weapon
    //  instead of Weapon, Flash, CrossHair
    bool render_invert;

    // Lobo 2022: sprite Y offset, mainly for Heretic weapons
    float y_adjust;

    // Lobo 2023:  Video menu option "Crosshair size" is ignored for this
    // weapons custom crosshair
    bool ignore_crosshair_scaling;

   public:
    inline int KeyPri(int idx) const  // next/prev order value
    {
        int key = 1 + HMM_MAX(-1, HMM_MIN(10, bind_key));
        int pri = 1 + HMM_MAX(-1, HMM_MIN(900, priority));

        return (pri * 20 + key) * 100 + idx;
    }

   private:
    // disable copy construct and assignment operator
    explicit weapondef_c(weapondef_c &rhs) { (void)rhs; }
    weapondef_c &operator=(weapondef_c &rhs)
    {
        (void)rhs;
        return *this;
    }
};

#endif /*__DDF_TYPE_H__*/

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
