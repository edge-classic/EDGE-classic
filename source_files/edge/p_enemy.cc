//----------------------------------------------------------------------------
//  EDGE Creature Action Code
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
// -KM- 1998/09/27 Sounds.ddf stuff
//
// -AJA- 1999/07/21: Replaced some non-critical RandomByteDeterministics with
// RandomByte,
//       and removed some X_Random()-X_Random() things.
//

#include <float.h>

#include "AlmostEquals.h"
#include "dm_state.h"
#include "epi.h"
#include "epi_doomdefs.h"
#include "g_game.h"
#include "m_random.h"
#include "p_action.h"
#include "p_local.h"
#include "r_misc.h"
#include "s_sound.h"
#include "w_wad.h"

DirectionType opposite[] = {kDirectionWest,      kDirectionSouthwest, kDirectionSouth,
                            kDirectionSoutheast, kDirectionEast,      kDirectionNorthEast,
                            kDirectionNorth,     kDirectionNorthWest, kDirectionNone};

DirectionType diagonals[] = {kDirectionNorthWest, kDirectionNorthEast, kDirectionSouthwest, kDirectionSoutheast};

// 0.7071067812f: The diagonal speed of creatures
float xspeed[8] = {1.0f, 0.7071067812f, 0.0f, -0.7071067812f, -1.0f, -0.7071067812f, 0.0f, 0.7071067812f};
float yspeed[8] = {0.0f, 0.7071067812f, 1.0f, 0.7071067812f, 0.0f, -0.7071067812f, -1.0f, -0.7071067812f};

//
//  ENEMY THINKING
//
// Enemies are allways spawned
// with targetplayer = -1, threshold = 0
// Most monsters are spawned unaware of all players,
// but some can be made preaware
//

//
// Called by NoiseAlert.
// Recursively traverse adjacent sectors,
// sound blocking lines cut off traversal.
//
static void RecurseSound(Sector *sec, int soundblocks, int player)
{
    int     i;
    Line   *check;
    Sector *other;

    // has the sound flooded this sector
    if (sec->valid_count == valid_count && sec->sound_traversed <= (soundblocks + 1))
        return;

    // wake up all monsters in this sector
    sec->valid_count     = valid_count;
    sec->sound_traversed = soundblocks + 1;
    sec->sound_player    = player;

    // Set any nearby monsters to have heard the player
    TouchNode *nd;
    for (nd = sec->touch_things; nd; nd = nd->sector_next)
    {
        if (nd->map_object != nullptr)
        {
            if (!AlmostEquals(nd->map_object->info_->hear_distance_,
                              -1.0f)) // if we have hear_distance set
            {
                float distance;
                distance = ApproximateDistance(players[player]->map_object_->x - nd->map_object->x,
                                               players[player]->map_object_->y - nd->map_object->y);
                distance = ApproximateDistance(players[player]->map_object_->z - nd->map_object->z, distance);
                if (distance < nd->map_object->info_->hear_distance_)
                {
                    nd->map_object->last_heard_ = player;
                }
            }
            else /// by default he heard
            {
                nd->map_object->last_heard_ = player;
            }
        }
    }

    for (i = 0; i < sec->line_count; i++)
    {
        check = sec->lines[i];

        if (!(check->flags & kLineFlagTwoSided))
            continue;

        // -AJA- 1999/07/19: Gaps are now stored in line_t.
        if (check->gap_number == 0)
            continue; // closed door

        // -AJA- 2001/11/11: handle closed Sliding doors
        if (check->slide_door && !check->slide_door->s_.see_through_ && !check->slider_move)
        {
            continue;
        }

        if (check->front_sector == sec)
            other = check->back_sector;
        else
            other = check->front_sector;

        if (check->flags & kLineFlagSoundBlock)
        {
            if (!soundblocks)
                RecurseSound(other, 1, player);
        }
        else
        {
            RecurseSound(other, soundblocks, player);
        }
    }
}

void NoiseAlert(Player *p)
{
    valid_count++;

    RecurseSound(p->map_object_->subsector_->sector, 0, p->player_number_);
}

// MBF21
void WA_NoiseAlert(MapObject *actor)
{
    EPI_ASSERT(actor->player_);

    valid_count++;

    RecurseSound(actor->subsector_->sector, 0, actor->player_->player_number_);
}

// Called by new NOISE_ALERT ddf action
void A_NoiseAlert(MapObject *actor)
{
    valid_count++;

    int WhatPlayer = 0;

    if (actor->last_heard_ != -1)
        WhatPlayer = actor->last_heard_;

    RecurseSound(actor->subsector_->sector, 0, WhatPlayer);
}

//
// Move in the current direction,
// returns false if the move is blocked.
//
bool DoMove(MapObject *actor, bool path)
{
    HMM_Vec3 orig_pos{{actor->x, actor->y, actor->z}};

    float tryx;
    float tryy;

    float   fric   = -1.0f;
    float   factor = -1.0f;
    Sector *sector = actor->subsector_->sector;
    float   speed  = actor->speed_;

    for (TouchNode *tn = actor->touch_sectors_; tn; tn = tn->map_object_next)
    {
        if (tn->sector)
        {
            float sec_fh =
                (tn->sector->floor_vertex_slope && sector == tn->sector) ? actor->floor_z_ : tn->sector->floor_height;
            if (!AlmostEquals(actor->z, sec_fh))
                continue;
            if (fric < 0.0f || tn->sector->properties.friction < fric)
            {
                fric   = tn->sector->properties.friction;
                factor = tn->sector->properties.movefactor;
            }
        }
    }

    // Dasho: This section deviates from Boom/MBF a bit since we can't really
    // use momentum or the delta between x/y and old_x/y here. Results look pretty
    // similar for high friction areas, but I've afforded a little more traction
    // for monsters on ice/low friction
    if (fric < 0.0f || AlmostEquals(fric, kFrictionDefault))
        fric = kFrictionDefault;
    else if (fric < kFrictionDefault)
    {
        factor *= 32;
        fric *= factor;
    }
    else
    {
        factor *= 16;
        fric *= factor;
    }

    speed *= fric;

    speed = HMM_Clamp(1.0f, speed, actor->speed_);

    if (path)
    {
        tryx = actor->x + speed * epi::BAMCos(actor->angle_);
        tryy = actor->y + speed * epi::BAMSin(actor->angle_);
    }
    else
    {
        if (actor->move_direction_ == kDirectionNone)
            return false;

        if ((unsigned)actor->move_direction_ >= 8)
        {
            // FatalError("Weird actor->move_direction_! = %d",(int)actor->move_direction_);
            actor->move_direction_ = kDirectionNone;
            return false;
        }

        tryx = actor->x + speed * xspeed[actor->move_direction_];
        tryy = actor->y + speed * yspeed[actor->move_direction_];
    }

    if (!TryMove(actor, tryx, tryy))
    {
        // open any specials
        if (actor->flags_ & kMapObjectFlagFloat && float_ok)
        {
            // must adjust height
            if (actor->z < float_destination_z)
                actor->z += actor->info_->float_speed_;
            else
                actor->z -= actor->info_->float_speed_;

            actor->flags_ |= kMapObjectFlagInFloat;
            // FIXME: position interpolation
            return true;
        }

        if (special_lines_hit.empty())
            return false;

        actor->move_direction_ = kDirectionNone;

        // -AJA- 1999/09/10: As Lee Killough points out, this is where
        //       monsters can get stuck in doortracks.  We follow Lee's
        //       method: return true 90% of the time if the blocking line
        //       was the one activated, or false 90% of the time if there
        //       was some other line activated.

        bool any_used   = false;
        bool block_used = false;

        for (std::vector<Line *>::reverse_iterator iter     = special_lines_hit.rbegin(),
                                                   iter_end = special_lines_hit.rend();
             iter != iter_end; iter++)
        {
            Line *ld = *iter;
            if (UseSpecialLine(actor, ld, 0, -FLT_MAX, FLT_MAX))
            {
                any_used = true;

                if (ld == block_line)
                    block_used = true;
            }
        }

        return any_used && (RandomByteDeterministic() < 230 ? block_used : !block_used);
    }

    actor->flags_ &= ~kMapObjectFlagInFloat;

    if (!(actor->flags_ & kMapObjectFlagFloat) && !(actor->extended_flags_ & kExtendedFlagGravityFall))
    {
        if (actor->z > actor->floor_z_)
        {
            actor->z = actor->floor_z_;
            HitLiquidFloor(actor);
        }
        else
            actor->z = actor->floor_z_;
    }

    // -AJA- 2008/01/16: position interpolation
    if ((actor->state_->flags & kStateFrameFlagModel) || (actor->flags_ & kMapObjectFlagFloat))
    {
        actor->interpolation_number_   = HMM_MAX(1, actor->state_->tics);
        actor->interpolation_position_ = 1;
        if (actor->old_x_ != kInvalidPosition)
            actor->interpolation_from_ = orig_pos;
        else
            actor->interpolation_from_ = {{actor->x, actor->y, actor->z}};
    }

    return true;
}

//
// Attempts to move actor on
// in its current (ob->moveangle) direction.
// If blocked by either a wall or an actor
// returns FALSE
// If move is either clear or blocked only by a door,
// returns TRUE and sets...
// If a door is in the way,
// an OpenDoor call is made to start it opening.
//
static bool TryWalk(MapObject *actor)
{
    if (!DoMove(actor, false))
        return false;

    actor->move_count_ = RandomByteDeterministic() & 15;

    return true;
}

// -ACB- 1998/09/06 actor is now an object; different movement choices.
void NewChaseDir(MapObject *object)
{
    float         deltax = 0;
    float         deltay = 0;
    DirectionType tdir;

    DirectionType d[3];
    DirectionType olddir;
    DirectionType turnaround;

    olddir     = object->move_direction_;
    turnaround = opposite[olddir];

    //
    // Movement choice: Previously this was calculation to find
    // the distance between object and target: if the object had
    // no target, a fatal error was returned. However it is now
    // possible to have movement without a target. if the object
    // has a target, go for that; else if it has a supporting
    // object aim to go within supporting distance of that; the
    // remaining option is to walk aimlessly: the target destination
    // is always 128 in the old movement direction, think
    // of it like the donkey and the carrot sketch: the donkey will
    // move towards the carrot, but since the carrot is always a
    // set distance away from the donkey, the rather stupid mammal
    // will spend eternity trying to get the carrot and will walk
    // forever.
    //
    // -ACB- 1998/09/06

    if (object->target_)
    {
        deltax = object->target_->x - object->x;
        deltay = object->target_->y - object->y;
    }
    else if (object->support_object_)
    {
        // not too close
        deltax = (object->support_object_->x - object->x) - (object->support_object_->radius_ * 4);
        deltay = (object->support_object_->y - object->y) - (object->support_object_->radius_ * 4);
    }
    else if (olddir < kDirectionNone) // use old direction only if not turning/evading/immobile
    {
        deltax = 128 * xspeed[olddir];
        deltay = 128 * yspeed[olddir];
    }

    if (deltax > 10)
        d[1] = kDirectionEast;
    else if (deltax < -10)
        d[1] = kDirectionWest;
    else
        d[1] = kDirectionNone;

    if (deltay < -10)
        d[2] = kDirectionSouth;
    else if (deltay > 10)
        d[2] = kDirectionNorth;
    else
        d[2] = kDirectionNone;

    // try direct route
    if (d[1] != kDirectionNone && d[2] != kDirectionNone)
    {
        object->move_direction_ = diagonals[((deltay < 0) << 1) + (deltax > 0)];
        if (object->move_direction_ != turnaround && TryWalk(object))
            return;
    }

    // try other directions
    if (RandomByteDeterministic() > 200 || fabs(deltay) > fabs(deltax))
    {
        tdir = d[1];
        d[1] = d[2];
        d[2] = tdir;
    }

    if (d[1] == turnaround)
        d[1] = kDirectionNone;

    if (d[2] == turnaround)
        d[2] = kDirectionNone;

    if (d[1] != kDirectionNone)
    {
        object->move_direction_ = d[1];
        if (TryWalk(object))
        {
            // either moved forward or attacked
            return;
        }
    }

    if (d[2] != kDirectionNone)
    {
        object->move_direction_ = d[2];

        if (TryWalk(object))
            return;
    }

    // there is no direct path to the player,
    // so pick another direction.
    if (olddir != kDirectionNone)
    {
        object->move_direction_ = olddir;

        if (TryWalk(object))
            return;
    }

    // randomly determine direction of search
    if (RandomByteDeterministic() & 1)
    {
        for (tdir = kDirectionEast; tdir <= kDirectionSoutheast; tdir = (DirectionType)((int)tdir + 1))
        {
            if (tdir != turnaround)
            {
                object->move_direction_ = tdir;

                if (TryWalk(object))
                    return;
            }
        }
    }
    else
    {
        for (tdir = kDirectionSoutheast; tdir != (DirectionType)(kDirectionEast - 1);
             tdir = (DirectionType)((int)tdir - 1))
        {
            if (tdir != turnaround)
            {
                object->move_direction_ = tdir;

                if (TryWalk(object))
                    return;
            }
        }
    }

    if (turnaround != kDirectionNone)
    {
        object->move_direction_ = turnaround;
        if (TryWalk(object))
            return;
    }

    // cannot move
    object->move_direction_ = kDirectionNone;
}

//
// Used to find a player, either to set as support object or to set as a target.
// Range is angle range on either side of eyes, 90 degrees for normal
// view, 180 degrees for total sight in all dirs.
// Returns true if a player is found.
//
bool LookForPlayers(MapObject *actor, BAMAngle range, bool ToSupport)
{
    int      c;
    int      stop;
    Player  *player;
    BAMAngle an;
    float    dist;

    c    = 0;
    stop = (actor->last_look_ - 1 + kMaximumPlayers) % kMaximumPlayers;

    for (; actor->last_look_ != stop; actor->last_look_ = (actor->last_look_ + 1) % kMaximumPlayers)
    {
        player = players[actor->last_look_];

        if (!player)
            continue;

        EPI_ASSERT(player->map_object_);

        // done looking ?
        if (c++ >= 2)
            break;

        // dead ?
        if (player->health_ <= 0)
            continue;

        // on the same team ?
        if ((actor->side_ & player->map_object_->side_) != 0)
        {
            if (!ToSupport) // not looking to support a player
                continue;
        }

        if (range < kBAMAngle180)
        {
            an = PointToAngle(actor->x, actor->y, player->map_object_->x, player->map_object_->y) - actor->angle_;

            if (range <= an && an <= (range * -1))
            {
                // behind back.
                // if real close, react anyway
                dist = ApproximateDistance(player->map_object_->x - actor->x, player->map_object_->y - actor->y);

                if (dist > kMeleeRange)
                    continue;
            }
        }

        // out of sight ?
        if (!CheckSight(actor, player->map_object_))
            continue;

        if (ToSupport) // support the player
            actor->SetSupportObject(player->map_object_);
        else           // target the player
            actor->SetTarget(player->map_object_);

        return true;
    }

    return false;
}

//
//   BOSS-BRAIN HANDLING
//

MapObject *LookForShootSpot(const MapObjectDefinition *spot_type)
{
    // -AJA- 2022: changed this to find all spots matching the wanted type,
    //             and return a random one.  Since brain spits occur seldomly
    //             (every few seconds), there is little need to pre-find them.

    std::vector<MapObject *> spots;

    for (MapObject *cur = map_object_list_head; cur != nullptr; cur = cur->next_)
        if (cur->info_ == spot_type && !cur->IsRemoved())
            spots.push_back(cur);

    if (spots.empty())
        return nullptr;

    int idx = RandomShort() % (int)spots.size();

    return spots[idx];
}

static void SpawnDeathMissile(MapObject *source, float x, float y, float z)
{
    const MapObjectDefinition *info;
    MapObject                 *th;

    info = mobjtypes.Lookup("BRAIN_DEATH_MISSILE");

    th = CreateMapObject(x, y, z, info);
    if (th->info_->seesound_)
        StartSoundEffect(th->info_->seesound_, GetSoundEffectCategory(th), th);

    th->SetRealSource(source);
    th->SetSpawnSource(source);

    th->momentum_.X = (x - source->x) / 50.0f;
    th->momentum_.Y = -0.25f;
    th->momentum_.Z = (z - source->z) / 50.0f;

    th->tics_ -= RandomByte() & 7;

    if (th->tics_ < 1)
        th->tics_ = 1;
}

void A_BrainScream(MapObject *bossbrain)
{
    // The brain and his pain...

    float x, y, z;
    float min_x, max_x;

    min_x = bossbrain->x - 280.0f;
    max_x = bossbrain->x + 280.0f;

    for (x = min_x; x < max_x; x += 4)
    {
        y = bossbrain->y - 320.0f;
        z = bossbrain->z + (RandomByteDeterministic() - 180.0f) * 2.0f;

        SpawnDeathMissile(bossbrain, x, y, z);
    }

    if (bossbrain->info_->deathsound_)
        StartSoundEffect(bossbrain->info_->deathsound_, GetSoundEffectCategory(bossbrain), bossbrain);
}

void A_BrainMissileExplode(MapObject *mo)
{
    float x, y, z;

    if (!mo->source_)
        return;

    x = mo->source_->x + (RandomByteDeterministic() - 128.0f) * 4.0f;
    y = mo->source_->y - 320.0f;
    z = mo->source_->z + (RandomByteDeterministic() - 180.0f) * 2.0f;

    SpawnDeathMissile(mo->source_, x, y, z);
}

void A_BrainDie(MapObject *mo)
{
    EPI_UNUSED(mo);
    ExitLevel(kTicRate);
}

void A_BrainSpit(MapObject *shooter)
{
    static int easy = 0;

    // when skill is easy, only fire every second cube.

    easy ^= 1;

    if (game_skill <= kSkillEasy && (!easy))
        return;

    // shoot out a cube
    A_RangeAttack(shooter);
}

void A_CubeSpawn(MapObject *cube)
{
    MapObject                 *targ;
    MapObject                 *newmobj;
    const MapObjectDefinition *type;
    int                        r;

    targ = cube->target_;

    // -AJA- 2007/07/28: workaround for DeHackEd patches using S_SPAWNFIRE
    if (!targ || !cube->current_attack_ || cube->current_attack_->attackstyle_ != kAttackStyleShootToSpot)
        return;

    // Randomly select monster to spawn.
    r = RandomByteDeterministic();

    // Probability distribution (kind of :)),
    // decreasing likelihood.
    if (r < 50)
        type = mobjtypes.Lookup("IMP");
    else if (r < 90)
        type = mobjtypes.Lookup("DEMON");
    else if (r < 120)
        type = mobjtypes.Lookup("SPECTRE");
    else if (r < 130)
        type = mobjtypes.Lookup("PAIN_ELEMENTAL");
    else if (r < 160)
        type = mobjtypes.Lookup("CACODEMON");
    else if (r < 162)
        type = mobjtypes.Lookup("ARCHVILE");
    else if (r < 172)
        type = mobjtypes.Lookup("REVENANT");
    else if (r < 192)
        type = mobjtypes.Lookup("ARACHNOTRON");
    else if (r < 222)
        type = mobjtypes.Lookup("MANCUBUS");
    else if (r < 246)
        type = mobjtypes.Lookup("HELL_KNIGHT");
    else
        type = mobjtypes.Lookup("BARON_OF_HELL");

    newmobj = CreateMapObject(targ->x, targ->y, targ->z, type);

    if (LookForPlayers(newmobj, kBAMAngle180))
    {
        if (newmobj->info_->chase_state_)
            MapObjectSetState(newmobj, newmobj->info_->chase_state_);
        else
            MapObjectSetState(newmobj, newmobj->info_->spawn_state_);
    }

    // telefrag anything in this spot
    TeleportMove(newmobj, newmobj->x, newmobj->y, newmobj->z);
}

void A_PlayerScream(MapObject *mo)
{
    SoundEffect *sound;

    sound = mo->info_->deathsound_;

    if ((mo->health_ < -50) && (IsLumpInAnyWad("DSPDIEHI")))
    {
        // if the player dies and unclipped health is < -50%...

        sound = sfxdefs.GetEffect("PDIEHI");
    }

    StartSoundEffect(sound, GetSoundEffectCategory(mo), mo);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
