//----------------------------------------------------------------------------
//  EDGE Arguments/Parameters Code
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2009  The EDGE Team.
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
//
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------

#include "i_defs.h"

#include "file.h"
#include "filesystem.h"

#include "m_argv.h"

static int myargc;
static const char **myargv = NULL;

// this one is here to avoid infinite recursion of param files.
typedef struct added_parm_s
{
	const char *name;
	struct added_parm_s *next;
}
added_parm_t;

static added_parm_t *added_parms;

//
// AddArgument
//
// Helper function that adds s to the argument list.
// Must use realloc, since this is done before Z_Init
//
static void AddArgument(const char *s, int pos)
{
	int i;

#ifdef DEVELOPERS
	L_WriteDebug("Adding parameter '%s'\n", s);
#endif

	SYS_ASSERT(pos >= 0 && pos <= myargc);

	if (s[0] == '@')
	{  // add it as a response file
		M_ApplyResponseFile(&s[1], pos);
		return;
	}

	myargc++;
	myargv = (const char**)realloc((void*)myargv, myargc * sizeof(char *));
	if (!myargv)
		I_Error("AddArgument: Out of memory!");

	// move any parameters forward
	for (i = myargc - 1; i > pos; i--)
		myargv[i] = myargv[i - 1];

	myargv[pos] = s;
}

//
// M_CheckNextParm
//
// Checks for the given parameter in the program's command line arguments.
// Starts at parameter prev+1, so this routine is useful if you want to
// check for several equal parameters (pass the last parameter number as
// prev).
//
// Returns the argument number (last+1 to argc-1)
// or 0 if not present
//
// -ES- 2000/01/01 Written.
//
int M_CheckNextParm(const char *check, int prev)
{
	int i;

	for (i = prev + 1; i < myargc; i++)
	{
		if (!epi::case_cmp(check, myargv[i]))
			return i;
	}

	return 0;
}

//
// M_CheckParm
//
// Checks for the given parameter
// in the program's command line arguments.
// Returns the argument number (1 to argc-1)
// or 0 if not present
int M_CheckParm(const char *check)
{
	return M_CheckNextParm(check, 0);
}

//
// M_GetParm
//
// Checks for the string in the command line, and returns the parameter after
// if it exists. Useful for all those parameters that look something like
// "-foo bar".
const char *M_GetParm(const char *check)
{
	int p;

	p = M_CheckParm(check);
	if (p && p + 1 < myargc)
		return myargv[p + 1];
	else
		return NULL;
}

static int ParseOneFilename(FILE *fp, char *buf)
{
	// -AJA- 2004/08/30: added this routine, in order to handle
	// filenames with spaces (which must be double-quoted).

	int ch;

	// skip whitespace
	do
	{
		ch = fgetc(fp);

		if (ch == EOF)
			return EOF;
	}
	while (isspace(ch));

	bool quoting = false;

	for (;;)
	{
		if (ch == '"')
		{
			quoting = ! quoting;
			ch = fgetc(fp);
			continue;
		}

		if (ch == EOF || (isspace(ch) && ! quoting))
			break;

		*buf++ = ch;

		ch = fgetc(fp);
	}

	*buf++ = 0;

	return 0;
}

//
// M_ApplyResponseFile
//
// Adds a response file
//
void M_ApplyResponseFile(const char *name, int position)
{
	char buf[1024];
	FILE *f;
	added_parm_t this_parm;
	added_parm_t *p;

	// check if the file has already been added
	for (p = added_parms; p; p = p->next)
	{
		if (!strcmp(p->name, name))
			return;
	}

	// mark that this file has been added
	this_parm.name = name;
	p = this_parm.next = added_parms;

	// add arguments from the given file
	f = fopen(name, "rb");
	if (!f)
		I_Error("Couldn't open \"%s\" for reading!", name);

	for (; EOF != ParseOneFilename(f, buf); position++)
		// we must use strdup: Z_Init might not have been called
		AddArgument(strdup(buf), position);

	// unlink from list
	added_parms = p;

	fclose(f);
}

//
// M_InitArguments
//
// Initialises the CheckParm system. Only called once, by main().
//
void M_InitArguments(int argc, const char **argv)
{
	int i;

	// argv[0] should always be placed before the response file.
	AddArgument(argv[0], 0);

	if (epi::FS_Access("edge.cmd", epi::file_c::ACCESS_READ))
		M_ApplyResponseFile("edge.cmd", 1);

	// scan through the arguments
	for (i = 1; i < argc; i++)
	{
		// add all new arguments to the end of the arg list.
		AddArgument(argv[i], myargc);
	}
}

//
// M_CheckBooleanParm
//
// Sets boolean variable to true if parm (prefixed with `-') is
// present, sets it to false if parm prefixed with `-no' is present,
// otherwise leaves it unchanged.
//
void M_CheckBooleanParm(const char *parm, bool *boolval, bool reverse)
{
	char parmbuf[100];

	sprintf(parmbuf, "-%s", parm);

	if (M_CheckParm(parmbuf) > 0)
	{
		*boolval = ! reverse;
		return;
	}

	sprintf(parmbuf, "-no%s", parm);

	if (M_CheckParm(parmbuf) > 0)
	{
		*boolval = reverse;
		return;
	}
}

void M_CheckBooleanCVar(const char *parm, cvar_c *var, bool reverse)
{
	char parmbuf[100];

	sprintf(parmbuf, "-%s", parm);

	if (M_CheckParm(parmbuf) > 0)
	{
		*var = (reverse ? 0 : 1);
		return;
	}

	sprintf(parmbuf, "-no%s", parm);

	if (M_CheckParm(parmbuf) > 0)
	{
		*var = (reverse ? 1 : 0);
		return;
	}
}

//
// M_GetArgument
//
// Returns the wished argument. argnum must be less than M_GetArgCount().
//
const char *M_GetArgument(int argnum)
{
	// this should never happen, so crash out if DEVELOPERS.
	if (argnum < 0 || argnum >= myargc)
#ifdef DEVELOPERS
		I_Error("M_GetArgument: Out of range (%d)", argnum);
#else
		return "";
#endif

	return myargv[argnum];
}

//
// M_GetArgCount
//
// Returns the number of program arguments.
int M_GetArgCount(void)
{
	return myargc;
}


void M_DebugDumpArgs(void)
{
	I_Printf("Command-line Options:\n");

	int i = 0;

	while (i < myargc)
	{
		bool pair_it_up = false;

		if (i > 0 && i+1 < myargc && myargv[i+1][0] != '-')
			pair_it_up = true;

		I_Printf("  %s %s\n", myargv[i], pair_it_up ? myargv[i+1] : "");

		i += pair_it_up ? 2 : 1;
	}
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
