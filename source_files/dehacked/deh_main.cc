//------------------------------------------------------------------------
//  MAIN Program
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "deh_i_defs.h"
#include "deh_edge.h"

#include "deh_ammo.h"
#include "deh_buffer.h"
#include "deh_frames.h"
#include "deh_info.h"
#include "deh_misc.h"
#include "deh_music.h"
#include "deh_patch.h"
#include "deh_rscript.h"
#include "deh_sounds.h"
#include "deh_sprites.h"
#include "deh_system.h"
#include "deh_things.h"
#include "deh_text.h"
#include "deh_util.h"
#include "deh_wad.h"
#include "deh_weapons.h"

namespace Deh_Edge
{

std::vector<input_buffer_c *> input_bufs;

bool quiet_mode;
bool all_mode;

const dehconvfuncs_t *cur_funcs = NULL;


void Init()
{
	System_Startup();

	Ammo   ::Init();
	Frames ::Init();
	Misc   ::Init();
	Rscript::Init();
	Sounds ::Init();
	Music  ::Init();
	Sprites::Init();
	TextStr::Init();
	Things ::Init();
	Weapons::Init();

	/* reset parameters */

	quiet_mode = false;
	all_mode = false;
}


void FreeInputBuffers(void)
{
	for (size_t i = 0; i < input_bufs.size() ; i++)
	{
		delete input_bufs[i];
	}

	input_bufs.clear();
}


dehret_e Convert(void)
{
	dehret_e result;

	// load DEH patch file(s)
	for (size_t i = 0; i < input_bufs.size() ; i++)
	{
		result = Patch::Load(input_bufs[i]);

		if (result != DEH_OK)
			return result;
	}

	// do conversions into DDF...

	Sprites::SpriteDependencies();
	Frames ::StateDependencies();
	Ammo   ::AmmoDependencies();

	// things and weapons must be before attacks
	Weapons::ConvertWEAP();
	Things ::ConvertTHING();
	Things ::ConvertATK();

	// rscript must be after things (for A_KeenDie)
	TextStr::ConvertLDF();
	Rscript::ConvertRAD();

	// sounds must be after things/weapons/attacks
	Sounds::ConvertSFX();
	Music ::ConvertMUS();

	PrintMsg("\n");

	return DEH_OK;
}


void Shutdown()
{
	Ammo   ::Shutdown();
	Frames ::Shutdown();
	Misc   ::Shutdown();
	Rscript::Shutdown();
	Sounds ::Shutdown();
	Music  ::Shutdown();
	Sprites::Shutdown();
	TextStr::Shutdown();
	Things ::Shutdown();
	Weapons::Shutdown();

	FreeInputBuffers();

	System_Shutdown();
}

}  // Deh_Edge

//------------------------------------------------------------------------

void DehEdgeStartup(const dehconvfuncs_t *funcs)
{
	Deh_Edge::Init();
	Deh_Edge::cur_funcs = funcs;

	Deh_Edge::PrintMsg("*** DeHackEd -> EDGE Conversion ***\n");
}


const char *DehEdgeGetError(void)
{
	return Deh_Edge::GetErrorMsg();
}


dehret_e DehEdgeSetQuiet(int quiet)
{
	Deh_Edge::quiet_mode = (quiet != 0);

	return DEH_OK;
}


dehret_e DehEdgeAddLump(const char *data, int length)
{
	auto buf = new Deh_Edge::input_buffer_c(data, length);

	Deh_Edge::input_bufs.push_back(buf);

	return DEH_OK;
}


dehret_e DehEdgeRunConversion(ddf_collection_c *dest)
{
	Deh_Edge::WAD::dest_container = dest;

	return Deh_Edge::Convert();
}


void DehEdgeShutdown(void)
{
	Deh_Edge::Shutdown();
	Deh_Edge::WAD::dest_container = NULL;
	Deh_Edge::cur_funcs = NULL;
}
