//----------------------------------------------------------------------------
//  EDGE Console Variables
//----------------------------------------------------------------------------
// 
//  Copyright (c) 2007-2009  The EDGE Team.
// 
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
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

class cvar_c
{
public:
	// current value
	int d;
	float f;
	const char *str;

	// this is incremented each time a value is set.
	// (Note: whether the value is different is not checked)
	int modified;

private:
	enum bufsize_e { BUFSIZE = 24 };

	// local buffer used for integers, floats and small strings.
	// in use whenever (s == buffer).  Otherwise s is on the heap,
	// using strdup() and free().
	char buffer[BUFSIZE];

public:
	cvar_c() : d(0), f(0.0f), str(buffer), modified(0)
	{
		buffer[0] = '0';
		buffer[1] =  0;
	}

	cvar_c(int value);
	cvar_c(float value);
	cvar_c(const char *value);
	cvar_c(const cvar_c& other);

	~cvar_c();

	cvar_c& operator= (int value);
	cvar_c& operator= (float value);
	cvar_c& operator= (const char *value);
	cvar_c& operator= (std::string value);
	cvar_c& operator= (const cvar_c& other);

	// this checks and clears the 'modified' value
	inline bool CheckModified()
	{
		if (modified)
		{
			modified = 0; return true;
		}
		return false;
	}

private:
	inline bool Allocd()
	{
		return (str != NULL) && (str != buffer);
	}

	void FmtFloat(float value);
	void DoStr(const char *value);
};


typedef struct cvar_link_s
{
	// name of variable
	const char *name;

	// the console variable itself
	cvar_c *var;

	// flags (a combination of letters, "" for none)
	const char *flags;

	// default value
	const char *def_val;
}
cvar_link_t;


extern cvar_link_t all_cvars[];


void CON_ResetAllVars(bool initial = false);
// sets all cvars to their default value.
// When 'initial' is true, the modified counts are set to 0.

cvar_link_t * CON_FindVar(const char *name);
// look for a CVAR with the given name.

bool CON_MatchPattern(const char *name, const char *pat);

int CON_MatchAllVars(std::vector<const char *>& list,
                     const char *pattern, const char *flags = "");
// find all cvars which match the pattern, and copy pointers to
// them into the given list.  The flags parameter, if present,
// contains lowercase letters to match the CVAR with the flag,
// and/or uppercase letters to require the flag to be absent.
//
// Returns number of matches found.

bool CON_SetVar(const char *name, const char *flags, const char *value);
// sets the cvar with the given name (possibly an alias) with the
// given value.  The flags parameter can limit the search, and
// must begin with an 'A' to prevent matching aliases.
// Returns true if the cvar was found.

void CON_HandleProgramArgs(void);
// scan the program arguments and set matching cvars

#endif // __CON_VAR_H__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
