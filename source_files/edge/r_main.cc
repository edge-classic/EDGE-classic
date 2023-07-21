//----------------------------------------------------------------------------
//  EDGE OpenGL Rendering (Main Stuff)
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
#include "i_defs_gl.h"

#include "g_game.h"
#include "r_misc.h"
#include "r_gldefs.h"
#include "r_units.h"
#include "r_colormap.h"
#include "r_draw.h"
#include "r_modes.h"
#include "r_image.h"

#define DEBUG  0

// implementation limits

int glmax_lights;
int glmax_clip_planes;
int glmax_tex_size;
int glmax_tex_units;

DEF_CVAR(r_nearclip, "4",      CVAR_ARCHIVE)
DEF_CVAR(r_farclip,  "64000",  CVAR_ARCHIVE)
DEF_CVAR(r_culling, "0", CVAR_ARCHIVE)
DEF_CVAR_CLAMPED(r_culldist, "3000", CVAR_ARCHIVE, 1000.0f, 16000.0f)
DEF_CVAR(r_cullfog, "0", CVAR_ARCHIVE)

DEF_CVAR(r_fogofwar, "0", CVAR_ARCHIVE)

//
// RGL_SetupMatrices2D
//
// Setup the GL matrices for drawing 2D stuff.
//
void RGL_SetupMatrices2D(void)
{
	glViewport(0, 0, SCREENWIDTH, SCREENHEIGHT);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0f, (float)SCREENWIDTH, 
			0.0f, (float)SCREENHEIGHT, -1.0f, 1.0f);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	// turn off lighting stuff
	glDisable(GL_LIGHTING);
	glDisable(GL_COLOR_MATERIAL);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

//
// RGL_SetupMatricesWorld2D
//
// Setup the GL matrices for drawing 2D stuff within the "world" rendered by HUD_RenderWorld
//
void RGL_SetupMatricesWorld2D(void)
{
	glViewport(viewwindow_x, viewwindow_y, viewwindow_w, viewwindow_h);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho((float)viewwindow_x, (float)viewwindow_w, 
			(float)viewwindow_y, (float)viewwindow_h, -1.0f, 1.0f);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	// turn off lighting stuff
	glDisable(GL_LIGHTING);
	glDisable(GL_COLOR_MATERIAL);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

//
// RGL_SetupMatrices3D
//
// Setup the GL matrices for drawing 3D stuff.
//
void RGL_SetupMatrices3D(void)
{
	GLfloat ambient[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

	glViewport(viewwindow_x, viewwindow_y, viewwindow_w, viewwindow_h);

	// calculate perspective matrix

	glMatrixMode(GL_PROJECTION);

	glLoadIdentity();

	glFrustum(-view_x_slope * r_nearclip.f, view_x_slope * r_nearclip.f,
			  -view_y_slope * r_nearclip.f, view_y_slope * r_nearclip.f,
			  r_nearclip.f, r_farclip.f);

	// calculate look-at matrix

	glMatrixMode(GL_MODELVIEW);

	glLoadIdentity();
	glRotatef(270.0f - ANG_2_FLOAT(viewvertangle), 1.0f, 0.0f, 0.0f);
	glRotatef(90.0f - ANG_2_FLOAT(viewangle), 0.0f, 0.0f, 1.0f);
	glTranslatef(-viewx, -viewy, -viewz);

	// turn on lighting.  Some drivers (e.g. TNT2) don't work properly
	// without it.
	if (r_colorlighting.d)
	{
		glEnable(GL_LIGHTING);
		glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
	}
	else
		glDisable(GL_LIGHTING);

	if (r_colormaterial.d)
	{
		glEnable(GL_COLOR_MATERIAL);
		glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
	}
	else
		glDisable(GL_COLOR_MATERIAL);

	/* glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // Additive lighting */
}

static inline const char *SafeStr(const void *s)
{
	return s ? (const char *)s : "";
}

//
// RGL_CheckExtensions
//
// Based on code by Bruce Lewis.
//
void RGL_CheckExtensions(void)
{

	// -ACB- 2004/08/11 Made local: these are not yet used elsewhere
	std::string glstr_version (SafeStr(glGetString(GL_VERSION)));
	std::string glstr_renderer(SafeStr(glGetString(GL_RENDERER)));
	std::string glstr_vendor  (SafeStr(glGetString(GL_VENDOR)));

	I_Printf("OpenGL: Version: %s\n", glstr_version.c_str());
	I_Printf("OpenGL: Renderer: %s\n", glstr_renderer.c_str());
	I_Printf("OpenGL: Vendor: %s\n", glstr_vendor.c_str());

	// Check for a windows software renderer
	if (epi::case_cmp(glstr_vendor.c_str(), "Microsoft Corporation") == 0)
	{
		if (epi::case_cmp(glstr_renderer.c_str(), "GDI Generic") == 0)
		{
			I_Error("OpenGL: SOFTWARE Renderer!\n");
		}		
	}

#ifndef EDGE_GL_ES2

	// Check for various extensions

	// We only use VBOs on the GLES2/Web path for now
	//if (GLAD_GL_VERSION_1_5 || GLAD_GL_ARB_vertex_buffer_object)
	//{ /* OK */ }
	//else
	//	I_Error("OpenGL driver does not support Vertex Buffer Objects.\n");

	if (!GLAD_GL_VERSION_1_3)
		I_Error("OpenGL supported version below minimum! (Requires OpenGL 1.3).\n");

#endif

}

//
// RGL_SoftInit
//
// All the stuff that can be re-initialised multiple times.
// 
void RGL_SoftInit(void)
{
	glDisable(GL_BLEND);
	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_STENCIL_TEST);

	glDisable(GL_LINE_SMOOTH);

#ifndef EDGE_GL_ES2
	glDisable(GL_POLYGON_SMOOTH);
#endif

	glEnable(GL_NORMALIZE);

	glShadeModel(GL_SMOOTH);
	glDepthFunc(GL_LEQUAL);
	glAlphaFunc(GL_GREATER, 0);

	glFrontFace(GL_CW);
	glCullFace(GL_BACK);
	glDisable(GL_CULL_FACE);

	glHint(GL_FOG_HINT, GL_FASTEST);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
}

//
// RGL_Init
//
void RGL_Init(void)
{
	I_Printf("OpenGL: Initialising...\n");

	RGL_CheckExtensions();


	// read implementation limits
	{
		GLint max_lights;
		GLint max_clip_planes;
		GLint max_tex_size;
		GLint max_tex_units;

		glGetIntegerv(GL_MAX_LIGHTS,        &max_lights);
		glGetIntegerv(GL_MAX_CLIP_PLANES,   &max_clip_planes);
		glGetIntegerv(GL_MAX_TEXTURE_SIZE,  &max_tex_size);
		glGetIntegerv(GL_MAX_TEXTURE_UNITS, &max_tex_units);

		glmax_lights = max_lights;
		glmax_clip_planes = max_clip_planes;
		glmax_tex_size = max_tex_size;
		glmax_tex_units = max_tex_units;
	}

	I_Printf("OpenGL: Lights: %d  Clips: %d  Tex: %d  Units: %d\n",
			 glmax_lights, glmax_clip_planes, glmax_tex_size, glmax_tex_units);
  
	RGL_SoftInit();

	R2_InitUtil();

	// initialise unit system
	RGL_InitUnits();

	RGL_SetupMatrices2D();
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
