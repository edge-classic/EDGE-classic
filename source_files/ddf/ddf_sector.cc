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

#include "ddf_colormap.h"
#include "ddf_line.h"
#include "ddf_local.h"

static SectorType *dynamic_sector;

SectorTypeContainer sectortypes; // <-- User-defined

static SectorType *default_sector;

void        DDFSectGetSpecialFlags(const char *info, void *storage);
static void DDFSectMakeCrush(const char *info);

static SectorType dummy_sector;

static const DDFCommandList sect_commands[] = {
    // sub-commands
    DDF_SUB_LIST("FLOOR", dummy_sector, f_, floor_commands),
    DDF_SUB_LIST("CEILING", dummy_sector, c_, floor_commands),
    DDF_SUB_LIST("DAMAGE", dummy_sector, damage_, damage_commands),

    DDF_FIELD("SECRET", dummy_sector, secret_, DDFMainGetBoolean),
    DDF_FIELD("HUB", dummy_sector, hub_, DDFMainGetBoolean),
    DDF_FIELD("SPECIAL", dummy_sector, special_flags_, DDFSectGetSpecialFlags),

    DDF_FIELD("LIGHT_TYPE", dummy_sector, l_.type_, DDFSectGetLighttype),
    DDF_FIELD("LIGHT_LEVEL", dummy_sector, l_.level_, DDFMainGetNumeric),
    DDF_FIELD("LIGHT_DARKTIME", dummy_sector, l_.darktime_, DDFMainGetTime),
    DDF_FIELD("LIGHT_BRIGHTTIME", dummy_sector, l_.brighttime_, DDFMainGetTime),
    DDF_FIELD("LIGHT_CHANCE", dummy_sector, l_.chance_, DDFMainGetPercent),
    DDF_FIELD("LIGHT_SYNC", dummy_sector, l_.sync_, DDFMainGetTime),
    DDF_FIELD("LIGHT_STEP", dummy_sector, l_.step_, DDFMainGetNumeric),
    DDF_FIELD("EXIT", dummy_sector, e_exit_, DDFSectGetExit),
    DDF_FIELD("USE_COLOURMAP", dummy_sector, use_colourmap_, DDFMainGetColourmap),
    DDF_FIELD("GRAVITY", dummy_sector, gravity_, DDFMainGetFloat),
    DDF_FIELD("FRICTION", dummy_sector, friction_, DDFMainGetFloat),
    DDF_FIELD("VISCOSITY", dummy_sector, viscosity_, DDFMainGetFloat),
    DDF_FIELD("DRAG", dummy_sector, drag_, DDFMainGetFloat),
    DDF_FIELD("AMBIENT_SOUND", dummy_sector, ambient_sfx_, DDFMainLookupSound),
    DDF_FIELD("SPLASH_SOUND", dummy_sector, splash_sfx_, DDFMainLookupSound),
    DDF_FIELD("WHEN_APPEAR", dummy_sector, appear_, DDFMainGetWhenAppear),
    DDF_FIELD("PUSH_ANGLE", dummy_sector, push_angle_, DDFMainGetAngle),
    DDF_FIELD("PUSH_SPEED", dummy_sector, push_speed_, DDFMainGetFloat),
    DDF_FIELD("PUSH_ZSPEED", dummy_sector, push_zspeed_, DDFMainGetFloat),

    // -AJA- backwards compatibility cruft...
    DDF_FIELD("DAMAGE", dummy_sector, damage_.nominal_, DDFMainGetFloat),
    DDF_FIELD("DAMAGETIME", dummy_sector, damage_.delay_, DDFMainGetTime),

    DDF_FIELD("FLOOR_BOB", dummy_sector, floor_bob_, DDFMainGetFloat),
    DDF_FIELD("CEILING_BOB", dummy_sector, ceiling_bob_, DDFMainGetFloat),

    DDF_FIELD("FOG_COLOR", dummy_sector, fog_cmap_, DDFMainGetColourmap),
    DDF_FIELD("FOG_DENSITY", dummy_sector, fog_density_, DDFMainGetPercent),

    DDF_FIELD("REVERB_PRESET", dummy_sector, reverb_preset_, ddf::ReverbDefinition::AssignReverb),

    {nullptr, nullptr, 0, nullptr}};

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
        DDFError("Bad sectortype number in sectors.ddf: %s\n", name);

    dynamic_sector = sectortypes.Lookup(number);

    if (extend)
    {
        if (!dynamic_sector)
            DDFError("Unknown sectortype to extend: %s\n", name);
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
        DDFError("Bad sectortype number for template: %s\n", contents);

    SectorType *other = sectortypes.Lookup(number);

    if (!other || other == dynamic_sector)
        DDFError("Unknown sector template: '%s'\n", contents);

    dynamic_sector->CopyDetail(*other);
}

//
// SectorParseField
//
static void SectorParseField(const char *field, const char *contents, int index, bool is_last)
{
#if (DDF_DEBUG)
    LogDebug("SECTOR_PARSE: %s = %s;\n", field, contents);
#endif
    EPI_UNUSED(index);
    EPI_UNUSED(is_last);
    if (DDFCompareName(field, "TEMPLATE") == 0)
    {
        SectorDoTemplate(contents);
        return;
    }

    // backwards compatibility...
    if (DDFCompareName(field, "CRUSH") == 0 || DDFCompareName(field, "CRUSH_DAMAGE") == 0)
    {
        DDFSectMakeCrush(contents);
        return;
    }

    if (DDFMainParseField(sect_commands, field, contents, (uint8_t *)dynamic_sector))
        return; // OK

    DDFWarnError("Unknown sectors.ddf command: %s\n", field);
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

void DDFReadSectors(const std::string &data)
{
    DDFReadInfo sects;

    sects.tag      = "SECTORS";
    sects.lumpname = "DDFSECT";

    sects.start_entry  = SectorStartEntry;
    sects.parse_field  = SectorParseField;
    sects.finish_entry = SectorFinishEntry;
    sects.clear_all    = SectorClearAll;

    DDFMainReadFile(&sects, data);
}

//
// DDFSectorInit
//
void DDFSectorInit(void)
{
    sectortypes.Reset();

    default_sector          = new SectorType;
    default_sector->number_ = 0;
}

//
// DDFSectorCleanUp
//
void DDFSectorCleanUp(void)
{
    sectortypes.shrink_to_fit();
}

//----------------------------------------------------------------------------

static DDFSpecialFlags sector_specials[] = {{"WHOLE_REGION", kSectorFlagWholeRegion, 0},
                                            {"PROPORTIONAL", kSectorFlagProportional, 0},
                                            {"PUSH_ALL", kSectorFlagPushAll, 0},
                                            {"PUSH_CONSTANT", kSectorFlagPushConstant, 0},
                                            {"AIRLESS", kSectorFlagAirLess, 0},
                                            {"SWIM", kSectorFlagSwimming, 0},
                                            {"SUBMERGED_SFX", kSectorFlagSubmergedSFX, 0},
                                            {"VACUUM_SFX", kSectorFlagVacuumSFX, 0},
                                            {nullptr, 0, 0}};

//
// DDFSectGetSpecialFlags
//
// Gets the sector specials.
//
void DDFSectGetSpecialFlags(const char *info, void *storage)
{
    SectorFlag *special = (SectorFlag *)storage;

    int flag_value;

    switch (DDFMainCheckSpecialFlag(info, sector_specials, &flag_value, true, false))
    {
    case kDDFCheckFlagPositive:
        *special = (SectorFlag)(*special | flag_value);

        break;

    case kDDFCheckFlagNegative:
        *special = (SectorFlag)(*special & ~flag_value);

        break;

    case kDDFCheckFlagUser:
    case kDDFCheckFlagUnknown:
        DDFWarnError("Unknown sector special: %s", info);
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
// DDFSectGetExit
//
// Get the exit type
//
void DDFSectGetExit(const char *info, void *storage)
{
    int *dest = (int *)storage;
    int  flag_value;

    switch (DDFMainCheckSpecialFlag(info, exit_types, &flag_value, false, false))
    {
    case kDDFCheckFlagPositive:
    case kDDFCheckFlagNegative:
        (*dest) = flag_value;
        break;

    case kDDFCheckFlagUser:
    case kDDFCheckFlagUnknown:
        DDFWarnError("Unknown Exit type: %s\n", info);
        break;
    }
}

static DDFSpecialFlags light_types[] = {
    {"NONE", kLightSpecialTypeNone, 0},           {"SET", kLightSpecialTypeSet, 0},
    {"FADE", kLightSpecialTypeFade, 0},           {"STROBE", kLightSpecialTypeStrobe, 0},
    {"FLASH", kLightSpecialTypeFlash, 0},         {"GLOW", kLightSpecialTypeGlow, 0},
    {"FLICKER", kLightSpecialTypeFireFlicker, 0}, {nullptr, 0, 0}};

//
// DDFSectGetLighttype
//
// Get the light type
//
void DDFSectGetLighttype(const char *info, void *storage)
{
    int *dest = (int *)storage;
    int  flag_value;

    switch (DDFMainCheckSpecialFlag(info, light_types, &flag_value, false, false))
    {
    case kDDFCheckFlagPositive:
    case kDDFCheckFlagNegative:
        (*dest) = flag_value;
        break;

    case kDDFCheckFlagUser:
    case kDDFCheckFlagUnknown:
        DDFWarnError("Unknown light type: %s\n", info);
        break;
    }
}

static DDFSpecialFlags movement_types[] = {{"MOVE", kPlaneMoverOnce, 0},
                                           {"MOVEWAITRETURN", kPlaneMoverMoveWaitReturn, 0},
                                           {"CONTINUOUS", kPlaneMoverContinuous, 0},
                                           {"PLAT", kPlaneMoverPlatform, 0},
                                           {"BUILDSTAIRS", kPlaneMoverStairs, 0},
                                           {"STOP", kPlaneMoverStop, 0},
                                           {"TOGGLE", kPlaneMoverToggle, 0},
                                           {"ELEVATOR", kPlaneMoverElevator, 0},
                                           {nullptr, 0, 0}};

//
// DDFSectGetMType
//
// Get movement types: MoveWaitReturn etc
//
void DDFSectGetMType(const char *info, void *storage)
{
    int *dest = (int *)storage;
    int  flag_value;

    switch (DDFMainCheckSpecialFlag(info, movement_types, &flag_value, false, false))
    {
    case kDDFCheckFlagPositive:
    case kDDFCheckFlagNegative:
        (*dest) = flag_value;
        break;

    case kDDFCheckFlagUser:
    case kDDFCheckFlagUnknown:
        DDFWarnError("Unknown Movement type: %s\n", info);
        break;
    }
}

static DDFSpecialFlags reference_types[] = {
    {"ABSOLUTE", kTriggerHeightReferenceAbsolute, false},

    {"FLOOR", kTriggerHeightReferenceCurrent, false},
    {"CEILING", kTriggerHeightReferenceCurrent + kTriggerHeightReferenceCeiling, false},

    {"TRIGGERFLOOR", kTriggerHeightReferenceTriggeringLinedef, false},
    {"TRIGGERCEILING", kTriggerHeightReferenceTriggeringLinedef + kTriggerHeightReferenceCeiling, false},

    // Note that LOSURROUNDINGFLOOR has the kTriggerHeightReferenceInclude flag,
    // but the others do not.  It's there to maintain backwards compatibility.
    //
    {"LOSURROUNDINGCEILING", kTriggerHeightReferenceSurrounding + kTriggerHeightReferenceCeiling, false},
    {"HISURROUNDINGCEILING",
     kTriggerHeightReferenceSurrounding + kTriggerHeightReferenceCeiling + kTriggerHeightReferenceHighest, false},
    {"LOSURROUNDINGFLOOR", kTriggerHeightReferenceSurrounding + kTriggerHeightReferenceInclude, false},
    {"HISURROUNDINGFLOOR", kTriggerHeightReferenceSurrounding + kTriggerHeightReferenceHighest, false},

    // Note that kTriggerHeightReferenceHighest is used for the NextLowest
    // types, and vice versa, which may seem strange.  It's because the next
    // lowest sector is actually the highest of all adjacent sectors
    // that are lower than the current sector.
    //
    {"NEXTLOWESTFLOOR",
     kTriggerHeightReferenceSurrounding + kTriggerHeightReferenceNext + kTriggerHeightReferenceHighest, false},
    {"NEXTHIGHESTFLOOR", kTriggerHeightReferenceSurrounding + kTriggerHeightReferenceNext, false},
    {"NEXTLOWESTCEILING",
     kTriggerHeightReferenceSurrounding + kTriggerHeightReferenceNext + kTriggerHeightReferenceCeiling +
         kTriggerHeightReferenceHighest,
     false},
    {"NEXTHIGHESTCEILING",
     kTriggerHeightReferenceSurrounding + kTriggerHeightReferenceNext + kTriggerHeightReferenceCeiling, false},

    {"LOWESTBOTTOMTEXTURE", kTriggerHeightReferenceLowestLowTexture, false}};

//
// DDFSectGetDestRef
//
// Get surroundingsectorceiling/floorheight etc
//
void DDFSectGetDestRef(const char *info, void *storage)
{
    int *dest = (int *)storage;
    int  flag_value;

    // check for modifier flags
    if (DDFCompareName(info, "INCLUDE") == 0)
    {
        *dest |= kTriggerHeightReferenceInclude;
        return;
    }
    else if (DDFCompareName(info, "EXCLUDE") == 0)
    {
        *dest &= ~kTriggerHeightReferenceInclude;
        return;
    }

    switch (DDFMainCheckSpecialFlag(info, reference_types, &flag_value, false, false))
    {
    case kDDFCheckFlagPositive:
    case kDDFCheckFlagNegative:
        (*dest) = flag_value;
        break;

    case kDDFCheckFlagUser:
    case kDDFCheckFlagUnknown:
        DDFWarnError("Unknown Reference Point: %s\n", info);
        break;
    }
}

static void DDFSectMakeCrush(const char *info)
{
    EPI_UNUSED(info);
    dynamic_sector->f_.crush_damage_ = 10;
    dynamic_sector->c_.crush_damage_ = 10;
}

//----------------------------------------------------------------------------

// --> Sector type definition class

//
// SectorType Constructor
//
SectorType::SectorType() : number_(0)
{
    Default();
}

//
// SectorType Destructor
//
SectorType::~SectorType()
{
}

//
// SectorType::CopyDetail()
//
void SectorType::CopyDetail(const SectorType &src)
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

    floor_bob_   = src.floor_bob_;
    ceiling_bob_ = src.ceiling_bob_;

    fog_cmap_    = src.fog_cmap_;
    fog_color_   = src.fog_color_;
    fog_density_ = src.fog_density_;

    reverb_preset_      = src.reverb_preset_;
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

    floor_bob_   = 0.0f;
    ceiling_bob_ = 0.0f;

    fog_cmap_    = nullptr;
    fog_color_   = kRGBANoValue;
    fog_density_ = 0;

    reverb_preset_      = nullptr;
}

SectorTypeContainer::SectorTypeContainer()
{
    Reset();
}

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
    if (id == 0)
        return default_sector;

    int slot = (((id) + kLookupCacheSize) % kLookupCacheSize);

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
    EPI_CLEAR_MEMORY(lookup_cache_, SectorType *, kLookupCacheSize);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
