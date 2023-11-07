//----------------------------------------------------------------------------
//  EDGE Main
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2023  The EDGE Team.
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

#include "i_defs.h"
#include "i_sdlinc.h" // needed for proper SDL main linkage
#include "filesystem.h"
#include "str_util.h"

#include "dm_defs.h"
#include "m_argv.h"
#include "e_main.h"
#include "version.h"

std::filesystem::path exe_path = ".";

extern "C"
{

    int main(int argc, char *argv[])
    {
        if (SDL_Init(0) < 0)
            I_Error("Couldn't init SDL!!\n%s\n", SDL_GetError());

        exe_path = std::filesystem::u8path(SDL_GetBasePath());

#ifdef _WIN32
        // -AJA- change current dir to match executable
        epi::FS_SetCurrDir(exe_path);
#endif

        // Run EDGE. it never returns
        E_Main(argc, (const char **)argv);

        return 0;
    }

} // extern "C"

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
