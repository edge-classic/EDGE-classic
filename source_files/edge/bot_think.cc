//----------------------------------------------------------------------------
//  EDGE: DeathBot s
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

#include "bot_think.h"

#include <string.h>

#include "AlmostEquals.h"
#include "bot_nav.h"
#include "con_main.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "g_game.h"
#include "m_bbox.h"
#include "m_random.h"
#include "p_action.h"
#include "p_local.h"
#include "p_weapon.h"
#include "r_misc.h"
#include "r_state.h"
#include "rad_trig.h"
#include "s_sound.h"
#include "w_wad.h"

#define DEBUG 0

// this ranges from 0 (VERY EASY) to 4 (VERY HARD)
EDGE_DEFINE_CONSOLE_VARIABLE(bot_skill, "2", kConsoleVariableFlagArchive)

#define MOVE_SPEED 20

//----------------------------------------------------------------------------
//  EVALUATING ITEMS, MONSTERS, WEAPONS
//----------------------------------------------------------------------------

bool DeathBot::HasWeapon(const WeaponDefinition *info) const
{
    for (int i = 0; i < MAXWEAPONS; i++)
        if (pl_->weapons[i].owned && pl_->weapons[i].info == info) return true;

    return false;
}

bool DeathBot::CanGetArmour(const Benefit *be, int extendedflags) const
{
    // this matches the logic in GiveArmour() in p_inter.cc

    ArmourType a_class = (ArmourType)be->sub.type;

    float amount = be->amount;

    if (extendedflags & kExtendedFlagSimpleArmour)
    {
        float slack = be->limit - pl_->armours[a_class];

        if (amount > slack) amount = slack;

        return (amount > 0);
    }

    float slack   = be->limit - pl_->totalarmour;
    float upgrade = 0;

    if (slack < 0) return false;

    for (int cl = a_class - 1; cl >= 0; cl--) upgrade += pl_->armours[cl];

    if (upgrade > amount) upgrade = amount;

    slack += upgrade;

    if (amount > slack) amount = slack;

    return !(AlmostEquals(amount, 0.0f) && AlmostEquals(upgrade, 0.0f));
}

bool DeathBot::MeleeWeapon() const
{
    int wp_num = pl_->ready_wp;

    if (pl_->pending_wp >= 0) wp_num = pl_->pending_wp;

    return pl_->weapons[wp_num].info->ammo_[0] == kAmmunitionTypeNoAmmo;
}

bool DeathBot::IsBarrel(const mobj_t *mo)
{
    if (mo->player) return false;

    if (0 == (mo->extendedflags & kExtendedFlagMonster)) return false;

    return true;
}

float DeathBot::EvalEnemy(const mobj_t *mo)
{
    // returns -1 to ignore, +1 to attack.
    // [ higher values are not possible, so no way to prioritize enemies ]

    // The following must be true to justify that you attack a target:
    // - target may not be yourself or your support obj.
    // - target must either want to attack you, or be on a different side
    // - target may not have the same supportobj as you.
    // - You must be able to see and shoot the target.

    if (0 == (mo->flags & kMapObjectFlagShootable) || mo->health <= 0)
        return -1;

    // occasionally shoot barrels
    if (IsBarrel(mo)) return (C_Random() % 100 < 20) ? +1 : -1;

    if (0 == (mo->extendedflags & kExtendedFlagMonster) && !mo->player)
        return -1;

    if (mo->player && mo->player == pl_) return -1;

    if (pl_->mo->supportobj == mo) return -1;

    if (!DEATHMATCH() && mo->player) return -1;

    if (!DEATHMATCH() && mo->supportobj && mo->supportobj->player) return -1;

    // EXTERMINATE !!

    return 1.0;
}

float DeathBot::EvalItem(const mobj_t *mo)
{
    // determine if an item is worth getting.
    // this depends on our current inventory, whether the game mode is COOP
    // or DEATHMATCH, and whether we are fighting or not.

    if (0 == (mo->flags & kMapObjectFlagSpecial)) return -1;

    bool fighting = (pl_->mo->target != nullptr);

    // do we *really* need some health?
    bool want_health = (pl_->mo->health < 90);
    bool need_health = (pl_->mo->health < 45);

    // handle weapons first (due to deathmatch rules)
    for (const Benefit *B = mo->info->pickup_benefits_; B != nullptr;
         B                = B->next)
    {
        if (B->type == kBenefitTypeWeapon)
        {
            if (!HasWeapon(B->sub.weap)) return BotNavigateEvaluateBigItem(mo);

            // try to get ammo from a dropped weapon
            if (mo->flags & kMapObjectFlagDropped) continue;

            // cannot get the ammo from a placed weapon except in altdeath
            if (deathmatch != 2) return -1;
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

    for (const Benefit *B = mo->info->pickup_benefits_; B != nullptr;
         B                = B->next)
    {
        switch (B->type)
        {
            case kBenefitTypeKey:
                // have it already?
                if (pl_->cards & (DoorKeyType)B->sub.type) continue;

                return 90;

            case kBenefitTypePowerup:
                return BotNavigateEvaluateBigItem(mo);

            case kBenefitTypeArmour:
                // ignore when fighting
                if (fighting) return -1;

                if (!CanGetArmour(B, mo->extendedflags)) continue;

                return BotNavigateEvaluateBigItem(mo);

            case kBenefitTypeHealth:
            {
                // cannot get it?
                if (pl_->health >= B->limit) return -1;

                // ignore potions unless really desperate
                if (B->amount < 2.5)
                {
                    if (pl_->health > 19) return -1;

                    return 2;
                }

                // don't grab health when fighting unless we NEED it
                if (!(need_health || (want_health && !fighting))) return -1;

                if (need_health)
                    return 120;
                else if (B->amount > 55)
                    return 40;
                else
                    return 30;
            }

            case kBenefitTypeAmmo:
            {
                if (B->sub.type == kAmmunitionTypeNoAmmo) continue;

                int ammo = B->sub.type;
                int max  = pl_->ammo[ammo].max;

                // in COOP mode, leave some ammo for others
                if (!DEATHMATCH()) max = max / 4;

                if (pl_->ammo[ammo].num >= max) continue;

                if (pl_->ammo[ammo].num == 0)
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

float DeathBot::EvaluateWeapon(int w_num, int &key) const
{
    // this evaluates weapons owned by the bot (NOT ones in the map).
    // returns -1 when not actually usable (e.g. no ammo).

    playerweapon_t *wp = &pl_->weapons[w_num];

    // don't have this weapon
    if (!wp->owned) return -1;

    WeaponDefinition *weapon = wp->info;
    SYS_ASSERT(weapon);

    AttackDefinition *attack = weapon->attack_[0];
    if (!attack) return -1;

    key = weapon->bind_key_;

    // have enough ammo?
    if (weapon->ammo_[0] != kAmmunitionTypeNoAmmo)
    {
        if (pl_->ammo[weapon->ammo_[0]].num < weapon->ammopershot_[0])
            return -1;
    }

    float score = 10.0f * weapon->priority_;

    // prefer smaller weapons for smaller monsters.
    // when not fighting, prefer biggest non-dangerous weapon.
    if (pl_->mo->target == nullptr || DEATHMATCH())
    {
        if (!weapon->dangerous_) score += 1000.0f;
    }
    else if (pl_->mo->target->spawnhealth > 250)
    {
        if (weapon->priority_ > 5) score += 1000.0f;
    }
    else
    {
        if (2 <= weapon->priority_ && weapon->priority_ <= 5) score += 1000.0f;
    }

    // small preference for the current weapon (break ties)
    if (w_num == pl_->ready_wp) score += 2.0f;

    // ultimate tie breaker (when two weapons have same priority)
    score += (float)w_num / 32.0f;

    return score;
}

//----------------------------------------------------------------------------

float DeathBot::DistTo(position_c pos) const
{
    float dx = fabs(pos.x - pl_->mo->x);
    float dy = fabs(pos.y - pl_->mo->y);

    return hypotf(dx, dy);
}

void DeathBot::PainResponse()
{
    // oneself?
    if (pl_->attacker == pl_->mo) return;

    // ignore friendly fire -- shit happens
    if (!DEATHMATCH() && pl_->attacker->player) return;

    if (pl_->attacker->health <= 0)
    {
        pl_->attacker = nullptr;
        return;
    }

    // TODO only update target if "threat" is greater than current target

    if (pl_->mo->target == nullptr)
    {
        if (IsEnemyVisible(pl_->attacker))
        {
            pl_->mo->SetTarget(pl_->attacker);
            UpdateEnemy();
            patience_ = kTicRate;
        }
    }
}

void DeathBot::LookForLeader()
{
    if (DEATHMATCH()) return;

    if (pl_->mo->supportobj != nullptr) return;

    for (int i = 0; i < MAXPLAYERS; i++)
    {
        player_t *p2 = players[i];

        if (p2 == nullptr || p2->isBot()) continue;

        // when multiple humans, make it random who is picked
        if (C_Random() % 100 < 90) continue;

        pl_->mo->SetSupportObj(p2->mo);
    }
}

bool DeathBot::IsEnemyVisible(mobj_t *enemy)
{
    float dx = enemy->x - pl_->mo->x;
    float dy = enemy->y - pl_->mo->y;
    float dz = enemy->z - pl_->mo->z;

    float slope = P_ApproxSlope(dx, dy, dz);

    // require slope to not be excessive, e.g. caged imps in MAP13
    if (slope > 1.0f) return false;

    return P_CheckSight(pl_->mo, enemy);
}

void DeathBot::LookForEnemies(float radius)
{
    // check sight of existing target
    if (pl_->mo->target != nullptr)
    {
        UpdateEnemy();

        if (see_enemy_)
        {
            patience_ = 2 * kTicRate;
            return;
        }

        // IDEA: if patience == kTicRate/2, try using pathing algo

        if (patience_-- >= 0) return;

        // look for a new enemy
        pl_->mo->SetTarget(nullptr);
    }

    // pick a random nearby monster, then check sight, since the enemy
    // may be on the other side of a wall.

    mobj_t *enemy = BotNavigateFindEnemy(this, radius);

    if (enemy != nullptr)
    {
        if (IsEnemyVisible(enemy))
        {
            pl_->mo->SetTarget(enemy);
            UpdateEnemy();
            patience_ = kTicRate;
        }
    }
}

void DeathBot::LookForItems(float radius)
{
    mobj_t  *item      = nullptr;
    BotPath *item_path = BotNavigateFindThing(this, radius, item);

    if (item_path == nullptr) return;

    // GET IT !!

    pl_->mo->SetTracer(item);

    DeletePath();

    task_      = kBotTaskGetItem;
    path_      = item_path;
    item_time_ = kTicRate;

    EstimateTravelTime();
}

void DeathBot::LookAround()
{
    look_time_--;

    LookForEnemies(1024);

    if ((look_time_ & 3) == 2) LookForLeader();

    if (look_time_ >= 0) return;

    // look for items every second or so
    look_time_ = 20 + C_Random() % 20;

    LookForItems(1024);
}

void DeathBot::SelectWeapon()
{
    // reconsider every second or so
    weapon_time_ = 20 + C_Random() % 20;

    // allow any weapon change to complete first
    if (pl_->pending_wp != WPSEL_NoChange) return;

    int   best       = pl_->ready_wp;
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

    if (best != pl_->ready_wp) { cmd_.weapon = best_key; }
}

void DeathBot::MoveToward(const position_c &pos)
{
    cmd_.speed     = MOVE_SPEED + (6.25 * bot_skill.d_);
    cmd_.direction = R_PointToAngle(pl_->mo->x, pl_->mo->y, pos.x, pos.y);
}

void DeathBot::WalkToward(const position_c &pos)
{
    cmd_.speed     = (MOVE_SPEED + (3.125 * bot_skill.d_));
    cmd_.direction = R_PointToAngle(pl_->mo->x, pl_->mo->y, pos.x, pos.y);
}

void DeathBot::TurnToward(BAMAngle want_angle, float want_slope, bool fast)
{
    // horizontal (yaw) angle
    BAMAngle delta = want_angle - pl_->mo->angle;

    if (delta < kBAMAngle180)
        delta = delta / (fast ? 3 : 8);
    else
        delta = kBAMAngle360 - (kBAMAngle360 - delta) / (fast ? 3 : 8);

    look_angle_ = pl_->mo->angle + delta;

    // vertical (pitch or mlook) angle
    if (want_slope < -2.0) want_slope = -2.0;
    if (want_slope > +2.0) want_slope = +2.0;

    float diff = want_slope - epi::BAMTan(pl_->mo->vertangle);

    if (fabs(diff) < (fast ? (0.04 + (0.02 * bot_skill.f_)) : 0.04))
        look_slope_ = want_slope;
    else if (diff < 0)
        look_slope_ -= fast ? (0.03 + (0.015 * bot_skill.f_)) : 0.03;
    else
        look_slope_ += fast ? (0.03 + (0.015 * bot_skill.f_)) : 0.03;
}

void DeathBot::TurnToward(const mobj_t *mo, bool fast)
{
    float dx = mo->x - pl_->mo->x;
    float dy = mo->y - pl_->mo->y;
    float dz = mo->z - pl_->mo->z;

    BAMAngle want_angle = R_PointToAngle(0, 0, dx, dy);
    float    want_slope = P_ApproxSlope(dx, dy, dz);

    TurnToward(want_angle, want_slope, fast);
}

void DeathBot::WeaveToward(const position_c &pos)
{
    // usually try to move directly toward a wanted position.
    // but if something gets in our way, we try to "weave" around it,
    // by sometimes going diagonally left and sometimes right.

    float dist = DistTo(pos);

    if (weave_time_-- < 0)
    {
        weave_time_ = 10 + C_Random() % 10;

        bool neg = weave_ < 0;

        if (hit_obstacle_)
            weave_ = neg ? +2 : -2;
        else if (dist > 192.0)
            weave_ = neg ? +1 : -1;
        else
            weave_ = 0;
    }

    MoveToward(pos);

    if (weave_ == -2) cmd_.direction -= kBAMAngle5 * 12;
    if (weave_ == -1) cmd_.direction -= kBAMAngle5 * 3;
    if (weave_ == +1) cmd_.direction += kBAMAngle5 * 3;
    if (weave_ == +2) cmd_.direction += kBAMAngle5 * 12;
}

void DeathBot::WeaveToward(const mobj_t *mo)
{
    position_c pos{mo->x, mo->y, mo->z};

    WeaveToward(pos);
}

void DeathBot::RetreatFrom(const mobj_t *enemy)
{
    float dx   = pl_->mo->x - enemy->x;
    float dy   = pl_->mo->y - enemy->y;
    float dlen = HMM_MAX(hypotf(dx, dy), 1.0f);

    position_c pos{pl_->mo->x, pl_->mo->y, pl_->mo->z};

    pos.x += 16.0f * (dx / dlen);
    pos.y += 16.0f * (dy / dlen);

    WeaveToward(pos);
}

void DeathBot::Strafe(bool right)
{
    cmd_.speed     = MOVE_SPEED + (6.25 * bot_skill.d_);
    cmd_.direction = pl_->mo->angle + (right ? kBAMAngle270 : kBAMAngle90);
}

void DeathBot::DetectObstacle()
{
    mobj_t *mo = pl_->mo;

    float dx = last_x_ - mo->x;
    float dy = last_y_ - mo->y;

    last_x_ = mo->x;
    last_y_ = mo->y;

    hit_obstacle_ = (dx * dx + dy * dy) < 0.2;
}

void DeathBot::Meander()
{
    // TODO wander about without falling into nukage pits (etc)
}

void DeathBot::UpdateEnemy()
{
    mobj_t *enemy = pl_->mo->target;

    // update angle, slope and distance, even if not seen
    position_c pos = {enemy->x, enemy->y, enemy->z};

    float dx = enemy->x - pl_->mo->x;
    float dy = enemy->y - pl_->mo->y;
    float dz = enemy->z - pl_->mo->z;

    enemy_angle_ = R_PointToAngle(0, 0, dx, dy);
    enemy_slope_ = P_ApproxSlope(dx, dy, dz);
    enemy_dist_  = DistTo(pos);

    // can see them?
    see_enemy_ = IsEnemyVisible(enemy);
}

void DeathBot::StrafeAroundEnemy()
{
    if (strafe_time_-- < 0)
    {
        // pick a random strafe direction.
        // it will often be the same as before, that is okay.
        int r = C_Random();

        if ((r & 3) == 0)
            strafe_dir_ = 0;
        else
            strafe_dir_ = (r & 16) ? -1 : +1;

        uint8_t wait = 60 - (bot_skill.d_* 10);

        strafe_time_ = wait + r % wait;
        return;
    }

    if (strafe_dir_ != 0) { Strafe(strafe_dir_ > 0); }
}

void DeathBot::ShootTarget()
{
    // no weapon to shoot?
    if (pl_->ready_wp == WPSEL_None || pl_->pending_wp != WPSEL_NoChange)
        return;

    // TODO: ammo check

    // too far away?
    if (enemy_dist_ > 2000) return;

    // too close for a dangerous weapon?
    const WeaponDefinition *weapon = pl_->weapons[pl_->ready_wp].info;
    if (weapon->dangerous_ && enemy_dist_ < 208) return;

    // check that we are facing the enemy
    BAMAngle delta   = enemy_angle_ - pl_->mo->angle;
    float    sl_diff = fabs(enemy_slope_ - epi::BAMTan(pl_->mo->vertangle));

    if (delta > kBAMAngle180) delta = kBAMAngle360 - delta;

    // the further away we are, the more accurate our shot must be.
    // e.g. at point-blank range, even 45 degrees away can hit.
    float acc_dist = HMM_MAX(enemy_dist_, 32.0f);
    float adjust   = acc_dist / 32.0f;

    if (delta > (BAMAngle)(kBAMAngle90 / adjust / (11 - (2.5 * bot_skill.d_))))
        return;

    if (sl_diff > (8.0f / adjust)) return;

    // in COOP, check if other players might be hit
    if (!DEATHMATCH())
    {
        // TODO
    }

    cmd_.attack = true;
}

void DeathBot::ThinkFight()
{
    // Note: LookAround() has done sight-checking of our target

    // face our foe
    TurnToward(enemy_angle_, enemy_slope_, true);

    const mobj_t *enemy = pl_->mo->target;

    // if lost sight, weave towards the target
    if (!see_enemy_)
    {
        // IDEA: check if a LOS exists in a position to our left or right.
        //       if it does, the strafe purely left/right.
        //       [ do it in ThinkHelp too, assuming it works ]

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
    float dz = fabs(pl_->mo->z - enemy->z);

    float min_dist = HMM_MIN(dz * 2.0f, 480.0f);
    float max_dist = 640.0f;

    // handle dangerous weapons
    const WeaponDefinition *weapon = pl_->weapons[pl_->ready_wp].info;

    if (weapon->dangerous_) min_dist = HMM_MAX(min_dist, 224.0f);

    // approach if too far away
    if (enemy_dist_ > max_dist)
    {
        WeaveToward(enemy);
        return;
    }

    // retreat if too close
    if (enemy_dist_ < min_dist)
    {
        RetreatFrom(enemy);
        return;
    }

    StrafeAroundEnemy();
}

void DeathBot::WeaveNearLeader(const mobj_t *leader)
{
    // pick a position some distance away, so that a human player
    // can get out of a narrow item closet (etc).

    float dx = pl_->mo->x - leader->x;
    float dy = pl_->mo->y - leader->y;

    float dlen = HMM_MAX(1.0f, hypotf(dx, dy));

    dx = dx * 96.0f / dlen;
    dy = dy * 96.0f / dlen;

    position_c pos{leader->x + dx, leader->y + dy, leader->z};

    TurnToward(leader, false);
    WeaveToward(pos);
}

void DeathBot::PathToLeader()
{
    mobj_t *leader = pl_->mo->supportobj;
    SYS_ASSERT(leader);

    DeletePath();

    path_ = BotNavigateFindPath(pl_->mo, leader, 0);

    if (path_ != nullptr) { EstimateTravelTime(); }
}

void DeathBot::EstimateTravelTime()
{
    // estimate time to travel one segment of a path.
    // overestimates by quite a bit, to account for obstacles.

    float dist = DistTo(path_->CurrentDestination());
    float tics = dist * 1.5f / 10.0f + 6.0f * kTicRate;

    travel_time_ = (int)tics;
}

void DeathBot::ThinkHelp()
{
    mobj_t *leader = pl_->mo->supportobj;

    // check if we are close to the leader, and can see them
    bool cur_near = false;

    position_c pos  = {leader->x, leader->y, leader->z};
    float      dist = DistTo(pos);

    // allow a bit of "hysteresis"
    float check_dist = near_leader_ ? 224.0 : 160.0;

    if (dist < check_dist && fabs(pl_->mo->z - pos.z) <= 24.0)
    {
        cur_near = P_CheckSight(pl_->mo, leader);
    }

    if (near_leader_ != cur_near)
    {
        near_leader_ = cur_near;

        DeletePath();

        if (!cur_near)
        {
            // wait a bit then find a path
            path_wait_ = 10 + C_Random() % 10;
        }
    }

    if (cur_near)
    {
        WeaveNearLeader(leader);
        return;
    }

    if (path_ != nullptr)
    {
        switch (FollowPath(true))
        {
            case kBotFollowPathResultOK:
                return;

            case kBotFollowPathResultDone:
                DeletePath();
                path_wait_ = 4 + C_Random() % 4;
                break;

            case kBotFollowPathResultFailed:
                DeletePath();
                path_wait_ = 30 + C_Random() % 10;
                break;
        }
    }

    // we are waiting until we can establish a path

    if (path_wait_-- < 0)
    {
        PathToLeader();
        path_wait_ = 30 + C_Random() % 10;
    }

    // if somewhat close, attempt to follow player
    if (dist < 512.0 && fabs(pl_->mo->z - pos.z) <= 24.0)
        WeaveNearLeader(leader);
    else
        Meander();
}

BotFollowPathResult DeathBot::FollowPath(bool do_look)
{
    // returns a kBotFollowPathResultXXX enum constant.

    SYS_ASSERT(path_ != nullptr);
    SYS_ASSERT(!path_->Finished());

    // handle doors and lifts
    int flags = path_->nodes_[path_->along_].flags;

    if (flags & kBotPathNodeDoor)
    {
        task_       = kBotTaskOpenDoor;
        door_stage_ = kBotOpenDoorTaskApproach;
        door_seg_   = path_->nodes_[path_->along_].seg;
        door_time_  = 5 * kTicRate;
        SYS_ASSERT(door_seg_ != nullptr);
        return kBotFollowPathResultOK;
    }
    else if (flags & kBotPathNodeLift)
    {
        task_       = kBotTaskUseLift;
        lift_stage_ = kBotUseLiftTaskApproach;
        lift_seg_   = path_->nodes_[path_->along_].seg;
        lift_time_  = 5 * kTicRate;
        SYS_ASSERT(lift_seg_ != nullptr);
        return kBotFollowPathResultOK;
    }
    else if (flags & kBotPathNodeLift)
    {
        // TODO: kBotTaskTelport which attempts not to telefrag / be telefragged
    }

    // have we reached the next node?
    if (path_->ReachedDestination(pl_->mo))
    {
        path_->along_ += 1;

        if (path_->Finished()) { return kBotFollowPathResultDone; }

        EstimateTravelTime();
    }

    if (travel_time_-- < 0) { return kBotFollowPathResultFailed; }

    // determine looking angle
    if (do_look)
    {
        position_c dest = path_->CurrentDestination();

        if (path_->along_ + 1 < path_->nodes_.size())
            dest = path_->nodes_[path_->along_ + 1].pos;

        float dx = dest.x - pl_->mo->x;
        float dy = dest.y - pl_->mo->y;
        float dz = dest.z - pl_->mo->z;

        BAMAngle want_angle = R_PointToAngle(0, 0, dx, dy);
        float    want_slope = P_ApproxSlope(dx, dy, dz);

        TurnToward(want_angle, want_slope, false);
    }

    WeaveToward(path_->CurrentDestination());

    return kBotFollowPathResultOK;
}

void DeathBot::ThinkRoam()
{
    if (path_ != nullptr)
    {
        switch (FollowPath(true))
        {
            case kBotFollowPathResultOK:
                return;

            case kBotFollowPathResultDone:
                // arrived at the spot!
                // TODO look for other nearby items

                DeletePath();
                path_wait_ = 4 + C_Random() % 4;
                break;

            case kBotFollowPathResultFailed:
                DeletePath();
                path_wait_ = 30 + C_Random() % 10;
                break;
        }
    }

    if (path_wait_-- < 0)
    {
        path_wait_ = 30 + C_Random() % 10;

        if (!BotNavigateNextRoamPoint(roam_goal_))
        {
            roam_goal_ = position_c{0, 0, 0};
            return;
        }

        path_ = BotNavigateFindPath(pl_->mo, &roam_goal_, 0);

        // if no path found, try again soon
        if (path_ == nullptr)
        {
            roam_goal_ = position_c{0, 0, 0};
            return;
        }

        EstimateTravelTime();
    }

    Meander();
}

void DeathBot::FinishGetItem()
{
    task_ = kBotTaskNone;
    pl_->mo->SetTracer(nullptr);

    DeletePath();
    path_wait_ = 4 + C_Random() % 4;

    // when fighting, look furthe for more items
    if (pl_->mo->target != nullptr)
    {
        LookForItems(1024);
        return;
    }

    // otherwise collect nearby items
    LookForItems(256);

    if (task_ == kBotTaskGetItem) return;

    // continue to follow player
    if (pl_->mo->supportobj != nullptr) return;

    // otherwise we were roaming about, so re-establish path
    if (!(AlmostEquals(roam_goal_.x, 0.0f) &&
          AlmostEquals(roam_goal_.y, 0.0f) && AlmostEquals(roam_goal_.z, 0.0f)))
    {
        path_ = BotNavigateFindPath(pl_->mo, &roam_goal_, 0);

        // if no path found, try again soon
        if (path_ == nullptr)
        {
            roam_goal_ = position_c{0, 0, 0};
            return;
        }

        EstimateTravelTime();
    }
}

void DeathBot::ThinkGetItem()
{
    // item gone?  (either we picked it up, or someone else did)
    if (pl_->mo->tracer == nullptr)
    {
        FinishGetItem();
        return;
    }

    // if we are being chased, look at them, shoot sometimes
    if (pl_->mo->target)
    {
        UpdateEnemy();

        TurnToward(enemy_angle_, enemy_slope_, false);

        if (see_enemy_) ShootTarget();
    }
    else { TurnToward(pl_->mo->tracer, false); }

    // follow the path previously found
    if (path_ != nullptr)
    {
        switch (FollowPath(false))
        {
            case kBotFollowPathResultOK:
                return;

            case kBotFollowPathResultDone:
                DeletePath();
                item_time_ = kTicRate;
                break;

            case kBotFollowPathResultFailed:
                // took too long? (e.g. we got stuck)
                FinishGetItem();
                return;
        }
    }

    // detect not picking up the item
    if (item_time_-- < 0)
    {
        FinishGetItem();
        return;
    }

    // move toward the item's location
    WeaveToward(pl_->mo->tracer);
}

void DeathBot::FinishDoorOrLift(bool ok)
{
    task_ = kBotTaskNone;

    if (ok) { path_->along_ += 1; }
    else
    {
        DeletePath();
        roam_goal_ = position_c{0, 0, 0};
    }
}

void DeathBot::ThinkOpenDoor()
{
    switch (door_stage_)
    {
        case kBotOpenDoorTaskApproach:
        {
            if (door_time_-- < 0)
            {
                FinishDoorOrLift(false);
                return;
            }

            float    dist = DistTo(path_->CurrentDestination());
            BAMAngle ang =
                path_->nodes_[path_->along_].seg->angle + kBAMAngle90;
            BAMAngle diff = ang - pl_->mo->angle;

            if (diff > kBAMAngle180) diff = kBAMAngle360 - diff;

            if (diff < kBAMAngle5 && dist < (USERANGE - 16))
            {
                door_stage_ = kBotOpenDoorTaskUse;
                door_time_  = kTicRate * 5;
                return;
            }

            TurnToward(ang, 0.0, false);
            WeaveToward(path_->CurrentDestination());
            return;
        }

        case kBotOpenDoorTaskUse:
        {
            if (door_time_-- < 0)
            {
                FinishDoorOrLift(false);
                return;
            }

            // if closing, try to re-open
            const sector_t     *sector = door_seg_->back_sub->sector;
            const plane_move_t *pm     = sector->ceil_move;

            if (pm != nullptr && pm->direction < 0)
            {
                if (door_time_ & 1) cmd_.use = true;
                return;
            }

            // already open?
            if (sector->c_h > sector->f_h + 56.0f)
            {
                FinishDoorOrLift(true);
                return;
            }

            // door is opening, so don't interfere
            if (pm != nullptr) return;

            if (door_time_ & 1) cmd_.use = true;

            return;
        }
    }
}

void DeathBot::ThinkUseLift()
{
    switch (lift_stage_)
    {
        case kBotOpenDoorTaskApproach:
        {
            if (lift_time_-- < 0)
            {
                FinishDoorOrLift(false);
                return;
            }

            float    dist = DistTo(path_->CurrentDestination());
            BAMAngle ang =
                path_->nodes_[path_->along_].seg->angle + kBAMAngle90;
            BAMAngle diff = ang - pl_->mo->angle;

            if (diff > kBAMAngle180) diff = kBAMAngle360 - diff;

            if (diff < kBAMAngle5 && dist < (USERANGE - 16))
            {
                lift_stage_ = kBotUseLiftTaskUse;
                lift_time_  = kTicRate * 5;
                return;
            }

            TurnToward(ang, 0.0, false);
            WeaveToward(path_->CurrentDestination());
            return;
        }

        case kBotUseLiftTaskUse:
        {
            if (lift_time_-- < 0)
            {
                FinishDoorOrLift(false);
                return;
            }

            // if lift is raising, try to re-lower
            const sector_t     *sector = lift_seg_->back_sub->sector;
            const plane_move_t *pm     = sector->floor_move;

            if (pm != nullptr && pm->direction > 0)
            {
                if (lift_time_ & 1) cmd_.use = true;
                return;
            }

            // already lowered?
            if (sector->f_h < lift_seg_->front_sub->sector->f_h + 24.0f)
            {
                // navigation code added a place to stand
                path_->along_ += 1;

                // TODO compute time it will take for lift to go fully up
                lift_stage_ = kBotUseLiftTaskRide;
                lift_time_  = kTicRate * 10;
                return;
            }

            // lift is lowering, so don't interfere
            if (pm != nullptr) return;

            // try to activate it
            if (lift_time_ & 1) cmd_.use = true;

            return;
        }

        case kBotUseLiftTaskRide:
            if (lift_time_-- < 0)
            {
                FinishDoorOrLift(false);
                return;
            }

            WalkToward(path_->CurrentDestination());

            const sector_t *lift_sec = lift_seg_->back_sub->sector;

            if (lift_sec->floor_move != nullptr)
            {
                // if lift went down again, don't time out
                if (lift_sec->floor_move->direction <= 0)
                    lift_time_ = 10 * kTicRate;

                return;
            }

            // reached the top?
            bool ok = pl_->mo->z > (lift_sec->f_h - 0.5);

            FinishDoorOrLift(ok);
            return;
    }
}

void DeathBot::DeletePath()
{
    if (path_ != nullptr)
    {
        delete path_;
        path_ = nullptr;
    }
}

//----------------------------------------------------------------------------

void DeathBot::Think()
{
    SYS_ASSERT(pl_ != nullptr);
    SYS_ASSERT(pl_->mo != nullptr);

    // initialize the BotCommand
    memset(&cmd_, 0, sizeof(BotCommand));
    cmd_.weapon = -1;

    // do nothing when game is paused
    if (paused) return;

    mobj_t *mo = pl_->mo;

    // dead?
    if (mo->health <= 0)
    {
        DeathThink();
        return;
    }

    // forget target (etc) if they died
    if (mo->target && mo->target->health <= 0) mo->SetTarget(nullptr);

    if (mo->supportobj && mo->supportobj->health <= 0)
        mo->SetSupportObj(nullptr);

    // hurt by somebody?
    if (pl_->attacker != nullptr) { PainResponse(); }

    DetectObstacle();

    // doing a task?
    switch (task_)
    {
        case kBotTaskGetItem:
            ThinkGetItem();
            return;

        case kBotTaskOpenDoor:
            ThinkOpenDoor();
            return;

        case kBotTaskUseLift:
            ThinkUseLift();
            return;

        default:
            break;
    }

    LookAround();

    if (weapon_time_-- < 0) SelectWeapon();

    // if we have a target enemy, fight it or flee it
    if (pl_->mo->target != nullptr)
    {
        ThinkFight();
        return;
    }

    // if we have a leader (in co-op), follow them
    if (pl_->mo->supportobj != nullptr)
    {
        ThinkHelp();
        return;
    }

    // in deathmatch, go to the roaming goal.
    // otherwise just meander around.

    ThinkRoam();
}

void DeathBot::DeathThink()
{
    dead_time_++;

    // respawn after a random interval, at least one second
    if (dead_time_ > 30)
    {
        dead_time_ = 0;

        if (C_Random() % 100 < 35) cmd_.use = true;
    }
}

void DeathBot::ConvertTiccmd(EventTicCommand *dest)
{
    // we assume caller has cleared the ticcmd_t to zero.

    mobj_t *mo = pl_->mo;

    if (cmd_.attack) dest->buttons |= kButtonCodeAttack;

    if (cmd_.attack2) dest->extended_buttons |= kExtendedButtonCodeSecondAttack;

    if (cmd_.use) dest->buttons |= kButtonCodeUse;

    if (cmd_.jump) dest->upward_move = 0x20;

    if (cmd_.weapon != -1)
    {
        dest->buttons |= kButtonCodeChangeWeapon;
        dest->buttons |= (cmd_.weapon << kButtonCodeWeaponMaskShift) & kButtonCodeWeaponMask;
    }

    dest->player_index = pl_->pnum;

    dest->angle_turn = (mo->angle - look_angle_) >> 16;
    dest->mouselook_turn = (epi::BAMFromATan(look_slope_) - mo->vertangle) >> 16;

    if (cmd_.speed != 0)
    {
        // get angle relative the player.
        BAMAngle a = cmd_.direction - look_angle_;

        float fwd  = epi::BAMCos(a) * cmd_.speed;
        float side = epi::BAMSin(a) * cmd_.speed;

        dest->forward_move = (int)fwd;
        dest->side_move    = -(int)side;
    }
}

void DeathBot::Respawn()
{
    task_ = kBotTaskNone;

    path_wait_   = C_Random() % 8;
    look_time_   = C_Random() % 8;
    weapon_time_ = C_Random() % 8;

    hit_obstacle_ = false;
    near_leader_  = false;
    roam_goal_    = position_c{0, 0, 0};

    DeletePath();
}

void DeathBot::EndLevel() { DeletePath(); }

//----------------------------------------------------------------------------

//
// Converts the player (which should be empty, i.e. neither a network
// or console player) to a bot.  Recreate is true for bot players
// loaded from a savegame.
//
void P_BotCreate(player_t *p, bool recreate)
{
    DeathBot *bot = new DeathBot;

    bot->pl_ = p;

    p->builder    = P_BotPlayerBuilder;
    p->build_data = (void *)bot;
    p->playerflags |= PFL_Bot;

    if (!recreate) sprintf(p->playername, "Bot%d", p->pnum + 1);
}

void P_BotPlayerBuilder(const player_t *p, void *data, EventTicCommand *cmd)
{
    memset(cmd, 0, sizeof(EventTicCommand));

    if (gamestate != GS_LEVEL) return;

    DeathBot *bot = (DeathBot *)data;
    SYS_ASSERT(bot);

    bot->Think();
    bot->ConvertTiccmd(cmd);
}

void BotBeginLevel(void)
{
    if (numbots > 0) BotNavigateAnalyseLevel();
}

//
// Done at level shutdown, right after all mobjs have been removed.
// Erases anything level specific from the bot structs.
//
void BotEndLevel(void)
{
    for (int i = 0; i < MAXPLAYERS; i++)
    {
        player_t *pl = players[i];
        if (pl != nullptr && pl->isBot())
        {
            DeathBot *bot = (DeathBot *)pl->build_data;
            SYS_ASSERT(bot);

            bot->EndLevel();
        }
    }

    BotNavigateFreeLevel();
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
