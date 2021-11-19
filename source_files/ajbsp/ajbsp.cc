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

#include <time.h>

#ifndef WIN32
#include <time.h>
#endif


#define MAX_SPLIT_COST  32

//
//  global variables
//

int opt_verbosity = 0;  // 0 is normal, 1+ is verbose

bool opt_backup		= false;
bool opt_fast		= true;

bool opt_no_gl		= false;
bool opt_force_v5	= false;
bool opt_force_xnod	= false;
int  opt_split_cost	= DEFAULT_FACTOR;

const char *opt_output = NULL;

bool opt_help		= false;
bool opt_version	= false;


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

static void StopHanging()
{
	cur_funcs->log_printf("\n");
}


//
//  show an error message and terminate the program
//
void FatalError(const char *fmt, ...)
{
	cur_funcs->log_printf(fmt);

	exit(3);
}

void PrintMsg(const char *fmt, ...)
{
	cur_funcs->log_printf(fmt);
}

void PrintVerbose(const char *fmt, ...)
{
	cur_funcs->log_printf(fmt);
}


void PrintDetail(const char *fmt, ...)
{
	cur_funcs->log_printf(fmt);
}


void PrintMapName(const char *name)
{
	cur_funcs->log_printf(name);
}


void DebugPrintf(const char *fmt, ...)
{
	cur_funcs->log_debugf(fmt);
}

void UpdateProgress(int perc)
{
	cur_funcs->display_setBar(perc);
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

	progress_chunk = 100 / num_levels;

	int visited = 0;
	int failures = 0;

	// prepare the build info struct
	nodebuildinfo_t nb_info;

	nb_info.factor		= opt_split_cost;
	nb_info.gl_nodes	= ! opt_no_gl;
	nb_info.fast		= opt_fast;

	nb_info.force_v5	= opt_force_v5;
	nb_info.force_xnod	= opt_force_xnod;

	build_result_e res = BUILD_OK;

	// loop over each level in the wad
	for (int n = 0 ; n < num_levels ; n++)
	{
		if (! CheckMapInMaplist(n))
			continue;

		visited += 1;

		if (n > 0 && opt_verbosity >= 2)
			PrintMsg("\n");

		res = AJBSP_BuildLevel(&nb_info, n);

		// handle a failed map (due to lump overflow)
		if (res == BUILD_LumpOverflow)
		{
			res = BUILD_OK;
			failures += 1;
			continue;
		}

		if (res != BUILD_OK)
			break;

		total_built_maps += 1;
		UpdateProgress(total_built_maps * progress_chunk);
	}

	StopHanging();

	if (res == BUILD_Cancelled)
		return res;

	if (visited == 0)
	{
		PrintMsg("  No matching levels\n");
		total_empty_files += 1;
		return BUILD_OK;
	}

	PrintMsg("\n");

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
		PrintMsg("  Failed maps: %d (out of %d)\n", failures, visited);

		// allow building other files
		total_failed_files += 1;
	}

	if (true)
	{
		PrintMsg("  Serious warnings: %d\n", nb_info.total_warnings);
	}

	if (opt_verbosity >= 1)
	{
		PrintMsg("  Minor issues: %d\n", nb_info.total_minor_issues);
	}

	return BUILD_OK;
}


void ValidateInputFilename(const char *filename)
{
	// NOTE: these checks are case-insensitive

	// files with ".bak" extension cannot be backed up, so refuse them
	if (MatchExtension(filename, "bak"))
		FatalError("cannot process a backup file: %s\n", filename);

	// GWA files only contain GL-nodes, never any maps
	if (MatchExtension(filename, "gwa"))
		FatalError("cannot process a GWA file: %s\n", filename);

	// we do not support packages
	if (MatchExtension(filename, "pak") || MatchExtension(filename, "pk2") ||
		MatchExtension(filename, "pk3") || MatchExtension(filename, "pk4") ||
		MatchExtension(filename, "pk7") ||
		MatchExtension(filename, "epk") || MatchExtension(filename, "pack") ||
		MatchExtension(filename, "zip") || MatchExtension(filename, "rar"))
	{
		FatalError("package files (like PK3) are not supported: %s\n", filename);
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
		FatalError("not a wad file: %s\n", filename);
	}
}


void BackupFile(const char *filename)
{
	const char *dest_name = ReplaceExtension(filename, "bak");

	if (! FileCopy(filename, dest_name))
		FatalError("failed to create backup: %s\n", dest_name);

	PrintVerbose("\nCreated backup: %s\n", dest_name);

	StringFree(dest_name);
}


void VisitFile(unsigned int idx, const char *filename)
{

	PrintMsg("\n");
	PrintMsg("Building %s\n", opt_output);

	edit_wad = Wad_file::Open(filename, 'r');
	if (! edit_wad)
		FatalError("Cannot open file: %s\n", filename);

	gwa_wad = Wad_file::Open(opt_output, 'a');

	if (gwa_wad->IsReadOnly())
	{
		delete gwa_wad; gwa_wad = NULL;

		FatalError("output file is read only: %s\n", filename);
	}

	build_result_e res = BuildFile();

	// this closes the file
	delete edit_wad; edit_wad = NULL;
	delete gwa_wad; gwa_wad = NULL;

	if (res == BUILD_Cancelled)
		FatalError("CANCELLED\n");
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
		FatalError("illegal map name: '%s'\n", low);

	if (! ValidateMapName(high))
		FatalError("illegal map name: '%s'\n", high);

	if (strlen(low) < strlen(high))
		FatalError("bad map range (%s shorter than %s)\n", low, high);

	if (strlen(low) > strlen(high))
		FatalError("bad map range (%s longer than %s)\n", low, high);

	if (low[0] != high[0])
		FatalError("bad map range (%s and %s start with different letters)\n", low, high);

	if (strcmp(low, high) > 0)
		FatalError("bad map range (wrong order, %s > %s)\n", low, high);

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
			FatalError("bad map list (empty element)\n");

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
		FatalError("no files to process\n");
		return 0;
	}

	if (opt_output != NULL)
	{
		if (opt_backup)
			FatalError("cannot use --backup with --output\n");

		if (total_files > 1)
			FatalError("cannot use multiple input files with --output\n");

		if (y_stricmp(wad_list[0], opt_output) == 0)
			FatalError("input and output files are the same\n");
	}

	// validate all filenames before processing any of them
	for (unsigned int i = 0 ; i < wad_list.size() ; i++)
	{
		const char *filename = wad_list[i];

		ValidateInputFilename(filename);

		if (! FileExists(filename))
			FatalError("no such file: %s\n", filename);
	}

	UpdateProgress(0);

	for (unsigned int i = 0 ; i < wad_list.size() ; i++)
	{
		VisitFile(i, wad_list[i]);
	}

	PrintMsg("\n");

	if (total_failed_files > 0)
	{
		PrintMsg("FAILURES occurred on %d map%s in %d file%s.\n",
				total_failed_maps,  total_failed_maps  == 1 ? "" : "s",
				total_failed_files, total_failed_files == 1 ? "" : "s");

		if (opt_verbosity == 0)
			PrintMsg("Rerun with --verbose to see more details.\n");

		return 0; // Some failures are tolerable and shouldn't close EDGE
	}
	else if (total_built_maps == 0)
	{
		PrintMsg("NOTHING was built!\n");

		return 1;
	}
	else if (total_empty_files == 0)
	{
		PrintMsg("Ok, built all files.\n");
	}
	else
	{
		int built = total_files - total_empty_files;
		int empty = total_empty_files;

		PrintMsg("Ok, built %d file%s, %d file%s empty.\n",
				built, (built == 1 ? "" : "s"),
				empty, (empty == 1 ? " was" : "s were"));
	}

	UpdateProgress(100);

	total_built_maps = 0;

	cur_funcs = NULL;

	wad_list.clear();

	// that's all folks!
	return 0;
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
