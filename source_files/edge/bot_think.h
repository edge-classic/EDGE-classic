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

#ifndef __P_BOT_H__
#define __P_BOT_H__

#include "types.h"
#include "p_mobj.h"
#include "e_ticcmd.h"
#include "r_defs.h"

class weapondef_c;
class bot_path_c;

typedef struct benefit_s benefit_t;

// specific tasks which the bot needs/wants to do.
// these can occur in combination with the behaviors above, e.g. while
// attacking something a bot may still want to pickup some health or
// use a lift.
enum bot_task_e
{
    TASK_None = 0, // no task right now
    TASK_GetItem,  // pickup a nearby item (in tracer)
    TASK_OpenDoor, // open a door
    TASK_UseLift,  // lower a lift, ride it to top
};

// stages for TASK_OpenDoor
enum task_open_door_e
{
    TKDOOR_Approach = 0, // walk to door and face it
    TKDOOR_Use      = 1, // press USE button, wait for it to open
};

// stages for TASK_UseLift
enum task_use_lift_e
{
    TKLIFT_Approach = 0, // walk to lift and face it
    TKLIFT_Use      = 1, // press USE button, wait for it to lower
    TKLIFT_Ride     = 2, // hop on lift, ride it to the top
};

// results of FollowPath()
enum bot_follow_path_e
{
    FOLLOW_OK = 0, // going okay...
    FOLLOW_Done,   // reached end of path
    FOLLOW_Failed  // got stuck somewhere
};

// This describes what action the bot wants to do.
// It will be translated to a ticcmd_t by P_BotPlayerBuilder.

struct botcmd_t
{
    // desired movement
    int     speed;
    BAMAngle direction;

    // buttons
    bool attack;
    bool attack2;
    bool use;
    bool jump;

    // weapon to switch to (key number), -1 means no change.
    int weapon;
};

class bot_t
{
  public:
    struct player_s *pl = nullptr;

    bot_task_e task = TASK_None;

    BAMAngle look_angle = 0;
    float   look_slope = 0;

    // 0 = go straight, -1 = left, +1 = right
    int weave      = 0;
    int weave_time = 0;

    // 0 = no strafing, -1 = left, +1 = right.  only used when fighting.
    int strafe_dir  = 0;
    int strafe_time = 0;

    // we lose patience for every tic which we cannot see our target
    int     patience    = 0;
    bool    see_enemy   = false;
    BAMAngle enemy_angle = 0;
    float   enemy_slope = 0;
    float   enemy_dist  = 0;

    int dead_time   = 0; // increases when dead
    int look_time   = 0; // when to look for items
    int weapon_time = 0; // when to reconsider weapons

    // last position, to check if we actually moved
    float last_x       = 0;
    float last_y       = 0;
    bool  hit_obstacle = false;
    bool  near_leader  = false;

    // -- pathing info --
    // used for DM roaming, COOP follow-the-leader, and getting items.
    // main_goal is final target.  travel_time detects losing the path.
    // path_wait is when we need a path, but are waiting a bit.
    bot_path_c *path = nullptr;
    position_c  roam_goal{0, 0, 0};
    int         travel_time = 0;
    int         path_wait   = 0;

    // information for TASK_GetItem (+ the pathing info)
    int item_time = 0;

    // information for TASK_OpenDoor
    int          door_stage = 0;
    int          door_time  = 0;
    const seg_t *door_seg   = nullptr;

    // information for TASK_UseLift
    int          lift_stage = 0;
    int          lift_time  = 0;
    const seg_t *lift_seg   = nullptr;

    botcmd_t cmd;

  public:
    void Think();
    void DeathThink();
    void ConvertTiccmd(ticcmd_t *dest);
    void Respawn();
    void EndLevel();

  private:
    float DistTo(position_c pos) const;

    void  SelectWeapon();
    bool  HasWeapon(const weapondef_c *info) const;
    bool  MeleeWeapon() const;
    float EvaluateWeapon(int w_num, int &key) const;
    bool  CanGetArmour(const benefit_t *be, int extendedflags) const;

    void MoveToward(const position_c &pos);
    void WalkToward(const position_c &pos);
    void WeaveToward(const position_c &pos);
    void WeaveToward(const mobj_t *mo);
    void WeaveNearLeader(const mobj_t *leader);
    void RetreatFrom(const mobj_t *mo);
    void Strafe(bool right);
    void StrafeAroundEnemy();

    bot_follow_path_e FollowPath(bool do_look);
    void              DetectObstacle();
    void              Meander();

    void LookAround();
    void LookForEnemies(float radius);
    void LookForItems(float radius);
    void LookForLeader();
    void PathToLeader();
    void EstimateTravelTime();
    void UpdateEnemy();
    bool IsEnemyVisible(mobj_t *mo);

    bool IsBarrel(const mobj_t *mo);

    void Think_Roam();
    void Think_Help();
    void Think_Fight();
    void Think_GetItem();
    void Think_OpenDoor();
    void Think_UseLift();

    void PainResponse();
    void FinishGetItem();
    void FinishDoorOrLift(bool ok);

    void TurnToward(BAMAngle angle, float slope, bool fast);
    void TurnToward(const mobj_t *mo, bool fast);
    void ShootTarget();

    void DeletePath();

  public:
    float EvalItem(const mobj_t *mo);
    float EvalEnemy(const mobj_t *mo);
};

void P_BotCreate(struct player_s *pl, bool recreate);

void BOT_BeginLevel(void);
void BOT_EndLevel(void);

#endif // __P_BOT_H__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
