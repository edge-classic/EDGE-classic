//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (Language handling settings)
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
// Language handling Setup and Parser Code
//
// 1998/10/29 -KM- Allow cmd line selection of language.
//
// This is somewhat different to most DDF reading files. In order to read the
// language specific strings, it uses the format:
//
// <RefName>=<String>;
//
// as opposed to the normal entry, which should be:
//
// [<Refname>]
// STRING=<string>;
//
// also the file suffix is LDF (Language Def File), this is to avoid confusion
// with the oridnary DDF files. The default file is DEFAULT.LDF, which can be
// subbed by using -lang <NameOfLangFile>.
//

#include "language.h"

#include <unordered_map>

#include "local.h"
#include "str_util.h"

// Globals
Language language;  // -ACB- 2004/07/28 Languages instance

std::string DDF_SanitizeName(const std::string &s)
{
    std::string out;

    for (char ch : s)
    {
        if (ch == ' ' || ch == '_') continue;

        out.push_back((char)epi::ToUpperASCII((int)ch));
    }

    if (out.empty()) out.push_back('_');

    return out;
}

class LanguageChoice
{
   public:
    std::string                                  name;
    std::unordered_map<std::string, std::string> refs;

    LanguageChoice() : name(), refs() {}

    ~LanguageChoice() {}

    bool HasEntry(const std::string &refname) const
    {
        return refs.find(refname) != refs.end();
    }

    void AddEntry(const char *refname, const char *value)
    {
        // ensure ref name is uppercase, with no spaces
        std::string ref = DDF_SanitizeName(refname);

        refs[ref] = value;
    }
};

static LanguageChoice *dynamic_choice;

//
//  DDF PARSING ROUTINES
//
static void LanguageStartEntry(const char *name, bool extend)
{
    if (!name || !name[0])
    {
        DDF_WarnError("New language entry is missing a name!");
        name = "DEAD_LANGUAGE";
    }

    // Note: extension is the norm for LANGUAGES.LDF

    dynamic_choice = language.AddChoice(name);
}

static void LanguageParseField(const char *field, const char *contents,
                               int index, bool is_last)
{
#if (DEBUG_DDF)
    DDF_Debug("LANGUAGE_PARSE: %s = %s;\n", field, contents);
#endif

    if (!is_last)
    {
        DDF_WarnError("Unexpected comma `,' in LANGUAGE.LDF\n");
        return;
    }

    dynamic_choice->AddEntry(field, contents);
}

static void LanguageFinishEntry(void) { dynamic_choice = nullptr; }

static void LanguageClearAll(void)
{
    // safe to delete all language entries
    language.Clear();
}

void DDF_ReadLangs(const std::string &data)
{
    DDFReadInfo languages;

    languages.tag      = "LANGUAGES";
    languages.lumpname = "DDFLANG";

    languages.start_entry  = LanguageStartEntry;
    languages.parse_field  = LanguageParseField;
    languages.finish_entry = LanguageFinishEntry;
    languages.clear_all    = LanguageClearAll;

    DDF_MainReadFile(&languages, data);
}

void DDF_LanguageCleanUp(void)
{
    if (language.GetChoiceCount() == 0) FatalError("Missing languages !\n");
}

Language::Language() { current_choice_ = -1; }

Language::~Language() { Clear(); }

LanguageChoice *Language::AddChoice(const char *name)
{
    for (size_t i = 0; i < choices_.size(); i++)
    {
        if (DDF_CompareName(name, choices_[i]->name.c_str()) == 0)
            return choices_[i];
    }

    LanguageChoice *choice = new LanguageChoice;
    choice->name           = name;

    choices_.push_back(choice);
    return choice;
}

void Language::AddOrReplace(const char *ref, const char *value)
{
    if (umapinfo_choice_ == nullptr) { umapinfo_choice_ = new LanguageChoice; }
    umapinfo_choice_->AddEntry(ref, value);
}

const char *Language::GetReferenceOrNull(const char *refname)
{
    if (!refname) return nullptr;

    if (current_choice_ < 0 || current_choice_ >= (int)choices_.size())
        return nullptr;

    // ensure ref name is uppercase, with no spaces
    std::string ref = DDF_SanitizeName(refname);

    if (umapinfo_choice_ != nullptr)
    {
        if (umapinfo_choice_->HasEntry(ref))
        {
            const std::string &value = umapinfo_choice_->refs[ref];
            return value.c_str();
        }
    }

    if (choices_[current_choice_]->HasEntry(ref))
    {
        const std::string &value = choices_[current_choice_]->refs[ref];
        return value.c_str();
    }

    // fallback, look through other language definitions...

    for (size_t i = 0; i < choices_.size(); i++)
    {
        if (choices_[i]->HasEntry(ref))
        {
            const std::string &value = choices_[i]->refs[ref];
            return value.c_str();
        }
    }

    // not found!
    return nullptr;
}

void Language::Clear()
{
    for (size_t i = 0; i < choices_.size(); i++) delete choices_[i];

    choices_.clear();

    if (umapinfo_choice_ != nullptr)
    {
        delete umapinfo_choice_;
        umapinfo_choice_ = nullptr;
    }

    current_choice_ = -1;
}

// returns the current_choice_ name if idx is negative.
const char *Language::GetName(int idx)
{
    // fallback in case no languages are loaded
    if (choices_.empty()) return "ENGLISH";

    if (idx < 0) idx = current_choice_;

    // caller must ensure index is valid
    if (idx < 0 || idx >= (int)choices_.size())
        FatalError("Bug in code calling language_c::GetName\n");

    return choices_[idx]->name.c_str();
}

bool Language::Select(const char *name)
{
    for (size_t i = 0; i < choices_.size(); i++)
    {
        if (DDF_CompareName(name, choices_[i]->name.c_str()) == 0)
        {
            current_choice_ = i;
            return true;
        }
    }

    return false;
}

bool Language::Select(int idx)
{
    if (idx < 0 || idx >= (int)choices_.size()) return false;

    current_choice_ = idx;
    return true;
}

bool Language::IsValidRef(const char *refname)
{
    if (refname == nullptr) return false;

    if (current_choice_ < 0 || current_choice_ >= (int)choices_.size())
        return false;

    // ensure ref name is uppercase, with no spaces
    std::string ref = DDF_SanitizeName(refname);

    if (umapinfo_choice_ != nullptr)
        if (umapinfo_choice_->HasEntry(ref)) return true;

    return choices_[current_choice_]->HasEntry(ref);
}

// this returns the given refname if the lookup fails.
const char *Language::operator[](const char *refname)
{
    if (refname == nullptr) return "";

    if (current_choice_ < 0 || current_choice_ >= (int)choices_.size())
        return refname;

    // ensure ref name is uppercase, with no spaces
    std::string ref = DDF_SanitizeName(refname);

    if (umapinfo_choice_ != nullptr)
    {
        if (umapinfo_choice_->HasEntry(ref))
        {
            const std::string &value = umapinfo_choice_->refs[ref];
            return value.c_str();
        }
    }

    if (choices_[current_choice_]->HasEntry(ref))
    {
        const std::string &value = choices_[current_choice_]->refs[ref];
        return value.c_str();
    }

    // fallback, look through other language definitions...

    for (size_t i = 0; i < choices_.size(); i++)
    {
        if (choices_[i]->HasEntry(ref))
        {
            const std::string &value = choices_[i]->refs[ref];
            return value.c_str();
        }
    }

    // not found!
    return refname;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
