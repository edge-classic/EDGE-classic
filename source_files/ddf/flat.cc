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

#include "flat.h"

#include <string.h>

#include "local.h"
#include "str_util.h"

static FlatDefinition *dynamic_flatdef;

FlatDefinitionContainer flatdefs;

#define DDF_CMD_BASE dummy_flatdef
static FlatDefinition dummy_flatdef;

static const commandlist_t flat_commands[] = {
    DDF_FIELD("LIQUID", liquid_, DDF_MainGetString),
    DDF_FIELD("FOOTSTEP", footstep_, DDF_MainLookupSound),
    DDF_FIELD("SPLASH", splash_, DDF_MainGetLumpName),
    DDF_FIELD("IMPACT_OBJECT", impactobject_ref_, DDF_MainGetString),
    DDF_FIELD("GLOW_OBJECT", glowobject_ref_, DDF_MainGetString),
    DDF_FIELD("SINK_DEPTH", sink_depth_, DDF_MainGetPercent),
    DDF_FIELD("BOB_DEPTH", bob_depth_, DDF_MainGetPercent),

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
        if (!dynamic_flatdef) DDF_Error("Unknown flat to extend: %s\n", name);
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

static void FlatFinishEntry(void) {}

static void FlatParseField(const char *field, const char *contents, int index,
                           bool is_last)
{
#if (DEBUG_DDF)
    I_Debugf("FLAT_PARSE: %s = %s;\n", field, contents);
#endif

    if (DDF_MainParseField(flat_commands, field, contents,
                           (uint8_t *)dynamic_flatdef))
        return;

    DDF_WarnError("Unknown flat.ddf command: %s\n", field);
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

void DDF_FlatInit(void) { FlatClearAll(); }

//
// DDF_FlatCleanUp
//
void DDF_FlatCleanUp(void)
{
    for (FlatDefinition *f : flatdefs)
    {
        cur_ddf_entryname =
            epi::StringFormat("[%s]  (flats.ddf)", f->name_.c_str());

        f->impactobject_ = f->impactobject_ref_ != ""
                               ? mobjtypes.Lookup(f->impactobject_ref_.c_str())
                               : nullptr;

        f->glowobject_ = f->glowobject_ref_ != ""
                             ? mobjtypes.Lookup(f->glowobject_ref_.c_str())
                             : nullptr;

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
        if (data[18] == 0)  // end marker
            break;

        char splash[10];

        // make sure names are NUL-terminated
        memcpy(splash, data + 0, 9);
        splash[8] = 0;

        // ignore zero-length names
        if (!splash[0]) continue;

        FlatDefinition *def = new FlatDefinition;

        def->name_ = "FLAT";

        def->Default();

        def->splash_ = splash;

        flatdefs.push_back(def);
    }
}

FlatDefinition::FlatDefinition() : name_() { Default(); }

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
    footstep_ = sfx_None;
    splash_.clear();
    impactobject_ = nullptr;
    impactobject_ref_.clear();
    glowobject_ = nullptr;
    glowobject_ref_.clear();
    sink_depth_ = PERCENT_MAKE(0);
    bob_depth_  = PERCENT_MAKE(0);
}

FlatDefinition *FlatDefinitionContainer::Find(const char *name)
{
    if (!name || !name[0]) return nullptr;

    for (std::vector<FlatDefinition *>::iterator iter     = begin(),
                                                 iter_end = end();
         iter != iter_end; iter++)
    {
        FlatDefinition *flt = *iter;
        if (DDF_CompareName(flt->name_.c_str(), name) == 0) return flt;
    }

    return nullptr;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
