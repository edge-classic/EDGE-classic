//----------------------------------------------------------------------------
//  EDGE Path Handling Methods
//----------------------------------------------------------------------------
//
//  Copyright (c) 2003-2023  The EDGE Team.
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
//
#ifndef __EPI_PATH_MODULE__
#define __EPI_PATH_MODULE__

#include <filesystem>

namespace epi
{

// *** Path Manipulation Functions ***


// Returns the basename (filename minus extension) if it exists
std::filesystem::path PATH_GetBasename(std::filesystem::path path);

// Returns the directory from the path if it exists
std::filesystem::path PATH_GetDir(std::filesystem::path path);

// Returns a filename extension from the path if it exists
std::filesystem::path PATH_GetExtension(std::filesystem::path path);

// Returns a filename from the path if it exists
std::filesystem::path PATH_GetFilename(std::filesystem::path path);

// Returns true if the given is an absolute path
bool PATH_IsAbsolute(std::filesystem:: path);

// Join two paths together
std::filesystem::path PATH_Join(std::filesystem::path lhs, std::string rhs);
#ifdef _WIN32
std::filesystem::path PATH_Join(std::filesystem::path lhs, std::u32string rhs);
#endif


} // namespace epi

#endif /* __EPI_PATH_MODULE__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
