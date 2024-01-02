//----------------------------------------------------------------------------
//  EDGE Data Definition File Codes (WAD-specific fixes)
//----------------------------------------------------------------------------
//
//  Copyright (c) 2022-2023 The EDGE Team.
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

#include "local.h"
#include "wadfixes.h"

#include "str_util.h"

static fixdef_c *dynamic_fixdef;

fixdef_container_c fixdefs;

#define DDF_CMD_BASE dummy_fixdef
static fixdef_c dummy_fixdef;

static const commandlist_t fix_commands[] = {DDF_FIELD("MD5", md5_string, DDF_MainGetString),

                                             DDF_CMD_END};

//
//  DDF PARSE ROUTINES
//

static void FixStartEntry(const char *name, bool extend)
{
    if (!name || !name[0])
    {
        DDF_WarnError("New wadfix entry is missing a name!");
        name = "FIX_WITH_NO_NAME";
    }

    dynamic_fixdef = fixdefs.Find(name);

    if (extend)
    {
        if (!dynamic_fixdef)
            DDF_Error("Unknown fix to extend: %s\n", name);
        return;
    }

    // replaces an existing entry
    if (dynamic_fixdef)
    {
        dynamic_fixdef->Default();
        return;
    }

    // not found, create a new one
    dynamic_fixdef = new fixdef_c;

    dynamic_fixdef->name = name;

    fixdefs.push_back(dynamic_fixdef);
}

static void FixFinishEntry(void)
{
    if (dynamic_fixdef->md5_string.empty())
        DDF_Warning("WADFIXES: No MD5 hash defined for %s.\n", dynamic_fixdef->name.c_str());
}

static void FixParseField(const char *field, const char *contents, int index, bool is_last)
{
#if (DEBUG_DDF)
    I_Debugf("FIX_PARSE: %s = %s;\n", field, contents);
#endif

    if (DDF_MainParseField(fix_commands, field, contents, (byte *)dynamic_fixdef))
        return;

    DDF_WarnError("Unknown WADFIXES command: %s\n", field);
}

static void FixClearAll(void)
{
    for (auto f : fixdefs)
    {
        delete f;
        f = nullptr;
    }
    fixdefs.clear();
}

void DDF_ReadFixes(const std::string &data)
{
    readinfo_t fixes;

    fixes.tag      = "FIXES";
    fixes.lumpname = "WADFIXES";

    fixes.start_entry  = FixStartEntry;
    fixes.parse_field  = FixParseField;
    fixes.finish_entry = FixFinishEntry;
    fixes.clear_all    = FixClearAll;

    DDF_MainReadFile(&fixes, data);
}

//
// DDF_FixInit
//
void DDF_FixInit(void)
{
    FixClearAll();
}

//
// DDF_FixCleanUp
//
void DDF_FixCleanUp(void)
{
    for (auto f : fixdefs)
    {
        cur_ddf_entryname = epi::STR_Format("[%s]  (wadfixes.ddf)", f->name.c_str());
        cur_ddf_entryname.clear();
    }

    fixdefs.shrink_to_fit();
}

// ---> fixdef_c class

//
// fixdef_c Constructor
//
fixdef_c::fixdef_c() : name()
{
    Default();
}

//
// fixdef_c::CopyDetail()
//
// Copies all the detail with the exception of ddf info
//
void fixdef_c::CopyDetail(fixdef_c &src)
{
    md5_string = src.md5_string;
}

//
// fixdef_c::Default()
//
void fixdef_c::Default()
{
    md5_string = "";
}

// --> fixdef_container_c Class

//
// fixdef_c* fixdef_container_c::Find()
//
fixdef_c *fixdef_container_c::Find(const char *name)
{
    for (auto iter = begin(); iter != end(); iter++)
    {
        fixdef_c *fl = *iter;
        if (DDF_CompareName(fl->name.c_str(), name) == 0)
            return fl;
    }

    return NULL;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
