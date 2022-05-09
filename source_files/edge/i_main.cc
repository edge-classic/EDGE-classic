//----------------------------------------------------------------------------
//  EDGE Main
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2009  The EDGE Team.
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

#include "i_defs.h"
#include "i_sdlinc.h"  // needed for proper SDL main linkage

#include "exe_path.h"

#include "dm_defs.h"
#include "m_argv.h"
#include "e_main.h"
#include "version.h"

const char *win32_exe_path = ".";

extern "C" {

int main(int argc, char *argv[])
{
#ifdef WIN32
	const char* title_string = "EDGE Engine";
	// -AJA- give us a proper name in the Task Manager
	SDL_RegisterApp((char *)title_string, 0, 0);
#endif

#ifdef WIN32
    // -AJA- change current dir to match executable
	win32_exe_path = epi::GetExecutablePath(argv[0]);

    ::SetCurrentDirectory(win32_exe_path);
#else
    // -ACB- 2005/11/26 We don't do on LINUX since we assume the 
    //                  executable is globally installed
#endif

	// Run EDGE. it never returns
	E_Main(argc, (const char **) argv);

	return 0;
}

} // extern "C"


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
