//------------------------------------------------------------------------
//  MAIN PROGRAM
//------------------------------------------------------------------------
//
//  AJ-BSP  Copyright (C) 2001-2018  Andrew Apted
//          Copyright (C) 1994-1998  Colin Reed
//          Copyright (C) 1997-1998  Lee Killough
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
//------------------------------------------------------------------------

#include "ajbsp.h"

//
//  global variables
//

const char *opt_output = NULL;

std::vector< const char * > wad_list;

const char *Level_name;

map_format_e Level_format;

int total_failed_files = 0;
int total_empty_files = 0;
int total_built_maps = 0;
int total_failed_maps = 0;


typedef struct map_range_s
{
	const char *low;
	const char *high;

} map_range_t;

std::vector< map_range_t > map_list;

const nodebuildfuncs_t *cur_funcs = NULL;

int progress_chunk = 0;

//
//  show an error message and terminate the program
//
void FatalError(const char *fmt)
{
	if (gwa_wad)
	{
		delete gwa_wad; 
		gwa_wad = NULL;
	}
	if (FileExists(opt_output))
		FileDelete(opt_output);
	cur_funcs->log_error(fmt);
}

void PrintMsg(const char *fmt)
{
	cur_funcs->log_printf(fmt);
}

void PrintVerbose(const char *fmt)
{
	cur_funcs->log_debugf(fmt);
}


void PrintDetail(const char *fmt)
{
	cur_funcs->log_debugf(fmt);
}


void PrintMapName(const char *name)
{
	cur_funcs->log_printf(name);
}


void DebugPrintf(const char *fmt)
{
	cur_funcs->log_debugf(fmt);
}

void UpdateProgress(const char *message)
{
	cur_funcs->progress_message(message);
}
//------------------------------------------------------------------------

static bool CheckMapInRange(const map_range_t *range, const char *name)
{
	if (strlen(name) != strlen(range->low))
		return false;

	if (strcmp(name, range->low) < 0)
		return false;

	if (strcmp(name, range->high) > 0)
		return false;

	return true;
}


static bool CheckMapInMaplist(short lev_idx)
{
	// when --map is not used, allow everything
	if (map_list.empty())
		return true;

	short lump_idx = edit_wad->LevelHeader(lev_idx);

	const char *name = edit_wad->GetLump(lump_idx)->Name();

	for (unsigned int i = 0 ; i < map_list.size() ; i++)
		if (CheckMapInRange(&map_list[i], name))
			return true;

	return false;
}


static build_result_e BuildFile()
{
	int num_levels = edit_wad->LevelCount();

	if (num_levels == 0)
	{
		PrintMsg("  No levels in wad\n");
		total_empty_files += 1;
		return BUILD_OK;
	}

	int visited = 0;
	int failures = 0;

	// prepare the build info struct
	nodebuildinfo_t *nb_info = new nodebuildinfo_t;

	build_result_e res = BUILD_OK;

	// loop over each level in the wad
	for (int n = 0 ; n < num_levels ; n++)
	{
		if (! CheckMapInMaplist(n))
			continue;

		visited += 1;


		res = AJBSP_BuildLevel(nb_info, n);

		if (res != BUILD_OK)
			break;

		total_built_maps += 1;
	}

	if (res == BUILD_Cancelled)
		return res;

	if (visited == 0)
	{
		PrintMsg("  No matching levels\n");
		total_empty_files += 1;
		return BUILD_OK;
	}

	total_failed_maps += failures;

	if (res == BUILD_BadFile)
	{
		PrintMsg("  Corrupted wad or level detected.\n");

		// allow building other files
		total_failed_files += 1;
		return BUILD_OK;
	}

	if (failures > 0)
	{
		// allow building other files
		total_failed_files += 1;
	}

	return BUILD_OK;
}


void ValidateInputFilename(const char *filename)
{
	// NOTE: these checks are case-insensitive

	// files with ".bak" extension cannot be backed up, so refuse them
	if (MatchExtension(filename, "bak"))
		FatalError(StringPrintf("cannot process a backup file: %s\n", filename));

	// GWA files only contain GL-nodes, never any maps
	if (MatchExtension(filename, "gwa"))
		FatalError(StringPrintf("cannot process a GWA file: %s\n", filename));

	// we do not support packages
	if (MatchExtension(filename, "pak") || MatchExtension(filename, "pk2") ||
		MatchExtension(filename, "pk3") || MatchExtension(filename, "pk4") ||
		MatchExtension(filename, "pk7") ||
		MatchExtension(filename, "epk") || MatchExtension(filename, "pack") ||
		MatchExtension(filename, "zip") || MatchExtension(filename, "rar"))
	{
		FatalError(StringPrintf("package files (like PK3) are not supported: %s\n", filename));
	}

	// check some very common formats
	if (MatchExtension(filename, "exe") || MatchExtension(filename, "dll") ||
		MatchExtension(filename, "com") || MatchExtension(filename, "bat") ||
		MatchExtension(filename, "txt") || MatchExtension(filename, "doc") ||
		MatchExtension(filename, "deh") || MatchExtension(filename, "bex") ||
		MatchExtension(filename, "lmp") || MatchExtension(filename, "cfg") ||
		MatchExtension(filename, "gif") || MatchExtension(filename, "png") ||
		MatchExtension(filename, "jpg") || MatchExtension(filename, "jpeg"))
	{
		FatalError(StringPrintf("not a wad file: %s\n", filename));
	}
}

void VisitFile(unsigned int idx, const char *filename)
{

	edit_wad = Wad_file::Open(filename, 'r');
	if (! edit_wad)
		FatalError(StringPrintf("Cannot open file: %s\n", filename));

	gwa_wad = Wad_file::Open(opt_output, 'w');

	if (gwa_wad->IsReadOnly())
	{
		delete gwa_wad; gwa_wad = NULL;

		FatalError(StringPrintf("output file is read only: %s\n", filename));
	}

	build_result_e res = BuildFile();

	// this closes the file
	delete edit_wad; edit_wad = NULL;
	delete gwa_wad; gwa_wad = NULL;

	if (res == BUILD_Cancelled)
		FatalError(StringPrintf("CANCELLED\n"));
}

bool ValidateMapName(char *name)
{
	if (strlen(name) < 2 || strlen(name) > 8)
		return false;

	if (! isalpha(name[0]))
		return false;

	for (const char *p = name ; *p ; p++)
	{
		if (! (isalnum(*p) || *p == '_'))
			return false;
	}

	// Ok, convert to upper case
	for (char *s = name ; *s ; s++)
	{
		*s = toupper(*s);
	}

	return true;
}


void ParseMapRange(char *tok)
{
	char *low  = tok;
	char *high = tok;

	// look for '-' separator
	char *p = strchr(tok, '-');

	if (p)
	{
		*p++ = 0;

		high = p;
	}

	if (! ValidateMapName(low))
		FatalError(StringPrintf("illegal map name: '%s'\n", low));

	if (! ValidateMapName(high))
		FatalError(StringPrintf("illegal map name: '%s'\n", high));

	if (strlen(low) < strlen(high))
		FatalError(StringPrintf("bad map range (%s shorter than %s)\n", low, high));

	if (strlen(low) > strlen(high))
		FatalError(StringPrintf("bad map range (%s longer than %s)\n", low, high));

	if (low[0] != high[0])
		FatalError(StringPrintf("bad map range (%s and %s start with different letters)\n", low, high));

	if (strcmp(low, high) > 0)
		FatalError(StringPrintf("bad map range (wrong order, %s > %s)\n", low, high));

	// Ok

	map_range_t range;

	range.low  = low;
	range.high = high;

	map_list.push_back(range);
}


void ParseMapList(const char *from_arg)
{
	// create a mutable copy of the string
	// [ we will keep long-term pointers into this buffer ]
	char *buf = StringDup(from_arg);

	char *arg = buf;

	while (*arg)
	{
		if (*arg == ',')
			FatalError(StringPrintf("bad map list (empty element)\n"));

		// find next comma
		char *tok = arg;
		arg++;

		while (*arg && *arg != ',')
			arg++;

		if (*arg == ',')
		{
			*arg++ = 0;
		}

		ParseMapRange(tok);
	}
}

//
//  the program starts here
//
int AJBSP_Build(const char *filename, const char *outname, const nodebuildfuncs_t *display_funcs)
{

	wad_list.push_back(filename);
	opt_output = outname;
	cur_funcs = display_funcs;

	// sanity check on type sizes (useful when porting)
	CheckTypeSizes();

	int total_files = (int)wad_list.size();

	if (total_files == 0)
	{
		FatalError(StringPrintf("no files to process\n"));
		return 0;
	}

	if (opt_output != NULL)
	{
		if (total_files > 1)
			FatalError(StringPrintf("cannot use multiple input files with --output\n"));

		if (y_stricmp(wad_list[0], opt_output) == 0)
			FatalError(StringPrintf("input and output files are the same\n"));
	}

	// validate all filenames before processing any of them
	for (unsigned int i = 0 ; i < wad_list.size() ; i++)
	{
		const char *filename = wad_list[i];

		ValidateInputFilename(filename);

		if (! FileExists(filename))
			FatalError(StringPrintf("no such file: %s\n", filename));
	}

	for (unsigned int i = 0 ; i < wad_list.size() ; i++)
	{
		VisitFile(i, wad_list[i]);
	}

	if (total_failed_files > 0)
	{
		PrintMsg("Non-fatal errors occurred on at least one map!.\n");

		return 0; // "Failures" in this case can mean things like overflowing the original Doom engine limits and shouldn't close EDGE
	}
	else if (total_built_maps == 0)
	{
		PrintMsg("NOTHING was built!\n");

		return 1;
	}
	else if (total_empty_files == 0)
	{
		PrintMsg("File processed successfully!\n");
	}
	else
	{
		int built = total_files - total_empty_files;
		int empty = total_empty_files;

		PrintMsg("Done, but file is empty!\n");
	}

	total_built_maps = 0;

	cur_funcs = NULL;

	wad_list.clear();

	// that's all folks!
	return 0;
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
