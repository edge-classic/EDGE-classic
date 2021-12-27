//----------------------------------------------------------------------------
//  EDGE OpenGL Rendering (Wipes)
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

#include "image_data.h"

#include "m_random.h"
#include "r_gldefs.h"
#include "r_wipe.h"
#include "r_image.h"
#include "r_modes.h"
#include "r_texgl.h"

#include <vector>

// we're limited to one wipe at a time...
static int cur_wipe_reverse = 0;
static wipetype_e cur_wipe_effect = WIPE_None;

static int cur_wipe_progress;
static int cur_wipe_lasttime;

static GLuint cur_wipe_tex = 0;
static float cur_wipe_right;
static float cur_wipe_top;


#define MELT_DIVS  128
static int melt_yoffs[MELT_DIVS+1];


static inline byte SpookyAlpha(int x, int y)
{
	y += (x & 32) / 2;

	x = (x & 31) - 15;
	y = (y & 31) - 15;

	return (x*x + y * y) / 2;
}

static void CaptureScreenAsTexture(bool speckly, bool spooky)
{
	int total_w = W_MakeValidSize(SCREENWIDTH);
	int total_h = W_MakeValidSize(SCREENHEIGHT);

	epi::image_data_c img(total_w, total_h, 4);

	img.Clear();

	cur_wipe_right = SCREENWIDTH  / (float)total_w;
	cur_wipe_top   = SCREENHEIGHT / (float)total_h;


	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	for (int y=0; y < SCREENHEIGHT; y++)
	{
		u8_t *dest = img.PixelAt(0, y);

		glReadPixels(0, y, SCREENWIDTH, 1, GL_RGBA, GL_UNSIGNED_BYTE, dest);

		int rnd_val = y;

		if (spooky)
		{
			for (int x=0; x < total_w; x++)
				dest[4*x+3] = SpookyAlpha(x, y);
		}
		else if (speckly)
		{
			for (int x=0; x < total_w; x++)
			{
				rnd_val = rnd_val * 1103515245 + 12345;

				dest[4*x+3] = (rnd_val >> 16);
			}
		}
	}

	cur_wipe_tex = R_UploadTexture(&img);
}

static void RGL_Init_Melt(void)
{
	int x, r;

	melt_yoffs[0] = - (M_Random() % 16);

	for (x=1; x <= MELT_DIVS; x++)
	{
		r = (M_Random() % 3) - 1;

		melt_yoffs[x] = melt_yoffs[x-1] + r;
		melt_yoffs[x] = MAX(-15, MIN(0, melt_yoffs[x]));
	}
}

static void RGL_Update_Melt(int tics)
{
	int x, r;

	for (; tics > 0; tics--)
	{
		for (x=0; x <= MELT_DIVS; x++)
		{
			r = melt_yoffs[x];

			if (r < 0)
				r = 1;
			else if (r > 15)
				r = 8;
			else
				r += 1;

			melt_yoffs[x] += r;
		}
	}
}


void RGL_InitWipe(int reverse, wipetype_e effect)
{
	cur_wipe_reverse  = reverse;
	cur_wipe_effect   = effect;

	cur_wipe_progress =  0;
	cur_wipe_lasttime = -1;

	if (cur_wipe_effect == WIPE_None)
		return;

	CaptureScreenAsTexture(effect == WIPE_Pixelfade,
		effect == WIPE_Spooky);

	if (cur_wipe_effect == WIPE_Melt)
		RGL_Init_Melt();
}


void RGL_StopWipe(void)
{
	cur_wipe_effect = WIPE_None;

	if (cur_wipe_tex != 0)
	{
		glDeleteTextures(1, &cur_wipe_tex);
		cur_wipe_tex = 0;
	}
}


//----------------------------------------------------------------------------

static void RGL_Wipe_Fading(float how_far)
{
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);

	glBindTexture(GL_TEXTURE_2D, cur_wipe_tex);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f - how_far);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	GLint wipe_vertices[] =
	{
		0, 0,
		0, SCREENHEIGHT,
		SCREENWIDTH, SCREENHEIGHT,
		SCREENWIDTH, 0
	};
	GLfloat wipe_texcoords[] =
	{
		0.0f, 0.0f,
		0.0f, cur_wipe_top,
		cur_wipe_right, cur_wipe_top,
		cur_wipe_right, 0.0f
	};

	glVertexPointer(2, GL_INT, 0, wipe_vertices);
	glTexCoordPointer(2, GL_FLOAT, 0, wipe_texcoords);
	glDrawArrays(GL_QUADS, 0, 4);

	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);

	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
}

static void RGL_Wipe_Pixelfade(float how_far)
{
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);

	glAlphaFunc(GL_GEQUAL, how_far);

	glBindTexture(GL_TEXTURE_2D, cur_wipe_tex);
	glColor3f(1.0f, 1.0f, 1.0f);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	GLint wipe_vertices[] =
	{
		0, 0,
		0, SCREENHEIGHT,
		SCREENWIDTH, SCREENHEIGHT,
		SCREENWIDTH, 0
	};
	GLfloat wipe_texcoords[] =
	{
		0.0f, 0.0f,
		0.0f, cur_wipe_top,
		cur_wipe_right, cur_wipe_top,
		cur_wipe_right, 0.0f
	};

	glVertexPointer(2, GL_INT, 0, wipe_vertices);
	glTexCoordPointer(2, GL_FLOAT, 0, wipe_texcoords);
	glDrawArrays(GL_QUADS, 0, 4);

	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);

	glDisable(GL_ALPHA_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);

	glAlphaFunc(GL_GREATER, 0);
}

static void RGL_Wipe_Melt(void)
{
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);

	glBindTexture(GL_TEXTURE_2D, cur_wipe_tex);
	glColor3f(1.0f, 1.0f, 1.0f);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	std::vector<GLfloat> wipe_vertices;
	std::vector<GLfloat> wipe_texcoords;

	for (int x=0; x <= MELT_DIVS; x++)
	{
		int yoffs = MAX(0, melt_yoffs[x]);

		float sx = (float) x * SCREENWIDTH / MELT_DIVS;
		float sy = (float) (200 - yoffs) * SCREENHEIGHT / 200.0f;

		float tx = cur_wipe_right * (float) x / MELT_DIVS;

		wipe_vertices.push_back(sx);
		wipe_vertices.push_back(sy);
		wipe_vertices.push_back(sx);
		wipe_vertices.push_back(sy - SCREENHEIGHT);
		wipe_texcoords.push_back(tx);
		wipe_texcoords.push_back(cur_wipe_top);
		wipe_texcoords.push_back(tx);
		wipe_texcoords.push_back(0.0f);
	}

	glVertexPointer(2, GL_FLOAT, 0, wipe_vertices.data());
	glTexCoordPointer(2, GL_FLOAT, 0, wipe_texcoords.data());
	glDrawArrays(GL_QUAD_STRIP, 0, wipe_vertices.size() / 2);

	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);

	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
}

static void RGL_Wipe_Slide(float how_far, float dx, float dy)
{
	dx *= how_far;
	dy *= how_far;

	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);

	glBindTexture(GL_TEXTURE_2D, cur_wipe_tex);
	glColor3f(1.0f, 1.0f, 1.0f);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	GLfloat wipe_vertices[] =
	{
		dx, dy,
		dx, dy + SCREENHEIGHT,
		dx + SCREENWIDTH, dy + SCREENHEIGHT,
		dx + SCREENWIDTH, dy
	};
	GLfloat wipe_texcoords[] =
	{
		0.0f, 0.0f,
		0.0f, cur_wipe_top,
		cur_wipe_right, cur_wipe_top,
		cur_wipe_right, 0.0f
	};

	glVertexPointer(2, GL_FLOAT, 0, wipe_vertices);
	glTexCoordPointer(2, GL_FLOAT, 0, wipe_texcoords);
	glDrawArrays(GL_QUADS, 0, 4);

	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);

	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
}

static void RGL_Wipe_Doors(float how_far)
{
	float dx = cos(how_far * M_PI / 2) * (SCREENWIDTH/2);
	float dy = sin(how_far * M_PI / 2) * (SCREENHEIGHT/3);

	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);

	glBindTexture(GL_TEXTURE_2D, cur_wipe_tex);
	glColor3f(1.0f, 1.0f, 1.0f);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	std::vector<GLfloat> wipe_vertices;
	std::vector<GLfloat> wipe_texcoords;

	for (int column = 0; column < 5; column++)
	{
		float c = column / 10.0f;
		float e = column / 5.0f;

		for (int side = 0; side < 2; side++)
		{
			float t_x1 = (side == 0) ? c : (0.9f - c);
			float t_x2 = t_x1 + 0.1f;

			float v_x1 = (side == 0) ? (dx * e) : (SCREENWIDTH - dx * (e + 0.2f));
			float v_x2 = v_x1 + dx * 0.2f;

			float v_y1 = (side == 0) ? (dy * e) : (dy * (e + 0.2f));
			float v_y2 = (side == 1) ? (dy * e) : (dy * (e + 0.2f));

			for (int row = 0; row <= 5; row++)
			{
				float t_y = cur_wipe_top * row / 5.0f;

				float j1 = (SCREENHEIGHT - v_y1 * 2.0f) / 5.0f;
				float j2 = (SCREENHEIGHT - v_y2 * 2.0f) / 5.0f;

				wipe_vertices.push_back(v_x2);
				wipe_vertices.push_back(v_y2 + j2 * row);
				wipe_vertices.push_back(v_x1);
				wipe_vertices.push_back(v_y1 + j1 * row);
				wipe_texcoords.push_back(t_x2 * cur_wipe_right);
				wipe_texcoords.push_back(t_y);
				wipe_texcoords.push_back(t_x1 * cur_wipe_right);
				wipe_texcoords.push_back(t_y);
			}
			glVertexPointer(2, GL_FLOAT, 0, wipe_vertices.data());
			glTexCoordPointer(2, GL_FLOAT, 0, wipe_texcoords.data());
			glDrawArrays(GL_QUAD_STRIP, 0, wipe_vertices.size() / 2);
			wipe_vertices.clear();
			wipe_texcoords.clear();
		}
	}

	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);

	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
}

bool RGL_DoWipe(void)
{
	//
	// NOTE: we assume 2D project matrix is already setup.
	//

	if (cur_wipe_effect == WIPE_None || cur_wipe_tex == 0)
		return true;

	// determine how many tics since we started.  If this is the first
	// call to DoWipe() since InitWipe(), then the clock starts now.
	int nowtime = I_GetTime();
	int tics = 0;

	if (cur_wipe_lasttime >= 0)
		tics = MAX(0, nowtime - cur_wipe_lasttime);
	
	cur_wipe_lasttime = nowtime;

	// hack for large delays (like when loading a level)
	tics = MIN(6, tics);

	cur_wipe_progress += tics;

	if (cur_wipe_progress > 40)  // FIXME: have option for wipe time
		return true;

	float how_far = (float) cur_wipe_progress / 40.0f;

	switch (cur_wipe_effect)
	{
		case WIPE_Melt:
			RGL_Wipe_Melt();
			RGL_Update_Melt(tics);
			break;

		case WIPE_Top:
			RGL_Wipe_Slide(how_far, 0, +SCREENHEIGHT);
			break;

		case WIPE_Bottom:
			RGL_Wipe_Slide(how_far, 0, -SCREENHEIGHT);
			break;

		case WIPE_Left:
			RGL_Wipe_Slide(how_far, -SCREENWIDTH, 0);
			break;

		case WIPE_Right:
			RGL_Wipe_Slide(how_far, +SCREENWIDTH, 0);
			break;


		case WIPE_Doors:
			RGL_Wipe_Doors(how_far);
			break;

		case WIPE_Spooky:  // difference is in alpha channel
		case WIPE_Pixelfade:
			RGL_Wipe_Pixelfade(how_far);
			break;

		case WIPE_Crossfade:
		default:
			RGL_Wipe_Fading(how_far);
			break;
	}

	return false;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
