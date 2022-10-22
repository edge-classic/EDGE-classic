//----------------------------------------------------------------------------
//  EDGE Navigation System
//----------------------------------------------------------------------------
//
//  Copyright (c) 2022  The EDGE Team.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
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

struct bot_t;

void NAV_AnalyseLevel();
void NAV_FreeLevel();

position_c NAV_NextRoamPoint(bot_t *bot);

#endif  /*__P_NAVIGATE_H__*/

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
