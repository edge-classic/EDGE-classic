//----------------------------------------------------------------------------
//  EDGE Path Handling Methods for Win32
//----------------------------------------------------------------------------
//
//  Copyright (c) 2003-2008  The EDGE Team.
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
//
#include "epi.h"
#include "path.h"

namespace epi
{

// Path Manipulation Functions
std::string PATH_GetDir(const char *path)
{
	SYS_ASSERT(path);

	const char *p = path + strlen(path) - 1;

	// back up until a slash or the start
	for (; p > path; p--)
		if (PATH_IsDirSep(*p))
			return std::string(path, (p - path));

    return std::string();  // nothing
}


std::string PATH_GetFilename(const char *path)
{
	SYS_ASSERT(path);

	const char *p = path + strlen(path) - 1;

	// back up until a slash or the start
	for (; p >= path; p--)
		if (PATH_IsDirSep(*p))
			return std::string(p + 1);

    return std::string(path);
}


std::string PATH_GetExtension(const char *path)
{
	SYS_ASSERT(path);

	const char *p = path + strlen(path) - 1;
	
	// back up until a dot
	for (; p >= path; p--)
	{
		if (PATH_IsDirSep(*p))
			break;

		if (*p == '.')
		{
            // handle filenames that being with a dot
            // (un*x style hidden files)
            if (p == path || PATH_IsDirSep(p[-1]))
				break;

			return std::string(p + 1);
		}
	}

    return std::string();  // nothing
}


std::string PATH_GetBasename(const char *path) 
{
	SYS_ASSERT(path);

	const char *p = path + strlen(path) - 1;
	const char *r = p;

	// back up until a slash or the start
	for (; p > path; p--)
	{
		if (PATH_IsDirSep(*p))
		{
			p++; break;
		}
	}

	SYS_ASSERT(p >= path);
	
	// back up until a dot
	for (; r >= p; r--)
	{
		if (*r == '.')
		{
            // handle filenames that being with a dot
            // (un*x style hidden files)
            if (r == p || PATH_IsDirSep(r[-1]))
				break;

			return std::string(p, r - p);
		}
	}

    return std::string(p);
}



bool PATH_IsAbsolute(const char *path)
{
	SYS_ASSERT(path);

#ifdef WIN32
    // Check for Drive letter, colon and slash...
    if (strlen(path) > 2 && path[1] == ':' && 
        (path[2] == '\\' || path[2] == '/') && 
        isalpha(path[0]))
    {
        return true;
    }

    // Check for share name...
    if (strlen(path) > 1 && path[0] == '\\' && path[1] == '\\')
        return true;

#else // LINUX

	if (PATH_IsDirSep(path[0]))
		return true;
#endif

    return false;
}


bool PATH_IsDirSep(const char c)
{
#ifdef WIN32
    return (c == '\\' || c == '/' || c == ':'); // Kester added ':'
#else // LINUX
    return (c == '\\' || c == '/');
#endif
}


std::string PATH_Join(const char *lhs, const char *rhs)
{
	SYS_ASSERT(lhs && rhs);

	if (PATH_IsAbsolute(rhs))
		return std::string(rhs);

    std::string result(lhs);

	if (result.size() > 0)
	{
		const char c = result[result.size()-1];

#ifdef WIN32
		if (c != '\\' && c != '/')
			result += DIRSEPARATOR;
#else // LINUX
		if (c != '/')
			result += DIRSEPARATOR;
#endif
	}

	result += rhs;

    return result;
}


} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
