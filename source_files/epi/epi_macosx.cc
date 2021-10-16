//----------------------------------------------------------------------------
//  MacOS X EPI System Specifics
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

#include "epi.h"


namespace epi
{
	static bool inited = false;

    //
    // Init
    //
    bool Init(void)
    {
		if (inited)
			Shutdown();

		inited = true;
        return inited;
    }

    //
    // Shutdown
    //
    void Shutdown(void)
    {
		inited = false;
    }

} // namespace epi

// FIXME: Better place for this?
void strupr(char *str)
{
	if (str)
		for (; *str; str++)
			*str = toupper(*str);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
