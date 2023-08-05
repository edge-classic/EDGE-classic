//----------------------------------------------------------------------------
//  EDGE OpenGL Rendering (Skies)
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

#include <math.h>

#include "image_data.h"

#include "dm_state.h"
#include "m_math.h"
#include "r_misc.h"
#include "w_flat.h"
#include "r_sky.h"
#include "r_gldefs.h"
#include "r_sky.h"
#include "r_units.h"
#include "r_colormap.h"
#include "r_modes.h"
#include "r_image.h"
#include "r_texgl.h"
#include "w_wad.h"

const image_c *sky_image;

bool custom_sky_box;

// needed for SKY
extern epi::image_data_c *ReadAsEpiBlock(image_c *rim);

extern cvar_c r_culling;

static GLfloat sky_cap_color[4];

typedef struct sec_sky_ring_s
{
	// which group of connected skies (0 if none)
	int group;

	// link of sector in RING
	struct sec_sky_ring_s *next;
	struct sec_sky_ring_s *prev;

	// maximal sky height of group
	float max_h;
}
sec_sky_ring_t;

//
// R_ComputeSkyHeights
// 
// This routine computes the sky height field in sector_t, which is
// the maximal sky height over all sky sectors (ceiling only) which
// are joined by 2S linedefs.
//
// Algorithm: Initially all sky sectors are in individual groups.  Now
// we scan the linedef list.  For each 2-sectored line with sky on
// both sides, merge the two groups into one.  Simple :).  We can
// compute the maximal height of the group as we go.
// 
void R_ComputeSkyHeights(void)
{
	int i;
	line_t *ld;
	sector_t *sec;

	// --- initialise ---

	sec_sky_ring_t *rings = new sec_sky_ring_t[numsectors];

	memset(rings, 0, numsectors * sizeof(sec_sky_ring_t));

	for (i=0, sec=sectors; i < numsectors; i++, sec++)
	{
		if (! IS_SKY(sec->ceil))
			continue;

		rings[i].group = (i + 1);
		rings[i].next = rings[i].prev = rings + i;
		rings[i].max_h = sec->c_h;

		// leave some room for tall sprites 
		static const float SPR_H_MAX = 256.0f;

		if (sec->c_h < 30000.0f && (sec->c_h > sec->f_h) &&
			(sec->c_h < sec->f_h + SPR_H_MAX))
		{
			rings[i].max_h = sec->f_h + SPR_H_MAX;
		}
	}

	// --- make the pass over linedefs ---

	for (i=0, ld=lines; i < numlines; i++, ld++)
	{
		sector_t *sec1, *sec2;
		sec_sky_ring_t *ring1, *ring2, *tmp_R;

		if (! ld->side[0] || ! ld->side[1])
			continue;

		sec1 = ld->frontsector;
		sec2 = ld->backsector;

		SYS_ASSERT(sec1 && sec2);

		if (sec1 == sec2)
			continue;

		ring1 = rings + (sec1 - sectors);
		ring2 = rings + (sec2 - sectors);

		// we require sky on both sides
		if (ring1->group == 0 || ring2->group == 0)
			continue;

		// already in the same group ?
		if (ring1->group == ring2->group)
			continue;

		// swap sectors to ensure the lower group is added to the higher
		// group, since we don't need to update the `max_h' fields of the
		// highest group.

		if (ring1->max_h < ring2->max_h)
		{
			tmp_R = ring1;  ring1 = ring2;  ring2 = tmp_R;
		}

		// update the group numbers in the second group

		ring2->group = ring1->group;
		ring2->max_h = ring1->max_h;

		for (tmp_R=ring2->next; tmp_R != ring2; tmp_R=tmp_R->next)
		{
			tmp_R->group = ring1->group;
			tmp_R->max_h = ring1->max_h;
		}

		// merge 'em baby...

		ring1->next->prev = ring2;
		ring2->next->prev = ring1;

		tmp_R = ring1->next; 
		ring1->next = ring2->next;
		ring2->next = tmp_R;
	}

	// --- now store the results, and free up ---

	for (i=0, sec=sectors; i < numsectors; i++, sec++)
	{
		if (rings[i].group > 0)
			sec->sky_h = rings[i].max_h;

#if 0   // DEBUG CODE
		L_WriteDebug("SKY: sec %d  group %d  max_h %1.1f\n", i,
				rings[i].group, rings[i].max_h);
#endif
	}

	delete[] rings;
}

//----------------------------------------------------------------------------


bool need_to_draw_sky = false;


typedef struct
{
	const image_c *base_sky;
	
	const colourmap_c *fx_colmap;

	int face_size;

	GLuint tex[6];

	// face images are only present for custom skyboxes.
	// pseudo skyboxes are generated outside of the image system.
	const image_c *face[6];
}
fake_skybox_t;

static fake_skybox_t fake_box[2] =
{
	{
		NULL, NULL, 1,
		{ 0,0,0,0,0,0 },
		{ NULL, NULL, NULL, NULL, NULL, NULL }
	},
	{
		NULL, NULL, 1,
		{ 0,0,0,0,0,0 },
		{ NULL, NULL, NULL, NULL, NULL, NULL }
	}
};


static void DeleteSkyTexGroup(int SK)
{
	for (int i = 0; i < 6; i++)
	{
		if (fake_box[SK].tex[i] != 0)
		{
			glDeleteTextures(1, &fake_box[SK].tex[i]);
			fake_box[SK].tex[i] = 0;
		}
	}
}

void DeleteSkyTextures(void)
{
	for (int SK = 0; SK < 2; SK++)
	{
		fake_box[SK].base_sky  = NULL;
		fake_box[SK].fx_colmap = NULL;

		DeleteSkyTexGroup(SK);
	}
}


static void RGL_SetupSkyMatrices(void)
{
	if (custom_sky_box)
	{
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();

		glLoadIdentity();
		glFrustum(-view_x_slope * r_nearclip.f, view_x_slope * r_nearclip.f,
				-view_y_slope * r_nearclip.f, view_y_slope * r_nearclip.f,
				r_nearclip.f, r_farclip.f);

		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();

		glLoadIdentity();
		
		glRotatef(270.0f - ANG_2_FLOAT(viewvertangle), 1.0f, 0.0f, 0.0f);
		glRotatef(90.0f  - ANG_2_FLOAT(viewangle), 0.0f, 0.0f, 1.0f);
	}
	else
	{
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();

		glLoadIdentity();
		glFrustum(-view_x_slope * r_nearclip.f, view_x_slope * r_nearclip.f,
				-view_y_slope * r_nearclip.f, view_y_slope * r_nearclip.f,
				r_nearclip.f, r_farclip.f * 2.0);

		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();

		glRotatef(270.0f - ANG_2_FLOAT(viewvertangle), 1.0f, 0.0f, 0.0f);
		glRotatef(90.0f  - ANG_2_FLOAT(viewangle), 0.0f, 0.0f, 1.0f);
		glTranslatef(0.0f, 0.0f, -(r_farclip.f / 2 * 0.15)); // Draw center below horizon a little
	}
}


/*static void RGL_SetupSkyMatrices2D(void)
{
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();

	glLoadIdentity();
	glOrtho(0.0f, (float)SCREENWIDTH, 
			0.0f, (float)SCREENHEIGHT, -1.0f, 1.0f);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	glLoadIdentity();
}*/


static void RGL_RevertSkyMatrices(void)
{
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
}


void RGL_BeginSky(void)
{
	need_to_draw_sky = false;

	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glDisable(GL_TEXTURE_2D);

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	// Draw the entire sky using only one glBegin/glEnd clause.
	// glEnd is called in RGL_FinishSky and this code assumes that only
	// RGL_DrawSkyWall and RGL_DrawSkyPlane is doing OpenGL calls in between.
	glBegin(GL_TRIANGLES);
}

static vec2_t sky_circle_[32];

static void buildSkyCircle()
{
	double rot = 0;
	for (auto& pos : sky_circle_)
	{
		pos.Set(sin(rot), -cos(rot));
		rot -= (3.1415926535897932384626433832795 * 2) / 32.0;
	}
}

// -----------------------------------------------------------------------------
// Renders a cylindrical 'slice' of the sky between [top] and [bottom] on the z
// axis
// -----------------------------------------------------------------------------
static void renderSkySlice(float top, float bottom, float atop, float abottom, float dist, float tx, float ty)
{
	float tc_x  = 0.0f;
	float tc_y1 = (top + 1.0f) * (ty * 0.5f);
	float tc_y2 = (bottom + 1.0f) * (ty * 0.5f);

	if (bottom < -0.5f)
	{
		tc_y1 = -tc_y1;
		tc_y2 = -tc_y2;
	}

	glBegin(GL_QUADS);

	// Go through circular points
	for (unsigned a = 0; a < 31; a++)
	{
		// Top
		glColor4f(1.0f, 1.0f, 1.0f, atop);
		glTexCoord2f(tc_x + tx, tc_y1);
		glVertex3f(
			(sky_circle_[a + 1].x * dist),
			- (sky_circle_[a + 1].y * dist),
			(top * dist));
		glTexCoord2f(tc_x, tc_y1);
		glVertex3f(
			(sky_circle_[a].x * dist),
			- (sky_circle_[a].y * dist),
			(top * dist));

		// Bottom
		glColor4f(1.0f, 1.0f, 1.0f, abottom);
		glTexCoord2f(tc_x, tc_y2);
		glVertex3f(
			(sky_circle_[a].x * dist),
			- (sky_circle_[a].y * dist),
			(bottom * dist));
		glTexCoord2f(tc_x + tx, tc_y2);
		glVertex3f(
			(sky_circle_[a + 1].x * dist),
			- (sky_circle_[a + 1].y * dist),
			(bottom * dist));

		tc_x += tx;
	}

	// Link last point -> first
	// Top
	glColor4f(1.0f, 1.0f, 1.0f, atop);
	glTexCoord2f(tc_x + tx, tc_y1);
	glVertex3f(
		(sky_circle_[0].x * dist),
		- (sky_circle_[0].y * dist),
		(top * dist));
	glTexCoord2f(tc_x, tc_y1);
	glVertex3f(
		(sky_circle_[31].x * dist),
		- (sky_circle_[31].y * dist),
		(top * dist));

	// Bottom
	glColor4f(1.0f, 1.0f, 1.0f, abottom);
	glTexCoord2f(tc_x, tc_y2);
	glVertex3f(
		(sky_circle_[31].x * dist),
		- (sky_circle_[31].y * dist),
		(bottom * dist));
	glTexCoord2f(tc_x + tx, tc_y2);
	glVertex3f(
		(sky_circle_[0].x * dist),
		- (sky_circle_[0].y * dist),
		(bottom * dist));

	glEnd();
}

static void RGL_DrawSkyCylinder(void)
{
	GLuint sky = W_ImageCache(sky_image, false, ren_fx_colmap);

	// Center skybox a bit below the camera view
	RGL_SetupSkyMatrices();

	glDisable(GL_TEXTURE_2D);

	// Render top cap
	float dist = r_farclip.f / 2.0;
	float cap_dist = dist * 1.5; // Ensure the caps extend beyond the cylindrical projection
	glColor4f(sky_cap_color[0],sky_cap_color[1],sky_cap_color[2],1.0);
	glBegin(GL_QUADS);
	glVertex3f(-cap_dist, -cap_dist, dist);
	glVertex3f(-cap_dist, cap_dist, dist);
	glVertex3f(cap_dist, cap_dist, dist);
	glVertex3f(cap_dist, -cap_dist, dist);
	glEnd();

	// Render bottom cap
	glBegin(GL_QUADS);
	glVertex3f(-cap_dist, -cap_dist, -dist);
	glVertex3f(-cap_dist, cap_dist, -dist);
	glVertex3f(cap_dist, cap_dist, -dist);
	glVertex3f(cap_dist, -cap_dist, -dist);
	glEnd();

	// Render skybox sides
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, sky);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	// Check for odd sky sizes
	float tx = 0.125f;
	float ty = 2.0f;
	if (IM_WIDTH(sky_image) > 256)
		tx = 0.125f / ((float)IM_WIDTH(sky_image) / 256.0f);
	if (IM_HEIGHT(sky_image) > 128)
		ty = (float)IM_HEIGHT(sky_image) / 128;

	glEnable(GL_ALPHA_TEST);
	glEnable(GL_BLEND);

	renderSkySlice(1.0f, 0.75f, 0.0f, 1.0f, dist, tx, ty);   // Top Fade
	renderSkySlice(0.75f, 0.0f, 1.0f, 1.0f, dist, tx, ty);  // Top Solid
	renderSkySlice(0.0f, -0.75f, 1.0f, 1.0f, dist, tx, ty);  // Bottom Solid (Mirror)
	renderSkySlice(-0.75f, -1.0f, 1.0f, 0.0f, dist, tx, ty); // Bottom Fade (Mirror)

	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);

	RGL_RevertSkyMatrices();
}

static void RGL_DrawSkyBox(void)
{
	float dist = r_farclip.f / 2.0f;

	int SK = RGL_UpdateSkyBoxTextures();

	SYS_ASSERT(SK >= 0);

	RGL_SetupSkyMatrices();

	float v0 = 0.0f;
	float v1 = 1.0f;

	if (r_dumbclamp.d)
	{
		float size = fake_box[SK].face_size;

		v0 = 0.5f / size;
		v1 = 1.0f - v0;
	}

	glEnable(GL_TEXTURE_2D);

	float col[4];

	col[0] = LT_RED(255);
	col[1] = LT_GRN(255);
	col[2] = LT_BLU(255);
	col[3] = 1.0f;

	if (r_colormaterial.d || ! r_colorlighting.d)
		glColor4fv(col);
	else
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, col);

	// top
	glBindTexture(GL_TEXTURE_2D, fake_box[SK].tex[WSKY_Top]);
        glNormal3i(0, 0, -1);
	#ifdef APPLE_SILICON
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	#endif
	glBegin(GL_QUADS);
	glTexCoord2f(v0, v0); glVertex3f(-dist,  dist, +dist);
	glTexCoord2f(v0, v1); glVertex3f(-dist, -dist, +dist);
	glTexCoord2f(v1, v1); glVertex3f( dist, -dist, +dist);
	glTexCoord2f(v1, v0); glVertex3f( dist,  dist, +dist);
	glEnd();

	// bottom
	glBindTexture(GL_TEXTURE_2D, fake_box[SK].tex[WSKY_Bottom]);
        glNormal3i(0, 0, +1);
	#ifdef APPLE_SILICON
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	#endif
	glBegin(GL_QUADS);
	glTexCoord2f(v0, v0); glVertex3f(-dist, -dist, -dist);
	glTexCoord2f(v0, v1); glVertex3f(-dist,  dist, -dist);
	glTexCoord2f(v1, v1); glVertex3f( dist,  dist, -dist);
	glTexCoord2f(v1, v0); glVertex3f( dist, -dist, -dist);
	glEnd();

	// north
	glBindTexture(GL_TEXTURE_2D, fake_box[SK].tex[WSKY_North]);
        glNormal3i(0, -1, 0);
	#ifdef APPLE_SILICON
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	#endif
	glBegin(GL_QUADS);
	glTexCoord2f(v0, v0); glVertex3f(-dist,  dist, -dist);
	glTexCoord2f(v0, v1); glVertex3f(-dist,  dist, +dist);
	glTexCoord2f(v1, v1); glVertex3f( dist,  dist, +dist);
	glTexCoord2f(v1, v0); glVertex3f( dist,  dist, -dist);
	glEnd();

	// east
	glBindTexture(GL_TEXTURE_2D, fake_box[SK].tex[WSKY_East]);
        glNormal3i(-1, 0, 0);
	#ifdef APPLE_SILICON
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	#endif
	glBegin(GL_QUADS);
	glTexCoord2f(v0, v0); glVertex3f( dist,  dist, -dist);
	glTexCoord2f(v0, v1); glVertex3f( dist,  dist, +dist);
	glTexCoord2f(v1, v1); glVertex3f( dist, -dist, +dist);
	glTexCoord2f(v1, v0); glVertex3f( dist, -dist, -dist);
	glEnd();

	// south
	glBindTexture(GL_TEXTURE_2D, fake_box[SK].tex[WSKY_South]);
        glNormal3i(0, +1, 0);
	#ifdef APPLE_SILICON
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	#endif
	glBegin(GL_QUADS);
	glTexCoord2f(v0, v0); glVertex3f( dist, -dist, -dist);
	glTexCoord2f(v0, v1); glVertex3f( dist, -dist, +dist);
	glTexCoord2f(v1, v1); glVertex3f(-dist, -dist, +dist);
	glTexCoord2f(v1, v0); glVertex3f(-dist, -dist, -dist);
	glEnd();

	// west
	glBindTexture(GL_TEXTURE_2D, fake_box[SK].tex[WSKY_West]);
        glNormal3i(+1, 0, 0);
	#ifdef APPLE_SILICON
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	#endif
	glBegin(GL_QUADS);
	glTexCoord2f(v0, v0); glVertex3f(-dist, -dist, -dist);
	glTexCoord2f(v0, v1); glVertex3f(-dist, -dist, +dist);
	glTexCoord2f(v1, v1); glVertex3f(-dist,  dist, +dist);
	glTexCoord2f(v1, v0); glVertex3f(-dist,  dist, -dist);
	glEnd();

	glDisable(GL_TEXTURE_2D);

	RGL_RevertSkyMatrices();
}


/*void RGL_DrawSkyOriginal(void)
{
	RGL_SetupSkyMatrices2D();

	float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

	if (r_colormaterial.d || ! r_colorlighting.d)
		glColor4fv(white);
	else
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, white);

	GLuint tex_id = W_ImageCache(sky_image, false, ren_fx_colmap);

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, tex_id);
 	#ifdef APPLE_SILICON
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	#endif

	glBegin(GL_QUAD_STRIP);

	// FIXME for widescreen
	float FIELDOFVIEW = CLAMP(5, r_fov.f, 175);

	float focal_len = tan(FIELDOFVIEW * M_PI / 360.0);
	float centerxfrac = SCREENWIDTH / 2.0f;

	float ty1 = 200.0f / 128.0f;
	float ty2 = 0;

	for (int i = 0; i <= 32; i++)
	{
		int sx = i * SCREENWIDTH / 32;

		// use formula from original Doom code
		angle_t ang = ANG180 + M_ATan((1.0f - sx / centerxfrac) * focal_len);

		// some mucking about here to prevent wrap-around
		float tx = (((viewangle >> 2) + (ang >> 2) + (ANG180 >> 2)) >> 20);

		if ((IM_WIDTH(sky_image) / IM_HEIGHT(sky_image)) < 2.28f)
			tx = tx / 256.0f;
		else
			tx = tx / 1024.0f;

#if 0  // DEBUGGING
I_Printf("[%i] --> %1.2f  tx %1.4f\n", i, ANG_2_FLOAT(ang), tx);
#endif
		glTexCoord2f(tx, 1.0f - ty1);
		glVertex2i(sx, 0);

		glTexCoord2f(tx, 1.0f - ty2); 
		glVertex2i(sx, SCREENHEIGHT);
 	}

	glEnd();

	glDisable(GL_TEXTURE_2D);

	RGL_RevertSkyMatrices();
}*/

void RGL_FinishSky(void)
{
	glEnd(); // End glBegin(GL_TRIANGLES) from RGL_BeginSky

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	if (! need_to_draw_sky)
		return;

	// draw sky picture, but DON'T affect the depth buffering

	glDepthMask(GL_FALSE);

	if (r_culling.d)
		glDisable(GL_DEPTH_TEST);

	if (! r_dumbsky.d)
		glDepthFunc(GL_GREATER);

	if (custom_sky_box)
		RGL_DrawSkyBox();
	else
		RGL_DrawSkyCylinder();

	if (r_culling.d)
		glEnable(GL_DEPTH_TEST);

	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_TRUE);

	glDisable(GL_TEXTURE_2D);

#if 0
	// clear buffer (EXPERIMENTAL) -- causes render problems: ceilings
	// you shouldn't be able to see (MAP05, MAP12).
	glClear(GL_DEPTH_BUFFER_BIT);
#endif
}

void RGL_DrawSkyPlane(subsector_t *sub, float h)
{
	need_to_draw_sky = true;

	if (r_dumbsky.d)
		return;

	MIR_Height(h);

	glNormal3f(0, 0, (viewz > h) ? 1.0f : -1.0f);

	seg_t *seg = sub->segs;
	if (!seg)
		return;

	float x0 = seg->v1->x;
	float y0 = seg->v1->y;
	MIR_Coordinate(x0, y0);
	seg = seg->sub_next;
	if (!seg)
		return;

	float x1 = seg->v1->x;
	float y1 = seg->v1->y;
	MIR_Coordinate(x1, y1);
	seg = seg->sub_next;
	if (!seg)
		return;

	while(seg)
	{
		float x2 = seg->v1->x;
		float y2 = seg->v1->y;
		MIR_Coordinate(x2, y2);

		glVertex3f(x0, y0, h);
		glVertex3f(x1, y1, h);
		glVertex3f(x2, y2, h);

		x1 = x2;
		y1 = y2;
		seg = seg->sub_next;
	}
}


void RGL_DrawSkyWall(seg_t *seg, float h1, float h2)
{
	need_to_draw_sky = true;

	if (r_dumbsky.d)
		return;

	float x1 = seg->v1->x;
	float y1 = seg->v1->y;
	float x2 = seg->v2->x;
	float y2 = seg->v2->y;

	MIR_Coordinate(x1, y1);
	MIR_Coordinate(x2, y2);

	MIR_Height(h1);
	MIR_Height(h2);

	glNormal3f(y2 - y1, x1 - x2, 0);

	glVertex3f(x1, y1, h1);
	glVertex3f(x1, y1, h2);
	glVertex3f(x2, y2, h2);

	glVertex3f(x2, y2, h1);
	glVertex3f(x2, y2, h2);
	glVertex3f(x1, y1, h1);
}


//----------------------------------------------------------------------------


#define PIXEL_RED(pix)  (what_palette[pix*3 + 0])
#define PIXEL_GRN(pix)  (what_palette[pix*3 + 1])
#define PIXEL_BLU(pix)  (what_palette[pix*3 + 2])


static bool SkyIsNarrow(const image_c *sky)
{
	// check the aspect of the image
	return (IM_WIDTH(sky) / IM_HEIGHT(sky)) < 2.28f;
}


static void CalcSkyCoord(int px, int py, int pw, int ph, int face,
		bool narrow, float *tx, float *ty)
{
	// the 0.5 here ensures we never hit exactly zero
	float ax = ((float)px + 0.5f) / (float)pw * 2.0f - 1.0f;
	float ay = ((float)py + 0.5f) / (float)ph * 2.0f - 1.0f;

	float sx, sy, sz;

	switch (face)
	{
		case WSKY_North:
			sx = ax; sy = 1.0f; sz = ay; break;

		case WSKY_South:
			sx = -ax; sy = -1.0f; sz = ay; break;

		case WSKY_East:
			sx = 1.0f; sy = -ax; sz = ay; break;

		case WSKY_West:
			sx = -1.0f; sy = ax; sz = ay; break;

		case WSKY_Top:
			sx = ax; sy = -ay; sz = 1.0f; break;

		case WSKY_Bottom:
			sx = ax; sy = ay; sz = -1.0f; break;

		default:
			I_Error("CalcSkyCoord: INTERNAL ERROR (lost face)\n");
			sx = sy = sz = 0;
			break; /* NOT REACHED */
	}

	float len2 = sqrt((sx) * (sx) + (sy) * (sy));

	angle_t H = ANG0  + R_PointToAngle(0, 0, sx, sy);
	angle_t V = ANG90 - R_PointToAngle(0, 0, len2, sz);

	if (narrow)
		*tx = (float)(H >> 1) / (float)(1 << 30);
	else
		*tx = (float)(H >> 2) / (float)(1 << 30);

	// want yy to range from 0.0 (top) to 2.0 (bottom)
	float yy = (float)(V) / (float)ANG90;

	// this lowers the effective centre of the pseudo skybox to
	// match the DOOM sky, which is 128 pixels on a 200 pixel high
	// screen (so it dips 28 pixels below the horizon).
	yy = yy / 1.15f;

	// mirror it (vertically)
	if (yy > 1.0f) yy = 2.0f - yy;

	*ty = 1.0f - pow(yy, 2.2);
}


static void BlurCentre(epi::image_data_c& img)
{
	// Blurs the center of the image (the top face of the
	// pseudo sky box).  The amount of blur is different at
	// different places: from none at all at the edges upto
	// maximum blur in the middle.

	SYS_ASSERT(img.bpp == 3);

	// create a copy to work from (as we cannot blur in-place)
	epi::image_data_c orig(img.width, img.height, 3);

	memcpy(orig.pixels, img.pixels, orig.width*orig.height*3);

#if (_MSC_VER > 1929)
		I_Warning("Using BlurCentre() workaround for MSVC2022! Revert this as soon as it is fixed upstream!\n");
#endif

	for (int y = 1+img.height/4; y < img.height*3/4; y++)
	for (int x = 1+img.width /4; x < img.width *3/4; x++)
	{
		int x_pos = 31 - ABS(x - img.width /2) * 127 / img.width;
		int y_pos = 31 - ABS(y - img.height/2) * 127 / img.height;

		// SYS_ASSERT(0 <= x_pos && x_pos <= 31);
		// SYS_ASSERT(0 <= y_pos && y_pos <= 31);

		int min_pos = MIN(x_pos, y_pos);

		int size = 16 + min_pos*2;  // range: 1.00 to 4.99 (times 16)

#if (_MSC_VER > 1929)
		static int d_size;
		d_size = (size | 15) / 16;
#else
		int d_size = (size | 15) / 16;
#endif

		// compute average over the box
		int r = 0;
		int g = 0;
		int b = 0;
		int total = 0;

		for (int dy = -d_size; dy <= +d_size; dy++)
		for (int dx = -d_size; dx <= +d_size; dx++)
		{
			u8_t *src = orig.PixelAt(x+dx, y+dy);

			int qty = ( (ABS(dx) < d_size) ? 16 : (size & 15) ) *
			          ( (ABS(dy) < d_size) ? 16 : (size & 15) );

			total += qty;

			r += src[0] * qty;
			g += src[1] * qty;
			b += src[2] * qty;
		}

		SYS_ASSERT(total > 0);

		u8_t *dest = img.PixelAt(x, y);

		dest[0] = r / total;
		dest[1] = g / total;
		dest[2] = b / total;
	}
}

static GLuint BuildFace(const epi::image_data_c *sky, int face,
				  	    fake_skybox_t *info, const byte *what_palette)
			
{
	int img_size = info->face_size;

	epi::image_data_c img(img_size, img_size, 3);


	bool narrow = SkyIsNarrow(info->base_sky);

	const byte *src = sky->pixels;

	int sky_w = sky->width;
	int sky_h = sky->height;

	for (int y=0; y < img_size; y++)
	{
		u8_t *dest = img.PixelAt(0, y);

		for (int x=0; x < img_size; x++, dest += 3)
		{
			float tx, ty;

			CalcSkyCoord(x, y, img_size, img_size, face, narrow, &tx, &ty);

			// Bilinear Filtering

			int TX = (int)(tx * sky_w * 16);
			int TY = (int)(ty * sky_h * 16);

			// negative values shouldn't occur, but just in case...
			TX = (TX + sky_w * 64) % (sky_w * 16);
			TY = (TY + sky_h * 64) % (sky_h * 16);

			// SYS_ASSERT(TX >= 0 && TY >= 0);

			int FX = TX % 16; TX >>= 4;
			int FY = TY % 16; TY >>= 4;

			// SYS_ASSERT(TX < sky_w && TY < sky_h);


			int TX2 = (TX + 1) % sky_w;
			int TY2 = (TY < sky_h-1) ? (TY+1) : TY;

			byte rA, rB, rC, rD;
			byte gA, gB, gC, gD;
			byte bA, bB, bC, bD;

			switch (sky->bpp)
			{
				case 1:
				{
					byte src_A = src[TY  * sky_w + TX];
					byte src_B = src[TY  * sky_w + TX2];
					byte src_C = src[TY2 * sky_w + TX];
					byte src_D = src[TY2 * sky_w + TX2];

					rA = PIXEL_RED(src_A); rB = PIXEL_RED(src_B);
					rC = PIXEL_RED(src_C); rD = PIXEL_RED(src_D);

					gA = PIXEL_GRN(src_A); gB = PIXEL_GRN(src_B);
					gC = PIXEL_GRN(src_C); gD = PIXEL_GRN(src_D);

					bA = PIXEL_BLU(src_A); bB = PIXEL_BLU(src_B);
					bC = PIXEL_BLU(src_C); bD = PIXEL_BLU(src_D);
				}
				break;

				case 3:
				{
					rA = src[(TY * sky_w + TX) * 3 + 0];
					gA = src[(TY * sky_w + TX) * 3 + 1];
					bA = src[(TY * sky_w + TX) * 3 + 2];

					rB = src[(TY * sky_w + TX2) * 3 + 0];
					gB = src[(TY * sky_w + TX2) * 3 + 1];
					bB = src[(TY * sky_w + TX2) * 3 + 2];

					rC = src[(TY2 * sky_w + TX) * 3 + 0];
					gC = src[(TY2 * sky_w + TX) * 3 + 1];
					bC = src[(TY2 * sky_w + TX) * 3 + 2];

					rD = src[(TY2 * sky_w + TX2) * 3 + 0];
					gD = src[(TY2 * sky_w + TX2) * 3 + 1];
					bD = src[(TY2 * sky_w + TX2) * 3 + 2];
				}
				break;

				case 4:
				{
					rA = src[(TY * sky_w + TX) * 4 + 0];
					gA = src[(TY * sky_w + TX) * 4 + 1];
					bA = src[(TY * sky_w + TX) * 4 + 2];

					rB = src[(TY * sky_w + TX2) * 4 + 0];
					gB = src[(TY * sky_w + TX2) * 4 + 1];
					bB = src[(TY * sky_w + TX2) * 4 + 2];

					rC = src[(TY2 * sky_w + TX) * 4 + 0];
					gC = src[(TY2 * sky_w + TX) * 4 + 1];
					bC = src[(TY2 * sky_w + TX) * 4 + 2];

					rD = src[(TY2 * sky_w + TX2) * 4 + 0];
					gD = src[(TY2 * sky_w + TX2) * 4 + 1];
					bD = src[(TY2 * sky_w + TX2) * 4 + 2];
				}
				break;

				default:  // remove compiler warning
					rA = rB = rC = rD = 0;
					gA = gB = gC = gD = 0;
					bA = bB = bC = bD = 0;
					break;
			}

			int r = (int)rA * (FX^15) * (FY^15) +
					(int)rB * (FX   ) * (FY^15) +
					(int)rC * (FX^15) * (FY   ) +
					(int)rD * (FX   ) * (FY   );

			int g = (int)gA * (FX^15) * (FY^15) +
					(int)gB * (FX   ) * (FY^15) +
					(int)gC * (FX^15) * (FY   ) +
					(int)gD * (FX   ) * (FY   );

			int b = (int)bA * (FX^15) * (FY^15) +
					(int)bB * (FX   ) * (FY^15) +
					(int)bC * (FX^15) * (FY   ) +
					(int)bD * (FX   ) * (FY   );

			dest[0] = r / 225;
			dest[1] = g / 225;
			dest[2] = b / 225;
		}
	}

	// make the top surface look less bad
	if (face == WSKY_Top)
	{
		BlurCentre(img);
	}

	return R_UploadTexture(&img, UPL_Smooth|UPL_Clamp);
}


static const char *UserSkyFaceName(const char *base, int face)
{
	static char buffer[64];
	static const char letters[] = "NESWTB";

	sprintf(buffer, "%s_%c", base, letters[face]);
	return buffer;
}


int RGL_UpdateSkyBoxTextures(void)
{
	int SK = ren_fx_colmap ? 1 : 0;

	fake_skybox_t *info = &fake_box[SK];

	if (info->base_sky  == sky_image &&
		info->fx_colmap == ren_fx_colmap)
	{
		return SK;
	}

	info->base_sky  = sky_image;
	info->fx_colmap = ren_fx_colmap;


	// check for custom sky boxes
	info->face[WSKY_North] = W_ImageLookup(
			UserSkyFaceName(sky_image->name.c_str(), WSKY_North), INS_Texture, ILF_Null);

	//LOBO 2022:
	//If we do nothing, our EWAD skybox will be used for all maps.
	//So we need to disable it if we have a pwad that contains it's
	//own sky.
	if (W_LoboDisableSkybox(sky_image->name.c_str()))
	{
		info->face[WSKY_North] = NULL;
		//I_Printf("Skybox turned OFF\n");
	}
	
	// Set colors for culling fog and faux skybox caps - Dasho
	const byte *what_palette = (const byte *) &playpal_data[0];
	if (sky_image->source_palette >= 0)
		what_palette = (const byte *) W_LoadLump(sky_image->source_palette);
	epi::image_data_c *tmp_img_data = R_PalettisedToRGB(ReadAsEpiBlock((image_c *)sky_image), what_palette, sky_image->opacity);
	u8_t *temp_rgb = new u8_t[3];
	tmp_img_data->AverageColor(temp_rgb, 0, sky_image->actual_w, 0, sky_image->actual_h/2);
	cull_fog_color[0] = (float)temp_rgb[0] / 255.0f;
	cull_fog_color[1] = (float)temp_rgb[1] / 255.0f;
	cull_fog_color[2] = (float)temp_rgb[2] / 255.0f;
	tmp_img_data->AverageColor(temp_rgb, 0, sky_image->actual_w, sky_image->actual_h * 3/4, sky_image->actual_h);
	sky_cap_color[0] = (float)temp_rgb[0] / 255.0f;
	sky_cap_color[1] = (float)temp_rgb[1] / 255.0f;
	sky_cap_color[2] = (float)temp_rgb[2] / 255.0f;
	delete tmp_img_data;
	delete[] temp_rgb;

	if (info->face[WSKY_North])
	{
		custom_sky_box = true;

		info->face_size = info->face[WSKY_North]->total_w;

		for (int i = WSKY_East; i < 6; i++)
			info->face[i] = W_ImageLookup(
					UserSkyFaceName(sky_image->name.c_str(), i), INS_Texture);

		for (int k = 0; k < 6; k++)
			info->tex[k] = W_ImageCache(info->face[k], false, ren_fx_colmap);

		return SK;
	}
	else
	{
		info->face_size = 256;
		custom_sky_box = false;
		return -1;
	}

	// Create pseudo sky box

	/*info->face_size = 256;

	custom_sky_box = false;

	
	// Intentional Const Override
	const epi::image_data_c *block = ReadAsEpiBlock((image_c*)sky_image);
	SYS_ASSERT(block);

	// get correct palette
	const byte *what_pal = (const byte *) &playpal_data[0];
	bool what_pal_cached = false;

	static byte trans_pal[256*3];

	if (ren_fx_colmap)
	{
		R_TranslatePalette(trans_pal, what_pal, ren_fx_colmap);
		what_pal = trans_pal;
	}
	else if (sky_image->source_palette >= 0)
	{
		what_pal = (const byte *) W_LoadLump(sky_image->source_palette);
		what_pal_cached = true;
	}

	//DeleteSkyTexGroup(SK); // Commented out to see if this fixes the skybox switching issue without bugs - Dasho

	info->tex[WSKY_North]  = BuildFace(block, WSKY_North,  info, what_pal);
	info->tex[WSKY_East]   = BuildFace(block, WSKY_East,   info, what_pal);
	info->tex[WSKY_Top]    = BuildFace(block, WSKY_Top,    info, what_pal);
	info->tex[WSKY_Bottom] = BuildFace(block, WSKY_Bottom, info, what_pal);

	// optimisation: can share side textures when narrow

	info->tex[WSKY_South] = SkyIsNarrow(sky_image) ? info->tex[WSKY_North] :
						 BuildFace(block, WSKY_South, info, what_pal );

	info->tex[WSKY_West]  = SkyIsNarrow(sky_image) ? info->tex[WSKY_East] :
						 BuildFace(block, WSKY_West, info, what_pal );

	delete block;

	if (what_pal_cached)
		delete[] what_pal;

	return SK;*/
}


void RGL_PreCacheSky(void)
{
	buildSkyCircle();
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
