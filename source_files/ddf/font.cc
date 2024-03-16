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
#include "str_util.h"

static FontDefinition *dynamic_font;

static void DDF_FontGetType(const char *info, void *storage);
static void DDF_FontGetPatch(const char *info, void *storage);

static FontDefinition dummy_font;

static const DDFCommandList font_commands[] = {
    DDF_FIELD("TYPE", dummy_font, type_, DDF_FontGetType),
    DDF_FIELD("PATCHES", dummy_font, patches_, DDF_FontGetPatch),
    DDF_FIELD("IMAGE", dummy_font, image_name_, DDF_MainGetString),
    DDF_FIELD("TTF", dummy_font, truetype_name_, DDF_MainGetString),
    DDF_FIELD("DEFAULT_SIZE", dummy_font, default_size_, DDF_MainGetFloat),
    DDF_FIELD("TTF_SMOOTHING", dummy_font, truetype_smoothing_string_,
              DDF_MainGetString),
    DDF_FIELD("MISSING_PATCH", dummy_font, missing_patch_, DDF_MainGetString),
    DDF_FIELD("SPACING", dummy_font, spacing_, DDF_MainGetFloat),

    { nullptr, nullptr, 0, nullptr }
};

// -ACB- 2004/06/03 Replaced array and size with purpose-built class
FontDefinitionContainer fontdefs;

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
    dynamic_font = new FontDefinition;

    dynamic_font->name_ = name;

    fontdefs.push_back(dynamic_font);
}

static void FontParseField(const char *field, const char *contents, int index,
                           bool is_last)
{
#if (DEBUG_DDF)
    LogDebug("FONT_PARSE: %s = %s;\n", field, contents);
#endif

    if (DDF_MainParseField(font_commands, field, contents,
                           (uint8_t *)dynamic_font))
        return;  // OK

    DDF_Error("Unknown fonts.ddf command: %s\n", field);
}

static void FontFinishEntry(void)
{
    if (dynamic_font->type_ == kFontTypeUnset)
        DDF_Error("No type specified for font.\n");

    if (dynamic_font->type_ == kFontTypePatch && !dynamic_font->patches_)
        DDF_Error("Missing font patch list.\n");

    if (dynamic_font->type_ == kFontTypeImage &&
        dynamic_font->image_name_.empty())
        DDF_Error("Missing font image name.\n");

    if (dynamic_font->type_ == kFontTypeTrueType &&
        dynamic_font->truetype_name_.empty())
        DDF_Error("Missing font TTF/OTF lump/file name.\n");

    if (dynamic_font->type_ == kFontTypeTrueType &&
        !dynamic_font->truetype_smoothing_string_.empty())
    {
        if (epi::StringCaseCompareASCII(
                dynamic_font->truetype_smoothing_string_, "NEVER") == 0)
            dynamic_font->truetype_smoothing_ =
                dynamic_font->kTrueTypeSmoothNever;
        else if (epi::StringCaseCompareASCII(
                     dynamic_font->truetype_smoothing_string_, "ALWAYS") == 0)
            dynamic_font->truetype_smoothing_ =
                dynamic_font->kTrueTypeSmoothAlways;
        else if (epi::StringCaseCompareASCII(
                     dynamic_font->truetype_smoothing_string_, "ON_DEMAND") ==
                 0)
            dynamic_font->truetype_smoothing_ =
                dynamic_font->kTrueTypeSmoothOnDemand;
    }
}

static void FontClearAll(void)
{
    LogWarning("Ignoring #CLEARALL in fonts.ddf\n");
}

void DDF_ReadFonts(const std::string &data)
{
    DDFReadInfo fonts;

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
    if (fontdefs.empty()) FatalError("There are no fonts defined in DDF !\n");

    fontdefs.shrink_to_fit();  // <-- Reduce to allocated size
}

//
// DDF_FontGetType
//
static void DDF_FontGetType(const char *info, void *storage)
{
    EPI_ASSERT(storage);

    FontType *type = (FontType *)storage;

    if (DDF_CompareName(info, "PATCH") == 0)
        (*type) = kFontTypePatch;
    else if (DDF_CompareName(info, "IMAGE") == 0)
        (*type) = kFontTypeImage;
    else if (DDF_CompareName(info, "TRUETYPE") == 0)
        (*type) = kFontTypeTrueType;
    else
        DDF_Error("Unknown font type: %s\n", info);
}

static int FontParseCharacter(const char *buf)
{
    if (buf[0] > 0 && epi::IsDigitASCII(buf[0]) && epi::IsDigitASCII(buf[1]))
        return atoi(buf);

    return buf[0];
}

//
// DDF_FontGetPatch
//
// Formats: PATCH123("x"), PATCH065(65),
//          PATCH456("a" : "z"), PATCH033(33:111).
//
static void DDF_FontGetPatch(const char *info, void *storage)
{
    FontPatch **patch_list = (FontPatch **)storage;

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

    FontPatch *pat = new FontPatch({ nullptr, char1, char2, patch_buf });

    // add to list
    pat->next = *patch_list;

    *patch_list = pat;
}

FontDefinition::FontDefinition() : name_() { Default(); }

void FontDefinition::CopyDetail(const FontDefinition &src)
{
    type_                      = src.type_;
    patches_                   = src.patches_;  // FIXME: copy list
    image_name_                = src.image_name_;
    missing_patch_             = src.missing_patch_;
    spacing_                   = src.spacing_;
    truetype_name_             = src.truetype_name_;
    default_size_              = src.default_size_;
    truetype_smoothing_        = src.truetype_smoothing_;
    truetype_smoothing_string_ = src.truetype_smoothing_string_;
}

//
// fontdef_c::Default()
//
void FontDefinition::Default()
{
    type_               = kFontTypePatch;
    patches_            = nullptr;
    default_size_       = 0.0;
    spacing_            = 0.0;
    truetype_smoothing_ = kTrueTypeSmoothOnDemand;
    truetype_smoothing_string_.clear();
    image_name_.clear();
    missing_patch_.clear();
    truetype_name_.clear();
}

//
// fontdef_container_c::Lookup()
//
FontDefinition *FontDefinitionContainer::Lookup(const char *refname)
{
    if (!refname || !refname[0]) return nullptr;

    for (std::vector<FontDefinition *>::iterator iter     = begin(),
                                                 iter_end = end();
         iter != iter_end; iter++)
    {
        FontDefinition *fnt = *iter;
        if (DDF_CompareName(fnt->name_.c_str(), refname) == 0) return fnt;
    }

    return nullptr;
}

//
// DDF_MainLookupFont
//
void DDF_MainLookupFont(const char *info, void *storage)
{
    FontDefinition **dest = (FontDefinition **)storage;

    *dest = fontdefs.Lookup(info);

    if (*dest == nullptr) DDF_Error("Unknown font: %s\n", info);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
