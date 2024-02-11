//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Animated textures)
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
// Animated Texture/Flat Setup and Parser Code
//

#include <string.h>

#include "local.h"

#include "anim.h"

static animdef_c *dynamic_anim;

static void DDF_AnimGetType(const char *info, void *storage);
static void DDF_AnimGetPic(const char *info, void *storage);

// -ACB- 1998/08/10 Use DDF_MainGetLumpName for getting the..lump name.
// -KM- 1998/09/27 Use DDF_MainGetTime for getting tics

#define DDF_CMD_BASE dummy_anim
static animdef_c dummy_anim;

static const commandlist_t anim_commands[] = {DDF_FIELD("TYPE", type, DDF_AnimGetType),
                                              DDF_FIELD("SEQUENCE", pics, DDF_AnimGetPic),
                                              DDF_FIELD("SPEED", speed, DDF_MainGetTime),
                                              DDF_FIELD("FIRST", startname, DDF_MainGetLumpName),
                                              DDF_FIELD("LAST", endname, DDF_MainGetLumpName),

                                              DDF_CMD_END};

// Floor/ceiling animation sequences, defined by first and last frame,
// i.e. the flat (64x64 tile) name or texture name to be used.
//
// The full animation sequence is given using all the flats between
// the start and end entry, in the order found in the WAD file.
//

// -ACB- 2004/06/03 Replaced array and size with purpose-built class
animdef_container_c animdefs;

static animdef_c *animdefs_Lookup(const char *name)
{
    for (auto iter = animdefs.begin(); iter != animdefs.end(); iter++)
    {
        animdef_c *anim = *iter;
        if (DDF_CompareName(anim->name.c_str(), name) == 0)
            return anim;
    }

    return nullptr; // not found
}

//
//  DDF PARSE ROUTINES
//
static void AnimStartEntry(const char *name, bool extend)
{
    if (!name || !name[0])
    {
        DDF_WarnError("New anim entry is missing a name!");
        name = "ANIM_WITH_NO_NAME";
    }

    dynamic_anim = animdefs_Lookup(name);

    if (extend)
    {
        if (!dynamic_anim)
            DDF_Error("Unknown animdef to extend: %s\n", name);
        return;
    }

    // replaces an existing entry
    if (dynamic_anim)
    {
        dynamic_anim->Default();
        return;
    }

    // not found, create a new one
    dynamic_anim = new animdef_c;

    dynamic_anim->name = name;

    animdefs.push_back(dynamic_anim);
}

static void AnimParseField(const char *field, const char *contents, int index, bool is_last)
{
#if (DEBUG_DDF)
    I_Debugf("ANIM_PARSE: %s = %s;\n", field, contents);
#endif

    if (DDF_MainParseField(anim_commands, field, contents, (uint8_t *)dynamic_anim))
        return;

    DDF_WarnError("Unknown anims.ddf command: %s\n", field);
}

static void AnimFinishEntry(void)
{
    if (dynamic_anim->speed <= 0)
    {
        DDF_WarnError("Bad TICS value for anim: %d\n", dynamic_anim->speed);
        dynamic_anim->speed = 8;
    }

    if (dynamic_anim->pics.empty())
    {
        if (dynamic_anim->startname.empty() || dynamic_anim->endname.empty())
        {
            DDF_Error("Missing animation sequence.\n");
        }

        if (dynamic_anim->type == animdef_c::A_Graphic)
            DDF_Error("TYPE=GRAPHIC animations must use the SEQUENCE command.\n");
    }
}

static void AnimClearAll(void)
{
    // 100% safe to delete all animations
    for (auto anim : animdefs)
    {
        delete anim;
        anim = nullptr;
    }
    animdefs.clear();
}

void DDF_ReadAnims(const std::string &data)
{
    readinfo_t anims;

    anims.tag      = "ANIMATIONS";
    anims.lumpname = "DDFANIM";

    anims.start_entry  = AnimStartEntry;
    anims.parse_field  = AnimParseField;
    anims.finish_entry = AnimFinishEntry;
    anims.clear_all    = AnimClearAll;

    DDF_MainReadFile(&anims, data);
}

//
// DDF_AnimInit
//
void DDF_AnimInit(void)
{
    AnimClearAll();
}

//
// DDF_AnimCleanUp
//
void DDF_AnimCleanUp(void)
{
    animdefs.shrink_to_fit(); // <-- Reduce to allocated size
}

//
// DDF_AnimGetType
//
static void DDF_AnimGetType(const char *info, void *storage)
{
    SYS_ASSERT(storage);

    int *type = (int *)storage;

    if (DDF_CompareName(info, "FLAT") == 0)
        (*type) = animdef_c::A_Flat;
    else if (DDF_CompareName(info, "TEXTURE") == 0)
        (*type) = animdef_c::A_Texture;
    else if (DDF_CompareName(info, "GRAPHIC") == 0)
        (*type) = animdef_c::A_Graphic;
    else
    {
        DDF_WarnError("Unknown animation type: %s\n", info);
        (*type) = animdef_c::A_Flat;
    }
}

static void DDF_AnimGetPic(const char *info, void *storage)
{
    dynamic_anim->pics.push_back(info);
}

// ---> animdef_c class

//
// animdef_c constructor
//
animdef_c::animdef_c() : name(), pics()
{
    Default();
}

//
// animdef_c::CopyDetail()
//
// Copies all the detail with the exception of ddf info
//
void animdef_c::CopyDetail(animdef_c &src)
{
    type      = src.type;
    pics      = src.pics;
    startname = src.startname;
    endname   = src.endname;
    speed     = src.speed;
}

//
// animdef_c::Default()
//
void animdef_c::Default()
{
    type = A_Texture;

    pics.clear();

    startname.clear();
    endname.clear();

    speed = 8;
}

//----------------------------------------------------------------------------

void DDF_ConvertANIMATED(const uint8_t *data, int size)
{
    // handles the Boom ANIMATED lump (in a wad).

    if (size < 23)
        return;

    std::string text = "<ANIMATIONS>\n\n";

    for (; size >= 23; data += 23, size -= 23)
    {
        if (data[0] & 0x80) // end marker
            break;

        int speed = data[19] + (data[20] << 8);
        if (speed < 1)
            speed = 1;

        char last[9];
        char first[9];

        // Clear to zeroes to prevent garbage from being added
        memset(last, 0, 9);
        memset(first, 0, 9);

        // make sure names are NUL-terminated
        memcpy(last, data + 1, 8);
        first[8] = 0;
        memcpy(first, data + 10, 8);
        last[8] = 0;

        I_Debugf("- ANIMATED LUMP: start '%s' : end '%s'\n", first, last);

        // ignore zero-length names
        if (first[0] == 0 || last[0] == 0)
            continue;

        // create the DDF equivalent...
        text += "[";
        text += first;
        text += "]\n";

        if (data[0] & 1)
            text += "type = TEXTURE;\n";
        else
            text += "type  = FLAT;\n";

        text += "first = \"";
        text += first;
        text += "\";\n";

        text += "last  = \"";
        text += last;
        text += "\";\n";

        char speed_buf[64];
        snprintf(speed_buf, sizeof(speed_buf), "%dT", speed);

        text += "speed = ";
        text += speed_buf;
        text += ";\n\n";
    }

    // DEBUG:
    // DDF_DumpFile(text);

    DDF_AddFile(DDF_Anim, text, "Boom ANIMATED lump");
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
