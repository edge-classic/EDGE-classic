//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Reverbs)
//----------------------------------------------------------------------------
//
//  Copyright (c) 2025 The EDGE Team.
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
// Reverb Setup and Parser Code
//

#include "ddf_reverb.h"

#include <string.h>

#include "ddf_local.h"

static ReverbDefinition *dynamic_reverb;

static ReverbDefinition dummy_reverb;

static const DDFCommandList reverb_commands[] = {DDF_FIELD("ROOM_SIZE", dummy_reverb, room_size_, DDFMainGetPercent),
                                                DDF_FIELD("DAMPING_LEVEL", dummy_reverb, damping_level_, DDFMainGetPercent),
                                                DDF_FIELD("WET_LEVEL", dummy_reverb, wet_level_, DDFMainGetPercent),
                                                DDF_FIELD("DRY_LEVEL", dummy_reverb, dry_level_, DDFMainGetPercent),
                                                DDF_FIELD("REVERB_WIDTH", dummy_reverb, reverb_width_, DDFMainGetPercent),
                                                {nullptr, nullptr, 0, nullptr}};

ReverbDefinitionContainer reverbdefs;

//
//  DDF PARSE ROUTINES
//

static void ReverbStartEntry(const char *name, bool extend)
{
    if (!name || !name[0])
        DDFError("New movie entry is missing a name!\n");

    epi::EName verb_ename = DDFCreateEName(name);

    if (reverbdefs.find(verb_ename) != reverbdefs.end())
        dynamic_reverb = reverbdefs[verb_ename];
    else
        dynamic_reverb = nullptr;

    if (extend)
    {
        if (!dynamic_reverb)
            DDFError("Unknown movie to extend: %s\n", name);
        return;
    }

    // replaces an existing entry?
    if (dynamic_reverb)
    {
        dynamic_reverb->Default();
        return;
    }

    // not found, create a new one
    dynamic_reverb = new ReverbDefinition;

    dynamic_reverb->name_ = verb_ename;

    reverbdefs.try_emplace(verb_ename, dynamic_reverb);
}

static void ReverbParseField(const char *field, const char *contents, int index, bool is_last)
{
#if (DDF_DEBUG)
    LogDebug("REVERB_PARSE: %s = %s;\n", field, contents);
#endif
    EPI_UNUSED(index);
    EPI_UNUSED(is_last);
    if (DDFMainParseField(reverb_commands, field, contents, (uint8_t *)dynamic_reverb))
        return; // OK

    DDFError("Unknown reverbs.ddf command: %s\n", field);
}

static void ReverbFinishEntry(void)
{
    // Bounds checking for params? - Dasho
}

static void ReverbClearAll(void)
{
    LogWarning("Ignoring #CLEARALL in reverbs.ddf\n");
}

void DDFReadReverbs(const std::string &data)
{
    DDFReadInfo reverbs;

    reverbs.tag      = "REVERBS";
    reverbs.lumpname = "DDFVERB";

    reverbs.start_entry  = ReverbStartEntry;
    reverbs.parse_field  = ReverbParseField;
    reverbs.finish_entry = ReverbFinishEntry;
    reverbs.clear_all    = ReverbClearAll;

    DDFMainReadFile(&reverbs, data);
}

void DDFReverbsInit(void)
{
    for (std::unordered_map<epi::EName, ReverbDefinition *>::iterator iter = reverbdefs.begin(), iter_end = reverbdefs.end(); iter != iter_end; iter++)
    {
        ReverbDefinition *verb = iter->second;
        delete verb;
        verb = nullptr;
    }
    reverbdefs.clear();
}

void DDFReverbsCleanUp(void)
{

}

// ---> ReverbDefinition class

ReverbDefinition::ReverbDefinition() : name_(epi::kENameNone)
{
    Default();
}

//
// Copies all the detail with the exception of ddf info
//
void ReverbDefinition::CopyDetail(const ReverbDefinition &src)
{
    room_size_ = src.room_size_;
    damping_level_ = src.damping_level_;
    wet_level_ = src.wet_level_;
    dry_level_ = src.dry_level_;
    reverb_width_ = src.reverb_width_;
}

void ReverbDefinition::ApplyReverb(verblib *reverb)
{
    verblib_set_room_size(reverb, room_size_);
    verblib_set_damping(reverb, damping_level_);
    verblib_set_wet(reverb, wet_level_);
    verblib_set_dry(reverb, dry_level_);
    verblib_set_width(reverb, reverb_width_);
}

void ReverbDefinition::Default()
{
    room_size_ = 0.0f;
    damping_level_ = 0.0f;
    wet_level_ = 0.0f;
    dry_level_ = 0.0f;
    reverb_width_ = 0.0f;
}

// ---> ReverbDefinitionContainer class

ReverbDefinition *ReverbDefinitionContainer::Lookup(std::string_view refname)
{
    if (refname.empty())
        return nullptr;

    epi::EName verb_ename = DDFCreateEName(refname);

    if (reverbdefs.find(verb_ename) != reverbdefs.end())
        return reverbdefs[verb_ename];
    else
        return nullptr;
}

ReverbDefinition *ReverbDefinitionContainer::Lookup(epi::KnownEName ref_ename)
{
    if (ref_ename == epi::kENameNone)
        return nullptr;

    if (reverbdefs.find(ref_ename) != reverbdefs.end())
        return reverbdefs[ref_ename];
    else
        return nullptr;
}

ReverbDefinition *ReverbDefinitionContainer::Lookup(epi::EName ref_ename)
{
    if (ref_ename == epi::kENameNone)
        return nullptr;

    if (reverbdefs.find(ref_ename) != reverbdefs.end())
        return reverbdefs[ref_ename];
    else
        return nullptr;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab