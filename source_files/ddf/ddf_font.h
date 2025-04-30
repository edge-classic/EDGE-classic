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

#pragma once

#include "ddf_types.h"
#include "epi.h"

enum FontType
{
    kFontTypeUnset    = 0,
    kFontTypePatch    = 1, // font is made up of individual patches
    kFontTypeImage    = 2, // font consists of one big image (16x16 chars)
    kFontTypeTrueType = 3  // font is a ttf/otf file or lump
};

struct FontPatch
{
    FontPatch  *next;         // link in list
    int         char1, char2; // range
    std::string patch1;
};

class FontDefinition
{
  public:
    FontDefinition();
    ~FontDefinition();

  public:
    void Default(void);
    void CopyDetail(const FontDefinition &src);

    // Member vars....
    std::string name_;

    FontType type_;

    FontPatch  *patches_ = nullptr;
    std::string missing_patch_;

    std::string image_name_;

    float spacing_;
    float default_size_;

    // TTF Stuff
    enum TrueTypeSmoothing
    {
        kTrueTypeSmoothOnDemand = 0,
        kTrueTypeSmoothAlways   = 1,
        kTrueTypeSmoothNever    = 2
    };

    std::string truetype_name_;
    int         truetype_smoothing_;
    std::string truetype_smoothing_string_; // User convenience

  private:
    // disable copy construct and assignment operator
    explicit FontDefinition(FontDefinition &rhs)
    {
        EPI_UNUSED(rhs);
    }
    FontDefinition &operator=(FontDefinition &rhs)
    {
        EPI_UNUSED(rhs);
        return *this;
    }
};

// Our fontdefs container
class FontDefinitionContainer : public std::vector<FontDefinition *>
{
  public:
    FontDefinitionContainer()
    {
    }
    ~FontDefinitionContainer()
    {
        for (std::vector<FontDefinition *>::iterator iter = begin(), iter_end = end(); iter != iter_end; iter++)
        {
            FontDefinition *fnt = *iter;
            delete fnt;
            fnt = nullptr;
        }
    }

  public:
    // Search Functions
    FontDefinition *Lookup(const char *refname);
};

extern FontDefinitionContainer fontdefs;

void DDFMainLookupFont(const char *info, void *storage);

void DDFReadFonts(const std::string &data);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
