//----------------------------------------------------------------------------
//  EDGE OpenGL Rendering (Screen Effects)
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

#include "dm_state.h"
#include "e_player.h"
#include "m_misc.h"
#include "r_misc.h"
#include "r_colormap.h"
#include "r_image.h"
#include "r_modes.h"
#include "r_texgl.h"
#include "w_wad.h"

#define DEBUG  0


int ren_extralight;

float ren_red_mul;
float ren_grn_mul;
float ren_blu_mul;

const colourmap_c *ren_fx_colmap;

cvar_c r_fadepower;
cvar_c debug_fullbright;


static inline float EffectStrength(player_t *player)
{
	if (player->effect_left >= EFFECT_MAX_TIME)
		return 1.0f;

	if (r_fadepower.d)
	{
		return player->effect_left / (float)EFFECT_MAX_TIME;
	}

	return (player->effect_left & 8) ? 1.0f : 0.0f;
}

//
// RGL_RainbowEffect
//
// Effects that modify all colours, e.g. nightvision green.
//
void RGL_RainbowEffect(player_t *player)
{
	ren_extralight = debug_fullbright.d ? 255 : player ? player->extralight * 16 : 0;

	ren_red_mul = ren_grn_mul = ren_blu_mul = 1.0f;

	ren_fx_colmap = NULL;

	if (! player)
		return;

	float s = EffectStrength(player);

	if (s > 0 && player->powers[PW_Invulnerable] > 0 &&
		(player->effect_left & 8))
	{
		if (var_invul_fx == INVULFX_Textured)
		{
			ren_fx_colmap = player->effect_colourmap;
		}
		else
		{
			ren_red_mul = 0.90f;
///???		ren_red_mul += (1.0f - ren_red_mul) * (1.0f - s);

			ren_grn_mul = ren_red_mul;
			ren_blu_mul = ren_red_mul;
		}

		ren_extralight = 255;
		return;
	}

	if (s > 0 && player->powers[PW_NightVision] > 0 &&
		player->effect_colourmap && !debug_fullbright.d)
	{
		float r, g, b;

		V_GetColmapRGB(player->effect_colourmap, &r, &g, &b);

		ren_red_mul = 1.0f - (1.0f - r) * s;
		ren_grn_mul = 1.0f - (1.0f - g) * s;
		ren_blu_mul = 1.0f - (1.0f - b) * s;

		ren_extralight = int(s * 255);
		return;
	}

	if (s > 0 && player->powers[PW_Infrared] > 0 && !debug_fullbright.d)
	{
		ren_extralight = int(s * 255);
		return;
	}
}


//
// RGL_ColourmapEffect
//
// For example: all white for invulnerability.
//
void RGL_ColourmapEffect(player_t *player)
{
	int x1, y1;
	int x2, y2;

	float s = EffectStrength(player);

	if (s > 0 && player->powers[PW_Invulnerable] > 0 &&
	    player->effect_colourmap && (player->effect_left & 8))
	{
		if (var_invul_fx > INVULFX_Complex)
			return;

		float r, g, b;

		V_GetColmapRGB(player->effect_colourmap, &r, &g, &b);

		r = 1.0f; // MAX(0.5f, r) * (s + 1.0f) / 2.0f;
		g = b = r;

		glColor4f(r, g, b, 0.0f);
		glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);

		glEnable(GL_BLEND);

		glBegin(GL_QUADS);
	  
		x1 = viewwindow_x;
		x2 = viewwindow_x + viewwindow_w;

		y1 = viewwindow_y + viewwindow_h;
		y2 = viewwindow_y;

		glVertex2i(x1, y1);
		glVertex2i(x2, y1);
		glVertex2i(x2, y2);
		glVertex2i(x1, y2);

		glEnd();
	  
		glDisable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
}

//
// RGL_PaletteEffect
//
// For example: red wash for pain.
//
void RGL_PaletteEffect(player_t *player)
{
	byte rgb_data[3];

	float s = EffectStrength(player);

	if (s > 0 && player->powers[PW_Invulnerable] > 0 &&
	    player->effect_colourmap && (player->effect_left & 8))
	{
		if (var_invul_fx != INVULFX_Complex)
			return;

		if (GLEW_ARB_imaging || GLEW_SGI_color_matrix)
		{
			glFlush();

			glMatrixMode(GL_COLOR);
			
			GLfloat gray_mat[16] =
			{
				0.33, 0.33, 0.33, 0,
				0.33, 0.33, 0.33, 0,
				0.33, 0.33, 0.33, 0,
				0,    0,    0,    1
			};

			glLoadMatrixf(gray_mat);

			int x = viewwindow_x;
			int y = viewwindow_y;

			glPixelZoom(1, 1);
			glRasterPos2i(x, y);

			glCopyPixels(x, y, viewwindow_w, viewwindow_h, GL_COLOR);

			glLoadIdentity();
		}
		return;
	}
	else if (s > 0 && player->powers[PW_NightVision] > 0 &&
	         player->effect_colourmap)
	{
		float r, g, b;
		V_GetColmapRGB(player->effect_colourmap, &r, &g, &b);
		glColor4f(r, g, b, 0.20f * s);
	}
	else
	{
		V_IndexColourToRGB(pal_black, rgb_data);

		int rgb_max = MAX(rgb_data[0], MAX(rgb_data[1], rgb_data[2]));

		if (rgb_max == 0)
			return;
	  
		rgb_max = MIN(200, rgb_max);

		glColor4f((float) rgb_data[0] / (float) rgb_max,
				  (float) rgb_data[1] / (float) rgb_max,
				  (float) rgb_data[2] / (float) rgb_max,
			      (float) rgb_max / 255.0f);
	}

	glEnable(GL_BLEND);

	glBegin(GL_QUADS);
  
	glVertex2i(0, SCREENHEIGHT);
	glVertex2i(SCREENWIDTH, SCREENHEIGHT);
	glVertex2i(SCREENWIDTH, 0);
	glVertex2i(0, 0);

	glEnd();
  
	glDisable(GL_BLEND);
}


//----------------------------------------------------------------------------
//  FUZZY Emulation
//----------------------------------------------------------------------------

const image_c *fuzz_image;

float fuzz_yoffset;


void FUZZ_Update(void)
{
	if (! fuzz_image)
	{
		fuzz_image = W_ImageLookup("FUZZ_MAP", INS_Texture, ILF_Exact|ILF_Null);
		if (! fuzz_image)
			I_Error("Cannot find essential image: FUZZ_MAP\n");
	}

	fuzz_yoffset = ((framecount * 3) & 1023) / 256.0;
}


void FUZZ_Adjust(vec2_t *tc, mobj_t *mo)
{
	tc->x += fmod(mo->x / 520.0, 1.0);
	tc->y += fmod(mo->y / 520.0, 1.0) + fuzz_yoffset;
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
