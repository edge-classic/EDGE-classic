//----------------------------------------------------------------------------
//  EDGE Radius Trigger Parsing
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

#include "i_defs.h"

#include <limits.h>
#include <unordered_map>

#include "str_util.h"

#include "dm_defs.h"
#include "dm_state.h"
#include "g_game.h"
#include "m_argv.h"
#include "m_math.h"
#include "p_local.h"
#include "p_spec.h"
#include "rad_trig.h"
#include "rad_act.h"
#include "r_defs.h"
#include "s_sound.h"
#include "r_modes.h"
#include "w_wad.h"
#include "version.h"

typedef std::vector<const char *> param_set_t;

static std::unordered_map<uint32_t, std::string> parsed_string_tags;

// TODO remove these eventually, use std::string
static char * Z_StrDup(const char *s)
{
	char *dup = strdup(s);
	if (dup == NULL)
		I_Error("out of memory\n");
	return dup;
}
static void Z_StrFree(const char *s)
{
	free((void *)s);
}


typedef struct rts_parser_s
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
	int min_pars, max_pars;

	// parser function
	void (* parser)(param_set_t& pars);
}
rts_parser_t;


static int rad_cur_linenum;
static const char *rad_cur_filename;
static std::string rad_cur_line;

// Determine whether the code blocks are started and terminated.
static int rad_cur_level = 0;

static const char *rad_level_names[3] =
	{ "outer area", "map area", "trigger area" };

// Location of current script
static rad_script_t *this_rad;
static char *this_map = NULL;

// Pending state info for current script
static int pending_wait_tics = 0;
static char *pending_label = NULL;

// Default tip properties (position, colour, etc)
static s_tip_prop_t default_tip_props =
{ -1, -1, -1, -1, NULL, -1.0f, 0 };

void RAD_Error(const char *err, ...)
{
	va_list argptr;
	char buffer[2048];
	char *pos;

	buffer[2047] = 0;

	// put actual message on first line
	va_start(argptr, err);
	vsprintf(buffer, err, argptr);
	va_end(argptr);

	pos = buffer + strlen(buffer);

	sprintf(pos, "Error occurred near line %d of %s\n", rad_cur_linenum, rad_cur_filename);
	pos += strlen(pos);

	sprintf(pos, "Line contents: %s\n", rad_cur_line.c_str());
	pos += strlen(pos);

	// check for buffer overflow
	if (buffer[2047] != 0)
		I_Error("Buffer overflow in RAD_Error.\n");

	// add a blank line for readability in the log file
	I_Printf("\n");

	I_Error("%s", buffer);
}

void RAD_Warning(const char *err, ...)
{
	if (no_warnings)
		return;

	va_list argptr;
	char buffer[1024];

	va_start(argptr, err);
	vsprintf(buffer, err, argptr);
	va_end(argptr);

	I_Warning("\n");
	I_Warning("Found problem near line %d of %s\n", rad_cur_linenum, rad_cur_filename);
	I_Warning("Line contents: %s\n", rad_cur_line.c_str());
	I_Warning("%s", buffer);
}

void RAD_WarnError(const char *err, ...)
{
	va_list argptr;
	char buffer[1024];

	va_start(argptr, err);
	vsprintf(buffer, err, argptr);
	va_end(argptr);

	if (strict_errors)
		RAD_Error("%s", buffer);
	else
		RAD_Warning("%s", buffer);
}


static void RAD_CheckForInt(const char *value, int *retvalue)
{
	const char *pos = value;
	int count = 0;
	int length = strlen(value);

	if (strchr(value, '%'))
		RAD_Error("Parameter '%s' should not be a percentage.\n", value);

	// Accomodate for "-" as you could have -5 or something like that.
	if (*pos == '-')
	{
		count++;
		pos++;
	}
	while (isdigit(*pos++))
		count++;

	// Is the value an integer?
	if (length != count)
		RAD_Error("Parameter '%s' is not of numeric type.\n", value);

	*retvalue = atoi(value);
}

static void RAD_CheckForFloat(const char *value, float *retvalue)
{
	if (strchr(value, '%'))
		RAD_Error("Parameter '%s' should not be a percentage.\n", value);

	if (sscanf(value, "%f", retvalue) != 1)
		RAD_Error("Parameter '%s' is not of numeric type.\n", value);
}

//
// RAD_CheckForPercent
//
// Reads percentages (0%..100%).
//
static void RAD_CheckForPercent(const char *info, void *storage)
{
	char s[101];
	char *p;
	float f;

	// just check that the string is valid
	Z_StrNCpy(s, info, 100);
	for (p = s; isdigit(*p) || *p == '.'; p++)
	{ /* nothing here */ }

	// the number must be followed by %
	if (*p != '%')
		RAD_Error("Parameter '%s' is not of percent type.\n", info);
	*p = 0;

	RAD_CheckForFloat(s, &f);
	if (f < 0.0f || f > 100.0f)
		RAD_Error("Percentage out of range: %s\n", info);

	*(percent_t *)storage = f / 100.0f;
}

//
// RAD_CheckForPercentAny
//
// Like the above routine, but don't limit to 0..100%.
//
static void RAD_CheckForPercentAny(const char *info, void *storage)
{
	char s[101];
	char *p;
	float f;

	// just check that the string is valid
	Z_StrNCpy(s, info, 100);
	for (p = s; isdigit(*p) || *p == '-' || *p == '.'; p++)
	{ /* nothing here */ }

	// the number must be followed by %
	if (*p != '%')
		RAD_Error("Parameter '%s' is not of percent type.\n", info);
	*p = 0;

	RAD_CheckForFloat(s, &f);

	*(percent_t *)storage = f / 100.0f;
}

// -ES- Copied from DDF_MainGetTime.
// FIXME: Collect all functions that are common to DDF and RTS,
// and move them to a new module for RTS+DDF common code.
static void RAD_CheckForTime(const char *info, void *storage)
{
	int *dest = (int *)storage;

	SYS_ASSERT(info && storage);

	// -ES- 1999/09/14 MAXT means that time should be maximal.
	if (!epi::case_cmp(info, "maxt"))
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

		RAD_CheckForInt(temp.c_str(), (int*)storage);
		return;
	}

	float val;
	if (sscanf(info, "%f", &val) != 1)
	{
		I_Warning("Bad time value '%s'.\n", info);
		return;
	}

	*dest = I_ROUND(val * (float)TICRATE);
}

static armour_type_e RAD_CheckForArmourType(const char *info)
{
	if (DDF_CompareName(info, "GREEN") == 0)
		return ARMOUR_Green;
	if (DDF_CompareName(info, "BLUE") == 0)
		return ARMOUR_Blue;
	if (DDF_CompareName(info, "PURPLE") == 0)
		return ARMOUR_Purple;
	if (DDF_CompareName(info, "YELLOW") == 0)
		return ARMOUR_Yellow;
	if (DDF_CompareName(info, "RED") == 0)
		return ARMOUR_Red;

	// this never returns
	RAD_Error("Unknown armour type: %s\n", info);
	return ARMOUR_Green;
}

static changetex_type_e RAD_CheckForChangetexType(const char *info)
{
	if (DDF_CompareName(info, "LEFT_UPPER") == 0 || DDF_CompareName(info, "BACK_UPPER") == 0)
		return CHTEX_LeftUpper;
	if (DDF_CompareName(info, "LEFT_MIDDLE") == 0 || DDF_CompareName(info, "BACK_MIDDLE") == 0)
		return CHTEX_LeftMiddle;
	if (DDF_CompareName(info, "LEFT_LOWER") == 0 || DDF_CompareName(info, "BACK_LOWER") == 0)
		return CHTEX_LeftLower;
	if (DDF_CompareName(info, "RIGHT_UPPER" ) == 0 || DDF_CompareName(info, "FRONT_UPPER") == 0)
		return CHTEX_RightUpper;
	if (DDF_CompareName(info, "RIGHT_MIDDLE") == 0 || DDF_CompareName(info, "FRONT_MIDDLE") == 0)
		return CHTEX_RightMiddle;
	if (DDF_CompareName(info, "RIGHT_LOWER") == 0  || DDF_CompareName(info, "FRONT_LOWER") == 0)
		return CHTEX_RightLower;
	if (DDF_CompareName(info, "FLOOR") == 0)
		return CHTEX_Floor;
	if (DDF_CompareName(info, "CEILING") == 0)
		return CHTEX_Ceiling;
	if (DDF_CompareName(info, "SKY") == 0)
		return CHTEX_Sky;

	// this never returns
	RAD_Error("Unknown ChangeTex type '%s'\n", info);
	return CHTEX_RightUpper;
}

//
// RAD_UnquoteString
//
// Remove the quotes from the given string, returning a newly
// allocated string.
//
static char *RAD_UnquoteString(const char *s)
{
	if (s[0] != '"')
		return Z_StrDup(s);

	// skip initial quote
	s++;

	std::string new_str;

	while (s[0] != '"')
	{
#ifdef DEVELOPERS
		if (s[0] == 0)
			I_Error("INTERNAL ERROR: bad string.\n");
#endif

		// -AJA- 1999/09/07: check for \n
		if (s[0] == '\\' && tolower(s[1]) == 'n')
		{
			new_str += '\n';
			s += 2;
			continue;
		}

		new_str += s[0];
		s += 1;
	}

	return Z_StrDup(new_str.c_str());
}

static bool CheckForBoolean(const char *s)
{
	if (epi::case_cmp(s, "TRUE") == 0 || epi::case_cmp(s, "1") == 0)
		return true;

	if (epi::case_cmp(s, "FALSE") == 0 || epi::case_cmp(s, "0") == 0)
		return false;

	// Nope, it's an error.
	RAD_Error("Bad boolean value (should be TRUE or FALSE): %s\n", s);
	return false;
}

#if 0 // UNUSED
static void DoParsePlayerSet(const char *info, u32_t *set)
{
	const char *p = info;
	const char *next;

	*set = 0;

	if (DDF_CompareName(info, "ALL") == 0)
	{
		*set = ~0;
		return;
	}

	for (;;)
	{
		if (! isdigit(p[0]))
			RAD_Error("Bad number in set of players: %s\n", info);

		int num = strtol(p, (char **) &next, 10);

		*set |= (1 << (num-1));

		p = next;

		if (p[0] == 0)
			break;

		if (p[0] != ':')
			RAD_Error("Missing ':' in set of players: %s\n", info);

		p++;
	}
}
#endif

// AddStateToScript
//
// Adds a new action state to the tail of the current set of states
// for the given radius trigger.
//
static void AddStateToScript(rad_script_t *R, int tics,
							 void (* action)(struct rad_trigger_s *R, void *param), 
							 void *param)
{
	rts_state_t *state = new rts_state_t;

	state->tics = tics;
	state->action = action;
	state->param = param;

	state->tics += pending_wait_tics;
	state->label = pending_label;

	pending_wait_tics = 0;
	pending_label = NULL;

	// link it in
	state->next = NULL;
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
static void ClearOneScript(rad_script_t *scr)
{
	if (scr->mapid)
	{
		Z_StrFree(scr->mapid);
		scr->mapid = nullptr;
	}

	while (scr->boss_trig)
	{
		s_ondeath_t *cur = scr->boss_trig;
		scr->boss_trig = cur->next;

		delete cur;
	}

	while (scr->height_trig)
	{
		s_onheight_t *cur = scr->height_trig;
		scr->height_trig = cur->next;

		delete cur;
	}

	// free all states
	while (scr->first_state)
	{
		rts_state_t *cur = scr->first_state;
		scr->first_state = cur->next;

		if (cur->param)
			delete cur->param;

		delete cur;
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
	if (DDF_CompareName(mapid, "ALL") == 0)
		return;

	rad_script_t *scr, *next;

	for (scr=r_scripts; scr; scr=next)
	{
		next = scr->next;

		if (epi::case_cmp(scr->mapid, mapid) == 0)
		{
			// unlink and free it
			if (scr->next)
				scr->next->prev = scr->prev;

			if (scr->prev)
				scr->prev->next = scr->next;
			else
				r_scripts = scr->next;

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
	while (r_scripts)
	{
		rad_script_t *scr = r_scripts;
		r_scripts = scr->next;

		ClearOneScript(scr);

		delete scr;
	}
}

//
// RAD_ComputeScriptCRC
//
static void RAD_ComputeScriptCRC(rad_script_t *scr)
{
	scr->crc.Reset();

	// Note: the mapid doesn't belong in the CRC

	if (scr->script_name)
		scr->crc.AddCStr(scr->script_name);

	scr->crc += (int) scr->tag[0];
	scr->crc += (int) scr->tag[1];
	scr->crc += (int) scr->appear;
	scr->crc += (int) scr->min_players;
	scr->crc += (int) scr->max_players;
	scr->crc += (int) scr->repeat_count;

	scr->crc += (int) I_ROUND(scr->x);
	scr->crc += (int) I_ROUND(scr->y);
	scr->crc += (int) I_ROUND(scr->z);
	scr->crc += (int) I_ROUND(scr->rad_x);
	scr->crc += (int) I_ROUND(scr->rad_y);
	scr->crc += (int) I_ROUND(scr->rad_z);

	// lastly handle miscellaneous parts

#undef M_FLAG
#define M_FLAG(bit, cond)  \
	if cond { flags |= (1 << (bit)); }

	int flags = 0;

	M_FLAG(0, (scr->tagged_disabled));
	M_FLAG(1, (scr->tagged_use));
	M_FLAG(2, (scr->tagged_independent));
	M_FLAG(3, (scr->tagged_immediate));

	M_FLAG(4, (scr->boss_trig != NULL));
	M_FLAG(5, (scr->height_trig != NULL));
	M_FLAG(6, (scr->cond_trig != NULL));
	M_FLAG(7, (scr->next_in_path != NULL));

	scr->crc += (int) flags;

	// Q/ add in states ?  
	// A/ Nah.
}

#undef M_FLAG

// RAD_TokenizeLine
//
// Collect the parameters from the line into an array of strings
// 'pars', which can hold at most 'max' string pointers.
// 
// -AJA- 2000/01/02: Moved #define handling to here.
//
static void RAD_TokenizeLine(param_set_t& pars)
{
	const char *line = rad_cur_line.c_str();

	std::string tokenbuf;

	bool want_token = true;
	bool in_string  = false;
	int  in_expr    = 0;  // add one for each open bracket.

	for (;;)
	{
		int ch = *line++;

		bool comment = (ch == ';' || (ch == '/' && *line == '/'));

		if (in_string)
			comment = false;

		if (ch == 0 && in_string)
			RAD_Error("Nonterminated string found.\n");

		if ((ch == 0 || comment) && in_expr)
			RAD_Error("Nonterminated expression found.\n");

		if (want_token)  // looking for a new token
		{
			SYS_ASSERT(!in_expr && !in_string);

			// end of line ?
			if (ch == 0 || comment)
				return;

			if (isspace(ch))
				continue;

			// string ? or expression ?
			if (ch == '"')
				in_string = true;
			else if (ch == '(' && ! in_string)
				in_expr++;
			else if (ch == ')' && ! in_string)
				RAD_Error("Unmatched ')' bracket found\n");

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

			if (! in_expr)
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
		else if (!in_expr && !in_string && (ch == 0 || comment || isspace(ch)))
		{
			end_token = true;
		}

		// end of token ?
		if (! end_token)
		{
			tokenbuf += ch;
			continue;
		}

		want_token = true;

		// check for defines
		const char *par_str = Z_StrDup(DDF_MainGetDefine(tokenbuf.c_str()));

		pars.push_back(par_str);

		// end of line ?
		if (ch == 0 || comment)
			break;
	}
}

// RAD_FreeParameters
//
// Free previously collected parameters.
// 
static void RAD_FreeParameters(param_set_t& pars)
{
	for (size_t i = 0 ; i < pars.size() ; i++)
	{
		Z_StrFree(pars[i]);
	}
}


// ---- Primitive Parsers ----------------------------------------------

static void RAD_ParseVersion(param_set_t& pars)
{
	// ignored for compatibility
}

static void RAD_ParseClearAll(param_set_t& pars)
{
	// #ClearAll

	ClearAllScripts();
}

static void RAD_ParseDefine(param_set_t& pars)
{
	// #Define <identifier> <num>

	DDF_MainAddDefine(pars[1], pars[2]);
}

static void RAD_ParseStartMap(param_set_t& pars)
{
	// Start_Map <map>

	if (rad_cur_level != 0)
		RAD_Error("%s found, but previous END_MAP missing !\n", pars[0]);

	// -AJA- 1999/08/02: New scripts replace old ones.
	ClearPreviousScripts(pars[1]);

	this_map = Z_StrDup(pars[1]);
	for (size_t i=0; i < strlen(this_map); i++) {
		this_map[i] = toupper(this_map[i]);
	}

	rad_cur_level++;
}

static void RAD_ParseRadiusTrigger(param_set_t& pars)
{
	// RadiusTrigger <x> <y> <radius>
	// RadiusTrigger <x> <y> <radius> <low z> <high z>
	//
	// RectTrigger <x1> <y1> <x2> <y2>
	// RectTrigger <x1> <y1> <x2> <y2> <z1> <z2>

	// -AJA- 1999/09/12: Reworked for having Z-restricted triggers.

	if (rad_cur_level == 2)
		RAD_Error("%s found, but previous END_RADIUS_TRIGGER missing !\n",
		pars[0]);

	if (rad_cur_level == 0)
		RAD_Error("%s found, but without any START_MAP !\n", pars[0]);

	// Set the node up,..

	this_rad = new rad_script_t;

	// set defaults
	this_rad->x = 0;
	this_rad->y = 0;
	this_rad->z = 0;
	this_rad->rad_x = -1;
	this_rad->rad_y = -1;
	this_rad->rad_z = -1;
	this_rad->appear = DEFAULT_APPEAR;
	this_rad->min_players = 0;
	this_rad->max_players = MAXPLAYERS;
	this_rad->absolute_req_players = 1;
	this_rad->repeat_count = -1;
	this_rad->repeat_delay = 0;

	pending_wait_tics = 0;
	pending_label = NULL;

	if (DDF_CompareName("RECT_TRIGGER", pars[0]) == 0)
	{
		float x1, y1, x2, y2, z1, z2;

		if (pars.size() == 6)
			RAD_Error("%s: Wrong number of parameters.\n", pars[0]);

		RAD_CheckForFloat(pars[1], &x1);
		RAD_CheckForFloat(pars[2], &y1);
		RAD_CheckForFloat(pars[3], &x2);
		RAD_CheckForFloat(pars[4], &y2);

		if (x1 > x2)
			RAD_WarnError("%s: bad X range %1.1f to %1.1f\n", pars[0], x1, x2);
		if (y1 > y2)
			RAD_WarnError("%s: bad Y range %1.1f to %1.1f\n", pars[0], y1, y2);

		this_rad->x = (float)(x1 + x2) / 2.0f;
		this_rad->y = (float)(y1 + y2) / 2.0f;
		this_rad->rad_x = (float)fabs(x1 - x2) / 2.0f;
		this_rad->rad_y = (float)fabs(y1 - y2) / 2.0f;

		if (pars.size() >= 7)
		{
			RAD_CheckForFloat(pars[5], &z1);
			RAD_CheckForFloat(pars[6], &z2);

			if (z1 > z2 + 1)
				RAD_WarnError("%s: bad height range %1.1f to %1.1f\n",
				pars[0], z1, z2);

			this_rad->z = (z1 + z2) / 2.0f;
			this_rad->rad_z = fabs(z1 - z2) / 2.0f;
		}
	}
	else
	{
		if (pars.size() == 5)
			RAD_Error("%s: Wrong number of parameters.\n", pars[0]);

		RAD_CheckForFloat(pars[1], &this_rad->x);
		RAD_CheckForFloat(pars[2], &this_rad->y);
		RAD_CheckForFloat(pars[3], &this_rad->rad_x);

		this_rad->rad_y = this_rad->rad_x;

		if (pars.size() >= 6)
		{
			float z1, z2;

			RAD_CheckForFloat(pars[4], &z1);
			RAD_CheckForFloat(pars[5], &z2);

			if (z1 > z2)
				RAD_WarnError("%s: bad height range %1.1f to %1.1f\n", pars[0], z1, z2);

			this_rad->z = (z1 + z2) / 2.0f;
			this_rad->rad_z = fabs(z1 - z2) / 2.0f;
		}
	}

	// link it in
	this_rad->next = r_scripts;
	this_rad->prev = NULL;

	if (r_scripts)
		r_scripts->prev = this_rad;

	r_scripts = this_rad;

	rad_cur_level++;
}

static void RAD_ParseEndRadiusTrigger(param_set_t& pars)
{
	// End_RadiusTrigger

	if (rad_cur_level != 2)
		RAD_Error("%s found, but without any RADIUS_TRIGGER !\n", pars[0]);

	// --- check stuff ---

	// handle any pending WAIT or LABEL values
	if (pending_wait_tics > 0 || pending_label)
	{
		AddStateToScript(this_rad, 0, RAD_ActNOP, NULL);
	}

	this_rad->mapid = Z_StrDup(this_map);
	RAD_ComputeScriptCRC(this_rad);
	this_rad = NULL;

	rad_cur_level--;
}

static void RAD_ParseEndMap(param_set_t& pars)
{
	// End_Map

	if (rad_cur_level == 2)
		RAD_Error("%s found, but previous END_RADIUS_TRIGGER missing !\n",
		pars[0]);

	if (rad_cur_level == 0)
		RAD_Error("%s found, but without any START_MAP !\n", pars[0]);

	this_map = NULL;

	rad_cur_level--;
}

static void RAD_ParseName(param_set_t& pars)
{
	// Name <name>

	if (this_rad->script_name)
		RAD_Error("Script already has a name: '%s'\n", this_rad->script_name);

	this_rad->script_name = Z_StrDup(pars[1]);
}

static void RAD_ParseTag(param_set_t& pars)
{
	// Tag <number>

	if (this_rad->tag[0] != 0)
		RAD_Error("Script already has a tag: '%d'\n", this_rad->tag[0]);

	if (this_rad->tag[1] != 0)
	{
		if (parsed_string_tags.find(this_rad->tag[1]) != parsed_string_tags.end())
			RAD_Error("Script already has a tag: '%s'\n", parsed_string_tags[this_rad->tag[1]].c_str());
		else
			RAD_Error("Script already has a tag: '%d'\n", this_rad->tag[1]);
	}

	// Modified RAD_CheckForInt
	const char *pos = pars[1];
	int count = 0;
	int length = strlen(pars[1]);

	while (isdigit(*pos++))
		count++;

	// Is the value an integer?
	if (length != count)
	{
		this_rad->tag[1] = epi::STR_Hash32(pars[1]);
		parsed_string_tags.try_emplace(this_rad->tag[1], pars[1]);
	}
	else
		this_rad->tag[0] = atoi(pars[1]);
}

static void RAD_ParseWhenAppear(param_set_t& pars)
{
	// When_Appear 1:2:3:4:5:SP:COOP:DM

	DDF_MainGetWhenAppear(pars[1], &this_rad->appear);
}

static void RAD_ParseWhenPlayerNum(param_set_t& pars)
{
	// When_Player_Num <min> [max]

	RAD_CheckForInt(pars[1], &this_rad->min_players);

	this_rad->max_players = MAXPLAYERS;

	if (pars.size() >= 3)
		RAD_CheckForInt(pars[2], &this_rad->max_players);

	if (this_rad->min_players < 0 || this_rad->min_players > this_rad->max_players)
	{
		RAD_Error("%s: Illegal range: %d..%d\n", pars[0],
			this_rad->min_players, this_rad->max_players);
	}
}

static void RAD_ParseNetMode(param_set_t& pars)
{
	// Net_Mode SEPARATE
	// Net_Mode ABSOLUTE
	//
	// NOTE: IGNORED FOR BACKWARDS COMPATIBILITY
}

static void RAD_ParseTaggedRepeatable(param_set_t& pars)
{
	// Tagged_Repeatable
	// Tagged_Repeatable <num repetitions>
	// Tagged_Repeatable <num repetitions> <delay>

	if (this_rad->repeat_count >= 0)
		RAD_Error("%s: can only be used once.\n", pars[0]);

	if (pars.size() >= 2)
		RAD_CheckForInt(pars[1], &this_rad->repeat_count);
	else
		this_rad->repeat_count = REPEAT_FOREVER;

	// -ES- 2000/03/03 Changed to RAD_CheckForTime.
	if (pars.size() >= 3)
		RAD_CheckForTime(pars[2], &this_rad->repeat_delay);
	else
		this_rad->repeat_delay = 1;
}

static void RAD_ParseTaggedUse(param_set_t& pars)
{
	// Tagged_Use

	this_rad->tagged_use = true;
}

static void RAD_ParseTaggedIndependent(param_set_t& pars)
{
	// Tagged_Independent

	this_rad->tagged_independent = true;
}

static void RAD_ParseTaggedImmediate(param_set_t& pars)
{
	// Tagged_Immediate

	this_rad->tagged_immediate = true;
}

static void RAD_ParseTaggedPlayerSpecific(param_set_t& pars)
{
	// Tagged_Player_Specific

	// NOTE: IGNORED FOR BACKWARDS COMPATIBILITY
}

static void RAD_ParseTaggedDisabled(param_set_t& pars)
{
	// Tagged_Disabled

	this_rad->tagged_disabled = true;
}

static void RAD_ParseTaggedPath(param_set_t& pars)
{
	// Tagged_Path  <next node>

	rts_path_t *path = new rts_path_t;

	path->next = this_rad->next_in_path;
	path->name = Z_StrDup(pars[1]);

	this_rad->next_in_path = path;
	this_rad->next_path_total += 1;
}

static void RAD_ParsePathEvent(param_set_t& pars)
{
	// Path_Event  <label>

	const char *div;
	int i;

	if (this_rad->path_event_label)
		RAD_Error("%s: Can only be used once per trigger.\n", pars[0]);

	// parse the label name
	div = strchr(pars[1], ':');

	i = div ? (div - pars[1]) : strlen(pars[1]);

	if (i <= 0)
		RAD_Error("%s: Bad label '%s'.\n", pars[0], pars[2]);

	this_rad->path_event_label = new char[i+1];
	Z_StrNCpy((char *)this_rad->path_event_label, pars[1], i);

	this_rad->path_event_offset = div ? MAX(0, atoi(div+1) - 1) : 0;
}

static void RAD_ParseOnDeath(param_set_t& pars)
{
	// OnDeath <thing type>
	// OnDeath <thing type> <threshhold>

	s_ondeath_t *cond = new s_ondeath_t;

	// get map thing
	if (pars[1][0] == '-' || pars[1][0] == '+' || isdigit(pars[1][0]))
	{
		RAD_CheckForInt(pars[1], &cond->thing_type);
	}
	else
		cond->thing_name = Z_StrDup(pars[1]);

	if (pars.size() >= 3)
	{
		RAD_CheckForInt(pars[2], &cond->threshhold);
	}

	// link it into list of ONDEATH conditions
	cond->next = this_rad->boss_trig;
	this_rad->boss_trig = cond;
}

static void RAD_ParseOnHeight(param_set_t& pars)
{
	// OnHeight <low Z> <high Z>
	// OnHeight <low Z> <high Z> <sector num>
	//
	// OnCeilingHeight <low Z> <high Z>
	// OnCeilingHeight <low Z> <high Z> <sector num>

	s_onheight_t *cond = new s_onheight_t;

	cond->sec_num = -1;

	RAD_CheckForFloat(pars[1], &cond->z1);
	RAD_CheckForFloat(pars[2], &cond->z2);

	if (cond->z1 > cond->z2)
		RAD_Error("%s: bad height range %1.1f..%1.1f\n", pars[0],
		cond->z1, cond->z2);

	// get sector reference
	if (pars.size() >= 4)
	{
		RAD_CheckForInt(pars[3], &cond->sec_num);
	}

	cond->is_ceil = (DDF_CompareName("ONCEILINGHEIGHT", pars[0]) == 0);

	// link it into list of ONHEIGHT conditions
	cond->next = this_rad->height_trig;
	this_rad->height_trig = cond;
}

static void RAD_ParseOnCondition(param_set_t& pars)
{
	// OnCondition  <condition>

	condition_check_t *cond = new condition_check_t;

	if (! DDF_MainParseCondition(pars[1], cond))
	{
		delete cond;
		return;
	}

	// link it into list of ONCONDITION list
	cond->next = this_rad->cond_trig;
	this_rad->cond_trig = cond;
}

static void RAD_ParseLabel(param_set_t& pars)
{
	// Label <label>

	if (pending_label)
		RAD_Error("State already has a label: '%s'\n", pending_label);

	// handle any pending WAIT value
	if (pending_wait_tics > 0)
		AddStateToScript(this_rad, 0, RAD_ActNOP, NULL);

	pending_label = Z_StrDup(pars[1]);
}

static void RAD_ParseEnableScript(param_set_t& pars)
{
	// Enable_Script  <script name>
	// Disable_Script <script name>

	s_enabler_t *t = new s_enabler_t;

	t->script_name = Z_StrDup(pars[1]);
	t->new_disabled = DDF_CompareName("DISABLE_SCRIPT", pars[0]) == 0;

	AddStateToScript(this_rad, 0, RAD_ActEnableScript, t);
}

static void RAD_ParseEnableTagged(param_set_t& pars)
{
	// Enable_Tagged  <tag num>
	// Disable_Tagged <tag num>

	s_enabler_t * t = new s_enabler_t;

	// Modified RAD_CheckForInt
	const char *pos = pars[1];
	int count = 0;
	int length = strlen(pars[1]);

	while (isdigit(*pos++))
		count++;

	// Is the value an integer?
	if (length != count)
		t->tag[1] = epi::STR_Hash32(pars[1]);
	else
		t->tag[0] = atoi(pars[1]);

	//if (t->tag <= 0)
		//RAD_Error("Bad tag value: %s\n", pars[1]);

	t->new_disabled = DDF_CompareName("DISABLE_TAGGED", pars[0]) == 0;

	AddStateToScript(this_rad, 0, RAD_ActEnableScript, t);
}

static void RAD_ParseExitLevel(param_set_t& pars)
{
	// ExitLevel
	// ExitLevel <wait time>
	//
	// SecretExit
	// SecretExit <wait time>

	s_exit_t *exit = new s_exit_t;

	exit->exittime = 10;
	exit->is_secret = DDF_CompareName("SECRETEXIT", pars[0]) == 0;

	if (pars.size() >= 2)
	{
		RAD_CheckForTime(pars[1], &exit->exittime);
	}

	AddStateToScript(this_rad, 0, RAD_ActExitLevel, exit);
}

//Lobo November 2021
static void RAD_ParseExitGame(param_set_t& pars)
{
	// ExitGame to TitleScreen

	AddStateToScript(this_rad, 0, RAD_ActExitGame, NULL);
}

static void RAD_ParseTip(param_set_t& pars)
{
	// Tip "<text>"
	// Tip "<text>" <time>
	// Tip "<text>" <time> <has sound>
	// Tip "<text>" <time> <has sound> <scale>
	//
	// (likewise for Tip_LDF)
	// (likewise for Tip_Graphic)

	s_tip_t *tip = new s_tip_t;

	tip->display_time = 3 * TICRATE;
	tip->playsound = false;
	tip->gfx_scale = 1.0f;

	if (DDF_CompareName(pars[0], "TIP_GRAPHIC") == 0)
	{
		tip->tip_graphic = Z_StrDup(pars[1]);
	}
	else if (DDF_CompareName(pars[0], "TIP_LDF") == 0)
	{
		tip->tip_ldf = Z_StrDup(pars[1]);
	}
	else if (pars[1][0] == '"')
	{
		tip->tip_text = RAD_UnquoteString(pars[1]);
	}
	else
		RAD_Error("Needed string for TIP command.\n");

	if (pars.size() >= 3)
		RAD_CheckForTime(pars[2], &tip->display_time);

	if (pars.size() >= 4)
		tip->playsound = CheckForBoolean(pars[3]);

	if (pars.size() >= 5)
	{
		/*if (! tip->tip_graphic)
			RAD_Error("%s: scale value only works with TIP_GRAPHIC.\n", pars[0]);
		*/
		RAD_CheckForFloat(pars[4], &tip->gfx_scale);
	}

	AddStateToScript(this_rad, 0, RAD_ActTip, tip);
}

static void RAD_ParseTipSlot(param_set_t& pars)
{
	// Tip_Slot <slotnum>

	s_tip_prop_t *tp;

	tp = new s_tip_prop_t;
	tp[0] = default_tip_props;

	RAD_CheckForInt(pars[1], &tp->slot_num);

	if (tp->slot_num < 1 || tp->slot_num > MAXTIPSLOT)
		RAD_Error("Bad tip slot '%d' -- must be between 1-%d\n",
		tp->slot_num, MAXTIPSLOT);

	tp->slot_num--;

	AddStateToScript(this_rad, 0, RAD_ActTipProps, tp);
}

static void RAD_ParseTipPos(param_set_t& pars)
{
	// Tip_Set_Pos <x> <y>
	// Tip_Set_Pos <x> <y> <time>

	s_tip_prop_t *tp;

	tp = new s_tip_prop_t;
	tp[0] = default_tip_props;

	RAD_CheckForPercentAny(pars[1], &tp->x_pos);
	RAD_CheckForPercentAny(pars[2], &tp->y_pos);

	if (pars.size() >= 4)
		RAD_CheckForTime(pars[3], &tp->time);

	AddStateToScript(this_rad, 0, RAD_ActTipProps, tp);
}

static void RAD_ParseTipColour(param_set_t& pars)
{
	// Tip_Set_Colour <color>
	// Tip_Set_Colour <color> <time>

	s_tip_prop_t *tp;

	tp = new s_tip_prop_t;
	tp[0] = default_tip_props;

	tp->color_name = Z_StrDup(pars[1]);

	if (pars.size() >= 3)
		RAD_CheckForTime(pars[2], &tp->time);

	AddStateToScript(this_rad, 0, RAD_ActTipProps, tp);
}

static void RAD_ParseTipTrans(param_set_t& pars)
{
	// Tip_Set_Trans <translucency>
	// Tip_Set_Trans <translucency> <time>

	s_tip_prop_t *tp;

	tp = new s_tip_prop_t;
	tp[0] = default_tip_props;

	RAD_CheckForPercent(pars[1], &tp->translucency);

	if (pars.size() >= 3)
		RAD_CheckForTime(pars[2], &tp->time);

	AddStateToScript(this_rad, 0, RAD_ActTipProps, tp);
}

static void RAD_ParseTipAlign(param_set_t& pars)
{
	// Tip_Set_Align  CENTER/LEFT

	s_tip_prop_t *tp;

	tp = new s_tip_prop_t;
	tp[0] = default_tip_props;

	if (DDF_CompareName(pars[1], "CENTER") == 0 ||
		DDF_CompareName(pars[1], "CENTRE") == 0)
	{
		tp->left_just = 0;
	}
	else if (DDF_CompareName(pars[1], "LEFT") == 0)
	{
		tp->left_just = 1;
	}
	else
	{
		RAD_WarnError("TIP_POS: unknown justify method '%s'\n", pars[1]);
	}

	AddStateToScript(this_rad, 0, RAD_ActTipProps, tp);
}

static void HandleSpawnKeyword(const char *par, s_thing_t *t)
{
	if (epi::prefix_case_cmp(par, "X=") == 0)
		RAD_CheckForFloat(par+2, &t->x);
	else if (epi::prefix_case_cmp(par, "Y=") == 0)
		RAD_CheckForFloat(par+2, &t->y);
	else if (epi::prefix_case_cmp(par, "Z=") == 0)
		RAD_CheckForFloat(par+2, &t->z);
	else if (epi::prefix_case_cmp(par, "TAG=") == 0)
		RAD_CheckForInt(par+4, &t->tag);
	else if (epi::prefix_case_cmp(par, "ANGLE=") == 0)
	{
		int val;
		RAD_CheckForInt(par+6, &val);

		if (ABS(val) <= 360)
			t->angle = FLOAT_2_ANG((float) val);
		else
			t->angle = val << 16;
	}
	else if (epi::prefix_case_cmp(par, "SLOPE=") == 0)
	{
		RAD_CheckForFloat(par+6, &t->slope);
		t->slope /= 45.0f;
	}
	else if (epi::prefix_case_cmp(par, "WHEN=") == 0)
	{
		DDF_MainGetWhenAppear(par+5, &t->appear);
	}
	else
	{
		RAD_Error("SPAWN_THING: unknown keyword parameter: %s\n", par);
	}
}

static void RAD_ParseSpawnThing(param_set_t& pars)
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
	// -ACB- 1998/08/06 Use mobjtype_c linked list
	// -AJA- 1999/09/11: Extra fields for Z and slope.

	// -AJA- 1999/09/11: Reworked for spawning things at Z.

	s_thing_t *t = new s_thing_t;

	// set defaults
	t->x = this_rad->x;
	t->y = this_rad->y;

	if (this_rad->rad_z < 0)
		t->z = ONFLOORZ;
	else
		t->z = this_rad->z - this_rad->rad_z;

	t->appear = DEFAULT_APPEAR;

	t->ambush = DDF_CompareName("SPAWNTHING_AMBUSH", pars[0]) == 0;
	t->spawn_effect = DDF_CompareName("SPAWNTHING_FLASH", pars[0]) == 0;

	// get map thing
	if (pars[1][0] == '-' || pars[1][0] == '+' || isdigit(pars[1][0]))
	{
		RAD_CheckForInt(pars[1], &t->thing_type);
	}
	else
		t->thing_name = Z_StrDup(pars[1]);

	// handle keyword parameters
	while (pars.size() >= 3 && strchr(pars.back(),'=') != NULL)
	{
		HandleSpawnKeyword(pars.back(), t);
		pars.pop_back();
	}

	// get angle
	const char *angle_str = (pars.size() == 3) ? pars[2] : (pars.size() >= 5) ? pars[4] : NULL;

	if (angle_str) 
	{
		int val;

		RAD_CheckForInt(angle_str, &val);

		if (ABS(val) <= 360)
			t->angle = FLOAT_2_ANG((float) val);
		else
			t->angle = val << 16;
	}

	// check for x, y, z, slope

	if (pars.size() >= 4)
	{
		RAD_CheckForFloat(pars[2], &t->x);
		RAD_CheckForFloat(pars[3], &t->y);
	}
	if (pars.size() >= 6)
	{
		RAD_CheckForFloat(pars[5], &t->z);
	}
	if (pars.size() >= 7)
	{
		RAD_CheckForFloat(pars[6], &t->slope);

		// FIXME: Merge with DDF_MainGetSlope someday.
		t->slope /= 45.0f;
	}

	AddStateToScript(this_rad, 0, RAD_ActSpawnThing, t);
}

static void RAD_ParsePlaySound(param_set_t& pars)
{
	// PlaySound <soundid>
	// PlaySound <soundid> <x> <y>
	// PlaySound <soundid> <x> <y> <z>
	//
	// (likewise for PlaySound_BossMan)
	//
	// -AJA- 1999/09/12: Reworked for playing sound at specific Z.

	if (pars.size() == 3)
		RAD_Error("%s: Wrong number of parameters.\n", pars[0]);

	s_sound_t *t = new s_sound_t;

	if (DDF_CompareName(pars[0], "PLAYSOUND_BOSSMAN") == 0)
		t->kind = PSOUND_BossMan;
	else
		t->kind = PSOUND_Normal;

	t->sfx = sfxdefs.GetEffect(pars[1]);

	t->x = this_rad->x;
	t->y = this_rad->y;
	t->z = (this_rad->rad_z < 0) ? ONFLOORZ : this_rad->z;

	if (pars.size() >= 4)
	{
		RAD_CheckForFloat(pars[2], &t->x);
		RAD_CheckForFloat(pars[3], &t->y);
	}

	if (pars.size() >= 5)
	{
		RAD_CheckForFloat(pars[4], &t->z);
	}

	AddStateToScript(this_rad, 0, RAD_ActPlaySound, t);
}

static void RAD_ParseKillSound(param_set_t& pars)
{
	// KillSound

	AddStateToScript(this_rad, 0, RAD_ActKillSound, NULL);
}

static void RAD_ParseChangeMusic(param_set_t& pars)
{
	// ChangeMusic <playlist num>

	s_music_t *music = new s_music_t;

	RAD_CheckForInt(pars[1], &music->playnum);

	music->looping = true;

	AddStateToScript(this_rad, 0, RAD_ActChangeMusic, music);
}

static void RAD_ParseDamagePlayer(param_set_t& pars)
{
	// DamagePlayer <amount>

	s_damagep_t *t = new s_damagep_t;

	RAD_CheckForFloat(pars[1], &t->damage_amount);

	AddStateToScript(this_rad, 0, RAD_ActDamagePlayers, t);
}

// FIXME: use the benefit system
static void RAD_ParseHealPlayer(param_set_t& pars)
{
	// HealPlayer <amount>
	// HealPlayer <amount> <limit>

	s_healp_t *heal = new s_healp_t;

	RAD_CheckForFloat(pars[1], &heal->heal_amount);

	if (pars.size() < 3)
		heal->limit = MAXHEALTH;
	else
		RAD_CheckForFloat(pars[2], &heal->limit);

	if (heal->limit < 0 || heal->limit > MAXHEALTH)
		RAD_Error("Health limit out of range: %1.1f\n", heal->limit);

	if (heal->heal_amount < 0 || heal->heal_amount > heal->limit)
		RAD_Error("Health value out of range: %1.1f\n", heal->heal_amount);

	AddStateToScript(this_rad, 0, RAD_ActHealPlayers, heal);
}

// FIXME: use the benefit system
static void RAD_ParseGiveArmour(param_set_t& pars)
{
	// GiveArmour <type> <amount>
	// GiveArmour <type> <amount> <limit>

	s_armour_t *armour = new s_armour_t;

	armour->type = RAD_CheckForArmourType(pars[1]);

	RAD_CheckForFloat(pars[2], &armour->armour_amount);

	if (pars.size() < 4)
		armour->limit = MAXARMOUR;
	else
		RAD_CheckForFloat(pars[3], &armour->limit);

	if (armour->limit < 0 || armour->limit > MAXARMOUR)
		RAD_Error("Armour limit out of range: %1.1f\n", armour->limit);

	if (armour->armour_amount < 0 || armour->armour_amount > armour->limit)
		RAD_Error("Armour value out of range: %1.1f\n", armour->armour_amount);

	AddStateToScript(this_rad, 0, RAD_ActArmourPlayers, armour);
}

static void RAD_ParseGiveLoseBenefit(param_set_t& pars)
{
	// Give_Benefit  <benefit>
	//   or
	// Lose_Benefit  <benefit>

	s_benefit_t *sb = new s_benefit_t;

	if (DDF_CompareName(pars[0], "LOSE_BENEFIT") == 0)
		sb->lose_it = true;

	DDF_MobjGetBenefit(pars[1], &sb->benefit);

	AddStateToScript(this_rad, 0, RAD_ActBenefitPlayers, sb);
}

static void RAD_ParseDamageMonsters(param_set_t& pars)
{
	// Damage_Monsters <monster> <amount>
	//
	// keyword parameters:
	//   TAG=<num>
	//
	// The monster can be 'ANY' to match all monsters.

	s_damage_monsters_t *mon = new s_damage_monsters_t;

	// get monster type
	if (pars[1][0] == '-' || pars[1][0] == '+' || isdigit(pars[1][0]))
	{
		RAD_CheckForInt(pars[1], &mon->thing_type);
	}
	else if (DDF_CompareName(pars[1], "ANY") == 0)
		mon->thing_type = -1;
	else
		mon->thing_name = Z_StrDup(pars[1]);

	RAD_CheckForFloat(pars[2], &mon->damage_amount);

	// parse the tag value
	if (pars.size() >= 4)
	{
		if (epi::prefix_case_cmp(pars[3], "TAG=") != 0)
			RAD_Error("%s: Bad keyword parameter: %s\n", pars[0], pars[3]);

		RAD_CheckForInt(pars[3]+4, &mon->thing_tag);
	}

	AddStateToScript(this_rad, 0, RAD_ActDamageMonsters, mon);
}

static void RAD_ParseThingEvent(param_set_t& pars)
{
	// Thing_Event <thing> <label>
	//
	// keyword parameters:
	//   TAG=<num>
	//
	// The thing can be 'ANY' to match all things.

	s_thing_event_t *tev;
	const char *div;
	int i;

	tev = new s_thing_event_t;

	if (pars[1][0] == '-' || pars[1][0] == '+' || isdigit(pars[1][0]))
		RAD_CheckForInt(pars[1], &tev->thing_type);
	else if (DDF_CompareName(pars[1], "ANY") == 0)
		tev->thing_type = -1;
	else
		tev->thing_name = Z_StrDup(pars[1]);

	// parse the label name
	div = strchr(pars[2], ':');

	i = div ? (div - pars[2]) : strlen(pars[2]);

	if (i <= 0)
		RAD_Error("%s: Bad label '%s'.\n", pars[0], pars[2]);

	tev->label = new char[i+1];
	Z_StrNCpy((char *)tev->label, pars[2], i);

	tev->offset = div ? MAX(0, atoi(div+1) - 1) : 0;

	// parse the tag value
	if (pars.size() >= 4)
	{
		if (epi::prefix_case_cmp(pars[3], "TAG=") != 0)
			RAD_Error("%s: Bad keyword parameter: %s\n", pars[0], pars[3]);

		RAD_CheckForInt(pars[3]+4, &tev->thing_tag);
	}

	AddStateToScript(this_rad, 0, RAD_ActThingEvent, tev);
}

static void RAD_ParseSkill(param_set_t& pars)
{
	// Skill <skill> <respawn> <fastmonsters>

	s_skill_t *skill;
	int val;

	skill = new s_skill_t;

	RAD_CheckForInt(pars[1], &val);

	skill->skill = (skill_t) (val - 1);
	skill->respawn = CheckForBoolean(pars[2]);
	skill->fastmonsters = CheckForBoolean(pars[3]);

	AddStateToScript(this_rad, 0, RAD_ActSkill, skill);
}

static void RAD_ParseGotoMap(param_set_t& pars)
{
	// GotoMap <map>
	// GotoMap <map> SKIP_ALL
	// GotoMap <map> HUB

	s_gotomap_t *go = new s_gotomap_t;

	go->map_name = Z_StrDup(pars[1]);

	if (pars.size() >= 3)
	{
		if (DDF_CompareName(pars[2], "SKIP_ALL") == 0)
		{
			go->skip_all = true;
		}
		else
			RAD_WarnError("%s: unknown flag '%s'.\n",
				pars[0], pars[2]);
	}

	AddStateToScript(this_rad, 0, RAD_ActGotoMap, go);
}

static void RAD_ParseHubExit(param_set_t& pars)
{
	// HubExit <map> <tag>

	s_gotomap_t *go = new s_gotomap_t;

	go->is_hub = true;
	go->map_name = Z_StrDup(pars[1]);

	RAD_CheckForInt(pars[2], &go->tag);

	AddStateToScript(this_rad, 0, RAD_ActGotoMap, go);
}

static void RAD_ParseMoveSector(param_set_t& pars)
{
	// MoveSector <tag> <amount> <ceil or floor>
	// MoveSector <tag> <amount> <ceil or floor> ABSOLUTE
	// 
	// backwards compatibility:
	//   SectorV <sector> <amount> <ceil or floor>

	s_movesector_t *secv;

	secv = new s_movesector_t;

	secv->relative = true;

	RAD_CheckForInt(pars[1], &secv->tag);
	RAD_CheckForFloat(pars[2], &secv->value);

	if (DDF_CompareName(pars[3], "FLOOR") == 0)
		secv->is_ceiling = 0;
	else if (DDF_CompareName(pars[3], "CEILING") == 0)
		secv->is_ceiling = 1;
	else
		secv->is_ceiling = !CheckForBoolean(pars[3]);

	if (DDF_CompareName(pars[0], "SECTORV") == 0)
	{
		secv->secnum = secv->tag;
		secv->tag = 0;
	}
	else  // MOVE_SECTOR
	{
		if (secv->tag == 0)
			RAD_Error("%s: Invalid tag number: %d\n", pars[0], secv->tag);

		if (pars.size() >= 5)
		{
			if (DDF_CompareName(pars[4], "ABSOLUTE") == 0)
				secv->relative = false;
			else
				RAD_WarnError("%s: expected 'ABSOLUTE' but got '%s'.\n",
				pars[0], pars[4]);
		}
	}

	AddStateToScript(this_rad, 0, RAD_ActMoveSector, secv);
}

static void RAD_ParseLightSector(param_set_t& pars)
{
	// LightSector <tag> <amount>
	// LightSector <tag> <amount> ABSOLUTE
	// 
	// backwards compatibility:
	//   SectorL <sector> <amount>

	s_lightsector_t *secl;

	secl = new s_lightsector_t;

	secl->relative = true;

	RAD_CheckForInt(pars[1], &secl->tag);
	RAD_CheckForFloat(pars[2], &secl->value);

	if (DDF_CompareName(pars[0], "SECTORL") == 0)
	{
		secl->secnum = secl->tag;
		secl->tag = 0;
	}
	else  // LIGHT_SECTOR
	{
		if (secl->tag == 0)
			RAD_Error("%s: Invalid tag number: %d\n", pars[0], secl->tag);

		if (pars.size() >= 4)
		{
			if (DDF_CompareName(pars[3], "ABSOLUTE") == 0)
				secl->relative = false;
			else
				RAD_WarnError("%s: expected 'ABSOLUTE' but got '%s'.\n",
				pars[0], pars[3]);
		}
	}

	AddStateToScript(this_rad, 0, RAD_ActLightSector, secl);
}

static void RAD_ParseFogSector(param_set_t& pars)
{
	// FogSector <tag> <color or SAME or CLEAR> <density(%) or SAME or CLEAR>
	// FogSector <tag> <color or SAME or CLEAR> <density(0-100%) or SAME or CLEAR> ABSOLUTE

	s_fogsector_t *secf;

	secf = new s_fogsector_t;

	RAD_CheckForInt(pars[1], &secf->tag);

	if (secf->tag == 0)
		RAD_Error("%s: Invalid tag number: %d\n", pars[0], secf->tag);

	if (pars.size() == 4) // color + relative density change
	{
		if (DDF_CompareName(pars[2], "SAME") == 0)
			secf->leave_color = true;
		else if (DDF_CompareName(pars[2], "CLEAR") == 0)
		{ /* nothing - we will use null pointer to denote clearing fog later */ }
		else
			secf->colmap_color = Z_StrDup(pars[2]);
		if (DDF_CompareName(pars[3], "SAME") == 0)
			secf->leave_density = true;
		else if (DDF_CompareName(pars[3], "CLEAR") == 0)
		{
			secf->relative = false;
			secf->density = 0;
		}
		else
			RAD_CheckForPercentAny(pars[3], &secf->density);
		AddStateToScript(this_rad, 0, RAD_ActFogSector, secf);
	}
	else if (DDF_CompareName(pars[4], "ABSOLUTE") == 0) // color + absolute density change
	{
		secf->relative = false;
		if (DDF_CompareName(pars[2], "SAME") == 0)
			secf->leave_color = true;
		else if (DDF_CompareName(pars[2], "CLEAR") == 0)
		{ /* nothing - we will use null pointer to denote clearing fog later */ }
		else
			secf->colmap_color = Z_StrDup(pars[2]);
		if (DDF_CompareName(pars[3], "SAME") == 0)
			secf->leave_density = true;
		else if (DDF_CompareName(pars[3], "CLEAR") == 0)
			secf->density = 0;
		else
			RAD_CheckForPercent(pars[3], &secf->density);
		AddStateToScript(this_rad, 0, RAD_ActFogSector, secf);
	}
	else // shouldn't get here
		RAD_Error("%s: Malformed FOG_SECTOR command\n");
}


static void RAD_ParseActivateLinetype(param_set_t& pars)
{
	// Activate_LineType <linetype> <tag>

	s_lineactivator_t *lineact;

	lineact = new s_lineactivator_t;

	RAD_CheckForInt(pars[1], &lineact->typenum);
	RAD_CheckForInt(pars[2], &lineact->tag);

	AddStateToScript(this_rad, 0, RAD_ActActivateLinetype, lineact);
}

static void RAD_ParseUnblockLines(param_set_t& pars)
{
	// Unblock_Lines <tag>

	s_lineunblocker_t *lineact;

	lineact = new s_lineunblocker_t;

	RAD_CheckForInt(pars[1], &lineact->tag);

	AddStateToScript(this_rad, 0, RAD_ActUnblockLines, lineact);
}

static void RAD_ParseBlockLines(param_set_t& pars)
{
	// Block_Lines <tag>

	s_lineunblocker_t *lineact;

	lineact = new s_lineunblocker_t;

	RAD_CheckForInt(pars[1], &lineact->tag);

	AddStateToScript(this_rad, 0, RAD_ActBlockLines, lineact);
}

static void RAD_ParseWait(param_set_t& pars)
{
	// Wait <time>

	int tics;

	RAD_CheckForTime(pars[1], &tics);

	if (tics <= 0)
		RAD_Error("%s: Invalid time: %d\n", pars[0], tics);

	pending_wait_tics += tics;
}

static void RAD_ParseJump(param_set_t& pars)
{
	// Jump <label>
	// Jump <label> <random chance>

	s_jump_t *jump = new s_jump_t;

	jump->label = Z_StrDup(pars[1]);
	jump->random_chance = PERCENT_MAKE(100);

	if (pars.size() >= 3)
		RAD_CheckForPercent(pars[2], &jump->random_chance);

	AddStateToScript(this_rad, 0, RAD_ActJump, jump);
}

static void RAD_ParseSleep(param_set_t& pars)
{
	// Sleep

	AddStateToScript(this_rad, 0, RAD_ActSleep, NULL);
}

static void RAD_ParseRetrigger(param_set_t& pars)
{
	// Retrigger

	if (! this_rad->tagged_independent)
		RAD_Error("%s can only be used with TAGGED_INDEPENDENT.\n", pars[0]);

	AddStateToScript(this_rad, 0, RAD_ActRetrigger, NULL);
}

static void RAD_ParseChangeTex(param_set_t& pars)
{
	// Change_Tex <where> <texname>
	// Change_Tex <where> <texname> <tag>
	// Change_Tex <where> <texname> <tag> <subtag>

	s_changetex_t *ctex;

	if (strlen(pars[2]) > 8)
		RAD_Error("%s: Texture name too long: %s\n", pars[0], pars[2]);

	ctex = new s_changetex_t;

	ctex->what = RAD_CheckForChangetexType(pars[1]);

	strcpy(ctex->texname, pars[2]);

	if (pars.size() >= 4)
		RAD_CheckForInt(pars[3], &ctex->tag);

	if (pars.size() >= 5)
		RAD_CheckForInt(pars[4], &ctex->subtag);

	AddStateToScript(this_rad, 0, RAD_ActChangeTex, ctex);
}

static void RAD_ParseShowMenu(param_set_t& pars)
{
	// Show_Menu     <title> <option1> ...
	// Show_Menu_LDF <title> <option1> ...

	s_show_menu_t *menu = new s_show_menu_t;

	if (pars.size() > 11)
		RAD_Error("%s: too many option strings (limit is 9)\n", pars[0]);

	if (DDF_CompareName(pars[0], "SHOW_MENU_LDF") == 0)
		menu->use_ldf = true;

	SYS_ASSERT(2 <= pars.size() && pars.size() <= 11);

	menu->title = RAD_UnquoteString(pars[1]);

	for (size_t p = 2; p < pars.size(); p++)
	{
		menu->options[p-2] = RAD_UnquoteString(pars[p]);
	}

	AddStateToScript(this_rad, 0, RAD_ActShowMenu, menu);
}

static void RAD_ParseMenuStyle(param_set_t& pars)
{
	// Menu_Style  <style>

	s_menu_style_t *mm = new s_menu_style_t;

	mm->style = RAD_UnquoteString(pars[1]);

	AddStateToScript(this_rad, 0, RAD_ActMenuStyle, mm);
}

static void RAD_ParseJumpOn(param_set_t& pars)
{
	// Jump_On <VAR> <label1> <label2> ...
	//
	// "MENU" is the only variable supported so far.

	s_jump_on_t *jump = new s_jump_on_t;

	if (pars.size() > 11)
		RAD_Error("%s: too many labels (limit is 9)\n", pars[0]);

	if (DDF_CompareName(pars[1], "MENU") != 0)
	{
		RAD_Error("%s: Unknown variable '%s' (should be MENU)\n",
			pars[0], pars[1]);
	}

	SYS_ASSERT(2 <= pars.size() && pars.size() <= 11);

	for (size_t p = 2; p < pars.size(); p++)
		jump->labels[p-2] = Z_StrDup(pars[p]);

	AddStateToScript(this_rad, 0, RAD_ActJumpOn, jump);
}

static void RAD_ParseWaitUntilDead(param_set_t& pars)
{
	// WaitUntilDead <monster> ...

	if (pars.size()-1 > 10)
		RAD_Error("%s: too many monsters (limit is 10)\n", pars[0]);

	static int current_tag = 70000;

	s_wait_until_dead_t *wud = new s_wait_until_dead_t;

	wud->tag = current_tag;  current_tag++;

	for (size_t p = 1; p < pars.size(); p++)
		wud->mon_names[p-1] = Z_StrDup(pars[p]);

	AddStateToScript(this_rad, 0, RAD_ActWaitUntilDead, wud);
}


static void RAD_ParseSwitchWeapon(param_set_t& pars)
{
	// SwitchWeapon <WeaponName>

	char * WeaponName = RAD_UnquoteString(pars[1]);

	s_weapon_t *weaparg = new s_weapon_t;

	weaparg->name = WeaponName;

	AddStateToScript(this_rad, 0, RAD_ActSwitchWeapon, weaparg);
}

static void RAD_ParseTeleportToStart(param_set_t& pars)
{
	// TELEPORT_TO_START

	AddStateToScript(this_rad, 0, RAD_ActTeleportToStart, NULL);
}

// Replace one weapon with another instantly (no up/down states run)
// It doesnt matter if we have the old one currently selected or not.
static void RAD_ParseReplaceWeapon(param_set_t& pars)
{
	// ReplaceWeapon <OldWeaponName> <NewWeaponName>

	char * OldWeaponName = RAD_UnquoteString(pars[1]);
	char * NewWeaponName = RAD_UnquoteString(pars[2]);

	s_weapon_replace_t *weaparg = new s_weapon_replace_t;

	weaparg->old_weapon = OldWeaponName;
	weaparg->new_weapon = NewWeaponName;

	AddStateToScript(this_rad, 0, RAD_ActReplaceWeapon, weaparg);
}

// If we have the weapon we insta-switch to it and 
// go to the STATE we indicated.
static void RAD_ParseWeaponEvent(param_set_t& pars)
{
	// Weapon_Event <weapon> <label>
	//

	s_weapon_event_t *tev;
	const char *div;
	int i;

	tev = new s_weapon_event_t;

	tev->weapon_name = Z_StrDup(pars[1]);

	// parse the label name
	div = strchr(pars[2], ':');

	i = div ? (div - pars[2]) : strlen(pars[2]);

	if (i <= 0)
		RAD_Error("%s: Bad label '%s'.\n", pars[0], pars[2]);

	tev->label = new char[i+1];
	Z_StrNCpy((char *)tev->label, pars[2], i);

	tev->offset = div ? MAX(0, atoi(div+1) - 1) : 0;

	AddStateToScript(this_rad, 0, RAD_ActWeaponEvent, tev);
}


// Replace one thing with another.
static void RAD_ParseReplaceThing(param_set_t& pars)
{
	// ReplaceThing <OldThingName> <NewThingName>

	s_thing_replace_t *thingarg = new s_thing_replace_t;


	// get old monster name
	if (isdigit(pars[1][0]))
	{
		RAD_CheckForInt(pars[1], &thingarg->old_thing_type);
	}
	else
		thingarg->old_thing_name = Z_StrDup(pars[1]);

	// get new monster name
	if (isdigit(pars[2][0]))
	{
		RAD_CheckForInt(pars[2], &thingarg->new_thing_type);
	}
	else
		thingarg->new_thing_name = Z_StrDup(pars[2]);

	AddStateToScript(this_rad, 0, RAD_ActReplaceThing, thingarg);
}

//  PARSER TABLE

static const rts_parser_t radtrig_parsers[] =
{
	// directives...
	{-1, "#DEFINE",  3,3, RAD_ParseDefine},
	{0, "#VERSION",  2,2, RAD_ParseVersion},
	{0, "#CLEARALL", 1,1, RAD_ParseClearAll},

	// basics...
	{-1, "START_MAP", 2,2, RAD_ParseStartMap},
	{-1, "RADIUS_TRIGGER", 4,6, RAD_ParseRadiusTrigger},
	{-1, "RECT_TRIGGER", 5,7, RAD_ParseRadiusTrigger},
	{-1, "END_RADIUS_TRIGGER", 1,1, RAD_ParseEndRadiusTrigger},
	{-1, "END_MAP",  1,1, RAD_ParseEndMap},

	// properties...
	{2, "NAME", 2,2, RAD_ParseName},
	{2, "TAG",  2,2, RAD_ParseTag},
	{2, "WHEN_APPEAR", 2,2, RAD_ParseWhenAppear},
	{2, "WHEN_PLAYER_NUM",   2,3, RAD_ParseWhenPlayerNum},
	{2, "NET_MODE", 2,3, RAD_ParseNetMode},
	{2, "TAGGED_REPEATABLE", 1,3, RAD_ParseTaggedRepeatable},
	{2, "TAGGED_USE", 1,1, RAD_ParseTaggedUse},
	{2, "TAGGED_INDEPENDENT", 1,1, RAD_ParseTaggedIndependent},
	{2, "TAGGED_IMMEDIATE",   1,1, RAD_ParseTaggedImmediate},
	{2, "TAGGED_PLAYER_SPECIFIC", 1,1, RAD_ParseTaggedPlayerSpecific},
	{2, "TAGGED_DISABLED", 1,1, RAD_ParseTaggedDisabled},
	{2, "TAGGED_PATH", 2,2, RAD_ParseTaggedPath},
	{2, "PATH_EVENT", 2,2, RAD_ParsePathEvent},
	{2, "ONDEATH",  2,3, RAD_ParseOnDeath},
	{2, "ONHEIGHT", 3,4, RAD_ParseOnHeight},
	{2, "ONCEILINGHEIGHT", 3,4, RAD_ParseOnHeight},
	{2, "ONCONDITION",  2,2, RAD_ParseOnCondition},

	// actions...
	{2, "TIP",     2,5, RAD_ParseTip},
	{2, "TIP_LDF", 2,5, RAD_ParseTip},
	{2, "TIP_GRAPHIC", 2,5, RAD_ParseTip},
	{2, "TIP_SLOT",    2,2, RAD_ParseTipSlot},
	{2, "TIP_SET_POS",    3,4, RAD_ParseTipPos},
	{2, "TIP_SET_COLOUR", 2,3, RAD_ParseTipColour},
	{2, "TIP_SET_TRANS",  2,3, RAD_ParseTipTrans},
	{2, "TIP_SET_ALIGN",  2,2, RAD_ParseTipAlign},
	{2, "EXITLEVEL",  1,2, RAD_ParseExitLevel},
	{2, "SECRETEXIT", 1,2, RAD_ParseExitLevel},
	{2, "SPAWNTHING", 2,22, RAD_ParseSpawnThing},
	{2, "SPAWNTHING_AMBUSH", 2,22, RAD_ParseSpawnThing},
	{2, "SPAWNTHING_FLASH",  2,22, RAD_ParseSpawnThing},
	{2, "PLAYSOUND", 2,5, RAD_ParsePlaySound},
	{2, "PLAYSOUND_BOSSMAN", 2,5, RAD_ParsePlaySound},
	{2, "KILLSOUND", 1,1, RAD_ParseKillSound},
	{2, "HEALPLAYER",   2,3, RAD_ParseHealPlayer},
	{2, "GIVEARMOUR",   3,4, RAD_ParseGiveArmour},
	{2, "DAMAGEPLAYER", 2,2, RAD_ParseDamagePlayer},
	{2, "GIVE_BENEFIT", 2,2, RAD_ParseGiveLoseBenefit},
	{2, "LOSE_BENEFIT", 2,2, RAD_ParseGiveLoseBenefit},
	{2, "DAMAGE_MONSTERS", 3,3, RAD_ParseDamageMonsters},
	{2, "THING_EVENT", 3,4, RAD_ParseThingEvent},
	{2, "SKILL",   4,4, RAD_ParseSkill},
	{2, "GOTOMAP", 2,3, RAD_ParseGotoMap},
	{2, "HUB_EXIT", 3,3, RAD_ParseHubExit},
	{2, "MOVE_SECTOR", 4,5, RAD_ParseMoveSector},
	{2, "LIGHT_SECTOR", 3,4, RAD_ParseLightSector},
	{2, "FOG_SECTOR", 4,5, RAD_ParseFogSector},
	{2, "ENABLE_SCRIPT",  2,2, RAD_ParseEnableScript},
	{2, "DISABLE_SCRIPT", 2,2, RAD_ParseEnableScript},
	{2, "ENABLE_TAGGED",  2,2, RAD_ParseEnableTagged},
	{2, "DISABLE_TAGGED", 2,2, RAD_ParseEnableTagged},
	{2, "ACTIVATE_LINETYPE", 3,3, RAD_ParseActivateLinetype},
	{2, "UNBLOCK_LINES", 2,2, RAD_ParseUnblockLines},
	{2, "BLOCK_LINES", 2,2, RAD_ParseBlockLines},
	{2, "WAIT",  2,2, RAD_ParseWait},
	{2, "JUMP",  2,3, RAD_ParseJump},
	{2, "LABEL", 2,2, RAD_ParseLabel},
	{2, "SLEEP", 1,1, RAD_ParseSleep},
	{2, "EXITGAME", 1,1, RAD_ParseExitGame},
	{2, "RETRIGGER", 1,1, RAD_ParseRetrigger},
	{2, "CHANGE_TEX", 3,5, RAD_ParseChangeTex},
	{2, "CHANGE_MUSIC", 2,2, RAD_ParseChangeMusic},
	{2, "SHOW_MENU", 2,99, RAD_ParseShowMenu},
	{2, "SHOW_MENU_LDF", 2,99, RAD_ParseShowMenu},
	{2, "MENU_STYLE", 2,2, RAD_ParseMenuStyle},
	{2, "JUMP_ON", 3,99, RAD_ParseJumpOn},
	{2, "WAIT_UNTIL_DEAD", 2,11, RAD_ParseWaitUntilDead},
	
	{2, "SWITCH_WEAPON", 2,2, RAD_ParseSwitchWeapon},
	{2, "TELEPORT_TO_START", 1,1, RAD_ParseTeleportToStart},
	{2, "REPLACE_WEAPON", 3,3, RAD_ParseReplaceWeapon},
	{2, "WEAPON_EVENT", 3,3, RAD_ParseWeaponEvent},
	{2, "REPLACE_THING", 3,3, RAD_ParseReplaceThing},

	// old crud
	{2, "SECTORV", 4,4, RAD_ParseMoveSector},
	{2, "SECTORL", 3,3, RAD_ParseLightSector},

	// that's all, folks.
	{0, NULL, 0,0, NULL}
};


void RAD_ParseLine()
{
	param_set_t pars;

	RAD_TokenizeLine(pars);

	// simply ignore blank lines
	if (pars.empty())
		return;

	for (const rts_parser_t *cur = radtrig_parsers; cur->name != NULL; cur++)
	{
		const char *cur_name = cur->name;

		if (DDF_CompareName(pars[0], cur_name) != 0)
			continue;

		// check level
		if (cur->level >= 0)
		{
			if (cur->level != rad_cur_level)
			{
				RAD_Error("RTS command '%s' used in wrong place "
					"(found in %s, should be in %s).\n", pars[0],
					rad_level_names[rad_cur_level],
					rad_level_names[cur->level]);

				// NOT REACHED
				return;
			}
		}

		// check number of parameters.  Too many is live-with-able, but
		// not enough is fatal.

		if ((int)pars.size() < cur->min_pars)
			RAD_Error("%s: Not enough parameters.\n", cur->name);

		if ((int)pars.size() > cur->max_pars)
			RAD_WarnError("%s: Too many parameters.\n", cur->name);

		// found it, invoke the parser function
		(* cur->parser)(pars);

		RAD_FreeParameters(pars);
		return;
	}

	RAD_WarnError("Unknown primitive: %s\n", pars[0]);

	RAD_FreeParameters(pars);
}


//----------------------------------------------------------------------------

static int ReadScriptLine(const std::string& data, size_t& pos, std::string& out_line)
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
		if (data[pos] == '\\' && pos+3 < limit)
		{
			if (data[pos+1] == '\n' ||
				(data[pos+1] == '\r' && data[pos+2] == '\n'))
			{
				pos += (data[pos+1] == '\n') ? 2 : 3;
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


static void RAD_ParserDone()
{
	if (rad_cur_level >= 2)
		RAD_Error("RADIUS_TRIGGER: block not terminated !\n");

	if (rad_cur_level == 1)
		RAD_Error("START_MAP: block not terminated !\n");

	DDF_MainFreeDefines();
}


void RAD_ReadScript(const std::string& data, const std::string& source)
{
	// FIXME store source somewhere, like rad_cur_filename

	I_Debugf("RTS: Loading LUMP (size=%d)\n", (int)data.size());

	// WISH: a more helpful filename
	rad_cur_filename = "RSCRIPT";
	rad_cur_linenum = 1;
	rad_cur_level = 0;

	size_t pos = 0;

	for (;;)
	{
		int real_num = ReadScriptLine(data, pos, rad_cur_line);
		if (real_num == 0)
			break;

#if (DEBUG_RTS)
		I_Debugf("RTS LINE: '%s'\n", rad_cur_line.c_str());
#endif

		RAD_ParseLine();

		rad_cur_linenum += real_num;
	}

	RAD_ParserDone();
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
