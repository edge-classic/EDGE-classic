//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Colourmaps)
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2008  The EDGE Team.
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
// Colourmap handling.
//

#include "local.h"

#include "colormap.h"

static colourmap_c *dynamic_colmap;

colourmap_container_c colourmaps;

void DDF_ColmapGetSpecial(const char *info, void *storage);

#define DDF_CMD_BASE  dummy_colmap
static colourmap_c dummy_colmap;

static const commandlist_t colmap_commands[] =
{
	DDF_FIELD("LUMP",    lump_name, DDF_MainGetLumpName),
	DDF_FIELD("START",   start,     DDF_MainGetNumeric),
	DDF_FIELD("LENGTH",  length,    DDF_MainGetNumeric),
	DDF_FIELD("SPECIAL", special,   DDF_ColmapGetSpecial),
	DDF_FIELD("GL_COLOUR", gl_colour, DDF_MainGetRGB),

	DDF_CMD_END
};


//
//  DDF PARSE ROUTINES
//

static void ColmapStartEntry(const char *name, bool extend)
{
	if (!name || name[0] == 0)
	{
		DDF_WarnError("New colormap entry is missing a name!");
		name = "COLORMAP_WITH_NO_NAME";
	}

	dynamic_colmap = colourmaps.Lookup(name);

	if (extend)
	{
		if (! dynamic_colmap)
			DDF_Error("Unknown colormap to extend: %s\n", name);
		return;
	}

	// replaces the existing entry
	if (dynamic_colmap)
	{
		dynamic_colmap->Default();

		if (strnicmp(name, "TEXT", 4) == 0)
			dynamic_colmap->special = COLSP_Whiten;

		return;
	}

	// not found, create a new one
	dynamic_colmap = new colourmap_c;

	dynamic_colmap->name.Set(name);

	// make sure fonts get whitened properly (as the default)
	if (strnicmp(name, "TEXT", 4) == 0)
		dynamic_colmap->special = COLSP_Whiten;

	colourmaps.Insert(dynamic_colmap);
}


static void ColmapParseField(const char *field, const char *contents,
    int index, bool is_last)
{
#if (DEBUG_DDF)  
	I_Debugf("COLMAP_PARSE: %s = %s;\n", field, contents);
#endif

	// -AJA- backwards compatibility cruft...
	if (DDF_CompareName(field, "PRIORITY") == 0)
		return;

	if (DDF_MainParseField(colmap_commands, field, contents, (byte *)dynamic_colmap))
		return;  // OK

	DDF_WarnError("Unknown colmap.ddf command: %s\n", field);
}


static void ColmapFinishEntry(void)
{
	if (dynamic_colmap->start < 0)
	{
		DDF_WarnError("Bad START value for colmap: %d\n", dynamic_colmap->start);
		dynamic_colmap->start = 0;
	}

	// don't need a length when using GL_COLOUR
	if (! dynamic_colmap->lump_name.empty() && dynamic_colmap->length <= 0)
	{
		DDF_WarnError("Bad LENGTH value for colmap: %d\n", dynamic_colmap->length);
		dynamic_colmap->length = 1;
	}

	if (dynamic_colmap->lump_name.empty() && dynamic_colmap->gl_colour == RGB_NO_VALUE)
		DDF_Error("Colourmap entry missing LUMP or GL_COLOUR.\n");
}


static void ColmapClearAll(void)
{
	I_Warning("Ignoring #CLEARALL in colormap.ddf\n");
}


bool DDF_ReadColourMaps(void *data, int size)
{
	readinfo_t colm_r;

	colm_r.memfile = (char*)data;
	colm_r.memsize = size;
	colm_r.tag = "COLOURMAPS";
	colm_r.entries_per_dot = 2;

	if (colm_r.memfile)
	{
		colm_r.message = NULL;
		colm_r.filename = NULL;
		colm_r.lumpname = "DDFCOLM";
	}
	else
	{
		colm_r.message = "DDF_InitColourMaps";
		colm_r.filename = "colmap.ddf";
		colm_r.lumpname = NULL;
	}

	colm_r.start_entry  = ColmapStartEntry;
	colm_r.parse_field  = ColmapParseField;
	colm_r.finish_entry = ColmapFinishEntry;
	colm_r.clear_all    = ColmapClearAll;

	return DDF_MainReadFile(&colm_r);
}


void DDF_ColmapInit(void)
{
	colourmaps.Clear();
}


void DDF_ColmapCleanUp(void)
{
	colourmaps.Trim();
}


specflags_t colmap_specials[] =
{
	{"FLASH",  COLSP_NoFlash, true},
	{"WHITEN", COLSP_Whiten,  false},

	// -AJA- backwards compatibility cruft...
	{"SKY", 0, 0},

	{NULL, 0, 0}
};


//
// DDF_ColmapGetSpecial
//
// Gets the colourmap specials.
//
void DDF_ColmapGetSpecial(const char *info, void *storage)
{
	colourspecial_e *spec = (colourspecial_e *)storage;

	int flag_value;

	switch (DDF_MainCheckSpecialFlag(info, colmap_specials, &flag_value,
				true, false))
	{
		case CHKF_Positive:
			*spec = (colourspecial_e)(*spec | flag_value);
			break;

		case CHKF_Negative:
			*spec = (colourspecial_e)(*spec & ~flag_value);
			break;

		case CHKF_User:
		case CHKF_Unknown:
			DDF_WarnError("DDF_ColmapGetSpecial: Unknown Special: %s", info);
			break;
	}
}


// 
// This is used to make entries for lumps between C_START and C_END
// markers in a (BOOM) WAD file.
//
void DDF_ColourmapAddRaw(const char *lump_name, int size)
{
	if (size < 256)
	{
		I_Warning("WAD Colourmap '%s' too small (%d < %d)\n", lump_name, size, 256);
		return;
	}

	colourmap_c *def = colourmaps.Lookup(lump_name);

	// not found, create a new one
	if (! def)
	{
		def = new colourmap_c;

		colourmaps.Insert(def);
	}

	def->Default();

	def->name = lump_name;

	def->lump_name.Set(lump_name);

	def->start  = 0;
	def->length = MIN(32, size / 256);

	I_Debugf("- Added RAW colourmap '%s' start=%d length=%d\n",
		lump_name, def->start, def->length);
}


// --> Colourmap Class

//
// colourmap_c Constructor
//
colourmap_c::colourmap_c() : name()
{
	Default();
}

//
// colourmap_c Deconstructor
//
colourmap_c::~colourmap_c()
{
}

//
// colourmap_c::CopyDetail()
//
void colourmap_c::CopyDetail(colourmap_c &src)
{
	lump_name = src.lump_name;

	start   = src.start;
	length  = src.length;
	special = src.special;

	gl_colour   = src.gl_colour;
	font_colour = src.font_colour;

	// FIXME!!! Cache struct to class
	cache.data = src.cache.data;
	analysis = NULL;
}

//
// colourmap_c::Default()
//
void colourmap_c::Default()
{
	lump_name.clear();

	start   = 0;
	length  = 0;
	special = COLSP_None;
	
	gl_colour   = RGB_NO_VALUE;
	font_colour = RGB_NO_VALUE;

	// FIXME!!! Cache struct to class
	cache.data = NULL;
	analysis = NULL;
}


// --> colourmap_container_c class

//
// colourmap_container_c::colourmap_container_c()
//
colourmap_container_c::colourmap_container_c() : epi::array_c(sizeof(atkdef_c*))
{
}

//
// ~colourmap_container_c::colourmap_container_c()
//
colourmap_container_c::~colourmap_container_c()
{
	Clear();					// <-- Destroy self before exiting
}

//
// colourmap_container_c::CleanupObject
//
void colourmap_container_c::CleanupObject(void *obj)
{
	colourmap_c *c = *(colourmap_c**)obj;

	if (c)
		delete c;

	return;
}

//
// colourmap_c* colourmap_container_c::Lookup()
//
colourmap_c* colourmap_container_c::Lookup(const char *refname)
{
	epi::array_iterator_c it;
	colourmap_c *c;

	if (!refname || !refname[0])
		return NULL;

	for (it = GetIterator(0); it.IsValid(); it++)
	{
		c = ITERATOR_TO_TYPE(it, colourmap_c*);
		if (DDF_CompareName(c->name.c_str(), refname) == 0)
			return c;
	}

	return NULL;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
