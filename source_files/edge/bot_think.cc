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

#include <string.h>

#include "i_defs.h"

#include "bot_think.h"
#include "bot_nav.h"
#include "con_main.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "g_game.h"
#include "m_bbox.h"
#include "m_random.h"
#include "p_local.h"
#include "p_weapon.h"
#include "p_action.h"
#include "rad_trig.h"
#include "r_state.h"
#include "s_sound.h"
#include "w_wad.h"

#include "AlmostEquals.h"

#define DEBUG 0

// this ranges from 0 (VERY EASY) to 4 (VERY HARD)
DEF_CVAR(bot_skill, "2", CVAR_ARCHIVE)

#define MOVE_SPEED 20

//----------------------------------------------------------------------------
//  EVALUATING ITEMS, MONSTERS, WEAPONS
//----------------------------------------------------------------------------

bool bot_t::HasWeapon(const WeaponDefinition *info) const
{
    for (int i = 0; i < MAXWEAPONS; i++)
        if (pl->weapons[i].owned && pl->weapons[i].info == info)
            return true;

    return false;
}

bool bot_t::CanGetArmour(const Benefit *be, int extendedflags) const
{
    // this matches the logic in GiveArmour() in p_inter.cc

    armour_type_e a_class = (armour_type_e)be->sub.type;

    float amount = be->amount;

    if (extendedflags & EF_SIMPLEARMOUR)
    {
        float slack = be->limit - pl->armours[a_class];

        if (amount > slack)
            amount = slack;

        return (amount > 0);
    }

    float slack   = be->limit - pl->totalarmour;
    float upgrade = 0;

    if (slack < 0)
        return false;

    for (int cl = a_class - 1; cl >= 0; cl--)
        upgrade += pl->armours[cl];

    if (upgrade > amount)
        upgrade = amount;

    slack += upgrade;

    if (amount > slack)
        amount = slack;

    return !(AlmostEquals(amount, 0.0f) && AlmostEquals(upgrade, 0.0f));
}

bool bot_t::MeleeWeapon() const
{
    int wp_num = pl->ready_wp;

    if (pl->pending_wp >= 0)
        wp_num = pl->pending_wp;

    return pl->weapons[wp_num].info->ammo_[0] == kAmmunitionTypeNoAmmo;
}

bool bot_t::IsBarrel(const mobj_t *mo)
{
    if (mo->player)
        return false;

    if (0 == (mo->extendedflags & EF_MONSTER))
        return false;

    return true;
}

float bot_t::EvalEnemy(const mobj_t *mo)
{
    // returns -1 to ignore, +1 to attack.
    // [ higher values are not possible, so no way to prioritize enemies ]

    // The following must be true to justify that you attack a target:
    // - target may not be yourself or your support obj.
    // - target must either want to attack you, or be on a different side
    // - target may not have the same supportobj as you.
    // - You must be able to see and shoot the target.

    if (0 == (mo->flags & MF_SHOOTABLE) || mo->health <= 0)
        return -1;

    // occasionally shoot barrels
    if (IsBarrel(mo))
        return (C_Random() % 100 < 20) ? +1 : -1;

    if (0 == (mo->extendedflags & EF_MONSTER) && !mo->player)
        return -1;

    if (mo->player && mo->player == pl)
        return -1;

    if (pl->mo->supportobj == mo)
        return -1;

    if (!DEATHMATCH() && mo->player)
        return -1;

    if (!DEATHMATCH() && mo->supportobj && mo->supportobj->player)
        return -1;

    // EXTERMINATE !!

    return 1.0;
}

float bot_t::EvalItem(const mobj_t *mo)
{
    // determine if an item is worth getting.
    // this depends on our current inventory, whether the game mode is COOP
    // or DEATHMATCH, and whether we are fighting or not.

    if (0 == (mo->flags & MF_SPECIAL))
        return -1;

    bool fighting = (pl->mo->target != nullptr);

    // do we *really* need some health?
    bool want_health = (pl->mo->health < 90);
    bool need_health = (pl->mo->health < 45);

    // handle weapons first (due to deathmatch rules)
    for (const Benefit *B = mo->info->pickup_benefits; B != nullptr; B = B->next)
    {
        if (B->type == kBenefitTypeWeapon)
        {
            if (!HasWeapon(B->sub.weap))
                return NAV_EvaluateBigItem(mo);

            // try to get ammo from a dropped weapon
            if (mo->flags & MF_DROPPED)
                continue;

            // cannot get the ammo from a placed weapon except in altdeath
            if (deathmatch != 2)
                return -1;
        }

        // ignore powerups, backpacks and armor in COOP.
        // [ leave them for the human players ]
        if (!DEATHMATCH())
        {
            switch (B->type)
            {
            case kBenefitTypePowerup:
            case kBenefitTypeArmour:
            case kBenefitTypeAmmoLimit:
                return -1;

            default:
                break;
            }
        }
    }

    for (const Benefit *B = mo->info->pickup_benefits; B != nullptr; B = B->next)
    {
        switch (B->type)
        {
        case kBenefitTypeKey:
            // have it already?
            if (pl->cards & (DoorKeyType)B->sub.type)
                continue;

            return 90;

        case kBenefitTypePowerup:
            return NAV_EvaluateBigItem(mo);

        case kBenefitTypeArmour:
            // ignore when fighting
            if (fighting)
                return -1;

            if (!CanGetArmour(B, mo->extendedflags))
                continue;

            return NAV_EvaluateBigItem(mo);

        case kBenefitTypeHealth: {
            // cannot get it?
            if (pl->health >= B->limit)
                return -1;

            // ignore potions unless really desperate
            if (B->amount < 2.5)
            {
                if (pl->health > 19)
                    return -1;

                return 2;
            }

            // don't grab health when fighting unless we NEED it
            if (!(need_health || (want_health && !fighting)))
                return -1;

            if (need_health)
                return 120;
            else if (B->amount > 55)
                return 40;
            else
                return 30;
        }

        case kBenefitTypeAmmo: {
            if (B->sub.type == kAmmunitionTypeNoAmmo)
                continue;

            int ammo = B->sub.type;
            int max  = pl->ammo[ammo].max;

            // in COOP mode, leave some ammo for others
            if (!DEATHMATCH())
                max = max / 4;

            if (pl->ammo[ammo].num >= max)
                continue;

            if (pl->ammo[ammo].num == 0)
                return 35;
            else if (fighting)
                // ignore unneeded ammo when fighting
                continue;
            else
                return 10;
        }

        case kBenefitTypeInventory:
            // TODO : heretic stuff
            continue;

        default:
            continue;
        }
    }

    return -1;
}

float bot_t::EvaluateWeapon(int w_num, int &key) const
{
    // this evaluates weapons owned by the bot (NOT ones in the map).
    // returns -1 when not actually usable (e.g. no ammo).

    playerweapon_t *wp = &pl->weapons[w_num];

    // don't have this weapon
    if (!wp->owned)
        return -1;

    WeaponDefinition *weapon = wp->info;
    SYS_ASSERT(weapon);

    AttackDefinition *attack = weapon->attack_[0];
    if (!attack)
        return -1;

    key = weapon->bind_key_;

    // have enough ammo?
    if (weapon->ammo_[0] != kAmmunitionTypeNoAmmo)
    {
        if (pl->ammo[weapon->ammo_[0]].num < weapon->ammopershot_[0])
            return -1;
    }

    float score = 10.0f * weapon->priority_;

    // prefer smaller weapons for smaller monsters.
    // when not fighting, prefer biggest non-dangerous weapon.
    if (pl->mo->target == nullptr || DEATHMATCH())
    {
        if (!weapon->dangerous_)
            score += 1000.0f;
    }
    else if (pl->mo->target->spawnhealth > 250)
    {
        if (weapon->priority_ > 5)
            score += 1000.0f;
    }
    else
    {
        if (2 <= weapon->priority_ && weapon->priority_ <= 5)
            score += 1000.0f;
    }

    // small preference for the current weapon (break ties)
    if (w_num == pl->ready_wp)
        score += 2.0f;

    // ultimate tie breaker (when two weapons have same priority)
    score += (float)w_num / 32.0f;

    return score;
}

//----------------------------------------------------------------------------

float bot_t::DistTo(position_c pos) const
{
    float dx = fabs(pos.x - pl->mo->x);
    float dy = fabs(pos.y - pl->mo->y);

    return hypotf(dx, dy);
}

void bot_t::PainResponse()
{
    // oneself?
    if (pl->attacker == pl->mo)
        return;

    // ignore friendly fire -- shit happens
    if (!DEATHMATCH() && pl->attacker->player)
        return;

    if (pl->attacker->health <= 0)
    {
        pl->attacker = nullptr;
        return;
    }

    // TODO only update target if "threat" is greater than current target

    if (pl->mo->target == nullptr)
    {
        if (IsEnemyVisible(pl->attacker))
        {
            pl->mo->SetTarget(pl->attacker);
            UpdateEnemy();
            patience = TICRATE;
        }
    }
}

void bot_t::LookForLeader()
{
    if (DEATHMATCH())
        return;

    if (pl->mo->supportobj != nullptr)
        return;

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        player_t *p2 = players[i];

        if (p2 == nullptr || p2->isBot())
            continue;

        // when multiple humans, make it random who is picked
        if (C_Random() % 100 < 90)
            continue;

        pl->mo->SetSupportObj(p2->mo);
    }
}

bool bot_t::IsEnemyVisible(mobj_t *enemy)
{
    float dx = enemy->x - pl->mo->x;
    float dy = enemy->y - pl->mo->y;
    float dz = enemy->z - pl->mo->z;

    float slope = P_ApproxSlope(dx, dy, dz);

    // require slope to not be excessive, e.g. caged imps in MAP13
    if (slope > 1.0f)
        return false;

    return P_CheckSight(pl->mo, enemy);
}

void bot_t::LookForEnemies(float radius)
{
    // check sight of existing target
    if (pl->mo->target != nullptr)
    {
        UpdateEnemy();

        if (see_enemy)
        {
            patience = 2 * TICRATE;
            return;
        }

        // IDEA: if patience == TICRATE/2, try using pathing algo

        if (patience-- >= 0)
            return;

        // look for a new enemy
        pl->mo->SetTarget(nullptr);
    }

    // pick a random nearby monster, then check sight, since the enemy
    // may be on the other side of a wall.

    mobj_t *enemy = NAV_FindEnemy(this, radius);

    if (enemy != nullptr)
    {
        if (IsEnemyVisible(enemy))
        {
            pl->mo->SetTarget(enemy);
            UpdateEnemy();
            patience = TICRATE;
        }
    }
}

void bot_t::LookForItems(float radius)
{
    mobj_t     *item      = nullptr;
    bot_path_c *item_path = NAV_FindThing(this, radius, item);

    if (item_path == nullptr)
        return;

    // GET IT !!

    pl->mo->SetTracer(item);

    DeletePath();

    task      = TASK_GetItem;
    path      = item_path;
    item_time = TICRATE;

    EstimateTravelTime();
}

void bot_t::LookAround()
{
    look_time--;

    LookForEnemies(1024);

    if ((look_time & 3) == 2)
        LookForLeader();

    if (look_time >= 0)
        return;

    // look for items every second or so
    look_time = 20 + C_Random() % 20;

    LookForItems(1024);
}

void bot_t::SelectWeapon()
{
    // reconsider every second or so
    weapon_time = 20 + C_Random() % 20;

    // allow any weapon change to complete first
    if (pl->pending_wp != WPSEL_NoChange)
        return;

    int   best       = pl->ready_wp;
    int   best_key   = -1;
    float best_score = 0;

    for (int i = 0; i < MAXWEAPONS; i++)
    {
        int   key   = -1;
        float score = EvaluateWeapon(i, key);

        if (score > best_score)
        {
            best       = i;
            best_key   = key;
            best_score = score;
        }
    }

    if (best != pl->ready_wp)
    {
        cmd.weapon = best_key;
    }
}

void bot_t::MoveToward(const position_c &pos)
{
    cmd.speed     = MOVE_SPEED + (6.25 * bot_skill.d);
    cmd.direction = R_PointToAngle(pl->mo->x, pl->mo->y, pos.x, pos.y);
}

void bot_t::WalkToward(const position_c &pos)
{
    cmd.speed     = (MOVE_SPEED + (3.125 * bot_skill.d));
    cmd.direction = R_PointToAngle(pl->mo->x, pl->mo->y, pos.x, pos.y);
}

void bot_t::TurnToward(BAMAngle want_angle, float want_slope, bool fast)
{
    // horizontal (yaw) angle
    BAMAngle delta = want_angle - pl->mo->angle;

    if (delta < kBAMAngle180)
        delta = delta / (fast ? 3 : 8);
    else
        delta = kBAMAngle360 - (kBAMAngle360 - delta) / (fast ? 3 : 8);

    look_angle = pl->mo->angle + delta;

    // vertical (pitch or mlook) angle
    if (want_slope < -2.0)
        want_slope = -2.0;
    if (want_slope > +2.0)
        want_slope = +2.0;

    float diff = want_slope - epi::BAMTan(pl->mo->vertangle);

    if (fabs(diff) < (fast ? (0.04 + (0.02 * bot_skill.f)) : 0.04))
        look_slope = want_slope;
    else if (diff < 0)
        look_slope -= fast ? (0.03 + (0.015 * bot_skill.f)) : 0.03;
    else
        look_slope += fast ? (0.03 + (0.015 * bot_skill.f)) : 0.03;
}

void bot_t::TurnToward(const mobj_t *mo, bool fast)
{
    float dx = mo->x - pl->mo->x;
    float dy = mo->y - pl->mo->y;
    float dz = mo->z - pl->mo->z;

    BAMAngle want_angle = R_PointToAngle(0, 0, dx, dy);
    float   want_slope = P_ApproxSlope(dx, dy, dz);

    TurnToward(want_angle, want_slope, fast);
}

void bot_t::WeaveToward(const position_c &pos)
{
    // usually try to move directly toward a wanted position.
    // but if something gets in our way, we try to "weave" around it,
    // by sometimes going diagonally left and sometimes right.

    float dist = DistTo(pos);

    if (weave_time-- < 0)
    {
        weave_time = 10 + C_Random() % 10;

        bool neg = weave < 0;

        if (hit_obstacle)
            weave = neg ? +2 : -2;
        else if (dist > 192.0)
            weave = neg ? +1 : -1;
        else
            weave = 0;
    }

    MoveToward(pos);

    if (weave == -2)
        cmd.direction -= kBAMAngle5 * 12;
    if (weave == -1)
        cmd.direction -= kBAMAngle5 * 3;
    if (weave == +1)
        cmd.direction += kBAMAngle5 * 3;
    if (weave == +2)
        cmd.direction += kBAMAngle5 * 12;
}

void bot_t::WeaveToward(const mobj_t *mo)
{
    position_c pos{mo->x, mo->y, mo->z};

    WeaveToward(pos);
}

void bot_t::RetreatFrom(const mobj_t *enemy)
{
    float dx   = pl->mo->x - enemy->x;
    float dy   = pl->mo->y - enemy->y;
    float dlen = HMM_MAX(hypotf(dx, dy), 1.0f);

    position_c pos{pl->mo->x, pl->mo->y, pl->mo->z};

    pos.x += 16.0f * (dx / dlen);
    pos.y += 16.0f * (dy / dlen);

    WeaveToward(pos);
}

void bot_t::Strafe(bool right)
{
    cmd.speed     = MOVE_SPEED + (6.25 * bot_skill.d);
    cmd.direction = pl->mo->angle + (right ? kBAMAngle270 : kBAMAngle90);
}

void bot_t::DetectObstacle()
{
    mobj_t *mo = pl->mo;

    float dx = last_x - mo->x;
    float dy = last_y - mo->y;

    last_x = mo->x;
    last_y = mo->y;

    hit_obstacle = (dx * dx + dy * dy) < 0.2;
}

void bot_t::Meander()
{
    // TODO wander about without falling into nukage pits (etc)
}

void bot_t::UpdateEnemy()
{
    mobj_t *enemy = pl->mo->target;

    // update angle, slope and distance, even if not seen
    position_c pos = {enemy->x, enemy->y, enemy->z};

    float dx = enemy->x - pl->mo->x;
    float dy = enemy->y - pl->mo->y;
    float dz = enemy->z - pl->mo->z;

    enemy_angle = R_PointToAngle(0, 0, dx, dy);
    enemy_slope = P_ApproxSlope(dx, dy, dz);
    enemy_dist  = DistTo(pos);

    // can see them?
    see_enemy = IsEnemyVisible(enemy);
}

void bot_t::StrafeAroundEnemy()
{
    if (strafe_time-- < 0)
    {
        // pick a random strafe direction.
        // it will often be the same as before, that is okay.
        int r = C_Random();

        if ((r & 3) == 0)
            strafe_dir = 0;
        else
            strafe_dir = (r & 16) ? -1 : +1;

        uint8_t wait = 60 - (bot_skill.d * 10);

        strafe_time = wait + r % wait;
        return;
    }

    if (strafe_dir != 0)
    {
        Strafe(strafe_dir > 0);
    }
}

void bot_t::ShootTarget()
{
    // no weapon to shoot?
    if (pl->ready_wp == WPSEL_None || pl->pending_wp != WPSEL_NoChange)
        return;

    // TODO: ammo check

    // too far away?
    if (enemy_dist > 2000)
        return;

    // too close for a dangerous weapon?
    const WeaponDefinition *weapon = pl->weapons[pl->ready_wp].info;
    if (weapon->dangerous_ && enemy_dist < 208)
        return;

    // check that we are facing the enemy
    BAMAngle delta   = enemy_angle - pl->mo->angle;
    float   sl_diff = fabs(enemy_slope - epi::BAMTan(pl->mo->vertangle));

    if (delta > kBAMAngle180)
        delta = kBAMAngle360 - delta;

    // the further away we are, the more accurate our shot must be.
    // e.g. at point-blank range, even 45 degrees away can hit.
    float acc_dist = HMM_MAX(enemy_dist, 32.0f);
    float adjust   = acc_dist / 32.0f;

    if (delta > (BAMAngle)(kBAMAngle90 / adjust / (11 - (2.5 * bot_skill.d))))
        return;

    if (sl_diff > (8.0f / adjust))
        return;

    // in COOP, check if other players might be hit
    if (!DEATHMATCH())
    {
        // TODO
    }

    cmd.attack = true;
}

void bot_t::Think_Fight()
{
    // Note: LookAround() has done sight-checking of our target

    // face our foe
    TurnToward(enemy_angle, enemy_slope, true);

    const mobj_t *enemy = pl->mo->target;

    // if lost sight, weave towards the target
    if (!see_enemy)
    {
        // IDEA: check if a LOS exists in a position to our left or right.
        //       if it does, the strafe purely left/right.
        //       [ do it in Think_Help too, assuming it works ]

        StrafeAroundEnemy();
        return;
    }

    // open fire!
    ShootTarget();

    /* --- decide where to move to --- */

    // DISTANCE:
    //   (1) melee weapons need to be as close, otherwise want *some* distance
    //   (2) dangerous weapons need a SAFE distance
    //   (3) hit-scan weapons lose accuracy when too far away
    //   (4) projectiles can be dodged when too far away
    //   (5) want the mlook angle (slope) to be reasonable
    //   (6) want to dodge a projectile from the side       (IDEA)
    //   (7) need to avoid [falling into] damaging sectors  (TODO)

    // SIDE-TO-SIDE:
    //   (1) want to dodge projectiles from the enemy
    //   (2) if enemy uses hit-scan, want to provide a moving target
    //   (3) need to avoid [falling into] damaging sectors  (TODO)

    if (MeleeWeapon())
    {
        WeaveToward(enemy);
        return;
    }

    // handle slope, equation is: `slope = dz / dist`
    float dz = fabs(pl->mo->z - enemy->z);

    float min_dist = HMM_MIN(dz * 2.0f, 480.0f);
    float max_dist = 640.0f;

    // handle dangerous weapons
    const WeaponDefinition *weapon = pl->weapons[pl->ready_wp].info;

    if (weapon->dangerous_)
        min_dist = HMM_MAX(min_dist, 224.0f);

    // approach if too far away
    if (enemy_dist > max_dist)
    {
        WeaveToward(enemy);
        return;
    }

    // retreat if too close
    if (enemy_dist < min_dist)
    {
        RetreatFrom(enemy);
        return;
    }

    StrafeAroundEnemy();
}

void bot_t::WeaveNearLeader(const mobj_t *leader)
{
    // pick a position some distance away, so that a human player
    // can get out of a narrow item closet (etc).

    float dx = pl->mo->x - leader->x;
    float dy = pl->mo->y - leader->y;

    float dlen = HMM_MAX(1.0f, hypotf(dx, dy));

    dx = dx * 96.0f / dlen;
    dy = dy * 96.0f / dlen;

    position_c pos{leader->x + dx, leader->y + dy, leader->z};

    TurnToward(leader, false);
    WeaveToward(pos);
}

void bot_t::PathToLeader()
{
    mobj_t *leader = pl->mo->supportobj;
    SYS_ASSERT(leader);

    DeletePath();

    path = NAV_FindPath(pl->mo, leader, 0);

    if (path != nullptr)
    {
        EstimateTravelTime();
    }
}

void bot_t::EstimateTravelTime()
{
    // estimate time to travel one segment of a path.
    // overestimates by quite a bit, to account for obstacles.

    float dist = DistTo(path->cur_dest());
    float tics = dist * 1.5f / 10.0f + 6.0f * TICRATE;

    travel_time = (int)tics;
}

void bot_t::Think_Help()
{
    mobj_t *leader = pl->mo->supportobj;

    // check if we are close to the leader, and can see them
    bool cur_near = false;

    position_c pos  = {leader->x, leader->y, leader->z};
    float      dist = DistTo(pos);

    // allow a bit of "hysteresis"
    float check_dist = near_leader ? 224.0 : 160.0;

    if (dist < check_dist && fabs(pl->mo->z - pos.z) <= 24.0)
    {
        cur_near = P_CheckSight(pl->mo, leader);
    }

    if (near_leader != cur_near)
    {
        near_leader = cur_near;

        DeletePath();

        if (!cur_near)
        {
            // wait a bit then find a path
            path_wait = 10 + C_Random() % 10;
        }
    }

    if (cur_near)
    {
        WeaveNearLeader(leader);
        return;
    }

    if (path != nullptr)
    {
        switch (FollowPath(true))
        {
        case FOLLOW_OK:
            return;

        case FOLLOW_Done:
            DeletePath();
            path_wait = 4 + C_Random() % 4;
            break;

        case FOLLOW_Failed:
            DeletePath();
            path_wait = 30 + C_Random() % 10;
            break;
        }
    }

    // we are waiting until we can establish a path

    if (path_wait-- < 0)
    {
        PathToLeader();
        path_wait = 30 + C_Random() % 10;
    }

    // if somewhat close, attempt to follow player
    if (dist < 512.0 && fabs(pl->mo->z - pos.z) <= 24.0)
        WeaveNearLeader(leader);
    else
        Meander();
}

bot_follow_path_e bot_t::FollowPath(bool do_look)
{
    // returns a FOLLOW_XXX enum constant.

    SYS_ASSERT(path != nullptr);
    SYS_ASSERT(!path->finished());

    // handle doors and lifts
    int flags = path->nodes[path->along].flags;

    if (flags & PNODE_Door)
    {
        task       = TASK_OpenDoor;
        door_stage = TKDOOR_Approach;
        door_seg   = path->nodes[path->along].seg;
        door_time  = 5 * TICRATE;
        SYS_ASSERT(door_seg != nullptr);
        return FOLLOW_OK;
    }
    else if (flags & PNODE_Lift)
    {
        task       = TASK_UseLift;
        lift_stage = TKLIFT_Approach;
        lift_seg   = path->nodes[path->along].seg;
        lift_time  = 5 * TICRATE;
        SYS_ASSERT(lift_seg != nullptr);
        return FOLLOW_OK;
    }
    else if (flags & PNODE_Lift)
    {
        // TODO: TASK_Telport which attempts not to telefrag / be telefragged
    }

    // have we reached the next node?
    if (path->reached_dest(pl->mo))
    {
        path->along += 1;

        if (path->finished())
        {
            return FOLLOW_Done;
        }

        EstimateTravelTime();
    }

    if (travel_time-- < 0)
    {
        return FOLLOW_Failed;
    }

    // determine looking angle
    if (do_look)
    {
        position_c dest = path->cur_dest();

        if (path->along + 1 < path->nodes.size())
            dest = path->nodes[path->along + 1].pos;

        float dx = dest.x - pl->mo->x;
        float dy = dest.y - pl->mo->y;
        float dz = dest.z - pl->mo->z;

        BAMAngle want_angle = R_PointToAngle(0, 0, dx, dy);
        float   want_slope = P_ApproxSlope(dx, dy, dz);

        TurnToward(want_angle, want_slope, false);
    }

    WeaveToward(path->cur_dest());

    return FOLLOW_OK;
}

void bot_t::Think_Roam()
{
    if (path != nullptr)
    {
        switch (FollowPath(true))
        {
        case FOLLOW_OK:
            return;

        case FOLLOW_Done:
            // arrived at the spot!
            // TODO look for other nearby items

            DeletePath();
            path_wait = 4 + C_Random() % 4;
            break;

        case FOLLOW_Failed:
            DeletePath();
            path_wait = 30 + C_Random() % 10;
            break;
        }
    }

    if (path_wait-- < 0)
    {
        path_wait = 30 + C_Random() % 10;

        if (!NAV_NextRoamPoint(roam_goal))
        {
            roam_goal = position_c{0, 0, 0};
            return;
        }

        path = NAV_FindPath(pl->mo, &roam_goal, 0);

        // if no path found, try again soon
        if (path == nullptr)
        {
            roam_goal = position_c{0, 0, 0};
            return;
        }

        EstimateTravelTime();
    }

    Meander();
}

void bot_t::FinishGetItem()
{
    task = TASK_None;
    pl->mo->SetTracer(nullptr);

    DeletePath();
    path_wait = 4 + C_Random() % 4;

    // when fighting, look furthe for more items
    if (pl->mo->target != nullptr)
    {
        LookForItems(1024);
        return;
    }

    // otherwise collect nearby items
    LookForItems(256);

    if (task == TASK_GetItem)
        return;

    // continue to follow player
    if (pl->mo->supportobj != nullptr)
        return;

    // otherwise we were roaming about, so re-establish path
    if (!(AlmostEquals(roam_goal.x, 0.0f) && AlmostEquals(roam_goal.y, 0.0f) && AlmostEquals(roam_goal.z, 0.0f)))
    {
        path = NAV_FindPath(pl->mo, &roam_goal, 0);

        // if no path found, try again soon
        if (path == nullptr)
        {
            roam_goal = position_c{0, 0, 0};
            return;
        }

        EstimateTravelTime();
    }
}

void bot_t::Think_GetItem()
{
    // item gone?  (either we picked it up, or someone else did)
    if (pl->mo->tracer == nullptr)
    {
        FinishGetItem();
        return;
    }

    // if we are being chased, look at them, shoot sometimes
    if (pl->mo->target)
    {
        UpdateEnemy();

        TurnToward(enemy_angle, enemy_slope, false);

        if (see_enemy)
            ShootTarget();
    }
    else
    {
        TurnToward(pl->mo->tracer, false);
    }

    // follow the path previously found
    if (path != nullptr)
    {
        switch (FollowPath(false))
        {
        case FOLLOW_OK:
            return;

        case FOLLOW_Done:
            DeletePath();
            item_time = TICRATE;
            break;

        case FOLLOW_Failed:
            // took too long? (e.g. we got stuck)
            FinishGetItem();
            return;
        }
    }

    // detect not picking up the item
    if (item_time-- < 0)
    {
        FinishGetItem();
        return;
    }

    // move toward the item's location
    WeaveToward(pl->mo->tracer);
}

void bot_t::FinishDoorOrLift(bool ok)
{
    task = TASK_None;

    if (ok)
    {
        path->along += 1;
    }
    else
    {
        DeletePath();
        roam_goal = position_c{0, 0, 0};
    }
}

void bot_t::Think_OpenDoor()
{
    switch (door_stage)
    {
    case TKDOOR_Approach: {
        if (door_time-- < 0)
        {
            FinishDoorOrLift(false);
            return;
        }

        float   dist = DistTo(path->cur_dest());
        BAMAngle ang  = path->nodes[path->along].seg->angle + kBAMAngle90;
        BAMAngle diff = ang - pl->mo->angle;

        if (diff > kBAMAngle180)
            diff = kBAMAngle360 - diff;

        if (diff < kBAMAngle5 && dist < (USERANGE - 16))
        {
            door_stage = TKDOOR_Use;
            door_time  = TICRATE * 5;
            return;
        }

        TurnToward(ang, 0.0, false);
        WeaveToward(path->cur_dest());
        return;
    }

    case TKDOOR_Use: {
        if (door_time-- < 0)
        {
            FinishDoorOrLift(false);
            return;
        }

        // if closing, try to re-open
        const sector_t     *sector = door_seg->back_sub->sector;
        const plane_move_t *pm     = sector->ceil_move;

        if (pm != nullptr && pm->direction < 0)
        {
            if (door_time & 1)
                cmd.use = true;
            return;
        }

        // already open?
        if (sector->c_h > sector->f_h + 56.0f)
        {
            FinishDoorOrLift(true);
            return;
        }

        // door is opening, so don't interfere
        if (pm != nullptr)
            return;

        if (door_time & 1)
            cmd.use = true;

        return;
    }
    }
}

void bot_t::Think_UseLift()
{
    switch (lift_stage)
    {
    case TKDOOR_Approach: {
        if (lift_time-- < 0)
        {
            FinishDoorOrLift(false);
            return;
        }

        float   dist = DistTo(path->cur_dest());
        BAMAngle ang  = path->nodes[path->along].seg->angle + kBAMAngle90;
        BAMAngle diff = ang - pl->mo->angle;

        if (diff > kBAMAngle180)
            diff = kBAMAngle360 - diff;

        if (diff < kBAMAngle5 && dist < (USERANGE - 16))
        {
            lift_stage = TKLIFT_Use;
            lift_time  = TICRATE * 5;
            return;
        }

        TurnToward(ang, 0.0, false);
        WeaveToward(path->cur_dest());
        return;
    }

    case TKLIFT_Use: {
        if (lift_time-- < 0)
        {
            FinishDoorOrLift(false);
            return;
        }

        // if lift is raising, try to re-lower
        const sector_t     *sector = lift_seg->back_sub->sector;
        const plane_move_t *pm     = sector->floor_move;

        if (pm != nullptr && pm->direction > 0)
        {
            if (lift_time & 1)
                cmd.use = true;
            return;
        }

        // already lowered?
        if (sector->f_h < lift_seg->front_sub->sector->f_h + 24.0f)
        {
            // navigation code added a place to stand
            path->along += 1;

            // TODO compute time it will take for lift to go fully up
            lift_stage = TKLIFT_Ride;
            lift_time  = TICRATE * 10;
            return;
        }

        // lift is lowering, so don't interfere
        if (pm != nullptr)
            return;

        // try to activate it
        if (lift_time & 1)
            cmd.use = true;

        return;
    }

    case TKLIFT_Ride:
        if (lift_time-- < 0)
        {
            FinishDoorOrLift(false);
            return;
        }

        WalkToward(path->cur_dest());

        const sector_t *lift_sec = lift_seg->back_sub->sector;

        if (lift_sec->floor_move != nullptr)
        {
            // if lift went down again, don't time out
            if (lift_sec->floor_move->direction <= 0)
                lift_time = 10 * TICRATE;

            return;
        }

        // reached the top?
        bool ok = pl->mo->z > (lift_sec->f_h - 0.5);

        FinishDoorOrLift(ok);
        return;
    }
}

void bot_t::DeletePath()
{
    if (path != nullptr)
    {
        delete path;
        path = nullptr;
    }
}

//----------------------------------------------------------------------------

void bot_t::Think()
{
    SYS_ASSERT(pl != nullptr);
    SYS_ASSERT(pl->mo != nullptr);

    // initialize the botcmd_t
    memset(&cmd, 0, sizeof(botcmd_t));
    cmd.weapon = -1;

    // do nothing when game is paused
    if (paused)
        return;

    mobj_t *mo = pl->mo;

    // dead?
    if (mo->health <= 0)
    {
        DeathThink();
        return;
    }

    // forget target (etc) if they died
    if (mo->target && mo->target->health <= 0)
        mo->SetTarget(nullptr);

    if (mo->supportobj && mo->supportobj->health <= 0)
        mo->SetSupportObj(nullptr);

    // hurt by somebody?
    if (pl->attacker != nullptr)
    {
        PainResponse();
    }

    DetectObstacle();

    // doing a task?
    switch (task)
    {
    case TASK_GetItem:
        Think_GetItem();
        return;

    case TASK_OpenDoor:
        Think_OpenDoor();
        return;

    case TASK_UseLift:
        Think_UseLift();
        return;

    default:
        break;
    }

    LookAround();

    if (weapon_time-- < 0)
        SelectWeapon();

    // if we have a target enemy, fight it or flee it
    if (pl->mo->target != nullptr)
    {
        Think_Fight();
        return;
    }

    // if we have a leader (in co-op), follow them
    if (pl->mo->supportobj != nullptr)
    {
        Think_Help();
        return;
    }

    // in deathmatch, go to the roaming goal.
    // otherwise just meander around.

    Think_Roam();
}

void bot_t::DeathThink()
{
    dead_time++;

    // respawn after a random interval, at least one second
    if (dead_time > 30)
    {
        dead_time = 0;

        if (C_Random() % 100 < 35)
            cmd.use = true;
    }
}

void bot_t::ConvertTiccmd(ticcmd_t *dest)
{
    // we assume caller has cleared the ticcmd_t to zero.

    mobj_t *mo = pl->mo;

    if (cmd.attack)
        dest->buttons |= BT_ATTACK;

    if (cmd.attack2)
        dest->extbuttons |= EBT_SECONDATK;

    if (cmd.use)
        dest->buttons |= BT_USE;

    if (cmd.jump)
        dest->upwardmove = 0x20;

    if (cmd.weapon != -1)
    {
        dest->buttons |= BT_CHANGE;
        dest->buttons |= (cmd.weapon << BT_WEAPONSHIFT) & BT_WEAPONMASK;
    }

    dest->player_idx = pl->pnum;

    dest->angleturn = (mo->angle - look_angle) >> 16;
    dest->mlookturn = (epi::BAMFromATan(look_slope) - mo->vertangle) >> 16;

    if (cmd.speed != 0)
    {
        // get angle relative the player.
        BAMAngle a = cmd.direction - look_angle;

        float fwd  = epi::BAMCos(a) * cmd.speed;
        float side = epi::BAMSin(a) * cmd.speed;

        dest->forwardmove = (int)fwd;
        dest->sidemove    = -(int)side;
    }
}

void bot_t::Respawn()
{
    task = TASK_None;

    path_wait   = C_Random() % 8;
    look_time   = C_Random() % 8;
    weapon_time = C_Random() % 8;

    hit_obstacle = false;
    near_leader  = false;
    roam_goal    = position_c{0, 0, 0};

    DeletePath();
}

void bot_t::EndLevel()
{
    DeletePath();
}

//----------------------------------------------------------------------------

//
// Converts the player (which should be empty, i.e. neither a network
// or console player) to a bot.  Recreate is true for bot players
// loaded from a savegame.
//
void P_BotCreate(player_t *p, bool recreate)
{
    bot_t *bot = new bot_t;

    bot->pl = p;

    p->builder    = P_BotPlayerBuilder;
    p->build_data = (void *)bot;
    p->playerflags |= PFL_Bot;

    if (!recreate)
        sprintf(p->playername, "Bot%d", p->pnum + 1);
}

void P_BotPlayerBuilder(const player_t *p, void *data, ticcmd_t *cmd)
{
    memset(cmd, 0, sizeof(ticcmd_t));

    if (gamestate != GS_LEVEL)
        return;

    bot_t *bot = (bot_t *)data;
    SYS_ASSERT(bot);

    bot->Think();
    bot->ConvertTiccmd(cmd);
}

void BOT_BeginLevel(void)
{
    if (numbots > 0)
        NAV_AnalyseLevel();
}

//
// Done at level shutdown, right after all mobjs have been removed.
// Erases anything level specific from the bot structs.
//
void BOT_EndLevel(void)
{
    for (int i = 0; i < MAXPLAYERS; i++)
    {
        player_t *pl = players[i];
        if (pl != nullptr && pl->isBot())
        {
            bot_t *bot = (bot_t *)pl->build_data;
            SYS_ASSERT(bot);

            bot->EndLevel();
        }
    }

    NAV_FreeLevel();
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
