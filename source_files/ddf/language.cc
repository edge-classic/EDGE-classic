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
// also the file suffix is LDF (Language Def File), this is to avoid confusion with
// the oridnary DDF files. The default file is DEFAULT.LDF, which can be subbed by
// using -lang <NameOfLangFile>.
//

#include "local.h"
#include "language.h"

#include <unordered_map>

// Globals
language_c language; // -ACB- 2004/07/28 Languages instance

std::string DDF_SanitizeName(const std::string &s)
{
    std::string out;

    for (size_t i = 0; i < s.size(); i++)
    {
        if (s[i] == ' ' || s[i] == '_')
            continue;

        out.push_back((char)epi::ToUpperASCII(s[i]));
    }

    if (out.empty())
        out.push_back('_');

    return out;
}

// Until unicode is truly implemented, restrict characters to extended ASCII
std::string DDF_SanitizePrintString(const std::string &s)
{
    return std::string(s);
    // This is always true and warns as std::string is char
    /*
    std::string out;

    for (size_t i = 0 ; i < s.size() ; i++)
    {
        if ((int)s[i] > 255 || (int)s[i] < 0)
            continue;

        out.push_back(s[i]);
    }

    return out;
    */
}

class lang_choice_c
{
  public:
    std::string                                  name;
    std::unordered_map<std::string, std::string> refs;

    lang_choice_c() : name(), refs()
    {
    }

    ~lang_choice_c()
    {
    }

    bool HasEntry(const std::string &refname) const
    {
        return refs.find(refname) != refs.end();
    }

    void AddEntry(const char *refname, const char *value)
    {
        // ensure ref name is uppercase, with no spaces
        std::string ref = DDF_SanitizeName(refname);

        std::string val = DDF_SanitizePrintString(value);

        refs[ref] = val;
    }
};

static lang_choice_c *dynamic_choice;

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

static void LanguageParseField(const char *field, const char *contents, int index, bool is_last)
{
#if (DEBUG_DDF)
    I_Debugf("LANGUAGE_PARSE: %s = %s;\n", field, contents);
#endif

    if (!is_last)
    {
        DDF_WarnError("Unexpected comma `,' in LANGUAGE.LDF\n");
        return;
    }

    dynamic_choice->AddEntry(field, contents);
}

static void LanguageFinishEntry(void)
{
    dynamic_choice = NULL;
}

static void LanguageClearAll(void)
{
    // safe to delete all language entries
    language.Clear();
}

void DDF_ReadLangs(const std::string &data)
{
    readinfo_t languages;

    languages.tag      = "LANGUAGES";
    languages.lumpname = "DDFLANG";

    languages.start_entry  = LanguageStartEntry;
    languages.parse_field  = LanguageParseField;
    languages.finish_entry = LanguageFinishEntry;
    languages.clear_all    = LanguageClearAll;

    DDF_MainReadFile(&languages, data);
}

void DDF_LanguageInit(void)
{
    // nothing needed
}

void DDF_LanguageCleanUp(void)
{
    if (language.GetChoiceCount() == 0)
        I_Error("Missing languages !\n");
}

language_c::language_c()
{
    current = -1;
}

language_c::~language_c()
{
    Clear();
}

lang_choice_c *language_c::AddChoice(const char *name)
{
    for (size_t i = 0; i < choices.size(); i++)
    {
        if (DDF_CompareName(name, choices[i]->name.c_str()) == 0)
            return choices[i];
    }

    lang_choice_c *choice = new lang_choice_c;
    choice->name          = name;

    choices.push_back(choice);
    return choice;
}

void language_c::AddOrReplace(const char *ref, const char *value)
{
    if (umap == NULL)
    {
        umap = new lang_choice_c;
    }
    umap->AddEntry(ref, value);
}

const char *language_c::GetRefOrNull(const char *refname)
{
    if (!refname)
        return nullptr;

    if (current < 0 || current >= (int)choices.size())
        return nullptr;

    // ensure ref name is uppercase, with no spaces
    std::string ref = DDF_SanitizeName(refname);

    if (umap != NULL)
    {
        if (umap->HasEntry(ref))
        {
            const std::string &value = umap->refs[ref];
            return value.c_str();
        }
    }

    if (choices[current]->HasEntry(ref))
    {
        const std::string &value = choices[current]->refs[ref];
        return value.c_str();
    }

    // fallback, look through other language definitions...

    for (size_t i = 0; i < choices.size(); i++)
    {
        if (choices[i]->HasEntry(ref))
        {
            const std::string &value = choices[i]->refs[ref];
            return value.c_str();
        }
    }

    // not found!
    return nullptr;
}

void language_c::Clear()
{
    for (size_t i = 0; i < choices.size(); i++)
        delete choices[i];

    choices.clear();

    if (umap != NULL)
    {
        delete umap;
        umap = NULL;
    }

    current = -1;
}

// returns the current name if idx is negative.
const char *language_c::GetName(int idx)
{
    // fallback in case no languages are loaded
    if (choices.empty())
        return "ENGLISH";

    if (idx < 0)
        idx = current;

    // caller must ensure index is valid
    if (idx < 0 || idx >= (int)choices.size())
        I_Error("Bug in code calling language_c::GetName\n");

    return choices[idx]->name.c_str();
}

bool language_c::Select(const char *name)
{
    for (size_t i = 0; i < choices.size(); i++)
    {
        if (DDF_CompareName(name, choices[i]->name.c_str()) == 0)
        {
            current = i;
            return true;
        }
    }

    return false;
}

bool language_c::Select(int idx)
{
    if (idx < 0 || idx >= (int)choices.size())
        return false;

    current = idx;
    return true;
}

bool language_c::IsValidRef(const char *refname)
{
    if (refname == NULL)
        return false;

    if (current < 0 || current >= (int)choices.size())
        return false;

    // ensure ref name is uppercase, with no spaces
    std::string ref = DDF_SanitizeName(refname);

    if (umap != NULL)
        if (umap->HasEntry(ref))
            return true;

    return choices[current]->HasEntry(ref);
}

// this returns the given refname if the lookup fails.
const char *language_c::operator[](const char *refname)
{
    if (refname == NULL)
        return "";

    if (current < 0 || current >= (int)choices.size())
        return refname;

    // ensure ref name is uppercase, with no spaces
    std::string ref = DDF_SanitizeName(refname);

    if (umap != NULL)
    {
        if (umap->HasEntry(ref))
        {
            const std::string &value = umap->refs[ref];
            return value.c_str();
        }
    }

    if (choices[current]->HasEntry(ref))
    {
        const std::string &value = choices[current]->refs[ref];
        return value.c_str();
    }

    // fallback, look through other language definitions...

    for (size_t i = 0; i < choices.size(); i++)
    {
        if (choices[i]->HasEntry(ref))
        {
            const std::string &value = choices[i]->refs[ref];
            return value.c_str();
        }
    }

    // not found!
    return refname;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
