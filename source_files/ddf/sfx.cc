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

#include "sfx.h"

#include "local.h"

static sfxdef_c *dynamic_sfx;

sfxdef_container_c sfxdefs;

#define DDF_CMD_BASE dummy_sfx
static sfxdef_c dummy_sfx;

static const commandlist_t sfx_commands[] = {
    DDF_FIELD("LUMP_NAME", lump_name, DDF_MainGetLumpName),
    DDF_FIELD("PACK_NAME", pack_name, DDF_MainGetString),
    DDF_FIELD("FILE_NAME", file_name, DDF_MainGetString),
    DDF_FIELD("PC_SPEAKER_LUMP", pc_speaker_sound,
              DDF_MainGetString),  // Kept for backwards compat
    DDF_FIELD("PC_SPEAKER_SOUND", pc_speaker_sound, DDF_MainGetString),
    DDF_FIELD("SINGULAR", singularity, DDF_MainGetNumeric),
    DDF_FIELD("PRIORITY", priority, DDF_MainGetNumeric),
    DDF_FIELD("VOLUME", volume, DDF_MainGetPercent),
    DDF_FIELD("LOOP", looping, DDF_MainGetBoolean),
    DDF_FIELD("PRECIOUS", precious, DDF_MainGetBoolean),
    DDF_FIELD("MAX_DISTANCE", max_distance, DDF_MainGetFloat),

    DDF_CMD_END};

//
//  DDF PARSE ROUTINES
//

static void SoundStartEntry(const char *name, bool extend)
{
    if (!name || !name[0])
    {
        DDF_WarnError("New sound entry is missing a name!");
        name = "SOUND_WITH_NO_NAME";
    }

    dynamic_sfx = sfxdefs.Lookup(name);

    if (extend)
    {
        if (!dynamic_sfx) DDF_Error("Unknown sound to extend: %s\n", name);
        return;
    }

    // replaces an existing entry?
    if (dynamic_sfx)
    {
        // maintain the internal ID
        int id = dynamic_sfx->normal.sounds[0];

        dynamic_sfx->Default();

        dynamic_sfx->normal.num       = 1;
        dynamic_sfx->normal.sounds[0] = id;
        return;
    }

    // not found, create a new one
    dynamic_sfx = new sfxdef_c;

    dynamic_sfx->name = name;

    sfxdefs.push_back(dynamic_sfx);

    // give it a self-referencing ID number
    dynamic_sfx->normal.sounds[0] = sfxdefs.size() - 1;
    dynamic_sfx->normal.num       = 1;
}

static void SoundParseField(const char *field, const char *contents, int index,
                            bool is_last)
{
#if (DEBUG_DDF)
    I_Debugf("SOUND_PARSE: %s = %s;\n", field, contents);
#endif

    // -AJA- ignore these for backwards compatibility
    if (DDF_CompareName(field, "BITS") == 0 ||
        DDF_CompareName(field, "STEREO") == 0)
        return;

    if (DDF_MainParseField(sfx_commands, field, contents,
                           (uint8_t *)dynamic_sfx))
        return;  // OK

    DDF_WarnError("Unknown sounds.ddf command: %s\n", field);
}

static void SoundFinishEntry(void)
{
    if (dynamic_sfx->lump_name.empty() && dynamic_sfx->file_name.empty() &&
        dynamic_sfx->pack_name.empty())
    {
        DDF_Error("Missing LUMP_NAME or PACK_NAME for sound.\n");
    }
}

static void SoundClearAll(void)
{
    I_Warning("Ignoring #CLEARALL in sounds.ddf\n");
}

void DDF_ReadSFX(const std::string &data)
{
    readinfo_t sfx_r;

    sfx_r.tag      = "SOUNDS";
    sfx_r.lumpname = "DDFSFX";

    sfx_r.start_entry  = SoundStartEntry;
    sfx_r.parse_field  = SoundParseField;
    sfx_r.finish_entry = SoundFinishEntry;
    sfx_r.clear_all    = SoundClearAll;

    DDF_MainReadFile(&sfx_r, data);
}

void DDF_SFXInit(void)
{
    for (auto s : sfxdefs)
    {
        delete s;
        s = nullptr;
    }
    sfxdefs.clear();
}

void DDF_SFXCleanUp(void) { sfxdefs.shrink_to_fit(); }

//
// DDF_MainLookupSound
//
// Lookup the sound specified.
//
// -ACB- 1998/07/08 Checked the S_sfx table for sfx names.
// -ACB- 1998/07/18 Removed to the need set *currentcmdlist[commandref].data to
// -1 -KM- 1998/09/27 Fixed this func because of sounds.ddf -KM- 1998/10/29
// sfx_t finished
//
void DDF_MainLookupSound(const char *info, void *storage)
{
    sfx_t **dest = (sfx_t **)storage;

    SYS_ASSERT(info && storage);

    *dest = sfxdefs.GetEffect(info);
}

// --> Sound Effect Definition Class

//
// sfxdef_c Constructor
//
sfxdef_c::sfxdef_c() : name() { Default(); }

//
// sfxdef_c Destructor
//
sfxdef_c::~sfxdef_c() {}

//
// sfxdef_c::CopyDetail()
//
void sfxdef_c::CopyDetail(sfxdef_c &src)
{
    lump_name        = src.lump_name;
    pc_speaker_sound = src.pc_speaker_sound;
    file_name        = src.file_name;
    pack_name        = src.pack_name;

    // clear the internal sfx_t (ID would be wrong)
    normal.sounds[0] = 0;
    normal.num       = 0;

    singularity  = src.singularity;   // singularity
    priority     = src.priority;      // priority (lower is more important)
    volume       = src.volume;        // volume
    looping      = src.looping;       // looping
    precious     = src.precious;      // precious
    max_distance = src.max_distance;  // max_distance
}

//
// sfxdef_c::Default()
//
void sfxdef_c::Default()
{
    lump_name.clear();
    pc_speaker_sound.clear();
    file_name.clear();
    pack_name.clear();

    normal.sounds[0] = 0;
    normal.num       = 0;

    singularity  = 0;                  // singularity
    priority     = 999;                // priority (lower is more important)
    volume       = PERCENT_MAKE(100);  // volume
    looping      = false;              // looping
    precious     = false;              // precious
    max_distance = S_CLIPPING_DIST;    // max_distance
}

// --> Sound Effect Definition Containter Class

static int strncasecmpwild(const char *s1, const char *s2, int n)
{
    int i = 0;

    for (i = 0; s1[i] && s2[i] && i < n; i++)
    {
        if ((epi::ToUpperASCII(s1[i]) != epi::ToUpperASCII(s2[i])) &&
            (s1[i] != '?') && (s2[i] != '?'))
            break;
    }
    // -KM- 1999/01/29 If strings are equal return equal.
    if (i == n) return 0;

    if (s1[i] == '?' || s2[i] == '?') return 0;

    return s1[i] - s2[i];
}

//
// sfxdef_container_c::GetEffect()
//
// FIXME!! Remove error param hack
// FIXME!! Cache results for those we create
//
sfx_t *sfxdef_container_c::GetEffect(const char *name, bool error)
{
    int count = 0;

    sfxdef_c *si   = nullptr;
    sfxdef_c *last = nullptr;
    sfx_t    *r    = nullptr;

    // nullptr Sound
    if (!name || !name[0] || DDF_CompareName(name, "NULL") == 0) return nullptr;

    // count them
    for (auto iter = rbegin(); iter != rend(); iter++)
    {
        si = *iter;

        if (strncasecmpwild(name, si->name.c_str(), 8) == 0)
        {
            count++;
            if (!last) last = si;
        }
    }

    if (count == 0)
    {
        if (error) DDF_WarnError("Unknown SFX: '%.8s'\n", name);

        return nullptr;
    }

    // -AJA- optimisation to save some memory
    if (count == 1)
    {
        si = last;
        r  = &si->normal;

        SYS_ASSERT(r->num == 1);

        return r;
    }

    //
    // allocate elements.  Uses (count-1) since sfx_t already includes
    // the first integer.
    //
    r      = (sfx_t *)new uint8_t[sizeof(sfx_t) + ((count - 1) * sizeof(int))];
    r->num = 0;

    // now store them
    for (int i = size() - 1; i >= 0; i--)
    {
        si = at(i);

        if (strncasecmpwild(name, si->name.c_str(), 8) == 0)
            r->sounds[r->num++] = i;
    }

    SYS_ASSERT(r->num == count);

    return r;
}

//
// sfxdef_container_c::Lookup()
//
sfxdef_c *sfxdef_container_c::Lookup(const char *name)
{
    for (auto iter = begin(); iter != end(); iter++)
    {
        sfxdef_c *s = *iter;

        if (DDF_CompareName(s->name.c_str(), name) == 0) return s;
    }

    return nullptr;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
