//----------------------------------------------------------------------------
//  EDGE Resolution Handling
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
//
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------

#include "i_defs.h"
#include "i_defs_gl.h"

#include <limits.h>

#include <vector>
#include <algorithm>

#include "r_modes.h"
#include "r_gldefs.h"
#include "r_colormap.h"
#include "am_map.h"
#include "r_image.h"
#include "r_units.h"
#include "r_draw.h"
#include "r_wipe.h"

// Globals
int SCREENWIDTH;
int SCREENHEIGHT;
int SCREENBITS;
bool FULLSCREEN;


static std::vector<scrmode_c *> screen_modes;


bool R_DepthIsEquivalent(int depth1, int depth2)
{
	if (depth1 == depth2)
		return true;

	if (MIN(depth1,depth2) == 15 && MAX(depth1,depth2) == 16)
		return true;

	if (MIN(depth1,depth2) == 24 && MAX(depth1,depth2) == 32)
		return true;

	return false;
}

static int SizeDiff(int w1, int h1, int w2, int h2)
{
	return (w1 * 10000 + h1) - (w2 * 10000 + h2);
}


scrmode_c *R_FindResolution(int w, int h, int depth, bool full)
{
	for (int i = 0; i < (int)screen_modes.size(); i++)
	{
		scrmode_c *cur = screen_modes[i];

		if (cur->width == w && cur->height == h &&
			R_DepthIsEquivalent(cur->depth, depth) &&
			cur->full == full)
		{
			return cur;
		}
	}

	return NULL;
}


//
// R_AddResolution
//
// Adds a resolution to the scrmodes list. This is used so we can
// select it within the video options menu.
//
void R_AddResolution(scrmode_c *mode)
{
    scrmode_c *exist = R_FindResolution(mode->width, mode->height,
							mode->depth, mode->full);
	if (exist)
	{
		if (mode->depth != exist->depth)
		{
			// depth is different but equivalent.  Update current
			// member in list, giving preference to power-of-two.
			if (mode->depth == 16 or mode->depth == 32)
				exist->depth = mode->depth;
		}

		return;
	}

	screen_modes.push_back(new scrmode_c(*mode));
}


void R_DumpResList(void)
{
    I_Printf("Available Resolutions:\n");

	for (int i = 0; i < (int)screen_modes.size(); i++)
	{
		scrmode_c *cur = screen_modes[i];

		if (i > 0 && (i % 3) == 0)
			I_Printf("\n");

        I_Printf("  %4dx%4d @ %02d %s", 
                 cur->width, cur->height, cur->depth,
                 cur->full ? "FS " : "win");
	}

	I_Printf("\n");
}

bool R_IncrementResolution(scrmode_c *mode, int what, int dir)
{
	// Algorithm:
	//   for RESINC_Depth and RESINC_Full, we simply toggle the
	//   value in question (depth or full), and find the mode
	//   with matching depth/full and the closest size.
	//
	//   for RESINC_Size, we find modes with matching depth/full
	//   and the *next* closest size (ignoring the same size or
	//   sizes that are in opposite direction to 'dir' param).

	SYS_ASSERT(dir == 1 || dir == -1);

	int depth = mode->depth;
	bool full = mode->full;

	if (what == RESINC_Depth)
		depth = (depth < 20) ? 32 : 16;

	if (what == RESINC_Full)
		full = !full;

	scrmode_c *best = NULL;
	int best_diff = (1 << 30);

	for (int i = 0; i < (int)screen_modes.size(); i++)
	{
		scrmode_c *cur = screen_modes[i];

		if (! R_DepthIsEquivalent(cur->depth, depth))
			continue;

		if (cur->full != full)
			continue;

		int diff = SizeDiff(cur->width, cur->height, mode->width, mode->height);

		if (what == RESINC_Size)
		{
			if (diff * dir <= 0)
				continue;
		}

		diff = ABS(diff);

		if (diff < best_diff)
		{
			best_diff = diff;
			best = cur;

			if (diff == 0)
				break;
		}
	}

	if (best)
	{
		mode->width  = best->width;
		mode->height = best->height;
		mode->depth  = best->depth;
		mode->full   = best->full;

		return true;
	}

	return false;
}


//----------------------------------------------------------------------------


void R_SoftInitResolution(void)
{
	L_WriteDebug("R_SoftInitResolution...\n");

	RGL_NewScreenSize(SCREENWIDTH, SCREENHEIGHT, SCREENBITS);

	// -ES- 1999/08/29 Fixes the garbage palettes, and the blank 16-bit console
	V_SetPalette(PALETTE_NORMAL, 0);
	V_ColourNewFrame();

	// re-initialise various bits of GL state
	RGL_SoftInit();
	RGL_SoftInitUnits();	// -ACB- 2004/02/15 Needed to sort vars lost in res change

	L_WriteDebug("-  returning true.\n");

	return ;
}


static bool DoExecuteChangeResolution(scrmode_c *mode)
{
	RGL_StopWipe();  // delete any wipe texture too

	W_DeleteAllImages();

	bool was_ok = I_SetScreenSize(mode);

	if (! was_ok)
		return false;

	SCREENWIDTH  = mode->width;
	SCREENHEIGHT = mode->height;
	SCREENBITS   = mode->depth;
	FULLSCREEN   = mode->full;

	// gfx card doesn't like to switch too rapidly
	I_Sleep(250);
	I_Sleep(250);

	return true;
}


struct Compare_Res_pred
{
	inline bool operator() (const scrmode_c * A, const scrmode_c * B) const
	{
		if (A->full != B->full)
		{
			return FULLSCREEN ? (A->full > B->full) : (A->full < B->full);
		}

		if (! R_DepthIsEquivalent(A->depth, B->depth))
		{
			int a_equiv = (A->depth < 20) ? 16 : 32;
			int b_equiv = (B->depth < 20) ? 16 : 32;

			return R_DepthIsEquivalent(SCREENBITS, 16) ?
				(a_equiv < b_equiv) : (a_equiv > b_equiv);
		}

		if (A->width != B->width)
		{
			int a_diff_w = ABS(SCREENWIDTH - A->width);
			int b_diff_w = ABS(SCREENWIDTH - B->width);

			return (a_diff_w < b_diff_w);
		}
		else
		{
			int a_diff_h = ABS(SCREENHEIGHT - A->height);
			int b_diff_h = ABS(SCREENHEIGHT - B->height);

			return (a_diff_h < b_diff_h);
		}
	}
};


void R_InitialResolution(void)
{
	L_WriteDebug("R_InitialResolution...\n");

	scrmode_c mode;

	mode.width  = SCREENWIDTH;
	mode.height = SCREENHEIGHT;
	mode.depth  = SCREENBITS;
	mode.full   = FULLSCREEN;

    if (DoExecuteChangeResolution(&mode))
	{
		// this mode worked, make sure it's in the list
		R_AddResolution(&mode);
		return;
	}

	L_WriteDebug("- Looking for another mode to try...\n");

	// sort modes into a good order, choosing sizes near the
	// request size first, and different depths/fullness last.

	std::sort(screen_modes.begin(), screen_modes.end(),
			  Compare_Res_pred());

	for (int i = 0; i < (int)screen_modes.size(); i++)
	{
		if (DoExecuteChangeResolution(screen_modes[i]))
			return;
	}

    // FOOBAR!
	I_Error("Unable to set any resolutions!");
}


bool R_ChangeResolution(scrmode_c *mode)
{
	L_WriteDebug("R_ChangeResolution...\n");

    if (DoExecuteChangeResolution(mode))
		return true;

	L_WriteDebug("- Failed : switching back...\n");

	scrmode_c old_mode;

	old_mode.width  = SCREENWIDTH;
	old_mode.height = SCREENHEIGHT;
	old_mode.depth  = SCREENBITS;
	old_mode.full   = FULLSCREEN;

    if (DoExecuteChangeResolution(&old_mode))
		return false;

	// This ain't good - current and previous resolutions do not work.
	I_Error("Switch back to old resolution failed!\n");
	return false; /* NOT REACHED */
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
