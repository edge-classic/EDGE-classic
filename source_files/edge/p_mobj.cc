//----------------------------------------------------------------------------
//  EDGE Moving Object Handling Code
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
// -MH- 1998/07/02  "shootupdown" --> "true_3d_gameplay"
//
// -ACB- 1998/07/30 Took an axe to the item respawn code: now uses a
//                  double-linked list to store to individual items;
//                  limit removed; P_MobjItemRespawn replaces P_RespawnSpecials
//                  as the procedure that handles respawning of items.
//
//                  P_NightmareRespawnOld -> P_TeleportRespawn
//                  P_NightmareRespawnNew -> P_ResurrectRespawn
//
// -ACB- 1998/07/31 Use new procedure to handle flying missiles that hammer
//                  into sky-hack walls & ceilings. Also don't explode the
//                  missile if it hits sky-hack ceiling or floor.
//
// -ACB- 1998/08/06 Implemented limitless mobjdef list, altered/removed all
//                  mobjdef[] references.
//
// -AJA- 1999/07/21: Replaced some non-critical RandomByteDeterministics with
// RandomByte.
//
// -AJA- 1999/07/30: Removed redundant code from P_SpawnMobj (it was
//                   virtually identical to P_MobjCreateObject).
//
// -AJA- 1999/09/15: Removed P_SpawnMobj itself :-).
//

#include "p_mobj.h"

#include <list>

#include "AlmostEquals.h"
#include "con_main.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "epi.h"
#include "f_interm.h"
#include "g_game.h"
#include "hu_stuff.h"
#include "i_defs_gl.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_random.h"
#include "n_network.h"
#include "p_local.h"
#include "r_gldefs.h"
#include "r_misc.h"
#include "r_shader.h"
#include "s_sound.h"

#define EDGE_DEBUG_MAP_OBJECTS 0

static constexpr float kLadderFriction = 0.5f;

static constexpr float kOofSpeed = 9.0f; // Lobo: original value 20.0f too high, almost never played oof

static constexpr uint8_t kMaxThinkLoop = 8;

static constexpr float   kMaximumMove  = 200.0f;
static constexpr float   kStepMove     = 16.0f;
static constexpr uint8_t kRespawnDelay = (kTicRate / 2);

EDGE_DEFINE_CONSOLE_VARIABLE(distance_cull_thinkers, "0", kConsoleVariableFlagArchive)

EDGE_DEFINE_CONSOLE_VARIABLE(gravity_factor, "1.0", kConsoleVariableFlagArchive)

// List of all objects in map.
MapObject *map_object_list_head;

// List of item respawn objects
RespawnQueueItem *respawn_queue_head;

std::unordered_set<const MapObjectDefinition *> seen_monsters;

bool time_stop_active = false;

static void AddItemToQueue(const MapObject *mo)
{
    // only respawn items in deathmatch or forced by level flags
    if (!(deathmatch >= 2 || level_flags.items_respawn))
        return;

    RespawnQueueItem *newbie = new RespawnQueueItem;

    newbie->spawnpoint = mo->spawnpoint_;
    newbie->time       = mo->info_->respawntime_;

    // add to end of list

    if (respawn_queue_head == nullptr)
    {
        newbie->next     = nullptr;
        newbie->previous = nullptr;

        respawn_queue_head = newbie;
    }
    else
    {
        RespawnQueueItem *tail = respawn_queue_head;

        while (tail->next != nullptr)
            tail = tail->next;

        newbie->next     = nullptr;
        newbie->previous = tail;

        tail->next = newbie;
    }
}

bool MapObject::IsRemoved() const
{
    return state_ == nullptr;
}

bool MapObject::IsSpawning()
{
    if (!info_ || !info_->spawn_state_)
    {
        return false;
    }

    return state_ == &states[info_->spawn_state_];
}

void MapObject::AddMomentum(float xm, float ym, float zm)
{
    momentum_.X += xm;
    momentum_.Y += ym;
    momentum_.Z += zm;

    if (IsSpawning())
    {
        old_x_ = old_y_ = old_z_ = kInvalidPosition;
    }
}

#if 1 // DEBUGGING
void P_DumpMobjs(void)
{
    MapObject *mo;

    int index = 0;

    LogDebug("MOBJs:\n");

    for (mo = map_object_list_head; mo; mo = mo->next_, index++)
    {
        LogDebug(" %4d: %p next:%p prev:%p [%s] at (%1.0f,%1.0f,%1.0f) states=%d > "
                 "%d tics=%d\n",
                 index, mo, mo->next_, mo->previous_, mo->info_->name_.c_str(), mo->x, mo->y, mo->z,
                 (int)(mo->state_ ? mo->state_ - states : -1), (int)(mo->next_state_ ? mo->next_state_ - states : -1),
                 mo->tics_);
    }

    LogDebug("END OF MOBJs\n");
}
#endif

// convenience function
// -AJA- FIXME: duplicate code from p_map.c
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
// EnterBounceStates
//
// -AJA- 1999/10/18: written.
//
static void EnterBounceStates(MapObject *mo)
{
    if (!mo->info_->bounce_state_)
        return;

    // ignore if disarmed
    if (mo->extended_flags_ & kExtendedFlagJustBounced)
        return;

    // give deferred states a higher priority
    if (!mo->state_ || !mo->next_state_ || (mo->next_state_ - states) != mo->state_->nextstate)
    {
        return;
    }

    mo->extended_flags_ |= kExtendedFlagJustBounced;

    MapObjectSetState(mo, mo->info_->bounce_state_);
}

//
// BounceOffWall
//
// -AJA- 1999/08/22: written.
//
static void BounceOffWall(MapObject *mo, Line *wall)
{
    BAMAngle angle;
    BAMAngle wall_angle;
    BAMAngle diff;

    DividingLine div;
    float        dest_x, dest_y;

    angle      = PointToAngle(0, 0, mo->momentum_.X, mo->momentum_.Y);
    wall_angle = PointToAngle(0, 0, wall->delta_x, wall->delta_y);

    diff = wall_angle - angle;

    if (diff > kBAMAngle90 && diff < kBAMAngle270)
        diff -= kBAMAngle180;

    // -AJA- Prevent getting stuck at some walls...

    dest_x = mo->x + epi::BAMCos(angle) * (mo->speed_ + mo->info_->radius_) * 4.0f;
    dest_y = mo->y + epi::BAMSin(angle) * (mo->speed_ + mo->info_->radius_) * 4.0f;

    div.x       = wall->vertex_1->X;
    div.y       = wall->vertex_1->Y;
    div.delta_x = wall->delta_x;
    div.delta_y = wall->delta_y;

    if (PointOnDividingLineSide(mo->x, mo->y, &div) == PointOnDividingLineSide(dest_x, dest_y, &div))
    {
        // Result is the same, thus we haven't crossed the line.  Choose a
        // random angle to bounce away.  And don't attenuate the speed (so
        // we can get far enough away).

        angle = RandomByteDeterministic() << (kBAMAngleBits - 8);
    }
    else
    {
        angle += diff << 1;
    }

    // calculate new momentum

    mo->speed_ *= mo->info_->bounce_speed_;

    mo->momentum_.X = epi::BAMCos(angle) * mo->speed_;
    mo->momentum_.Y = epi::BAMSin(angle) * mo->speed_;
    mo->angle_      = angle;

    EnterBounceStates(mo);
}

//
// BounceOffPlane
//
// -AJA- 1999/10/18: written.
//
static void BounceOffPlane(MapObject *mo, float dir)
{
    // calculate new momentum

    mo->speed_ *= mo->info_->bounce_speed_;

    mo->momentum_.X = (float)(epi::BAMCos(mo->angle_) * mo->speed_);
    mo->momentum_.Y = (float)(epi::BAMSin(mo->angle_) * mo->speed_);
    mo->momentum_.Z = (float)(dir * mo->speed_ * mo->info_->bounce_up_);

    EnterBounceStates(mo);
}

//
// CorpseShouldSlide
//
// -AJA- 1999/09/25: written.
//
static bool CorpseShouldSlide(MapObject *mo)
{
    float floor, ceil;

    if (-0.25f < mo->momentum_.X && mo->momentum_.X < 0.25f && -0.25f < mo->momentum_.Y && mo->momentum_.Y < 0.25f)
    {
        return false;
    }

    float floor_slope_z   = 0;
    float ceiling_slope_z = 0;

    // Vertex slope check here?
    if (mo->subsector_->sector->floor_vertex_slope)
    {
        HMM_Vec3 line_a{{mo->x, mo->y, -40000}};
        HMM_Vec3 line_b{{mo->x, mo->y, 40000}};
        float    z_test = LinePlaneIntersection(line_a, line_b, mo->subsector_->sector->floor_z_vertices[2],
                                                mo->subsector_->sector->floor_vertex_slope_normal)
                           .Z;
        if (isfinite(z_test))
            floor_slope_z = z_test - mo->subsector_->sector->floor_height;
    }

    if (mo->subsector_->sector->ceiling_vertex_slope)
    {
        HMM_Vec3 line_a{{mo->x, mo->y, -40000}};
        HMM_Vec3 line_b{{mo->x, mo->y, 40000}};
        float    z_test = LinePlaneIntersection(line_a, line_b, mo->subsector_->sector->ceiling_z_vertices[2],
                                                mo->subsector_->sector->ceiling_vertex_slope_normal)
                           .Z;
        if (isfinite(z_test))
            ceiling_slope_z = mo->subsector_->sector->ceiling_height - z_test;
    }

    ComputeThingGap(mo, mo->subsector_->sector, mo->z, &floor, &ceil, floor_slope_z, ceiling_slope_z);

    return (!AlmostEquals(mo->floor_z_, floor));
}

//
// TeleportRespawn
//
static void TeleportRespawn(MapObject *mobj)
{
    float                      x, y, z, oldradius, oldheight;
    const MapObjectDefinition *info = mobj->spawnpoint_.info;
    MapObject                 *new_mo;
    int                        oldflags;

    if (!info)
        return;

    x = mobj->spawnpoint_.x;
    y = mobj->spawnpoint_.y;
    z = mobj->spawnpoint_.z;

    // something is occupying it's position?

    //
    // -ACB- 2004/02/01 Check if the object can respawn in this position with
    // its correct radius. Should this check fail restore the old values back
    //
    oldradius = mobj->radius_;
    oldheight = mobj->height_;
    oldflags  = mobj->flags_;

    mobj->radius_ = mobj->spawnpoint_.info->radius_;
    mobj->height_ = mobj->spawnpoint_.info->height_;

    if (info->flags_ & kMapObjectFlagSolid) // Should it be solid?
        mobj->flags_ |= kMapObjectFlagSolid;

    if (!CheckAbsolutePosition(mobj, x, y, z))
    {
        mobj->radius_ = oldradius;
        mobj->height_ = oldheight;
        mobj->flags_  = oldflags;
        return;
    }

    // spawn a teleport fog at old spot
    // because of removal of the body...

    // temp fix for teleport flash...
    if (info->respawneffect_)
        CreateMapObject(mobj->x, mobj->y, mobj->z, info->respawneffect_);

    // spawn a teleport fog at the new spot...

    // temp fix for teleport flash...
    if (info->respawneffect_)
        CreateMapObject(x, y, z, info->respawneffect_);

    // spawn it, inheriting attributes from deceased one
    // -ACB- 1998/08/06 Create Object
    new_mo = CreateMapObject(x, y, z, info);

    new_mo->spawnpoint_     = mobj->spawnpoint_;
    new_mo->angle_          = mobj->spawnpoint_.angle;
    new_mo->vertical_angle_ = mobj->spawnpoint_.vertical_angle;
    new_mo->tag_            = mobj->spawnpoint_.tag;

    if (mobj->spawnpoint_.flags & kMapObjectFlagAmbush)
        new_mo->flags_ |= kMapObjectFlagAmbush;

    new_mo->reaction_time_ = kRespawnDelay;

    // remove the old monster.
    RemoveMapObject(mobj);
}

//
// ResurrectRespawn
//
// -ACB- 1998/07/29 Prevented respawning of ghosts
//                  Make monster deaf, if originally deaf
//                  Given a reaction time, delays monster starting up
//                  immediately. Doesn't try to raise an object with no
//                  raisestate
//
static void ResurrectRespawn(MapObject *mobj)
{
    float                      x, y, z, oldradius, oldheight;
    const MapObjectDefinition *info;
    int                        oldflags;

    x = mobj->x;
    y = mobj->y;
    z = mobj->z;

    info = mobj->info_;

    // cannot raise the unraisable
    if (!info->raise_state_)
        return;

    // don't respawn gibs
    if (mobj->extended_flags_ & kExtendedFlagGibbed)
        return;

    //
    // -ACB- 2004/02/01 Check if the object can respawn in this position with
    // its correct radius. Should this check fail restore the old values back
    //
    oldradius = mobj->radius_;
    oldheight = mobj->height_;
    oldflags  = mobj->flags_;

    mobj->radius_ = info->radius_;
    mobj->height_ = info->height_;

    if (info->flags_ & kMapObjectFlagSolid) // Should it be solid?
        mobj->flags_ |= kMapObjectFlagSolid;

    if (!CheckAbsolutePosition(mobj, x, y, z))
    {
        mobj->radius_ = oldradius;
        mobj->height_ = oldheight;
        mobj->flags_  = oldflags;
        return;
    }

    // Resurrect monster
    if (info->overkill_sound_)
        StartSoundEffect(info->overkill_sound_, GetSoundEffectCategory(mobj), mobj);

    MapObjectSetState(mobj, info->raise_state_);

    EPI_ASSERT(!mobj->IsRemoved());

    mobj->flags_          = info->flags_;
    mobj->extended_flags_ = info->extended_flags_;
    mobj->hyper_flags_    = info->hyper_flags_;
    mobj->mbf21_flags_    = info->mbf21_flags_;
    mobj->health_         = mobj->spawn_health_;

    mobj->visibility_ = info->translucency_;
    if (!AlmostEquals(mobj->alpha_, 1.0f))
        mobj->target_visibility_ = mobj->alpha_;
    mobj->move_count_ = 0; // -ACB- 1998/08/03 Don't head off in any direction

    mobj->pain_chance_ = info->pain_chance_;

    mobj->SetSource(nullptr);
    mobj->SetTarget(nullptr);

    mobj->tag_ = mobj->spawnpoint_.tag;

    if (mobj->spawnpoint_.flags & kMapObjectFlagAmbush)
        mobj->flags_ |= kMapObjectFlagAmbush;

    mobj->reaction_time_ = kRespawnDelay;
    return;
}

//
// P_SetMobjState
//
// Returns true if the mobj is still present.
//
bool MapObjectSetState(MapObject *mobj, int state)
{
    // ignore removed objects
    if (mobj->IsRemoved())
        return false;

    if (state == 0)
    {
        RemoveMapObject(mobj);
        return false;
    }

    State *st = &states[state];

    // model interpolation stuff
    if ((st->flags & kStateFrameFlagModel) && (mobj->state_->flags & kStateFrameFlagModel) &&
        (st->sprite == mobj->state_->sprite) && st->tics > 1)
    {
        mobj->model_last_frame_ = mobj->state_->frame;
    }
    else
        mobj->model_last_frame_ = -1;

    mobj->state_      = st;
    mobj->tics_       = st->tics;
    mobj->next_state_ = (st->nextstate == 0) ? nullptr : (states + st->nextstate);

    if (st->action)
        (*st->action)(mobj);

    return true;
}

bool P_SetMobjState2(MapObject *mobj, int state)
{
    // -AJA- 2010/07/10: mundo hack for DDF inheritance. When jumping
    //                   to an old state, check if a newer one exists.

    if (mobj->IsRemoved())
        return false;

    if (state == 0)
        return MapObjectSetState(mobj, state);

    EPI_ASSERT(!mobj->info_->state_grp_.empty());

    // state is old?
    if (state < mobj->info_->state_grp_.back().first)
    {
        State *st = &states[state];

        if (st->label)
        {
            int new_state = MapObjectFindLabel(mobj, st->label);

            if (new_state != 0)
                state = new_state;
        }
    }

    return MapObjectSetState(mobj, state);
}

//
// P_SetMobjStateDeferred
//
// Similiar to P_SetMobjState, but no actions are performed yet.
// The new state will entered when the P_MobjThinker code reaches it,
// which may happen in the current tick, or at worst the next tick.
//
// Prevents re-entrancy into code like CheckRelativePosition which is
// inherently non re-entrant.
//
// -AJA- 1999/09/12: written.
//
bool MapObjectSetStateDeferred(MapObject *mo, int stnum, int tic_skip)
{
    // ignore removed objects
    if (mo->IsRemoved() || !mo->next_state_)
        return false;

    ///???	if (stnum == 0)
    ///???	{
    ///???		P_RemoveMobj(mo);
    ///???		return false;
    ///???	}

    mo->next_state_ = (stnum == 0) ? nullptr : (states + stnum);

    mo->tics_     = 0;
    mo->tic_skip_ = tic_skip;

    return true;
}

//
// P_MobjFindLabel
//
// Look for the given label in the mobj's states.  Returns the state
// number if found, otherwise 0.
//
int MapObjectFindLabel(MapObject *mobj, const char *label)
{
    return DDFStateFindLabel(mobj->info_->state_grp_, label, true /* quiet */);
}

//
// P_SetMobjDirAndSpeed
//
void MapObjectSetDirectionAndSpeed(MapObject *mo, BAMAngle angle, float slope, float speed)
{
    mo->angle_          = angle;
    mo->vertical_angle_ = epi::BAMFromATan(slope);

    mo->momentum_.Z = epi::BAMSin(mo->vertical_angle_) * speed;
    speed *= epi::BAMCos(mo->vertical_angle_);

    mo->momentum_.X = epi::BAMCos(angle) * speed;
    mo->momentum_.Y = epi::BAMSin(angle) * speed;
}

//
// P_MobjExplodeMissile
//
// -AJA- 1999/09/12: Now uses P_SetMobjStateDeferred, since this
//       routine can be called by TryMove/CheckRelativeThingCallback.
//
void ExplodeMissile(MapObject *mo)
{
    mo->momentum_.X = mo->momentum_.Y = mo->momentum_.Z = 0;

    mo->flags_ &= ~(kMapObjectFlagMissile | kMapObjectFlagTouchy);
    mo->extended_flags_ &= ~(kExtendedFlagBounce | kExtendedFlagUsable);

    if (mo->info_->deathsound_)
        StartSoundEffect(mo->info_->deathsound_, kCategoryObject, mo);

    // mobjdef used -ACB- 1998/08/06
    MapObjectSetStateDeferred(mo, mo->info_->death_state_, RandomByteDeterministic() & 3);
}

static inline void AddRegionProperties(const MapObject *mo, float bz, float tz, RegionProperties *new_p,
                                       float floor_height, float ceiling_height, const RegionProperties *p,
                                       bool iterate_pushers)
{
    int flags = p->special ? p->special->special_flags_ : kSectorFlagPushConstant;

    float factor = 1.0f;
    float push_mul;

    EPI_ASSERT(tz > bz);

    if (tz > ceiling_height)
        factor -= factor * (tz - ceiling_height) / (tz - bz);

    if (bz < floor_height)
        factor -= factor * (floor_height - bz) / (tz - bz);

    if (factor <= 0)
        return;

    new_p->gravity += factor * p->gravity;
    new_p->viscosity += factor * p->viscosity;
    new_p->drag += factor * p->drag;

    if (iterate_pushers)
    {
        int      countx     = 0;
        int      county     = 0;
        HMM_Vec2 cumulative = {{0, 0}};
        // handle push sectors
        for (TouchNode *tn = mo->touch_sectors_; tn; tn = tn->map_object_next)
        {
            if (tn->sector)
            {
                RegionProperties tn_props = tn->sector->properties;
                if (tn_props.push.X || tn_props.push.Y || tn_props.push.Z)
                {
                    SectorFlag tn_flags = tn_props.special ? tn_props.special->special_flags_ : kSectorFlagPushConstant;

                    if (!(tn_flags & kSectorFlagWholeRegion) && bz > tn->sector->floor_height + 1)
                        continue;

                    push_mul = 1.0f;

                    if (!(tn_flags & kSectorFlagPushConstant))
                    {
                        EPI_ASSERT(mo->info_->mass_ > 0);
                        push_mul = 100.0f / mo->info_->mass_;
                    }

                    if (tn_flags & kSectorFlagProportional)
                        push_mul *= factor;

                    if (tn_props.push.X)
                    {
                        countx++;
                        cumulative.X += push_mul * tn_props.push.X;
                    }
                    if (tn_props.push.Y)
                    {
                        county++;
                        cumulative.Y += push_mul * tn_props.push.Y;
                    }
                    new_p->push.Z += push_mul * tn_props.push.Z;
                }
            }
        }
        // Average it out a la ZDoom so we aren't getting sent to the shadow
        // realm in certain Boom maps. Don't think it is necessary for z
        // push at this time - Dasho
        if (countx)
            new_p->push.X += (cumulative.X / countx);
        if (county)
            new_p->push.Y += (cumulative.Y / county);
    }
    else
    {
        if (p->push.X || p->push.Y || p->push.Z)
        {
            if (!(flags & kSectorFlagWholeRegion) && bz > floor_height + 1)
                return;

            push_mul = 1.0f;

            if (!(flags & kSectorFlagPushConstant))
            {
                EPI_ASSERT(mo->info_->mass_ > 0);
                push_mul = 100.0f / mo->info_->mass_;
            }

            if (flags & kSectorFlagProportional)
                push_mul *= factor;

            new_p->push.X += push_mul * p->push.X;
            new_p->push.Y += push_mul * p->push.Y;
            new_p->push.Z += push_mul * p->push.Z;
        }
    }
}

//
// P_CalcFullProperties
//
// Calculates the properties (gravity etc..) acting on an object,
// especially when the object is in multiple extrafloors with
// different properties.
//
// Only used for players for now (too expensive to be used by
// everything).
//
void CalculateFullRegionProperties(const MapObject *mo, RegionProperties *new_p)
{
    Sector *sector = mo->subsector_->sector;

    Extrafloor *S, *L, *C;
    float       floor_h;

    float bz = mo->z;
    float tz = bz + mo->height_;

    new_p->gravity   = 0;
    new_p->viscosity = 0;
    new_p->drag      = 0;

    new_p->push.X = new_p->push.Y = new_p->push.Z = 0;

    new_p->type    = 0; // these shouldn't be used
    new_p->special = nullptr;

    // Note: friction not averaged: comes from region foot is in
    new_p->friction = sector->active_properties->friction;

    floor_h = sector->floor_height;

    if (sector->floor_vertex_slope)
        floor_h = mo->floor_z_;

    S = sector->bottom_extrafloor;
    L = sector->bottom_liquid;

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
        if (C->bottom_height < floor_h || C->bottom_height > sector->ceiling_height)
            continue;

        if (bz < C->bottom_height)
            new_p->friction = C->properties->friction;

        AddRegionProperties(mo, bz, tz, new_p, floor_h, C->top_height, C->properties, false);

        floor_h = C->top_height;
    }

    AddRegionProperties(mo, bz, tz, new_p, floor_h, sector->ceiling_height, sector->active_properties, true);
}

//
// P_XYMovement
//
static void P_XYMovement(MapObject *mo, const RegionProperties *props)
{
    float orig_x = mo->x;
    float orig_y = mo->y;

    float ptryx;
    float ptryy;
    float xstep;
    float ystep;
    float absx, absy;
    float maxstep;

    if (fabs(mo->momentum_.X) > kMaximumMove)
    {
        float factor = kMaximumMove / fabs(mo->momentum_.X);
        mo->momentum_.X *= factor;
        mo->momentum_.Y *= factor;
    }

    if (fabs(mo->momentum_.Y) > kMaximumMove)
    {
        float factor = kMaximumMove / fabs(mo->momentum_.Y);
        mo->momentum_.X *= factor;
        mo->momentum_.Y *= factor;
    }

    float xmove = mo->momentum_.X;
    float ymove = mo->momentum_.Y;

    // -AJA- 1999/07/31: Ride that rawhide :->
    if (mo->above_object_ && !(mo->above_object_->flags_ & kMapObjectFlagFloat) &&
        mo->above_object_->floor_z_ < (mo->z + mo->height_ + 1))
    {
        mo->above_object_->momentum_.X += xmove * mo->info_->ride_friction_;
        mo->above_object_->momentum_.Y += ymove * mo->info_->ride_friction_;
    }

    // -AJA- 1999/10/09: Reworked viscosity.
    xmove *= 1.0f - props->viscosity;
    ymove *= 1.0f - props->viscosity;

    // -ES- 1999/10/16 For fast mobjs, break down
    //  the move into steps of max half radius for collision purposes.

    // Use half radius as max step, if not exceptionally small.
    if (mo->radius_ > kStepMove)
        maxstep = mo->radius_ / 2;
    else
        maxstep = kStepMove / 2;

    // precalculate these two, they are used frequently
    absx = (float)fabs(xmove);
    absy = (float)fabs(ymove);

    if (absx > maxstep || absy > maxstep)
    {
        // Do it in the most number of steps.
        if (absx > absy)
        {
            xstep = (xmove > 0) ? maxstep : -maxstep;

            // almost orthogonal movements are rounded to orthogonal, to prevent
            // an infinite loop in some extreme cases.
            if (absy * 256 < absx)
                ystep = ymove = 0;
            else
                ystep = ymove * xstep / xmove;
        }
        else
        {
            ystep = (ymove > 0) ? maxstep : -maxstep;

            if (absx * 256 < absy)
                xstep = xmove = 0;
            else
                xstep = xmove * ystep / ymove;
        }
    }
    else
    {
        // Step is less than half radius, so one iteration is enough.
        xstep = xmove;
        ystep = ymove;
    }

    // Keep attempting moves until object has lost all momentum.
    do
    {
        // if movement is more than half that of the maximum, attempt the move
        // in two halves or move.
        if (fabs(xmove) > fabs(xstep))
        {
            ptryx = mo->x + xstep;
            xmove -= xstep;
        }
        else
        {
            ptryx = mo->x + xmove;
            xmove = 0;
        }

        if (fabs(ymove) > fabs(ystep))
        {
            ptryy = mo->y + ystep;
            ymove -= ystep;
        }
        else
        {
            ptryy = mo->y + ymove;
            ymove = 0;
        }

        int did_move = TryMove(mo, ptryx, ptryy);

        // unable to complete desired move ?
        if (!did_move)
        {
            // check for missiles hitting shootable lines
            // NOTE: this is for solid lines.  The "pass over" case is
            // handled in TryMove().

            if ((mo->flags_ & kMapObjectFlagMissile) &&
                (!mo->current_attack_ || !(mo->current_attack_->flags_ & kAttackFlagNoTriggerLines)))
            {
                //
                // -AJA- Seems this is called to handle this situation:
                // TryMove is called, but fails because missile would hit
                // solid line.  BUT missile did pass over some special lines.
                // These special lines were not activated in TryMove since it
                // failed.  Ugh !
                //
                if (special_lines_hit.size() > 0)
                {
                    for (std::vector<Line *>::reverse_iterator iter     = special_lines_hit.rbegin(),
                                                               iter_end = special_lines_hit.rend();
                         iter != iter_end; iter++)
                    {
                        Line *ld = *iter;

                        ShootSpecialLine(ld, PointOnLineSide(mo->x, mo->y, ld), mo->source_);
                    }
                }

                if (block_line && block_line->special)
                {
                    // ShootSpecialLine()->P_ActivateSpecialLine() can remove
                    //  the special so we need to get the info before calling it
                    const LineType            *tempspecial = block_line->special;
                    const MapObjectDefinition *DebrisThing;

                    ShootSpecialLine(block_line, PointOnLineSide(mo->x, mo->y, block_line), mo->source_);

                    if (tempspecial->type_ == kLineTriggerShootable)
                    {
                        UnblockLineEffectDebris(block_line, tempspecial);
                        if (tempspecial->effectobject_)
                        {
                            DebrisThing = tempspecial->effectobject_;
                            SpawnDebris(mo->x, mo->y, mo->z, mo->angle_ + kBAMAngle180, DebrisThing);
                        }
                    }
                }
            }

            // -AJA- 2008/01/20: Jumping out of Water
            if (block_line && block_line->back_sector && mo->player_ && mo->player_->map_object_ == mo &&
                mo->player_->wet_feet_ && !mo->player_->swimming_ && mo->player_->jump_wait_ == 0 &&
                mo->z > mo->floor_z_ + 0.5f && mo->momentum_.Z >= 0.0f)
            {
                float ground_h;

                int i = FindThingGap(block_line->gaps, block_line->gap_number, mo->z + mo->height_,
                                     mo->z + 2 * mo->height_);
                if (i >= 0)
                {
                    ground_h = block_line->gaps[i].floor;
                }
                else
                {
                    ground_h = HMM_MAX(block_line->front_sector->floor_height, block_line->back_sector->floor_height);
                }

                // LogDebug("ground_h: %1.0f  mo_Z: %1.0f\n", ground_h, mo->z);

                if (mo->z < ground_h - 20.5f && mo->z > ground_h - mo->height_ * 1.4)
                {
                    PlayerJump(mo->player_, mo->info_->jumpheight_, 2 * kTicRate);
                }
            }

            if (mo->info_->flags_ & kMapObjectFlagSlide)
            {
                SlideMove(mo, ptryx, ptryy);
            }
            else if (mo->extended_flags_ & kExtendedFlagBounce)
            {
                // -KM- 1999/01/31 Bouncy objects (grenades)
                // -AJA- 1999/07/30: Moved up here.

                if (!block_line)
                {
                    if (map_object_hit_sky)
                        RemoveMissile(mo);
                    else
                        ExplodeMissile(mo);

                    return;
                }

                BounceOffWall(mo, block_line);
                xmove = ymove = 0;
            }
            else if (mo->flags_ & kMapObjectFlagMissile)
            {
                if (map_object_hit_sky)
                    RemoveMissile(mo); // New Procedure -ACB- 1998/07/30
                else
                    ExplodeMissile(mo);

                return;
            }
            else
            {
                xmove = ymove   = 0;
                mo->momentum_.X = mo->momentum_.Y = 0;
            }
        }
    } while (xmove || ymove);

    if ((mo->extended_flags_ & kExtendedFlagNoFriction) || (mo->flags_ & kMapObjectFlagSkullFly))
        return;

    if (mo->flags_ & kMapObjectFlagCorpse)
    {
        // do not stop sliding if halfway off a step with some momentum
        if (CorpseShouldSlide(mo))
            return;
    }

    //
    // -MH- 1998/08/18 - make mid-air movement normal when using the jetpack
    //      When in mid-air there's no friction so you slide about
    //      uncontrollably. This is realistic but makes the game
    //      difficult to control to the extent that for normal people,
    //      it's not worth playing - a bit like having auto-aim
    //      permanently off (as most real people are not crack-shots!)
    //
    float friction = props->friction;

    if ((mo->z > mo->floor_z_) && !(mo->on_ladder_ >= 0) &&
        !(mo->player_ && mo->player_->powers_[kPowerTypeJetpack] > 0) && !mo->on_slope_)
    {
        // apply drag when airborne
        friction = props->drag;
    }

    // when we are confident that a mikoportal is being used, do not apply
    // friction or drag to the voodoo doll
    if (!mo->is_voodoo_ || !AlmostEquals(mo->floor_z_, -32768.0f) || AlmostEquals(mo->momentum_.Z, 0.0f))
    {
        mo->momentum_.X *= friction;
        mo->momentum_.Y *= friction;
    }

    if (mo->player_)
    {
        float x_diff = fabs(orig_x - mo->x);
        float y_diff = fabs(orig_y - mo->y);

        float speed = FastApproximateDistance(x_diff, y_diff);

        mo->player_->actual_speed_ = (mo->player_->actual_speed_ * 0.8 + speed * 0.2);

        // LogDebug("Actual speed = %1.4f\n", mo->player_->actual_speed_);

        if (fabs(mo->momentum_.X) < kStopSpeed && fabs(mo->momentum_.Y) < kStopSpeed &&
            mo->player_->command_.forward_move == 0 && mo->player_->command_.side_move == 0)
        {
            mo->momentum_.X = mo->momentum_.Y = 0;
        }
    }
}

//
// P_ZMovement
//
static void P_ZMovement(MapObject *mo, const RegionProperties *props)
{
    float dist;
    float delta;
    float zmove;
    float zmove_vs = 0;

    // -KM- 1998/11/25 Gravity is now not precalculated so that
    //  menu changes affect instantly.
    float gravity = props->gravity / 8.0f * (float)level_flags.menu_gravity_factor / kGravityDefault *
                    gravity_factor.f_; // New global gravity menu item

    // check for smooth step up
    if (mo->player_ && mo->player_->map_object_ == mo && mo->z < mo->floor_z_)
    {
        mo->player_->view_height_ -= (mo->floor_z_ - mo->z);
        mo->player_->view_z_ -= (mo->floor_z_ - mo->z);
        mo->player_->delta_view_height_ = (mo->player_->standard_view_height_ - mo->player_->view_height_) / 8.0f;
    }

    zmove = mo->momentum_.Z * (1.0f - props->viscosity);

    if (mo->on_slope_ && mo->z > mo->floor_z_ && std::abs(mo->z - mo->floor_z_) < 6.0f) // 1/4 of default step size
        zmove_vs = mo->floor_z_ - mo->z;

    // adjust height
    mo->z += zmove + zmove_vs;

    if (mo->flags_ & kMapObjectFlagFloat && mo->target_)
    {
        // float down towards target if too close
        if (!(mo->flags_ & kMapObjectFlagSkullFly) && !(mo->flags_ & kMapObjectFlagInFloat))
        {
            dist  = ApproximateDistance(mo->x - mo->target_->x, mo->y - mo->target_->y);
            delta = mo->target_->z + (mo->height_ / 2) - mo->z;

            if (delta < 0 && dist < -(delta * 3))
                mo->z -= mo->info_->float_speed_;
            else if (delta > 0 && dist < (delta * 3))
                mo->z += mo->info_->float_speed_;
        }
    }

    //
    //  HIT FLOOR ?
    //

    if (mo->z <= mo->floor_z_)
    {
        // Test for mikoportal
        if (mo->is_voodoo_ && AlmostEquals(mo->floor_z_, -32768.0f))
        {
            mo->z = mo->ceiling_z_ - mo->height_;
            TryMove(mo, mo->x, mo->y);
            return;
        }

        if (mo->flags_ & kMapObjectFlagSkullFly)
            mo->momentum_.Z = -mo->momentum_.Z;

        if (mo->momentum_.Z < 0)
        {
            float hurt_momz   = gravity * mo->info_->maxfall_;
            bool  fly_or_swim = mo->player_ && (mo->player_->swimming_ || mo->player_->powers_[kPowerTypeJetpack] > 0 ||
                                               mo->on_ladder_ >= 0);

            if (mo->player_ && gravity > 0 && -zmove > kOofSpeed && !fly_or_swim)
            {
                // Squat down. Decrease viewheight for a moment after hitting
                // the ground (hard), and utter appropriate sound.
                mo->player_->delta_view_height_ = zmove / 8.0f;
                if (mo->info_->maxfall_ > 0 && -mo->momentum_.Z > hurt_momz)
                {
                    if (!(mo->player_->cheats_ & kCheatingGodMode) && mo->player_->powers_[kPowerTypeInvulnerable] < 1)
                        StartSoundEffect(mo->info_->fallpain_sound_, GetSoundEffectCategory(mo), mo);
                    else
                        StartSoundEffect(mo->info_->oof_sound_, GetSoundEffectCategory(mo), mo);
                }
                else
                    StartSoundEffect(mo->info_->oof_sound_, GetSoundEffectCategory(mo), mo);

                HitLiquidFloor(mo);
            }
            // -KM- 1998/12/16 If bigger than max fall, take damage.
            if (mo->info_->maxfall_ > 0 && gravity > 0 && -mo->momentum_.Z > hurt_momz &&
                (!mo->player_ || !fly_or_swim))
            {
                DamageMapObject(mo, nullptr, nullptr, (-mo->momentum_.Z - hurt_momz), nullptr);
            }

            // -KM- 1999/01/31 Bouncy bouncy...
            if (mo->extended_flags_ & kExtendedFlagBounce)
            {
                BounceOffPlane(mo, +1.0f);

                // don't bounce forever on the floor
                if (!(mo->flags_ & kMapObjectFlagNoGravity) &&
                    fabs(mo->momentum_.Z) <
                        kStopSpeed + fabs(gravity / (mo->mbf21_flags_ & kMBF21FlagLowGravity ? 8 : 1)))
                {
                    mo->momentum_.X = mo->momentum_.Y = mo->momentum_.Z = 0;
                }
            }
            else
                mo->momentum_.Z = 0;
        }

        if (mo->z - mo->momentum_.Z > mo->floor_z_)
        { // Spawn splashes, etc.
            HitLiquidFloor(mo);
        }

        mo->z = mo->floor_z_;

        if ((mo->flags_ & kMapObjectFlagMissile) && !(mo->flags_ & kMapObjectFlagNoClip))
        {
            // -AJA- 2003/10/09: handle missiles that hit a monster on
            //       the head from a sharp downward angle (such a case
            //       is missed by CheckRelativeThingCallback).  FIXME: more
            //       kludge.

            if (mo->below_object_ &&
                (int)mo->floor_z_ == (int)(mo->below_object_->z + mo->below_object_->info_->height_) &&
                (mo->below_object_->flags_ & kMapObjectFlagShootable) && (mo->source_ != mo->below_object_))
            {
                if (MissileContact(mo, mo->below_object_) < 0 || (mo->extended_flags_ & kExtendedFlagTunnel))
                    return;
            }

            // if the floor is sky, don't explode missile -ACB- 1998/07/31
            if (EDGE_IMAGE_IS_SKY(mo->subsector_->sector->floor) &&
                mo->subsector_->sector->floor_height >= mo->floor_z_)
            {
                RemoveMissile(mo);
            }
            else
            {
                if (!(mo->extended_flags_ & kExtendedFlagBounce))
                    ExplodeMissile(mo);
            }
            return;
        }
    }
    else if (gravity > 0.0f)
    {
        // thing is above the ground, therefore apply gravity

        // -MH- 1998/08/18 - Disable gravity while player has jetpack
        //                   (nearly forgot this one:-)

        if (!(mo->flags_ & kMapObjectFlagNoGravity) && !(mo->player_ && mo->player_->powers_[kPowerTypeJetpack] > 0) &&
            !(mo->on_ladder_ >= 0))
        {
            mo->momentum_.Z -= gravity / (mo->mbf21_flags_ & kMBF21FlagLowGravity ? 8 : 1);
        }
    }

    //
    //  HIT CEILING ?
    //

    if (mo->z + mo->height_ > mo->ceiling_z_)
    {
        if (mo->flags_ & kMapObjectFlagSkullFly)
            mo->momentum_.Z = -mo->momentum_.Z; // the skull slammed into something

        // hit the ceiling
        if (mo->momentum_.Z > 0)
        {
            float hurt_momz   = gravity * mo->info_->maxfall_;
            bool  fly_or_swim = mo->player_ && (mo->player_->swimming_ || mo->player_->powers_[kPowerTypeJetpack] > 0 ||
                                               mo->on_ladder_ >= 0);

            if (mo->player_ && gravity < 0 && zmove > kOofSpeed && !fly_or_swim)
            {
                mo->player_->delta_view_height_ = zmove / 8.0f;
                StartSoundEffect(mo->info_->oof_sound_, GetSoundEffectCategory(mo), mo);
            }
            if (mo->info_->maxfall_ > 0 && gravity < 0 && mo->momentum_.Z > hurt_momz && (!mo->player_ || !fly_or_swim))
            {
                DamageMapObject(mo, nullptr, nullptr, (mo->momentum_.Z - hurt_momz), nullptr);
            }

            // -KM- 1999/01/31 More bouncing.
            if (mo->extended_flags_ & kExtendedFlagBounce)
            {
                BounceOffPlane(mo, -1.0f);

                // don't bounce forever on the ceiling
                if (!(mo->flags_ & kMapObjectFlagNoGravity) &&
                    fabs(mo->momentum_.Z) <
                        kStopSpeed + fabs(gravity / (mo->mbf21_flags_ & kMBF21FlagLowGravity ? 8 : 1)))
                {
                    mo->momentum_.X = mo->momentum_.Y = mo->momentum_.Z = 0;
                }
            }
            else
                mo->momentum_.Z = 0;
        }

        mo->z = mo->ceiling_z_ - mo->height_;

        if ((mo->flags_ & kMapObjectFlagMissile) && !(mo->flags_ & kMapObjectFlagNoClip))
        {
            if (mo->above_object_ && (int)mo->ceiling_z_ == (int)(mo->above_object_->z) &&
                (mo->above_object_->flags_ & kMapObjectFlagShootable) && (mo->source_ != mo->above_object_))
            {
                if (MissileContact(mo, mo->above_object_) < 0 || (mo->extended_flags_ & kExtendedFlagTunnel))
                    return;
            }

            // if the ceiling is sky, don't explode missile -ACB- 1998/07/31
            if (EDGE_IMAGE_IS_SKY(mo->subsector_->sector->ceiling) &&
                mo->subsector_->sector->ceiling_height <= mo->ceiling_z_)
            {
                RemoveMissile(mo);
            }
            else
            {
                if (!(mo->extended_flags_ & kExtendedFlagBounce))
                    ExplodeMissile(mo);
            }
            return;
        }
    }
    else if (gravity < 0.0f)
    {
        // thing is below ceiling, therefore apply any negative gravity

        // -MH- 1998/08/18 - Disable gravity while player has jetpack
        //                   (nearly forgot this one:-)

        if (!(mo->flags_ & kMapObjectFlagNoGravity) && !(mo->player_ && mo->player_->powers_[kPowerTypeJetpack] > 0) &&
            !(mo->on_ladder_ >= 0))
        {
            mo->momentum_.Z += -gravity / (mo->mbf21_flags_ & kMBF21FlagLowGravity ? 8 : 1);
        }
    }

    // update the object's vertical region
    TryMove(mo, mo->x, mo->y);

    // apply drag -- but not to frictionless things
    if ((mo->extended_flags_ & kExtendedFlagNoFriction) || (mo->flags_ & kMapObjectFlagSkullFly))
        return;

    // ladders have friction
    if (mo->on_ladder_ >= 0)
        mo->momentum_.Z *= kLadderFriction;
    else if (mo->player_ && mo->player_->powers_[kPowerTypeJetpack] > 0)
        mo->momentum_.Z *= props->friction;
    else
        mo->momentum_.Z *= props->drag;

    if (mo->player_)
    {
        if (fabs(mo->momentum_.Z) < kStopSpeed && mo->player_->command_.upward_move == 0)
        {
            mo->momentum_.Z = 0;
        }
    }
}

//
// P_MobjThinker
//
static void P_MobjThinker(MapObject *mobj)
{
    if (mobj->next_ == (MapObject *)-1)
        FatalError("P_MobjThinker INTERNAL ERROR: mobj has been freed");

    if (mobj->IsRemoved())
        return;

    if (!(mobj->player_ != NULL && mobj == mobj->player_->map_object_))
    {
        mobj->interpolate_ = mobj->old_x_ == kInvalidPosition ? false : true;

        // Store starting position for mobj interpolation.
        mobj->old_x_     = mobj->x;
        mobj->old_y_     = mobj->y;
        mobj->old_z_     = mobj->z;
        mobj->old_angle_ = mobj->angle_;
    }

    const RegionProperties *props;
    RegionProperties        player_props;

    mobj->old_z_       = mobj->z;
    mobj->old_floor_z_ = mobj->floor_z_;
    mobj->on_slope_    = false;

    mobj->ClearStaleReferences();

    EPI_ASSERT(mobj->state_);
    EPI_ASSERT(mobj->reference_count_ >= 0);

    mobj->visibility_      = (15 * mobj->visibility_ + mobj->target_visibility_) / 16;
    mobj->dynamic_light_.r = (15 * mobj->dynamic_light_.r + mobj->dynamic_light_.target) / 16;

    // position interpolation
    if (mobj->interpolation_number_ > 1)
    {
        mobj->interpolation_position_++;

        if (mobj->interpolation_position_ >= mobj->interpolation_number_)
        {
            mobj->interpolation_position_ = mobj->interpolation_number_ = 0;
        }
    }

    // handle SKULLFLY attacks
    if ((mobj->flags_ & kMapObjectFlagSkullFly) && AlmostEquals(mobj->momentum_.X, 0.0f) &&
        AlmostEquals(mobj->momentum_.Y, 0.0f))
    {
        // the skull slammed into something
        mobj->flags_ &= ~kMapObjectFlagSkullFly;
        mobj->momentum_.X = mobj->momentum_.Y = mobj->momentum_.Z = 0;

        MapObjectSetState(mobj, mobj->info_->idle_state_);

        if (mobj->IsRemoved())
            return;
    }

    // determine properties, & handle push sectors

    EPI_ASSERT(mobj->region_properties_);

    if (mobj->player_)
    {
        CalculateFullRegionProperties(mobj, &player_props);

        mobj->momentum_.X += player_props.push.X;
        mobj->momentum_.Y += player_props.push.Y;
        mobj->momentum_.Z += player_props.push.Z;

        props = &player_props;
    }
    else
    {
        // handle push sectors
        for (TouchNode *tn = mobj->touch_sectors_; tn; tn = tn->map_object_next)
        {
            if (tn->sector)
            {
                RegionProperties tn_props = tn->sector->properties;
                if (tn_props.push.X || tn_props.push.Y || tn_props.push.Z)
                {
                    SectorFlag flags = tn_props.special ? tn_props.special->special_flags_ : kSectorFlagPushConstant;

                    if (!((mobj->flags_ & kMapObjectFlagNoGravity) || (flags & kSectorFlagPushAll)) &&
                        (mobj->z <= mobj->floor_z_ + 1.0f || (flags & kSectorFlagWholeRegion)))
                    {
                        float push_mul = 1.0f;

                        EPI_ASSERT(mobj->info_->mass_ > 0);
                        if (!(flags & kSectorFlagPushConstant))
                            push_mul = 100.0f / mobj->info_->mass_;

                        mobj->momentum_.X += push_mul * tn_props.push.X;
                        mobj->momentum_.Y += push_mul * tn_props.push.Y;
                        mobj->momentum_.Z += push_mul * tn_props.push.Z;
                    }
                }
            }
        }

        props = mobj->region_properties_;

        // Only damage grounded monsters (not players)
        if (props->special && props->special->damage_.grounded_monsters_ && mobj->z <= mobj->floor_z_ + 1.0f)
        {
            DamageMapObject(mobj, nullptr, nullptr, 5.0, &props->special->damage_, false);
        }
    }

    if (mobj->subsector_->sector->floor_vertex_slope)
    {
        if (AlmostEquals(mobj->old_z_, mobj->old_floor_z_))
            mobj->on_slope_ = true;
    }

    if (!AlmostEquals(mobj->momentum_.X, 0.0f) || !AlmostEquals(mobj->momentum_.Y, 0.0f) || mobj->player_)
    {
        P_XYMovement(mobj, props);

        if (mobj->IsRemoved())
            return;
    }

    if ((!AlmostEquals(mobj->z, mobj->floor_z_)) || !AlmostEquals(mobj->momentum_.Z, 0.0f)) //  || mobj->ride_em)
    {
        P_ZMovement(mobj, props);

        if (mobj->IsRemoved())
            return;
    }

    if (mobj->fuse_ >= 0)
    {
        if (!--mobj->fuse_)
            ExplodeMissile(mobj);

        if (mobj->IsRemoved())
            return;
    }

    // When morphtimeout is reached, we go to "MORPH" state if we
    //  have one. If there is no MORPH state then the mobj is removed
    if (mobj->health_ > 0 && mobj->morph_timeout_ >= 0)
    {
        if (!--mobj->morph_timeout_)
            MapObjectSetState(mobj, mobj->info_->morph_state_);

        if (mobj->IsRemoved())
            return;
    }

    if (mobj->tics_ < 0)
    {
        // check for nightmare respawn
        if (!(mobj->extended_flags_ & kExtendedFlagMonster))
            return;

        // replaced respawnmonsters & newnmrespawn with respawnsetting
        // -ACB- 1998/07/30
        if (!level_flags.enemies_respawn)
            return;

        // check for NO_RESPAWN flag
        if (mobj->extended_flags_ & kExtendedFlagNoRespawn)
            return;

        mobj->move_count_++;

        //
        // Uses movecount as a timer, when movecount hits 12*kTicRate the
        // object will try to respawn. So after 12 seconds the object will
        // try to respawn.
        //
        if (mobj->move_count_ < mobj->info_->respawntime_)
            return;

        // if the first 5 bits of level_time_elapsed are on, don't respawn
        // now...ok?
        if (level_time_elapsed & 31)
            return;

        // give a limited "random" chance that respawn don't respawn now
        if (RandomByteDeterministic() > 32)
            return;

        // replaced respawnmonsters & newnmrespawn with respawnsetting
        // -ACB- 1998/07/30
        if (level_flags.enemy_respawn_mode)
            ResurrectRespawn(mobj);
        else
            TeleportRespawn(mobj);

        return;
    }

    // Cycle through states, calling action functions at transitions.
    // -AJA- 1999/09/12: reworked for deferred states.
    // -AJA- 2000/10/17: reworked again.

    for (int loop_count = 0; loop_count < kMaxThinkLoop; loop_count++)
    {
        if (level_flags.fast_monsters)
            mobj->tics_ -= (1 * mobj->info_->fast_ + mobj->tic_skip_);
        else
            mobj->tics_ -= (1 + mobj->tic_skip_);

        mobj->tic_skip_ = 0;

        if (mobj->tics_ >= 1)
            break;

        // You can cycle through multiple states in a tic.
        // NOTE: returns false if object freed itself.

        P_SetMobjState2(mobj, mobj->next_state_ ? (mobj->next_state_ - states) : 0);

        if (mobj->IsRemoved())
            return;

        if (mobj->tics_ != 0)
            break;
    }
}

//---------------------------------------------------------------------------

void MapObject::ClearStaleReferences()
{
    if (target_ && target_->IsRemoved())
        SetTarget(nullptr);
    if (source_ && source_->IsRemoved())
        SetSource(nullptr);
    if (tracer_ && tracer_->IsRemoved())
        SetTracer(nullptr);

    if (support_object_ && support_object_->IsRemoved())
        SetSupportObject(nullptr);
    if (above_object_ && above_object_->IsRemoved())
        SetAboveObject(nullptr);
    if (below_object_ && below_object_->IsRemoved())
        SetBelowObject(nullptr);
}

//
// Finally destroy the map object.
//
static void DeleteMobj(MapObject *mo)
{
    if (mo->reference_count_ != 0)
    {
        FatalError("INTERNAL ERROR: DeleteMobh with refcount %d", mo->reference_count_);
        return;
    }

#if (EDGE_DEBUG_MAP_OBJECTS > 0)
    LogDebug("tics=%05d  DELETE %p [%s]\n", level_time_elapsed, mo, mo->info_ ? mo->info_->name_.c_str() : "???");
#endif

    // Sound might still be playing, so use remove the
    // link between object and effect.

    StopSoundEffect(mo);

    if (mo->dynamic_light_.shader)
        delete mo->dynamic_light_.shader;

    mo->next_     = (MapObject *)-1;
    mo->previous_ = (MapObject *)-1;

    delete mo;
}

static inline void UpdateMobjRef(MapObject *self, MapObject *&field, MapObject *other)
{
    // prevent a reference to oneself
    if (other == self)
        other = nullptr;

    // never refer to a removed object
    if (other != nullptr && other->IsRemoved())
        other = nullptr;

    if (field != nullptr)
        field->reference_count_--;

    if (other != nullptr)
        other->reference_count_++;

    field = other;
}

void MapObject::SetTarget(MapObject *other)
{
    UpdateMobjRef(this, target_, other);
}

void MapObject::SetSource(MapObject *other)
{
    UpdateMobjRef(this, source_, other);
}

void MapObject::SetTracer(MapObject *other)
{
    UpdateMobjRef(this, tracer_, other);
}

void MapObject::SetSupportObject(MapObject *other)
{
    UpdateMobjRef(this, support_object_, other);
}

void MapObject::SetAboveObject(MapObject *other)
{
    UpdateMobjRef(this, above_object_, other);
}

void MapObject::SetBelowObject(MapObject *other)
{
    UpdateMobjRef(this, below_object_, other);
}

//
// P_MobjSetRealSource
//
// -AJA- This is for missiles that spawn other missiles -- what we
//       really want to know is who spawned the original missile
//       (the "instigator" of all the mayhem :-).
//
void MapObject::SetRealSource(MapObject *ref)
{
    while (ref && ref->source_ && (ref->flags_ & kMapObjectFlagMissile))
        ref = ref->source_;

    SetSource(ref);
}

void ClearAllStaleReferences(void)
{
    for (MapObject *mo = map_object_list_head; mo != nullptr; mo = mo->next_)
    {
        mo->ClearStaleReferences();
    }
}

static void AddMobjToList(MapObject *mo)
{
    mo->previous_ = nullptr;
    mo->next_     = map_object_list_head;

    if (mo->next_ != nullptr)
    {
        EPI_ASSERT(mo->next_->previous_ == nullptr);
        mo->next_->previous_ = mo;
    }

    map_object_list_head = mo;

    if (seen_monsters.count(mo->info_) == 0)
        seen_monsters.insert(mo->info_);

#if (EDGE_DEBUG_MAP_OBJECTS > 0)
    LogDebug("tics=%05d  ADD %p [%s]\n", level_time_elapsed, mo, mo->info_ ? mo->info_->name_.c_str() : "???");
#endif
}

static void RemoveMobjFromList(MapObject *mo)
{
#if (EDGE_DEBUG_MAP_OBJECTS > 0)
    LogDebug("tics=%05d  REMOVE %p [%s]\n", level_time_elapsed, mo, mo->info_ ? mo->info_->name_.c_str() : "???");
#endif

    if (mo->previous_ != nullptr)
    {
        EPI_ASSERT(mo->previous_->next_ == mo);
        mo->previous_->next_ = mo->next_;
    }
    else // no previous, must be first item
    {
        EPI_ASSERT(map_object_list_head == mo);
        map_object_list_head = mo->next_;
    }

    if (mo->next_ != nullptr)
    {
        EPI_ASSERT(mo->next_->previous_ == mo);
        mo->next_->previous_ = mo->previous_;
    }
}

//
// P_RemoveMobj
//
// Removes the object from the play simulation: no longer thinks, if
// the mobj is kMapObjectFlagSpecial: i.e. item can be picked up, it is added to
// the item-respawn-que, so it gets respawned if needed; The respawning
// only happens if items_respawn is set or the deathmatch mode is altdeath.
//
void RemoveMapObject(MapObject *mo)
{
    for (int pnum = 0; pnum < kMaximumPlayers; pnum++)
    {
        Player *p = players[pnum];
        if (p && p->attacker_ == mo)
            p->attacker_ = nullptr;
    }

    if (mo->IsRemoved())
    {
        LogDebug("Warning: object %p already removed.\n", mo);
        return;
    }

    if ((mo->info_->flags_ & kMapObjectFlagSpecial) && 0 == (mo->extended_flags_ & kExtendedFlagNoRespawn) &&
        0 == (mo->flags_ & (kMapObjectFlagMissile | kMapObjectFlagDropped)) && mo->spawnpoint_.info)
    {
        AddItemToQueue(mo);
    }

    // unlink from sector and block lists
    UnsetThingFinal(mo);

    // mark as REMOVED
    mo->state_      = nullptr;
    mo->next_state_ = nullptr;

    mo->flags_          = 0;
    mo->extended_flags_ = 0;
    mo->hyper_flags_    = 0;
    mo->health_         = 0;
    mo->tag_            = 0;
    mo->tics_           = -1;
    mo->wait_until_dead_tags_.clear();

    // Clear all references to other mobjs
    mo->SetTarget(nullptr);
    mo->SetSource(nullptr);
    mo->SetTracer(nullptr);

    mo->SetSupportObject(nullptr);
    mo->SetAboveObject(nullptr);
    mo->SetBelowObject(nullptr);

    // NOTE: object is kept in mobjlist until no there are no more
    //       references to it, and until 5 seconds has elapsed (giving time
    //       for deaths sounds to play out).  such "zombie" objects may be
    //       legally stored and restored from savegames.

    mo->fuse_ = kTicRate * 5;
    // mo->morphtimeout = kTicRate * 5; //maybe we need this?
}

void RemoveAllMapObjects(bool loading)
{
    while (map_object_list_head != nullptr)
    {
        MapObject *mo        = map_object_list_head;
        map_object_list_head = mo->next_;

        // Need to unlink from existing map in the case of loading a savegame
        if (loading)
        {
            UnsetThingFinal(mo);
        }

        mo->reference_count_ = 0;
        DeleteMobj(mo);
    }
}

void ClearRespawnQueue(void)
{
    while (respawn_queue_head != nullptr)
    {
        RespawnQueueItem *tmp = respawn_queue_head;
        respawn_queue_head    = respawn_queue_head->next;

        delete tmp;
    }
}

//
// RunMobjThinkers
//
// Cycle through all mobjs and let them think.
// Also handles removed objects which have no more references.
//
void RunMapObjectThinkers()
{
    MapObject *mo;
    MapObject *next;

    time_stop_active = false;

    for (int pnum = 0; pnum < kMaximumPlayers; pnum++)
    {
        if (players[pnum] && players[pnum]->powers_[kPowerTypeTimeStop] > 0)
        {
            time_stop_active = true;
            break;
        }
    }

    for (mo = map_object_list_head; mo != nullptr; mo = next)
    {
        next = mo->next_;

        if (mo->IsRemoved())
        {
            if (mo->fuse_ > 0)
            {
                mo->fuse_--;
            }
            else if (mo->reference_count_ == 0)
            {
                RemoveMobjFromList(mo);
                DeleteMobj(mo);
            }

            continue;
        }

        if (mo->player_)
            P_MobjThinker(mo);
        else
        {
            if (time_stop_active)
                continue;
            if (!distance_cull_thinkers.d_ ||
                (game_tic / 2 %
                     RoundToInteger(1 + PointToDistance(players[console_player]->map_object_->x,
                                                        players[console_player]->map_object_->y, mo->x, mo->y) /
                                            1500) ==
                 0))
                P_MobjThinker(mo);
        }
    }
}

//---------------------------------------------------------------------------
//
// GAME SPAWN FUNCTIONS
//

//
// P_SpawnDebris
//
void SpawnDebris(float x, float y, float z, BAMAngle angle, const MapObjectDefinition *debris)
{
    // if (!level_flags.have_extra && (splash->extended_flags_ &
    // kExtendedFlagExtra)) return; if (! (splash->extended_flags_ &
    // kExtendedFlagExtra)) return; //Optional extra
    MapObject *th;

    th = CreateMapObject(x, y, z, debris);
    MapObjectSetDirectionAndSpeed(th, angle, 2.0f, 0.25f);

    th->tics_ -= RandomByteDeterministic() & 3;

    if (th->tics_ < 1)
        th->tics_ = 1;
}

//
// P_SpawnPuff
//
void SpawnPuff(float x, float y, float z, const MapObjectDefinition *puff, BAMAngle angle)
{
    MapObject *th;

    z += (float)RandomByteSkewToZeroDeterministic() / 80.0f;

    // -ACB- 1998/08/06 Specials table for non-negotiables....
    th = CreateMapObject(x, y, z, puff);

    // -AJA- 1999/07/14: DDF-itised.
    th->momentum_.Z = puff->float_speed_;

    // -AJA- 2011/03/14: set the angle
    th->angle_ = angle;

    th->tics_ -= RandomByteDeterministic() & 3;

    if (th->tics_ < 1)
        th->tics_ = 1;
}

//
// P_SpawnBlood
//
// -KM- 1998/11/25 Made more violent. :-)
// -KM- 1999/01/31 Different blood objects for different mobjs.
//
void SpawnBlood(float x, float y, float z, float damage, BAMAngle angle, const MapObjectDefinition *blood)
{
    int        num;
    MapObject *th;

    angle += kBAMAngle180;

    num = (int)(!level_flags.more_blood ? 1.0f : (RandomByte() % 7) + (float)((HMM_MAX(damage / 4.0f, 7.0f))));

    while (num--)
    {
        z += (float)(RandomByteSkewToZeroDeterministic() / 64.0f);

        angle += (BAMAngle)(RandomByteSkewToZeroDeterministic() * (int)(kBAMAngle1 / 2));

        th = CreateMapObject(x, y, z, blood);

        MapObjectSetDirectionAndSpeed(th, angle, ((float)num + 12.0f) / 6.0f, (float)num / 4.0f);

        th->tics_ -= RandomByteDeterministic() & 3;

        if (th->tics_ < 1)
            th->tics_ = 1;

        if (damage <= 12 && th->state_ && th->next_state_)
            MapObjectSetState(th, th->next_state_ - states);

        if (damage <= 8 && th->state_ && th->next_state_)
            MapObjectSetState(th, th->next_state_ - states);
    }
}

//---------------------------------------------------------------------------
//
// FUNC P_IsThingOnLiquidFloor
//
//---------------------------------------------------------------------------

FlatDefinition *P_IsThingOnLiquidFloor(MapObject *thing)
{
    FlatDefinition *current_flatdef = nullptr;

    if (thing->flags_ & kMapObjectFlagFloat)
        return current_flatdef;

    // If no 3D floors, just return the flat
    if (thing->subsector_->sector->extrafloor_used == 0)
    {
        if (thing->z > thing->floor_z_) // are we actually touching the floor
            return current_flatdef;

        current_flatdef = flatdefs.Find(thing->subsector_->sector->floor.image->name_.c_str());
    }
    // Start from the lowest exfloor and check if the player is standing on it,
    // then return the control sector's flat
    else
    {
        float       player_floor_height = thing->floor_z_;
        Extrafloor *floor_checker       = thing->subsector_->sector->bottom_extrafloor;
        Extrafloor *liquid_checker      = thing->subsector_->sector->bottom_liquid;
        for (Extrafloor *ef = floor_checker; ef; ef = ef->higher)
        {
            if (AlmostEquals(player_floor_height, ef->top_height))
                current_flatdef = flatdefs.Find(ef->extrafloor_line->front_sector->floor.image->name_.c_str());
        }
        for (Extrafloor *ef = liquid_checker; ef; ef = ef->higher)
        {
            if (AlmostEquals(player_floor_height, ef->top_height))
                current_flatdef = flatdefs.Find(ef->extrafloor_line->front_sector->floor.image->name_.c_str());
        }
        // if (!current_flatdef)
        //	current_flatdef =
        // flatdefs.Find(thing->subsector_->sector->floor.image->name); //
        // Fallback if nothing else satisfies these conditions
    }

    return current_flatdef;
}

//---------------------------------------------------------------------------
//
// FUNC P_HitLiquidFloor
//
//---------------------------------------------------------------------------

bool HitLiquidFloor(MapObject *thing)
{

    // marked as not making splashes (e.g. a leaf)
    if (thing->hyper_flags_ & kHyperFlagNoSplash)
        return false;

    // don't splash if landing on the edge above water/lava/etc....
    if (thing->subsector_->sector->floor_vertex_slope && thing->z > thing->floor_z_)
        return false;
    else if (!AlmostEquals(thing->floor_z_, thing->subsector_->sector->floor_height))
        return false;

    FlatDefinition *current_flatdef = P_IsThingOnLiquidFloor(thing);

    if (current_flatdef)
    {
        if (current_flatdef->impactobject_)
        {
            BAMAngle angle = thing->angle_;
            angle += (BAMAngle)(RandomByteSkewToZeroDeterministic() * (int)(kBAMAngle1 / 2));

            SpawnDebris(thing->x, thing->y, thing->z, angle, current_flatdef->impactobject_);

            StartSoundEffect(current_flatdef->footstep_, GetSoundEffectCategory(thing), thing);
        }
        if (current_flatdef->liquid_.empty())
        {
            return false;
        }
        else
            return true;
    }
    return false;
}

//
// P_MobjItemRespawn
//
// Replacement procedure for P_RespawnSpecials, uses a linked list to go through
// the item-respawn-que. The time until respawn (in tics) is decremented every
// tic, when the item-in-the-que has a time of zero is it respawned.
//
// -ACB- 1998/07/30 Procedure written.
// -KM- 1999/01/31 Custom respawn fog.
//
void ItemRespawn(void)
{
    // only respawn items in deathmatch or forced by level flags
    if (!(deathmatch >= 2 || level_flags.items_respawn))
        return;

    float                      x, y, z;
    MapObject                 *mo;
    const MapObjectDefinition *objtype;

    RespawnQueueItem *cur, *next;

    // let's start from the beginning....
    for (cur = respawn_queue_head; cur; cur = next)
    {
        next = cur->next;

        cur->time--;

        if (cur->time > 0)
            continue;

        // no time left, so respawn object

        x = cur->spawnpoint.x;
        y = cur->spawnpoint.y;
        z = cur->spawnpoint.z;

        objtype = cur->spawnpoint.info;

        if (objtype == nullptr)
        {
            FatalError("P_MobjItemRespawn: No such item type!");
            return; // shouldn't happen.
        }

        // spawn a teleport fog at the new spot
        EPI_ASSERT(objtype->respawneffect_);
        CreateMapObject(x, y, z, objtype->respawneffect_);

        // -ACB- 1998/08/06 Use MobjCreateObject
        mo = CreateMapObject(x, y, z, objtype);

        mo->angle_          = cur->spawnpoint.angle;
        mo->vertical_angle_ = cur->spawnpoint.vertical_angle;
        mo->spawnpoint_     = cur->spawnpoint;

        // Taking this item-in-que out of the que, remove
        // any references by the previous and next items to
        // the current one.....

        if (cur->next)
            cur->next->previous = cur->previous;

        if (cur->previous)
            cur->previous->next = next;
        else
            respawn_queue_head = next;

        delete cur;
    }
}

//
// P_MobjRemoveMissile
//
// This procedure only is used when a flying missile is removed because
// it "hit" a wall or ceiling that in the simulation acts as a sky. The
// only major differences with P_RemoveMobj are that now item respawn check
// is not done (not needed) and any sound will continue playing despite
// the fact the missile has been removed: This is only done due to the
// fact that a missile in reality would continue flying through a sky and
// you should still be able to hear it.
//
// -ACB- 1998/07/31 Procedure written.
// -AJA- 1999/09/15: Functionality subsumed by DoRemoveMobj.
// -ES- 1999/10/24 Removal Queue.
//
void RemoveMissile(MapObject *missile)
{
    RemoveMapObject(missile);

    missile->momentum_.X = missile->momentum_.Y = missile->momentum_.Z = 0;

    missile->flags_ &= ~(kMapObjectFlagMissile | kMapObjectFlagTouchy);
    missile->extended_flags_ &= ~(kExtendedFlagBounce);
}

//
// P_MobjCreateObject
//
// Creates a Map Object (MOBJ) at the specified location, with the
// specified type (given by DDF).  The special z values kOnFloorZ and
// kOnCeilingZ are recognised and handled appropriately.
//
// -ACB- 1998/08/02 Procedure written.
//
MapObject *CreateMapObject(float x, float y, float z, const MapObjectDefinition *info)
{
    MapObject *mobj = new MapObject;

#if (EDGE_DEBUG_MAP_OBJECTS > 0)
    LogDebug("tics=%05d  CREATE %p [%s]  AT %1.0f,%1.0f,%1.0f\n", level_time_elapsed, mobj, info->name.c_str(), x, y,
             z);
#endif

    mobj->info_             = info;
    mobj->x                 = x;
    mobj->y                 = y;
    mobj->radius_           = info->radius_;
    mobj->height_           = info->height_;
    mobj->scale_            = info->scale_;
    mobj->aspect_           = info->aspect_;
    mobj->flags_            = info->flags_;
    mobj->health_           = info->spawn_health_;
    mobj->spawn_health_     = info->spawn_health_;
    mobj->speed_            = info->speed_;
    mobj->fuse_             = info->fuse_;
    mobj->side_             = info->side_;
    mobj->model_skin_       = info->model_skin_;
    mobj->model_last_frame_ = -1;
    mobj->model_aspect_     = info->model_aspect_;
    mobj->model_scale_      = info->model_scale_;
    mobj->wait_until_dead_tags_.clear();

    mobj->pain_chance_ = info->pain_chance_;

    mobj->morph_timeout_ = info->morphtimeout_;

    if (level_flags.fast_monsters && info->fast_speed_ > -1)
        mobj->speed_ = info->fast_speed_;

    // -ACB- 1998/06/25 new mobj Stuff (1998/07/11 - invisibility added)
    mobj->extended_flags_    = info->extended_flags_;
    mobj->hyper_flags_       = info->hyper_flags_;
    mobj->mbf21_flags_       = info->mbf21_flags_;
    mobj->target_visibility_ = mobj->visibility_ = info->translucency_;

    mobj->current_attack_ = nullptr;
    mobj->on_ladder_      = -1;

    if (game_skill != kSkillNightmare)
        mobj->reaction_time_ = info->reaction_time_;

    mobj->last_look_ = RandomByteDeterministic() % kMaximumPlayers;

    //
    // Do not set the state with P_SetMobjState,
    // because action routines can not be called yet
    //
    // if we have a spawnstate use that; else try the meanderstate
    // -ACB- 1998/09/06
    //
    // -AJA- So that the first action gets executed, the `next_state'
    //       is set to the first state and `tics' set to 0.
    //
    State *st;

    if (info->spawn_state_)
        st = &states[info->spawn_state_];
    else if (info->meander_state_)
        st = &states[info->meander_state_];
    else
        st = &states[info->idle_state_];

    mobj->state_      = st;
    mobj->tics_       = 0;
    mobj->next_state_ = st;

    EPI_ASSERT(!mobj->IsRemoved());

    // enable usable items
    if (mobj->extended_flags_ & kExtendedFlagUsable)
        mobj->flags_ |= kMapObjectFlagTouchy;

    // handle dynamic lights
    {
        const DynamicLightDefinition *dinfo = &info->dlight_;

        if (dinfo->type_ != kDynamicLightTypeNone)
        {
            mobj->dynamic_light_.r = mobj->dynamic_light_.target = dinfo->radius_;
            mobj->dynamic_light_.color                           = dinfo->colour_;

            // leave 'shader' field as nullptr : renderer will create it
        }
    }

    // set subsector and/or block links
    SetThingPosition(mobj);

    // -AJA- 1999/07/30: Updated for extra floors.

    Sector *sec = mobj->subsector_->sector;

    float floor_slope_z   = 0;
    float ceiling_slope_z = 0;

    if (sec->floor_vertex_slope)
    {
        float sz = LinePlaneIntersection({{x, y, -40000}}, {{x, y, 40000}}, sec->floor_z_vertices[2],
                                         sec->floor_vertex_slope_normal)
                       .Z;
        if (isfinite(sz))
            floor_slope_z = sz - sec->floor_height;
    }
    if (sec->ceiling_vertex_slope)
    {
        float sz = LinePlaneIntersection({{x, y, -40000}}, {{x, y, 40000}}, sec->ceiling_z_vertices[2],
                                         sec->ceiling_vertex_slope_normal)
                       .Z;
        if (isfinite(sz))
            ceiling_slope_z = sec->ceiling_height - sz;
    }

    mobj->z = ComputeThingGap(mobj, sec, z, &mobj->floor_z_, &mobj->ceiling_z_, floor_slope_z, ceiling_slope_z);

    // Find the real players height (TELEPORT WEAPONS).
    mobj->original_height_ = z;

    // update totals for countable items.  Doing it here means that
    // things spawned dynamically can be counted as well.  Whilst this
    // has its dangers, at least it is consistent (more than can be said
    // when RTS comes into play -- trying to second guess which
    // spawnthings should not be counted just doesn't work).

    if (mobj->flags_ & kMapObjectFlagCountKill)
        intermission_stats.kills++;

    if (mobj->flags_ & kMapObjectFlagCountItem)
        intermission_stats.items++;

    mobj->last_heard_ = -1; // For now, the last player we heard
    //
    // -ACB- 1998/08/27 Mobj Linked-List Addition
    //
    // A useful way of cycling through the current things without
    // having to deref everything using thinkers.
    //
    // -AJA- 1999/09/15: now adds to _head_ of list (for speed).
    //
    AddMobjToList(mobj);

    return mobj;
}

//
// P_MobjGetSfxCategory
//
// Returns the sound category for an object.
//
int GetSoundEffectCategory(const MapObject *mo)
{
    if (mo->player_)
    {
        if (mo->player_ == players[display_player])
            return kCategoryPlayer;

        return kCategoryOpponent;
    }
    else
    {
        if (mo->extended_flags_ & kExtendedFlagMonster)
            return kCategoryMonster;

        return kCategoryObject;
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
