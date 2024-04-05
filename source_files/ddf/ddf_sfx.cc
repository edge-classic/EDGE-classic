//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Sounds)
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
// -KM- 1998/09/27 Finished :-)
//

#include "ddf_sfx.h"

#include "ddf_local.h"
#include "epi_str_util.h"

static SoundEffectDefinition *dynamic_sfx;

SoundEffectDefinitionContainer sfxdefs;

static SoundEffectDefinition dummy_sfx;

static const DDFCommandList sfx_commands[] = {
    DDF_FIELD("LUMP_NAME", dummy_sfx, lump_name_, DDFMainGetLumpName),
    DDF_FIELD("PACK_NAME", dummy_sfx, pack_name_, DDFMainGetString),
    DDF_FIELD("FILE_NAME", dummy_sfx, file_name_, DDFMainGetString),
    DDF_FIELD("PC_SPEAKER_LUMP", dummy_sfx, pc_speaker_sound_,
              DDFMainGetString), // Kept for backwards compat
    DDF_FIELD("PC_SPEAKER_SOUND", dummy_sfx, pc_speaker_sound_, DDFMainGetString),
    DDF_FIELD("SINGULAR", dummy_sfx, singularity_, DDFMainGetNumeric),
    DDF_FIELD("PRIORITY", dummy_sfx, priority_, DDFMainGetNumeric),
    DDF_FIELD("VOLUME", dummy_sfx, volume_, DDFMainGetPercent),
    DDF_FIELD("LOOP", dummy_sfx, looping_, DDFMainGetBoolean),
    DDF_FIELD("PRECIOUS", dummy_sfx, precious_, DDFMainGetBoolean),
    DDF_FIELD("MAX_DISTANCE", dummy_sfx, max_distance_, DDFMainGetFloat),

    {nullptr, nullptr, 0, nullptr}};

//
//  DDF PARSE ROUTINES
//

static void SoundStartEntry(const char *name, bool extend)
{
    if (!name || !name[0])
    {
        DDFWarnError("New sound entry is missing a name!");
        name = "SOUND_WITH_NO_NAME";
    }

    dynamic_sfx = sfxdefs.Lookup(name);

    if (extend)
    {
        if (!dynamic_sfx)
            DDFError("Unknown sound to extend: %s\n", name);
        return;
    }

    // replaces an existing entry?
    if (dynamic_sfx)
    {
        // maintain the internal ID
        int id = dynamic_sfx->normal_.sounds[0];

        dynamic_sfx->Default();

        dynamic_sfx->normal_.num       = 1;
        dynamic_sfx->normal_.sounds[0] = id;
        return;
    }

    // not found, create a new one
    dynamic_sfx = new SoundEffectDefinition;

    dynamic_sfx->name_ = name;

    sfxdefs.push_back(dynamic_sfx);

    // give it a self-referencing ID number
    dynamic_sfx->normal_.sounds[0] = sfxdefs.size() - 1;
    dynamic_sfx->normal_.num       = 1;
}

static void SoundParseField(const char *field, const char *contents, int index, bool is_last)
{
#if (DDF_DEBUG)
    LogDebug("SOUND_PARSE: %s = %s;\n", field, contents);
#endif

    // -AJA- ignore these for backwards compatibility
    if (DDFCompareName(field, "BITS") == 0 || DDFCompareName(field, "STEREO") == 0)
        return;

    if (DDFMainParseField(sfx_commands, field, contents, (uint8_t *)dynamic_sfx))
        return; // OK

    DDFWarnError("Unknown sounds.ddf command: %s\n", field);
}

static void SoundFinishEntry(void)
{
    if (dynamic_sfx->lump_name_.empty() && dynamic_sfx->file_name_.empty() && dynamic_sfx->pack_name_.empty())
    {
        DDFError("Missing LUMP_NAME or PACK_NAME for sound.\n");
    }
}

static void SoundClearAll(void)
{
    LogWarning("Ignoring #CLEARALL in sounds.ddf\n");
}

void DDFReadSFX(const std::string &data)
{
    DDFReadInfo sfx_r;

    sfx_r.tag      = "SOUNDS";
    sfx_r.lumpname = "DDFSFX";

    sfx_r.start_entry  = SoundStartEntry;
    sfx_r.parse_field  = SoundParseField;
    sfx_r.finish_entry = SoundFinishEntry;
    sfx_r.clear_all    = SoundClearAll;

    DDFMainReadFile(&sfx_r, data);
}

void DDFSFXInit(void)
{
    for (SoundEffectDefinition *s : sfxdefs)
    {
        delete s;
        s = nullptr;
    }
    sfxdefs.clear();
}

void DDFSFXCleanUp(void)
{
    sfxdefs.shrink_to_fit();
}

//
// DDFMainLookupSound
//
// Lookup the sound specified.
//
// -ACB- 1998/07/08 Checked the S_sfx table for sfx names.
// -ACB- 1998/07/18 Removed to the need set *currentcmdlist[commandref].data to
// -1 -KM- 1998/09/27 Fixed this func because of sounds.ddf -KM- 1998/10/29
// SoundEffect finished
//
void DDFMainLookupSound(const char *info, void *storage)
{
    SoundEffect **dest = (SoundEffect **)storage;

    EPI_ASSERT(info && storage);

    *dest = sfxdefs.GetEffect(info);
}

// --> Sound Effect Definition Class

//
// SoundEffectDefinition Constructor
//
SoundEffectDefinition::SoundEffectDefinition() : name_()
{
    Default();
}

//
// SoundEffectDefinition Destructor
//
SoundEffectDefinition::~SoundEffectDefinition()
{
}

//
// SoundEffectDefinition::CopyDetail()
//
void SoundEffectDefinition::CopyDetail(SoundEffectDefinition &src)
{
    lump_name_        = src.lump_name_;
    pc_speaker_sound_ = src.pc_speaker_sound_;
    file_name_        = src.file_name_;
    pack_name_        = src.pack_name_;

    // clear the internal SoundEffect (ID would be wrong)
    normal_.sounds[0] = 0;
    normal_.num       = 0;

    singularity_  = src.singularity_;  // singularity
    priority_     = src.priority_;     // priority (lower is more important)
    volume_       = src.volume_;       // volume
    looping_      = src.looping_;      // looping
    precious_     = src.precious_;     // precious
    max_distance_ = src.max_distance_; // max_distance
}

//
// SoundEffectDefinition::Default()
//
void SoundEffectDefinition::Default()
{
    lump_name_.clear();
    pc_speaker_sound_.clear();
    file_name_.clear();
    pack_name_.clear();

    normal_.sounds[0] = 0;
    normal_.num       = 0;

    singularity_  = 0;       // singularity
    priority_     = 999;     // priority (lower is more important)
    volume_       = 1.0f;    // volume
    looping_      = false;   // looping
    precious_     = false;   // precious
    max_distance_ = 4000.0f; // max_distance
}

// --> Sound Effect Definition Containter Class

static int strncasecmpwild(const char *s1, const char *s2, int n)
{
    int i = 0;

    for (i = 0; s1[i] && s2[i] && i < n; i++)
    {
        if ((epi::ToUpperASCII(s1[i]) != epi::ToUpperASCII(s2[i])) && (s1[i] != '?') && (s2[i] != '?'))
            break;
    }
    // -KM- 1999/01/29 If strings are equal return equal.
    if (i == n)
        return 0;

    if (s1[i] == '?' || s2[i] == '?')
        return 0;

    return s1[i] - s2[i];
}

//
// SoundEffectDefinitionContainer::GetEffect()
//
// FIXME!! Remove error param hack
// FIXME!! Cache results for those we create
//
SoundEffect *SoundEffectDefinitionContainer::GetEffect(const char *name, bool error)
{
    int count = 0;

    SoundEffectDefinition *si   = nullptr;
    SoundEffectDefinition *last = nullptr;
    SoundEffect           *r    = nullptr;

    // nullptr Sound
    if (!name || !name[0] || DDFCompareName(name, "NULL") == 0)
        return nullptr;

    // count them
    for (std::vector<SoundEffectDefinition *>::reverse_iterator iter = rbegin(), iter_end = rend(); iter != iter_end;
         iter++)
    {
        si = *iter;

        if (strncasecmpwild(name, si->name_.c_str(), 8) == 0)
        {
            count++;
            if (!last)
                last = si;
        }
    }

    if (count == 0)
    {
        if (error)
            DDFWarnError("Unknown SFX: '%.8s'\n", name);

        return nullptr;
    }

    // -AJA- optimisation to save some memory
    if (count == 1)
    {
        si = last;
        r  = &si->normal_;

        EPI_ASSERT(r->num == 1);

        return r;
    }

    //
    // allocate elements.  Uses (count-1) since SoundEffect already includes
    // the first integer.
    //
    r      = (SoundEffect *)new uint8_t[sizeof(SoundEffect) + ((count - 1) * sizeof(int))];
    r->num = 0;

    // now store them
    for (int i = size() - 1; i >= 0; i--)
    {
        si = at(i);

        if (strncasecmpwild(name, si->name_.c_str(), 8) == 0)
            r->sounds[r->num++] = i;
    }

    EPI_ASSERT(r->num == count);

    return r;
}

//
// SoundEffectDefinitionContainer::Lookup()
//
SoundEffectDefinition *SoundEffectDefinitionContainer::Lookup(const char *name)
{
    for (std::vector<SoundEffectDefinition *>::iterator iter = begin(), iter_end = end(); iter != iter_end; iter++)
    {
        SoundEffectDefinition *s = *iter;

        if (DDFCompareName(s->name_.c_str(), name) == 0)
            return s;
    }

    return nullptr;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
