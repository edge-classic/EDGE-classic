//----------------------------------------------------------------------------
//  EDGE Data Definition File Codes (WAD-specific fixes)
//----------------------------------------------------------------------------
//
//  Copyright (c) 2022-2024 The EDGE Team.
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

#include "ddf_wadfixes.h"

#include "ddf_local.h"
#include "epi_str_util.h"

static WadFixDefinition *dynamic_fixdef;

WadFixDefinitionContainer fixdefs;

static WadFixDefinition dummy_fixdef;

static const DDFCommandList fix_commands[] = {DDF_FIELD("MD5", dummy_fixdef, md5_string_, DDFMainGetString),

                                              {nullptr, nullptr, 0, nullptr}};

//
//  DDF PARSE ROUTINES
//

static void FixStartEntry(const char *name, bool extend)
{
    if (!name || !name[0])
    {
        DDFWarnError("New wadfix entry is missing a name!");
        name = "FIX_WITH_NO_NAME";
    }

    dynamic_fixdef = fixdefs.Find(name);

    if (extend)
    {
        if (!dynamic_fixdef)
            DDFError("Unknown fix to extend: %s\n", name);
        return;
    }

    // replaces an existing entry
    if (dynamic_fixdef)
    {
        dynamic_fixdef->Default();
        return;
    }

    // not found, create a new one
    dynamic_fixdef = new WadFixDefinition;

    dynamic_fixdef->name_ = name;

    fixdefs.push_back(dynamic_fixdef);
}

static void FixFinishEntry(void)
{
    if (dynamic_fixdef->md5_string_.empty())
        DDFWarning("WADFIXES: No MD5 hash defined for %s.\n", dynamic_fixdef->name_.c_str());
}

static void FixParseField(const char *field, const char *contents, int index, bool is_last)
{
    EPI_UNUSED(index);
    EPI_UNUSED(is_last);
#if (DDF_DEBUG)
    LogDebug("FIX_PARSE: %s = %s;\n", field, contents);
#endif

    if (DDFMainParseField(fix_commands, field, contents, (uint8_t *)dynamic_fixdef))
        return;

    DDFWarnError("Unknown WADFIXES command: %s\n", field);
}

static void FixClearAll(void)
{
    for (WadFixDefinition *f : fixdefs)
    {
        delete f;
        f = nullptr;
    }
    fixdefs.clear();
}

void DDFReadFixes(const std::string &data)
{
    DDFReadInfo fixes;

    fixes.tag      = "FIXES";
    fixes.lumpname = "WADFIXES";

    fixes.start_entry  = FixStartEntry;
    fixes.parse_field  = FixParseField;
    fixes.finish_entry = FixFinishEntry;
    fixes.clear_all    = FixClearAll;

    DDFMainReadFile(&fixes, data);
}

//
// DDFFixInit
//
void DDFFixInit(void)
{
    FixClearAll();
}

//
// DDFFixCleanUp
//
void DDFFixCleanUp(void)
{
    for (WadFixDefinition *f : fixdefs)
    {
        cur_ddf_entryname = epi::StringFormat("[%s]  (wadfixes.ddf)", f->name_.c_str());
        cur_ddf_entryname.clear();
    }

    fixdefs.shrink_to_fit();
}

// ---> WadFixDefinition class

//
// WadFixDefinition Constructor
//
WadFixDefinition::WadFixDefinition() : name_()
{
    Default();
}

//
// WadFixDefinition::CopyDetail()
//
// Copies all the detail with the exception of ddf info
//
void WadFixDefinition::CopyDetail(const WadFixDefinition &src)
{
    md5_string_ = src.md5_string_;
}

//
// WadFixDefinition::Default()
//
void WadFixDefinition::Default()
{
    md5_string_ = "";
}

// --> WadFixDefinitionContainer Class

//
// WadFixDefinition* WadFixDefinitionContainer::Find()
//
WadFixDefinition *WadFixDefinitionContainer::Find(const char *name)
{
    for (std::vector<WadFixDefinition *>::iterator iter = begin(), iter_end = end(); iter != iter_end; iter++)
    {
        WadFixDefinition *fl = *iter;
        if (DDFCompareName(fl->name_.c_str(), name) == 0)
            return fl;
    }

    return nullptr;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
