//----------------------------------------------------------------------------
//  EDGE: DeathBots
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

#pragma once

#include "e_ticcmd.h"
#include "math_bam.h"
#include "p_mobj.h"
#include "r_defs.h"

class BotPath;

// specific tasks which the bot needs/wants to do.
// these can occur in combination with the behaviors above, e.g. while
// attacking something a bot may still want to pickup some health or
// use a lift.
enum BotTask
{
    kBotTaskNone = 0, // no task right now
    kBotTaskGetItem,  // pickup a nearby item (in tracer)
    kBotTaskOpenDoor, // open a door
    kBotTaskUseLift,  // lower a lift, ride it to top
};

// stages for kBotTaskOpenDoor
enum BotOpenDoorTask
{
    kBotOpenDoorTaskApproach = 0, // walk to door and face it
    kBotOpenDoorTaskUse      = 1, // press USE button, wait for it to open
};

// stages for kBotTaskUseLift
enum BotUseLiftTask
{
    kBotUseLiftTaskApproach = 0, // walk to lift and face it
    kBotUseLiftTaskUse      = 1, // press USE button, wait for it to lower
    kBotUseLiftTaskRide     = 2, // hop on lift, ride it to the top
};

// results of FollowPath()
enum BotFollowPathResult
{
    kBotFollowPathResultOK = 0, // going okay...
    kBotFollowPathResultDone,   // reached end of path
    kBotFollowPathResultFailed  // got stuck somewhere
};

// This describes what action the bot wants to do.
// It will be translated to a TicCommand by BotPlayerBuilder.

struct BotCommand
{
    // desired movement
    int      speed;
    BAMAngle direction;

    // buttons
    bool attack;
    bool attack2;
    // Add attack3/4? - Dasho
    bool use;
    bool jump;

    // weapon to switch to (key number), -1 means no change.
    int weapon;
};

class DeathBot
{
  public:
    class Player *pl_ = nullptr;

    BotTask task_ = kBotTaskNone;

    BAMAngle look_angle_ = 0;
    float    look_slope_ = 0;

    // 0 = go straight, -1 = left, +1 = right
    int weave_      = 0;
    int weave_time_ = 0;

    // 0 = no strafing, -1 = left, +1 = right.  only used when fighting.
    int strafe_dir_  = 0;
    int strafe_time_ = 0;

    // we lose patience for every tic which we cannot see our target
    int      patience_    = 0;
    bool     see_enemy_   = false;
    BAMAngle enemy_angle_ = 0;
    float    enemy_slope_ = 0;
    float    enemy_dist_  = 0;

    int dead_time_   = 0; // increases when dead
    int look_time_   = 0; // when to look for items
    int weapon_time_ = 0; // when to reconsider weapons

    // last position, to check if we actually moved
    float last_x_       = 0;
    float last_y_       = 0;
    bool  hit_obstacle_ = false;
    bool  near_leader_  = false;

    // -- pathing info --
    // used for DM roaming, COOP follow-the-leader, and getting items.
    // main_goal is final target.  travel_time detects losing the path.
    // path_wait is when we need a path, but are waiting a bit.
    BotPath *path_ = nullptr;
    Position roam_goal_{0, 0, 0};
    int      travel_time_ = 0;
    int      path_wait_   = 0;

    // information for kBotTaskGetItem (+ the pathing info)
    int item_time_ = 0;

    // information for kBotTaskOpenDoor
    int        door_stage_ = 0;
    int        door_time_  = 0;
    const Seg *door_seg_   = nullptr;

    // information for kBotTaskUseLift
    int        lift_stage_ = 0;
    int        lift_time_  = 0;
    const Seg *lift_seg_   = nullptr;

    BotCommand cmd_;

  public:
    void Think();
    void DeathThink();
    void ConvertTiccmd(EventTicCommand *dest);
    void Respawn();
    void EndLevel();

  private:
    float DistTo(Position pos) const;

    void  SelectWeapon();
    bool  HasWeapon(const WeaponDefinition *info) const;
    bool  MeleeWeapon() const;
    float EvaluateWeapon(int w_num, int &key) const;
    bool  CanGetArmour(const Benefit *be, int extendedflags) const;

    void MoveToward(const Position &pos);
    void WalkToward(const Position &pos);
    void WeaveToward(const Position &pos);
    void WeaveToward(const MapObject *mo);
    void WeaveNearLeader(const MapObject *leader);
    void RetreatFrom(const MapObject *mo);
    void Strafe(bool right);
    void StrafeAroundEnemy();

    BotFollowPathResult FollowPath(bool do_look);
    void                DetectObstacle();
    void                Meander();

    void LookAround();
    void LookForEnemies(float radius);
    void LookForItems(float radius);
    void LookForLeader();
    void PathToLeader();
    void EstimateTravelTime();
    void UpdateEnemy();
    bool IsEnemyVisible(MapObject *mo);

    bool IsBarrel(const MapObject *mo);

    void ThinkRoam();
    void ThinkHelp();
    void ThinkFight();
    void ThinkGetItem();
    void ThinkOpenDoor();
    void ThinkUseLift();

    void PainResponse();
    void FinishGetItem();
    void FinishDoorOrLift(bool ok);

    void TurnToward(BAMAngle angle, float slope, bool fast);
    void TurnToward(const MapObject *mo, bool fast);
    void ShootTarget();

    void DeletePath();

  public:
    float EvalItem(const MapObject *mo);
    float EvalEnemy(const MapObject *mo);
};

void P_BotCreate(class Player *pl, bool recreate);

void BotBeginLevel(void);
void BotEndLevel(void);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
