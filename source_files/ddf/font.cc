//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Fonts)
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
// Font Setup and Parser Code
//

#include "font.h"

#include <string.h>

#include "local.h"
#include "str_compare.h"

static fontdef_c *dynamic_font;

static void DDF_FontGetType(const char *info, void *storage);
static void DDF_FontGetPatch(const char *info, void *storage);

#define DDF_CMD_BASE dummy_font
static fontdef_c dummy_font;

static const commandlist_t font_commands[] = {
    DDF_FIELD("TYPE", type, DDF_FontGetType),
    DDF_FIELD("PATCHES", patches, DDF_FontGetPatch),
    DDF_FIELD("IMAGE", image_name, DDF_MainGetString),
    DDF_FIELD("TTF", ttf_name, DDF_MainGetString),
    DDF_FIELD("DEFAULT_SIZE", default_size, DDF_MainGetFloat),
    DDF_FIELD("TTF_SMOOTHING", ttf_smoothing_string, DDF_MainGetString),
    DDF_FIELD("MISSING_PATCH", missing_patch, DDF_MainGetString),
    DDF_FIELD("SPACING", spacing, DDF_MainGetFloat),

    DDF_CMD_END};

// -ACB- 2004/06/03 Replaced array and size with purpose-built class
fontdef_container_c fontdefs;

//
//  DDF PARSE ROUTINES
//
static void FontStartEntry(const char *name, bool extend)
{
    if (!name || !name[0])
    {
        DDF_WarnError("New font entry is missing a name!");
        name = "FONT_WITH_NO_NAME";
    }

    dynamic_font = fontdefs.Lookup(name);

    if (extend)
    {
        if (!dynamic_font) DDF_Error("Unknown font to extend: %s\n", name);
        return;
    }

    // replaces the existing entry
    if (dynamic_font)
    {
        dynamic_font->Default();
        return;
    }

    // not found, create a new one
    dynamic_font = new fontdef_c;

    dynamic_font->name = name;

    fontdefs.push_back(dynamic_font);
}

static void FontParseField(const char *field, const char *contents, int index,
                           bool is_last)
{
#if (DEBUG_DDF)
    I_Debugf("FONT_PARSE: %s = %s;\n", field, contents);
#endif

    if (DDF_MainParseField(font_commands, field, contents,
                           (uint8_t *)dynamic_font))
        return;  // OK

    DDF_Error("Unknown fonts.ddf command: %s\n", field);
}

static void FontFinishEntry(void)
{
    if (dynamic_font->type == FNTYP_UNSET)
        DDF_Error("No type specified for font.\n");

    if (dynamic_font->type == FNTYP_Patch && !dynamic_font->patches)
        DDF_Error("Missing font patch list.\n");

    if (dynamic_font->type == FNTYP_Image && dynamic_font->image_name.empty())
        DDF_Error("Missing font image name.\n");

    if (dynamic_font->type == FNTYP_TrueType && dynamic_font->ttf_name.empty())
        DDF_Error("Missing font TTF/OTF lump/file name.\n");

    if (dynamic_font->type == FNTYP_TrueType &&
        !dynamic_font->ttf_smoothing_string.empty())
    {
        if (epi::StringCaseCompareASCII(dynamic_font->ttf_smoothing_string,
                                        "NEVER") == 0)
            dynamic_font->ttf_smoothing = dynamic_font->TTF_SMOOTH_NEVER;
        else if (epi::StringCaseCompareASCII(dynamic_font->ttf_smoothing_string,
                                             "ALWAYS") == 0)
            dynamic_font->ttf_smoothing = dynamic_font->TTF_SMOOTH_ALWAYS;
        else if (epi::StringCaseCompareASCII(dynamic_font->ttf_smoothing_string,
                                             "ON_DEMAND") == 0)
            dynamic_font->ttf_smoothing = dynamic_font->TTF_SMOOTH_ON_DEMAND;
    }
}

static void FontClearAll(void)
{
    I_Warning("Ignoring #CLEARALL in fonts.ddf\n");
}

void DDF_ReadFonts(const std::string &data)
{
    readinfo_t fonts;

    fonts.tag      = "FONTS";
    fonts.lumpname = "DDFFONT";

    fonts.start_entry  = FontStartEntry;
    fonts.parse_field  = FontParseField;
    fonts.finish_entry = FontFinishEntry;
    fonts.clear_all    = FontClearAll;

    DDF_MainReadFile(&fonts, data);
}

void DDF_FontInit(void)
{
    for (auto fnt : fontdefs)
    {
        delete fnt;
        fnt = nullptr;
    }
    fontdefs.clear();
}

void DDF_FontCleanUp(void)
{
    if (fontdefs.empty()) I_Error("There are no fonts defined in DDF !\n");

    fontdefs.shrink_to_fit();  // <-- Reduce to allocated size
}

//
// DDF_FontGetType
//
static void DDF_FontGetType(const char *info, void *storage)
{
    SYS_ASSERT(storage);

    fonttype_e *type = (fonttype_e *)storage;

    if (DDF_CompareName(info, "PATCH") == 0)
        (*type) = FNTYP_Patch;
    else if (DDF_CompareName(info, "IMAGE") == 0)
        (*type) = FNTYP_Image;
    else if (DDF_CompareName(info, "TRUETYPE") == 0)
        (*type) = FNTYP_TrueType;
    else
        DDF_Error("Unknown font type: %s\n", info);
}

static int FontParseCharacter(const char *buf)
{
#if 0  // the main parser strips out all the " quotes
	while (epi::IsSpaceASCII(*buf))
		buf++;

	if (buf[0] == '"')
	{
		// check for escaped quote
		if (buf[1] == '\\' && buf[2] == '"')
			return '"';

		return buf[1];
	}
#endif

    if (buf[0] > 0 && epi::IsDigitASCII(buf[0]) && epi::IsDigitASCII(buf[1]))
        return atoi(buf);

    return buf[0];

#if 0
	DDF_Error("Malformed character name: %s\n", buf);
	return 0;
#endif
}

//
// DDF_FontGetPatch
//
// Formats: PATCH123("x"), PATCH065(65),
//          PATCH456("a" : "z"), PATCH033(33:111).
//
static void DDF_FontGetPatch(const char *info, void *storage)
{
    fontpatch_c **patch_list = (fontpatch_c **)storage;

    char patch_buf[100];
    char range_buf[100];

    if (!DDF_MainDecodeBrackets(info, patch_buf, range_buf, 100))
        DDF_Error("Malformed font patch: %s\n", info);

    // find dividing colon
    char *colon = nullptr;

    if (strlen(range_buf) > 1)
        colon = (char *)DDF_MainDecodeList(range_buf, ':', true);

    if (colon) *colon++ = 0;

    int char1, char2;

    // get the characters

    char1 = FontParseCharacter(range_buf);

    if (colon)
    {
        char2 = FontParseCharacter(colon);

        if (char1 > char2)
            DDF_Error("Bad character range: %s > %s\n", range_buf, colon);
    }
    else
        char2 = char1;

    fontpatch_c *pat = new fontpatch_c(char1, char2, patch_buf);

    // add to list
    pat->next = *patch_list;

    *patch_list = pat;
}

// ---> fontpatch_c class

fontpatch_c::fontpatch_c(int _ch1, int _ch2, const char *_pat1)
    : next(nullptr), char1(_ch1), char2(_ch2), patch1(_pat1)
{
}

// ---> fontdef_c class

//
// fontdef_c constructor
//
fontdef_c::fontdef_c() : name() { Default(); }

//
// fontdef_c::CopyDetail()
//
// Copies all the detail with the exception of ddf info
//
void fontdef_c::CopyDetail(const fontdef_c &src)
{
    type                 = src.type;
    patches              = src.patches;  // FIXME: copy list
    image_name           = src.image_name;
    missing_patch        = src.missing_patch;
    spacing              = src.spacing;
    ttf_name             = src.ttf_name;
    default_size         = src.default_size;
    ttf_smoothing        = src.ttf_smoothing;
    ttf_smoothing_string = src.ttf_smoothing_string;
}

//
// fontdef_c::Default()
//
void fontdef_c::Default()
{
    type    = FNTYP_Patch;
    patches = nullptr;
    image_name.clear();
    missing_patch.clear();
    ttf_name.clear();
    default_size  = 0.0;
    spacing       = 0.0;
    ttf_smoothing = TTF_SMOOTH_ON_DEMAND;
    ttf_smoothing_string.clear();
}

//
// fontdef_container_c::Lookup()
//
fontdef_c *fontdef_container_c::Lookup(const char *refname)
{
    if (!refname || !refname[0]) return nullptr;

    for (auto iter = begin(); iter != end(); iter++)
    {
        fontdef_c *fnt = *iter;
        if (DDF_CompareName(fnt->name.c_str(), refname) == 0) return fnt;
    }

    return nullptr;
}

//
// DDF_MainLookupFont
//
void DDF_MainLookupFont(const char *info, void *storage)
{
    fontdef_c **dest = (fontdef_c **)storage;

    *dest = fontdefs.Lookup(info);

    if (*dest == nullptr) DDF_Error("Unknown font: %s\n", info);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
