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

#ifndef __DDF_FONT__
#define __DDF_FONT__

#include "epi.h"
#include "types.h"

//
// -AJA- 2004/11/13 Fonts.ddf
//
typedef enum
{
    FNTYP_UNSET = 0,

    FNTYP_Patch    = 1,  // font is made up of individual patches
    FNTYP_Image    = 2,  // font consists of one big image (16x16 chars)
    FNTYP_TrueType = 3   // font is a ttf/otf file or lump
} fonttype_e;

class fontpatch_c
{
   public:
    fontpatch_c(int _ch1, int _ch2, const char *_pat1);
    ~fontpatch_c();

    fontpatch_c *next;  // link in list

    int char1, char2;  // range

    std::string patch1;
};

class fontdef_c
{
   public:
    fontdef_c();
    ~fontdef_c(){};

   public:
    void Default(void);
    void CopyDetail(const fontdef_c &src);

    // Member vars....
    std::string name;

    fonttype_e type;

    fontpatch_c *patches;
    std::string  missing_patch;

    std::string image_name;

    float spacing;
    float default_size;

    // TTF Stuff
    enum
    {
        TTF_SMOOTH_ON_DEMAND = 0,
        TTF_SMOOTH_ALWAYS    = 1,
        TTF_SMOOTH_NEVER     = 2
    };

    std::string ttf_name;
    int         ttf_smoothing;
    std::string ttf_smoothing_string;  // User convenience

   private:
    // disable copy construct and assignment operator
    explicit fontdef_c(fontdef_c &rhs) { (void)rhs; }
    fontdef_c &operator=(fontdef_c &rhs)
    {
        (void)rhs;
        return *this;
    }
};

// Our fontdefs container
class fontdef_container_c : public std::vector<fontdef_c *>
{
   public:
    fontdef_container_c() {}
    ~fontdef_container_c()
    {
        for (auto iter = begin(); iter != end(); iter++)
        {
            fontdef_c *fnt = *iter;
            delete fnt;
            fnt = nullptr;
        }
    }

   public:
    // Search Functions
    fontdef_c *Lookup(const char *refname);
};

extern fontdef_container_c fontdefs;

void DDF_MainLookupFont(const char *info, void *storage);

void DDF_ReadFonts(const std::string &data);

#endif /* __DDF_FONT__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
