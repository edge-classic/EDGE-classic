//----------------------------------------------------------------------------
//  EDGE Specials Lines & Floor Code
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
//
// -KM- 1998/09/01 Lines.ddf
//
//

#include "p_spec.h"

#include <limits.h>

#include "AlmostEquals.h"
#include "con_main.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "epi.h"
#include "epi_doomdefs.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"
#include "f_interm.h"
#include "g_game.h"
#include "m_argv.h"
#include "m_random.h"
#include "n_network.h"
#include "p_local.h"
#include "r_misc.h"
#include "r_sky.h" //Lobo 2022: added for our Sky Transfer special
#include "rad_trig.h"
#include "s_blit.h"
#include "s_music.h"
#include "s_sound.h"

extern ConsoleVariable double_framerate;

// Level exit timer
bool level_timer;
int  level_time_count;

//
// Animating line and sector specials
//
std::list<Line *>            active_line_animations;
std::list<Sector *>          active_sector_animations;
std::vector<SectorAnimation> sector_animations;
std::vector<LineAnimation>   line_animations;
std::vector<LightAnimation>  light_animations;

static bool DoSectorsFromTag(int tag, const void *p1, void *p2, bool (*func)(Sector *, const void *, void *));

static bool DoPlaneWrapper(Sector *s, const void *p1, void *p2)
{
    return RunPlaneMover(s, (const PlaneMoverDefinition *)p1, (Sector *)p2);
}

static bool DoLightsWrapper(Sector *s, const void *p1, void *p2)
{
    return RunSectorLight(s, (const LightSpecialDefinition *)p1);
}

static bool DoDonutWrapper(Sector *s, const void *p1, void *p2)
{
    return RunDonutSpecial(s, (SoundEffect **)p2);
}

//
// UTILITIES
//

//
// Will return a side_t * given the number of the current sector,
// the line number, and the side (0/1) that you want.
//
Side *GetLineSidedef(int currentSector, int line, int side)
{
    Line *ldef = level_sectors[currentSector].lines[line];

    return ldef->side[side];
}

//
// Will return a sector_t*
//  given the number of the current sector,
//  the line number and the side (0/1) that you want.
//
Sector *GetLineSector(int currentSector, int line, int side)
{
    Line *ldef = level_sectors[currentSector].lines[line];

    return side ? ldef->back_sector : ldef->front_sector;
}

//
// Given the sector number and the line number, it will tell you whether the
// line is two-sided or not.
//
int LineIsTwoSided(int sector, int line)
{
    return (level_sectors[sector].lines[line])->flags & kLineFlagTwoSided;
}

//
// Return sector_t * of sector next to current; nullptr if not two-sided line
//
Sector *GetLineSectorAdjacent(const Line *line, const Sector *sec, bool ignore_selfref)
{
    if (!(line->flags & kLineFlagTwoSided))
        return nullptr;

    // -AJA- 2011/03/31: follow BOOM's logic for self-ref linedefs, which
    //                   fixes the red door of MAP01 of 1024CLAU.wad
    if (ignore_selfref && (line->front_sector == line->back_sector))
        return nullptr;

    if (line->front_sector == sec)
        return line->back_sector;

    return line->front_sector;
}

//
// -AJA- 2001/05/29: this is an amalgamation of the previous bunch of
//       routines, using the new REF_* flag names.  Now there's just
//       this one big routine -- the compiler's optimiser had better
//       kick in !
//
#define EDGE_REFERENCE_PLANE_HEIGHT(sector)                                                                            \
    ((ref & kTriggerHeightReferenceCeiling) ? (sector)->ceiling_height : (sector)->floor_height)

float FindSurroundingHeight(const TriggerHeightReference ref, const Sector *sec)
{
    int   i, count;
    float height;
    float base = EDGE_REFERENCE_PLANE_HEIGHT(sec);

    if (ref & kTriggerHeightReferenceInclude)
        height = base;
    else if (ref & kTriggerHeightReferenceHighest)
        height = -32000.0f; // BOOM compatible value
    else
        height = +32000.0f;

    for (i = count = 0; i < sec->line_count; i++)
    {
        Sector *other = GetLineSectorAdjacent(sec->lines[i], sec, true);

        if (!other)
            continue;

        float other_h = EDGE_REFERENCE_PLANE_HEIGHT(other);

        if (ref & kTriggerHeightReferenceNext)
        {
            bool satisfy;

            // Note that kTriggerHeightReferenceHighest is used for the
            // NextLowest types, and vice versa, which may seem strange.  It's
            // because the next lowest sector is actually the highest of all
            // adjacent sectors that are lower than the current sector.

            if (ref & kTriggerHeightReferenceHighest)
                satisfy = (other_h < base); // next lowest
            else
                satisfy = (other_h > base); // next highest

            if (!satisfy)
                continue;
        }

        count++;

        if (ref & kTriggerHeightReferenceHighest)
            height = HMM_MAX(height, other_h);
        else
            height = HMM_MIN(height, other_h);
    }

    if ((ref & kTriggerHeightReferenceNext) && count == 0)
        return base;

    return height;
}

//
// FIND THE SHORTEST BOTTOM TEXTURE SURROUNDING sec
// AND RETURN IT'S TOP HEIGHT
//
// -KM- 1998/09/01 Lines.ddf; used to be inlined in p_floors
//
float FindRaiseToTexture(Sector *sec)
{
    int   i;
    Side *side;
    float minsize = (float)INT_MAX;
    int   secnum  = sec - level_sectors;

    for (i = 0; i < sec->line_count; i++)
    {
        if (LineIsTwoSided(secnum, i))
        {
            side = GetLineSidedef(secnum, i, 0);

            if (side->bottom.image)
            {
                if (side->bottom.image->ScaledHeightActual() < minsize)
                    minsize = side->bottom.image->ScaledHeightActual();
            }

            side = GetLineSidedef(secnum, i, 1);

            if (side->bottom.image)
            {
                if (side->bottom.image->ScaledHeightActual() < minsize)
                    minsize = side->bottom.image->ScaledHeightActual();
            }
        }
    }

    return sec->floor_height + minsize;
}

//
// Returns the FIRST sector that tag refers to.
//
// -KM- 1998/09/27 Doesn't need a line.
// -AJA- 1999/09/29: Now returns a sector_t, and has no start.
//
Sector *FindSectorFromTag(int tag)
{
    int i;

    for (i = 0; i < total_level_sectors; i++)
    {
        if (level_sectors[i].tag == tag)
            return level_sectors + i;
    }

    return nullptr;
}

//
// Find minimum light from an adjacent sector
//
int FindMinimumSurroundingLight(Sector *sector, int max)
{
    int     i;
    int     min;
    Line   *line;
    Sector *check;

    min = max;
    for (i = 0; i < sector->line_count; i++)
    {
        line  = sector->lines[i];
        check = GetLineSectorAdjacent(line, sector);

        if (!check)
            continue;

        if (check->properties.light_level < min)
            min = check->properties.light_level;
    }
    return min;
}

//
// Find maximum light from an adjacent sector
//
int FindMaxSurroundingLight(Sector *sector, int min)
{
    int     i;
    int     max;
    Line   *line;
    Sector *check;

    max = min;
    for (i = 0; i < sector->line_count; i++)
    {
        line  = sector->lines[i];
        check = GetLineSectorAdjacent(line, sector);

        if (!check)
            continue;

        if (check->properties.light_level > max)
            max = check->properties.light_level;
    }
    return max;
}

void AddSpecialLine(Line *ld)
{
    // check if already linked
    std::list<Line *>::iterator LI;

    for (LI = active_line_animations.begin(); LI != active_line_animations.end(); LI++)
    {
        if (*LI == ld)
            return;
    }

    active_line_animations.push_back(ld);
}

void AddSpecialSector(Sector *sec)
{
    // check if already linked
    std::list<Sector *>::iterator SI;

    for (SI = active_sector_animations.begin(); SI != active_sector_animations.end(); SI++)
    {
        if (*SI == sec)
            return;
    }

    active_sector_animations.push_back(sec);
}

static void AdjustScrollParts(Side *side, bool left, ScrollingPart parts, float x_speed, float y_speed)
{
    float xmul = (left && (parts & kScrollingPartLeftRevX)) ? -1 : 1;
    float ymul = (left && (parts & kScrollingPartLeftRevY)) ? -1 : 1;

    if (!side)
        return;

    // -AJA- this is an inconsistency, needed for compatibility with
    //       original DOOM and Boom.  (Should be kScrollingPartRIGHT |
    //       kScrollingPartLEFT).
    if (parts == kScrollingPartNone)
        parts = kScrollingPartRight;

    if (parts & (left ? kScrollingPartLeftUpper : kScrollingPartRightUpper))
    {
        side->top.scroll.X += x_speed * xmul;
        side->top.scroll.Y += y_speed * ymul;
    }
    if (parts & (left ? kScrollingPartLeftMiddle : kScrollingPartRightMiddle))
    {
        side->middle.scroll.X += x_speed * xmul;
        side->middle.scroll.Y += y_speed * ymul;
    }
    if (parts & (left ? kScrollingPartLeftLower : kScrollingPartRightLower))
    {
        side->bottom.scroll.X += x_speed * xmul;
        side->bottom.scroll.Y += y_speed * ymul;
    }
}

static void AdjustScaleParts(Side *side, bool left, ScrollingPart parts, float factor)
{
    if (!side)
        return;

    if (parts == kScrollingPartNone)
        parts = (ScrollingPart)(kScrollingPartLeft | kScrollingPartRight);

    if (parts & (left ? kScrollingPartLeftUpper : kScrollingPartRightUpper))
    {
        side->top.x_matrix.X *= factor;
        side->top.y_matrix.Y *= factor;
    }
    if (parts & (left ? kScrollingPartLeftMiddle : kScrollingPartRightMiddle))
    {
        side->middle.x_matrix.X *= factor;
        side->middle.y_matrix.Y *= factor;
    }
    if (parts & (left ? kScrollingPartLeftLower : kScrollingPartRightLower))
    {
        side->bottom.x_matrix.X *= factor;
        side->bottom.y_matrix.Y *= factor;
    }
}

static void AdjustStretchParts(Side *side, bool left, ScrollingPart parts, float linelength, bool widthOnly)
{
    if (!side)
        return;

    float factor = 0;

    if (parts == kScrollingPartNone)
        parts = (ScrollingPart)(kScrollingPartLeft | kScrollingPartRight);

    if (parts & (left ? kScrollingPartLeftUpper : kScrollingPartRightUpper))
    {
        if (side->top.image)
            factor = side->top.image->ScaledWidthActual() / linelength;

        if (widthOnly)
            side->top.x_matrix.X *= factor;

        if (!widthOnly)
            side->top.y_matrix.Y *= factor;
    }
    if (parts & (left ? kScrollingPartLeftMiddle : kScrollingPartRightMiddle))
    {
        if (side->middle.image)
            factor = side->middle.image->ScaledWidthActual() / linelength;

        if (widthOnly)
            side->middle.x_matrix.X *= factor;

        if (!widthOnly)
            side->middle.y_matrix.Y *= factor;
    }
    if (parts & (left ? kScrollingPartLeftLower : kScrollingPartRightLower))
    {
        if (side->bottom.image)
            factor = side->bottom.image->ScaledWidthActual() / linelength;

        if (widthOnly)
            side->bottom.x_matrix.X *= factor;

        if (!widthOnly)
            side->bottom.y_matrix.Y *= factor;
    }
}

static void AdjustSkewParts(Side *side, bool left, ScrollingPart parts, float skew)
{
    if (!side)
        return;

    if (parts == kScrollingPartNone)
        parts = (ScrollingPart)(kScrollingPartLeft | kScrollingPartRight);

    if (parts & (left ? kScrollingPartLeftUpper : kScrollingPartRightUpper))
        side->top.y_matrix.X = skew * side->top.y_matrix.Y;

    if (parts & (left ? kScrollingPartLeftMiddle : kScrollingPartRightMiddle))
        side->middle.y_matrix.X = skew * side->middle.y_matrix.Y;

    if (parts & (left ? kScrollingPartLeftLower : kScrollingPartRightLower))
        side->bottom.y_matrix.X = skew * side->bottom.y_matrix.Y;
}

static void AdjustLightParts(Side *side, bool left, ScrollingPart parts, RegionProperties *p)
{
    if (!side)
        return;

    if (parts == kScrollingPartNone)
        parts = (ScrollingPart)(kScrollingPartLeft | kScrollingPartRight);

    if (parts & (left ? kScrollingPartLeftUpper : kScrollingPartRightUpper))
        side->top.override_properties = p;

    if (parts & (left ? kScrollingPartLeftMiddle : kScrollingPartRightMiddle))
        side->middle.override_properties = p;

    if (parts & (left ? kScrollingPartLeftLower : kScrollingPartRightLower))
        side->bottom.override_properties = p;
}

static float ScaleFactorForPlane(MapSurface &surf, float line_len, bool use_height)
{
    if (use_height)
        return surf.image->ScaledHeightActual() / line_len;
    else
        return surf.image->ScaledWidthActual() / line_len;
}

static void P_EFTransferTrans(Sector *ctrl, Sector *sec, Line *line, const ExtraFloorDefinition *ef, float trans)
{
    int i;

    // floor and ceiling

    if (ctrl->floor.translucency > trans)
        ctrl->floor.translucency = trans;

    if (ctrl->ceiling.translucency > trans)
        ctrl->ceiling.translucency = trans;

    // sides

    if (!(ef->type_ & kExtraFloorTypeThick))
        return;

    if (ef->type_ & (kExtraFloorTypeSideUpper | kExtraFloorTypeSideLower))
    {
        for (i = 0; i < sec->line_count; i++)
        {
            Line *L = sec->lines[i];
            Side *S = nullptr;

            if (L->front_sector == sec)
                S = L->side[1];
            else if (L->back_sector == sec)
                S = L->side[0];

            if (!S)
                continue;

            if (ef->type_ & kExtraFloorTypeSideUpper)
                S->top.translucency = trans;
            else // kExtraFloorTypeSideLower
                S->bottom.translucency = trans;
        }

        return;
    }

    line->side[0]->middle.translucency = trans;
}

//
// Lobo:2021 Setup our special debris linetype.
//
// This is because we want to set the "line_effect_="" specials
// BLOCK_SHOTS and BLOCK_SIGHT
// without actually activating the line.
//
static void P_LineEffectDebris(Line *TheLine, const LineType *special)
{
    if (TheLine->side[0] && TheLine->side[1])
    {
        // block bullets/missiles
        if (special->line_effect_ & kLineEffectTypeBlockShots)
        {
            TheLine->flags |= kLineFlagShootBlock;
        }

        // block monster sight
        if (special->line_effect_ & kLineEffectTypeBlockSight)
        {
            TheLine->flags |= kLineFlagSightBlock;
        }

        // It should be set in the map editor like this
        // anyway, but force it just in case
        TheLine->flags |= kLineFlagBlocking;
        TheLine->flags |= kLineFlagBlockMonsters;
    }
}

//
// Lobo:2021 Spawn debris on our special linetype.
//
static void P_SpawnLineEffectDebris(Line *TheLine, const LineType *special)
{
    if (!special)
        return; // found nothing so exit

    // Spawn our debris thing
    const MapObjectDefinition *info;

    info = special->effectobject_;
    if (!info)
        return; // found nothing so exit

    if (!level_flags.have_extra && (info->extended_flags_ & kExtendedFlagExtra))
        return;

    // if it's shootable we've already handled this elsewhere
    if (special->type_ == kLineTriggerShootable)
        return;

    float midx = 0;
    float midy = 0;
    float midz = 0;

    // calculate midpoint
    midx = (TheLine->vertex_1->X + TheLine->vertex_2->X) / 2;
    midy = (TheLine->vertex_1->Y + TheLine->vertex_2->Y) / 2;
    midz = kOnFloorZ;

    float dx = RandomByteDeterministic() * info->radius_ / 255.0f;
    float dy = RandomByteDeterministic() * info->radius_ / 255.0f;

    // move slightly forward to spawn the debris
    midx += dx + info->radius_;
    midy += dy + info->radius_;

    SpawnDebris(midx, midy, midz, 0 + kBAMAngle180, info);

    midx = (TheLine->vertex_1->X + TheLine->vertex_2->X) / 2;
    midy = (TheLine->vertex_1->Y + TheLine->vertex_2->Y) / 2;

    // move slightly backward to spawn the debris
    midx -= dx + info->radius_;
    midy -= dy + info->radius_;

    SpawnDebris(midx, midy, midz, 0 + kBAMAngle180, info);
}

//
// Handles BOOM's line -> tagged line transfers.
//
static void P_LineEffect(Line *target, Line *source, const LineType *special)
{
    float length = PointToDistance(0, 0, source->delta_x, source->delta_y);
    float factor = 64.0 / length;

    if ((special->line_effect_ & kLineEffectTypeTranslucency) && (target->flags & kLineFlagTwoSided))
    {
        target->side[0]->middle.translucency = 0.5f;
        target->side[1]->middle.translucency = 0.5f;
    }

    if ((special->line_effect_ & kLineEffectTypeOffsetScroll) && target->side[0])
    {
        float x_speed = -target->side[0]->middle.offset.X;
        float y_speed = target->side[0]->middle.offset.Y;

        AdjustScrollParts(target->side[0], 0, special->line_parts_, x_speed, y_speed);

        AddSpecialLine(target);
    }

    if ((special->line_effect_ & kLineEffectTypeTaggedOffsetScroll) && target->side[0] && source->side[0])
    {
        LineAnimation anim;
        anim.target = target;
        if (special->scroll_type_ == BoomScrollerTypeNone)
        {
            anim.side_0_x_speed = -source->side[0]->middle.offset.X / 8.0;
            anim.side_0_y_speed = source->side[0]->middle.offset.Y / 8.0;
        }
        else
        {
            // BOOM spec states that the front sector is the height reference
            // for displace/accel scrollers
            if (source->front_sector)
            {
                anim.scroll_sector_reference  = source->front_sector;
                anim.scroll_special_reference = special;
                anim.scroll_line_reference    = source;
                anim.side_0_x_offset_speed    = -source->side[0]->middle.offset.X / 8.0;
                anim.side_0_y_offset_speed    = source->side[0]->middle.offset.Y / 8.0;
                for (int i = 0; i < total_level_lines; i++)
                {
                    if (level_lines[i].tag == source->front_sector->tag)
                    {
                        if (!level_lines[i].special || level_lines[i].special->count_ == 1)
                            anim.permanent = true;
                    }
                }
                anim.last_height = anim.scroll_sector_reference->original_height;
            }
        }
        line_animations.push_back(anim);
        AddSpecialLine(target);
    }

    if (special->line_effect_ & kLineEffectTypeVectorScroll)
    {
        LineAnimation anim;
        anim.target = target;
        float dx    = source->delta_x / 32.0f;
        float dy    = source->delta_y / 32.0f;
        float ldx   = target->delta_x;
        float ldy   = target->delta_y;
        float x     = HMM_ABS(ldx);
        float y     = HMM_ABS(ldy);
        if (y > x)
            std::swap(x, y);
        if (x)
        {
            float d = x / HMM_SINF(atan(y / x) + HMM_PI / 2.0);
            if (isfinite(d))
            {
                x = -(dy * ldy + dx * ldx) / d;
                y = -(dx * ldy - dy * ldx) / d;
            }
            else
            {
                x = 0;
                y = 0;
            }
        }
        else
        {
            x = 0;
            y = 0;
        }

        if (x || y)
        {
            if (special->scroll_type_ == BoomScrollerTypeNone)
            {
                anim.side_0_x_speed += x;
                anim.side_1_x_speed += x;
                anim.side_0_y_speed += y;
                anim.side_1_y_speed += y;
            }
            else
            {
                // BOOM spec states that the front sector is the height
                // reference for displace/accel scrollers
                if (source->front_sector)
                {
                    anim.scroll_sector_reference  = source->front_sector;
                    anim.scroll_special_reference = special;
                    anim.scroll_line_reference    = source;
                    anim.dynamic_delta_x += x;
                    anim.dynamic_delta_y += y;
                    for (int i = 0; i < total_level_lines; i++)
                    {
                        if (level_lines[i].tag == source->front_sector->tag)
                        {
                            if (!level_lines[i].special || level_lines[i].special->count_ == 1)
                                anim.permanent = true;
                        }
                    }
                    anim.last_height = anim.scroll_sector_reference->original_height;
                }
            }
            line_animations.push_back(anim);
            AddSpecialLine(target);
        }
    }

    // experimental: unblock line(s)
    if (special->line_effect_ & kLineEffectTypeUnblockThings)
    {
        if (target->side[0] && target->side[1] && (target != source))
            target->flags &=
                ~(kLineFlagBlocking | kLineFlagBlockMonsters | kLineFlagBlockGroundedMonsters | kLineFlagBlockPlayers);
    }

    // experimental: block bullets/missiles
    if (special->line_effect_ & kLineEffectTypeBlockShots)
    {
        if (target->side[0] && target->side[1])
            target->flags |= kLineFlagShootBlock;
    }

    // experimental: block monster sight
    if (special->line_effect_ & kLineEffectTypeBlockSight)
    {
        if (target->side[0] && target->side[1])
            target->flags |= kLineFlagSightBlock;
    }

    // experimental: scale wall texture(s) by line length
    if (special->line_effect_ & kLineEffectTypeScale)
    {
        AdjustScaleParts(target->side[0], 0, special->line_parts_, factor);
        AdjustScaleParts(target->side[1], 1, special->line_parts_, factor);
    }

    // experimental: skew wall texture(s) by sidedef Y offset
    if ((special->line_effect_ & kLineEffectTypeSkew) && source->side[0])
    {
        float skew = source->side[0]->top.offset.X / 128.0f;

        AdjustSkewParts(target->side[0], 0, special->line_parts_, skew);
        AdjustSkewParts(target->side[1], 1, special->line_parts_, skew);

        if (target == source)
        {
            source->side[0]->middle.offset.X = 0;
            source->side[0]->bottom.offset.X = 0;
        }
    }

    // experimental: transfer lighting to wall parts
    if (special->line_effect_ & kLineEffectTypeLightWall)
    {
        AdjustLightParts(target->side[0], 0, special->line_parts_, &source->front_sector->properties);
        AdjustLightParts(target->side[1], 1, special->line_parts_, &source->front_sector->properties);
    }

    // Lobo 2022: experimental partial sky transfer support
    if ((special->line_effect_ & kLineEffectTypeSkyTransfer) && source->side[0])
    {
        if (source->side[0]->top.image)
            sky_image = ImageLookup(source->side[0]->top.image->name_.c_str(), kImageNamespaceTexture);
    }

    // experimental: stretch wall texture(s) by line length
    if (special->line_effect_ & kLineEffectTypeStretchWidth)
    {
        AdjustStretchParts(target->side[0], 0, special->line_parts_, length, true);
        AdjustStretchParts(target->side[1], 1, special->line_parts_, length, true);
    }

    // experimental: stretch wall texture(s) by line length
    if (special->line_effect_ & kLineEffectTypeStretchHeight)
    {
        AdjustStretchParts(target->side[0], 0, special->line_parts_, length, false);
        AdjustStretchParts(target->side[1], 1, special->line_parts_, length, false);
    }
}

//
// Handles BOOM's line -> tagged sector transfers.
//
static void SectorEffect(Sector *target, Line *source, const LineType *special)
{
    if (!target)
        return;

    float    length  = PointToDistance(0, 0, source->delta_x, source->delta_y);
    BAMAngle angle   = kBAMAngle360 - PointToAngle(0, 0, -source->delta_x, -source->delta_y);
    bool     is_vert = fabs(source->delta_y) > fabs(source->delta_x);

    if (special->sector_effect_ & kSectorEffectTypeLightFloor)
        target->floor.override_properties = &source->front_sector->properties;

    if (special->sector_effect_ & kSectorEffectTypeLightCeiling)
        target->ceiling.override_properties = &source->front_sector->properties;

    if (special->sector_effect_ & kSectorEffectTypeScrollFloor ||
        special->sector_effect_ & kSectorEffectTypeScrollCeiling ||
        special->sector_effect_ & kSectorEffectTypePushThings)
    {
        SectorAnimation anim;
        anim.target = target;
        if (special->scroll_type_ == BoomScrollerTypeNone)
        {
            if (special->sector_effect_ & kSectorEffectTypeScrollFloor)
            {
                anim.floor_scroll.X -= source->delta_x / 32.0f;
                anim.floor_scroll.Y -= source->delta_y / 32.0f;
            }
            if (special->sector_effect_ & kSectorEffectTypeScrollCeiling)
            {
                anim.ceil_scroll.X -= source->delta_x / 32.0f;
                anim.ceil_scroll.Y -= source->delta_y / 32.0f;
            }
            if (special->sector_effect_ & kSectorEffectTypePushThings)
            {
                anim.push.X += source->delta_x / 32.0f * kBoomCarryFactor;
                anim.push.Y += source->delta_y / 32.0f * kBoomCarryFactor;
            }
        }
        else
        {
            // BOOM spec states that the front sector is the height reference
            // for displace/accel scrollers
            if (source->front_sector)
            {
                anim.scroll_sector_reference  = source->front_sector;
                anim.scroll_special_reference = special;
                anim.scroll_line_reference    = source;
                for (int i = 0; i < total_level_lines; i++)
                {
                    if (level_lines[i].tag == source->front_sector->tag)
                    {
                        if (!level_lines[i].special || level_lines[i].special->count_ == 1)
                            anim.permanent = true;
                    }
                }
                anim.last_height = anim.scroll_sector_reference->original_height;
            }
        }
        sector_animations.push_back(anim);
        AddSpecialSector(target);
    }

    if (special->sector_effect_ & kSectorEffectTypeSetFriction)
    {
        // TODO: this is not 100% correct, because the MSF_Friction flag is
        //       supposed to turn the custom friction on/off, but with this
        //       code, the custom value is either permanent or forgotten.
        if (target->properties.type & kBoomSectorFlagFriction)
        {
            if (length > 100)
                target->properties.friction = HMM_MIN(1.0f, 0.8125f + length / 1066.7f);
            else
                target->properties.friction = HMM_MAX(0.2f, length / 100.0f);
        }
    }

    if (special->sector_effect_ & kSectorEffectTypePointForce)
    {
        AddPointForce(target, length);
    }
    if (special->sector_effect_ & kSectorEffectTypeWindForce)
    {
        AddSectorForce(target, true /* is_wind */, source->delta_x, source->delta_y);
    }
    if (special->sector_effect_ & kSectorEffectTypeCurrentForce)
    {
        AddSectorForce(target, false /* is_wind */, source->delta_x, source->delta_y);
    }

    if (special->sector_effect_ & kSectorEffectTypeResetFloor)
    {
        target->floor.override_properties = nullptr;
        target->floor.scroll.X = target->floor.scroll.Y = 0;
        target->properties.push.X = target->properties.push.Y = target->properties.push.Z = 0;
    }
    if (special->sector_effect_ & kSectorEffectTypeResetCeiling)
    {
        target->ceiling.override_properties = nullptr;
        target->ceiling.scroll.X = target->ceiling.scroll.Y = 0;
    }

    // set texture alignment
    if (special->sector_effect_ & kSectorEffectTypeAlignFloor)
    {
        target->floor.offset.X = -source->vertex_1->X;
        target->floor.offset.Y = -source->vertex_1->Y;
        if (source->side[0]) // Lobo: Experiment to read and apply line offsets
                             // to floor offsets
        {
            target->floor.offset.X += source->side[0]->bottom.offset.X;
            target->floor.offset.Y += source->side[0]->bottom.offset.Y;
        }
        target->floor.rotation = angle;
    }
    if (special->sector_effect_ & kSectorEffectTypeAlignCeiling)
    {
        target->ceiling.offset.X = -source->vertex_1->X;
        target->ceiling.offset.Y = -source->vertex_1->Y;
        if (source->side[0]) // Lobo: Experiment to read and apply line offsets
                             // to floor offsets
        {
            target->ceiling.offset.X += source->side[0]->bottom.offset.X;
            target->ceiling.offset.Y += source->side[0]->bottom.offset.Y;
        }
        target->ceiling.rotation = angle;
    }

    // set texture scale
    if (special->sector_effect_ & kSectorEffectTypeScaleFloor)
    {
        bool  aligned = (special->sector_effect_ & kSectorEffectTypeAlignFloor) != 0;
        float factor  = ScaleFactorForPlane(target->floor, length, is_vert && !aligned);

        target->floor.x_matrix.X *= factor;
        target->floor.x_matrix.Y *= factor;
        target->floor.y_matrix.X *= factor;
        target->floor.y_matrix.Y *= factor;
    }
    if (special->sector_effect_ & kSectorEffectTypeScaleCeiling)
    {
        bool  aligned = (special->sector_effect_ & kSectorEffectTypeAlignCeiling) != 0;
        float factor  = ScaleFactorForPlane(target->ceiling, length, is_vert && !aligned);

        target->ceiling.x_matrix.X *= factor;
        target->ceiling.x_matrix.Y *= factor;
        target->ceiling.y_matrix.X *= factor;
        target->ceiling.y_matrix.Y *= factor;
    }

    // killough 3/7/98 and AJA 2022:
    // support for drawn heights coming from different sector
    if (special->sector_effect_ & kSectorEffectTypeBoomHeights)
    {
        target->height_sector      = source->front_sector;
        target->height_sector_side = source->side[0];
        // Quick band-aid fix for Line 242 "windows" - Dasho
        if (target->ceiling_height - target->floor_height < 1)
        {
            target->ceiling_height = source->front_sector->ceiling_height;
            target->floor_height   = source->front_sector->floor_height;
            for (int i = 0; i < target->line_count; i++)
            {
                if (target->lines[i]->side[1])
                {
                    target->lines[i]->blocked = false;
                    if (target->lines[i]->side[0]->middle.image && target->lines[i]->side[1]->middle.image &&
                        target->lines[i]->side[0]->middle.image == target->lines[i]->side[1]->middle.image)
                    {
                        target->lines[i]->side[0]->middle_mask_offset = 0;
                        target->lines[i]->side[1]->middle_mask_offset = 0;
                        for (Seg *seg = target->subsectors->segs; seg != nullptr; seg = seg->subsector_next)
                        {
                            if (seg->linedef == target->lines[i])
                                seg->linedef->flags |= kLineFlagLowerUnpegged;
                        }
                    }
                }
            }
        }
        else
        {
            for (int i = 0; i < target->line_count; i++)
            {
                if (target->lines[i]->side[1])
                    target->lines[i]->blocked = false;
            }
        }
    }
}

static void P_PortalEffect(Line *ld)
{
    // already linked?
    if (ld->portal_pair)
        return;

    if (ld->side[1])
    {
        LogWarning("Portal on line #%d disabled: Not one-sided!\n", (int)(ld - level_lines));
        return;
    }

    if (ld->special->portal_effect_ & kPortalEffectTypeMirror)
    {
        ld->flags |= kLineFlagMirror;
        return;
    }

    if (ld->tag <= 0)
    {
        LogWarning("Portal on line #%d disabled: Missing tag.\n", (int)(ld - level_lines));
        return;
    }

    bool is_camera = (ld->special->portal_effect_ & kPortalEffectTypeCamera) ? true : false;

    for (int i = 0; i < total_level_lines; i++)
    {
        Line *other = level_lines + i;

        if (other == ld)
            continue;

        if (other->tag != ld->tag)
            continue;

        float h1 = ld->front_sector->ceiling_height - ld->front_sector->floor_height;
        float h2 = other->front_sector->ceiling_height - other->front_sector->floor_height;

        if (h1 < 1 || h2 < 1)
        {
            LogWarning("Portal on line #%d disabled: sector is closed.\n", (int)(ld - level_lines));
            return;
        }

        if (is_camera)
        {
            // camera are much less restrictive than pass-able portals
            // (they are also one-way).

            ld->portal_pair = other;
            return;
        }

        if (other->portal_pair)
        {
            LogWarning("Portal on line #%d disabled: Partner already a portal.\n", (int)(ld - level_lines));
            return;
        }

        if (other->side[1])
        {
            LogWarning("Portal on line #%d disabled: Partner not one-sided.\n", (int)(ld - level_lines));
            return;
        }

        float h_ratio = h1 / h2;

        if (h_ratio < 0.95f || h_ratio > 1.05f)
        {
            LogWarning("Portal on line #%d disabled: Partner is different height.\n", (int)(ld - level_lines));
            return;
        }

        float len_ratio = ld->length / other->length;

        if (len_ratio < 0.95f || len_ratio > 1.05f)
        {
            LogWarning("Portal on line #%d disabled: Partner is different length.\n", (int)(ld - level_lines));
            return;
        }

        ld->portal_pair    = other;
        other->portal_pair = ld;

        // let renderer (etc) know the portal information
        other->special = ld->special;

        return; // Success !!
    }

    LogWarning("Portal on line #%d disabled: Cannot find partner!\n", (int)(ld - level_lines));
}

static SlopePlane *DetailSlope_BoundIt(Line *ld, Sector *sec, float dz1, float dz2)
{
    // determine slope's 2D coordinates
    float d_close = 0;
    float d_far   = 0;

    float nx = ld->delta_y / ld->length;
    float ny = -ld->delta_x / ld->length;

    if (sec == ld->back_sector)
    {
        nx = -nx;
        ny = -ny;
    }

    for (int k = 0; k < sec->line_count; k++)
    {
        for (int vert = 0; vert < 2; vert++)
        {
            Vertex *V = (vert == 0) ? sec->lines[k]->vertex_1 : sec->lines[k]->vertex_2;

            float dist = nx * (V->X - ld->vertex_1->X) + ny * (V->Y - ld->vertex_1->Y);

            d_close = HMM_MIN(d_close, dist);
            d_far   = HMM_MAX(d_far, dist);
        }
    }

    // LogDebug("DETAIL SLOPE in #%d: dists %1.3f -> %1.3f\n", (int)(sec -
    // level_sectors), d_close, d_far);

    if (d_far - d_close < 0.5)
    {
        LogWarning("Detail slope in sector #%d disabled: no area?!?\n", (int)(sec - level_sectors));
        return nullptr;
    }

    SlopePlane *result = new SlopePlane;

    result->x1       = ld->vertex_1->X + nx * d_close;
    result->y1       = ld->vertex_1->Y + ny * d_close;
    result->delta_z1 = dz1;

    result->x2       = ld->vertex_1->X + nx * d_far;
    result->y2       = ld->vertex_1->Y + ny * d_far;
    result->delta_z2 = dz2;

    return result;
}

static void DetailSlope_Floor(Line *ld)
{
    if (!ld->side[1])
    {
        LogWarning("Detail slope on line #%d disabled: Not two-sided!\n", (int)(ld - level_lines));
        return;
    }

    Sector *sec = ld->front_sector;

    float z1 = ld->back_sector->floor_height;
    float z2 = ld->front_sector->floor_height;

    if (fabs(z1 - z2) < 0.5)
    {
        LogWarning("Detail slope on line #%d disabled: floors are same height\n", (int)(ld - level_lines));
        return;
    }

    if (z1 > z2)
    {
        sec = ld->back_sector;

        z1 = ld->front_sector->floor_height;
        z2 = ld->back_sector->floor_height;
    }

    if (sec->floor_slope)
    {
        LogWarning("Detail slope in sector #%d disabled: floor already sloped!\n", (int)(sec - level_sectors));
        return;
    }

    ld->blocked = false;

    // limit height difference to no more than player step
    z1 = HMM_MAX(z1, z2 - 24.0);

    sec->floor_slope = DetailSlope_BoundIt(ld, sec, z1 - sec->floor_height, z2 - sec->floor_height);
}

static void DetailSlope_Ceiling(Line *ld)
{
    if (!ld->side[1])
        return;

    Sector *sec = ld->front_sector;

    float z1 = ld->front_sector->ceiling_height;
    float z2 = ld->back_sector->ceiling_height;

    if (fabs(z1 - z2) < 0.5)
    {
        LogWarning("Detail slope on line #%d disabled: ceilings are same height\n", (int)(ld - level_lines));
        return;
    }

    if (z1 > z2)
    {
        sec = ld->back_sector;

        z1 = ld->back_sector->ceiling_height;
        z2 = ld->front_sector->ceiling_height;
    }

    if (sec->ceiling_slope)
    {
        LogWarning("Detail slope in sector #%d disabled: ceiling already sloped!\n", (int)(sec - level_sectors));
        return;
    }

    ld->blocked = false;

    sec->ceiling_slope = DetailSlope_BoundIt(ld, sec, z2 - sec->ceiling_height, z1 - sec->ceiling_height);
}

//
// EVENTS
//
// Events are operations triggered by using, crossing,
// or shooting special lines, or by timed thinkers.
//

//
// P_ActivateSpecialLine
//
// Called when a special line is activated.
//
// line is the line to be activated, side is the side activated from,
// (as lines can only be activated from the right), thing is the thing
// activating, to check for player/monster only lines trig is how it
// was activated, ie shot/crossed/pushed.  `line' can be nullptr for
// non-line activations.
//
// -KM- 1998/09/01 Procedure Written.
//
// -ACB- 1998/09/11 Return Success or Failure.
//
// -AJA- 1999/09/29: Updated for new tagged sector links.
//
// -AJA- 1999/10/21: Allow non-line activation (line == nullptr), and
//                   added `typenum' and `tag' parameter.
//
// -AJA- 2000/01/02: New trigger method `kLineTriggerAny'.
//
// -ACB- 2001/01/14: Added Elevator Sector Type
//
static bool P_ActivateSpecialLine(Line *line, const LineType *special, int tag, int side, MapObject *thing,
                                  LineTrigger trig, int can_reach, int no_care_who)
{
    bool texSwitch   = false;
    bool playedSound = false;

    SoundEffect *sfx[4];
    Sector      *tsec;

    int i;

#ifdef DEVELOPERS
    if (!special)
    {
        if (line == nullptr)
            FatalError("P_ActivateSpecialLine: Special type is 0\n");
        else
            FatalError("P_ActivateSpecialLine: Line %d is not Special\n", (int)(line - lines));
    }
#endif

    if (!CheckWhenAppear(special->appear_))
    {
        if (line)
            line->special = nullptr;

        return true;
    }

    if (trig != kLineTriggerAny && special->type_ != trig &&
        !(special->type_ == kLineTriggerManual && trig == kLineTriggerPushable))
        return false;

    // Check for use once.
    if (line && line->count == 0)
        return false;

    // Single sided line
    if (trig != kLineTriggerAny && special->singlesided_ && side == 1)
        return false;

    // -AJA- 1999/12/07: Height checking.
    if (line && thing && thing->player_ && (special->special_flags_ & kLineSpecialMustReach) && !can_reach)
    {
        StartSoundEffect(thing->info_->noway_sound_, GetSoundEffectCategory(thing), thing);

        return false;
    }

    // Check this type of thing can trigger
    if (!no_care_who)
    {
        if (thing && thing->player_)
        {
            // Players can only trigger if the kTriggerActivatorPlayer is set
            if (!(special->obj_ & kTriggerActivatorPlayer))
                return false;

            if (thing->player_->IsBot() && (special->obj_ & kTriggerActivatorNoBot))
                return false;
        }
        else if (thing && (thing->info_->extended_flags_ & kExtendedFlagMonster))
        {
            // Monsters can only trigger if the kTriggerActivatorMonster flag is
            // set
            if (!(special->obj_ & kTriggerActivatorMonster))
                return false;

            // Monsters don't trigger secrets
            if (line && (line->flags & kLineFlagSecret))
                return false;
            
            // Monster is not allowed to trigger lines
            if (thing->info_->hyper_flags_ & kHyperFlagNoTriggerLines)
            {
                //Except maybe teleporters
                if (special->t_.teleport_)
                {
                    if (!(thing->info_->hyper_flags_ & kHyperFlagTriggerTeleports))
                    {
                        return false;  
                    }
                }
                else
                {
                     return false;         
                }
            }
        }
        else
        {
            // Other stuff can only trigger if kTriggerActivatorOther is set
            if (!(special->obj_ & kTriggerActivatorOther))
                return false;

            // Other stuff doesn't trigger secrets
            if (line && (line->flags & kLineFlagSecret))
                return false;
        }
    }

    // Don't let monsters activate crossable special lines that they
    // wouldn't otherwise cross (for now, the edge of a high dropoff)
    // Note: I believe this assumes no 3D floors, but I think it's a
    // very particular situation anyway - Dasho
    if (trig == kLineTriggerWalkable && line->back_sector && thing &&
        (thing->info_->extended_flags_ & kExtendedFlagMonster) &&
        !(thing->flags_ & (kMapObjectFlagTeleport | kMapObjectFlagDropOff | kMapObjectFlagFloat)))
    {
        if (std::abs(line->front_sector->floor_height - line->back_sector->floor_height) > thing->info_->step_size_)
            return false;
    }

    if (thing && !no_care_who)
    {
        // Check for keys
        // -ACB- 1998/09/11 Key possibilites extended
        if (special->keys_ != kDoorKeyNone)
        {
            DoorKeyType req = (DoorKeyType)(special->keys_ & kDoorKeyBitmask);
            DoorKeyType cards;

            // Monsters/Missiles have no keys
            if (!thing->player_)
                return false;

            //
            // New Security Checks, allows for any combination of keys in
            // an AND or OR function. Therefore it extends the possibilities
            // of security above 3 possible combinations..
            //
            // -AJA- Reworked this for the 10 new keys.
            //
            cards = thing->player_->cards_;

            bool failedsecurity = false;

            if (special->keys_ & kDoorKeyCardOrSkull)
            {
                // Boom compatibility: treat card and skull types the same
                cards = (DoorKeyType)(ExpandKeyBits(cards));
            }

            if (special->keys_ & kDoorKeyStrictlyAllKeys)
            {
                if ((cards & req) != req)
                    failedsecurity = true;
            }
            else
            {
                if ((cards & req) == 0)
                    failedsecurity = true;
            }

            if (failedsecurity)
            {
                if (special->failedmessage_ != "")
                    ImportantConsoleMessageLDF(special->failedmessage_.c_str());
                    //PlayerConsoleMessageLDF(thing->player_->player_number_, special->failedmessage_.c_str());

                if (special->failed_sfx_)
                    StartSoundEffect(special->failed_sfx_, kCategoryLevel, thing);

                return false;
            }
        }
    }

    // Check if button already pressed
    if (line && ButtonIsPressed(line))
        return false;

    // Tagged line effect_object
    if (line && special->effectobject_)
    {
        if (!tag)
        {
            P_SpawnLineEffectDebris(line, special);
        }
        else
        {
            for (i = 0; i < total_level_lines; i++)
            {
                if (level_lines[i].tag == tag)
                {
                    P_SpawnLineEffectDebris(level_lines + i, special);
                }
            }
        }
    }

    // Do lights
    // -KM- 1998/09/27 Generalised light types.
    switch (special->l_.type_)
    {
    case kLightSpecialTypeSet:
        RunLineTagLights(tag, special->l_.level_);
        texSwitch = true;
        break;

    case kLightSpecialTypeNone:
        break;

    default:
        texSwitch = DoSectorsFromTag(tag, &special->l_, nullptr, DoLightsWrapper);
        break;
    }

    // -ACB- 1998/09/13 Use teleport define..
    if (special->t_.teleport_)
    {
        texSwitch = TeleportMapObject(line, tag, thing, &special->t_);
    }

    if (special->e_exit_ == kExitTypeNormal)
    {
        ExitLevel(5);
        texSwitch = true;
    }
    else if (special->e_exit_ == kExitTypeSecret)
    {
        ExitLevelSecret(5);
        texSwitch = true;
    }
    else if (special->e_exit_ == kExitTypeHub)
    {
        ExitToHub(special->hub_exit_, line ? line->tag : tag);
        texSwitch = true;
    }

    if (special->d_.dodonut_)
    {
        // Proper ANSI C++ Init
        sfx[0] = special->d_.d_sfxout_;
        sfx[1] = special->d_.d_sfxoutstop_;
        sfx[2] = special->d_.d_sfxin_;
        sfx[3] = special->d_.d_sfxinstop_;

        texSwitch = DoSectorsFromTag(tag, nullptr, sfx, DoDonutWrapper);
    }

    //
    // - Plats/Floors -
    //
    if (special->f_.type_ != kPlaneMoverUndefined)
    {
        if (!tag || special->type_ == kLineTriggerManual)
        {
            if (line)
                texSwitch = RunManualPlaneMover(line, thing, &special->f_);
        }
        else
        {
            texSwitch = DoSectorsFromTag(tag, &special->f_, line ? line->front_sector : nullptr, DoPlaneWrapper);
        }
    }

    //
    // - Doors/Ceilings -
    //
    if (special->c_.type_ != kPlaneMoverUndefined)
    {
        if (!tag || special->type_ == kLineTriggerManual)
        {
            if (line)
                texSwitch = RunManualPlaneMover(line, thing, &special->c_);
        }
        else
        {
            texSwitch = DoSectorsFromTag(tag, &special->c_, line ? line->front_sector : nullptr, DoPlaneWrapper);
        }
    }

    //
    // - Thin Sliding Doors -
    //
    if (special->s_.type_ != kSlidingDoorTypeNone)
    {
        if (line && (!tag || special->type_ == kLineTriggerManual))
        {
            RunSlidingDoor(line, line, thing, special);
            texSwitch = false;

            // Must handle line count here, since the normal code in p_spec.c
            // will clear the line->special pointer, confusing various bits of
            // code that deal with sliding doors (--> crash).
            if (line->count > 0)
                line->count--;

            return true;
        }
        else if (tag)
        {
            for (i = 0; i < total_level_lines; i++)
            {
                Line *other = level_lines + i;

                if (other->tag == tag && other != line)
                    if (RunSlidingDoor(other, line, thing, special))
                        texSwitch = true;
            }
        }
    }

    if (special->use_colourmap_ && tag > 0)
    {
        for (tsec = FindSectorFromTag(tag); tsec; tsec = tsec->tag_next)
        {
            tsec->properties.colourmap = special->use_colourmap_;
            texSwitch                  = true;
        }
    }

    if (!AlmostEquals(special->gravity_, kFloatUnused) && tag > 0)
    {
        for (tsec = FindSectorFromTag(tag); tsec; tsec = tsec->tag_next)
        {
            tsec->properties.gravity = special->gravity_;
            texSwitch                = true;
        }
    }

    if (!AlmostEquals(special->friction_, kFloatUnused) && tag > 0)
    {
        for (tsec = FindSectorFromTag(tag); tsec; tsec = tsec->tag_next)
        {
            tsec->properties.friction = special->friction_;
            texSwitch                 = true;
        }
    }

    if (!AlmostEquals(special->viscosity_, kFloatUnused) && tag > 0)
    {
        for (tsec = FindSectorFromTag(tag); tsec; tsec = tsec->tag_next)
        {
            tsec->properties.viscosity = special->viscosity_;
            texSwitch                  = true;
        }
    }

    if (!AlmostEquals(special->drag_, kFloatUnused) && tag > 0)
    {
        for (tsec = FindSectorFromTag(tag); tsec; tsec = tsec->tag_next)
        {
            tsec->properties.drag = special->drag_;
            texSwitch             = true;
        }
    }

    // Tagged line effects
    if (line && special->line_effect_)
    {
        if (!tag)
        {
            P_LineEffect(line, line, special);
            texSwitch = true;
        }
        else
        {
            for (i = 0; i < total_level_lines; i++)
            {
                if (level_lines[i].tag == tag && &level_lines[i] != line)
                {
                    P_LineEffect(level_lines + i, line, special);
                    texSwitch = true;
                }
            }
        }
    }

    // Tagged sector effects
    if (line && special->sector_effect_)
    {
        if (!tag)
        {
            if (special->special_flags_ & kLineSpecialBackSector)
                SectorEffect(line->back_sector, line, special);
            else
                SectorEffect(line->front_sector, line, special);

            texSwitch = true;
        }
        else
        {
            for (tsec = FindSectorFromTag(tag); tsec; tsec = tsec->tag_next)
            {
                SectorEffect(tsec, line, special);
                texSwitch = true;
            }
        }
    }

    if (special->trigger_effect_ && tag > 0)
    {
        ScriptEnableByTag(thing, tag, special->trigger_effect_ < 0, kTriggerTagNumber);
        texSwitch = true;
    }

    if (special->ambient_sfx_ && tag > 0)
    {
        for (tsec = FindSectorFromTag(tag); tsec; tsec = tsec->tag_next)
        {
            AddAmbientSounds(tsec, special->ambient_sfx_);
            texSwitch = true;
        }
    }

    if (special->music_)
    {
        ChangeMusic(special->music_, true);
        texSwitch = true;
    }

    if (special->activate_sfx_)
    {
        if (line)
        {
            StartSoundEffect(special->activate_sfx_, kCategoryLevel, &line->front_sector->sound_effects_origin);
        }
        else if (thing)
        {
            StartSoundEffect(special->activate_sfx_, GetSoundEffectCategory(thing), thing);
        }

        playedSound = true;
    }

    // reduce count & clear special if necessary
    if (line && texSwitch)
    {
        if (line->count != -1)
        {
            line->count--;

            if (!line->count)
                line->special = nullptr;
        }
        // -KM- 1998/09/27 Reversable linedefs.
        if (line->special && special->newtrignum_)
        {
            line->special = (special->newtrignum_ <= 0) ? nullptr : LookupLineType(special->newtrignum_);
        }

        ChangeSwitchTexture(line, line->special && (special->newtrignum_ == 0), special->special_flags_, playedSound);
    }
    return true;
}

//
// Called every time a thing origin is about
// to cross a line with a non-zero special.
//
// -KM- 1998/09/01 Now much simpler
// -ACB- 1998/09/12 Return success/failure
//
bool CrossSpecialLine(Line *ld, int side, MapObject *thing)
{
    return P_ActivateSpecialLine(ld, ld->special, ld->tag, side, thing, kLineTriggerWalkable, 1, 0);
}

//
// Called when a thing shoots a special line.
//
void ShootSpecialLine(Line *ld, int side, MapObject *thing)
{
    P_ActivateSpecialLine(ld, ld->special, ld->tag, side, thing, kLineTriggerShootable, 1, 0);
}

//
// Called when a thing uses a special line.
// Only the front sides of lines are usable.
//
// -KM- 1998/09/01 Uses new lines.ddf code in p_spec.c
//
// -ACB- 1998/09/07 Uses the return value to discern if a move if possible.
//
// -AJA- 1999/12/07: New parameters `open_bottom' and `open_top',
//       which give a vertical range through which the linedef is
//       accessible.  Could be used for smarter switches, like one on
//       a lower wall-part which is out of reach (e.g. MAP02).
//
bool UseSpecialLine(MapObject *thing, Line *line, int side, float open_bottom, float open_top)
{
    int can_reach = (thing->z < open_top) && (thing->z + thing->height_ + kUseZRange >= open_bottom);

    return P_ActivateSpecialLine(line, line->special, line->tag, side, thing, kLineTriggerPushable, can_reach, 0);
}

//
// Called by the RTS `ACTIVATE_LINETYPE' primitive, and also the code
// pointer in things.ddf of the same name.  Thing can be nullptr.
//
// -AJA- 1999/10/21: written.
//
void RemoteActivation(MapObject *thing, int typenum, int tag, int side, LineTrigger method)
{
    const LineType *spec = LookupLineType(typenum);

    P_ActivateSpecialLine(nullptr, spec, tag, side, thing, method, 1, (thing == nullptr));
}

static inline void PlayerInProperties(Player *player, float bz, float tz, float floor_height, float ceiling_height,
                                      RegionProperties *props, const SectorType **swim_special,
                                      bool should_choke = true)
{
    const SectorType *special = props->special;
    float             damage, factor;

    bool extra_tic = ((game_tic & 1) == 1);

    if (!special || ceiling_height < floor_height)
        return;

    if (!CheckWhenAppear(special->appear_))
        return;

    // breathing support
    // (Mouth is where the eye is !)
    //
    float mouth_z = player->map_object_->z + player->view_z_;

    if ((special->special_flags_ & kSectorFlagAirLess) && mouth_z >= floor_height && mouth_z <= ceiling_height &&
        player->powers_[kPowerTypeScuba] <= 0)
    {
        int subtract = 1;
        if ((double_framerate.d_ && extra_tic) || !should_choke)
            subtract = 0;
        player->air_in_lungs_ -= subtract;
        player->underwater_ = true;

        if (subtract && player->air_in_lungs_ <= 0 &&
            (level_time_elapsed % (1 + player->map_object_->info_->choke_damage_.delay_)) == 0)
        {
            EDGE_DAMAGE_COMPUTE(damage, &player->map_object_->info_->choke_damage_);

            if (damage)
                DamageMapObject(player->map_object_, nullptr, nullptr, damage,
                                &player->map_object_->info_->choke_damage_);
        }
    }

    if ((special->special_flags_ & kSectorFlagAirLess) && mouth_z >= floor_height && mouth_z <= ceiling_height)
    {
        player->airless_ = true;
    }

    if ((special->special_flags_ & kSectorFlagSwimming) && mouth_z >= floor_height && mouth_z <= ceiling_height)
    {
        player->swimming_ = true;
        *swim_special     = special;
        if (special->special_flags_ & kSectorFlagSubmergedSFX)
            submerged_sound_effects = true;
    }

    if ((special->special_flags_ & kSectorFlagSwimming) && player->map_object_->z >= floor_height &&
        player->map_object_->z <= ceiling_height)
    {
        player->wet_feet_ = true;
        HitLiquidFloor(player->map_object_);
    }

    if (special->special_flags_ & kSectorFlagVacuumSFX)
        vacuum_sound_effects = true;

    if (special->special_flags_ & kSectorFlagReverbSFX)
    {
        ddf_reverb = true;
        if (epi::StringCaseCompareASCII(special->reverb_type_, "REVERB") == 0)
            ddf_reverb_type = 1;
        else if (epi::StringCaseCompareASCII(special->reverb_type_, "ECHO") == 0)
            ddf_reverb_type = 2;
        ddf_reverb_delay = HMM_MAX(0, special->reverb_delay_);
        ddf_reverb_ratio = HMM_Clamp(0, special->reverb_ratio_, 100);
    }

    factor = 1.0f;

    if (special->special_flags_ & kSectorFlagWholeRegion)
    {
        if (special->special_flags_ & kSectorFlagProportional)
        {
            // only partially in region -- mitigate damage
            if (tz > ceiling_height)
                factor -= factor * (tz - ceiling_height) / (tz - bz);

            if (bz < floor_height)
                factor -= factor * (floor_height - bz) / (tz - bz);
        }
        else
        {
            if (bz > ceiling_height || tz < floor_height)
                factor = 0;
        }
    }
    else
    {
        // Not touching the floor ?
        if (player->map_object_->z > floor_height + 2.0f)
            return;
    }

    // Check for DAMAGE_UNLESS/DAMAGE_IF DDF specials
    if (special->damage_.damage_unless_ || special->damage_.damage_if_)
    {
        bool unless_damage = (special->damage_.damage_unless_ != nullptr);
        bool if_damage     = false;
        if (special->damage_.damage_unless_ && HasBenefitInList(player, special->damage_.damage_unless_))
            unless_damage = false;
        if (special->damage_.damage_if_ && HasBenefitInList(player, special->damage_.damage_if_))
            if_damage = true;
        if (!unless_damage && !if_damage && !special->damage_.bypass_all_)
            factor = 0;
    }
    else if (player->powers_[kPowerTypeAcidSuit] && !special->damage_.bypass_all_)
        factor = 0;

    if (double_framerate.d_ && extra_tic)
        factor = 0;

    if (factor > 0 && (level_time_elapsed % (1 + special->damage_.delay_)) == 0)
    {
        EDGE_DAMAGE_COMPUTE(damage, &special->damage_);

        if (damage || special->damage_.instakill_)
            DamageMapObject(player->map_object_, nullptr, nullptr, damage * factor, &special->damage_);
    }

    if (special->secret_ && !props->secret_found)
    {
        player->secret_count_++;

        if (!InDeathmatch())
        {
            ImportantConsoleMessageLDF("FoundSecret"); // Lobo: get text from language.ddf

            StartSoundEffect(player->map_object_->info_->secretsound_, kCategoryUi, player->map_object_);
            // StartSoundEffect(player->map_object_->info_->secretsound_,
            //		P_MobjGetSfxCategory(player->map_object_),
            // player->map_object_);
        }

        props->secret_found = true;
    }

    if (special->e_exit_ != kExitTypeNone)
    {
        player->cheats_ &= ~kCheatingGodMode;

        if (player->health_ < (player->map_object_->spawn_health_ * 0.11f))
        {
            // -KM- 1998/12/16 We don't want to alter the special type,
            //   modify the sector's attributes instead.
            props->special = nullptr;

            if (special->e_exit_ == kExitTypeSecret)
                ExitLevelSecret(1);
            else
                ExitLevel(1);
        }
    }
}

//
// Called every tic frame that the player origin is in a special sector
//
// -KM- 1998/09/27 Generalised for sectors.ddf
// -AJA- 1999/10/09: Updated for new sector handling.
//
void PlayerInSpecialSector(Player *player, Sector *sec, bool should_choke)
{
    Extrafloor *S, *L, *C;
    float       floor_h;
    float       ceil_h;

    float bz = player->map_object_->z;
    float tz = player->map_object_->z + player->map_object_->height_;

    bool was_underwater = player->underwater_;
    bool was_airless    = player->airless_;
    bool was_swimming   = player->swimming_;

    const SectorType *swim_special = nullptr;

    player->swimming_   = false;
    player->underwater_ = false;
    player->airless_    = false;
    player->wet_feet_   = false;

    // traverse extrafloor list
    floor_h = sec->floor_height;
    ceil_h  = sec->ceiling_height;

    S = sec->bottom_extrafloor;
    L = sec->bottom_liquid;

    while (S || L)
    {
        if (!L || (S && S->bottom_height < L->bottom_height))
        {
            C = S;
            S = S->higher;
        }
        else
        {
            C = L;
            L = L->higher;
        }

        EPI_ASSERT(C);

        // ignore "hidden" liquids
        if (C->bottom_height < floor_h || C->bottom_height > sec->ceiling_height)
            continue;

        PlayerInProperties(player, bz, tz, floor_h, C->top_height, C->properties, &swim_special, should_choke);

        floor_h = C->top_height;
    }

    if (sec->floor_vertex_slope)
        floor_h = player->map_object_->floor_z_;

    if (sec->ceiling_vertex_slope)
        ceil_h = player->map_object_->ceiling_z_;

    PlayerInProperties(player, bz, tz, floor_h, ceil_h, sec->active_properties, &swim_special, should_choke);

    // breathing support: handle gasping when leaving the water
    if ((was_underwater && !player->underwater_) || (was_airless && !player->airless_))
    {
        if (player->air_in_lungs_ <=
            (player->map_object_->info_->lung_capacity_ - player->map_object_->info_->gasp_start_))
        {
            if (player->map_object_->info_->gasp_sound_)
            {
                StartSoundEffect(player->map_object_->info_->gasp_sound_, GetSoundEffectCategory(player->map_object_),
                                 player->map_object_);
            }
        }

        player->air_in_lungs_ = player->map_object_->info_->lung_capacity_;
    }

    // -AJA- 2008/01/20: water splash sounds for players
    if (!was_swimming && player->swimming_)
    {
        EPI_ASSERT(swim_special);

        if (player->splash_wait_ == 0 && swim_special->splash_sfx_)
        {
            // StartSoundEffect(swim_special->splash_sfx, kCategoryUi,
            // player->map_object_);
            StartSoundEffect(swim_special->splash_sfx_, GetSoundEffectCategory(player->map_object_),
                             player->map_object_);

            HitLiquidFloor(player->map_object_);
        }
    }
    else if (was_swimming && !player->swimming_)
    {
        player->splash_wait_ = kTicRate;
    }
}

//
// Animate planes, scroll walls, etc.
//
void UpdateSpecials(bool extra_tic)
{
    // For anim stuff
    float factor = double_framerate.d_ ? 0.5f : 1.0f;

    // LEVEL TIMER
    if (level_timer == true)
    {
        level_time_count -= (double_framerate.d_ && extra_tic) ? 0 : 1;

        if (!level_time_count)
            ExitLevel(1);
    }

    for (size_t i = 0; i < light_animations.size(); i++)
    {
        struct Sector *sec_ref  = light_animations[i].light_sector_reference;
        Line          *line_ref = light_animations[i].light_line_reference;

        if (!sec_ref || !line_ref)
            continue;

        // Only do "normal" (raising) doors for now
        if (sec_ref->ceiling_move && sec_ref->ceiling_move->destination_height > sec_ref->ceiling_move->start_height)
        {
            float ratio = (sec_ref->ceiling_height - sec_ref->ceiling_move->start_height) /
                          (sec_ref->ceiling_move->destination_height - sec_ref->ceiling_move->start_height);
            for (Sector *tsec = FindSectorFromTag(light_animations[i].light_line_reference->tag); tsec;
                 tsec         = tsec->tag_next)
            {
                tsec->properties.light_level = (tsec->maximum_neighbor_light - tsec->minimum_neighbor_light) * ratio +
                                               tsec->minimum_neighbor_light;
            }
        }
    }

    if (active_line_animations.size() > 0)
    {
        // Calculate net offset/scroll/push for walls
        for (size_t i = 0; i < line_animations.size(); i++)
        {
            Line *ld = line_animations[i].target;
            if (!ld)
                continue;

            // Add static values
            if (ld->side[0])
            {
                if (ld->side[0]->top.image)
                {
                    ld->side[0]->top.net_scroll.X += line_animations[i].side_0_x_speed;
                    ld->side[0]->top.net_scroll.Y += line_animations[i].side_0_y_speed;
                }
                if (ld->side[0]->middle.image)
                {
                    ld->side[0]->middle.net_scroll.X += line_animations[i].side_0_x_speed;
                    ld->side[0]->middle.net_scroll.Y += line_animations[i].side_0_y_speed;
                }
                if (ld->side[0]->bottom.image)
                {
                    ld->side[0]->bottom.net_scroll.X += line_animations[i].side_0_x_speed;
                    ld->side[0]->bottom.net_scroll.Y += line_animations[i].side_0_y_speed;
                }
            }
            if (ld->side[1])
            {
                if (ld->side[1]->top.image)
                {
                    ld->side[1]->top.net_scroll.X += line_animations[i].side_1_x_speed;
                    ld->side[1]->top.net_scroll.Y += line_animations[i].side_1_y_speed;
                }
                if (ld->side[1]->middle.image)
                {
                    ld->side[1]->middle.net_scroll.X += line_animations[i].side_1_x_speed;
                    ld->side[1]->middle.net_scroll.Y += line_animations[i].side_1_y_speed;
                }
                if (ld->side[1]->bottom.image)
                {
                    ld->side[1]->bottom.net_scroll.X += line_animations[i].side_1_x_speed;
                    ld->side[1]->bottom.net_scroll.Y += line_animations[i].side_1_y_speed;
                }
            }

            // Update dynamic values
            struct Sector  *sec_ref     = line_animations[i].scroll_sector_reference;
            const LineType *special_ref = line_animations[i].scroll_special_reference;
            Line           *line_ref    = line_animations[i].scroll_line_reference;

            if (!sec_ref || !special_ref || !line_ref)
                continue;

            if (special_ref->line_effect_ & kLineEffectTypeVectorScroll)
            {
                float tdx       = line_animations[i].dynamic_delta_x;
                float tdy       = line_animations[i].dynamic_delta_y;
                float heightref = special_ref->scroll_type_ & BoomScrollerTypeDisplace ? line_animations[i].last_height
                                                                                       : sec_ref->original_height;
                float sy        = tdy * ((sec_ref->floor_height + sec_ref->ceiling_height) - heightref);
                float sx        = tdx * ((sec_ref->floor_height + sec_ref->ceiling_height) - heightref);
                if (double_framerate.d_ && special_ref->scroll_type_ & BoomScrollerTypeDisplace)
                {
                    sy *= 2;
                    sx *= 2;
                }
                if (ld->side[0])
                {
                    if (ld->side[0]->top.image)
                    {
                        ld->side[0]->top.net_scroll.X += sx;
                        ld->side[0]->top.net_scroll.Y += sy;
                    }
                    if (ld->side[0]->middle.image)
                    {
                        ld->side[0]->middle.net_scroll.X += sx;
                        ld->side[0]->middle.net_scroll.Y += sy;
                    }
                    if (ld->side[0]->bottom.image)
                    {
                        ld->side[0]->bottom.net_scroll.X += sx;
                        ld->side[0]->bottom.net_scroll.Y += sy;
                    }
                }
                if (ld->side[1])
                {
                    if (ld->side[1]->top.image)
                    {
                        ld->side[1]->top.net_scroll.X += sx;
                        ld->side[1]->top.net_scroll.Y += sy;
                    }
                    if (ld->side[1]->middle.image)
                    {
                        ld->side[1]->middle.net_scroll.X += sx;
                        ld->side[1]->middle.net_scroll.Y += sy;
                    }
                    if (ld->side[1]->bottom.image)
                    {
                        ld->side[1]->bottom.net_scroll.X += sx;
                        ld->side[1]->bottom.net_scroll.Y += sy;
                    }
                }
            }
            if (special_ref->line_effect_ & kLineEffectTypeTaggedOffsetScroll)
            {
                float x_speed   = line_animations[i].side_0_x_offset_speed;
                float y_speed   = line_animations[i].side_0_y_offset_speed;
                float heightref = special_ref->scroll_type_ & BoomScrollerTypeDisplace ? line_animations[i].last_height
                                                                                       : sec_ref->original_height;
                float sy        = x_speed * ((sec_ref->floor_height + sec_ref->ceiling_height) - heightref);
                float sx        = y_speed * ((sec_ref->floor_height + sec_ref->ceiling_height) - heightref);
                if (double_framerate.d_ && special_ref->scroll_type_ & BoomScrollerTypeDisplace)
                {
                    sy *= 2;
                    sx *= 2;
                }
                if (ld->side[0])
                {
                    if (ld->side[0]->top.image)
                    {
                        ld->side[0]->top.net_scroll.X += sx;
                        ld->side[0]->top.net_scroll.Y += sy;
                    }
                    if (ld->side[0]->middle.image)
                    {
                        ld->side[0]->middle.net_scroll.X += sx;
                        ld->side[0]->middle.net_scroll.Y += sy;
                    }
                    if (ld->side[0]->bottom.image)
                    {
                        ld->side[0]->bottom.net_scroll.X += sx;
                        ld->side[0]->bottom.net_scroll.Y += sy;
                    }
                }
            }
            line_animations[i].last_height = sec_ref->floor_height + sec_ref->ceiling_height;
        }
    }

    // ANIMATE LINE SPECIALS
    // -KM- 1998/09/01 Lines.ddf
    std::list<Line *>::iterator LI;

    for (LI = active_line_animations.begin(); LI != active_line_animations.end(); LI++)
    {
        Line *ld = *LI;

        if (!ld->old_stored)
        {
            if (ld->side[0])
            {
                if (ld->side[0]->top.image)
                {
                    ld->side[0]->top.old_scroll.X = ld->side[0]->top.scroll.X;
                    ld->side[0]->top.old_scroll.Y = ld->side[0]->top.scroll.Y;
                }
                if (ld->side[0]->middle.image)
                {
                    ld->side[0]->middle.old_scroll.X = ld->side[0]->middle.scroll.X;
                    ld->side[0]->middle.old_scroll.Y = ld->side[0]->middle.scroll.Y;
                }
                if (ld->side[0]->bottom.image)
                {
                    ld->side[0]->bottom.old_scroll.X = ld->side[0]->bottom.scroll.X;
                    ld->side[0]->bottom.old_scroll.Y = ld->side[0]->bottom.scroll.Y;
                }
            }
            if (ld->side[1])
            {
                if (ld->side[1]->top.image)
                {
                    ld->side[1]->top.old_scroll.X = ld->side[1]->top.scroll.X;
                    ld->side[1]->top.old_scroll.Y = ld->side[1]->top.scroll.Y;
                }
                if (ld->side[1]->middle.image)
                {
                    ld->side[1]->middle.old_scroll.X = ld->side[1]->middle.scroll.X;
                    ld->side[1]->middle.old_scroll.Y = ld->side[1]->middle.scroll.Y;
                }
                if (ld->side[1]->bottom.image)
                {
                    ld->side[1]->bottom.old_scroll.X = ld->side[1]->bottom.scroll.X;
                    ld->side[1]->bottom.old_scroll.Y = ld->side[1]->bottom.scroll.Y;
                }
            }
            ld->old_stored = true;
        }
        else
        {
            if (ld->side[0])
            {
                if (ld->side[0]->top.image)
                {
                    ld->side[0]->top.scroll.X = ld->side[0]->top.old_scroll.X;
                    ld->side[0]->top.scroll.Y = ld->side[0]->top.old_scroll.Y;
                }
                if (ld->side[0]->middle.image)
                {
                    ld->side[0]->middle.scroll.X = ld->side[0]->middle.old_scroll.X;
                    ld->side[0]->middle.scroll.Y = ld->side[0]->middle.old_scroll.Y;
                }
                if (ld->side[0]->bottom.image)
                {
                    ld->side[0]->bottom.scroll.X = ld->side[0]->bottom.old_scroll.X;
                    ld->side[0]->bottom.scroll.Y = ld->side[0]->bottom.old_scroll.Y;
                }
            }
            if (ld->side[1])
            {
                if (ld->side[1]->top.image)
                {
                    ld->side[1]->top.scroll.X = ld->side[1]->top.old_scroll.X;
                    ld->side[1]->top.scroll.Y = ld->side[1]->top.old_scroll.Y;
                }
                if (ld->side[1]->middle.image)
                {
                    ld->side[1]->middle.scroll.X = ld->side[1]->middle.old_scroll.X;
                    ld->side[1]->middle.scroll.Y = ld->side[1]->middle.old_scroll.Y;
                }
                if (ld->side[1]->bottom.image)
                {
                    ld->side[1]->bottom.scroll.X = ld->side[1]->bottom.old_scroll.X;
                    ld->side[1]->bottom.scroll.Y = ld->side[1]->bottom.old_scroll.Y;
                }
            }
        }

        // -KM- 1999/01/31 Use new method.
        // -AJA- 1999/07/01: Handle both sidedefs.
        if (ld->side[0])
        {
            if (ld->side[0]->top.image)
            {
                ld->side[0]->top.offset.X = fmod(
                    ld->side[0]->top.offset.X + (ld->side[0]->top.scroll.X + ld->side[0]->top.net_scroll.X) * factor,
                    ld->side[0]->top.image->actual_width_);
                ld->side[0]->top.offset.Y = fmod(
                    ld->side[0]->top.offset.Y + (ld->side[0]->top.scroll.Y + ld->side[0]->top.net_scroll.Y) * factor,
                    ld->side[0]->top.image->actual_height_);
                ld->side[0]->top.net_scroll = {{0, 0}};
            }
            if (ld->side[0]->middle.image)
            {
                ld->side[0]->middle.offset.X =
                    fmod(ld->side[0]->middle.offset.X +
                             (ld->side[0]->middle.scroll.X + ld->side[0]->middle.net_scroll.X) * factor,
                         ld->side[0]->middle.image->actual_width_);
                ld->side[0]->middle.offset.Y =
                    fmod(ld->side[0]->middle.offset.Y +
                             (ld->side[0]->middle.scroll.Y + ld->side[0]->middle.net_scroll.Y) * factor,
                         ld->side[0]->middle.image->actual_height_);
                ld->side[0]->middle.net_scroll = {{0, 0}};
            }
            if (ld->side[0]->bottom.image)
            {
                ld->side[0]->bottom.offset.X =
                    fmod(ld->side[0]->bottom.offset.X +
                             (ld->side[0]->bottom.scroll.X + ld->side[0]->bottom.net_scroll.X) * factor,
                         ld->side[0]->bottom.image->actual_width_);
                ld->side[0]->bottom.offset.Y =
                    fmod(ld->side[0]->bottom.offset.Y +
                             (ld->side[0]->bottom.scroll.Y + ld->side[0]->bottom.net_scroll.Y) * factor,
                         ld->side[0]->bottom.image->actual_height_);
                ld->side[0]->bottom.net_scroll = {{0, 0}};
            }
        }

        if (ld->side[1])
        {
            if (ld->side[1]->top.image)
            {
                ld->side[1]->top.offset.X = fmod(
                    ld->side[1]->top.offset.X + (ld->side[1]->top.scroll.X + ld->side[1]->top.net_scroll.X) * factor,
                    ld->side[1]->top.image->actual_width_);
                ld->side[1]->top.offset.Y = fmod(
                    ld->side[1]->top.offset.Y + (ld->side[1]->top.scroll.Y + ld->side[1]->top.net_scroll.Y) * factor,
                    ld->side[1]->top.image->actual_height_);
                ld->side[1]->top.net_scroll = {{0, 0}};
            }
            if (ld->side[1]->middle.image)
            {
                ld->side[1]->middle.offset.X =
                    fmod(ld->side[1]->middle.offset.X +
                             (ld->side[1]->middle.scroll.X + ld->side[1]->middle.net_scroll.X) * factor,
                         ld->side[1]->middle.image->actual_width_);
                ld->side[1]->middle.offset.Y =
                    fmod(ld->side[1]->middle.offset.Y +
                             (ld->side[1]->middle.scroll.Y + ld->side[1]->middle.net_scroll.Y) * factor,
                         ld->side[1]->middle.image->actual_height_);
                ld->side[1]->middle.net_scroll = {{0, 0}};
            }
            if (ld->side[1]->bottom.image)
            {
                ld->side[1]->bottom.offset.X =
                    fmod(ld->side[1]->bottom.offset.X +
                             (ld->side[1]->bottom.scroll.X + ld->side[1]->bottom.net_scroll.X) * factor,
                         ld->side[1]->bottom.image->actual_width_);
                ld->side[1]->bottom.offset.Y =
                    fmod(ld->side[1]->bottom.offset.Y +
                             (ld->side[1]->bottom.scroll.Y + ld->side[1]->bottom.net_scroll.Y) * factor,
                         ld->side[1]->bottom.image->actual_height_);
                ld->side[1]->bottom.net_scroll = {{0, 0}};
            }
        }
    }

    if (active_sector_animations.size() > 0)
    {
        // Calculate net offset/scroll/push for floor/ceilings
        for (size_t i = 0; i < sector_animations.size(); i++)
        {
            Sector *sec = sector_animations[i].target;
            if (!sec)
                continue;

            // Add static values
            sec->properties.net_push.X += sector_animations[i].push.X;
            sec->properties.net_push.Y += sector_animations[i].push.Y;
            sec->floor.net_scroll.X += sector_animations[i].floor_scroll.X;
            sec->floor.net_scroll.Y += sector_animations[i].floor_scroll.Y;
            sec->ceiling.net_scroll.X += sector_animations[i].ceil_scroll.X;
            sec->ceiling.net_scroll.Y += sector_animations[i].ceil_scroll.Y;

            // Update dynamic values
            struct Sector  *sec_ref     = sector_animations[i].scroll_sector_reference;
            const LineType *special_ref = sector_animations[i].scroll_special_reference;
            Line           *line_ref    = sector_animations[i].scroll_line_reference;

            if (!sec_ref || !special_ref || !line_ref ||
                !(special_ref->scroll_type_ & BoomScrollerTypeDisplace ||
                  special_ref->scroll_type_ & BoomScrollerTypeAccel))
                continue;

            float heightref = special_ref->scroll_type_ & BoomScrollerTypeDisplace ? sector_animations[i].last_height
                                                                                   : sec_ref->original_height;
            float sy        = line_ref->length / 32.0f * line_ref->delta_y / line_ref->length *
                       ((sec_ref->floor_height + sec_ref->ceiling_height) - heightref);
            float sx = line_ref->length / 32.0f * line_ref->delta_x / line_ref->length *
                       ((sec_ref->floor_height + sec_ref->ceiling_height) - heightref);
            if (double_framerate.d_ && special_ref->scroll_type_ & BoomScrollerTypeDisplace)
            {
                sy *= 2;
                sx *= 2;
            }
            if (special_ref->sector_effect_ & kSectorEffectTypePushThings)
            {
                sec->properties.net_push.Y += kBoomCarryFactor * sy;
                sec->properties.net_push.X += kBoomCarryFactor * sx;
            }
            if (special_ref->sector_effect_ & kSectorEffectTypeScrollFloor)
            {
                sec->floor.net_scroll.Y -= sy;
                sec->floor.net_scroll.X -= sx;
            }
            if (special_ref->sector_effect_ & kSectorEffectTypeScrollCeiling)
            {
                sec->ceiling.net_scroll.Y -= sy;
                sec->ceiling.net_scroll.X -= sx;
            }
            sector_animations[i].last_height = sec_ref->floor_height + sec_ref->ceiling_height;
        }
    }

    // ANIMATE SECTOR SPECIALS
    std::list<Sector *>::iterator SI;

    for (SI = active_sector_animations.begin(); SI != active_sector_animations.end(); SI++)
    {
        Sector *sec = *SI;

        if (!sec->old_stored)
        {
            sec->floor.old_scroll.X    = sec->floor.offset.X;
            sec->floor.old_scroll.Y    = sec->floor.offset.Y;
            sec->ceiling.old_scroll.X  = sec->ceiling.offset.X;
            sec->ceiling.old_scroll.Y  = sec->ceiling.offset.Y;
            sec->properties.old_push.X = sec->properties.push.X;
            sec->properties.old_push.Y = sec->properties.push.Y;
            sec->properties.old_push.Z = sec->properties.push.Z;
            sec->old_stored            = true;
        }
        else
        {
            sec->floor.scroll.X    = sec->floor.old_scroll.X;
            sec->floor.scroll.Y    = sec->floor.old_scroll.Y;
            sec->ceiling.scroll.X  = sec->ceiling.old_scroll.X;
            sec->ceiling.scroll.Y  = sec->ceiling.old_scroll.Y;
            sec->properties.push.X = sec->properties.old_push.X;
            sec->properties.push.Y = sec->properties.old_push.Y;
            sec->properties.push.Z = sec->properties.old_push.Z;
        }

        sec->floor.offset.X = fmod(sec->floor.offset.X + (sec->floor.scroll.X + sec->floor.net_scroll.X) * factor,
                                   sec->floor.image->actual_width_);
        sec->floor.offset.Y = fmod(sec->floor.offset.Y + (sec->floor.scroll.Y + sec->floor.net_scroll.Y) * factor,
                                   sec->floor.image->actual_height_);
        sec->ceiling.offset.X =
            fmod(sec->ceiling.offset.X + (sec->ceiling.scroll.X + sec->ceiling.net_scroll.X) * factor,
                 sec->ceiling.image->actual_width_);
        sec->ceiling.offset.Y =
            fmod(sec->ceiling.offset.Y + (sec->ceiling.scroll.Y + sec->ceiling.net_scroll.Y) * factor,
                 sec->ceiling.image->actual_height_);
        sec->properties.push.X = sec->properties.push.X + sec->properties.net_push.X;
        sec->properties.push.Y = sec->properties.push.Y + sec->properties.net_push.Y;

        // Reset dynamic stuff
        sec->properties.net_push = {{0, 0, 0}};
        sec->floor.net_scroll    = {{0, 0}};
        sec->ceiling.net_scroll  = {{0, 0}};
    }

    // DO BUTTONS
    if (!double_framerate.d_ || !extra_tic)
        UpdateButtons();
}

//
//  SPECIAL SPAWNING
//

//
// This function is called at the start of every level.  It parses command line
// parameters for level timer, spawns passive special sectors, (ie sectors that
// act even when a player is not in them, and counts total secrets) spawns
// passive lines, (ie scrollers) and resets floor/ceiling movement.
//
// -KM- 1998/09/27 Generalised for sectors.ddf
// -KM- 1998/11/25 Lines with auto tag are automatically triggered.
//
// -AJA- split into two, the first is called _before_ things are
//       loaded, and the second one _afterwards_.
//
void SpawnMapSpecials1(void)
{
    int i;

    active_sector_animations.clear();
    active_line_animations.clear();
    sector_animations.clear();
    line_animations.clear();
    light_animations.clear();

    ClearButtons();

    // See if -TIMER needs to be used.
    level_timer = false;

    i = FindArgument("avg");
    if (i > 0 && InDeathmatch())
    {
        level_timer      = true;
        level_time_count = 20 * 60 * kTicRate;
    }

    std::string s = ArgumentValue("timer");

    if (!s.empty() && InDeathmatch())
    {
        int time;

        time             = atoi(s.c_str()) * 60 * kTicRate;
        level_timer      = true;
        level_time_count = time;
    }

    for (i = 0; i < total_level_lines; i++)
    {
        const LineType *special = level_lines[i].special;

        if (!special)
        {
            level_lines[i].count = 0;
            continue;
        }

        // -AJA- 1999/10/23: weed out non-appearing lines.
        if (!CheckWhenAppear(special->appear_))
        {
            level_lines[i].special = nullptr;
            continue;
        }

        level_lines[i].count = special->count_;

        // -AJA- 2007/12/29: Portal effects
        if (special->portal_effect_ != kPortalEffectTypeNone)
        {
            P_PortalEffect(&level_lines[i]);
        }

        // Extrafloor creation
        if (special->ef_.type_ != kExtraFloorTypeNone && level_lines[i].tag > 0)
        {
            Sector *ctrl = level_lines[i].front_sector;

            for (Sector *tsec = FindSectorFromTag(level_lines[i].tag); tsec; tsec = tsec->tag_next)
            {
                // the OLD method of Boom deep water (the BOOMTEX flag)
                if (special->ef_.type_ & kExtraFloorTypeBoomTex)
                {
                    if (ctrl->floor_height <= tsec->floor_height)
                    {
                        tsec->properties.colourmap = ctrl->properties.colourmap;
                        continue;
                    }
                }

                AddExtraFloor(tsec, &level_lines[i]);

                // transfer any translucency
                if (special->translucency_ <= 0.99f)
                {
                    P_EFTransferTrans(ctrl, tsec, &level_lines[i], &special->ef_, special->translucency_);
                }

                // update the line gaps & things:
                RecomputeGapsAroundSector(tsec);

                FloodExtraFloors(tsec);
            }
        }

        // Detail slopes
        if (special->slope_type_ & kSlopeTypeDetailFloor)
        {
            DetailSlope_Floor(&level_lines[i]);
        }
        if (special->slope_type_ & kSlopeTypeDetailCeiling)
        {
            DetailSlope_Ceiling(&level_lines[i]);
        }

        // Handle our Glass line type now
        if (special->glass_)
        {
            P_LineEffectDebris(&level_lines[i], special);
        }
    }
}

void SpawnMapSpecials2(int autotag)
{
    Sector           *sector;
    const SectorType *secSpecial;
    const LineType   *special;

    int i;

    //
    // Init special SECTORs.
    //

    sector = level_sectors;
    for (i = 0; i < total_level_sectors; i++, sector++)
    {
        if (!sector->properties.special)
            continue;

        secSpecial = sector->properties.special;

        if (!CheckWhenAppear(secSpecial->appear_))
        {
            SectorChangeSpecial(sector, 0);
            continue;
        }

        if (secSpecial->l_.type_ != kLightSpecialTypeNone)
            RunSectorLight(sector, &secSpecial->l_);

        if (secSpecial->secret_)
            intermission_stats.secrets++;

        if (secSpecial->use_colourmap_)
            sector->properties.colourmap = secSpecial->use_colourmap_;

        if (secSpecial->ambient_sfx_)
            AddAmbientSounds(sector, secSpecial->ambient_sfx_);

        // - Plats/Floors -
        if (secSpecial->f_.type_ != kPlaneMoverUndefined)
            RunPlaneMover(sector, &secSpecial->f_, sector);

        // - Doors/Ceilings -
        if (secSpecial->c_.type_ != kPlaneMoverUndefined)
            RunPlaneMover(sector, &secSpecial->c_, sector);

        sector->properties.gravity   = secSpecial->gravity_;
        sector->properties.friction  = secSpecial->friction_;
        sector->properties.viscosity = secSpecial->viscosity_;
        sector->properties.drag      = secSpecial->drag_;

        // compute pushing force
        if (secSpecial->push_speed_ > 0 || secSpecial->push_zspeed_ > 0)
        {
            float mul = secSpecial->push_speed_ / 100.0f;

            sector->properties.push.X += epi::BAMCos(secSpecial->push_angle_) * mul;
            sector->properties.push.Y += epi::BAMSin(secSpecial->push_angle_) * mul;
            sector->properties.push.Z += secSpecial->push_zspeed_ / (double_framerate.d_ ? 89.2f : 100.0f);
        }

        // Scrollers
        if (secSpecial->f_.scroll_speed_ > 0)
        {
            SectorAnimation anim;
            anim.target = sector;

            float dx = epi::BAMCos(secSpecial->f_.scroll_angle_);
            float dy = epi::BAMSin(secSpecial->f_.scroll_angle_);

            anim.floor_scroll.X -= dx * secSpecial->f_.scroll_speed_ / 32.0f;
            anim.floor_scroll.Y -= dy * secSpecial->f_.scroll_speed_ / 32.0f;

            anim.last_height = sector->original_height;

            sector_animations.push_back(anim);

            AddSpecialSector(sector);
        }
        if (secSpecial->c_.scroll_speed_ > 0)
        {
            SectorAnimation anim;
            anim.target = sector;

            float dx = epi::BAMCos(secSpecial->c_.scroll_angle_);
            float dy = epi::BAMSin(secSpecial->c_.scroll_angle_);

            anim.ceil_scroll.X -= dx * secSpecial->c_.scroll_speed_ / 32.0f;
            anim.ceil_scroll.Y -= dy * secSpecial->c_.scroll_speed_ / 32.0f;

            anim.last_height = sector->original_height;

            sector_animations.push_back(anim);

            AddSpecialSector(sector);
        }
    }

    //
    // Init special LINEs.
    //
    // -ACB- & -JC- 1998/06/10 Implemented additional scroll effects
    //
    // -ACB- Added the code
    // -JC-  Designed and contributed code
    // -KM-  Removed Limit
    // -KM- 1998/09/01 Added lines.ddf support
    //
    for (i = 0; i < total_level_lines; i++)
    {
        special = level_lines[i].special;

        if (!special)
            continue;

        if (special->s_xspeed_ || special->s_yspeed_)
        {
            AdjustScrollParts(level_lines[i].side[0], 0, special->scroll_parts_, special->s_xspeed_,
                              special->s_yspeed_);

            AdjustScrollParts(level_lines[i].side[1], 1, special->scroll_parts_, special->s_xspeed_,
                              special->s_yspeed_);

            AddSpecialLine(level_lines + i);
        }

        // -AJA- 1999/06/30: Translucency effect.
        if (special->translucency_ <= 0.99f && level_lines[i].side[0])
            level_lines[i].side[0]->middle.translucency = special->translucency_;

        if (special->translucency_ <= 0.99f && level_lines[i].side[1])
            level_lines[i].side[1]->middle.translucency = special->translucency_;

        if (special->autoline_)
        {
            P_ActivateSpecialLine(&level_lines[i], level_lines[i].special, level_lines[i].tag, 0, nullptr,
                                  kLineTriggerAny, 1, 1);
        }

        // -KM- 1998/11/25 This line should be pushed automatically
        if (autotag && level_lines[i].special && level_lines[i].tag == autotag)
        {
            P_ActivateSpecialLine(&level_lines[i], level_lines[i].special, level_lines[i].tag, 0, nullptr,
                                  kLineTriggerPushable, 1, 1);
        }

        // add lightanim for manual doors with tags
        if (special->type_ == kLineTriggerManual && special->c_.type_ != kPlaneMoverUndefined && level_lines[i].tag)
        {
            LightAnimation anim;
            anim.light_line_reference   = &level_lines[i];
            anim.light_sector_reference = level_lines[i].back_sector;
            for (Sector *tsec = FindSectorFromTag(anim.light_line_reference->tag); tsec; tsec = tsec->tag_next)
            {
                tsec->minimum_neighbor_light = FindMinimumSurroundingLight(tsec, tsec->properties.light_level);
                tsec->maximum_neighbor_light = FindMaxSurroundingLight(tsec, tsec->properties.light_level);
            }
            light_animations.push_back(anim);
        }
    }
}

//
// -KM- 1998/09/27 This helper function is used to do stuff to all the
//                 sectors with the specified line's tag.
//
// -AJA- 1999/09/29: Updated for new tagged sector links.
//
static bool DoSectorsFromTag(int tag, const void *p1, void *p2, bool (*func)(Sector *, const void *, void *))
{
    Sector *tsec;
    bool    rtn = false;

    for (tsec = FindSectorFromTag(tag); tsec; tsec = tsec->tag_next)
    {
        if ((*func)(tsec, p1, p2))
            rtn = true;
    }

    return rtn;
}

void SectorChangeSpecial(Sector *sec, int new_type)
{
    sec->properties.type = HMM_MAX(0, new_type);

    sec->properties.special = LookupSectorType(sec->properties.type);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
