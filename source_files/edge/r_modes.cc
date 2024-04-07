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

#include "r_modes.h"

#include <limits.h>

#include <algorithm>
#include <vector>

#include "am_map.h"
#include "epi.h"
#include "hu_font.h" // current_font_size
#include "i_defs_gl.h"
#include "i_system.h"
#include "r_colormap.h"
#include "r_draw.h"
#include "r_gldefs.h"
#include "r_image.h"
#include "r_misc.h"
#include "r_units.h"
#include "r_wipe.h"

// Globals
int current_screen_width;
int current_screen_height;
int current_screen_depth;
int current_window_mode;

DisplayMode borderless_mode;
EDGE_DEFINE_CONSOLE_VARIABLE(toggle_fullscreen_width, "0", kConsoleVariableFlagArchive)
EDGE_DEFINE_CONSOLE_VARIABLE(toggle_fullscreen_height, "0", kConsoleVariableFlagArchive)
EDGE_DEFINE_CONSOLE_VARIABLE(toggle_fullscreen_depth, "0", kConsoleVariableFlagArchive)
EDGE_DEFINE_CONSOLE_VARIABLE(toggle_fullscreen_window_mode, "-1", kConsoleVariableFlagArchive)
EDGE_DEFINE_CONSOLE_VARIABLE(toggle_windowed_width, "0", kConsoleVariableFlagArchive)
EDGE_DEFINE_CONSOLE_VARIABLE(toggle_windowed_height, "0", kConsoleVariableFlagArchive)
EDGE_DEFINE_CONSOLE_VARIABLE(toggle_windowed_depth, "0", kConsoleVariableFlagArchive)
EDGE_DEFINE_CONSOLE_VARIABLE(toggle_windowed_window_mode, "-1", kConsoleVariableFlagArchive)

std::vector<DisplayMode *> screen_modes;

bool EquivalentDisplayDepth(int depth1, int depth2)
{
    if (depth1 == depth2)
        return true;

    if (HMM_MIN(depth1, depth2) == 15 && HMM_MAX(depth1, depth2) == 16)
        return true;

    if (HMM_MIN(depth1, depth2) == 24 && HMM_MAX(depth1, depth2) == 32)
        return true;

    return false;
}

static int SizeDifference(int w1, int h1, int w2, int h2)
{
    return (w1 * 10000 + h1) - (w2 * 10000 + h2);
}

static DisplayMode *FindResolution(int w, int h, int depth, int window_mode)
{
    for (int i = 0; i < (int)screen_modes.size(); i++)
    {
        DisplayMode *cur = screen_modes[i];

        if (cur->width == w && cur->height == h && EquivalentDisplayDepth(cur->depth, depth) &&
            cur->window_mode == window_mode)
        {
            return cur;
        }
    }

    return nullptr;
}

//
// AddDisplayResolution
//
// Adds a resolution to the scrmodes list. This is used so we can
// select it within the video options menu.
//
void AddDisplayResolution(DisplayMode *mode)
{
    DisplayMode *exist = FindResolution(mode->width, mode->height, mode->depth, mode->window_mode);
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

void DumpResolutionList(void)
{
    LogPrint("Available Resolutions:\n");

    for (int i = 0; i < (int)screen_modes.size(); i++)
    {
        DisplayMode *cur = screen_modes[i];

        if (i > 0 && (i % 3) == 0)
            LogPrint("\n");

        LogPrint("  %4dx%4d @ %02d %s", cur->width, cur->height, cur->depth,
                 cur->window_mode == kWindowModeBorderless
                     ? "BL"
                     : (cur->window_mode == kWindowModeFullscreen ? "FS " : "win"));
    }

    LogPrint("\n");
}

bool IncrementResolution(DisplayMode *mode, int what, int dir)
{
    // Algorithm:
    //   for kIncrementWindowMode, we simply adjust the
    //   value in question, and find the mode
    //   with matching window_mode and the closest size.
    //
    //   for kIncrementSize, we find modes with matching depth/window_mode
    //   and the *next* closest size (ignoring the same size or
    //   sizes that are in opposite direction to 'dir' param).

    EPI_ASSERT(dir == 1 || dir == -1);

    int depth       = mode->depth;
    int window_mode = mode->window_mode;

    if (what == kIncrementWindowMode)
    {
        if (dir == 1)
            window_mode = (window_mode + 1) % 3;
        else
        {
            if (window_mode > 0)
                window_mode--;
            else
                window_mode = 2;
        }
    }

    if (window_mode == 2)
    {
        mode->width       = borderless_mode.width;
        mode->height      = borderless_mode.height;
        mode->depth       = borderless_mode.depth;
        mode->window_mode = borderless_mode.window_mode;

        return true;
    }

    DisplayMode *best      = nullptr;
    int          best_diff = (1 << 30);

    for (int i = 0; i < (int)screen_modes.size(); i++)
    {
        DisplayMode *cur = screen_modes[i];

        if (!EquivalentDisplayDepth(cur->depth, depth))
            continue;

        if (cur->window_mode != window_mode)
            continue;

        int diff = SizeDifference(cur->width, cur->height, mode->width, mode->height);

        if (what == kIncrementSize)
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
        mode->width       = best->width;
        mode->height      = best->height;
        mode->depth       = best->depth;
        mode->window_mode = best->window_mode;

        return true;
    }

    return false;
}

void ToggleFullscreen(void)
{
    DisplayMode toggle;
    if (current_window_mode > kWindowModeWindowed)
    {
        toggle.depth       = toggle_windowed_depth.d_;
        toggle.height      = toggle_windowed_height.d_;
        toggle.width       = toggle_windowed_width.d_;
        toggle.window_mode = kWindowModeWindowed;
        ChangeResolution(&toggle);
    }
    else
    {
        toggle.depth       = toggle_fullscreen_depth.d_;
        toggle.height      = toggle_fullscreen_height.d_;
        toggle.width       = toggle_fullscreen_width.d_;
        toggle.window_mode = toggle_fullscreen_window_mode.d_;
        ChangeResolution(&toggle);
    }
    SoftInitializeResolution();
}

//----------------------------------------------------------------------------

void SoftInitializeResolution(void)
{
    LogDebug("SoftInitializeResolution...\n");

    RendererNewScreenSize(current_screen_width, current_screen_height, current_screen_depth);

    if (current_screen_width < 720)
        current_font_size = 0;
    else if (current_screen_width < 1440)
        current_font_size = 1;
    else
        current_font_size = 2;

    // -ES- 1999/08/29 Fixes the garbage palettes, and the blank 16-bit console
    SetPalette(kPaletteNormal, 0);

    // re-initialise various bits of GL state
    RendererSoftInit();

    LogDebug("-  returning true.\n");

    return;
}

static bool DoExecuteChangeResolution(DisplayMode *mode)
{
    StopWipe(); // delete any wipe texture too

    DeleteAllImages();

    bool was_ok = SetScreenSize(mode);

    if (!was_ok)
        return false;

    current_screen_width  = mode->width;
    current_screen_height = mode->height;
    current_screen_depth  = mode->depth;
    current_window_mode   = mode->window_mode;

    if (current_screen_width < 720)
        current_font_size = 0;
    else if (current_screen_width < 1440)
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

struct CompareResolutionPredicate
{
    inline bool operator()(const DisplayMode *A, const DisplayMode *B) const
    {
        if (A->window_mode != B->window_mode)
        {
            return current_window_mode ? (A->window_mode > B->window_mode) : (A->window_mode < B->window_mode);
        }

        if (!EquivalentDisplayDepth(A->depth, B->depth))
        {
            int a_equiv = (A->depth < 20) ? 16 : 32;
            int b_equiv = (B->depth < 20) ? 16 : 32;

            return EquivalentDisplayDepth(current_screen_depth, 16) ? (a_equiv < b_equiv) : (a_equiv > b_equiv);
        }

        if (A->width != B->width)
        {
            int a_diff_w = HMM_ABS(current_screen_width - A->width);
            int b_diff_w = HMM_ABS(current_screen_width - B->width);

            return (a_diff_w < b_diff_w);
        }
        else
        {
            int a_diffloor_height = HMM_ABS(current_screen_height - A->height);
            int b_diffloor_height = HMM_ABS(current_screen_height - B->height);

            return (a_diffloor_height < b_diffloor_height);
        }
    }
};

void SetInitialResolution(void)
{
    LogDebug("SetInitialResolution...\n");

    if (current_window_mode == 2)
    {
        if (DoExecuteChangeResolution(&borderless_mode))
            return;
    }

    DisplayMode mode;

    mode.width       = current_screen_width;
    mode.height      = current_screen_height;
    mode.depth       = current_screen_depth;
    mode.window_mode = current_window_mode;

    if (DoExecuteChangeResolution(&mode))
    {
        // this mode worked, make sure it's in the list
        AddDisplayResolution(&mode);
        return;
    }

    LogDebug("- Looking for another mode to try...\n");

    // sort modes into a good order, choosing sizes near the
    // request size first, and different depths/fullness last.

    std::sort(screen_modes.begin(), screen_modes.end(), CompareResolutionPredicate());

    for (int i = 0; i < (int)screen_modes.size(); i++)
    {
        if (DoExecuteChangeResolution(screen_modes[i]))
            return;
    }

    // FOOBAR!
    FatalError("Unable to set any resolutions!");
}

bool ChangeResolution(DisplayMode *mode)
{
    LogDebug("ChangeResolution...\n");

    if (DoExecuteChangeResolution(mode->window_mode == 2 ? &borderless_mode : mode))
        return true;

    LogDebug("- Failed : switching back...\n");

    DisplayMode old_mode;

    old_mode.width       = current_screen_width;
    old_mode.height      = current_screen_height;
    old_mode.depth       = current_screen_depth;
    old_mode.window_mode = current_window_mode;

    if (DoExecuteChangeResolution(&old_mode))
        return false;

    // This ain't good - current and previous resolutions do not work.
    FatalError("Switch back to old resolution failed!\n");
    return false; /* NOT REACHED */
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
