//------------------------------------------------------------------------
//  Path to Executable
//------------------------------------------------------------------------
// 
//  Copyright (c) 2006-2008  The EDGE Team.
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

#include "epi.h"
#include "exe_path.h"
#include "path.h"

#ifdef WIN32
#include <io.h>      // access()
#else
#include <unistd.h>  // access(), readlink()
#endif

#ifdef __APPLE__
#include <sys/param.h>
#include <mach-o/dyld.h> // _NSGetExecutablePath
#endif

#include <limits.h>  // PATH_MAX

#ifndef PATH_MAX
#define PATH_MAX  2048
#endif

#include "whereami.h"

namespace epi
{

const char *GetExecutablePath()
{
	int length;
	std::string path;
	length = wai_getExecutablePath(NULL, 0, NULL);
	path.resize(length + 1, 0);
	wai_getExecutablePath(path.data(), length, NULL);
	path = PATH_GetDir(path.data());
	return strdup(path.c_str());
}

//
// Get default path to resources
//
const char* GetResourcePath() 
{
	std::string path = ".";

#ifdef __APPLE__
	// Used infrequently, hence the code is not as brutally 
	// efficiently as it could. Clarity came first here.
	const std::string appdir_suffix = ".app";
	const std::string contents_subdir = "Contents";
	const std::string exe_subdir = "MacOS";
	const std::string res_subdir = "Resources";

	std::string dir_match = appdir_suffix;
	dir_match += '/';
	dir_match += contents_subdir;
	dir_match += '/';
	dir_match += exe_subdir; 

    const char *ep = GetExecutablePath(NULL);
	std::string exe_path = ep;
    free((void*)ep);

	if (exe_path != "")
	{
		std::string::size_type pos = exe_path.rfind(dir_match);
		if (pos != std::string::npos) // Found it 
		{
			// Only alter if it is where it should be (i.e. at the end)
			std::string::size_type expected_pos = 
					exe_path.size() - dir_match.size();
			if (pos == expected_pos)
			{
				// Set path
				path = exe_path;

				// Replace the executable sub directory with
				// the resource sub directory
				pos = exe_path.rfind(exe_subdir);

				path.replace(exe_path.find(exe_subdir), 
							 exe_subdir.size(), 
							 res_subdir);
			}
		}
	}
#endif

	return strdup(path.c_str());
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
