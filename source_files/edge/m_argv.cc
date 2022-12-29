//------------------------------------------------------------------------
//  EDGE Arguments/Parameters Code
//------------------------------------------------------------------------
//
//  Copyright (C) 2022 The EDGE Team
//  Copyright (C) 2021-2022 The OBSIDIAN Team
//  Copyright (C) 2006-2017 Andrew Apted
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
//------------------------------------------------------------------------

#include "i_defs.h"
#include "m_argv.h"
#include "str_util.h"

#ifdef _WIN32
std::vector<std::u32string> argv::list;
#else
std::vector<std::string> argv::list;
#endif

// this one is here to avoid infinite recursion of param files.
typedef struct added_parm_s
{
	std::filesystem::path name;
	struct added_parm_s *next;
}
added_parm_t;

static added_parm_t *added_parms;

//
// ArgvInit
//
// Initialise argument list. The strings (and array) are copied.
//
// NOTE: doesn't merge multiple uses of an option, hence
//       using ArgvFind() will only return the first usage.
//
void argv::Init(const int argc, const char *const *argv) {
    list.reserve(argc);
    SYS_ASSERT(argv::list.size() >= 0);

    for (int i = 0; i < argc; i++) {
        SYS_ASSERT(argv[i] != nullptr);

#ifdef __APPLE__
        // ignore MacOS X rubbish
        if (cur == "-psn") {
            continue;
        }
#endif
        // Just place argv[0] as is
        if (i == 0)
        {
            list.emplace_back(UTFSTR(argv[i]));
            continue;
        }

        if (argv[i][0] == '@')
        {  // add it as a response file
            ApplyResponseFile(UTFSTR(&argv[i][1]));
            continue;
        }

        list.emplace_back(UTFSTR(argv[i]));
    }
}

#ifdef _WIN32
int argv::Find(std::u32string longName, int *numParams) {
    SYS_ASSERT(!longName.empty());

    if (numParams) {
        *numParams = 0;
    }

    size_t p = 0;

    for (; p < list.size(); ++p) {
        if (!IsOption(p)) {
            continue;
        }

        const std::u32string &str = list[p];

        if (epi::case_cmp(longName,
                std::u32string{&str[1], (str.size() - 1)}) == 0) {
            break;
        }
    }

    if (p == list.size()) {
        // NOT FOUND
        return -1;
    }

    if (numParams) {
        size_t q = p + 1;

        while (q < list.size() && !IsOption(q)) {
            ++q;
        }

        *numParams = q - p - 1;
    }

    return p;
}
#else
int argv::Find(std::string longName, int *numParams) {
    SYS_ASSERT(!longName.empty());

    if (numParams) {
        *numParams = 0;
    }

    size_t p = 0;

    for (; p < list.size(); ++p) {
        if (!IsOption(p)) {
            continue;
        }

        const std::string &str = list[p];

        if (epi::case_cmp(longName,
                std::string{&str[1], (str.size() - 1)}) == 0) {
            break;
        }
    }

    if (p == list.size()) {
        // NOT FOUND
        return -1;
    }

    if (numParams) {
        size_t q = p + 1;

        while (q < list.size() && !IsOption(q)) {
            ++q;
        }

        *numParams = q - p - 1;
    }

    return p;
}
#endif

#ifdef _WIN32
std::u32string argv::Value(std::u32string longName, int *numParams) {
#else
std::string argv::Value(std::string longName, int *numParams) {
#endif
    SYS_ASSERT(!longName.empty());

    int pos = Find(longName, numParams);

    if (pos <= 0)
        return UTFSTR("");

    if (pos + 1 < list.size() && !IsOption(pos+1))
        return list[pos+1];
    else
        return UTFSTR("");
}

// Sets boolean variable to true if parm (prefixed with `-') is
// present, sets it to false if parm prefixed with `-no' is present,
// otherwise leaves it unchanged.
//
#ifdef _WIN32
void argv::CheckBooleanParm(std::u32string parm, bool *boolval, bool reverse)
{
	if (Find(parm) > 0)
	{
		*boolval = ! reverse;
		return;
	}

	if (Find(UTFSTR("no").append(parm)) > 0)
	{
		*boolval = reverse;
		return;
	}
}

void argv::CheckBooleanCVar(std::u32string parm, cvar_c *var, bool reverse)
{
	if (Find(parm) > 0)
	{
		*var = (reverse ? 0 : 1);
		return;
	}
	
	if (Find(UTFSTR("no").append(parm)) > 0)
	{
		*var = (reverse ? 1 : 0);
		return;
	}
}
#else
void argv::CheckBooleanParm(std::string parm, bool *boolval, bool reverse)
{
	if (Find(parm) > 0)
	{
		*boolval = ! reverse;
		return;
	}

	if (Find(epi::STR_Format("no%s", parm.c_str())) > 0)
	{
		*boolval = reverse;
		return;
	}
}

void argv::CheckBooleanCVar(std::string parm, cvar_c *var, bool reverse)
{
	if (Find(parm) > 0)
	{
		*var = (reverse ? 0 : 1);
		return;
	}

	if (Find(epi::STR_Format("no%s", parm.c_str())) > 0)
	{
		*var = (reverse ? 1 : 0);
		return;
	}
}
#endif

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
// Adds a response file
//
void argv::ApplyResponseFile(std::filesystem::path name)
{
	char buf[1024];
	FILE *f;
	added_parm_t this_parm;
	added_parm_t *p;

	// check if the file has already been added
	for (p = added_parms; p; p = p->next)
	{
		if (p->name.compare(name) == 0)
			return;
	}

	// mark that this file has been added
	this_parm.name = name;
	p = this_parm.next = added_parms;

	// add arguments from the given file
	f = fopen(name.u8string().c_str(), "rb");
	if (!f)
		I_Error("Couldn't open \"%s\" for reading!", name.u8string().c_str());

	for (; EOF != ParseOneFilename(f, buf);)
    {
		// we must use strdup: Z_Init might not have been called
        list.push_back(UTFSTR(strdup(buf)));
    }

	// unlink from list
	added_parms = p;

	fclose(f);
}

void argv::DebugDumpArgs(void)
{
	I_Printf("Command-line Options:\n");

	int i = 0;

	while (i < list.size())
	{
		bool pair_it_up = false;

		if (i>0 && i+1 < list.size() && !IsOption(i+1))
			pair_it_up = true;

		I_Printf("  %s %s\n", epi::to_u8string(list[i]).c_str(), pair_it_up ? epi::to_u8string(list[i+1]).c_str() : "");

		i += pair_it_up ? 2 : 1;
	}
}

bool argv::IsOption(const int index) { return list.at(index)[0] == '-'; }

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
