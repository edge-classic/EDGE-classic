//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Colourmaps)
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
// Colourmap handling.
//

#include "local.h"

#include "colormap.h"

static colourmap_c *dynamic_colmap;

colourmap_container_c colourmaps;

void DDF_ColmapGetSpecial(const char *info, void *storage);

#define DDF_CMD_BASE dummy_colmap
static colourmap_c dummy_colmap;

static const commandlist_t colmap_commands[] = {DDF_FIELD("LUMP", lump_name, DDF_MainGetLumpName),
                                                DDF_FIELD("PACK", pack_name, DDF_MainGetString),
                                                DDF_FIELD("START", start, DDF_MainGetNumeric),
                                                DDF_FIELD("LENGTH", length, DDF_MainGetNumeric),
                                                DDF_FIELD("SPECIAL", special, DDF_ColmapGetSpecial),
                                                DDF_FIELD("GL_COLOUR", gl_colour, DDF_MainGetRGB),

                                                DDF_CMD_END};

//
//  DDF PARSE ROUTINES
//

static void ColmapStartEntry(const char *name, bool extend)
{
    if (!name || name[0] == 0)
    {
        DDF_WarnError("New colormap entry is missing a name!");
        name = "COLORMAP_WITH_NO_NAME";
    }

    dynamic_colmap = colourmaps.Lookup(name);

    if (extend)
    {
        if (!dynamic_colmap)
            DDF_Error("Unknown colormap to extend: %s\n", name);
        return;
    }

    // replaces the existing entry
    if (dynamic_colmap)
    {
        dynamic_colmap->Default();

        if (epi::StringPrefixCaseCompareASCII(name, "TEXT") == 0)
            dynamic_colmap->special = COLSP_Whiten;

        return;
    }

    // not found, create a new one
    dynamic_colmap = new colourmap_c;

    dynamic_colmap->name = name;

    // make sure fonts get whitened properly (as the default)
    if (epi::StringPrefixCaseCompareASCII(name, "TEXT") == 0)
        dynamic_colmap->special = COLSP_Whiten;

    colourmaps.push_back(dynamic_colmap);
}

static void ColmapParseField(const char *field, const char *contents, int index, bool is_last)
{
#if (DEBUG_DDF)
    I_Debugf("COLMAP_PARSE: %s = %s;\n", field, contents);
#endif

    // -AJA- backwards compatibility cruft...
    if (DDF_CompareName(field, "PRIORITY") == 0)
        return;

    if (DDF_MainParseField(colmap_commands, field, contents, (uint8_t *)dynamic_colmap))
        return; // OK

    DDF_WarnError("Unknown colmap.ddf command: %s\n", field);
}

static void ColmapFinishEntry(void)
{
    if (dynamic_colmap->start < 0)
    {
        DDF_WarnError("Bad START value for colmap: %d\n", dynamic_colmap->start);
        dynamic_colmap->start = 0;
    }

    // don't need a length when using GL_COLOUR
    if (!dynamic_colmap->lump_name.empty() && !dynamic_colmap->pack_name.empty() && dynamic_colmap->length <= 0)
    {
        DDF_WarnError("Bad LENGTH value for colmap: %d\n", dynamic_colmap->length);
        dynamic_colmap->length = 1;
    }

    if (dynamic_colmap->lump_name.empty() && dynamic_colmap->pack_name.empty() &&
        dynamic_colmap->gl_colour == kRGBANoValue)
    {
        DDF_WarnError("Colourmap entry missing LUMP, PACK or GL_COLOUR.\n");
        // We are now assuming that the intent is to remove all
        // colmaps with this name (i.e., "null" it), as the only way to get here
        // is to create an empty entry or use gl_colour=NONE; - Dasho
        std::string doomed_name = dynamic_colmap->name;
        for (auto iter = colourmaps.begin(); iter != colourmaps.end();)
        {
            colourmap_c *cmap = *iter;
            if (DDF_CompareName(doomed_name.c_str(), cmap->name.c_str()) == 0)
            {
                delete cmap;
                cmap = nullptr;
                iter = colourmaps.erase(iter);
            }
            else
                ++iter;
        }
    }
}

static void ColmapClearAll(void)
{
    I_Warning("Ignoring #CLEARALL in colormap.ddf\n");
}

void DDF_ReadColourMaps(const std::string &data)
{
    readinfo_t colm_r;

    colm_r.tag      = "COLOURMAPS";
    colm_r.lumpname = "DDFCOLM";

    colm_r.start_entry  = ColmapStartEntry;
    colm_r.parse_field  = ColmapParseField;
    colm_r.finish_entry = ColmapFinishEntry;
    colm_r.clear_all    = ColmapClearAll;

    DDF_MainReadFile(&colm_r, data);
}

void DDF_ColmapInit(void)
{
    for (auto cmap : colourmaps)
    {
        delete cmap;
        cmap = nullptr;
    }
    colourmaps.clear();
}

void DDF_ColmapCleanUp(void)
{
    colourmaps.shrink_to_fit();
}

specflags_t colmap_specials[] = {{"FLASH", COLSP_NoFlash, true},
                                 {"WHITEN", COLSP_Whiten, false},

                                 // -AJA- backwards compatibility cruft...
                                 {"SKY", 0, 0},

                                 {NULL, 0, 0}};

//
// DDF_ColmapGetSpecial
//
// Gets the colourmap specials.
//
void DDF_ColmapGetSpecial(const char *info, void *storage)
{
    colourspecial_e *spec = (colourspecial_e *)storage;

    int flag_value;

    switch (DDF_MainCheckSpecialFlag(info, colmap_specials, &flag_value, true, false))
    {
    case CHKF_Positive:
        *spec = (colourspecial_e)(*spec | flag_value);
        break;

    case CHKF_Negative:
        *spec = (colourspecial_e)(*spec & ~flag_value);
        break;

    case CHKF_User:
    case CHKF_Unknown:
        DDF_WarnError("DDF_ColmapGetSpecial: Unknown Special: %s", info);
        break;
    }
}

// --> Colourmap Class

//
// colourmap_c Constructor
//
colourmap_c::colourmap_c() : name()
{
    Default();
}

//
// colourmap_c Deconstructor
//
colourmap_c::~colourmap_c()
{
}

//
// colourmap_c::CopyDetail()
//
void colourmap_c::CopyDetail(colourmap_c &src)
{
    lump_name = src.lump_name;

    start   = src.start;
    length  = src.length;
    special = src.special;

    gl_colour   = src.gl_colour;
    font_colour = src.font_colour;

    cache.data = NULL;
    analysis   = NULL;
}

//
// colourmap_c::Default()
//
void colourmap_c::Default()
{
    lump_name.clear();

    start   = 0;
    length  = 0;
    special = COLSP_None;

    gl_colour   = kRGBANoValue;
    font_colour = kRGBANoValue;

    cache.data = NULL;
    analysis   = NULL;
}

// --> colourmap_container_c class

//
// colourmap_container_c::colourmap_container_c()
//
colourmap_container_c::colourmap_container_c()
{
}

//
// ~colourmap_container_c::colourmap_container_c()
//
colourmap_container_c::~colourmap_container_c()
{
    for (auto iter = begin(); iter != end(); iter++)
    {
        colourmap_c *cmap = *iter;
        delete cmap;
        cmap = nullptr;
    }
}

//
// colourmap_c* colourmap_container_c::Lookup()
//
colourmap_c *colourmap_container_c::Lookup(const char *refname)
{
    if (!refname || !refname[0])
        return NULL;

    for (auto iter = begin(); iter != end(); iter++)
    {
        colourmap_c *cmap = *iter;
        if (DDF_CompareName(cmap->name.c_str(), refname) == 0)
            return cmap;
    }

    return NULL;
}

//----------------------------------------------------------------------------

//
// This is used to make entries for lumps between C_START and C_END
// markers in a (BOOM) WAD file.
//
void DDF_AddRawColourmap(const char *name, int size, const char *pack_name)
{
    if (size < 256)
    {
        I_Warning("WAD Colourmap '%s' too small (%d < %d)\n", name, size, 256);
        return;
    }

    // limit length to 32
    size = std::min(32, size / 256);

    std::string text = "<COLOURMAPS>\n\n";

    text += "[";
    text += name;
    text += "]\n";

    if (pack_name != NULL)
    {
        text += "pack   = \"";
        text += pack_name;
        text += "\";\n";
    }
    else
    {
        text += "lump   = \"";
        text += name;
        text += "\";\n";
    }

    char length_buf[64];
    snprintf(length_buf, sizeof(length_buf), "%d", size);

    text += "start  = 0;\n";
    text += "length = ";
    text += length_buf;
    text += ";\n";

    // DEBUG:
    DDF_DumpFile(text);

    DDF_AddFile(DDF_ColourMap, text, pack_name ? pack_name : name);

    I_Debugf("- Added RAW colourmap '%s' start=0 length=%s\n", name, length_buf);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
