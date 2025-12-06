//----------------------------------------------------------------------------
//  EDGE Play Simulation Action routines
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
// Notes:
//  All Procedures here are never called directly, except possibly
//  by another A_* Routine. Otherwise the procedure is called
//  by referencing an code pointer from the states[] table. The only
//  exception to these rules are A_MissileContact and
//  SlammedIntoObject that requiring "acting" on the part
//  of an obj.
//
// This file was created for all action code by DDF.
//
// -KM- 1998/09/27 Added sounds.ddf capability
// -KM- 1998/12/21 New smooth visibility.
// -AJA- 1999/07/21: Replaced some non-critical RandomByteDeterministics with
// RandomByte. -AJA- 1999/08/08: Replaced some
// RandomByteDeterministic()-RandomByteDeterministic() stuff.
//

#include "p_action.h"

#include "AlmostEquals.h"
#include "con_main.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "epi.h"
#include "f_interm.h" // intermission_stats
#include "g_game.h"
#include "i_system.h"
#include "m_misc.h"
#include "m_random.h"
#include "p_local.h"
#include "p_weapon.h"
#include "r_misc.h"
#include "r_state.h"
#include "rad_trig.h"
#include "s_sound.h"
#include "script/compat/lua_compat.h"
#include "w_wad.h"

extern FlatDefinition *P_IsThingOnLiquidFloor(MapObject *thing);

static constexpr float kLongMeleeRange    = 128.0f; // For kMBF21FlagLongMeleeRange
static constexpr float kShortMissileRange = 896.0f; // For kMBF21FlagShortMissileRange

static const MapObjectDefinition *mushroom_mobj = nullptr;

static int AttackSfxCat(const MapObject *mo)
{
    int category = GetSoundEffectCategory(mo);

    if (category == kCategoryPlayer)
        return kCategoryWeapon;

    return category;
}

static int SfxFlags(const MapObjectDefinition *info)
{
    int flags = 0;

    if (info->extended_flags_ & kExtendedFlagAlwaysLoud)
        flags |= kSoundEffectBoss;

    return flags;
}

//-----------------------------------------
//--------------MISCELLANOUS---------------
//-----------------------------------------

//
// A_ActivateLineType
//
// Allows things to also activate linetypes, bringing them into the
// fold with radius triggers, which can also do it.  There's only two
// parameters needed: linetype number & tag number, which are stored
// in the state's `action_par' field as a pointer to two integers.
//
void A_ActivateLineType(MapObject *mo)
{
    int *values;

    if (!mo->state_ || !mo->state_->action_par)
        return;

    values = (int *)mo->state_->action_par;

    // Note the `nullptr' here: this prevents the activation from failing
    // because the object isn't a PLAYER, for example.
    RemoteActivation(nullptr, values[0], values[1], 0, kLineTriggerAny);
}

//
// A_EnableRadTrig
// A_DisableRadTrig
//
// Allows things to enable or disable radius triggers (by tag number),
// like linetypes can do already.
//
void A_EnableRadTrig(MapObject *mo)
{
    if (!mo->state_ || !mo->state_->action_par)
        return;

    uint64_t *value = (uint64_t *)mo->state_->action_par;

    ScriptEnableByTag(value[0], false, (RADScriptTag)mo->state_->rts_tag_type);
}

void A_DisableRadTrig(MapObject *mo)
{
    if (!mo->state_ || !mo->state_->action_par)
        return;

    uint64_t *value = (uint64_t *)mo->state_->action_par;

    ScriptEnableByTag(value[0], true, (RADScriptTag)mo->state_->rts_tag_type);
}

//
// A_RunLuaScript
//
// Allows things to execute Lua scripts, passing themselves
// as a parameter
//
void A_RunLuaScript(MapObject *mo)
{
    if (!mo->state_ || !mo->state_->action_par)
        return;

    const char *script = (const char *)mo->state_->action_par;

    LuaCallGlobalFunction(LuaGetGlobalVM(), script, mo);
}

//
// A_LookForTargets
//
// Looks for targets: used in the same way as enemy things look
// for players
//
// TODO: Write a decent procedure.
// -KM- 1999/01/31 Added sides. Still has to search every mobj on the
//  map to find a target.  There must be a better way...
// -AJA- 2004/04/28: Rewritten. Mobjs on same side are never targeted.
//
// Dasho - Rewrote this to use A_LookForBlockmapTarget, but without FOV
// restrictions and in a 4 blockmap radius of the thing; this seems to
// mimic the Boom P_LookForTargets behavior a little better
//
bool A_LookForTargets(MapObject *we)
{
    // Optimisation: nobody to support when side is zero
    if (we->side_ == 0)
        return LookForPlayers(we, we->info_->sight_angle_);

    float we_x = we->x;
    float we_y = we->y;

    float radius = kBlockmapUnitSize * 4;
    float x1     = we_x - radius;
    float x2     = we_x + radius;
    float y1     = we_y - radius;
    float y2     = we_y + radius;

    int we_bx = BlockmapGetX(we_x);
    int we_by = BlockmapGetY(we_y);
    we_bx     = HMM_Clamp(0, we_bx, blockmap_width - 1);
    we_by     = HMM_Clamp(0, we_by, blockmap_height - 1);
    int lx    = we_bx - 5;
    int ly    = we_by - 5;
    int hx    = we_bx + 5;
    int hy    = we_by + 5;
    lx        = HMM_MAX(0, lx);
    hx        = HMM_MIN(blockmap_width - 1, hx);
    ly        = HMM_MAX(0, ly);
    hy        = HMM_MIN(blockmap_height - 1, hy);

    // first check the blockmap in our immediate vicinity
    for (MapObject *mo = blockmap_things[we_by * blockmap_width + we_bx]; mo; mo = mo->blockmap_next_)
    {
        if (mo == we)
            continue;

        if (we->source_ == mo)
            continue;

        // check whether thing touches the given bbox
        float r = mo->radius_;

        if (mo->x + r <= x1 || mo->x - r >= x2 || mo->y + r <= y1 || mo->y - r >= y2)
            continue;

        bool same_side = ((mo->side_ & we->side_) != 0);

        // only target monsters or players (not barrels)
        if (!(mo->extended_flags_ & kExtendedFlagMonster) && !mo->player_)
            continue;

        if (!(mo->flags_ & kMapObjectFlagShootable))
            continue;

        if (same_side && !we->support_object_ && mo->support_object_ != we)
        {
            if (mo->support_object_ && CheckSight(we, mo->support_object_))
                mo = mo->support_object_;
            else if (!CheckSight(we, mo))
                continue; // OK since same side

            if (mo)
            {
                we->SetSupportObject(mo);
                if (we->info_->meander_state_)
                    MapObjectSetStateDeferred(we, we->info_->meander_state_, 0);
                return true;
            }
        }

        if (same_side)
            continue;

        if ((we->info_ == mo->info_) && !(we->extended_flags_ & kExtendedFlagDisloyalToOwnType))
            continue;

        if (CheckSight(we, mo))
        {
            we->SetTarget(mo);
            if (we->info_->chase_state_)
                MapObjectSetStateDeferred(we, we->info_->chase_state_, 0);
            return true;
        }
    }

    int      blockX;
    int      blockY;
    int      blockIndex;
    int      firstStop;
    int      secondStop;
    int      thirdStop;
    int      finalStop;
    uint32_t count;

    for (count = 1; count <= 4; count++)
    {
        blockX = HMM_Clamp(we_bx - count, 0, blockmap_width - 1);
        blockY = HMM_Clamp(we_by - count, 0, blockmap_height - 1);

        blockIndex = blockY * blockmap_width + blockX;
        firstStop  = we_bx + count;
        if (firstStop < 0)
        {
            continue;
        }
        if (firstStop >= blockmap_width)
        {
            firstStop = blockmap_width - 1;
        }
        secondStop = we_by + count;
        if (secondStop < 0)
        {
            continue;
        }
        if (secondStop >= blockmap_height)
        {
            secondStop = blockmap_height - 1;
        }
        thirdStop  = secondStop * blockmap_width + blockX;
        secondStop = secondStop * blockmap_width + firstStop;
        firstStop += blockY * blockmap_width;
        finalStop = blockIndex;

        // Trace the first block section (along the top)
        for (; blockIndex <= firstStop; blockIndex++)
        {
            for (MapObject *mo = blockmap_things[blockIndex]; mo; mo = mo->blockmap_next_)
            {
                if (we->source_ == mo)
                    continue;

                // check whether thing touches the given bbox
                float r = mo->radius_;

                if (mo->x + r <= x1 || mo->x - r >= x2 || mo->y + r <= y1 || mo->y - r >= y2)
                    continue;

                bool same_side = ((mo->side_ & we->side_) != 0);

                // only target monsters or players (not barrels)
                if (!(mo->extended_flags_ & kExtendedFlagMonster) && !mo->player_)
                    continue;

                if (!(mo->flags_ & kMapObjectFlagShootable))
                    continue;

                if (same_side && !we->support_object_ && mo->support_object_ != we)
                {
                    if (mo->support_object_ && CheckSight(we, mo->support_object_))
                        mo = mo->support_object_;
                    else if (!CheckSight(we, mo))
                        continue; // OK since same side

                    if (mo)
                    {
                        we->SetSupportObject(mo);
                        if (we->info_->meander_state_)
                            MapObjectSetStateDeferred(we, we->info_->meander_state_, 0);
                        return true;
                    }
                }

                if (same_side)
                    continue;

                if ((we->info_ == mo->info_) && !(we->extended_flags_ & kExtendedFlagDisloyalToOwnType))
                    continue;

                if (CheckSight(we, mo))
                {
                    we->SetTarget(mo);
                    if (we->info_->chase_state_)
                        MapObjectSetStateDeferred(we, we->info_->chase_state_, 0);
                    return true;
                }
            }
        }
        // Trace the second block section (right edge)
        for (blockIndex--; blockIndex <= secondStop; blockIndex += blockmap_width)
        {
            for (MapObject *mo = blockmap_things[blockIndex]; mo; mo = mo->blockmap_next_)
            {
                if (we->source_ == mo)
                    continue;

                // check whether thing touches the given bbox
                float r = mo->radius_;

                if (mo->x + r <= x1 || mo->x - r >= x2 || mo->y + r <= y1 || mo->y - r >= y2)
                    continue;

                bool same_side = ((mo->side_ & we->side_) != 0);

                // only target monsters or players (not barrels)
                if (!(mo->extended_flags_ & kExtendedFlagMonster) && !mo->player_)
                    continue;

                if (!(mo->flags_ & kMapObjectFlagShootable))
                    continue;

                if (same_side && !we->support_object_ && mo->support_object_ != we)
                {
                    if (mo->support_object_ && CheckSight(we, mo->support_object_))
                        mo = mo->support_object_;
                    else if (!CheckSight(we, mo))
                        continue; // OK since same side

                    if (mo)
                    {
                        we->SetSupportObject(mo);
                        if (we->info_->meander_state_)
                            MapObjectSetStateDeferred(we, we->info_->meander_state_, 0);
                        return true;
                    }
                }

                if (same_side)
                    continue;

                if ((we->info_ == mo->info_) && !(we->extended_flags_ & kExtendedFlagDisloyalToOwnType))
                    continue;

                if (CheckSight(we, mo))
                {
                    we->SetTarget(mo);
                    if (we->info_->chase_state_)
                        MapObjectSetStateDeferred(we, we->info_->chase_state_, 0);
                    return true;
                }
            }
        }
        // Trace the third block section (bottom edge)
        for (blockIndex -= blockmap_width; blockIndex >= thirdStop; blockIndex--)
        {
            for (MapObject *mo = blockmap_things[blockIndex]; mo; mo = mo->blockmap_next_)
            {
                if (we->source_ == mo)
                    continue;

                // check whether thing touches the given bbox
                float r = mo->radius_;

                if (mo->x + r <= x1 || mo->x - r >= x2 || mo->y + r <= y1 || mo->y - r >= y2)
                    continue;

                bool same_side = ((mo->side_ & we->side_) != 0);

                // only target monsters or players (not barrels)
                if (!(mo->extended_flags_ & kExtendedFlagMonster) && !mo->player_)
                    continue;

                if (!(mo->flags_ & kMapObjectFlagShootable))
                    continue;

                if (same_side && !we->support_object_ && mo->support_object_ != we)
                {
                    if (mo->support_object_ && CheckSight(we, mo->support_object_))
                        mo = mo->support_object_;
                    else if (!CheckSight(we, mo))
                        continue; // OK since same side

                    if (mo)
                    {
                        we->SetSupportObject(mo);
                        if (we->info_->meander_state_)
                            MapObjectSetStateDeferred(we, we->info_->meander_state_, 0);
                        return true;
                    }
                }

                if (same_side)
                    continue;

                if ((we->info_ == mo->info_) && !(we->extended_flags_ & kExtendedFlagDisloyalToOwnType))
                    continue;

                if (CheckSight(we, mo))
                {
                    we->SetTarget(mo);
                    if (we->info_->chase_state_)
                        MapObjectSetStateDeferred(we, we->info_->chase_state_, 0);
                    return true;
                }
            }
        }
        // Trace the final block section (left edge)
        for (blockIndex++; blockIndex > finalStop; blockIndex -= blockmap_width)
        {
            for (MapObject *mo = blockmap_things[blockIndex]; mo; mo = mo->blockmap_next_)
            {
                if (we->source_ == mo)
                    continue;

                // check whether thing touches the given bbox
                float r = mo->radius_;

                if (mo->x + r <= x1 || mo->x - r >= x2 || mo->y + r <= y1 || mo->y - r >= y2)
                    continue;

                bool same_side = ((mo->side_ & we->side_) != 0);

                // only target monsters or players (not barrels)
                if (!(mo->extended_flags_ & kExtendedFlagMonster) && !mo->player_)
                    continue;

                if (!(mo->flags_ & kMapObjectFlagShootable))
                    continue;

                if (same_side && !we->support_object_ && mo->support_object_ != we)
                {
                    if (mo->support_object_ && CheckSight(we, mo->support_object_))
                        mo = mo->support_object_;
                    else if (!CheckSight(we, mo))
                        continue; // OK since same side

                    if (mo)
                    {
                        we->SetSupportObject(mo);
                        if (we->info_->meander_state_)
                            MapObjectSetStateDeferred(we, we->info_->meander_state_, 0);
                        return true;
                    }
                }

                if (same_side)
                    continue;

                if ((we->info_ == mo->info_) && !(we->extended_flags_ & kExtendedFlagDisloyalToOwnType))
                    continue;

                if (CheckSight(we, mo))
                {
                    we->SetTarget(mo);
                    if (we->info_->chase_state_)
                        MapObjectSetStateDeferred(we, we->info_->chase_state_, 0);
                    return true;
                }
            }
        }
    }

    return false;
}

// Same as above, but iterate through the blockmap within a
// given radius and return the first valid target (or nullptr if none)
// Also, does not actually set the target unlike A_LookForTargets
MapObject *A_LookForBlockmapTarget(MapObject *we, uint32_t rangeblocks, BAMAngle fov)
{
    MapObject *them = nullptr;

    float we_x     = we->x;
    float we_y     = we->y;
    float we_angle = we->angle_;

    float radius = kBlockmapUnitSize * rangeblocks;
    float x1     = we_x - radius;
    float x2     = we_x + radius;
    float y1     = we_y - radius;
    float y2     = we_y + radius;

    int we_bx = BlockmapGetX(we_x);
    int we_by = BlockmapGetY(we_y);
    we_bx     = HMM_Clamp(0, we_bx, blockmap_width - 1);
    we_by     = HMM_Clamp(0, we_by, blockmap_height - 1);
    int lx    = we_bx - rangeblocks - 1;
    int ly    = we_by - rangeblocks - 1;
    int hx    = we_bx + rangeblocks + 1;
    int hy    = we_by + rangeblocks + 1;
    lx        = HMM_MAX(0, lx);
    hx        = HMM_MIN(blockmap_width - 1, hx);
    ly        = HMM_MAX(0, ly);
    hy        = HMM_MIN(blockmap_height - 1, hy);

    // first check the blockmap in our immediate vicinity
    for (MapObject *mo = blockmap_things[we_by * blockmap_width + we_bx]; mo; mo = mo->blockmap_next_)
    {
        // check whether thing touches the given bbox
        float r = mo->radius_;

        if (mo->x + r <= x1 || mo->x - r >= x2 || mo->y + r <= y1 || mo->y - r >= y2)
            continue;

        if (mo == we)
            continue;

        if (we->source_ == mo)
            continue;

        bool same_side = ((mo->side_ & we->side_) != 0);

        // only target monsters or players (not barrels)
        if (!(mo->extended_flags_ & kExtendedFlagMonster) && !mo->player_)
            continue;

        if (!(mo->flags_ & kMapObjectFlagShootable))
            continue;

        if (same_side)
            continue;

        if ((we->info_ == mo->info_) && !(we->extended_flags_ & kExtendedFlagDisloyalToOwnType))
            continue;

        if (CheckSight(we, mo))
        {
            if (fov != 0)
            {
                if (!epi::BAMCheckFOV(PointToAngle(we_x, we_y, mo->x, mo->y), fov, we_angle))
                    continue;
            }
            them = mo;
            return them;
        }
    }

    int      blockX;
    int      blockY;
    int      blockIndex;
    int      firstStop;
    int      secondStop;
    int      thirdStop;
    int      finalStop;
    uint32_t count;

    for (count = 1; count <= rangeblocks; count++)
    {
        blockX = HMM_Clamp(we_bx - count, 0, blockmap_width - 1);
        blockY = HMM_Clamp(we_by - count, 0, blockmap_height - 1);

        blockIndex = blockY * blockmap_width + blockX;
        firstStop  = we_bx + count;
        if (firstStop < 0)
        {
            continue;
        }
        if (firstStop >= blockmap_width)
        {
            firstStop = blockmap_width - 1;
        }
        secondStop = we_by + count;
        if (secondStop < 0)
        {
            continue;
        }
        if (secondStop >= blockmap_height)
        {
            secondStop = blockmap_height - 1;
        }
        thirdStop  = secondStop * blockmap_width + blockX;
        secondStop = secondStop * blockmap_width + firstStop;
        firstStop += blockY * blockmap_width;
        finalStop = blockIndex;

        // Trace the first block section (along the top)
        for (; blockIndex <= firstStop; blockIndex++)
        {
            for (MapObject *mo = blockmap_things[blockIndex]; mo; mo = mo->blockmap_next_)
            {
                // check whether thing touches the given bbox
                float r = mo->radius_;

                if (mo->x + r <= x1 || mo->x - r >= x2 || mo->y + r <= y1 || mo->y - r >= y2)
                    continue;

                if (mo == we)
                    continue;

                if (we->source_ == mo)
                    continue;

                bool same_side = ((mo->side_ & we->side_) != 0);

                // only target monsters or players (not barrels)
                if (!(mo->extended_flags_ & kExtendedFlagMonster) && !mo->player_)
                    continue;

                if (!(mo->flags_ & kMapObjectFlagShootable))
                    continue;

                if (same_side)
                    continue;

                if ((we->info_ == mo->info_) && !(we->extended_flags_ & kExtendedFlagDisloyalToOwnType))
                    continue;

                if (CheckSight(we, mo))
                {
                    if (fov != 0)
                    {
                        if (!epi::BAMCheckFOV(PointToAngle(we_x, we_y, mo->x, mo->y), fov, we_angle))
                            continue;
                    }
                    them = mo;
                    return them;
                }
            }
        }
        // Trace the second block section (right edge)
        for (blockIndex--; blockIndex <= secondStop; blockIndex += blockmap_width)
        {
            for (MapObject *mo = blockmap_things[blockIndex]; mo; mo = mo->blockmap_next_)
            {
                // check whether thing touches the given bbox
                float r = mo->radius_;

                if (mo->x + r <= x1 || mo->x - r >= x2 || mo->y + r <= y1 || mo->y - r >= y2)
                    continue;

                if (mo == we)
                    continue;

                if (we->source_ == mo)
                    continue;

                bool same_side = ((mo->side_ & we->side_) != 0);

                // only target monsters or players (not barrels)
                if (!(mo->extended_flags_ & kExtendedFlagMonster) && !mo->player_)
                    continue;

                if (!(mo->flags_ & kMapObjectFlagShootable))
                    continue;

                if (same_side)
                    continue;

                if ((we->info_ == mo->info_) && !(we->extended_flags_ & kExtendedFlagDisloyalToOwnType))
                    continue;

                if (CheckSight(we, mo))
                {
                    if (fov != 0)
                    {
                        if (!epi::BAMCheckFOV(PointToAngle(we_x, we_y, mo->x, mo->y), fov, we_angle))
                            continue;
                    }
                    them = mo;
                    return them;
                }
            }
        }
        // Trace the third block section (bottom edge)
        for (blockIndex -= blockmap_width; blockIndex >= thirdStop; blockIndex--)
        {
            for (MapObject *mo = blockmap_things[blockIndex]; mo; mo = mo->blockmap_next_)
            {
                // check whether thing touches the given bbox
                float r = mo->radius_;

                if (mo->x + r <= x1 || mo->x - r >= x2 || mo->y + r <= y1 || mo->y - r >= y2)
                    continue;

                if (mo == we)
                    continue;

                if (we->source_ == mo)
                    continue;

                bool same_side = ((mo->side_ & we->side_) != 0);

                // only target monsters or players (not barrels)
                if (!(mo->extended_flags_ & kExtendedFlagMonster) && !mo->player_)
                    continue;

                if (!(mo->flags_ & kMapObjectFlagShootable))
                    continue;

                if (same_side)
                    continue;

                if ((we->info_ == mo->info_) && !(we->extended_flags_ & kExtendedFlagDisloyalToOwnType))
                    continue;

                if (CheckSight(we, mo))
                {
                    if (fov != 0)
                    {
                        if (!epi::BAMCheckFOV(PointToAngle(we_x, we_y, mo->x, mo->y), fov, we_angle))
                            continue;
                    }
                    them = mo;
                    return them;
                }
            }
        }
        // Trace the final block section (left edge)
        for (blockIndex++; blockIndex > finalStop; blockIndex -= blockmap_width)
        {
            for (MapObject *mo = blockmap_things[blockIndex]; mo; mo = mo->blockmap_next_)
            {
                // check whether thing touches the given bbox
                float r = mo->radius_;

                if (mo->x + r <= x1 || mo->x - r >= x2 || mo->y + r <= y1 || mo->y - r >= y2)
                    continue;

                if (mo == we)
                    continue;

                if (we->source_ == mo)
                    continue;

                bool same_side = ((mo->side_ & we->side_) != 0);

                // only target monsters or players (not barrels)
                if (!(mo->extended_flags_ & kExtendedFlagMonster) && !mo->player_)
                    continue;

                if (!(mo->flags_ & kMapObjectFlagShootable))
                    continue;

                if (same_side)
                    continue;

                if ((we->info_ == mo->info_) && !(we->extended_flags_ & kExtendedFlagDisloyalToOwnType))
                    continue;

                if (CheckSight(we, mo))
                {
                    if (fov != 0)
                    {
                        if (!epi::BAMCheckFOV(PointToAngle(we_x, we_y, mo->x, mo->y), fov, we_angle))
                            continue;
                    }
                    them = mo;
                    return them;
                }
            }
        }
    }

    return them;
}

//
// DecideMeleeAttack
//
// This is based on P_CheckMeleeRange, except that it relys upon
// info from the objects close combat attack, the original code
// used a set value for all objects which was kMeleeRange + 20,
// this code allows different melee ranges for different objects.
//
// -ACB- 1998/08/15
// -KM- 1998/11/25 Added attack parameter.
//
static bool DecideMeleeAttack(MapObject *object, const AttackDefinition *attack)
{
    MapObject *target;
    float      distance;
    float      meleedist;

    target = object->target_;

    if (!target)
        return false;

    distance = ApproximateDistance(target->x - object->x, target->y - object->y);

    if (level_flags.true_3d_gameplay)
        distance = ApproximateDistance(target->z - object->z, distance);

    if (attack)
        meleedist = attack->range_;
    else
    {
        meleedist = kMeleeRange;
        if (object->mbf21_flags_ & kMBF21FlagLongMeleeRange)
            meleedist = kLongMeleeRange;
        // I guess a specific MBF21 Thing Melee range should override the above
        // choices?
        if (object->info_->melee_range_ > -1)
            meleedist = object->info_->melee_range_;
    }
    meleedist += target->radius_ - 20.0f; // Check the thing's actual radius

    if (distance >= meleedist)
        return false;

    return CheckSight(object, target);
}

//
// DecideRangeAttack
//
// This is based on P_CheckMissileRange, contrary the name it does more
// than check the missile range, it makes a decision of whether or not an
// attack should be made or not depending on the object with the option
// to attack. A return of false is mandatory if the object cannot see its
// target (makes sense, doesn't it?), after this the distance is calculated,
// it will eventually be check to see if it is greater than a number from
// the Random Number Generator; if so the procedure returns true. Essentially
// the closer the object is to its target, the more chance an attack will
// be made (another logical decision).
//
// -ACB- 1998/08/15
//
static bool DecideRangeAttack(MapObject *object)
{
    float                   chance;
    float                   distance;
    const AttackDefinition *attack = object->info_->rangeattack_;

    if (!object->target_)
        return false;

    // If no rangeattack present, continue if the mobj
    // still has a missile state (most likely Dehacked/MBF21)
    if (!attack && !object->info_->missile_state_)
        return false;

    // Just been hit (and have felt pain), so in true tit-for-tat
    // style, the object - without regard to anything else - hits back.
    if (object->flags_ & kMapObjectFlagJustHit)
    {
        if (!CheckSight(object, object->target_))
            return false;

        object->flags_ &= ~kMapObjectFlagJustHit;
        return true;
    }

    // Bit slow on the up-take: the object hasn't had time to
    // react his target.
    if (object->reaction_time_)
        return false;

    // Get the distance, a basis for our decision making from now on
    distance = ApproximateDistance(object->x - object->target_->x, object->y - object->target_->y);

    // If no close-combat attack, increase the chance of a missile attack
    if (!object->info_->melee_state_)
        distance -= 192;
    else
        distance -= 64;

    // Object is too far away to attack?
    if (attack && attack->range_ && distance >= attack->range_)
        return false;

    // MBF21 SHORTMRANGE flag
    if ((object->mbf21_flags_ & kMBF21FlagShortMissileRange) && distance >= kShortMissileRange)
        return false;

    // Object is too close to target
    if (attack && attack->tooclose_ && attack->tooclose_ >= distance)
        return false;

    // Object likes to fire? if so, double the chance of it happening
    if (object->extended_flags_ & kExtendedFlagTriggerHappy)
        distance /= 2;

    if (object->mbf21_flags_ & kMBF21FlagHigherMissileProb)
        distance = HMM_MIN(distance, 160.0f);
    else
        distance = HMM_MIN(distance, 200.0f);

    // The chance in the object is one given that the attack will happen, so
    // we inverse the result (since its one in 255) to get the chance that
    // the attack will not happen.
    chance = 1.0f - object->info_->minatkchance_;
    chance = HMM_MIN(distance / 255.0f, chance);

    // now after modifing distance where applicable, we get the random number
    // and check if it is less than distance, if so no attack is made.
    if (RandomByteTestDeterministic(chance))
        return false;

    return CheckSight(object, object->target_);
}

//
// A_FaceTarget
//
// Look at the prey......
//
void A_FaceTarget(MapObject *object)
{
    MapObject *target = object->target_;

    if (!target)
        return;

    if (object->flags_ & kMapObjectFlagStealth)
        object->target_visibility_ = 1.0f;

    object->flags_ &= ~kMapObjectFlagAmbush;

    object->angle_ = PointToAngle(object->x, object->y, target->x, target->y);

    float dist = PointToDistance(object->x, object->y, target->x, target->y);

    if (dist >= 0.1f)
    {
        float dz = MapObjectMidZ(target) - MapObjectMidZ(object);

        object->vertical_angle_ = epi::BAMFromATan(dz / dist);
    }

    if (target->flags_ & kMapObjectFlagFuzzy)
    {
        object->angle_ += RandomByteSkewToZeroDeterministic() << (kBAMAngleBits - 11);
        object->vertical_angle_ += epi::BAMFromATan(RandomByteSkewToZeroDeterministic() / 1024.0f);
    }

    if (target->visibility_ < 1.0f)
    {
        float amount = (1.0f - target->visibility_);

        object->angle_ += (BAMAngle)(RandomByteSkewToZeroDeterministic() * (kBAMAngleBits - 12) * amount);
        object->vertical_angle_ += epi::BAMFromATan(RandomByteSkewToZeroDeterministic() * amount / 2048.0f);
    }

    // don't look up/down too far...
    if (object->vertical_angle_ < kBAMAngle180 && object->vertical_angle_ > kBAMAngle45)
        object->vertical_angle_ = kBAMAngle45;

    if (object->vertical_angle_ >= kBAMAngle180 && object->vertical_angle_ < kBAMAngle315)
        object->vertical_angle_ = kBAMAngle315;
}

//
// P_ForceFaceTarget
//
// FaceTarget, but ignoring visibility modifiers
//
void P_ForceFaceTarget(MapObject *object)
{
    MapObject *target = object->target_;

    if (!target)
        return;

    if (object->flags_ & kMapObjectFlagStealth)
        object->target_visibility_ = 1.0f;

    object->flags_ &= ~kMapObjectFlagAmbush;

    object->angle_ = PointToAngle(object->x, object->y, target->x, target->y);

    float dist = PointToDistance(object->x, object->y, target->x, target->y);

    if (dist >= 0.1f)
    {
        float dz = MapObjectMidZ(target) - MapObjectMidZ(object);

        object->vertical_angle_ = epi::BAMFromATan(dz / dist);
    }

    // don't look up/down too far...
    if (object->vertical_angle_ < kBAMAngle180 && object->vertical_angle_ > kBAMAngle45)
        object->vertical_angle_ = kBAMAngle45;

    if (object->vertical_angle_ >= kBAMAngle180 && object->vertical_angle_ < kBAMAngle315)
        object->vertical_angle_ = kBAMAngle315;
}

void A_MakeIntoCorpse(MapObject *mo)
{
    // Gives the effect of the object being a corpse....

    if (mo->flags_ & kMapObjectFlagStealth)
        mo->target_visibility_ = 1.0f; // dead and very visible

    // object is on ground, it can be walked over
    mo->flags_ &= ~kMapObjectFlagSolid;

    if (mo->tag_)
    {
        auto mobjs = active_tagged_map_objects.equal_range(mo->tag_);
        for (auto mobj = mobjs.first; mobj != mobjs.second;)
        {
            if (mobj->second == mo)
                mobj = active_tagged_map_objects.erase(mobj);
            else
                ++mobj;
        }
    }

    if (mo->tid_)
    {
        auto mobjs = active_tids.equal_range(mo->tid_);
        for (auto mobj = mobjs.first; mobj != mobjs.second;)
        {
            if (mobj->second == mo)
                mobj = active_tids.erase(mobj);
            else
                ++mobj;
        }
    }

    mo->tag_ = 0;
    mo->tid_ = 0;

    HitLiquidFloor(mo);
}

void BringCorpseToLife(MapObject *corpse)
{
    // Bring a corpse back to life (the opposite of the above routine).
    // Handles players too !

    const MapObjectDefinition *info = corpse->info_;

    corpse->flags_             = info->flags_;
    corpse->health_            = corpse->spawn_health_;
    corpse->radius_            = info->radius_;
    corpse->height_            = info->height_;
    corpse->extended_flags_    = info->extended_flags_;
    corpse->hyper_flags_       = info->hyper_flags_;
    corpse->target_visibility_ = info->translucency_;
    // UDMF check
    if (!AlmostEquals(corpse->alpha_, 1.0f))
        corpse->target_visibility_ = corpse->alpha_;
    corpse->tag_ = corpse->spawnpoint_.tag;
    corpse->tid_ = corpse->spawnpoint_.tid;

    corpse->flags_ &= ~kMapObjectFlagCountKill; // Lobo 2023: don't add to killcount

    if (corpse->player_)
    {
        corpse->player_->player_state_         = kPlayerAlive;
        corpse->player_->health_               = corpse->health_;
        corpse->player_->standard_view_height_ = corpse->height_ * info->viewheight_;
    }

    if (info->overkill_sound_)
        StartSoundEffect(info->overkill_sound_, GetSoundEffectCategory(corpse), corpse);

    if (info->raise_state_)
        MapObjectSetState(corpse, info->raise_state_);
    else if (info->meander_state_)
        MapObjectSetState(corpse, info->meander_state_);
    else if (info->idle_state_)
        MapObjectSetState(corpse, info->idle_state_);
    else
        FatalError("Object %s has no RESURRECT states.\n", info->name_.c_str());
}

void A_ResetSpreadCount(MapObject *mo)
{
    // Resets the spreader count for fixed-order spreaders, normally used
    // at the beginning of a set of missile states to ensure that an object
    // fires in the same object each time.

    mo->spread_count_ = 0;
}

//-------------------------------------------------------------------
//-------------------VISIBILITY HANDLING ROUTINES--------------------
//-------------------------------------------------------------------

void A_TransSet(MapObject *mo)
{
    float value = 1.0f;

    const State *st = mo->state_;

    if (st && st->action_par)
    {
        value = ((float *)st->action_par)[0];
        value = HMM_MAX(0.0f, HMM_MIN(1.0f, value));
    }

    mo->visibility_ = mo->target_visibility_ = value;
}

void A_TransFade(MapObject *mo)
{
    float value = 0.0f;

    const State *st = mo->state_;

    if (st && st->action_par)
    {
        value = ((float *)st->action_par)[0];
        value = HMM_MAX(0.0f, HMM_MIN(1.0f, value));
    }

    mo->target_visibility_ = value;
}

void A_TransLess(MapObject *mo)
{
    float value = 0.05f;

    const State *st = mo->state_;

    if (st && st->action_par)
    {
        value = ((float *)st->action_par)[0];
        value = HMM_MAX(0.0f, HMM_MIN(1.0f, value));
    }

    mo->target_visibility_ -= value;

    if (mo->target_visibility_ < 0.0f)
        mo->target_visibility_ = 0.0f;
}

void A_TransMore(MapObject *mo)
{
    float value = 0.05f;

    const State *st = mo->state_;

    if (st && st->action_par)
    {
        value = ((float *)st->action_par)[0];
        value = HMM_MAX(0.0f, HMM_MIN(1.0f, value));
    }

    mo->target_visibility_ += value;

    if (mo->target_visibility_ > 1.0f)
        mo->target_visibility_ = 1.0f;
}

//
// A_TransAlternate
//
// Alters the translucency of an item, kExtendedFlagLessVisible is used
// internally to tell the object if it should be getting
// more visible or less visible; kExtendedFlagLessVisible is set when an
// object is to get less visible (because it has become
// to a level of lowest translucency) and the flag is unset
// if the object has become as highly translucent as possible.
//
void A_TransAlternate(MapObject *object)
{
    const State *st;
    float        value = 0.05f;

    st = object->state_;

    if (st && st->action_par)
    {
        value = ((float *)st->action_par)[0];
        value = HMM_MAX(0.0f, HMM_MIN(1.0f, value));
    }

    if (object->extended_flags_ & kExtendedFlagLessVisible)
    {
        object->target_visibility_ -= value;
        if (object->target_visibility_ <= 0.0f)
        {
            object->target_visibility_ = 0.0f;
            object->extended_flags_ &= ~kExtendedFlagLessVisible;
        }
    }
    else
    {
        object->target_visibility_ += value;
        if (object->target_visibility_ >= 1.0f)
        {
            object->target_visibility_ = 1.0f;
            object->extended_flags_ |= kExtendedFlagLessVisible;
        }
    }
}

void A_DLightSet(MapObject *mo)
{
    const State *st = mo->state_;

    if (st && st->action_par)
    {
        mo->dynamic_light_.r = HMM_MAX(0.0f, ((int *)st->action_par)[0]);

        if (mo->info_->hyper_flags_ & kHyperFlagQuadraticDynamicLight)
            mo->dynamic_light_.r = DynamicLightCompatibilityRadius(mo->dynamic_light_.r);

        mo->dynamic_light_.target = mo->dynamic_light_.r;
    }
}

void A_DLightFade(MapObject *mo)
{
    const State *st = mo->state_;

    if (st && st->action_par)
    {
        mo->dynamic_light_.target = HMM_MAX(0.0f, ((int *)st->action_par)[0]);

        if (mo->info_->hyper_flags_ & kHyperFlagQuadraticDynamicLight)
            mo->dynamic_light_.target = DynamicLightCompatibilityRadius(mo->dynamic_light_.target);
    }
}

void A_DLightRandom(MapObject *mo)
{
    const State *st = mo->state_;

    if (st && st->action_par)
    {
        int low  = ((int *)st->action_par)[0];
        int high = ((int *)st->action_par)[1];

        // Note: using RandomByte so that gameplay is unaffected
        float qty = low + (high - low) * RandomByte() / 255.0f;

        if (mo->info_->hyper_flags_ & kHyperFlagQuadraticDynamicLight)
            qty = DynamicLightCompatibilityRadius(qty);

        mo->dynamic_light_.r      = HMM_MAX(0.0f, qty);
        mo->dynamic_light_.target = mo->dynamic_light_.r;
    }
}

void A_DLightColour(MapObject *mo)
{
    const State *st = mo->state_;

    if (st && st->action_par)
    {
        mo->dynamic_light_.color = ((RGBAColor *)st->action_par)[0];
    }
}

void A_SetSkin(MapObject *mo)
{
    const State *st = mo->state_;

    if (st && st->action_par)
    {
        int skin = ((int *)st->action_par)[0];

        if (skin < 0 || skin > 9)
            FatalError("Thing [%s]: Bad skin number %d in SET_SKIN action.\n", mo->info_->name_.c_str(), skin);

        mo->model_skin_ = skin;
    }
}

//-------------------------------------------------------------------
//------------------- MOVEMENT ROUTINES -----------------------------
//-------------------------------------------------------------------

void A_MlookFace(MapObject *mo)
{
    const State *st = mo->state_;

    if (st && st->action_par)
        mo->vertical_angle_ = epi::BAMFromATan(*(float *)st->action_par);
    else
        mo->vertical_angle_ = 0;
}

void A_FaceDir(MapObject *mo)
{
    const State *st = mo->state_;

    if (st && st->action_par)
        mo->vertical_angle_ += epi::BAMFromATan(*(float *)st->action_par);
}

void A_MlookTurn(MapObject *mo)
{
    const State *st = mo->state_;

    if (st && st->action_par)
        mo->angle_ = *(BAMAngle *)st->action_par;
    else
        mo->angle_ = 0;
}

void A_TurnDir(MapObject *mo)
{
    const State *st = mo->state_;

    BAMAngle turn = kBAMAngle180;

    if (st && st->action_par)
        turn = *(BAMAngle *)st->action_par;

    mo->angle_ += turn;
}

void A_TurnRandom(MapObject *mo)
{
    const State *st   = mo->state_;
    int          turn = 359;

    if (st && st->action_par)
    {
        turn = (int)epi::DegreesFromBAM(*(BAMAngle *)st->action_par);
    }

    turn = turn * RandomByteDeterministic() / 90; // 10 bits of angle

    if (turn < 0)
        mo->angle_ -= (BAMAngle)((-turn) << (kBAMAngleBits - 10));
    else
        mo->angle_ += (BAMAngle)(turn << (kBAMAngleBits - 10));
}

void A_MoveFwd(MapObject *mo)
{
    const State *st = mo->state_;

    if (st && st->action_par)
    {
        float amount = *(float *)st->action_par;

        float dx = epi::BAMCos(mo->angle_);
        float dy = epi::BAMSin(mo->angle_);

        mo->AddMomentum(dx * amount, dy * amount, 0);
    }
}

void A_MoveRight(MapObject *mo)
{
    const State *st = mo->state_;

    if (st && st->action_par)
    {
        float amount = *(float *)st->action_par;

        float dx = epi::BAMCos(mo->angle_ - kBAMAngle90);
        float dy = epi::BAMSin(mo->angle_ - kBAMAngle90);

        mo->AddMomentum(dx * amount, dy * amount, 0);
    }
}

void A_MoveUp(MapObject *mo)
{
    const State *st = mo->state_;

    if (st && st->action_par)
    {
        mo->AddMomentum(0, 0, *(float *)st->action_par);
    }
}

void A_StopMoving(MapObject *mo)
{
    mo->momentum_.X = mo->momentum_.Y = mo->momentum_.Z = 0;
}

//-------------------------------------------------------------------
//-------------------SOUND CAUSING ROUTINES--------------------------
//-------------------------------------------------------------------

void A_PlaySound(MapObject *mo)
{
    // Generate an arbitrary sound.

    SoundEffect *sound = nullptr;

    if (mo->state_ && mo->state_->action_par)
        sound = (SoundEffect *)mo->state_->action_par;

    if (!sound)
    {
        WarningOrError("A_PlaySound: missing sound name in %s.\n", mo->info_->name_.c_str());
        return;
    }

    // StartSoundEffect(sound, P_MobjGetSfxCategory(mo), mo,
    // SfxFlags(mo->info_));
    StartSoundEffect(sound, GetSoundEffectCategory(mo), mo);
}

// Same as above but always loud
void A_PlaySoundBoss(MapObject *mo)
{
    // Generate an arbitrary sound.

    SoundEffect *sound = nullptr;

    if (mo->state_ && mo->state_->action_par)
        sound = (SoundEffect *)mo->state_->action_par;

    if (!sound)
    {
        WarningOrError("A_PlaySoundBoss: missing sound name in %s.\n", mo->info_->name_.c_str());
        return;
    }

    int flags = 0;
    flags |= kSoundEffectBoss;

    StartSoundEffect(sound, GetSoundEffectCategory(mo), mo, flags);
}

void A_KillSound(MapObject *mo)
{
    // Kill any current sounds from this thing.

    StopSoundEffect(mo);
}

void A_MakeAmbientSound(MapObject *mo)
{
    // Just a sound generating procedure that cause the sound ref
    // in seesound_ to be generated.

    if (mo->info_->seesound_)
        StartSoundEffect(mo->info_->seesound_, GetSoundEffectCategory(mo), mo);
    else
        LogDebug("%s has no ambient sound\n", mo->info_->name_.c_str());
}

void A_MakeAmbientSoundRandom(MapObject *mo)
{
    // Give a small "random" chance that this object will make its
    // ambient sound. Currently this is a set value of 50, however
    // the code that drives this, should allow for the user to set
    // the value, note for further DDF Development.

    if (mo->info_->seesound_)
    {
        if (RandomByte() < 50)
            StartSoundEffect(mo->info_->seesound_, GetSoundEffectCategory(mo), mo);
    }
    else
        LogDebug("%s has no ambient sound\n", mo->info_->name_.c_str());
}

void A_MakeActiveSound(MapObject *mo)
{
    // Just a sound generating procedure that cause the sound ref
    // in activesound_ to be generated.
    //
    // -KM- 1999/01/31

    if (mo->info_->activesound_)
        StartSoundEffect(mo->info_->activesound_, GetSoundEffectCategory(mo), mo);
    else
        LogDebug("%s has no ambient sound\n", mo->info_->name_.c_str());
}

void A_MakeDyingSound(MapObject *mo)
{
    // This procedure is like every other sound generating
    // procedure with the exception that if the object is
    // a boss (kExtendedFlagAlwaysLoud extended flag) then the sound is
    // generated at full volume (source = nullptr).

    SoundEffect *sound = mo->info_->deathsound_;

    if (sound)
        StartSoundEffect(sound, GetSoundEffectCategory(mo), mo, SfxFlags(mo->info_));
    else
        LogDebug("%s has no death sound\n", mo->info_->name_.c_str());
}

void A_MakePainSound(MapObject *mo)
{
    // Ow!! it hurts!

    if (mo->info_->painsound_)
        StartSoundEffect(mo->info_->painsound_, GetSoundEffectCategory(mo), mo, SfxFlags(mo->info_));
    else
        LogDebug("%s has no pain sound\n", mo->info_->name_.c_str());
}

void A_MakeOverKillSound(MapObject *mo)
{
    if (mo->info_->overkill_sound_)
        StartSoundEffect(mo->info_->overkill_sound_, GetSoundEffectCategory(mo), mo, SfxFlags(mo->info_));
    else
        LogDebug("%s has no overkill sound\n", mo->info_->name_.c_str());
}

void A_MakeCloseAttemptSound(MapObject *mo)
{
    // Attempting close combat sound

    if (!mo->info_->closecombat_)
        FatalError("Object [%s] used CLOSEATTEMPTSND action, "
                   "but has no CLOSE_ATTACK\n",
                   mo->info_->name_.c_str());

    SoundEffect *sound = mo->info_->closecombat_->initsound_;

    if (sound)
        StartSoundEffect(sound, GetSoundEffectCategory(mo), mo);
    else
        LogDebug("%s has no close combat attempt sound\n", mo->info_->name_.c_str());
}

void A_MakeRangeAttemptSound(MapObject *mo)
{
    // Attempting range attack sound

    if (!mo->info_->rangeattack_)
        FatalError("Object [%s] used RANGEATTEMPTSND action, "
                   "but has no RANGE_ATTACK\n",
                   mo->info_->name_.c_str());

    SoundEffect *sound = mo->info_->rangeattack_->initsound_;

    if (sound)
        StartSoundEffect(sound, GetSoundEffectCategory(mo), mo);
    else
        LogDebug("%s has no range attack attempt sound\n", mo->info_->name_.c_str());
}

//-------------------------------------------------------------------
//-------------------EXPLOSION DAMAGE ROUTINES-----------------------
//-------------------------------------------------------------------

//
// A_DamageExplosion
//
// Radius Attack damage set by info->damage. Used for the original Barrels
//
void A_DamageExplosion(MapObject *object)
{
    float damage;

    EDGE_DAMAGE_COMPUTE(damage, &object->info_->explode_damage_);

#ifdef DEVELOPERS
    if (!damage)
    {
        LogDebug("%s caused no explosion damage\n", object->info_->name.c_str());
        return;
    }
#endif

    // -AJA- 2004/09/27: new EXPLODE_RADIUS command (overrides normal calc)
    float radius = object->info_->explode_radius_;
    if (AlmostEquals(radius, 0.0f))
        radius = damage;

    RadiusAttack(object, object->source_, radius, damage, &object->info_->explode_damage_, false);
}

//
// A_Thrust
//
// Thrust set by info->damage.
//
void A_Thrust(MapObject *object)
{
    float damage;

    EDGE_DAMAGE_COMPUTE(damage, &object->info_->explode_damage_);

#ifdef DEVELOPERS
    if (!damage)
    {
        LogDebug("%s caused no thrust\n", object->info_->name.c_str());
        return;
    }
#endif

    float radius = object->info_->explode_radius_;
    if (AlmostEquals(radius, 0.0f))
        radius = damage;

    RadiusAttack(object, object->source_, radius, damage, &object->info_->explode_damage_, true);
}

//-------------------------------------------------------------------
//-------------------MISSILE HANDLING ROUTINES-----------------------
//-------------------------------------------------------------------

//
// A_Explode
//
// The object blows up, like a missile.
//
// -AJA- 1999/08/21: Replaced A_ExplodeMissile (which was identical
//       to p_mobj's P_ExplodeMissile) with this.
//
void A_Explode(MapObject *object)
{
    ExplodeMissile(object);
}

//
// A_CheckMissileSpawn
//
// This procedure handles a newly spawned missile, it moved
// by half the amount of momentum and then checked to see
// if the move is possible, if not the projectile is
// exploded. Also the number of initial tics on its
// current state is taken away from by a random number
// between 0 and 3, although the number of tics will never
// go below 1.
//
// -ACB- 1998/08/04
//
// -AJA- 1999/08/22: Fixed a bug that occasionally caused the game to
//       go into an infinite loop.  NOTE WELL: don't fiddle with the
//       object's x & y directly, use TryMove instead, or
//       ChangeThingPosition.
//
static void CheckMissileSpawn(MapObject *projectile)
{
    projectile->tics_ -= RandomByteDeterministic() & 3;

    if (projectile->tics_ < 1)
        projectile->tics_ = 1;

    HMM_Vec3 check_pos = {
        {projectile->momentum_.X * 0.5f, projectile->momentum_.Y * 0.5f, projectile->momentum_.Z * 0.5f}};

    while (PointToDistance(projectile->x, projectile->y, projectile->x + check_pos.X, projectile->y + check_pos.Y) >
           projectile->radius_)
    {
        check_pos *= 0.5f;
    }

    projectile->z += check_pos.Z;

    if (!TryMove(projectile, projectile->x + check_pos.X, projectile->y + check_pos.Y))
    {
        ExplodeMissile(projectile);
    }
}

//
// LaunchProjectile
//
// This procedure launches a project the direction of the target mobj.
// * source - the source of the projectile, required
// * target - the target of the projectile, can be nullptr
// * type   - the mobj type of the projectile
//
// For all sense and purposes it is possible for the target to be a dummy
// mobj, just to act as a carrier for a set of target co-ordinates.
//
// Missiles can be spawned at different locations on and around
// the mobj. Traditionally an mobj would fire a projectile
// at a height of 32 from the centerpoint of that
// mobj, this was true for all creatures from the Cyberdemon to
// the Imp. The currentattack holds the height and x & y
// offsets that dictates the spawning location of a projectile.
//
// Traditionally: Height   = 4*8
//                x-offset = 0
//                y-offset = 0
//
// The exception to this rule is the revenant, which did increase
// its z value by 16 before firing: This was a hack
// to launch a missile at a height of 48. The revenants
// height was reduced to normal after firing, this new code
// makes that an unnecesary procedure.
//
// projx, projy & projz are the projectiles spawn location
//
// NOTE: may return nullptr.
//
static MapObject *DoLaunchProjectile(MapObject *source, float tx, float ty, float tz, MapObject *target,
                                     const MapObjectDefinition *type)
{
    const AttackDefinition *attack = source->current_attack_;

    if (!attack)
        return nullptr;

    // -AJA- projz now handles crouching
    float      projx          = source->x;
    float      projy          = source->y;
    float      projz          = source->z + (attack->height_ * ((source->height_ > 0 && source->info_->height_ > 0)
                                                                    ? source->height_ / source->info_->height_
                                                                    : 1.0f));
    Sector    *cur_source_sec = source->subsector_->sector;
    BAMAngle   angle          = 0;
    float      slope          = 0.0f;
    MapObject *projectile     = nullptr;

    if (cur_source_sec->sink_depth > 0 && !cur_source_sec->extrafloor_used && !cur_source_sec->height_sector &&
        AlmostEquals(source->z, cur_source_sec->floor_height))
        projz -= (source->height_ * 0.5 * cur_source_sec->sink_depth);

    if (attack->flags_ & kAttackFlagOffsetsLast)
        angle = PointToAngle(projx, projy, tx, ty);
    else
        angle = source->angle_;

    if (!(attack->flags_ & kAttackFlagOffsetsLast))
    {
        projx += attack->xoffset_ * epi::BAMCos(angle + kBAMAngle90);
        projy += attack->xoffset_ * epi::BAMSin(angle + kBAMAngle90);

        float yoffset;

        if (!AlmostEquals(attack->yoffset_, 0.0f))
            yoffset = attack->yoffset_;
        else
            yoffset = source->radius_ - 0.5f;

        projx += yoffset * epi::BAMCos(angle) * epi::BAMCos(source->vertical_angle_);
        projy += yoffset * epi::BAMSin(angle) * epi::BAMCos(source->vertical_angle_);
        projz += yoffset * epi::BAMSin(source->vertical_angle_) + attack->zoffset_;

        projectile = CreateMapObject(projx, projy, projz, type);

        angle = PointToAngle(projx, projy, tx, ty);

        if (!target)
        {
            tz += attack->height_;
        }
        else
        {
            projectile->extended_flags_ |= kExtendedFlagFirstTracerCheck;

            if (!(attack->flags_ & kAttackFlagPlayer))
            {
                if (target->flags_ & kMapObjectFlagFuzzy)
                    angle += RandomByteSkewToZeroDeterministic() << (kBAMAngleBits - 12);

                if (target->visibility_ < 1.0f)
                    angle += (BAMAngle)(RandomByteSkewToZeroDeterministic() * 64 * (1.0f - target->visibility_));
            }

            Sector *cur_target_sec = target->subsector_->sector;

            if (cur_target_sec->sink_depth > 0 && !cur_target_sec->extrafloor_used && !cur_target_sec->height_sector &&
                AlmostEquals(target->z, cur_target_sec->floor_height))
                tz -= (target->height_ * 0.5 * cur_target_sec->sink_depth);
        }

        slope = ApproximateSlope(tx - projx, ty - projy, tz - projz);
    }
    else
    {
        projectile = CreateMapObject(projx, projy, projz, type);

        if (!target)
        {
            tz += attack->height_;
        }
        else
        {
            projectile->extended_flags_ |= kExtendedFlagFirstTracerCheck;

            if (!(attack->flags_ & kAttackFlagPlayer))
            {
                if (target->flags_ & kMapObjectFlagFuzzy)
                    angle += RandomByteSkewToZeroDeterministic() << (kBAMAngleBits - 12);

                if (target->visibility_ < 1.0f)
                    angle += (BAMAngle)(RandomByteSkewToZeroDeterministic() * 64 * (1.0f - target->visibility_));
            }

            Sector *cur_target_sec = target->subsector_->sector;

            if (cur_target_sec->sink_depth > 0 && !cur_target_sec->extrafloor_used && !cur_target_sec->height_sector &&
                AlmostEquals(target->z, cur_target_sec->floor_height))
                tz -= (target->height_ * 0.5 * cur_target_sec->sink_depth);
        }

        slope = ApproximateSlope(tx - projx, ty - projy, tz - projz);
        projx += attack->xoffset_ * epi::BAMCos(angle + kBAMAngle90);
        projy += attack->xoffset_ * epi::BAMSin(angle + kBAMAngle90);

        float yoffset;

        if (!AlmostEquals(attack->yoffset_, 0.0f))
            yoffset = attack->yoffset_;
        else
            yoffset = source->radius_ - 0.5f;

        projx += yoffset * epi::BAMCos(angle) * epi::BAMCos(source->vertical_angle_);
        projy += yoffset * epi::BAMSin(angle) * epi::BAMCos(source->vertical_angle_);
        projz += yoffset * epi::BAMSin(source->vertical_angle_) + attack->zoffset_;
        ChangeThingPosition(projectile, projx, projy, projz);
    }

    // -AJA- 1999/09/11: add in attack's angle & slope offsets.
    angle -= attack->angle_offset_;
    slope += attack->slope_offset_;

    // is the attack not accurate?
    if (!source->player_ || source->player_->refire_ > 0)
    {
        if (attack->accuracy_angle_ > 0.0f)
            angle += (attack->accuracy_angle_ >> 8) * RandomByteSkewToZeroDeterministic();

        if (attack->accuracy_slope_ > 0.0f)
            slope += attack->accuracy_slope_ * (RandomByteSkewToZeroDeterministic() / 255.0f);
    }

    MapObjectSetDirectionAndSpeed(projectile, angle, slope, projectile->speed_);

    // currentattack is held so that when a collision takes place
    // with another object, we know whether or not the object hit
    // can shake off the attack or is damaged by it.
    //
    projectile->current_attack_ = attack;
    projectile->SetRealSource(source);
    projectile->SetSpawnSource(source);

    // check for blocking lines between source and projectile
    if (MapCheckBlockingLine(source, projectile))
    {
        ExplodeMissile(projectile);
        return nullptr;
    }

    // launch sound
    if (projectile->info_ && projectile->info_->seesound_)
    {
        int category = AttackSfxCat(source);
        int flags    = SfxFlags(projectile->info_);

        MapObject *sfx_source = projectile;
        if (category == kCategoryPlayer || category == kCategoryWeapon)
            sfx_source = source;

        StartSoundEffect(projectile->info_->seesound_, category, sfx_source, flags);
    }

    // Now add the fact that the target may be difficult to spot and
    // make the projectile's target the same as the sources. Only
    // do these if the object is not a dummy object, otherwise just
    // flag the missile not to trace: you cannot track a target that
    // does not exist...

    projectile->SetTarget(target);

    if (attack->flags_ & kAttackFlagInheritTracerFromTarget) // MBF21
        projectile->SetTracer(source->target_);

    if (projectile->flags_ & kMapObjectFlagPreserveMomentum)
    {
        projectile->momentum_.X += source->momentum_.X;
        projectile->momentum_.Y += source->momentum_.Y;
        projectile->momentum_.Z += source->momentum_.Z;
    }
    CheckMissileSpawn(projectile);

    return projectile;
}

static MapObject *LaunchProjectile(MapObject *source, MapObject *target, const MapObjectDefinition *type)
{
    float tx, ty, tz;

    if (source->current_attack_ && (source->current_attack_->flags_ & kAttackFlagNoTarget))
        target = nullptr;

    TargetTheory(source, target, &tx, &ty, &tz);

    return DoLaunchProjectile(source, tx, ty, tz, target, type);
}

//
// LaunchSmartProjectile
//
// This procedure has the same effect as
// LaunchProjectile, but it calculates a point where the target
// and missile will intersect.  This comes from the fact that to shoot
// something, you have to aim slightly ahead of it.  It will also put
// an end to circle-strafing.  :-)
//
// -KM- 1998/10/29
// -KM- 1998/12/16 Fixed it up.  Works quite well :-)
//
static void LaunchSmartProjectile(MapObject *source, MapObject *target, const MapObjectDefinition *type)
{
    float t  = -1;
    float mx = 0, my = 0;

    if (target)
    {
        mx = target->momentum_.X;
        my = target->momentum_.Y;

        float dx = source->x - target->x;
        float dy = source->y - target->y;

        float s = type->speed_;
        if (level_flags.fast_monsters && type->fast_speed_ > -1)
            s = type->fast_speed_;

        float a = mx * mx + my * my - s * s;
        float b = 2 * (dx * mx + dy * my);
        float c = dx * dx + dy * dy;

        float t1 = -1, t2 = -1;

        // find solution to the quadratic equation
        if (a && ((b * b - 4 * a * c) >= 0))
        {
            t1 = -b + (float)sqrt(b * b - 4.0f * a * c);
            t1 /= 2.0f * a;

            t2 = -b - (float)sqrt(b * b - 4.0f * a * c);
            t2 /= 2.0f * a;
        }

        if (t1 < 0)
            t = t2;
        else if (t2 < 0)
            t = t1;
        else
            t = (t1 < t2) ? t1 : t2;
    }

    if (t <= 0)
    {
        // -AJA- when no target, fall back to "dumb mode"
        LaunchProjectile(source, target, type);
    }
    else
    {
        // -AJA- 2005/02/07: assumes target doesn't move up or down

        float tx = target->x + mx * t;
        float ty = target->y + my * t;
        float tz = MapObjectMidZ(target);

        DoLaunchProjectile(source, tx, ty, tz, target, type);

#if 0 // -AJA- this doesn't seem correct / consistent
		if (projectile)
			source->angle = projectile->angle;
#endif
    }
}

static inline bool Weakness_CheckHit(MapObject *target, const AttackDefinition *attack, float x, float y, float z)
{
    const WeaknessDefinition *weak = &target->info_->weak_;

    if (weak->classes_ == 0)
        return false;

    if (!attack) // Lobo: This fixes the long standing bug where EDGE crashes
                 // out sometimes.
    {
        return false;
    }

    if (0 != (attack->attack_class_ & ~weak->classes_))
        return false;

    if (target->height_ < 1)
        return false;

    // LogDebug("Weakness_CheckHit: target=[%s] classes=0x%08x\n",
    // target->info->name.c_str(), weak->classes);

    // compute vertical position.  Clamping it means that a missile
    // which hits the target on the head (coming sharply down) will
    // still register as a head-shot.
    z = (z - target->z) / target->height_;
    z = HMM_Clamp(0.01f, z, 0.99f);

    // LogDebug("HEIGHT CHECK: %1.2f < %1.2f < %1.2f\n",
    //		  weak->height[0], z, weak->height[1]);

    if (z < weak->height_[0] || z > weak->height_[1])
        return false;

    BAMAngle ang = PointToAngle(target->x, target->y, x, y);

    ang -= target->angle_;

    // LogDebug("ANGLE CHECK: %1.2f < %1.2f < %1.2f\n",
    //		 ANG_2_FLOAT(weak->angle[0]), ANG_2_FLOAT(ang),
    //		 ANG_2_FLOAT(weak->angle[1]));

    if (weak->angle_[0] <= weak->angle_[1])
    {
        if (ang < weak->angle_[0] || ang > weak->angle_[1])
            return false;
    }
    else
    {
        if (ang < weak->angle_[0] && ang > weak->angle_[1])
            return false;
    }

    return true;
}

//
// MissileContact
//
// Called by CheckRelativeThingCallback when a missile comes into
// contact with another object. Placed here with
// the other missile code for cleaner code.
//
// Returns: -1 if missile should pass through.
//           0 if hit but no damage was done.
//          +1 if hit and damage was done.
//
int MissileContact(MapObject *object, MapObject *target)
{
    MapObject *source = object->source_;

    if (source)
    {
        // check for ghosts (attack passes through)
        if (object->current_attack_ && 0 == (object->current_attack_->attack_class_ & ~target->info_->ghost_))
            return -1;

        if ((target->side_ & source->side_) != 0)
        {
            if (target->hyper_flags_ & kHyperFlagFriendlyFirePassesThrough)
                return -1;

            if (target->hyper_flags_ & kHyperFlagFriendlyFireImmune)
                return 0;
        }

        if (source->info_ == target->info_)
        {
            if (!(target->extended_flags_ & kExtendedFlagDisloyalToOwnType) && (source->info_->proj_group_ != -1))
                return 0;
        }

        // "Real" missile source check
        if (source->source_ && source->source_->info_ == target->info_)
        {
            if (!(target->extended_flags_ & kExtendedFlagDisloyalToOwnType) &&
                (source->source_->info_->proj_group_ != -1))
                return 0;
        }

        // MBF21: If in same projectile group, attack does no damage
        if (source->info_->proj_group_ > 0 && target->info_->proj_group_ > 0 &&
            (source->info_->proj_group_ == target->info_->proj_group_))
        {
            if (object->extended_flags_ & kExtendedFlagTunnel)
                return -1;
            else
                return 0;
        }

        if (object->current_attack_ != nullptr && !(target->extended_flags_ & kExtendedFlagOwnAttackHurts))
        {
            if (object->current_attack_ == target->info_->rangeattack_)
                return 0;
            if (object->current_attack_ == target->info_->closecombat_)
                return 0;
        }
    }

    const DamageClass *damtype;

    if (object->current_attack_)
        damtype = &object->current_attack_->damage_;
    else
        damtype = &object->info_->proj_damage_;

    float damage;
    EDGE_DAMAGE_COMPUTE(damage, damtype);

    bool weak_spot = false;

    // check for Weakness against the attack
    if (Weakness_CheckHit(target, object->current_attack_, object->x, object->y, MapObjectMidZ(object)))
    {
        damage *= target->info_->weak_.multiply_;
        weak_spot = true;
    }

    // check for immunity against the attack
    if (object->hyper_flags_ & kHyperFlagInvulnerable)
        return 0;

    if (!weak_spot && source->current_attack_ &&
        0 == (source->current_attack_->attack_class_ & ~target->info_->immunity_))
    {
        int state = 0;
        state = MapObjectFindLabel(target, "IMMUNITYHIT");
        if (state != 0)
            MapObjectSetStateDeferred(target, state, 0);

        return 0;
    }


    // support for "tunnelling" missiles, which should only do damage at
    // the first impact.
    if (object->extended_flags_ & kExtendedFlagTunnel)
    {
        // this hash is very basic, but should work OK
        uint32_t hash = (uint32_t)(long long)target;

        if (object->tunnel_hash_[0] == hash || object->tunnel_hash_[1] == hash)
            return -1;

        object->tunnel_hash_[0] = object->tunnel_hash_[1];
        object->tunnel_hash_[1] = hash;
        if (object->info_->rip_sound_)
            StartSoundEffect(object->info_->rip_sound_, kCategoryObject, object, 0);
    }

    if (source)
    {
        // Berserk handling
        if (source->player_ && object->current_attack_ &&
            !AlmostEquals(source->player_->powers_[kPowerTypeBerserk], 0.0f))
        {
            damage *= object->current_attack_->berserk_mul_;
        }
    }

    if (!damage)
    {
#ifdef DEVELOPERS
        LogDebug("%s missile did zero damage.\n", object->info_->name.c_str());
#endif
        return 0;
    }

    DamageMapObject(target, object, object->source_, damage, damtype, weak_spot);
    return 1;
}

//
// BulletContact
//
// Called by ShootTraverseCallback when a bullet comes into contact with
// another object.  Needed so that the "DISLOYAL" special will behave
// in the same manner for bullets as for missiles.
//
// Note: also used for Close-Combat attacks.
//
// Returns: -1 if bullet should pass through.
//           0 if hit but no damage was done.
//          +1 if hit and damage was done.
//
int BulletContact(MapObject *source, MapObject *target, float damage, const DamageClass *damtype, float x, float y,
                  float z)
{
    // check for ghosts (attack passes through)
    if (source->current_attack_ && 0 == (source->current_attack_->attack_class_ & ~target->info_->ghost_))
        return -1;

    if ((target->side_ & source->side_) != 0)
    {
        if (target->hyper_flags_ & kHyperFlagFriendlyFirePassesThrough)
            return -1;

        if (target->hyper_flags_ & kHyperFlagFriendlyFireImmune)
            return 0;
    }

    if (source->info_ == target->info_)
    {
        if (!(target->extended_flags_ & kExtendedFlagDisloyalToOwnType))
            return 0;
    }

    if (source->current_attack_ != nullptr && !(target->extended_flags_ & kExtendedFlagOwnAttackHurts))
    {
        if (source->current_attack_ == target->info_->rangeattack_)
            return 0;
        if (source->current_attack_ == target->info_->closecombat_)
            return 0;
    }

    // ignore damage in GOD mode, or with INVUL powerup
    if (target->player_)
    {
        if ((target->player_->cheats_ & kCheatingGodMode) || target->player_->powers_[kPowerTypeInvulnerable] > 0)
        {
            // emulate the thrust that DamageMapObject() would have done
            if (source && damage > 0 && !(target->flags_ & kMapObjectFlagNoClip))
                ThrustMapObject(target, source, damage);

            return 0;
        }
    }

    bool weak_spot = false;

    // check for Weakness against the attack
    if (Weakness_CheckHit(target, source->current_attack_, x, y, z))
    {
        damage *= target->info_->weak_.multiply_;
        weak_spot = true;
    }

    // check for immunity against the attack
    if (target->hyper_flags_ & kHyperFlagInvulnerable)
        return 0;


    if (!weak_spot && source->current_attack_ &&
        0 == (source->current_attack_->attack_class_ & ~target->info_->immunity_))
    {
        int state = 0;
        state = MapObjectFindLabel(target, "IMMUNITYHIT");
        if (state != 0)
            MapObjectSetStateDeferred(target, state, 0);

        return 0;
    }

    if (!damage)
    {
#ifdef DEVELOPERS
        LogDebug("%s's shoot/combat attack did zero damage.\n", source->info_->name.c_str());
#endif
        return 0;
    }

    DamageMapObject(target, source, source, damage, damtype, weak_spot);
    return 1;
}

//
// A_CreateSmokeTrail
//
// Just spawns smoke behind an mobj: the smoke is
// risen by giving it z momentum, in order to
// prevent the smoke appearing uniform (which obviously
// does not happen), the number of tics that the smoke
// mobj has is "randomly" reduced, although the number
// of tics never gets to zero or below.
//
// -ACB- 1998/08/10 Written
// -ACB- 1999/10/01 Check thing's current attack has a smoke projectile
//
void A_CreateSmokeTrail(MapObject *projectile)
{
    const AttackDefinition *attack = projectile->current_attack_;

    if (attack == nullptr)
        return;

    if (attack->puff_ == nullptr)
    {
        WarningOrError("A_CreateSmokeTrail: attack %s has no PUFF object\n", attack->name_.c_str());
        return;
    }

    // spawn a puff of smoke behind the rocket
    MapObject *smoke = CreateMapObject(projectile->x - projectile->momentum_.X / 2.0f,
                                       projectile->y - projectile->momentum_.Y / 2.0f, projectile->z, attack->puff_);

    smoke->momentum_.Z = smoke->info_->float_speed_;
    smoke->tics_ -= RandomByte() & 3;
    if (smoke->tics_ < 1)
        smoke->tics_ = 1;
}

//
// A_HomingProjectile
//
// This projectile will alter its course to intercept its
// target, if is possible for this procedure to be called
// and nothing results because of a chance that the
// projectile will not chase its target.
//
// As this code is based on the revenant tracer, it did use
// a bit check on the current game_tic - which was why every so
// often a revenant fires a missile straight and not one that
// homes in on its target: If the game_tic has bits 1+2 on
// (which boils down to 1 in every 4 tics), the trick in this
// is that - in conjuntion with the tic count for the
// tracing object's states - the tracing will always fail or
// pass the check: if it passes first time, it will always
// pass and vice versa. The problem is that for someone designing a new
// tracing projectile it would be more than a bit confusing to
// joe "dooming" public.
//
// The new system that affects the original gameplay slightly is
// to get a random chance of the projectile not homing in on its
// target and working this out first time round, the test result
// is recorded (by clearing the 'target' field).
//
// -ACB- 1998/08/10
//
void A_HomingProjectile(MapObject *projectile)
{
    const AttackDefinition *attack = projectile->current_attack_;

    if (attack == nullptr)
        return;

    if (attack->flags_ & kAttackFlagSmokingTracer)
        A_CreateSmokeTrail(projectile);

    if (projectile->extended_flags_ & kExtendedFlagFirstTracerCheck)
    {
        projectile->extended_flags_ &= ~kExtendedFlagFirstTracerCheck;

        if (RandomByteTestDeterministic(attack->notracechance_))
        {
            projectile->SetTarget(nullptr);
            return;
        }
    }

    MapObject *destination = projectile->target_;

    if (!destination || destination->health_ <= 0)
        return;

    // change angle
    BAMAngle exact = PointToAngle(projectile->x, projectile->y, destination->x, destination->y);

    if (exact != projectile->angle_)
    {
        if (exact - projectile->angle_ > kBAMAngle180)
        {
            projectile->angle_ -= attack->trace_angle_;

            if (exact - projectile->angle_ < kBAMAngle180)
                projectile->angle_ = exact;
        }
        else
        {
            projectile->angle_ += attack->trace_angle_;

            if (exact - projectile->angle_ > kBAMAngle180)
                projectile->angle_ = exact;
        }
    }

    projectile->momentum_.X = projectile->speed_ * epi::BAMCos(projectile->angle_);
    projectile->momentum_.Y = projectile->speed_ * epi::BAMSin(projectile->angle_);

    // change slope
    float slope = ApproximateSlope(destination->x - projectile->x, destination->y - projectile->y,
                                   MapObjectMidZ(destination) - projectile->z);

    slope *= projectile->speed_;

    if (slope < projectile->momentum_.Z)
        projectile->momentum_.Z -= 0.125f;
    else
        projectile->momentum_.Z += 0.125f;
}

//
// A_HomeToSpot
//
// This projectile will alter its course to intercept its target,
// or explode if it has reached it.  Used by the bossbrain cube.
//
void A_HomeToSpot(MapObject *projectile)
{
    MapObject *target = projectile->target_;

    if (target == nullptr)
    {
        ExplodeMissile(projectile);
        return;
    }

    float dx = target->x - projectile->x;
    float dy = target->y - projectile->y;
    float dz = target->z - projectile->z;

    float ck_radius = target->radius_ + projectile->radius_ + 2;
    float ck_height = target->height_ + projectile->height_ + 2;

    // reached target ?
    if (fabs(dx) <= ck_radius && fabs(dy) <= ck_radius && fabs(dz) <= ck_height)
    {
        ExplodeMissile(projectile);
        return;
    }

    // calculate new angles
    BAMAngle angle = PointToAngle(0, 0, dx, dy);
    float    slope = ApproximateSlope(dx, dy, dz);

    MapObjectSetDirectionAndSpeed(projectile, angle, slope, projectile->speed_);
}

//
// LaunchOrderedSpread
//
// Due to the unique way of handling that the mancubus fires, it is necessary
// to write a single procedure to handle the firing. In real terms it amounts
// to a glorified hack; The table holds the angle modifier and the choice of
// whether the firing object or the projectile is affected. This procedure
// should NOT be used for players as it will alter the player's mobj, bypassing
// the normal player controls; The only reason for its existance is to maintain
// the original mancubus behaviour. Although it is possible to make this
// generic, the benefits of doing so are minimal. Purist function....
//
// -ACB- 1998/08/15
//
static void LaunchOrderedSpread(MapObject *mo)
{
    // left side = angle modifier
    // right side = object or projectile (true for object).
    static int spreadorder[] = {(int)(kBAMAngle90 / 8),   true,  (int)(kBAMAngle90 / 8),  false,
                                -(int)(kBAMAngle90 / 8),  true,  -(int)(kBAMAngle90 / 4), false,
                                -(int)(kBAMAngle90 / 16), false, (int)(kBAMAngle90 / 16), false};

    const AttackDefinition *attack = mo->current_attack_;

    if (attack == nullptr)
        return;

    int count = mo->spread_count_;

    if (count < 0 || count > 10)
        count = mo->spread_count_ = 0;

    // object or projectile?
    // true --> the object, false --> the projectile.
    if (spreadorder[count + 1])
    {
        mo->angle_ += spreadorder[count];

        LaunchProjectile(mo, mo->target_, attack->atk_mobj_);
    }
    else
    {
        MapObject *projectile = LaunchProjectile(mo, mo->target_, attack->atk_mobj_);
        if (projectile == nullptr)
            return;

        projectile->angle_ += spreadorder[count];

        projectile->momentum_.X = projectile->speed_ * epi::BAMCos(projectile->angle_);
        projectile->momentum_.Y = projectile->speed_ * epi::BAMSin(projectile->angle_);
    }

    mo->spread_count_ += 2;
}

//
// LaunchRandomSpread
//
// This is a the generic function that should be used for a spreader like
// mancubus, although its random nature would certainly be a change to the
// ordered method used now. The random number is bit shifted to the right
// and then the kBAMAngle90 is divided by it, the first bit of the RN is checked
// to detemine if the angle is change is negative or not (approx 50% chance).
// The result is the modifier for the projectile's angle.
//
// -ACB- 1998/08/15
//
static void LaunchRandomSpread(MapObject *mo)
{
    if (mo->current_attack_ == nullptr)
        return;

    MapObject *projectile = LaunchProjectile(mo, mo->target_, mo->current_attack_->atk_mobj_);
    if (projectile == nullptr)
        return;

    int i = RandomByteDeterministic() & 127;

    if (i >> 1)
    {
        BAMAngle spreadangle = (kBAMAngle90 / (i >> 1));

        if (i & 1)
            spreadangle -= spreadangle << 1;

        projectile->angle_ += spreadangle;
    }

    projectile->momentum_.X = projectile->speed_ * epi::BAMCos(projectile->angle_);
    projectile->momentum_.Y = projectile->speed_ * epi::BAMSin(projectile->angle_);
}

//-------------------------------------------------------------------
//-------------------LINEATTACK ATTACK ROUTINES-----------------------
//-------------------------------------------------------------------

// -KM- 1998/11/25 Added uncertainty to the z component of the line.
static void ShotAttack(MapObject *mo)
{
    const AttackDefinition *attack = mo->current_attack_;

    if (!attack)
        return;

    float range = (attack->range_ > 0) ? attack->range_ : kMissileRange;

    // -ACB- 1998/09/05 Remember to use the object angle, fool!
    BAMAngle objangle = mo->angle_;
    float    objslope;

    if ((mo->player_ && !mo->target_) || (attack->flags_ & kAttackFlagNoTarget))
        objslope = epi::BAMTan(mo->vertical_angle_);
    else
        AimLineAttack(mo, objangle, range, &objslope);

    if (attack->sound_)
        StartSoundEffect(attack->sound_, AttackSfxCat(mo), mo);

    // -AJA- 1999/09/10: apply the attack's angle offsets.
    objangle -= attack->angle_offset_;
    objslope += attack->slope_offset_;

    for (int i = 0; i < attack->count_; i++)
    {
        BAMAngle angle = objangle;
        float    slope = objslope;

        // is the attack not accurate?
        if (!mo->player_ || mo->player_->refire_ > 0)
        {
            if (attack->accuracy_angle_ > 0)
                angle += (attack->accuracy_angle_ >> 8) * RandomByteSkewToZeroDeterministic();

            if (attack->accuracy_slope_ > 0)
                slope += attack->accuracy_slope_ * (RandomByteSkewToZeroDeterministic() / 255.0f);
        }

        float damage;
        EDGE_DAMAGE_COMPUTE(damage, &attack->damage_);

        if (mo->player_ && !AlmostEquals(mo->player_->powers_[kPowerTypeBerserk], 0.0f))
            damage *= attack->berserk_mul_;

        LineAttack(mo, angle, range, slope, damage, &attack->damage_, attack->puff_, attack->blood_);
    }
}

// -KM- 1998/11/25 BFG Spray attack.  Must be used from missiles.
//   Will do a BFG spray on every monster in sight.
static void SprayAttack(MapObject *mo)
{
    const AttackDefinition *attack = mo->current_attack_;

    if (!attack)
        return;

    float range = (attack->range_ > 0) ? attack->range_ : kMissileRange;

    // offset angles from its attack angle
    for (int i = 0; i < 40; i++)
    {
        BAMAngle an = mo->angle_ - kBAMAngle90 / 2 + (kBAMAngle90 / 40) * i;

        // mo->source is the originator (player) of the missile
        MapObject *target = AimLineAttack(mo->source_ ? mo->source_ : mo, an, range, nullptr);

        if (!target)
            continue;

        MapObject *ball = CreateMapObject(target->x, target->y, target->z + target->height_ / 4, attack->atk_mobj_);

        ball->SetTarget(mo->target_);

        // check for immunity against the attack
        if (target->hyper_flags_ & kHyperFlagInvulnerable)
            continue;

        if (0 == (attack->attack_class_ & ~target->info_->immunity_))
        {
            int state = 0;
            state = MapObjectFindLabel(target, "IMMUNITYHIT");
            if (state != 0)
                MapObjectSetStateDeferred(target, state, 0);

                continue;
        }
            

        float damage;
        EDGE_DAMAGE_COMPUTE(damage, &attack->damage_);

        if (mo->player_ && !AlmostEquals(mo->player_->powers_[kPowerTypeBerserk], 0.0f))
            damage *= attack->berserk_mul_;

        if (damage)
            DamageMapObject(target, nullptr, mo->source_, damage, &attack->damage_);
    }
}

static void DoMeleeAttack(MapObject *mo)
{
    const AttackDefinition *attack = mo->current_attack_;

    float range = (attack->range_ > 0) ? attack->range_ : kMissileRange;

    float damage;
    EDGE_DAMAGE_COMPUTE(damage, &attack->damage_);

    // -KM- 1998/11/25 Berserk ability
    // -ACB- 2004/02/04 Only zero is off
    if (mo->player_ && !AlmostEquals(mo->player_->powers_[kPowerTypeBerserk], 0.0f))
        damage *= attack->berserk_mul_;

    // -KM- 1998/12/21 Use Line attack so bullet puffs are spawned.

    if (!DecideMeleeAttack(mo, attack))
    {
        LineAttack(mo, mo->angle_, range, epi::BAMTan(mo->vertical_angle_), damage, &attack->damage_, attack->puff_,
                   attack->blood_);
        return;
    }

    if (attack->sound_)
        StartSoundEffect(attack->sound_, AttackSfxCat(mo), mo);

    float slope;

    AimLineAttack(mo, mo->angle_, range, &slope);

    LineAttack(mo, mo->angle_, range, slope, damage, &attack->damage_, attack->puff_, attack->blood_);
}

//-------------------------------------------------------------------
//--------------------TRACKER HANDLING ROUTINES----------------------
//-------------------------------------------------------------------

//
// A Tracker is an object that follows its target, by being on top of
// it. This is the attack style used by an Arch-Vile. The first routines
// handle the tracker itself, the last two are called by the source of
// the tracker.
//

//
// A_TrackerFollow
//
// Called by the tracker to follow its target.
//
// -ACB- 1998/08/22
//
void A_TrackerFollow(MapObject *tracker)
{
    MapObject *destination = tracker->target_;

    if (!destination || !tracker->source_)
        return;

    // Can the parent of the tracker see the target?
    if (!CheckSight(tracker->source_, destination))
        return;

    BAMAngle angle = destination->angle_;

    ChangeThingPosition(tracker, destination->x + 24 * epi::BAMCos(angle), destination->y + 24 * epi::BAMSin(angle),
                        destination->z);
}

//
// A_TrackerActive
//
// Called by the tracker to make its active sound: also tracks
//
// -ACB- 1998/08/22
//
void A_TrackerActive(MapObject *tracker)
{
    if (tracker->info_->activesound_)
        StartSoundEffect(tracker->info_->activesound_, GetSoundEffectCategory(tracker), tracker);

    A_TrackerFollow(tracker);
}

//
// A_TrackerStart
//
// Called by the tracker to make its launch (see) sound: also tracks
//
// -ACB- 1998/08/22
//
void A_TrackerStart(MapObject *tracker)
{
    if (tracker->info_->seesound_)
        StartSoundEffect(tracker->info_->seesound_, GetSoundEffectCategory(tracker), tracker);

    A_TrackerFollow(tracker);
}

//
// LaunchTracker
//
// This procedure starts a tracking object off and links
// the tracker and the monster together.
//
// -ACB- 1998/08/22
//
static void LaunchTracker(MapObject *object)
{
    const AttackDefinition *attack = object->current_attack_;
    MapObject              *target = object->target_;

    if (!attack || !target)
        return;

    MapObject *tracker = CreateMapObject(target->x, target->y, target->z, attack->atk_mobj_);

    // link the tracker to the object
    object->SetTracer(tracker);

    // tracker source is the object
    tracker->SetRealSource(object);
    tracker->SetSpawnSource(object);

    // tracker's target is the object's target
    tracker->SetTarget(target);

    A_TrackerFollow(tracker);
}

//
// A_EffectTracker
//
// Called by the object that launched the tracker to
// cause damage to its target and a radius attack
// (explosion) at the location of the tracker.
//
// -ACB- 1998/08/22
//
void A_EffectTracker(MapObject *object)
{
    MapObject              *tracker = nullptr;
    MapObject              *target  = nullptr;
    const AttackDefinition *attack  = nullptr;
    BAMAngle                angle;
    float                   damage;

    if (!object->target_)
        return;

    if (object->current_attack_)
        attack = object->current_attack_;
    else
    {
        // If the object's current attack is null, hope that this is Dehacked using
        // A_VileAttack directly and that ARCHVILE_FIRE is the intended attack - Dasho
        attack = atkdefs.Lookup("ARCHVILE_FIRE");
    }

    if (!attack)
        return;

    target = object->target_;

    if (attack->flags_ & kAttackFlagFaceTarget)
        A_FaceTarget(object);

    if (attack->flags_ & kAttackFlagNeedSight)
    {
        if (!CheckSight(object, target))
            return;
    }

    if (attack->sound_)
        StartSoundEffect(attack->sound_, GetSoundEffectCategory(object), object);

    angle   = object->angle_;
    tracker = object->tracer_;

    EDGE_DAMAGE_COMPUTE(damage, &attack->damage_);

    if (damage)
        DamageMapObject(target, object, object, damage, &attack->damage_);
#ifdef DEVELOPERS
    else
        LogDebug("%s + %s attack has zero damage\n", object->info_->name.c_str(), tracker->info->name.c_str());
#endif

    // -ACB- 2000/03/11 Check for zero mass
    if (target->info_->mass_)
        target->momentum_.Z = 1000 / target->info_->mass_;
    else
        target->momentum_.Z = 2000;

    if (!tracker)
        return;

    // move the tracker between the object and the object's target

    ChangeThingPosition(tracker, target->x - 24 * epi::BAMCos(angle), target->y - 24 * epi::BAMSin(angle), target->z);

#ifdef DEVELOPERS
    if (!tracker->info->explode_damage_.nominal)
        LogDebug("%s + %s explosion has zero damage\n", object->info_->name.c_str(), tracker->info->name.c_str());
#endif

    EDGE_DAMAGE_COMPUTE(damage, &tracker->info_->explode_damage_);

    float radius = object->info_->explode_radius_;
    if (AlmostEquals(radius, 0.0f))
        radius = damage;

    RadiusAttack(tracker, object, radius, damage, &tracker->info_->explode_damage_, false);
}

//
// A_PsychicEffect
//
// Same as above, but with a single non-explosive damage instance and no lifting
// of the target
//
void A_PsychicEffect(MapObject *object)
{
    MapObject              *target;
    const AttackDefinition *attack;
    float                   damage;

    if (!object->target_ || !object->current_attack_)
        return;

    attack = object->current_attack_;
    target = object->target_;

    if (attack->flags_ & kAttackFlagFaceTarget)
        A_FaceTarget(object);

    if (attack->flags_ & kAttackFlagNeedSight)
    {
        if (!CheckSight(object, target))
            return;
    }

    if (attack->sound_)
        StartSoundEffect(attack->sound_, GetSoundEffectCategory(object), object);

    EDGE_DAMAGE_COMPUTE(damage, &attack->damage_);

    if (damage)
        DamageMapObject(target, object, object, damage, &attack->damage_);
#ifdef DEVELOPERS
    else
        LogDebug("%s + %s attack has zero damage\n", object->info_->name.c_str(), tracker->info->name.c_str());
#endif
}

//-----------------------------------------------------------------
//--------------------BOSS HANDLING PROCEDURES---------------------
//-----------------------------------------------------------------

static void ShootToSpot(MapObject *object)
{
    if (!object->current_attack_)
        return;

    const MapObjectDefinition *spot_type = object->info_->spitspot_;

    if (spot_type == nullptr)
    {
        WarningOrError("Thing [%s] used SHOOT_TO_SPOT attack, but has no SPIT_SPOT\n", object->info_->name_.c_str());
        return;
    }

    MapObject *spot = LookForShootSpot(object->info_->spitspot_);

    if (spot == nullptr)
    {
        LogWarning("No [%s] objects found for BossBrain shooter.\n", spot_type->name_.c_str());
        return;
    }

    LaunchProjectile(object, spot, object->current_attack_->atk_mobj_);
}

//-------------------------------------------------------------------
//-------------------OBJECT-SPAWN-OBJECT HANDLING--------------------
//-------------------------------------------------------------------

//
// A_ObjectSpawning
//
// An Object spawns another object and is spawned in the state specificed
// by attack->objinitstate. The procedure is based on the A_PainShootSkull
// which is the routine for shooting skulls from a pain elemental. In
// this the object being created is decided in the attack. This
// procedure also used the new blocking line check to see if
// the object is spawned across a blocking line, if so the procedure
// terminates.
//
// -ACB- 1998/08/23
//
static void ObjectSpawning(MapObject *parent, BAMAngle angle)
{
    float                   slope;
    const AttackDefinition *attack;

    attack = parent->current_attack_;
    if (!attack)
        return;

    const MapObjectDefinition *shoottype = attack->spawnedobj_;

    if (!shoottype)
    {
        FatalError("Object [%s] uses spawning attack [%s], but no object "
                   "specified.\n",
                   parent->info_->name_.c_str(), attack->name_.c_str());
    }

    if (attack->spawn_limit_ > 0)
    {
        int count = 0;
        for (MapObject *mo = map_object_list_head; mo; mo = mo->next_)
            if (mo->info_ == shoottype)
                if (++count >= attack->spawn_limit_)
                    return;
    }

    // -AJA- 1999/09/10: apply the angle offset of the attack.
    angle -= attack->angle_offset_;
    slope = epi::BAMTan(parent->vertical_angle_) + attack->slope_offset_;

    float spawnx = parent->x;
    float spawny = parent->y;
    float spawnz = parent->z + attack->height_;

    if (attack->flags_ & kAttackFlagPrestepSpawn)
    {
        float prestep = 4.0f + 1.5f * parent->radius_ + shoottype->radius_;

        spawnx += prestep * epi::BAMCos(angle);
        spawny += prestep * epi::BAMSin(angle);
    }

    MapObject *child = CreateMapObject(spawnx, spawny, spawnz, shoottype);

    // Blocking line detected between object and spawnpoint?
    if (MapCheckBlockingLine(parent, child))
    {
        if (child->flags_ & kMapObjectFlagCountKill)
            intermission_stats.kills--;
        if (child->flags_ & kMapObjectFlagCountItem)
            intermission_stats.items--;
        // -KM- 1999/01/31 Explode objects over remove them.
        // -AJA- 2000/02/01: Remove now the default.
        if (attack->flags_ & kAttackFlagKillFailedSpawn)
        {
            KillMapObject(parent, child, nullptr);
            if (child->flags_ & kMapObjectFlagCountKill)
                players[console_player]->kill_count_--;
        }
        else
            RemoveMapObject(child);

        return;
    }

    if (attack->sound_)
        StartSoundEffect(attack->sound_, AttackSfxCat(parent), parent);

    // If the object cannot move from its position, remove it or kill it.
    if (!TryMove(child, child->x, child->y))
    {
        if (child->flags_ & kMapObjectFlagCountKill)
            intermission_stats.kills--;
        if (child->flags_ & kMapObjectFlagCountItem)
            intermission_stats.items--;
        if (attack->flags_ & kAttackFlagKillFailedSpawn)
        {
            KillMapObject(parent, child, nullptr);
            if (child->flags_ & kMapObjectFlagCountKill)
                players[console_player]->kill_count_--;
        }
        else
            RemoveMapObject(child);

        return;
    }

    if (!(attack->flags_ & kAttackFlagNoTarget))
        child->SetTarget(parent->target_);

    child->SetSupportObject(parent);

    child->side_ = parent->side_;

    // -AJA- 2004/09/27: keep ambush status of parent
    child->flags_ |= (parent->flags_ & kMapObjectFlagAmbush);

    // -AJA- 1999/09/25: Set the initial direction & momentum when
    //       the ANGLED_SPAWN attack special is used.
    if (attack->flags_ & kAttackFlagAngledSpawn)
        MapObjectSetDirectionAndSpeed(child, angle, slope, attack->assault_speed_);

    MapObjectSetStateDeferred(child, attack->objinitstate_, 0);
}

//
// A_ObjectTripleSpawn
//
// Spawns three objects at 90, 180 and 270 degrees. This is essentially
// another purist function to support the death sequence of the Pain
// elemental. However it could be used as in conjunction with radius
// triggers to generate a nice teleport spawn invasion.
//
// -ACB- 1998/08/23 (I think....)
//

static void ObjectTripleSpawn(MapObject *object)
{
    ObjectSpawning(object, object->angle_ + kBAMAngle90);
    ObjectSpawning(object, object->angle_ + kBAMAngle180);
    ObjectSpawning(object, object->angle_ + kBAMAngle270);
}

//
// A_ObjectDoubleSpawn
//
// Spawns two objects at 90 and 270 degrees.
// Like the death sequence of the Pain
// elemental.
//
// Lobo: 2021 to mimic the Doom64 pain elemental
//

static void ObjectDoubleSpawn(MapObject *object)
{
    ObjectSpawning(object, object->angle_ + kBAMAngle90);
    ObjectSpawning(object, object->angle_ + kBAMAngle270);
}

//-------------------------------------------------------------------
//-------------------SKULLFLY HANDLING ROUTINES----------------------
//-------------------------------------------------------------------

//
// SkullFlyAssault
//
// This is the attack procedure for objects that launch themselves
// at their target like a missile.
//
// -ACB- 1998/08/16
//
static void SkullFlyAssault(MapObject *object)
{
    if (!object->current_attack_)
        return;

    if (!object->target_ && !object->player_)
    {
        // -AJA- 2000/09/29: fix for the zombie lost soul bug
        // -AJA- 2000/10/22: monsters only !  Don't stuff up gibs/missiles.
        if (object->extended_flags_ & kExtendedFlagMonster)
            object->flags_ |= kMapObjectFlagSkullFly;
        return;
    }

    float speed = object->current_attack_->assault_speed_;

    SoundEffect *sound = object->current_attack_->initsound_;

    if (sound)
        StartSoundEffect(sound, GetSoundEffectCategory(object), object);

    object->flags_ |= kMapObjectFlagSkullFly;

    // determine destination
    float tx, ty, tz;

    TargetTheory(object, object->target_, &tx, &ty, &tz);

    float slope = ApproximateSlope(tx - object->x, ty - object->y, tz - object->z);

    MapObjectSetDirectionAndSpeed(object, object->angle_, slope, speed);
}

//
// SlammedIntoObject
//
// Used when a flying object hammers into another object when on the
// attack. Replaces the code in PIT_Checkthing.
//
// -ACB- 1998/07/29: Written
//
// -AJA- 1999/09/12: Now uses P_SetMobjStateDeferred, since this
//                   routine can be called by
//                   TryMove/CheckRelativeThingCallback.
//
void SlammedIntoObject(MapObject *object, MapObject *target)
{
    if (object->current_attack_)
    {
        if (target != nullptr)
        {
            // -KM- 1999/01/31 Only hurt shootable objects...
            if (target->flags_ & kMapObjectFlagShootable)
            {
                float damage;

                EDGE_DAMAGE_COMPUTE(damage, &object->current_attack_->damage_);

                if (damage)
                    DamageMapObject(target, object, object, damage, &object->current_attack_->damage_);
            }
        }

        SoundEffect *sound = object->current_attack_->sound_;
        if (sound)
            StartSoundEffect(sound, GetSoundEffectCategory(object), object);
    }

    object->flags_ &= ~kMapObjectFlagSkullFly;
    object->momentum_.X = object->momentum_.Y = object->momentum_.Z = 0;

    MapObjectSetStateDeferred(object, object->info_->idle_state_, 0);
}

bool UseThing(MapObject *user, MapObject *thing, float open_bottom, float open_top)
{
    // Called when this thing is attempted to be used (i.e. by pressing
    // the spacebar near it) by the player.  Returns true if successfully
    // used, or false if other things should be checked.

    // item is disarmed ?
    if (!(thing->flags_ & kMapObjectFlagTouchy))
        return false;

    // can be reached ?
    open_top    = HMM_MIN(open_top, thing->z + thing->height_);
    open_bottom = HMM_MAX(open_bottom, thing->z);

    if (user->z >= open_top || (user->z + user->height_ + kUseZRange < open_bottom))
        return false;

    // OK, disarm and put into touch states
    EPI_ASSERT(thing->info_->touch_state_ > 0);

    thing->flags_ &= ~kMapObjectFlagTouchy;
    MapObjectSetStateDeferred(thing, thing->info_->touch_state_, 0);

    return true;
}

void TouchyContact(MapObject *touchy, MapObject *victim)
{
    // Used whenever a thing comes into contact with a TOUCHY object.
    //
    // -AJA- 1999/09/12: Now uses P_SetMobjStateDeferred, since this
    //       routine can be called by TryMove/CheckRelativeThingCallback.

    // dead thing touching. Can happen with a sliding player corpse.
    if (victim->health_ <= 0)
        return;

    // don't harm the grenadier...
    if (touchy->source_ == victim)
        return;

    touchy->SetTarget(victim);
    touchy->flags_ &= ~kMapObjectFlagTouchy; // disarm

    if (touchy->info_->touch_state_)
        MapObjectSetStateDeferred(touchy, touchy->info_->touch_state_, 0);
    else
        ExplodeMissile(touchy);
}

void A_TouchyRearm(MapObject *touchy)
{
    touchy->flags_ |= kMapObjectFlagTouchy;
}

void A_TouchyDisarm(MapObject *touchy)
{
    touchy->flags_ &= ~kMapObjectFlagTouchy;
}

void A_BounceRearm(MapObject *mo)
{
    mo->extended_flags_ &= ~kExtendedFlagJustBounced;
}

void A_BounceDisarm(MapObject *mo)
{
    mo->extended_flags_ |= kExtendedFlagJustBounced;
}

void A_DropItem(MapObject *mo)
{
    const MapObjectDefinition *info = mo->info_->dropitem_;

    if (mo->state_ && mo->state_->action_par)
    {
        MobjStringReference *ref = (MobjStringReference *)mo->state_->action_par;

        info = ref->GetRef();
    }

    if (!info)
    {
        WarningOrError("A_DropItem: %s specifies no item to drop.\n", mo->info_->name_.c_str());
        return;
    }

    // unlike normal drops, these ones are displaced randomly

    float dx = RandomByteSkewToZeroDeterministic() * mo->info_->radius_ / 255.0f;
    float dy = RandomByteSkewToZeroDeterministic() * mo->info_->radius_ / 255.0f;

    MapObject *item = CreateMapObject(mo->x + dx, mo->y + dy, mo->floor_z_, info);
    EPI_ASSERT(item);

    item->flags_ |= kMapObjectFlagDropped;
    item->flags_ &= ~kMapObjectFlagSolid;

    item->angle_ = mo->angle_;

    // allow respawning
    item->spawnpoint_.x              = item->x;
    item->spawnpoint_.y              = item->y;
    item->spawnpoint_.z              = item->z;
    item->spawnpoint_.angle          = item->angle_;
    item->spawnpoint_.vertical_angle = item->vertical_angle_;
    item->spawnpoint_.info           = info;
    item->spawnpoint_.flags          = 0;
}

void A_Spawn(MapObject *mo)
{
    if (!mo->state_ || !mo->state_->action_par)
        FatalError("SPAWN() action used without a object name!\n");

    MobjStringReference *ref = (MobjStringReference *)mo->state_->action_par;

    const MapObjectDefinition *info = ref->GetRef();
    EPI_ASSERT(info);

    MapObject *item = CreateMapObject(mo->x, mo->y, mo->z, info);
    EPI_ASSERT(item);

    item->angle_ = mo->angle_;
    item->side_  = mo->side_;

    item->SetRealSource(mo);
    item->SetSpawnSource(mo);
}

void A_PathCheck(MapObject *mo)
{
    // Checks if the creature is a path follower, and if so enters the
    // meander states.

    if (!mo->path_trigger_ || !mo->info_->meander_state_)
        return;

    MapObjectSetStateDeferred(mo, mo->info_->meander_state_, 0);

    mo->move_direction_ = kDirectionSlowTurn;
    mo->move_count_     = 0;
}

void A_PathFollow(MapObject *mo)
{
    // For path-following creatures (spawned via RTS), makes the creature
    // follow the path by trying to get to the next node.

    if (!mo->path_trigger_)
        return;

    if (ScriptUpdatePath(mo))
    {
        // reached the very last one ?
        if (!mo->path_trigger_)
        {
            mo->move_direction_ = kDirectionNone;
            return;
        }

        mo->move_direction_ = kDirectionSlowTurn;
        return;
    }

    float dx = mo->path_trigger_->x - mo->x;
    float dy = mo->path_trigger_->y - mo->y;

    BAMAngle diff = PointToAngle(0, 0, dx, dy) - mo->angle_;

    // movedir value:
    //   0 for slow turning.
    //   1 for fast turning.
    //   2 for walking.
    //   3 for evasive maneouvres.

    // if (mo->movedir < 2)

    if (mo->move_direction_ == kDirectionSlowTurn || mo->move_direction_ == kDirectionFastTurn)
    {
        if (diff > kBAMAngle15 && diff < (kBAMAngle360 - kBAMAngle15))
        {
            BAMAngle step = kBAMAngle30;

            if (diff < kBAMAngle180)
                mo->angle_ += RandomByteDeterministic() * (step >> 8);
            else
                mo->angle_ -= RandomByteDeterministic() * (step >> 8);

            return;
        }

        // we are now facing the next node
        mo->angle_ += diff;
        mo->move_direction_ = kDirectionWalking;

        diff = 0;
    }

    if (mo->move_direction_ == kDirectionWalking)
    {
        if (diff < kBAMAngle30)
            mo->angle_ += kBAMAngle1 * 2;
        else if (diff > (kBAMAngle360 - kBAMAngle30))
            mo->angle_ -= kBAMAngle1 * 2;
        else
            mo->move_direction_ = kDirectionSlowTurn;

        if (!DoMove(mo, true))
        {
            mo->move_direction_ = kDirectionEvasive;
            mo->angle_          = RandomByteDeterministic() << (kBAMAngleBits - 8);
            mo->move_count_     = 1 + (RandomByteDeterministic() & 7);
        }
        return;
    }

    // make evasive maneouvres
    mo->move_count_--;

    if (mo->move_count_ <= 0)
    {
        mo->move_direction_ = kDirectionFastTurn;
        return;
    }

    DoMove(mo, true);
}

//-------------------------------------------------------------------
//--------------------ATTACK HANDLING PROCEDURES---------------------
//-------------------------------------------------------------------

//
// P_DoAttack
//
// When an object goes on the attack, it current attack is handled here;
// the attack type is discerned and the assault is launched.
//
// -ACB- 1998/08/07
//
static void P_DoAttack(MapObject *object)
{
    const AttackDefinition *attack = object->current_attack_;

    EPI_ASSERT(attack);

    switch (attack->attackstyle_)
    {
    case kAttackStyleCloseCombat: {
        DoMeleeAttack(object);
        break;
    }

    case kAttackStyleProjectile: {
        LaunchProjectile(object, object->target_, attack->atk_mobj_);
        break;
    }

    case kAttackStyleSmartProjectile: {
        LaunchSmartProjectile(object, object->target_, attack->atk_mobj_);
        break;
    }

    case kAttackStyleRandomSpread: {
        LaunchRandomSpread(object);
        break;
    }

    case kAttackStyleShootToSpot: {
        ShootToSpot(object);
        break;
    }

    case kAttackStyleShot: {
        ShotAttack(object);
        break;
    }

    case kAttackStyleSkullFly: {
        SkullFlyAssault(object);
        break;
    }

    case kAttackStyleSpawner: {
        ObjectSpawning(object, object->angle_);
        break;
    }

    case kAttackStyleSpreader: {
        LaunchOrderedSpread(object);
        break;
    }

    case kAttackStyleTracker: {
        LaunchTracker(object);
        break;
    }

    case kAttackStylePsychic: {
        LaunchTracker(object);
        A_PsychicEffect(object);
        break;
    }

    // Lobo 2021: added doublespawner like the Doom64 elemental
    case kAttackStyleDoubleSpawner: {
        ObjectDoubleSpawn(object);
        break;
    }

    case kAttackStyleTripleSpawner: {
        ObjectTripleSpawn(object);
        break;
    }

    // -KM- 1998/11/25 Added spray attack
    case kAttackStyleSpray: {
        SprayAttack(object);
        break;
    }

    default: // THIS SHOULD NOT HAPPEN
    {
        if (strict_errors)
            FatalError("P_DoAttack: %s has an unknown attack type.\n", object->info_->name_.c_str());
        break;
    }
    }
}

//
// A_ComboAttack
//
// This is called at end of a set of states that can result in
// either a closecombat_ or ranged attack. The procedure checks
// to see if the target is within melee range and picks the
// approiate attack.
//
// -ACB- 1998/08/07
//
void A_ComboAttack(MapObject *object)
{
    const AttackDefinition *attack;

    if (!object->target_)
        return;

    if (DecideMeleeAttack(object, object->info_->closecombat_))
        attack = object->info_->closecombat_;
    else
        attack = object->info_->rangeattack_;

    if (attack)
    {
        if (attack->flags_ & kAttackFlagFaceTarget)
            A_FaceTarget(object);

        if (attack->flags_ & kAttackFlagNeedSight)
        {
            if (!CheckSight(object, object->target_))
                return;
        }

        object->current_attack_ = attack;
        P_DoAttack(object);
    }
#ifdef DEVELOPERS
    else
    {
        if (!object->info_->closecombat_)
            WarningOrError("%s hasn't got a close combat attack\n", object->info_->name.c_str());
        else
            WarningOrError("%s hasn't got a range attack\n", object->info_->name.c_str());
    }
#endif
}

//
// A_MeleeAttack
//
// Setup a close combat assault
//
// -ACB- 1998/08/07
//
void A_MeleeAttack(MapObject *object)
{
    const AttackDefinition *attack;

    attack = object->info_->closecombat_;

    // -AJA- 1999/08/10: Multiple attack support.
    if (object->state_ && object->state_->action_par)
        attack = (const AttackDefinition *)object->state_->action_par;

    if (!attack)
    {
        WarningOrError("A_MeleeAttack: %s has no close combat attack.\n", object->info_->name_.c_str());
        return;
    }

    if (attack->flags_ & kAttackFlagFaceTarget)
        A_FaceTarget(object);

    if (attack->flags_ & kAttackFlagNeedSight)
    {
        if (!object->target_ || !CheckSight(object, object->target_))
            return;
    }

    object->current_attack_ = attack;
    P_DoAttack(object);
}

//
// A_RangeAttack
//
// Setup an attack at range
//
// -ACB- 1998/08/07
//
void A_RangeAttack(MapObject *object)
{
    const AttackDefinition *attack;

    attack = object->info_->rangeattack_;

    // -AJA- 1999/08/10: Multiple attack support.
    if (object->state_ && object->state_->action_par)
        attack = (const AttackDefinition *)object->state_->action_par;

    if (!attack)
    {
        WarningOrError("A_RangeAttack: %s hasn't got a range attack.\n", object->info_->name_.c_str());
        return;
    }

    if (attack->flags_ & kAttackFlagFaceTarget)
        A_FaceTarget(object);

    if (attack->flags_ & kAttackFlagNeedSight)
    {
        if (!object->target_ || !CheckSight(object, object->target_))
            return;
    }

    object->current_attack_ = attack;
    P_DoAttack(object);
}

//
// A_SpareAttack
//
// Setup an attack that is not defined as close or range. can be
// used to act as a follow attack for close or range, if you want one to
// add to the others.
//
// -ACB- 1998/08/24
//
void A_SpareAttack(MapObject *object)
{
    const AttackDefinition *attack;

    attack = object->info_->spareattack_;

    // -AJA- 1999/08/10: Multiple attack support.
    if (object->state_ && object->state_->action_par)
        attack = (const AttackDefinition *)object->state_->action_par;

    if (attack)
    {
        if ((attack->flags_ & kAttackFlagFaceTarget) && object->target_)
            A_FaceTarget(object);

        if ((attack->flags_ & kAttackFlagNeedSight) && object->target_)
        {
            if (!CheckSight(object, object->target_))
                return;
        }

        object->current_attack_ = attack;
        P_DoAttack(object);
    }
#ifdef DEVELOPERS
    else
    {
        WarningOrError("A_SpareAttack: %s hasn't got a spare attack\n", object->info_->name.c_str());
        return;
    }
#endif
}

//
// A_RefireCheck
//
// This procedure will be called inbetween firing on an object
// that will fire repeatly (Chaingunner/Arachontron etc...), the
// purpose of this is to see if the object should refire and
// performs checks to that effect, first there is a check to see
// if the object will keep firing regardless and the others
// check if the the target exists, is alive and within view. The
// only other code here is a stealth check: a object with stealth
// capabilitys will lose the ability while firing.
//
// -ACB- 1998/08/10
//
void A_RefireCheck(MapObject *object)
{
    MapObject              *target;
    const AttackDefinition *attack;

    attack = object->current_attack_;

    if (!attack)
        return;

    if (attack->flags_ & kAttackFlagFaceTarget)
        A_FaceTarget(object);

    // Random chance that object will keep firing regardless
    if (RandomByteTestDeterministic(attack->keepfirechance_))
        return;

    target = object->target_;

    if (!target || (target->health_ <= 0) || !CheckSight(object, target))
    {
        if (object->info_->chase_state_)
            MapObjectSetStateDeferred(object, object->info_->chase_state_, 0);
    }
    else if (object->flags_ & kMapObjectFlagStealth)
    {
        object->target_visibility_ = 1.0f;
    }
}

//
// A_ReloadCheck
//
// Enter reload states if the monster has shot a certain number of
// shots (given by RELOAD_SHOTS command).
//
// -AJA- 2004/11/15: added this.
//
void A_ReloadCheck(MapObject *object)
{
    object->shot_count_++;

    if (object->shot_count_ >= object->info_->reload_shots_)
    {
        object->shot_count_ = 0;

        if (object->info_->reload_state_)
            MapObjectSetStateDeferred(object, object->info_->reload_state_, 0);
    }
}

void A_ReloadReset(MapObject *object)
{
    object->shot_count_ = 0;
}

//---------------------------------------------
//-----------LOOKING AND CHASING---------------
//---------------------------------------------

extern MapObject **blockmap_things;

//
// CreateAggression
//
// Sets an object up to target a previously stored object.
//
// -ACB- 2000/06/20 Re-written and Simplified
//
// -AJA- 2009/07/05 Rewritten again, using the blockmap
//
static bool CreateAggression(MapObject *mo)
{
    if (mo->target_ && mo->target_->health_ > 0)
        return false;

    // pick a block in blockmap to check
    int bdx = RandomByteSkewToZeroDeterministic() / 17;
    int bdy = RandomByteSkewToZeroDeterministic() / 17;

    int block_x = BlockmapGetX(mo->x) + bdx;
    int block_y = BlockmapGetX(mo->y) + bdy;

    block_x = abs(block_x + blockmap_width) % blockmap_width;
    block_y = abs(block_y + blockmap_height) % blockmap_height;

    //  LogDebug("BLOCKMAP POS: %3d %3d  (size: %d %d)\n", block_x, block_y,
    //  blockmap_width, blockmap_height);

    int bnum = block_y * blockmap_width + block_x;

    for (MapObject *other = blockmap_things[bnum]; other; other = other->blockmap_next_)
    {
        if (!(other->info_->extended_flags_ & kExtendedFlagMonster) || other->health_ <= 0)
            continue;

        if (other == mo)
            continue;

        if (other->info_ == mo->info_)
        {
            if (!(other->info_->extended_flags_ & kExtendedFlagDisloyalToOwnType))
                continue;

            // Type the same and it can't hurt own kind - not good.
            if (!(other->info_->extended_flags_ & kExtendedFlagOwnAttackHurts))
                continue;
        }

        // don't attack a friend if we cannot hurt them.
        // -AJA- I'm assuming that even friends will 'infight'.
        if ((mo->info_->side_ & other->info_->side_) != 0 &&
            (other->info_->hyper_flags_ & (kHyperFlagFriendlyFireImmune | kHyperFlagUltraLoyal)))
        {
            continue;
        }

        // MBF21: If in same infighting group, never target each other even if
        // hit with 'friendly fire'
        if (mo->info_->infight_group_ > 0 && other->info_->infight_group_ > 0 &&
            (mo->info_->infight_group_ == other->info_->infight_group_))
        {
            continue;
        }

        // POTENTIAL TARGET

        // fairly low chance of trying it, in case this block
        // contains many monsters (spread the love)
        if (RandomByteDeterministic() > 99)
            continue;

        // sight check is expensive, do it last
        if (!CheckSight(mo, other))
            continue;

        // OK, you got me
        mo->SetTarget(other);

        LogDebug("Created aggression : %s --> %s\n", mo->info_->name_.c_str(), other->info_->name_.c_str());

        if (mo->info_->seesound_)
            StartSoundEffect(mo->info_->seesound_, GetSoundEffectCategory(mo), mo, SfxFlags(mo->info_));

        if (mo->info_->chase_state_)
            MapObjectSetStateDeferred(mo, mo->info_->chase_state_, 0);

        return true;
    }

    return false;
}

//
// A_StandardLook
//
// Standard Lookout procedure
//
// -ACB- 1998/08/22
//
void A_StandardLook(MapObject *object)
{
    int        targ_pnum;
    MapObject *targ = nullptr;

    object->threshold_ = 0; // any shot will wake up

    // FIXME: replace with cvar/Menu toggle
    bool CVAR_DOOM_TARGETTING = false;

    if (CVAR_DOOM_TARGETTING == true)
        targ_pnum = object->subsector_->sector->sound_player; // old way
    else
        targ_pnum = object->last_heard_;                      // new way

    if (targ_pnum >= 0 && targ_pnum < kMaximumPlayers && players[targ_pnum])
    {
        targ = players[targ_pnum]->map_object_;
    }

    // -AJA- 2004/09/02: ignore the sound of a friend
    // FIXME: maybe wake up and support that player ??
    // if (targ && (targ->side & object->side) != 0)
    if (object->side_ != 0)
    {
        // targ = nullptr;
        // A_PlayerSupportLook(object);

        // Dasho - MBF21 testing - A_PlayerSupportMeander was really jacking
        // up projectiles and spawned objects with the MF_FRIENDLY MBF flag
        // A_PlayerSupportMeander(object);
        A_FriendLook(object);
        return;
    }

    if (object->flags_ & kMapObjectFlagStealth)
        object->target_visibility_ = 1.0f;

    if (force_infighting.d_)
        if (CreateAggression(object) || CreateAggression(object))
            return;

    if (targ && (targ->flags_ & kMapObjectFlagShootable))
    {
        object->SetTarget(targ);

        if (object->flags_ & kMapObjectFlagAmbush)
        {
            if (!CheckSight(object, object->target_) && !LookForPlayers(object, object->info_->sight_angle_))
                return;
        }
    }
    else
    {
        if (!LookForPlayers(object, object->info_->sight_angle_))
            return;
    }

    if (object->info_->seesound_)
    {
        StartSoundEffect(object->info_->seesound_, GetSoundEffectCategory(object), object, SfxFlags(object->info_));
    }

    // -AJA- this will remove objects which have no chase states.
    // For compatibility with original DOOM.
    MapObjectSetStateDeferred(object, object->info_->chase_state_, 0);
}

//
// A_PlayerSupportLook
//
// Player Support Lookout procedure
//
// -ACB- 1998/09/05
//
void A_PlayerSupportLook(MapObject *object)
{
    object->threshold_ = 0; // any shot will wake up

    if (object->flags_ & kMapObjectFlagStealth)
        object->target_visibility_ = 1.0f;

    if (!object->support_object_)
    {
        if (!A_LookForTargets(object))
            return;

        // -AJA- 2004/09/02: join the player's side
        if (object->side_ == 0)
            object->side_ = object->target_->side_;

        if (object->info_->seesound_)
        {
            StartSoundEffect(object->info_->seesound_, GetSoundEffectCategory(object), object, SfxFlags(object->info_));
        }
    }

    if (object->info_->meander_state_)
        MapObjectSetStateDeferred(object, object->info_->meander_state_, 0);
}

//
// A_StandardMeander
//
void A_StandardMeander(MapObject *object)
{
    int delta;

    object->threshold_ = 0; // any shot will wake up

    // move within supporting distance of player
    if (--object->move_count_ < 0 || !DoMove(object, false))
        NewChaseDir(object);

    // turn towards movement direction if not there yet
    if (object->move_direction_ < kDirectionNone)
    {
        object->angle_ &= (7 << 29);
        delta = object->angle_ - (object->move_direction_ << 29);

        if (delta > 0)
            object->angle_ -= kBAMAngle45;
        else if (delta < 0)
            object->angle_ += kBAMAngle45;
    }
}

//
// A_PlayerSupportMeander
//
void A_PlayerSupportMeander(MapObject *object)
{
    int delta;

    object->threshold_ = 0; // any shot will wake up

    // move within supporting distance of player
    if (--object->move_count_ < 0 || !DoMove(object, false))
        NewChaseDir(object);

    // turn towards movement direction if not there yet
    if (object->move_direction_ < kDirectionNone)
    {
        object->angle_ &= (7 << 29);
        delta = object->angle_ - (object->move_direction_ << 29);

        if (delta > 0)
            object->angle_ -= kBAMAngle45;
        else if (delta < 0)
            object->angle_ += kBAMAngle45;
    }

    A_LookForTargets(object);
}

//
// A_StandardChase
//
// Standard AI Chase Procedure
//
// -ACB- 1998/08/22 Procedure Written
// -ACB- 1998/09/05 Added Support Object Check
//
void A_StandardChase(MapObject *object)
{
    int          delta;
    SoundEffect *sound;

    if (object->reaction_time_)
        object->reaction_time_--;

    // object has a pain threshold, while this is true, reduce it. while
    // the threshold is true, the object will remain intent on its target.
    if (object->threshold_)
    {
        if (!object->target_ || object->target_->health_ <= 0)
            object->threshold_ = 0;
        else
            object->threshold_--;
    }

    // A Chasing Stealth Creature becomes less visible
    if (object->flags_ & kMapObjectFlagStealth)
        object->target_visibility_ = 0.0f;

    // turn towards movement direction if not there yet
    if (object->move_direction_ < kDirectionNone)
    {
        object->angle_ &= (7 << 29);
        delta = object->angle_ - (object->move_direction_ << 29);

        if (delta > 0)
            object->angle_ -= kBAMAngle45;
        else if (delta < 0)
            object->angle_ += kBAMAngle45;
    }

    if (!object->target_ || !(object->target_->flags_ & kMapObjectFlagShootable))
    {
        if (A_LookForTargets(object))
            return;

        // -ACB- 1998/09/06 Target is not relevant: nullptrify.
        object->SetTarget(nullptr);

        MapObjectSetStateDeferred(object, object->info_->idle_state_, 0);
        return;
    }

    // do not attack twice in a row
    if (object->flags_ & kMapObjectFlagJustAttacked)
    {
        object->flags_ &= ~kMapObjectFlagJustAttacked;

        // -KM- 1998/12/16 Nightmare mode set the fast parm.
        if (!level_flags.fast_monsters)
            NewChaseDir(object);

        return;
    }

    sound = object->info_->attacksound_;

    // check for melee attack
    if (object->info_->melee_state_ && DecideMeleeAttack(object, object->info_->closecombat_))
    {
        if (sound)
            StartSoundEffect(sound, GetSoundEffectCategory(object), object);

        if (object->info_->melee_state_)
            MapObjectSetStateDeferred(object, object->info_->melee_state_, 0);
        return;
    }

    // check for missile attack
    if (object->info_->missile_state_)
    {
        // -KM- 1998/12/16 Nightmare set the fast_monsters.
        if (!(!level_flags.fast_monsters && object->move_count_))
        {
            if (DecideRangeAttack(object))
            {
                if (object->info_->missile_state_)
                    MapObjectSetStateDeferred(object, object->info_->missile_state_, 0);
                object->flags_ |= kMapObjectFlagJustAttacked;
                return;
            }
        }
    }

    // possibly choose another target
    // -ACB- 1998/09/05 Object->support->object check, go for new targets
    if (!CheckSight(object, object->target_) && !object->threshold_)
    {
        if (A_LookForTargets(object))
            return;
    }

    // chase towards player
    if (--object->move_count_ < 0 || !DoMove(object, false))
        NewChaseDir(object);

    // make active sound
    if (object->info_->activesound_ && RandomByte() < 3)
        StartSoundEffect(object->info_->activesound_, GetSoundEffectCategory(object), object);
}

//
// A_ResurrectChase
//
// Before undertaking the standard chase procedure, the object
// will check for a nearby corpse and raises one if it exists.
//
// -ACB- 1998/09/05 Support Check: Raised object supports raiser's supportobj
//
void A_ResurrectChase(MapObject *object)
{
    MapObject *corpse;

    corpse = FindCorpseForResurrection(object);

    if (corpse)
    {
        object->angle_ = PointToAngle(object->x, object->y, corpse->x, corpse->y);
        if (object->info_->res_state_)
            MapObjectSetStateDeferred(object, object->info_->res_state_, 0);

        // corpses without raise states should be skipped
        EPI_ASSERT(corpse->info_->raise_state_);

        BringCorpseToLife(corpse);

        // -ACB- 1998/09/05 Support Check: Res creatures to support that object
        if (object->support_object_)
        {
            corpse->SetSupportObject(object->support_object_);
            corpse->SetTarget(object->target_);
        }
        else
        {
            corpse->SetSupportObject(nullptr);
            corpse->SetTarget(nullptr);
        }

        // -AJA- Resurrected creatures are on Archvile's side (like MBF)
        corpse->side_ = object->side_;
        return;
    }

    A_StandardChase(object);
}

//
// A_WalkSoundChase
//
// Make a sound and then chase...
//
void A_WalkSoundChase(MapObject *object)
{
    if (!object->info_->walksound_)
    {
        if (strict_errors)
            FatalError("WALKSOUND_CHASE: %s hasn't got a walksound_.\n", object->info_->name_.c_str());
        return;
    }

    StartSoundEffect(object->info_->walksound_, GetSoundEffectCategory(object), object);
    A_StandardChase(object);
}

void A_Die(MapObject *mo)
{
    // Boom/MBF compatibility.
    DamageMapObject(mo, nullptr, nullptr, mo->health_);
}

void A_KeenDie(MapObject *mo)
{
    A_MakeIntoCorpse(mo);

    // see if all other Keens are dead
    for (MapObject *cur = map_object_list_head; cur != nullptr; cur = cur->next_)
    {
        if (cur == mo)
            continue;

        if (cur->info_ != mo->info_)
            continue;

        if (cur->health_ > 0)
            return; // other Keen not dead
    }

    LogDebug("A_KeenDie: ALL DEAD, activating...\n");

    RemoteActivation(nullptr, 2 /* door type */, 666 /* tag */, 0, kLineTriggerAny);
}

void A_CheckMoving(MapObject *mo)
{
    // -KM- 1999/01/31 Returns a player to spawnstate when not moving.

    Player *pl = mo->player_;

    if (pl)
    {
        if (pl->actual_speed_ < kPlayerStopSpeed)
        {
            MapObjectSetStateDeferred(mo, mo->info_->idle_state_, 0);

            // we delay a little bit, in order to prevent a loop where
            // CHECK_ACTIVITY jumps to SWIM states (for example) and
            // then CHECK_MOVING jumps right back to IDLE states.
            mo->tics_ = 2;
        }
        return;
    }

    if (fabs(mo->momentum_.X) < kStopSpeed && fabs(mo->momentum_.Y) < kStopSpeed)
    {
        mo->momentum_.X = mo->momentum_.Y = 0;
        MapObjectSetStateDeferred(mo, mo->info_->idle_state_, 0);
    }
}

void A_CheckActivity(MapObject *mo)
{
    Player *pl = mo->player_;

    if (!pl)
        return;

    if (pl->swimming_)
    {
        // enter the SWIM states (if present)
        int swim_st = MapObjectFindLabel(pl->map_object_, "SWIM");

        if (swim_st == 0)
            swim_st = pl->map_object_->info_->chase_state_;

        if (swim_st != 0)
            MapObjectSetStateDeferred(pl->map_object_, swim_st, 0);

        return;
    }

    if (pl->powers_[kPowerTypeJetpack] > 0)
    {
        // enter the FLY states (if present)
        int fly_st = MapObjectFindLabel(pl->map_object_, "FLY");

        if (fly_st != 0)
            MapObjectSetStateDeferred(pl->map_object_, fly_st, 0);

        return;
    }

    if (mo->on_ladder_ >= 0)
    {
        // enter the CLIMB states (if present)
        int climb_st = MapObjectFindLabel(pl->map_object_, "CLIMB");

        if (climb_st != 0)
            MapObjectSetStateDeferred(pl->map_object_, climb_st, 0);

        return;
    }

    // Lobo 2022: use crouch states if we have them and we are, you know,
    // crouching ;)
    if (pl->map_object_->extended_flags_ & kExtendedFlagCrouching)
    {
        // enter the CROUCH states (if present)
        int crouch_st = MapObjectFindLabel(pl->map_object_, "CROUCH");

        if (crouch_st != 0)
            MapObjectSetStateDeferred(pl->map_object_, crouch_st, 0);

        return;
    }

    /* Otherwise: do nothing */
}

void A_CheckBlood(MapObject *mo)
{
    // -KM- 1999/01/31 Part of the extra blood option, makes blood stick
    // around... -AJA- 1999/10/02: ...but not indefinitely.

    if (level_flags.more_blood && mo->tics_ >= 0)
    {
        int val = RandomByteDeterministic();

        // exponential formula
        mo->tics_ = ((val * val * val) >> 18) * kTicRate + kTicRate;
    }
}

void A_Jump(MapObject *mo)
{
    // Jumps to the given label, possibly randomly.
    //
    // Note: nothing to do with monsters physically jumping.

    if (!mo->state_ || !mo->state_->action_par)
    {
        WarningOrError("JUMP action used in [%s] without a label !\n", mo->info_->name_.c_str());
        return;
    }

    JumpActionInfo *jump = (JumpActionInfo *)mo->state_->action_par;

    EPI_ASSERT(jump->chance >= 0);
    EPI_ASSERT(jump->chance <= 1);

    if (RandomByteTestDeterministic(jump->chance))
    {
        mo->next_state_ = (mo->state_->jumpstate == 0) ? nullptr : (states + mo->state_->jumpstate);
    }
}

void A_JumpLiquid(MapObject *mo)
{
    // Jumps to the given label, possibly randomly.
    //
    // Note: nothing to do with monsters physically jumping.

    if (!P_IsThingOnLiquidFloor(mo)) // Are we touching a liquid floor?
    {
        return;
    }

    if (!mo->state_ || !mo->state_->action_par)
    {
        WarningOrError("JUMP_LIQUID action used in [%s] without a label !\n", mo->info_->name_.c_str());
        return;
    }

    JumpActionInfo *jump = (JumpActionInfo *)mo->state_->action_par;

    EPI_ASSERT(jump->chance >= 0);
    EPI_ASSERT(jump->chance <= 1);

    if (RandomByteTestDeterministic(jump->chance))
    {
        mo->next_state_ = (mo->state_->jumpstate == 0) ? nullptr : (states + mo->state_->jumpstate);
    }
}

void A_JumpSky(MapObject *mo)
{
    // Jumps to the given label, possibly randomly.
    //
    // Note: nothing to do with monsters physically jumping.

    if (mo->subsector_->sector->ceiling.image != sky_flat_image) // is it outdoors?
    {
        return;
    }
    if (!mo->state_ || !mo->state_->action_par)
    {
        WarningOrError("JUMP_SKY action used in [%s] without a label !\n", mo->info_->name_.c_str());
        return;
    }

    JumpActionInfo *jump = (JumpActionInfo *)mo->state_->action_par;

    EPI_ASSERT(jump->chance >= 0);
    EPI_ASSERT(jump->chance <= 1);

    if (RandomByteTestDeterministic(jump->chance))
    {
        mo->next_state_ = (mo->state_->jumpstate == 0) ? nullptr : (states + mo->state_->jumpstate);
    }
}

void A_SetInvuln(MapObject *mo)
{
    mo->hyper_flags_ |= kHyperFlagInvulnerable;
}

void A_ClearInvuln(MapObject *mo)
{
    mo->hyper_flags_ &= ~kHyperFlagInvulnerable;
}

void A_Become(MapObject *mo)
{
    if (!mo->state_ || !mo->state_->action_par)
    {
        FatalError("BECOME action used in [%s] without arguments!\n", mo->info_->name_.c_str());
    }

    BecomeActionInfo *become = (BecomeActionInfo *)mo->state_->action_par;

    if (!become->info_)
    {
        become->info_ = mobjtypes.Lookup(become->info_ref_.c_str());
        EPI_ASSERT(become->info_); // lookup should be OK (fatal error if not found)
    }

    // DO THE DEED !!
    mo->pre_become_ = mo->info_; // store what we used to be

    UnsetThingPosition(mo);
    {
        mo->info_ = become->info_;

        mo->morph_timeout_ = mo->info_->morphtimeout_;

        // Note: health is not changed
        mo->radius_ = mo->info_->radius_;
        mo->height_ = mo->info_->height_;
        if (mo->info_->fast_speed_ > -1 && level_flags.fast_monsters)
            mo->speed_ = mo->info_->fast_speed_;
        else
            mo->speed_ = mo->info_->speed_;

        if (mo->flags_ & kMapObjectFlagAmbush) // preserve map editor AMBUSH flag
        {
            mo->flags_ = mo->info_->flags_;
            mo->flags_ |= kMapObjectFlagAmbush;
        }
        else
            mo->flags_ = mo->info_->flags_;

        mo->extended_flags_ = mo->info_->extended_flags_;
        mo->hyper_flags_    = mo->info_->hyper_flags_;

        mo->target_visibility_ = mo->info_->translucency_;
        mo->current_attack_    = nullptr;
        mo->model_skin_        = mo->info_->model_skin_;
        mo->model_last_frame_  = -1;
        mo->model_scale_       = mo->info_->model_scale_;
        mo->model_aspect_      = mo->info_->model_aspect_;
        mo->scale_             = mo->info_->scale_;
        mo->aspect_            = mo->info_->aspect_;

        mo->pain_chance_ = mo->info_->pain_chance_;

        // handle dynamic lights
        {
            const DynamicLightDefinition *dinfo = &mo->info_->dlight_;

            if (dinfo->type_ != kDynamicLightTypeNone)
            {
                mo->dynamic_light_.target = dinfo->radius_;
                mo->dynamic_light_.color  = dinfo->colour_;

                // make renderer re-create shader info
                if (mo->dynamic_light_.shader)
                {
                    // FIXME: delete mo->dynamic_light_.shader;
                    mo->dynamic_light_.shader = nullptr;
                }
            }
        }
    }
    SetThingPosition(mo);

    int state = MapObjectFindLabel(mo, become->start_.label_.c_str());
    if (state == 0)
        FatalError("BECOME action: frame '%s' in [%s] not found!\n", become->start_.label_.c_str(),
                   mo->info_->name_.c_str());

    state += become->start_.offset_;

    MapObjectSetStateDeferred(mo, state, 0);
}

void A_UnBecome(MapObject *mo)
{
    if (!mo->pre_become_)
    {
        return;
    }

    const MapObjectDefinition *preBecome = nullptr;
    preBecome                            = mo->pre_become_;

    // DO THE DEED !!
    mo->pre_become_ = nullptr; // remove old reference

    UnsetThingPosition(mo);
    {
        mo->info_ = preBecome;

        mo->morph_timeout_ = mo->info_->morphtimeout_;

        mo->radius_ = mo->info_->radius_;
        mo->height_ = mo->info_->height_;
        if (mo->info_->fast_speed_ > -1 && level_flags.fast_monsters)
            mo->speed_ = mo->info_->fast_speed_;
        else
            mo->speed_ = mo->info_->speed_;

        // Note: health is not changed
        if (mo->flags_ & kMapObjectFlagAmbush) // preserve map editor AMBUSH flag
        {
            mo->flags_ = mo->info_->flags_;
            mo->flags_ |= kMapObjectFlagAmbush;
        }
        else
            mo->flags_ = mo->info_->flags_;

        mo->extended_flags_ = mo->info_->extended_flags_;
        mo->hyper_flags_    = mo->info_->hyper_flags_;

        mo->target_visibility_ = mo->info_->translucency_;
        mo->current_attack_    = nullptr;
        mo->model_skin_        = mo->info_->model_skin_;
        mo->model_last_frame_  = -1;
        mo->model_scale_       = mo->info_->model_scale_;
        mo->model_aspect_      = mo->info_->model_aspect_;
        mo->scale_             = mo->info_->scale_;
        mo->aspect_            = mo->info_->aspect_;

        mo->pain_chance_ = mo->info_->pain_chance_;

        // handle dynamic lights
        {
            const DynamicLightDefinition *dinfo = &mo->info_->dlight_;

            if (dinfo->type_ != kDynamicLightTypeNone)
            {
                mo->dynamic_light_.target = dinfo->radius_;
                mo->dynamic_light_.color  = dinfo->colour_;

                // make renderer re-create shader info
                if (mo->dynamic_light_.shader)
                {
                    // FIXME: delete mo->dynamic_light_.shader;
                    mo->dynamic_light_.shader = nullptr;
                }
            }
        }
    }
    SetThingPosition(mo);

    int state = MapObjectFindLabel(mo, "IDLE");
    if (state == 0)
        FatalError("UNBECOME action: frame '%s' in [%s] not found!\n", "IDLE", mo->info_->name_.c_str());

    MapObjectSetStateDeferred(mo, state, 0);
}

// Same as A_Become, but health is set to max
void A_Morph(MapObject *mo)
{
    if (!mo->state_ || !mo->state_->action_par)
    {
        FatalError("MORPH action used in [%s] without arguments!\n", mo->info_->name_.c_str());
    }

    MorphActionInfo *morph = (MorphActionInfo *)mo->state_->action_par;

    if (!morph->info_)
    {
        morph->info_ = mobjtypes.Lookup(morph->info_ref_.c_str());
        EPI_ASSERT(morph->info_); // lookup should be OK (fatal error if not found)
    }

    // DO THE DEED !!
    mo->pre_become_ = mo->info_; // store what we used to be

    UnsetThingPosition(mo);
    {
        mo->info_   = morph->info_;
        mo->health_ = mo->info_->spawn_health_; // Set health to full again

        mo->morph_timeout_ = mo->info_->morphtimeout_;

        mo->radius_ = mo->info_->radius_;
        mo->height_ = mo->info_->height_;
        if (mo->info_->fast_speed_ > -1 && level_flags.fast_monsters)
            mo->speed_ = mo->info_->fast_speed_;
        else
            mo->speed_ = mo->info_->speed_;

        if (mo->flags_ & kMapObjectFlagAmbush) // preserve map editor AMBUSH flag
        {
            mo->flags_ = mo->info_->flags_;
            mo->flags_ |= kMapObjectFlagAmbush;
        }
        else
            mo->flags_ = mo->info_->flags_;

        mo->extended_flags_ = mo->info_->extended_flags_;
        mo->hyper_flags_    = mo->info_->hyper_flags_;

        mo->target_visibility_ = mo->info_->translucency_;
        mo->current_attack_    = nullptr;
        mo->model_skin_        = mo->info_->model_skin_;
        mo->model_last_frame_  = -1;
        mo->model_scale_       = mo->info_->model_scale_;
        mo->model_aspect_      = mo->info_->model_aspect_;
        mo->scale_             = mo->info_->scale_;
        mo->aspect_            = mo->info_->aspect_;

        mo->pain_chance_ = mo->info_->pain_chance_;

        // handle dynamic lights
        {
            const DynamicLightDefinition *dinfo = &mo->info_->dlight_;

            if (dinfo->type_ != kDynamicLightTypeNone)
            {
                mo->dynamic_light_.target = dinfo->radius_;
                mo->dynamic_light_.color  = dinfo->colour_;

                // make renderer re-create shader info
                if (mo->dynamic_light_.shader)
                {
                    // FIXME: delete mo->dynamic_light_.shader;
                    mo->dynamic_light_.shader = nullptr;
                }
            }
        }
    }
    SetThingPosition(mo);

    int state = MapObjectFindLabel(mo, morph->start_.label_.c_str());
    if (state == 0)
        FatalError("MORPH action: frame '%s' in [%s] not found!\n", morph->start_.label_.c_str(),
                   mo->info_->name_.c_str());

    state += morph->start_.offset_;

    MapObjectSetStateDeferred(mo, state, 0);
}

// Same as A_UnBecome, but health is set to max
void A_UnMorph(MapObject *mo)
{
    if (!mo->pre_become_)
    {
        return;
    }

    const MapObjectDefinition *preBecome = nullptr;
    preBecome                            = mo->pre_become_;

    // DO THE DEED !!
    mo->pre_become_ = nullptr; // remove old reference

    UnsetThingPosition(mo);
    {
        mo->info_ = preBecome;

        mo->health_ = mo->info_->spawn_health_; // Set health to max again

        mo->morph_timeout_ = mo->info_->morphtimeout_;

        mo->radius_ = mo->info_->radius_;
        mo->height_ = mo->info_->height_;
        if (mo->info_->fast_speed_ > -1 && level_flags.fast_monsters)
            mo->speed_ = mo->info_->fast_speed_;
        else
            mo->speed_ = mo->info_->speed_;

        // Note: health is not changed

        if (mo->flags_ & kMapObjectFlagAmbush) // preserve map editor AMBUSH flag
        {
            mo->flags_ = mo->info_->flags_;
            mo->flags_ |= kMapObjectFlagAmbush;
        }
        else
            mo->flags_ = mo->info_->flags_;

        mo->extended_flags_ = mo->info_->extended_flags_;
        mo->hyper_flags_    = mo->info_->hyper_flags_;

        mo->target_visibility_ = mo->info_->translucency_;
        mo->current_attack_    = nullptr;
        mo->model_skin_        = mo->info_->model_skin_;
        mo->model_last_frame_  = -1;
        mo->model_scale_       = mo->info_->model_scale_;
        mo->model_aspect_      = mo->info_->model_aspect_;
        mo->scale_             = mo->info_->scale_;
        mo->aspect_            = mo->info_->aspect_;

        mo->pain_chance_ = mo->info_->pain_chance_;

        // handle dynamic lights
        {
            const DynamicLightDefinition *dinfo = &mo->info_->dlight_;

            if (dinfo->type_ != kDynamicLightTypeNone)
            {
                mo->dynamic_light_.target = dinfo->radius_;
                mo->dynamic_light_.color  = dinfo->colour_;

                // make renderer re-create shader info
                if (mo->dynamic_light_.shader)
                {
                    // FIXME: delete mo->dynamic_light_.shader;
                    mo->dynamic_light_.shader = nullptr;
                }
            }
        }
    }
    SetThingPosition(mo);

    int state = MapObjectFindLabel(mo, "IDLE");
    if (state == 0)
        FatalError("UNMORPH action: frame '%s' in [%s] not found!\n", "IDLE", mo->info_->name_.c_str());

    MapObjectSetStateDeferred(mo, state, 0);
}

// -AJA- 1999/08/08: New attack flag FORCEAIM, which fixes chainsaw.
//
void PlayerAttack(MapObject *p_obj, const AttackDefinition *attack)
{
    EPI_ASSERT(attack);

    p_obj->current_attack_ = attack;

    if (attack->attackstyle_ != kAttackStyleDualAttack)
    {
        float range = (attack->range_ > 0) ? attack->range_ : kMissileRange;

        // see which target is to be aimed at
        MapObject *target =
            MapTargetAutoAim(p_obj, p_obj->angle_, range, (attack->flags_ & kAttackFlagForceAim) ? true : false);

        MapObject *old_target = p_obj->target_;

        p_obj->SetTarget(target);

        if (attack->flags_ & kAttackFlagFaceTarget)
        {
            if (attack->flags_ & kAttackFlagForceAim)
                P_ForceFaceTarget(p_obj);
            else
                A_FaceTarget(p_obj);
        }

        P_DoAttack(p_obj);
        // restore the previous target for bots
        if (p_obj->player_ && (p_obj->player_->player_flags_ & kPlayerFlagBot))
            p_obj->SetTarget(old_target);
    }
    else
    {
        EPI_ASSERT(attack->dualattack1_ && attack->dualattack2_);

        if (attack->dualattack1_->attackstyle_ == kAttackStyleDualAttack)
            PlayerAttack(p_obj, attack->dualattack1_);
        else
        {
            p_obj->current_attack_ = attack->dualattack1_;

            float range = (p_obj->current_attack_->range_ > 0) ? p_obj->current_attack_->range_ : kMissileRange;

            // see which target is to be aimed at
            MapObject *target = MapTargetAutoAim(p_obj, p_obj->angle_, range,
                                                 (p_obj->current_attack_->flags_ & kAttackFlagForceAim) ? true : false);

            MapObject *old_target = p_obj->target_;

            p_obj->SetTarget(target);

            if (p_obj->current_attack_->flags_ & kAttackFlagFaceTarget)
            {
                if (p_obj->current_attack_->flags_ & kAttackFlagForceAim)
                    P_ForceFaceTarget(p_obj);
                else
                    A_FaceTarget(p_obj);
            }

            P_DoAttack(p_obj);
            // restore the previous target for bots
            if (p_obj->player_ && (p_obj->player_->player_flags_ & kPlayerFlagBot))
                p_obj->SetTarget(old_target);
        }

        if (attack->dualattack2_->attackstyle_ == kAttackStyleDualAttack)
            PlayerAttack(p_obj, attack->dualattack2_);
        else
        {
            p_obj->current_attack_ = attack->dualattack2_;

            float range = (p_obj->current_attack_->range_ > 0) ? p_obj->current_attack_->range_ : kMissileRange;

            // see which target is to be aimed at
            MapObject *target = MapTargetAutoAim(p_obj, p_obj->angle_, range,
                                                 (p_obj->current_attack_->flags_ & kAttackFlagForceAim) ? true : false);

            MapObject *old_target = p_obj->target_;

            p_obj->SetTarget(target);

            if (p_obj->current_attack_->flags_ & kAttackFlagFaceTarget)
            {
                if (p_obj->current_attack_->flags_ & kAttackFlagForceAim)
                    P_ForceFaceTarget(p_obj);
                else
                    A_FaceTarget(p_obj);
            }

            P_DoAttack(p_obj);
            // restore the previous target for bots
            if (p_obj->player_ && (p_obj->player_->player_flags_ & kPlayerFlagBot))
                p_obj->SetTarget(old_target);
        }
    }
}

//-------------------------------------------------------------------
//----------------------   MBF / MBF21  -----------------------------
//-------------------------------------------------------------------

void A_AddFlags(MapObject *mo)
{
    if (!mo->state_->action_par)
    {
        WarningOrError("A_AddFlags used for thing [%s] without values !\n", mo->info_->name_.c_str());
        return;
    }

    int *args = (int *)mo->state_->action_par;

    mo->flags_ |= args[0];
    mo->mbf21_flags_ |= args[1];

    // Unlink from blockmap if necessary
    if (args[0] & kMapObjectFlagNoBlockmap)
    {
        if (mo->blockmap_next_)
        {
            if (mo->blockmap_next_->blockmap_previous_)
            {
                EPI_ASSERT(mo->blockmap_next_->blockmap_previous_ == mo);

                mo->blockmap_next_->blockmap_previous_ = mo->blockmap_previous_;
            }
        }

        if (mo->blockmap_previous_)
        {
            if (mo->blockmap_previous_->blockmap_next_)
            {
                EPI_ASSERT(mo->blockmap_previous_->blockmap_next_ == mo);

                mo->blockmap_previous_->blockmap_next_ = mo->blockmap_next_;
            }
        }
        else
        {
            int blockx = BlockmapGetX(mo->x);
            int blocky = BlockmapGetY(mo->y);

            if (blockx >= 0 && blockx < blockmap_width && blocky >= 0 && blocky < blockmap_height)
            {
                int bnum = blocky * blockmap_width + blockx;

                EPI_ASSERT(blockmap_things[bnum] == mo);

                blockmap_things[bnum] = mo->blockmap_next_;
            }
        }

        mo->blockmap_previous_ = nullptr;
        mo->blockmap_next_     = nullptr;
    }

    // Unlink from subsector if necessary
    if (args[0] & kMapObjectFlagNoSector)
    {
        if (mo->subsector_next_)
        {
            if (mo->subsector_next_->subsector_previous_)
            {
                EPI_ASSERT(mo->subsector_next_->subsector_previous_ == mo);

                mo->subsector_next_->subsector_previous_ = mo->subsector_previous_;
            }
        }

        if (mo->subsector_previous_)
        {
            if (mo->subsector_previous_->subsector_next_)
            {
                EPI_ASSERT(mo->subsector_previous_->subsector_next_ == mo);

                mo->subsector_previous_->subsector_next_ = mo->subsector_next_;
            }
        }
        else
        {
            if (mo->subsector_->thing_list)
            {
                EPI_ASSERT(mo->subsector_->thing_list == mo);

                mo->subsector_->thing_list = mo->subsector_next_;
            }
        }

        mo->subsector_next_     = nullptr;
        mo->subsector_previous_ = nullptr;
    }
}

void A_RemoveFlags(MapObject *mo)
{
    if (!mo->state_->action_par)
    {
        WarningOrError("A_AddFlags used for thing [%s] without values !\n", mo->info_->name_.c_str());
        return;
    }

    int *args = (int *)mo->state_->action_par;

    mo->flags_ &= ~(args[0]);
    mo->mbf21_flags_ &= ~(args[1]);

    // Link into blockmap if necessary
    if (args[0] & kMapObjectFlagNoBlockmap)
    {
        int blockx = BlockmapGetX(mo->x);
        int blocky = BlockmapGetY(mo->y);

        if (blockx >= 0 && blockx < blockmap_width && blocky >= 0 && blocky < blockmap_height)
        {
            int bnum = blocky * blockmap_width + blockx;

            mo->blockmap_previous_ = nullptr;
            mo->blockmap_next_     = blockmap_things[bnum];

            if (blockmap_things[bnum])
                (blockmap_things[bnum])->blockmap_previous_ = mo;

            blockmap_things[bnum] = mo;
        }
        else
        {
            // thing is off the map
            mo->blockmap_next_ = mo->blockmap_previous_ = nullptr;
        }
    }

    // Link into sector if necessary
    if (args[0] & kMapObjectFlagNoSector)
    {
        mo->subsector_next_     = mo->subsector_->thing_list;
        mo->subsector_previous_ = nullptr;

        if (mo->subsector_->thing_list)
            mo->subsector_->thing_list->subsector_previous_ = mo;

        mo->subsector_->thing_list = mo;
    }
}

void A_JumpIfFlagsSet(MapObject *mo)
{
    if (mo->state_->jumpstate == 0)
        return;

    JumpActionInfo *jump = nullptr;

    if (!mo->state_->action_par)
    {
        WarningOrError("A_JumpIfTracerCloser used for thing [%s] without a label !\n", mo->info_->name_.c_str());
        return;
    }
    else
        jump = (JumpActionInfo *)mo->state_->action_par;

    if (!jump->amount && !jump->amount2)
        return;

    bool jumpit = true;

    if (jump->amount)
    {
        if ((mo->flags_ & jump->amount) != jump->amount)
            jumpit = false;
    }
    if (jump->amount2)
    {
        if ((mo->mbf21_flags_ & jump->amount2) != jump->amount2)
            jumpit = false;
    }

    if (jumpit)
        mo->next_state_ = states + mo->state_->jumpstate;
}

void A_JumpIfTracerCloser(MapObject *mo)
{
    if (mo->state_->jumpstate == 0)
        return;

    JumpActionInfo *jump = nullptr;

    if (!mo->state_->action_par)
    {
        WarningOrError("A_JumpIfTracerCloser used for thing [%s] without a label !\n", mo->info_->name_.c_str());
        return;
    }
    else
        jump = (JumpActionInfo *)mo->state_->action_par;

    if (mo->tracer_ &&
        ApproximateDistance(mo->tracer_->x - mo->x, mo->tracer_->y - mo->y) < (float)jump->amount / 65536.0f)
        mo->next_state_ = states + mo->state_->jumpstate;
}

void A_JumpIfTracerInSight(MapObject *mo)
{
    if (mo->state_->jumpstate == 0)
        return;

    JumpActionInfo *jump = nullptr;

    if (!mo->state_->action_par)
    {
        WarningOrError("A_JumpIfTracerInSight used for thing [%s] without a label !\n", mo->info_->name_.c_str());
        return;
    }
    else
        jump = (JumpActionInfo *)mo->state_->action_par;

    if (mo->tracer_ && CheckSight(mo, mo->tracer_) &&
        (!jump->amount || epi::BAMCheckFOV(PointToAngle(mo->x, mo->y, mo->tracer_->x, mo->tracer_->y),
                                           epi::BAMFromDegrees((float)jump->amount / 65536.0f), mo->angle_)))
        mo->next_state_ = states + mo->state_->jumpstate;
}

void A_JumpIfTargetCloser(MapObject *mo)
{
    if (mo->state_->jumpstate == 0)
        return;

    JumpActionInfo *jump = nullptr;

    if (!mo->state_->action_par)
    {
        WarningOrError("A_JumpIfTargetCloser used for thing [%s] without a label !\n", mo->info_->name_.c_str());
        return;
    }
    else
        jump = (JumpActionInfo *)mo->state_->action_par;

    if (mo->target_ &&
        ApproximateDistance(mo->target_->x - mo->x, mo->target_->y - mo->y) < (float)jump->amount / 65536.0f)
        mo->next_state_ = states + mo->state_->jumpstate;
}

void A_JumpIfTargetInSight(MapObject *mo)
{
    if (mo->state_->jumpstate == 0)
        return;

    JumpActionInfo *jump = nullptr;

    if (!mo->state_->action_par)
    {
        WarningOrError("A_JumpIfTargetInSight used for thing [%s] without a label !\n", mo->info_->name_.c_str());
        return;
    }
    else
        jump = (JumpActionInfo *)mo->state_->action_par;

    if (mo->target_ && CheckSight(mo, mo->target_) &&
        (!jump->amount || epi::BAMCheckFOV(PointToAngle(mo->x, mo->y, mo->target_->x, mo->target_->y),
                                           epi::BAMFromDegrees((float)jump->amount / 65536.0f), mo->angle_)))
        mo->next_state_ = states + mo->state_->jumpstate;
}

void A_FindTracer(MapObject *mo)
{
    MapObject *destination = mo->tracer_;

    if (destination)
        return;

    if (!mo->state_->action_par)
    {
        WarningOrError("A_FindTracer used for thing [%s] without values !\n", mo->info_->name_.c_str());
        return;
    }

    int *args = (int *)mo->state_->action_par;

    BAMAngle fov         = args[0] == 0 ? kBAMAngle0 : epi::BAMFromDegrees((float)args[0] / 65536.0f);
    uint32_t rangeblocks = args[1] != 0 ? args[1] : 10;

    MapObject *target = A_LookForBlockmapTarget(mo, rangeblocks, fov);

    if (target)
        mo->SetTracer(target);
}

void A_SeekTracer(MapObject *mo)
{
    MapObject *destination = mo->tracer_;

    if (!destination || destination->health_ <= 0)
        return;

    if (!mo->state_->action_par)
    {
        WarningOrError("A_SeekTracer used for thing [%s] without values !\n", mo->info_->name_.c_str());
        return;
    }

    int *args = (int *)mo->state_->action_par;

    BAMAngle maxturn = epi::BAMFromDegrees((float)args[1] / 65536.0f);

    // change angle
    BAMAngle exact = PointToAngle(mo->x, mo->y, destination->x, destination->y);

    if (exact != mo->angle_)
    {
        if (exact - mo->angle_ > kBAMAngle180)
        {
            mo->angle_ -= maxturn;

            if (exact - mo->angle_ < kBAMAngle180)
                mo->angle_ = exact;
        }
        else
        {
            mo->angle_ += maxturn;

            if (exact - mo->angle_ > kBAMAngle180)
                mo->angle_ = exact;
        }
    }

    mo->momentum_.X = mo->speed_ * epi::BAMCos(mo->angle_);
    mo->momentum_.Y = mo->speed_ * epi::BAMSin(mo->angle_);

    // change slope
    float slope = ApproximateSlope(destination->x - mo->x, destination->y - mo->y, MapObjectMidZ(destination) - mo->z);

    slope *= mo->speed_;

    if (slope < mo->momentum_.Z)
        mo->momentum_.Z -= 0.125f;
    else
        mo->momentum_.Z += 0.125f;
}

void A_JumpIfHealthBelow(MapObject *mo)
{
    if (mo->state_->jumpstate == 0)
        return;

    JumpActionInfo *jump = nullptr;

    if (!mo->state_->action_par)
    {
        WarningOrError("A_JumpIfHealthBelow used for thing [%s] without a label !\n", mo->info_->name_.c_str());
        return;
    }
    else
        jump = (JumpActionInfo *)mo->state_->action_par;

    if (mo->health_ < jump->amount)
        mo->next_state_ = states + mo->state_->jumpstate;
}

void A_ClearTracer(MapObject *object)
{
    object->SetTracer(nullptr);
}

void A_MonsterMeleeAttack(MapObject *object)
{
    const AttackDefinition *attack = nullptr;

    if (object->state_ && object->state_->action_par)
        attack = (const AttackDefinition *)object->state_->action_par;

    if (!attack)
    {
        WarningOrError("A_MonsterMeleeAttack: %s has no melee attack.\n", object->info_->name_.c_str());
        return;
    }

    if (attack->flags_ & kAttackFlagFaceTarget)
        A_FaceTarget(object);

    if (attack->flags_ & kAttackFlagNeedSight)
    {
        if (!object->target_ || !CheckSight(object, object->target_))
            return;
    }

    object->current_attack_ = attack;
    P_DoAttack(object);
}

void A_MonsterProjectile(MapObject *object)
{
    const AttackDefinition *attack = nullptr;

    if (object->state_ && object->state_->action_par)
        attack = (const AttackDefinition *)object->state_->action_par;

    if (!attack || !attack->atk_mobj_)
    {
        WarningOrError("A_MonsterProjectile: %s has an invalid projectile attack.\n", object->info_->name_.c_str());
        return;
    }

    if (attack->flags_ & kAttackFlagFaceTarget)
        A_FaceTarget(object);

    object->current_attack_ = attack;
    P_DoAttack(object);
}

void A_MonsterBulletAttack(MapObject *object)
{
    const AttackDefinition *attack = nullptr;

    if (object->state_ && object->state_->action_par)
        attack = (const AttackDefinition *)object->state_->action_par;

    if (!attack)
    {
        WarningOrError("A_MonsterBulletAttack: %s has no hitscan attack defined.\n", object->info_->name_.c_str());
        return;
    }

    if (attack->flags_ & kAttackFlagFaceTarget)
        A_FaceTarget(object);

    if (object->info_->attacksound_)
        StartSoundEffect(object->info_->attacksound_, GetSoundEffectCategory(object), object);

    object->current_attack_ = attack;
    P_DoAttack(object);
}

void A_WeaponMeleeAttack(MapObject *mo)
{
    Player                 *p    = mo->player_;
    PlayerSprite           *psp  = &p->player_sprites_[p->action_player_sprite_];
    WeaponDefinition       *info = p->weapons_[p->ready_weapon_].info;
    const AttackDefinition *atk  = nullptr;

    if (psp->state && psp->state->action_par)
        atk = (const AttackDefinition *)psp->state->action_par;

    if (!atk)
        FatalError("Weapon [%s] missing attack for A_WeaponMeleeAttack.\n", info->name_.c_str());

    // wake up monsters
    if (!(info->specials_[0] & WeaponFlagSilentToMonsters))
        NoiseAlert(p);

    PlayerAttack(mo, atk);
}

void A_WeaponBulletAttack(MapObject *mo)
{
    Player                 *p    = mo->player_;
    PlayerSprite           *psp  = &p->player_sprites_[p->action_player_sprite_];
    WeaponDefinition       *info = p->weapons_[p->ready_weapon_].info;
    const AttackDefinition *atk  = nullptr;

    if (psp->state && psp->state->action_par)
        atk = (const AttackDefinition *)psp->state->action_par;

    if (!atk)
        FatalError("Weapon [%s] missing attack for A_WeaponBulletAttack.\n", info->name_.c_str());

    // wake up monsters
    if (!(info->specials_[0] & WeaponFlagSilentToMonsters))
        NoiseAlert(p);

    PlayerAttack(mo, atk);
}

void A_WeaponProjectile(MapObject *mo)
{
    Player                 *p    = mo->player_;
    PlayerSprite           *psp  = &p->player_sprites_[p->action_player_sprite_];
    WeaponDefinition       *info = p->weapons_[p->ready_weapon_].info;
    const AttackDefinition *atk  = nullptr;

    if (psp->state && psp->state->action_par)
        atk = (const AttackDefinition *)psp->state->action_par;

    if (!atk)
        FatalError("Weapon [%s] missing attack for A_WeaponProjectile.\n", info->name_.c_str());

    if (!atk->atk_mobj_)
        FatalError("Weapon [%s] missing projectile map object for A_WeaponProjectile.\n", info->name_.c_str());

    // wake up monsters
    if (!(info->specials_[0] & WeaponFlagSilentToMonsters))
        NoiseAlert(p);

    PlayerAttack(mo, atk);
}

//
// A_RadiusDamage
//
// Radius attack from MBF21
//
void A_RadiusDamage(MapObject *mo)
{
    int *args = (int *)mo->state_->action_par;

    if (!args)
        FatalError("Map Object [%s] given no parameters for A_RadiusDamage.\n", mo->info_->name_.c_str());

#ifdef DEVELOPERS
    if (!damage)
    {
        LogDebug("%s caused no explosion damage\n", mo->info_->name.c_str());
        return;
    }
#endif

    RadiusAttack(mo, mo->source_ ? mo->source_ : mo, (float)args[1], (float)args[0], nullptr, false);
}

//
// A_HealChase
//
//
void A_HealChase(MapObject *object)
{
    if (object->state_->jumpstate == 0)
        return;

    JumpActionInfo *jump = nullptr;

    if (!object->state_->action_par)
    {
        WarningOrError("A_HealChase used for map object [%s] without a label !\n", object->info_->name_.c_str());
        return;
    }
    else
        jump = (JumpActionInfo *)object->state_->action_par;

    MapObject *corpse;

    corpse = FindCorpseForResurrection(object);

    if (corpse)
    {
        object->angle_ = PointToAngle(object->x, object->y, corpse->x, corpse->y);
        if (object->info_->res_state_)
            MapObjectSetStateDeferred(object, object->info_->res_state_, 0);
        SoundEffectDefinition *def = sfxdefs.DEHLookup(jump->amount);
        if (def)
            StartSoundEffect(sfxdefs.GetEffect(def->name_.c_str()), GetSoundEffectCategory(object), object);

        // corpses without raise states should be skipped
        EPI_ASSERT(corpse->info_->raise_state_);

        BringCorpseToLife(corpse);

        // -ACB- 1998/09/05 Support Check: Res creatures to support that object
        if (object->support_object_)
        {
            corpse->SetSupportObject(object->support_object_);
            corpse->SetTarget(object->target_);
        }
        else
        {
            corpse->SetSupportObject(nullptr);
            corpse->SetTarget(nullptr);
        }

        // -AJA- Resurrected creatures are on Archvile's side (like MBF)
        corpse->side_ = object->side_;
        return;
    }

    A_StandardChase(object);
}

void A_SpawnObject(MapObject *mo)
{
    if (!mo->state_->action_par)
        FatalError("A_SpawnObject action used without a object name!\n");

    DEHSpawnParameters *ref = (DEHSpawnParameters *)mo->state_->action_par;

    const MapObjectDefinition *type = mobjtypes.Lookup(ref->spawn_name);

    if (!type)
        FatalError("A_SpawnObject action used with %s, but it doesn't exist?\n", ref->spawn_name);

    BAMAngle newangle = mo->angle_ + ref->angle;
    float    newcos   = epi::BAMCos(newangle);
    float    newsin   = epi::BAMSin(newangle);

    MapObject *spawn =
        CreateMapObject(mo->x + (ref->x_offset * newcos - ref->y_offset * newsin),
                        mo->y + (ref->x_offset * newsin + ref->y_offset * newcos), mo->z + ref->z_offset, type);
    EPI_ASSERT(spawn);

    spawn->angle_ = newangle;
    spawn->momentum_.X += newcos * ref->x_velocity - ref->y_velocity * newsin;
    spawn->momentum_.Y += newsin * ref->x_velocity + newcos * ref->y_velocity;
    spawn->momentum_.Z += ref->z_velocity;
    spawn->side_ = mo->side_;

    spawn->SetRealSource(mo);
    spawn->SetSpawnSource(mo);

    if ((spawn->flags_ & kMapObjectFlagMissile) || (spawn->extended_flags_ & kExtendedFlagBounce))
    {
        if ((mo->flags_ & kMapObjectFlagMissile) || (mo->extended_flags_ & kExtendedFlagBounce))
        {
            spawn->SetTarget(mo->target_);
            spawn->SetTracer(mo->tracer_);
        }
        else
        {
            spawn->SetTarget(mo);
            spawn->SetTracer(mo->target_);
        }
    }
}

//
// killough 9/98: a mushroom explosion effect, sorta :)
// Original idea: Linguica
//
void A_Mushroom(MapObject *mo)
{
    float height = 4.0f;
    float speed  = 0.5f;

    const State *st = mo->state_;

    if (!mo->state_ || !mo->state_->action_par)
        return;

    if (st && st->action_par)
    {
        int *values = (int *)mo->state_->action_par;
        if (values[0])
        {
            height = (float)values[0] / 65536.0f;
        }
        if (values[1])
        {
            speed = (float)values[1] / 65536.0f;
        }
    }

    // First make normal explosion damage
    A_DamageExplosion(mo);

    // Now launch mushroom cloud
    if (!mushroom_mobj)
    {
        for (std::vector<MapObjectDefinition *>::reverse_iterator iter     = mobjtypes.dynamic_atk_mobjtypes.rbegin(),
                                                                  iter_end = mobjtypes.dynamic_atk_mobjtypes.rend();
             iter != iter_end; ++iter)
        {
            MapObjectDefinition *mobj = *iter;
            if (mobj->name_ == "atk:MANCUBUS_FIREBALL")
            {
                mushroom_mobj = mobj;
                break;
            }
        }
        if (!mushroom_mobj)
        {
            FatalError("A_Mushroom called, but the MANCUBUS_FIREBALL attack has been removed!\n");
        }
    }

    // Spread is determined by the 'missile damage' mobj property,
    // which from our Dehacked conversion equates to nominal
    // projectile damage
    int i, j, spread = mo->info_->proj_damage_.nominal_;

    for (i = -spread; i <= spread; i += 8)
    {
        for (j = -spread; j <= spread; j += 8)
        {
            // Aim in many directions from source
            float tx = mo->x + i;
            float ty = mo->y + j;
            float tz = mo->z + ApproximateDistance(i, j) * height;

            MapObject *proj = CreateMapObject(mo->x, mo->y, mo->z + 32.0f, mushroom_mobj);
            if (proj)
            {
                proj->flags_ &= ~kMapObjectFlagNoGravity;
                proj->angle_ = PointToAngle(mo->x, mo->y, tx, ty);
                float dist   = ApproximateDistance(i, j);
                dist /= proj->info_->speed_;

                if (dist < 1.0f)
                    dist = 1.0f;

                proj->momentum_.Z = (tz - mo->z) / dist;
                proj->momentum_.X = proj->info_->speed_ * epi::BAMCos(proj->angle_);
                proj->momentum_.Y = proj->info_->speed_ * epi::BAMSin(proj->angle_);
                proj->momentum_   = HMM_MulV3F(proj->momentum_, speed);
                if (proj->flags_ & kMapObjectFlagPreserveMomentum)
                {
                    proj->momentum_.X += mo->momentum_.X;
                    proj->momentum_.Y += mo->momentum_.Y;
                    proj->momentum_.Z += mo->momentum_.Z;
                }
                proj->SetRealSource(mo);
                proj->SetSpawnSource(mo);
            }
        }
    }
}

void A_PainChanceSet(MapObject *mo)
{
    float value = 0;

    const State *st = mo->state_;

    if (st && st->action_par)
    {
        value = ((float *)st->action_par)[0];
        value = HMM_MAX(0.0f, HMM_MIN(1.0f, value));
    }
    mo->pain_chance_ = value;
}

void A_ScaleSet(MapObject *mo)
{
    float valueSprite = mo->info_->scale_;       // grab the default scale for this thing as a fallback
    float valueModel  = mo->info_->model_scale_; // grab the default scale for this thing as a fallback

    const State *st = mo->state_;

    if (st && st->action_par)
    {
        valueSprite = ((float *)st->action_par)[0];
        valueModel  = valueSprite;
    }
    mo->scale_       = valueSprite;
    mo->model_scale_ = valueModel;
}

void A_Gravity(MapObject *mo)
{
    mo->flags_ &= ~kMapObjectFlagNoGravity; // Remove NoGravity flag
}

void A_NoGravity(MapObject *mo)
{
    mo->flags_ |= kMapObjectFlagNoGravity; // Set NoGravity flag
}

// Thing will forget both current target and supported player
void A_ClearTarget(MapObject *object)
{
    object->SetTarget(nullptr);
    object->SetSupportObject(nullptr);
}

//
// Similar to SUPPORT_LOOKOUT but will not go to MEANDER states automatically.
// Look for players AND enemies.
// - If we have no player to support we try and find one.
// - If we have no SIDE set then we will get one.
// - If we see an enemy then we target him.
void A_FriendLook(MapObject *object)
{
    object->threshold_ = 0;              // any shot will wake up

    if (!object->support_object_)        // no player to support yet
    {
        if (FindPlayerToSupport(object)) // try and find a player. One way or the other we will have a side at least
        {
            if (object->info_->seesound_)
            {
                StartSoundEffect(object->info_->seesound_, GetSoundEffectCategory(object), object,
                                 SfxFlags(object->info_));
            }
        }
    }

    if (!A_LookForTargets(object)) // No target found
        return;
    else
    {
        if (object->info_->seesound_)
        {
            StartSoundEffect(object->info_->seesound_, GetSoundEffectCategory(object), object, SfxFlags(object->info_));
        }
    }
}

//
// FindPlayerToSupport
//
// Look for a Player to support
//
bool FindPlayerToSupport(MapObject *object)
{
    if (object->flags_ & kMapObjectFlagStealth)
        object->target_visibility_ = 1.0f;

    if (LookForPlayers(object, object->info_->sight_angle_, true)) // any players around to support?
    {
        // join the player's side
        if (object->side_ == 0)
        {
            if (object->support_object_ && object->support_object_->player_)
                object->side_ = object->support_object_->side_;
        }

        return true;
    }

    // default to something at least
    object->side_ = 1;

    return false;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
