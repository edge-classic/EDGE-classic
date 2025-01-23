//----------------------------------------------------------------------------
//  EDGE Data Definition File Codes (Switch textures)
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2024 The EDGE Team.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
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

#include "ddf_switch.h"

#include <string.h>

#include "ddf_local.h"

static SwitchDefinition *dynamic_switchdef;

SwitchDefinitionContainer switchdefs;

static SwitchDefinition dummy_switchdef;

static const DDFCommandList switch_commands[] = {
    DDF_FIELD("ON_TEXTURE", dummy_switchdef, on_name_, DDFMainGetLumpName),
    DDF_FIELD("OFF_TEXTURE", dummy_switchdef, off_name_, DDFMainGetLumpName),
    DDF_FIELD("ON_SOUND", dummy_switchdef, on_sfx_, DDFMainLookupSound),
    DDF_FIELD("OFF_SOUND", dummy_switchdef, off_sfx_, DDFMainLookupSound),
    DDF_FIELD("TIME", dummy_switchdef, time_, DDFMainGetTime),

    // -AJA- backwards compatibility cruft...
    DDF_FIELD("SOUND", dummy_switchdef, on_sfx_, DDFMainLookupSound),

    {nullptr, nullptr, 0, nullptr}};

//
//  DDF PARSE ROUTINES
//

static void SwitchStartEntry(const char *name, bool extend)
{
    if (!name || !name[0])
    {
        DDFWarnError("New switch entry is missing a name!");
        name = "SWITCH_WITH_NO_NAME";
    }

    dynamic_switchdef = switchdefs.Find(name);

    if (extend)
    {
        if (!dynamic_switchdef)
            DDFError("Unknown switch to extend: %s\n", name);
        return;
    }

    // replaces an existing entry
    if (dynamic_switchdef)
    {
        dynamic_switchdef->Default();
        return;
    }

    // not found, create a new one
    dynamic_switchdef = new SwitchDefinition;

    dynamic_switchdef->name_ = name;

    switchdefs.push_back(dynamic_switchdef);
}

static void SwitchParseField(const char *field, const char *contents, int index, bool is_last)
{
    EPI_UNUSED(index);
    EPI_UNUSED(is_last);
#if (DDF_DEBUG)
    LogDebug("SWITCH_PARSE: %s = %s;\n", field, contents);
#endif

    if (DDFMainParseField(switch_commands, field, contents, (uint8_t *)dynamic_switchdef))
        return;

    DDFWarnError("Unknown switch.ddf command: %s\n", field);
}

static void SwitchFinishEntry(void)
{
    if (!dynamic_switchdef->on_name_[0])
        DDFError("Missing first name for switch.\n");

    if (!dynamic_switchdef->off_name_[0])
        DDFError("Missing last name for switch.\n");

    if (dynamic_switchdef->time_ <= 0)
        DDFError("Bad time value for switch: %d\n", dynamic_switchdef->time_);
}

static void SwitchClearAll(void)
{
    // 100% safe to delete all switchdefs
    for (auto s : switchdefs)
    {
        delete s;
        s = nullptr;
    }
    switchdefs.clear();
}

void DDFReadSwitch(const std::string &data)
{
    DDFReadInfo switches;

    switches.tag      = "SWITCHES";
    switches.lumpname = "DDFSWTH";

    switches.start_entry  = SwitchStartEntry;
    switches.parse_field  = SwitchParseField;
    switches.finish_entry = SwitchFinishEntry;
    switches.clear_all    = SwitchClearAll;

    DDFMainReadFile(&switches, data);

#if (DDF_DEBUG)
    epi::array_iterator_c it;
    SwitchDefinition     *sw;

    LogDebug("DDFReadSW: Switch List:\n");

    for (it = switchdefs.GetBaseIterator(); it.IsValid(); it++)
    {
        sw = ITERATOR_TO_TYPE(it, SwitchDefinition *);

        LogDebug("  Num: %d  ON: '%s'  OFF: '%s'\n", i, sw->on_name, sw->off_name);
    }
#endif
}

//
// DDFSwitchInit
//
void DDFSwitchInit(void)
{
    SwitchClearAll();
}

//
// DDFSwitchCleanUp
//
void DDFSwitchCleanUp(void)
{
    switchdefs.shrink_to_fit();
}

// ---> SwitchDefinition class

//
// SwitchDefinition Constructor
//
SwitchDefinition::SwitchDefinition() : name_()
{
    Default();
}

//
// SwitchDefinition::CopyDetail()
//
// Copies all the detail with the exception of ddf info
//
void SwitchDefinition::CopyDetail(const SwitchDefinition &src)
{
    on_name_  = src.on_name_;
    off_name_ = src.off_name_;

    on_sfx_  = src.on_sfx_;
    off_sfx_ = src.off_sfx_;

    time_ = src.time_;
}

//
// SwitchDefinition::Default()
//
void SwitchDefinition::Default()
{
    on_name_.clear();
    off_name_.clear();

    on_sfx_  = nullptr;
    off_sfx_ = nullptr;

    time_ = 35;
}

// --> SwitchDefinitionContainer Class

//
// SwitchDefinition* SwitchDefinitionContainer::Find()
//
SwitchDefinition *SwitchDefinitionContainer::Find(const char *name)
{
    for (std::vector<SwitchDefinition *>::iterator iter = begin(), iter_end = end(); iter != iter_end; iter++)
    {
        SwitchDefinition *sw = *iter;
        if (DDFCompareName(sw->name_.c_str(), name) == 0)
            return sw;
    }

    return nullptr;
}

//----------------------------------------------------------------------------

void DDFConvertSwitchesLump(const uint8_t *data, int size)
{
    // handles the Boom SWITCHES lump (in a wad).

    if (size < 20)
        return;

    std::string text = "<SWITCHES>\n\n";

    for (; size >= 20; data += 20, size -= 20)
    {
        if (data[18] == 0) // end marker
            break;

        char off_name[9];
        char on_name[9];

        // clear to zeroes to prevent garbage being passed to DDF
        EPI_CLEAR_MEMORY(off_name, char, 9);
        EPI_CLEAR_MEMORY(on_name, char, 9);

        // make sure names are NUL-terminated
        memcpy(off_name, data + 0, 8);
        off_name[8] = 0;
        memcpy(on_name, data + 9, 8);
        on_name[8] = 0;

        LogDebug("- SWITCHES LUMP: off '%s' : on '%s'\n", off_name, on_name);

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
    // DDFDumpFile(text);

    DDFAddFile(kDDFTypeSwitch, text, "Boom SWITCHES lump");
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
