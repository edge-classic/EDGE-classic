//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Sectors)
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
// Sector Setup and Parser Code
//
// -KM- 1998/09/27 Written.

#include <string.h>

#include "colormap.h"
#include "line.h"
#include "local.h"

#undef DF
#define DF DDF_FIELD

#define DDF_SectHashFunc(x) (((x) + kLookupCacheSize) % kLookupCacheSize)

static SectorType *dynamic_sector;

SectorTypeContainer sectortypes;  // <-- User-defined

static SectorType *default_sector;

void        DDF_SectGetSpecialFlags(const char *info, void *storage);
static void DDF_SectMakeCrush(const char *info);

#undef DDF_CMD_BASE
#define DDF_CMD_BASE dummy_sector
static SectorType dummy_sector;

static const DDFCommandList sect_commands[] = {
    // sub-commands
    DDF_SUB_LIST("FLOOR", f_, floor_commands),
    DDF_SUB_LIST("CEILING", c_, floor_commands),
    DDF_SUB_LIST("DAMAGE", damage_, damage_commands),

    DF("SECRET", secret_, DDF_MainGetBoolean),
    DF("HUB", hub_, DDF_MainGetBoolean),
    DF("SPECIAL", special_flags_, DDF_SectGetSpecialFlags),

    DF("LIGHT_TYPE", l_.type_, DDF_SectGetLighttype),
    DF("LIGHT_LEVEL", l_.level_, DDF_MainGetNumeric),
    DF("LIGHT_DARKTIME", l_.darktime_, DDF_MainGetTime),
    DF("LIGHT_BRIGHTTIME", l_.brighttime_, DDF_MainGetTime),
    DF("LIGHT_CHANCE", l_.chance_, DDF_MainGetPercent),
    DF("LIGHT_SYNC", l_.sync_, DDF_MainGetTime),
    DF("LIGHT_STEP", l_.step_, DDF_MainGetNumeric),
    DF("EXIT", e_exit_, DDF_SectGetExit),
    DF("USE_COLOURMAP", use_colourmap_, DDF_MainGetColourmap),
    DF("GRAVITY", gravity_, DDF_MainGetFloat),
    DF("FRICTION", friction_, DDF_MainGetFloat),
    DF("VISCOSITY", viscosity_, DDF_MainGetFloat),
    DF("DRAG", drag_, DDF_MainGetFloat),
    DF("AMBIENT_SOUND", ambient_sfx_, DDF_MainLookupSound),
    DF("SPLASH_SOUND", splash_sfx_, DDF_MainLookupSound),
    DF("WHEN_APPEAR", appear_, DDF_MainGetWhenAppear),
    DF("PUSH_ANGLE", push_angle_, DDF_MainGetAngle),
    DF("PUSH_SPEED", push_speed_, DDF_MainGetFloat),
    DF("PUSH_ZSPEED", push_zspeed_, DDF_MainGetFloat),

    // -AJA- backwards compatibility cruft...
    DF("DAMAGE", damage_.nominal_, DDF_MainGetFloat),
    DF("DAMAGETIME", damage_.delay_, DDF_MainGetTime),

    DF("REVERB TYPE", reverb_type_, DDF_MainGetString),
    DF("REVERB RATIO", reverb_ratio_, DDF_MainGetFloat),
    DF("REVERB DELAY", reverb_delay_, DDF_MainGetFloat),

    DF("FLOOR_BOB", floor_bob_, DDF_MainGetFloat),
    DF("CEILING_BOB", ceiling_bob_, DDF_MainGetFloat),

    DF("FOG_COLOR", fog_cmap_, DDF_MainGetColourmap),
    DF("FOG_DENSITY", fog_density_, DDF_MainGetPercent),

    DDF_CMD_END};

//
//  DDF PARSE ROUTINES
//

//
// SectorStartEntry
//
static void SectorStartEntry(const char *name, bool extend)
{
    int number = HMM_MAX(0, atoi(name));

    if (number == 0)
        DDF_Error("Bad sectortype number in sectors.ddf: %s\n", name);

    dynamic_sector = sectortypes.Lookup(number);

    if (extend)
    {
        if (!dynamic_sector)
            DDF_Error("Unknown sectortype to extend: %s\n", name);
        return;
    }

    // replaces an existing entry?
    if (dynamic_sector)
    {
        dynamic_sector->Default();
        return;
    }

    // not found, create a new one
    dynamic_sector          = new SectorType;
    dynamic_sector->number_ = number;

    sectortypes.push_back(dynamic_sector);
}

static void SectorDoTemplate(const char *contents)
{
    int number = HMM_MAX(0, atoi(contents));
    if (number == 0)
        DDF_Error("Bad sectortype number for template: %s\n", contents);

    SectorType *other = sectortypes.Lookup(number);

    if (!other || other == dynamic_sector)
        DDF_Error("Unknown sector template: '%s'\n", contents);

    dynamic_sector->CopyDetail(*other);
}

//
// SectorParseField
//
static void SectorParseField(const char *field, const char *contents, int index,
                             bool is_last)
{
#if (DEBUG_DDF)
    LogDebug("SECTOR_PARSE: %s = %s;\n", field, contents);
#endif

    if (DDF_CompareName(field, "TEMPLATE") == 0)
    {
        SectorDoTemplate(contents);
        return;
    }

    // backwards compatibility...
    if (DDF_CompareName(field, "CRUSH") == 0 ||
        DDF_CompareName(field, "CRUSH_DAMAGE") == 0)
    {
        DDF_SectMakeCrush(contents);
        return;
    }

    if (DDF_MainParseField(sect_commands, field, contents,
                           (uint8_t *)dynamic_sector))
        return;  // OK

    DDF_WarnError("Unknown sectors.ddf command: %s\n", field);
}

//
// SectorFinishEntry
//
static void SectorFinishEntry(void)
{
    if (dynamic_sector->fog_cmap_)
        dynamic_sector->fog_color_ = dynamic_sector->fog_cmap_->gl_color_;
}

//
// SectorClearAll
//
static void SectorClearAll(void)
{
    // 100% safe to delete all sector types
    sectortypes.Reset();
}

void DDF_ReadSectors(const std::string &data)
{
    DDFReadInfo sects;

    sects.tag      = "SECTORS";
    sects.lumpname = "DDFSECT";

    sects.start_entry  = SectorStartEntry;
    sects.parse_field  = SectorParseField;
    sects.finish_entry = SectorFinishEntry;
    sects.clear_all    = SectorClearAll;

    DDF_MainReadFile(&sects, data);
}

//
// DDF_SectorInit
//
void DDF_SectorInit(void)
{
    sectortypes.Reset();

    default_sector          = new SectorType;
    default_sector->number_ = 0;
}

//
// DDF_SectorCleanUp
//
void DDF_SectorCleanUp(void) { sectortypes.shrink_to_fit(); }

//----------------------------------------------------------------------------

static DDFSpecialFlags sector_specials[] = {
    {"WHOLE_REGION", kSectorFlagWholeRegion, 0},
    {"PROPORTIONAL", kSectorFlagProportional, 0},
    {"PUSH_ALL", kSectorFlagPushAll, 0},
    {"PUSH_CONSTANT", kSectorFlagPushConstant, 0},
    {"AIRLESS", kSectorFlagAirLess, 0},
    {"SWIM", kSectorFlagSwimming, 0},
    {"SUBMERGED_SFX", kSectorFlagSubmergedSFX, 0},
    {"VACUUM_SFX", kSectorFlagVacuumSFX, 0},
    {"REVERB_SFX", kSectorFlagReverbSFX, 0},
    {nullptr, 0, 0}};

//
// DDF_SectGetSpecialFlags
//
// Gets the sector specials.
//
void DDF_SectGetSpecialFlags(const char *info, void *storage)
{
    SectorFlag *special = (SectorFlag *)storage;

    int flag_value;

    switch (DDF_MainCheckSpecialFlag(info, sector_specials, &flag_value, true,
                                     false))
    {
        case kDDFCheckFlagPositive:
            *special = (SectorFlag)(*special | flag_value);

            break;

        case kDDFCheckFlagNegative:
            *special = (SectorFlag)(*special & ~flag_value);

            break;

        case kDDFCheckFlagUser:
        case kDDFCheckFlagUnknown:
            DDF_WarnError("Unknown sector special: %s", info);
            break;
    }
}

static DDFSpecialFlags exit_types[] = {{"NONE", kExitTypeNone, 0},
                                   {"NORMAL", kExitTypeNormal, 0},
                                   {"SECRET", kExitTypeSecret, 0},

                                   // -AJA- backwards compatibility cruft...
                                   {"!EXIT", kExitTypeNormal, 0},
                                   {nullptr, 0, 0}};

//
// DDF_SectGetExit
//
// Get the exit type
//
void DDF_SectGetExit(const char *info, void *storage)
{
    int *dest = (int *)storage;
    int  flag_value;

    switch (
        DDF_MainCheckSpecialFlag(info, exit_types, &flag_value, false, false))
    {
        case kDDFCheckFlagPositive:
        case kDDFCheckFlagNegative:
            (*dest) = flag_value;
            break;

        case kDDFCheckFlagUser:
        case kDDFCheckFlagUnknown:
            DDF_WarnError("Unknown Exit type: %s\n", info);
            break;
    }
}

static DDFSpecialFlags light_types[] = {
    {"NONE", kLightSpecialTypeNone, 0},
    {"SET", kLightSpecialTypeSet, 0},
    {"FADE", kLightSpecialTypeFade, 0},
    {"STROBE", kLightSpecialTypeStrobe, 0},
    {"FLASH", kLightSpecialTypeFlash, 0},
    {"GLOW", kLightSpecialTypeGlow, 0},
    {"FLICKER", kLightSpecialTypeFireFlicker, 0},
    {nullptr, 0, 0}};

//
// DDF_SectGetLighttype
//
// Get the light type
//
void DDF_SectGetLighttype(const char *info, void *storage)
{
    int *dest = (int *)storage;
    int  flag_value;

    switch (
        DDF_MainCheckSpecialFlag(info, light_types, &flag_value, false, false))
    {
        case kDDFCheckFlagPositive:
        case kDDFCheckFlagNegative:
            (*dest) = flag_value;
            break;

        case kDDFCheckFlagUser:
        case kDDFCheckFlagUnknown:
            DDF_WarnError("Unknown light type: %s\n", info);
            break;
    }
}

static DDFSpecialFlags movement_types[] = {
    {"MOVE", kPlaneMoverOnce, 0},
    {"MOVEWAITRETURN", kPlaneMoverMoveWaitReturn, 0},
    {"CONTINUOUS", kPlaneMoverContinuous, 0},
    {"PLAT", kPlaneMoverPlatform, 0},
    {"BUILDSTAIRS", kPlaneMoverStairs, 0},
    {"STOP", kPlaneMoverStop, 0},
    {"TOGGLE", kPlaneMoverToggle, 0},
    {"ELEVATOR", kPlaneMoverElevator, 0},
    {nullptr, 0, 0}};

//
// DDF_SectGetMType
//
// Get movement types: MoveWaitReturn etc
//
void DDF_SectGetMType(const char *info, void *storage)
{
    int *dest = (int *)storage;
    int  flag_value;

    switch (DDF_MainCheckSpecialFlag(info, movement_types, &flag_value, false,
                                     false))
    {
        case kDDFCheckFlagPositive:
        case kDDFCheckFlagNegative:
            (*dest) = flag_value;
            break;

        case kDDFCheckFlagUser:
        case kDDFCheckFlagUnknown:
            DDF_WarnError("Unknown Movement type: %s\n", info);
            break;
    }
}

static DDFSpecialFlags reference_types[] = {
    {"ABSOLUTE", kTriggerHeightReferenceAbsolute, false},

    {"FLOOR", kTriggerHeightReferenceCurrent, false},
    {"CEILING", kTriggerHeightReferenceCurrent + kTriggerHeightReferenceCeiling,
     false},

    {"TRIGGERFLOOR", kTriggerHeightReferenceTriggeringLinedef, false},
    {"TRIGGERCEILING",
     kTriggerHeightReferenceTriggeringLinedef + kTriggerHeightReferenceCeiling,
     false},

    // Note that LOSURROUNDINGFLOOR has the kTriggerHeightReferenceInclude flag,
    // but the others do not.  It's there to maintain backwards compatibility.
    //
    {"LOSURROUNDINGCEILING",
     kTriggerHeightReferenceSurrounding + kTriggerHeightReferenceCeiling,
     false},
    {"HISURROUNDINGCEILING",
     kTriggerHeightReferenceSurrounding + kTriggerHeightReferenceCeiling +
         kTriggerHeightReferenceHighest,
     false},
    {"LOSURROUNDINGFLOOR",
     kTriggerHeightReferenceSurrounding + kTriggerHeightReferenceInclude,
     false},
    {"HISURROUNDINGFLOOR",
     kTriggerHeightReferenceSurrounding + kTriggerHeightReferenceHighest,
     false},

    // Note that kTriggerHeightReferenceHighest is used for the NextLowest
    // types, and vice versa, which may seem strange.  It's because the next
    // lowest sector is actually the highest of all adjacent sectors
    // that are lower than the current sector.
    //
    {"NEXTLOWESTFLOOR",
     kTriggerHeightReferenceSurrounding + kTriggerHeightReferenceNext +
         kTriggerHeightReferenceHighest,
     false},
    {"NEXTHIGHESTFLOOR",
     kTriggerHeightReferenceSurrounding + kTriggerHeightReferenceNext, false},
    {"NEXTLOWESTCEILING",
     kTriggerHeightReferenceSurrounding + kTriggerHeightReferenceNext +
         kTriggerHeightReferenceCeiling + kTriggerHeightReferenceHighest,
     false},
    {"NEXTHIGHESTCEILING",
     kTriggerHeightReferenceSurrounding + kTriggerHeightReferenceNext +
         kTriggerHeightReferenceCeiling,
     false},

    {"LOWESTBOTTOMTEXTURE", kTriggerHeightReferenceLowestLowTexture, false}};

//
// DDF_SectGetDestRef
//
// Get surroundingsectorceiling/floorheight etc
//
void DDF_SectGetDestRef(const char *info, void *storage)
{
    int *dest = (int *)storage;
    int  flag_value;

    // check for modifier flags
    if (DDF_CompareName(info, "INCLUDE") == 0)
    {
        *dest |= kTriggerHeightReferenceInclude;
        return;
    }
    else if (DDF_CompareName(info, "EXCLUDE") == 0)
    {
        *dest &= ~kTriggerHeightReferenceInclude;
        return;
    }

    switch (DDF_MainCheckSpecialFlag(info, reference_types, &flag_value, false,
                                     false))
    {
        case kDDFCheckFlagPositive:
        case kDDFCheckFlagNegative:
            (*dest) = flag_value;
            break;

        case kDDFCheckFlagUser:
        case kDDFCheckFlagUnknown:
            DDF_WarnError("Unknown Reference Point: %s\n", info);
            break;
    }
}

static void DDF_SectMakeCrush(const char *info)
{
    dynamic_sector->f_.crush_damage_ = 10;
    dynamic_sector->c_.crush_damage_ = 10;
}

//----------------------------------------------------------------------------

// --> Sector type definition class

//
// SectorType Constructor
//
SectorType::SectorType() : number_(0) { Default(); }

//
// SectorType Destructor
//
SectorType::~SectorType() {}

//
// SectorType::CopyDetail()
//
void SectorType::CopyDetail(SectorType &src)
{
    secret_ = src.secret_;
    hub_    = src.hub_;

    gravity_   = src.gravity_;
    friction_  = src.friction_;
    viscosity_ = src.viscosity_;
    drag_      = src.drag_;

    f_ = src.f_;
    c_ = src.c_;
    l_ = src.l_;

    damage_ = src.damage_;

    special_flags_ = src.special_flags_;
    e_exit_        = src.e_exit_;

    use_colourmap_ = src.use_colourmap_;

    ambient_sfx_ = src.ambient_sfx_;
    splash_sfx_  = src.splash_sfx_;

    appear_ = src.appear_;

    push_speed_  = src.push_speed_;
    push_zspeed_ = src.push_zspeed_;
    push_angle_  = src.push_angle_;

    reverb_type_  = src.reverb_type_;
    reverb_ratio_ = src.reverb_ratio_;
    reverb_delay_ = src.reverb_delay_;

    floor_bob_   = src.floor_bob_;
    ceiling_bob_ = src.ceiling_bob_;

    fog_cmap_    = src.fog_cmap_;
    fog_color_   = src.fog_color_;
    fog_density_ = src.fog_density_;
}

void SectorType::Default()
{
    secret_ = false;
    hub_    = false;

    gravity_   = kGravityDefault;
    friction_  = kFrictionDefault;
    viscosity_ = kViscosityDefault;
    drag_      = kDragDefault;

    f_.Default(PlaneMoverDefinition::kPlaneMoverDefaultFloorSect);
    c_.Default(PlaneMoverDefinition::kPlaneMoverDefaultCeilingSect);

    l_.Default();

    damage_.Default(DamageClass::kDamageClassDefaultSector);

    special_flags_ = kSectorFlagNone;
    e_exit_        = kExitTypeNone;
    use_colourmap_ = nullptr;
    ambient_sfx_   = nullptr;
    splash_sfx_    = nullptr;

    appear_ = kAppearsWhenDefault;

    push_speed_  = 0.0f;
    push_zspeed_ = 0.0f;

    push_angle_ = 0;

    reverb_type_  = "NONE";
    reverb_delay_ = 0;
    reverb_ratio_ = 0;

    floor_bob_   = 0.0f;
    ceiling_bob_ = 0.0f;

    fog_cmap_    = nullptr;
    fog_color_   = kRGBANoValue;
    fog_density_ = 0;
}

SectorTypeContainer::SectorTypeContainer() { Reset(); }

SectorTypeContainer::~SectorTypeContainer()
{
    for (auto iter = begin(); iter != end(); iter++)
    {
        SectorType *sec = *iter;
        delete sec;
        sec = nullptr;
    }
}

//
// Looks an linetype by id, returns nullptr if line can't be found.
//
SectorType *SectorTypeContainer::Lookup(const int id)
{
    if (id == 0) return default_sector;

    int slot = DDF_SectHashFunc(id);

    // check the cache
    if (lookup_cache_[slot] && lookup_cache_[slot]->number_ == id)
    {
        return lookup_cache_[slot];
    }

    for (auto iter = rbegin(); iter != rend(); iter++)
    {
        SectorType *s = *iter;

        if (s->number_ == id)
        {
            // update the cache
            lookup_cache_[slot] = s;
            return s;
        }
    }

    return nullptr;
}

//
// SectorTypeContainer::Reset()
//
// Clears down both the data and the cache
//
void SectorTypeContainer::Reset()
{
    for (auto iter = begin(); iter != end(); iter++)
    {
        SectorType *sec = *iter;
        delete sec;
        sec = nullptr;
    }
    clear();
    memset(lookup_cache_, 0, sizeof(SectorType *) * kLookupCacheSize);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
