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
    kBotPathNodeDoor     = (1 << 0), // manual door (press USE to open)
    kBotPathNodeLift     = (1 << 1), // manual lift (press USE to lower)
    kBotPathNodeTeleport = (1 << 2), // teleporter line, walk over it
};

struct BotPathNode
{
    Position   pos{0, 0, 0};
    int        flags = kBotPathNodeNormal;
    const Seg *seg   = nullptr;
};

// a path from a start point to a finish one.
// includes both start and finish (at least two entries).
class BotPath
{
  public:
    std::vector<BotPathNode> nodes_;

    size_t along_ = 1;

    bool Finished() const
    {
        return along_ == nodes_.size();
    }

    Position CurrentDestination() const;
    Position CurrentFrom() const;

    float    CurrentLength() const;
    BAMAngle CurrentAngle() const;

    bool ReachedDestination(const Position *pos) const;
};

void BotAnalyseLevel();
void BotFreeLevel();

float BotEvaluateBigItem(const MapObject *mo);
bool  BotNextRoamPoint(Position &out);

// attempt to find a traversible path, returns nullptr if failed.
BotPath *BotFindPath(const Position *start, const Position *finish, int flags);

// find an pickup item in a nearby area, returns nullptr if none found.
BotPath *BotFindThing(DeathBot *bot, float radius, MapObject *&best);

// find an enemy to fight, or nullptr if none found.
// caller is responsible to do a sight checks.
MapObject *BotFindEnemy(DeathBot *bot, float radius);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
