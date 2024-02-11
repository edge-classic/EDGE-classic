//----------------------------------------------------------------------------
//  EDGE OpenGL Rendering (Skies)
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

#include "i_defs.h"
#include "i_defs_gl.h"

#include <math.h>

#include "image_data.h"

#include "dm_state.h"
#include "g_game.h" // currmap
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
extern image_data_c *ReadAsEpiBlock(image_c *rim);

extern cvar_c r_culling;

static sg_color sky_cap_color;

static skystretch_e current_sky_stretch = SKS_Unset;

DEF_CVAR_CLAMPED(r_skystretch, "0", CVAR_ARCHIVE, 0, 3);

typedef struct sec_sky_ring_s
{
    // which group of connected skies (0 if none)
    int group;

    // link of sector in RING
    struct sec_sky_ring_s *next;
    struct sec_sky_ring_s *prev;

    // maximal sky height of group
    float max_h;
} sec_sky_ring_t;

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
    int       i;
    line_t   *ld;
    sector_t *sec;

    // --- initialise ---

    sec_sky_ring_t *rings = new sec_sky_ring_t[numsectors];

    memset(rings, 0, numsectors * sizeof(sec_sky_ring_t));

    for (i = 0, sec = sectors; i < numsectors; i++, sec++)
    {
        if (!IS_SKY(sec->ceil))
            continue;

        rings[i].group = (i + 1);
        rings[i].next = rings[i].prev = rings + i;
        rings[i].max_h                = sec->c_h;

        // leave some room for tall sprites
        static const float SPR_H_MAX = 256.0f;

        if (sec->c_h < 30000.0f && (sec->c_h > sec->f_h) && (sec->c_h < sec->f_h + SPR_H_MAX))
        {
            rings[i].max_h = sec->f_h + SPR_H_MAX;
        }
    }

    // --- make the pass over linedefs ---

    for (i = 0, ld = lines; i < numlines; i++, ld++)
    {
        sector_t       *sec1, *sec2;
        sec_sky_ring_t *ring1, *ring2, *tmp_R;

        if (!ld->side[0] || !ld->side[1])
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
            tmp_R = ring1;
            ring1 = ring2;
            ring2 = tmp_R;
        }

        // update the group numbers in the second group

        ring2->group = ring1->group;
        ring2->max_h = ring1->max_h;

        for (tmp_R = ring2->next; tmp_R != ring2; tmp_R = tmp_R->next)
        {
            tmp_R->group = ring1->group;
            tmp_R->max_h = ring1->max_h;
        }

        // merge 'em baby...

        ring1->next->prev = ring2;
        ring2->next->prev = ring1;

        tmp_R       = ring1->next;
        ring1->next = ring2->next;
        ring2->next = tmp_R;
    }

    // --- now store the results, and free up ---

    for (i = 0, sec = sectors; i < numsectors; i++, sec++)
    {
        if (rings[i].group > 0)
            sec->sky_h = rings[i].max_h;

#if 0 // DEBUG CODE
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
} fake_skybox_t;

static fake_skybox_t fake_box[2] = {{nullptr, nullptr, 1, {0, 0, 0, 0, 0, 0}, {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr}},
                                    {nullptr, nullptr, 1, {0, 0, 0, 0, 0, 0}, {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr}}};

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
        fake_box[SK].base_sky  = nullptr;
        fake_box[SK].fx_colmap = nullptr;

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
        glFrustum(-view_x_slope * r_nearclip.f, view_x_slope * r_nearclip.f, -view_y_slope * r_nearclip.f,
                  view_y_slope * r_nearclip.f, r_nearclip.f, r_farclip.f);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        glLoadIdentity();

        glRotatef(270.0f - epi::DegreesFromBAM(viewvertangle), 1.0f, 0.0f, 0.0f);
        glRotatef(90.0f - epi::DegreesFromBAM(viewangle), 0.0f, 0.0f, 1.0f);
    }
    else
    {
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();

        glLoadIdentity();
        glFrustum(-view_x_slope * r_nearclip.f, view_x_slope * r_nearclip.f, -view_y_slope * r_nearclip.f,
                  view_y_slope * r_nearclip.f, r_nearclip.f, r_farclip.f * 4.0);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        glRotatef(270.0f - epi::DegreesFromBAM(viewvertangle), 1.0f, 0.0f, 0.0f);
        glRotatef(90.0f - epi::DegreesFromBAM(viewangle), 0.0f, 0.0f, 1.0f);
        if (current_sky_stretch == SKS_Stretch)
            glTranslatef(0.0f, 0.0f, (r_farclip.f * 2 * 0.15)); // Draw center above horizon a little
        else
            glTranslatef(0.0f, 0.0f, -(r_farclip.f * 2 * 0.15)); // Draw center below horizon a little
    }
}

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

// The following cylindrical sky-drawing routines are adapted from SLADE's 3D Renderer
// (https://github.com/sirjuddington/SLADE/blob/master/src/MapEditor/Renderer/MapRenderer3D.cpp)
// with additional modes and other tweaks

static HMM_Vec2 sky_circle[32];

static void buildSkyCircle()
{
    float rot = 0;
    for (auto &pos : sky_circle)
    {
        pos = {{HMM_SINF(rot), -HMM_COSF(rot)}};
        rot -= (HMM_PI32 * 2) / 32.0;
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

    if (current_sky_stretch == SKS_Mirror && bottom < -0.5f)
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
        glVertex3f((sky_circle[a + 1].X * dist), -(sky_circle[a + 1].Y * dist), (top * dist));
        glTexCoord2f(tc_x, tc_y1);
        glVertex3f((sky_circle[a].X * dist), -(sky_circle[a].Y * dist), (top * dist));

        // Bottom
        glColor4f(1.0f, 1.0f, 1.0f, abottom);
        glTexCoord2f(tc_x, tc_y2);
        glVertex3f((sky_circle[a].X * dist), -(sky_circle[a].Y * dist), (bottom * dist));
        glTexCoord2f(tc_x + tx, tc_y2);
        glVertex3f((sky_circle[a + 1].X * dist), -(sky_circle[a + 1].Y * dist), (bottom * dist));

        tc_x += tx;
    }

    // Link last point -> first
    // Top
    glColor4f(1.0f, 1.0f, 1.0f, atop);
    glTexCoord2f(tc_x + tx, tc_y1);
    glVertex3f((sky_circle[0].X * dist), -(sky_circle[0].Y * dist), (top * dist));
    glTexCoord2f(tc_x, tc_y1);
    glVertex3f((sky_circle[31].X * dist), -(sky_circle[31].Y * dist), (top * dist));

    // Bottom
    glColor4f(1.0f, 1.0f, 1.0f, abottom);
    glTexCoord2f(tc_x, tc_y2);
    glVertex3f((sky_circle[31].X * dist), -(sky_circle[31].Y * dist), (bottom * dist));
    glTexCoord2f(tc_x + tx, tc_y2);
    glVertex3f((sky_circle[0].X * dist), -(sky_circle[0].Y * dist), (bottom * dist));

    glEnd();
}

static void RGL_DrawSkyCylinder(void)
{
    GLuint sky = W_ImageCache(sky_image, false, ren_fx_colmap);

    if (currmap->forced_skystretch > SKS_Unset)
        current_sky_stretch = currmap->forced_skystretch;
    else if (!level_flags.mlook)
        current_sky_stretch = SKS_Vanilla;
    else
        current_sky_stretch = (skystretch_e)r_skystretch.d;

    // Center skybox a bit below the camera view
    RGL_SetupSkyMatrices();

    glDisable(GL_TEXTURE_2D);

    float dist     = r_farclip.f * 2.0f;
    float cap_dist = dist * 2.0f; // Ensure the caps extend beyond the cylindrical projection
                                  // Calculate some stuff based on sky height
    float sky_h_ratio;
    float solid_sky_h;
    if (IM_HEIGHT(sky_image) > 128 && current_sky_stretch != SKS_Stretch)
        sky_h_ratio = (float)IM_HEIGHT(sky_image) / 256;
    else if (current_sky_stretch == SKS_Vanilla)
        sky_h_ratio = 0.5f;
    else
        sky_h_ratio = 1.0f;
    if (current_sky_stretch == SKS_Vanilla)
        solid_sky_h = sky_h_ratio * 0.98f;
    else
        solid_sky_h = sky_h_ratio * 0.75f;
    float cap_z = dist * sky_h_ratio;

    RGBAColor fc_to_use = currmap->outdoor_fog_color;
    float    fd_to_use = 0.01f * currmap->outdoor_fog_density;
    // check for sector fog
    if (fc_to_use == kRGBANoValue)
    {
        fc_to_use = view_props->fog_color;
        fd_to_use = view_props->fog_density;
    }

    if (!r_culling.d && fc_to_use != kRGBANoValue)
    {
        sg_color fc = sg_make_color_1i(fc_to_use);
        glClearColor(fc.r, fc.g, fc.b, fc.a);
        glFogi(GL_FOG_MODE, GL_EXP);
        glFogfv(GL_FOG_COLOR, &fc.r);
        glFogf(GL_FOG_DENSITY, std::log1p(fd_to_use * 0.005f));
        glEnable(GL_FOG);
    }

    // Render top cap
    glColor4f(sky_cap_color.r, sky_cap_color.g, sky_cap_color.b, 1.0f);
    glBegin(GL_QUADS);
    glVertex3f(-cap_dist, -cap_dist, cap_z);
    glVertex3f(-cap_dist, cap_dist, cap_z);
    glVertex3f(cap_dist, cap_dist, cap_z);
    glVertex3f(cap_dist, -cap_dist, cap_z);
    glEnd();

    // Render bottom cap
    if (current_sky_stretch > SKS_Mirror)
        glColor4f(cull_fog_color.r, cull_fog_color.g, cull_fog_color.b, 1.0f);
    glBegin(GL_QUADS);
    if (current_sky_stretch == SKS_Vanilla)
        cap_z = 0;
    glVertex3f(-cap_dist, -cap_dist, -cap_z);
    glVertex3f(-cap_dist, cap_dist, -cap_z);
    glVertex3f(cap_dist, cap_dist, -cap_z);
    glVertex3f(cap_dist, -cap_dist, -cap_z);
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

    glEnable(GL_ALPHA_TEST);
    glEnable(GL_BLEND);

    if (current_sky_stretch == SKS_Mirror)
    {
        if (IM_HEIGHT(sky_image) > 128)
        {
            renderSkySlice(sky_h_ratio, solid_sky_h, 0.0f, 1.0f, dist, tx, ty);   // Top Fade
            renderSkySlice(solid_sky_h, 0.0f, 1.0f, 1.0f, dist, tx, ty);          // Top Solid
            renderSkySlice(0.0f, -solid_sky_h, 1.0f, 1.0f, dist, tx, ty);         // Bottom Solid
            renderSkySlice(-solid_sky_h, -sky_h_ratio, 1.0f, 0.0f, dist, tx, ty); // Bottom Fade
        }
        else
        {
            renderSkySlice(1.0f, 0.75f, 0.0f, 1.0f, dist, tx, ty);   // Top Fade
            renderSkySlice(0.75f, 0.0f, 1.0f, 1.0f, dist, tx, ty);   // Top Solid
            renderSkySlice(0.0f, -0.75f, 1.0f, 1.0f, dist, tx, ty);  // Bottom Solid
            renderSkySlice(-0.75f, -1.0f, 1.0f, 0.0f, dist, tx, ty); // Bottom Fade
        }
    }
    else if (current_sky_stretch == SKS_Repeat)
    {
        if (IM_HEIGHT(sky_image) > 128)
        {
            renderSkySlice(sky_h_ratio, solid_sky_h, 0.0f, 1.0f, dist, tx, ty);   // Top Fade
            renderSkySlice(solid_sky_h, -solid_sky_h, 1.0f, 1.0f, dist, tx, ty);  // Middle Solid
            renderSkySlice(-solid_sky_h, -sky_h_ratio, 1.0f, 0.0f, dist, tx, ty); // Bottom Fade
        }
        else
        {
            renderSkySlice(1.0f, 0.75f, 0.0f, 1.0f, dist, tx, ty);   // Top Fade
            renderSkySlice(0.75f, -0.75f, 1.0f, 1.0f, dist, tx, ty); // Middle Solid
            renderSkySlice(-0.75f, -1.0f, 1.0f, 0.0f, dist, tx, ty); // Bottom Fade
        }
    }
    else if (current_sky_stretch == SKS_Stretch)
    {
        if (IM_HEIGHT(sky_image) > 128)
        {
            ty = ((float)IM_HEIGHT(sky_image) / 256.0f);
            renderSkySlice(sky_h_ratio, solid_sky_h, 0.0f, 1.0f, dist, tx, ty);   // Top Fade
            renderSkySlice(solid_sky_h, -solid_sky_h, 1.0f, 1.0f, dist, tx, ty);  // Middle Solid
            renderSkySlice(-solid_sky_h, -sky_h_ratio, 1.0f, 0.0f, dist, tx, ty); // Bottom Fade
        }
        else
        {
            ty = 1.0f;
            renderSkySlice(1.0f, 0.75f, 0.0f, 1.0f, dist, tx, ty);   // Top Fade
            renderSkySlice(0.75f, -0.75f, 1.0f, 1.0f, dist, tx, ty); // Middle Solid
            renderSkySlice(-0.75f, -1.0f, 1.0f, 0.0f, dist, tx, ty); // Bottom Fade
        }
    }
    else // Vanilla (or sane value if somehow this gets set out of expected range)
    {
        if (IM_HEIGHT(sky_image) > 128)
        {
            renderSkySlice(sky_h_ratio, solid_sky_h, 0.0f, 1.0f, dist / 2, tx, ty);               // Top Fade
            renderSkySlice(solid_sky_h, sky_h_ratio - solid_sky_h, 1.0f, 1.0f, dist / 2, tx, ty); // Middle Solid
            renderSkySlice(sky_h_ratio - solid_sky_h, 0.0f, 1.0f, 0.0f, dist / 2, tx, ty);        // Bottom Fade
        }
        else
        {
            ty *= 1.5f;
            renderSkySlice(1.0f, 0.98f, 0.0f, 1.0f, dist / 3, tx, ty);  // Top Fade
            renderSkySlice(0.98f, 0.35f, 1.0f, 1.0f, dist / 3, tx, ty); // Middle Solid
            renderSkySlice(0.35f, 0.33f, 1.0f, 0.0f, dist / 3, tx, ty); // Bottom Fade
        }
    }

    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    if (!r_culling.d && current_fog_rgb != kRGBANoValue)
        glDisable(GL_FOG);

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

    if (r_colormaterial.d || !r_colorlighting.d)
        glColor4fv(col);
    else
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, col);

    RGBAColor fc_to_use = currmap->outdoor_fog_color;
    float    fd_to_use = 0.01f * currmap->outdoor_fog_density;
    // check for sector fog
    if (fc_to_use == kRGBANoValue)
    {
        fc_to_use = view_props->fog_color;
        fd_to_use = view_props->fog_density;
    }

    if (!r_culling.d && fc_to_use != kRGBANoValue)
    {
        sg_color fc = sg_make_color_1i(fc_to_use);
        glClearColor(fc.r, fc.g, fc.b, fc.a);
        glFogi(GL_FOG_MODE, GL_EXP);
        glFogfv(GL_FOG_COLOR, &fc.r);
        glFogf(GL_FOG_DENSITY, std::log1p(fd_to_use * 0.01f));
        glEnable(GL_FOG);
    }

    // top
    glBindTexture(GL_TEXTURE_2D, fake_box[SK].tex[WSKY_Top]);
    glNormal3i(0, 0, -1);
#ifdef APPLE_SILICON
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
#endif
    glBegin(GL_QUADS);
    glTexCoord2f(v0, v0);
    glVertex3f(-dist, dist, +dist);
    glTexCoord2f(v0, v1);
    glVertex3f(-dist, -dist, +dist);
    glTexCoord2f(v1, v1);
    glVertex3f(dist, -dist, +dist);
    glTexCoord2f(v1, v0);
    glVertex3f(dist, dist, +dist);
    glEnd();

    // bottom
    glBindTexture(GL_TEXTURE_2D, fake_box[SK].tex[WSKY_Bottom]);
    glNormal3i(0, 0, +1);
#ifdef APPLE_SILICON
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
#endif
    glBegin(GL_QUADS);
    glTexCoord2f(v0, v0);
    glVertex3f(-dist, -dist, -dist);
    glTexCoord2f(v0, v1);
    glVertex3f(-dist, dist, -dist);
    glTexCoord2f(v1, v1);
    glVertex3f(dist, dist, -dist);
    glTexCoord2f(v1, v0);
    glVertex3f(dist, -dist, -dist);
    glEnd();

    // north
    glBindTexture(GL_TEXTURE_2D, fake_box[SK].tex[WSKY_North]);
    glNormal3i(0, -1, 0);
#ifdef APPLE_SILICON
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
#endif
    glBegin(GL_QUADS);
    glTexCoord2f(v0, v0);
    glVertex3f(-dist, dist, -dist);
    glTexCoord2f(v0, v1);
    glVertex3f(-dist, dist, +dist);
    glTexCoord2f(v1, v1);
    glVertex3f(dist, dist, +dist);
    glTexCoord2f(v1, v0);
    glVertex3f(dist, dist, -dist);
    glEnd();

    // east
    glBindTexture(GL_TEXTURE_2D, fake_box[SK].tex[WSKY_East]);
    glNormal3i(-1, 0, 0);
#ifdef APPLE_SILICON
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
#endif
    glBegin(GL_QUADS);
    glTexCoord2f(v0, v0);
    glVertex3f(dist, dist, -dist);
    glTexCoord2f(v0, v1);
    glVertex3f(dist, dist, +dist);
    glTexCoord2f(v1, v1);
    glVertex3f(dist, -dist, +dist);
    glTexCoord2f(v1, v0);
    glVertex3f(dist, -dist, -dist);
    glEnd();

    // south
    glBindTexture(GL_TEXTURE_2D, fake_box[SK].tex[WSKY_South]);
    glNormal3i(0, +1, 0);
#ifdef APPLE_SILICON
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
#endif
    glBegin(GL_QUADS);
    glTexCoord2f(v0, v0);
    glVertex3f(dist, -dist, -dist);
    glTexCoord2f(v0, v1);
    glVertex3f(dist, -dist, +dist);
    glTexCoord2f(v1, v1);
    glVertex3f(-dist, -dist, +dist);
    glTexCoord2f(v1, v0);
    glVertex3f(-dist, -dist, -dist);
    glEnd();

    // west
    glBindTexture(GL_TEXTURE_2D, fake_box[SK].tex[WSKY_West]);
    glNormal3i(+1, 0, 0);
#ifdef APPLE_SILICON
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
#endif
    glBegin(GL_QUADS);
    glTexCoord2f(v0, v0);
    glVertex3f(-dist, -dist, -dist);
    glTexCoord2f(v0, v1);
    glVertex3f(-dist, -dist, +dist);
    glTexCoord2f(v1, v1);
    glVertex3f(-dist, dist, +dist);
    glTexCoord2f(v1, v0);
    glVertex3f(-dist, dist, -dist);
    glEnd();

    glDisable(GL_TEXTURE_2D);
    if (!r_culling.d && current_fog_rgb != kRGBANoValue)
        glDisable(GL_FOG);

    RGL_RevertSkyMatrices();
}

void RGL_FinishSky(void)
{
    glEnd(); // End glBegin(GL_TRIANGLES) from RGL_BeginSky

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    if (!need_to_draw_sky)
        return;

    // draw sky picture, but DON'T affect the depth buffering

    glDepthMask(GL_FALSE);

    if (r_culling.d)
        glDisable(GL_DEPTH_TEST);

    if (!r_dumbsky.d)
        glDepthFunc(GL_GREATER);

#if defined(EDGE_GL_ES2)
    // On ES2 the clip planes seem to maybe be inverting z values, this fixes that
    glDepthFunc(GL_ALWAYS);
#endif

    if (custom_sky_box)
        RGL_DrawSkyBox();
    else
        RGL_DrawSkyCylinder();

    if (r_culling.d)
        glEnable(GL_DEPTH_TEST);

    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);

    glDisable(GL_TEXTURE_2D);
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

    float x0 = seg->v1->X;
    float y0 = seg->v1->Y;
    MIR_Coordinate(x0, y0);
    seg = seg->sub_next;
    if (!seg)
        return;

    float x1 = seg->v1->X;
    float y1 = seg->v1->Y;
    MIR_Coordinate(x1, y1);
    seg = seg->sub_next;
    if (!seg)
        return;

    while (seg)
    {
        float x2 = seg->v1->X;
        float y2 = seg->v1->Y;
        MIR_Coordinate(x2, y2);

        glVertex3f(x0, y0, h);
        glVertex3f(x1, y1, h);
        glVertex3f(x2, y2, h);

        x1  = x2;
        y1  = y2;
        seg = seg->sub_next;
    }
}

void RGL_DrawSkyWall(seg_t *seg, float h1, float h2)
{
    need_to_draw_sky = true;

    if (r_dumbsky.d)
        return;

    float x1 = seg->v1->X;
    float y1 = seg->v1->Y;
    float x2 = seg->v2->X;
    float y2 = seg->v2->Y;

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

static const char *UserSkyFaceName(const char *base, int face)
{
    static char       buffer[64];
    static const char letters[] = "NESWTB";

    sprintf(buffer, "%s_%c", base, letters[face]);
    return buffer;
}

int RGL_UpdateSkyBoxTextures(void)
{
    int SK = ren_fx_colmap ? 1 : 0;

    fake_skybox_t *info = &fake_box[SK];

    if (info->base_sky == sky_image && info->fx_colmap == ren_fx_colmap)
    {
        return SK;
    }

    info->base_sky  = sky_image;
    info->fx_colmap = ren_fx_colmap;

    // check for custom sky boxes
    info->face[WSKY_North] = W_ImageLookup(UserSkyFaceName(sky_image->name.c_str(), WSKY_North), INS_Texture, ILF_Null);

    // LOBO 2022:
    // If we do nothing, our EWAD skybox will be used for all maps.
    // So we need to disable it if we have a pwad that contains it's
    // own sky.
    if (W_LoboDisableSkybox(sky_image->name.c_str()))
    {
        info->face[WSKY_North] = nullptr;
        // I_Printf("Skybox turned OFF\n");
    }

    // Set colors for culling fog and faux skybox caps - Dasho
    const uint8_t *what_palette = (const uint8_t *)&playpal_data[0];
    if (sky_image->source_palette >= 0)
        what_palette = (const uint8_t *)W_LoadLump(sky_image->source_palette);
    image_data_c *tmp_img_data =
        R_PalettisedToRGB(ReadAsEpiBlock((image_c *)sky_image), what_palette, sky_image->opacity);
    cull_fog_color = sg_make_color_1i(tmp_img_data->AverageColor(0, sky_image->actual_w, 0, sky_image->actual_h / 2));
    sky_cap_color = sg_make_color_1i(tmp_img_data->AverageColor(0, sky_image->actual_w, sky_image->actual_h * 3 / 4, sky_image->actual_h));
    delete tmp_img_data;

    if (info->face[WSKY_North])
    {
        custom_sky_box = true;

        info->face_size = info->face[WSKY_North]->total_w;

        for (int i = WSKY_East; i < 6; i++)
            info->face[i] = W_ImageLookup(UserSkyFaceName(sky_image->name.c_str(), i), INS_Texture);

        for (int k = 0; k < 6; k++)
            info->tex[k] = W_ImageCache(info->face[k], false, ren_fx_colmap);

        return SK;
    }
    else
    {
        info->face_size = 256;
        custom_sky_box  = false;
        return -1;
    }
}

void RGL_PreCacheSky(void)
{
    buildSkyCircle();
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
