//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Styles)
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

#pragma once

#include "ddf_types.h"

class FontDefinition;
class Colormap;

//
// -AJA- 2004/11/14 Styles.ddf
//
class BackgroundStyle
{
  public:
    BackgroundStyle();
    BackgroundStyle(const BackgroundStyle &rhs);
    ~BackgroundStyle();

    void             Default();
    BackgroundStyle &operator=(const BackgroundStyle &rhs);

    RGBAColor colour_;
    float     translucency_;

    std::string image_name_;

    float scale_;
    float aspect_;
};

class TextStyle
{
  public:
    TextStyle();
    TextStyle(const TextStyle &rhs);
    ~TextStyle();

    void       Default();
    TextStyle &operator=(const TextStyle &rhs);

    const Colormap *colmap_;

    float translucency_;

    FontDefinition *font_;

    float scale_;
    float aspect_;
    int   x_offset_;
    int   y_offset_;
};

class CursorStyle
{
  public:
    CursorStyle();
    CursorStyle(const CursorStyle &rhs);
    ~CursorStyle();

    void         Default();
    CursorStyle &operator=(const CursorStyle &rhs);

    int   position_;
    float translucency_;

    std::string alt_cursor_;
    std::string pos_string_; // Here for user convenience, is translated to a
                             // value for position
    std::string cursor_string_;

    bool force_offsets_;
    bool scaling_;
    bool border_;
};

class SoundStyle
{
  public:
    SoundStyle();
    SoundStyle(const SoundStyle &rhs);
    ~SoundStyle();

    void        Default();
    SoundStyle &operator=(const SoundStyle &rhs);

    struct SoundEffect *begin_;
    struct SoundEffect *end_;
    struct SoundEffect *select_;
    struct SoundEffect *back_;
    struct SoundEffect *error_;
    struct SoundEffect *move_;
    struct SoundEffect *slider_;
};

enum StyleSpecial
{
    kStyleSpecialNone              = 0,
    kStyleSpecialTiled             = 0x0001, // bg image should tile (otherwise covers whole area)
    kStyleSpecialTiledNoScale      = 0x0002, // bg image should tile (1:1 pixels)
    kStyleSpecialStretchFullScreen = 0x0004, // bg image will be stretched to fill the screen
};

class StyleDefinition
{
  public:
    StyleDefinition();
    ~StyleDefinition();

  public:
    void Default(void);
    void CopyDetail(const StyleDefinition &src);

    // Member vars....
    std::string name_;

    BackgroundStyle bg_;

    // the four text styles
    enum TextSection
    {
        kTextSectionText = 0,  // main text style
        kTextSectionAlternate, // alternative text style
        kTextSectionTitle,     // title style
        kTextSectionHelp,      // for help messages
        kTextSectionHeader,    // for header /main title
        kTextSectionSelected,  // for selected menu item
        kTotalTextSections
    };

    enum Alignment
    {
        kAlignmentLeft   = 0,
        kAlignmentCenter = 1,
        kAlignmentRight  = 2,
        kAlignmentBoth   = 3
    };

    TextStyle text_[kTotalTextSections];

    CursorStyle cursor_;

    SoundStyle sounds_;

    StyleSpecial special_;

    int x_offset_;
    int y_offset_;

    int         entry_alignment_;
    int         entry_spacing_;
    std::string entry_align_string_; // User convenience

  private:
    // disable copy construct and assignment operator
    explicit StyleDefinition(StyleDefinition &rhs)
    {
    }
    StyleDefinition &operator=(StyleDefinition &rhs)
    {
        return *this;
    }
};

// Our styledefs container
class StyleDefinitionContainer : public std::vector<StyleDefinition *>
{
  public:
    StyleDefinitionContainer()
    {
    }
    ~StyleDefinitionContainer()
    {
        for (std::vector<StyleDefinition *>::iterator iter = begin(), iter_end = end(); iter != iter_end; iter++)
        {
            StyleDefinition *s = *iter;
            delete s;
            s = nullptr;
        }
    }

  public:
    // Search Functions
    StyleDefinition *Lookup(const char *refname);
};

extern StyleDefinitionContainer styledefs;
extern StyleDefinition         *default_style;

void DDF_ReadStyles(const std::string &data);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
