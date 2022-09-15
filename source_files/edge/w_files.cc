//----------------------------------------------------------------------------
//  EDGE file handling
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
//
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------

#include "i_defs.h"

#include <list>
#include <vector>
#include <algorithm>

// EPI
#include "file.h"
#include "file_sub.h"
#include "filesystem.h"
#include "path.h"

// DDF
#include "main.h"
#include "anim.h"
#include "colormap.h"
#include "font.h"
#include "image.h"
#include "style.h"
#include "switch.h"
#include "flat.h"
#include "wadfixes.h"

#include "dstrings.h"
#include "dm_state.h"
#include "rad_trig.h"
#include "w_files.h"
#include "w_wad.h"


std::vector<data_file_c *> data_files;


data_file_c::data_file_c(const char *_name, int _kind) :
		name(_name), kind(_kind), file(NULL), wad(NULL)
{ }

data_file_c::~data_file_c()
{ }

int W_GetNumFiles(void)
{
	return (int)data_files.size();
}

//
// W_AddFilename
//
size_t W_AddFilename(const char *file, int kind)
{
	I_Debugf("Added filename: %s\n", file);

	size_t index = data_files.size();

	data_file_c *df = new data_file_c(file, kind);
	data_files.push_back(df);

	return index;
}

//----------------------------------------------------------------------------

std::vector<data_file_c *> pending_files;

size_t W_AddPending(const char *file, int kind)
{
	size_t index = pending_files.size();

	data_file_c *df = new data_file_c(file, kind);
	pending_files.push_back(df);

	return index;
}

// TODO tidy this
extern void ProcessFixers(data_file_c *df);
extern void ProcessDehacked(data_file_c *df);
extern void ProcessWad(data_file_c *df, size_t file_index);
extern void ProcessSingleLump(data_file_c *df);

extern std::string W_BuildNodesForWad(data_file_c *df);


void W_ReadWADFIXES(void)
{
	I_Printf("Loading WADFIXES\n");

	int length;
	char *data = (char *) W_LoadLump("WADFIXES", &length);

	DDF_ReadFixes(data, length);

	W_DoneWithLump(data);
}


static void ProcessFile(data_file_c *df)
{
	size_t file_index = data_files.size();
	data_files.push_back(df);

	// open a WAD/PK3 file and add contents to directory
	const char *filename = df->name.c_str();

	I_Printf("  Adding %s\n", filename);

	if (df->kind <= FLKIND_HWad || df->kind == FLKIND_PK3)
	{
		epi::file_c *file = epi::FS_Open(filename, epi::file_c::ACCESS_READ | epi::file_c::ACCESS_BINARY);
		if (file == NULL)
		{
			I_Error("Couldn't open file %s\n", filename);
			return;
		}

		df->file = file;
	}

	// for DDF and RTS, adding the data_file is enough
	if (df->kind == FLKIND_RTS || df->kind == FLKIND_DDF)
		return;

	if (df->kind <= FLKIND_HWad)
	{
		ProcessWad(df, file_index);

		if (file_index == 0)  // "edge-defs.wad"
			W_ReadWADFIXES();
	}
	else
	{
		ProcessSingleLump(df);
	}

	// handle DeHackEd patch files
	ProcessDehacked(df);

	// handle fixer-uppers
	ProcessFixers(df);
}


//
// W_InitMultipleFiles
//
void W_InitMultipleFiles(void)
{
	// open all the files, add all the lumps.
	// NOTE: we rebuild the list, since new files can get added as we go along,
	//       and they should appear *after* the one which produced it.

	std::vector<data_file_c *> copied_files(data_files);
	data_files.clear();

	for (size_t i = 0 ; i < copied_files.size() ; i++)
	{
		ProcessFile(copied_files[i]);

		for (size_t k = 0 ; k < pending_files.size() ; k++)
		{
			ProcessFile(pending_files[k]);
		}

		pending_files.clear();
	}

//??	if (lumpinfo.empty())
//??		I_Error("W_InitMultipleFiles: no files found!\n");
}


//
// W_BuildNodes
//
void W_BuildNodes(void)
{
	for (size_t i = 0 ; i < data_files.size() ; i++)
	{
		data_file_c *df = data_files[i];

		if (df->wad != NULL)
		{
			std::string gwa_filename = W_BuildNodesForWad(df);

			if (! gwa_filename.empty())
			{
				size_t new_index = W_AddFilename(gwa_filename.c_str(), FLKIND_GWad);

				ProcessFile(data_files[new_index]);
			}
		}
	}
}

//----------------------------------------------------------------------------

// TODO move to header
extern int W_GetDDFLump(wad_file_c *wad, int d);
extern int W_GetAnimated(wad_file_c *wad);
extern int W_GetSwitches(wad_file_c *wad);
extern void W_AddColourmaps(wad_file_c *wad);


// -KM- 1999/01/31 Order is important, Languages are loaded before sfx, etc...
typedef struct ddf_reader_s
{
	const char *lump_name;
	const char *pack_name;
	const char *print_name;
	bool (* func)(void *data, int size);
}
ddf_reader_t;

static ddf_reader_t DDF_Readers[] =
{
	{ "DDFLANG",  "language.ldf", "Languages",  DDF_ReadLangs },
	{ "DDFSFX",   "sounds.ddf",   "Sounds",     DDF_ReadSFX },
	{ "DDFCOLM",  "colmap.ddf",   "ColourMaps", DDF_ReadColourMaps },
	{ "DDFIMAGE", "images.ddf",   "Images",     DDF_ReadImages },
	{ "DDFFONT",  "fonts.ddf",    "Fonts",      DDF_ReadFonts },
	{ "DDFSTYLE", "styles.ddf",   "Styles",     DDF_ReadStyles },
	{ "DDFATK",   "attacks.ddf",  "Attacks",    DDF_ReadAtks },
	{ "DDFWEAP",  "weapons.ddf",  "Weapons",    DDF_ReadWeapons },
	{ "DDFTHING", "things.ddf",   "Things",     DDF_ReadThings },

	{ "DDFPLAY",  "playlist.ddf", "Playlists",  DDF_ReadMusicPlaylist },
	{ "DDFLINE",  "lines.ddf",    "Lines",      DDF_ReadLines },
	{ "DDFSECT",  "sectors.ddf",  "Sectors",    DDF_ReadSectors },
	{ "DDFSWTH",  "switch.ddf",   "Switches",   DDF_ReadSwitch },
	{ "DDFANIM",  "anims.ddf",    "Anims",      DDF_ReadAnims },
	{ "DDFGAME",  "games.ddf",    "Games",      DDF_ReadGames },
	{ "DDFLEVL",  "levels.ddf",   "Levels",     DDF_ReadLevels },
	{ "DDFFLAT",  "flats.ddf",    "Flats",      DDF_ReadFlat },

	{ "RSCRIPT",  "rscript.rts",  "RadTrig",    RAD_ReadScript }
};

#define NUM_DDF_READERS  (int)(sizeof(DDF_Readers) / sizeof(ddf_reader_t))


int W_CheckDDFLumpName(const char *name)
{
	for (int d=0; d < NUM_DDF_READERS; d++)
	{
		if (strncmp(name, DDF_Readers[d].lump_name, 8) == 0)
			return d;
	}
	return -1;  // nope
}


static void W_ReadDDF_FromDir(int d)
{
	// no directory specified?
	if (ddf_dir.empty())
		return;

	std::string filename = epi::PATH_Join(ddf_dir.c_str(), DDF_Readers[d].pack_name);

	epi::file_c *F = epi::FS_Open(filename.c_str(), epi::file_c::ACCESS_READ);
	if (F == NULL)
	{
		// ignore files which don't exist
		return;
	}

	I_Printf("Loading %s from: %s\n", DDF_Readers[d].lump_name, filename.c_str());

	char *data = (char *) F->LoadIntoMemory();
	if (data == NULL)
		I_Error("Couldn't read file: %s\n", filename.c_str());

	int length = (int)strlen(data);  // TODO make it not needed

	// call read function
	(* DDF_Readers[d].func)(data, length);

	delete[] data;
	delete F;
}


static void W_ReadDDF_FromFile(data_file_c *df, int d)
{
	wad_file_c  *wad  = df->wad;
	pack_file_c *pack = df->pack;

	const char * lump_name = DDF_Readers[d].lump_name;

	// handle external scripts (from `-script` or `-file` option)
	if (strcmp(lump_name, "RSCRIPT") == 0 && df->kind == FLKIND_RTS)
	{
		I_Printf("Loading RTS script: %s\n", df->name.c_str());

		epi::file_c *F = epi::FS_Open(df->name.c_str(), epi::file_c::ACCESS_READ);
		if (F == NULL)
			I_Error("Couldn't open file: %s\n", df->name.c_str());

		char *data = (char *) F->LoadIntoMemory();
		if (data == NULL)
			I_Error("Couldn't read file: %s\n", df->name.c_str());

		RAD_ReadScript(data, -1);

		delete[] data;
		delete F;

		return;
	}

	// handle external ddf/ldf files (from `-file` option)
	if (df->kind == FLKIND_DDF)
	{
		std::string base_name = epi::PATH_GetFilename(df->name.c_str());

		if (stricmp(base_name.c_str(), DDF_Readers[d].pack_name) == 0)
		{
			I_Printf("Loading %s from: %s\n", DDF_Readers[d].lump_name, df->name.c_str());

			epi::file_c *F = epi::FS_Open(df->name.c_str(), epi::file_c::ACCESS_READ);
			if (F == NULL)
				I_Error("Couldn't open file: %s\n", df->name.c_str());

			char *data = (char *) F->LoadIntoMemory();
			if (data == NULL)
				I_Error("Couldn't read file: %s\n", df->name.c_str());

			int length = (int)strlen(data);  // TODO make it not needed

			// call read function
			(* DDF_Readers[d].func)(data, length);

			delete[] data;
			delete F;

			return;
		}

		/* FIXME this don't work, do it another way
		if (d == NUM_DDF_READERS-1)
			I_Error("Unknown DDF filename: %s\n", base_name.c_str());
		*/
		return;
	}

	if (df->kind >= FLKIND_RTS)
		return;

	if (wad != NULL)
	{
		int lump = W_GetDDFLump(wad, d);

		if (lump >= 0)
		{
			I_Printf("Loading %s from: %s\n", DDF_Readers[d].lump_name, df->name.c_str());

			int length;
			char *data = (char *) W_LoadLump(lump, &length);

			// call read function
			(* DDF_Readers[d].func)(data, length);

			W_DoneWithLump(data);
		}
	}

	if (pack != NULL)
	{
		// TODO : PK3
	}

	if (wad != NULL)
	{
		// handle Boom's ANIMATED and SWITCHES lumps

		int animated = W_GetAnimated(wad);
		int switches = W_GetSwitches(wad);

		if (strcmp(lump_name, "DDFANIM") == 0 && animated >= 0)
		{
			I_Printf("Loading ANIMATED from: %s\n", df->name.c_str());

			int length;
			byte *data = W_LoadLump(animated, &length);

			DDF_ParseANIMATED(data, length);
			W_DoneWithLump(data);
		}

		if (strcmp(lump_name, "DDFSWTH") == 0 && switches >= 0)
		{
			I_Printf("Loading SWITCHES from: %s\n", df->name.c_str());

			int length;
			byte *data = W_LoadLump(switches, &length);

			DDF_ParseSWITCHES(data, length);
			W_DoneWithLump(data);
		}

		// handle BOOM Colourmaps (between C_START and C_END)
		if (strcmp(lump_name, "DDFCOLM") == 0)
		{
			W_AddColourmaps(wad);
		}
	}
}


void W_ReadDDF(void)
{
	// -AJA- the order here may look strange.  Since DDF files
	// have dependencies between them, it makes more sense to
	// load all lumps of a certain type together (e.g. all
	// DDFSFX lumps before all the DDFTHING lumps).

	for (int d = 0; d < NUM_DDF_READERS; d++)
	{
		I_Printf("Loading %s\n", DDF_Readers[d].print_name);

		for (int f = 0; f < (int)data_files.size(); f++)
		{
			data_file_c *df = data_files[f];

			W_ReadDDF_FromFile(df, d);
		}

		// handle the `-ddf` option.
		// files from that directory are done AFTER all other ones.
		W_ReadDDF_FromDir(d);

/* helpful ???
		std::string msg_buf(epi::STR_Format(
			"Loaded %s %s\n", (d == NUM_DDF_READERS-1) ? "RTS" : "DDF",
				DDF_Readers[d].print_name));

		I_Printf(msg_buf.c_str());
*/
	}
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
