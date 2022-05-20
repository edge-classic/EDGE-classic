//----------------------------------------------------------------------------
//  Win32 EDGE System Specifics
//----------------------------------------------------------------------------
//
//  Copyright (c) 2002-2008  The EDGE Team.
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

#include "epi.h"


namespace epi
{

	static bool inited = false;

    //
    // Init
    //
    bool Init(void)
    {
		bool ok;

		if (inited)
			Shutdown();

		ok = true;

		inited = ok;
        return ok;
    }

    //
    // Shutdown
    //
    void Shutdown(void)
    {
		inited = false;
    }

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
