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

#include "r_sky.h"

#include <math.h>

#include "dm_state.h"
#include "epi.h"
#include "g_game.h" // current_map
#include "i_defs_gl.h"
#include "im_data.h"
#include "m_math.h"
#include "r_colormap.h"
#include "r_gldefs.h"
#include "r_image.h"
#include "r_misc.h"
#include "r_modes.h"
#include "r_sky.h"
#include "r_texgl.h"
#include "r_units.h"
#include "w_flat.h"
#include "w_wad.h"

const Image *sky_image;

bool custom_skybox;

// needed for SKY
extern ImageData *ReadAsEpiBlock(Image *rim);

extern ConsoleVariable draw_culling;

static sg_color sky_cap_color;

static SkyStretch current_sky_stretch = kSkyStretchUnset;

EDGE_DEFINE_CONSOLE_VARIABLE_CLAMPED(sky_stretch_mode, "0", kConsoleVariableFlagArchive, 0, 3);

struct SectorSkyRing
{
    // which group of connected skies (0 if none)
    int group;

    // link of sector in RING
    SectorSkyRing *next;
    SectorSkyRing *previous;

    // maximal sky height of group
    float maximum_height;
};

//
// ComputeSkyHeights
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
void ComputeSkyHeights(void)
{
    int     i;
    Line   *ld;
    Sector *sec;

    // --- initialise ---

    SectorSkyRing *rings = new SectorSkyRing[total_level_sectors];

    memset(rings, 0, total_level_sectors * sizeof(SectorSkyRing));

    for (i = 0, sec = level_sectors; i < total_level_sectors; i++, sec++)
    {
        if (!EDGE_IMAGE_IS_SKY(sec->ceiling))
            continue;

        rings[i].group = (i + 1);
        rings[i].next = rings[i].previous = rings + i;
        rings[i].maximum_height           = sec->ceiling_height;

        // leave some room for tall sprites
        static const float SPR_H_MAX = 256.0f;

        if (sec->ceiling_height < 30000.0f && (sec->ceiling_height > sec->floor_height) &&
            (sec->ceiling_height < sec->floor_height + SPR_H_MAX))
        {
            rings[i].maximum_height = sec->floor_height + SPR_H_MAX;
        }
    }

    // --- make the pass over linedefs ---

    for (i = 0, ld = level_lines; i < total_level_lines; i++, ld++)
    {
        Sector        *sec1, *sec2;
        SectorSkyRing *ring1, *ring2, *tmp_R;

        if (!ld->side[0] || !ld->side[1])
            continue;

        sec1 = ld->front_sector;
        sec2 = ld->back_sector;

        EPI_ASSERT(sec1 && sec2);

        if (sec1 == sec2)
            continue;

        ring1 = rings + (sec1 - level_sectors);
        ring2 = rings + (sec2 - level_sectors);

        // we require sky on both sides
        if (ring1->group == 0 || ring2->group == 0)
            continue;

        // already in the same group ?
        if (ring1->group == ring2->group)
            continue;

        // swap sectors to ensure the lower group is added to the higher
        // group, since we don't need to update the `max_h' fields of the
        // highest group.

        if (ring1->maximum_height < ring2->maximum_height)
        {
            tmp_R = ring1;
            ring1 = ring2;
            ring2 = tmp_R;
        }

        // update the group numbers in the second group

        ring2->group          = ring1->group;
        ring2->maximum_height = ring1->maximum_height;

        for (tmp_R = ring2->next; tmp_R != ring2; tmp_R = tmp_R->next)
        {
            tmp_R->group          = ring1->group;
            tmp_R->maximum_height = ring1->maximum_height;
        }

        // merge 'em baby...

        ring1->next->previous = ring2;
        ring2->next->previous = ring1;

        tmp_R       = ring1->next;
        ring1->next = ring2->next;
        ring2->next = tmp_R;
    }

    // --- now store the results, and free up ---

    for (i = 0, sec = level_sectors; i < total_level_sectors; i++, sec++)
    {
        if (rings[i].group > 0)
            sec->sky_height = rings[i].maximum_height;
    }

    delete[] rings;
}

//----------------------------------------------------------------------------

bool need_to_draw_sky = false;

struct FakeSkybox
{
    const Image *base_sky;

    const Colormap *effect_colormap;

    int face_size;

    GLuint texture[6];

    // face images are only present for custom skyboxes.
    // pseudo skyboxes are generated outside of the image system.
    const Image *face[6];
};

static FakeSkybox fake_box[2] = {
    {nullptr, nullptr, 1, {0, 0, 0, 0, 0, 0}, {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr}},
    {nullptr, nullptr, 1, {0, 0, 0, 0, 0, 0}, {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr}}};

static void DeleteSkyTexGroup(int SK)
{
    for (int i = 0; i < 6; i++)
    {
        if (fake_box[SK].texture[i] != 0)
        {
            glDeleteTextures(1, &fake_box[SK].texture[i]);
            fake_box[SK].texture[i] = 0;
        }
    }
}

void DeleteSkyTextures(void)
{
    for (int SK = 0; SK < 2; SK++)
    {
        fake_box[SK].base_sky        = nullptr;
        fake_box[SK].effect_colormap = nullptr;

        DeleteSkyTexGroup(SK);
    }
}

static void SetupSkyMatrices(void)
{
    if (custom_skybox)
    {
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();

        glLoadIdentity();
        glFrustum(-view_x_slope * renderer_near_clip.f_, view_x_slope * renderer_near_clip.f_,
                  -view_y_slope * renderer_near_clip.f_, view_y_slope * renderer_near_clip.f_, renderer_near_clip.f_,
                  renderer_far_clip.f_);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        glLoadIdentity();

        glRotatef(270.0f - epi::DegreesFromBAM(view_vertical_angle), 1.0f, 0.0f, 0.0f);
        glRotatef(90.0f - epi::DegreesFromBAM(view_angle), 0.0f, 0.0f, 1.0f);
    }
    else
    {
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();

        glLoadIdentity();
        glFrustum(-view_x_slope * renderer_near_clip.f_, view_x_slope * renderer_near_clip.f_,
                  -view_y_slope * renderer_near_clip.f_, view_y_slope * renderer_near_clip.f_, renderer_near_clip.f_,
                  renderer_far_clip.f_ * 4.0);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        glRotatef(270.0f - epi::DegreesFromBAM(view_vertical_angle), 1.0f, 0.0f, 0.0f);
        glRotatef(90.0f - epi::DegreesFromBAM(view_angle), 0.0f, 0.0f, 1.0f);
        if (current_sky_stretch == kSkyStretchStretch)
            glTranslatef(0.0f, 0.0f,
                         (renderer_far_clip.f_ * 2 * 0.15));  // Draw center above horizon a little
        else
            glTranslatef(0.0f, 0.0f,
                         -(renderer_far_clip.f_ * 2 * 0.15)); // Draw center below horizon a little
    }
}

static void RendererRevertSkyMatrices(void)
{
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void BeginSky(void)
{
    need_to_draw_sky = false;

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDisable(GL_TEXTURE_2D);

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    // Draw the entire sky using only one glBegin/glEnd clause.
    // glEnd is called in FinishSky and this code assumes that only
    // RenderSkyWall and RenderSkyPlane is doing OpenGL calls in
    // between.
    glBegin(GL_TRIANGLES);
}

// The following cylindrical sky-drawing routines are adapted from SLADE's 3D
// Renderer
// (https://github.com/sirjuddington/SLADE/blob/master/src/MapEditor/Renderer/MapRenderer3D.cpp)
// with additional modes and other tweaks

static HMM_Vec2 sky_circle[32];

static void BuildSkyCircle()
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
static void RenderSkySlice(float top, float bottom, float atop, float abottom, float dist, float tx, float ty)
{
    float tc_x  = 0.0f;
    float tc_y1 = (top + 1.0f) * (ty * 0.5f);
    float tc_y2 = (bottom + 1.0f) * (ty * 0.5f);

    if (current_sky_stretch == kSkyStretchMirror && bottom < -0.5f)
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

static void RenderSkyCylinder(void)
{
    GLuint sky = ImageCache(sky_image, false, render_view_effect_colormap);

    if (current_map->forced_skystretch_ > kSkyStretchUnset)
        current_sky_stretch = current_map->forced_skystretch_;
    else if (!level_flags.mouselook)
        current_sky_stretch = kSkyStretchVanilla;
    else
        current_sky_stretch = (SkyStretch)sky_stretch_mode.d_;

    // Center skybox a bit below the camera view
    SetupSkyMatrices();

    glDisable(GL_TEXTURE_2D);

    float dist     = renderer_far_clip.f_ * 2.0f;
    float cap_dist = dist * 2.0f; // Ensure the caps extend beyond the cylindrical
                                  // projection Calculate some stuff based on sky height
    float sky_h_ratio;
    float solid_sky_h;
    if (sky_image->ScaledHeightActual() > 128 && current_sky_stretch != kSkyStretchStretch)
        sky_h_ratio = (float)sky_image->ScaledHeightActual() / 256;
    else if (current_sky_stretch == kSkyStretchVanilla)
        sky_h_ratio = 0.5f;
    else
        sky_h_ratio = 1.0f;
    if (current_sky_stretch == kSkyStretchVanilla)
        solid_sky_h = sky_h_ratio * 0.98f;
    else
        solid_sky_h = sky_h_ratio * 0.75f;
    float cap_z = dist * sky_h_ratio;

    RGBAColor fc_to_use = current_map->outdoor_fog_color_;
    float     fd_to_use = 0.01f * current_map->outdoor_fog_density_;
    // check for sector fog
    if (fc_to_use == kRGBANoValue)
    {
        fc_to_use = view_properties->fog_color;
        fd_to_use = view_properties->fog_density;
    }

    if (!draw_culling.d_ && fc_to_use != kRGBANoValue)
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
    if (current_sky_stretch > kSkyStretchMirror)
        glColor4f(culling_fog_color.r, culling_fog_color.g, culling_fog_color.b, 1.0f);
    glBegin(GL_QUADS);
    if (current_sky_stretch == kSkyStretchVanilla)
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
    if (sky_image->ScaledWidthActual() > 256)
        tx = 0.125f / ((float)sky_image->ScaledWidthActual() / 256.0f);

    glEnable(GL_ALPHA_TEST);
    glEnable(GL_BLEND);

    if (current_sky_stretch == kSkyStretchMirror)
    {
        if (sky_image->ScaledHeightActual() > 128)
        {
            RenderSkySlice(sky_h_ratio, solid_sky_h, 0.0f, 1.0f, dist, tx,
                           ty); // Top Fade
            RenderSkySlice(solid_sky_h, 0.0f, 1.0f, 1.0f, dist, tx,
                           ty); // Top Solid
            RenderSkySlice(0.0f, -solid_sky_h, 1.0f, 1.0f, dist, tx,
                           ty); // Bottom Solid
            RenderSkySlice(-solid_sky_h, -sky_h_ratio, 1.0f, 0.0f, dist, tx,
                           ty); // Bottom Fade
        }
        else
        {
            RenderSkySlice(1.0f, 0.75f, 0.0f, 1.0f, dist, tx, ty); // Top Fade
            RenderSkySlice(0.75f, 0.0f, 1.0f, 1.0f, dist, tx, ty); // Top Solid
            RenderSkySlice(0.0f, -0.75f, 1.0f, 1.0f, dist, tx,
                           ty);                                    // Bottom Solid
            RenderSkySlice(-0.75f, -1.0f, 1.0f, 0.0f, dist, tx,
                           ty);                                    // Bottom Fade
        }
    }
    else if (current_sky_stretch == kSkyStretchRepeat)
    {
        if (sky_image->ScaledHeightActual() > 128)
        {
            RenderSkySlice(sky_h_ratio, solid_sky_h, 0.0f, 1.0f, dist, tx,
                           ty); // Top Fade
            RenderSkySlice(solid_sky_h, -solid_sky_h, 1.0f, 1.0f, dist, tx,
                           ty); // Middle Solid
            RenderSkySlice(-solid_sky_h, -sky_h_ratio, 1.0f, 0.0f, dist, tx,
                           ty); // Bottom Fade
        }
        else
        {
            RenderSkySlice(1.0f, 0.75f, 0.0f, 1.0f, dist, tx, ty); // Top Fade
            RenderSkySlice(0.75f, -0.75f, 1.0f, 1.0f, dist, tx,
                           ty);                                    // Middle Solid
            RenderSkySlice(-0.75f, -1.0f, 1.0f, 0.0f, dist, tx,
                           ty);                                    // Bottom Fade
        }
    }
    else if (current_sky_stretch == kSkyStretchStretch)
    {
        if (sky_image->ScaledHeightActual() > 128)
        {
            ty = ((float)sky_image->ScaledHeightActual() / 256.0f);
            RenderSkySlice(sky_h_ratio, solid_sky_h, 0.0f, 1.0f, dist, tx,
                           ty); // Top Fade
            RenderSkySlice(solid_sky_h, -solid_sky_h, 1.0f, 1.0f, dist, tx,
                           ty); // Middle Solid
            RenderSkySlice(-solid_sky_h, -sky_h_ratio, 1.0f, 0.0f, dist, tx,
                           ty); // Bottom Fade
        }
        else
        {
            ty = 1.0f;
            RenderSkySlice(1.0f, 0.75f, 0.0f, 1.0f, dist, tx, ty); // Top Fade
            RenderSkySlice(0.75f, -0.75f, 1.0f, 1.0f, dist, tx,
                           ty);                                    // Middle Solid
            RenderSkySlice(-0.75f, -1.0f, 1.0f, 0.0f, dist, tx,
                           ty);                                    // Bottom Fade
        }
    }
    else // Vanilla (or sane value if somehow this gets set out of expected
         // range)
    {
        if (sky_image->ScaledHeightActual() > 128)
        {
            RenderSkySlice(sky_h_ratio, solid_sky_h, 0.0f, 1.0f, dist / 2, tx,
                           ty);                                                                   // Top Fade
            RenderSkySlice(solid_sky_h, sky_h_ratio - solid_sky_h, 1.0f, 1.0f, dist / 2, tx, ty); // Middle Solid
            RenderSkySlice(sky_h_ratio - solid_sky_h, 0.0f, 1.0f, 0.0f, dist / 2, tx, ty);        // Bottom Fade
        }
        else
        {
            ty *= 1.5f;
            RenderSkySlice(1.0f, 0.98f, 0.0f, 1.0f, dist / 3, tx,
                           ty); // Top Fade
            RenderSkySlice(0.98f, 0.35f, 1.0f, 1.0f, dist / 3, tx,
                           ty); // Middle Solid
            RenderSkySlice(0.35f, 0.33f, 1.0f, 0.0f, dist / 3, tx,
                           ty); // Bottom Fade
        }
    }

    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    if (!draw_culling.d_)
        glDisable(GL_FOG);

    RendererRevertSkyMatrices();
}

static void RenderSkybox(void)
{
    float dist = renderer_far_clip.f_ / 2.0f;

    int SK = UpdateSkyboxTextures();

    EPI_ASSERT(SK >= 0);

    SetupSkyMatrices();

    float v0 = 0.0f;
    float v1 = 1.0f;

    if (renderer_dumb_clamp.d_)
    {
        float size = fake_box[SK].face_size;

        v0 = 0.5f / size;
        v1 = 1.0f - v0;
    }

    glEnable(GL_TEXTURE_2D);

    float col[4];

    col[0] = render_view_red_multiplier;
    col[1] = render_view_green_multiplier;
    col[2] = render_view_blue_multiplier;
    col[3] = 1.0f;

    glColor4fv(col);

    RGBAColor fc_to_use = current_map->outdoor_fog_color_;
    float     fd_to_use = 0.01f * current_map->outdoor_fog_density_;
    // check for sector fog
    if (fc_to_use == kRGBANoValue)
    {
        fc_to_use = view_properties->fog_color;
        fd_to_use = view_properties->fog_density;
    }

    if (!draw_culling.d_ && fc_to_use != kRGBANoValue)
    {
        sg_color fc = sg_make_color_1i(fc_to_use);
        glClearColor(fc.r, fc.g, fc.b, fc.a);
        glFogi(GL_FOG_MODE, GL_EXP);
        glFogfv(GL_FOG_COLOR, &fc.r);
        glFogf(GL_FOG_DENSITY, std::log1p(fd_to_use * 0.01f));
        glEnable(GL_FOG);
    }

    // top
    glBindTexture(GL_TEXTURE_2D, fake_box[SK].texture[kSkyboxTop]);
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
    glBindTexture(GL_TEXTURE_2D, fake_box[SK].texture[kSkyboxBottom]);
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
    glBindTexture(GL_TEXTURE_2D, fake_box[SK].texture[kSkyboxNorth]);
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
    glBindTexture(GL_TEXTURE_2D, fake_box[SK].texture[kSkyboxEast]);
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
    glBindTexture(GL_TEXTURE_2D, fake_box[SK].texture[kSkyboxSouth]);
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
    glBindTexture(GL_TEXTURE_2D, fake_box[SK].texture[kSkyboxWest]);
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
    if (!draw_culling.d_)
        glDisable(GL_FOG);

    RendererRevertSkyMatrices();
}

void FinishSky(void)
{
    glEnd(); // End glBegin(GL_TRIANGLES) from BeginSky

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    if (!need_to_draw_sky)
        return;

    // draw sky picture, but DON'T affect the depth buffering

    glDepthMask(GL_FALSE);

    if (draw_culling.d_)
        glDisable(GL_DEPTH_TEST);

    if (!renderer_dumb_sky.d_)
        glDepthFunc(GL_GREATER);

#if defined(EDGE_GL_ES2)
    // On ES2 the clip planes seem to maybe be inverting z values, this fixes
    // that
    glDepthFunc(GL_ALWAYS);
#endif

    if (custom_skybox)
        RenderSkybox();
    else
        RenderSkyCylinder();

    if (draw_culling.d_)
        glEnable(GL_DEPTH_TEST);

    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);

    glDisable(GL_TEXTURE_2D);
}

void RenderSkyPlane(Subsector *sub, float h)
{
    need_to_draw_sky = true;

    if (renderer_dumb_sky.d_)
        return;

    MirrorHeight(h);

    glNormal3f(0, 0, (view_z > h) ? 1.0f : -1.0f);

    Seg *seg = sub->segs;
    if (!seg)
        return;

    float x0 = seg->vertex_1->X;
    float y0 = seg->vertex_1->Y;
    MirrorCoordinate(x0, y0);
    seg = seg->subsector_next;
    if (!seg)
        return;

    float x1 = seg->vertex_1->X;
    float y1 = seg->vertex_1->Y;
    MirrorCoordinate(x1, y1);
    seg = seg->subsector_next;
    if (!seg)
        return;

    while (seg)
    {
        float x2 = seg->vertex_1->X;
        float y2 = seg->vertex_1->Y;
        MirrorCoordinate(x2, y2);

        glVertex3f(x0, y0, h);
        glVertex3f(x1, y1, h);
        glVertex3f(x2, y2, h);

        x1  = x2;
        y1  = y2;
        seg = seg->subsector_next;
    }
}

void RenderSkyWall(Seg *seg, float h1, float h2)
{
    need_to_draw_sky = true;

    if (renderer_dumb_sky.d_)
        return;

    float x1 = seg->vertex_1->X;
    float y1 = seg->vertex_1->Y;
    float x2 = seg->vertex_2->X;
    float y2 = seg->vertex_2->Y;

    MirrorCoordinate(x1, y1);
    MirrorCoordinate(x2, y2);

    MirrorHeight(h1);
    MirrorHeight(h2);

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

int UpdateSkyboxTextures(void)
{
    int SK = render_view_effect_colormap ? 1 : 0;

    FakeSkybox *info = &fake_box[SK];

    if (info->base_sky == sky_image && info->effect_colormap == render_view_effect_colormap)
    {
        return SK;
    }

    info->base_sky        = sky_image;
    info->effect_colormap = render_view_effect_colormap;

    // check for custom sky boxes
    info->face[kSkyboxNorth] =
        ImageLookup(UserSkyFaceName(sky_image->name_.c_str(), kSkyboxNorth), kImageNamespaceTexture, kImageLookupNull);

    // LOBO 2022:
    // If we do nothing, our EWAD skybox will be used for all maps.
    // So we need to disable it if we have a pwad that contains it's
    // own sky.
    if (DisableStockSkybox(sky_image->name_.c_str()))
    {
        info->face[kSkyboxNorth] = nullptr;
        // LogPrint("Skybox turned OFF\n");
    }

    // Set colors for culling fog and faux skybox caps - Dasho
    const uint8_t *what_palette = (const uint8_t *)&playpal_data[0];
    if (sky_image->source_palette_ >= 0)
        what_palette = (const uint8_t *)LoadLumpIntoMemory(sky_image->source_palette_);
    ImageData *tmp_img_data = ReadAsEpiBlock((Image *)sky_image);
    ImageData *tmp_rgb_img_data = RGBFromPalettised(tmp_img_data, what_palette, sky_image->opacity_);
    culling_fog_color =
        sg_make_color_1i(tmp_rgb_img_data->AverageColor(0, sky_image->actual_width_, 0, sky_image->actual_height_ / 2));
    sky_cap_color = sg_make_color_1i(tmp_rgb_img_data->AverageColor(
        0, sky_image->actual_width_, sky_image->actual_height_ * 3 / 4, sky_image->actual_height_));
    delete tmp_img_data;
    delete tmp_rgb_img_data;
    if (what_palette != (const uint8_t *)&playpal_data[0])
        delete[] what_palette;

    if (info->face[kSkyboxNorth])
    {
        custom_skybox = true;

        info->face_size = info->face[kSkyboxNorth]->total_width_;

        for (int i = kSkyboxEast; i < 6; i++)
            info->face[i] = ImageLookup(UserSkyFaceName(sky_image->name_.c_str(), i), kImageNamespaceTexture);

        for (int k = 0; k < 6; k++)
            info->texture[k] = ImageCache(info->face[k], false, render_view_effect_colormap);

        return SK;
    }
    else
    {
        info->face_size = 256;
        custom_skybox   = false;
        return -1;
    }
}

void PrecacheSky(void)
{
    BuildSkyCircle();
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
