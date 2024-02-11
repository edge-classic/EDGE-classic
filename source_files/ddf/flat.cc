//----------------------------------------------------------------------------
//  EDGE Data Definition File Codes (Flat properties)
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

#include <string.h>

#include "local.h"
#include "flat.h"

#include "str_util.h"

static flatdef_c *dynamic_flatdef;

flatdef_container_c flatdefs;

#define DDF_CMD_BASE dummy_flatdef
static flatdef_c dummy_flatdef;

static const commandlist_t flat_commands[] = {DDF_FIELD("LIQUID", liquid, DDF_MainGetString),
                                              DDF_FIELD("FOOTSTEP", footstep, DDF_MainLookupSound),
                                              DDF_FIELD("SPLASH", splash, DDF_MainGetLumpName),
                                              DDF_FIELD("IMPACT_OBJECT", impactobject_ref, DDF_MainGetString),
                                              DDF_FIELD("GLOW_OBJECT", glowobject_ref, DDF_MainGetString),
                                              DDF_FIELD("SINK_DEPTH", sink_depth, DDF_MainGetPercent),
                                              DDF_FIELD("BOB_DEPTH", bob_depth, DDF_MainGetPercent),

                                              DDF_CMD_END};

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
        if (!dynamic_flatdef)
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

    flatdefs.push_back(dynamic_flatdef);
}

static void FlatFinishEntry(void)
{

}

static void FlatParseField(const char *field, const char *contents, int index, bool is_last)
{
#if (DEBUG_DDF)
    I_Debugf("FLAT_PARSE: %s = %s;\n", field, contents);
#endif

    if (DDF_MainParseField(flat_commands, field, contents, (uint8_t *)dynamic_flatdef))
        return;

    DDF_WarnError("Unknown flat.ddf command: %s\n", field);
}

static void FlatClearAll(void)
{
    for (auto flt : flatdefs)
    {
        delete flt;
        flt = nullptr;
    }
    flatdefs.clear();
}

void DDF_ReadFlat(const std::string &data)
{
    readinfo_t flats;

    flats.tag      = "FLATS";
    flats.lumpname = "DDFFLAT";

    flats.start_entry  = FlatStartEntry;
    flats.parse_field  = FlatParseField;
    flats.finish_entry = FlatFinishEntry;
    flats.clear_all    = FlatClearAll;

    DDF_MainReadFile(&flats, data);
}

void DDF_FlatInit(void)
{
    FlatClearAll();
}

//
// DDF_FlatCleanUp
//
void DDF_FlatCleanUp(void)
{
    for (auto f : flatdefs)
    {
        cur_ddf_entryname = epi::StringFormat("[%s]  (flats.ddf)", f->name.c_str());

        f->impactobject = f->impactobject_ref != "" ? mobjtypes.Lookup(f->impactobject_ref.c_str()) : nullptr;

        f->glowobject = f->glowobject_ref != "" ? mobjtypes.Lookup(f->glowobject_ref.c_str()) : nullptr;

        // f->effectobject = f->effectobject_ref.empty() ?
        //		nullptr : mobjtypes.Lookup(f->effectobject_ref);
        cur_ddf_entryname.clear();
    }

    flatdefs.shrink_to_fit();
}

void DDF_ParseFLATS(const uint8_t *data, int size)
{
    for (; size >= 20; data += 20, size -= 20)
    {
        if (data[18] == 0) // end marker
            break;

        char splash[10];

        // make sure names are NUL-terminated
        memcpy(splash, data + 0, 9);
        splash[8] = 0;

        // ignore zero-length names
        if (!splash[0])
            continue;

        flatdef_c *def = new flatdef_c;

        def->name = "FLAT";

        def->Default();

        def->splash = splash;

        flatdefs.push_back(def);
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
    liquid           = src.liquid;
    footstep         = src.footstep;
    splash           = src.splash;
    impactobject     = src.impactobject;
    impactobject_ref = src.impactobject_ref;
    glowobject       = src.glowobject;
    glowobject_ref   = src.glowobject_ref;
    sink_depth       = src.sink_depth;
    bob_depth        = src.bob_depth;
}

//
// flatdef_c::Default()
//
void flatdef_c::Default()
{
    liquid   = "";
    footstep = sfx_None;
    splash.clear();
    impactobject = nullptr;
    impactobject_ref.clear();
    glowobject = nullptr;
    glowobject_ref.clear();
    sink_depth = PERCENT_MAKE(0);
    bob_depth  = PERCENT_MAKE(0);
}

//
// flatdef_c* flatdef_container_c::Find()
//
flatdef_c *flatdef_container_c::Find(const char *name)
{
    if (!name || !name[0])
        return nullptr;

    for (auto iter = begin(); iter != end(); iter++)
    {
        flatdef_c *flt = *iter;
        if (DDF_CompareName(flt->name.c_str(), name) == 0)
            return flt;
    }

    return nullptr;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
