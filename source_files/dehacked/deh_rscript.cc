//------------------------------------------------------------------------
//  RSCRIPT output
//------------------------------------------------------------------------
//
//  DEH_EDGE  Copyright (C) 2004-2005  The EDGE Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License (in COPYING.txt) for more details.
//
//------------------------------------------------------------------------
//
//  DEH_EDGE is based on:
//
//  +  DeHackEd source code, by Greg Lewis.
//  -  DOOM source code (C) 1993-1996 id Software, Inc.
//  -  Linux DOOM Hack Editor, by Sam Lantinga.
//  -  PrBoom's DEH/BEX code, by Ty Halderman, TeamTNT.
//
//------------------------------------------------------------------------

#include "deh_i_defs.h"
#include "deh_edge.h"

#include "deh_info.h"
#include "deh_mobj.h"
#include "deh_rscript.h"
#include "deh_system.h"
#include "deh_things.h"
#include "deh_util.h"
#include "deh_wad.h"

#include <assert.h>


namespace Deh_Edge
{

#define MAP07  \
	"  radiustrigger 0 0 -1\n"  \
	"    ondeath MANCUBUS\n"  \
	"    activate_linetype 38 666\n"  \
	"  end_radiustrigger\n"  \
	"\n"  \
	"  radiustrigger 0 0 -1\n"  \
	"    ondeath ARACHNOTRON\n"  \
	"    activate_linetype 30 667\n"  \
	"  end_radiustrigger\n"

#define MAP32  \
	"  radiustrigger 0 0 -1\n"  \
	"    ondeath COMMANDER_KEEN\n"  \
	"    activate_linetype 2 666\n"  \
	"  end_radiustrigger\n"

#define E1M8  \
	"  radiustrigger 0 0 -1\n"  \
	"    ondeath BARON_OF_HELL\n"  \
	"    activate_linetype 38 666\n"  \
	"  end_radiustrigger\n"

#define E2M8  \
	"  radiustrigger 0 0 -1\n"  \
	"    ondeath THE_CYBERDEMON\n"  \
	"    exitlevel 5\n"  \
	"  end_radiustrigger\n"

#define E3M8  \
	"  radiustrigger 0 0 -1\n"  \
	"    ondeath THE_SPIDER_MASTERMIND\n"  \
	"    exitlevel 5\n"  \
	"  end_radiustrigger\n"

#define E4M6  \
	"  radiustrigger 0 0 -1\n"  \
	"    ondeath THE_CYBERDEMON\n"  \
	"    activate_linetype 2 666\n"  \
	"  end_radiustrigger\n"

#define E4M8  \
	"  radiustrigger 0 0 -1\n"  \
	"    ondeath THE_SPIDER_MASTERMIND\n"  \
	"    activate_linetype 38 666\n"  \
	"  end_radiustrigger\n"


//------------------------------------------------------------------------

namespace Rscript
{
	std::vector<int> boss_mobjs;

	void BeginLump();
	void FinishLump();

	bool IsBoss(int mt_num);

	void CollectMatchingBosses(std::vector<int>& list, int flag);
	void OutputTrigger(const std::string& map, const std::vector<int>& list);
	void HandleLevel(const std::string& map, int flag1, int mt1, int flag2, int mt2);
}


void Rscript::Init()
{
	boss_mobjs.clear();
}


void Rscript::Shutdown()
{
	boss_mobjs.clear();
}


void Rscript::BeginLump()
{
	WAD::NewLump(DDF_RadScript);

	WAD::Printf("// <SCRIPTS>\n\n");
}

void Rscript::FinishLump()
{
	WAD::Printf("\n");
}


bool Rscript::IsBoss(int mt_num)
{
	for (int num : boss_mobjs)
		if (num == mt_num)
			return true;

	return false;
}


void Rscript::MarkBossDeath(int mt_num)
{
	// only monsters which use A_BossDeath can trigger the special code
	// in DOOM for the maps ExM8, E4M6 and MAP07.

	if (! IsBoss(mt_num))
		boss_mobjs.push_back(mt_num);
}


void Rscript::CollectMatchingBosses(std::vector<int>& list, int flag)
{
	for (int mt_num : boss_mobjs)
	{
		if ((Things::GetMobjMBF21Flags(mt_num) & flag) != 0)
			list.push_back(mt_num);
	}
}


void Rscript::OutputTrigger(const std::string& map, const std::vector<int>& list)
{
	WAD::Printf("  radiustrigger 0 0 -1\n");

	// TODO

	WAD::Printf("  end_radiustrigger\n");
}


void Rscript::HandleLevel(const std::string& map, int flag1, int mt1, int flag2, int mt2)
{
	std::vector<int> list1;
	std::vector<int> list2;

	if (flag1 != 0) CollectMatchingBosses(list1, flag1);
	if (flag2 != 0) CollectMatchingBosses(list2, flag2);

	// check if the results are any different from normal.
	// if there was no change, then we output no script.
	bool different = false;

	if (flag1 != 0 && (list1.size() != 1 || list1[0] != mt1)) different = true;
	if (flag2 != 0 && (list2.size() != 1 || list2[0] != mt2)) different = true;

	if (! different)
		return;

	WAD::Printf("start_map %s\n", map.c_str());

	if (flag1 != 0) OutputTrigger(map, list1);
	if (flag2 != 0) OutputTrigger(map, list2);

	WAD::Printf("end_map\n\n\n");
}


void Rscript::ConvertRAD()
{
	BeginLump();

	WAD::Printf("// --- DOOM I Scripts ---\n\n");

	HandleLevel("E1M8", MBF21_E1M8BOSS, MT_BRUISER, 0, 0);
	HandleLevel("E2M8", MBF21_E2M8BOSS, MT_CYBORG,  0, 0);
	HandleLevel("E3M8", MBF21_E3M8BOSS, MT_SPIDER,  0, 0);
	HandleLevel("E4M6", MBF21_E4M6BOSS, MT_CYBORG,  0, 0);
	HandleLevel("E4M8", MBF21_E4M8BOSS, MT_SPIDER,  0, 0);

	WAD::Printf("// --- DOOM II Scripts ---\n\n");

	HandleLevel("MAP07", MBF21_MAP07BOSS1, MT_FATSO,  MBF21_MAP07BOSS2, MT_BABY);

	FinishLump();
}

}  // Deh_Edge
