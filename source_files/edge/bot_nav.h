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

#pragma once

#include "p_mobj.h"
#include "r_defs.h"

class DeathBot;

enum BotPathNodeFlag
{
    kBotPathNodeNormal   = 0,
    kBotPathNodeDoor     = (1 << 0),  // manual door (press USE to open)
    kBotPathNodeLift     = (1 << 1),  // manual lift (press USE to lower)
    kBotPathNodeTeleport = (1 << 2),  // teleporter line, walk over it
};

struct BotPathNode
{
    position_c   pos{0, 0, 0};
    int          flags = kBotPathNodeNormal;
    const seg_t *seg   = nullptr;
};

// a path from a start point to a finish one.
// includes both start and finish (at least two entries).
class BotPath
{
   public:
    std::vector<BotPathNode> nodes_;

    size_t along_ = 1;

    bool Finished() const { return along_ == nodes_.size(); }

    position_c CurrentDestination() const;
    position_c CurrentFrom() const;

    float    CurrentLength() const;
    BAMAngle CurrentAngle() const;

    bool ReachedDestination(const position_c *pos) const;
};

void BotNavigateAnalyseLevel();
void BotNavigateFreeLevel();

float BotNavigateEvaluateBigItem(const mobj_t *mo);
bool  BotNavigateNextRoamPoint(position_c &out);

// attempt to find a traversible path, returns nullptr if failed.
BotPath *BotNavigateFindPath(const position_c *start, const position_c *finish,
                             int flags);

// find an pickup item in a nearby area, returns nullptr if none found.
BotPath *BotNavigateFindThing(DeathBot *bot, float radius, mobj_t *&best);

// find an enemy to fight, or nullptr if none found.
// caller is responsible to do a sight checks.
mobj_t *BotNavigateFindEnemy(DeathBot *bot, float radius);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
