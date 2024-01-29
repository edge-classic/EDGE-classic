//----------------------------------------------------------------------------
//  EDGE SDL Video Code
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

#include "i_defs.h"
#include "i_video.h"
#include "i_defs_gl.h"
#include "version.h"

#include <signal.h>

#include "m_argv.h"
#include "m_misc.h"
#include "r_modes.h"

#include "str_util.h"
#include "edge_profiling.h"

SDL_Window *my_vis;

int graphics_shutdown = 0;

DEF_CVAR(in_grab, "1", CVAR_ARCHIVE)
DEF_CVAR(v_sync, "0", CVAR_ARCHIVE)
DEF_CVAR_CLAMPED(v_gamma, "0", CVAR_ARCHIVE, -1.0, 1.0)

// this is the Monitor Size setting, really an aspect ratio.
// it defaults to 16:9, as that is the most common monitor size nowadays.
DEF_CVAR(v_monitorsize, "1.77777", CVAR_ARCHIVE)

// these are zero until I_StartupGraphics is called.
// after that they never change (we assume the desktop won't become other
// resolutions while EC is running).
DEF_CVAR(v_desktop_width, "0", CVAR_ROM)
DEF_CVAR(v_desktop_height, "0", CVAR_ROM)

DEF_CVAR(v_pixelaspect, "1.0", CVAR_ROM);

// when > 0, this will force the pixel_aspect to a particular value, for
// cases where a normal logic fails.  however, it will apply to *all* modes,
// including windowed mode.
DEF_CVAR(v_force_pixelaspect, "0", CVAR_ARCHIVE)

static bool grab_state;

extern cvar_c r_farclip;
extern cvar_c r_culling;
extern cvar_c r_culldist;

void I_GrabCursor(bool enable)
{

#ifdef EDGE_WEB
    // On web, cursor lock is exclusively handled by selecting canvas
    return;
#endif

    if (!my_vis || graphics_shutdown)
        return;

    grab_state = enable;

    if (grab_state && in_grab.d)
    {
        SDL_SetRelativeMouseMode(SDL_TRUE);
    }
    else
    {
        SDL_SetRelativeMouseMode(SDL_FALSE);
    }
}

void I_DeterminePixelAspect()
{
    // the pixel aspect is the shape of pixels on the monitor for the current
    // video mode.  on modern LCDs (etc) it is usuall 1.0 (1:1).  knowing this
    // is critical to get things drawn correctly.  for example, Doom assets
    // assumed a 320x200 resolution on a 4:3 monitor, a pixel aspect of 5:6 or
    // 0.833333, and we must adjust image drawing to get "correct" results.

    // allow user to override
    if (v_force_pixelaspect.f > 0.1)
    {
        v_pixelaspect = v_force_pixelaspect.f;
        return;
    }

    // if not a fullscreen mode, check for a modern LCD (etc) monitor -- they
    // will have square pixels (1:1 aspect).
    bool is_crt = (v_desktop_width.d < v_desktop_height.d * 7 / 5);

    bool is_fullscreen = (DISPLAYMODE > 0);
    if (is_fullscreen && SCREENWIDTH == v_desktop_width.d && SCREENHEIGHT == v_desktop_height.d && graphics_shutdown)
        is_fullscreen = false;

    if (!is_fullscreen && !is_crt)
    {
        v_pixelaspect = 1.0f;
        return;
    }

    // in fullscreen modes, or a CRT monitor, compute the pixel aspect from
    // the current resolution and Monitor Size setting.  this assumes that the
    // video mode is filling the whole monitor (i.e. the monitor is not doing
    // any letter-boxing or pillar-boxing).  DPI setting does not matter here.

    v_pixelaspect = v_monitorsize.f * (float)SCREENHEIGHT / (float)SCREENWIDTH;
}

void I_StartupGraphics(void)
{
    std::string driver = argv::Value("videodriver");

    if (driver.empty())
    {
        const char *check = SDL_getenv("SDL_VIDEODRIVER");
        if (check)
            driver = check;
    }

    if (driver.empty())
        driver = "default";

    if (epi::StringCaseCompareASCII(driver, "default") != 0)
    {
        SDL_setenv("SDL_VIDEODRIVER", driver.c_str(), 1);
    }

    I_Printf("SDL_Video_Driver: %s\n", driver.c_str());

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
        I_Error("Couldn't init SDL VIDEO!\n%s\n", SDL_GetError());

    if (argv::Find("nograb") > 0)
        in_grab = 0;

    // -AJA- FIXME these are wrong (probably ignored though)
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
#ifdef EDGE_GL_ES2
    SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    initialize_gl4es();
#endif

    // -DS- 2005/06/27 Detect SDL Resolutions
    SDL_DisplayMode info;
    SDL_GetDesktopDisplayMode(0, &info);

    v_desktop_width  = info.w;
    v_desktop_height = info.h;

    if (SCREENWIDTH > v_desktop_width.d)
        SCREENWIDTH = v_desktop_width.d;
    if (SCREENHEIGHT > v_desktop_height.d)
        SCREENHEIGHT = v_desktop_height.d;

    I_Printf("Desktop resolution: %dx%d\n", v_desktop_width.d, v_desktop_height.d);

    int num_modes = SDL_GetNumDisplayModes(0);

    for (int i = 0; i < num_modes; i++)
    {
        SDL_DisplayMode possible_mode;
        SDL_GetDisplayMode(0, i, &possible_mode);

        if (possible_mode.w > v_desktop_width.d || possible_mode.h > v_desktop_height.d)
            continue;

        scrmode_c test_mode;

        test_mode.width        = possible_mode.w;
        test_mode.height       = possible_mode.h;
        test_mode.depth        = SDL_BITSPERPIXEL(possible_mode.format);
        test_mode.display_mode = scrmode_c::SCR_FULLSCREEN;

        if ((test_mode.width & 15) != 0)
            continue;

        if (test_mode.depth == 15 || test_mode.depth == 16 || test_mode.depth == 24 || test_mode.depth == 32)
        {
            R_AddResolution(&test_mode);

            if (test_mode.width < v_desktop_width.d && test_mode.height < v_desktop_height.d)
            {
                scrmode_c win_mode    = test_mode;
                win_mode.display_mode = scrmode_c::SCR_WINDOW;
                R_AddResolution(&win_mode);
            }
        }
    }

    // If needed, set the default window toggle mode to the largest non-native res
    if (tw_displaymode.d == scrmode_c::SCR_INVALID)
    {
        for (size_t i = 0; i < screen_modes.size(); i++)
        {
            scrmode_c *check = screen_modes[i];
            if (check->display_mode == scrmode_c::SCR_WINDOW)
            {
                tw_displaymode  = scrmode_c::SCR_WINDOW;
                tw_screenheight = check->height;
                tw_screenwidth  = check->width;
                tw_screendepth  = check->depth;
                break;
            }
        }
    }

    // Fill in borderless mode scrmode with the native display info
    borderless_mode.display_mode = scrmode_c::SCR_BORDERLESS;
    borderless_mode.width        = info.w;
    borderless_mode.height       = info.h;
    borderless_mode.depth        = SDL_BITSPERPIXEL(info.format);

    // If needed, also make the default fullscreen toggle mode borderless
    if (tf_displaymode.d == scrmode_c::SCR_INVALID)
    {
        tf_displaymode  = scrmode_c::SCR_BORDERLESS;
        tf_screenwidth  = info.w;
        tf_screenheight = info.h;
        tf_screendepth  = (int)SDL_BITSPERPIXEL(info.format);
    }

    I_Printf("I_StartupGraphics: initialisation OK\n");
}

static bool I_CreateWindow(scrmode_c *mode)
{
    std::string temp_title = windowtitle.s;
    temp_title.append(" ").append(edgeversion.s);

#if EDGE_WEB
    int resizeable = SDL_WINDOW_RESIZABLE;
#else
    int resizeable = 0;
#endif

    my_vis =
        SDL_CreateWindow(temp_title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, mode->width, mode->height,
                         SDL_WINDOW_OPENGL |
                             (mode->display_mode == scrmode_c::SCR_BORDERLESS
                                  ? (SDL_WINDOW_FULLSCREEN_DESKTOP)
                                  : (mode->display_mode == scrmode_c::SCR_FULLSCREEN ? SDL_WINDOW_FULLSCREEN : 0)) |
                             resizeable);

    if (my_vis == NULL)
    {
        I_Printf("Failed to create window: %s\n", SDL_GetError());
        return false;
    }

    if (mode->display_mode == scrmode_c::SCR_BORDERLESS)
        SDL_GetWindowSize(my_vis, &borderless_mode.width, &borderless_mode.height);

    if (mode->display_mode == scrmode_c::SCR_WINDOW)
    {
        tw_screendepth  = mode->depth;
        tw_screenheight = mode->height;
        tw_screenwidth  = mode->width;
        tw_displaymode  = scrmode_c::SCR_WINDOW;
    }
    else if (mode->display_mode == scrmode_c::SCR_FULLSCREEN)
    {
        tf_screendepth  = mode->depth;
        tf_screenheight = mode->height;
        tf_screenwidth  = mode->width;
        tf_displaymode  = scrmode_c::SCR_FULLSCREEN;
    }
    else
    {
        tf_screendepth  = borderless_mode.depth;
        tf_screenheight = borderless_mode.height;
        tf_screenwidth  = borderless_mode.width;
        tf_displaymode  = scrmode_c::SCR_BORDERLESS;
    }

    if (SDL_GL_CreateContext(my_vis) == NULL)
        I_Error("Failed to create OpenGL context.\n");

    if (v_sync.d == 2)
    {
        // Fallback to normal VSync if Adaptive doesn't work
        if (SDL_GL_SetSwapInterval(-1) == -1)
        {
            v_sync = 1;
            SDL_GL_SetSwapInterval(v_sync.d);
        }
    }
    else
        SDL_GL_SetSwapInterval(v_sync.d);

#ifndef EDGE_GL_ES2
    gladLoaderLoadGL();
#endif

    return true;
}

bool I_SetScreenSize(scrmode_c *mode)
{
    I_GrabCursor(false);

    I_Printf("I_SetScreenSize: trying %dx%d %dbpp (%s)\n", mode->width, mode->height, mode->depth,
             mode->display_mode == scrmode_c::SCR_BORDERLESS
                 ? "borderless"
                 : (mode->display_mode == scrmode_c::SCR_FULLSCREEN ? "fullscreen" : "windowed"));

    if (my_vis == NULL)
    {
        if (!I_CreateWindow(mode))
        {
            return false;
        }
    }
    else if (mode->display_mode == scrmode_c::SCR_BORDERLESS)
    {
        SDL_SetWindowFullscreen(my_vis, SDL_WINDOW_FULLSCREEN_DESKTOP);
        SDL_GetWindowSize(my_vis, &borderless_mode.width, &borderless_mode.height);

        I_Printf("I_SetScreenSize: mode now %dx%d %dbpp\n", mode->width, mode->height, mode->depth);
    }
    else if (mode->display_mode == scrmode_c::SCR_FULLSCREEN)
    {
        SDL_SetWindowFullscreen(my_vis, SDL_WINDOW_FULLSCREEN);
        SDL_DisplayMode *new_mode = new SDL_DisplayMode;
        new_mode->h               = mode->height;
        new_mode->w               = mode->width;
        SDL_SetWindowDisplayMode(my_vis, new_mode);
        SDL_SetWindowSize(my_vis, mode->width, mode->height);
        delete new_mode;
        new_mode = NULL;

        I_Printf("I_SetScreenSize: mode now %dx%d %dbpp\n", mode->width, mode->height, mode->depth);
    }
    else /* SCR_WINDOW */
    {
        SDL_SetWindowFullscreen(my_vis, 0);
        SDL_SetWindowSize(my_vis, mode->width, mode->height);
        SDL_SetWindowPosition(my_vis, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

        I_Printf("I_SetScreenSize: mode now %dx%d %dbpp\n", mode->width, mode->height, mode->depth);
    }

    // -AJA- turn off cursor -- BIG performance increase.
    //       Plus, the combination of no-cursor + grab gives
    //       continuous relative mouse motion.
    I_GrabCursor(true);

#ifdef DEVELOPERS
    // override SDL signal handlers (the so-called "parachute").
    signal(SIGFPE, SIG_DFL);
    signal(SIGSEGV, SIG_DFL);
#endif

    glClearColor(0, 0, 0, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    SDL_GL_SwapWindow(my_vis);

    return true;
}

void I_StartFrame(void)
{
    ecframe_stats.Clear();
    glClearColor(0, 0, 0, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (r_culling.d)
        r_farclip.f = r_culldist.f;
    else
        r_farclip.f = 64000.0;
}

void I_FinishFrame(void)
{
    SDL_GL_SwapWindow(my_vis);
    
    EDGE_TracyPlot("draw_runits", (int64_t) ecframe_stats.draw_runits);
    EDGE_TracyPlot("draw_wallparts", (int64_t) ecframe_stats.draw_wallparts);
    EDGE_TracyPlot("draw_planes", (int64_t) ecframe_stats.draw_planes);
    EDGE_TracyPlot("draw_things", (int64_t) ecframe_stats.draw_things);
    EDGE_TracyPlot("draw_lightiterator", (int64_t) ecframe_stats.draw_lightiterator);
    EDGE_TracyPlot("draw_sectorglowiterator", (int64_t) ecframe_stats.draw_sectorglowiterator);
    
    EDGE_FrameMark;

    if (in_grab.CheckModified())
        I_GrabCursor(grab_state);

    if (v_sync.CheckModified())
    {
        if (v_sync.d == 2)
        {
            // Fallback to normal VSync if Adaptive doesn't work
            if (SDL_GL_SetSwapInterval(-1) == -1)
            {
                v_sync = 1;
                SDL_GL_SetSwapInterval(v_sync.d);
            }
        }
        else
            SDL_GL_SetSwapInterval(v_sync.d);
    }

    if (v_monitorsize.CheckModified() || v_force_pixelaspect.CheckModified())
        I_DeterminePixelAspect();
}

void I_ShutdownGraphics(void)
{
    if (graphics_shutdown)
        return;

    graphics_shutdown = 1;

    if (SDL_WasInit(SDL_INIT_EVERYTHING))
    {
        I_DeterminePixelAspect();

        SDL_Quit();
    }

#ifndef EDGE_GL_ES2
    gladLoaderUnloadGL();
#else
    close_gl4es();
#endif
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
