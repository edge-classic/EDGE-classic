//----------------------------------------------------------------------------
//  EDGE SDL Video Code
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

#include "i_defs.h"
#include "i_video.h"
#include "i_defs_gl.h"
#include "version.h"

#include <signal.h>

#include "m_argv.h"
#include "m_misc.h"
#include "r_modes.h"


SDL_Window *my_vis;

int graphics_shutdown = 0;

DEF_CVAR(in_grab, "1", CVAR_ARCHIVE)
DEF_CVAR(v_sync,  "1", CVAR_ARCHIVE)

// this is the Monitor Size setting, really an aspect ratio.
// it defaults to 16:9, as that is the most common monitor size nowadays.
DEF_CVAR(v_monitorsize, "1.77777", CVAR_ARCHIVE)

// these are zero until I_StartupGraphics is called.
// after that they never change (we assume the desktop won't become other
// resolutions while EC is running).
DEF_CVAR(v_desktop_width,  "0", CVAR_ROM)
DEF_CVAR(v_desktop_height, "0", CVAR_ROM)

DEF_CVAR(v_pixelaspect, "1.0", CVAR_ROM);

// when > 0, this will force the pixel_aspect to a particular value, for
// cases where a normal logic fails.  however, it will apply to *all* modes,
// including windowed mode.
DEF_CVAR(v_force_pixelaspect, "0", CVAR_ARCHIVE)

static bool grab_state;


// Possible Windowed Modes
// These reflect 4:3 and 16:9 historical and current standards
// Should cover up to ~4K monitors
static struct { int w, h; } possible_modes[] =
{
	{  320, 240  }, // 4:3 Quarter VGA
	{  400, 300  }, // 4:3 Quarter SVGA
	{  640, 360  }, // 16:9 Ninth HD
	{  640, 480  }, // 4:3 VGA
	{  800, 600  }, // 4:3 SVGA
	{  960, 540  }, // 16:9 Quarter HD
	{ 1024, 768  }, // 4:3 XGA
	{ 1152, 864  }, // 4:3 XGA+
	{ 1280, 720  }, // 16:9 HD
	{ 1280, 960  }, // 4:3 Super XGA-
	{ 1400, 1050 }, // 4:3 Super XGA+
	{ 1600, 900  }, // 16:9 HD+
	{ 1600, 1200 }, // 4:3 Ultra XGA
	{ 1920, 1080 }, // 16:9 Full HD
	{ 2048, 1152 }, // 16:9 Quad WXGA
	{ 2048, 1536 }, // 4:3 Quad XGA
	{ 2560, 1440 }, // 16:9 Quad HD
	{ 2800, 2100 }, // 4:3 Quad SXGA+

	{  -1,  -1   }
};


void I_GrabCursor(bool enable)
{
	if (! my_vis || graphics_shutdown)
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

	bool is_fullscreen = (DISPLAYMODE == 1);
	if (is_fullscreen && SCREENWIDTH == v_desktop_width.d && SCREENHEIGHT == v_desktop_height.d)
		is_fullscreen = false;

	if (! is_fullscreen && ! is_crt)
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
	if (M_CheckParm("-directx"))
		force_directx = true;

	if (M_CheckParm("-gdi") || M_CheckParm("-nodirectx"))
		force_directx = false;

	const char *driver = M_GetParm("-videodriver");

	if (! driver)
		driver = SDL_getenv("SDL_VIDEODRIVER");

	if (! driver)
	{
		driver = "default";

#ifdef WIN32
		if (force_directx)
			driver = "directx";
#endif
	}

	if (epi::case_cmp(driver, "default") != 0)
	{
		SDL_setenv("SDL_VIDEODRIVER", driver, 1);
	}

	I_Printf("SDL_Video_Driver: %s\n", driver);


	if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
		I_Error("Couldn't init SDL VIDEO!\n%s\n", SDL_GetError());

	if (M_CheckParm("-nograb"))
		in_grab = 0;

	// -AJA- FIXME these are wrong (probably ignored though)
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE,     5);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,   5);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,    5);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,   16);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);

    // -DS- 2005/06/27 Detect SDL Resolutions
	SDL_DisplayMode info;
	SDL_GetDesktopDisplayMode(0, &info);

	v_desktop_width  = info.w;
	v_desktop_height = info.h;

	if (SCREENWIDTH > v_desktop_width.d) SCREENWIDTH = v_desktop_width.d;
	if (SCREENHEIGHT > v_desktop_height.d) SCREENHEIGHT = v_desktop_height.d;

	I_Printf("Desktop resolution: %dx%d\n", v_desktop_width.d, v_desktop_height.d);

	int num_modes = SDL_GetNumDisplayModes(0);

	for (int i = 0; i < num_modes; i++ )
	{
		SDL_DisplayMode possible_mode;
		SDL_GetDisplayMode(0, i, &possible_mode);

		if (possible_mode.w > v_desktop_width.d || possible_mode.h > v_desktop_height.d)
			continue;

		scrmode_c test_mode;

		test_mode.width  = possible_mode.w;
		test_mode.height = possible_mode.h;
		test_mode.depth  = SDL_BITSPERPIXEL(possible_mode.format);
		test_mode.display_mode = test_mode.SCR_FULLSCREEN;

		if ((test_mode.width & 15) != 0)
			continue;

		if (test_mode.depth == 15 || test_mode.depth == 16 ||
		    test_mode.depth == 24 || test_mode.depth == 32)
		{
			R_AddResolution(&test_mode);
		}
	}

	// Fill in borderless mode scrmode with the native display info
    borderless_mode.display_mode = borderless_mode.SCR_BORDERLESS;
    borderless_mode.width = info.w;
    borderless_mode.height = info.h;
    borderless_mode.depth = SDL_BITSPERPIXEL(info.format);

	// -ACB- 2000/03/16 Test for possible windowed resolutions
	// -AJA- TODO see if SDL2 can give us a definitive list, rather than this silliness
	for (int i = 0; possible_modes[i].w != -1; i++)
	{
		scrmode_c mode;

		if (possible_modes[i].w >= v_desktop_width.d || possible_modes[i].h >= v_desktop_height.d)
			continue;

		mode.width = possible_modes[i].w;
		mode.height = possible_modes[i].h;
		mode.depth  = SDL_BITSPERPIXEL(info.format);
		mode.display_mode   = mode.SCR_WINDOW;

		R_AddResolution(&mode);
	}

	I_Printf("I_StartupGraphics: initialisation OK\n");
}


static bool I_CreateWindow(scrmode_c *mode)
{
	std::string temp_title = TITLE;
	temp_title.append(" ").append(EDGEVERSTR);

	my_vis = SDL_CreateWindow(temp_title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, mode->width, mode->height,
		SDL_WINDOW_OPENGL | (mode->display_mode == mode->SCR_BORDERLESS ? (SDL_WINDOW_FULLSCREEN_DESKTOP) :
		(mode->display_mode == mode->SCR_FULLSCREEN ? SDL_WINDOW_FULLSCREEN : 0)));

	if (my_vis == NULL)
	{
		I_Printf("Failed to create window: %s\n", SDL_GetError());
		return false;
	}

	if (mode->display_mode == mode->SCR_BORDERLESS)
	{
		SDL_GetWindowSize(my_vis, &borderless_mode.width, &borderless_mode.height);
		//-- display_W = borderless_mode.width;
		//-- display_H = borderless_mode.height;
	}

	if (SDL_GL_CreateContext(my_vis) == NULL)
		I_Error("Failed to create OpenGL context.\n");

	SDL_GL_SetSwapInterval(v_sync.d ? 1 : 0);

	gladLoaderLoadGL();

	return true;
}


bool I_SetScreenSize(scrmode_c *mode)
{
	I_GrabCursor(false);

	I_Printf("I_SetScreenSize: trying %dx%d %dbpp (%s)\n",
			 mode->width, mode->height, mode->depth,
			 mode->display_mode == mode->SCR_BORDERLESS ? "borderless" : 
			 (mode->display_mode == mode->SCR_FULLSCREEN ? "fullscreen" : "windowed"));

	if (my_vis == NULL)
	{
		if (! I_CreateWindow(mode))
		{
			return false;
		}
	}
	else if (mode->display_mode == mode->SCR_BORDERLESS) 
	{
		SDL_SetWindowFullscreen(my_vis, SDL_WINDOW_FULLSCREEN_DESKTOP);
		SDL_GetWindowSize(my_vis, &borderless_mode.width, &borderless_mode.height);
		//-- display_W = borderless_mode.width;
		//-- display_H = borderless_mode.height;

		I_Printf("I_SetScreenSize: mode now %dx%d %dbpp\n",
			mode->width, mode->height, mode->depth);
	}
	else if (mode->display_mode == mode->SCR_FULLSCREEN)
	{
		SDL_SetWindowFullscreen(my_vis, SDL_WINDOW_FULLSCREEN);
		SDL_DisplayMode *new_mode = new SDL_DisplayMode;
		new_mode->h = mode->height;
		new_mode->w = mode->width;
		SDL_SetWindowDisplayMode(my_vis, new_mode);
		SDL_SetWindowSize(my_vis, mode->width, mode->height);
		delete new_mode;
		new_mode = NULL;

		I_Printf("I_SetScreenSize: mode now %dx%d %dbpp\n",
			mode->width, mode->height, mode->depth);
	}
	else  /* SCR_WINDOW */
	{
		SDL_SetWindowFullscreen(my_vis, 0);
		SDL_SetWindowSize(my_vis, mode->width, mode->height);
		SDL_SetWindowPosition(my_vis, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

		I_Printf("I_SetScreenSize: mode now %dx%d %dbpp\n",
			mode->width, mode->height, mode->depth);
	}

	// -AJA- turn off cursor -- BIG performance increase.
	//       Plus, the combination of no-cursor + grab gives 
	//       continuous relative mouse motion.
	I_GrabCursor(true);

#ifdef DEVELOPERS
	// override SDL signal handlers (the so-called "parachute").
	signal(SIGFPE,SIG_DFL);
	signal(SIGSEGV,SIG_DFL);
#endif

	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	SDL_GL_SwapWindow(my_vis);

	return true;
}


void I_StartFrame(void)
{
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}


void I_FinishFrame(void)
{
	SDL_GL_SwapWindow(my_vis);

	if (in_grab.CheckModified())
		I_GrabCursor(grab_state);

	if (v_sync.CheckModified())
		SDL_GL_SetSwapInterval(v_sync.d ? 1 : 0);

	if (v_monitorsize.CheckModified() || v_force_pixelaspect.CheckModified())
		I_DeterminePixelAspect();
}


void I_SetGamma(float gamma)
{
	if (SDL_SetWindowBrightness(my_vis, gamma) < 0)
		I_Printf("Failed to change gamma.\n");
}


void I_ShutdownGraphics(void)
{
	if (graphics_shutdown)
		return;

	graphics_shutdown = 1;

	if (SDL_WasInit(SDL_INIT_EVERYTHING))
	{
        // reset gamma to default
        I_SetGamma(1.0f);

		SDL_Quit ();
	}

	gladLoaderUnloadGL();
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
