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

#pragma once

#include <string>
#include <vector>

// ------------------------------------------------------------------
// ---------------------------LANGUAGES------------------------------
// ------------------------------------------------------------------

class LanguageChoice;

class Language
{
   public:
    Language();
    ~Language();

   private:
    std::vector<LanguageChoice *> choices_;

    // UMAPINFO strings
    LanguageChoice *umapinfo_choice_;

    // the current language choice
    int current_choice_;

   public:
    void Clear();

    int GetChoiceCount() { return (int)choices_.size(); }
    int GetChoice() { return current_choice_; }

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
    const char *GetReferenceOrNull(const char *refname);

    // private (except for code in language.cc)
    LanguageChoice *AddChoice(const char *name);
};

extern Language language;  // -ACB- 2004/06/27 Implemented

void DDF_ReadLangs(const std::string &data);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
