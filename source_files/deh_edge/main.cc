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

#include "i_defs.h"

#include "ammo.h"
#include "attacks.h"
#include "buffer.h"
#include "dh_plugin.h"
#include "frames.h"
#include "info.h"
#include "misc.h"
#include "patch.h"
#include "rscript.h"
#include "sounds.h"
#include "storage.h"
#include "system.h"
#include "things.h"
#include "text.h"
#include "util.h"
#include "wad.h"
#include "weapons.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

namespace Deh_Edge
{

class input_buffer_c
{
public:
	parse_buffer_api *buf;
	const char *infoname;
	bool is_lump;

	input_buffer_c(parse_buffer_api *_buf, const char *_info, bool _lump) :
		buf(_buf), is_lump(_lump)
	{
		infoname = StringDup(_info);
	}

	~input_buffer_c()
	{
		delete buf;
		buf = NULL;

		free((void*)infoname); 
	}
};

#define MAX_INPUTS  32

input_buffer_c *input_bufs[MAX_INPUTS];
int num_inputs = 0;

#define DEFAULT_TARGET  100

int target_version;
bool quiet_mode;
bool all_mode;

const dehconvfuncs_t *cur_funcs = NULL;


/* ----- user information ----------------------------- */

#ifndef DEH_EDGE_PLUGIN

void ShowTitle(void)
{
  PrintMsg(
    "\n"
	"=================================================\n"
    "|    DeHackEd -> EDGE Conversion Tool  V" DEH_EDGE_VERS "     |\n"
	"|                                               |\n"
	"|  The EDGE Team.  http://edge.sourceforge.net  |\n"
	"=================================================\n"
	"\n"
  );
}

void ShowInfo(void)
{
  PrintMsg(
    "USAGE:  deh_edge  (Options...)  input.deh (...)  (-o output.wad)\n"
	"\n"
	"Available options:\n"
	"   -e --edge #.##   Specify EDGE version to target.\n"
	"   -o --output      Override output filename.\n"
	"   -q --quiet       Quiet mode, suppress warnings.\n"
	"\n"
  );
}

#endif

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
	WAD::Startup();

	/* reset parameters */

	num_inputs = 0;

	target_version = DEFAULT_TARGET;
	quiet_mode = false;
	all_mode = false;
}

dehret_e AddFile(const char *filename)
{
	if (num_inputs >= MAX_INPUTS)
	{
		SetErrorMsg("Too many input files !!\n");
		return DEH_E_BadArgs;
	}

	if (strlen(ReplaceExtension(filename, NULL)) == 0)
	{
		SetErrorMsg("Illegal input filename: %s\n", filename);
		return DEH_E_BadArgs;
	}

	if (CheckExtension(filename, "wad") || CheckExtension(filename, "hwa"))
	{
		SetErrorMsg("Input filename cannot be a WAD file.\n");
		return DEH_E_BadArgs;
	}

	if (CheckExtension(filename, NULL))
	{
		// memory management here is "optimised" (i.e. a bit dodgy),
		// since the result of ReplaceExtension() is an internal static
		// buffer.

		const char *bex_name = ReplaceExtension(filename, "bex");

		if (FileExists(bex_name))
		{
			parse_buffer_api *buf = Buffer::OpenFile(bex_name);

			if (! buf) return DEH_E_NoFile;  // normally won't happen

			input_bufs[num_inputs++] = new input_buffer_c(buf,
				FileBaseName(bex_name), false);

			return DEH_OK;
		}

		const char *deh_name = ReplaceExtension(filename, "deh");

		if (FileExists(deh_name))
		{
			parse_buffer_api *buf = Buffer::OpenFile(deh_name);

			if (! buf) return DEH_E_NoFile;  // normally won't happen

			input_bufs[num_inputs++] = new input_buffer_c(buf,
				FileBaseName(deh_name), false);

			return DEH_OK;
		}
	}

	parse_buffer_api *buf = Buffer::OpenFile(filename);

	if (! buf)
		return DEH_E_NoFile;

	input_bufs[num_inputs++] = new input_buffer_c(buf,
		FileBaseName(filename), false);

	return DEH_OK;
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
		sprintf(temp_text, "Parsing %s", input_bufs[j]->infoname);

		ProgressText(temp_text);
		ProgressMajor(j * 70 / num_inputs, (j+1) * 70 / num_inputs);

		PrintMsg("Loading patch file: %s\n", input_bufs[j]->infoname);

		result = Patch::Load(input_bufs[j]->buf);

		if (result != DEH_OK)
			return result;
	}

	FreeInputBuffers();

	ProgressText("Converting DEH");
	ProgressMajor(70, 80);

	Storage::ApplyAll();

	// do conversions into DDF...
	PrintMsg("Converting data into EDGE %d.%02d DDF...\n",
		target_version / 100, target_version % 100);

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
	WAD::Shutdown();
	System_Shutdown();
}


/* ----- option handling ----------------------------- */

dehret_e ValidateArgs(void)
{
	if (num_inputs == 0)
	{
		SetErrorMsg("Missing input filename !\n");
		return DEH_E_BadArgs;
	}

	if (target_version < 100 || target_version >= 300)
	{
		SetErrorMsg("Illegal version number: %d.%02d\n", target_version / 100,
			target_version % 100);
		return DEH_E_BadArgs;
	}

	return DEH_OK;
}

}  // Deh_Edge

//------------------------------------------------------------------------

#ifndef DEH_EDGE_PLUGIN

namespace Deh_Edge
{

int main(int argc, char **argv)
{
	Startup();
	ShowTitle();
	
	// skip program name itself
	argv++, argc--;

	if (argc <= 0)
	{
		ShowInfo();
		System_Shutdown();

		return 1;
	}

	if (StrCaseCmp(argv[0], "/?") == 0 ||
		StrCaseCmp(argv[0], "-h") == 0 ||
		StrCaseCmp(argv[0], "-help") == 0 ||
		StrCaseCmp(argv[0], "--help") == 0)
	{
		ShowInfo();
		System_Shutdown();

		return 1;
	}

	ParseArgs(argc, argv);

	if (ValidateArgs() != DEH_OK)
		FatalError("%s", GetErrorMsg());

	if (Convert() != DEH_OK)
		FatalError("%s", GetErrorMsg());

	Shutdown();

	return 0;
}

}  // Deh_Edge

int main(int argc, char **argv)
{
	return Deh_Edge::main(argc, argv);
}

#endif  // DEH_EDGE_PLUGIN

//------------------------------------------------------------------------

#ifdef DEH_EDGE_PLUGIN

void DehEdgeStartup(const dehconvfuncs_t *funcs)
{
	Deh_Edge::Startup();
	Deh_Edge::cur_funcs = funcs;

	Deh_Edge::PrintMsg("*** DeHackEd -> EDGE Conversion Tool V%s ***\n",
		DEH_EDGE_VERS);
}

const char *DehEdgeGetError(void)
{
	return Deh_Edge::GetErrorMsg();
}

dehret_e DehEdgeSetVersion(int version)
{
	Deh_Edge::target_version = version;  // validated later

	return DEH_OK;
}

dehret_e DehEdgeSetQuiet(int quiet)
{
	Deh_Edge::quiet_mode = (quiet != 0);

	return DEH_OK;
}

dehret_e DehEdgeAddFile(const char *filename)
{
	return Deh_Edge::AddFile(filename);
}

dehret_e DehEdgeAddLump(const char *data, int length, const char *infoname)
{
	if (Deh_Edge::num_inputs >= MAX_INPUTS)
	{
		Deh_Edge::SetErrorMsg("Too many input lumps !!\n");
		return DEH_E_BadArgs;
	}

	Deh_Edge::input_bufs[Deh_Edge::num_inputs++] =
		new Deh_Edge::input_buffer_c(
			Deh_Edge::Buffer::OpenLump(data, length), infoname, true);

	return DEH_OK;
}

dehret_e DehEdgeRunConversion()
{
	dehret_e result = Deh_Edge::ValidateArgs();

	if (result != DEH_OK)
		return result;

	return Deh_Edge::Convert();
}

void DehEdgeShutdown(void)
{
	Deh_Edge::Shutdown();
	Deh_Edge::cur_funcs = NULL;
}

#endif  // DEH_EDGE_PLUGIN
