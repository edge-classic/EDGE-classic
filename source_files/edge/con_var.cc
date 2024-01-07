//----------------------------------------------------------------------------
//  EDGE Console Variables
//----------------------------------------------------------------------------
//
//  Copyright (c) 2007-2024 The EDGE Team.
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

#include "i_defs.h"

#include "con_var.h"
#include "con_main.h"

#include "m_argv.h"

#include "str_util.h"

// NOTE: we must use a plain linked list (and not std::vector) here,
//       because constructors run very early (before main is called)
//       and we cannot rely on a std::vector being initialized.

static cvar_c *all_cvars = NULL;

cvar_c::cvar_c(const char *_name, const char *_def, int _flags, float _min, float _max, cvar_callback _cb)
    : d(), f(), s(_def), name(_name), def(_def), flags(_flags), min(_min), max(_max), cvar_cb(_cb), modified(0)
{
    ParseString();

    // add this cvar into the list.  it is sorted later.
    next      = all_cvars;
    all_cvars = this;
}

cvar_c::~cvar_c()
{
    // nothing needed
}

cvar_c &cvar_c::operator=(int value)
{
    if (value < min || value > max)
    {
        I_Warning("Value %d exceeds lower/upper limits for %s! Resetting to default value!\n", value, name);
        s = def;
        ParseString();
    }
    else
    {
        d = value;
        f = value;
        FmtInt(value);
    }

    if (cvar_cb)
    {
        cvar_cb(this);
    }
    modified++;
    return *this;
}

cvar_c &cvar_c::operator=(float value)
{
    if (value < min || value > max)
    {
        I_Warning("Value %g exceeds lower/upper limits for %s! Resetting to default value!\n", value, name);
        s = def;
        ParseString();
    }
    else
    {
        d = I_ROUND(value);
        f = value;
        FmtFloat(value);
    }

    if (cvar_cb)
    {
        cvar_cb(this);
    }
    modified++;
    return *this;
}

cvar_c &cvar_c::operator=(const char *value)
{
    s = value;
    ParseString();

    if (cvar_cb)
    {
        cvar_cb(this);
    }
    modified++;
    return *this;
}

cvar_c &cvar_c::operator=(std::string value)
{
    s = value;
    ParseString();

    if (cvar_cb)
    {
        cvar_cb(this);
    }
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

    if (ab >= 1e10) // handle huge numbers
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
    if (f < min || f > max)
    {
        I_Warning("Value %g exceeds lower/upper limits for %s! Resetting to default value!\n", f, name);
        s = def;
        d = atoi(s.c_str());
        f = atof(s.c_str());
    }
}

//----------------------------------------------------------------------------

static cvar_c *MergeSort(cvar_c *list)
{
    SYS_ASSERT(list != NULL);

    // only a single item?  done!
    if (list->next == NULL)
        return list;

    // split into left and right lists
    cvar_c *L = NULL;
    cvar_c *R = NULL;

    while (list != NULL)
    {
        cvar_c *var = list;
        list        = list->next;

        var->next = L;
        L         = var;

        std::swap(L, R);
    }

    L = MergeSort(L);
    R = MergeSort(R);

    // now merge them
    cvar_c *tail = NULL;

    while (L != NULL || R != NULL)
    {
        // pick the smallest name
        if (L == NULL)
            std::swap(L, R);
        else if (R != NULL && epi::case_cmp(L->name, R->name) > 0)
            std::swap(L, R);

        // remove it, add to tail of the new list
        cvar_c *var = L;
        L           = L->next;

        if (list == NULL)
            list = var;
        else
            tail->next = var;

        var->next = NULL;
        tail      = var;
    }

    return list;
}

void CON_SortVars()
{
    all_cvars = MergeSort(all_cvars);
}

void CON_ResetAllVars()
{
    for (cvar_c *var = all_cvars; var != NULL; var = var->next)
    {
        if (!(var->flags & CVAR_NO_RESET))
            *var = var->def;
    }
}

cvar_c *CON_FindVar(const char *name)
{
    for (cvar_c *var = all_cvars; var != NULL; var = var->next)
    {
        if (epi::case_cmp(var->name, name) == 0)
            return var;
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

int CON_MatchAllVars(std::vector<const char *> &list, const char *pattern)
{
    list.clear();

    for (cvar_c *var = all_cvars; var != NULL; var = var->next)
    {
        if (!CON_MatchPattern(var->name, pattern))
            continue;

        list.push_back(var->name);
    }

    return (int)list.size();
}

void CON_HandleProgramArgs(void)
{
    for (size_t p = 1; p < argv::list.size(); p++)
    {
        if (!argv::IsOption(p))
            continue;

        std::string s = argv::list[p];

        cvar_c *var = CON_FindVar(s.data() + 1);

        if (var == nullptr)
            continue;

        p++;

        if (p >= argv::list.size() || argv::IsOption(p))
        {
            I_Warning("Missing value for option: %s\n", s.c_str());
            continue;
        }

        // FIXME allow CVAR_ROM here ?

        *var = argv::list[p].c_str();
    }
}

int CON_PrintVars(const char *match, bool show_default)
{
    int total = 0;

    for (cvar_c *var = all_cvars; var != NULL; var = var->next)
    {
        if (match && *match)
            if (!strstr(var->name, match))
                continue;

        if (show_default)
            I_Printf("  %-20s \"%s\" (%s)\n", var->name, var->c_str(), var->def);
        else
            I_Printf("  %-20s \"%s\"\n", var->name, var->c_str());

        total++;
    }

    return total;
}

void CON_WriteVars(FILE *f)
{
    for (cvar_c *var = all_cvars; var != NULL; var = var->next)
    {
        if ((var->flags & CVAR_ARCHIVE) != 0)
        {
            if (var->flags & CVAR_PATH)
            {
                std::string line = epi::STR_Format("/%s\t\"%s\"\n", var->name,
                                                   std::filesystem::u8path(var->s).generic_u8string().c_str());
                fwrite(line.data(), line.size(), 1, f);
            }
            else
            {
                std::string line = epi::STR_Format("/%s\t\"%s\"\n", var->name, var->c_str());
                fwrite(line.data(), line.size(), 1, f);
            }
        }
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
