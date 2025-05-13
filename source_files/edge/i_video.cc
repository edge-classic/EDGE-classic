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

#include "i_video.h"

#include "con_main.h"
#include "ddf_main.h"
#include "dm_state.h"
#include "edge_profiling.h"
#include "epi_str_compare.h"
#include "epi_str_util.h"
#include "i_defs_gl.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_misc.h"
#include "n_network.h"
#include "r_backend.h"
#include "r_modes.h"
#include "r_state.h"
#include "version.h"

SDL_Window *program_window;

int graphics_shutdown = 0;

// I think grab_mouse should be an internal bool instead of a cvar...why would a
// user need to adjust this on the fly? - Dasho
EDGE_DEFINE_CONSOLE_VARIABLE(grab_mouse, "1", kConsoleVariableFlagArchive)
EDGE_DEFINE_CONSOLE_VARIABLE(vsync, "1", kConsoleVariableFlagArchive)
EDGE_DEFINE_CONSOLE_VARIABLE_CLAMPED(gamma_correction, "0", kConsoleVariableFlagArchive, -1.0, 1.0)

// this is the Monitor Size setting, really an aspect ratio.
// it defaults to 16:9, as that is the most common monitor size nowadays.
EDGE_DEFINE_CONSOLE_VARIABLE(monitor_aspect_ratio, "1.77777", kConsoleVariableFlagArchive)

// these are zero until StartupGraphics is called.
// after that they never change (we assume the desktop won't become other
// resolutions while EC is running).
EDGE_DEFINE_CONSOLE_VARIABLE(desktop_resolution_width, "0", kConsoleVariableFlagReadOnly)
EDGE_DEFINE_CONSOLE_VARIABLE(desktop_resolution_height, "0", kConsoleVariableFlagReadOnly)

EDGE_DEFINE_CONSOLE_VARIABLE(pixel_aspect_ratio, "1.0", kConsoleVariableFlagReadOnly);

// when > 0, this will force the pixel_aspect to a particular value, for
// cases where a normal logic fails.  however, it will apply to *all* modes,
// including windowed mode.
EDGE_DEFINE_CONSOLE_VARIABLE(forced_pixel_aspect_ratio, "0", kConsoleVariableFlagArchive)

#ifdef EDGE_WEB
EDGE_DEFINE_CONSOLE_VARIABLE(framerate_limit, "0", kConsoleVariableFlagReadOnly)
#else
EDGE_DEFINE_CONSOLE_VARIABLE(framerate_limit, "500", kConsoleVariableFlagArchive)
#endif

static bool grab_state;

extern ConsoleVariable renderer_far_clip;
extern ConsoleVariable draw_culling;
extern ConsoleVariable draw_culling_distance;

extern bool need_mouse_recapture;

void GrabCursor(bool enable)
{
#ifdef EDGE_WEB
    // On web, cursor lock is exclusively handled by selecting canvas
    return;
#endif

    if (!program_window || graphics_shutdown)
        return;

    grab_state = enable;

    if (grab_state)
        need_mouse_recapture = false;
    else
        need_mouse_recapture = true;

    if (grab_state && grab_mouse.d_)
    {
        SDL_SetRelativeMouseMode(SDL_TRUE);
    }
    else
    {
        SDL_SetRelativeMouseMode(SDL_FALSE);
    }
}

void DeterminePixelAspect()
{
    // the pixel aspect is the shape of pixels on the monitor for the current
    // video mode.  on modern LCDs (etc) it is usuall 1.0 (1:1).  knowing this
    // is critical to get things drawn correctly.  for example, Doom assets
    // assumed a 320x200 resolution on a 4:3 monitor, a pixel aspect of 5:6 or
    // 0.833333, and we must adjust image drawing to get "correct" results.

    // allow user to override
    if (forced_pixel_aspect_ratio.f_ > 0.1)
    {
        pixel_aspect_ratio = forced_pixel_aspect_ratio.f_;
        return;
    }

    // if not a fullscreen mode, check for a modern LCD (etc) monitor -- they
    // will have square pixels (1:1 aspect).
    bool is_crt = (desktop_resolution_width.d_ < desktop_resolution_height.d_ * 7 / 5);

    bool is_fullscreen = (current_window_mode == kWindowModeBorderless);
    if (is_fullscreen && current_screen_width == desktop_resolution_width.d_ &&
        current_screen_height == desktop_resolution_height.d_ && graphics_shutdown)
        is_fullscreen = false;

    if (!is_fullscreen && !is_crt)
    {
        pixel_aspect_ratio = 1.0f;
        return;
    }

    // in fullscreen modes, or a CRT monitor, compute the pixel aspect from
    // the current resolution and Monitor Size setting.  this assumes that the
    // video mode is filling the whole monitor (i.e. the monitor is not doing
    // any letter-boxing or pillar-boxing).  DPI setting does not matter here.

    pixel_aspect_ratio = monitor_aspect_ratio.f_ * (float)current_screen_height / (float)current_screen_width;
}

void StartupGraphics(void)
{
    std::string driver = ArgumentValue("videodriver");

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

    LogPrint("SDL_Video_Driver: %s\n", driver.c_str());

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
        FatalError("Couldn't init SDL VIDEO!\n%s\n", SDL_GetError());

    if (FindArgument("nograb") > 0)
        grab_mouse = 0;

#ifndef SOKOL_D3D11
    // -AJA- FIXME these are wrong (probably ignored though)
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
#endif

#ifdef SOKOL_GLCORE
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#elif SOKOL_GLES3
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#endif

    // -DS- 2005/06/27 Detect SDL Resolutions
    SDL_DisplayMode info;
    SDL_GetDesktopDisplayMode(0, &info);

    desktop_resolution_width  = info.w;
    desktop_resolution_height = info.h;

    if (current_screen_width > desktop_resolution_width.d_)
        current_screen_width = desktop_resolution_width.d_;
    if (current_screen_height > desktop_resolution_height.d_)
        current_screen_height = desktop_resolution_height.d_;

    LogPrint("Desktop resolution: %dx%d\n", desktop_resolution_width.d_, desktop_resolution_height.d_);

    int num_modes = SDL_GetNumDisplayModes(0);

    for (int i = 0; i < num_modes; i++)
    {
        SDL_DisplayMode possible_mode;
        SDL_GetDisplayMode(0, i, &possible_mode);

        if (possible_mode.w >= desktop_resolution_width.d_ || possible_mode.h >= desktop_resolution_height.d_)
            continue;

        DisplayMode test_mode;

        test_mode.width       = possible_mode.w;
        test_mode.height      = possible_mode.h;
        test_mode.depth       = SDL_BITSPERPIXEL(possible_mode.format);
        test_mode.window_mode = kWindowModeWindowed;

        if ((test_mode.width & 15) != 0)
            continue;

        if (test_mode.depth == 15 || test_mode.depth == 16 || test_mode.depth == 24 || test_mode.depth == 32)
            AddDisplayResolution(&test_mode);
    }

    // If needed, set the default window toggle mode to the largest non-native
    // res
    if (toggle_windowed_window_mode.d_ == kWindowModeInvalid)
    {
        for (size_t i = 0; i < screen_modes.size(); i++)
        {
            DisplayMode *check = screen_modes[i];
            if (check->window_mode == kWindowModeWindowed)
            {
                toggle_windowed_window_mode = kWindowModeWindowed;
                toggle_windowed_height      = check->height;
                toggle_windowed_width       = check->width;
                toggle_windowed_depth       = check->depth;
                break;
            }
        }
    }

    // Fill in borderless mode scrmode with the native display info
    borderless_mode.window_mode = kWindowModeBorderless;
    borderless_mode.width       = info.w;
    borderless_mode.height      = info.h;
    borderless_mode.depth       = SDL_BITSPERPIXEL(info.format);

    LogPrint("StartupGraphics: initialisation OK\n");
}

static bool InitializeWindow(DisplayMode *mode)
{
    std::string temp_title = window_title.s_;
    temp_title.append(" ").append(edge_version.s_);

#if EDGE_WEB
    int resizeable = SDL_WINDOW_RESIZABLE;
#else
    int resizeable = 0;
#endif

    uint32_t window_flags =
        (mode->window_mode == kWindowModeBorderless ? (SDL_WINDOW_FULLSCREEN_DESKTOP) : (0)) | resizeable;

#ifndef SOKOL_D3D11
    window_flags |= SDL_WINDOW_OPENGL;
#endif

    program_window = SDL_CreateWindow(temp_title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, mode->width,
                                      mode->height, window_flags);

    if (program_window == nullptr)
    {
        LogPrint("Failed to create window: %s\n", SDL_GetError());
        return false;
    }

    if (mode->window_mode == kWindowModeBorderless)
        SDL_GetWindowSize(program_window, &borderless_mode.width, &borderless_mode.height);

    if (mode->window_mode == kWindowModeWindowed)
    {
        toggle_windowed_depth       = mode->depth;
        toggle_windowed_height      = mode->height;
        toggle_windowed_width       = mode->width;
        toggle_windowed_window_mode = kWindowModeWindowed;
    }

#ifndef SOKOL_D3D11
    if (SDL_GL_CreateContext(program_window) == nullptr)
        FatalError("Failed to create OpenGL context.\n");
#endif

    if (vsync.d_ == 2)
    {
#ifndef SOKOL_D3D11
        // Fallback to normal VSync if Adaptive doesn't work
        if (SDL_GL_SetSwapInterval(-1) == -1)
        {
            vsync = 1;
            SDL_GL_SetSwapInterval(vsync.d_);
        }
#endif
    }
    else
    {
#ifndef SOKOL_D3D11
        SDL_GL_SetSwapInterval(vsync.d_);
#endif
    }

#ifndef EDGE_SOKOL
    gladLoadGL();

    if (GLVersion.major == 1 && GLVersion.minor < 3)
        FatalError("System only supports GL %d.%d. Minimum GL version 1.3 required!\n", GLVersion.major,
                   GLVersion.minor);
#endif

    return true;
}

bool SetScreenSize(DisplayMode *mode)
{
    bool initializing = false;
    GrabCursor(false);

    LogPrint("SetScreenSize: trying %dx%d %dbpp (%s)\n", mode->width, mode->height, mode->depth,
             mode->window_mode == kWindowModeBorderless ? "borderless" : "windowed");

    if (program_window == nullptr)
    {
        initializing = true;
        if (!InitializeWindow(mode))
        {
            return false;
        }
    }
    else if (mode->window_mode == kWindowModeBorderless)
    {
        SDL_SetWindowFullscreen(program_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        SDL_GetWindowSize(program_window, &borderless_mode.width, &borderless_mode.height);

        LogPrint("SetScreenSize: mode now %dx%d %dbpp\n", mode->width, mode->height, mode->depth);
    }
    else /* kWindowModeWindowed */
    {
        SDL_SetWindowFullscreen(program_window, 0);
        SDL_SetWindowSize(program_window, mode->width, mode->height);
        SDL_SetWindowPosition(program_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

        LogPrint("SetScreenSize: mode now %dx%d %dbpp\n", mode->width, mode->height, mode->depth);
    }

    if (!initializing)
    {
        render_backend->Resize(mode->width, mode->height);
    }

    // -AJA- turn off cursor -- BIG performance increase.
    //       Plus, the combination of no-cursor + grab gives
    //       continuous relative mouse motion.
    GrabCursor(true);

#ifdef DEVELOPERS
    // override SDL signal handlers (the so-called "parachute").
    signal(SIGFPE, SIG_DFL);
    signal(SIGSEGV, SIG_DFL);
#endif

    render_state->ClearColor(kRGBABlack);
#ifndef EDGE_SOKOL
    render_state->Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#endif

#ifndef SOKOL_D3D11
    SDL_GL_SwapWindow(program_window);
#endif

    return true;
}

void StartFrame(void)
{
    ec_frame_stats.Clear();
    render_state->ClearColor(kRGBABlack);
#ifndef EDGE_SOKOL
    render_state->Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#endif
    if (draw_culling.d_)
        renderer_far_clip.f_ = draw_culling_distance.f_;
    else
        renderer_far_clip.f_ = 64000.0;

    render_backend->StartFrame(current_screen_width, current_screen_height);
}

static void SwapBuffers(void)
{
    EDGE_ZoneScoped;

    render_backend->SwapBuffers();

#ifndef SOKOL_D3D11
    // move me and other SDL_GL to backend
    SDL_GL_SwapWindow(program_window);
#endif
}

void FinishFrame(void)
{
    render_backend->FinishFrame();

    SwapBuffers();

    EDGE_TracyPlot("draw_render_units", (int64_t)ec_frame_stats.draw_render_units);
    EDGE_TracyPlot("draw_wall_parts", (int64_t)ec_frame_stats.draw_wall_parts);
    EDGE_TracyPlot("draw_planes", (int64_t)ec_frame_stats.draw_planes);
    EDGE_TracyPlot("draw_things", (int64_t)ec_frame_stats.draw_things);
    EDGE_TracyPlot("draw_light_iterator", (int64_t)ec_frame_stats.draw_light_iterator);
    EDGE_TracyPlot("draw_sector_glow_iterator", (int64_t)ec_frame_stats.draw_sector_glow_iterator);

    {
        EDGE_ZoneNamedN(ZoneHandleCursor, "HandleCursor", true);

        if (ConsoleIsVisible())
            GrabCursor(false);
        else
        {
            if (grab_mouse.CheckModified())
                GrabCursor(grab_state);
            else
                GrabCursor(true);
        }
    }

    {
        EDGE_ZoneNamedN(ZoneFrameLimiting, "FrameLimiting", true);

        if (!single_tics)
        {
            if (framerate_limit.d_ >= kTicRate)
            {
                uint64_t        target_time = 1000000ull / framerate_limit.d_;
                static uint64_t start_time;

                while (1)
                {
                    uint64_t current_time   = GetMicroseconds();
                    uint64_t elapsed_time   = current_time - start_time;
                    uint64_t remaining_time = 0;

                    if (elapsed_time >= target_time)
                    {
                        start_time = current_time;
                        break;
                    }

                    remaining_time = target_time - elapsed_time;

                    if (remaining_time > 1000)
                    {
                        SleepForMilliseconds((remaining_time - 1000) / 1000);
                    }
                }
            }
        }

        fractional_tic = (float)(GetMilliseconds() * kTicRate % 1000) / 1000;
    }

    if (vsync.CheckModified())
    {
        if (vsync.d_ == 2)
        {
#ifndef SOKOL_D3D11
            // Fallback to normal VSync if Adaptive doesn't work
            if (SDL_GL_SetSwapInterval(-1) == -1)
            {
                vsync = 1;
                SDL_GL_SetSwapInterval(vsync.d_);
            }
#endif
        }
        else
        {
#ifndef SOKOL_D3D11
            SDL_GL_SetSwapInterval(vsync.d_);
#endif
        }
    }

    if (monitor_aspect_ratio.CheckModified() || forced_pixel_aspect_ratio.CheckModified())
        DeterminePixelAspect();

    EDGE_FrameMark;
}

void ShutdownGraphics(void)
{
    if (graphics_shutdown)
        return;

    graphics_shutdown = 1;

    render_backend->Shutdown();

    if (program_window != nullptr)
    {
        SDL_DestroyWindow(program_window);
        program_window = nullptr;
    }

    for (DisplayMode *mode : screen_modes)
    {
        delete mode;
    }

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
