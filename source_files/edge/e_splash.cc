//----------------------------------------------------------------------------
//  EDGE TITLE SCREEN
//----------------------------------------------------------------------------
// 
//  Copyright (c) 2009  Andrew Apted
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
// #include "hu_draw.h"
#include "r_modes.h"
#include "r_texgl.h"


extern unsigned char rndtable[256];


static int color;  // 0 to 7

typedef struct
{
	float x, y;
	float t;
	float size;
	float r, g, b;
}
star_info_t;

static std::list<star_info_t> star_field;

static GLuint star_tex;
static GLuint logo_tex;


static const byte edge_logo_data[] =
{
	200,0, 200,0, 200,0, 200,0, 200,0, 200,0, 200,0, 200,0,
	200,0, 200,0, 200,0, 118,0, 1,255, 39,0, 1,255, 48,0,
	1,255, 127,0, 1,255, 28,0, 9,255, 1,0, 1,255, 21,0,
	7,255, 11,0, 1,255, 48,0, 1,255, 6,0, 3,255, 1,246,
	1,224, 1,176, 1,88, 1,1, 4,0, 1,15, 1,142, 1,225,
	1,250, 1,225, 1,141, 1,14, 4,0, 1,15, 1,142, 1,225,
	1,250, 1,225, 1,141, 1,14, 3,0, 1,255, 1,199, 6,0,
	1,199, 1,255, 7,0, 1,2, 1,100, 1,195, 1,240, 1,250,
	1,228, 1,169, 1,66, 36,0, 7,255, 20,0, 1,255, 32,0,
	1,255, 5,0, 1,255, 21,0, 1,255, 17,0, 1,255, 48,0,
	1,255, 6,0, 1,255, 2,0, 1,6, 1,29, 1,85, 1,199,
	1,184, 1,7, 2,0, 1,10, 1,208, 1,158, 1,37, 1,6,
	1,37, 1,157, 1,206, 1,9, 2,0, 1,10, 1,208, 1,158,
	1,37, 1,6, 1,37, 1,157, 1,206, 1,9, 2,0, 1,255,
	1,215, 1,63, 4,0, 1,64, 1,214, 1,255, 6,0, 1,4,
	1,178, 1,177, 1,60, 1,12, 1,5, 1,31, 1,91, 1,192,
	36,0, 1,255, 59,0, 1,255, 5,0, 1,255, 1,90, 1,206,
	1,248, 1,241, 1,172, 1,21, 3,0, 1,78, 1,207, 1,249,
	1,231, 1,135, 1,4, 6,0, 1,255, 8,0, 1,255, 1,90,
	1,206, 1,248, 1,241, 1,172, 1,21, 2,0, 1,255, 1,90,
	1,206, 1,248, 1,241, 1,172, 1,21, 2,0, 1,34, 1,178,
	1,243, 1,248, 1,183, 1,22, 2,0, 1,255, 1,90, 1,206,
	1,248, 1,241, 1,172, 1,21, 3,0, 1,78, 1,204, 1,248,
	1,225, 1,99, 3,0, 1,78, 1,207, 1,249, 1,231, 1,135,
	1,4, 3,0, 1,97, 1,221, 1,250, 1,219, 1,103, 1,255,
	6,0, 1,255, 5,0, 1,4, 1,186, 1,121, 2,0, 1,126,
	1,168, 5,0, 1,169, 1,124, 2,0, 1,126, 1,168, 5,0,
	1,169, 1,124, 2,0, 1,255, 1,89, 1,182, 4,0, 1,183,
	1,88, 1,255, 6,0, 1,112, 1,173, 9,0, 1,34, 1,178,
	1,243, 1,248, 1,183, 1,22, 2,0, 1,255, 1,105, 1,222,
	1,249, 1,210, 1,57, 1,93, 1,218, 1,249, 1,203, 1,37,
	3,0, 1,78, 1,207, 1,249, 1,231, 1,135, 1,4, 6,0,
	1,255, 8,0, 1,255, 1,90, 1,206, 1,248, 1,241, 1,172,
	1,21, 3,0, 1,102, 1,223, 1,250, 1,219, 1,103, 1,255,
	2,0, 1,255, 2,0, 1,255, 1,90, 1,206, 1,248, 1,241,
	1,172, 1,21, 3,0, 1,78, 1,207, 1,249, 1,231, 1,135,
	1,4, 14,0, 1,255, 5,0, 1,255, 1,209, 1,59, 1,6,
	1,21, 1,158, 1,175, 2,0, 1,71, 1,205, 1,54, 1,5,
	1,31, 1,179, 1,133, 6,0, 1,255, 8,0, 1,255, 1,209,
	1,59, 1,6, 1,21, 1,158, 1,175, 2,0, 1,255, 1,209,
	1,59, 1,6, 1,21, 1,158, 1,175, 2,0, 1,213, 1,66,
	1,9, 1,15, 1,144, 1,164, 2,0, 1,255, 1,209, 1,59,
	1,6, 1,21, 1,158, 1,175, 2,0, 1,75, 1,227, 1,68,
	1,7, 1,36, 1,162, 2,0, 1,71, 1,205, 1,54, 1,5,
	1,31, 1,179, 1,133, 2,0, 1,81, 1,220, 1,54, 1,6,
	1,56, 1,222, 1,255, 6,0, 1,255, 6,0, 1,57, 1,210,
	2,0, 1,211, 1,54, 5,0, 1,54, 1,210, 2,0, 1,211,
	1,54, 5,0, 1,54, 1,210, 2,0, 1,255, 1,2, 1,213,
	1,46, 2,0, 1,47, 1,212, 1,2, 1,255, 6,0, 1,207,
	1,53, 9,0, 1,213, 1,66, 1,9, 1,15, 1,144, 1,164,
	2,0, 1,255, 1,188, 1,33, 1,8, 1,129, 1,239, 1,187,
	1,33, 1,8, 1,129, 1,186, 2,0, 1,71, 1,205, 1,54,
	1,5, 1,31, 1,179, 1,133, 6,0, 1,255, 8,0, 1,255,
	1,209, 1,59, 1,6, 1,21, 1,158, 1,175, 2,0, 1,87,
	1,219, 1,54, 1,6, 1,54, 1,220, 1,255, 2,0, 1,255,
	2,0, 1,255, 1,209, 1,59, 1,6, 1,21, 1,158, 1,175,
	2,0, 1,71, 1,205, 1,54, 1,5, 1,31, 1,179, 1,133,
	14,0, 1,255, 5,0, 1,255, 1,41, 3,0, 1,17, 1,244,
	2,0, 1,194, 1,47, 3,0, 1,23, 1,229, 6,0, 7,255,
	2,0, 1,255, 1,41, 3,0, 1,17, 1,244, 2,0, 1,255,
	1,41, 3,0, 1,17, 1,244, 6,0, 1,10, 1,234, 2,0,
	1,255, 1,41, 3,0, 1,17, 1,244, 2,0, 1,198, 1,76,
	6,0, 1,194, 1,47, 3,0, 1,23, 1,229, 2,0, 1,199,
	1,70, 3,0, 1,72, 1,255, 6,0, 1,255, 6,0, 1,10,
	1,246, 2,0, 1,247, 1,8, 5,0, 1,8, 1,246, 2,0,
	1,247, 1,8, 5,0, 1,8, 1,246, 2,0, 1,255, 1,0,
	1,87, 1,166, 2,0, 1,167, 1,86, 1,0, 1,255, 6,0,
	1,246, 1,8, 13,0, 1,10, 1,234, 2,0, 1,255, 1,33,
	2,0, 1,13, 1,255, 1,33, 2,0, 1,13, 1,245, 2,0,
	1,194, 1,47, 3,0, 1,23, 1,229, 6,0, 7,255, 2,0,
	1,255, 1,41, 3,0, 1,17, 1,244, 2,0, 1,201, 1,69,
	3,0, 1,70, 1,255, 2,0, 1,255, 2,0, 1,255, 1,41,
	3,0, 1,17, 1,244, 2,0, 1,194, 1,47, 3,0, 1,23,
	1,229, 14,0, 1,255, 5,0, 1,255, 1,1, 4,0, 1,255,
	2,0, 1,245, 2,252, 1,253, 1,254, 1,255, 1,254, 6,0,
	1,255, 8,0, 1,255, 1,1, 4,0, 1,255, 2,0, 1,255,
	1,1, 4,0, 1,255, 2,0, 1,35, 1,181, 1,241, 2,255,
	1,254, 2,0, 1,255, 1,1, 4,0, 1,255, 2,0, 1,246,
	1,10, 6,0, 1,245, 2,252, 1,253, 1,254, 1,255, 1,254,
	2,0, 1,246, 1,9, 3,0, 1,10, 1,255, 6,0, 1,255,
	6,0, 1,10, 1,245, 2,0, 1,247, 1,8, 5,0, 1,8,
	1,246, 2,0, 1,247, 1,8, 5,0, 1,8, 1,246, 2,0,
	1,255, 1,0, 1,2, 1,208, 2,33, 1,208, 1,1, 1,0,
	1,255, 6,0, 1,247, 1,9, 4,0, 3,255, 2,0, 1,35,
	1,181, 1,241, 2,255, 1,254, 2,0, 1,255, 4,0, 1,255,
	4,0, 1,255, 2,0, 1,245, 2,252, 1,253, 1,254, 1,255,
	1,254, 6,0, 1,255, 8,0, 1,255, 1,1, 4,0, 1,255,
	2,0, 1,247, 1,9, 3,0, 1,10, 1,255, 2,0, 1,255,
	2,0, 1,255, 1,1, 4,0, 1,255, 2,0, 1,245, 2,252,
	1,253, 1,254, 1,255, 1,254, 14,0, 1,255, 5,0, 1,255,
	5,0, 1,255, 2,0, 1,247, 1,17, 11,0, 1,255, 8,0,
	1,255, 5,0, 1,255, 2,0, 1,255, 5,0, 1,255, 2,0,
	1,200, 1,128, 1,23, 1,2, 1,4, 1,255, 2,0, 1,255,
	5,0, 1,255, 2,0, 1,246, 1,10, 6,0, 1,247, 1,17,
	7,0, 1,246, 1,9, 3,0, 1,10, 1,255, 6,0, 1,255,
	6,0, 1,58, 1,208, 2,0, 1,211, 1,54, 5,0, 1,54,
	1,210, 2,0, 1,211, 1,54, 5,0, 1,54, 1,210, 2,0,
	1,255, 2,0, 1,85, 1,149, 1,150, 1,84, 2,0, 1,255,
	6,0, 1,209, 1,55, 6,0, 1,255, 2,0, 1,200, 1,128,
	1,23, 1,2, 1,4, 1,255, 2,0, 1,255, 4,0, 1,255,
	4,0, 1,255, 2,0, 1,247, 1,17, 11,0, 1,255, 8,0,
	1,255, 5,0, 1,255, 2,0, 1,247, 1,9, 3,0, 1,10,
	1,255, 2,0, 1,255, 2,0, 1,255, 5,0, 1,255, 2,0,
	1,247, 1,17, 19,0, 1,255, 5,0, 1,255, 5,0, 1,255,
	2,0, 1,196, 1,96, 11,0, 1,255, 8,0, 1,255, 5,0,
	1,255, 2,0, 1,255, 5,0, 1,255, 2,0, 1,251, 1,8,
	2,0, 1,56, 1,255, 2,0, 1,255, 5,0, 1,255, 2,0,
	1,198, 1,75, 6,0, 1,196, 1,96, 7,0, 1,199, 1,70,
	3,0, 1,72, 1,255, 6,0, 1,255, 5,0, 1,3, 1,184,
	1,119, 2,0, 1,126, 1,168, 5,0, 1,168, 1,124, 2,0,
	1,126, 1,168, 5,0, 1,168, 1,124, 2,0, 1,255, 2,0,
	1,1, 2,201, 1,1, 2,0, 1,255, 6,0, 1,118, 1,176,
	6,0, 1,255, 2,0, 1,251, 1,8, 2,0, 1,56, 1,255,
	2,0, 1,255, 4,0, 1,255, 4,0, 1,255, 2,0, 1,196,
	1,96, 11,0, 1,255, 8,0, 1,255, 5,0, 1,255, 2,0,
	1,201, 1,68, 3,0, 1,69, 1,255, 2,0, 1,255, 2,0,
	1,255, 5,0, 1,255, 2,0, 1,196, 1,96, 19,0, 1,255,
	5,0, 1,255, 5,0, 1,255, 2,0, 1,67, 1,236, 1,87,
	2,11, 1,67, 1,181, 6,0, 1,255, 8,0, 1,255, 5,0,
	1,255, 2,0, 1,255, 5,0, 1,255, 2,0, 1,213, 1,113,
	1,8, 1,43, 1,209, 1,255, 2,0, 1,255, 5,0, 1,255,
	2,0, 1,77, 1,226, 1,66, 1,6, 1,36, 1,162, 2,0,
	1,67, 1,236, 1,87, 2,11, 1,67, 1,181, 2,0, 1,84,
	1,219, 1,54, 1,5, 1,55, 1,222, 1,255, 6,0, 1,255,
	2,0, 1,5, 1,27, 1,83, 1,196, 1,184, 1,7, 2,0,
	1,11, 1,209, 1,157, 1,36, 1,5, 1,36, 1,156, 1,206,
	1,9, 2,0, 1,11, 1,209, 1,157, 1,36, 1,5, 1,36,
	1,156, 1,206, 1,9, 2,0, 1,255, 8,0, 1,255, 6,0,
	1,6, 1,191, 1,181, 1,64, 1,14, 1,2, 1,19, 1,77,
	1,255, 2,0, 1,213, 1,113, 1,8, 1,43, 1,209, 1,255,
	2,0, 1,255, 4,0, 1,255, 4,0, 1,255, 2,0, 1,67,
	1,236, 1,87, 2,11, 1,67, 1,181, 6,0, 1,255, 8,0,
	1,255, 5,0, 1,255, 2,0, 1,87, 1,218, 1,52, 1,5,
	1,52, 1,218, 1,255, 2,0, 1,255, 2,0, 1,255, 5,0,
	1,255, 2,0, 1,67, 1,236, 1,87, 2,11, 1,67, 1,181,
	14,0, 1,255, 5,0, 1,255, 5,0, 1,255, 3,0, 1,63,
	1,190, 1,244, 1,246, 1,191, 1,75, 6,0, 7,255, 2,0,
	1,255, 5,0, 1,255, 2,0, 1,255, 5,0, 1,255, 2,0,
	1,54, 1,212, 1,251, 1,225, 1,109, 1,255, 2,0, 1,255,
	5,0, 1,255, 3,0, 1,82, 1,207, 1,249, 1,223, 1,98,
	3,0, 1,63, 1,190, 1,244, 1,246, 1,191, 1,75, 3,0,
	1,99, 1,223, 1,250, 1,219, 1,103, 1,255, 6,0, 3,255,
	1,247, 1,225, 1,177, 1,89, 1,1, 4,0, 1,16, 1,143,
	1,227, 1,251, 1,226, 1,142, 1,14, 4,0, 1,16, 1,143,
	1,227, 1,251, 1,226, 1,142, 1,14, 3,0, 1,255, 8,0,
	1,255, 7,0, 1,5, 1,113, 1,204, 1,245, 1,250, 1,225,
	1,166, 1,61, 2,0, 1,54, 1,212, 1,251, 1,225, 1,109,
	1,255, 2,0, 1,255, 4,0, 1,255, 4,0, 1,255, 3,0,
	1,63, 1,190, 1,244, 1,246, 1,191, 1,75, 6,0, 7,255,
	2,0, 1,255, 5,0, 1,255, 3,0, 1,103, 1,224, 1,250,
	1,219, 1,107, 1,249, 2,0, 1,255, 2,0, 1,255, 5,0,
	1,255, 3,0, 1,63, 1,190, 1,244, 1,246, 1,191, 1,75,
	233,0, 1,50, 1,215, 250,0, 1,163, 1,37, 1,4, 1,48,
	1,204, 1,117, 250,0, 1,95, 1,219, 1,251, 1,224, 1,126,
	1,2, 200,0, 200,0, 200,0, 200,0, 200,0, 200,0, 200,0,
	200,0, 200,0, 200,0, 200,0, 131,0,

	0,0
};


static inline int FudgeDist(int x, int y)
{
	if (x < 0) x = -x;
	if (y < 0) y = -y;

	if (x < y) { int tmp = x; x = y; y = tmp; }

	if (x == 0) return 0;

	int div = x * 23 / 10;

	return x + y * y / div;
}


static inline int FudgeAngle(int x, int y)
{
	if (abs(x) < abs(y))
	{
		return 64 - FudgeAngle(y, x);
	}

	if (x == 0)
		return 0;
	
	if (x > 0)
		return 32 * y / x;
	else
		return 128 + 32 * y / x;
}



#define STAR_ALPHA(t_diff)  (0.99 - (t_diff) /  7000.0)

#define STAR_Z(t_diff)  (0.3 + (t_diff) /  100.0)


static void CreateStarTex(void)
{
	epi::image_data_c *img = new epi::image_data_c(128, 128);

	for (int y = 0; y < 128; y++)
	for (int x = 0; x < 128; x++)
	{
		u8_t *pix = img->PixelAt(x, y);

		int dist = FudgeDist (x-64, y-64);
		int ang  = FudgeAngle(x-64, y-64) & 0xFF;

		int rnd = rndtable[ang / 4];
		int val = 250 - dist * 4;

		rnd = rnd + ((val * val) >> 8);
		val = val * (rnd + 16) / 384;

		*pix = CLAMP(0, val, 255);

		pix[1] = pix[2] = pix[0];
	}

	star_tex = R_UploadTexture(img, UPL_Smooth);

	delete img;
}

static void CreateLogoTex(void)
{
	epi::image_data_c *img = new epi::image_data_c(256, 32);

	const byte *src = edge_logo_data;
	byte *dest = img->PixelAt(0, 0);

	for (; *src; src += 2)
	{
		for (int j=0; j < src[0]; j++)
		{
			dest[0] = dest[1] = dest[2] = src[1];
			dest += 3;
		}
	}

	logo_tex = R_UploadTexture(img, UPL_Smooth);

	delete img;
}

static void RemoveDeadStars(int millies)
{
	for (;;)
	{
		if (star_field.empty())
			break;

		float t_diff = star_field.front().t - millies;

		float z = STAR_Z(t_diff);

		if (z * 611 < 6930)
			break;

		star_field.pop_front();
	}
}


static void DrawOneStar(float mx, float my, float sz,
                        float r, float g, float b, float alpha)
{
	float x = SCREENWIDTH  * mx / 320.0f;
	float y = SCREENHEIGHT * my / 200.0f;

	sz = sz * SCREENWIDTH / 640.0f;

	glColor4f(r, g, b, alpha);

	glBegin(GL_POLYGON);

	glTexCoord2f(0.0, 0.0); glVertex2f(x - sz, y - sz);
	glTexCoord2f(0.0, 1.0); glVertex2f(x - sz, y + sz);
	glTexCoord2f(1.0, 1.0); glVertex2f(x + sz, y + sz);
	glTexCoord2f(1.0, 0.0); glVertex2f(x + sz, y - sz);

	glEnd();
}

static void DrawStars(int millies)
{
	// additive blending
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, star_tex);

	std::list<star_info_t>::iterator SI;

	for (SI = star_field.begin(); SI != star_field.end(); SI++)
	{
		float t_diff = millies - SI->t;

		float alpha = STAR_ALPHA(t_diff);
		float z     = STAR_Z(t_diff);

		if (z * 611 < 6930)
		{
			float w = SI->size / (7000 - z * 611);

			float dx = (SI->x - 160) * z / 7.0;
			float dy = (SI->y - 100) * z / 12.0;
		
			DrawOneStar(SI->x + dx, SI->y + dy, w,
						SI->r, SI->g, SI->b, alpha);
		}
  	}

	glDisable(GL_TEXTURE_2D);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}


static const char *edge_shape[7] =
{
	"                 ",
	" 3#7 ##1 3#7 3#7 ",
	" #   #8# #   #   ",
	" #7  # # # 1 #7  ",
	" #   #2# #2# #   ",
	" 9#1 ##7 9## 9#1 ",
	"                 ",
};

static int edge_row_sizes[] =
{ 64, 24, 12, 12, 12, 24, 52 };

static int edge_column_sizes[] =
{
   11, 24,28,18, 6, 24,28,18, 6, 24,28,18, 6, 24,28,18, 11,
};

static void DrawName(int millies)
{
	int rows = 7;
	int cols = 17;

	glColor3f(0, 0, 0);

	int y1 = 0;
	int y2 = 0;

	for (int y = 0; y < rows; y++, y1 = y2)
	{
		y2 = y1 + edge_row_sizes[y];

		int x1 = 0;
		int x2 = 0;

		for (int x = 0; x < cols; x++, x1 = x2)
		{
			x2 = x1 + edge_column_sizes[x];

			char shape = edge_shape[rows-1-y][x];

			if (shape == '#')
				continue;

			int ix1 = x1 * SCREENWIDTH / 320;
			int ix2 = x2 * SCREENWIDTH / 320;

			int iy1 = y1 * SCREENHEIGHT / 200;
			int iy2 = y2 * SCREENHEIGHT / 200;

			int hx = (ix1 + ix2) / 2;
			int hy = (iy1 + iy2) / 2;

			int ix3 = ix1 + (ix2 - ix1) / 7;
			int ix4 = ix2 - (ix2 - ix1) / 7;

			int iy3 = iy1 + (iy2 - iy1) / 7;
			int iy4 = iy2 - (iy2 - iy1) / 7;

			switch (shape)
			{
				case '9':
					glBegin(GL_TRIANGLE_FAN);
					glVertex2i(ix1, iy1);
					glVertex2i(ix2, iy1);
					glVertex2i(hx,  iy3);
					glVertex2i(ix3, hy);
					glVertex2i(ix1, iy2);
					glEnd();
					break;

				case '7':
					glBegin(GL_TRIANGLE_FAN);
					glVertex2i(ix2, iy1);
					glVertex2i(ix1, iy1);
					glVertex2i(hx,  iy3);
					glVertex2i(ix4, hy);
					glVertex2i(ix2, iy2);
					glEnd();
					break;

				case '3':
					glBegin(GL_TRIANGLE_FAN);
					glVertex2i(ix1, iy2);
					glVertex2i(ix2, iy2);
					glVertex2i(hx,  iy4);
					glVertex2i(ix3, hy);
					glVertex2i(ix1, iy1);
					glEnd();
					break;

				case '1':
					glBegin(GL_TRIANGLE_FAN);
					glVertex2i(ix2, iy2);
					glVertex2i(ix2, iy1);
					glVertex2i(ix4, hy);
					glVertex2i(hx,  iy4);
					glVertex2i(ix1, iy2);
					glEnd();
					break;

				case '8':
					glBegin(GL_POLYGON);
					glVertex2i(ix2, iy1);
					glVertex2i(ix1, iy1);
					glVertex2i(ix1, iy2);
					glVertex2f(hx,  iy4);
					glVertex2f(ix4, hy);
					glEnd();
					break;

				case '2':
					glBegin(GL_POLYGON);
					glVertex2i(ix1, iy1);
					glVertex2i(ix1, iy2);
					glVertex2i(ix2, iy2);
					glVertex2f(ix4, hy);
					glVertex2f(hx,  iy3);
					glEnd();
					break;

				default:
					glBegin(GL_POLYGON);
					glVertex2i(ix1, iy1);
					glVertex2i(ix1, iy2);
					glVertex2i(ix2, iy2);
					glVertex2i(ix2, iy1);
					glEnd();
					break;
			}
		}
	}
}


static void DrawLogo(int millies)
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, logo_tex);

	float alpha = millies / 1800.0;
	if (alpha > 1)
		alpha = 3.0 - alpha * 2.0;

	glColor4f(1.0, 1.0, 1.0, alpha);

	int x1 = SCREENWIDTH / 6;
	int x2 = SCREENWIDTH * 5 / 6;

	int h  = (x2 - x1 + 7) / 8;
	int y1 = SCREENHEIGHT / 6 - h/2;

	glBegin(GL_POLYGON);

	glTexCoord2f(0.0, 1.0); glVertex2f(x1, y1);
	glTexCoord2f(0.0, 0.0); glVertex2f(x1, y1+h);
	glTexCoord2f(1.0, 0.0); glVertex2f(x2, y1+h);
	glTexCoord2f(1.0, 1.0); glVertex2f(x2, y1);

	glEnd();

	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}


static void InsertNewStars(int millies)
{
	static int all_count = 0;

	float r_factor = (color & 4) ? 1.0f : 0.5f;
	float g_factor = (color & 2) ? 1.0f : 0.5f;
	float b_factor = (color & 1) ? 1.0f : 0.5f;

	int total = millies;

	if (millies > 7740)
	{
		float f = (2800 - millies) / float(2800 - 1740);
		total = millies + int(millies * f);
	}

	// total = total * 5 / 4;

	for (; all_count < total; all_count++)
	{
		star_info_t st;

		st.x = (rand() & 0x0FFF) / 12.80f;
		st.y = (rand() & 0x0FFF) / 20.48f;

		st.t = millies;

		st.size = ((rand() & 0x0FFF) / 4096.0 + 1) * 16000.0;

		if (color == 0)
		{
			st.r = pow((rand() & 0xFF) / 255.0, 0.3);
			st.g = st.r;
			st.b = st.r;
		}
		else
		{
			st.r = pow((rand() & 0xFF) / 255.0, 0.3) * r_factor; 
			st.g = pow((rand() & 0xFF) / 255.0, 0.3) * g_factor;
			st.b = pow((rand() & 0xFF) / 255.0, 0.3) * b_factor;
		}

		star_field.push_back(st);
	}
}


bool E_DrawSplash(int millies)
{
	int max_time = 2700;

	millies = millies - 400;

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glEnable(GL_BLEND);

	if (millies > 0 && millies < max_time)
	{
		RemoveDeadStars(millies);

		DrawStars(millies);

  		DrawName(millies);
  		DrawLogo(millies);

		InsertNewStars(millies);
	}

	glDisable(GL_BLEND);

	I_FinishFrame();
	I_StartFrame();

	return (millies >= max_time);  // finished
}


void E_SplashScreen(void)
{
	srand(time(NULL));

	color = rand() & 7;

	CreateStarTex();
	CreateLogoTex();

	int start_millies = I_GetMillies();

	for (;;)
	{
		int millies = I_GetMillies() - start_millies;

		if (E_DrawSplash(millies))
			break;
	}

	glDeleteTextures(1, &star_tex);
	glDeleteTextures(1, &logo_tex);
}


//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
