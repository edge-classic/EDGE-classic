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

#include "colormap.h"

#include "local.h"
#include "str_compare.h"

static Colormap *dynamic_colmap;

ColormapContainer colormaps;

void DDF_ColmapGetSpecial(const char *info, void *storage);

#define DDF_CMD_BASE dummy_colmap
static Colormap dummy_colmap;

static const DDFCommandList colmap_commands[] = {
    DDF_FIELD("LUMP", lump_name_, DDF_MainGetLumpName),
    DDF_FIELD("PACK", pack_name_, DDF_MainGetString),
    DDF_FIELD("START", start_, DDF_MainGetNumeric),
    DDF_FIELD("LENGTH", length_, DDF_MainGetNumeric),
    DDF_FIELD("SPECIAL", special_, DDF_ColmapGetSpecial),
    DDF_FIELD("GL_COLOUR", gl_color_, DDF_MainGetRGB),

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

    dynamic_colmap = colormaps.Lookup(name);

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
            dynamic_colmap->special_ = kColorSpecialWhiten;

        return;
    }

    // not found, create a new one
    dynamic_colmap = new Colormap;

    dynamic_colmap->name_ = name;

    // make sure fonts get whitened properly (as the default)
    if (epi::StringPrefixCaseCompareASCII(name, "TEXT") == 0)
        dynamic_colmap->special_ = kColorSpecialWhiten;

    colormaps.push_back(dynamic_colmap);
}

static void ColmapParseField(const char *field, const char *contents, int index,
                             bool is_last)
{
#if (DEBUG_DDF)
    EDGEDebugf("COLMAP_PARSE: %s = %s;\n", field, contents);
#endif

    // -AJA- backwards compatibility cruft...
    if (DDF_CompareName(field, "PRIORITY") == 0) return;

    if (DDF_MainParseField(colmap_commands, field, contents,
                           (uint8_t *)dynamic_colmap))
        return;  // OK

    DDF_WarnError("Unknown colmap.ddf command: %s\n", field);
}

static void ColmapFinishEntry(void)
{
    if (dynamic_colmap->start_ < 0)
    {
        DDF_WarnError("Bad START value for colmap: %d\n",
                      dynamic_colmap->start_);
        dynamic_colmap->start_ = 0;
    }

    // don't need a length when using GL_COLOUR
    if (!dynamic_colmap->lump_name_.empty() &&
        !dynamic_colmap->pack_name_.empty() && dynamic_colmap->length_ <= 0)
    {
        DDF_WarnError("Bad LENGTH value for colmap: %d\n",
                      dynamic_colmap->length_);
        dynamic_colmap->length_ = 1;
    }

    if (dynamic_colmap->lump_name_.empty() &&
        dynamic_colmap->pack_name_.empty() &&
        dynamic_colmap->gl_color_ == kRGBANoValue)
    {
        DDF_WarnError("Colourmap entry missing LUMP, PACK or GL_COLOUR.\n");
        // We are now assuming that the intent is to remove all
        // colmaps with this name (i.e., "null" it), as the only way to get here
        // is to create an empty entry or use gl_color_=NONE; - Dasho
        std::string doomed_name = dynamic_colmap->name_;
        for (std::vector<Colormap *>::iterator iter     = colormaps.begin(),
                                               iter_end = colormaps.end();
             iter != iter_end; iter++)
        {
            Colormap *cmap = *iter;
            if (DDF_CompareName(doomed_name.c_str(), cmap->name_.c_str()) == 0)
            {
                delete cmap;
                cmap = nullptr;
                iter = colormaps.erase(iter);
            }
            else
                ++iter;
        }
    }
}

static void ColmapClearAll(void)
{
    EDGEWarning("Ignoring #CLEARALL in colormap.ddf\n");
}

void DDF_ReadColourMaps(const std::string &data)
{
    DDFReadInfo colm_r;

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
    for (Colormap *cmap : colormaps)
    {
        delete cmap;
        cmap = nullptr;
    }
    colormaps.clear();
}

void DDF_ColmapCleanUp(void) { colormaps.shrink_to_fit(); }

DDFSpecialFlags colmap_specials[] = {{"FLASH", kColorSpecialNoFlash, true},
                                 {"WHITEN", kColorSpecialWhiten, false},

                                 // -AJA- backwards compatibility cruft...
                                 {"SKY", 0, 0},

                                 {nullptr, 0, 0}};

//
// DDF_ColmapGetSpecial
//
// Gets the colormap specials.
//
void DDF_ColmapGetSpecial(const char *info, void *storage)
{
    ColorSpecial *spec = (ColorSpecial *)storage;

    int flag_value;

    switch (DDF_MainCheckSpecialFlag(info, colmap_specials, &flag_value, true,
                                     false))
    {
        case kDDFCheckFlagPositive:
            *spec = (ColorSpecial)(*spec | flag_value);
            break;

        case kDDFCheckFlagNegative:
            *spec = (ColorSpecial)(*spec & ~flag_value);
            break;

        case kDDFCheckFlagUser:
        case kDDFCheckFlagUnknown:
            DDF_WarnError("DDF_ColmapGetSpecial: Unknown Special: %s", info);
            break;
    }
}

// --> Colourmap Class

//
// Colormap Constructor
//
Colormap::Colormap() : name_() { Default(); }

//
// Colormap Deconstructor
//
Colormap::~Colormap() {}

//
// Colormap::CopyDetail()
//
void Colormap::CopyDetail(Colormap &src)
{
    lump_name_ = src.lump_name_;

    start_   = src.start_;
    length_  = src.length_;
    special_ = src.special_;

    gl_color_    = src.gl_color_;
    font_colour_ = src.font_colour_;

    cache_.data = nullptr;
    analysis_   = nullptr;
}

//
// Colormap::Default()
//
void Colormap::Default()
{
    lump_name_.clear();

    start_   = 0;
    length_  = 0;
    special_ = kColorSpecialNone;

    gl_color_    = kRGBANoValue;
    font_colour_ = kRGBANoValue;

    cache_.data = nullptr;
    analysis_   = nullptr;
}

// --> ColormapContainer class

//
// ColormapContainer::ColormapContainer()
//
ColormapContainer::ColormapContainer() {}

//
// ~ColormapContainer::ColormapContainer()
//
ColormapContainer::~ColormapContainer()
{
    for (std::vector<Colormap *>::iterator iter = begin(), iter_end = end();
         iter != iter_end; iter++)
    {
        Colormap *cmap = *iter;
        delete cmap;
        cmap = nullptr;
    }
}

//
// Colormap* ColormapContainer::Lookup()
//
Colormap *ColormapContainer::Lookup(const char *refname)
{
    if (!refname || !refname[0]) return nullptr;

    for (std::vector<Colormap *>::iterator iter = begin(), iter_end = end();
         iter != iter_end; iter++)
    {
        Colormap *cmap = *iter;
        if (DDF_CompareName(cmap->name_.c_str(), refname) == 0) return cmap;
    }

    return nullptr;
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
        EDGEWarning("WAD Colourmap '%s' too small (%d < %d)\n", name, size, 256);
        return;
    }

    // limit length to 32
    size = std::min(32, size / 256);

    std::string text = "<COLOURMAPS>\n\n";

    text += "[";
    text += name;
    text += "]\n";

    if (pack_name != nullptr)
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

    DDF_AddFile(kDDFTypeColourMap, text, pack_name ? pack_name : name);

    EDGEDebugf("- Added RAW colormap '%s' start=0 length=%s\n", name, length_buf);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
