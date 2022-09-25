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

// DEHACKED
#include "deh_edge.h"

#include "con_main.h"
#include "dstrings.h"
#include "dm_state.h"
#include "l_deh.h"
#include "rad_trig.h"
#include "w_files.h"
#include "w_wad.h"


DEF_CVAR(debug_dehacked, "0", CVAR_ARCHIVE)


std::vector<data_file_c *> data_files;


data_file_c::data_file_c(const char *_name, filekind_e _kind) :
		name(_name), kind(_kind), file(NULL), wad(NULL), deh(NULL)
{ }

data_file_c::~data_file_c()
{ }

int W_GetNumFiles()
{
	return (int)data_files.size();
}

//
// W_AddFilename
//
size_t W_AddFilename(const char *file, filekind_e kind)
{
	I_Debugf("Added filename: %s\n", file);

	size_t index = data_files.size();

	data_file_c *df = new data_file_c(file, kind);
	data_files.push_back(df);

	return index;
}

//----------------------------------------------------------------------------

std::vector<data_file_c *> pending_files;

size_t W_AddPending(const char *file, filekind_e kind)
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
extern void ProcessPackage(data_file_c *df, size_t file_index);

extern std::string W_BuildNodesForWad(data_file_c *df);


void W_ReadWADFIXES(void)
{
	I_Printf("Loading WADFIXES\n");

	auto data = W_LoadString("WADFIXES");

	DDF_ReadFixes(data);
}


static void ProcessFile(data_file_c *df)
{
	size_t file_index = data_files.size();
	data_files.push_back(df);

	// open a WAD/PK3 file and add contents to directory
	const char *filename = df->name.c_str();

	I_Printf("  Adding %s\n", filename);

	if (df->kind <= FLKIND_GWad || df->kind == FLKIND_PK3)
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

	if (df->kind <= FLKIND_GWad)
	{
		ProcessWad(df, file_index);

		if (file_index == 0)  // "edge-defs.wad"
			W_ReadWADFIXES();
	}
	else if (df->kind == FLKIND_PK3)
	{
		ProcessPackage(df, file_index);
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
	void (* func)(const std::string& data);
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


static void W_ReadExternalDDF(int d, epi::file_c * F, const std::string& filename)
{
	// WISH: load directly into a std::string

	char *raw_data = (char *) F->LoadIntoMemory();
	if (raw_data == NULL)
		I_Error("Couldn't read file: %s\n", filename.c_str());

	std::string data(raw_data);
	delete[] raw_data;

	// call read function
	(* DDF_Readers[d].func)(data);

	// close file
	delete F;
}


static void W_ReadDDF_FromDir(data_file_c *df, int d)
{
	std::string filename = epi::PATH_Join(df->name.c_str(), DDF_Readers[d].pack_name);

	epi::file_c *F = epi::FS_Open(filename.c_str(), epi::file_c::ACCESS_READ);
	if (F == NULL)
	{
		// ignore files which don't exist
		return;
	}

	I_Printf("Loading %s from: %s\n", DDF_Readers[d].lump_name, filename.c_str());

	W_ReadExternalDDF(d, F, filename);
}


static void W_ReadDDF_DataFile(data_file_c *df, int d)
{
	if (df->kind == FLKIND_Folder)
	{
		W_ReadDDF_FromDir(df, d);
		return;
	}

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

		W_ReadExternalDDF(NUM_DDF_READERS-1, F, df->name);
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

			W_ReadExternalDDF(d, F, df->name);
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

			std::string data = W_LoadString(lump);

			// call read function
			(* DDF_Readers[d].func)(data);
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


static void W_ReadDehacked(data_file_c *df, int d)
{
	deh_container_c *deh = df->deh;
	if (deh == NULL)
		return;

	// look for the appropriate lump (DDFTHING etc)
	for (size_t i = 0 ; i < deh->lumps.size() ; i++)
	{
		deh_lump_c * lump = deh->lumps[i];

		if (strcmp(lump->name.c_str(), DDF_Readers[d].lump_name) == 0)
		{
			std::string where = df->name;
			if (df->wad != NULL)
			{
				where = "DEHACKED in ";
				where += df->name;
			}

			I_Printf("Loading %s from: %s\n", DDF_Readers[d].lump_name, where.c_str());

			const char *data = lump->data.c_str();

			if (debug_dehacked.d)
			{
				I_Debugf("\n");

				// we need to break it into lines
				const char *pos = data;
				const char *end;

				while (*pos != 0)
				{
					for (end = pos ; *end != 0 ; end++)
					{
						if (*end == '\n')
						{
							end++;
							break;
						}
					}

					std::string line(pos, end - pos);
					I_Debugf("%s", line.c_str());

					pos = end;
				}
			}

			// call read function
			(* DDF_Readers[d].func)(lump->data);

			// free up some memory
			lump->data.clear();
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

		for (int i = 0; i < (int)data_files.size(); i++)
		{
			data_file_c *df = data_files[i];

			W_ReadDDF_DataFile(df, d);
			W_ReadDehacked(df, d);
		}
	}
}

//----------------------------------------------------------------------------

static const char *FileKindString(filekind_e kind)
{
	switch (kind)
	{
		case FLKIND_IWad:   return "iwad";
		case FLKIND_PWad:   return "pwad";
		case FLKIND_EWad:   return "edge";
		case FLKIND_GWad:   return "gwa";

		case FLKIND_Folder: return "DIR";
		case FLKIND_PK3:    return "pk3";

		case FLKIND_DDF:    return "ddf";
		case FLKIND_RTS:    return "rts";
		case FLKIND_Deh:    return "deh";

		default: return "???";
	}
}


void W_ShowFiles()
{
	I_Printf("File list:\n");

	for (int i = 0; i < (int)data_files.size(); i++)
	{
		data_file_c *df = data_files[i];

		I_Printf(" %2d: %-4s \"%s\"\n", i+1, FileKindString(df->kind), df->name.c_str());
	}
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
