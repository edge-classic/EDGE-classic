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

#pragma once

#include <string>
#include <vector>

#include "con_var.h"

extern std::vector<std::string> program_argument_list;

void ParseArguments(int argc, const char *const *argv);

// Return position in arg list, if found
int FindArgument(std::string_view long_name, int *total_parameters = nullptr);

//  Same as above, but return the value of position + 1 if valid, else an empty
//  string
std::string ArgumentValue(std::string_view long_name, int *total_parameters = nullptr);

void CheckBooleanParameter(const std::string &parameter, bool *boolean_value, bool reverse);

void CheckBooleanConsoleVariable(const std::string &parameter, ConsoleVariable *variable, bool reverse);

void ApplyResponseFile(std::string_view name);

void DumpArguments(void);

bool ArgumentIsOption(int index);

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
