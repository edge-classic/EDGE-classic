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
#include "deh_attacks.h"
#include "deh_buffer.h"
#include "deh_frames.h"
#include "deh_info.h"
#include "deh_misc.h"
#include "deh_patch.h"
#include "deh_rscript.h"
#include "deh_sounds.h"
#include "deh_storage.h"
#include "deh_system.h"
#include "deh_things.h"
#include "deh_text.h"
#include "deh_util.h"
#include "deh_wad.h"
#include "deh_weapons.h"

namespace Deh_Edge
{

class input_buffer_c
{
public:
	parse_buffer_api *buf;

	input_buffer_c(parse_buffer_api *_buf) : buf(_buf)
	{ }

	~input_buffer_c()
	{
		delete buf;
		buf = NULL;
	}
};

#define MAX_INPUTS  32

input_buffer_c *input_bufs[MAX_INPUTS];
int num_inputs = 0;

bool quiet_mode;
bool all_mode;

const dehconvfuncs_t *cur_funcs = NULL;


void Startup(void)
{
	System_Startup();

	Ammo::Startup();
	Frames::Startup();
	Misc::Startup();
	Rscript::Startup();
	Sounds::Startup();
	TextStr::Startup();
	Things::Startup();
	Weapons::Startup();

	Storage::Startup();

	/* reset parameters */

	num_inputs = 0;

	quiet_mode = false;
	all_mode = false;
}

void FreeInputBuffers(void)
{
	for (int j = 0; j < num_inputs; j++)
	{
		if (input_bufs[j])
		{
			delete input_bufs[j];
			input_bufs[j] = NULL;
		}
	}
}

dehret_e Convert(void)
{
	dehret_e result;

	// load DEH patch file(s)
	for (int j = 0; j < num_inputs; j++)
	{
		char temp_text[256];

		ProgressText(temp_text);
		ProgressMajor(j * 70 / num_inputs, (j+1) * 70 / num_inputs);

		result = Patch::Load(input_bufs[j]->buf);

		if (result != DEH_OK)
			return result;
	}

	FreeInputBuffers();

	ProgressText("Converting DEH");
	ProgressMajor(70, 80);

	Storage::ApplyAll();

	// do conversions into DDF...

	TextStr::SpriteDependencies();
	Frames::StateDependencies();
	Ammo::AmmoDependencies();

	Things::FixHeights();

	Sounds::ConvertSFX();
	Sounds::ConvertMUS();
	Attacks::ConvertATK();
	Things::ConvertTHING();
	Weapons::ConvertWEAP();
	TextStr::ConvertLDF();
	Rscript::ConvertRAD();

	Storage::RestoreAll();

	PrintMsg("\n");

	return DEH_OK;
}

void Shutdown(void)
{
	System_Shutdown();
}

}  // Deh_Edge

//------------------------------------------------------------------------

void DehEdgeStartup(const dehconvfuncs_t *funcs)
{
	Deh_Edge::Startup();
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
	if (Deh_Edge::num_inputs >= MAX_INPUTS)
	{
		Deh_Edge::SetErrorMsg("Too many dehacked lumps !\n");
		return DEH_E_Unknown;
	}

	auto buf = Deh_Edge::Buffer::OpenLump((const char *)data, length);

	Deh_Edge::input_bufs[Deh_Edge::num_inputs++] =
		new Deh_Edge::input_buffer_c(buf);

	return DEH_OK;
}

dehret_e DehEdgeRunConversion(deh_container_c *dest)
{
	Deh_Edge::WAD::dest_container = dest;

	return Deh_Edge::Convert();
}

void DehEdgeShutdown(void)
{
	Deh_Edge::WAD::dest_container = NULL;

	Deh_Edge::Shutdown();
	Deh_Edge::cur_funcs = NULL;
}
