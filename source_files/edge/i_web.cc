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

#include <emscripten/html5.h>

#include "dm_defs.h"
#include "e_main.h"
#include "epi.h"
#include "epi_filesystem.h"
#include "epi_sdl.h" // needed for proper SDL main linkage
#include "epi_str_util.h"
#include "i_system.h"
#include "i_video.h"
#include "m_argv.h"
#include "m_menu.h"
#include "r_modes.h"
#include "version.h"

// Event reference
// https://github.com/emscripten-ports/SDL2/blob/master/src/video/emscripten/SDL_emscriptenevents.c

std::string executable_path = ".";

static int web_deferred_screen_width  = -1;
static int web_deferred_screen_height = -1;
static int web_deferred_menu          = -1;

static void WebSyncScreenSize(int width, int height)
{
    SDL_SetWindowSize(program_window, width, height);
    current_screen_width  = (int)width;
    current_screen_height = (int)height;
    current_screen_depth  = 24;
    current_window_mode   = kWindowModeWindowed;
    DeterminePixelAspect();

    SoftInitializeResolution();
}

void WebTick(void)
{
    if (web_deferred_screen_width != -1)
    {
        WebSyncScreenSize(web_deferred_screen_width, web_deferred_screen_height);
        web_deferred_screen_width = web_deferred_screen_height = -1;
    }

    if (web_deferred_menu != -1)
    {
        if (web_deferred_menu)
        {
            StartControlPanel();
        }
        else
        {
            MenuClear();
        }

        web_deferred_menu = -1;
    }

    // We always do this once here, although the engine may
    // makes in own calls to keep on top of the event processing
    ControlGetEvents();

    if (app_state & kApplicationActive)
        EdgeTicker();
}

extern "C"
{
    static EM_BOOL WebHandlePointerLockChange(int eventType, const EmscriptenPointerlockChangeEvent *changeEvent,
                                              void *userData)
    {
        EPI_UNUSED(eventType);
        EPI_UNUSED(userData);
        if (changeEvent->isActive)
        {
            SDL_ShowCursor(SDL_FALSE);
        }
        else
        {
            SDL_ShowCursor(SDL_TRUE);
        }

        return 0;
    }

    static EM_BOOL WebWindowResizedCallback(int eventType, const void *reserved, void *userData)
    {
        EPI_UNUSED(eventType);
        EPI_UNUSED(reserved);
        EPI_UNUSED(userData);
        double width, height;
        emscripten_get_element_css_size("canvas", &width, &height);

        printf("window fullscreen resized %i %i\n", (int)width, (int)height);

        WebSyncScreenSize(width, height);

        EM_ASM_({
            if (Module.onFullscreen)
            {
                Module.onFullscreen();
            }
        });

        return true;
    }

    void EMSCRIPTEN_KEEPALIVE WebSetFullscreen(int fullscreen)
    {
        if (fullscreen)
        {
            EmscriptenFullscreenStrategy strategy;
            strategy.scaleMode             = EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE_STDDEF;
            strategy.filteringMode         = EMSCRIPTEN_FULLSCREEN_FILTERING_DEFAULT;
            strategy.canvasResizedCallback = WebWindowResizedCallback;
            emscripten_enter_soft_fullscreen("canvas", &strategy);
        }
        else
        {
            emscripten_exit_soft_fullscreen();
        }
    }

    void EMSCRIPTEN_KEEPALIVE WebOpenGameMenu(int open)
    {
        web_deferred_menu = open;
    }

    void EMSCRIPTEN_KEEPALIVE WebSyncScreenSize()
    {
        double width, height;
        emscripten_get_element_css_size("canvas", &width, &height);

        web_deferred_screen_width  = (int)width;
        web_deferred_screen_height = (int)height;
    }

    void EMSCRIPTEN_KEEPALIVE WebMain(int argc, const char **argv)
    {
        emscripten_set_main_loop(WebTick, 0, 0);

        emscripten_set_pointerlockchange_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr, 0,
                                                  WebHandlePointerLockChange);

        if (SDL_Init(0) < 0)
            FatalError("Couldn't init SDL!!\n%s\n", SDL_GetError());

        executable_path = SDL_GetBasePath();

        EdgeMain(argc, argv);

        EM_ASM_({
            if (Module.edgePostInit)
            {
                Module.edgePostInit();
            }
        });
    }

    int main(int argc, char *argv[])
    {
        EM_ASM_(
            {
                const args = [];
                for (let i = 0; i < $0; i++)
                {
                    args.push(UTF8ToString(HEAP32[($1 >> 2) + i]));
                }

                console.log(`Edge command line : $ { args }`);

                const homeIndex = args.indexOf("-home");
                // clang-format off
                if (homeIndex === -1 || homeIndex >= args.length || args[homeIndex + 1].startsWith("-"))
                {
                    throw "No home command line option specified"
                }
                // clang-format on

                const homeDir = args[homeIndex + 1];

                if (!FS.analyzePath(homeDir).exists)
                {
                    FS.mkdirTree(homeDir);
                }

                FS.mount(IDBFS, {}, homeDir);
                FS.syncfs(
                    true, function(err) {
                        if (err)
                        {
                            console.error(`Error mounting home dir $ { err }`);
                            return;
                        }
                        Module._WebMain($0, $1);
                    });
            },
            argc, argv);

        return 0;
    }
}
