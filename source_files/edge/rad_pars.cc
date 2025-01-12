//----------------------------------------------------------------------------
//  EDGE Radius Trigger Parsing
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

#include <limits.h>
#include <stdarg.h>

#include <unordered_map>

#include "dm_defs.h"
#include "dm_state.h"
#include "epi.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"
#include "g_game.h"
#include "m_argv.h"
#include "m_math.h"
#include "p_local.h"
#include "p_spec.h"
#include "r_defs.h"
#include "r_modes.h"
#include "rad_act.h"
#include "rad_trig.h"
#include "s_sound.h"
#include "stb_sprintf.h"
#include "version.h"
#include "w_wad.h"

static std::unordered_map<uint32_t, std::string> parsed_string_tags;

struct RADScriptParser
{
    // needed level:
    //   -1 : don't care
    //    0 : outside any block
    //    1 : within START_MAP block
    //    2 : within RADIUS_TRIGGER block
    int level;

    // name
    const char *name;

    // number of parameters
    int minimum_parameters, maximum_parameters;

    // parser function
    void (*parser)(std::vector<const char *> &pars);
};

static int         current_script_line_number;
static const char *current_script_filename;
static std::string current_script_line;

// Determine whether the code blocks are started and terminated.
static int current_script_level = 0;

static constexpr const char *rad_level_names[3] = {"outer area", "map area", "trigger area"};

// Location of current script
static RADScript *this_script;
static char      *this_map = nullptr;

// Pending state info for current script
static int   pending_wait_tics = 0;
static char *pending_label     = nullptr;

void ScriptError(const char *err, ...)
{
    va_list argptr;
    char    buffer[2048];
    char   *pos;

    buffer[2047] = 0;

    // put actual message on first line
    va_start(argptr, err);
    stbsp_vsprintf(buffer, err, argptr);
    va_end(argptr);

    pos = buffer + strlen(buffer);

    stbsp_sprintf(pos, "Error occurred near line %d of %s\n", current_script_line_number, current_script_filename);
    pos += strlen(pos);

    stbsp_sprintf(pos, "Line contents: %s\n", current_script_line.c_str());
    pos += strlen(pos);

    // check for buffer overflow
    if (buffer[2047] != 0)
        FatalError("Buffer overflow in ScriptError.\n");

    // add a blank line for readability in the log file
    LogPrint("\n");

    FatalError("%s", buffer);
}

void ScriptWarning(const char *err, ...)
{
    if (no_warnings)
        return;

    va_list argptr;
    char    buffer[1024];

    va_start(argptr, err);
    stbsp_vsprintf(buffer, err, argptr);
    va_end(argptr);

    LogWarning("\n");
    LogWarning("Found problem near line %d of %s\n", current_script_line_number, current_script_filename);
    LogWarning("Line contents: %s\n", current_script_line.c_str());
    LogWarning("%s", buffer);
}

void ScriptWarnError(const char *err, ...)
{
    va_list argptr;
    char    buffer[1024];

    va_start(argptr, err);
    stbsp_vsprintf(buffer, err, argptr);
    va_end(argptr);

    if (strict_errors)
        ScriptError("%s", buffer);
    else
        ScriptWarning("%s", buffer);
}

static void ScriptCheckForInt(const char *value, int *retvalue)
{
    const char *pos    = value;
    int         count  = 0;
    int         length = strlen(value);

    if (strchr(value, '%'))
        ScriptError("Parameter '%s' should not be a percentage.\n", value);

    // Accomodate for "-" as you could have -5 or something like that.
    if (*pos == '-')
    {
        count++;
        pos++;
    }
    while (epi::IsDigitASCII(*pos++))
        count++;

    // Is the value an integer?
    if (length != count)
        ScriptError("Parameter '%s' is not of numeric type.\n", value);

    *retvalue = atoi(value);
}

static void ScriptCheckForFloat(const char *value, float *retvalue)
{
    if (strchr(value, '%'))
        ScriptError("Parameter '%s' should not be a percentage.\n", value);

    if (sscanf(value, "%f", retvalue) != 1)
        ScriptError("Parameter '%s' is not of numeric type.\n", value);
}

//
// ScriptCheckForPercent
//
// Reads percentages (0%..100%).
//
static void ScriptCheckForPercent(const char *info, void *storage)
{
    char  s[101];
    char *p;
    float f;

    // just check that the string is valid
    epi::CStringCopyMax(s, info, 100);
    for (p = s; epi::IsDigitASCII(*p) || *p == '.'; p++)
    { /* nothing here */
    }

    // the number must be followed by %
    if (*p != '%')
        ScriptError("Parameter '%s' is not of percent type.\n", info);
    *p = 0;

    ScriptCheckForFloat(s, &f);
    if (f < 0.0f || f > 100.0f)
        ScriptError("Percentage out of range: %s\n", info);

    *(float *)storage = f / 100.0f;
}

//
// ScriptCheckForPercentAny
//
// Like the above routine, but don't limit to 0..100%.
//
static void ScriptCheckForPercentAny(const char *info, void *storage)
{
    char  s[101];
    char *p;
    float f;

    // just check that the string is valid
    epi::CStringCopyMax(s, info, 100);
    for (p = s; epi::IsDigitASCII(*p) || *p == '-' || *p == '.'; p++)
    { /* nothing here */
    }

    // the number must be followed by %
    if (*p != '%')
        ScriptError("Parameter '%s' is not of percent type.\n", info);
    *p = 0;

    ScriptCheckForFloat(s, &f);

    *(float *)storage = f / 100.0f;
}

// -ES- Copied from DDFMainGetTime.
// FIXME: Collect all functions that are common to DDF and RTS,
// and move them to a new module for RTS+DDF common code.
static void ScriptCheckForTime(const char *info, void *storage)
{
    int *dest = (int *)storage;

    EPI_ASSERT(info && storage);

    // -ES- 1999/09/14 MAXT means that time should be maximal.
    if (!epi::StringCaseCompareASCII(info, "maxt"))
    {
        *dest = INT_MAX; // -ACB- 1999/09/22 Standards, Please.
        return;
    }

    const char *p = strchr(info, 'T');

    if (!p)
        p = strchr(info, 't');

    if (p)
    {
        std::string temp(info, p - info);

        ScriptCheckForInt(temp.c_str(), (int *)storage);
        return;
    }

    float val;
    if (sscanf(info, "%f", &val) != 1)
    {
        // LogWarning("Bad time value '%s'.\n", info);
        LogWarning("RTS: Bad time value '%s' near line %d.\n", info, current_script_line_number);
        return;
    }

    *dest = RoundToInteger(val * (float)kTicRate);
}

static ArmourType ScriptCheckForArmourType(const char *info)
{
    if (DDFCompareName(info, "GREEN") == 0)
        return kArmourTypeGreen;
    if (DDFCompareName(info, "BLUE") == 0)
        return kArmourTypeBlue;
    if (DDFCompareName(info, "PURPLE") == 0)
        return kArmourTypePurple;
    if (DDFCompareName(info, "YELLOW") == 0)
        return kArmourTypeYellow;
    if (DDFCompareName(info, "RED") == 0)
        return kArmourTypeRed;

    // this never returns
    ScriptError("Unknown armour type: %s\n", info);
    return kArmourTypeGreen;
}

static ScriptChangeTexturetureType ScriptCheckForChangetexType(const char *info)
{
    if (DDFCompareName(info, "LEFT_UPPER") == 0 || DDFCompareName(info, "BACK_UPPER") == 0)
        return kChangeTextureLeftUpper;
    if (DDFCompareName(info, "LEFT_MIDDLE") == 0 || DDFCompareName(info, "BACK_MIDDLE") == 0)
        return kChangeTextureLeftMiddle;
    if (DDFCompareName(info, "LEFT_LOWER") == 0 || DDFCompareName(info, "BACK_LOWER") == 0)
        return kChangeTextureLeftLower;
    if (DDFCompareName(info, "RIGHT_UPPER") == 0 || DDFCompareName(info, "FRONT_UPPER") == 0)
        return kChangeTextureRightUpper;
    if (DDFCompareName(info, "RIGHT_MIDDLE") == 0 || DDFCompareName(info, "FRONT_MIDDLE") == 0)
        return kChangeTextureRightMiddle;
    if (DDFCompareName(info, "RIGHT_LOWER") == 0 || DDFCompareName(info, "FRONT_LOWER") == 0)
        return kChangeTextureRightLower;
    if (DDFCompareName(info, "FLOOR") == 0)
        return kChangeTextureFloor;
    if (DDFCompareName(info, "CEILING") == 0)
        return kChangeTextureCeiling;
    if (DDFCompareName(info, "SKY") == 0)
        return kChangeTextureSky;

    // this never returns
    ScriptError("Unknown ChangeTex type '%s'\n", info);
    return kChangeTextureRightUpper;
}

//
// ScriptUnquoteString
//
// Remove the quotes from the given string, returning a newly
// allocated string.
//
static char *ScriptUnquoteString(const char *s)
{
    if (s[0] != '"')
        return epi::CStringDuplicate(s);

    // skip initial quote
    s++;

    std::string new_str;

    while (s[0] != '"')
    {
#ifdef DEVELOPERS
        if (s[0] == 0)
            FatalError("INTERNAL ERROR: bad string.\n");
#endif

        // -AJA- 1999/09/07: check for \n
        if (s[0] == '\\' && epi::ToLowerASCII(s[1]) == 'n')
        {
            new_str += '\n';
            s += 2;
            continue;
        }

        new_str += s[0];
        s += 1;
    }

    return epi::CStringDuplicate(new_str.c_str());
}

static bool CheckForBoolean(const char *s)
{
    if (epi::StringCaseCompareASCII(s, "TRUE") == 0 || epi::StringCaseCompareASCII(s, "1") == 0)
        return true;

    if (epi::StringCaseCompareASCII(s, "FALSE") == 0 || epi::StringCaseCompareASCII(s, "0") == 0)
        return false;

    // Nope, it's an error.
    ScriptError("Bad boolean value (should be TRUE or FALSE): %s\n", s);
    return false;
}

// AddStateToScript
//
// Adds a new action state to the tail of the current set of states
// for the given radius trigger.
//
static void AddStateToScript(RADScript *R, int tics, void (*action)(struct RADScriptTrigger *R, void *param),
                             void *param)
{
    RADScriptState *state = new RADScriptState;

    state->tics   = tics;
    state->action = action;
    state->param  = param;

    state->tics += pending_wait_tics;
    state->label = pending_label;

    pending_wait_tics = 0;
    pending_label     = nullptr;

    // link it in
    state->next = nullptr;
    state->prev = R->last_state;

    if (R->last_state)
        R->last_state->next = state;
    else
        R->first_state = state;

    R->last_state = state;
}

//
// ClearOneScripts
//
static void ClearOneScript(RADScript *scr)
{
    if (scr->mapid)
    {
        epi::CStringFree(scr->mapid);
        scr->mapid = nullptr;
    }

    while (scr->boss_trig)
    {
        ScriptOnDeathParameter *cur = scr->boss_trig;
        scr->boss_trig              = cur->next;

        delete cur;
        cur = nullptr;
    }

    while (scr->height_trig)
    {
        ScriptOnHeightParameter *cur = scr->height_trig;
        scr->height_trig             = cur->next;

        delete cur;
        cur = nullptr;
    }

    // free all states
    while (scr->first_state)
    {
        RADScriptState *cur = scr->first_state;
        scr->first_state    = cur->next;

        if (cur->param)
        {
            RADScriptParameter *param_pointer = (RADScriptParameter *)cur->param;
            delete param_pointer;
            cur->param = nullptr;
        }

        delete cur;
        cur = nullptr;
    }
}

// ClearPreviousScripts
//
// Removes any radius triggers for a given map when start_map is used.
// Thus triggers in later RTS files/lumps replace those in earlier RTS
// files/lumps in the specified level.
//
static void ClearPreviousScripts(const char *mapid)
{
    // the "ALL" keyword is not a valid map name
    if (DDFCompareName(mapid, "ALL") == 0)
        return;

    RADScript *scr, *next;

    for (scr = current_scripts; scr; scr = next)
    {
        next = scr->next;

        if (epi::StringCaseCompareASCII(scr->mapid, mapid) == 0)
        {
            // unlink and free it
            if (scr->next)
                scr->next->prev = scr->prev;

            if (scr->prev)
                scr->prev->next = scr->next;
            else
                current_scripts = scr->next;

            ClearOneScript(scr);

            delete scr;
        }
    }
}

// ClearAllScripts
//
// Removes all radius triggers from all maps.
//
static void ClearAllScripts(void)
{
    while (current_scripts)
    {
        RADScript *scr  = current_scripts;
        current_scripts = scr->next;

        ClearOneScript(scr);

        delete scr;

        scr = nullptr;
    }
}

//
// ScriptComputeScriptCRC
//
static void ScriptComputeScriptCRC(RADScript *scr)
{
    scr->crc.Reset();

    // Note: the mapid doesn't belong in the CRC

    if (scr->script_name)
        scr->crc.AddCString(scr->script_name);

    scr->crc += (int)scr->tag[0];
    scr->crc += (int)scr->tag[1];
    scr->crc += (int)scr->appear;
    scr->crc += (int)scr->min_players;
    scr->crc += (int)scr->max_players;
    scr->crc += (int)scr->repeat_count;

    scr->crc += (int)RoundToInteger(scr->x);
    scr->crc += (int)RoundToInteger(scr->y);
    scr->crc += (int)RoundToInteger(scr->z);
    scr->crc += (int)RoundToInteger(scr->rad_x);
    scr->crc += (int)RoundToInteger(scr->rad_y);
    scr->crc += (int)RoundToInteger(scr->rad_z);
    scr->crc += scr->sector_tag;
    scr->crc += scr->sector_index;

    // lastly handle miscellaneous parts

    int flags = 0;

    if ((scr->tagged_disabled))
    {
        flags |= (1 << (0));
    }
    if ((scr->tagged_use))
    {
        flags |= (1 << (1));
    }
    if ((scr->tagged_independent))
    {
        flags |= (1 << (2));
    }
    if ((scr->tagged_immediate))
    {
        flags |= (1 << (3));
    }

    if ((scr->boss_trig != nullptr))
    {
        flags |= (1 << (4));
    }
    if ((scr->height_trig != nullptr))
    {
        flags |= (1 << (5));
    }
    if ((scr->cond_trig != nullptr))
    {
        flags |= (1 << (6));
    }
    if ((scr->next_in_path != nullptr))
    {
        flags |= (1 << (7));
    }

    scr->crc += (int)flags;

    // Q/ add in states ?
    // A/ Nah.
}

// ScriptTokenizeLine
//
// Collect the parameters from the line into an array of strings
// 'pars', which can hold at most 'max' string pointers.
//
// -AJA- 2000/01/02: Moved #define handling to here.
//
static void ScriptTokenizeLine(std::vector<const char *> &pars)
{
    const char *line = current_script_line.c_str();

    std::string tokenbuf;

    bool want_token = true;
    bool in_string  = false;
    int  in_expr    = 0; // add one for each open bracket.

    for (;;)
    {
        int ch = *line++;

        bool comment = (ch == ';' || (ch == '/' && *line == '/'));

        if (in_string)
            comment = false;

        if (ch == 0 && in_string)
            ScriptError("Nonterminated string found.\n");

        if ((ch == 0 || comment) && in_expr)
            ScriptError("Nonterminated expression found.\n");

        if (want_token) // looking for a new token
        {
            EPI_ASSERT(!in_expr && !in_string);

            // end of line ?
            if (ch == 0 || comment)
                return;

            if (epi::IsSpaceASCII(ch))
                continue;

            // string ? or expression ?
            if (ch == '"')
                in_string = true;
            else if (ch == '(' && !in_string)
                in_expr++;
            else if (ch == ')' && !in_string)
                ScriptError("Unmatched ')' bracket found\n");

            // begin a new token
            tokenbuf.clear();
            tokenbuf += ch;

            want_token = false;
            continue;
        }

        bool end_token = false;

        if (ch == '"' && in_string)
        {
            in_string = false;

            if (!in_expr)
            {
                tokenbuf += ch;
                end_token = true;
            }
        }
        else if (ch == '(' && in_expr)
        {
            in_expr++;
        }
        else if (ch == ')' && in_expr)
        {
            in_expr--;

            if (in_expr == 0)
            {
                tokenbuf += ch;
                end_token = true;
            }
        }
        else if (!in_expr && !in_string && (ch == 0 || comment || epi::IsSpaceASCII(ch)))
        {
            end_token = true;
        }

        // end of token ?
        if (!end_token)
        {
            tokenbuf += ch;
            continue;
        }

        want_token = true;

        // check for defines
        const char *par_str = epi::CStringDuplicate(DDFMainGetDefine(tokenbuf.c_str()));

        pars.push_back(par_str);

        // end of line ?
        if (ch == 0 || comment)
            break;
    }
}

// ScriptFreeParameters
//
// Free previously collected parameters.
//
static void ScriptFreeParameters(std::vector<const char *> &pars)
{
    for (size_t i = 0; i < pars.size(); i++)
    {
        epi::CStringFree(pars[i]);
    }
}

// ---- Primitive Parsers ----------------------------------------------

static void ScriptParseVersion(std::vector<const char *> &pars)
{
    // ignored for compatibility
    EPI_UNUSED(pars);
}

static void ScriptParseClearAll(std::vector<const char *> &pars)
{
    // #ClearAll
    EPI_UNUSED(pars);
    ClearAllScripts();
}

static void ScriptParseClearMap(std::vector<const char *> &pars)
{
    // #CLEAR_MAP <map>

    ClearPreviousScripts(pars[1]);
}

static void ScriptParseDefine(std::vector<const char *> &pars)
{
    // #Define <identifier> <num>

    DDFMainAddDefine(pars[1], pars[2]);
}

static void ScriptParseStartMap(std::vector<const char *> &pars)
{
    // Start_Map <map>

    if (current_script_level != 0)
        ScriptError("%s found, but previous END_MAP missing !\n", pars[0]);

    // -AJA- 1999/08/02: New scripts replace old ones.
    // Dasho 2023/12/07: Commented out in lieu of the new
    // #CLEAR_MAP directive when the modders actually wants this
    // ClearPreviousScripts(pars[1]);

    this_map = epi::CStringDuplicate(pars[1]);
    for (size_t i = 0; i < strlen(this_map); i++)
    {
        this_map[i] = epi::ToUpperASCII(this_map[i]);
    }

    current_script_level++;
}

static void ScriptParseRadiusTrigger(std::vector<const char *> &pars)
{
    // RadiusTrigger <x> <y> <radius>
    // RadiusTrigger <x> <y> <radius> <low z> <high z>
    //
    // RectTrigger <x1> <y1> <x2> <y2>
    // RectTrigger <x1> <y1> <x2> <y2> <z1> <z2>

    // -AJA- 1999/09/12: Reworked for having Z-restricted triggers.

    if (current_script_level == 2)
        ScriptError("%s found, but previous END_RADIUS_TRIGGER missing !\n", pars[0]);

    if (current_script_level == 0)
        ScriptError("%s found, but without any START_MAP !\n", pars[0]);

    // Set the node up,..

    this_script = new RADScript;

    // set defaults
    this_script->x                    = 0;
    this_script->y                    = 0;
    this_script->z                    = 0;
    this_script->rad_x                = -1;
    this_script->rad_y                = -1;
    this_script->rad_z                = -1;
    this_script->sector_tag           = 0;
    this_script->sector_index         = -1;
    this_script->appear               = kAppearsWhenDefault;
    this_script->min_players          = 0;
    this_script->max_players          = kMaximumPlayers;
    this_script->absolute_req_players = 1;
    this_script->repeat_count         = -1;
    this_script->repeat_delay         = 0;

    pending_wait_tics = 0;
    pending_label     = nullptr;

    if (DDFCompareName("RECT_TRIGGER", pars[0]) == 0)
    {
        float x1, y1, x2, y2, z1, z2;

        if (pars.size() == 6)
            ScriptError("%s: Wrong number of parameters.\n", pars[0]);

        ScriptCheckForFloat(pars[1], &x1);
        ScriptCheckForFloat(pars[2], &y1);
        ScriptCheckForFloat(pars[3], &x2);
        ScriptCheckForFloat(pars[4], &y2);

        if (x1 > x2)
            ScriptWarnError("%s: bad X range %1.1f to %1.1f\n", pars[0], x1, x2);
        if (y1 > y2)
            ScriptWarnError("%s: bad Y range %1.1f to %1.1f\n", pars[0], y1, y2);

        this_script->x     = (float)(x1 + x2) / 2.0f;
        this_script->y     = (float)(y1 + y2) / 2.0f;
        this_script->rad_x = (float)fabs(x1 - x2) / 2.0f;
        this_script->rad_y = (float)fabs(y1 - y2) / 2.0f;

        if (pars.size() >= 7)
        {
            ScriptCheckForFloat(pars[5], &z1);
            ScriptCheckForFloat(pars[6], &z2);

            if (z1 > z2 + 1)
                ScriptWarnError("%s: bad height range %1.1f to %1.1f\n", pars[0], z1, z2);

            this_script->z     = (z1 + z2) / 2.0f;
            this_script->rad_z = fabs(z1 - z2) / 2.0f;
        }
    }
    else
    {
        if (pars.size() == 5)
            ScriptError("%s: Wrong number of parameters.\n", pars[0]);

        ScriptCheckForFloat(pars[1], &this_script->x);
        ScriptCheckForFloat(pars[2], &this_script->y);
        ScriptCheckForFloat(pars[3], &this_script->rad_x);

        this_script->rad_y = this_script->rad_x;

        if (pars.size() >= 6)
        {
            float z1, z2;

            ScriptCheckForFloat(pars[4], &z1);
            ScriptCheckForFloat(pars[5], &z2);

            if (z1 > z2)
                ScriptWarnError("%s: bad height range %1.1f to %1.1f\n", pars[0], z1, z2);

            this_script->z     = (z1 + z2) / 2.0f;
            this_script->rad_z = fabs(z1 - z2) / 2.0f;
        }
    }

    // link it in
    this_script->next = current_scripts;
    this_script->prev = nullptr;

    if (current_scripts)
        current_scripts->prev = this_script;

    current_scripts = this_script;

    current_script_level++;
}

static void ScriptParseSectorTrigger(std::vector<const char *> &pars)
{
    // SectorTriggerTag <sector tag>
    // SectorTriggerTag <sector tag> <low z> <high z>

    // SectorTriggerIndex <sector index>
    // SectorTriggerIndex <sector index> <low z> <high z>

    if (current_script_level == 2)
        ScriptError("%s found, but previous END_RADIUS_TRIGGER missing !\n", pars[0]);

    if (current_script_level == 0)
        ScriptError("%s found, but without any START_MAP !\n", pars[0]);

    // Set the node up,..

    this_script = new RADScript;

    // set defaults
    this_script->x                    = 0;
    this_script->y                    = 0;
    this_script->z                    = 0;
    this_script->rad_x                = -1;
    this_script->rad_y                = -1;
    this_script->rad_z                = -1;
    this_script->sector_tag           = 0;
    this_script->sector_index         = -1;
    this_script->appear               = kAppearsWhenDefault;
    this_script->min_players          = 0;
    this_script->max_players          = kMaximumPlayers;
    this_script->absolute_req_players = 1;
    this_script->repeat_count         = -1;
    this_script->repeat_delay         = 0;

    pending_wait_tics = 0;
    pending_label     = nullptr;

    if (pars.size() != 2 && pars.size() != 4)
        ScriptError("%s: Wrong number of parameters.\n", pars[0]);

    if (epi::StringCaseCompareASCII(pars[0], "SECTOR_TRIGGER_TAG") == 0)
        ScriptCheckForInt(pars[1], &this_script->sector_tag);
    else
        ScriptCheckForInt(pars[1], &this_script->sector_index);

    if (pars.size() == 4)
    {
        float z1, z2;

        ScriptCheckForFloat(pars[2], &z1);
        ScriptCheckForFloat(pars[3], &z2);

        if (z1 > z2)
            ScriptWarnError("%s: bad height range %1.1f to %1.1f\n", pars[0], z1, z2);

        this_script->z     = (z1 + z2) / 2.0f;
        this_script->rad_z = fabs(z1 - z2) / 2.0f;
    }

    // link it in
    this_script->next = current_scripts;
    this_script->prev = nullptr;

    if (current_scripts)
        current_scripts->prev = this_script;

    current_scripts = this_script;

    current_script_level++;
}

static void ScriptParseEndRadiusTrigger(std::vector<const char *> &pars)
{
    // End_RadiusTrigger

    if (current_script_level != 2)
        ScriptError("%s found, but without any SECTOR_TRIGGER or RADIUS_TRIGGER !\n", pars[0]);

    // --- check stuff ---

    // handle any pending WAIT or LABEL values
    if (pending_wait_tics > 0 || pending_label)
    {
        AddStateToScript(this_script, 0, ScriptNoOperation, nullptr);
    }

    this_script->mapid = epi::CStringDuplicate(this_map);
    ScriptComputeScriptCRC(this_script);
    this_script = nullptr;

    current_script_level--;
}

static void ScriptParseEndMap(std::vector<const char *> &pars)
{
    // End_Map

    if (current_script_level == 2)
        ScriptError("%s found, but previous END_RADIUS_TRIGGER missing !\n", pars[0]);

    if (current_script_level == 0)
        ScriptError("%s found, but without any START_MAP !\n", pars[0]);

    this_map = nullptr;

    current_script_level--;
}

static void ScriptParseName(std::vector<const char *> &pars)
{
    // Name <name>

    if (this_script->script_name)
        ScriptError("Script already has a name: '%s'\n", this_script->script_name);

    this_script->script_name = epi::CStringDuplicate(pars[1]);
}

static void ScriptParseTag(std::vector<const char *> &pars)
{
    // Tag <number>

    if (this_script->tag[0] != 0)
        ScriptError("Script already has a tag: '%d'\n", this_script->tag[0]);

    if (this_script->tag[1] != 0)
    {
        if (parsed_string_tags.find(this_script->tag[1]) != parsed_string_tags.end())
            ScriptError("Script already has a tag: '%s'\n", parsed_string_tags[this_script->tag[1]].c_str());
        else
            ScriptError("Script already has a tag: '%d'\n", this_script->tag[1]);
    }

    // Modified ScriptCheckForInt
    const char *pos    = pars[1];
    int         count  = 0;
    int         length = strlen(pars[1]);

    while (epi::IsDigitASCII(*pos++))
        count++;

    // Is the value an integer?
    if (length != count)
    {
        this_script->tag[1] = epi::StringHash64(pars[1]);
        parsed_string_tags.try_emplace(this_script->tag[1], pars[1]);
    }
    else
        this_script->tag[0] = atoi(pars[1]);
}

static void ScriptParseWhenAppear(std::vector<const char *> &pars)
{
    // When_Appear 1:2:3:4:5:SP:COOP:DM

    DDFMainGetWhenAppear(pars[1], &this_script->appear);
}

static void ScriptParseWhenPlayerNum(std::vector<const char *> &pars)
{
    // When_Player_Num <min> [max]

    ScriptCheckForInt(pars[1], &this_script->min_players);

    this_script->max_players = kMaximumPlayers;

    if (pars.size() >= 3)
        ScriptCheckForInt(pars[2], &this_script->max_players);

    if (this_script->min_players < 0 || this_script->min_players > this_script->max_players)
    {
        ScriptError("%s: Illegal range: %d..%d\n", pars[0], this_script->min_players, this_script->max_players);
    }
}

static void ScriptParseNetMode(std::vector<const char *> &pars)
{
    // Net_Mode SEPARATE
    // Net_Mode ABSOLUTE
    //
    // NOTE: IGNORED FOR BACKWARDS COMPATIBILITY
    EPI_UNUSED(pars);
}

static void ScriptParseTaggedRepeatable(std::vector<const char *> &pars)
{
    // Tagged_Repeatable
    // Tagged_Repeatable <num repetitions>
    // Tagged_Repeatable <num repetitions> <delay>

    if (this_script->repeat_count >= 0)
        ScriptError("%s: can only be used once.\n", pars[0]);

    if (pars.size() >= 2)
        ScriptCheckForInt(pars[1], &this_script->repeat_count);
    else
        this_script->repeat_count = 0;

    // -ES- 2000/03/03 Changed to ScriptCheckForTime.
    if (pars.size() >= 3)
        ScriptCheckForTime(pars[2], &this_script->repeat_delay);
    else
        this_script->repeat_delay = 1;
}

static void ScriptParseTaggedUse(std::vector<const char *> &pars)
{
    // Tagged_Use
    EPI_UNUSED(pars);
    this_script->tagged_use = true;
}

static void ScriptParseTaggedIndependent(std::vector<const char *> &pars)
{
    // Tagged_Independent
    EPI_UNUSED(pars);
    this_script->tagged_independent = true;
}

static void ScriptParseTaggedImmediate(std::vector<const char *> &pars)
{
    // Tagged_Immediate
    EPI_UNUSED(pars);
    this_script->tagged_immediate = true;
}

static void ScriptParseTaggedPlayerSpecific(std::vector<const char *> &pars)
{
    // Tagged_Player_Specific

    // NOTE: IGNORED FOR BACKWARDS COMPATIBILITY
    EPI_UNUSED(pars);
}

static void ScriptParseTaggedDisabled(std::vector<const char *> &pars)
{
    // Tagged_Disabled
    EPI_UNUSED(pars);
    this_script->tagged_disabled = true;
}

static void ScriptParseTaggedPath(std::vector<const char *> &pars)
{
    // Tagged_Path  <next node>

    RADScriptPath *path = new RADScriptPath;

    path->next = this_script->next_in_path;
    path->name = epi::CStringDuplicate(pars[1]);

    this_script->next_in_path = path;
    this_script->next_path_total += 1;
}

static void ScriptParsePathEvent(std::vector<const char *> &pars)
{
    // Path_Event  <label>

    const char *div;
    int         i;

    if (this_script->path_event_label)
        ScriptError("%s: Can only be used once per trigger.\n", pars[0]);

    // parse the label name
    div = strchr(pars[1], ':');

    i = div ? (div - pars[1]) : strlen(pars[1]);

    if (i <= 0)
        ScriptError("%s: Bad label '%s'.\n", pars[0], pars[2]);

    this_script->path_event_label = new char[i + 1];
    epi::CStringCopyMax((char *)this_script->path_event_label, pars[1], i);

    this_script->path_event_offset = div ? HMM_MAX(0, atoi(div + 1) - 1) : 0;
}

static void ScriptParseOnDeath(std::vector<const char *> &pars)
{
    // OnDeath <thing type>
    // OnDeath <thing type> <threshhold>

    ScriptOnDeathParameter *cond = new ScriptOnDeathParameter;

    // get map thing
    if (pars[1][0] == '-' || pars[1][0] == '+' || epi::IsDigitASCII(pars[1][0]))
    {
        ScriptCheckForInt(pars[1], &cond->thing_type);
    }
    else
        cond->thing_name = epi::CStringDuplicate(pars[1]);

    if (pars.size() >= 3)
    {
        ScriptCheckForInt(pars[2], &cond->threshhold);
    }

    // link it into list of ONDEATH conditions
    cond->next             = this_script->boss_trig;
    this_script->boss_trig = cond;
}

static void ScriptParseOnHeight(std::vector<const char *> &pars)
{
    // OnHeight <low Z> <high Z>
    // OnHeight <low Z> <high Z> <sector num>
    //
    // OnCeilingHeight <low Z> <high Z>
    // OnCeilingHeight <low Z> <high Z> <sector num>

    ScriptOnHeightParameter *cond = new ScriptOnHeightParameter;

    cond->sec_num = -1;

    ScriptCheckForFloat(pars[1], &cond->z1);
    ScriptCheckForFloat(pars[2], &cond->z2);

    if (cond->z1 > cond->z2)
        ScriptError("%s: bad height range %1.1f..%1.1f\n", pars[0], cond->z1, cond->z2);

    // get sector reference
    if (pars.size() >= 4)
    {
        ScriptCheckForInt(pars[3], &cond->sec_num);
    }

    cond->is_ceil = (DDFCompareName("ONCEILINGHEIGHT", pars[0]) == 0);

    // link it into list of ONHEIGHT conditions
    cond->next               = this_script->height_trig;
    this_script->height_trig = cond;
}

static void ScriptParseOnCondition(std::vector<const char *> &pars)
{
    // OnCondition  <condition>

    ConditionCheck *cond = new ConditionCheck;

    if (!DDFMainParseCondition(pars[1], cond))
    {
        delete cond;
        return;
    }

    // link it into list of ONCONDITION list
    cond->next             = this_script->cond_trig;
    this_script->cond_trig = cond;
}

static void ScriptParseLabel(std::vector<const char *> &pars)
{
    // Label <label>

    if (pending_label)
        ScriptError("State already has a label: '%s'\n", pending_label);

    // handle any pending WAIT value
    if (pending_wait_tics > 0)
        AddStateToScript(this_script, 0, ScriptNoOperation, nullptr);

    pending_label = epi::CStringDuplicate(pars[1]);
}

static void ScriptParseEnableScript(std::vector<const char *> &pars)
{
    // Enable_Script  <script name>
    // Disable_Script <script name>

    ScriptEnablerParameter *t = new ScriptEnablerParameter;

    t->script_name  = epi::CStringDuplicate(pars[1]);
    t->new_disabled = DDFCompareName("DISABLE_SCRIPT", pars[0]) == 0;

    AddStateToScript(this_script, 0, ScriptEnableScript, t);
}

static void ScriptParseEnableTagged(std::vector<const char *> &pars)
{
    // Enable_Tagged  <tag num>
    // Disable_Tagged <tag num>

    ScriptEnablerParameter *t = new ScriptEnablerParameter;

    // Modified ScriptCheckForInt
    const char *pos    = pars[1];
    int         count  = 0;
    int         length = strlen(pars[1]);

    while (epi::IsDigitASCII(*pos++))
        count++;

    // Is the value an integer?
    if (length != count)
        t->tag[1] = epi::StringHash64(pars[1]);
    else
        t->tag[0] = atoi(pars[1]);

    // if (t->tag <= 0)
    // ScriptError("Bad tag value: %s\n", pars[1]);

    t->new_disabled = DDFCompareName("DISABLE_TAGGED", pars[0]) == 0;

    AddStateToScript(this_script, 0, ScriptEnableScript, t);
}

static void ScriptParseExitLevel(std::vector<const char *> &pars)
{
    // ExitLevel
    // ExitLevel <wait time>
    //
    // SecretExit
    // SecretExit <wait time>

    ScriptExitParameter *exit = new ScriptExitParameter;

    exit->exit_time = 10;
    exit->is_secret = DDFCompareName("SECRETEXIT", pars[0]) == 0;

    if (pars.size() >= 2)
    {
        ScriptCheckForTime(pars[1], &exit->exit_time);
    }

    AddStateToScript(this_script, 0, ScriptExitLevel, exit);
}

// Lobo November 2021
static void ScriptParseExitGame(std::vector<const char *> &pars)
{
    // ExitGame to TitleScreen
    EPI_UNUSED(pars);
    AddStateToScript(this_script, 0, ScriptExitGame, nullptr);
}

static void ScriptParseTip(std::vector<const char *> &pars)
{
    // Tip "<text>"
    // Tip "<text>" <time>
    // Tip "<text>" <time> <has sound>
    // Tip "<text>" <time> <has sound> <scale>
    //
    // (likewise for Tip_LDF)
    // (likewise for Tip_Graphic)

    ScriptTip *tip = new ScriptTip;

    tip->display_time = 3 * kTicRate;
    tip->playsound    = false;
    tip->gfx_scale    = 1.0f;

    if (DDFCompareName(pars[0], "TIP_GRAPHIC") == 0)
    {
        tip->tip_graphic = epi::CStringDuplicate(pars[1]);
    }
    else if (DDFCompareName(pars[0], "TIP_LDF") == 0)
    {
        tip->tip_ldf = epi::CStringDuplicate(pars[1]);
    }
    else if (pars[1][0] == '"')
    {
        tip->tip_text = ScriptUnquoteString(pars[1]);
    }
    else
        ScriptError("Needed string for TIP command.\n");

    if (pars.size() >= 3)
        ScriptCheckForTime(pars[2], &tip->display_time);

    if (pars.size() >= 4)
        tip->playsound = CheckForBoolean(pars[3]);

    if (pars.size() >= 5)
    {
        /*if (! tip->tip_graphic)
            ScriptError("%s: scale value only works with TIP_GRAPHIC.\n",
           pars[0]);
        */
        ScriptCheckForFloat(pars[4], &tip->gfx_scale);
    }

    AddStateToScript(this_script, 0, ScriptShowTip, tip);
}

static void ScriptParseTipSlot(std::vector<const char *> &pars)
{
    // Tip_Slot <slotnum>

    ScriptTipProperties *tp;

    tp = new ScriptTipProperties;

    ScriptCheckForInt(pars[1], &tp->slot_num);

    if (tp->slot_num < 1 || tp->slot_num > kMaximumTipSlots)
        ScriptError("Bad tip slot '%d' -- must be between 1-%d\n", tp->slot_num, kMaximumTipSlots);

    tp->slot_num--;

    AddStateToScript(this_script, 0, ScriptUpdateTipProperties, tp);
}

static void ScriptParseTipPos(std::vector<const char *> &pars)
{
    // Tip_Set_Pos <x> <y>
    // Tip_Set_Pos <x> <y> <time>

    ScriptTipProperties *tp;

    tp = new ScriptTipProperties;

    ScriptCheckForPercentAny(pars[1], &tp->x_pos);
    ScriptCheckForPercentAny(pars[2], &tp->y_pos);

    if (pars.size() >= 4)
        ScriptCheckForTime(pars[3], &tp->time);

    AddStateToScript(this_script, 0, ScriptUpdateTipProperties, tp);
}

static void ScriptParseTipColour(std::vector<const char *> &pars)
{
    // Tip_Set_Colour <color>
    // Tip_Set_Colour <color> <time>

    ScriptTipProperties *tp;

    tp = new ScriptTipProperties;

    tp->color_name = epi::CStringDuplicate(pars[1]);

    if (pars.size() >= 3)
        ScriptCheckForTime(pars[2], &tp->time);

    AddStateToScript(this_script, 0, ScriptUpdateTipProperties, tp);
}

static void ScriptParseTipTrans(std::vector<const char *> &pars)
{
    // Tip_Set_Trans <translucency>
    // Tip_Set_Trans <translucency> <time>

    ScriptTipProperties *tp;

    tp = new ScriptTipProperties;

    ScriptCheckForPercent(pars[1], &tp->translucency);

    if (pars.size() >= 3)
        ScriptCheckForTime(pars[2], &tp->time);

    AddStateToScript(this_script, 0, ScriptUpdateTipProperties, tp);
}

static void ScriptParseTipAlign(std::vector<const char *> &pars)
{
    // Tip_Set_Align  CENTER/LEFT

    ScriptTipProperties *tp;

    tp = new ScriptTipProperties;

    if (DDFCompareName(pars[1], "CENTER") == 0 || DDFCompareName(pars[1], "CENTRE") == 0)
    {
        tp->left_just = 0;
    }
    else if (DDFCompareName(pars[1], "LEFT") == 0)
    {
        tp->left_just = 1;
    }
    else
    {
        ScriptWarnError("TIP_POS: unknown justify method '%s'\n", pars[1]);
    }

    AddStateToScript(this_script, 0, ScriptUpdateTipProperties, tp);
}

static void HandleSpawnKeyword(const char *par, ScriptThingParameter *t)
{
    if (epi::StringPrefixCaseCompareASCII(par, "X=") == 0)
        ScriptCheckForFloat(par + 2, &t->x);
    else if (epi::StringPrefixCaseCompareASCII(par, "Y=") == 0)
        ScriptCheckForFloat(par + 2, &t->y);
    else if (epi::StringPrefixCaseCompareASCII(par, "Z=") == 0)
        ScriptCheckForFloat(par + 2, &t->z);
    else if (epi::StringPrefixCaseCompareASCII(par, "TAG=") == 0)
        ScriptCheckForInt(par + 4, &t->tag);
    else if (epi::StringPrefixCaseCompareASCII(par, "ANGLE=") == 0)
    {
        int val;
        ScriptCheckForInt(par + 6, &val);

        if (HMM_ABS(val) <= 360)
            t->angle = epi::BAMFromDegrees((float)val);
        else
            t->angle = val << 16;
    }
    else if (epi::StringPrefixCaseCompareASCII(par, "SLOPE=") == 0)
    {
        ScriptCheckForFloat(par + 6, &t->slope);
        t->slope /= 45.0f;
    }
    else if (epi::StringPrefixCaseCompareASCII(par, "WHEN=") == 0)
    {
        DDFMainGetWhenAppear(par + 5, &t->appear);
    }
    else
    {
        ScriptError("SPAWN_THING: unknown keyword parameter: %s\n", par);
    }
}

static void ScriptParseSpawnThing(std::vector<const char *> &pars)
{
    // SpawnThing <thingid>
    // SpawnThing <thingid> <angle>
    // SpawnThing <thingid> <x> <y>
    // SpawnThing <thingid> <x> <y> <angle>
    // SpawnThing <thingid> <x> <y> <angle> <z>
    // SpawnThing <thingid> <x> <y> <angle> <z> <slope>
    //
    // (likewise for SpawnThing_Ambush)
    // (likewise for SpawnThing_Flash)
    //
    // Keyword parameters (after all positional parameters)
    //   X=<num>
    //   Y=<num>
    //   Z=<num>
    //   ANGLE=<num>
    //   SLOPE=<num>
    //   TAG=<num>
    //   WHEN=<when-appear>
    //
    // -ACB- 1998/08/06 Use MapObjectDefinition linked list
    // -AJA- 1999/09/11: Extra fields for Z and slope.

    // -AJA- 1999/09/11: Reworked for spawning things at Z.

    ScriptThingParameter *t = new ScriptThingParameter;

    // set defaults
    t->x = this_script->x;
    t->y = this_script->y;

    if (this_script->rad_z < 0)
        t->z = kOnFloorZ;
    else
        t->z = this_script->z - this_script->rad_z;

    t->appear = kAppearsWhenDefault;

    t->ambush       = DDFCompareName("SPAWNTHING_AMBUSH", pars[0]) == 0;
    t->spawn_effect = DDFCompareName("SPAWNTHING_FLASH", pars[0]) == 0;

    // get map thing
    if (pars[1][0] == '-' || pars[1][0] == '+' || epi::IsDigitASCII(pars[1][0]))
    {
        ScriptCheckForInt(pars[1], &t->thing_type);
    }
    else
        t->thing_name = epi::CStringDuplicate(pars[1]);

    // handle keyword parameters
    while (pars.size() >= 3 && strchr(pars.back(), '=') != nullptr)
    {
        HandleSpawnKeyword(pars.back(), t);
        pars.pop_back();
    }

    // get angle
    const char *angle_str = (pars.size() == 3) ? pars[2] : (pars.size() >= 5) ? pars[4] : nullptr;

    if (angle_str)
    {
        int val;

        ScriptCheckForInt(angle_str, &val);

        if (HMM_ABS(val) <= 360)
            t->angle = epi::BAMFromDegrees((float)val);
        else
            t->angle = val << 16;
    }

    // check for x, y, z, slope

    if (pars.size() >= 4)
    {
        ScriptCheckForFloat(pars[2], &t->x);
        ScriptCheckForFloat(pars[3], &t->y);
    }
    if (pars.size() >= 6)
    {
        ScriptCheckForFloat(pars[5], &t->z);
    }
    if (pars.size() >= 7)
    {
        ScriptCheckForFloat(pars[6], &t->slope);

        // FIXME: Merge with DDFMainGetSlope someday.
        t->slope /= 45.0f;
    }

    AddStateToScript(this_script, 0, ScriptSpawnThing, t);
}

static void ScriptParsePlaySound(std::vector<const char *> &pars)
{
    // PlaySound <soundid>
    // PlaySound <soundid> <x> <y>
    // PlaySound <soundid> <x> <y> <z>
    //
    // (likewise for PlaySound_BossMan)
    //
    // -AJA- 1999/09/12: Reworked for playing sound at specific Z.

    if (pars.size() == 3)
        ScriptError("%s: Wrong number of parameters.\n", pars[0]);

    ScriptSoundParameter *t = new ScriptSoundParameter;

    if (DDFCompareName(pars[0], "PLAYSOUND_BOSSMAN") == 0)
        t->kind = kScriptSoundBossMan;
    else
        t->kind = kScriptSoundNormal;

    t->sfx = sfxdefs.GetEffect(pars[1]);

    t->x = this_script->x;
    t->y = this_script->y;
    t->z = (this_script->rad_z < 0) ? kOnFloorZ : this_script->z;

    if (pars.size() >= 4)
    {
        ScriptCheckForFloat(pars[2], &t->x);
        ScriptCheckForFloat(pars[3], &t->y);
    }

    if (pars.size() >= 5)
    {
        ScriptCheckForFloat(pars[4], &t->z);
    }

    AddStateToScript(this_script, 0, ScriptPlaySound, t);
}

static void ScriptParseKillSound(std::vector<const char *> &pars)
{
    // KillSound
    EPI_UNUSED(pars);
    AddStateToScript(this_script, 0, ScriptKillSound, nullptr);
}

static void ScriptParseChangeMusic(std::vector<const char *> &pars)
{
    // ChangeMusic <playlist num>

    ScriptMusicParameter *music = new ScriptMusicParameter;

    ScriptCheckForInt(pars[1], &music->playnum);

    music->looping = true;

    AddStateToScript(this_script, 0, ScriptChangeMusic, music);
}

static void ScriptParsePlayMovie(std::vector<const char *> &pars)
{
    // PlayMovie <lump or packfile name>

    EPI_ASSERT(pars[1]);

    ScriptMovieParameter *mov = new ScriptMovieParameter;

    mov->movie = pars[1];

    AddStateToScript(this_script, 0, ScriptPlayMovie, mov);
}

static void ScriptParseDamagePlayer(std::vector<const char *> &pars)
{
    // DamagePlayer <amount>

    ScriptDamagePlayerParameter *t = new ScriptDamagePlayerParameter;

    ScriptCheckForFloat(pars[1], &t->damage_amount);

    AddStateToScript(this_script, 0, ScriptDamagePlayers, t);
}

// FIXME: use the benefit system
static void ScriptParseHealPlayer(std::vector<const char *> &pars)
{
    // HealPlayer <amount>
    // HealPlayer <amount> <limit>

    ScriptHealParameter *heal = new ScriptHealParameter;

    ScriptCheckForFloat(pars[1], &heal->heal_amount);

    if (pars.size() < 3)
        heal->limit = kMaximumHealth;
    else
        ScriptCheckForFloat(pars[2], &heal->limit);

    if (heal->limit < 0 || heal->limit > kMaximumHealth)
        ScriptError("Health limit out of range: %1.1f\n", heal->limit);

    if (heal->heal_amount < 0 || heal->heal_amount > heal->limit)
        ScriptError("Health value out of range: %1.1f\n", heal->heal_amount);

    AddStateToScript(this_script, 0, ScriptHealPlayers, heal);
}

// FIXME: use the benefit system
static void ScriptParseGiveArmour(std::vector<const char *> &pars)
{
    // GiveArmour <type> <amount>
    // GiveArmour <type> <amount> <limit>

    ScriptArmourParameter *armour = new ScriptArmourParameter;

    armour->type = ScriptCheckForArmourType(pars[1]);

    ScriptCheckForFloat(pars[2], &armour->armour_amount);

    if (pars.size() < 4)
        armour->limit = kMaximumArmor;
    else
        ScriptCheckForFloat(pars[3], &armour->limit);

    if (armour->limit < 0 || armour->limit > kMaximumArmor)
        ScriptError("Armour limit out of range: %1.1f\n", armour->limit);

    if (armour->armour_amount < 0 || armour->armour_amount > armour->limit)
        ScriptError("Armour value out of range: %1.1f\n", armour->armour_amount);

    AddStateToScript(this_script, 0, ScriptArmourPlayers, armour);
}

static void ScriptParseGiveLoseBenefit(std::vector<const char *> &pars)
{
    // Give_Benefit  <benefit>
    //   or
    // Lose_Benefit  <benefit>

    ScriptBenefitParameter *sb = new ScriptBenefitParameter;

    if (DDFCompareName(pars[0], "LOSE_BENEFIT") == 0)
        sb->lose_it = true;

    DDFMobjGetBenefit(pars[1], &sb->benefit);

    AddStateToScript(this_script, 0, ScriptBenefitPlayers, sb);
}

static void ScriptParseDamageMonsters(std::vector<const char *> &pars)
{
    // Damage_Monsters <monster> <amount>
    //
    // keyword parameters:
    //   TAG=<num>
    //
    // The monster can be 'ANY' to match all monsters.

    ScriptDamangeMonstersParameter *mon = new ScriptDamangeMonstersParameter;

    // get monster type
    if (pars[1][0] == '-' || pars[1][0] == '+' || epi::IsDigitASCII(pars[1][0]))
    {
        ScriptCheckForInt(pars[1], &mon->thing_type);
    }
    else if (DDFCompareName(pars[1], "ANY") == 0)
        mon->thing_type = -1;
    else
        mon->thing_name = epi::CStringDuplicate(pars[1]);

    ScriptCheckForFloat(pars[2], &mon->damage_amount);

    // parse the tag value
    if (pars.size() >= 4)
    {
        if (epi::StringPrefixCaseCompareASCII(pars[3], "TAG=") != 0)
            ScriptError("%s: Bad keyword parameter: %s\n", pars[0], pars[3]);

        ScriptCheckForInt(pars[3] + 4, &mon->thing_tag);
    }

    AddStateToScript(this_script, 0, ScriptDamageMonsters, mon);
}

static void ScriptParseThingEvent(std::vector<const char *> &pars)
{
    // Thing_Event <thing> <label>
    //
    // keyword parameters:
    //   TAG=<num>
    //
    // The thing can be 'ANY' to match all things.

    ScriptThingEventParameter *tev;
    const char                *div;
    int                        i;

    tev = new ScriptThingEventParameter;

    if (pars[1][0] == '-' || pars[1][0] == '+' || epi::IsDigitASCII(pars[1][0]))
        ScriptCheckForInt(pars[1], &tev->thing_type);
    else if (DDFCompareName(pars[1], "ANY") == 0)
        tev->thing_type = -1;
    else
        tev->thing_name = epi::CStringDuplicate(pars[1]);

    // parse the label name
    div = strchr(pars[2], ':');

    i = div ? (div - pars[2]) : strlen(pars[2]);

    if (i <= 0)
        ScriptError("%s: Bad label '%s'.\n", pars[0], pars[2]);

    tev->label = new char[i + 1];
    epi::CStringCopyMax((char *)tev->label, pars[2], i);

    tev->offset = div ? HMM_MAX(0, atoi(div + 1) - 1) : 0;

    // parse the tag value
    if (pars.size() >= 4)
    {
        if (epi::StringPrefixCaseCompareASCII(pars[3], "TAG=") != 0)
            ScriptError("%s: Bad keyword parameter: %s\n", pars[0], pars[3]);

        ScriptCheckForInt(pars[3] + 4, &tev->thing_tag);
    }

    AddStateToScript(this_script, 0, ScriptThingEvent, tev);
}

static void ScriptParseSkill(std::vector<const char *> &pars)
{
    // Skill <skill> <respawn> <fastmonsters>

    ScriptSkillParameter *skill;
    int                   val;

    skill = new ScriptSkillParameter;

    ScriptCheckForInt(pars[1], &val);

    skill->skill        = (SkillLevel)(val - 1);
    skill->respawn      = CheckForBoolean(pars[2]);
    skill->fastmonsters = CheckForBoolean(pars[3]);

    AddStateToScript(this_script, 0, ScriptSkill, skill);
}

static void ScriptParseGotoMap(std::vector<const char *> &pars)
{
    // GotoMap <map>
    // GotoMap <map> SKIP_ALL
    // GotoMap <map> HUB

    ScriptGoToMapParameter *go = new ScriptGoToMapParameter;

    go->map_name = epi::CStringDuplicate(pars[1]);

    if (pars.size() >= 3)
    {
        if (DDFCompareName(pars[2], "SKIP_ALL") == 0)
        {
            go->skip_all = true;
        }
        else
            ScriptWarnError("%s: unknown flag '%s'.\n", pars[0], pars[2]);
    }

    AddStateToScript(this_script, 0, ScriptGotoMap, go);
}

static void ScriptParseHubExit(std::vector<const char *> &pars)
{
    // HubExit <map> <tag>

    ScriptGoToMapParameter *go = new ScriptGoToMapParameter;

    go->is_hub   = true;
    go->map_name = epi::CStringDuplicate(pars[1]);

    ScriptCheckForInt(pars[2], &go->tag);

    AddStateToScript(this_script, 0, ScriptGotoMap, go);
}

static void ScriptParseMoveSector(std::vector<const char *> &pars)
{
    // MoveSector <tag> <amount> <ceil or floor>
    // MoveSector <tag> <amount> <ceil or floor> ABSOLUTE
    //
    // backwards compatibility:
    //   SectorV <sector> <amount> <ceil or floor>

    ScriptMoveSectorParameter *secv;

    secv = new ScriptMoveSectorParameter;

    secv->relative = true;

    ScriptCheckForInt(pars[1], &secv->tag);
    ScriptCheckForFloat(pars[2], &secv->value);

    if (DDFCompareName(pars[3], "FLOOR") == 0)
        secv->is_ceiling = 0;
    else if (DDFCompareName(pars[3], "CEILING") == 0)
        secv->is_ceiling = 1;
    else
        secv->is_ceiling = !CheckForBoolean(pars[3]);

    if (DDFCompareName(pars[0], "SECTORV") == 0)
    {
        secv->secnum = secv->tag;
        secv->tag    = 0;
    }
    else // MOVE_SECTOR
    {
        if (secv->tag == 0)
            ScriptError("%s: Invalid tag number: %d\n", pars[0], secv->tag);

        if (pars.size() >= 5)
        {
            if (DDFCompareName(pars[4], "ABSOLUTE") == 0)
                secv->relative = false;
            else
                ScriptWarnError("%s: expected 'ABSOLUTE' but got '%s'.\n", pars[0], pars[4]);
        }
    }

    AddStateToScript(this_script, 0, ScriptMoveSector, secv);
}

static void ScriptParseLightSector(std::vector<const char *> &pars)
{
    // LightSector <tag> <amount>
    // LightSector <tag> <amount> ABSOLUTE
    //
    // backwards compatibility:
    //   SectorL <sector> <amount>

    ScriptSectorLightParameter *secl;

    secl = new ScriptSectorLightParameter;

    secl->relative = true;

    ScriptCheckForInt(pars[1], &secl->tag);
    ScriptCheckForFloat(pars[2], &secl->value);

    if (DDFCompareName(pars[0], "SECTORL") == 0)
    {
        secl->secnum = secl->tag;
        secl->tag    = 0;
    }
    else // LIGHT_SECTOR
    {
        if (secl->tag == 0)
            ScriptError("%s: Invalid tag number: %d\n", pars[0], secl->tag);

        if (pars.size() >= 4)
        {
            if (DDFCompareName(pars[3], "ABSOLUTE") == 0)
                secl->relative = false;
            else
                ScriptWarnError("%s: expected 'ABSOLUTE' but got '%s'.\n", pars[0], pars[3]);
        }
    }

    AddStateToScript(this_script, 0, ScriptLightSector, secl);
}

static void ScriptParseFogSector(std::vector<const char *> &pars)
{
    // FogSector <tag> <color or SAME or CLEAR> <density(%) or SAME or CLEAR>
    // FogSector <tag> <color or SAME or CLEAR> <density(0-100%) or SAME or
    // CLEAR> ABSOLUTE

    ScriptFogSectorParameter *secf;

    secf = new ScriptFogSectorParameter;

    ScriptCheckForInt(pars[1], &secf->tag);

    if (secf->tag == 0)
        ScriptError("%s: Invalid tag number: %d\n", pars[0], secf->tag);

    if (pars.size() == 4) // color + relative density change
    {
        if (DDFCompareName(pars[2], "SAME") == 0)
            secf->leave_color = true;
        else if (DDFCompareName(pars[2], "CLEAR") == 0)
        { /* nothing - we will use null pointer to denote clearing fog later */
        }
        else
            secf->colmap_color = epi::CStringDuplicate(pars[2]);
        if (DDFCompareName(pars[3], "SAME") == 0)
            secf->leave_density = true;
        else if (DDFCompareName(pars[3], "CLEAR") == 0)
        {
            secf->relative = false;
            secf->density  = 0;
        }
        else
            ScriptCheckForPercentAny(pars[3], &secf->density);
        AddStateToScript(this_script, 0, ScriptFogSector, secf);
    }
    else if (DDFCompareName(pars[4], "ABSOLUTE") == 0) // color + absolute density change
    {
        secf->relative = false;
        if (DDFCompareName(pars[2], "SAME") == 0)
            secf->leave_color = true;
        else if (DDFCompareName(pars[2], "CLEAR") == 0)
        { /* nothing - we will use null pointer to denote clearing fog later */
        }
        else
            secf->colmap_color = epi::CStringDuplicate(pars[2]);
        if (DDFCompareName(pars[3], "SAME") == 0)
            secf->leave_density = true;
        else if (DDFCompareName(pars[3], "CLEAR") == 0)
            secf->density = 0;
        else
            ScriptCheckForPercent(pars[3], &secf->density);
        AddStateToScript(this_script, 0, ScriptFogSector, secf);
    }
    else // shouldn't get here
        ScriptError("%s: Malformed FOG_SECTOR command\n");
}

static void ScriptParseActivateLinetype(std::vector<const char *> &pars)
{
    // Activate_LineType <linetype> <tag>

    ScriptActivateLineParameter *lineact;

    lineact = new ScriptActivateLineParameter;

    ScriptCheckForInt(pars[1], &lineact->typenum);
    ScriptCheckForInt(pars[2], &lineact->tag);

    AddStateToScript(this_script, 0, ScriptActivateLinetype, lineact);
}

static void ScriptParseUnblockLines(std::vector<const char *> &pars)
{
    // Unblock_Lines <tag>

    ScriptLineBlockParameter *lineact;

    lineact = new ScriptLineBlockParameter;

    ScriptCheckForInt(pars[1], &lineact->tag);

    AddStateToScript(this_script, 0, ScriptUnblockLines, lineact);
}

static void ScriptParseBlockLines(std::vector<const char *> &pars)
{
    // Block_Lines <tag>

    ScriptLineBlockParameter *lineact;

    lineact = new ScriptLineBlockParameter;

    ScriptCheckForInt(pars[1], &lineact->tag);

    AddStateToScript(this_script, 0, ScriptBlockLines, lineact);
}

static void ScriptParseWait(std::vector<const char *> &pars)
{
    // Wait <time>

    int tics;

    ScriptCheckForTime(pars[1], &tics);

    if (tics <= 0)
        ScriptError("%s: Invalid time: %d\n", pars[0], tics);

    pending_wait_tics += tics;
}

static void ScriptParseJump(std::vector<const char *> &pars)
{
    // Jump <label>
    // Jump <label> <random chance>

    ScriptJumpParameter *jump = new ScriptJumpParameter;

    jump->label         = epi::CStringDuplicate(pars[1]);
    jump->random_chance = 1.0f;

    if (pars.size() >= 3)
        ScriptCheckForPercent(pars[2], &jump->random_chance);

    AddStateToScript(this_script, 0, ScriptJump, jump);
}

static void ScriptParseSleep(std::vector<const char *> &pars)
{
    // Sleep
    EPI_UNUSED(pars);
    AddStateToScript(this_script, 0, ScriptSleep, nullptr);
}

static void ScriptParseRetrigger(std::vector<const char *> &pars)
{
    // Retrigger

    if (!this_script->tagged_independent)
        ScriptError("%s can only be used with TAGGED_INDEPENDENT.\n", pars[0]);

    AddStateToScript(this_script, 0, ScriptRetrigger, nullptr);
}

static void ScriptParseChangeTex(std::vector<const char *> &pars)
{
    // Change_Tex <where> <texname>
    // Change_Tex <where> <texname> <tag>
    // Change_Tex <where> <texname> <tag> <subtag>

    ScriptChangeTexturetureParameter *ctex;

    if (strlen(pars[2]) > 8)
        ScriptError("%s: Texture name too long: %s\n", pars[0], pars[2]);

    ctex = new ScriptChangeTexturetureParameter;

    ctex->what = ScriptCheckForChangetexType(pars[1]);

    strcpy(ctex->texname, pars[2]);

    if (pars.size() >= 4)
        ScriptCheckForInt(pars[3], &ctex->tag);

    if (pars.size() >= 5)
        ScriptCheckForInt(pars[4], &ctex->subtag);

    AddStateToScript(this_script, 0, ScriptChangeTexture, ctex);
}

static void ScriptParseShowMenu(std::vector<const char *> &pars)
{
    // Show_Menu     <title> <option1> ...
    // Show_Menu_LDF <title> <option1> ...

    ScriptShowMenuParameter *menu = new ScriptShowMenuParameter;

    if (pars.size() > 11)
        ScriptError("%s: too many option strings (limit is 9)\n", pars[0]);

    if (DDFCompareName(pars[0], "SHOW_MENU_LDF") == 0)
        menu->use_ldf = true;

    EPI_ASSERT(2 <= pars.size() && pars.size() <= 11);

    menu->title = ScriptUnquoteString(pars[1]);

    for (size_t p = 2; p < pars.size(); p++)
    {
        menu->options[p - 2] = ScriptUnquoteString(pars[p]);
    }

    AddStateToScript(this_script, 0, ScriptShowMenu, menu);
}

static void ScriptUpdateMenuStyle(std::vector<const char *> &pars)
{
    // Menu_Style  <style>

    ScriptMenuStyle *mm = new ScriptMenuStyle;

    mm->style = ScriptUnquoteString(pars[1]);

    AddStateToScript(this_script, 0, ScriptUpdateMenuStyle, mm);
}

static void ScriptParseJumpOn(std::vector<const char *> &pars)
{
    // Jump_On <VAR> <label1> <label2> ...
    //
    // "MENU" is the only variable supported so far.

    ScriptJumpOnParameter *jump = new ScriptJumpOnParameter;

    if (pars.size() > 11)
        ScriptError("%s: too many labels (limit is 9)\n", pars[0]);

    if (DDFCompareName(pars[1], "MENU") != 0)
    {
        ScriptError("%s: Unknown variable '%s' (should be MENU)\n", pars[0], pars[1]);
    }

    EPI_ASSERT(2 <= pars.size() && pars.size() <= 11);

    for (size_t p = 2; p < pars.size(); p++)
        jump->labels[p - 2] = epi::CStringDuplicate(pars[p]);

    AddStateToScript(this_script, 0, ScriptJumpOn, jump);
}

static void ScriptParseWaitUntilDead(std::vector<const char *> &pars)
{
    // WaitUntilDead <monster> ...

    if (pars.size() - 1 > 10)
        ScriptError("%s: too many monsters (limit is 10)\n", pars[0]);

    static int current_tag = 70000;

    ScriptWaitUntilDeadParameter *wud = new ScriptWaitUntilDeadParameter;

    wud->tag = current_tag;
    current_tag++;

    for (size_t p = 1; p < pars.size(); p++)
        wud->mon_names[p - 1] = epi::CStringDuplicate(pars[p]);

    AddStateToScript(this_script, 0, ScriptWaitUntilDead, wud);
}

static void ScriptParseSwitchWeapon(std::vector<const char *> &pars)
{
    // SwitchWeapon <WeaponName>

    char *WeaponName = ScriptUnquoteString(pars[1]);

    ScriptWeaponParameter *weaparg = new ScriptWeaponParameter;

    weaparg->name = WeaponName;

    AddStateToScript(this_script, 0, ScriptSwitchWeapon, weaparg);
}

static void ScriptParseTeleportToStart(std::vector<const char *> &pars)
{
    // TELEPORT_TO_START
    EPI_UNUSED(pars);
    AddStateToScript(this_script, 0, ScriptTeleportToStart, nullptr);
}

// Replace one weapon with another instantly (no up/down states run)
// It doesnt matter if we have the old one currently selected or not.
static void ScriptParseReplaceWeapon(std::vector<const char *> &pars)
{
    // ReplaceWeapon <OldWeaponName> <NewWeaponName>

    char *OldWeaponName = ScriptUnquoteString(pars[1]);
    char *NewWeaponName = ScriptUnquoteString(pars[2]);

    ScriptWeaponReplaceParameter *weaparg = new ScriptWeaponReplaceParameter;

    weaparg->old_weapon = OldWeaponName;
    weaparg->new_weapon = NewWeaponName;

    AddStateToScript(this_script, 0, ScriptReplaceWeapon, weaparg);
}

// If we have the weapon we insta-switch to it and
// go to the STATE we indicated.
static void ScriptParseWeaponEvent(std::vector<const char *> &pars)
{
    // Weapon_Event <weapon> <label>
    //

    ScriptWeaponEventParameter *tev;
    const char                 *div;
    int                         i;

    tev = new ScriptWeaponEventParameter;

    tev->weapon_name = epi::CStringDuplicate(pars[1]);

    // parse the label name
    div = strchr(pars[2], ':');

    i = div ? (div - pars[2]) : strlen(pars[2]);

    if (i <= 0)
        ScriptError("%s: Bad label '%s'.\n", pars[0], pars[2]);

    tev->label = new char[i + 1];
    epi::CStringCopyMax((char *)tev->label, pars[2], i);

    tev->offset = div ? HMM_MAX(0, atoi(div + 1) - 1) : 0;

    AddStateToScript(this_script, 0, ScriptWeaponEvent, tev);
}

// Replace one thing with another.
static void ScriptParseReplaceThing(std::vector<const char *> &pars)
{
    // ReplaceThing <OldThingName> <NewThingName>

    ScriptThingReplaceParameter *thingarg = new ScriptThingReplaceParameter;

    // get old monster name
    if (epi::IsDigitASCII(pars[1][0]))
    {
        ScriptCheckForInt(pars[1], &thingarg->old_thing_type);
    }
    else
        thingarg->old_thing_name = epi::CStringDuplicate(pars[1]);

    // get new monster name
    if (epi::IsDigitASCII(pars[2][0]))
    {
        ScriptCheckForInt(pars[2], &thingarg->new_thing_type);
    }
    else
        thingarg->new_thing_name = epi::CStringDuplicate(pars[2]);

    AddStateToScript(this_script, 0, ScriptReplaceThing, thingarg);
}

//  PARSER TABLE

static const RADScriptParser radtrig_parsers[] = {
    // directives...
    {-1, "#DEFINE", 3, 3, ScriptParseDefine},
    {0, "#VERSION", 2, 2, ScriptParseVersion},
    {0, "#CLEARALL", 1, 1, ScriptParseClearAll},
    {0, "#CLEAR_MAP", 2, 2, ScriptParseClearMap},

    // basics...
    {-1, "START_MAP", 2, 2, ScriptParseStartMap},
    {-1, "RADIUS_TRIGGER", 4, 6, ScriptParseRadiusTrigger},
    {-1, "RECT_TRIGGER", 5, 7, ScriptParseRadiusTrigger},
    {-1, "SECTOR_TRIGGER_TAG", 2, 4, ScriptParseSectorTrigger},
    {-1, "SECTOR_TRIGGER_INDEX", 2, 4, ScriptParseSectorTrigger},
    {-1, "END_SECTOR_TRIGGER", 1, 1, ScriptParseEndRadiusTrigger},
    {-1, "END_RADIUS_TRIGGER", 1, 1, ScriptParseEndRadiusTrigger},
    {-1, "END_MAP", 1, 1, ScriptParseEndMap},

    // properties...
    {2, "NAME", 2, 2, ScriptParseName},
    {2, "TAG", 2, 2, ScriptParseTag},
    {2, "WHEN_APPEAR", 2, 2, ScriptParseWhenAppear},
    {2, "WHEN_PLAYER_NUM", 2, 3, ScriptParseWhenPlayerNum},
    {2, "NET_MODE", 2, 3, ScriptParseNetMode},
    {2, "TAGGED_REPEATABLE", 1, 3, ScriptParseTaggedRepeatable},
    {2, "TAGGED_USE", 1, 1, ScriptParseTaggedUse},
    {2, "TAGGED_INDEPENDENT", 1, 1, ScriptParseTaggedIndependent},
    {2, "TAGGED_IMMEDIATE", 1, 1, ScriptParseTaggedImmediate},
    {2, "TAGGED_PLAYER_SPECIFIC", 1, 1, ScriptParseTaggedPlayerSpecific},
    {2, "TAGGED_DISABLED", 1, 1, ScriptParseTaggedDisabled},
    {2, "TAGGED_PATH", 2, 2, ScriptParseTaggedPath},
    {2, "PATH_EVENT", 2, 2, ScriptParsePathEvent},
    {2, "ONDEATH", 2, 3, ScriptParseOnDeath},
    {2, "ONHEIGHT", 3, 4, ScriptParseOnHeight},
    {2, "ONCEILINGHEIGHT", 3, 4, ScriptParseOnHeight},
    {2, "ONCONDITION", 2, 2, ScriptParseOnCondition},

    // actions...
    {2, "TIP", 2, 5, ScriptParseTip},
    {2, "TIP_LDF", 2, 5, ScriptParseTip},
    {2, "TIP_GRAPHIC", 2, 5, ScriptParseTip},
    {2, "TIP_SLOT", 2, 2, ScriptParseTipSlot},
    {2, "TIP_SET_POS", 3, 4, ScriptParseTipPos},
    {2, "TIP_SET_COLOUR", 2, 3, ScriptParseTipColour},
    {2, "TIP_SET_TRANS", 2, 3, ScriptParseTipTrans},
    {2, "TIP_SET_ALIGN", 2, 2, ScriptParseTipAlign},
    {2, "EXITLEVEL", 1, 2, ScriptParseExitLevel},
    {2, "SECRETEXIT", 1, 2, ScriptParseExitLevel},
    {2, "SPAWNTHING", 2, 22, ScriptParseSpawnThing},
    {2, "SPAWNTHING_AMBUSH", 2, 22, ScriptParseSpawnThing},
    {2, "SPAWNTHING_FLASH", 2, 22, ScriptParseSpawnThing},
    {2, "PLAYSOUND", 2, 5, ScriptParsePlaySound},
    {2, "PLAYSOUND_BOSSMAN", 2, 5, ScriptParsePlaySound},
    {2, "KILLSOUND", 1, 1, ScriptParseKillSound},
    {2, "HEALPLAYER", 2, 3, ScriptParseHealPlayer},
    {2, "GIVEARMOUR", 3, 4, ScriptParseGiveArmour},
    {2, "DAMAGEPLAYER", 2, 2, ScriptParseDamagePlayer},
    {2, "GIVE_BENEFIT", 2, 2, ScriptParseGiveLoseBenefit},
    {2, "LOSE_BENEFIT", 2, 2, ScriptParseGiveLoseBenefit},
    {2, "DAMAGE_MONSTERS", 3, 3, ScriptParseDamageMonsters},
    {2, "THING_EVENT", 3, 4, ScriptParseThingEvent},
    {2, "SKILL", 4, 4, ScriptParseSkill},
    {2, "GOTOMAP", 2, 3, ScriptParseGotoMap},
    {2, "HUB_EXIT", 3, 3, ScriptParseHubExit},
    {2, "MOVE_SECTOR", 4, 5, ScriptParseMoveSector},
    {2, "LIGHT_SECTOR", 3, 4, ScriptParseLightSector},
    {2, "FOG_SECTOR", 4, 5, ScriptParseFogSector},
    {2, "ENABLE_SCRIPT", 2, 2, ScriptParseEnableScript},
    {2, "DISABLE_SCRIPT", 2, 2, ScriptParseEnableScript},
    {2, "ENABLE_TAGGED", 2, 2, ScriptParseEnableTagged},
    {2, "DISABLE_TAGGED", 2, 2, ScriptParseEnableTagged},
    {2, "ACTIVATE_LINETYPE", 3, 3, ScriptParseActivateLinetype},
    {2, "UNBLOCK_LINES", 2, 2, ScriptParseUnblockLines},
    {2, "BLOCK_LINES", 2, 2, ScriptParseBlockLines},
    {2, "WAIT", 2, 2, ScriptParseWait},
    {2, "JUMP", 2, 3, ScriptParseJump},
    {2, "LABEL", 2, 2, ScriptParseLabel},
    {2, "SLEEP", 1, 1, ScriptParseSleep},
    {2, "EXITGAME", 1, 1, ScriptParseExitGame},
    {2, "RETRIGGER", 1, 1, ScriptParseRetrigger},
    {2, "CHANGE_TEX", 3, 5, ScriptParseChangeTex},
    {2, "CHANGE_MUSIC", 2, 2, ScriptParseChangeMusic},
    {2, "PLAY_MOVIE", 2, 2, ScriptParsePlayMovie},
    {2, "SHOW_MENU", 2, 99, ScriptParseShowMenu},
    {2, "SHOW_MENU_LDF", 2, 99, ScriptParseShowMenu},
    {2, "MENU_STYLE", 2, 2, ScriptUpdateMenuStyle},
    {2, "JUMP_ON", 3, 99, ScriptParseJumpOn},
    {2, "WAIT_UNTIL_DEAD", 2, 11, ScriptParseWaitUntilDead},

    {2, "SWITCH_WEAPON", 2, 2, ScriptParseSwitchWeapon},
    {2, "TELEPORT_TO_START", 1, 1, ScriptParseTeleportToStart},
    {2, "REPLACE_WEAPON", 3, 3, ScriptParseReplaceWeapon},
    {2, "WEAPON_EVENT", 3, 3, ScriptParseWeaponEvent},
    {2, "REPLACE_THING", 3, 3, ScriptParseReplaceThing},

    // old crud
    {2, "SECTORV", 4, 4, ScriptParseMoveSector},
    {2, "SECTORL", 3, 3, ScriptParseLightSector},

    // that's all, folks.
    {0, nullptr, 0, 0, nullptr}};

void ScriptParseLine()
{
    std::vector<const char *> pars;

    ScriptTokenizeLine(pars);

    // simply ignore blank lines
    if (pars.empty())
        return;

    for (const RADScriptParser *cur = radtrig_parsers; cur->name != nullptr; cur++)
    {
        const char *cur_name = cur->name;

        if (DDFCompareName(pars[0], cur_name) != 0)
            continue;

        // check level
        if (cur->level >= 0)
        {
            if (cur->level != current_script_level)
            {
                ScriptError("RTS command '%s' used in wrong place "
                            "(found in %s, should be in %s).\n",
                            pars[0], rad_level_names[current_script_level], rad_level_names[cur->level]);

                // NOT REACHED
                return;
            }
        }

        // check number of parameters.  Too many is live-with-able, but
        // not enough is fatal.

        if ((int)pars.size() < cur->minimum_parameters)
            ScriptError("%s: Not enough parameters.\n", cur->name);

        if ((int)pars.size() > cur->maximum_parameters)
            ScriptWarnError("%s: Too many parameters.\n", cur->name);

        // found it, invoke the parser function
        (*cur->parser)(pars);

        ScriptFreeParameters(pars);
        return;
    }

    ScriptWarnError("Unknown primitive: %s\n", pars[0]);

    ScriptFreeParameters(pars);
}

//----------------------------------------------------------------------------

static int ReadScriptLine(const std::string &data, size_t &pos, std::string &out_line)
{
    out_line.clear();

    // reached the end of file?
    size_t limit = data.size();
    if (pos >= limit)
        return 0;

    int real_num = 1;

    while (pos < data.size())
    {
        // ignore carriage returns
        if (data[pos] == '\r')
        {
            pos++;
            continue;
        }

        // reached the end of the line?
        if (data[pos] == '\n')
        {
            pos++;
            break;
        }

        // line concatenation
        if (data[pos] == '\\' && pos + 3 < limit)
        {
            if (data[pos + 1] == '\n' || (data[pos + 1] == '\r' && data[pos + 2] == '\n'))
            {
                pos += (data[pos + 1] == '\n') ? 2 : 3;
                real_num++;
                continue;
            }
        }

        // append current character
        out_line += data[pos];
        pos++;
    }

    return real_num;
}

static void ScriptParserDone()
{
    if (current_script_level >= 2)
        ScriptError("RADIUS_TRIGGER: block not terminated !\n");

    if (current_script_level == 1)
        ScriptError("START_MAP: block not terminated !\n");

    DDFMainFreeDefines();
}

void ReadRADScript(const std::string &data, const std::string &source)
{
    // FIXME store source somewhere, like current_script_filename
    EPI_UNUSED(source);
    LogDebug("RTS: Loading LUMP (size=%d)\n", (int)data.size());

    // WISH: a more helpful filename
    current_script_filename    = "RSCRIPT";
    current_script_line_number = 1;
    current_script_level       = 0;

    size_t pos = 0;

    for (;;)
    {
        int real_num = ReadScriptLine(data, pos, current_script_line);
        if (real_num == 0)
            break;

#if (EDGE_DEBUG_TRIGGER_SCRIPTS)
        LogDebug("RTS LINE: '%s'\n", current_script_line.c_str());
#endif

        ScriptParseLine();

        current_script_line_number += real_num;
    }

    ScriptParserDone();
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
