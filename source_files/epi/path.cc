//----------------------------------------------------------------------------
//  EDGE Path Handling Methods for Win32
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
#include "epi.h"
#include "path.h"

namespace epi
{

// Path Manipulation Functions
std::filesystem::path PATH_GetDir(std::filesystem::path path)
{
	SYS_ASSERT(!path.empty());

	return std::filesystem::path(path).remove_filename();
}


std::filesystem::path PATH_GetFilename(std::filesystem::path path)
{
	SYS_ASSERT(!path.empty());

	return std::filesystem::path(path).filename();
}


std::filesystem::path PATH_GetExtension(std::filesystem::path path)
{
	SYS_ASSERT(!path.empty());

	return std::filesystem::path(path).extension();
}


std::filesystem::path PATH_GetBasename(std::filesystem::path path) 
{
	SYS_ASSERT(!path.empty());

	return std::filesystem::path(path).stem();
}

bool PATH_IsAbsolute(std::filesystem::path path)
{
	SYS_ASSERT(!path.empty());

	return std::filesystem::path(path).is_absolute();
}

std::filesystem::path PATH_Join(std::filesystem::path lhs, std::string rhs)
{
	SYS_ASSERT(!lhs.empty() && !rhs.empty());

	return std::filesystem::path(lhs).append(rhs);
}

#ifdef _WIN32
std::filesystem::path PATH_Join(std::filesystem::path lhs, std::u32string rhs)
{
	SYS_ASSERT(!lhs.empty() && !rhs.empty());

	return std::filesystem::path(lhs).append(rhs);
}
#endif


} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
