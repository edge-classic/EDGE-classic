//----------------------------------------------------------------------------
//  EDGE Data Definition File Code (States)
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

#include "ddf_states.h"

#include <string.h>

#include "ddf_local.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"
#include "p_action.h"

static const State template_state = {
    0,       // sprite ref
    0,       // frame ref
    0,       // bright
    0,       // flags
    -1,      // tics

    nullptr, // model_frame
    nullptr, // label
    nullptr, // routine
    nullptr, // parameter
    0,       // rts_tag_type

    0,       // next state ref
    -1       // jump state ref
};

State *states = nullptr;
int    num_states;

std::vector<std::string> ddf_sprite_names;
std::vector<std::string> ddf_model_names;

// Until the DDFStateFinishState() routine is called, the `nextstate'
// field of each state contains a special value.  0 for normal (no
// redirector).  -1 for the #REMOVE redirector.  Otherwise the top 16
// bits is a redirector, and the bottom 16 bits is a positive offset
// from that redirector (usually 0).
//
// Every time a new redirector is used, it is added to this list.  The
// top 16 bits (minus 1) will be an index into this list of redirector
// names.  These labels will be looked for in the states when the
// fixup routine is called.
static std::vector<std::string> redirs;

static constexpr uint8_t kMaximumStateSplits = 10;

static std::string stateinfo[kMaximumStateSplits + 1];

// a little caching makes a big difference here
// (because DDF entries are usually limited to a single sprite)
static int last_sprite = -1;
static int last_model  = -1;

static int AddSpriteName(const char *name)
{
    if (epi::StringCaseCompareASCII(name, "NULL") == 0)
        return 0;

    if (last_sprite >= 0 && epi::StringCaseCompareASCII(ddf_sprite_names[last_sprite], name) == 0)
        return last_sprite;

    // look backwards, assuming a recent sprite is more likely
    for (int i = (int)ddf_sprite_names.size() - 1; i > 0; i--)
        if (epi::StringCaseCompareASCII(ddf_sprite_names[i], name) == 0)
            return ((last_sprite = i));

    last_sprite = (int)ddf_sprite_names.size();

    // not found, so insert it
    ddf_sprite_names.push_back(std::string(name));

    return last_sprite;
}

static int AddModelName(const char *name)
{
    if (epi::StringCaseCompareASCII(name, "NULL") == 0)
        return 0;

    if (last_model >= 0 && epi::StringCaseCompareASCII(ddf_model_names[last_model], name) == 0)
        return last_model;

    // look backwards, assuming a recent model is more likely
    for (int i = (int)ddf_model_names.size() - 1; i > 0; i--)
        if (epi::StringCaseCompareASCII(ddf_model_names[i], name) == 0)
            return ((last_model = i));

    last_model = (int)ddf_model_names.size();

    // not found, so insert it
    ddf_model_names.push_back(std::string(name));

    return last_model;
}

void DDFStateInit(void)
{
    // create states array with a single 'S_NULL' state
    states = (State *)malloc(sizeof(State));
    if (states == nullptr)
        FatalError("could not allocate states\n");

    states[0]  = template_state;
    num_states = 1;

    // create the 'SPR_NULL' sprite
    // (Not strictly needed, but means we can access the arrays
    //  without subtracting 1)
    AddSpriteName("!nullptr!");
    AddModelName("!nullptr!");
}

void DDFStateCleanUp(void)
{ /* nothing to do */
}

//
// DDFMainSplitIntoState
//
// Small procedure that takes the info and splits it into relevant stuff
//
// -KM- 1998/12/21 Rewrote procedure, much cleaner now.
//
// -AJA- 2000/09/03: Rewrote _again_ damn it, in order to handle `:'
//       appearing inside brackets.
//
static int DDFMainSplitIntoState(const char *info)
{
    char *temp;
    char *first;

    int  cur;
    int  brackets = 0;
    bool done     = false;

    // use a buffer, since we modify the string
    char infobuf[512];

    strcpy(infobuf, info);

    for (cur = 0; cur < kMaximumStateSplits + 1; cur++)
        stateinfo[cur] = std::string();

    first = temp = infobuf;

    for (cur = 0; !done && cur < kMaximumStateSplits; temp++)
    {
        if (*temp == '(')
        {
            brackets++;
            continue;
        }

        if (*temp == ')')
        {
            if (brackets == 0)
                DDFError("Mismatched ) bracket in states: %s\n", info);

            brackets--;
            continue;
        }

        if (*temp && *temp != ':')
            continue;

        if (brackets > 0)
            continue;

        if (*temp == 0)
            done = true;

        *temp = 0;

        if (first[0] == '#')
        {
            // signify that we have found redirector
            stateinfo[0] = std::string(first + 1);
            stateinfo[1] = std::string();
            stateinfo[2] = std::string();

            if (!done)
                stateinfo[1] = std::string(temp + 1);

            return -1;
        }

        stateinfo[cur++] = std::string(first);

        first = temp + 1;
    }

    if (brackets > 0)
        DDFError("Unclosed ( bracket in states: %s\n", info);

    return cur;
}

//
// DDFMainSplitActionArg
//
// Small procedure that takes an action like "FOO(BAR)", and splits it
// into two strings "FOO" and "BAR".
//
// -AJA- 1999/08/10: written.
//
static void DDFMainSplitActionArg(const char *info, char *actname, char *actarg)
{
    int len = strlen(info);

    const char *mid = strchr(info, '(');

    if (mid && len >= 4 && info[len - 1] == ')')
    {
        int len2 = (mid - info);

        epi::CStringCopyMax(actname, info, len2);

        len -= len2 + 2;
        epi::CStringCopyMax(actarg, mid + 1, len);
    }
    else
    {
        strcpy(actname, info);
        actarg[0] = 0;
    }
}

//
// StateGetRedirector
//
static int StateGetRedirector(const char *redir)
{
    for (size_t i = 0; i < redirs.size(); i++)
    {
        if (DDFCompareName(redirs[i].c_str(), redir) == 0)
            return (int)i;
    }

    redirs.push_back(redir);

    return (int)redirs.size() - 1;
}

//
// DDFStateFindLabel
//
int DDFStateFindLabel(const std::vector<StateRange> &group, const char *label, bool quiet)
{
    for (int g = (int)group.size() - 1; g >= 0; g--)
    {
        for (int i = group[g].last; i >= group[g].first; i--)
        {
            if (!states[i].label)
                continue;

            if (DDFCompareName(states[i].label, label) == 0)
                return i;
        }
    }

    // compatibility hack:
    if (DDFCompareName(label, "IDLE") == 0)
    {
        return DDFStateFindLabel(group, "SPAWN");
    }

    if (!quiet)
        DDFError("Unknown label '%s' (object has no such frames).\n", label);

    return 0;
}

//
// DDFStateReadState
//
void DDFStateReadState(const char *info, const char *label, std::vector<StateRange> &group, int *state_num, int index,
                       const char *redir, const DDFActionCode *action_list, bool is_weapon)
{
    EPI_ASSERT(group.size() > 0);

    StateRange &range = group.back();

    int i, j;

    char action_name[128];
    char action_arg[128];

    State *cur;

    // Split the state info into component parts
    // -ACB- 1998/07/26 New Procedure, for cleaner code.

    i = DDFMainSplitIntoState(info);
    if (i < 5 && i >= 0)
    {
        if (strchr(info, '['))
        {
            // -ES- 2000/02/02 Probably unterminated state.
            DDFError("DDFMainLoadStates: Bad state '%s', possibly missing ';'\n", info);
        }
        DDFError("Bad state '%s'\n", info);
    }

    if (stateinfo[0].empty())
        DDFError("Missing sprite in state frames: `%s'\n", info);

    //--------------------------------------------------
    //----------------REDIRECTOR HANDLING---------------
    //--------------------------------------------------

    if (stateinfo[2].empty())
    {
        if (!range.first)
            DDFError("Redirector used without any states (`%s')\n", info);

        cur = &states[range.last];

        EPI_ASSERT(!stateinfo[0].empty());

        if (DDFCompareName(stateinfo[0].c_str(), "REMOVE") == 0)
        {
            cur->nextstate = -1;
            return;
        }

        cur->nextstate = (StateGetRedirector(stateinfo[0].c_str()) + 1) << 16;

        if (!stateinfo[1].empty())
            cur->nextstate += HMM_MAX(0, atoi(stateinfo[1].c_str()) - 1);

        return;
    }

    //--------------------------------------------------
    //---------------- ALLOCATE NEW STATE --------------
    //--------------------------------------------------

    num_states += 1;

    states = (State *)realloc(states, num_states * sizeof(State));
    if (states == nullptr)
        FatalError("could not allocate states\n");

    cur = &states[num_states - 1];

    // initialise with defaults
    cur[0] = template_state;

    if (range.first == 0)
    {
        // very first state for thing/weapon
        range.first = num_states - 1;
    }

    range.last = num_states - 1;

    if (index == 0)
    {
        // first state in this set of states
        if (state_num)
            state_num[0] = num_states - 1;

        // ...therefore copy the label
        cur->label = strdup(label);
    }

    if (redir && cur->nextstate == 0)
    {
        if (DDFCompareName("REMOVE", redir) == 0)
            cur->nextstate = -1;
        else
            cur->nextstate = (StateGetRedirector(redir) + 1) << 16;
    }

    //--------------------------------------------------
    //----------------SPRITE NAME HANDLING--------------
    //--------------------------------------------------

    if (stateinfo[1].empty() || stateinfo[2].empty() || stateinfo[3].empty())
        DDFError("Bad state frame, missing fields: %s\n", info);

    //--------------------------------------------------
    //--------------SPRITE INDEX HANDLING---------------
    //--------------------------------------------------

    cur->flags = 0;

    // look at the first character
    const char *sprite_x = stateinfo[1].c_str();

    j = sprite_x[0];

    if ('A' <= j && j <= ']')
    {
        cur->frame = j - (int)'A';
    }
    else if (j == '@')
    {
        cur->frame = -1;

        char first_ch = sprite_x[1];

        if (epi::IsDigitASCII(first_ch))
        {
            cur->flags = kStateFrameFlagModel;
            cur->frame = atol(sprite_x + 1) - 1;
        }
        else if (epi::IsAlphaASCII(first_ch) || (first_ch == '_'))
        {
            cur->flags       = kStateFrameFlagModel | kStateFrameFlagUnmapped;
            cur->frame       = 0;
            cur->model_frame = strdup(sprite_x + 1);
        }

        if (cur->frame < 0)
            DDFError("DDFMainLoadStates: Illegal model frame: %s\n", sprite_x);
    }
    else
        DDFError("DDFMainLoadStates: Illegal sprite frame: %s\n", sprite_x);

    if (is_weapon)
        cur->flags |= kStateFrameFlagWeapon;

    if (cur->flags & kStateFrameFlagModel)
        cur->sprite = AddModelName(stateinfo[0].c_str());
    else
        cur->sprite = AddSpriteName(stateinfo[0].c_str());

    //--------------------------------------------------
    //------------STATE TIC COUNT HANDLING--------------
    //--------------------------------------------------

    cur->tics = atol(stateinfo[2].c_str());

    //--------------------------------------------------
    //------------STATE BRIGHTNESS LEVEL----------------
    //--------------------------------------------------

    if (epi::StringCaseCompareASCII(stateinfo[3], "NORMAL") == 0)
        cur->bright = 0;
    else if (epi::StringCaseCompareASCII(stateinfo[3], "BRIGHT") == 0)
        cur->bright = 255;
    else if (epi::StringPrefixCaseCompareASCII(stateinfo[3], "LIT") == 0)
    {
        cur->bright = strtol(stateinfo[3].c_str() + 3, nullptr, 10);
        cur->bright = HMM_Clamp(0, cur->bright * 255 / 99, 255);
    }
    else
        DDFWarnError("DDFMainLoadStates: Lighting is not BRIGHT or NORMAL\n");

    //--------------------------------------------------
    //------------STATE ACTION CODE HANDLING------------
    //--------------------------------------------------

    if (!stateinfo[4].empty())
    {
        // Get Action Code Ref (Using remainder of the string).
        // Go through all the actions, end if terminator or action found
        //
        // -AJA- 1999/08/10: Updated to handle a special argument.

        DDFMainSplitActionArg(stateinfo[4].c_str(), action_name, action_arg);

        for (i = 0; action_list[i].actionname; i++)
        {
            const char *current = action_list[i].actionname;

            if (current[0] == '!')
                current++;

            if (DDFCompareName(current, action_name) == 0)
                break;
        }

        if (!action_list[i].actionname)
        {
            DDFWarnError("Unknown code pointer: %s\n", stateinfo[4].c_str());
        }
        else
        {
            cur->action     = action_list[i].action;
            cur->action_par = nullptr;

            if (action_list[i].handle_arg)
                (*action_list[i].handle_arg)(action_arg, cur);
        }
    }
}

bool DDFMainParseState(uint8_t *object, std::vector<StateRange> &group, const char *field, const char *contents,
                       int index, bool is_last, bool is_weapon, const DDFStateStarter *starters,
                       const DDFActionCode *actions)
{
    if (epi::StringPrefixCaseCompareASCII(field, "STATES(") != 0)
        return false;

    // extract label name
    field += 7;

    const char *pos = strchr(field, ')');

    if (pos == nullptr || pos == field || (pos - field) > 64)
        return false;

    std::string labname(field, pos - field);

    // check for the "standard" states
    int i;
    for (i = 0; starters[i].label; i++)
        if (DDFCompareName(starters[i].label, labname.c_str()) == 0)
            break;

    const DDFStateStarter *starter = nullptr;
    if (starters[i].label)
        starter = &starters[i];

    int *var = nullptr;
    if (starter)
        var = (int *)(object + starter->offset);

    const char *redir = nullptr;
    if (is_last)
        redir = starter ? starter->last_redir : (is_weapon ? "READY" : "IDLE");

    DDFStateReadState(contents, labname.c_str(), group, var, index, redir, actions, is_weapon);
    return true;
}

void DDFStateBeginRange(std::vector<StateRange> &group)
{
    StateRange range;

    range.first = 0;
    range.last  = 0;

    group.push_back(range);
}

//
// DDFStateFinishRange
//
// Check through the states on an mobj and attempts to dereference any
// encoded state redirectors.
//
void DDFStateFinishRange(std::vector<StateRange> &group)
{
    EPI_ASSERT(!group.empty());

    StateRange &range = group.back();

    // if no states were added, remove the unused range
    if (range.first == 0)
    {
        group.pop_back();

        redirs.clear();
        return;
    }

    for (int i = range.first; i <= range.last; i++)
    {
        // handle next state ref
        if (states[i].nextstate == -1)
        {
            states[i].nextstate = 0;
        }
        else if ((states[i].nextstate >> 16) == 0)
        {
            states[i].nextstate = (i == range.last) ? 0 : i + 1;
        }
        else
        {
            int RI = (states[i].nextstate >> 16) - 1;

            // FIXME: is this validated anywhere?
            states[i].nextstate = DDFStateFindLabel(group, redirs[RI].c_str()) + (states[i].nextstate & 0xFFFF);
        }

        // handle jump state ref
        if (states[i].jumpstate == -1)
        {
            states[i].jumpstate = 0;
        }
        else if ((states[i].jumpstate >> 16) == 0)
        {
            states[i].jumpstate = (i == range.last) ? 0 : i + 1;
        }
        else
        {
            int RI = (states[i].jumpstate >> 16) - 1;

            states[i].jumpstate = DDFStateFindLabel(group, redirs[RI].c_str()) + (states[i].jumpstate & 0xFFFF);
        }
    }

    redirs.clear();
}

bool DDFStateGroupHasState(const std::vector<StateRange> &group, int st)
{
    for (int g = 0; g < (int)group.size(); g++)
    {
        const StateRange &range = group[g];

        if (range.first <= st && st <= range.last)
            return true;
    }

    return false;
}

//----------------------------------------------------------------------------

//
// DDFStateGetAttack
//
// Parse the special argument for the state as an attack.
//
// -AJA- 1999/08/10: written.
//
void DDFStateGetAttack(const char *arg, State *cur_state)
{
    if (!arg || !arg[0])
        return;

    cur_state->action_par = (void *)atkdefs.Lookup(arg);
    if (cur_state->action_par == nullptr)
        DDFWarnError("Unknown Attack (States): %s\n", arg);
}

void DDFStateGetMobj(const char *arg, State *cur_state)
{
    if (!arg || !arg[0])
        return;

    cur_state->action_par = new MobjStringReference(arg);
}

void DDFStateGetSound(const char *arg, State *cur_state)
{
    if (!arg || !arg[0])
        return;

    cur_state->action_par = (void *)sfxdefs.GetEffect(arg);
}

void DDFStateGetInteger(const char *arg, State *cur_state)
{
    if (!arg || !arg[0])
        return;

    int *val_ptr = new int;

    if (sscanf(arg, " %i ", val_ptr) != 1)
        DDFError("DDFStateGetInteger: bad value: %s\n", arg);

    cur_state->action_par = val_ptr;
}

void DDFStateGetIntPair(const char *arg, State *cur_state)
{
    // Parses two integers separated by commas.
    //
    int *values;

    if (!arg || !arg[0])
        return;

    values = new int[2];

    if (sscanf(arg, " %i , %i ", &values[0], &values[1]) != 2)
        DDFError("DDFStateGetIntPair: bad values: %s\n", arg);

    cur_state->action_par = values;
}

void DDFStateGetDEHParams(const char *arg, State *cur_state)
{
    // Parses up to eight integers separated by commas
    int *values;

    if (!arg)
        return;

    values = new int[8]{0,0,0,0,0,0,0,0};

    std::vector<std::string> args = epi::SeparatedStringVector(arg, ',');

    for (int i = 0, i_end = args.size(); i < i_end; i++)
    {
        if (i >= 8)
            break;
        if (sscanf(args[i].c_str(), "%d", &values[i]) != 1)
            values[i] = 0;
    }

    cur_state->action_par = values;
}

void DDFStateGetFloat(const char *arg, State *cur_state)
{
    if (!arg || !arg[0])
        return;

    float *val_ptr = new float;

    if (sscanf(arg, " %f ", val_ptr) != 1)
        DDFError("DDFStateGetFloat: bad value: %s\n", arg);

    cur_state->action_par = val_ptr;
}

void DDFStateGetPercent(const char *arg, State *cur_state)
{
    if (!arg || !arg[0])
        return;

    float *val_ptr = new float;

    if (sscanf(arg, " %f%% ", val_ptr) != 1 || (*val_ptr) < 0)
        DDFError("DDFStateGetPercent: Bad percentage: %s\n", arg);

    (*val_ptr) /= 100.0f;

    cur_state->action_par = val_ptr;
}

void DDFStateGetJump(const char *arg, State *cur_state)
{
    // JUMP(label)
    // JUMP(label,chance)

    // Dasho 2023.10.16 - Changed to allow negative percentages to use for
    // special values (A_RefireTo ammo check, etc)

    if (!arg || !arg[0])
        return;

    JumpActionInfo *jump = new JumpActionInfo;

    int len;
    int offset = 0;

    const char *s = strchr(arg, ',');

    if (!s)
    {
        len = strlen(arg);
    }
    else
    {
        // convert chance value
        DDFMainGetPercentAny(s + 1, &jump->chance);

        len = s - arg;
    }

    if (len == 0)
        DDFError("DDFStateGetJump: missing label!\n");

    if (len > 75)
        DDFError("DDFStateGetJump: label name too long!\n");

    // copy label name
    char buffer[80];

    for (len = 0; *arg && (*arg != ':') && (*arg != ','); len++, arg++)
        buffer[len] = *arg;

    buffer[len] = 0;

    if (*arg == ':')
        offset = HMM_MAX(0, atoi(arg + 1) - 1);

    // set the jump state
    cur_state->jumpstate  = ((StateGetRedirector(buffer) + 1) << 16) + offset;
    cur_state->action_par = jump;
}

// Like the above, but accepts an arbtrary int for the second parameter
void DDFStateGetJumpInt(const char *arg, State *cur_state)
{
    // JUMP(label)
    // JUMP(label,value)

    if (!arg || !arg[0])
        return;

    JumpActionInfo *jump = new JumpActionInfo;

    int len;
    int offset = 0;

    const char *s = strchr(arg, ',');

    if (!s)
    {
        len = strlen(arg);
    }
    else
    {
        DDFMainGetNumeric(s+1, &jump->amount);
        len = s - arg;
    }

    if (len == 0)
        DDFError("DDFStateGetJump: missing label!\n");

    if (len > 75)
        DDFError("DDFStateGetJump: label name too long!\n");

    // copy label name
    char buffer[80];

    for (len = 0; *arg && (*arg != ':') && (*arg != ','); len++, arg++)
        buffer[len] = *arg;

    buffer[len] = 0;

    if (*arg == ':')
        offset = HMM_MAX(0, atoi(arg + 1) - 1);

    // set the jump state
    cur_state->jumpstate  = ((StateGetRedirector(buffer) + 1) << 16) + offset;
    cur_state->action_par = jump;
}

// Like the above, but accepts a pair of arbtrary ints for the second and third parameters
void DDFStateGetJumpIntPair(const char *arg, State *cur_state)
{
    // JUMP(label)
    // JUMP(label,value)
    // JUMP(label,value,value)

    if (!arg || !arg[0])
        return;

    JumpActionInfo *jump = new JumpActionInfo;

    int len;
    int offset = 0;

    const char *s = strchr(arg, ',');

    if (!s)
    {
        len = strlen(arg);
    }
    else
    {
        const char *s2 = strchr(s+1, ',');
        if (!s2)
        {
            DDFMainGetNumeric(s+1, &jump->amount);
            jump->amount2 = 0;
        }
        else
        {
            if (sscanf(s+1, " %i , %i ", &jump->amount, &jump->amount2) != 2)
                DDFError("DDFStateGetJumpIntPair: bad values: %s\n", s+1);
        }
        len = s - arg;
    }

    if (len == 0)
        DDFError("DDFStateGetJump: missing label!\n");

    if (len > 75)
        DDFError("DDFStateGetJump: label name too long!\n");

    // copy label name
    char buffer[80];

    for (len = 0; *arg && (*arg != ':') && (*arg != ','); len++, arg++)
        buffer[len] = *arg;

    buffer[len] = 0;

    if (*arg == ':')
        offset = HMM_MAX(0, atoi(arg + 1) - 1);

    // set the jump state
    cur_state->jumpstate  = ((StateGetRedirector(buffer) + 1) << 16) + offset;
    cur_state->action_par = jump;
}

void DDFStateGetFrame(const char *arg, State *cur_state)
{
    // Sets the jump_state, like DDFStateGetJump above.
    //
    // ACTION(label)

    if (!arg || !arg[0])
        return;

    int len;
    int offset = 0;

    // copy label name
    char buffer[80];

    for (len = 0; *arg && (*arg != ':'); len++, arg++)
        buffer[len] = *arg;

    buffer[len] = 0;

    if (*arg == ':')
        offset = HMM_MAX(0, atoi(arg + 1) - 1);

    // set the jump state
    cur_state->jumpstate = ((StateGetRedirector(buffer) + 1) << 16) + offset;
}

MorphActionInfo::MorphActionInfo() : info_(nullptr), info_ref_(), start_()
{
}

MorphActionInfo::~MorphActionInfo()
{
}

void DDFStateGetMorph(const char *arg, State *cur_state)
{
    // MORPH(typename)
    // MORPH(typename,label)

    if (!arg || !arg[0])
        return;

    MorphActionInfo *morph = new MorphActionInfo;

    morph->start_.label_ = "IDLE";

    const char *s = strchr(arg, ',');

    // get type name
    char buffer[80];

    int len = s ? (s - arg) : strlen(arg);

    if (len == 0)
        DDFError("DDFStateGetMorph: missing type name!\n");

    if (len > 75)
        DDFError("DDFStateGetMorph: type name too long!\n");

    for (len = 0; *arg && (*arg != ':') && (*arg != ','); len++, arg++)
        buffer[len] = *arg;

    buffer[len] = 0;

    morph->info_ref_ = buffer;

    // get start label (if present)
    if (s)
    {
        s++;

        len = strlen(s);

        if (len == 0)
            DDFError("DDFStateGetMorph: missing label!\n");

        if (len > 75)
            DDFError("DDFStateGetMorph: label too long!\n");

        for (len = 0; *s && (*s != ':') && (*s != ','); len++, s++)
            buffer[len] = *s;

        buffer[len] = 0;

        morph->start_.label_ = buffer;

        if (*s == ':')
            morph->start_.offset_ = HMM_MAX(0, atoi(s + 1) - 1);
    }

    cur_state->action_par = morph;
}

BecomeActionInfo::BecomeActionInfo() : info_(nullptr), info_ref_(), start_()
{
}

BecomeActionInfo::~BecomeActionInfo()
{
}

void DDFStateGetBecome(const char *arg, State *cur_state)
{
    // BECOME(typename)
    // BECOME(typename,label)

    if (!arg || !arg[0])
        return;

    BecomeActionInfo *become = new BecomeActionInfo;

    become->start_.label_ = "IDLE";

    const char *s = strchr(arg, ',');

    // get type name
    char buffer[80];

    int len = s ? (s - arg) : strlen(arg);

    if (len == 0)
        DDFError("DDFStateGetBecome: missing type name!\n");

    if (len > 75)
        DDFError("DDFStateGetBecome: type name too long!\n");

    for (len = 0; *arg && (*arg != ':') && (*arg != ','); len++, arg++)
        buffer[len] = *arg;

    buffer[len] = 0;

    become->info_ref_ = buffer;

    // get start label (if present)
    if (s)
    {
        s++;

        len = strlen(s);

        if (len == 0)
            DDFError("DDFStateGetBecome: missing label!\n");

        if (len > 75)
            DDFError("DDFStateGetBecome: label too long!\n");

        for (len = 0; *s && (*s != ':') && (*s != ','); len++, s++)
            buffer[len] = *s;

        buffer[len] = 0;

        become->start_.label_ = buffer;

        if (*s == ':')
            become->start_.offset_ = HMM_MAX(0, atoi(s + 1) - 1);
    }

    cur_state->action_par = become;
}

WeaponBecomeActionInfo::WeaponBecomeActionInfo() : info_(nullptr), info_ref_(), start_()
{
}

WeaponBecomeActionInfo::~WeaponBecomeActionInfo()
{
}

void DDFStateGetBecomeWeapon(const char *arg, State *cur_state)
{
    // BECOME(typename)
    // BECOME(typename,label)

    if (!arg || !arg[0])
        return;

    WeaponBecomeActionInfo *become = new WeaponBecomeActionInfo;

    become->start_.label_ = "READY";

    const char *s = strchr(arg, ',');

    // get type name
    char buffer[80];

    int len = s ? (s - arg) : strlen(arg);

    if (len == 0)
        DDFError("DDFStateGetBecomeWeapon: missing type name!\n");

    if (len > 75)
        DDFError("DDFStateGetBecomeWeapon: type name too long!\n");

    for (len = 0; *arg && (*arg != ':') && (*arg != ','); len++, arg++)
        buffer[len] = *arg;

    buffer[len] = 0;

    become->info_ref_ = buffer;

    // get start label (if present)
    if (s)
    {
        s++;

        len = strlen(s);

        if (len == 0)
            DDFError("DDFStateGetBecomeWeapon: missing label!\n");

        if (len > 75)
            DDFError("DDFStateGetBecomeWeapon: label too long!\n");

        for (len = 0; *s && (*s != ':') && (*s != ','); len++, s++)
            buffer[len] = *s;

        buffer[len] = 0;

        become->start_.label_ = buffer;

        if (*s == ':')
            become->start_.offset_ = HMM_MAX(0, atoi(s + 1) - 1);
    }

    cur_state->action_par = become;
}

void DDFStateGetAngle(const char *arg, State *cur_state)
{
    BAMAngle *value;
    float     tmp;

    if (!arg || !arg[0])
        return;

    value = new BAMAngle;

    if (sscanf(arg, " %f ", &tmp) != 1)
        DDFError("DDFStateGetAngle: bad value: %s\n", arg);

    *value = epi::BAMFromDegrees(tmp);

    cur_state->action_par = value;
}

void DDFStateGetSlope(const char *arg, State *cur_state)
{
    float *value, tmp;

    if (!arg || !arg[0])
        return;

    value = new float;

    if (sscanf(arg, " %f ", &tmp) != 1)
        DDFError("DDFStateGetSlope: bad value: %s\n", arg);

    if (tmp > +89.5f)
        tmp = +89.5f;
    if (tmp < -89.5f)
        tmp = -89.5f;

    *value = tan(tmp * HMM_PI / 180.0);

    cur_state->action_par = value;
}

void DDFStateGetRGB(const char *arg, State *cur_state)
{
    if (!arg || !arg[0])
        return;

    cur_state->action_par = new RGBAColor;

    DDFMainGetRGB(arg, cur_state->action_par);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
