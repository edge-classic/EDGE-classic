//----------------------------------------------------------------------------
//  EDGE Linux Main
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2008  The EDGE Team.
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
// DESCRIPTION:
//      Main program, simply calls E_Main high level loop.
//

#include "i_defs.h"

#include <unistd.h>

#ifdef MACOSX
#include <SDL.h>  // needed for proper SDL main linkage
#endif

#include "dm_defs.h"
#include "m_argv.h"
#include "e_main.h"

// cleanup handling -- killough:

static void I_SignalHandler(int s)
{
	// CPhipps - report but don't crash on SIGPIPE
	if (s == SIGPIPE)
	{
		// -AJA- linux signals reset when raised.
		signal(SIGPIPE, I_SignalHandler);

		fprintf(stderr, "EDGE: Broken pipe\n");
		return;
	}

	signal(s, SIG_IGN);    // Ignore future instances of this signal.

	switch (s)
	{
		case SIGSEGV: I_Error("EDGE: Segmentation Violation"); break;
		case SIGINT:  I_Error("EDGE: Interrupted by User"); break;
		case SIGILL:  I_Error("EDGE: Illegal Instruction"); break;
		case SIGFPE:  I_Error("EDGE: Floating Point Exception"); break;
		case SIGTERM: I_Error("EDGE: Killed"); break;
	}

	I_Error("EDGE: Terminated by signal %d", s);
}

#if 0
void ChangeToExeDir(const char *full_path)
{
	const char *r = strrchr(full_path, '/');

	if (r == NULL || r == full_path)
		return;

#ifdef MACOSX
        // -AJA- It seems argv[0] points directly to the "gledge" binary
        //       inside of the Edge.app folder (when run from the Finder).
        //       Hence we need to strip the extra bits off.
        const char *app = r - 4;

        for (; app > full_path; app--)
        {
            if (app[0] == '.' && app[1] == 'a' && app[2] == 'p' &&
                app[3] == 'p' && app[4] == '/')
              break;
        }
        if (app > full_path)
        {
          for (; app > full_path; app--)
            if (app[0] == '/')
              break;
        }
        if (app > full_path)
          r = app;
#endif
	int length = (r - full_path) + 1;

	std::string str;
	str.AddChars(full_path, 0, length);

	chdir(str.c_str());
}
#endif


#ifdef MACOSX
int main(int argc, char *argv[])
#else
int main(int argc, const char **argv)
#endif
{
	signal(SIGPIPE, I_SignalHandler); // CPhipps - add SIGPIPE, as this is fatal

#ifdef DEVELOPERS

	// -AJA- Disable signal handlers, otherwise we don't get core dumps
	//       and core dumps are _DAMN_ useful for debugging.

#else
	signal(SIGSEGV, I_SignalHandler);
	signal(SIGTERM, I_SignalHandler);
	signal(SIGILL,  I_SignalHandler);
	signal(SIGFPE,  I_SignalHandler);
	signal(SIGILL,  I_SignalHandler);
	signal(SIGINT,  I_SignalHandler);  // killough 3/6/98: allow CTRL-BRK during init
	signal(SIGABRT, I_SignalHandler);
#endif

	// Run EDGE. it never returns
	E_Main(argc, (const char **) argv);

	return 0;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
