//----------------------------------------------------------------------------
//  EDGE Resolution Handling
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
//
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------
//
// Original Author: Chi Hoang
//

#ifndef __VIDEORES_H__
#define __VIDEORES_H__

#include "i_defs.h"

#include "arrays.h"

// Macros
#define FROM_320(x)  ((x) * SCREENWIDTH  / 320)
#define FROM_200(y)  ((y) * SCREENHEIGHT / 200)

// Screen mode information
class scrmode_c
{
public:
	int width;
	int height;
	int depth;
	int display_mode;

	enum
	{
		SCR_WINDOW,
		SCR_FULLSCREEN,
		SCR_BORDERLESS
	};

public:
	scrmode_c() : width(0), height(0), depth(0), display_mode(SCR_WINDOW)
	{ }
		
	scrmode_c(int _w, int _h, int _depth, int _display_mode) :
		width(_w), height(_h), depth(_depth), display_mode(_display_mode)
	{ }
	
	scrmode_c(const scrmode_c& other) :
		width(other.width), height(other.height),
		depth(other.depth), display_mode(other.display_mode)
	{ }

	~scrmode_c()
	{ }
};


// Exported Vars
extern int SCREENWIDTH;
extern int SCREENHEIGHT;
extern int SCREENBITS;
extern int DISPLAYMODE;
extern scrmode_c borderless_mode;
extern scrmode_c toggle_full_mode;
extern scrmode_c toggle_win_mode;
extern std::vector<scrmode_c *> screen_modes;

// Exported Func
bool R_DepthIsEquivalent(int depth1, int depth2);

void R_AddResolution(scrmode_c *mode);
void R_DumpResList(void);

typedef enum
{
	RESINC_Size = 0,
	RESINC_DisplayMode,
}
increment_res_e;

bool R_IncrementResolution(scrmode_c *mode, int what, int dir);
// update the given screen mode with the next highest (dir=1)
// or next lowest (dir=-1) attribute given by 'what' parameter,
// either the size, depth or fullscreen/windowed mode.  Returns
// true on succses.  If no such resolution exists (whether or
// not the given mode exists) then false is returned.

void R_ToggleFullscreen(void);

void R_InitialResolution(void);
bool R_ChangeResolution(scrmode_c *mode); 

void R_SoftInitResolution(void);

// only call these when it really is time to do the actual resolution
// or view size change, i.e. at the start of a frame.

#endif // __VIDEORES_H__

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
