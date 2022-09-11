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

#include "i_defs.h"

#include "con_var.h"
#include "con_main.h"

#include "m_argv.h"


// Note: must use a plain array (and not std::vector) here
//       because constructors run very early (before main) and
//       no order is not guaranteed.

#define MAX_CVARS  2000

cvar_link_t all_cvars[MAX_CVARS];

int total_cvars = 0;


cvar_c::cvar_c(const char *name, const char *value, int flags) : s(value), modified(0)
{
	ParseString();

	// add this cvar link into the global array
	SYS_ASSERT(total_cvars < MAX_CVARS);

	cvar_link_t *link = &all_cvars[total_cvars];
	total_cvars += 1;

	link->name    = name;
	link->var     = this;
	link->flags   = flags;
	link->def_val = value;
}

cvar_c::~cvar_c()
{
	// nothing needed
}

void cvar_c::Reset(const char *value)
{
	s = value;
	ParseString();
	modified = 0;
}

cvar_c& cvar_c::operator= (int value)
{
	d = value;
	f = value;
	FmtInt(value);

	modified++;
	return *this;
}

cvar_c& cvar_c::operator= (float value)
{
	d = I_ROUND(value);
	f = value;
	FmtFloat(value);

	modified++;
	return *this;
}

cvar_c& cvar_c::operator= (const char *value)
{
	s = value;
	ParseString();

	modified++;
	return *this;
}

cvar_c& cvar_c::operator= (std::string value)
{
	s = value;
	ParseString();

	modified++;
	return *this;
}

// private method
void cvar_c::FmtInt(int value)
{
	char buffer[64];
	sprintf(buffer, "%d", value);
	s = buffer;
}

// private method
void cvar_c::FmtFloat(float value)
{
	char buffer[64];

	float ab = fabs(value);

	if (ab >= 1e10)  // handle huge numbers
		sprintf(buffer, "%1.5e", value);
	else if (ab >= 1e5)
		sprintf(buffer, "%1.1f", value);
	else if (ab >= 1e3)
		sprintf(buffer, "%1.3f", value);
	else if (ab >= 1.0)
		sprintf(buffer, "%1.5f", value);
	else
		sprintf(buffer, "%1.7f", value);

	s = buffer;
}

// private method
void cvar_c::ParseString()
{
	d = atoi(s.c_str());
	f = atof(s.c_str());
}


//----------------------------------------------------------------------------

void CON_ResetAllVars()
{
	for (int i = 0; all_cvars[i].var; i++)
	{
		*all_cvars[i].var = all_cvars[i].def_val;
	}
}


cvar_link_t * CON_FindVar(const char *name)
{
	for (int i = 0; all_cvars[i].var; i++)
	{
		if (stricmp(all_cvars[i].name, name) == 0)
			return &all_cvars[i];
	}

	return NULL;
}


bool CON_MatchPattern(const char *name, const char *pat)
{
	while (*name && *pat)
	{
		if (*name != *pat)
			return false;

		name++;
		pat++;
	}

	return (*pat == 0);
}


int CON_MatchAllVars(std::vector<const char *>& list, const char *pattern)
{
	list.clear();

	for (int i = 0; all_cvars[i].var; i++)
	{
		if (! CON_MatchPattern(all_cvars[i].name, pattern))
			continue;

		list.push_back(all_cvars[i].name);
	}

	return (int)list.size();
}


void CON_HandleProgramArgs(void)
{
	for (int p = 1; p < M_GetArgCount(); p++)
	{
		const char *s = M_GetArgument(p);

		if (s[0] != '-')
			continue;

		cvar_link_t *link = CON_FindVar(s+1);

		if (! link)
			continue;

		p++;

		if (p >= M_GetArgCount() || M_GetArgument(p)[0] == '-')
		{
			I_Error("Missing value for option: %s\n", s);
			continue;
		}

		*link->var = M_GetArgument(p);
	}
}


void CON_WriteVars(FILE *f)
{
	for (int k = 0 ; all_cvars[k].name ; k++)
	{
		if ((all_cvars[k].flags & CVAR_ARCHIVE) != 0)
		{
			cvar_c *var = all_cvars[k].var;
			fprintf(f, "/%s\t\"%s\"\n", all_cvars[k].name, var->c_str());
		}
	}
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
