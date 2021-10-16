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

#include "i_defs.h"
#include "rscript.h"

#include "dh_plugin.h"
#include "info.h"
#include "mobj.h"
#include "storage.h"
#include "system.h"
#include "util.h"
#include "wad.h"

#include <assert.h>

namespace Deh_Edge
{

#define MAX_KEENS  16


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
	int keen_mobjs[MAX_KEENS];
	int keen_count;

	void Startup(void)
	{
		keen_count = 0;
	}

	void MarkKeenDie(int mt_num)
	{
		for (int i = 0; i < keen_count; i++)
			if (keen_mobjs[i] == mt_num)
				return;

		if (keen_count >= MAX_KEENS)
		{
			PrintWarn("Too many keen deaths ! (monster %s)\n", mobjinfo[mt_num].name);
			return;
		}

		keen_mobjs[keen_count++] = mt_num;
	}

	void BeginLump(void)
	{
		WAD::NewLump("RSCRIPT");

		WAD::Printf(GEN_BY_COMMENT);
		WAD::Printf("// <SCRIPTS>\n\n");
	}

	void FinishLump(void)
	{
		WAD::Printf("\n");
		WAD::FinishLump();
	}

	// returns MT number of monster involved
	int SpecialLevel(int episode /* 0 for DOOM II */, int map)
	{
		if (episode == 0)
		{
			if (map == 7)
			{
				WAD::Printf("%s\n", MAP07);
				return MT_FATSO;
			}

			if (map == 32)
			{
				WAD::Printf("%s\n", MAP32);
				return MT_KEEN;
			}

			return -2;
		}

		switch (episode*10 + map)
		{
			case 18: WAD::Printf("%s\n", E1M8); return MT_BRUISER;
		    case 28: WAD::Printf("%s\n", E2M8); return MT_CYBORG;
		    case 38: WAD::Printf("%s\n", E3M8); return MT_SPIDER;
		    case 46: WAD::Printf("%s\n", E4M6); return MT_CYBORG;
		    case 48: WAD::Printf("%s\n", E4M8); return MT_SPIDER;

			default: return -2;
		}
	}

	void OutputMonsterDeath(int mt_num, int idx)
	{
		assert(mt_num >= 0);

		// This is complicated because the ONDEATH normally succeeds when
		// there are NO such monsters on the map.  What we want is that
		// only maps containing such monsters to be affected.  We achieve
		// this with two extra scripts.  One script will enable the main
		// ONDEATH script after a short delay -- this script is then
		// disabled by another script

		idx *= 10;

		WAD::Printf("  radiustrigger 0 0 -1\n");
		WAD::Printf("    tag %d\n", idx);
		WAD::Printf("    tagged_disabled\n");
		WAD::Printf("    ondeath %s\n", mobjinfo[mt_num].name);
		WAD::Printf("    activate_linetype 2 666\n");
		WAD::Printf("  end_radiustrigger\n");
		WAD::Printf("  //\n");
		WAD::Printf("  radiustrigger 0 0 -1\n");
		WAD::Printf("    tag %d\n", idx+1);
		WAD::Printf("    wait 2\n");
		WAD::Printf("    enable_tagged %d\n", idx);
		WAD::Printf("  end_radiustrigger\n");
		WAD::Printf("  //\n");
		WAD::Printf("  radiustrigger 0 0 -1\n");
		WAD::Printf("    ondeath %s\n", mobjinfo[mt_num].name);
		WAD::Printf("    disable_tagged %d\n", idx+1);
		WAD::Printf("  end_radiustrigger\n");
	}
	
	void OutputLevel(int episode /* 0 for DOOM II */, int map)
	{
		if (episode == 0)
			WAD::Printf("start_map MAP%02d\n", map);
		else
			WAD::Printf("start_map E%dM%d\n", episode, map);

		int spec_mt = SpecialLevel(episode, map);

		for (int i = 0; i < keen_count; i++)
		{
			// don't let keen deaths interfere with boss-death scripts
			if (spec_mt == keen_mobjs[i])
				continue;

			if (i > 0)
				WAD::Printf("\n");

			OutputMonsterDeath(keen_mobjs[i], i + 1);
		}

		WAD::Printf("end_map\n\n\n");
	}

	void ConvertRAD(void)
	{
		if (! all_mode && keen_count == 0)
			return;

		BeginLump();

		WAD::Printf("// --- DOOM I Scripts ---\n\n");

		for (int e = 1; e <= 4; e++)
			for (int m = 1; m <= 9; m++)
				OutputLevel(e, m);

		WAD::Printf("// --- DOOM II Scripts ---\n\n");

		for (int j = 1; j <= 32; j++)
			OutputLevel(0, j);

		FinishLump();
	}
}

}  // Deh_Edge
