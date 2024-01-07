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

#ifndef __P_NAVIGATE_H__
#define __P_NAVIGATE_H__

#include "types.h"
#include "p_mobj.h"
#include "r_defs.h"

class bot_t;

enum path_node_flags_e
{
    PNODE_Normal   = 0,
    PNODE_Door     = (1 << 0), // manual door (press USE to open)
    PNODE_Lift     = (1 << 1), // manual lift (press USE to lower)
    PNODE_Teleport = (1 << 2), // teleporter line, walk over it
};

class path_node_c
{
  public:
    position_c   pos{0, 0, 0};
    int          flags = PNODE_Normal;
    const seg_t *seg   = NULL;
};

// a path from a start point to a finish one.
// includes both start and finish (at least two entries).
class bot_path_c
{
  public:
    std::vector<path_node_c> nodes;

    size_t along = 1;

    bool finished() const
    {
        return along == nodes.size();
    }

    position_c cur_dest() const;
    position_c cur_from() const;

    float   cur_length() const;
    bam_angle cur_angle() const;

    bool reached_dest(const position_c *pos) const;
};

void NAV_AnalyseLevel();
void NAV_FreeLevel();

float NAV_EvaluateBigItem(const mobj_t *mo);
bool  NAV_NextRoamPoint(position_c &out);

// attempt to find a traversible path, returns NULL if failed.
bot_path_c *NAV_FindPath(const position_c *start, const position_c *finish, int flags);

// find an pickup item in a nearby area, returns NULL if none found.
bot_path_c *NAV_FindThing(bot_t *bot, float radius, mobj_t *&best);

// find an enemy to fight, or NULL if none found.
// caller is responsible to do a sight checks.
mobj_t *NAV_FindEnemy(bot_t *bot, float radius);

#endif /*__P_NAVIGATE_H__*/

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
