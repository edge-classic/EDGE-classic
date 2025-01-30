//----------------------------------------------------------------------------
//  EDGE Main
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2024 The EDGE Team.
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

#ifdef EDGE_MIMALLOC
#include <mimalloc.h>
#endif

#include "dm_defs.h"
#include "e_main.h"
#include "epi_filesystem.h"
#include "epi_sdl.h"
#include "epi_str_util.h"
#include "i_system.h"
#include "m_argv.h"
#include "version.h"

std::string executable_path = ".";

extern "C"
{
    int main(int argc, char *argv[])
    {
#ifdef EDGE_MIMALLOC        
        if (SDL_SetMemoryFunctions(mi_malloc, mi_calloc, mi_realloc, mi_free) != 0)
            FatalError("Couldn't init SDL memory functions!!\n%s\n", SDL_GetError());
#endif

        if (SDL_Init(0) < 0)
            FatalError("Couldn't init SDL!!\n%s\n", SDL_GetError());

        executable_path = SDL_GetBasePath();

#ifdef _WIN32
        // -AJA- change current dir to match executable
        if (!epi::CurrentDirectorySet(executable_path))
            FatalError("Couldn't set program directory to %s!!\n", executable_path.c_str());
#endif

        // Run EDGE. it never returns
        EdgeMain(argc, (const char **)argv);

        return 0;
    }

} // extern "C"

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
