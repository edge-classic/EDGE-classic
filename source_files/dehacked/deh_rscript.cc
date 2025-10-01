//------------------------------------------------------------------------
//  RSCRIPT output
//------------------------------------------------------------------------
//
//  DEH_EDGE  Copyright (C) 2004-2024 The EDGE Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
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

#include "deh_rscript.h"

#include "deh_edge.h"
#include "deh_info.h"
#include "deh_mobj.h"
#include "deh_system.h"
#include "deh_things.h"
#include "deh_wad.h"

namespace dehacked
{

namespace rscript
{
std::vector<int> keen_mobjs;

void BeginLump();
void FinishLump();

bool IsKeen(int kMT_num);

void CollectMatchingBosses(std::vector<int> &list, int flag);
void OutputTrigger(const std::string &map, const std::vector<int> &list, bool boss2);
void HandleLevel(const std::string &map, int flag1, int mt1, int flag2, int mt2);
} // namespace rscript

void rscript::Init()
{
    keen_mobjs.clear();
}

void rscript::Shutdown()
{
    keen_mobjs.clear();
}

void rscript::BeginLump()
{
    wad::NewLump(kDDFTypeRadScript);

    wad::Printf("// <SCRIPTS>\n\n");
}

void rscript::FinishLump()
{
    wad::Printf("\n");
}

bool rscript::IsKeen(int kMT_num)
{
    for (int num : keen_mobjs)
        if (num == kMT_num)
            return true; // peachy keen!

    return false;        // not so keen
}

void rscript::MarkKeenDie(int kMT_num)
{
    if (!IsKeen(kMT_num))
        keen_mobjs.push_back(kMT_num);
}

void rscript::CollectMatchingBosses(std::vector<int> &list, int flag)
{
    for (int i = 1; i <= 32767; i++)
    {
        // Note: we skip kMT_PLAYER (index == 0)

        // skip monsters using A_KeenDie, since the KEEN_DIE action already
        // handles their death and we don't want to intefere with that.
        if (IsKeen(i))
            continue;

        if (0 != (things::GetMobjMBF21Flags(i) & flag))
            list.push_back(i);
    }
}

void rscript::OutputTrigger(const std::string &map, const std::vector<int> &list, bool boss2)
{
    // when there is no monsters, that is okay, we just don't output any
    // radius trigger (there is nothing it could do).
    if (list.empty())
        return;

    wad::Printf("  radiustrigger 0 0 -1\n");
    wad::Printf("    wait_until_dead");

    for (int kMT_num : list)
    {
        wad::Printf(" %s", things::GetMobjName(kMT_num));
    }

    wad::Printf("\n");

    // the command to execute depends on the map...

    if (map == "E1M8")
        wad::Printf("    activate_linetype 38 666\n");
    else if (map == "E2M8")
        wad::Printf("    exit_level 5\n");
    else if (map == "E3M8")
        wad::Printf("    exit_level 5\n");
    else if (map == "E4M6")
        wad::Printf("    activate_linetype 2 666\n");
    else if (map == "E4M8")
        wad::Printf("    activate_linetype 38 666\n");
    else if (!boss2)
        wad::Printf("    activate_linetype 38 666\n"); // MAP07 Mancubus
    else
        wad::Printf("    activate_linetype 30 667\n"); // MAP07 Arachnotron

    wad::Printf("  end_radiustrigger\n");
}

void rscript::HandleLevel(const std::string &map, int flag1, int mt1, int flag2, int mt2)
{
    std::vector<int> list1;
    std::vector<int> list2;

    if (flag1 != 0)
        CollectMatchingBosses(list1, flag1);
    if (flag2 != 0)
        CollectMatchingBosses(list2, flag2);

    // check if the results are any different from normal.
    // if there was no change, then we output no script.
    bool different = false;

    if (flag1 != 0 && (list1.size() != 1 || list1[0] != mt1))
        different = true;
    if (flag2 != 0 && (list2.size() != 1 || list2[0] != mt2))
        different = true;

    if (!different)
        return;

    wad::Printf("START_MAP %s\n", map.c_str());

    if (flag1 != 0)
        OutputTrigger(map, list1, false);
    if (flag2 != 0)
        OutputTrigger(map, list2, true);

    wad::Printf("END_MAP\n\n\n");
}

void rscript::ConvertRAD()
{
    BeginLump();

    wad::Printf("// --- DOOM I Scripts ---\n\n");

    HandleLevel("E1M8", kMBF21_E1M8BOSS, kMT_BRUISER, 0, 0);
    HandleLevel("E2M8", kMBF21_E2M8BOSS, kMT_CYBORG, 0, 0);
    HandleLevel("E3M8", kMBF21_E3M8BOSS, kMT_SPIDER, 0, 0);
    HandleLevel("E4M6", kMBF21_E4M6BOSS, kMT_CYBORG, 0, 0);
    HandleLevel("E4M8", kMBF21_E4M8BOSS, kMT_SPIDER, 0, 0);

    wad::Printf("// --- DOOM II Scripts ---\n\n");

    HandleLevel("MAP07", kMBF21_MAP07BOSS1, kMT_FATSO, kMBF21_MAP07BOSS2, kMT_BABY);

    FinishLump();
}

} // namespace dehacked
