//------------------------------------------------------------------------
//  EDGE Arguments/Parameters Code
//------------------------------------------------------------------------
//
//  Copyright (C) 2022 The EDGE Team
//  Copyright (C) 2021-2022 The OBSIDIAN Team
//  Copyright (C) 2006-2017 Andrew Apted
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
//------------------------------------------------------------------------

#ifndef M_ARGV_H_
#define M_ARGV_H_

#include <string>
#include <vector>
#include <filesystem>

namespace argv {

#ifdef _WIN32
extern std::vector<std::u32string> list;
#else
extern std::vector<std::string> list;
#endif

void Init(int argc, const char *const *argv);

// Return position in arg list, if found
#ifdef _WIN32
int Find(std::u32string longName, int *numParams = nullptr);
#else
int Find(std::string longName, int *numParams = nullptr);
#endif

//  Same as above, but return the value of position + 1 if valid, else an empty string
#ifdef _WIN32
std::u32string Value(std::u32string longName, int *numParams = nullptr);
#else
std::string Value(std::string longName, int *numParams = nullptr);
#endif

#ifdef _WIN32
void CheckBooleanParm(std::u32string parm, bool *boolval, bool reverse);
void CheckBooleanCVar(std::u32string parm, cvar_c *var, bool reverse);
#else
void CheckBooleanParm(std::string parm, bool *boolval, bool reverse);
void CheckBooleanCVar(std::string parm, cvar_c *var, bool reverse);
#endif

void ApplyResponseFile(std::filesystem::path name);

void DebugDumpArgs(void);

bool IsOption(int index);

}  // namespace argv

#endif /* __LIB_ARGV_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
