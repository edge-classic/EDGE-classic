//----------------------------------------------------------------------------
//  EDGE Data Definition File Codes (Switch textures)
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
// Switch Texture Setup and Parser Code
//

#include "local.h"

#include "switch.h"

static switchdef_c *dynamic_switchdef;

switchdef_container_c switchdefs;

#define DDF_CMD_BASE  dummy_switchdef
static switchdef_c dummy_switchdef;

static const commandlist_t switch_commands[] =
{
	DDF_FIELD("ON_TEXTURE",   on_name, DDF_MainGetLumpName),
	DDF_FIELD("OFF_TEXTURE", off_name, DDF_MainGetLumpName),
	DDF_FIELD("ON_SOUND",   on_sfx, DDF_MainLookupSound),
	DDF_FIELD("OFF_SOUND", off_sfx, DDF_MainLookupSound),
	DDF_FIELD("TIME", time, DDF_MainGetTime),

	// -AJA- backwards compatibility cruft...
	DDF_FIELD("SOUND", on_sfx, DDF_MainLookupSound),

	DDF_CMD_END
};


//
//  DDF PARSE ROUTINES
//

static void SwitchStartEntry(const char *name, bool extend)
{
	if (!name || !name[0])
	{
		DDF_WarnError("New switch entry is missing a name!");
		name = "SWITCH_WITH_NO_NAME";
	}

	dynamic_switchdef = switchdefs.Find(name);

	if (extend)
	{
		if (! dynamic_switchdef)
			DDF_Error("Unknown switch to extend: %s\n", name);
		return;
	}
	
	// replaces an existing entry
	if (dynamic_switchdef)
	{
		dynamic_switchdef->Default();
		return;
	}

	// not found, create a new one
	dynamic_switchdef = new switchdef_c;
	
	dynamic_switchdef->name = name;

	switchdefs.Insert(dynamic_switchdef);
}


static void SwitchParseField(const char *field, const char *contents,
		int index, bool is_last)
{
#if (DEBUG_DDF)  
	I_Debugf("SWITCH_PARSE: %s = %s;\n", field, contents);
#endif

	if (DDF_MainParseField(switch_commands, field, contents, (byte *)dynamic_switchdef))
		return;

	DDF_WarnError("Unknown switch.ddf command: %s\n", field);
}


static void SwitchFinishEntry(void)
{
	if (!dynamic_switchdef->on_name[0])
		DDF_Error("Missing first name for switch.\n");

	if (!dynamic_switchdef->off_name[0])
		DDF_Error("Missing last name for switch.\n");

	if (dynamic_switchdef->time <= 0)
		DDF_Error("Bad time value for switch: %d\n", dynamic_switchdef->time);
}


static void SwitchClearAll(void)
{
	// 100% safe to delete all switchdefs
	switchdefs.Clear();
}


void DDF_ReadSwitch(const std::string& data)
{
	readinfo_t switches;

	switches.tag = "SWITCHES";
	switches.lumpname = "DDFSWTH";

	switches.start_entry  = SwitchStartEntry;
	switches.parse_field  = SwitchParseField;
	switches.finish_entry = SwitchFinishEntry;
	switches.clear_all    = SwitchClearAll;

	DDF_MainReadFile(&switches, data);

#if (DEBUG_DDF)
	epi::array_iterator_c it;
	switchdef_c *sw;

	I_Debugf("DDF_ReadSW: Switch List:\n");

	for (it = switchdefs.GetBaseIterator(); it.IsValid(); it++)
	{
		sw = ITERATOR_TO_TYPE(it, switchdef_c*);
		
		I_Debugf("  Num: %d  ON: '%s'  OFF: '%s'\n", i, sw->on_name, sw->off_name);
	}
#endif
}

//
// DDF_SwitchInit
//
void DDF_SwitchInit(void)
{
	switchdefs.Clear();
}

//
// DDF_SwitchCleanUp
//
void DDF_SwitchCleanUp(void)
{
	switchdefs.Trim();
}



// ---> switchdef_c class

//
// switchdef_c Constructor
//
switchdef_c::switchdef_c() : name()
{
	Default();
}


//
// switchdef_c::CopyDetail()
//
// Copies all the detail with the exception of ddf info
//
void switchdef_c::CopyDetail(switchdef_c &src)
{
	 on_name = src.on_name;
	off_name = src.off_name;

	 on_sfx = src.on_sfx;
	off_sfx = src.off_sfx;

	time = src.time;
}

//
// switchdef_c::Default()
//
void switchdef_c::Default()
{
	 on_name.clear();
	off_name.clear();

	 on_sfx = sfx_None;
	off_sfx = sfx_None;

	time = BUTTONTIME;
}


// --> switchdef_container_c Class

//
// switchdef_container_c::CleanupObject()
//
void switchdef_container_c::CleanupObject(void *obj)
{
	switchdef_c *sw = *(switchdef_c**)obj;

	if (sw)
		delete sw;

	return;
}

//
// switchdef_c* switchdef_container_c::Find()
//
switchdef_c* switchdef_container_c::Find(const char *name)
{
	epi::array_iterator_c it;
	switchdef_c *sw;

	for (it = GetBaseIterator(); it.IsValid(); it++)
	{
		sw = ITERATOR_TO_TYPE(it, switchdef_c*);
		if (DDF_CompareName(sw->name.c_str(), name) == 0)
			return sw;
	}

	return NULL;
}

//----------------------------------------------------------------------------

void DDF_ConvertSWITCHES(const byte *data, int size)
{
	// handles the Boom SWITCHES lump (in a wad).

	if (size < 20)
		return;

	std::string text = "<SWITCHES>\n\n";

	for (; size >= 20 ; data += 20, size -= 20)
	{
		if (data[18] == 0)  // end marker
			break;

		char off_name[10];
		char  on_name[10];

		// make sure names are NUL-terminated
		memcpy(off_name, data+0, 9); off_name[8] = 0;
		memcpy( on_name, data+9, 9);  on_name[8] = 0;

		I_Debugf("- SWITCHES LUMP: off '%s' : on '%s'\n", off_name, on_name);

		// ignore zero-length names
		if (off_name[0] == 0 || on_name[0] == 0)
			continue;

		// create the DDF equivalent...
		text += "[";
		text += on_name;
		text += "]\n";

		text += "on_texture  = \"";
		text += on_name;
		text += "\";\n";

		text += "off_texture = \"";
		text += off_name;
		text += "\";\n";

		text += "on_sound  = \"SWTCHN\";\n";
		text += "off_sound = \"SWTCHN\";\n";
		text += "\n";
	}

	// DEBUG:
	// DDF_DumpFile(text);

	DDF_AddFile(DDF_Switch, text, "Boom SWITCHES lump");
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
