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
#include "filesystem.h"
#include "str_util.h"

#include "dm_defs.h"
#include "m_argv.h"
#include "e_main.h"
#include "version.h"

#ifdef EDGE_WEB
	#include <emscripten.h>
#endif

std::filesystem::path exe_path = ".";

#ifdef EDGE_WEB
void E_WebTick(void)
{
	// We always do this once here, although the engine may
	// makes in own calls to keep on top of the event processing
	I_ControlGetEvents(); 

	if (app_state & APP_STATE_ACTIVE)
		E_Tick();
}
#endif

extern "C" {

int main(int argc, char *argv[])
{

#ifdef EDGE_WEB
	emscripten_set_main_loop(E_WebTick, 0, 0);	
#endif	

	if (SDL_Init(0) < 0)
		I_Error("Couldn't init SDL!!\n%s\n", SDL_GetError());

	exe_path = UTFSTR(SDL_GetBasePath());

#ifdef _WIN32
    // -AJA- change current dir to match executable
    epi::FS_SetCurrDir(exe_path);
#endif

	// Run EDGE. it never returns
	E_Main(argc, (const char **) argv);

	return 0;
}

} // extern "C"


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
