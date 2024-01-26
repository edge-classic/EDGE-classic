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

#ifndef __CON_VAR_H__
#define __CON_VAR_H__

#include <vector>
#include <string>
#include <limits>

#define DEF_CVAR(name, value, flags)                   cvar_c name(#name, value, flags);
#define DEF_CVAR_CLAMPED(name, value, flags, min, max) cvar_c name(#name, value, flags, min, max);
#define DEF_CVAR_CB(name, value, flags, cb)                                                                            \
    cvar_c name(#name, value, flags, FLT_MIN, FLT_MAX, cb);

class cvar_c
{
  public:
    // current value
    int         d;
    float       f;
    std::string s;
    typedef void (*cvar_callback)(cvar_c *self);

    // name of variable
    const char *name;

    // default value
    const char *def;

    // a combination of CVAR_XXX bits
    int flags;

    float min;
    float max;

    // link in list
    cvar_c *next;

    cvar_callback cvar_cb;

  private:
    // this is incremented each time a value is set.
    // (Note: whether the value is different is not checked)
    int modified;

  public:
    cvar_c(const char *_name, const char *_def, int _flags = 0, float _min = std::numeric_limits<float>::lowest(),
           float _max = std::numeric_limits<float>::max(), cvar_callback _cb = nullptr);

    ~cvar_c();

    cvar_c &operator=(int value);
    cvar_c &operator=(float value);
    cvar_c &operator=(const char *value);
    cvar_c &operator=(std::string value);

    inline const char *c_str() const
    {
        return s.c_str();
    }

    // this checks and clears the 'modified' value
    inline bool CheckModified()
    {
        if (modified)
        {
            modified = 0;
            return true;
        }
        return false;
    }

  private:
    void FmtInt(int value);
    void FmtFloat(float value);

    void ParseString();
};

enum
{
    CVAR_ARCHIVE  = (1 << 0), // saved in the config file
    CVAR_CHEAT    = (1 << 1), // disabled in multi-player games
    CVAR_NO_RESET = (1 << 2), // do not reset to default
    CVAR_ROM      = (1 << 3), // read-only
};

// called by CON_InitConsole.
void CON_SortVars();

// sets all cvars to their default value.
void CON_ResetAllVars();

// look for a CVAR with the given name.
cvar_c *CON_FindVar(const char *name);

bool CON_MatchPattern(const char *name, const char *pat);

// find all cvars which match the pattern, and copy pointers to
// them into the given list.  The flags parameter, if present,
// contains lowercase letters to match the CVAR with the flag,
// and/or uppercase letters to require the flag to be absent.
//
// Returns number of matches found.
int CON_MatchAllVars(std::vector<const char *> &list, const char *pattern);

// scan the program arguments and set matching cvars.
void CON_HandleProgramArgs(void);

// display value of matching cvars.  match can be NULL to match everything.
int CON_PrintVars(const char *match, bool show_default);

// write all cvars to the config file.
void CON_WriteVars(FILE *f);

#endif // __CON_VAR_H__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
