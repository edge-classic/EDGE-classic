//----------------------------------------------------------------------------
//  EDGE Moving, Aiming, Shooting & Collision code
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
// -MH- 1998/07/02 "shootupdown" --> "true_3d_gameplay"
//
// -AJA- 1999/07/19: Removed P_LineOpening.  Gaps are now stored
//       in line_t, and updated whenever sector heights change.
//
// -AJA- 1999/07/21: Replaced some non-critical RandomByteDeterministics with
// RandomByte.
//
// -AJA- 1999/07/30: Big changes for extra floor handling. Split
//       P_CheckPosition into two new routines (one handling absolute
//       positions, the other handling relative positions). Split the
//       Check* routines similiarly.
//

#include <float.h>

#include "AlmostEquals.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "epi.h"
#include "epi_doomdefs.h"
#include "g_game.h"
#include "m_bbox.h"
#include "m_math.h" // Vert slope intercept check
#include "m_random.h"
#include "p_local.h"
#include "r_misc.h"
#include "s_sound.h"

static constexpr uint8_t kRaiseRadius = 32;

static void GoreSettingCallback(ConsoleVariable *self)
{
    if (self->d_ == 2) // No blood
        return;

    if (current_map && ((current_map->force_on_ | current_map->force_off_) & kMapFlagMoreBlood))
        return;

    level_flags.more_blood = global_flags.more_blood = self->d_;
}

EDGE_DEFINE_CONSOLE_VARIABLE_WITH_CALLBACK_CLAMPED(gore_level, "0", kConsoleVariableFlagArchive, GoreSettingCallback, 0,
                                                   2)

// Forward declaration for ShootCheckGap
static bool ShootTraverseCallback(PathIntercept *in, void *dataptr);

struct MoveAttempt
{
    // --- input --

    // thing trying to move
    MapObject *mover;
    int        flags, extended_flags;

    // attempted destination
    float x, y, z;

    float floor_slope_z   = -40000;
    float ceiling_slope_z = 40000;

    float bounding_box[4];

    // --- output ---

    Subsector *subsector;

    // vertical space over all contacted lines
    float floor_z, ceiling_z;
    float dropoff;

    // objects that end up above and below us
    MapObject *above;
    MapObject *below;

    // -AJA- FIXME: this is a "quick fix" (hack).  If only one line is
    // hit, and TryMove decides the move is impossible, then we know
    // this line must be the blocking line.  Real solution ?  Probably
    // to move most of the checks from TryMove into CheckRelLine.  It
    // definitely needs a lot of consideration.

    Line *line_which;
    int   line_count;
};

static MoveAttempt move_check;

bool  map_object_hit_sky;
Line *block_line;

// If "float_ok" true, move would be ok if at float_destination_z.
bool  float_ok;
float float_destination_z;

// keep track of special lines as they are hit,
// but don't process them until the move is proven valid

std::vector<Line *> special_lines_hit; // List of special lines that have been hit

struct ShootAttempt
{
    MapObject *source;

    float    range;
    float    start_z;
    BAMAngle angle;
    float    slope;
    float    top_slope;
    float    bottom_slope;
    bool     forced;

    float                      damage;
    const DamageClass         *damage_type;
    const MapObjectDefinition *puff;
    const MapObjectDefinition *blood;
    float                      previous_z;

    // output field:
    MapObject *target;
};

static ShootAttempt shoot_check;
static ShootAttempt aim_check;

// convenience function
static inline int PointOnLineSide(float x, float y, Line *ld)
{
    DividingLine div;

    div.x       = ld->vertex_1->X;
    div.y       = ld->vertex_1->Y;
    div.delta_x = ld->delta_x;
    div.delta_y = ld->delta_y;

    return PointOnDividingLineSide(x, y, &div);
}

//
// TELEPORT MOVE
//

static bool StompThingCallback(MapObject *thing, void *data)
{
    if (!(thing->flags_ & kMapObjectFlagShootable))
        return true;

    // check we aren't trying to stomp ourselves
    if (thing == move_check.mover)
        return true;

    // ignore old avatars (for Hub reloads), which get removed after loading
    if (thing->hyper_flags_ & kHyperFlagRememberOldAvatars)
        return true;

    float blockdist = thing->radius_ + move_check.mover->radius_;

    // check to see we hit it
    if (fabs(thing->x - move_check.x) >= blockdist || fabs(thing->y - move_check.y) >= blockdist)
        return true; // no, we did not

    // -AJA- 1999/07/30: True 3d gameplay checks.
    if (level_flags.true_3d_gameplay)
    {
        if (move_check.z >= thing->z + thing->height_)
        {
            // went over
            move_check.floor_z = HMM_MAX(move_check.floor_z, thing->z + thing->height_);
            return true;
        }

        if (move_check.z + move_check.mover->height_ <= thing->z)
        {
            // went under
            move_check.ceiling_z = HMM_MIN(move_check.ceiling_z, thing->z);
            return true;
        }
    }

    if (!move_check.mover->player_ && (current_map->force_off_ & kMapFlagStomp))
        return false;

    TelefragMapObject(thing, move_check.mover, nullptr);
    return true;
}

//
// Kill anything occupying the position
//
bool TeleportMove(MapObject *thing, float x, float y, float z)
{
    move_check.mover          = thing;
    move_check.flags          = thing->flags_;
    move_check.extended_flags = thing->extended_flags_;

    move_check.x = x;
    move_check.y = y;
    move_check.z = z;

    move_check.subsector = PointInSubsector(x, y);

    ComputeThingGap(thing, move_check.subsector->sector, z, &move_check.floor_z, &move_check.ceiling_z);

    // The base floor/ceiling is from the subsector that contains the point.
    // Any contacted lines the step closer together will adjust them.
    move_check.dropoff = move_check.floor_z;
    move_check.above   = nullptr;
    move_check.below   = nullptr;

    // -ACB- 2004/08/01 Don't think this is needed
    //	special_lines_hit.ZeroiseCount();

    float r = thing->radius_;

    if (!BlockmapThingIterator(x - r, y - r, x + r, y + r, StompThingCallback))
        return false;

    // everything on the spot has been stomped,
    // so link the thing into its new position

    thing->floor_z_   = move_check.floor_z;
    thing->ceiling_z_ = move_check.ceiling_z;

    ChangeThingPosition(thing, x, y, z);

    return true;
}

//
// ABSOLUTE POSITION CLIPPING
//

static bool CheckAbsoluteLineCallback(Line *ld, void *data)
{
    if (BoxOnLineSide(move_check.bounding_box, ld) != -1)
        return true;

    // The spawning thing's position touches the given line.
    // If this should not be allowed, return false.

    if (move_check.mover->player_ && ld->special && (ld->special->portal_effect_ & kPortalEffectTypeStandard))
        return true;

    if (!ld->back_sector || ld->gap_number == 0)
        return false; // one sided line

    if (move_check.extended_flags & kExtendedFlagCrossBlockingLines)
    {
        if ((ld->flags & kLineFlagShootBlock) && (move_check.flags & kMapObjectFlagMissile))
            return false;
    }
    else
    {
        // explicitly blocking everything ?
        if (ld->flags & kLineFlagBlocking)
            return false;

        // block players ?
        if (move_check.mover->player_ && ((ld->flags & kLineFlagBlockPlayers) ||
                                          (ld->special && (ld->special->line_effect_ & kLineEffectTypeBlockPlayers))))
        {
            return false;
        }

        // block grounded monsters ?
        if ((move_check.extended_flags & kExtendedFlagMonster) &&
            ((ld->flags & kLineFlagBlockGroundedMonsters) ||
             (ld->special && (ld->special->line_effect_ & kLineEffectTypeBlockGroundedMonsters))) &&
            (move_check.mover->z <= move_check.mover->floor_z_ + 1.0f))
        {
            return false;
        }

        // block monsters ?
        if ((move_check.extended_flags & kExtendedFlagMonster) && (ld->flags & kLineFlagBlockMonsters))
        {
            return false;
        }
    }

    // does the thing fit in one of the line gaps ?
    for (int i = 0; i < ld->gap_number; i++)
    {
        // -AJA- FIXME: this kOnFloorZ stuff is a DIRTY HACK!
        if (AlmostEquals(move_check.z, kOnFloorZ) || AlmostEquals(move_check.z, kOnCeilingZ))
        {
            if (move_check.mover->height_ <= (ld->gaps[i].ceiling - ld->gaps[i].floor))
                return true;
        }
        else
        {
            if (ld->gaps[i].floor <= move_check.z && move_check.z + move_check.mover->height_ <= ld->gaps[i].ceiling)
                return true;
        }
    }

    return false;
}

static bool CheckAbsoluteThingCallback(MapObject *thing, void *data)
{
    float blockdist;
    bool  solid;

    if (thing == move_check.mover)
        return true;

    if (!(thing->flags_ & (kMapObjectFlagSolid | kMapObjectFlagShootable)))
        return true;

    blockdist = thing->radius_ + move_check.mover->radius_;

    // Check that we didn't hit it
    if (fabs(thing->x - move_check.x) >= blockdist || fabs(thing->y - move_check.y) >= blockdist)
        return true; // no we missed this thing

    // -AJA- FIXME: this kOnFloorZ stuff is a DIRTY HACK!
    if (!AlmostEquals(move_check.z, kOnFloorZ) && !AlmostEquals(move_check.z, kOnCeilingZ))
    {
        // -KM- 1998/9/19 True 3d gameplay checks.
        if ((move_check.flags & kMapObjectFlagMissile) || level_flags.true_3d_gameplay)
        {
            // overhead ?
            if (move_check.z >= thing->z + thing->height_)
                return true;

            // underneath ?
            if (move_check.z + move_check.mover->height_ <= thing->z)
                return true;
        }
    }

    solid = (thing->flags_ & kMapObjectFlagSolid) ? true : false;

    // check for missiles making contact
    // -ACB- 1998/08/04 Procedure for missile contact

    if (move_check.mover->source_ && move_check.mover->source_ == thing)
        return true;

    if (move_check.flags & kMapObjectFlagMissile)
    {
        // ignore the missile's shooter
        if (move_check.mover->source_ && move_check.mover->source_ == thing)
            return true;

        if ((thing->hyper_flags_ & kHyperFlagMissilesPassThrough) && level_flags.pass_missile)
            return true;

        // thing isn't shootable, return depending on if the thing is solid.
        if (!(thing->flags_ & kMapObjectFlagShootable))
            return !solid;

        if (MissileContact(move_check.mover, thing) < 0)
            return true;

        return (move_check.extended_flags & kExtendedFlagTunnel) ? true : false;
    }

    // -AJA- 2000/06/09: Follow MBF semantics: allow the non-solid
    // moving things to pass through solid things.
    return !solid || (thing->flags_ & kMapObjectFlagNoClip) || !(move_check.flags & kMapObjectFlagSolid);
}

//
// CheckAbsolutePosition
//
// Check whether the thing can be placed at the absolute position
// (x,y,z).  Makes no assumptions about the thing's current position.
//
// This is purely informative, nothing is modified, nothing is picked
// up, no special lines are recorded, no special things are touched, and
// no information (apart from true/false) is returned.
//
// Only used for checking if an object can be spawned at a
// particular location.
//
bool CheckAbsolutePosition(MapObject *thing, float x, float y, float z)
{
    // can go anywhere
    if (thing->flags_ & kMapObjectFlagNoClip)
        return true;

    move_check.mover          = thing;
    move_check.flags          = thing->flags_;
    move_check.extended_flags = thing->extended_flags_;

    move_check.x = x;
    move_check.y = y;
    move_check.z = z;

    move_check.subsector = PointInSubsector(x, y);

    float r = move_check.mover->radius_;

    move_check.bounding_box[kBoundingBoxLeft]   = x - r;
    move_check.bounding_box[kBoundingBoxBottom] = y - r;
    move_check.bounding_box[kBoundingBoxRight]  = x + r;
    move_check.bounding_box[kBoundingBoxTop]    = y + r;

    // check things first.

    if (!BlockmapThingIterator(x - r, y - r, x + r, y + r, CheckAbsoluteThingCallback))
        return false;

    // check lines

    if (!BlockmapLineIterator(x - r, y - r, x + r, y + r, CheckAbsoluteLineCallback))
        return false;

    return true;
}

//
// RELATIVE MOVEMENT CLIPPING
//

static bool CheckRelativeLineCallback(Line *ld, void *data)
{
    // Adjusts move_check.floor_z & move_check.ceiling_z as lines are contacted

    if (BoxOnLineSide(move_check.bounding_box, ld) != -1)
        return true;

    // A line has been hit

    // The moving thing's destination position will cross the given line.
    // If this should not be allowed, return false.
    // If the line is special, keep track of it
    // to process later if the move is proven ok.
    // NOTE: specials are NOT sorted by order,
    // so two special lines that are only 8 pixels apart
    // could be crossed in either order.

    if (move_check.mover->player_ && ld->special && (ld->special->portal_effect_ & kPortalEffectTypeStandard))
        return true;

    if (!ld->back_sector)
    {
        block_line = ld;

        // one sided line
        return false;
    }

    if (move_check.extended_flags & kExtendedFlagCrossBlockingLines)
    {
        if ((ld->flags & kLineFlagShootBlock) && (move_check.flags & kMapObjectFlagMissile))
        {
            block_line = ld;
            return false;
        }
    }
    else
    {
        // explicitly blocking everything ?
        // or just blocking monsters ?

        if ((ld->flags & kLineFlagBlocking) ||
            ((ld->flags & kLineFlagBlockMonsters) && (move_check.extended_flags & kExtendedFlagMonster)) ||
            (((ld->special && (ld->special->line_effect_ & kLineEffectTypeBlockGroundedMonsters)) ||
              (ld->flags & kLineFlagBlockGroundedMonsters)) &&
             (move_check.extended_flags & kExtendedFlagMonster) &&
             (move_check.mover->z <= move_check.mover->floor_z_ + 1.0f)) ||
            (((ld->special && (ld->special->line_effect_ & kLineEffectTypeBlockPlayers)) ||
              (ld->flags & kLineFlagBlockPlayers)) &&
             (move_check.mover->player_)))
        {
            block_line = ld;
            return false;
        }
    }

    // -AJA- for players, disable stepping up onto a lowering sector
    if (move_check.mover->player_ && !AlmostEquals(ld->front_sector->floor_height, ld->back_sector->floor_height))
    {
        if ((move_check.mover->z < ld->front_sector->floor_height && SectorIsLowering(ld->front_sector)) ||
            (move_check.mover->z < ld->back_sector->floor_height && SectorIsLowering(ld->back_sector)))
        {
            block_line = ld;
            return false;
        }
    }

    // handle ladders (players only !)
    if (move_check.mover->player_ && ld->special && ld->special->ladder_.height_ > 0)
    {
        float z1, z2;
        float pz1, pz2;

        z1 = ld->front_sector->floor_height + ld->side[0]->middle.offset.Y;
        z2 = z1 + ld->special->ladder_.height_;

        pz1 = move_check.mover->z;
        pz2 = move_check.mover->z + move_check.mover->height_;

        do
        {
            // can't reach the ladder ?
            if (pz1 > z2 || pz2 < z1)
                break;

            // FIXME: if more than one ladder, choose best one

            move_check.mover->on_ladder_ = (ld - level_lines);
        } while (0);
    }

    // if contacted a special line, add it to the list
    if (ld->special)
        special_lines_hit.push_back(ld);

    // check for hitting a sky-hack line
    {
        float f1, c1;
        float f2, c2;

        f1 = ld->front_sector->floor_height;
        c1 = ld->front_sector->ceiling_height;
        f2 = ld->back_sector->floor_height;
        c2 = ld->back_sector->ceiling_height;

        if (!AlmostEquals(c1, c2) && EDGE_IMAGE_IS_SKY(ld->front_sector->ceiling) &&
            EDGE_IMAGE_IS_SKY(ld->back_sector->ceiling) && move_check.z > HMM_MIN(c1, c2))
        {
            map_object_hit_sky = true;
        }

        if (!AlmostEquals(f1, f2) && EDGE_IMAGE_IS_SKY(ld->front_sector->floor) &&
            EDGE_IMAGE_IS_SKY(ld->back_sector->floor) && move_check.z + move_check.mover->height_ < HMM_MAX(f1, f2))
        {
            map_object_hit_sky = true;
        }
    }

    // Only basic vertex slope checks will work here (simple rectangular slope
    // sides), but more detailed movement checks are made later on so it
    // shouldn't allow anything crazy - Dasho
    if (ld->front_sector->floor_vertex_slope || ld->back_sector->floor_vertex_slope)
    {
        DividingLine divver;
        divver.x       = ld->vertex_1->X;
        divver.y       = ld->vertex_1->Y;
        divver.delta_x = ld->delta_x;
        divver.delta_y = ld->delta_y;
        float iz       = 0;
        // Prevent player from getting stuck if actually on linedef and moving
        // parallel to it
        if (PointOnDividingLineThick(move_check.mover->x, move_check.mover->y, &divver, ld->length,
                                     move_check.mover->radius_) == 2)
            return true;
        if (ld->front_sector->floor_vertex_slope && ld->front_sector->line_count == 4 &&
            PointInSubsector(move_check.mover->x, move_check.mover->y)->sector != ld->front_sector)
        {
            float ix = 0;
            float iy = 0;
            ComputeIntersection(&divver, move_check.mover->x, move_check.mover->y, move_check.x, move_check.y, &ix,
                                &iy);
            if (isfinite(ix) && isfinite(iy))
            {
                iz = LinePlaneIntersection({{ix, iy, -40000}}, {{ix, iy, 40000}},
                                               ld->front_sector->floor_z_vertices[2],
                                               ld->front_sector->floor_vertex_slope_normal)
                         .Z;
                if (isfinite(iz) && iz > move_check.mover->z + move_check.mover->info_->step_size_)
                {
                    block_line = ld;
                    return false;
                }
            }
        }
        else if (ld->back_sector->floor_vertex_slope && ld->back_sector->line_count == 4 &&
                 PointInSubsector(move_check.mover->x, move_check.mover->y)->sector != ld->back_sector)
        {
            float ix = 0;
            float iy = 0;
            ComputeIntersection(&divver, move_check.mover->x, move_check.mover->y, move_check.x, move_check.y, &ix,
                                &iy);
            if (isfinite(ix) && isfinite(iy))
            {
                iz = LinePlaneIntersection({{ix, iy, -40000}}, {{ix, iy, 40000}},
                                               ld->back_sector->floor_z_vertices[2],
                                               ld->back_sector->floor_vertex_slope_normal)
                         .Z;
                if (isfinite(iz) && iz > move_check.mover->z + move_check.mover->info_->step_size_)
                {
                    block_line = ld;
                    return false;
                }
            }
        }
        else if (ld->front_sector->floor_vertex_slope && ld->front_sector->line_count == 4 &&
                 PointInSubsector(move_check.mover->x, move_check.mover->y)->sector == ld->front_sector)
        {
            if (!ld->back_sector->floor_vertex_slope)
            {
                iz = ld->back_sector->floor_height;
                if (move_check.mover->z + move_check.mover->info_->step_size_ < iz)
                {
                    block_line = ld;
                    return false;
                }
            }
            else
            {
                float ix = 0;
                float iy = 0;
                ComputeIntersection(&divver, move_check.mover->x, move_check.mover->y, move_check.x, move_check.y, &ix,
                                    &iy);
                if (isfinite(ix) && isfinite(iy))
                {
                    iz = LinePlaneIntersection({{ix, iy, -40000}}, {{ix, iy, 40000}},
                                                   ld->back_sector->floor_z_vertices[2],
                                                   ld->back_sector->floor_vertex_slope_normal)
                             .Z;
                    if (isfinite(iz) && iz > move_check.mover->z + move_check.mover->info_->step_size_)
                    {
                        block_line = ld;
                        return false;
                    }
                }
            }
        }
        else if (ld->back_sector->floor_vertex_slope && ld->back_sector->line_count == 4 &&
                 PointInSubsector(move_check.mover->x, move_check.mover->y)->sector == ld->back_sector)
        {
            if (!ld->front_sector->floor_vertex_slope)
            {
                iz = ld->front_sector->floor_height;
                if (move_check.mover->z + move_check.mover->info_->step_size_ < iz)
                {
                    block_line = ld;
                    return false;
                }
            }
            else
            {
                float ix = 0;
                float iy = 0;
                ComputeIntersection(&divver, move_check.mover->x, move_check.mover->y, move_check.x, move_check.y, &ix,
                                    &iy);
                if (isfinite(ix) && isfinite(iy))
                {
                    iz = LinePlaneIntersection({{ix, iy, -40000}}, {{ix, iy, 40000}},
                                                   ld->front_sector->floor_z_vertices[2],
                                                   ld->front_sector->floor_vertex_slope_normal)
                             .Z;
                    if (isfinite(iz) && iz > move_check.mover->z + move_check.mover->info_->step_size_)
                    {
                        block_line = ld;
                        return false;
                    }
                }
            }
        }
        if (ld->front_sector->ceiling_vertex_slope && ld->front_sector->line_count == 4 &&
            PointInSubsector(move_check.mover->x, move_check.mover->y)->sector != ld->front_sector)
        {
            float ix = 0;
            float iy = 0;
            ComputeIntersection(&divver, move_check.mover->x, move_check.mover->y, move_check.x, move_check.y, &ix,
                                &iy);
            if (isfinite(ix) && isfinite(iy))
            {
                float icz = LinePlaneIntersection({{ix, iy, -40000}}, {{ix, iy, 40000}},
                                                      ld->front_sector->ceiling_z_vertices[2],
                                                      ld->front_sector->ceiling_vertex_slope_normal)
                                .Z;
                if (isfinite(icz) && icz <= iz + move_check.mover->height_)
                {
                    block_line = ld;
                    return false;
                }
            }
        }
        else if (ld->back_sector->ceiling_vertex_slope && ld->back_sector->line_count == 4 &&
                 PointInSubsector(move_check.mover->x, move_check.mover->y)->sector != ld->back_sector)
        {
            float ix = 0;
            float iy = 0;
            ComputeIntersection(&divver, move_check.mover->x, move_check.mover->y, move_check.x, move_check.y, &ix,
                                &iy);
            if (isfinite(ix) && isfinite(iy))
            {
                float icz = LinePlaneIntersection({{ix, iy, -40000}}, {{ix, iy, 40000}},
                                                      ld->back_sector->ceiling_z_vertices[2],
                                                      ld->back_sector->ceiling_vertex_slope_normal)
                                .Z;
                if (isfinite(icz) && icz <= iz + move_check.mover->height_)
                {
                    block_line = ld;
                    return false;
                }
            }
        }
        else if (ld->front_sector->ceiling_vertex_slope && ld->front_sector->line_count == 4 &&
                 PointInSubsector(move_check.mover->x, move_check.mover->y)->sector == ld->front_sector)
        {
            if (!ld->back_sector->ceiling_vertex_slope)
            {
                if (iz + move_check.mover->height_ >= ld->back_sector->ceiling_height)
                {
                    block_line = ld;
                    return false;
                }
            }
            else
            {
                float ix = 0;
                float iy = 0;
                ComputeIntersection(&divver, move_check.mover->x, move_check.mover->y, move_check.x, move_check.y, &ix,
                                    &iy);
                if (isfinite(ix) && isfinite(iy))
                {
                    float icz = LinePlaneIntersection({{ix, iy, -40000}}, {{ix, iy, 40000}},
                                                          ld->back_sector->ceiling_z_vertices[2],
                                                          ld->back_sector->ceiling_vertex_slope_normal)
                                    .Z;
                    if (isfinite(icz) && icz <= iz + move_check.mover->height_)
                    {
                        block_line = ld;
                        return false;
                    }
                }
            }
        }
        else if (ld->back_sector->ceiling_vertex_slope && ld->back_sector->line_count == 4 &&
                 PointInSubsector(move_check.mover->x, move_check.mover->y)->sector == ld->back_sector)
        {
            if (!ld->front_sector->ceiling_vertex_slope)
            {
                if (iz + move_check.mover->height_ >= ld->front_sector->ceiling_height)
                {
                    block_line = ld;
                    return false;
                }
            }
            else
            {
                float ix = 0;
                float iy = 0;
                ComputeIntersection(&divver, move_check.mover->x, move_check.mover->y, move_check.x, move_check.y, &ix,
                                    &iy);
                if (isfinite(ix) && isfinite(iy))
                {
                    float icz = LinePlaneIntersection({{ix, iy, -40000}}, {{ix, iy, 40000}},
                                                          ld->front_sector->ceiling_z_vertices[2],
                                                          ld->front_sector->ceiling_vertex_slope_normal)
                                    .Z;
                    if (isfinite(icz) && icz <= iz + move_check.mover->height_)
                    {
                        block_line = ld;
                        return false;
                    }
                }
            }
        }
        return true;
    }

    // CHOOSE GAP
    //
    // If this line borders a sector with multiple floors, then there will
    // be multiple gaps and we must choose one here, based on the thing's
    // current position (esp. Z).

    int i = FindThingGap(ld->gaps, ld->gap_number, move_check.z, move_check.z + move_check.mover->height_);

    // gap has been chosen. apply it.

    if (i >= 0)
    {
        if (ld->gaps[i].floor >= move_check.floor_z && !move_check.subsector->sector->floor_vertex_slope)
        {
            move_check.floor_z = ld->gaps[i].floor;
            move_check.below   = nullptr;
        }

        if (ld->gaps[i].ceiling < move_check.ceiling_z)
            move_check.ceiling_z = ld->gaps[i].ceiling;

        if (ld->gaps[i].floor < move_check.dropoff)
            move_check.dropoff = ld->gaps[i].floor;
    }
    else
    {
        move_check.ceiling_z = move_check.floor_z;
    }

    if (move_check.ceiling_z < move_check.floor_z + move_check.mover->height_)
        block_line = ld;

    if (!block_line)
    {
        if (move_check.line_count == 0)
            move_check.line_which = ld;

        move_check.line_count++;
    }

    return true;
}

static bool CheckRelativeThingCallback(MapObject *thing, void *data)
{
    float blockdist;
    bool  solid;

    if (thing == move_check.mover)
        return true;

    if (0 == (thing->flags_ &
              (kMapObjectFlagSolid | kMapObjectFlagSpecial | kMapObjectFlagShootable | kMapObjectFlagTouchy)))
        return true;

    blockdist = move_check.mover->radius_ + thing->radius_;

    // Check that we didn't hit it
    if (fabs(thing->x - move_check.x) >= blockdist || fabs(thing->y - move_check.y) >= blockdist)
        return true; // no we missed this thing

    // -KM- 1998/9/19 True 3d gameplay checks.
    if (level_flags.true_3d_gameplay && !(thing->flags_ & kMapObjectFlagSpecial))
    {
        float top_z = thing->z + thing->height_;

        // see if we went over
        if (move_check.z >= top_z)
        {
            if (top_z > move_check.floor_z && !(thing->flags_ & kMapObjectFlagMissile))
            {
                move_check.floor_z = top_z;
                move_check.below   = thing;
            }
            return true;
        }

        // see if we went underneath
        if (move_check.z + move_check.mover->height_ <= thing->z)
        {
            if (thing->z < move_check.ceiling_z && !(thing->flags_ & kMapObjectFlagMissile))
            {
                move_check.ceiling_z = thing->z;
            }
            return true;
        }

        // -AJA- 1999/07/21: allow climbing on top of things.

        if (top_z > move_check.floor_z && (thing->extended_flags_ & kExtendedFlagClimbable) &&
            (move_check.mover->player_ || (move_check.extended_flags & kExtendedFlagMonster)) &&
            ((move_check.flags & kMapObjectFlagDropOff) || (move_check.extended_flags & kExtendedFlagEdgeWalker)) &&
            (move_check.z + move_check.mover->info_->step_size_ >= top_z))
        {
            move_check.floor_z = top_z;
            move_check.below   = thing;
            return true;
        }
    }

    // check for skulls slamming into things
    // -ACB- 1998/08/04 Use procedure
    // -KM- 1998/09/01 After I noticed Skulls slamming into boxes of rockets...

    solid = (thing->flags_ & kMapObjectFlagSolid) ? true : false;

    if ((move_check.flags & kMapObjectFlagSkullFly) && solid)
    {
        SlammedIntoObject(move_check.mover, thing);

        // stop moving
        return false;
    }

    // check for missiles making contact
    // -ACB- 1998/08/04 Procedure for missile contact

    if (move_check.flags & kMapObjectFlagMissile)
    {
        // see if it went over / under
        if (move_check.z > thing->z + thing->height_)
            return true; // overhead

        if (move_check.z + move_check.mover->height_ < thing->z)
            return true; // underneath

        // ignore the missile's shooter
        if (move_check.mover->source_ && move_check.mover->source_ == thing)
            return true;

        if ((thing->hyper_flags_ & kHyperFlagMissilesPassThrough) && level_flags.pass_missile)
            return true;

        // thing isn't shootable, return depending on if the thing is solid.
        if (!(thing->flags_ & kMapObjectFlagShootable))
            return !solid;

        if (MissileContact(move_check.mover, thing) < 0)
            return true;

        return (move_check.extended_flags & kExtendedFlagTunnel) ? true : false;
    }

    // check for special pickup
    if ((move_check.flags & kMapObjectFlagPickup) && (thing->flags_ & kMapObjectFlagSpecial))
    {
        // can remove thing
        TouchSpecialThing(thing, move_check.mover);
    }

    // -AJA- 1999/08/21: check for touchy objects.
    if ((thing->flags_ & kMapObjectFlagTouchy) && (move_check.flags & kMapObjectFlagSolid) &&
        !(thing->extended_flags_ & kExtendedFlagUsable))
    {
        TouchyContact(thing, move_check.mover);
        return !solid;
    }

    if (thing->hyper_flags_ & kHyperFlagShoveable)
    { // Shoveable thing
        float ThrustSpeed = 8;
        PushMapObject(thing, move_check.mover, ThrustSpeed);
        // return false;
    }

    // -AJA- 2000/06/09: Follow MBF semantics: allow the non-solid
    // moving things to pass through solid things.
    return !solid || (thing->flags_ & kMapObjectFlagNoClip) || !(move_check.flags & kMapObjectFlagSolid);
}

//
// CheckRelativePosition
//
// Checks whether the thing can be moved to the position (x,y), which is
// assumed to be relative to the thing's current position.
//
// This is purely informative, nothing is modified
// (except things picked up).
//
// Only used by TryMove and ThingHeightClip.
//
// in:
//  a mobj_t (can be valid or invalid)
//  a position to be checked
//
// during:
//  special things are touched if kMapObjectFlagPickup
//  early out on solid lines?
//
// out:
//  move_check.subsector
//  move_check.floor_z
//  move_check.ceiling_z
//  move_check.dropoff
//  move_check.above
//  move_check.below
//  speciallines[]
//  numspeciallines
//
static bool CheckRelativePosition(MapObject *thing, float x, float y)
{
    map_object_hit_sky = false;
    block_line         = nullptr;

    move_check.mover          = thing;
    move_check.flags          = thing->flags_;
    move_check.extended_flags = thing->extended_flags_;

    move_check.x = x;
    move_check.y = y;
    move_check.z = thing->z;

    move_check.subsector = PointInSubsector(x, y);

    move_check.floor_slope_z   = 0;
    move_check.ceiling_slope_z = 0;

    // Vertex slope check here?
    if (move_check.subsector->sector->floor_vertex_slope)
    {
        HMM_Vec3 line_a{{move_check.x, move_check.y, -40000}};
        HMM_Vec3 line_b{{move_check.x, move_check.y, 40000}};
        float    z_test = LinePlaneIntersection(line_a, line_b, move_check.subsector->sector->floor_z_vertices[2],
                                                    move_check.subsector->sector->floor_vertex_slope_normal)
                           .Z;
        if (isfinite(z_test))
            move_check.floor_slope_z = z_test - move_check.subsector->sector->floor_height;
    }

    if (move_check.subsector->sector->ceiling_vertex_slope)
    {
        HMM_Vec3 line_a{{move_check.x, move_check.y, -40000}};
        HMM_Vec3 line_b{{move_check.x, move_check.y, 40000}};
        float    z_test = LinePlaneIntersection(line_a, line_b, move_check.subsector->sector->ceiling_z_vertices[2],
                                                    move_check.subsector->sector->ceiling_vertex_slope_normal)
                           .Z;
        if (isfinite(z_test))
            move_check.ceiling_slope_z = move_check.subsector->sector->ceiling_height - z_test;
    }

    float r = move_check.mover->radius_;

    move_check.bounding_box[kBoundingBoxLeft]   = x - r;
    move_check.bounding_box[kBoundingBoxBottom] = y - r;
    move_check.bounding_box[kBoundingBoxRight]  = x + r;
    move_check.bounding_box[kBoundingBoxTop]    = y + r;

    // The base floor / ceiling is from the sector that contains the
    // point.  Any contacted lines the step closer together will adjust them.
    // -AJA- 1999/07/19: Extra floor support.
    ComputeThingGap(thing, move_check.subsector->sector, move_check.z, &move_check.floor_z, &move_check.ceiling_z,
                    move_check.floor_slope_z, move_check.ceiling_slope_z);

    move_check.dropoff    = move_check.floor_z;
    move_check.above      = nullptr;
    move_check.below      = nullptr;
    move_check.line_count = 0;

    // can go anywhere
    if (move_check.flags & kMapObjectFlagNoClip)
        return true;

    special_lines_hit.clear();

    // -KM- 1998/11/25 Corpses aren't supposed to hang in the air...
    if (!(move_check.flags & (kMapObjectFlagNoClip | kMapObjectFlagCorpse)))
    {
        // check things first, possibly picking things up

        if (!BlockmapThingIterator(x - r, y - r, x + r, y + r, CheckRelativeThingCallback))
            return false;
    }

    // check lines

    thing->on_ladder_ = -1;

    if (!BlockmapLineIterator(x - r, y - r, x + r, y + r, CheckRelativeLineCallback))
        return false;

    return true;
}

//
// TryMove
//
// Attempt to move to a new position,
// crossing special lines unless kMapObjectFlagTeleport is set.
//
bool TryMove(MapObject *thing, float x, float y)
{
    float oldx;
    float oldy;
    Line *ld;
    bool  fell_off_thing;

    float z = thing->z;

    float_ok = false;

    // solid wall or thing ?
    if (!CheckRelativePosition(thing, x, y))
        return false;

    fell_off_thing = (thing->below_object_ && !move_check.below);

    if (!(thing->flags_ & kMapObjectFlagNoClip))
    {
        if (thing->height_ > move_check.ceiling_z - move_check.floor_z)
        {
            // doesn't fit
            if (!block_line && move_check.line_count >= 1)
                block_line = move_check.line_which;
            return false;
        }

        float_ok            = true;
        float_destination_z = move_check.floor_z;

        if (!(thing->flags_ & kMapObjectFlagTeleport) && (thing->z + thing->height_ > move_check.ceiling_z))
        {
            // mobj must lower itself to fit.
            if (!block_line && move_check.line_count >= 1)
                block_line = move_check.line_which;
            return false;
        }

        if (!(thing->flags_ & kMapObjectFlagTeleport) && (thing->z + thing->info_->step_size_) < move_check.floor_z)
        {
            // too big a step up.
            if (!block_line && move_check.line_count >= 1)
                block_line = move_check.line_which;
            return false;
        }

        if (!fell_off_thing && (thing->extended_flags_ & kExtendedFlagMonster) &&
            !(thing->flags_ & (kMapObjectFlagTeleport | kMapObjectFlagDropOff | kMapObjectFlagFloat)) &&
            (thing->z - thing->info_->step_size_) > move_check.floor_z)
        {
            // too big a step down.
            return false;
        }

        if (!fell_off_thing && (thing->extended_flags_ & kExtendedFlagMonster) &&
            !((thing->flags_ & (kMapObjectFlagDropOff | kMapObjectFlagFloat)) ||
              (thing->extended_flags_ & (kExtendedFlagEdgeWalker | kExtendedFlagWaterWalker))) &&
            (move_check.floor_z - move_check.dropoff > thing->info_->step_size_) &&
            (thing->floor_z_ - thing->dropoff_z_ <= thing->info_->step_size_))
        {
            // don't stand over a dropoff.
            return false;
        }
    }

    // the move is ok, so link the thing into its new position

    oldx              = thing->x;
    oldy              = thing->y;
    thing->floor_z_   = move_check.floor_z;
    thing->ceiling_z_ = move_check.ceiling_z;
    thing->dropoff_z_ = move_check.dropoff;

    // -AJA- 1999/08/02: Improved kMapObjectFlagTeleport handling.
    if (thing->flags_ & (kMapObjectFlagTeleport | kMapObjectFlagNoClip))
    {
        if (z <= thing->floor_z_)
            z = thing->floor_z_;
        else if (z + thing->height_ > thing->ceiling_z_)
            z = thing->ceiling_z_ - thing->height_;
    }

    ChangeThingPosition(thing, x, y, z);

    thing->SetAboveObject(move_check.above);
    thing->SetBelowObject(move_check.below);

    // if any special lines were hit, do the effect
    if (!special_lines_hit.empty() && !(thing->flags_ & (kMapObjectFlagTeleport | kMapObjectFlagNoClip)))
    {
        // Thing doesn't change, so we check the notriggerlines flag once..
        if (thing->player_ || (thing->extended_flags_ & kExtendedFlagMonster) ||
            !(thing->current_attack_ && (thing->current_attack_->flags_ & kAttackFlagNoTriggerLines)))
        {
            for (auto iter = special_lines_hit.rbegin(); iter != special_lines_hit.rend(); iter++)
            {
                ld = *iter;
                if (ld->special) // Shouldn't this always be a special?
                {
                    int side;
                    int oldside;

                    side    = PointOnLineSide(thing->x, thing->y, ld);
                    oldside = PointOnLineSide(oldx, oldy, ld);

                    if (side != oldside)
                    {
                        if (thing->flags_ & kMapObjectFlagMissile)
                            ShootSpecialLine(ld, oldside, thing->source_);
                        else
                            CrossSpecialLine(ld, oldside, thing);
                    }
                }
            }
        }
    }

    return true;
}

//
// ThingHeightClip
//
// Takes a valid thing and adjusts the thing->floor_z_, thing->ceiling_z_,
// and possibly thing->z.
//
// This is called for all nearby things whenever a sector changes height.
//
// If the thing doesn't fit, the z will be set to the lowest value
// and false will be returned.
//
static bool ThingHeightClip(MapObject *thing)
{
    bool onfloor = (fabs(thing->z - thing->floor_z_) < 1);

    if (!(thing->flags_ & kMapObjectFlagSolid))
    {
        thing->radius_ = thing->radius_ / 2 - 1;
        CheckRelativePosition(thing, thing->x, thing->y);
        thing->radius_ = (thing->radius_ + 1) * 2;
    }
    else
        CheckRelativePosition(thing, thing->x, thing->y);

    thing->floor_z_   = move_check.floor_z;
    thing->ceiling_z_ = move_check.ceiling_z;
    thing->dropoff_z_ = move_check.dropoff;

    thing->SetAboveObject(move_check.above);
    thing->SetBelowObject(move_check.below);

    if (onfloor)
    {
        // walking monsters rise and fall with the floor
        thing->z = thing->floor_z_;
    }
    else
    {
        // don't adjust a floating monster unless forced to
        if (thing->z + thing->height_ > thing->ceiling_z_)
            thing->z = thing->ceiling_z_ - thing->height_;
    }

    if (thing->ceiling_z_ - thing->floor_z_ < thing->height_)
        return false;

    return true;
}

//
// SLIDE MOVE
//
// Allows the player to slide along any angled walls.
//
static float best_slide_along;
static Line *best_slide_line;

static float slide_move_x;
static float slide_move_y;

static MapObject *slide_map_object;

//
// P_HitSlideLine
//
// Adjusts the xmove / ymove
// so that the next move will slide along the wall.
//
static void HitSlideLine(Line *ld)
{
    if (ld->slope_type == kLineClipHorizontal)
    {
        slide_move_y = 0;
        return;
    }

    if (ld->slope_type == kLineClipVertical)
    {
        slide_move_x = 0;
        return;
    }

    int side = PointOnLineSide(slide_map_object->x, slide_map_object->y, ld);

    BAMAngle lineangle = PointToAngle(0, 0, ld->delta_x, ld->delta_y);

    if (side == 1)
        lineangle += kBAMAngle180;

    BAMAngle moveangle  = PointToAngle(0, 0, slide_move_x, slide_move_y);
    BAMAngle deltaangle = moveangle - lineangle;

    if (deltaangle > kBAMAngle180)
        deltaangle += kBAMAngle180;
    // FatalError ("SlideLine: ang>kBAMAngle180");

    float movelen = ApproximateDistance(slide_move_x, slide_move_y);
    float newlen  = movelen * epi::BAMCos(deltaangle);

    slide_move_x = newlen * epi::BAMCos(lineangle);
    slide_move_y = newlen * epi::BAMSin(lineangle);
}

static bool PTR_SlideTraverse(PathIntercept *in, void *dataptr)
{
    Line *ld = in->line;

    EPI_ASSERT(ld);

    if (!(ld->flags & kLineFlagTwoSided))
    {
        // hit the back side ?
        if (PointOnLineSide(slide_map_object->x, slide_map_object->y, ld) != 0)
            return true;
    }

    // -AJA- 2022: allow sliding along railings (etc)
    bool is_blocking = false;

    if (slide_map_object->player_ != nullptr)
    {
        if (0 != (ld->flags & (kLineFlagBlocking | kLineFlagBlockPlayers)))
            is_blocking = true;
    }

    if (!is_blocking)
    {
        // -AJA- 1999/07/19: Gaps are now stored in line_t.

        for (int i = 0; i < ld->gap_number; i++)
        {
            // check if it can fit in the space
            if (slide_map_object->height_ > ld->gaps[i].ceiling - ld->gaps[i].floor)
                continue;

            // check slide mobj is not too high
            if (slide_map_object->z + slide_map_object->height_ > ld->gaps[i].ceiling)
                continue;

            // check slide mobj can step over
            if (slide_map_object->z + slide_map_object->info_->step_size_ < ld->gaps[i].floor)
                continue;

            return true;
        }
    }

    // the line does block movement,
    // see if it is closer than best so far
    if (in->along < best_slide_along)
    {
        best_slide_along = in->along;
        best_slide_line  = ld;
    }

    // stop
    return false;
}

//
// P_SlideMove
//
// The momx / momy move is bad, so try to slide along a wall.
//
// Find the first line hit, move flush to it, and slide along it
//
// -ACB- 1998/07/28 This is NO LONGER a kludgy mess; removed goto rubbish.
//
void SlideMove(MapObject *mo, float x, float y)
{
    slide_map_object = mo;

    float dx = x - mo->x;
    float dy = y - mo->y;

    for (int hitcount = 0; hitcount < 2; hitcount++)
    {
        float leadx, leady;
        float trailx, traily;

        // trace along the three leading corners
        if (dx > 0)
        {
            leadx  = mo->x + mo->radius_;
            trailx = mo->x - mo->radius_;
        }
        else
        {
            leadx  = mo->x - mo->radius_;
            trailx = mo->x + mo->radius_;
        }

        if (dy > 0)
        {
            leady  = mo->y + mo->radius_;
            traily = mo->y - mo->radius_;
        }
        else
        {
            leady  = mo->y - mo->radius_;
            traily = mo->y + mo->radius_;
        }

        best_slide_along = 1.0001f;

        PathTraverse(leadx, leady, leadx + dx, leady + dy, kPathAddLines, PTR_SlideTraverse);
        PathTraverse(trailx, leady, trailx + dx, leady + dy, kPathAddLines, PTR_SlideTraverse);
        PathTraverse(leadx, traily, leadx + dx, traily + dy, kPathAddLines, PTR_SlideTraverse);

        // move up to the wall
        if (AlmostEquals(best_slide_along, 1.0001f))
        {
            // the move must have hit the middle, so stairstep
            break; // goto stairstep
        }

        // fudge a bit to make sure it doesn't hit
        best_slide_along -= 0.01f;
        if (best_slide_along > 0.0f)
        {
            float newx = dx * best_slide_along;
            float newy = dy * best_slide_along;

            if (!TryMove(mo, mo->x + newx, mo->y + newy))
                break; // goto stairstep
        }

        // Now continue along the wall.
        // First calculate remainder.
        best_slide_along = 1.0f - (best_slide_along + 0.01f);

        if (best_slide_along > 1.0f)
            best_slide_along = 1.0f;

        if (best_slide_along <= 0.0f)
            return;

        slide_move_x = dx * best_slide_along;
        slide_move_y = dy * best_slide_along;

        HitSlideLine(best_slide_line); // clip the moves

        dx = slide_move_x;
        dy = slide_move_y;

        if (TryMove(mo, mo->x + slide_move_x, mo->y + slide_move_y))
            return;
    }

    // stairstep: last ditch attempt
    if (!TryMove(mo, mo->x, mo->y + dy))
        TryMove(mo, mo->x + dx, mo->y);
}

//
// PTR_AimTraverse
//
// Sets aim_check.target and slope when a target is aimed at.
//
static bool PTR_AimTraverse(PathIntercept *in, void *dataptr)
{
    float dist = aim_check.range * in->along;

    if (dist < 0.01f)
        return true;

    if (in->line)
    {
        Line *ld = in->line;

        if (!(ld->flags & kLineFlagTwoSided) || ld->gap_number == 0)
            return false; // stop

        // Crosses a two sided line.
        // A two sided line will restrict
        // the possible target ranges.
        //
        // -AJA- 1999/07/19: Gaps are now kept in line_t.

        if (!AlmostEquals(ld->front_sector->floor_height, ld->back_sector->floor_height))
        {
            float maxfloor = HMM_MAX(ld->front_sector->floor_height, ld->back_sector->floor_height);
            float slope    = (maxfloor - aim_check.start_z) / dist;

            if (slope > aim_check.bottom_slope)
                aim_check.bottom_slope = slope;
        }

        if (!AlmostEquals(ld->front_sector->ceiling_height, ld->back_sector->ceiling_height))
        {
            float minceil = HMM_MIN(ld->front_sector->ceiling_height, ld->back_sector->ceiling_height);
            float slope   = (minceil - aim_check.start_z) / dist;

            if (slope < aim_check.top_slope)
                aim_check.top_slope = slope;
        }

        if (aim_check.top_slope <= aim_check.bottom_slope)
            return false; // stop

        // shot continues
        return true;
    }

    // shoot a thing
    MapObject *mo = in->thing;

    EPI_ASSERT(mo);

    if (mo == aim_check.source)
        return true; // can't shoot self

    if (!(mo->flags_ & kMapObjectFlagShootable))
        return true; // has to be able to be shot

    if (mo->hyper_flags_ & kHyperFlagNoAutoaim)
        return true; // never should be aimed at

    if (aim_check.source && !aim_check.forced && (aim_check.source->side_ & mo->side_) != 0)
        return true; // don't aim at our good friend

    // check angles to see if the thing can be aimed at
    float thingtopslope = (mo->z + mo->height_ - aim_check.start_z) / dist;

    if (thingtopslope < aim_check.bottom_slope)
        return true; // shot over the thing

    float thingbottomslope = (mo->z - aim_check.start_z) / dist;

    if (thingbottomslope > aim_check.top_slope)
        return true; // shot under the thing

    // this thing can be hit!
    if (thingtopslope > aim_check.top_slope)
        thingtopslope = aim_check.top_slope;

    if (thingbottomslope < aim_check.bottom_slope)
        thingbottomslope = aim_check.bottom_slope;

    aim_check.slope  = (thingtopslope + thingbottomslope) / 2;
    aim_check.target = mo;

    return false; // don't go any farther
}

//
// PTR_AimTraverse2
//
// Sets aim_check.target and slope when a target is aimed at.
// Same as above except targets everything except scenery
//
static bool PTR_AimTraverse2(PathIntercept *in, void *dataptr)
{
    float dist = aim_check.range * in->along;

    if (dist < 0.01f)
        return true;

    if (in->line)
    {
        Line *ld = in->line;

        if (!(ld->flags & kLineFlagTwoSided) || ld->gap_number == 0)
            return false; // stop

        // Crosses a two sided line.
        // A two sided line will restrict
        // the possible target ranges.
        //
        // -AJA- 1999/07/19: Gaps are now kept in line_t.

        if (!AlmostEquals(ld->front_sector->floor_height, ld->back_sector->floor_height))
        {
            float maxfloor = HMM_MAX(ld->front_sector->floor_height, ld->back_sector->floor_height);
            float slope    = (maxfloor - aim_check.start_z) / dist;

            if (slope > aim_check.bottom_slope)
                aim_check.bottom_slope = slope;
        }

        if (!AlmostEquals(ld->front_sector->ceiling_height, ld->back_sector->ceiling_height))
        {
            float minceil = HMM_MIN(ld->front_sector->ceiling_height, ld->back_sector->ceiling_height);
            float slope   = (minceil - aim_check.start_z) / dist;

            if (slope < aim_check.top_slope)
                aim_check.top_slope = slope;
        }

        if (aim_check.top_slope <= aim_check.bottom_slope)
            return false; // stop

        // shot continues
        return true;
    }

    // shoot a thing
    MapObject *mo = in->thing;

    EPI_ASSERT(mo);

    if (mo == aim_check.source)
        return true;                                                    // can't shoot self

    if (aim_check.source && (aim_check.source->side_ & mo->side_) == 0) // not a friend
    {
        if (!(mo->extended_flags_ & kExtendedFlagMonster) && !(mo->flags_ & kMapObjectFlagSpecial))
            return true;                                                // scenery
    }
    if (mo->extended_flags_ & kExtendedFlagMonster && mo->health_ <= 0)
        return true;                                                    // don't aim at dead monsters

    if (mo->flags_ & kMapObjectFlagCorpse)
        return true;                                                    // don't aim at corpses

    if (mo->flags_ & kMapObjectFlagNoBlockmap)
        return true;                                                    // don't aim at inert things

    if (mo->flags_ & kMapObjectFlagNoSector)
        return true;                                                    // don't aim at invisible things

    // check angles to see if the thing can be aimed at
    float thingtopslope = (mo->z + mo->height_ - aim_check.start_z) / dist;

    if (thingtopslope < aim_check.bottom_slope)
        return true; // shot over the thing

    float thingbottomslope = (mo->z - aim_check.start_z) / dist;

    if (thingbottomslope > aim_check.top_slope)
        return true; // shot under the thing

    // this thing can be hit!
    if (thingtopslope > aim_check.top_slope)
        thingtopslope = aim_check.top_slope;

    if (thingbottomslope < aim_check.bottom_slope)
        thingbottomslope = aim_check.bottom_slope;

    aim_check.slope  = (thingtopslope + thingbottomslope) / 2;
    aim_check.target = mo;

    return false; // don't go any farther
}

static inline bool ShootCheckGap(float sx, float sy, float z, float floor_height, MapSurface *floor,
                                 float ceiling_height, MapSurface *ceil, Sector *sec_check, Line *ld)
{
    /* Returns true if successfully passed gap */

    // perfectly horizontal shots cannot hit planes
    if (AlmostEquals(shoot_check.slope, 0.0f) &&
        (!sec_check || (!sec_check->floor_vertex_slope && !sec_check->ceiling_vertex_slope)))
        return true;

    if (sec_check && sec_check->floor_vertex_slope)
    {
        if (sec_check->floor_vertex_slope_high_low.X > sec_check->floor_height)
        {
            // Check to see if hitting the side of a vertex slope sector
            HMM_Vec3 tri_v1 = {{0, 0, 0}};
            HMM_Vec3 tri_v2 = {{0, 0, 0}};
            for (auto v : sec_check->floor_z_vertices)
            {
                if (AlmostEquals(ld->vertex_1->X, v.X) && AlmostEquals(ld->vertex_1->Y, v.Y))
                {
                    tri_v1.X = v.X;
                    tri_v1.Y = v.Y;
                    tri_v1.Z = v.Z;
                }
                else if (AlmostEquals(ld->vertex_2->X, v.X) && AlmostEquals(ld->vertex_2->Y, v.Y))
                {
                    tri_v2.X = v.X;
                    tri_v2.Y = v.Y;
                    tri_v2.Z = v.Z;
                }
            }
            if (AlmostEquals(tri_v1.Z, tri_v2.Z) &&
                AlmostEquals(HMM_Clamp(HMM_MIN(sec_check->floor_height, tri_v1.Z), z,
                                       HMM_MAX(sec_check->floor_height, tri_v1.Z)),
                             z)) // Hitting rectangular side; no fancier check needed
            {
                if (shoot_check.puff)
                {
                    sx -= trace.delta_x * 6.0f / shoot_check.range;
                    sy -= trace.delta_y * 6.0f / shoot_check.range;
                    SpawnPuff(sx, sy, z, shoot_check.puff, shoot_check.angle + kBAMAngle180);
                }
                return false;
            }
            else
            {
                // Test point against 2D projection of the slope side
                if (std::abs(tri_v1.X - tri_v2.X) > std::abs(tri_v1.Y - tri_v2.Y))
                {
                    if (PointInTriangle({{tri_v1.X, tri_v1.Z}}, {{tri_v2.X, tri_v2.Z}},
                                            {{(tri_v1.Z > tri_v2.Z ? tri_v1.X : tri_v2.X), sec_check->floor_height}},
                                            {{sx, z}}))
                    {
                        if (shoot_check.puff)
                        {
                            sx -= trace.delta_x * 6.0f / shoot_check.range;
                            sy -= trace.delta_y * 6.0f / shoot_check.range;
                            SpawnPuff(sx, sy, z, shoot_check.puff, shoot_check.angle + kBAMAngle180);
                        }
                        return false;
                    }
                }
                else
                {
                    if (PointInTriangle({{tri_v1.Y, tri_v1.Z}}, {{tri_v2.Y, tri_v2.Z}},
                                            {{(tri_v1.Z > tri_v2.Z ? tri_v1.Y : tri_v2.Y), sec_check->floor_height}},
                                            {{sy, z}}))
                    {
                        if (shoot_check.puff)
                            SpawnPuff(sx, sy, z, shoot_check.puff, shoot_check.angle + kBAMAngle180);
                        return false;
                    }
                }
            }
        }
    }
    if (sec_check && sec_check->ceiling_vertex_slope)
    {
        if (sec_check->ceiling_vertex_slope_high_low.Y < sec_check->ceiling_height)
        {
            // Check to see if hitting the side of a vertex slope sector
            HMM_Vec3 tri_v1 = {{0, 0, 0}};
            HMM_Vec3 tri_v2 = {{0, 0, 0}};
            for (auto v : sec_check->ceiling_z_vertices)
            {
                if (AlmostEquals(ld->vertex_1->X, v.X) && AlmostEquals(ld->vertex_1->Y, v.Y))
                {
                    tri_v1.X = v.X;
                    tri_v1.Y = v.Y;
                    tri_v1.Z = v.Z;
                }
                else if (AlmostEquals(ld->vertex_2->X, v.X) && AlmostEquals(ld->vertex_2->Y, v.Y))
                {
                    tri_v2.X = v.X;
                    tri_v2.Y = v.Y;
                    tri_v2.Z = v.Z;
                }
            }
            if (AlmostEquals(tri_v1.Z, tri_v2.Z) &&
                AlmostEquals(HMM_Clamp(HMM_MIN(sec_check->ceiling_height, tri_v1.Z), z,
                                       HMM_MAX(sec_check->ceiling_height, tri_v1.Z)),
                             z)) // Hitting rectangular side; no fancier check needed
            {
                if (shoot_check.puff)
                {
                    sx -= trace.delta_x * 6.0f / shoot_check.range;
                    sy -= trace.delta_y * 6.0f / shoot_check.range;
                    SpawnPuff(sx, sy, z, shoot_check.puff, shoot_check.angle + kBAMAngle180);
                }
                return false;
            }
            else
            {
                // Test point against 2D projection of the slope side
                if (std::abs(tri_v1.X - tri_v2.X) > std::abs(tri_v1.Y - tri_v2.Y))
                {
                    if (PointInTriangle({{tri_v1.X, tri_v1.Z}}, {{tri_v2.X, tri_v2.Z}},
                                            {{(tri_v1.Z < tri_v2.Z ? tri_v1.X : tri_v2.X), sec_check->ceiling_height}},
                                            {{sx, z}}))
                    {
                        if (shoot_check.puff)
                        {
                            sx -= trace.delta_x * 6.0f / shoot_check.range;
                            sy -= trace.delta_y * 6.0f / shoot_check.range;
                            SpawnPuff(sx, sy, z, shoot_check.puff, shoot_check.angle + kBAMAngle180);
                        }
                        return false;
                    }
                }
                else
                {
                    if (PointInTriangle({{tri_v1.Y, tri_v1.Z}}, {{tri_v2.Y, tri_v2.Z}},
                                            {{(tri_v1.Z < tri_v2.Z ? tri_v1.Y : tri_v2.Y), sec_check->ceiling_height}},
                                            {{sy, z}}))
                    {
                        if (shoot_check.puff)
                            SpawnPuff(sx, sy, z, shoot_check.puff, shoot_check.angle + kBAMAngle180);
                        return false;
                    }
                }
            }
        }
    }

    // check if hit the floor
    if (shoot_check.previous_z > floor_height && z < floor_height)
    { /* nothing */
    }
    // check if hit the ceiling
    else if (shoot_check.previous_z < ceiling_height && z > ceiling_height)
    {
        floor_height = ceiling_height;
        floor        = ceil;
    }
    else
    {
        if (sec_check && sec_check->floor_vertex_slope)
        {
            // Check floor vertex slope intersect from shooter's angle
            HMM_Vec3 shoota = LinePlaneIntersection(
                {{shoot_check.source->x, shoot_check.source->y, shoot_check.start_z}}, {{sx, sy, z}},
                sec_check->floor_z_vertices[2], sec_check->floor_vertex_slope_normal);
            Sector *shoota_sec = PointInSubsector(shoota.X, shoota.Y)->sector;
            if (shoota_sec && shoota_sec == sec_check && shoota.Z <= sec_check->floor_vertex_slope_high_low.X &&
                shoota.Z >= sec_check->floor_vertex_slope_high_low.Y)
            {
                // It will strike the floor slope in this sector; see if it will
                // hit a thing first, otherwise let it hit the slope
                if (PathTraverse(sx, sy, shoota.X, shoota.Y, kPathAddThings, ShootTraverseCallback))
                {
                    if (shoot_check.puff)
                        SpawnPuff(shoota.X, shoota.Y, shoota.Z, shoot_check.puff, shoot_check.angle + kBAMAngle180);
                    return false;
                }
            }
            else if (sec_check->ceiling_vertex_slope)
            {
                // Check ceiling vertex slope intersect from shooter's angle
                shoota = LinePlaneIntersection(
                    {{shoot_check.source->x, shoot_check.source->y, shoot_check.start_z}}, {{sx, sy, z}},
                    sec_check->ceiling_z_vertices[2], sec_check->ceiling_vertex_slope_normal);
                shoota_sec = PointInSubsector(shoota.X, shoota.Y)->sector;
                if (shoota_sec && shoota_sec == sec_check && shoota.Z <= sec_check->ceiling_vertex_slope_high_low.X &&
                    shoota.Z >= sec_check->ceiling_vertex_slope_high_low.Y)
                {
                    // It will strike the ceiling slope in this sector; see if
                    // it will hit a thing first, otherwise let it hit the slope
                    if (PathTraverse(sx, sy, shoota.X, shoota.Y, kPathAddThings, ShootTraverseCallback))
                    {
                        if (shoot_check.puff)
                            SpawnPuff(shoota.X, shoota.Y, shoota.Z, shoot_check.puff, shoot_check.angle + kBAMAngle180);
                        return false;
                    }
                }
                else
                    return true;
            }
            else
                return true;
        }
        else if (sec_check && sec_check->ceiling_vertex_slope)
        {
            // Check ceiling vertex slope intersect from shooter's angle
            HMM_Vec3 shoota = LinePlaneIntersection(
                {{shoot_check.source->x, shoot_check.source->y, shoot_check.start_z}}, {{sx, sy, z}},
                sec_check->ceiling_z_vertices[2], sec_check->ceiling_vertex_slope_normal);
            Sector *shoota_sec = PointInSubsector(shoota.X, shoota.Y)->sector;
            if (shoota_sec && shoota_sec == sec_check && shoota.Z <= sec_check->ceiling_vertex_slope_high_low.X &&
                shoota.Z >= sec_check->ceiling_vertex_slope_high_low.Y)
            {
                // It will strike the ceiling slope in this sector; see if it
                // will hit a thing first, otherwise let it hit the slope
                if (PathTraverse(sx, sy, shoota.X, shoota.Y, kPathAddThings, ShootTraverseCallback))
                {
                    if (shoot_check.puff)
                        SpawnPuff(shoota.X, shoota.Y, shoota.Z, shoot_check.puff, shoot_check.angle + kBAMAngle180);
                    return false;
                }
            }
            else
                return true;
        }
        else
            return true;
    }

    // don't shoot the sky!
    if (EDGE_IMAGE_IS_SKY(floor[0]))
        return false;

    float along = (floor_height - shoot_check.start_z) / (shoot_check.slope * shoot_check.range);

    float x = trace.x + trace.delta_x * along;
    float y = trace.y + trace.delta_y * along;

    z = (z < shoot_check.previous_z) ? floor_height + 2 : floor_height - 2;

    // Check for vert slope at potential puff point
    Sector *last_shoota_sec = PointInSubsector(x, y)->sector;

    if (last_shoota_sec && (last_shoota_sec->floor_vertex_slope || last_shoota_sec->ceiling_vertex_slope))
    {
        bool fs_good = true;
        bool cs_good = true;
        if (last_shoota_sec->floor_vertex_slope)
        {
            if (z <= LinePlaneIntersection({{x, y, -40000}}, {{x, y, 40000}}, last_shoota_sec->floor_z_vertices[2],
                                               last_shoota_sec->floor_vertex_slope_normal)
                         .Z)
                fs_good = false;
        }
        if (last_shoota_sec->ceiling_vertex_slope)
        {
            if (z >= LinePlaneIntersection({{x, y, -40000}}, {{x, y, 40000}},
                                               last_shoota_sec->ceiling_z_vertices[2],
                                               last_shoota_sec->ceiling_vertex_slope_normal)
                         .Z)
                cs_good = false;
        }
        if (fs_good && cs_good)
            return true;
    }

    // Lobo 2021: respect our NO_TRIGGER_LINES attack flag
    if (!shoot_check.source || !shoot_check.source->current_attack_ ||
        !(shoot_check.source->current_attack_->flags_ & kAttackFlagNoTriggerLines))
    {
        const char     *flat            = floor->image->name_.c_str();
        FlatDefinition *current_flatdef = flatdefs.Find(flat);
        if (current_flatdef)
        {
            if (current_flatdef->impactobject_)
            {
                BAMAngle angle = shoot_check.angle + kBAMAngle180;
                angle += (BAMAngle)(RandomByteSkewToZeroDeterministic() * (int)(kBAMAngle1 / 2));

                SpawnDebris(x, y, z, angle, current_flatdef->impactobject_);
                // don't go any farther
                return false;
            }
        }
    }

    // Spawn bullet puff
    if (shoot_check.puff)
        SpawnPuff(x, y, z, shoot_check.puff, shoot_check.angle + kBAMAngle180);

    // don't go any farther
    return false;
}

// Lobo: 2022
// Try and get a texture for our midtex.
//-If we specified a LINE_PART copy that texture over.
//-If not, just remove the current midtex we have (only on 2-sided lines).
bool ReplaceMidTexFromPart(Line *TheLine, ScrollingPart parts)
{
    bool IsFront = true;

    if (parts <= kScrollingPartRightLower) // assume right is back
        IsFront = false;

    if (IsFront == false)
    {
        if (!TheLine->side[1]) // back and 1-sided so no-go
            return false;
    }
    Side *side = (IsFront) ? TheLine->side[0] : TheLine->side[1];

    const Image *image = nullptr;

    // if (parts == kScrollingPartNone)
    // return false;
    // parts = (scroll_part_e)(kScrollingPartLEFT | kScrollingPartRIGHT);

    if (parts & (kScrollingPartLeftUpper))
    {
        image = side->top.image;
    }
    if (parts & (kScrollingPartRightUpper))
    {
        image = side->top.image;
    }
    if (parts & (kScrollingPartLeftLower))
    {
        image = side->bottom.image;
    }
    if (parts & (kScrollingPartRightLower))
    {
        image = side->bottom.image;
    }

    if (parts & (kScrollingPartLeftMiddle))
    {
        image = side->middle.image; // redundant but whatever ;)
    }
    if (parts & (kScrollingPartRightMiddle))
    {
        image = side->middle.image;                       // redundant but whatever ;)
    }

    if (!image && !TheLine->side[1])                      // no image and 1-sided so leave alone
        return false;

    if (!image)                                           // 2 sided and no image so add default
    {
        image = ImageLookup("-", kImageNamespaceTexture); // default is blank
    }

    TheLine->side[0]->middle.image = image;

    if (TheLine->side[1])
        TheLine->side[1]->middle.image = image;

    return true;
}

//
// Lobo:2021 Unblock and remove texture from our special debris linetype.
//
void UnblockLineEffectDebris(Line *TheLine, const LineType *special)
{
    if (!TheLine)
    {
        return;
    }

    bool TwoSided = false;

    if (TheLine->side[0] && TheLine->side[1])
        TwoSided = true;

    if (special->glass_)
    {
        // 1. Change the texture on our line

        // if it's got a BROKEN_TEXTURE=<tex> then use that
        if (!special->brokentex_.empty())
        {
            const Image *image             = ImageLookup(special->brokentex_.c_str(), kImageNamespaceTexture);
            TheLine->side[0]->middle.image = image;
            if (TwoSided)
            {
                TheLine->side[1]->middle.image = image;
            }
        }
        else // otherwise try get the texture from our LINE_PART=
        {
            ReplaceMidTexFromPart(TheLine, special->line_parts_);
        }

        // 2. if it's 2 sided, make it unblocking now
        if (TwoSided)
        {
            // clear standard flags
            TheLine->flags &=
                ~(kLineFlagBlocking | kLineFlagBlockMonsters | kLineFlagBlockGroundedMonsters | kLineFlagBlockPlayers | kLineFlagSoundBlock);

            // clear EDGE's extended lineflags too
            TheLine->flags &= ~(kLineFlagSightBlock | kLineFlagShootBlock);
        }
    }
}

static bool ShootTraverseCallback(PathIntercept *in, void *dataptr)
{
    float dist = shoot_check.range * in->along;

    if (dist < 0.1f)
        dist = 0.1f;

    // Intercept is a line?
    if (in->line)
    {
        Line *ld = in->line;

        // determine coordinates of intersect
        float along = in->along;
        float x     = trace.x + trace.delta_x * along;
        float y     = trace.y + trace.delta_y * along;
        float z     = shoot_check.start_z + along * shoot_check.slope * shoot_check.range;

        int   sidenum = PointOnLineSide(trace.x, trace.y, ld);
        Side *side    = ld->side[sidenum];

        // ShootSpecialLine()->P_ActivateSpecialLine() can remove
        //  the special so we need to get the info before calling it
        const LineType *tempspecial = ld->special;

        // Lobo 2021: moved the line check (2.) to be after
        // the floor/ceiling check (1.)

        //(1.) check if shot has hit a floor or ceiling...
        if (side)
        {
            Extrafloor *ef;
            MapSurface *floor_s   = &side->sector->floor;
            float       floor_h   = side->sector->floor_height;
            Sector     *sec_check = nullptr;
            if (ld->side[sidenum ^ 1])
                sec_check = ld->side[sidenum ^ 1]->sector;

            // FIXME: must go in correct order
            for (ef = side->sector->bottom_extrafloor; ef; ef = ef->higher)
            {
                if (!ShootCheckGap(x, y, z, floor_h, floor_s, ef->bottom_height, ef->bottom, sec_check, ld))
                    return false;

                floor_s = ef->top;
                floor_h = ef->top_height;
            }

            if (!ShootCheckGap(x, y, z, floor_h, floor_s, side->sector->ceiling_height, &side->sector->ceiling,
                               sec_check, ld))
            {
                return false;
            }
        }

        //(2.) Line is a special, Cause action....
        // -AJA- honour the NO_TRIGGER_LINES attack special too
        if (ld->special && (!shoot_check.source || !shoot_check.source->current_attack_ ||
                            !(shoot_check.source->current_attack_->flags_ & kAttackFlagNoTriggerLines)))
        {
            ShootSpecialLine(ld, sidenum, shoot_check.source);
        }

        // shot doesn't go through a one-sided line, since one sided lines
        // do not have a sector on the other side.

        if ((ld->flags & kLineFlagTwoSided) && ld->gap_number > 0 && !(ld->flags & kLineFlagShootBlock))
        {
            EPI_ASSERT(ld->back_sector);

            // check all line gaps
            for (int i = 0; i < ld->gap_number; i++)
            {
                if (ld->gaps[i].floor <= z && z <= ld->gaps[i].ceiling)
                {
                    shoot_check.previous_z = z;
                    return true;
                }
            }
        }

        // check if bullet hit a sky hack line...
        if (ld->front_sector && ld->back_sector)
        {
            if (EDGE_IMAGE_IS_SKY(ld->front_sector->ceiling) && EDGE_IMAGE_IS_SKY(ld->back_sector->ceiling))
            {
                float c1 = ld->front_sector->ceiling_height;
                float c2 = ld->back_sector->ceiling_height;

                if (HMM_MIN(c1, c2) <= z && z <= HMM_MAX(c1, c2))
                    return false;
            }

            if (EDGE_IMAGE_IS_SKY(ld->front_sector->floor) && EDGE_IMAGE_IS_SKY(ld->back_sector->floor))
            {
                float f1 = ld->front_sector->floor_height;
                float f2 = ld->back_sector->floor_height;

                if (HMM_MIN(f1, f2) <= z && z <= HMM_MAX(f1, f2))
                    return false;
            }
        }

        Sector *last_shoota_sec = PointInSubsector(x, y)->sector;

        if (last_shoota_sec &&
            ((ld->front_sector && (ld->front_sector->floor_vertex_slope || ld->front_sector->ceiling_vertex_slope)) ||
             (ld->back_sector && (ld->back_sector->floor_vertex_slope || ld->back_sector->ceiling_vertex_slope))))
        {
            bool fs_good = true;
            bool cs_good = true;
            if (last_shoota_sec->floor_vertex_slope)
            {
                if (z <= LinePlaneIntersection({{x, y, -40000}}, {{x, y, 40000}},
                                                   last_shoota_sec->floor_z_vertices[2],
                                                   last_shoota_sec->floor_vertex_slope_normal)
                             .Z)
                    fs_good = false;
            }
            else
            {
                if (z <= last_shoota_sec->floor_height)
                    fs_good = false;
            }
            if (last_shoota_sec->ceiling_vertex_slope)
            {
                if (z >= LinePlaneIntersection({{x, y, -40000}}, {{x, y, 40000}},
                                                   last_shoota_sec->ceiling_z_vertices[2],
                                                   last_shoota_sec->ceiling_vertex_slope_normal)
                             .Z)
                    cs_good = false;
            }
            else
            {
                if (z >= last_shoota_sec->ceiling_height)
                    cs_good = false;
            }
            if (fs_good && cs_good)
                return true;
        }

        // position puff off the wall
        x -= trace.delta_x * 6.0f / shoot_check.range;
        y -= trace.delta_y * 6.0f / shoot_check.range;

        // Spawn bullet puffs.
        if (shoot_check.puff)
            SpawnPuff(x, y, z, shoot_check.puff, shoot_check.angle + kBAMAngle180);

        // Lobo:2022
        // Check if we're using EFFECT_OBJECT for this line
        // and spawn that as well as the previous bullet puff
        if (tempspecial && (!shoot_check.source || !shoot_check.source->current_attack_ ||
                            !(shoot_check.source->current_attack_->flags_ & kAttackFlagNoTriggerLines)))
        {
            const MapObjectDefinition *info;
            info = tempspecial->effectobject_;

            if (info && tempspecial->type_ == kLineTriggerShootable)
            {
                SpawnDebris(x, y, z, shoot_check.angle + kBAMAngle180, info);
            }
            UnblockLineEffectDebris(ld, tempspecial);
        }

        // don't go any farther
        return false;
    }

    // shoot a thing
    MapObject *mo = in->thing;

    EPI_ASSERT(mo);

    // don't shoot self
    if (mo == shoot_check.source)
        return true;

    // got to able to shoot it
    if (!(mo->flags_ & kMapObjectFlagShootable) && !(mo->extended_flags_ & kExtendedFlagBlockShots))
        return true;

    // check angles to see if the thing can be aimed at
    float thingtopslope = (mo->z + mo->height_ - shoot_check.start_z) / dist;

    // shot over the thing ?
    if (thingtopslope < shoot_check.slope)
        return true;

    float thingbottomslope = (mo->z - shoot_check.start_z) / dist;

    // shot under the thing ?
    if (thingbottomslope > shoot_check.slope)
        return true;

    // hit thing

    // Checking sight against target on vertex slope?
    if (mo->subsector_->sector || mo->subsector_->sector->ceiling_vertex_slope)
        mo->slope_sight_hit_ = true;

    // position a bit closer
    float along = in->along - 10.0f / shoot_check.range;

    float x = trace.x + trace.delta_x * along;
    float y = trace.y + trace.delta_y * along;
    float z = shoot_check.start_z + along * shoot_check.slope * shoot_check.range;

    // Spawn bullet puffs or blood spots,
    // depending on target type.

    bool use_blood =
        (mo->flags_ & kMapObjectFlagShootable) && !(mo->flags_ & kMapObjectFlagNoBlood) && (gore_level.d_ < 2);

    if (mo->flags_ & kMapObjectFlagShootable)
    {
        int what = BulletContact(shoot_check.source, mo, shoot_check.damage, shoot_check.damage_type, x, y, z);

        // bullets pass through?
        if (what < 0)
            return true;

        if (what == 0)
            use_blood = false;
    }

    if (use_blood)
    {
        if (shoot_check.blood)
            SpawnBlood(x, y, z, shoot_check.damage, shoot_check.angle, shoot_check.blood);
        else if (mo->info_->blood_)
            SpawnBlood(x, y, z, shoot_check.damage, shoot_check.angle, mo->info_->blood_);
    }
    else
    {
        if (shoot_check.puff)
            SpawnPuff(x, y, z, shoot_check.puff, shoot_check.angle + kBAMAngle180);
    }

    // don't go any farther
    return false;
}

MapObject *AimLineAttack(MapObject *t1, BAMAngle angle, float distance, float *slope)
{
    float x2 = t1->x + distance * epi::BAMCos(angle);
    float y2 = t1->y + distance * epi::BAMSin(angle);

    EPI_CLEAR_MEMORY(&aim_check, ShootAttempt, 1);

    if (t1->info_)
        aim_check.start_z = t1->z + t1->height_ * t1->info_->shotheight_;
    else
        aim_check.start_z = t1->z + t1->height_ / 2 + 8;

    if (t1->player_)
    {
        float vertslope = epi::BAMTan(t1->vertical_angle_);

        aim_check.top_slope    = (vertslope * 256.0f + 100.0f) / 160.0f;
        aim_check.bottom_slope = (vertslope * 256.0f - 100.0f) / 160.0f;
    }
    else
    {
        aim_check.top_slope    = 100.0f / 160.0f;
        aim_check.bottom_slope = -100.0f / 160.0f;
    }

    aim_check.source = t1;
    aim_check.range  = distance;
    aim_check.angle  = angle;
    aim_check.slope  = 0.0f;
    aim_check.target = nullptr;

    PathTraverse(t1->x, t1->y, x2, y2, kPathAddLines | kPathAddThings, PTR_AimTraverse);

    if (slope)
        (*slope) = aim_check.slope;

    return aim_check.target;
}

void LineAttack(MapObject *t1, BAMAngle angle, float distance, float slope, float damage, const DamageClass *damtype,
                const MapObjectDefinition *puff, const MapObjectDefinition *blood)
{
    // Note: Damtype can be nullptr.

    float x2 = t1->x + distance * epi::BAMCos(angle);
    float y2 = t1->y + distance * epi::BAMSin(angle);

    EPI_CLEAR_MEMORY(&shoot_check, ShootAttempt, 1);

    if (t1->info_)
        shoot_check.start_z = t1->z + t1->height_ * t1->info_->shotheight_;
    else
        shoot_check.start_z = t1->z + t1->height_ / 2 + 8;

    shoot_check.source      = t1;
    shoot_check.range       = distance;
    shoot_check.angle       = angle;
    shoot_check.slope       = slope;
    shoot_check.damage      = damage;
    shoot_check.damage_type = damtype;
    shoot_check.previous_z  = shoot_check.start_z;
    shoot_check.puff        = puff;
    shoot_check.blood       = blood;

    PathTraverse(t1->x, t1->y, x2, y2, kPathAddLines | kPathAddThings, ShootTraverseCallback);
}

//
// P_TargetTheory
//
// Compute destination for projectiles, allowing for targets that
// don't exist (e.g. since we have autoaim disabled).
//
// -AJA- 2005/02/07: Rewrote the DUMMYMOBJ stuff.
//
void TargetTheory(MapObject *source, MapObject *target, float *x, float *y, float *z)
{
    if (target)
    {
        (*x) = target->x;
        (*y) = target->y;
        (*z) = MapObjectMidZ(target);
    }
    else
    {
        float start_z;

        if (source->info_)
            start_z = source->z + source->height_ * source->info_->shotheight_;
        else
            start_z = source->z + source->height_ / 2 + 8;

        (*x) = source->x + kMissileRange * epi::BAMCos(source->angle_);
        (*y) = source->y + kMissileRange * epi::BAMSin(source->angle_);
        (*z) = start_z + kMissileRange * epi::BAMTan(source->vertical_angle_);
    }
}

MapObject *GetMapTargetAimInfo(MapObject *source, BAMAngle angle, float distance)
{
    float x2, y2;

    EPI_CLEAR_MEMORY(&aim_check, ShootAttempt, 1);

    aim_check.source = source;
    aim_check.forced = false;

    x2 = source->x + distance * epi::BAMCos(angle);
    y2 = source->y + distance * epi::BAMSin(angle);

    if (source->info_)
        aim_check.start_z = source->z + source->height_ * source->info_->shotheight_;
    else
        aim_check.start_z = source->z + source->height_ / 2 + 8;

    aim_check.range  = distance;
    aim_check.target = nullptr;

    // Lobo: try and limit the vertical range somewhat
    float vertslope        = epi::BAMTan(source->vertical_angle_);
    aim_check.top_slope    = (100 + vertslope * 320) / 160.0f;
    aim_check.bottom_slope = (-100 + vertslope * 576) / 160.0f;

    PathTraverse(source->x, source->y, x2, y2, kPathAddLines | kPathAddThings, PTR_AimTraverse2);

    if (!aim_check.target)
        return nullptr;

    return aim_check.target;
}

//
// P_MapTargetAutoAim
//
// Returns a moving object for a target.  Will search for a mobj
// to lock onto.  Returns nullptr if nothing could be locked onto.
//
// -ACB- 1998/09/01
// -AJA- 1999/08/08: Added `force_aim' to fix chainsaw.
//
MapObject *DoMapTargetAutoAim(MapObject *source, BAMAngle angle, float distance, bool force_aim)
{
    float x2, y2;

    // -KM- 1999/01/31 Autoaim is an option.
    if (source->player_ && !level_flags.autoaim && !force_aim)
    {
        return nullptr;
    }

    EPI_CLEAR_MEMORY(&aim_check, ShootAttempt, 1);

    aim_check.source = source;
    aim_check.forced = force_aim;

    x2 = source->x + distance * epi::BAMCos(angle);
    y2 = source->y + distance * epi::BAMSin(angle);

    if (source->info_)
        aim_check.start_z = source->z + source->height_ * source->info_->shotheight_;
    else
        aim_check.start_z = source->z + source->height_ / 2 + 8;

    if (source->player_)
    {
        float vertslope = epi::BAMTan(source->vertical_angle_);

        aim_check.top_slope    = (100 + vertslope * 256) / 160.0f;
        aim_check.bottom_slope = (-100 + vertslope * 256) / 160.0f;
    }
    else
    {
        aim_check.top_slope    = 100.0f / 160.0f;
        aim_check.bottom_slope = -100.0f / 160.0f;
    }

    aim_check.range  = distance;
    aim_check.target = nullptr;

    PathTraverse(source->x, source->y, x2, y2, kPathAddLines | kPathAddThings, PTR_AimTraverse);

    if (!aim_check.target)
        return nullptr;

    // -KM- 1999/01/31 Look at the thing you aimed at.  Is sometimes
    //   useful, sometimes annoying :-)
    // Dasho: Updated to have the player know of and be able to choose "Snap To" behavior
    if (source->player_ && (level_flags.autoaim == kAutoAimVerticalSnap || level_flags.autoaim == kAutoAimFullSnap))
    {
        float slope = ApproximateSlope(source->x - aim_check.target->x, source->y - aim_check.target->y,
                                       aim_check.target->z - source->z);

        slope = HMM_Clamp(-1.0f, slope, 1.0f);

        source->vertical_angle_ = epi::BAMFromATan(slope);

        if (level_flags.autoaim == kAutoAimFullSnap)
            source->angle_ = angle;
    }

    return aim_check.target;
}

MapObject *MapTargetAutoAim(MapObject *source, BAMAngle angle, float distance, bool force_aim)
{
    MapObject *target = DoMapTargetAutoAim(source, angle, distance, force_aim);

    // If that is a miss, aim slightly to the left or right in full autoaim
    if (!target && source->player_ && level_flags.autoaim > kAutoAimVerticalSnap)
    {
        BAMAngle diff = kBAMAngle5;

        if (level_time_elapsed & 1)
            diff = 0 - diff;

        MapObject *T2 = DoMapTargetAutoAim(source, angle + diff, distance, force_aim);
        if (T2)
            return T2;

        T2 = DoMapTargetAutoAim(source, angle - diff, distance, force_aim);
        if (T2)
            return T2;
    }

    return target;
}

//
// USE LINES
//
static MapObject *use_thing;
static float      use_lower, use_upper;

static bool PTR_UseTraverse(PathIntercept *in, void *dataptr)
{
    // intercept is a thing ?
    if (in->thing)
    {
        MapObject *mo = in->thing;

        // not a usable thing ?
        if (!(mo->extended_flags_ & kExtendedFlagUsable) || !mo->info_->touch_state_)
            return true;

        if (!UseThing(use_thing, mo, use_lower, use_upper))
            return true;

        // don't go any farther (thing was usable)
        return false;
    }

    Line *ld = in->line;

    EPI_ASSERT(ld);

    int sidenum = PointOnLineSide(use_thing->x, use_thing->y, ld);
    sidenum     = (sidenum == 1) ? 1 : 0;

    Side *side = ld->side[sidenum];

    // update open vertical range (extrafloors are NOT checked)
    if (side)
    {
        use_lower = HMM_MAX(use_lower, side->sector->floor_height);
        use_upper = HMM_MIN(use_upper, side->sector->ceiling_height);
    }

    if (!ld->special || ld->special->type_ == kLineTriggerShootable || ld->special->type_ == kLineTriggerWalkable)
    {
        if (ld->gap_number == 0 || use_upper <= use_lower)
        {
            // can't use through a wall
            StartSoundEffect(use_thing->info_->noway_sound_, GetSoundEffectCategory(use_thing), use_thing);
            return false;
        }

        // not a special line, but keep checking
        return true;
    }

    UseSpecialLine(use_thing, ld, sidenum, use_lower, use_upper);

    // can't use more than one special line in a row
    // -AJA- 1999/09/25: ...unless the line has the PASSTHRU flag
    //       (Boom compatibility).

    // Lobo 2022: slopes should be considered PASSTHRU by default
    //  otherwise you cant open a door if there's a slope just in front of it
    /*
    if (ld->special)
    {
        if(ld->special->slope_type & kSlopeTypeDetailFloor ||
    ld->special->slope_type & kSlopeTypeDetailCeiling) return true;
    }
    */
    return (ld->flags & kLineFlagBoomPassThrough) ? true : false;
}

//
// P_UseLines
//
// Looks for special lines in front of the player to activate.
//
void UseLines(Player *player)
{
    int   angle;
    float x1;
    float y1;
    float x2;
    float y2;

    use_thing = player->map_object_;
    use_lower = -FLT_MAX;
    use_upper = FLT_MAX;

    angle = player->map_object_->angle_;

    x1 = player->map_object_->x;
    y1 = player->map_object_->y;
    x2 = x1 + kUseRange * epi::BAMCos(angle);
    y2 = y1 + kUseRange * epi::BAMSin(angle);

    PathTraverse(x1, y1, x2, y2, kPathAddLines | kPathAddThings, PTR_UseTraverse);
}

//
// RADIUS ATTACK
//

struct RadiusAttackInfo
{
    float              range;
    MapObject         *spot;
    MapObject         *source;
    float              damage;
    const DamageClass *damage_type;
    bool               thrust;
    bool               use_3d;
};

static RadiusAttackInfo radius_attack_check;

//
// RadiusAttackCallback
//
// "bombsource" is the creature that caused the explosion at "bombspot".
//
// -ACB- 1998/07/15 New procedure that differs for RadiusAttack -
//                  it checks Height, therefore it is a sphere attack.
//
// -KM-  1998/11/25 Fixed.  Added z movement for rocket jumping.
//
static bool RadiusAttackCallback(MapObject *thing, void *data)
{
    float dx, dy, dz;
    float dist;

    /* 2023/05/01 - Disabled this upon discovering that DEHACKED explosions
       weren't damaging themeselves in DBP58. I could not find another source
       port at all where the bomb spot mobj itself would be immune to its own
       damage. We already have flags for explosion immunity so this can still be
       mitigated if the situation requires it. - Dasho */

    // ignore the bomb spot itself
    // if (thing == radius_attack_check.spot)
    // return true;

    if (!(thing->flags_ & kMapObjectFlagShootable))
        return true;

    if ((thing->hyper_flags_ & kHyperFlagFriendlyFireImmune) && radius_attack_check.source &&
        (thing->side_ & radius_attack_check.source->side_) != 0)
    {
        return true;
    }

    // MBF21: If in same splash group, don't damage it
    if (thing->info_->splash_group_ >= 0 && radius_attack_check.source->info_->splash_group_ >= 0 &&
        (thing->info_->splash_group_ == radius_attack_check.source->info_->splash_group_))
    {
        return true;
    }

    //
    // Boss types take no damage from concussion.
    // -ACB- 1998/06/14 Changed enum reference to extended flag check.
    //
    if (thing->info_->extended_flags_ & kExtendedFlagExplodeImmune)
    {
        if (!radius_attack_check.source)
            return true;
        // MBF21 FORCERADIUSDMG flag
        if (!(radius_attack_check.source->mbf21_flags_ & kMBF21FlagForceRadiusDamage))
            return true;
    }

    // -KM- 1999/01/31 Use thing->height_/2
    dx = (float)fabs(thing->x - radius_attack_check.spot->x);
    dy = (float)fabs(thing->y - radius_attack_check.spot->y);
    dz = (float)fabs(MapObjectMidZ(thing) - MapObjectMidZ(radius_attack_check.spot));

    // dist is the distance to the *edge* of the thing
    dist = HMM_MAX(dx, dy) - thing->radius_;

    if (radius_attack_check.use_3d)
        dist = HMM_MAX(dist, dz - thing->height_ / 2);

    if (dist < 0)
        dist = 0;

    if (dist >= radius_attack_check.range)
        return true; // out of range

    // recompute dist to be in range 0.0 (far away) to 1.0 (close)
    EPI_ASSERT(radius_attack_check.range > 0);
    dist = (radius_attack_check.range - dist) / radius_attack_check.range;

    if (CheckSight(radius_attack_check.spot, thing))
    {
        if (radius_attack_check.thrust)
            ThrustMapObject(thing, radius_attack_check.spot, radius_attack_check.damage * dist);
        else
            DamageMapObject(thing, radius_attack_check.spot, radius_attack_check.source,
                            radius_attack_check.damage * dist, radius_attack_check.damage_type);
    }
    return true;
}

//
// RadiusAttack
//
// Source is the creature that caused the explosion at spot.
//
// Note: Damtype can be nullptr.
//
void RadiusAttack(MapObject *spot, MapObject *source, float radius, float damage, const DamageClass *damtype,
                  bool thrust_only)
{
    radius_attack_check.range       = radius;
    radius_attack_check.spot        = spot;
    radius_attack_check.source      = source;
    radius_attack_check.damage      = damage;
    radius_attack_check.damage_type = damtype;
    radius_attack_check.thrust      = thrust_only;
    radius_attack_check.use_3d      = level_flags.true_3d_gameplay;

    //
    // -ACB- 1998/07/15 This normally does damage to everything within
    //                  a radius regards of height, however true 3D uses
    //                  a sphere attack, which checks height.
    //
    float r = radius_attack_check.range;

    BlockmapThingIterator(spot->x - r, spot->y - r, spot->x + r, spot->y + r, RadiusAttackCallback);
}

//
//  SECTOR HEIGHT CHANGING
//

static bool no_fit;
static int  crush_damage;

static bool ChangeSectorCallback(MapObject *thing, bool widening)
{
    MapObject *mo;

    if (ThingHeightClip(thing))
    {
        // keep checking
        return true;
    }

    // dropped items get removed by a falling ceiling
    if (thing->flags_ & kMapObjectFlagDropped)
    {
        RemoveMapObject(thing);
        return true;
    }

    // crunch bodies to giblets
    if (thing->health_ <= 0)
    {
        if (thing->info_->gib_state_ && !(thing->extended_flags_ & kExtendedFlagGibbed) && gore_level.d_ < 2)
        {
            thing->extended_flags_ |= kExtendedFlagGibbed;
            // P_SetMobjStateDeferred(thing, thing->info->gib_state_, 0);
            MapObjectSetState(thing, thing->info_->gib_state_);
        }

        if (thing->player_)
        {
            if (!widening)
                no_fit = true;

            return true;
        }

        // just been crushed, isn't solid.
        thing->flags_ &= ~kMapObjectFlagSolid;

        thing->height_ = 0;
        thing->radius_ = 0;

        return true;
    }

    // if thing is not shootable, can't be crushed
    if (!(thing->flags_ & kMapObjectFlagShootable) || (thing->flags_ & kMapObjectFlagNoClip))
        return true;

    // -AJA- 2003/12/02: if the space is widening, we don't care if something
    //       doesn't fit (before the move it also didn't fit !).  This is a
    //       fix for the "MAP06 ceiling not opening" bug.

    if (!widening)
        no_fit = true;

    if (crush_damage > 0 && (level_time_elapsed % 4) == 0)
    {
        DamageMapObject(thing, nullptr, nullptr, crush_damage, nullptr);

        // spray blood in a random direction
        if (gore_level.d_ < 2)
        {
            mo = CreateMapObject(thing->x, thing->y, MapObjectMidZ(thing), thing->info_->blood_);

            mo->momentum_.X = (float)(RandomByte() - 128) / 4.0f;
            mo->momentum_.Y = (float)(RandomByte() - 128) / 4.0f;
        }
    }

    // keep checking (crush other things)
    return true;
}

//
// ChangeSectorHeights
//
// Checks all things in the given sector which is changing height.
// The original space is in floor_height..ceiling_height, and the f_dh, c_dh
// parameters give the amount the floor/ceiling is moving.
//
// Things will be moved vertically if they need to.  When
// "crush_damage" is non-zero, things that no longer fit will be crushed
// (and will also set the "no_fit" variable).
//
// NOTE: the heights (floor_height, ceiling_height) currently broken.
//
static void ChangeSectorHeights(Sector *sec, float floor_height, float ceiling_height, float f_dh, float c_dh)
{
    TouchNode *tn, *next;
    MapObject *mo;

    bool widening = (f_dh <= 0) && (c_dh >= 0);

    for (tn = sec->touch_things; tn; tn = next)
    {
        // allow for thing removal
        next = tn->sector_next;

        mo = tn->map_object;
        EPI_ASSERT(mo);

        ChangeSectorCallback(mo, widening);
    }
}

//
// CheckSolidSectorMove
//
// Checks if the sector (and any attached extrafloors) can be moved.
// Only checks againgst hitting other solid floors, things are NOT
// considered here.  Returns true if OK, otherwise false.
//
bool CheckSolidSectorMove(Sector *sec, bool is_ceiling, float dh)
{
    Extrafloor *ef;

    if (AlmostEquals(dh, 0.0f))
        return true;

    //
    // first check real sector
    //

    if (is_ceiling && dh < 0 && sec->top_extrafloor && (sec->ceiling_height - dh < sec->top_extrafloor->top_height))
    {
        return false;
    }

    if (!is_ceiling && dh > 0 && sec->bottom_extrafloor &&
        (sec->floor_height + dh > sec->bottom_extrafloor->bottom_height))
    {
        return false;
    }

    // Test fix for Doom 1 E3M4 crusher bug - Dasho
    if (is_ceiling && dh < 0 && AlmostEquals(sec->ceiling_height, sec->floor_height))
    {
        if (sec->ceiling_move)
            sec->ceiling_move->destination_height = sec->floor_height - dh;
    }

    // don't allow a dummy sector to go FUBAR
    if (sec->control_floors)
    {
        if (is_ceiling && (sec->ceiling_height + dh < sec->floor_height))
            return false;

        if (!is_ceiling && (sec->floor_height + dh > sec->ceiling_height))
            return false;
    }

    //
    // second, check attached extrafloors
    //

    for (ef = sec->control_floors; ef; ef = ef->control_sector_next)
    {
        // liquids can go anywhere, anytime
        if (ef->extrafloor_definition->type_ & kExtraFloorTypeLiquid)
            continue;

        // moving a thin extrafloor ?
        if (!is_ceiling && !(ef->extrafloor_definition->type_ & kExtraFloorTypeThick))
        {
            float new_h = ef->top_height + dh;

            if (dh > 0 && new_h > (ef->higher ? ef->higher->bottom_height : ef->sector->ceiling_height))
            {
                return false;
            }

            if (dh < 0 && new_h < (ef->lower ? ef->lower->top_height : ef->sector->floor_height))
            {
                return false;
            }
            continue;
        }

        // moving the top of a thick extrafloor ?
        if (is_ceiling && (ef->extrafloor_definition->type_ & kExtraFloorTypeThick))
        {
            float new_h = ef->top_height + dh;

            if (dh < 0 && new_h < ef->bottom_height)
                return false;

            if (dh > 0 && new_h > (ef->higher ? ef->higher->bottom_height : ef->sector->ceiling_height))
            {
                return false;
            }
            continue;
        }

        // moving the bottom of a thick extrafloor ?
        if (!is_ceiling && (ef->extrafloor_definition->type_ & kExtraFloorTypeThick))
        {
            float new_h = ef->bottom_height + dh;

            if (dh > 0 && new_h > ef->top_height)
                return false;

            if (dh < 0 && new_h < (ef->lower ? ef->lower->top_height : ef->sector->floor_height))
            {
                return false;
            }
            continue;
        }
    }

    return true;
}

//
// SolidSectorMove
//
// Moves the sector and any attached extrafloors.  You MUST call
// CheckSolidSectorMove() first to check if move is possible.
//
// Things are checked here, and will be moved if they overlap the
// move.  If they no longer fit and the "crush" parameter is non-zero,
// they will take damage.  Returns true if at least one thing no
// longers fits, otherwise false.
//
bool SolidSectorMove(Sector *sec, bool is_ceiling, float dh, int crush, bool nocarething)
{
    Extrafloor *ef;

    if (AlmostEquals(dh, 0.0f))
        return false;

    no_fit       = false;
    crush_damage = crush;

    //
    // first update real sector
    //

    if (is_ceiling)
        sec->ceiling_height += dh;
    else
        sec->floor_height += dh;

    RecomputeGapsAroundSector(sec);
    FloodExtraFloors(sec);

    if (!nocarething)
    {
        if (is_ceiling)
        {
            float h = sec->top_extrafloor ? sec->top_extrafloor->top_height : sec->floor_height;
            ChangeSectorHeights(sec, h, sec->ceiling_height, 0, dh);
        }
        else
        {
            float h = sec->bottom_extrafloor ? sec->bottom_extrafloor->bottom_height : sec->ceiling_height;
            ChangeSectorHeights(sec, sec->floor_height, h, dh, 0);
        }
    }

    //
    // second, update attached extrafloors
    //

    for (ef = sec->control_floors; ef; ef = ef->control_sector_next)
    {
        if (ef->extrafloor_definition->type_ & kExtraFloorTypeThick)
        {
            ef->top_height    = sec->ceiling_height;
            ef->bottom_height = sec->floor_height;
        }
        else
        {
            ef->top_height = ef->bottom_height = sec->floor_height;
        }

        RecomputeGapsAroundSector(ef->sector);
        FloodExtraFloors(ef->sector);
    }

    if (!nocarething)
    {
        for (ef = sec->control_floors; ef; ef = ef->control_sector_next)
        {
            // liquids can go anywhere, anytime
            if (ef->extrafloor_definition->type_ & kExtraFloorTypeLiquid)
                continue;

            // moving a thin extrafloor ?
            if (!is_ceiling && !(ef->extrafloor_definition->type_ & kExtraFloorTypeThick))
            {
                if (dh > 0)
                {
                    float h = ef->higher ? ef->higher->bottom_height : ef->sector->ceiling_height;
                    ChangeSectorHeights(ef->sector, ef->top_height, h, dh, 0);
                }
                else if (dh < 0)
                {
                    float h = ef->lower ? ef->lower->top_height : ef->sector->floor_height;
                    ChangeSectorHeights(ef->sector, h, ef->top_height, 0, dh);
                }
                continue;
            }

            // moving the top of a thick extrafloor ?
            if (is_ceiling && (ef->extrafloor_definition->type_ & kExtraFloorTypeThick))
            {
                float h = ef->higher ? ef->higher->bottom_height : ef->sector->ceiling_height;
                ChangeSectorHeights(ef->sector, ef->top_height, h, dh, 0);
                continue;
            }

            // moving the bottom of a thick extrafloor ?
            if (!is_ceiling && (ef->extrafloor_definition->type_ & kExtraFloorTypeThick))
            {
                float h = ef->lower ? ef->lower->top_height : ef->sector->floor_height;
                ChangeSectorHeights(ef->sector, h, ef->bottom_height, 0, dh);
                continue;
            }
        }
    }

    return no_fit;
}

//
// CorpseCheckCallback
//
// Detect a corpse that could be raised.
//
// Based upon VileCheck: checks for any corpse within thing's radius.
//
// -ACB- 1998/08/22
//
static MapObject *raiser_corpse_found;
static MapObject *raiser_try_object;
static float      raiser_try_x;
static float      raiser_try_y;

static bool CorpseCheckCallback(MapObject *thing, void *data)
{
    float maxdist;
    float oldradius;
    float oldheight;
    int   oldflags;
    bool  check;

    if (!(thing->flags_ & kMapObjectFlagCorpse))
        return true; // not a corpse

    if (thing->tics_ != -1)
        return true; // not lying still yet

    if (thing->info_->raise_state_ == 0)
        return true; // monster doesn't have a raise state

    // -KM- 1998/12/21 Monster can't be resurrected.
    if (thing->info_->extended_flags_ & kExtendedFlagCannotResurrect)
        return true;

    // -ACB- 1998/08/06 Use raiser_try_object for radius info
    maxdist = thing->info_->radius_ + raiser_try_object->radius_;

    if (fabs(thing->x - raiser_try_x) > maxdist || fabs(thing->y - raiser_try_y) > maxdist)
        return true; // not actually touching

    // -AJA- don't raise corpses blocked by extrafloors
    if (!QuickVerticalSightCheck(raiser_try_object, thing))
        return true;

    // -AJA- don't raise players unless on their side
    if (thing->player_ && (raiser_try_object->info_->side_ & thing->info_->side_) == 0)
        return true;

    oldradius = thing->radius_;
    oldheight = thing->height_;
    oldflags  = thing->flags_;

    // -ACB- 1998/08/22 Check making sure with have the correct radius & height.
    thing->radius_ = thing->info_->radius_;
    thing->height_ = thing->info_->height_;

    if (thing->info_->flags_ & kMapObjectFlagSolid) // Should it be solid?
        thing->flags_ |= kMapObjectFlagSolid;

    check = CheckAbsolutePosition(thing, thing->x, thing->y, thing->z);

    // -ACB- 1998/08/22 Restore radius & height: we are only checking.
    thing->radius_ = oldradius;
    thing->height_ = oldheight;
    thing->flags_  = oldflags;

    // got one, so stop checking
    if (!check)
        return true; // doesn't fit here

    raiser_corpse_found              = thing;
    raiser_corpse_found->momentum_.X = raiser_corpse_found->momentum_.Y = 0;
    return false;
}

//
// FindCorpseForResurrection
//
// Used to detect corpses that have a raise state and therefore can be
// raised. Arch-Viles (Raisers in-general) use this procedure to pick
// their corpse. nullptr is returned if no corpse is found, if one is
// found it is returned.
//
// -ACB- 1998/08/22
//
MapObject *FindCorpseForResurrection(MapObject *thing)
{
    if (thing->move_direction_ != kDirectionNone)
    {
        raiser_try_object = thing;

        // check for corpses to raise
        raiser_try_x = thing->x + thing->speed_ * xspeed[thing->move_direction_];
        raiser_try_y = thing->y + thing->speed_ * yspeed[thing->move_direction_];

        if (!BlockmapThingIterator(raiser_try_x - kRaiseRadius, raiser_try_y - kRaiseRadius,
                                   raiser_try_x + kRaiseRadius, raiser_try_y + kRaiseRadius, CorpseCheckCallback))
        {
            return raiser_corpse_found; // got one - return it
        }
    }

    return nullptr;
}

//
// CheckBlockingLineCallback
//
// Used for checking that any movement between one set of coordinates does not
// cross blocking lines. If the line is twosided and has no restrictions, the
// move is allowed; the next check is to check the respective bounding boxes,
// see if any contact is made and the check is made to see if the objects are on
// different sides of the line.
//
// -ACB- 1998/08/23
//
// -AJA- 1999/09/30: Updated for extra floors.
//
static bool crosser;

// Moving Object x,y cordinates
// for object one and object two.

static float mx1;
static float my1;
static float mx2;
static float my2;

// spawn object base
static float mb2;

// spawn object top
static float mt2;

static bool CheckBlockingLineCallback(Line *line, void *data)
{
    // if the result is the same, we haven't crossed the line.
    if (PointOnLineSide(mx1, my1, line) == PointOnLineSide(mx2, my2, line))
        return true;

    // -KM- 1999/01/31 Save ceilingline for bounce.
    if ((crosser && (line->flags & kLineFlagShootBlock)) ||
        (!crosser &&
         (line->flags & (kLineFlagBlocking | kLineFlagBlockMonsters)))) // How to handle kLineFlagBlockGrounded
                                                                        // and kLineFlagBlockPlayer?
    {
        block_line = line;
        return false;
    }

    if (!(line->flags & kLineFlagTwoSided) || line->gap_number == 0)
    {
        block_line = line;
        return false;
    }

    for (int i = 0; i < line->gap_number; i++)
    {
        // gap with no restriction ?
        if (line->gaps[i].floor <= mb2 && mt2 <= line->gaps[i].ceiling)
            return true;
    }

    // Vertex slope check
    Sector *slope_sec = PointInSubsector(mx2, my2)->sector;

    if (slope_sec && (slope_sec->floor_vertex_slope || slope_sec->ceiling_vertex_slope))
    {
        bool fs_good = true;
        bool cs_good = true;
        if (slope_sec->floor_vertex_slope)
        {
            if (mb2 <= LinePlaneIntersection({{mx2, my2, -40000}}, {{mx2, my2, 40000}},
                                                 slope_sec->floor_z_vertices[2], slope_sec->floor_vertex_slope_normal)
                           .Z)
                fs_good = false;
        }
        if (slope_sec->ceiling_vertex_slope)
        {
            if (mt2 >= LinePlaneIntersection({{mx2, my2, -40000}}, {{mx2, my2, 40000}},
                                                 slope_sec->ceiling_z_vertices[2],
                                                 slope_sec->ceiling_vertex_slope_normal)
                           .Z)
                cs_good = false;
        }
        if (fs_good && cs_good)
            return true;
    }

    // stop checking, objects are on different sides of a blocking line
    block_line = line;
    return false;
}

//
// MapCheckBlockingLine
//
// Checks for a blocking line between thing and the spawnthing coordinates
// given. Return true if there is a line; crossable indicates whether or not
// whether the kLineFlagBLOCKING & kLineFlagBLOCKMONSTERS should be ignored or
// not.
//
// -ACB- 1998/08/23
//
bool MapCheckBlockingLine(MapObject *thing, MapObject *spawnthing)
{
    mx1 = thing->x;
    my1 = thing->y;
    mx2 = spawnthing->x;
    my2 = spawnthing->y;
    mb2 = spawnthing->z;
    mt2 = spawnthing->z + spawnthing->height_;

    crosser = (spawnthing->extended_flags_ & kExtendedFlagCrossBlockingLines) ? true : false;

    block_line         = nullptr;
    map_object_hit_sky = false;

    if (!BlockmapLineIterator(HMM_MIN(mx1, mx2), HMM_MIN(my1, my2), HMM_MAX(mx1, mx2), HMM_MAX(my1, my2),
                              CheckBlockingLineCallback))
    {
        return true;
    }

    return false;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
