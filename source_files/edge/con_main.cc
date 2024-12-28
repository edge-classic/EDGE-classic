//----------------------------------------------------------------------------
//  EDGE Console Main
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

#include "con_main.h"

#include <stdarg.h>
#include <string.h>

#include "con_var.h"
#include "ddf_language.h"
#include "ddf_sfx.h"
#include "dm_state.h"
#include "e_input.h"
#include "epi_filesystem.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"
#include "g_game.h"
#include "i_system.h"
#include "m_menu.h"
#include "m_misc.h"
#include "s_sound.h"
#include "stb_sprintf.h"
#include "version.h"
#include "w_files.h"
#include "w_wad.h"

static constexpr uint8_t kMaximumConsoleArguments = 64;

static std::string readme_names[4] = {"readme.txt", "readme.1st", "read.me", "readme.md"};

std::string working_directory;

struct ConsoleCommand
{
    const char *name;

    int (*func)(char **argv, int argc);
};

// forward decl.
extern const ConsoleCommand builtin_commands[];

extern void M_ChangeLevelCheat(const char *string);
extern void I_ShowGamepads(void);

int ConsoleCommandExec(char **argv, int argc)
{
    if (argc != 2)
    {
        ConsolePrint("Usage: exec <filename>\n");
        return 1;
    }

    if (epi::IsPathAbsolute(argv[1]))
    {
        LogPrint("Absolute path %s not allowed!\n", argv[1]);
        return 1;
    }

    if (strstr(argv[1], "..") != NULL)
    {
        LogPrint("Path traversal with .. is not allowed!\n");
        return 1;
    }

    std::string path = epi::PathAppend(working_directory, argv[1]);

    FILE *script = epi::FileOpenRaw(path, epi::kFileAccessRead | epi::kFileAccessBinary);
    if (!script)
    {
        ConsolePrint("Unable to open file: %s\n", argv[1]);
        return 1;
    }

    char buffer[200];

    while (fgets(buffer, sizeof(buffer) - 1, script))
    {
        TryConsoleCommand(buffer);
    }

    fclose(script);
    return 0;
}

int ConsoleCommandType(char **argv, int argc)
{
    FILE *script;
    char  buffer[200];

    if (argc != 2)
    {
        ConsolePrint("Usage: %s <filename>\n", argv[0]);
        return 2;
    }

    if (epi::IsPathAbsolute(argv[1]))
    {
        LogPrint("Absolute path %s not allowed!\n", argv[1]);
        return 1;
    }

    if (strstr(argv[1], "..") != NULL)
    {
        LogPrint("Path traversal with .. is not allowed!\n");
        return 1;
    }

    std::string path = epi::PathAppend(working_directory, argv[1]);

    script = epi::FileOpenRaw(path, epi::kFileAccessRead);
    if (!script)
    {
        ConsolePrint("Unable to open \'%s\'!\n", argv[1]);
        return 3;
    }
    while (fgets(buffer, sizeof(buffer) - 1, script))
    {
        ConsolePrint("%s", buffer);
    }
    fclose(script);

    return 0;
}

int ConsoleCommandReadme(char **argv, int argc)
{
    EPI_UNUSED(argv);
    EPI_UNUSED(argc);
    epi::File *readme_file = nullptr;

    // Check well known readme filenames
    for (auto name : readme_names)
    {
        readme_file = OpenFileFromPack(name);
        if (readme_file)
            break;
    }

    if (!readme_file)
    {
        // Check for the existence of a .txt file whose name matches a WAD or
        // pack in the load order
        for (int i = data_files.size() - 1; i > 0; i--)
        {
            std::string readme_check = data_files[i]->name_;
            epi::ReplaceExtension(readme_check, ".txt");
            readme_file = OpenFileFromPack(readme_check);
            if (readme_file)
                break;
        }
    }

    // Check for WADINFO or README lumps
    if (!readme_file)
    {
        if (IsLumpInAnyWad("WADINFO"))
            readme_file = LoadLumpAsFile("WADINFO");
        else if (IsLumpInAnyWad("README"))
            readme_file = LoadLumpAsFile("README");
    }

    // Check for an EDGEGAME lump or file and print (if it has text; these
    // aren't required to)
    if (!readme_file)
    {
        // Datafile at index 1 should always be either the IWAD or standalone
        // EPK
        if (data_files[1]->wad_)
        {
            if (IsLumpInAnyWad("EDGEGAME"))
                readme_file = LoadLumpAsFile("EDGEGAME");
        }
        else
            readme_file = OpenFileFromPack("EDGEGAME.txt");
    }

    if (!readme_file)
    {
        ConsolePrint("No readme files found in current load order!\n");
        return 1;
    }
    else
    {
        std::string readme = readme_file->ReadText();
        delete readme_file;
        std::vector<std::string> readme_strings = epi::SeparatedStringVector(readme, '\n');
        for (std::string &line : readme_strings)
        {
            ConsolePrint("%s\n", line.c_str());
        }
    }

    return 0;
}

int ConsoleCommandChangeDir(char **argv, int argc)
{
    if (argc == 1 || argc > 2)
    {
        LogPrint("Usage: %s <home or game>\n", argv[0]);
        return 1;
    }

    if (home_directory == game_directory)
    {
        LogPrint("Home and game directory are both %s!\nRemaining in current directory.\n",
                 epi::SanitizePath(working_directory).c_str());
        return 1;
    }
    else if (epi::StringCaseCompareASCII(argv[1], "game") == 0)
    {
        working_directory = game_directory;
        LogPrint("Switched to game directory %s\n", epi::SanitizePath(working_directory).c_str());
    }
    else if (epi::StringCaseCompareASCII(argv[1], "home") == 0)
    {
        working_directory = home_directory;
        LogPrint("Switched to home directory %s\n", epi::SanitizePath(working_directory).c_str());
    }
    else
    {
        LogPrint("Unknown cd target %s (must be \"home\" or \"game\")\n", argv[1]);
        return 1;
    }

    return 0;
}

int ConsoleCommandPrintWorkingDir(char **argv, int argc)
{
    if (argc > 1)
    {
        LogPrint("Usage: %s\n", argv[0]);
        return 1;
    }

    if (home_directory != game_directory)
    {
        if (working_directory == game_directory)
            LogPrint("Using game directory %s\n", epi::SanitizePath(working_directory).c_str());
        else
            LogPrint("Using home directory %s\n", epi::SanitizePath(working_directory).c_str());
    }
    else
        LogPrint("Using directory %s\n", epi::SanitizePath(working_directory).c_str());

    return 0;
}

int ConsoleCommandDir(char **argv, int argc)
{
    std::string path = ".";
    std::string mask = "*.*";

    if (argc >= 2)
    {
        // Assume a leading * is the beginning of a mask for the current dir
        if (argv[1][0] == '*')
            mask = argv[1];
        else
            path = argv[1];
    }

    if (argc >= 3)
        mask = argv[2];

    if (epi::IsPathAbsolute(path))
    {
        LogPrint("Absolute path %s not allowed!\n", path.c_str());
        return 1;
    }

    if (path.find("..") != std::string::npos)
    {
        LogPrint("Path traversal with .. is not allowed!\n");
        return 1;
    }

    path = epi::PathAppend(working_directory, path);

    std::vector<epi::DirectoryEntry> fsd;

    if (!ReadDirectory(fsd, path, mask.c_str()))
    {
        LogPrint("Failed to read dir: %s\n", path.c_str());
        return 1;
    }

    if (fsd.empty())
    {
        LogPrint("No files found in provided path %s\n", path.c_str());
        return 0;
    }

    LogPrint("Directory contents for %s matching %s\n", epi::SanitizePath(epi::GetDirectory(path)).c_str(),
             mask.c_str());

    for (size_t i = 0; i < fsd.size(); i++)
    {
        LogPrint("%4d:  %-4s  \"%s\"\n", (int)i + 1, fsd[i].is_dir ? "dir  " : "file",
                 epi::GetFilename(fsd[i].name).c_str());
    }

    return 0;
}

int ConsoleCommandScreenShot(char **argv, int argc)
{
    EPI_UNUSED(argv);
    EPI_UNUSED(argc);
    DeferredScreenShot();

    return 0;
}

int ConsoleCommandQuitEDGE(char **argv, int argc)
{
#ifdef EDGE_WEB
    ConsolePrint("%s\n", language["QuitWhenWebPlayer"]);
    return 1;
#else
    if (argc >= 2 && epi::StringCaseCompareASCII(argv[1], "now") == 0)
        // this never returns
        ImmediateQuit();
    else
        QuitEdge(0);

    return 0;
#endif
}

int ConsoleCommandPlaySound(char **argv, int argc)
{
    SoundEffect *sfx;

    if (argc != 2)
    {
        ConsolePrint("Usage: playsound <name>\n");
        return 1;
    }

    sfx = sfxdefs.GetEffect(argv[1], false);
    if (!sfx)
    {
        ConsolePrint("No such sound: %s\n", argv[1]);
    }
    else
    {
        StartSoundEffect(sfx, kCategoryUi);
    }

    return 0;
}

int ConsoleCommandResetVars(char **argv, int argc)
{
    EPI_UNUSED(argv);
    EPI_UNUSED(argc);
    ResetAllConsoleVariables();
    ResetDefaults(0);
    return 0;
}

int ConsoleCommandShowFiles(char **argv, int argc)
{
    EPI_UNUSED(argv);
    EPI_UNUSED(argc);
    ShowLoadedFiles();
    return 0;
}

int ConsoleCommandBrowse(char **argv, int argc)
{
    EPI_UNUSED(argv);
    EPI_UNUSED(argc);
#ifdef EDGE_WEB
    ConsolePrint("%s\n", language["NoBrowseFromWeb"]);
    return 1;
#else
    epi::OpenDirectory(working_directory);
    return 0;
#endif
}

int ConsoleCommandShowVars(char **argv, int argc)
{
    bool show_default = false;

    char *match = nullptr;

    if (argc >= 2 && epi::StringCaseCompareASCII(argv[1], "-l") == 0)
    {
        show_default = true;
        argv++;
        argc--;
    }

    if (argc >= 2)
        match = argv[1];

    LogPrint("Console Variables:\n");

    int total = PrintConsoleVariables(match, show_default);

    if (total == 0)
        LogPrint("Nothing matched.\n");

    return 0;
}

int ConsoleCommandShowCommands(char **argv, int argc)
{
    char *match = nullptr;

    if (argc >= 2)
        match = argv[1];

    LogPrint("Console Commands:\n");

    int total = 0;

    for (int i = 0; builtin_commands[i].name; i++)
    {
        if (match && *match)
            if (!strstr(builtin_commands[i].name, match))
                continue;

        LogPrint("  %-15s\n", builtin_commands[i].name);
        total++;
    }

    if (total == 0)
        LogPrint("Nothing matched.\n");

    return 0;
}

int ConsoleCommandShowMaps(char **argv, int argc)
{
    EPI_UNUSED(argv);
    EPI_UNUSED(argc);
    LogPrint("Warp Name           Description\n");

    for (size_t i = 0; i < mapdefs.size(); i++)
    {
        if (MapExists(mapdefs[i]) && mapdefs[i]->episode_)
            LogPrint("  %s                     %s\n", mapdefs[i]->name_.c_str(),
                     language[mapdefs[i]->description_.c_str()]);
    }

    return 0;
}

int ConsoleCommandShowGamepads(char **argv, int argc)
{
    EPI_UNUSED(argv);
    EPI_UNUSED(argc);

    I_ShowGamepads();
    return 0;
}

int ConsoleCommandHelp(char **argv, int argc)
{
    EPI_UNUSED(argv);
    EPI_UNUSED(argc);
    LogPrint("Welcome to the EDGE Console.\n");
    LogPrint("\n");
    LogPrint("Use the 'showcmds' command to list all commands.\n");
    LogPrint("The 'showvars' command will list all variables.\n");
    LogPrint("Both of these can take a keyword to match the names with.\n");
    LogPrint("\n");
    LogPrint("To show the value of a variable, just type its name.\n");
    LogPrint("To change it, follow the name with a space and the new value.\n");
    LogPrint("\n");
    LogPrint("Press ESC key to close the console.\n");
    LogPrint("The PGUP and PGDN keys scroll the console up and down.\n");
    LogPrint("The UP and DOWN arrow keys let you recall previous commands.\n");
    LogPrint("\n");
    LogPrint("Have a nice day!\n");

    return 0;
}

int ConsoleCommandVersion(char **argv, int argc)
{
    EPI_UNUSED(argv);
    EPI_UNUSED(argc);
    LogPrint("%s v%s\n", application_name.c_str(), edge_version.c_str());
    return 0;
}

int ConsoleCommandMap(char **argv, int argc)
{
    if (argc <= 1)
    {
        ConsolePrint("Usage: map <level>\n");
        return 0;
    }

    M_ChangeLevelCheat(argv[1]);
    return 0;
}

int ConsoleCommandEndoom(char **argv, int argc)
{
    EPI_UNUSED(argv);
    EPI_UNUSED(argc);
    ConsolePrintEndoom();
    return 0;
}

int ConsoleCommandClear(char **argv, int argc)
{
    EPI_UNUSED(argv);
    EPI_UNUSED(argc);
    ClearConsoleLines();
    return 0;
}

//----------------------------------------------------------------------------

// oh lordy....
static char *StrDup(const char *start, int len)
{
    char *buf = new char[len + 2];

    memcpy(buf, start, len);
    buf[len] = 0;

    return buf;
}

static int GetArgs(const char *line, char **argv, int max_argc)
{
    int argc = 0;

    for (;;)
    {
        while (epi::IsSpaceASCII(*(uint8_t *)line))
            line++;

        if (!*line)
            break;

        // silent truncation (bad?)
        if (argc >= max_argc)
            break;

        const char *start = line;

        if (*line == '"')
        {
            start++;
            line++;

            while (*line && *line != '"')
                line++;
        }
        else
        {
            while (*line && !epi::IsSpaceASCII(*(uint8_t *)line))
                line++;
        }

        // ignore empty strings at beginning of the line
        if (!(argc == 0 && start == line))
        {
            argv[argc++] = StrDup(start, line - start);
        }

        if (*line)
            line++;
    }

    return argc;
}

static void KillArgs(char **argv, int argc)
{
    for (int i = 0; i < argc; i++)
        delete[] argv[i];
}

//
// Current console commands:
//
const ConsoleCommand builtin_commands[] = {{"cat", ConsoleCommandType},
                                           {"cd", ConsoleCommandChangeDir},
                                           {"chdir", ConsoleCommandChangeDir},
                                           {"cls", ConsoleCommandClear},
                                           {"clear", ConsoleCommandClear},
                                           {"dir", ConsoleCommandDir},
                                           {"ls", ConsoleCommandDir},
                                           {"endoom", ConsoleCommandEndoom},
                                           {"endtext", ConsoleCommandEndoom},
                                           {"exec", ConsoleCommandExec},
                                           {"help", ConsoleCommandHelp},
                                           {"map", ConsoleCommandMap},
                                           {"warp", ConsoleCommandMap}, // compatibility
                                           {"playsound", ConsoleCommandPlaySound},
                                           {"readme", ConsoleCommandReadme},
                                           {"browse", ConsoleCommandBrowse},
                                           {"pwd", ConsoleCommandPrintWorkingDir},
                                           {"resetvars", ConsoleCommandResetVars},
                                           {"showfiles", ConsoleCommandShowFiles},
                                           {"showgamepads", ConsoleCommandShowGamepads},
                                           {"showcmds", ConsoleCommandShowCommands},
                                           {"showmaps", ConsoleCommandShowMaps},
                                           {"showvars", ConsoleCommandShowVars},
                                           {"screenshot", ConsoleCommandScreenShot},
                                           {"type", ConsoleCommandType},
                                           {"version", ConsoleCommandVersion},
                                           {"quit", ConsoleCommandQuitEDGE},
                                           {"exit", ConsoleCommandQuitEDGE},

                                           // end of list
                                           {nullptr, nullptr}};

static int FindCommand(const char *name)
{
    for (int i = 0; builtin_commands[i].name; i++)
    {
        if (epi::StringCaseCompareASCII(name, builtin_commands[i].name) == 0)
            return i;
    }

    return -1; // not found
}

void TryConsoleCommand(const char *cmd)
{
    char *argv[kMaximumConsoleArguments];
    int   argc = GetArgs(cmd, argv, kMaximumConsoleArguments);

    if (argc == 0)
        return;

    int index = FindCommand(argv[0]);
    if (index >= 0)
    {
        (*builtin_commands[index].func)(argv, argc);

        KillArgs(argv, argc);
        return;
    }

    ConsoleVariable *var = FindConsoleVariable(argv[0]);
    if (var != nullptr)
    {
        if (argc <= 1)
        {
            if (var->flags_ & kConsoleVariableFlagFilepath)
                LogPrint("%s \"%s\"\n", argv[0], epi::SanitizePath(var->s_).c_str());
            else
                LogPrint("%s \"%s\"\n", argv[0], var->c_str());
        }
        else if (argc - 1 >= 2) // Assume string with spaces; concat args into
                                // one string and try it
        {
            std::string concatter = argv[1];
            for (int i = 2; i < argc; i++)
            {
                // preserve spaces in original string
                concatter.append(" ").append(argv[i]);
            }
            if (var->flags_ & kConsoleVariableFlagFilepath)
                *var = epi::SanitizePath(concatter).c_str();
            else
                *var = concatter.c_str();
        }
        else if ((var->flags_ & kConsoleVariableFlagReadOnly) != 0)
            LogPrint("The cvar '%s' is read only.\n", var->name_);
        else
        {
            if (var->flags_ & kConsoleVariableFlagFilepath)
                *var = epi::SanitizePath(argv[1]).c_str();
            else
                *var = argv[1];
        }

        KillArgs(argv, argc);
        return;
    }

    LogPrint("Unknown console command: %s\n", argv[0]);

    KillArgs(argv, argc);
    return;
}

int MatchConsoleCommands(std::vector<const char *> &list, const char *pattern)
{
    list.clear();

    for (int i = 0; builtin_commands[i].name; i++)
    {
        if (!ConsoleMatchPattern(builtin_commands[i].name, pattern))
            continue;

        list.push_back(builtin_commands[i].name);
    }

    return (int)list.size();
}

//
// ConsolePlayerMessage
//
// -ACB- 1999/09/22 Console Player Message Only. Changed from
//                  #define to procedure because of compiler
//                  differences.
//
void ConsolePlayerMessage(int plyr, const char *message, ...)
{
    va_list argptr;
    char    buffer[256];

    if (console_player != plyr)
        return;

    va_start(argptr, message);
    stbsp_vsnprintf(buffer, sizeof(buffer), message, argptr);
    va_end(argptr);

    buffer[sizeof(buffer) - 1] = 0;

    ConsoleMessage("%s", buffer);
}

//
// PlayerConsoleMessageLDF
//
// -ACB- 1999/09/22 Console Player Message Only. Changed from
//                  #define to procedure because of compiler
//                  differences.
//
void PlayerConsoleMessageLDF(int plyr, const char *message, ...)
{
    va_list argptr;
    char    buffer[256];

    if (console_player != plyr)
        return;

    message = language[message];

    va_start(argptr, message);
    stbsp_vsnprintf(buffer, sizeof(buffer), message, argptr);
    va_end(argptr);

    buffer[sizeof(buffer) - 1] = 0;

    ConsoleMessage("%s", buffer);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
