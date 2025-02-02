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

#include "ddf_local.h"

namespace ddf
{

static ReverbDefinition *dynamic_reverb = nullptr;

static ReverbDefinition dummy_reverb;

static const DDFCommandList reverb_commands[] = {DDF_FIELD("ROOM_SIZE", dummy_reverb, room_size_, DDFMainGetPercent),
                                                DDF_FIELD("DAMPING_LEVEL", dummy_reverb, damping_level_, DDFMainGetPercent),
                                                DDF_FIELD("WET_LEVEL", dummy_reverb, wet_level_, DDFMainGetPercent),
                                                DDF_FIELD("DRY_LEVEL", dummy_reverb, dry_level_, DDFMainGetPercent),
                                                DDF_FIELD("REVERB_WIDTH", dummy_reverb, reverb_width_, DDFMainGetPercent),
                                                DDF_FIELD("REVERB_GAIN", dummy_reverb, reverb_gain_, DDFMainGetPercent),
                                                {nullptr, nullptr, 0, nullptr}};

//
//  DDF PARSE ROUTINES
//

static void ReverbStartEntry(const char *name, bool extend)
{
    if (!name || !name[0])
        DDFError("New movie entry is missing a name!\n");

    epi::EName verb_ename = DDFCreateEName(name, false);

    dynamic_reverb = ReverbDefinition::Lookup(verb_ename);

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

    ReverbDefinition::StoreReverb(verb_ename, dynamic_reverb);
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
    // Map the 0.0-1.0 range presented to the user via DDF
    // to the range of 0.000-0.100
    dynamic_reverb->reverb_gain_ *= 0.1f;
}

static void ReverbClearAll(void)
{
    LogWarning("Ignoring #CLEARALL in reverbs.ddf\n");
}

void ReverbDefinition::ReadDDF(const std::string &data)
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

// ---> ReverbDefinition class

ReverbDefinition::ReverbDefinition()
{
    Default();
}

ReverbDefinition::ReverbDefinition(float size, float damp, float wet, float dry, float width, float gain) :
room_size_(size), damping_level_(damp), wet_level_(wet), dry_level_(dry), reverb_width_(width), reverb_gain_(gain)
{

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
    reverb_gain_ = src.reverb_gain_;
}

void ReverbDefinition::ApplyReverb(verblib *reverb) const
{
    verblib_set_room_size(reverb, room_size_);
    verblib_set_damping(reverb, damping_level_);
    verblib_set_wet(reverb, wet_level_);
    verblib_set_dry(reverb, dry_level_);
    verblib_set_width(reverb, reverb_width_);
    verblib_set_gain(reverb, reverb_gain_);
}

void ReverbDefinition::Default()
{
    room_size_ = 0.0f;
    damping_level_ = 0.0f;
    wet_level_ = 0.0f;
    dry_level_ = 0.0f;
    reverb_width_ = 0.0f;
    reverb_gain_ = 0.0f;
}

const ReverbDefinition ReverbDefinition::kOutdoorStrong(0.30f, 0.35f, 0.25f, 0.50f, 0.15f, 0.015f);
const ReverbDefinition ReverbDefinition::kIndoorStrong(0.40f, 0.35f, 0.35f, 0.50f, 0.65f, 0.015f);
const ReverbDefinition ReverbDefinition::kOutdoorWeak(0.30f, 0.45f, 0.20f, 0.65f, 0.15f, 0.010f);
const ReverbDefinition ReverbDefinition::kIndoorWeak(0.40f, 0.50f, 0.20f, 0.70f, 0.50f, 0.010f);

// ---> Container class

ReverbDefinition::Container ReverbDefinition::reverb_defs_;

} // namespace ddf

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab