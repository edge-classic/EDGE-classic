//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Attack Types)
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
// Attacks Setup and Parser Code
//
// 1998/10/29 -KM- Finalisation of sound code.  SmartProjectile.
//

#include <string.h>

#include "ddf_local.h"
#include "epi_bitset.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"
#include "stb_sprintf.h"

AttackDefinitionContainer atkdefs;

extern MapObjectDefinition *dynamic_mobj;

// these logically belongs with buffer_atk:
static float a_damage_range;
static float a_damage_multi;

static void DDFAtkGetType(const char *info, void *storage);
static void DDFAtkGetSpecial(const char *info, void *storage);
static void DDFAtkGetLabel(const char *info, void *storage);

DamageClass dummy_damage;

const DDFCommandList damage_commands[] = {
    DDF_FIELD("VAL", dummy_damage, nominal_, DDFMainGetFloat),
    DDF_FIELD("MAX", dummy_damage, linear_max_, DDFMainGetFloat),
    DDF_FIELD("ERROR", dummy_damage, error_, DDFMainGetFloat),
    DDF_FIELD("DELAY", dummy_damage, delay_, DDFMainGetTime),

    DDF_FIELD("BYPASS_ALL", dummy_damage, bypass_all_, DDFMainGetBoolean),
    DDF_FIELD("INSTAKILL", dummy_damage, instakill_, DDFMainGetBoolean),
    DDF_FIELD("DAMAGE_UNLESS_BENEFIT", dummy_damage, damage_unless_, DDFMobjGetBenefit),
    DDF_FIELD("DAMAGE_IF_BENEFIT", dummy_damage, damage_if_, DDFMobjGetBenefit),
    DDF_FIELD("ALL_PLAYERS", dummy_damage, all_players_,
              DDFMainGetBoolean), // Doesn't do anything (yet)
    DDF_FIELD("ONLY_AFFECTS", dummy_damage, only_affects_, DDFMainGetBitSet),
    DDF_FIELD("FLASH_COLOUR", dummy_damage, damage_flash_colour_, DDFMainGetRGB),

    DDF_FIELD("OBITUARY", dummy_damage, obituary_, DDFMainGetString),
    DDF_FIELD("PAIN_STATE", dummy_damage, pain_, DDFAtkGetLabel),
    DDF_FIELD("DEATH_STATE", dummy_damage, death_, DDFAtkGetLabel),
    DDF_FIELD("OVERKILL_STATE", dummy_damage, overkill_, DDFAtkGetLabel),

    {nullptr, nullptr, 0, nullptr}};

// -KM- 1998/09/27 Major changes to sound handling
// -KM- 1998/11/25 Accuracy + Translucency are now fraction.  Added a spare
// attack for BFG. -KM- 1999/01/29 Merged in thing commands, so there is one
// list of
//  thing commands for all types of things (scenery, items, creatures +
//  projectiles)

static AttackDefinition *dynamic_atk;

static AttackDefinition dummy_atk;

static const DDFCommandList attack_commands[] = {
    // sub-commands
    DDF_SUB_LIST("DAMAGE", dummy_atk, damage_, damage_commands),

    DDF_FIELD("ATTACKTYPE", dummy_atk, attackstyle_, DDFAtkGetType),
    DDF_FIELD("ATTACK_SPECIAL", dummy_atk, flags_, DDFAtkGetSpecial),
    DDF_FIELD("ACCURACY_SLOPE", dummy_atk, accuracy_slope_, DDFMainGetSlope),
    DDF_FIELD("ACCURACY_ANGLE", dummy_atk, accuracy_angle_, DDFMainGetAngle),
    DDF_FIELD("ATTACK_HEIGHT", dummy_atk, height_, DDFMainGetFloat),
    DDF_FIELD("SHOTCOUNT", dummy_atk, count_, DDFMainGetNumeric),
    DDF_FIELD("X_OFFSET", dummy_atk, xoffset_, DDFMainGetFloat),
    DDF_FIELD("Y_OFFSET", dummy_atk, yoffset_, DDFMainGetFloat),
    DDF_FIELD("Z_OFFSET", dummy_atk, zoffset_, DDFMainGetFloat),
    DDF_FIELD("ANGLE_OFFSET", dummy_atk, angle_offset_, DDFMainGetAngle),
    DDF_FIELD("SLOPE_OFFSET", dummy_atk, slope_offset_, DDFMainGetSlope),
    DDF_FIELD("ATTACKRANGE", dummy_atk, range_, DDFMainGetFloat),
    DDF_FIELD("TOO_CLOSE_RANGE", dummy_atk, tooclose_, DDFMainGetNumeric),
    DDF_FIELD("BERSERK_MULTIPLY", dummy_atk, berserk_mul_, DDFMainGetFloat),
    DDF_FIELD("NO_TRACE_CHANCE", dummy_atk, notracechance_, DDFMainGetPercent),
    DDF_FIELD("KEEP_FIRING_CHANCE", dummy_atk, keepfirechance_, DDFMainGetPercent),
    DDF_FIELD("TRACE_ANGLE", dummy_atk, trace_angle_, DDFMainGetAngle),
    DDF_FIELD("ASSAULT_SPEED", dummy_atk, assault_speed_, DDFMainGetFloat),
    DDF_FIELD("ATTEMPT_SOUND", dummy_atk, initsound_, DDFMainLookupSound),
    DDF_FIELD("ENGAGED_SOUND", dummy_atk, sound_, DDFMainLookupSound),
    DDF_FIELD("SPAWNED_OBJECT", dummy_atk, spawnedobj_ref_, DDFMainGetString),
    DDF_FIELD("SPAWN_OBJECT_STATE", dummy_atk, objinitstate_ref_, DDFMainGetString),
    DDF_FIELD("SPAWN_LIMIT", dummy_atk, spawn_limit_, DDFMainGetNumeric),
    DDF_FIELD("PUFF", dummy_atk, puff_ref_, DDFMainGetString),
    DDF_FIELD("BLOOD", dummy_atk, blood_ref_, DDFMainGetString),
    DDF_FIELD("ATTACK_CLASS", dummy_atk, attack_class_, DDFMainGetBitSet),
    DDF_FIELD("DUALATTACK1", dummy_atk, dualattack1_, DDFMainRefAttack),
    DDF_FIELD("DUALATTACK2", dummy_atk, dualattack2_, DDFMainRefAttack),

    // -AJA- backward compatibility cruft...
    DDF_FIELD("DAMAGE", dummy_atk, damage_.nominal_, DDFMainGetFloat),

    {nullptr, nullptr, 0, nullptr}};

static MapObjectDefinition *CreateAtkMobj(const char *atk_name)
{
    MapObjectDefinition *mobj = new MapObjectDefinition();

    // determine a name
    char mobj_name[256];

    stbsp_snprintf(mobj_name, sizeof(mobj_name) - 2, "atk:%s", atk_name);
    mobj_name[255] = 0;

    mobj->name_   = mobj_name; // copies it
    mobj->number_ = -7777;

    mobjtypes.dynamic_atk_mobjtypes.push_back(mobj);

    return mobj;
}

//
//  DDF PARSE ROUTINES
//

static void AttackStartEntry(const char *name, bool extend)
{
    if (!name || !name[0])
    {
        DDFWarnError("New attack entry is missing a name!");
        name = "ATTACK_WITH_NO_NAME";
    }

    a_damage_range = -1;
    a_damage_multi = -1;

    // mobj counterpart will be created only if needed
    dynamic_mobj = nullptr;

    dynamic_atk = atkdefs.Lookup(name);

    if (extend)
    {
        if (!dynamic_atk)
            DDFError("Unknown attack to extend: %s\n", name);

        // Intentional Const Override
        dynamic_mobj = (MapObjectDefinition *)dynamic_atk->atk_mobj_;

        if (dynamic_mobj)
            DDFStateBeginRange(dynamic_mobj->state_grp_);

        return;
    }

    // replaces an existing entry?
    if (dynamic_atk)
    {
        dynamic_atk->Default();
        return;
    }

    // not found, create a new one
    dynamic_atk        = new AttackDefinition;
    dynamic_atk->name_ = name;

    atkdefs.push_back(dynamic_atk);
}

static void AttackDoTemplate(const char *contents)
{
    AttackDefinition *other = atkdefs.Lookup(contents);

    if (!other || other == dynamic_atk)
        DDFError("Unknown attack template: '%s'\n", contents);

    dynamic_atk->CopyDetail(*other);
    dynamic_atk->atk_mobj_ = nullptr;

    dynamic_mobj = nullptr;

    if (other->atk_mobj_)
    {
        dynamic_mobj = CreateAtkMobj(dynamic_atk->name_.c_str());

        dynamic_mobj->CopyDetail(*(MapObjectDefinition *)other->atk_mobj_);

        dynamic_atk->atk_mobj_ = dynamic_mobj;

        DDFStateBeginRange(dynamic_mobj->state_grp_);
    }
}

static void AttackParseField(const char *field, const char *contents, int index, bool is_last)
{
#if (DDF_DEBUG)
    LogDebug("ATTACK_PARSE: %s = %s;\n", field, contents);
#endif

    if (DDFCompareName(field, "TEMPLATE") == 0)
    {
        AttackDoTemplate(contents);
        return;
    }

    // backward compatibility...
    if (DDFCompareName(field, "DAMAGE_RANGE") == 0)
    {
        a_damage_range = atof(contents);
        return;
    }
    else if (DDFCompareName(field, "DAMAGE_MULTI") == 0)
    {
        a_damage_multi = atof(contents);
        return;
    }

    // first, check attack commands
    if (DDFMainParseField(attack_commands, field, contents, (uint8_t *)dynamic_atk))
        return;

    // we need to create an MOBJ for this attack
    if (!dynamic_mobj)
    {
        dynamic_mobj = CreateAtkMobj(dynamic_atk->name_.c_str());

        dynamic_atk->atk_mobj_ = dynamic_mobj;

        DDFStateBeginRange(dynamic_mobj->state_grp_);
    }

    ThingParseField(field, contents, index, is_last);
}

static void AttackFinishEntry(void)
{
    // handle attacks that have mobjs
    if (dynamic_mobj)
    {
        DDFStateFinishRange(dynamic_mobj->state_grp_);

        // check MOBJ stuff

        if (dynamic_mobj->explode_damage_.nominal_ < 0)
        {
            DDFWarnError("Bad EXPLODE_DAMAGE.VAL value %f in DDF.\n", dynamic_mobj->explode_damage_.nominal_);
        }

        if (dynamic_mobj->explode_radius_ < 0)
        {
            DDFWarnError("Bad EXPLODE_RADIUS value %f in DDF.\n", dynamic_mobj->explode_radius_);
        }

        if (dynamic_mobj->model_skin_ < 0 || dynamic_mobj->model_skin_ > 9)
            DDFError("Bad MODEL_SKIN value %d in DDF (must be 0-9).\n", dynamic_mobj->model_skin_);

        if (dynamic_mobj->dlight_.radius_ > 512)
            DDFWarning("DLIGHT RADIUS value %1.1f too large (over 512).\n", dynamic_mobj->dlight_.radius_);

        dynamic_mobj->proj_damage_ = dynamic_atk->damage_;
    }

    // check DAMAGE stuff
    if (dynamic_atk->damage_.nominal_ < 0)
    {
        DDFWarnError("Bad DAMAGE.VAL value %f in DDF.\n", dynamic_atk->damage_.nominal_);
    }

    // check kAttackStyleDualAttack to make sure both attacks are defined
    if (dynamic_atk->attackstyle_ == kAttackStyleDualAttack)
    {
        if (!dynamic_atk->dualattack1_ || !dynamic_atk->dualattack2_)
        {
            DDFError("DUALATTACK %s missing one or both dual attack definitions!\n", dynamic_atk->name_.c_str());
        }
        if (dynamic_atk->dualattack1_->name_ == dynamic_atk->name_ ||
            dynamic_atk->dualattack2_->name_ == dynamic_atk->name_)
        {
            DDFError("DUALATTACK %s is referencing itself!\n", dynamic_atk->name_.c_str());
        }
    }
    // Create a minimal mobj for psychic attacks for their tracker
    else if (dynamic_atk->attackstyle_ == kAttackStylePsychic && !dynamic_mobj)
    {
        dynamic_mobj           = CreateAtkMobj(dynamic_atk->name_.c_str());
        dynamic_mobj->radius_  = 1;
        dynamic_atk->atk_mobj_ = dynamic_mobj;
    }

    // compute an attack class, if none specified
    if (dynamic_atk->attack_class_ == 0)
    {
        dynamic_atk->attack_class_ = dynamic_mobj ? epi::BitSetFromChar('M')
                                     : (dynamic_atk->attackstyle_ == kAttackStyleCloseCombat ||
                                        dynamic_atk->attackstyle_ == kAttackStyleSkullFly)
                                         ? epi::BitSetFromChar('C')
                                         : epi::BitSetFromChar('B');
    }

    // -AJA- 2001/01/27: Backwards compatibility
    if (a_damage_range > 0)
    {
        dynamic_atk->damage_.nominal_ = a_damage_range;

        if (a_damage_multi > 0)
            dynamic_atk->damage_.linear_max_ = a_damage_range * a_damage_multi;
    }

    // -AJA- 2005/08/06: Berserk backwards compatibility
    if (DDFCompareName(dynamic_atk->name_.c_str(), "PLAYER_PUNCH") == 0 && dynamic_atk->berserk_mul_ == 1.0f)
    {
        dynamic_atk->berserk_mul_ = 10.0f;
    }

    // TODO: check more stuff...
}

static void AttackClearAll(void)
{
    LogWarning("Ignoring #CLEARALL in attacks.ddf\n");
}

void DDFReadAtks(const std::string &data)
{
    DDFReadInfo attacks;

    attacks.tag      = "ATTACKS";
    attacks.lumpname = "DDFATK";

    attacks.start_entry  = AttackStartEntry;
    attacks.parse_field  = AttackParseField;
    attacks.finish_entry = AttackFinishEntry;
    attacks.clear_all    = AttackClearAll;

    DDFMainReadFile(&attacks, data);
}

void DDFAttackInit(void)
{
    for (AttackDefinition *atk : atkdefs)
    {
        delete atk;
        atk = nullptr;
    }
    atkdefs.clear();
}

void DDFAttackCleanUp(void)
{
    for (AttackDefinition *a : atkdefs)
    {
        cur_ddf_entryname = epi::StringFormat("[%s]  (attacks.ddf)", a->name_.c_str());

        // lookup thing references

        // This should only happen via MBF21, as the
        // atk_mobj_ref shouldn't be populated otherwise
        if (!a->atk_mobj_ref_.empty())
        {
            // This happens if an attack references a mobj that doesn't have a standalone definition in DDFTHING,
            // but is created ad-hoc via DDFATK, like PLAYER_PLASMA
            if (epi::StringPrefixCaseCompareASCII(a->atk_mobj_ref_, "deh_atk_") == 0)
            {
                std::string real_name = a->atk_mobj_ref_.substr(8);
                for (std::vector<MapObjectDefinition *>::reverse_iterator
                         iter     = mobjtypes.dynamic_atk_mobjtypes.rbegin(),
                         iter_end = mobjtypes.dynamic_atk_mobjtypes.rend();
                     iter != iter_end; ++iter)
                {
                    MapObjectDefinition *mobj = *iter;
                    if (epi::StringCaseCompareASCII(real_name, mobj->name_.substr(4)) == 0)
                    {
                        a->atk_mobj_ = mobj;
                        break;
                    }
                }
            }
            else
                a->atk_mobj_ = mobjtypes.Lookup(a->atk_mobj_ref_.c_str());
            if (a->atk_mobj_)
            {
                a->damage_.nominal_          = a->atk_mobj_->proj_damage_.nominal_;
                a->damage_.linear_max_       = a->atk_mobj_->proj_damage_.linear_max_;
                MapObjectDefinition *atk_mod = (MapObjectDefinition *)a->atk_mobj_; // const override
                if (atk_mod->dlight_.type_ == kDynamicLightTypeNone)
                {
                    atk_mod->dlight_.type_              = kDynamicLightTypeModulate;
                    atk_mod->dlight_.radius_            = atk_mod->radius_ * 4;
                    atk_mod->dlight_.autocolour_sprite_ = states[atk_mod->idle_state_].sprite;
                }
            }
        }

        a->puff_ = a->puff_ref_.empty() ? nullptr : mobjtypes.Lookup(a->puff_ref_.c_str());

        a->blood_ = a->blood_ref_.empty() ? nullptr : mobjtypes.Lookup(a->blood_ref_.c_str());

        a->spawnedobj_ = a->spawnedobj_ref_.empty() ? nullptr : mobjtypes.Lookup(a->spawnedobj_ref_.c_str());

        if (a->spawnedobj_)
        {
            if (a->objinitstate_ref_.empty())
                a->objinitstate_ = a->spawnedobj_->spawn_state_;
            else
            {
                a->objinitstate_ = DDFMainLookupDirector(a->spawnedobj_, a->objinitstate_ref_.c_str());
                // Fallback to spawn state if objinitstate isn't valid (could be a DDFTHING entry modified
                // via mods or Dehacked)
                if (a->objinitstate_ == 0)
                    a->objinitstate_ = a->spawnedobj_->spawn_state_;
            }
        }

        cur_ddf_entryname.clear();
    }

    atkdefs.shrink_to_fit();
}

static const DDFSpecialFlags attack_specials[] = {{"SMOKING_TRACER", kAttackFlagSmokingTracer, 0},
                                                  {"KILL_FAILED_SPAWN", kAttackFlagKillFailedSpawn, 0},
                                                  {"REMOVE_FAILED_SPAWN", kAttackFlagKillFailedSpawn, 1},
                                                  {"PRESTEP_SPAWN", kAttackFlagPrestepSpawn, 0},
                                                  {"SPAWN_TELEFRAGS", kAttackFlagSpawnTelefrags, 0},
                                                  {"NEED_SIGHT", kAttackFlagNeedSight, 0},
                                                  {"FACE_TARGET", kAttackFlagFaceTarget, 0},

                                                  {"FORCE_AIM", kAttackFlagForceAim, 0},
                                                  {"ANGLED_SPAWN", kAttackFlagAngledSpawn, 0},
                                                  {"PLAYER_ATTACK", kAttackFlagPlayer, 0},
                                                  {"TRIGGER_LINES", kAttackFlagNoTriggerLines, 1},
                                                  {"SILENT_TO_MONSTERS", kAttackFlagSilentToMonsters, 0},
                                                  {"TARGET", kAttackFlagNoTarget, 1},
                                                  {"VAMPIRE", kAttackFlagVampire, 0},
                                                  {"OFFSETS_LAST", kAttackFlagOffsetsLast, 0},

                                                  // -AJA- backwards compatibility cruft...
                                                  {"NOAMMO", kAttackFlagNone, 0},

                                                  {nullptr, kAttackFlagNone, 0}};

static void DDFAtkGetSpecial(const char *info, void *storage)
{
    AttackFlags *var = (AttackFlags *)storage;

    int flag_value;

    switch (DDFMainCheckSpecialFlag(info, attack_specials, &flag_value, true, false))
    {
    case kDDFCheckFlagPositive:
        *var = (AttackFlags)(*var | flag_value);
        break;

    case kDDFCheckFlagNegative:
        *var = (AttackFlags)(*var & ~flag_value);
        break;

    case kDDFCheckFlagUser:
    case kDDFCheckFlagUnknown:
        DDFWarnError("DDFAtkGetSpecials: Unknown Attack Special: %s\n", info);
        break;
    }
}

// -KM- 1998/11/25 Added new attack type for BFG: Spray
static const char *attack_class[kTotalAttackStyles] = {
    "NONE",           "PROJECTILE",     "SPAWNER",
    "DOUBLE_SPAWNER", // Lobo 2021: doom64 pain elemental
    "TRIPLE_SPAWNER", "FIXED_SPREADER", "RANDOM_SPREADER", "SHOT",  "TRACKER",    "CLOSECOMBAT",
    "SHOOTTOSPOT",    "SKULLFLY",       "SMARTPROJECTILE", "SPRAY", "DUALATTACK", "PSYCHIC"};

static void DDFAtkGetType(const char *info, void *storage)
{
    AttackStyle *var = (AttackStyle *)storage;

    int i;

    for (i = 0; i < kTotalAttackStyles; i++)
        if (DDFCompareName(info, attack_class[i]) == 0)
            break;

    if (i >= kTotalAttackStyles)
    {
        DDFWarnError("DDFAtkGetType: No such attack type '%s'\n", info);
        *var = kAttackStyleShot;
        return;
    }

    *var = (AttackStyle)i;
}

//
// DDFAtkGetLabel
//
static void DDFAtkGetLabel(const char *info, void *storage)
{
    LabelOffset *lab = (LabelOffset *)storage;

    // check for `:' in string
    const char *div = strchr(info, ':');

    int i = div ? (div - info) : (int)strlen(info);

    if (i <= 0)
        DDFError("Bad State `%s'.\n", info);

    lab->label_  = std::string(info, i);
    lab->offset_ = div ? HMM_MAX(0, atoi(div + 1) - 1) : 0;
}

// Attack definition class

//
// AttackDefinition Constructor
//
AttackDefinition::AttackDefinition() : name_()
{
    Default();
}

//
// AttackDefinition Destructor
//
AttackDefinition::~AttackDefinition()
{
}

//
// AttackDefinition::CopyDetail()
//
void AttackDefinition::CopyDetail(const AttackDefinition &src)
{
    attackstyle_    = src.attackstyle_;
    flags_          = src.flags_;
    initsound_      = src.initsound_;
    sound_          = src.sound_;
    accuracy_slope_ = src.accuracy_slope_;
    accuracy_angle_ = src.accuracy_angle_;
    xoffset_        = src.xoffset_;
    yoffset_        = src.yoffset_;
    zoffset_        = src.zoffset_;
    angle_offset_   = src.angle_offset_;
    slope_offset_   = src.slope_offset_;
    trace_angle_    = src.trace_angle_;
    assault_speed_  = src.assault_speed_;
    height_         = src.height_;
    range_          = src.range_;
    count_          = src.count_;
    tooclose_       = src.tooclose_;
    berserk_mul_    = src.berserk_mul_;

    damage_ = src.damage_;

    attack_class_     = src.attack_class_;
    objinitstate_     = src.objinitstate_;
    objinitstate_ref_ = src.objinitstate_ref_;
    notracechance_    = src.notracechance_;
    keepfirechance_   = src.keepfirechance_;
    atk_mobj_         = src.atk_mobj_;
    atk_mobj_ref_     = src.atk_mobj_ref_;
    spawnedobj_       = src.spawnedobj_;
    spawnedobj_ref_   = src.spawnedobj_ref_;
    spawn_limit_      = src.spawn_limit_;
    puff_             = src.puff_;
    puff_ref_         = src.puff_ref_;
    blood_            = src.blood_;
    blood_ref_        = src.blood_ref_;
    dualattack1_      = src.dualattack1_;
    dualattack2_      = src.dualattack2_;
}

//
// AttackDefinition::Default()
//
void AttackDefinition::Default()
{
    attackstyle_    = kAttackStyleNone;
    flags_          = kAttackFlagNone;
    initsound_      = nullptr;
    sound_          = nullptr;
    accuracy_slope_ = 0;
    accuracy_angle_ = 0;
    xoffset_        = 0;
    yoffset_        = 0;
    zoffset_        = 0;
    angle_offset_   = 0;
    slope_offset_   = 0;
    trace_angle_    = (kBAMAngle270 / 16);
    assault_speed_  = 0;
    height_         = 0;
    range_          = 0;
    count_          = 0;
    tooclose_       = 0;
    berserk_mul_    = 1.0f;

    damage_.Default(DamageClass::kDamageClassDefaultAttack);

    attack_class_ = 0;
    objinitstate_ = 0;
    objinitstate_ref_.clear();
    notracechance_  = 0.0f;
    keepfirechance_ = 0.0f;
    atk_mobj_       = nullptr;
    atk_mobj_ref_.clear();
    spawnedobj_ = nullptr;
    spawnedobj_ref_.clear();
    spawn_limit_ = 0; // unlimited
    puff_        = nullptr;
    puff_ref_.clear();
    blood_ = nullptr;
    blood_ref_.clear();
    dualattack1_ = nullptr;
    dualattack2_ = nullptr;
}

// --> AttackDefinitionContainer class

//
// AttackDefinitionContainer::AttackDefinitionContainer()
//
AttackDefinitionContainer::AttackDefinitionContainer()
{
}

//
// ~AttackDefinitionContainer::AttackDefinitionContainer()
//
AttackDefinitionContainer::~AttackDefinitionContainer()
{
    for (std::vector<AttackDefinition *>::iterator iter = begin(), iter_end = end(); iter != iter_end; iter++)
    {
        AttackDefinition *atk = *iter;
        delete atk;
        atk = nullptr;
    }
}

//
// AttackDefinition* AttackDefinitionContainer::Lookup()
//
// Looks an atkdef by name, returns a fatal error if it does not exist.
//
AttackDefinition *AttackDefinitionContainer::Lookup(const char *refname)
{
    if (!refname || !refname[0])
        return nullptr;

    for (std::vector<AttackDefinition *>::iterator iter = begin(), iter_end = end(); iter != iter_end; iter++)
    {
        AttackDefinition *atk = *iter;
        if (DDFCompareName(atk->name_.c_str(), refname) == 0)
            return atk;
    }

    return nullptr;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
