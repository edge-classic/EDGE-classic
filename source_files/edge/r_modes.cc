//----------------------------------------------------------------------------
//  EDGE Resolution Handling
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
//
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------


#include "i_defs_gl.h"
#include "i_system.h"

#include <limits.h>

#include <vector>
#include <algorithm>

#include "hu_font.h" // current_font_size
#include "r_modes.h"
#include "r_gldefs.h"
#include "r_colormap.h"
#include "am_map.h"
#include "r_image.h"
#include "r_misc.h"
#include "r_units.h"
#include "r_draw.h"
#include "r_wipe.h"

// Globals
int SCREENWIDTH;
int SCREENHEIGHT;
int SCREENBITS;
int DISPLAYMODE;

DisplayMode borderless_mode;
EDGE_DEFINE_CONSOLE_VARIABLE(tf_screenwidth, "0", kConsoleVariableFlagArchive)
EDGE_DEFINE_CONSOLE_VARIABLE(tf_screenheight, "0", kConsoleVariableFlagArchive)
EDGE_DEFINE_CONSOLE_VARIABLE(tf_screendepth, "0", kConsoleVariableFlagArchive)
EDGE_DEFINE_CONSOLE_VARIABLE(tf_displaymode, "-1", kConsoleVariableFlagArchive)
EDGE_DEFINE_CONSOLE_VARIABLE(tw_screenwidth, "0", kConsoleVariableFlagArchive)
EDGE_DEFINE_CONSOLE_VARIABLE(tw_screenheight, "0", kConsoleVariableFlagArchive)
EDGE_DEFINE_CONSOLE_VARIABLE(tw_screendepth, "0", kConsoleVariableFlagArchive)
EDGE_DEFINE_CONSOLE_VARIABLE(tw_displaymode, "-1", kConsoleVariableFlagArchive)

std::vector<DisplayMode *> screen_modes;

bool R_DepthIsEquivalent(int depth1, int depth2)
{
    if (depth1 == depth2)
        return true;

    if (HMM_MIN(depth1, depth2) == 15 && HMM_MAX(depth1, depth2) == 16)
        return true;

    if (HMM_MIN(depth1, depth2) == 24 && HMM_MAX(depth1, depth2) == 32)
        return true;

    return false;
}

static int SizeDiff(int w1, int h1, int w2, int h2)
{
    return (w1 * 10000 + h1) - (w2 * 10000 + h2);
}

static DisplayMode *R_FindResolution(int w, int h, int depth, int display_mode)
{
    for (int i = 0; i < (int)screen_modes.size(); i++)
    {
        DisplayMode *cur = screen_modes[i];

        if (cur->width == w && cur->height == h && R_DepthIsEquivalent(cur->depth, depth) &&
            cur->display_mode == display_mode)
        {
            return cur;
        }
    }

    return nullptr;
}

//
// R_AddResolution
//
// Adds a resolution to the scrmodes list. This is used so we can
// select it within the video options menu.
//
void R_AddResolution(DisplayMode *mode)
{
    DisplayMode *exist = R_FindResolution(mode->width, mode->height, mode->depth, mode->display_mode);
    if (exist)
    {
        if (mode->depth != exist->depth)
        {
            // depth is different but equivalent.  Update current
            // member in list, giving preference to power-of-two.
            if (mode->depth == 16 || mode->depth == 32)
                exist->depth = mode->depth;
        }

        return;
    }

    screen_modes.push_back(new DisplayMode(*mode));
}

void R_DumpResList(void)
{
    LogPrint("Available Resolutions:\n");

    for (int i = 0; i < (int)screen_modes.size(); i++)
    {
        DisplayMode *cur = screen_modes[i];

        if (i > 0 && (i % 3) == 0)
            LogPrint("\n");

        LogPrint("  %4dx%4d @ %02d %s", cur->width, cur->height, cur->depth,
                 cur->display_mode == cur->SCR_BORDERLESS ? "BL"
                                                          : (cur->display_mode == cur->SCR_FULLSCREEN ? "FS " : "win"));
    }

    LogPrint("\n");
}

bool R_IncrementResolution(DisplayMode *mode, int what, int dir)
{
    // Algorithm:
    //   for RESINC_DisplayMode, we simply adjust the
    //   value in question, and find the mode
    //   with matching display_mode and the closest size.
    //
    //   for RESINC_Size, we find modes with matching depth/display_mode
    //   and the *next* closest size (ignoring the same size or
    //   sizes that are in opposite direction to 'dir' param).

    SYS_ASSERT(dir == 1 || dir == -1);

    int depth        = mode->depth;
    int display_mode = mode->display_mode;

    if (what == RESINC_DisplayMode)
    {
        if (dir == 1)
            display_mode = (display_mode + 1) % 3;
        else
        {
            if (display_mode > 0)
                display_mode--;
            else
                display_mode = 2;
        }
    }

    if (display_mode == 2)
    {
        mode->width        = borderless_mode.width;
        mode->height       = borderless_mode.height;
        mode->depth        = borderless_mode.depth;
        mode->display_mode = borderless_mode.display_mode;

        return true;
    }

    DisplayMode *best      = nullptr;
    int        best_diff = (1 << 30);

    for (int i = 0; i < (int)screen_modes.size(); i++)
    {
        DisplayMode *cur = screen_modes[i];

        if (!R_DepthIsEquivalent(cur->depth, depth))
            continue;

        if (cur->display_mode != display_mode)
            continue;

        int diff = SizeDiff(cur->width, cur->height, mode->width, mode->height);

        if (what == RESINC_Size)
        {
            if (diff * dir <= 0)
                continue;
        }

        diff = HMM_ABS(diff);

        if (diff < best_diff)
        {
            best_diff = diff;
            best      = cur;

            if (diff == 0)
                break;
        }
    }

    if (best)
    {
        mode->width        = best->width;
        mode->height       = best->height;
        mode->depth        = best->depth;
        mode->display_mode = best->display_mode;

        return true;
    }

    return false;
}

void R_ToggleFullscreen(void)
{
    DisplayMode toggle;
    if (DISPLAYMODE > DisplayMode::SCR_WINDOW)
    {
        toggle.depth        = tw_screendepth.d_;
        toggle.height       = tw_screenheight.d_;
        toggle.width        = tw_screenwidth.d_;
        toggle.display_mode = DisplayMode::SCR_WINDOW;
        R_ChangeResolution(&toggle);
    }
    else
    {
        toggle.depth        = tf_screendepth.d_;
        toggle.height       = tf_screenheight.d_;
        toggle.width        = tf_screenwidth.d_;
        toggle.display_mode = tf_displaymode.d_;
        R_ChangeResolution(&toggle);
    }
    R_SoftInitResolution();
}

//----------------------------------------------------------------------------

void R_SoftInitResolution(void)
{
    LogDebug("R_SoftInitResolution...\n");

    RGL_NewScreenSize(SCREENWIDTH, SCREENHEIGHT, SCREENBITS);

    if (SCREENWIDTH < 720)
        current_font_size = 0;
    else if (SCREENWIDTH < 1440)
        current_font_size = 1;
    else
        current_font_size = 2;

    // -ES- 1999/08/29 Fixes the garbage palettes, and the blank 16-bit console
    V_SetPalette(PALETTE_NORMAL, 0);

    // re-initialise various bits of GL state
    RGL_SoftInit();
    RGL_SoftInitUnits(); // -ACB- 2004/02/15 Needed to sort vars lost in res change

    LogDebug("-  returning true.\n");

    return;
}

static bool DoExecuteChangeResolution(DisplayMode *mode)
{
    RGL_StopWipe(); // delete any wipe texture too

    W_DeleteAllImages();

    bool was_ok = SetScreenSize(mode);

    if (!was_ok)
        return false;

    SCREENWIDTH  = mode->width;
    SCREENHEIGHT = mode->height;
    SCREENBITS   = mode->depth;
    DISPLAYMODE  = mode->display_mode;

    if (SCREENWIDTH < 720)
        current_font_size = 0;
    else if (SCREENWIDTH < 1440)
        current_font_size = 1;
    else
        current_font_size = 2;

    DeterminePixelAspect();

    LogPrint("Pixel aspect: %1.3f\n", pixel_aspect_ratio.f_);

    // gfx card doesn't like to switch too rapidly
    SleepForMilliseconds(250);
    SleepForMilliseconds(250);

    return true;
}

struct Compare_Res_pred
{
    inline bool operator()(const DisplayMode *A, const DisplayMode *B) const
    {
        if (A->display_mode != B->display_mode)
        {
            return DISPLAYMODE ? (A->display_mode > B->display_mode) : (A->display_mode < B->display_mode);
        }

        if (!R_DepthIsEquivalent(A->depth, B->depth))
        {
            int a_equiv = (A->depth < 20) ? 16 : 32;
            int b_equiv = (B->depth < 20) ? 16 : 32;

            return R_DepthIsEquivalent(SCREENBITS, 16) ? (a_equiv < b_equiv) : (a_equiv > b_equiv);
        }

        if (A->width != B->width)
        {
            int a_diff_w = HMM_ABS(SCREENWIDTH - A->width);
            int b_diff_w = HMM_ABS(SCREENWIDTH - B->width);

            return (a_diff_w < b_diff_w);
        }
        else
        {
            int a_diff_h = HMM_ABS(SCREENHEIGHT - A->height);
            int b_diff_h = HMM_ABS(SCREENHEIGHT - B->height);

            return (a_diff_h < b_diff_h);
        }
    }
};

void R_InitialResolution(void)
{
    LogDebug("R_InitialResolution...\n");

    if (DISPLAYMODE == 2)
    {
        if (DoExecuteChangeResolution(&borderless_mode))
            return;
    }

    DisplayMode mode;

    mode.width        = SCREENWIDTH;
    mode.height       = SCREENHEIGHT;
    mode.depth        = SCREENBITS;
    mode.display_mode = DISPLAYMODE;

    if (DoExecuteChangeResolution(&mode))
    {
        // this mode worked, make sure it's in the list
        R_AddResolution(&mode);
        return;
    }

    LogDebug("- Looking for another mode to try...\n");

    // sort modes into a good order, choosing sizes near the
    // request size first, and different depths/fullness last.

    std::sort(screen_modes.begin(), screen_modes.end(), Compare_Res_pred());

    for (int i = 0; i < (int)screen_modes.size(); i++)
    {
        if (DoExecuteChangeResolution(screen_modes[i]))
            return;
    }

    // FOOBAR!
    FatalError("Unable to set any resolutions!");
}

bool R_ChangeResolution(DisplayMode *mode)
{
    LogDebug("R_ChangeResolution...\n");

    if (DoExecuteChangeResolution(mode->display_mode == 2 ? &borderless_mode : mode))
        return true;

    LogDebug("- Failed : switching back...\n");

    DisplayMode old_mode;

    old_mode.width        = SCREENWIDTH;
    old_mode.height       = SCREENHEIGHT;
    old_mode.depth        = SCREENBITS;
    old_mode.display_mode = DISPLAYMODE;

    if (DoExecuteChangeResolution(&old_mode))
        return false;

    // This ain't good - current and previous resolutions do not work.
    FatalError("Switch back to old resolution failed!\n");
    return false; /* NOT REACHED */
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
