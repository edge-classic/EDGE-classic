//----------------------------------------------------------------------------
//  EDGE Data Definition File Codes (Flat properties)
//----------------------------------------------------------------------------
// 
//  Copyright (c) 2022 The EDGE-Classic community.
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

#include "local.h"

#include "flat.h"

static flatdef_c *dynamic_flatdef;

flatdef_container_c flatdefs;

#define DDF_CMD_BASE  dummy_flatdef
static flatdef_c dummy_flatdef;

static const commandlist_t flat_commands[] =
{
	DDF_FIELD("LIQUID", liquid, DDF_MainGetString),
	DDF_FIELD("FOOTSTEP",   footstep, DDF_MainLookupSound),
	DDF_FIELD("SPLASH", splash, DDF_MainGetLumpName),
	DDF_FIELD("IMPACT_OBJECT", impactobject_ref, DDF_MainGetString),
	DDF_FIELD("GLOW_OBJECT", glowobject_ref, DDF_MainGetString),

	DDF_CMD_END
};


//
//  DDF PARSE ROUTINES
//

static void FlatStartEntry(const char *name, bool extend)
{
	if (!name || !name[0])
	{
		DDF_WarnError("New flat entry is missing a name!");
		name = "FLAT_WITH_NO_NAME";
	}

	dynamic_flatdef = flatdefs.Find(name);

	if (extend)
	{
		if (! dynamic_flatdef)
			DDF_Error("Unknown flat to extend: %s\n", name);
		return;
	}
	
	// replaces an existing entry
	if (dynamic_flatdef)
	{
		dynamic_flatdef->Default();
		return;
	}

	// not found, create a new one
	dynamic_flatdef = new flatdef_c;
	
	dynamic_flatdef->name = name;

	flatdefs.Insert(dynamic_flatdef);
}

static void FlatFinishEntry(void)
{
	// These are only warnings to inform the user in case they actually did want a footstep or splash - Dasho

	//Lobo 2022: very annoying warnings. Silenced ;)
	/*
	if (!dynamic_flatdef->footstep)
		DDF_Warning("DDFFLAT: No footstep sound defined for %s.\n", dynamic_flatdef->name.c_str());

	if (!dynamic_flatdef->splash[0])
		DDF_Warning("DDFFLAT: No splash sprite defined for %s.\n", dynamic_flatdef->name.c_str());

	*/
}

static void FlatParseField(const char *field, const char *contents,
		int index, bool is_last)
{
#if (DEBUG_DDF)  
	I_Debugf("FLAT_PARSE: %s = %s;\n", field, contents);
#endif

	if (DDF_MainParseField(flat_commands, field, contents, (byte *)dynamic_flatdef))
		return;

	DDF_WarnError("Unknown flat.ddf command: %s\n", field);
}

static void FlatClearAll(void)
{
	flatdefs.Clear();
}

bool DDF_ReadFlat(void *data, int size)
{
#if (DEBUG_DDF)
	epi::array_iterator_c it;
	flatdef_c *sw;
#endif

	readinfo_t flats;

	flats.memfile = (char*)data;
	flats.memsize = size;
	flats.tag = "FLATS";
	flats.entries_per_dot = 2;

	if (flats.memfile)
	{
		flats.message = NULL;
		flats.filename = NULL;
		flats.lumpname = "DDFFLAT";
	}
	else
	{
		flats.message = "DDF_InitFlats";
		flats.filename = "flats.ddf";
		flats.lumpname = NULL;
	}

	flats.start_entry  = FlatStartEntry;
	flats.parse_field  = FlatParseField;
	flats.finish_entry = FlatFinishEntry;
	flats.clear_all    = FlatClearAll;

	if (! DDF_MainReadFile(&flats))
		return false;

	return true;
}

//
// DDF_FlatInit
//
void DDF_FlatInit(void)
{
	flatdefs.Clear();
}

//
// DDF_FlatCleanUp
//
void DDF_FlatCleanUp(void)
{
	epi::array_iterator_c it;
	flatdef_c *f;
	
	for (it=flatdefs.GetBaseIterator(); it.IsValid(); it++)
	{
		f = ITERATOR_TO_TYPE(it, flatdef_c*);
		cur_ddf_entryname = epi::STR_Format("[%s]  (flats.ddf)", f->name.c_str());

		f->impactobject = f->impactobject_ref ?
			mobjtypes.Lookup(f->impactobject_ref) : NULL;
		
		f->glowobject = f->glowobject_ref ?
			mobjtypes.Lookup(f->glowobject_ref) : NULL;

		//f->effectobject = f->effectobject_ref.empty() ? 
		//		NULL : mobjtypes.Lookup(f->effectobject_ref);
		cur_ddf_entryname.clear();
	}
	
	flatdefs.Trim();
}

void DDF_ParseFLATS(const byte *data, int size)
{
	for (; size >= 20; data += 20, size -= 20)
	{
		if (data[18] == 0)  // end marker
			break;

		char splash[10];

		// make sure names are NUL-terminated
		memcpy(splash, data+0, 9); splash[8] = 0;

		// ignore zero-length names
		if (!splash[0])
			continue;

		flatdef_c *def = new flatdef_c;

		def->name = "FLAT";

		def->Default();
		
		def->splash.Set(splash);

		flatdefs.Insert(def);
	}
}


// ---> flatdef_c class

//
// flatdef_c Constructor
//
flatdef_c::flatdef_c() : name()
{
	Default();
}


//
// flatdef_c::CopyDetail()
//
// Copies all the detail with the exception of ddf info
//
void flatdef_c::CopyDetail(flatdef_c &src)
{
	liquid = src.liquid;
	footstep = src.footstep;
	splash = src.splash;
	impactobject = src.impactobject;	
	impactobject_ref = src.impactobject_ref;
	glowobject = src.glowobject;	
	glowobject_ref = src.glowobject_ref;
}

//
// flatdef_c::Default()
//
void flatdef_c::Default()
{
	liquid = "";
	footstep = sfx_None;
	splash.clear();
	impactobject = NULL;	
	impactobject_ref.clear();
	glowobject = NULL;	
	glowobject_ref.clear();
}


// --> flatdef_container_c Class

//
// flatdef_container_c::CleanupObject()
//
void flatdef_container_c::CleanupObject(void *obj)
{
	flatdef_c *fl = *(flatdef_c**)obj;

	if (fl)
		delete fl;

	return;
}

//
// flatdef_c* flatdef_container_c::Find()
//
flatdef_c* flatdef_container_c::Find(const char *name)
{
	epi::array_iterator_c it;
	flatdef_c *fl;

	for (it = GetBaseIterator(); it.IsValid(); it++)
	{
		fl = ITERATOR_TO_TYPE(it, flatdef_c*);
		if (DDF_CompareName(fl->name.c_str(), name) == 0)
			return fl;
	}

	return NULL;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
