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

static bool grab_state;

static int display_W, display_H;


// Possible Screen Modes
static struct { int w, h; } possible_modes[] =
{
	{  320, 200, },
	{  320, 240, },
	{  400, 300, },
	{  512, 384, },
	{  640, 360, },
	{  640, 400, },
	{  640, 480, },
	{  800, 600, },
	{ 1024, 768, },
	{ 1280, 720, },
	{ 1280,1024, },
	{ 1366, 768, },
	{ 1600, 900, },
	{ 1600,1200, },

	{  -1,  -1, }
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

	if (stricmp(driver, "default") != 0)
	{
		SDL_setenv("SDL_VIDEODRIVER", driver, 1);
	}

	I_Printf("SDL_Video_Driver: %s\n", driver);


	if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
		I_Error("Couldn't init SDL VIDEO!\n%s\n", SDL_GetError());

	if (M_CheckParm("-nograb"))
		in_grab = 0;

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE,     5);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,   5);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,    5);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,   16);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);

	
    // -DS- 2005/06/27 Detect SDL Resolutions
	SDL_DisplayMode info;
	SDL_GetDesktopDisplayMode(0, &info);

	display_W = info.w;
	display_H = info.h;

	if (SCREENWIDTH > display_W) SCREENWIDTH = display_W;
	if (SCREENHEIGHT > display_H) SCREENHEIGHT = display_H;

	I_Printf("Desktop resolution: %dx%d\n", display_W, display_H);

	int num_modes = SDL_GetNumDisplayModes(0);

	for (int i = 0; i < num_modes; i++ )
	{
		SDL_DisplayMode possible_mode;
		SDL_GetDisplayMode(0, i, &possible_mode);

		if (possible_mode.w > display_W || possible_mode.h > display_H)
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
	for (int depth = 16; depth <= 32; depth = depth+16)
	{
		for (int i = 0; possible_modes[i].w != -1; i++)
		{
			scrmode_c mode;
			SDL_DisplayMode test_mode;
			SDL_DisplayMode closest_mode;

			if (possible_modes[i].w > display_W || possible_modes[i].h > display_H)
				continue;

			mode.width = possible_modes[i].w;
			mode.height = possible_modes[i].h;
			mode.depth  = depth;
			mode.display_mode   = mode.SCR_WINDOW;

			test_mode.w = possible_modes[i].w;
			test_mode.h = possible_modes[i].h;
			test_mode.format = (depth << 8);

			SDL_GetClosestDisplayMode(0, &test_mode, &closest_mode);

			if (R_DepthIsEquivalent(SDL_BITSPERPIXEL(closest_mode.format), mode.depth))
				R_AddResolution(&mode);
		}
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
		display_W = borderless_mode.width;
		display_H = borderless_mode.height;
	}

	if (SDL_GL_CreateContext(my_vis) == NULL)
		I_Error("Failed to create OpenGL context.\n");

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
		display_W = borderless_mode.width;
		display_H = borderless_mode.height;

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


void I_GetDesktopSize(int *width, int *height)
{
	*width  = display_W;
	*height = display_H;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
