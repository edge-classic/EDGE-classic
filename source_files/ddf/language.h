//----------------------------------------------------------------------------
//  EDGE Language Definitions
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

#ifndef __DDF_LANG_H__
#define __DDF_LANG_H__

#include "epi.h"
#include "types.h"

// ------------------------------------------------------------------
// ---------------------------LANGUAGES------------------------------
// ------------------------------------------------------------------

class lang_choice_c;

class language_c
{
   public:
    language_c();
    ~language_c();

   private:
    std::vector<lang_choice_c *> choices;

    // UMAPINFO strings
    lang_choice_c *umap;

    // the current language choice
    int current;

   public:
    void Clear();

    int GetChoiceCount() { return (int)choices.size(); }
    int GetChoice() { return current; }

    const char *GetName(int idx = -1);
    bool        IsValidRef(const char *refname);

    bool Select(const char *name);
    bool Select(int idx);

    const char *operator[](const char *refname);
    const char *operator[](const std::string &refname)
    {
        return (*this)[refname.c_str()];
    }

    // this is for UMAPINFO strings
    void        AddOrReplace(const char *ref, const char *value);
    const char *GetRefOrNull(const char *refname);

    // private (except for code in language.cc)
    lang_choice_c *AddChoice(const char *name);
};

extern language_c language;  // -ACB- 2004/06/27 Implemented

void DDF_ReadLangs(const std::string &data);

#endif /* __DDF_LANG_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
