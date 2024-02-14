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

#include <string.h>

#include "i_defs.h"

#include "filesystem.h"
#include "math_crc.h"
#include "str_util.h"

#include "language.h"
#include "sfx.h"

#include "con_main.h"
#include "con_var.h"
#include "dm_state.h"
#include "e_input.h"
#include "g_game.h"
#include "m_menu.h"
#include "m_misc.h"
#include "s_sound.h"
#include "w_files.h"
#include "w_wad.h"
#include "version.h"
#include "filesystem.h"
#include "str_compare.h"
#include <stdarg.h>
#define MAX_CON_ARGS 64

static std::string readme_names[4] = {"readme.txt", "readme.1st", "read.me", "readme.md"};

typedef struct
{
    const char *name;

    int (*func)(char **argv, int argc);
} con_cmd_t;

// forward decl.
extern const con_cmd_t builtin_commands[];

extern void M_ChangeLevelCheat(const char *string);
extern void I_ShowGamepads(void);

int CMD_Exec(char **argv, int argc)
{
    if (argc != 2)
    {
        CON_Printf("Usage: exec <script.cfg>\n");
        return 1;
    }

    FILE *script = epi::FileOpenRaw(argv[1], epi::kFileAccessRead|epi::kFileAccessBinary);
    if (!script)
    {
        CON_Printf("Unable to open file: %s\n", argv[1]);
        return 1;
    }

    char buffer[200];

    while (fgets(buffer, sizeof(buffer) - 1, script))
    {
        CON_TryCommand(buffer);
    }

    fclose(script);
    return 0;
}

int CMD_Type(char **argv, int argc)
{
    FILE *script;
    char  buffer[200];

    if (argc != 2)
    {
        CON_Printf("Usage: %s <filename.txt>\n", argv[0]);
        return 2;
    }

    script = epi::FileOpenRaw(argv[1], epi::kFileAccessRead);
    if (!script)
    {
        CON_Printf("Unable to open \'%s\'!\n", argv[1]);
        return 3;
    }
    while (fgets(buffer, sizeof(buffer) - 1, script))
    {
        CON_Printf("%s", buffer);
    }
    fclose(script);

    return 0;
}

int CMD_Readme(char **argv, int argc)
{
    epi::File *readme_file = nullptr;

    // Check well known readme filenames
    for (auto name : readme_names)
    {
        readme_file = W_OpenPackFile(name);
        if (readme_file)
            break;
    }

    if (!readme_file)
    {
        // Check for the existence of a .txt file whose name matches a WAD or pack
        // in the load order
        for (int i = data_files.size() - 1; i > 0; i--)
        {
            std::string readme_check = data_files[i]->name;
            epi::ReplaceExtension(readme_check, ".txt");
            readme_file = W_OpenPackFile(readme_check);
            if (readme_file)
                break;
        }
    }

    // Check for WADINFO or README lumps
    if (!readme_file)
    {
        if (W_IsLumpInAnyWad("WADINFO"))
            readme_file = W_OpenLump("WADINFO");
        else if (W_IsLumpInAnyWad("README"))
            readme_file = W_OpenLump("README");
    }

    // Check for an EDGEGAME lump or file and print (if it has text; these aren't required to)
    if (!readme_file)
    {
        // Datafile at index 1 should always be either the IWAD or standalone EPK
        if (data_files[1]->wad)
        {
            if (W_IsLumpInAnyWad("EDGEGAME"))
                readme_file = W_OpenLump("EDGEGAME");
        }
        else
            readme_file = W_OpenPackFile("EDGEGAME.txt");
    }

    if (!readme_file)
    {
        CON_Printf("No readme files found in current load order!\n");
        return 1;
    }
    else
    {
        std::string readme = readme_file->ReadText();
        delete readme_file;
        std::vector<std::string> readme_strings = epi::SeparatedStringVector(readme, '\n');
        for (std::string &line : readme_strings)
        {
            CON_Printf("%s\n", line.c_str());
        }
    }

    return 0;
}

int CMD_Dir(char **argv, int argc)
{
    std::string path = ".";
    std::string           mask = "*.*";

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

    std::vector<epi::DirectoryEntry> fsd;

    if (!ReadDirectory(fsd, path, mask.c_str()))
    {
        I_Printf("Failed to read dir: %s\n", path.c_str());
        return 1;
    }

    if (fsd.empty())
    {
        I_Printf("No files found in provided path %s\n", path.c_str());
        return 0;
    }

    I_Printf("Directory contents for %s matching %s\n", epi::GetDirectory(fsd[0].name).c_str(), mask.c_str());

    for (size_t i = 0; i < fsd.size(); i++)
    {
        I_Printf("%4d: %10d  %s  \"%s\"\n", (int)i + 1, (int)fsd[i].size, fsd[i].is_dir ? "DIR" : "   ",
                 epi::GetFilename(fsd[i].name).c_str());
    }

    return 0;
}

int CMD_ArgList(char **argv, int argc)
{
    I_Printf("Arguments:\n");

    for (int i = 0; i < argc; i++)
        I_Printf(" %2d len:%d text:\"%s\"\n", i, (int)strlen(argv[i]), argv[i]);

    return 0;
}

int CMD_ScreenShot(char **argv, int argc)
{
    G_DeferredScreenShot();

    return 0;
}

int CMD_QuitEDGE(char **argv, int argc)
{
    if (argc >= 2 && epi::StringCaseCompareASCII(argv[1], "now") == 0)
        // this never returns
        M_ImmediateQuit();
    else
        M_QuitEDGE(0);

    return 0;
}

int CMD_Crc(char **argv, int argc)
{
    if (argc < 2)
    {
        CON_Printf("Usage: crc <lump>\n");
        return 1;
    }

    for (int i = 1; i < argc; i++)
    {
        int lump = W_CheckNumForName(argv[i]);

        if (lump == -1)
        {
            CON_Printf("No such lump: %s\n", argv[i]);
        }
        else
        {
            int   length;
            uint8_t *data = (uint8_t *)W_LoadLump(lump, &length);

            epi::CRC32 result;

            result.Reset();
            result.AddBlock(data, length);

            delete[] data;

            CON_Printf("  %s  %d bytes  crc = %08x\n", argv[i], length, result.GetCRC());
        }
    }

    return 0;
}

int CMD_PlaySound(char **argv, int argc)
{
    sfx_t *sfx;

    if (argc != 2)
    {
        CON_Printf("Usage: playsound <name>\n");
        return 1;
    }

    sfx = sfxdefs.GetEffect(argv[1], false);
    if (!sfx)
    {
        CON_Printf("No such sound: %s\n", argv[1]);
    }
    else
    {
        S_StartFX(sfx, SNCAT_UI);
    }

    return 0;
}

int CMD_ResetVars(char **argv, int argc)
{
    CON_ResetAllVars();
    M_ResetDefaults(0);
    return 0;
}

int CMD_ShowFiles(char **argv, int argc)
{
    W_ShowFiles();
    return 0;
}

int CMD_OpenHome(char **argv, int argc)
{
    epi::OpenDirectory(home_dir);
    return 0;
}

int CMD_ShowLumps(char **argv, int argc)
{
    int for_file = -1; // all files

    char *match = nullptr;

    if (argc >= 2 && epi::IsDigitASCII(argv[1][0]))
        for_file = atoi(argv[1]);

    if (argc >= 3)
    {
        match = argv[2];
        for (size_t i = 0; i < strlen(match); i++)
        {
            match[i] = epi::ToUpperASCII(match[i]);
        }
    }

    W_ShowLumps(for_file, match);
    return 0;
}

int CMD_ShowVars(char **argv, int argc)
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

    I_Printf("Console Variables:\n");

    int total = CON_PrintVars(match, show_default);

    if (total == 0)
        I_Printf("Nothing matched.\n");

    return 0;
}

int CMD_ShowCmds(char **argv, int argc)
{
    char *match = nullptr;

    if (argc >= 2)
        match = argv[1];

    I_Printf("Console Commands:\n");

    int total = 0;

    for (int i = 0; builtin_commands[i].name; i++)
    {
        if (match && *match)
            if (!strstr(builtin_commands[i].name, match))
                continue;

        I_Printf("  %-15s\n", builtin_commands[i].name);
        total++;
    }

    if (total == 0)
        I_Printf("Nothing matched.\n");

    return 0;
}

int CMD_ShowMaps(char **argv, int argc)
{

    I_Printf("Warp Name           Description\n");

    for (int i = 0; i < mapdefs.size(); i++)
    {
        if (G_MapExists(mapdefs[i]) && mapdefs[i]->episode_)
            I_Printf("  %s           %s\n", mapdefs[i]->name_.c_str(), language[mapdefs[i]->description_.c_str()]);
    }

    return 0;
}

int CMD_ShowKeys(char **argv, int argc)
{
#if 0 // TODO
	char *match = nullptr;

	if (argc >= 2)
		match = argv[1];

	I_Printf("Key Bindings:\n");

	int total = 0;

	for (int i = 0; all_binds[i].name; i++)
	{
		if (match && *match)
			if (! strstr(all_binds[i].name, match))
				continue;

		std::string keylist = all_binds[i].bind->FormatKeyList();

		I_Printf("  %-15s %s\n", all_binds[i].name, keylist.c_str());
		total++;
	}

	if (total == 0)
		I_Printf("Nothing matched.\n");
#endif
    return 0;
}

int CMD_ShowGamepads(char **argv, int argc)
{
    (void)argv;
    (void)argc;

    I_ShowGamepads();
    return 0;
}

int CMD_Help(char **argv, int argc)
{
    I_Printf("Welcome to the EDGE Console.\n");
    I_Printf("\n");
    I_Printf("Use the 'showcmds' command to list all commands.\n");
    I_Printf("The 'showvars' command will list all variables.\n");
    I_Printf("Both of these can take a keyword to match the names with.\n");
    I_Printf("\n");
    I_Printf("To show the value of a variable, just type its name.\n");
    I_Printf("To change it, follow the name with a space and the new value.\n");
    I_Printf("\n");
    I_Printf("Press ESC key to close the console.\n");
    I_Printf("The PGUP and PGDN keys scroll the console up and down.\n");
    I_Printf("The UP and DOWN arrow keys let you recall previous commands.\n");
    I_Printf("\n");
    I_Printf("Have a nice day!\n");

    return 0;
}

int CMD_Version(char **argv, int argc)
{
    I_Printf("%s v%s\n", appname.c_str(), edgeversion.c_str());
    return 0;
}

int CMD_Map(char **argv, int argc)
{
    if (argc <= 1)
    {
        CON_Printf("Usage: map <level>\n");
        return 0;
    }

    M_ChangeLevelCheat(argv[1]);
    return 0;
}

int CMD_Endoom(char **argv, int argc)
{
    CON_PrintEndoom();
    return 0;
}

int CMD_Clear(char **argv, int argc)
{
    CON_ClearLines();
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
const con_cmd_t builtin_commands[] = {{"args", CMD_ArgList},
                                      {"cat", CMD_Type},
                                      {"cls", CMD_Clear},
                                      {"clear", CMD_Clear},
                                      {"crc", CMD_Crc},
                                      {"dir", CMD_Dir},
                                      {"ls", CMD_Dir},
                                      {"endoom", CMD_Endoom},
                                      {"exec", CMD_Exec},
                                      {"help", CMD_Help},
                                      {"map", CMD_Map},
                                      {"warp", CMD_Map}, // compatibility
                                      {"playsound", CMD_PlaySound},
                                      {"readme", CMD_Readme},
                                      {"openhome", CMD_OpenHome},
                                      {"resetvars", CMD_ResetVars},
                                      {"showfiles", CMD_ShowFiles},
                                      {"showgamepads", CMD_ShowGamepads},
                                      {"showlumps", CMD_ShowLumps},
                                      {"showcmds", CMD_ShowCmds},
                                      {"showmaps", CMD_ShowMaps},
                                      {"showvars", CMD_ShowVars},
                                      {"screenshot", CMD_ScreenShot},
                                      {"type", CMD_Type},
                                      {"version", CMD_Version},
                                      {"quit", CMD_QuitEDGE},
                                      {"exit", CMD_QuitEDGE},

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

#if 0
static void ProcessBind(key_link_t *link, char **argv, int argc)
{
	for (int i = 1; i < argc; i++)
	{
		if (epi::StringCaseCompareASCII(argv[i], "-c") == 0)
		{
			link->bind->Clear();
			continue;
		}

		int keyd = E_KeyFromName(argv[i]);
		if (keyd == 0)
		{
			CON_Printf("Invalid key name: %s\n", argv[i]);
			continue;
		}

		link->bind->Toggle(keyd);
	}
}
#endif

void CON_TryCommand(const char *cmd)
{
    char *argv[MAX_CON_ARGS];
    int   argc = GetArgs(cmd, argv, MAX_CON_ARGS);

    if (argc == 0)
        return;

    int index = FindCommand(argv[0]);
    if (index >= 0)
    {
        (*builtin_commands[index].func)(argv, argc);

        KillArgs(argv, argc);
        return;
    }

    cvar_c *var = CON_FindVar(argv[0]);
    if (var != nullptr)
    {
        if (argc <= 1)
        {
            if (var->flags & CVAR_PATH)
                I_Printf("%s \"%s\"\n", argv[0], epi::SanitizePath(var->s).c_str());
            else
                I_Printf("%s \"%s\"\n", argv[0], var->c_str());
        }
        else if (argc - 1 >= 2) // Assume string with spaces; concat args into one string and try it
        {
            std::string concatter = argv[1];
            for (int i = 2; i < argc; i++)
            {
                // preserve spaces in original string
                concatter.append(" ").append(argv[i]);
            }
            if (var->flags & CVAR_PATH)
                *var = epi::SanitizePath(concatter).c_str();
            else
                *var = concatter.c_str();
        }
        else if ((var->flags & CVAR_ROM) != 0)
            I_Printf("The cvar '%s' is read only.\n", var->name);
        else
        {
            if (var->flags & CVAR_PATH)
                *var = epi::SanitizePath(argv[1]).c_str();
            else
                *var = argv[1];
        }

        KillArgs(argv, argc);
        return;
    }

#if 0
	// hmmm I like it kinky...
	key_link_t *kink = E_FindKeyBinding(argv[0]);
	if (kink)
	{
		if (argc <= 1)
		{
			std::string keylist = kink->bind->FormatKeyList();

			I_Printf("%s %s\n", argv[0], keylist.c_str());
		}
		else
		{
			ProcessBind(kink, argv, argc);
		}

		KillArgs(argv, argc);
		return;
	}
#endif

    I_Printf("Unknown console command: %s\n", argv[0]);

    KillArgs(argv, argc);
    return;
}

int CON_MatchAllCmds(std::vector<const char *> &list, const char *pattern)
{
    list.clear();

    for (int i = 0; builtin_commands[i].name; i++)
    {
        if (!CON_MatchPattern(builtin_commands[i].name, pattern))
            continue;

        list.push_back(builtin_commands[i].name);
    }

    return (int)list.size();
}

//
// CON_PlayerMessage
//
// -ACB- 1999/09/22 Console Player Message Only. Changed from
//                  #define to procedure because of compiler
//                  differences.
//
void CON_PlayerMessage(int plyr, const char *message, ...)
{
    va_list argptr;
    char    buffer[256];

    if (consoleplayer != plyr)
        return;

    va_start(argptr, message);
    vsnprintf(buffer, sizeof(buffer), message, argptr);
    va_end(argptr);

    buffer[sizeof(buffer) - 1] = 0;

    CON_Message("%s", buffer);
}

//
// CON_PlayerMessageLDF
//
// -ACB- 1999/09/22 Console Player Message Only. Changed from
//                  #define to procedure because of compiler
//                  differences.
//
void CON_PlayerMessageLDF(int plyr, const char *message, ...)
{
    va_list argptr;
    char    buffer[256];

    if (consoleplayer != plyr)
        return;

    message = language[message];

    va_start(argptr, message);
    vsnprintf(buffer, sizeof(buffer), message, argptr);
    va_end(argptr);

    buffer[sizeof(buffer) - 1] = 0;

    CON_Message("%s", buffer);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
