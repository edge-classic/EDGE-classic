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

#ifndef M_ARGV_H_
#define M_ARGV_H_

#include <string>
#include <vector>

#include "con_var.h"

namespace argv
{

extern std::vector<std::string> list;

void Init(int argc, const char *const *argv);

// Return position in arg list, if found
int Find(std::string longName, int *numParams = nullptr);

//  Same as above, but return the value of position + 1 if valid, else an empty string
std::string Value(std::string longName, int *numParams = nullptr);

void CheckBooleanParm(std::string parm, bool *boolval, bool reverse);
void CheckBooleanCVar(std::string parm, ConsoleVariable *var, bool reverse);

void ApplyResponseFile(std::string name);

void DebugDumpArgs(void);

bool IsOption(int index);

} // namespace argv

#endif /* __LIB_ARGV_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
