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

#ifndef __EPI_EXE_PATH_H__
#define __EPI_EXE_PATH_H__

namespace epi
{

const char *GetExecutablePath(const char *argv0);
// returns the path containing the running executable.
// You must pass in argv[0], which is used as a last resort
// when better methods fail.  The returned path never has a
// trailing directory separator (and is never NULL).

const char* GetResourcePath();
// Attempts to determine an appropriate directory to find resources
// for an application. Useful only on platforms with formalised
// organisation of application directories. (e.g. MacOS X)

} // namespace epi

#endif  /* __EPI_EXE_PATH_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
