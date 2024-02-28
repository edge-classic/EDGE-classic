//----------------------------------------------------------------------------
//  EDGE Navigation System
//----------------------------------------------------------------------------
//
//  Copyright (c) 2022-2024 The EDGE Team.
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

#include "bot_nav.h"

#include <algorithm>
#include <forward_list>

#include "AlmostEquals.h"
#include "bot_think.h"
#include "con_main.h"
#include "dm_data.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "m_bbox.h"
#include "m_random.h"
#include "main.h"
#include "p_local.h"
#include "p_mobj.h"
#include "r_bsp.h"
#include "r_defs.h"
#include "r_misc.h"
#include "r_state.h"
#include "thing.h"

extern MapObject *P_FindTeleportMan(int tag, const MapObjectDefinition *info);
extern line_t *p_FindTeleportLine(int tag, line_t *original);

class big_item_c
{
   public:
    float x     = 0;
    float y     = 0;
    float z     = 0;
    float score = 0;
};

static std::vector<big_item_c> big_items;

float BotNavigateEvaluateBigItem(const MapObject *mo)
{
    AmmunitionType ammotype;

    for (const Benefit *B = mo->info_->pickup_benefits_; B != nullptr;
         B                = B->next)
    {
        switch (B->type)
        {
            case kBenefitTypeWeapon:
                // crude guess of powerfulness based on ammo
                ammotype = B->sub.weap->ammo_[0];

                switch (ammotype)
                {
                    case kAmmunitionTypeNoAmmo:
                        return 25;
                    case kAmmunitionTypeBullet:
                        return 50;
                    case kAmmunitionTypeShell:
                        return 60;
                    case kAmmunitionTypeRocket:
                        return 70;
                    case kAmmunitionTypeCell:
                        return 80;
                    default:
                        return 65;
                }
                break;

            case kBenefitTypePowerup:
                // invisibility is not here, since in COOP it makes monster
                // projectiles harder to dodge, and powerups are rare in DM.
                // hence for bots, only invulnerability is actually useful.
                switch (B->sub.type)
                {
                    case kPowerTypeInvulnerable:
                        return 100;
                    default:
                        return -1;
                }
                break;

            case kBenefitTypeAmmo:
                // ignored here
                break;

            case kBenefitTypeHealth:
                // ignore small amounts (e.g. potions, stimpacks)
                if (B->amount >= 100) return 40;
                break;

            case kBenefitTypeArmour:
                // ignore small amounts (e.g. helmets)
                if (B->amount >= 50) return 20;
                break;

            default:
                break;
        }
    }

    return -1;
}

static void BotNavigateCollectBigItems()
{
    // collect the location of all the significant pickups on the map.
    // the main purpose of this is allowing the bots to roam, since big
    // items (e.g. weapons) tend to be well distributed around a map.
    // it will also be useful for finding a weapon after spawning.

    for (const MapObject *mo = map_object_list_head; mo != nullptr; mo = mo->next_)
    {
        if ((mo->flags_ & kMapObjectFlagSpecial) == 0) continue;

        float score = BotNavigateEvaluateBigItem(mo);
        if (score < 0) continue;

        big_items.push_back(big_item_c{mo->x, mo->y, mo->z, score});
    }

    // TODO : if < 4, pad out with DM spawn spots or random locs
}

bool BotNavigateNextRoamPoint(Position &out)
{
    if (big_items.empty()) return false;

    for (int loop = 0; loop < 100; loop++)
    {
        int idx = RandomShort() % (int)big_items.size();

        const big_item_c &item = big_items[idx];

        float dx = fabs(item.x - out.x);
        float dy = fabs(item.y - out.y);

        // too close to last goal?
        if (dx < 200 && dy < 200) continue;

        out.x = item.x;
        out.y = item.y;
        out.z = item.z;

        return true;
    }

    return false;
}

//----------------------------------------------------------------------------
//  A* PATHING ALGORITHM
//----------------------------------------------------------------------------

// NOTE: for the A* algorithm, we use *time* in seconds for measuring
//       the cost of travelling between two nodes.

// player travel speed when running, in map units per second.
#define RUNNING_SPEED 450.0f

class nav_area_c
{
   public:
    int id         = 0;
    int first_link = 0;
    int num_links  = 0;

    // middle coordinate
    float mid_x;
    float mid_y;

    // info for A* path finding...

    bool  open   = false;  // in the OPEN set?
    int   parent = -1;     // parent nav_area_c / subsector_t
    float G      = 0;      // cost of this node (from start node)
    float H      = 0;      // estimated cost to reach end node

    nav_area_c(int _id) : id(_id) {}

    ~nav_area_c() {}

    Position get_middle() const;

    void compute_middle(const subsector_t &sub);
};

class nav_link_c
{
   public:
    int          dest_id = -1;
    float        length  = 0;
    int          flags   = kBotPathNodeNormal;
    const seg_t *seg     = nullptr;
};

// there is a one-to-one correspondence from a subsector_t to a
// nav_area_c in this vector.
static std::vector<nav_area_c> nav_areas;
static std::vector<nav_link_c> nav_links;

static Position nav_finish_mid;

Position nav_area_c::get_middle() const
{
    float z = level_subsectors[id].sector->f_h;

    return Position{mid_x, mid_y, z};
}

void nav_area_c::compute_middle(const subsector_t &sub)
{
    double sum_x = 0;
    double sum_y = 0;

    int total = 0;

    for (const seg_t *seg = sub.segs; seg != nullptr;
         seg              = seg->sub_next, total += 1)
    {
        sum_x += seg->v1->X;
        sum_y += seg->v1->Y;
    }

    if (total == 0) total = 1;

    mid_x = sum_x / total;
    mid_y = sum_y / total;
}

static int BotNavigateCheckDoorOrLift(const seg_t *seg)
{
    if (seg->miniseg) return kBotPathNodeNormal;

    const line_t *ld = seg->linedef;

    if (ld->special == nullptr) return kBotPathNodeNormal;

    const LineType *spec = ld->special;

    if (spec->type_ == kLineTriggerManual)
    {
        // ok
    }
    else if (spec->type_ == kLineTriggerPushable)
    {
        // require tag to match the back sector
        if (ld->tag <= 0) return kBotPathNodeNormal;

        if (seg->back_sub->sector->tag != ld->tag) return kBotPathNodeNormal;
    }
    else
    {
        // we don't support shootable doors
        return kBotPathNodeNormal;
    }

    // don't open single-use doors in COOP -- a human should do it
    if (!DEATHMATCH() && spec->count_ > 0) return kBotPathNodeNormal;

    if (spec->c_.type_ == kPlaneMoverOnce ||
        spec->c_.type_ == kPlaneMoverMoveWaitReturn)
    {
        // determine "front" of door by ceiling heights
        if (seg->back_sub->sector->c_h >= seg->front_sub->sector->c_h)
            return kBotPathNodeNormal;

        // ignore locked doors in COOP, since bots don't puzzle solve (yet)
        if (!DEATHMATCH() && spec->keys_ != kDoorKeyNone)
            return kBotPathNodeNormal;

        return kBotPathNodeDoor;
    }

    if (spec->f_.type_ == kPlaneMoverOnce ||
        spec->f_.type_ == kPlaneMoverMoveWaitReturn ||
        spec->f_.type_ == kPlaneMoverPlatform ||
        spec->f_.type_ == kPlaneMoverElevator)
    {
        // determine "front" of lift by floor heights
        if (seg->back_sub->sector->f_h <= seg->front_sub->sector->f_h)
            return kBotPathNodeNormal;

        return kBotPathNodeLift;
    }

    return kBotPathNodeNormal;
}

static int BotNavigateCheckTeleporter(const seg_t *seg)
{
    // returns # of destination subsector, or -1 if not a teleporter.
    // TODO: we don't support line-to-line teleporters yet...

    if (seg->miniseg) return -1;

    // teleporters only work on front of a linedef
    if (seg->side != 0) return -1;

    const line_t *ld = seg->linedef;

    if (ld->special == nullptr) return -1;

    const LineType *spec = ld->special;

    if (spec->type_ != kLineTriggerWalkable) return -1;

    if (!spec->t_.teleport_) return -1;

    // ignore a single-use teleporter
    if (spec->count_ > 0) return -1;

    if (ld->tag <= 0) return -1;

    if (spec->t_.special_ & kTeleportSpecialLine) return -1;

    // find the destination thing...
    if (spec->t_.outspawnobj_ == nullptr) return -1;

    const MapObject *dest = P_FindTeleportMan(ld->tag, spec->t_.outspawnobj_);
    if (dest == nullptr) return -1;

    return (int)(dest->subsector_ - level_subsectors);
}

static void BotNavigateCreateLinks()
{
    for (int i = 0; i < total_level_subsectors; i++)
    {
        nav_areas.push_back(nav_area_c(i));

        nav_areas.back().compute_middle(level_subsectors[i]);
    }

    for (int i = 0; i < total_level_subsectors; i++)
    {
        const subsector_t &sub = level_subsectors[i];

        nav_area_c &area = nav_areas[i];
        area.first_link  = (int)nav_links.size();

        for (const seg_t *seg = sub.segs; seg != nullptr; seg = seg->sub_next)
        {
            // no link for a one-sided wall
            if (seg->back_sub == nullptr) continue;

            int dest_id = (int)(seg->back_sub - level_subsectors);

            // ignore player-blocking lines
            if (!seg->miniseg)
                if (0 !=
                    (seg->linedef->flags & (MLF_Blocking | MLF_BlockPlayers)))
                    continue;

            // NOTE: a big height difference is allowed here, it is checked
            //       during play (since we need to allow lowering floors etc).

            // WISH: check if link is blocked by obstacle things

            // compute length of link
            auto p1 = area.get_middle();
            auto p2 = nav_areas[dest_id].get_middle();

            float length = R_PointToDist(p1.x, p1.y, p2.x, p2.y);

            // determine if a manual door, a lift, or a teleporter
            int flags   = BotNavigateCheckDoorOrLift(seg);
            int tele_id = BotNavigateCheckTeleporter(seg);

            if (tele_id >= 0)
                nav_links.push_back(
                    nav_link_c{tele_id, length, kBotPathNodeTeleport, seg});
            else
                nav_links.push_back(nav_link_c{dest_id, length, flags, seg});

            area.num_links += 1;

            // DEBUG
            //  fprintf(stderr, "link area %d --> %d\n", area.id, dest_id);
        }
    }
}

static float BotNavigateTraverseLinkCost(int cur, const nav_link_c &link,
                                         bool allow_doors)
{
    const sector_t *s1 = level_subsectors[cur].sector;
    const sector_t *s2 = level_subsectors[link.dest_id].sector;

    float time   = link.length / RUNNING_SPEED;
    float f_diff = s2->f_h - s1->f_h;

    // special check for teleport heights (dest_id is far away)
    if (link.flags & kBotPathNodeTeleport)
    {
        const sector_t *s3 = link.seg->back_sub->sector;

        if (s3->f_h > s1->f_h + 24.0f) return -1;

        if (s3->c_h < s3->f_h + 56.0f) return -1;

        if (s2->c_h < s2->f_h + 56.0f) return -1;

        return time + 1.0f;
    }

    // estimate time for lift
    if (link.flags & kBotPathNodeLift) { time += 10.0f; }
    else if (f_diff > 24.0f)
    {
        // too big a step up
        return -1;
    }

    // estimate time for door
    if (link.flags & kBotPathNodeLift) { time += 2.0f; }
    else
    {
        // check for travelling THROUGH a door
        if ((link.flags & kBotPathNodeDoor) || (s1->c_h < s1->f_h + 56.0f))
        {
            // okay
        }
        else
        {
            // enough vertical space?
            float high_f = HMM_MAX(s1->f_h, s2->f_h);
            float low_c  = HMM_MIN(s1->c_h, s2->c_h);

            if (low_c - high_f < 56.0f) return -1;
        }
    }

    // for a big drop-off, estimate time to fall
    if (f_diff < -100.0f) { time += sqrtf(-f_diff - 80.0f) / 18.0f; }

    return time;
}

static float BotNavigateEstimateH(const subsector_t *cur_sub)
{
    int  id = (int)(cur_sub - level_subsectors);
    auto p  = nav_areas[id].get_middle();

    float dist = R_PointToDist(p.x, p.y, nav_finish_mid.x, nav_finish_mid.y);
    float time = dist / RUNNING_SPEED;

    // over-estimate, to account for height changes, obstacles etc
    return time * 1.25f;
}

static int BotNavigateLowestOpenF()
{
    // return index of the nav_area_c which is in the OPEN set and has the
    // lowest F value, where F = G + H.  returns -1 if OPEN set is empty.

    // this is a brute force search -- consider OPTIMISING it...

    int   result = -1;
    float best_F = 9e18;

    for (int i = 0; i < (int)nav_areas.size(); i++)
    {
        const nav_area_c &area = nav_areas[i];

        if (area.open)
        {
            float F = area.G + area.H;
            if (F < best_F)
            {
                best_F = F;
                result = i;
            }
        }
    }

    return result;
}

static void BotNavigateTryOpenArea(int idx, int parent, float cost)
{
    nav_area_c &area = nav_areas[idx];

    if (cost < area.G)
    {
        area.open   = true;
        area.parent = parent;
        area.G      = cost;

        if (AlmostEquals(area.H, 0.0f))
            area.H = BotNavigateEstimateH(&level_subsectors[idx]);
    }
}

static void BotNavigateStoreSegMiddle(BotPath *path, int flags,
                                      const seg_t *seg)
{
    SYS_ASSERT(seg);

    // calc middle of the adjoining seg
    Position pos;

    pos.x = (seg->v1->X + seg->v2->X) * 0.5f;
    pos.y = (seg->v1->Y + seg->v2->Y) * 0.5f;
    pos.z = seg->front_sub->sector->f_h;

    path->nodes_.push_back(BotPathNode{pos, flags, seg});
}

static BotPath *BotNavigateStorePath(Position start, int start_id,
                                     Position finish, int finish_id)
{
    BotPath *path = new BotPath;

    path->nodes_.push_back(BotPathNode{start, 0, nullptr});

    // handle case of same subsector -- no segs
    if (start_id == finish_id)
    {
        path->nodes_.push_back(BotPathNode{finish, 0, nullptr});
        return path;
    }

    // use a list to put the subsectors into the correct order
    std::forward_list<int> subsec_list;

    for (int cur_id = finish_id;;)
    {
        subsec_list.push_front(cur_id);

        if (cur_id == start_id) break;

        cur_id = nav_areas[cur_id].parent;
    }

    // visit each pair of subsectors in order...
    int prev_id = -1;

    for (int cur_id : subsec_list)
    {
        if (prev_id < 0)
        {
            prev_id = cur_id;
            continue;
        }

        // find the link
        const nav_area_c &area = nav_areas[prev_id];
        const nav_link_c *link = nullptr;

        for (int k = 0; k < area.num_links; k++)
        {
            int L = area.first_link + k;

            if (nav_links[L].dest_id == cur_id)
            {
                link = &nav_links[L];
                break;
            }
        }

        // this should never happen
        if (link == nullptr)
            FatalError("could not find link in path (%d -> %d)\n", prev_id,
                    cur_id);

        BotNavigateStoreSegMiddle(path, link->flags, link->seg);

        // for a lift, also store the place to ride the lift
        if (link->flags & kBotPathNodeLift)
        {
            auto pos = nav_areas[link->dest_id].get_middle();
            path->nodes_.push_back(BotPathNode{pos, 0, nullptr});
        }

        prev_id = cur_id;
    }

    path->nodes_.push_back(BotPathNode{finish, 0, nullptr});

    return path;
}

BotPath *BotNavigateFindPath(const Position *start, const Position *finish,
                             int flags)
{
    // tries to find a path from start to finish.
    // if successful, returns a path, otherwise returns nullptr.
    //
    // the path may include manual lifts and doors, but more complicated
    // things (e.g. a door activated by a nearby switch) will fail.

    SYS_ASSERT(start);
    SYS_ASSERT(finish);

    subsector_t *start_sub  = R_PointInSubsector(start->x, start->y);
    subsector_t *finish_sub = R_PointInSubsector(finish->x, finish->y);

    int start_id  = (int)(start_sub - level_subsectors);
    int finish_id = (int)(finish_sub - level_subsectors);

    if (start_id == finish_id)
    {
        return BotNavigateStorePath(*start, start_id, *finish, finish_id);
    }

    // get coordinate of finish subsec
    nav_finish_mid = nav_areas[finish_id].get_middle();

    // prepare all nodes
    for (nav_area_c &area : nav_areas)
    {
        area.open   = false;
        area.G      = 9e19;
        area.H      = 0.0;
        area.parent = -1;
    }

    BotNavigateTryOpenArea(start_id, -1, 0);

    for (;;)
    {
        int cur = BotNavigateLowestOpenF();

        // no path at all?
        if (cur < 0) return nullptr;

        // reached the destination?
        if (cur == finish_id)
        {
            return BotNavigateStorePath(*start, start_id, *finish, finish_id);
        }

        // move current node to CLOSED set
        nav_area_c &area = nav_areas[cur];
        area.open        = false;

        // visit each neighbor node
        for (int k = 0; k < area.num_links; k++)
        {
            const nav_link_c &link = nav_links[area.first_link + k];

            float cost = BotNavigateTraverseLinkCost(cur, link, true);
            if (cost < 0) continue;

            // we need the total traversal time
            cost += area.G;

            // update neighbor if this path is a better one
            BotNavigateTryOpenArea(link.dest_id, cur, cost);
        }
    }
}

//----------------------------------------------------------------------------

static void BotNavigateItemsInSubsector(subsector_t *sub, DeathBot *bot,
                                        Position &pos, float radius,
                                        int sub_id, int &best_id,
                                        float &best_score, MapObject *&best_mo)
{
    for (MapObject *mo = sub->thinglist; mo != nullptr; mo = mo->subsector_next_)
    {
        float score = bot->EvalItem(mo);
        if (score < 0) continue;

        float dist = R_PointToDist(pos.x, pos.y, mo->x, mo->y);
        if (dist > radius) continue;

        // very close things get a boost
        if (dist < radius * 0.25f) score = score * 2.0f;

        // randomize the score -- to break ties
        score += (float)RandomShort() / 65535.0f;

        if (score > best_score)
        {
            best_id    = sub_id;
            best_score = score;
            best_mo    = mo;
        }
    }
}

BotPath *BotNavigateFindThing(DeathBot *bot, float radius, MapObject *&best)
{
    // find an item to pickup or enemy to fight.
    // each nearby thing (limited roughly by `radius') will be passed to the
    // EvalThing() method of the bot.  returns nullptr if nothing was found.

    Position pos{bot->pl_->mo->x, bot->pl_->mo->y, bot->pl_->mo->z};

    subsector_t *start    = R_PointInSubsector(pos.x, pos.y);
    int          start_id = (int)(start - level_subsectors);

    // the best thing so far...
    best             = nullptr;
    float best_score = 0;
    int   best_id    = -1;

    // prepare all nodes
    for (nav_area_c &area : nav_areas)
    {
        area.open   = false;
        area.G      = 9e19;
        area.H      = 1.0;  // a constant gives a Djikstra search
        area.parent = -1;
    }

    BotNavigateTryOpenArea(start_id, -1, 0);

    for (;;)
    {
        int cur = BotNavigateLowestOpenF();

        // no areas left to visit?
        if (cur < 0)
        {
            if (best == nullptr) return nullptr;

            return BotNavigateStorePath(pos, start_id, *best, best_id);
        }

        // move current node to CLOSED set
        nav_area_c &area = nav_areas[cur];
        area.open        = false;

        // visit the things
        BotNavigateItemsInSubsector(&level_subsectors[cur], bot, pos, radius, cur,
                                    best_id, best_score, best);

        // visit each neighbor node
        for (int k = 0; k < area.num_links; k++)
        {
            // check link is passable
            const nav_link_c &link = nav_links[area.first_link + k];

            // doors, lifts and teleporters are not allowed for things.
            // [ since getting an item and opening a door are both tasks ]
            if (link.flags != kBotPathNodeNormal) continue;

            float cost = BotNavigateTraverseLinkCost(cur, link, false);
            if (cost < 0) continue;

            // we need the total traversal time
            cost += area.G;

            if (cost > (radius * 1.4) / RUNNING_SPEED) continue;

            // update neighbor if this path is a better one
            BotNavigateTryOpenArea(link.dest_id, cur, cost);
        }
    }
}

//----------------------------------------------------------------------------

static void BotNavigateEnemiesInSubsector(const subsector_t *sub, DeathBot *bot,
                                          float radius, MapObject *&best_mo,
                                          float &best_score)
{
    for (MapObject *mo = sub->thinglist; mo != nullptr; mo = mo->subsector_next_)
    {
        if (bot->EvalEnemy(mo) < 0) continue;

        float dx = fabs(bot->pl_->mo->x - mo->x);
        float dy = fabs(bot->pl_->mo->y - mo->y);

        if (dx > radius || dy > radius) continue;

        // pick one of the monsters at random
        float score = (float)RandomShort() / 65535.0f;

        if (score > best_score)
        {
            best_mo    = mo;
            best_score = score;
        }
    }
}

static void BotNavigateEnemiesInNode(unsigned int bspnum, DeathBot *bot,
                                     float radius, MapObject *&best_mo,
                                     float &best_score)
{
    if (bspnum & NF_V5_SUBSECTOR)
    {
        bspnum &= ~NF_V5_SUBSECTOR;
        BotNavigateEnemiesInSubsector(&level_subsectors[bspnum], bot, radius, best_mo,
                                      best_score);
        return;
    }

    const node_t *node = &level_nodes[bspnum];

    Position pos{bot->pl_->mo->x, bot->pl_->mo->y, bot->pl_->mo->z};

    for (int c = 0; c < 2; c++)
    {
        // reject children outside of the bounds
        if (node->bbox[c][kBoundingBoxLeft] > pos.x + radius) continue;
        if (node->bbox[c][kBoundingBoxRight] < pos.x - radius) continue;
        if (node->bbox[c][kBoundingBoxBottom] > pos.y + radius) continue;
        if (node->bbox[c][kBoundingBoxTop] < pos.y - radius) continue;

        BotNavigateEnemiesInNode(node->children[c], bot, radius, best_mo,
                                 best_score);
    }
}

MapObject *BotNavigateFindEnemy(DeathBot *bot, float radius)
{
    // find an enemy to fight, or nullptr if none found.
    // caller is responsible to do a sight checks.
    // radius is the size of a square box (not a circle).

    MapObject *best_mo    = nullptr;
    float   best_score = 0;

    BotNavigateEnemiesInNode(root_node, bot, radius, best_mo, best_score);

    return best_mo;
}

//----------------------------------------------------------------------------

Position BotPath::CurrentDestination() const { return nodes_.at(along_).pos; }

Position BotPath::CurrentFrom() const { return nodes_.at(along_ - 1).pos; }

float BotPath::CurrentLength() const
{
    Position src  = nodes_.at(along_ - 1).pos;
    Position dest = nodes_.at(along_ + 0).pos;

    return hypotf(dest.x - src.x, dest.y - src.y);
}

BAMAngle BotPath::CurrentAngle() const
{
    Position src  = nodes_.at(along_ - 1).pos;
    Position dest = nodes_.at(along_ + 0).pos;

    return R_PointToAngle(src.x, src.y, dest.x, dest.y);
}

bool BotPath::ReachedDestination(const Position *pos) const
{
    Position dest = CurrentDestination();

    // too low?
    if (pos->z < dest.z - 15.0) { return false; }

    if (pos->x < dest.x - 64) return false;
    if (pos->x > dest.x + 64) return false;
    if (pos->y < dest.y - 64) return false;
    if (pos->y > dest.y + 64) return false;

    // check bot has entered the other half plane
    Position from = nodes_.at(along_ - 1).pos;

    float ux   = dest.x - from.x;
    float uy   = dest.y - from.y;
    float ulen = hypotf(ux, uy);

    if (ulen < 1.0) return true;

    ux /= ulen;
    uy /= ulen;

    float dot_p = (pos->x - dest.x) * ux + (pos->y - dest.y) * uy;

    if (dot_p < -16.0) return false;

    return true;
}

//----------------------------------------------------------------------------

void BotNavigateAnalyseLevel()
{
    BotNavigateFreeLevel();

    BotNavigateCollectBigItems();
    BotNavigateCreateLinks();
}

void BotNavigateFreeLevel()
{
    big_items.clear();
    nav_areas.clear();
    nav_links.clear();
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
