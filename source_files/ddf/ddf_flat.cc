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

#include "ddf_flat.h"

#include <string.h>

#include "ddf_local.h"
#include "epi_str_util.h"

static FlatDefinition *dynamic_flatdef;

FlatDefinitionContainer flatdefs;

static FlatDefinition dummy_flatdef;

static const DDFCommandList flat_commands[] = {
    DDF_FIELD("LIQUID", dummy_flatdef, liquid_, DDFMainGetString),
    DDF_FIELD("FOOTSTEP", dummy_flatdef, footstep_, DDFMainLookupSound),
    DDF_FIELD("SPLASH", dummy_flatdef, splash_, DDFMainGetLumpName),
    DDF_FIELD("IMPACT_OBJECT", dummy_flatdef, impactobject_ref_, DDFMainGetString),
    DDF_FIELD("GLOW_OBJECT", dummy_flatdef, glowobject_ref_, DDFMainGetString),
    DDF_FIELD("SINK_DEPTH", dummy_flatdef, sink_depth_, DDFMainGetPercent),
    DDF_FIELD("BOB_DEPTH", dummy_flatdef, bob_depth_, DDFMainGetPercent),

    {nullptr, nullptr, 0, nullptr}};

//
//  DDF PARSE ROUTINES
//

static void FlatStartEntry(const char *name, bool extend)
{
    if (!name || !name[0])
    {
        DDFWarnError("New flat entry is missing a name!");
        name = "FLAT_WITH_NO_NAME";
    }

    dynamic_flatdef = flatdefs.Find(name);

    if (extend)
    {
        if (!dynamic_flatdef)
            DDFError("Unknown flat to extend: %s\n", name);
        return;
    }

    // replaces an existing entry
    if (dynamic_flatdef)
    {
        dynamic_flatdef->Default();
        return;
    }

    // not found, create a new one
    dynamic_flatdef = new FlatDefinition;

    dynamic_flatdef->name_ = name;

    flatdefs.push_back(dynamic_flatdef);
}

static void FlatFinishEntry(void)
{
}

static void FlatParseField(const char *field, const char *contents, int index, bool is_last)
{
#if (DDF_DEBUG)
    LogDebug("FLAT_PARSE: %s = %s;\n", field, contents);
#endif
    EPI_UNUSED(index);
    EPI_UNUSED(is_last);
    if (DDFMainParseField(flat_commands, field, contents, (uint8_t *)dynamic_flatdef))
        return;

    DDFWarnError("Unknown flat.ddf command: %s\n", field);
}

static void FlatClearAll(void)
{
    for (FlatDefinition *flt : flatdefs)
    {
        delete flt;
        flt = nullptr;
    }
    flatdefs.clear();
}

void DDFReadFlat(const std::string &data)
{
    DDFReadInfo flats;

    flats.tag      = "FLATS";
    flats.lumpname = "DDFFLAT";

    flats.start_entry  = FlatStartEntry;
    flats.parse_field  = FlatParseField;
    flats.finish_entry = FlatFinishEntry;
    flats.clear_all    = FlatClearAll;

    DDFMainReadFile(&flats, data);
}

void DDFFlatInit(void)
{
    FlatClearAll();
}

//
// DDFFlatCleanUp
//
void DDFFlatCleanUp(void)
{
    for (FlatDefinition *f : flatdefs)
    {
        cur_ddf_entryname = epi::StringFormat("[%s]  (flats.ddf)", f->name_.c_str());

        f->impactobject_ = f->impactobject_ref_ != "" ? mobjtypes.Lookup(f->impactobject_ref_.c_str()) : nullptr;

        f->glowobject_ = f->glowobject_ref_ != "" ? mobjtypes.Lookup(f->glowobject_ref_.c_str()) : nullptr;

        // f->effectobject = f->effectobject_ref.empty() ?
        //		nullptr : mobjtypes.Lookup(f->effectobject_ref);
        cur_ddf_entryname.clear();
    }

    flatdefs.shrink_to_fit();
}

void DDFParseFlats(const uint8_t *data, int size)
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

        FlatDefinition *def = new FlatDefinition;

        def->name_ = "FLAT";

        def->Default();

        def->splash_ = splash;

        flatdefs.push_back(def);
    }
}

FlatDefinition::FlatDefinition() : name_()
{
    Default();
}

void FlatDefinition::CopyDetail(FlatDefinition &src)
{
    liquid_           = src.liquid_;
    footstep_         = src.footstep_;
    splash_           = src.splash_;
    impactobject_     = src.impactobject_;
    impactobject_ref_ = src.impactobject_ref_;
    glowobject_       = src.glowobject_;
    glowobject_ref_   = src.glowobject_ref_;
    sink_depth_       = src.sink_depth_;
    bob_depth_        = src.bob_depth_;
}

void FlatDefinition::Default()
{
    liquid_   = "";
    footstep_ = nullptr;
    splash_.clear();
    impactobject_ = nullptr;
    impactobject_ref_.clear();
    glowobject_ = nullptr;
    glowobject_ref_.clear();
    sink_depth_ = 0.0f;
    bob_depth_  = 0.0f;
}

FlatDefinition *FlatDefinitionContainer::Find(const char *name)
{
    if (!name || !name[0])
        return nullptr;

    for (std::vector<FlatDefinition *>::iterator iter = begin(), iter_end = end(); iter != iter_end; iter++)
    {
        FlatDefinition *flt = *iter;
        if (DDFCompareName(flt->name_.c_str(), name) == 0)
            return flt;
    }

    return nullptr;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
