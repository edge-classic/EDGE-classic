//----------------------------------------------------------------------------
//  EDGE Console Variables
//----------------------------------------------------------------------------
//
//  Copyright (c) 2007-2024 The EDGE Team.
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

#define EDGE_DEFINE_CONSOLE_VARIABLE(name, value, flags) ConsoleVariable name(#name, value, flags);
#define EDGE_DEFINE_CONSOLE_VARIABLE_CLAMPED(name, value, flags, min, max)                                             \
    ConsoleVariable name(#name, value, flags, nullptr, min, max);
#define EDGE_DEFINE_CONSOLE_VARIABLE_WITH_CALLBACK(name, value, flags, cb)                                             \
    ConsoleVariable name(#name, value, flags, cb);
#define EDGE_DEFINE_CONSOLE_VARIABLE_WITH_CALLBACK_CLAMPED(name, value, flags, cb, min, max)                           \
    ConsoleVariable name(#name, value, flags, cb, min, max);

enum ConsoleVariableFlag
{
    kConsoleVariableFlagNone     = 0,
    kConsoleVariableFlagArchive  = (1 << 0), // saved in the config file
    kConsoleVariableFlagCheat    = (1 << 1), // disabled in multi-player games
    kConsoleVariableFlagNoReset  = (1 << 2), // do not reset to default
    kConsoleVariableFlagReadOnly = (1 << 3)  // read-only
};

class ConsoleVariable
{
  public:
    // current value
    int         d_;
    float       f_;
    std::string s_;
    typedef void (*ConsoleVariableCallback)(ConsoleVariable *self);

    // name of variable
    const char *name_;

    // default value
    const char *def_;

    // a combination of CVAR_XXX bits
    ConsoleVariableFlag flags_;

    float min_;
    float max_;

    // link in list
    ConsoleVariable *next_;

    ConsoleVariableCallback callback_;

  private:
    // this is incremented each time a value is set.
    // (Note: whether the value is different is not checked)
    int modified_;

  public:
    ConsoleVariable(const char *name, const char *def, ConsoleVariableFlag flags = kConsoleVariableFlagNone,
                    ConsoleVariableCallback cb = nullptr, float min = -256000.0f, float max = 256000.0f);

    ~ConsoleVariable();

    ConsoleVariable &operator=(int value);
    ConsoleVariable &operator=(float value);
    ConsoleVariable &operator=(std::string_view value);

    inline const char *c_str() const
    {
        return s_.c_str();
    }

    // this checks and clears the 'modified' value
    inline bool CheckModified()
    {
        if (modified_)
        {
            modified_ = 0;
            return true;
        }
        return false;
    }

  private:
    void FormatInteger(int value);
    void FormatFloat(float value);

    void ParseString();
};

// called by ConsoleInitConsole.
void SortConsoleVariables();

// sets all cvars to their default value.
void ResetAllConsoleVariables();

// look for a CVAR with the given name.
ConsoleVariable *FindConsoleVariable(const char *name);

bool ConsoleMatchPattern(const char *name, const char *pat);

// find all cvars which match the pattern, and copy pointers to
// them into the given list.  The flags parameter, if present,
// contains lowercase letters to match the CVAR with the flag,
// and/or uppercase letters to require the flag to be absent.
//
// Returns number of matches found.
int MatchConsoleVariables(std::vector<const char *> &list, const char *pattern);

// scan the program arguments and set matching cvars.
void HandleProgramArguments(void);

// display value of matching cvars.  match can be nullptr to match everything.
int PrintConsoleVariables(const char *match, bool show_default);

// write all cvars to the config file.
void WriteConsoleVariables(FILE *f);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
