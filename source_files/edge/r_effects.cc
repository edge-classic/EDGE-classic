//----------------------------------------------------------------------------
//  EDGE OpenGL Rendering (Screen Effects)
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2023  The EDGE Team.
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
#include "i_defs_gl.h"

#include "dm_state.h"
#include "e_player.h"
#include "hu_draw.h" // HUD_* functions
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

DEF_CVAR(r_fadepower, "1.0", CVAR_ARCHIVE)
DEF_CVAR(debug_fullbright, "0", CVAR_CHEAT)


static inline float EffectStrength(player_t *player)
{
	if (player->effect_left >= EFFECT_MAX_TIME)
		return 1.0f;

	if (r_fadepower.d || reduce_flash)
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
		(player->effect_left & 8) && !reduce_flash)
	{
		if (var_invul_fx == INVULFX_Textured && !reduce_flash)
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
	
	//Lobo 2021: un-hardcode berserk color tint
	if (s > 0 && player->powers[PW_Berserk] > 0 &&
		player->effect_colourmap && !debug_fullbright.d)
	{
		float r, g, b;

		V_GetColmapRGB(player->effect_colourmap, &r, &g, &b);

		ren_red_mul = 1.0f - (1.0f - r) * s;
		ren_grn_mul = 1.0f - (1.0f - g) * s;
		ren_blu_mul = 1.0f - (1.0f - b) * s;

		// fallthrough...
	}

	// AJA 2022: handle BOOM colourmaps (linetype 242)
	sector_t *sector = player->mo->subsector->sector;

	if (sector->heightsec != NULL)
	{
		const colourmap_c *colmap = NULL;

		// see which region the camera is in
		if (viewz > sector->heightsec->c_h)
			colmap = sector->heightsec_side->top.boom_colmap;
		else if (viewz < sector->heightsec->f_h)
			colmap = sector->heightsec_side->bottom.boom_colmap;
		else
			colmap = sector->heightsec_side->middle.boom_colmap;

		ren_fx_colmap = colmap;
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
	    player->effect_colourmap && (player->effect_left & 8 || reduce_flash))
	{
		if (var_invul_fx == INVULFX_Textured && !reduce_flash)
			return;

		glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);

		if (!reduce_flash)
		{
			glColor4f(1.0f, 1.0f, 1.0f, 0.0f);

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
		}
		else
		{
			float old_alpha = HUD_GetAlpha();
			HUD_SetAlpha(0.0f);
			s = MAX(0.5f, s);
			HUD_ThinBox(hud_x_left, hud_visible_top, hud_x_right, hud_visible_bottom, RGB_MAKE(I_ROUND(s*255),I_ROUND(s*255),I_ROUND(s*255)), 25.0f);
			HUD_SetAlpha(old_alpha);
		}
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

	float old_alpha = HUD_GetAlpha();

	if (s > 0 && player->powers[PW_Invulnerable] > 0 &&
	    player->effect_colourmap && (player->effect_left & 8 || reduce_flash))
	{
		return;
	}
	else if (s > 0 && player->powers[PW_NightVision] > 0 &&
	         player->effect_colourmap)
	{
		float r, g, b;
		V_GetColmapRGB(player->effect_colourmap, &r, &g, &b);
		if (!reduce_flash)
			glColor4f(r, g, b, 0.20f * s);
		else
		{
			HUD_SetAlpha(0.20f * s);
			HUD_ThinBox(hud_x_left, hud_visible_top, hud_x_right, hud_visible_bottom, RGB_MAKE(I_ROUND(r*255),I_ROUND(g*255),I_ROUND(b*255)), 25.0f);
		}
	}
	else
	{
		V_IndexColourToRGB(pal_black, rgb_data, player->damagecount);

		int rgb_max = MAX(rgb_data[0], MAX(rgb_data[1], rgb_data[2]));

		if (rgb_max == 0)
			return;
	  
		rgb_max = MIN(200, rgb_max);

		if (!reduce_flash)
			glColor4f((float) rgb_data[0] / (float) rgb_max,
					(float) rgb_data[1] / (float) rgb_max,
					(float) rgb_data[2] / (float) rgb_max,
					(float) rgb_max / 255.0f);
		else
		{
			HUD_SetAlpha((float) rgb_max / 255.0f);
			HUD_ThinBox(hud_x_left, hud_visible_top, hud_x_right, hud_visible_bottom, RGB_MAKE(I_ROUND((float)rgb_data[0]/rgb_max*255),
				I_ROUND((float)rgb_data[1]/rgb_max*255),I_ROUND((float)rgb_data[2]/rgb_max*255)), 25.0f);
		}
	}

	HUD_SetAlpha(old_alpha);

	if (!reduce_flash)
	{
		glEnable(GL_BLEND);

		glBegin(GL_QUADS);

		glVertex2i(0, SCREENHEIGHT);
		glVertex2i(SCREENWIDTH, SCREENHEIGHT);
		glVertex2i(SCREENWIDTH, 0);
		glVertex2i(0, 0);

		glEnd();
	
		glDisable(GL_BLEND);
	}
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
