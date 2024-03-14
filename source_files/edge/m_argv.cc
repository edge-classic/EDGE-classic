//------------------------------------------------------------------------
//  EDGE Arguments/Parameters Code
//------------------------------------------------------------------------
//
//  Copyright (C) 2023-2024 The EDGE Team
//  Copyright (C) 2021-2022 The OBSIDIAN Team
//  Copyright (C) 2006-2017 Andrew Apted
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
//------------------------------------------------------------------------

#include "m_argv.h"

#include <string.h>

#include "epi.h"
#include "epi_windows.h"
#include "filesystem.h"
#include "i_system.h"
#include "str_compare.h"
#include "str_util.h"
#ifdef _WIN32
#include <shellapi.h>
#endif

std::vector<std::string> program_argument_list;

// this one is here to avoid infinite recursion of param files.
struct AddedParameter
{
    std::string            name;
    struct AddedParameter *next;
};

static AddedParameter *added_parameters;

//
// ArgvInit
//
// Initialise argument program_argument_list. The strings (and array) are
// copied.
//
// NOTE: doesn't merge multiple uses of an option, hence
//       using ArgvFind() will only return the first usage.
//
#ifdef _WIN32
void ArgumentParse(const int argc, const char *const *argv)
{
    (void)argc;
    (void)argv;

    int       win_argc = 0;
    size_t    i;
    wchar_t **win_argv = CommandLineToArgvW(GetCommandLineW(), &win_argc);

    if (!win_argv)
        FatalError(
            "ArgumentParse: Could not retrieve command line arguments!\n");

    program_argument_list.reserve(win_argc);

    std::vector<std::string> argv_block;

    for (i = 0; i < win_argc; i++)
    {
        EPI_ASSERT(win_argv[i] != nullptr);
        argv_block.push_back(epi::WStringToUTF8(win_argv[i]));
    }

    LocalFree(win_argv);

    for (i = 0; i < argv_block.size(); i++)
    {
        // Just place argv[0] as is
        if (i == 0)
        {
            program_argument_list.emplace_back(argv_block[i]);
            continue;
        }

        if (argv_block[i][0] == '@')
        {  // add it as a response file
            ArgumentApplyResponseFile(&argv_block[i][1]);
            continue;
        }

        program_argument_list.emplace_back(argv_block[i]);
    }
}
#else
void ArgumentParse(const int argc, const char *const *argv)
{
    EPI_ASSERT(argc >= 0);
    program_argument_list.reserve(argc);

    for (int i = 0; i < argc; i++)
    {
        EPI_ASSERT(argv[i] != nullptr);

#ifdef __APPLE__
        // ignore MacOS X rubbish
        if (argv[i] == "-psn") continue;
#endif
        // Just place argv[0] as is
        if (i == 0)
        {
            program_argument_list.emplace_back(argv[i]);
            continue;
        }

        if (argv[i][0] == '@')
        {  // add it as a response file
            ArgumentApplyResponseFile(&argv[i][1]);
            continue;
        }

        program_argument_list.emplace_back(argv[i]);
    }
}
#endif

int ArgumentFind(std::string_view long_name, int *total_parameters)
{
    EPI_ASSERT(!long_name.empty());

    if (total_parameters) *total_parameters = 0;

    size_t p = 0;

    for (; p < program_argument_list.size(); ++p)
    {
        if (!ArgumentIsOption(p)) continue;

        if (epi::StringCaseCompareASCII(
                long_name, program_argument_list[p].substr(1)) == 0)
            break;
    }

    if (p == program_argument_list.size()) return -1;

    if (total_parameters)
    {
        size_t q = p + 1;

        while (q < program_argument_list.size() && !ArgumentIsOption(q)) ++q;

        *total_parameters = q - p - 1;
    }

    return p;
}

std::string ArgumentValue(std::string_view long_name, int *total_parameters)
{
    EPI_ASSERT(!long_name.empty());

    int pos = ArgumentFind(long_name, total_parameters);

    if (pos <= 0) return "";

    if (pos + 1 < int(program_argument_list.size()) &&
        !ArgumentIsOption(pos + 1))
        return program_argument_list[pos + 1];
    else
        return "";
}

// Sets boolean variable to true if parm (prefixed with `-') is
// present, sets it to false if parm prefixed with `-no' is present,
// otherwise leaves it unchanged.
//
void ArgumentCheckBooleanParameter(const std::string &parameter,
                                   bool *boolean_value, bool reverse)
{
    if (ArgumentFind(parameter) > 0)
    {
        *boolean_value = !reverse;
        return;
    }

    if (ArgumentFind(epi::StringFormat("no%s", parameter.c_str())) > 0)
    {
        *boolean_value = reverse;
        return;
    }
}

void ArgumentCheckBooleanConsoleVariable(const std::string &parameter,
                                         ConsoleVariable   *variable,
                                         bool               reverse)
{
    if (ArgumentFind(parameter) > 0)
    {
        *variable = (reverse ? 0 : 1);
        return;
    }

    if (ArgumentFind(epi::StringFormat("no%s", parameter.c_str())) > 0)
    {
        *variable = (reverse ? 1 : 0);
        return;
    }
}

static int ParseOneFilename(FILE *fp, char *buf)
{
    // -AJA- 2004/08/30: added this routine, in order to handle
    // filenames with spaces (which must be double-quoted).

    int ch;

    // skip whitespace
    do {
        ch = fgetc(fp);

        if (ch == EOF) return EOF;
    } while (epi::IsSpaceASCII(ch));

    bool quoting = false;

    for (;;)
    {
        if (ch == '"')
        {
            quoting = !quoting;
            ch      = fgetc(fp);
            continue;
        }

        if (ch == EOF || (epi::IsSpaceASCII(ch) && !quoting)) break;

        *buf++ = ch;

        ch = fgetc(fp);
    }

    *buf++ = 0;

    return 0;
}

//
// Adds a response file
//
void ArgumentApplyResponseFile(std::string_view name)
{
    char            buf[1024];
    FILE           *f;
    AddedParameter  this_parm;
    AddedParameter *p;

    // check if the file has already been added
    for (p = added_parameters; p; p = p->next)
    {
        if (epi::StringCompare(p->name, name) == 0) return;
    }

    // mark that this file has been added
    this_parm.name = name;
    p = this_parm.next = added_parameters;

    // add arguments from the given file
    f = epi::FileOpenRaw(name, epi::kFileAccessRead | epi::kFileAccessBinary);
    if (!f)
        FatalError("Couldn't open \"%s\" for reading!", this_parm.name.c_str());

    for (; EOF != ParseOneFilename(f, buf);)
        program_argument_list.push_back(strdup(buf));

    // unlink from list
    added_parameters = p;

    fclose(f);
}

void ArgumentDebugDump(void)
{
    LogPrint("Command-line Options:\n");

    int i = 0;

    while (i < int(program_argument_list.size()))
    {
        bool pair_it_up = false;

        if (i > 0 && i + 1 < int(program_argument_list.size()) &&
            !ArgumentIsOption(i + 1))
            pair_it_up = true;

        LogPrint("  %s %s\n", program_argument_list[i].c_str(),
                 pair_it_up ? program_argument_list[i + 1].c_str() : "");

        i += pair_it_up ? 2 : 1;
    }
}

bool ArgumentIsOption(const int index)
{
    return program_argument_list.at(index)[0] == '-';
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
