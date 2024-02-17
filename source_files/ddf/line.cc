//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Linedefs)
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
// Line Definitions Setup and Parser Code
//
// -KM- 1998/09/01 Written.
// -ACB- 1998/09/06 Beautification: cleaned up so I can read it :).
// -KM- 1998/10/29 New types of linedefs added: colourmap, sound, friction,
// gravity
//                  auto, singlesided, music, lumpcheck
//                  Removed sector movement to ddf_main.c, so can be accesed by
//                  ddf_sect.c
//
// -ACB- 2001/02/04 DDF_GetSecHeightReference moved to p_plane.c
//

#include "line.h"

#include <limits.h>
#include <string.h>

#include "AlmostEquals.h"
#include "local.h"
#include "str_util.h"

#undef DF
#define DF DDF_FIELD

// -KM- 1999/01/29 Improved scrolling.
// Scrolling
enum ScrollDirections
{
    kScrollDirectionNone       = 0,
    kScrollDirectionVertical   = 1,
    kScrollDirectionUp         = 2,
    kScrollDirectionHorizontal = 4,
    kScrollDirectionLeft       = 8
};

LineTypeContainer linetypes;  // <-- User-defined

static LineType *default_linetype;

static void DDF_LineGetTrigType(const char *info, void *storage);
static void DDF_LineGetActivators(const char *info, void *storage);
static void DDF_LineGetSecurity(const char *info, void *storage);
static void DDF_LineGetScroller(const char *info, void *storage);
static void DDF_LineGetScrollPart(const char *info, void *storage);
static void DDF_LineGetExtraFloor(const char *info, void *storage);
static void DDF_LineGetEFControl(const char *info, void *storage);
static void DDF_LineGetTeleportSpecial(const char *info, void *storage);
static void DDF_LineGetRadTrig(const char *info, void *storage);
static void DDF_LineGetSpecialFlags(const char *info, void *storage);
static void DDF_LineGetSlideType(const char *info, void *storage);
static void DDF_LineGetLineEffect(const char *info, void *storage);
static void DDF_LineGetScrollType(const char *info, void *storage);
static void DDF_LineGetSectorEffect(const char *info, void *storage);
static void DDF_LineGetPortalEffect(const char *info, void *storage);
static void DDF_LineGetSlopeType(const char *info, void *storage);

static void DDF_LineMakeCrush(const char *info);

#undef DDF_CMD_BASE
#define DDF_CMD_BASE dummy_floor
static PlaneMoverDefinition dummy_floor;

const DDFCommandList floor_commands[] = {
    DF("TYPE", type_, DDF_SectGetMType),
    DF("SPEED_UP", speed_up_, DDF_MainGetFloat),
    DF("SPEED_DOWN", speed_down_, DDF_MainGetFloat),
    DF("DEST_REF", destref_, DDF_SectGetDestRef),
    DF("DEST_OFFSET", dest_, DDF_MainGetFloat),
    DF("OTHER_REF", otherref_, DDF_SectGetDestRef),
    DF("OTHER_OFFSET", other_, DDF_MainGetFloat),
    DF("CRUSH_DAMAGE", crush_damage_, DDF_MainGetNumeric),
    DF("TEXTURE", tex_, DDF_MainGetLumpName),
    DF("PAUSE_TIME", wait_, DDF_MainGetTime),
    DF("WAIT_TIME", prewait_, DDF_MainGetTime),
    DF("SFX_START", sfxstart_, DDF_MainLookupSound),
    DF("SFX_UP", sfxup_, DDF_MainLookupSound),
    DF("SFX_DOWN", sfxdown_, DDF_MainLookupSound),
    DF("SFX_STOP", sfxstop_, DDF_MainLookupSound),
    DF("SCROLL_ANGLE", scroll_angle_, DDF_MainGetAngle),
    DF("SCROLL_SPEED", scroll_speed_, DDF_MainGetFloat),
    DF("IGNORE_TEXTURE", ignore_texture_, DDF_MainGetBoolean),

    DDF_CMD_END};

#undef DDF_CMD_BASE
#define DDF_CMD_BASE dummy_ladder
static LadderDefinition dummy_ladder;

const DDFCommandList ladder_commands[] = {
    DF("HEIGHT", height_, DDF_MainGetFloat), DDF_CMD_END};

#undef DDF_CMD_BASE
#define DDF_CMD_BASE dummy_slider
static SlidingDoor dummy_slider;

const DDFCommandList slider_commands[] = {
    DF("TYPE", type_, DDF_LineGetSlideType),
    DF("SPEED", speed_, DDF_MainGetFloat),
    DF("PAUSE_TIME", wait_, DDF_MainGetTime),
    DF("SEE_THROUGH", see_through_, DDF_MainGetBoolean),
    DF("DISTANCE", distance_, DDF_MainGetPercent),
    DF("SFX_START", sfx_start_, DDF_MainLookupSound),
    DF("SFX_OPEN", sfx_open_, DDF_MainLookupSound),
    DF("SFX_CLOSE", sfx_close_, DDF_MainLookupSound),
    DF("SFX_STOP", sfx_stop_, DDF_MainLookupSound),

    DDF_CMD_END};

static LineType *dynamic_line;

// these bits logically belong with buffer_line:
static float            scrolling_speed;
static ScrollDirections scrolling_dir;

#undef DDF_CMD_BASE
#define DDF_CMD_BASE dummy_line
static LineType dummy_line;

static const DDFCommandList linedef_commands[] = {
    // sub-commands
    DDF_SUB_LIST("FLOOR", f_, floor_commands),
    DDF_SUB_LIST("CEILING", c_, floor_commands),
    DDF_SUB_LIST("SLIDER", s_, slider_commands),
    DDF_SUB_LIST("LADDER", ladder_, ladder_commands),

    DF("NEWTRIGGER", newtrignum_, DDF_MainGetNumeric),
    DF("ACTIVATORS", obj_, DDF_LineGetActivators),
    DF("TYPE", type_, DDF_LineGetTrigType),
    DF("KEYS", keys_, DDF_LineGetSecurity),
    DF("FAILED_MESSAGE", failedmessage_, DDF_MainGetString),
    DF("FAILED_SFX", failed_sfx_, DDF_MainLookupSound),
    DF("COUNT", count_, DDF_MainGetNumeric),

    DF("DONUT", d_.dodonut_, DDF_MainGetBoolean),
    DF("DONUT_IN_SFX", d_.d_sfxin_, DDF_MainLookupSound),
    DF("DONUT_IN_SFXSTOP", d_.d_sfxinstop_, DDF_MainLookupSound),
    DF("DONUT_OUT_SFX", d_.d_sfxout_, DDF_MainLookupSound),
    DF("DONUT_OUT_SFXSTOP", d_.d_sfxoutstop_, DDF_MainLookupSound),

    DF("TELEPORT", t_.teleport_, DDF_MainGetBoolean),
    DF("TELEPORT_DELAY", t_.delay_, DDF_MainGetTime),
    DF("TELEIN_EFFECTOBJ", t_.inspawnobj_ref_, DDF_MainGetString),
    DF("TELEOUT_EFFECTOBJ", t_.outspawnobj_ref_, DDF_MainGetString),
    DF("TELEPORT_SPECIAL", t_.special_, DDF_LineGetTeleportSpecial),

    DF("LIGHT_TYPE", l_.type_, DDF_SectGetLighttype),
    DF("LIGHT_LEVEL", l_.level_, DDF_MainGetNumeric),
    DF("LIGHT_DARK_TIME", l_.darktime_, DDF_MainGetTime),
    DF("LIGHT_BRIGHT_TIME", l_.brighttime_, DDF_MainGetTime),
    DF("LIGHT_CHANCE", l_.chance_, DDF_MainGetPercent),
    DF("LIGHT_SYNC", l_.sync_, DDF_MainGetTime),
    DF("LIGHT_STEP", l_.step_, DDF_MainGetNumeric),
    DF("EXIT", e_exit_, DDF_SectGetExit),
    DF("HUB_EXIT", hub_exit_, DDF_MainGetNumeric),

    DF("SCROLL_XSPEED", s_xspeed_, DDF_MainGetFloat),
    DF("SCROLL_YSPEED", s_yspeed_, DDF_MainGetFloat),
    DF("SCROLL_PARTS", scroll_parts_, DDF_LineGetScrollPart),
    DF("USE_COLOURMAP", use_colourmap_, DDF_MainGetColourmap),
    DF("GRAVITY", gravity_, DDF_MainGetFloat),
    DF("FRICTION", friction_, DDF_MainGetFloat),
    DF("VISCOSITY", viscosity_, DDF_MainGetFloat),
    DF("DRAG", drag_, DDF_MainGetFloat),
    DF("AMBIENT_SOUND", ambient_sfx_, DDF_MainLookupSound),
    DF("ACTIVATE_SOUND", activate_sfx_, DDF_MainLookupSound),
    DF("MUSIC", music_, DDF_MainGetNumeric),
    DF("AUTO", autoline_, DDF_MainGetBoolean),
    DF("SINGLESIDED", singlesided_, DDF_MainGetBoolean),
    DF("EXTRAFLOOR_TYPE", ef_.type_, DDF_LineGetExtraFloor),
    DF("EXTRAFLOOR_CONTROL", ef_.control_, DDF_LineGetEFControl),
    DF("TRANSLUCENCY", translucency_, DDF_MainGetPercent),
    DF("WHEN_APPEAR", appear_, DDF_MainGetWhenAppear),
    DF("SPECIAL", special_flags_, DDF_LineGetSpecialFlags),
    DF("RADIUS_TRIGGER", trigger_effect_, DDF_LineGetRadTrig),
    DF("LINE_EFFECT", line_effect_, DDF_LineGetLineEffect),
    DF("SCROLL_TYPE", scroll_type_, DDF_LineGetScrollType),
    DF("LINE_PARTS", line_parts_, DDF_LineGetScrollPart),
    DF("SECTOR_EFFECT", sector_effect_, DDF_LineGetSectorEffect),
    DF("PORTAL_TYPE", portal_effect_, DDF_LineGetPortalEffect),
    DF("SLOPE_TYPE", slope_type_, DDF_LineGetSlopeType),
    DF("COLOUR", fx_color_, DDF_MainGetRGB),

    // -AJA- backwards compatibility cruft...
    DF("EXTRAFLOOR_TRANSLUCENCY", translucency_, DDF_MainGetPercent),

    // Lobo: 2022
    DF("EFFECT_OBJECT", effectobject_ref_, DDF_MainGetString),
    DF("GLASS", glass_, DDF_MainGetBoolean),
    DF("BROKEN_TEXTURE", brokentex_, DDF_MainGetLumpName),

    DDF_CMD_END};

struct ScrollKludge
{
    const char      *s;
    ScrollDirections dir;
};

static ScrollKludge s_scroll[] = {
    {"NONE", kScrollDirectionNone},
    {"UP", (ScrollDirections)(kScrollDirectionVertical | kScrollDirectionUp)},
    {"DOWN", kScrollDirectionVertical},
    {"LEFT",
     (ScrollDirections)(kScrollDirectionHorizontal | kScrollDirectionLeft)},
    {"RIGHT", kScrollDirectionHorizontal},
    {nullptr, kScrollDirectionNone}};

static struct  // FIXME: APPLIES TO NEXT 3 TABLES !
{
    const char *s;
    int         n;
}

// FIXME: use keytype_names (in ddf_mobj.c)
s_keys[] = {{"NONE", kDoorKeyNone},

            {"BLUE_CARD", kDoorKeyBlueCard},
            {"YELLOW_CARD", kDoorKeyYellowCard},
            {"RED_CARD", kDoorKeyRedCard},
            {"BLUE_SKULL", kDoorKeyBlueSkull},
            {"YELLOW_SKULL", kDoorKeyYellowSkull},
            {"RED_SKULL", kDoorKeyRedSkull},
            {"GREEN_CARD", kDoorKeyGreenCard},
            {"GREEN_SKULL", kDoorKeyGreenSkull},

            {"GOLD_KEY", kDoorKeyGoldKey},
            {"SILVER_KEY", kDoorKeySilverKey},
            {"BRASS_KEY", kDoorKeyBrassKey},
            {"COPPER_KEY", kDoorKeyCopperKey},
            {"STEEL_KEY", kDoorKeySteelKey},
            {"WOODEN_KEY", kDoorKeyWoodenKey},
            {"FIRE_KEY", kDoorKeyFireKey},
            {"WATER_KEY", kDoorKeyWaterKey},

            // backwards compatibility
            {"REQUIRES_ALL", kDoorKeyStrictlyAllKeys | kDoorKeyBlueCard |
                                 kDoorKeyYellowCard | kDoorKeyRedCard |
                                 kDoorKeyBlueSkull | kDoorKeyYellowSkull |
                                 kDoorKeyRedSkull}},

    s_trigger[] = {{"WALK", kLineTriggerWalkable},
                   {"PUSH", kLineTriggerPushable},
                   {"SHOOT", kLineTriggerShootable},
                   {"MANUAL", kLineTriggerManual}},

    s_activators[] = {{"PLAYER", kTriggerActivatorPlayer},
                      {"MONSTER", kTriggerActivatorMonster},
                      {"OTHER", kTriggerActivatorOther},
                      {"NOBOT", kTriggerActivatorNoBot},

                      // obsolete stuff
                      {"MISSILE", 0}};

//
//  DDF PARSE ROUTINES
//

static void LinedefStartEntry(const char *name, bool extend)
{
    int number = HMM_MAX(0, atoi(name));

    if (number == 0) DDF_Error("Bad linetype number in lines.ddf: %s\n", name);

    scrolling_dir   = kScrollDirectionNone;
    scrolling_speed = 1.0f;

    dynamic_line = linetypes.Lookup(number);

    if (extend)
    {
        if (!dynamic_line) DDF_Error("Unknown linetype to extend: %s\n", name);
        return;
    }

    // replaces an existing entry?
    if (dynamic_line)
    {
        dynamic_line->Default();
        return;
    }

    // not found, create a new one
    dynamic_line          = new LineType;
    dynamic_line->number_ = number;

    linetypes.push_back(dynamic_line);
}

static void LinedefDoTemplate(const char *contents)
{
    int number = HMM_MAX(0, atoi(contents));
    if (number == 0)
        DDF_Error("Bad linetype number for template: %s\n", contents);

    LineType *other = linetypes.Lookup(number);

    if (!other || other == dynamic_line)
        DDF_Error("Unknown linetype template: '%s'\n", contents);

    dynamic_line->CopyDetail(*other);
}

static void LinedefParseField(const char *field, const char *contents,
                              int index, bool is_last)
{
#if (DEBUG_DDF)
    I_Debugf("LINEDEF_PARSE: %s = %s;\n", field, contents);
#endif

    if (DDF_CompareName(field, "TEMPLATE") == 0)
    {
        LinedefDoTemplate(contents);
        return;
    }

    // ignored for backwards compatibility
    if (DDF_CompareName(field, "SECSPECIAL") == 0) return;

    // -AJA- backwards compatibility cruft...
    if (DDF_CompareName(field, "CRUSH") == 0)
    {
        DDF_LineMakeCrush(contents);
        return;
    }
    else if (DDF_CompareName(field, "SCROLL") == 0)
    {
        DDF_LineGetScroller(contents, &scrolling_dir);
        return;
    }
    else if (DDF_CompareName(field, "SCROLLING_SPEED") == 0)
    {
        scrolling_speed = atof(contents);
        return;
    }

    if (DDF_MainParseField(linedef_commands, field, contents,
                           (uint8_t *)dynamic_line))
        return;  // OK

    DDF_WarnError("Unknown lines.ddf command: %s\n", field);
}

static void LinedefFinishEntry(void)
{
    // -KM- 1999/01/29 Convert old style scroller to new.
    if (scrolling_dir & kScrollDirectionVertical)
    {
        if (scrolling_dir & kScrollDirectionUp)
            dynamic_line->s_yspeed_ = scrolling_speed;
        else
            dynamic_line->s_yspeed_ = -scrolling_speed;
    }

    if (scrolling_dir & kScrollDirectionHorizontal)
    {
        if (scrolling_dir & kScrollDirectionLeft)
            dynamic_line->s_xspeed_ = scrolling_speed;
        else
            dynamic_line->s_xspeed_ = -scrolling_speed;
    }

    // backwards compat: COUNT=0 means no limit on triggering
    if (dynamic_line->count_ == 0) dynamic_line->count_ = -1;

    if (dynamic_line->hub_exit_ > 0) dynamic_line->e_exit_ = kExitTypeHub;

    // check stuff...

    if (dynamic_line->ef_.type_ != kExtraFloorTypeNone)
    {
        // AUTO is no longer needed for extrafloors
        dynamic_line->autoline_ = false;

        if ((dynamic_line->ef_.type_ & kExtraFloorTypeFlooder) &&
            (dynamic_line->ef_.type_ & kExtraFloorTypeNoShade))
        {
            DDF_WarnError(
                "FLOODER and NOSHADE tags cannot be used together.\n");
            dynamic_line->ef_.type_ = (ExtraFloorType)(dynamic_line->ef_.type_ &
                                                       ~kExtraFloorTypeFlooder);
        }

        if (!(dynamic_line->ef_.type_ & kExtraFloorTypePresent))
        {
            DDF_WarnError("Extrafloor type missing THIN, THICK or LIQUID.\n");
            dynamic_line->ef_.type_ = kExtraFloorTypeNone;
        }
    }

    if (!AlmostEquals(dynamic_line->friction_, kFloatUnused) &&
        dynamic_line->friction_ < 0.05f)
    {
        DDF_WarnError(
            "Friction value too low (%1.2f), it would prevent "
            "all movement.\n",
            dynamic_line->friction_);
        dynamic_line->friction_ = 0.05f;
    }

    if (!AlmostEquals(dynamic_line->viscosity_, kFloatUnused) &&
        dynamic_line->viscosity_ > 0.95f)
    {
        DDF_WarnError(
            "Viscosity value too high (%1.2f), it would prevent "
            "all movement.\n",
            dynamic_line->viscosity_);
        dynamic_line->viscosity_ = 0.95f;
    }

    // TODO: check more stuff...
}

static void LinedefClearAll(void)
{
    // 100% safe to delete all the linetypes
    linetypes.Reset();
}

void DDF_ReadLines(const std::string &data)
{
    DDFReadInfo lines;

    lines.tag      = "LINES";
    lines.lumpname = "DDFLINE";

    lines.start_entry  = LinedefStartEntry;
    lines.parse_field  = LinedefParseField;
    lines.finish_entry = LinedefFinishEntry;
    lines.clear_all    = LinedefClearAll;

    DDF_MainReadFile(&lines, data);
}

void DDF_LinedefInit(void)
{
    linetypes.Reset();

    default_linetype          = new LineType();
    default_linetype->number_ = 0;
}

void DDF_LinedefCleanUp(void)
{
    for (auto l : linetypes)
    {
        cur_ddf_entryname = epi::StringFormat("[%d]  (lines.ddf)", l->number_);

        l->t_.inspawnobj_ =
            l->t_.inspawnobj_ref_ != ""
                ? mobjtypes.Lookup(l->t_.inspawnobj_ref_.c_str())
                : nullptr;

        l->t_.outspawnobj_ =
            l->t_.outspawnobj_ref_ != ""
                ? mobjtypes.Lookup(l->t_.outspawnobj_ref_.c_str())
                : nullptr;

        // Lobo: 2021
        l->effectobject_ = l->effectobject_ref_ != ""
                               ? mobjtypes.Lookup(l->effectobject_ref_.c_str())
                               : nullptr;

        cur_ddf_entryname.clear();
    }

    linetypes.shrink_to_fit();
}

//
// DDF_LineGetScroller
//
// Check for scroll types
//
void DDF_LineGetScroller(const char *info, void *storage)
{
    for (int i = 0; s_scroll[i].s; i++)
    {
        if (DDF_CompareName(info, s_scroll[i].s) == 0)
        {
            scrolling_dir = (ScrollDirections)(scrolling_dir | s_scroll[i].dir);
            return;
        }
    }
    DDF_WarnError("Unknown scroll direction %s\n", info);
}

//
// DDF_LineGetSecurity
//
// Get Red/Blue/Yellow
//
void DDF_LineGetSecurity(const char *info, void *storage)
{
    DoorKeyType *var = (DoorKeyType *)storage;

    bool required = false;

    if (info[0] == '+')
    {
        required = true;
        info++;
    }
    else if (*var & kDoorKeyStrictlyAllKeys)
    {
        // -AJA- when there is at least one required key, then the
        // non-required keys don't have any effect.
        return;
    }

    for (int i = sizeof(s_keys) / sizeof(s_keys[0]); i--;)
    {
        if (DDF_CompareName(info, s_keys[i].s) == 0)
        {
            *var = (DoorKeyType)(*var | s_keys[i].n);

            if (required) *var = (DoorKeyType)(*var | kDoorKeyStrictlyAllKeys);

            return;
        }
    }

    DDF_WarnError("Unknown key type %s\n", info);
}

//
// DDF_LineGetTrigType
//
// Check for walk/push/shoot
//
void DDF_LineGetTrigType(const char *info, void *storage)
{
    LineTrigger *var = (LineTrigger *)storage;

    for (int i = sizeof(s_trigger) / sizeof(s_trigger[0]); i--;)
    {
        if (DDF_CompareName(info, s_trigger[i].s) == 0)
        {
            *var = (LineTrigger)s_trigger[i].n;
            return;
        }
    }

    DDF_WarnError("Unknown Trigger type %s\n", info);
}

//
// DDF_LineGetActivators
//
// Get player/monsters/missiles
//
void DDF_LineGetActivators(const char *info, void *storage)
{
    TriggerActivator *var = (TriggerActivator *)storage;

    for (int i = sizeof(s_activators) / sizeof(s_activators[0]); i--;)
    {
        if (DDF_CompareName(info, s_activators[i].s) == 0)
        {
            *var = (TriggerActivator)(*var | s_activators[i].n);
            return;
        }
    }

    DDF_WarnError("Unknown Activator type %s\n", info);
}

static DDFSpecialFlags extrafloor_types[] = {
    // definers:
    {"THIN", kExtraFloorThinDefaults, 0},
    {"THICK", kExtraFloorThickDefaults, 0},
    {"LIQUID", kExtraFloorLiquidDefaults, 0},

    // modifiers:
    {"SEE_THROUGH", kExtraFloorTypeSeeThrough, 0},
    {"WATER", kExtraFloorTypeWater, 0},
    {"SHADE", kExtraFloorTypeNoShade, 1},
    {"FLOODER", kExtraFloorTypeFlooder, 0},
    {"SIDE_UPPER", kExtraFloorTypeSideUpper, 0},
    {"SIDE_LOWER", kExtraFloorTypeSideLower, 0},
    {"SIDE_MIDY", kExtraFloorTypeSideMidY, 0},
    {"BOOMTEX", kExtraFloorTypeBoomTex, 0},

    // backwards compatibility...
    {"FALL_THROUGH", kExtraFloorTypeLiquid, 0},
    {"SHOOT_THROUGH", 0, 0},
    {nullptr, 0, 0}};

//
// DDF_LineGetExtraFloor
//
// Gets the extra floor type(s).
//
// -AJA- 1999/06/21: written.
// -AJA- 2000/03/27: updated for simpler system.
//
void DDF_LineGetExtraFloor(const char *info, void *storage)
{
    ExtraFloorType *var = (ExtraFloorType *)storage;

    if (DDF_CompareName(info, "NONE") == 0)
    {
        *var = kExtraFloorTypeNone;
        return;
    }

    int flag_value;

    switch (DDF_MainCheckSpecialFlag(info, extrafloor_types, &flag_value, true,
                                     false))
    {
        case kDDFCheckFlagPositive:
            *var = (ExtraFloorType)(*var | flag_value);
            break;

        case kDDFCheckFlagNegative:
            *var = (ExtraFloorType)(*var & ~flag_value);
            break;

        case kDDFCheckFlagUser:
        case kDDFCheckFlagUnknown:
            DDF_WarnError("Unknown Extrafloor Type: %s\n", info);
            break;
    }
}

static DDFSpecialFlags ef_control_types[] = {
    {"NONE", kExtraFloorControlNone, 0},
    {"REMOVE", kExtraFloorControlRemove, 0},
    {nullptr, 0, 0}};

//
// DDF_LineGetEFControl
//
void DDF_LineGetEFControl(const char *info, void *storage)
{
    ExtraFloorControl *var = (ExtraFloorControl *)storage;

    int flag_value;

    switch (DDF_MainCheckSpecialFlag(info, ef_control_types, &flag_value, false,
                                     false))
    {
        case kDDFCheckFlagPositive:
        case kDDFCheckFlagNegative:
            *var = (ExtraFloorControl)flag_value;
            break;

        case kDDFCheckFlagUser:
        case kDDFCheckFlagUnknown:
            DDF_WarnError("Unknown CONTROL_EXTRAFLOOR tag: %s", info);
            break;
    }
}

static constexpr int kTeleportSpecialAllSame =
    ((TeleportSpecial)(kTeleportSpecialRelative | kTeleportSpecialSameHeight |
                       kTeleportSpecialSameSpeed | kTeleportSpecialSameOffset));

static constexpr int kTeleportSpecialPreserve =
    ((TeleportSpecial)(kTeleportSpecialSameAbsDir | kTeleportSpecialSameHeight |
                       kTeleportSpecialSameSpeed));

static DDFSpecialFlags teleport_specials[] = {
    {"RELATIVE", kTeleportSpecialRelative, 0},
    {"SAME_HEIGHT", kTeleportSpecialSameHeight, 0},
    {"SAME_SPEED", kTeleportSpecialSameSpeed, 0},
    {"SAME_OFFSET", kTeleportSpecialSameOffset, 0},
    {"ALL_SAME", kTeleportSpecialAllSame, 0},

    {"LINE", kTeleportSpecialLine, 0},
    {"FLIPPED", kTeleportSpecialFlipped, 0},
    {"SILENT", kTeleportSpecialSilent, 0},

    // these modes are deprecated (kept for B.C.)
    {"SAME_DIR", kTeleportSpecialSameAbsDir, 0},
    {"ROTATE", kTeleportSpecialRotate, 0},
    {"PRESERVE", kTeleportSpecialPreserve, 0},

    {nullptr, 0, 0}};

//
// DDF_LineGetTeleportSpecial
//
// Gets the teleporter special flags.
//
// -AJA- 1999/07/12: written.
//
void DDF_LineGetTeleportSpecial(const char *info, void *storage)
{
    TeleportSpecial *var = (TeleportSpecial *)storage;

    int flag_value;

    switch (DDF_MainCheckSpecialFlag(info, teleport_specials, &flag_value, true,
                                     false))
    {
        case kDDFCheckFlagPositive:
            *var = (TeleportSpecial)(*var | flag_value);
            break;

        case kDDFCheckFlagNegative:
            *var = (TeleportSpecial)(*var & ~flag_value);
            break;

        case kDDFCheckFlagUser:
        case kDDFCheckFlagUnknown:
            DDF_WarnError("DDF_LineGetTeleportSpecial: Unknown Special: %s\n",
                          info);
            break;
    }
}

static DDFSpecialFlags scrollpart_specials[] = {
    {"RIGHT_UPPER", kScrollingPartRightUpper, 0},
    {"RIGHT_MIDDLE", kScrollingPartRightMiddle, 0},
    {"RIGHT_LOWER", kScrollingPartRightLower, 0},
    {"RIGHT", kScrollingPartRight, 0},
    {"LEFT_UPPER", kScrollingPartLeftUpper, 0},
    {"LEFT_MIDDLE", kScrollingPartLeftMiddle, 0},
    {"LEFT_LOWER", kScrollingPartLeftLower, 0},
    {"LEFT", kScrollingPartLeft, 0},
    {"LEFT_REVERSE_X", kScrollingPartLeftRevX, 0},
    {"LEFT_REVERSE_Y", kScrollingPartLeftRevY, 0},
    {nullptr, 0, 0}};

//
// DDF_LineGetScrollPart
//
// Gets the scroll part flags.
//
// -AJA- 1999/07/12: written.
//
void DDF_LineGetScrollPart(const char *info, void *storage)
{
    int            flag_value;
    ScrollingPart *dest = (ScrollingPart *)storage;

    if (DDF_CompareName(info, "NONE") == 0)
    {
        (*dest) = kScrollingPartNone;
        return;
    }

    switch (DDF_MainCheckSpecialFlag(info, scrollpart_specials, &flag_value,
                                     true, false))
    {
        case kDDFCheckFlagPositive:
            (*dest) = (ScrollingPart)((*dest) | flag_value);
            break;

        case kDDFCheckFlagNegative:
            (*dest) = (ScrollingPart)((*dest) & ~flag_value);
            break;

        case kDDFCheckFlagUser:
        case kDDFCheckFlagUnknown:
            DDF_WarnError("DDF_LineGetScrollPart: Unknown Part: %s", info);
            break;
    }
}

//----------------------------------------------------------------------------

static DDFSpecialFlags line_specials[] = {
    {"MUST_REACH", kLineSpecialMustReach, 0},
    {"SWITCH_SEPARATE", kLineSpecialSwitchSeparate, 0},
    {"BACK_SECTOR", kLineSpecialBackSector, 0},
    {nullptr, 0, 0}};

//
// DDF_LineGetSpecialFlags
//
// Gets the line special flags.
//
void DDF_LineGetSpecialFlags(const char *info, void *storage)
{
    LineSpecial *var = (LineSpecial *)storage;

    int flag_value;

    switch (
        DDF_MainCheckSpecialFlag(info, line_specials, &flag_value, true, false))
    {
        case kDDFCheckFlagPositive:
            *var = (LineSpecial)(*var | flag_value);
            break;

        case kDDFCheckFlagNegative:
            *var = (LineSpecial)(*var & ~flag_value);
            break;

        case kDDFCheckFlagUser:
        case kDDFCheckFlagUnknown:
            DDF_WarnError("Unknown line special: %s", info);
            break;
    }
}

//
// DDF_LineGetRadTrig
//
// Gets the line's radius trigger effect.
//
static void DDF_LineGetRadTrig(const char *info, void *storage)
{
    int *trigger_effect = (int *)storage;

    if (DDF_CompareName(info, "ENABLE_TAGGED") == 0)
    {
        *trigger_effect = +1;
        return;
    }
    if (DDF_CompareName(info, "DISABLE_TAGGED") == 0)
    {
        *trigger_effect = -1;
        return;
    }

    DDF_WarnError("DDF_LineGetRadTrig: Unknown effect: %s\n", info);
}

static const DDFSpecialFlags slidingdoor_names[] = {
    {"NONE", kSlidingDoorTypeNone, 0},
    {"LEFT", kSlidingDoorTypeLeft, 0},
    {"RIGHT", kSlidingDoorTypeRight, 0},
    {"CENTER", kSlidingDoorTypeCenter, 0},
    {"CENTRE", kSlidingDoorTypeCenter, 0},  // synonym
    {nullptr, 0, 0}};

//
// DDF_LineGetSlideType
//
static void DDF_LineGetSlideType(const char *info, void *storage)
{
    if (kDDFCheckFlagPositive != DDF_MainCheckSpecialFlag(info, slidingdoor_names,
                                                  (int *)storage, false, false))
    {
        DDF_WarnError("DDF_LineGetSlideType: Unknown slider: %s\n", info);
    }
}

static DDFSpecialFlags line_effect_names[] = {
    {"TRANSLUCENT", kLineEffectTypeTranslucency, 0},
    {"VECTOR_SCROLL", kLineEffectTypeVectorScroll, 0},
    {"OFFSET_SCROLL", kLineEffectTypeOffsetScroll, 0},

    {"SCALE_TEX", kLineEffectTypeScale, 0},
    {"SKEW_TEX", kLineEffectTypeSkew, 0},
    {"LIGHT_WALL", kLineEffectTypeLightWall, 0},

    {"UNBLOCK_THINGS", kLineEffectTypeUnblockThings, 0},
    {"BLOCK_SHOTS", kLineEffectTypeBlockShots, 0},
    {"BLOCK_SIGHT", kLineEffectTypeBlockSight, 0},
    {"SKY_TRANSFER", kLineEffectTypeSkyTransfer, 0},  // Lobo 2022
    {"TAGGED_OFFSET_SCROLL", kLineEffectTypeTaggedOffsetScroll, 0},    // MBF21
    {"BLOCK_LAND_MONSTERS", kLineEffectTypeBlockGroundedMonsters, 0},  // MBF21
    {"BLOCK_PLAYERS", kLineEffectTypeBlockPlayers, 0},                 // MBF21
    {"STRETCH_TEX_WIDTH", kLineEffectTypeStretchWidth, 0},    // Lobo 2023
    {"STRETCH_TEX_HEIGHT", kLineEffectTypeStretchHeight, 0},  // Lobo 2023
    {nullptr, 0, 0}};

//
// Gets the line effect flags.
//
static void DDF_LineGetLineEffect(const char *info, void *storage)
{
    LineEffectType *var = (LineEffectType *)storage;

    int flag_value;

    if (DDF_CompareName(info, "NONE") == 0)
    {
        *var = kLineEffectTypeNONE;
        return;
    }

    switch (DDF_MainCheckSpecialFlag(info, line_effect_names, &flag_value, true,
                                     false))
    {
        case kDDFCheckFlagPositive:
            *var = (LineEffectType)(*var | flag_value);
            break;

        case kDDFCheckFlagNegative:
            *var = (LineEffectType)(*var & ~flag_value);
            break;

        case kDDFCheckFlagUser:
        case kDDFCheckFlagUnknown:
            DDF_WarnError("Unknown line effect type: %s", info);
            break;
    }
}

static DDFSpecialFlags scroll_type_names[] = {
    {"DISPLACE", BoomScrollerTypeDisplace, 0},
    {"ACCEL", BoomScrollerTypeAccel, 0},
    {nullptr, 0, 0}};

//
// Gets the scroll type flags.
//
static void DDF_LineGetScrollType(const char *info, void *storage)
{
    BoomScrollerType *var = (BoomScrollerType *)storage;

    int flag_value;

    if (DDF_CompareName(info, "NONE") == 0)
    {
        *var = BoomScrollerTypeNone;
        return;
    }

    switch (DDF_MainCheckSpecialFlag(info, scroll_type_names, &flag_value, true,
                                     false))
    {
        case kDDFCheckFlagPositive:
            *var = (BoomScrollerType)(*var | flag_value);
            break;

        case kDDFCheckFlagNegative:
            *var = (BoomScrollerType)(*var & ~flag_value);
            break;

        case kDDFCheckFlagUser:
        case kDDFCheckFlagUnknown:
            DDF_WarnError("Unknown scroll type: %s", info);
            break;
    }
}

static DDFSpecialFlags sector_effect_names[] = {
    {"LIGHT_FLOOR", kSectorEffectTypeLightFloor, 0},
    {"LIGHT_CEILING", kSectorEffectTypeLightCeiling, 0},
    {"SCROLL_FLOOR", kSectorEffectTypeScrollFloor, 0},
    {"SCROLL_CEILING", kSectorEffectTypeScrollCeiling, 0},

    {"PUSH_THINGS", kSectorEffectTypePushThings, 0},
    {"SET_FRICTION", kSectorEffectTypeSetFriction, 0},
    {"WIND_FORCE", kSectorEffectTypeWindForce, 0},
    {"CURRENT_FORCE", kSectorEffectTypeCurrentForce, 0},
    {"POINT_FORCE", kSectorEffectTypePointForce, 0},

    {"RESET_FLOOR", kSectorEffectTypeResetFloor, 0},
    {"RESET_CEILING", kSectorEffectTypeResetCeiling, 0},
    {"ALIGN_FLOOR", kSectorEffectTypeAlignFloor, 0},
    {"ALIGN_CEILING", kSectorEffectTypeAlignCeiling, 0},
    {"SCALE_FLOOR", kSectorEffectTypeScaleFloor, 0},
    {"SCALE_CEILING", kSectorEffectTypeScaleCeiling, 0},

    {"BOOM_HEIGHTS", kSectorEffectTypeBoomHeights, 0},
    {nullptr, 0, 0}};

//
// Gets the sector effect flags.
//
static void DDF_LineGetSectorEffect(const char *info, void *storage)
{
    SectorEffectType *var = (SectorEffectType *)storage;

    int flag_value;

    if (DDF_CompareName(info, "NONE") == 0)
    {
        *var = kSectorEffectTypeNone;
        return;
    }

    switch (DDF_MainCheckSpecialFlag(info, sector_effect_names, &flag_value,
                                     true, false))
    {
        case kDDFCheckFlagPositive:
            *var = (SectorEffectType)(*var | flag_value);
            break;

        case kDDFCheckFlagNegative:
            *var = (SectorEffectType)(*var & ~flag_value);
            break;

        case kDDFCheckFlagUser:
        case kDDFCheckFlagUnknown:
            DDF_WarnError("Unknown sector effect type: %s", info);
            break;
    }
}

static DDFSpecialFlags portal_effect_names[] = {
    {"STANDARD", kPortalEffectTypeStandard, 0},
    {"MIRROR", kPortalEffectTypeMirror, 0},
    {"CAMERA", kPortalEffectTypeCamera, 0},

    {nullptr, 0, 0}};

//
// Gets the portal effect flags.
//
static void DDF_LineGetPortalEffect(const char *info, void *storage)
{
    PortalEffectType *var = (PortalEffectType *)storage;

    int flag_value;

    if (DDF_CompareName(info, "NONE") == 0)
    {
        *var = kPortalEffectTypeNone;
        return;
    }

    switch (DDF_MainCheckSpecialFlag(info, portal_effect_names, &flag_value,
                                     true, false))
    {
        case kDDFCheckFlagPositive:
            *var = (PortalEffectType)(*var | flag_value);
            break;

        case kDDFCheckFlagNegative:
            *var = (PortalEffectType)(*var & ~flag_value);
            break;

        case kDDFCheckFlagUser:
        case kDDFCheckFlagUnknown:
            DDF_WarnError("Unknown portal type: %s", info);
            break;
    }
}

static DDFSpecialFlags slope_type_names[] = {
    {"FAKE_FLOOR", kSlopeTypeDetailFloor, 0},
    {"FAKE_CEILING", kSlopeTypeDetailCeiling, 0},

    {nullptr, 0, 0}};

static void DDF_LineGetSlopeType(const char *info, void *storage)
{
    SlopeType *var = (SlopeType *)storage;

    int flag_value;

    if (DDF_CompareName(info, "NONE") == 0)
    {
        *var = kSlopeTypeNONE;
        return;
    }

    switch (DDF_MainCheckSpecialFlag(info, slope_type_names, &flag_value, true,
                                     false))
    {
        case kDDFCheckFlagPositive:
            *var = (SlopeType)(*var | flag_value);
            break;

        case kDDFCheckFlagNegative:
            *var = (SlopeType)(*var & ~flag_value);
            break;

        case kDDFCheckFlagUser:
        case kDDFCheckFlagUnknown:
            DDF_WarnError("Unknown slope type: %s", info);
            break;
    }
}

static void DDF_LineMakeCrush(const char *info)
{
    dynamic_line->f_.crush_damage_ = 10;
    dynamic_line->c_.crush_damage_ = 10;
}

//----------------------------------------------------------------------------

// --> Donut definition class

//
// donutdef_c Constructor
//
DonutDefinition::DonutDefinition() {}

//
// donutdef_c Copy constructor
//
DonutDefinition::DonutDefinition(DonutDefinition &rhs) { Copy(rhs); }

//
// donutdef_c Destructor
//
DonutDefinition::~DonutDefinition() {}

//
// donutdef_c::Copy()
//
void DonutDefinition::Copy(DonutDefinition &src)
{
    dodonut_ = src.dodonut_;

    // FIXME! Strip out the d_ since we're not trying to
    // to differentiate them now?
    d_sfxin_      = src.d_sfxin_;
    d_sfxinstop_  = src.d_sfxinstop_;
    d_sfxout_     = src.d_sfxout_;
    d_sfxoutstop_ = src.d_sfxoutstop_;
}

//
// donutdef_c::Default()
//
void DonutDefinition::Default()
{
    dodonut_      = false;
    d_sfxin_      = nullptr;
    d_sfxinstop_  = nullptr;
    d_sfxout_     = nullptr;
    d_sfxoutstop_ = nullptr;
}

//
// donutdef_c assignment operator
//
DonutDefinition &DonutDefinition::operator=(DonutDefinition &rhs)
{
    if (&rhs != this) Copy(rhs);

    return *this;
}

// --> Extrafloor definition class

//
// extrafloordef_c Constructor
//
ExtraFloorDefinition::ExtraFloorDefinition() {}

//
// extrafloordef_c Copy constructor
//
ExtraFloorDefinition::ExtraFloorDefinition(ExtraFloorDefinition &rhs)
{
    Copy(rhs);
}

//
// extrafloordef_c Destructor
//
ExtraFloorDefinition::~ExtraFloorDefinition() {}

//
// extrafloordef_c::Copy()
//
void ExtraFloorDefinition::Copy(ExtraFloorDefinition &src)
{
    control_ = src.control_;
    type_    = src.type_;
}

//
// extrafloordef_c::Default()
//
void ExtraFloorDefinition::Default()
{
    control_ = kExtraFloorControlNone;
    type_    = kExtraFloorTypeNone;
}

//
// extrafloordef_c assignment operator
//
ExtraFloorDefinition &ExtraFloorDefinition::operator=(ExtraFloorDefinition &rhs)
{
    if (&rhs != this) Copy(rhs);

    return *this;
}

// --> Ladder definition class

//
// ladderdef_c Constructor
//
LadderDefinition::LadderDefinition() {}

//
// ladderdef_c Copy constructor
//
LadderDefinition::LadderDefinition(LadderDefinition &rhs) { Copy(rhs); }

//
// ladderdef_c Destructor
//
LadderDefinition::~LadderDefinition() {}

//
// ladderdef_c::Copy()
//
void LadderDefinition::Copy(LadderDefinition &src) { height_ = src.height_; }

//
// ladderdef_c::Default()
//
void LadderDefinition::Default() { height_ = 0.0f; }

//
// ladderdef_c assignment operator
//
LadderDefinition &LadderDefinition::operator=(LadderDefinition &rhs)
{
    if (&rhs != this) Copy(rhs);

    return *this;
}

// --> Light effect definition class

//
// lightdef_c Constructor
//
LightSpecialDefinition::LightSpecialDefinition() {}

//
// lightdef_c Copy constructor
//
LightSpecialDefinition::LightSpecialDefinition(LightSpecialDefinition &rhs)
{
    Copy(rhs);
}

//
// lightdef_c Destructor
//
LightSpecialDefinition::~LightSpecialDefinition() {}

//
// lightdef_c::Copy()
//
void LightSpecialDefinition::Copy(LightSpecialDefinition &src)
{
    type_       = src.type_;
    level_      = src.level_;
    chance_     = src.chance_;
    darktime_   = src.darktime_;
    brighttime_ = src.brighttime_;
    sync_       = src.sync_;
    step_       = src.step_;
}

//
// lightdef_c::Default()
//
void LightSpecialDefinition::Default()
{
    type_       = kLightSpecialTypeNone;
    level_      = 64;
    chance_     = PERCENT_MAKE(50);
    darktime_   = 0;
    brighttime_ = 0;
    sync_       = 0;
    step_       = 8;
}

//
// lightdef_c assignment operator
//
LightSpecialDefinition &LightSpecialDefinition::operator=(
    LightSpecialDefinition &rhs)
{
    if (&rhs != this) Copy(rhs);

    return *this;
}

// --> Moving plane definition class

//
// movplanedef_c Constructor
//
PlaneMoverDefinition::PlaneMoverDefinition() {}

//
// movplanedef_c Copy constructor
//
PlaneMoverDefinition::PlaneMoverDefinition(PlaneMoverDefinition &rhs)
{
    Copy(rhs);
}

//
// movplanedef_c Destructor
//
PlaneMoverDefinition::~PlaneMoverDefinition() {}

//
// movplanedef_c::Copy()
//
void PlaneMoverDefinition::Copy(PlaneMoverDefinition &src)
{
    type_           = src.type_;
    is_ceiling_     = src.is_ceiling_;
    speed_up_       = src.speed_up_;
    speed_down_     = src.speed_down_;
    destref_        = src.destref_;
    dest_           = src.dest_;
    otherref_       = src.otherref_;
    other_          = src.other_;
    crush_damage_   = src.crush_damage_;
    tex_            = src.tex_;
    wait_           = src.wait_;
    prewait_        = src.prewait_;
    sfxstart_       = src.sfxstart_;
    sfxup_          = src.sfxup_;
    sfxdown_        = src.sfxdown_;
    sfxstop_        = src.sfxstop_;
    scroll_angle_   = src.scroll_angle_;
    scroll_speed_   = src.scroll_speed_;
    ignore_texture_ = src.ignore_texture_;
}

//
// movplanedef_c::Default()
//
void PlaneMoverDefinition::Default(PlaneMoverDefinition::PlaneMoverDefault def)
{
    type_ = kPlaneMoverUndefined;

    if (def == kPlaneMoverDefaultCeilingLine ||
        def == kPlaneMoverDefaultCeilingSect)
        is_ceiling_ = true;
    else
        is_ceiling_ = false;

    switch (def)
    {
        case kPlaneMoverDefaultCeilingLine:
        case kPlaneMoverDefaultFloorLine:
        {
            speed_up_   = -1;
            speed_down_ = -1;
            break;
        }

        case kPlaneMoverDefaultDonutFloor:
        {
            speed_up_   = FLOORSPEED / 2;
            speed_down_ = FLOORSPEED / 2;
            break;
        }

        default:
        {
            speed_up_   = 0;
            speed_down_ = 0;
            break;
        }
    }

    destref_ = kTriggerHeightReferenceAbsolute;

    // FIXME!!! Why are we using INT_MAX with a fp number?
    dest_ = (def != kPlaneMoverDefaultDonutFloor) ? 0.0f : (float)INT_MAX;

    switch (def)
    {
        case kPlaneMoverDefaultCeilingLine:
        {
            otherref_ =
                (TriggerHeightReference)(kTriggerHeightReferenceCurrent |
                                         kTriggerHeightReferenceCeiling);
            break;
        }

        case kPlaneMoverDefaultFloorLine:
        {
            otherref_ =
                (TriggerHeightReference)(kTriggerHeightReferenceSurrounding |
                                         kTriggerHeightReferenceHighest |
                                         kTriggerHeightReferenceInclude);
            break;
        }

        default:
        {
            otherref_ = kTriggerHeightReferenceAbsolute;
            break;
        }
    }

    // FIXME!!! Why are we using INT_MAX with a fp number?
    other_ = (def != kPlaneMoverDefaultDonutFloor) ? 0.0f : (float)INT_MAX;

    crush_damage_ = 0;

    tex_.clear();

    wait_    = 0;
    prewait_ = 0;

    sfxstart_ = nullptr;
    sfxup_    = nullptr;
    sfxdown_  = nullptr;
    sfxstop_  = nullptr;

    scroll_angle_ = 0;
    scroll_speed_ = 0.0f;

    ignore_texture_ = false;
}

//
// movplanedef_c assignment operator
//
PlaneMoverDefinition &PlaneMoverDefinition::operator=(PlaneMoverDefinition &rhs)
{
    if (&rhs != this) Copy(rhs);

    return *this;
}

// --> Sliding door definition class

//
// sliding_door_c Constructor
//
SlidingDoor::SlidingDoor() {}

//
// sliding_door_c Copy constructor
//
SlidingDoor::SlidingDoor(SlidingDoor &rhs) { Copy(rhs); }

//
// sliding_door_c Destructor
//
SlidingDoor::~SlidingDoor() {}

//
// sliding_door_c::Copy()
//
void SlidingDoor::Copy(SlidingDoor &src)
{
    type_        = src.type_;
    speed_       = src.speed_;
    wait_        = src.wait_;
    see_through_ = src.see_through_;
    distance_    = src.distance_;
    sfx_start_   = src.sfx_start_;
    sfx_open_    = src.sfx_open_;
    sfx_close_   = src.sfx_close_;
    sfx_stop_    = src.sfx_stop_;
}

//
// sliding_door_c::Default()
//
void SlidingDoor::Default()
{
    type_        = kSlidingDoorTypeNone;
    speed_       = 4.0f;
    wait_        = 150;
    see_through_ = false;
    distance_    = PERCENT_MAKE(90);
    sfx_start_   = sfx_None;
    sfx_open_    = sfx_None;
    sfx_close_   = sfx_None;
    sfx_stop_    = sfx_None;
}

//
// sliding_door_c assignment operator
//
SlidingDoor &SlidingDoor::operator=(SlidingDoor &rhs)
{
    if (&rhs != this) Copy(rhs);

    return *this;
}

// --> Teleport point definition class

//
// teleportdef_c Constructor
//
TeleportDefinition::TeleportDefinition() {}

//
// teleportdef_c Copy constructor
//
TeleportDefinition::TeleportDefinition(TeleportDefinition &rhs) { Copy(rhs); }

//
// teleportdef_c Destructor
//
TeleportDefinition::~TeleportDefinition() {}

//
// teleportdef_c::Copy()
//
void TeleportDefinition::Copy(TeleportDefinition &src)
{
    teleport_ = src.teleport_;

    inspawnobj_     = src.inspawnobj_;
    inspawnobj_ref_ = src.inspawnobj_ref_;

    outspawnobj_     = src.outspawnobj_;
    outspawnobj_ref_ = src.outspawnobj_ref_;

    special_ = src.special_;
    delay_   = src.delay_;
}

//
// teleportdef_c::Default()
//
void TeleportDefinition::Default()
{
    teleport_ = false;

    inspawnobj_ = nullptr;
    inspawnobj_ref_.clear();

    outspawnobj_ = nullptr;
    outspawnobj_ref_.clear();

    delay_   = 0;
    special_ = kTeleportSpecialNone;
}

//
// teleportdef_c assignment operator
//
TeleportDefinition &TeleportDefinition::operator=(TeleportDefinition &rhs)
{
    if (&rhs != this) Copy(rhs);

    return *this;
}

// --> Line definition type class

//
// LineType Constructor
//
LineType::LineType() : number_(0) { Default(); }

//
// LineType Destructor
//
LineType::~LineType() {}

void LineType::CopyDetail(LineType &src)
{
    newtrignum_ = src.newtrignum_;
    type_       = src.type_;
    obj_        = src.obj_;
    keys_       = src.keys_;
    count_      = src.count_;

    f_ = src.f_;
    c_ = src.c_;
    d_ = src.d_;
    s_ = src.s_;
    t_ = src.t_;
    l_ = src.l_;

    ladder_       = src.ladder_;
    e_exit_       = src.e_exit_;
    hub_exit_     = src.hub_exit_;
    s_xspeed_     = src.s_xspeed_;
    s_yspeed_     = src.s_yspeed_;
    scroll_parts_ = src.scroll_parts_;

    failedmessage_ = src.failedmessage_;
    failed_sfx_    = src.failed_sfx_;

    use_colourmap_ = src.use_colourmap_;
    gravity_       = src.gravity_;
    friction_      = src.friction_;
    viscosity_     = src.viscosity_;
    drag_          = src.drag_;
    ambient_sfx_   = src.ambient_sfx_;
    activate_sfx_  = src.activate_sfx_;
    music_         = src.music_;
    autoline_      = src.autoline_;
    singlesided_   = src.singlesided_;
    ef_            = src.ef_;
    translucency_  = src.translucency_;
    appear_        = src.appear_;

    special_flags_  = src.special_flags_;
    trigger_effect_ = src.trigger_effect_;
    line_effect_    = src.line_effect_;
    line_parts_     = src.line_parts_;
    scroll_type_    = src.scroll_type_;
    sector_effect_  = src.sector_effect_;
    portal_effect_  = src.portal_effect_;
    slope_type_     = src.slope_type_;
    fx_color_       = src.fx_color_;

    // lobo 2022
    effectobject_     = src.effectobject_;
    effectobject_ref_ = src.effectobject_ref_;
    glass_            = src.glass_;
    brokentex_        = src.brokentex_;
}

void LineType::Default(void)
{
    newtrignum_ = 0;
    type_       = kLineTriggerNone;
    obj_        = kTriggerActivatorNone;
    keys_       = kDoorKeyNone;
    count_      = -1;

    f_.Default(PlaneMoverDefinition::kPlaneMoverDefaultFloorLine);
    c_.Default(PlaneMoverDefinition::kPlaneMoverDefaultCeilingLine);

    d_.Default();  // Donut
    s_.Default();  // Sliding Door

    t_.Default();  // Teleport
    l_.Default();  // Light definition

    ladder_.Default();  // Ladder

    e_exit_       = kExitTypeNone;
    hub_exit_     = 0;
    s_xspeed_     = 0.0f;
    s_yspeed_     = 0.0f;
    scroll_parts_ = kScrollingPartNone;

    failedmessage_.clear();
    failed_sfx_ = nullptr;

    use_colourmap_ = nullptr;
    gravity_       = kFloatUnused;
    friction_      = kFloatUnused;
    viscosity_     = kFloatUnused;
    drag_          = kFloatUnused;
    ambient_sfx_   = sfx_None;
    activate_sfx_  = sfx_None;
    music_         = 0;
    autoline_      = false;
    singlesided_   = false;

    ef_.Default();

    translucency_   = PERCENT_MAKE(100);
    appear_         = kAppearsWhenDefault;
    special_flags_  = kLineSpecialNone;
    trigger_effect_ = 0;
    line_effect_    = kLineEffectTypeNONE;
    line_parts_     = kScrollingPartNone;
    scroll_type_    = BoomScrollerTypeNone;
    sector_effect_  = kSectorEffectTypeNone;
    portal_effect_  = kPortalEffectTypeNone;
    slope_type_     = kSlopeTypeNONE;
    fx_color_       = SG_BLACK_RGBA32;

    // lobo 2022
    effectobject_ = nullptr;
    effectobject_ref_.clear();
    glass_ = false;
    brokentex_.clear();
}

// --> Line definition type container class

//
// LineTypeContainer Constructor
//
LineTypeContainer::LineTypeContainer() { Reset(); }

//
// LineTypeContainer Destructor
//
LineTypeContainer::~LineTypeContainer()
{
    for (auto iter = begin(); iter != end(); iter++)
    {
        LineType *line = *iter;
        delete line;
        line = nullptr;
    }
}

//
// LineType* LineTypeContainer::Lookup()
//
// Looks an linetype by id, returns nullptr if line can't be found.
//
LineType *LineTypeContainer::Lookup(const int id)
{
    if (id == 0) return default_linetype;

    int slot = (((id) + LOOKUP_CACHESIZE) % LOOKUP_CACHESIZE);

    // check the cache
    if (lookup_cache_[slot] && lookup_cache_[slot]->number_ == id)
    {
        return lookup_cache_[slot];
    }

    for (auto iter = rbegin(); iter != rend(); iter++)
    {
        LineType *l = *iter;

        if (l->number_ == id)
        {
            // update the cache
            lookup_cache_[slot] = l;
            return l;
        }
    }

    return nullptr;
}

//
// LineTypeContainer::Reset()
//
// Clears down both the data and the cache
//
void LineTypeContainer::Reset()
{
    for (auto iter = begin(); iter != end(); iter++)
    {
        LineType *line = *iter;
        delete line;
        line = nullptr;
    }
    clear();
    memset(lookup_cache_, 0, sizeof(LineType *) * LOOKUP_CACHESIZE);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
