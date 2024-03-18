//----------------------------------------------------------------------------
//  EDGE DDF: Lines and Sectors
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

#include "ddf_colormap.h"
#include "ddf_types.h"

constexpr float kFloatUnused = 3.18081979f;

enum LineTrigger
{
    kLineTriggerNone,
    kLineTriggerShootable,
    kLineTriggerWalkable,
    kLineTriggerPushable,
    kLineTriggerManual, // same as pushable, but ignore any tag
    kLineTriggerAny
};

enum TriggerActivator
{
    kTriggerActivatorNone    = 0,
    kTriggerActivatorPlayer  = 1,
    kTriggerActivatorMonster = 2,
    kTriggerActivatorOther   = 4,
    kTriggerActivatorNoBot   = 8 // -AJA- 2009/10/17
};

enum TriggerHeightReference
{
    kTriggerHeightReferenceAbsolute = 0,      // Absolute from current position
    kTriggerHeightReferenceCurrent,           // Measure from current sector height
    kTriggerHeightReferenceSurrounding,       // Measure from surrounding heights
    kTriggerHeightReferenceLowestLowTexture,
    kTriggerHeightReferenceTriggeringLinedef, // Use the triggering linedef
    // additive flags
    kTriggerHeightReferenceMask    = 0x00FF,
    kTriggerHeightReferenceCeiling = 0x0100, // otherwise floor
    kTriggerHeightReferenceHighest = 0x0200, // otherwise lowest
    kTriggerHeightReferenceNext    = 0x0400, // otherwise absolute
    kTriggerHeightReferenceInclude = 0x0800, // otherwise excludes self
};

// Movement type
enum PlaneMoverType
{
    kPlaneMoverUndefined = 0,
    kPlaneMoverOnce,
    kPlaneMoverMoveWaitReturn,
    kPlaneMoverContinuous,
    kPlaneMoverPlatform,
    kPlaneMoverStairs,
    kPlaneMoverStop,
    kPlaneMoverToggle,   // -AJA- 2004/10/07: added.
    kPlaneMoverElevator, // -AJA- 2006/11/17: added.
};

enum DoorKeyType
{
    kDoorKeyNone = 0,
    // keep card/skull together, for easy SKCK check
    kDoorKeyBlueCard    = (1 << 0),
    kDoorKeyYellowCard  = (1 << 1),
    kDoorKeyRedCard     = (1 << 2),
    kDoorKeyGreenCard   = (1 << 3),
    kDoorKeyBlueSkull   = (1 << 4),
    kDoorKeyYellowSkull = (1 << 5),
    kDoorKeyRedSkull    = (1 << 6),
    kDoorKeyGreenSkull  = (1 << 7),
    // -AJA- 2001/06/30: ten new keys (these + Green ones)
    kDoorKeyGoldKey   = (1 << 8),
    kDoorKeySilverKey = (1 << 9),
    kDoorKeyBrassKey  = (1 << 10),
    kDoorKeyCopperKey = (1 << 11),
    kDoorKeySteelKey  = (1 << 12),
    kDoorKeyWoodenKey = (1 << 13),
    kDoorKeyFireKey   = (1 << 14),
    kDoorKeyWaterKey  = (1 << 15),
    // this is a special flag value that indicates that _all_ of the
    // keys in the bitfield must be held.  Normally we require _any_ of
    // the keys in the bitfield to be held.
    kDoorKeyStrictlyAllKeys = (1 << 16),
    // Boom compatibility: don't care if card or skull
    kDoorKeyCardOrSkull = (1 << 17),
    // mask of actual key bits
    kDoorKeyCardBits  = 0x000F,
    kDoorKeySkullBits = 0x00F0,
    kDoorKeyBitmask   = 0xFFFF
};

inline int ExpandKeyBits(int set)
{
    return ((set) | (((set)&kDoorKeyCardBits) << 4) | (((set)&kDoorKeySkullBits) >> 4));
}

enum ExitType
{
    kExitTypeNone = 0,
    kExitTypeNormal,
    kExitTypeSecret,
    kExitTypeHub
};

enum AppearsFlag
{
    kAppearsWhenNone        = 0x0000,
    kAppearsWhenSkillLevel1 = 0x0001,
    kAppearsWhenSkillLevel2 = 0x0002,
    kAppearsWhenSkillLevel3 = 0x0004,
    kAppearsWhenSkillLevel4 = 0x0008,
    kAppearsWhenSkillLevel5 = 0x0010,
    kAppearsWhenSingle      = 0x0100,
    kAppearsWhenCoop        = 0x0200,
    kAppearsWhenDeathMatch  = 0x0400,
    kAppearsWhenSkillBits   = 0x001F,
    kAppearsWhenNetBits     = 0x0700,
    kAppearsWhenDefault     = 0xFFFF
};

enum ExtraFloorType
{
    kExtraFloorTypeNone = 0x0000,
    // keeps the value from being zero
    kExtraFloorTypePresent = 0x0001,
    // floor is thick, has sides.  When clear: surface only
    kExtraFloorTypeThick = 0x0002,
    // floor is liquid, i.e. non-solid.  When clear: solid
    kExtraFloorTypeLiquid = 0x0004,
    // can monsters see through this extrafloor ?
    kExtraFloorTypeSeeThrough = 0x0010,
    // things with the WATERWALKER tag will not fall through.
    // Also, certain player sounds (pain, death) can be overridden when
    // in a water region.  Scope for other "waterish" effects...
    kExtraFloorTypeWater = 0x0020,
    // the region properties will "flood" all lower regions (unless it
    // finds another flooder).
    kExtraFloorTypeFlooder = 0x0040,
    // the properties (lighting etc..) below are not transferred from
    // the dummy sector, they'll be the same as the above region.
    kExtraFloorTypeNoShade = 0x0080,
    // take the side texture for THICK floors from the upper part of the
    // sidedef where the thick floor is drawn (instead of tagging line).
    kExtraFloorTypeSideUpper = 0x0100,
    // like above, but use the lower part.
    kExtraFloorTypeSideLower = 0x0200,
    // this controls the Y offsets on normal THICK floors.
    kExtraFloorTypeSideMidY = 0x0800,
    // Boom compatibility flag (for linetype 242)
    kExtraFloorTypeBoomTex = 0x1000
};

constexpr ExtraFloorType kExtraFloorThinDefaults   = ((ExtraFloorType)(kExtraFloorTypePresent | 0));
constexpr ExtraFloorType kExtraFloorThickDefaults  = ((ExtraFloorType)(kExtraFloorTypePresent | kExtraFloorTypeThick));
constexpr ExtraFloorType kExtraFloorLiquidDefaults = ((ExtraFloorType)(kExtraFloorTypePresent | kExtraFloorTypeLiquid));

enum ExtraFloorControl
{
    // remove an extra floor
    kExtraFloorControlNone = 0,
    kExtraFloorControlRemove
};

class ExtraFloorDefinition
{
  public:
    ExtraFloorDefinition();
    ExtraFloorDefinition(ExtraFloorDefinition &rhs);
    ~ExtraFloorDefinition();

  private:
    void Copy(ExtraFloorDefinition &src);

  public:
    void                  Default(void);
    ExtraFloorDefinition &operator=(ExtraFloorDefinition &src);

    ExtraFloorType    type_;
    ExtraFloorControl control_;
};

class PlaneMoverDefinition
{
  public:
    PlaneMoverDefinition();
    PlaneMoverDefinition(PlaneMoverDefinition &rhs);
    ~PlaneMoverDefinition();

    enum PlaneMoverDefault
    {
        kPlaneMoverDefaultCeilingLine,
        kPlaneMoverDefaultCeilingSect,
        kPlaneMoverDefaultDonutFloor,
        kPlaneMoverDefaultFloorLine,
        kPlaneMoverDefaultFloorSect,
        kTotalPlaneMoverDefaultTypes
    };

  private:
    void Copy(PlaneMoverDefinition &src);

  public:
    void                  Default(PlaneMoverDefault def);
    PlaneMoverDefinition &operator=(PlaneMoverDefinition &rhs);

    // Type of floor: raise/lower/etc
    PlaneMoverType type_;

    // True for a ceiling, false for a floor
    bool is_ceiling_;

    // How fast the plane moves.
    float speed_up_;
    float speed_down_;

    // This refers to what the dest. height refers to.
    TriggerHeightReference destref_;

    // Destination height.
    float dest_;

    // -AJA- 2001/05/28: This specifies the other height used.
    TriggerHeightReference otherref_;
    float                  other_;

    // Floor texture to change to.
    std::string tex_;

    // How much crush damage to do (0 for none).
    int crush_damage_;

    // PLAT/DOOR Specific: Time to wait before returning.
    int wait_;
    int prewait_;

    // Up/Down/Stop sfx
    struct SoundEffect *sfxstart_, *sfxup_, *sfxdown_, *sfxstop_;

    // Scrolling. -AJA- 2000/04/16
    BAMAngle scroll_angle_;
    float    scroll_speed_;

    // Boom compatibility bits
    bool ignore_texture_;
};

// --> Sliding door definition class

// FIXME Move inside sliding_door_c?
enum SlidingDoorType
{
    // not a slider
    kSlidingDoorTypeNone = 0,
    // door slides left (when looking at the right side)
    kSlidingDoorTypeLeft,
    // door slides right (when looking at the right side)
    kSlidingDoorTypeRight,
    // door opens from middle
    kSlidingDoorTypeCenter
};

// --> Sliding Door Definition

//
// Thin Sliding Doors
//
// -AJA- 2000/08/05: added this.
//
class SlidingDoor
{
  public:
    SlidingDoor();
    SlidingDoor(SlidingDoor &rhs);
    ~SlidingDoor();

  private:
    void Copy(SlidingDoor &src);

  public:
    void         Default(void);
    SlidingDoor &operator=(SlidingDoor &rhs);

    // type of slider, normally kSlidingDoorTypeNone
    SlidingDoorType type_;

    // how fast it opens/closes
    float speed_;

    // time to wait before returning (in tics).  Note: door stays open
    // after the last activation.
    int wait_;

    // whether or not the texture can be seen through
    bool see_through_;

    // how far it actually opens (usually 100%)
    float distance_;

    // sound effects.
    struct SoundEffect *sfx_start_;
    struct SoundEffect *sfx_open_;
    struct SoundEffect *sfx_close_;
    struct SoundEffect *sfx_stop_;
};

class DonutDefinition
{
  public:
    DonutDefinition();
    DonutDefinition(DonutDefinition &rhs);
    ~DonutDefinition();

  private:
    void Copy(DonutDefinition &src);

  public:
    void             Default(void);
    DonutDefinition &operator=(DonutDefinition &rhs);

    // Do Donut?

    //
    // FIXME! Make the objects that use this require
    //        a pointer/ref. This becomes an
    //        therefore becomes an unnecessary entry
    //
    bool dodonut_;

    // FIXME! Strip out the d_ since we're not trying to
    // to differentiate them now?

    // SFX for inner donut parts
    struct SoundEffect *d_sfxin_, *d_sfxinstop_;

    // SFX for outer donut parts
    struct SoundEffect *d_sfxout_, *d_sfxoutstop_;
};

// -AJA- 1999/07/12: teleporter special flags.
// FIXME!! Move into teleport def class?
enum TeleportSpecial
{
    kTeleportSpecialNone       = 0,
    kTeleportSpecialRelative   = 0x0001, // keep same relative angle
    kTeleportSpecialSameHeight = 0x0002, // keep same height off the floor
    kTeleportSpecialSameSpeed  = 0x0004, // keep same momentum
    kTeleportSpecialSameOffset = 0x0008, // keep same X/Y offset along line
    kTeleportSpecialSameAbsDir = 0x0010, // keep same _absolute_ angle (DEPRECATED)
    kTeleportSpecialRotate     = 0x0020, // rotate by target angle     (DEPRECATED)
    kTeleportSpecialLine       = 0x0100, // target is a line (not a thing)
    kTeleportSpecialFlipped    = 0x0200, // pretend target was flipped 180 degrees
    kTeleportSpecialSilent     = 0x0400  // no fog or sound
};

class TeleportDefinition
{
  public:
    TeleportDefinition();
    TeleportDefinition(TeleportDefinition &rhs);
    ~TeleportDefinition();

  private:
    void Copy(TeleportDefinition &src);

  public:
    void                Default(void);
    TeleportDefinition &operator=(TeleportDefinition &rhs);

    // If true, teleport activator
    //
    // FIXME! Make the objects that use this require
    //        a pointer/ref. This
    //        therefore becomes an unnecessary entry
    //
    bool teleport_;

    // effect object spawned when going in...
    const MapObjectDefinition *inspawnobj_; // FIXME! Do mobjtypes.Lookup()?
    std::string                inspawnobj_ref_;

    // effect object spawned when going out...
    const MapObjectDefinition *outspawnobj_; // FIXME! Do mobjtypes.Lookup()?
    std::string                outspawnobj_ref_;

    // Teleport delay
    int delay_;

    // Special flags.
    TeleportSpecial special_;
};

enum LightSpecialType
{
    kLightSpecialTypeNone,
    // set light to new level instantly
    kLightSpecialTypeSet,
    // fade light to new level over time
    kLightSpecialTypeFade,
    // flicker like a fire
    kLightSpecialTypeFireFlicker,
    // smoothly fade between bright and dark, continously
    kLightSpecialTypeGlow,
    // blink randomly between bright and dark
    kLightSpecialTypeFlash,
    // blink between bright and dark, alternating
    kLightSpecialTypeStrobe
};

// --> Light information class
class LightSpecialDefinition
{
  public:
    LightSpecialDefinition();
    LightSpecialDefinition(LightSpecialDefinition &rhs);
    ~LightSpecialDefinition();

  private:
    void Copy(LightSpecialDefinition &src);

  public:
    void                    Default(void);
    LightSpecialDefinition &operator=(LightSpecialDefinition &rhs);

    LightSpecialType type_;

    // light level to change to (for SET and FADE)
    int level_;

    // chance value for FLASH type
    float chance_;

    // time remaining dark and bright, in tics
    int darktime_;
    int brighttime_;

    // synchronisation time, in tics
    int sync_;

    // stepping used for FADE and GLOW types
    int step_;
};

class LadderDefinition
{
  public:
    LadderDefinition();
    LadderDefinition(LadderDefinition &rhs);
    ~LadderDefinition();

  private:
    void Copy(LadderDefinition &src);

  public:
    void              Default(void);
    LadderDefinition &operator=(LadderDefinition &rhs);

    // height of ladder itself.  Zero or negative disables.  Bottom of
    // ladder comes from Y_OFFSET on the linedef.
    float height_;
};

enum LineEffectType
{
    kLineEffectTypeNONE = 0,
    // make tagged lines (inclusive) 50% translucent
    kLineEffectTypeTranslucency = (1 << 0),
    // make tagged walls (inclusive) scroll using vector
    kLineEffectTypeVectorScroll = (1 << 1),
    // make source line scroll using sidedef offsets
    kLineEffectTypeOffsetScroll = (1 << 2),
    // experimental: tagged walls (inclusive) scaling & skewing
    kLineEffectTypeScale = (1 << 3),
    kLineEffectTypeSkew  = (1 << 4),
    // experimental: transfer properties to tagged walls (incl)
    kLineEffectTypeLightWall = (1 << 5),
    // experimental: make tagged lines (exclusive) non-blocking
    kLineEffectTypeUnblockThings = (1 << 6),
    // experimental: make tagged lines (incl) block bullets/missiles
    kLineEffectTypeBlockShots = (1 << 7),
    // experimental: make tagged lines (incl) block monster sight
    kLineEffectTypeBlockSight = (1 << 8),
    // experimental: transfer upper texture to SKY
    kLineEffectTypeSkyTransfer = (1 << 9),
    // make all tagged lines scroll using this sidedef's offsets (MBF21)
    kLineEffectTypeTaggedOffsetScroll = (1 << 10),
    // block land monsters (MBF21)
    kLineEffectTypeBlockGroundedMonsters = (1 << 11),
    // block players (MBF21)
    kLineEffectTypeBlockPlayers  = (1 << 12),
    kLineEffectTypeStretchWidth  = (1 << 13), // stretch the texture horizontally to line length
    kLineEffectTypeStretchHeight = (1 << 14), // stretch the texture vertically to line length
};

enum SectorEffectType
{
    kSectorEffectTypeNone = 0,
    // transfer sector lighting to tagged floors/ceilings
    kSectorEffectTypeLightFloor   = (1 << 0),
    kSectorEffectTypeLightCeiling = (1 << 1),
    // make tagged floors/ceilings scroll
    kSectorEffectTypeScrollFloor   = (1 << 2),
    kSectorEffectTypeScrollCeiling = (1 << 3),
    // push things on tagged floor
    kSectorEffectTypePushThings = (1 << 4),
    // restore light/scroll/push in tagged floors/ceilings
    kSectorEffectTypeResetFloor   = (1 << 6),
    kSectorEffectTypeResetCeiling = (1 << 7),
    // set floor/ceiling texture scale
    kSectorEffectTypeScaleFloor   = (1 << 8),
    kSectorEffectTypeScaleCeiling = (1 << 9),
    // align floor/ceiling texture to line
    kSectorEffectTypeAlignFloor   = (1 << 10),
    kSectorEffectTypeAlignCeiling = (1 << 11),
    // set various force parameters
    kSectorEffectTypeSetFriction  = (1 << 12),
    kSectorEffectTypeWindForce    = (1 << 13),
    kSectorEffectTypeCurrentForce = (1 << 14),
    kSectorEffectTypePointForce   = (1 << 15),
    // BOOM's linetype 242 -- deep water effect (etc)
    kSectorEffectTypeBoomHeights = (1 << 16)
};

enum PortalEffectType
{
    kPortalEffectTypeNone     = 0,
    kPortalEffectTypeStandard = (1 << 0),
    kPortalEffectTypeMirror   = (1 << 1),
    kPortalEffectTypeCamera   = (1 << 2),
};

// -AJA- 2008/03/08: slope types
enum SlopeType
{
    kSlopeTypeNONE          = 0,
    kSlopeTypeDetailFloor   = (1 << 0),
    kSlopeTypeDetailCeiling = (1 << 1),
};

// -AJA- 1999/10/12: Generalised scrolling parts of walls.
enum ScrollingPart
{
    kScrollingPartNone = 0,

    kScrollingPartRightUpper  = 0x0001,
    kScrollingPartRightMiddle = 0x0002,
    kScrollingPartRightLower  = 0x0004,

    kScrollingPartLeftUpper  = 0x0010,
    kScrollingPartLeftMiddle = 0x0020,
    kScrollingPartLeftLower  = 0x0040,

    kScrollingPartLeftRevX = 0x0100,
    kScrollingPartLeftRevY = 0x0200
};

constexpr ScrollingPart kScrollingPartRight =
    ((ScrollingPart)(kScrollingPartRightUpper | kScrollingPartRightMiddle | kScrollingPartRightLower));
constexpr ScrollingPart kScrollingPartLeft =
    ((ScrollingPart)(kScrollingPartLeftUpper | kScrollingPartLeftMiddle | kScrollingPartLeftLower));

// -AJA- 1999/12/07: Linedef special flags
enum LineSpecial
{
    kLineSpecialNone = 0,
    // player must be able to vertically reach this linedef to press it
    kLineSpecialMustReach = (1 << 0),
    // don't change the texture on other linedefs with the same tag
    kLineSpecialSwitchSeparate = (1 << 1),
    // -AJA- 2007/09/14: for SECTOR_EFFECT with no tag
    kLineSpecialBackSector = (1 << 2),
};

// BOOM scroll types (didn't want to eat up flags elsewhere)
enum BoomScrollerType
{
    BoomScrollerTypeNone     = 0,
    BoomScrollerTypeDisplace = (1 << 0),
    BoomScrollerTypeAccel    = (1 << 1),
};

class LineType
{
  public:
    LineType();
    ~LineType();

  public:
    void Default(void);
    void CopyDetail(LineType &src);

    // Member vars....
    int number_;

    // Linedef will change to this.
    int newtrignum_;

    // Determines whether line is shootable/walkable/pushable
    LineTrigger type_;

    // Determines whether line is acted on by monsters/players/projectiles
    TriggerActivator obj_;

    // Keys required to use
    DoorKeyType keys_;

    // Number of times this line can be triggered. -1 = Any amount
    int count_;

    // Floor
    PlaneMoverDefinition f_;

    // Ceiling
    PlaneMoverDefinition c_;

    // Donut
    DonutDefinition d_;

    // Slider
    SlidingDoor s_;

    // Ladder -AJA- 2001/03/10
    LadderDefinition ladder_;

    // Teleporter
    TeleportDefinition t_;

    // Lobo: item to spawn (or nullptr).  The mobjdef pointer is only valid
    // after
    //  DdfMobjCleanUp() has been called.
    const MapObjectDefinition *effectobject_;
    std::string                effectobject_ref_;

    // Handle this line differently
    bool glass_;

    // line texture to change to.
    std::string brokentex_;

    // LIGHT SPECIFIC
    // Things may be added here; start strobing/flashing glowing lights.
    LightSpecialDefinition l_;

    // EXIT SPECIFIC
    ExitType e_exit_;
    int      hub_exit_;

    // SCROLLER SPECIFIC
    float         s_xspeed_;
    float         s_yspeed_;
    ScrollingPart scroll_parts_;

    // -ACB- 1998/09/11 Message handling
    std::string failedmessage_;

    // -AJA- 2011/01/14: sound for unusable locked door
    struct SoundEffect *failed_sfx_;

    // Colourmap changing
    // -AJA- 1999/07/09: Now uses colmap.ddf
    const Colormap *use_colourmap_;

    // Property Transfers (kFloatUnused if unset)
    float gravity_;
    float friction_;
    float viscosity_;
    float drag_;

    // Ambient sound transfer
    struct SoundEffect *ambient_sfx_;

    // Activation sound (overrides the switch sound)
    struct SoundEffect *activate_sfx_;

    int music_;

    // Automatically trigger this line at level start ?
    bool autoline_;

    // Activation only possible from right side of line
    bool singlesided_;

    // -AJA- 1999/06/21: Extra floor handling
    ExtraFloorDefinition ef_;

    // -AJA- 1999/06/30: TRANSLUCENT MID-TEXTURES
    float translucency_;

    // -AJA- 1999/10/24: Appearance control.
    AppearsFlag appear_;

    // -AJA- 1999/12/07: line special flags
    LineSpecial special_flags_;

    // -AJA- 2000/01/09: enable (if +1) or disable (if -1) all radius
    //       triggers with the same tag as the linedef.
    int trigger_effect_;

    // -AJA- 2000/09/28: BOOM compatibility fields (and more !).
    LineEffectType   line_effect_;
    ScrollingPart    line_parts_;
    BoomScrollerType scroll_type_;

    SectorEffectType sector_effect_;
    PortalEffectType portal_effect_;

    SlopeType slope_type_;

    // -AJA- 2007/07/05: color for effects (e.g. MIRRORs)
    RGBAColor fx_color_;

  private:
    // disable copy construct and assignment operator
    explicit LineType(LineType &rhs)
    {
        (void)rhs;
    }
    LineType &operator=(LineType &rhs)
    {
        (void)rhs;
        return *this;
    }
};

// --> Linetype container class

class LineTypeContainer : public std::vector<LineType *>
{
  public:
    LineTypeContainer();
    ~LineTypeContainer();

  private:
    LineType *lookup_cache_[kLookupCacheSize];

  public:
    LineType *Lookup(int num);
    void      Reset();
};

// ------------------------------------------------------------------
// -------------------------SECTOR TYPES-----------------------------
// ------------------------------------------------------------------

// -AJA- 1999/11/25: Sector special flags
enum SectorFlag
{
    kSectorFlagNone = 0x0000,

    // apply damage whenever in whole region (not just touching floor)
    kSectorFlagWholeRegion = 0x0001,

    // goes with above: damage is proportional to how deep you're in
    // Also affects pushing sectors.
    kSectorFlagProportional = 0x0002,

    // push _all_ things, including NOGRAVITY ones
    kSectorFlagPushAll = 0x0008,

    // the push force is constant, regardless of the mass
    kSectorFlagPushConstant = 0x0010,

    // breathing support: this sector contains no air.
    kSectorFlagAirLess = 0x0020,

    // player can swim in this sector
    kSectorFlagSwimming = 0x0040,

    // sounds will apply underwater effects in this sector
    kSectorFlagSubmergedSFX = 0x0080,

    // sounds will be heavily muffled in this sector
    kSectorFlagVacuumSFX = 0x0100,

    // sounds will reverberate/echo in this sector
    kSectorFlagReverbSFX = 0x0200
};

class SectorType
{
  public:
    SectorType();
    ~SectorType();

  public:
    void Default(void);
    void CopyDetail(SectorType &src);

    // Member vars....
    int number_;

    // This sector gives you secret count
    bool secret_;
    bool crush_;

    // Hub entry, player starts are treated differently
    bool hub_;

    // Gravity
    float gravity_;
    float friction_;
    float viscosity_;
    float drag_;

    // Movement
    PlaneMoverDefinition f_, c_;

    // Lighting
    LightSpecialDefinition l_;

    // Slime
    DamageClass damage_;

    // -AJA- 1999/11/25: sector special flags
    SectorFlag special_flags_;

    // Exit.  Also disables god mode.
    ExitType e_exit_;

    // Colourmap changing
    // -AJA- 1999/07/09: Now uses colmap.ddf
    const Colormap *use_colourmap_;

    // Ambient sound transfer
    struct SoundEffect *ambient_sfx_;

    // -AJA- 2008/01/20: Splash sounds
    struct SoundEffect *splash_sfx_;

    // -AJA- 1999/10/24: Appearance control.
    AppearsFlag appear_;

    // -AJA- 2000/04/16: Pushing (fixed direction).
    float    push_speed_;
    float    push_zspeed_;
    BAMAngle push_angle_;

    // Dasho 2022 - Params for user-defined reverb in sectors
    std::string reverb_type_;
    float       reverb_ratio_;
    float       reverb_delay_;

    float floor_bob_;
    float ceiling_bob_;

    Colormap *fog_cmap_;
    RGBAColor fog_color_;
    float     fog_density_;

  private:
    // disable copy construct and assignment operator
    explicit SectorType(SectorType &rhs)
    {
        (void)rhs;
    }
    SectorType &operator=(SectorType &rhs)
    {
        (void)rhs;
        return *this;
    }
};

class SectorTypeContainer : public std::vector<SectorType *>
{
  public:
    SectorTypeContainer();
    ~SectorTypeContainer();

  private:
    SectorType *lookup_cache_[kLookupCacheSize];

  public:
    SectorType *Lookup(int num);
    void        Reset();
};

/* EXTERNALISATIONS */

extern LineTypeContainer   linetypes;   // -ACB- 2004/07/05 Implemented
extern SectorTypeContainer sectortypes; // -ACB- 2004/07/05 Implemented

void DdfReadLines(const std::string &data);
void DdfReadSectors(const std::string &data);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
