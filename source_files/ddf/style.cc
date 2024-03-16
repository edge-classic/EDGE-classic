//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Styles)
//----------------------------------------------------------------------------
//
//  Copyright (c) 2004-2024 The EDGE Team.
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
// Style Setup and Parser Code
//

#include "style.h"

#include "font.h"
#include "local.h"
#include "str_compare.h"

StyleDefinition *default_style;

static void DDF_StyleGetSpecials(const char *info, void *storage);

StyleDefinitionContainer styledefs;

static BackgroundStyle dummy_bgstyle;

static const DDFCommandList background_commands[] = {
    DDF_FIELD("COLOUR", dummy_bgstyle, colour_, DDF_MainGetRGB),
    DDF_FIELD("TRANSLUCENCY", dummy_bgstyle, translucency_, DDF_MainGetPercent),
    DDF_FIELD("IMAGE", dummy_bgstyle, image_name_, DDF_MainGetString),
    DDF_FIELD("SCALE", dummy_bgstyle, scale_, DDF_MainGetFloat),
    DDF_FIELD("ASPECT", dummy_bgstyle, aspect_, DDF_MainGetFloat),

    { nullptr, nullptr, 0, nullptr }
};

static TextStyle dummy_textstyle;

static const DDFCommandList text_commands[] = {
    DDF_FIELD("COLOURMAP", dummy_textstyle, colmap_, DDF_MainGetColourmap),
    DDF_FIELD("TRANSLUCENCY", dummy_textstyle, translucency_,
              DDF_MainGetPercent),
    DDF_FIELD("FONT", dummy_textstyle, font_, DDF_MainLookupFont),
    DDF_FIELD("SCALE", dummy_textstyle, scale_, DDF_MainGetFloat),
    DDF_FIELD("ASPECT", dummy_textstyle, aspect_, DDF_MainGetFloat),
    DDF_FIELD("X_OFFSET", dummy_textstyle, x_offset_, DDF_MainGetNumeric),
    DDF_FIELD("Y_OFFSET", dummy_textstyle, y_offset_, DDF_MainGetNumeric),

    { nullptr, nullptr, 0, nullptr }
};

static CursorStyle dummy_cursorstyle;

static const DDFCommandList cursor_commands[] = {
    DDF_FIELD("POSITION", dummy_cursorstyle, pos_string_, DDF_MainGetString),
    DDF_FIELD("TRANSLUCENCY", dummy_cursorstyle, translucency_,
              DDF_MainGetPercent),
    DDF_FIELD("IMAGE", dummy_cursorstyle, alt_cursor_, DDF_MainGetString),
    DDF_FIELD("STRING", dummy_cursorstyle, cursor_string_, DDF_MainGetString),
    DDF_FIELD("BORDER", dummy_cursorstyle, border_, DDF_MainGetBoolean),
    DDF_FIELD("SCALING", dummy_cursorstyle, scaling_, DDF_MainGetBoolean),
    DDF_FIELD("FORCE_OFFSETS", dummy_cursorstyle, force_offsets_,
              DDF_MainGetBoolean),

    { nullptr, nullptr, 0, nullptr }
};

static SoundStyle dummy_soundstyle;

static const DDFCommandList sound_commands[] = {
    DDF_FIELD("BEGIN", dummy_soundstyle, begin_, DDF_MainLookupSound),
    DDF_FIELD("END", dummy_soundstyle, end_, DDF_MainLookupSound),
    DDF_FIELD("SELECT", dummy_soundstyle, select_, DDF_MainLookupSound),
    DDF_FIELD("BACK", dummy_soundstyle, back_, DDF_MainLookupSound),
    DDF_FIELD("ERROR", dummy_soundstyle, error_, DDF_MainLookupSound),
    DDF_FIELD("MOVE", dummy_soundstyle, move_, DDF_MainLookupSound),
    DDF_FIELD("SLIDER", dummy_soundstyle, slider_, DDF_MainLookupSound),

    { nullptr, nullptr, 0, nullptr }
};

static StyleDefinition *dynamic_style;

static StyleDefinition dummy_style;

static const DDFCommandList style_commands[] = {
    // sub-commands
    DDF_SUB_LIST("BACKGROUND", dummy_style, bg_, background_commands),
    DDF_SUB_LIST("CURSOR", dummy_style, cursor_, cursor_commands),
    DDF_SUB_LIST("TEXT", dummy_style, text_[0], text_commands),
    DDF_SUB_LIST("ALT", dummy_style, text_[1], text_commands),
    DDF_SUB_LIST("TITLE", dummy_style, text_[2], text_commands),
    DDF_SUB_LIST("HELP", dummy_style, text_[3], text_commands),
    DDF_SUB_LIST("HEADER", dummy_style, text_[4], text_commands),
    DDF_SUB_LIST("SELECTED", dummy_style, text_[5], text_commands),
    DDF_SUB_LIST("SOUND", dummy_style, sounds_, sound_commands),
    DDF_FIELD("X_OFFSET", dummy_style, x_offset_, DDF_MainGetNumeric),
    DDF_FIELD("Y_OFFSET", dummy_style, y_offset_, DDF_MainGetNumeric),
    DDF_FIELD("ENTRY_ALIGNMENT", dummy_style, entry_align_string_,
              DDF_MainGetString),
    DDF_FIELD("ENTRY_SPACING", dummy_style, entry_spacing_, DDF_MainGetNumeric),

    DDF_FIELD("SPECIAL", dummy_style, special_, DDF_StyleGetSpecials),

    { nullptr, nullptr, 0, nullptr }
};

//
//  DDF PARSE ROUTINES
//

static void StyleStartEntry(const char *name, bool extend)
{
    if (!name || !name[0])
    {
        DDF_WarnError("New style entry is missing a name!");
        name = "STYLE_WITH_NO_NAME";
    }

    // replaces an existing entry?
    dynamic_style = styledefs.Lookup(name);

    if (extend)
    {
        if (!dynamic_style) DDF_Error("Unknown style to extend: %s\n", name);
        return;
    }

    if (dynamic_style)
    {
        dynamic_style->Default();
        return;
    }

    // not found, create a new one
    dynamic_style        = new StyleDefinition;
    dynamic_style->name_ = name;

    styledefs.push_back(dynamic_style);
}

static void StyleParseField(const char *field, const char *contents, int index,
                            bool is_last)
{
#if (DEBUG_DDF)
    LogDebug("STYLE_PARSE: %s = %s;\n", field, contents);
#endif

    if (DDF_MainParseField(style_commands, field, contents,
                           (uint8_t *)dynamic_style))
        return;  // OK

    DDF_WarnError("Unknown styles.ddf command: %s\n", field);
}

static void StyleFinishEntry(void)
{
    if (dynamic_style->cursor_.pos_string_ != "")
    {
        const char *pos_str = dynamic_style->cursor_.pos_string_.c_str();

        if (epi::StringCaseCompareASCII(pos_str, "LEFT") == 0)
            dynamic_style->cursor_.position_ = StyleDefinition::kAlignmentLeft;
        else if (epi::StringCaseCompareASCII(pos_str, "CENTER") == 0)
            dynamic_style->cursor_.position_ =
                StyleDefinition::kAlignmentCenter;
        else if (epi::StringCaseCompareASCII(pos_str, "RIGHT") == 0)
            dynamic_style->cursor_.position_ = StyleDefinition::kAlignmentRight;
        else if (epi::StringCaseCompareASCII(pos_str, "BOTH") == 0)
            dynamic_style->cursor_.position_ = StyleDefinition::kAlignmentBoth;
    }

    if (dynamic_style->entry_align_string_ != "")
    {
        const char *align_str = dynamic_style->entry_align_string_.c_str();

        if (epi::StringCaseCompareASCII(align_str, "LEFT") == 0)
            dynamic_style->entry_alignment_ = StyleDefinition::kAlignmentLeft;
        else if (epi::StringCaseCompareASCII(align_str, "CENTER") == 0)
            dynamic_style->entry_alignment_ = StyleDefinition::kAlignmentCenter;
        else if (epi::StringCaseCompareASCII(align_str, "RIGHT") == 0)
            dynamic_style->entry_alignment_ = StyleDefinition::kAlignmentRight;
    }
}

static void StyleClearAll(void)
{
    LogWarning("Ignoring #CLEARALL in styles.ddf\n");
}

void DDF_ReadStyles(const std::string &data)
{
    DDFReadInfo styles;

    styles.tag      = "STYLES";
    styles.lumpname = "DDFSTYLE";

    styles.start_entry  = StyleStartEntry;
    styles.parse_field  = StyleParseField;
    styles.finish_entry = StyleFinishEntry;
    styles.clear_all    = StyleClearAll;

    DDF_MainReadFile(&styles, data);
}

void DDF_StyleInit(void)
{
    for (auto s : styledefs)
    {
        delete s;
        s = nullptr;
    }
    styledefs.clear();
}

void DDF_StyleCleanUp(void)
{
    if (styledefs.empty()) FatalError("There are no styles defined in DDF !\n");

    default_style = styledefs.Lookup("DEFAULT");

    if (!default_style)
        FatalError("Styles.ddf is missing the [DEFAULT] style.\n");
    else if (!default_style->text_[0].font_)
        LogWarning("The [DEFAULT] style is missing TEXT.FONT\n");

    styledefs.shrink_to_fit();
}

static DDFSpecialFlags style_specials[] = {
    { "TILED", kStyleSpecialTiled, 0 },
    { "TILED_NOSCALE", kStyleSpecialTiledNoScale, 0 },
    { "STRETCH_FULLSCREEN", kStyleSpecialStretchFullScreen, 0 },
    { nullptr, 0, 0 }
};

void DDF_StyleGetSpecials(const char *info, void *storage)
{
    StyleSpecial *dest = (StyleSpecial *)storage;

    int flag_value;

    switch (DDF_MainCheckSpecialFlag(info, style_specials, &flag_value, true,
                                     false))
    {
        case kDDFCheckFlagPositive:
            *dest = (StyleSpecial)(*dest | flag_value);
            break;

        case kDDFCheckFlagNegative:
            *dest = (StyleSpecial)(*dest & ~flag_value);
            break;

        case kDDFCheckFlagUser:
        case kDDFCheckFlagUnknown:
            DDF_WarnError("Unknown style special: %s", info);
            break;
    }
}

// --> BackgroundStyle definition class

//
// BackgroundStyle Constructor
//
BackgroundStyle::BackgroundStyle() { Default(); }

//
// BackgroundStyle Copy constructor
//
BackgroundStyle::BackgroundStyle(const BackgroundStyle &rhs) { *this = rhs; }

//
// BackgroundStyle Destructor
//
BackgroundStyle::~BackgroundStyle() {}

//
// BackgroundStyle::Default()
//
void BackgroundStyle::Default()
{
    colour_       = kRGBANoValue;
    translucency_ = 1.0f;

    image_name_.clear();

    scale_  = 1.0f;
    aspect_ = 1.0f;
}

//
// BackgroundStyle assignment operator
//
BackgroundStyle &BackgroundStyle::operator=(const BackgroundStyle &rhs)
{
    if (&rhs != this)
    {
        colour_       = rhs.colour_;
        translucency_ = rhs.translucency_;

        image_name_ = rhs.image_name_;

        scale_  = rhs.scale_;
        aspect_ = rhs.aspect_;
    }

    return *this;
}

// --> TextStyle definition class

//
// TextStyle Constructor
//
TextStyle::TextStyle() { Default(); }

//
// TextStyle Copy constructor
//
TextStyle::TextStyle(const TextStyle &rhs) { *this = rhs; }

//
// TextStyle Destructor
//
TextStyle::~TextStyle() {}

//
// TextStyle::Default()
//
void TextStyle::Default()
{
    colmap_       = nullptr;
    translucency_ = 1.0f;

    font_     = nullptr;
    scale_    = 1.0f;
    aspect_   = 1.0f;
    x_offset_ = 0;
    y_offset_ = 0;
}

//
// TextStyle assignment operator
//
TextStyle &TextStyle::operator=(const TextStyle &rhs)
{
    if (&rhs != this)
    {
        colmap_       = rhs.colmap_;
        translucency_ = rhs.translucency_;

        font_     = rhs.font_;
        scale_    = rhs.scale_;
        aspect_   = rhs.aspect_;
        x_offset_ = rhs.x_offset_;
        y_offset_ = rhs.y_offset_;
    }

    return *this;
}

// --> CursorStyle definition class

//
// CursorStyle Constructor
//
CursorStyle::CursorStyle() { Default(); }

//
// CursorStyle Copy constructor
//
CursorStyle::CursorStyle(const CursorStyle &rhs) { *this = rhs; }

//
// CursorStyle Destructor
//
CursorStyle::~CursorStyle() {}

//
// CursorStyle::Default()
//
void CursorStyle::Default()
{
    position_      = 0;
    translucency_  = 1.0f;
    pos_string_    = "";
    alt_cursor_    = "";
    cursor_string_ = "";
    border_        = false;
    scaling_       = true;
    force_offsets_ = false;
}

//
// CursorStyle assignment operator
//
CursorStyle &CursorStyle::operator=(const CursorStyle &rhs)
{
    if (&rhs != this)
    {
        position_      = rhs.position_;
        translucency_  = rhs.translucency_;
        pos_string_    = rhs.pos_string_;
        alt_cursor_    = rhs.alt_cursor_;
        cursor_string_ = rhs.cursor_string_;
        border_        = rhs.border_;
        scaling_       = rhs.scaling_;
        force_offsets_ = rhs.force_offsets_;
    }

    return *this;
}

// --> SoundStyle definition class

//
// SoundStyle Constructor
//
SoundStyle::SoundStyle() { Default(); }

//
// SoundStyle Copy constructor
//
SoundStyle::SoundStyle(const SoundStyle &rhs) { *this = rhs; }

//
// SoundStyle Destructor
//
SoundStyle::~SoundStyle() {}

//
// SoundStyle::Default()
//
void SoundStyle::Default()
{
    begin_  = nullptr;
    end_    = nullptr;
    select_ = nullptr;
    back_   = nullptr;
    error_  = nullptr;
    move_   = nullptr;
    slider_ = nullptr;
}

//
// SoundStyle assignment operator
//
SoundStyle &SoundStyle::operator=(const SoundStyle &rhs)
{
    if (&rhs != this)
    {
        begin_  = rhs.begin_;
        end_    = rhs.end_;
        select_ = rhs.select_;
        back_   = rhs.back_;
        error_  = rhs.error_;
        move_   = rhs.move_;
        slider_ = rhs.slider_;
    }

    return *this;
}

// --> style definition class

//
// StyleDefinition Constructor
//
StyleDefinition::StyleDefinition() : name_() { Default(); }

//
// StyleDefinition Destructor
//
StyleDefinition::~StyleDefinition() {}

//
// StyleDefinition::CopyDetail()
//
void StyleDefinition::CopyDetail(const StyleDefinition &src)
{
    bg_ = src.bg_;

    for (int T = 0; T < kTotalTextSections; T++) text_[T] = src.text_[T];

    sounds_ = src.sounds_;

    x_offset_ = src.x_offset_;
    y_offset_ = src.y_offset_;

    special_ = src.special_;

    entry_align_string_ = src.entry_align_string_;
    entry_alignment_    = src.entry_alignment_;
    entry_spacing_      = src.entry_spacing_;
}

//
// StyleDefinition::Default()
//
void StyleDefinition::Default()
{
    bg_.Default();

    for (int T = 0; T < kTotalTextSections; T++) text_[T].Default();

    sounds_.Default();

    x_offset_ = 0;
    y_offset_ = 0;

    special_ = kStyleSpecialNone;  //(StyleSpecial)
                                   // kStyleSpecialStretchFullScreen; // I
                                   // think this might be better for backwards
                                   // compat, revert to 0 if needed - Dasho

    entry_align_string_ = "";
    entry_alignment_    = 0;
    entry_spacing_      = 0;
}

// --> map definition container class

//
// StyleDefinitionContainer::Lookup()
//
// Finds a styledef by name, returns nullptr if it doesn't exist.
//
StyleDefinition *StyleDefinitionContainer::Lookup(const char *refname)
{
    if (!refname || !refname[0]) return nullptr;

    for (std::vector<StyleDefinition *>::reverse_iterator iter     = rbegin(),
                                                          iter_end = rend();
         iter != iter_end; iter++)
    {
        StyleDefinition *m = *iter;
        if (DDF_CompareName(m->name_.c_str(), refname) == 0) return m;
    }

    return nullptr;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
