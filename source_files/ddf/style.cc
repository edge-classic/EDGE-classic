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

#include "local.h"
#include "font.h"
#include "style.h"

#undef DF
#define DF DDF_FIELD

styledef_c *default_style;

static void DDF_StyleGetSpecials(const char *info, void *storage);

styledef_container_c styledefs;

#undef DDF_CMD_BASE
#define DDF_CMD_BASE dummy_bgstyle
static backgroundstyle_c dummy_bgstyle;

static const commandlist_t background_commands[] = {DF("COLOUR", colour, DDF_MainGetRGB),
                                                    DF("TRANSLUCENCY", translucency, DDF_MainGetPercent),
                                                    DF("IMAGE", image_name, DDF_MainGetString),
                                                    DF("SCALE", scale, DDF_MainGetFloat),
                                                    DF("ASPECT", aspect, DDF_MainGetFloat),

                                                    DDF_CMD_END};

#undef DDF_CMD_BASE
#define DDF_CMD_BASE dummy_textstyle
static textstyle_c dummy_textstyle;

static const commandlist_t text_commands[] = {DF("COLOURMAP", colmap, DDF_MainGetColourmap),
                                              DF("TRANSLUCENCY", translucency, DDF_MainGetPercent),
                                              DF("FONT", font, DDF_MainLookupFont),
                                              DF("SCALE", scale, DDF_MainGetFloat),
                                              DF("ASPECT", aspect, DDF_MainGetFloat),
                                              DF("X_OFFSET", x_offset, DDF_MainGetNumeric),
                                              DF("Y_OFFSET", y_offset, DDF_MainGetNumeric),

                                              DDF_CMD_END};

#undef DDF_CMD_BASE
#define DDF_CMD_BASE dummy_cursorstyle
static cursorstyle_c dummy_cursorstyle;

static const commandlist_t cursor_commands[] = {DF("POSITION", pos_string, DDF_MainGetString),
                                                DF("TRANSLUCENCY", translucency, DDF_MainGetPercent),
                                                DF("IMAGE", alt_cursor, DDF_MainGetString),
                                                DF("STRING", cursor_string, DDF_MainGetString),
                                                DF("BORDER", border, DDF_MainGetBoolean),
                                                DF("SCALING", scaling, DDF_MainGetBoolean),
                                                DF("FORCE_OFFSETS", force_offsets, DDF_MainGetBoolean),

                                                DDF_CMD_END};

#undef DDF_CMD_BASE
#define DDF_CMD_BASE dummy_soundstyle
static soundstyle_c dummy_soundstyle;

static const commandlist_t sound_commands[] = {DF("BEGIN", begin, DDF_MainLookupSound),
                                               DF("END", end, DDF_MainLookupSound),
                                               DF("SELECT", select, DDF_MainLookupSound),
                                               DF("BACK", back, DDF_MainLookupSound),
                                               DF("ERROR", error, DDF_MainLookupSound),
                                               DF("MOVE", move, DDF_MainLookupSound),
                                               DF("SLIDER", slider, DDF_MainLookupSound),

                                               DDF_CMD_END};

static styledef_c *dynamic_style;

#undef DDF_CMD_BASE
#define DDF_CMD_BASE dummy_style
static styledef_c dummy_style;

static const commandlist_t style_commands[] = {
    // sub-commands
    DDF_SUB_LIST("BACKGROUND", bg, background_commands),
    DDF_SUB_LIST("CURSOR", cursor, cursor_commands),
    DDF_SUB_LIST("TEXT", text[0], text_commands),
    DDF_SUB_LIST("ALT", text[1], text_commands),
    DDF_SUB_LIST("TITLE", text[2], text_commands),
    DDF_SUB_LIST("HELP", text[3], text_commands),
    DDF_SUB_LIST("HEADER", text[4], text_commands),
    DDF_SUB_LIST("SELECTED", text[5], text_commands),
    DDF_SUB_LIST("SOUND", sounds, sound_commands),
    DF("X_OFFSET", x_offset, DDF_MainGetNumeric),
    DF("Y_OFFSET", y_offset, DDF_MainGetNumeric),
    DF("ENTRY_ALIGNMENT", entry_align_string, DDF_MainGetString),
    DF("ENTRY_SPACING", entry_spacing, DDF_MainGetNumeric),

    DF("SPECIAL", special, DDF_StyleGetSpecials),

    DDF_CMD_END};

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
        if (!dynamic_style)
            DDF_Error("Unknown style to extend: %s\n", name);
        return;
    }

    if (dynamic_style)
    {
        dynamic_style->Default();
        return;
    }

    // not found, create a new one
    dynamic_style       = new styledef_c;
    dynamic_style->name = name;

    styledefs.push_back(dynamic_style);
}

static void StyleParseField(const char *field, const char *contents, int index, bool is_last)
{
#if (DEBUG_DDF)
    I_Debugf("STYLE_PARSE: %s = %s;\n", field, contents);
#endif

    if (DDF_MainParseField(style_commands, field, contents, (uint8_t *)dynamic_style))
        return; // OK

    DDF_WarnError("Unknown styles.ddf command: %s\n", field);
}

static void StyleFinishEntry(void)
{
    if (dynamic_style->cursor.pos_string != "")
    {
        const char *pos_str = dynamic_style->cursor.pos_string.c_str();

        if (epi::case_cmp(pos_str, "LEFT") == 0)
            dynamic_style->cursor.position = dynamic_style->C_LEFT;
        else if (epi::case_cmp(pos_str, "CENTER") == 0)
            dynamic_style->cursor.position = dynamic_style->C_CENTER;
        else if (epi::case_cmp(pos_str, "RIGHT") == 0)
            dynamic_style->cursor.position = dynamic_style->C_RIGHT;
        else if (epi::case_cmp(pos_str, "BOTH") == 0)
            dynamic_style->cursor.position = dynamic_style->C_BOTH;
    }

    if (dynamic_style->entry_align_string != "")
    {
        const char *align_str = dynamic_style->entry_align_string.c_str();

        if (epi::case_cmp(align_str, "LEFT") == 0)
            dynamic_style->entry_alignment = dynamic_style->C_LEFT;
        else if (epi::case_cmp(align_str, "CENTER") == 0)
            dynamic_style->entry_alignment = dynamic_style->C_CENTER;
        else if (epi::case_cmp(align_str, "RIGHT") == 0)
            dynamic_style->entry_alignment = dynamic_style->C_RIGHT;
    }
}

static void StyleClearAll(void)
{
    I_Warning("Ignoring #CLEARALL in styles.ddf\n");
}

void DDF_ReadStyles(const std::string &data)
{
    readinfo_t styles;

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
    if (styledefs.empty())
        I_Error("There are no styles defined in DDF !\n");

    default_style = styledefs.Lookup("DEFAULT");

    if (!default_style)
        I_Error("Styles.ddf is missing the [DEFAULT] style.\n");
    else if (!default_style->text[0].font)
        I_Warning("The [DEFAULT] style is missing TEXT.FONT\n");

    styledefs.shrink_to_fit();
}

static specflags_t style_specials[] = {{"TILED", SYLSP_Tiled, 0},
                                       {"TILED_NOSCALE", SYLSP_TiledNoScale, 0},
                                       {"STRETCH_FULLSCREEN", SYLSP_StretchFullScreen, 0},
                                       {NULL, 0, 0}};

void DDF_StyleGetSpecials(const char *info, void *storage)
{
    style_special_e *dest = (style_special_e *)storage;

    int flag_value;

    switch (DDF_MainCheckSpecialFlag(info, style_specials, &flag_value, true, false))
    {
    case CHKF_Positive:
        *dest = (style_special_e)(*dest | flag_value);
        break;

    case CHKF_Negative:
        *dest = (style_special_e)(*dest & ~flag_value);
        break;

    case CHKF_User:
    case CHKF_Unknown:
        DDF_WarnError("Unknown style special: %s", info);
        break;
    }
}

// --> backgroundstyle_c definition class

//
// backgroundstyle_c Constructor
//
backgroundstyle_c::backgroundstyle_c()
{
    Default();
}

//
// backgroundstyle_c Copy constructor
//
backgroundstyle_c::backgroundstyle_c(const backgroundstyle_c &rhs)
{
    *this = rhs;
}

//
// backgroundstyle_c Destructor
//
backgroundstyle_c::~backgroundstyle_c()
{
}

//
// backgroundstyle_c::Default()
//
void backgroundstyle_c::Default()
{
    colour       = RGB_NO_VALUE;
    translucency = PERCENT_MAKE(100);

    image_name.clear();

    scale  = 1.0f;
    aspect = 1.0f;
}

//
// backgroundstyle_c assignment operator
//
backgroundstyle_c &backgroundstyle_c::operator=(const backgroundstyle_c &rhs)
{
    if (&rhs != this)
    {
        colour       = rhs.colour;
        translucency = rhs.translucency;

        image_name = rhs.image_name;

        scale  = rhs.scale;
        aspect = rhs.aspect;
    }

    return *this;
}

// --> textstyle_c definition class

//
// textstyle_c Constructor
//
textstyle_c::textstyle_c()
{
    Default();
}

//
// textstyle_c Copy constructor
//
textstyle_c::textstyle_c(const textstyle_c &rhs)
{
    *this = rhs;
}

//
// textstyle_c Destructor
//
textstyle_c::~textstyle_c()
{
}

//
// textstyle_c::Default()
//
void textstyle_c::Default()
{
    colmap       = NULL;
    translucency = PERCENT_MAKE(100);

    font     = NULL;
    scale    = 1.0f;
    aspect   = 1.0f;
    x_offset = 0;
    y_offset = 0;
}

//
// textstyle_c assignment operator
//
textstyle_c &textstyle_c::operator=(const textstyle_c &rhs)
{
    if (&rhs != this)
    {
        colmap       = rhs.colmap;
        translucency = rhs.translucency;

        font     = rhs.font;
        scale    = rhs.scale;
        aspect   = rhs.aspect;
        x_offset = rhs.x_offset;
        y_offset = rhs.y_offset;
    }

    return *this;
}

// --> cursorstyle_c definition class

//
// cursorstyle_c Constructor
//
cursorstyle_c::cursorstyle_c()
{
    Default();
}

//
// cursorstyle_c Copy constructor
//
cursorstyle_c::cursorstyle_c(const cursorstyle_c &rhs)
{
    *this = rhs;
}

//
// cursorstyle_c Destructor
//
cursorstyle_c::~cursorstyle_c()
{
}

//
// cursorstyle_c::Default()
//
void cursorstyle_c::Default()
{
    position      = 0;
    translucency  = PERCENT_MAKE(100);
    pos_string    = "";
    alt_cursor    = "";
    cursor_string = "";
    border        = false;
    scaling       = true;
    force_offsets = false;
}

//
// cursorstyle_c assignment operator
//
cursorstyle_c &cursorstyle_c::operator=(const cursorstyle_c &rhs)
{
    if (&rhs != this)
    {
        position      = rhs.position;
        translucency  = rhs.translucency;
        pos_string    = rhs.pos_string;
        alt_cursor    = rhs.alt_cursor;
        cursor_string = rhs.cursor_string;
        border        = rhs.border;
        scaling       = rhs.scaling;
        force_offsets = rhs.force_offsets;
    }

    return *this;
}

// --> soundstyle_c definition class

//
// soundstyle_c Constructor
//
soundstyle_c::soundstyle_c()
{
    Default();
}

//
// soundstyle_c Copy constructor
//
soundstyle_c::soundstyle_c(const soundstyle_c &rhs)
{
    *this = rhs;
}

//
// soundstyle_c Destructor
//
soundstyle_c::~soundstyle_c()
{
}

//
// soundstyle_c::Default()
//
void soundstyle_c::Default()
{
    begin  = NULL;
    end    = NULL;
    select = NULL;
    back   = NULL;
    error  = NULL;
    move   = NULL;
    slider = NULL;
}

//
// soundstyle_c assignment operator
//
soundstyle_c &soundstyle_c::operator=(const soundstyle_c &rhs)
{
    if (&rhs != this)
    {
        begin  = rhs.begin;
        end    = rhs.end;
        select = rhs.select;
        back   = rhs.back;
        error  = rhs.error;
        move   = rhs.move;
        slider = rhs.slider;
    }

    return *this;
}

// --> style definition class

//
// styledef_c Constructor
//
styledef_c::styledef_c() : name()
{
    Default();
}

//
// styledef_c Destructor
//
styledef_c::~styledef_c()
{
}

//
// styledef_c::CopyDetail()
//
void styledef_c::CopyDetail(const styledef_c &src)
{
    bg = src.bg;

    for (int T = 0; T < NUM_TXST; T++)
        text[T] = src.text[T];

    sounds = src.sounds;

    x_offset = src.x_offset;
    y_offset = src.y_offset;

    special = src.special;

    entry_align_string = src.entry_align_string;
    entry_alignment    = src.entry_alignment;
    entry_spacing      = src.entry_spacing;
}

//
// styledef_c::Default()
//
void styledef_c::Default()
{
    bg.Default();

    for (int T = 0; T < NUM_TXST; T++)
        text[T].Default();

    sounds.Default();

    x_offset = 0;
    y_offset = 0;

    special = SYLSP_None; //(style_special_e) SYLSP_StretchFullScreen; // I think this might be better for backwards
                          //compat, revert to 0 if needed - Dasho

    entry_align_string = "";
    entry_alignment    = 0;
    entry_spacing      = 0;
}

// --> map definition container class

//
// styledef_container_c::Lookup()
//
// Finds a styledef by name, returns NULL if it doesn't exist.
//
styledef_c *styledef_container_c::Lookup(const char *refname)
{
    if (!refname || !refname[0])
        return NULL;

    for (auto iter = rbegin(); iter != rend(); iter++)
    {
        styledef_c *m = *iter;
        if (DDF_CompareName(m->name.c_str(), refname) == 0)
            return m;
    }

    return NULL;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
