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

#include "con_var.h"

#include <string.h>

#include "con_main.h"
#include "filesystem.h"
#include "m_argv.h"
#include "str_compare.h"
#include "str_util.h"

// NOTE: we must use a plain linked list (and not std::vector) here,
//       because constructors run very early (before main is called)
//       and we cannot rely on a std::vector being initialized.

static ConsoleVariable *all_console_variables = nullptr;

ConsoleVariable::ConsoleVariable(const char *name, const char *def,
                                 ConsoleVariableFlag     flags,
                                 ConsoleVariableCallback cb, float min,
                                 float max)
    : d_(),
      f_(),
      s_(def),
      name_(name),
      def_(def),
      flags_(flags),
      min_(min),
      max_(max),
      callback_(cb),
      modified_(0)
{
    ParseString();

    // add this cvar into the list.  it is sorted later.
    next_                 = all_console_variables;
    all_console_variables = this;
}

ConsoleVariable::~ConsoleVariable()
{
    // nothing needed
}

ConsoleVariable &ConsoleVariable::operator=(int value)
{
    if (value < min_ || value > max_)
    {
        LogWarning(
            "Value %d exceeds lower/upper limits for %s! Resetting to default "
            "value!\n",
            value, name_);
        s_ = def_;
        ParseString();
    }
    else
    {
        d_ = value;
        f_ = value;
        FormatInteger(value);
    }

    if (callback_) { callback_(this); }
    modified_++;
    return *this;
}

ConsoleVariable &ConsoleVariable::operator=(float value)
{
    if (value < min_ || value > max_)
    {
        LogWarning(
            "Value %g exceeds lower/upper limits for %s! Resetting to default "
            "value!\n",
            value, name_);
        s_ = def_;
        ParseString();
    }
    else
    {
        d_ = RoundToInteger(value);
        f_ = value;
        FormatFloat(value);
    }

    if (callback_) { callback_(this); }
    modified_++;
    return *this;
}

ConsoleVariable &ConsoleVariable::operator=(const char *value)
{
    s_ = value;
    ParseString();

    if (callback_) { callback_(this); }
    modified_++;
    return *this;
}

ConsoleVariable &ConsoleVariable::operator=(std::string value)
{
    s_ = value;
    ParseString();

    if (callback_) { callback_(this); }
    modified_++;
    return *this;
}

// private method
void ConsoleVariable::FormatInteger(int value)
{
    char buffer[64];
    sprintf(buffer, "%d", value);
    s_ = buffer;
}

// private method
void ConsoleVariable::FormatFloat(float value)
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

    s_ = buffer;
}

// private method
void ConsoleVariable::ParseString()
{
    d_ = atoi(s_.c_str());
    f_ = atof(s_.c_str());
    if (f_ < min_ || f_ > max_)
    {
        LogWarning(
            "Value %g exceeds lower/upper limits for %s! Resetting to default "
            "value!\n",
            f_, name_);
        s_ = def_;
        d_ = atoi(s_.c_str());
        f_ = atof(s_.c_str());
    }
}

//----------------------------------------------------------------------------

static ConsoleVariable *MergeSort(ConsoleVariable *list)
{
    SYS_ASSERT(list != nullptr);

    // only a single item?  done!
    if (list->next_ == nullptr) return list;

    // split into left and right lists
    ConsoleVariable *L = nullptr;
    ConsoleVariable *R = nullptr;

    while (list != nullptr)
    {
        ConsoleVariable *var = list;
        list                 = list->next_;

        var->next_ = L;
        L          = var;

        std::swap(L, R);
    }

    L = MergeSort(L);
    R = MergeSort(R);

    // now merge them
    ConsoleVariable *tail = nullptr;

    while (L != nullptr || R != nullptr)
    {
        // pick the smallest name
        if (L == nullptr)
            std::swap(L, R);
        else if (R != nullptr &&
                 epi::StringCaseCompareASCII(L->name_, R->name_) > 0)
            std::swap(L, R);

        // remove it, add to tail of the new list
        ConsoleVariable *var = L;
        L                    = L->next_;

        if (list == nullptr)
            list = var;
        else
            tail->next_ = var;

        var->next_ = nullptr;
        tail       = var;
    }

    return list;
}

void ConsoleSortVariables()
{
    all_console_variables = MergeSort(all_console_variables);
}

void ConsoleResetAllVariables()
{
    for (ConsoleVariable *var = all_console_variables; var != nullptr;
         var                  = var->next_)
    {
        if (!(var->flags_ & kConsoleVariableFlagNoReset)) *var = var->def_;
    }
}

ConsoleVariable *ConsoleFindVariable(const char *name)
{
    for (ConsoleVariable *var = all_console_variables; var != nullptr;
         var                  = var->next_)
    {
        if (epi::StringCaseCompareASCII(var->name_, name) == 0) return var;
    }

    return nullptr;
}

bool ConsoleMatchPattern(const char *name, const char *pat)
{
    while (*name && *pat)
    {
        if (*name != *pat) return false;

        name++;
        pat++;
    }

    return (*pat == 0);
}

int ConsoleMatchAllVariables(std::vector<const char *> &list,
                             const char                *pattern)
{
    list.clear();

    for (ConsoleVariable *var = all_console_variables; var != nullptr;
         var                  = var->next_)
    {
        if (!ConsoleMatchPattern(var->name_, pattern)) continue;

        list.push_back(var->name_);
    }

    return (int)list.size();
}

void ConsoleHandleProgramArguments(void)
{
    for (size_t p = 1; p < program_argument_list.size(); p++)
    {
        if (!ArgumentIsOption(p)) continue;

        std::string s = program_argument_list[p];

        ConsoleVariable *var = ConsoleFindVariable(s.data() + 1);

        if (var == nullptr) continue;

        p++;

        if (p >= program_argument_list.size() || ArgumentIsOption(p))
        {
            LogWarning("Missing value for option: %s\n", s.c_str());
            continue;
        }

        // FIXME allow kConsoleVariableFlagReadOnly here ?

        *var = program_argument_list[p].c_str();
    }
}

int ConsolePrintVariables(const char *match, bool show_default)
{
    int total = 0;

    for (ConsoleVariable *var = all_console_variables; var != nullptr;
         var                  = var->next_)
    {
        if (match && *match)
            if (!strstr(var->name_, match)) continue;

        if (show_default)
            LogPrint("  %-20s \"%s\" (%s)\n", var->name_, var->c_str(),
                     var->def_);
        else
            LogPrint("  %-20s \"%s\"\n", var->name_, var->c_str());

        total++;
    }

    return total;
}

void ConsoleWriteVariables(FILE *f)
{
    for (ConsoleVariable *var = all_console_variables; var != nullptr;
         var                  = var->next_)
    {
        if (var->flags_ & kConsoleVariableFlagArchive)
        {
            std::string line;
            if (var->flags_ & kConsoleVariableFlagFilepath)
                line = epi::SanitizePath(epi::StringFormat(
                    "/%s\t\"%s\"\n", var->name_, var->c_str()));
            else
                line = epi::StringFormat("/%s\t\"%s\"\n", var->name_,
                                         var->c_str());
            fwrite(line.data(), line.size(), 1, f);
        }
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
