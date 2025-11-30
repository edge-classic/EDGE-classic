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
#include "n_network.h"
#include "p_tick.h"
#include "r_colormap.h"
#include "r_gldefs.h"
#include "r_image.h"
#include "r_mirror.h"
#include "r_misc.h"
#include "r_modes.h"
#include "r_sky.h"
#include "r_texgl.h"
#include "r_units.h"
#include "stb_sprintf.h"
#include "w_flat.h"
#include "w_wad.h"

const Image *sky_image;

// Reference for Boom sky transfer, if applicable
MapSurface *sky_ref = nullptr;

bool custom_skybox;

// needed for SKY
extern ImageData *ReadAsEpiBlock(Image *rim);

extern ConsoleVariable draw_culling;

static RGBAColor sky_cap_color;

static uint32_t total_sky_verts = 0;

static RendererVertex *sky_glvert = nullptr;

static bool sky_unit_started = false;

static HMM_Vec2 ddf_sky_scroll     = {{0, 0}};
static HMM_Vec2 ddf_old_sky_scroll = {{0, 0}};
static int      ddf_scroll_tic     = -1;

SkyStretch current_sky_stretch = kSkyStretchUnset;

static constexpr uint8_t kMBFSkyYShift = 28;

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

    EPI_CLEAR_MEMORY(rings, SectorSkyRing, total_level_sectors);

    for (i = 0, sec = level_sectors; i < total_level_sectors; i++, sec++)
    {
        if (!EDGE_IMAGE_IS_SKY(sec->ceiling))
            continue;

        // leave some room for tall sprites
        static const float SPR_H_MAX = 256.0f;

        rings[i].group = (i + 1);
        rings[i].next = rings[i].previous = rings + i;
        rings[i].maximum_height           = sec->ceiling_height + SPR_H_MAX;
    }

    // --- make the pass over linedefs ---

    for (i = 0, ld = level_lines; i < total_level_lines; i++, ld++)
    {
        const Sector  *sec1, *sec2;
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
            render_state->DeleteTexture(&fake_box[SK].texture[i]);
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

static void BeginSkyUnit(void)
{
    total_sky_verts = 0;
    StartUnitBatch(false);
    sky_glvert       = BeginRenderUnit(GL_TRIANGLES, kMaximumLocalVertices, GL_MODULATE, 0,
                                       (GLuint)kTextureEnvironmentDisable, 0, 0, kBlendingNone);
    sky_unit_started = true;
}

void BeginSky(void)
{
    need_to_draw_sky = false;

    render_state->ColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
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
static void RenderSkySlice(float top, float bottom, float atop, float abottom, float dist, float tx, float ty,
                           float offx, float offy, GLuint sky_tex_id, BlendingMode blend, RGBAColor fc_to_use,
                           float fd_to_use)
{
    float tc_x  = offx;
    float tc_y1 = (top + 1.0f) * (ty * 0.5f);
    float tc_y2 = (bottom + 1.0f) * (ty * 0.5f);

    if (current_sky_stretch == kSkyStretchMirror && bottom < -0.5f)
    {
        tc_y1 += offy;
        tc_y2 += offy;
    }
    else
    {
        tc_y1 -= offy;
        tc_y2 -= offy;
    }

    if (current_sky_stretch == kSkyStretchMirror && bottom < -0.5f)
    {
        tc_y1 = -tc_y1;
        tc_y2 = -tc_y2;
    }

    RGBAColor topcol    = epi::MakeRGBA(255, 255, 255, (uint8_t)(atop * 255.0f));
    RGBAColor bottomcol = epi::MakeRGBA(255, 255, 255, (uint8_t)(abottom * 255.0f));

    RendererVertex *glvert = BeginRenderUnit(GL_QUADS, 128, GL_MODULATE, sky_tex_id, (GLuint)kTextureEnvironmentDisable,
                                             0, 0, blend, fc_to_use, fd_to_use);

    // Go through circular points
    for (unsigned a = 0; a < 31; a++)
    {
        // Top
        glvert->rgba                   = topcol;
        glvert->texture_coordinates[0] = {{tc_x + tx, tc_y1}};
        glvert++->position             = {{(sky_circle[a + 1].X * dist), -(sky_circle[a + 1].Y * dist), (top * dist)}};
        glvert->rgba                   = topcol;
        glvert->texture_coordinates[0] = {{tc_x, tc_y1}};
        glvert++->position             = {{(sky_circle[a].X * dist), -(sky_circle[a].Y * dist), (top * dist)}};

        // Bottom
        glvert->rgba                   = bottomcol;
        glvert->texture_coordinates[0] = {{tc_x, tc_y2}};
        glvert++->position             = {{(sky_circle[a].X * dist), -(sky_circle[a].Y * dist), (bottom * dist)}};
        glvert->rgba                   = bottomcol;
        glvert->texture_coordinates[0] = {{tc_x + tx, tc_y2}};
        glvert++->position = {{(sky_circle[a + 1].X * dist), -(sky_circle[a + 1].Y * dist), (bottom * dist)}};

        tc_x += tx;
    }

    // Link last point -> first
    // Top
    glvert->rgba                   = topcol;
    glvert->texture_coordinates[0] = {{tc_x + tx, tc_y1}};
    glvert++->position             = {{(sky_circle[0].X * dist), -(sky_circle[0].Y * dist), (top * dist)}};
    glvert->rgba                   = topcol;
    glvert->texture_coordinates[0] = {{tc_x, tc_y1}};
    glvert++->position             = {{(sky_circle[31].X * dist), -(sky_circle[31].Y * dist), (top * dist)}};

    // Bottom
    glvert->rgba                   = bottomcol;
    glvert->texture_coordinates[0] = {{tc_x, tc_y2}};
    glvert++->position             = {{(sky_circle[31].X * dist), -(sky_circle[31].Y * dist), (bottom * dist)}};
    glvert->rgba                   = bottomcol;
    glvert->texture_coordinates[0] = {{tc_x + tx, tc_y2}};
    glvert->position               = {{(sky_circle[0].X * dist), -(sky_circle[0].Y * dist), (bottom * dist)}};

    EndRenderUnit(128);
}

static void RenderSkyCylinder(void)
{
    GLuint sky_tex_id = ImageCache(sky_image, true, render_view_effect_colormap);

    if (current_map->forced_skystretch_ > kSkyStretchUnset)
        current_sky_stretch = current_map->forced_skystretch_;
    else if (!level_flags.mouselook)
        current_sky_stretch = kSkyStretchVanilla;
    else
        current_sky_stretch = (SkyStretch)sky_stretch_mode.d_;

    // Center skybox a bit below the camera view
    SetupSkyMatrices();

    float dist     = renderer_far_clip.f_ * 2.0f;
    float cap_dist = dist * 2.0f; // Ensure the caps extend beyond the cylindrical
                                  // projection Calculate some stuff based on sky height
    float sky_h_ratio;
    float solid_sky_h;
    if (sky_image->ScaledHeight() > 128 && current_sky_stretch != kSkyStretchStretch)
        sky_h_ratio = (float)sky_image->ScaledHeight() / 256;
    else if (current_sky_stretch == kSkyStretchVanilla)
        sky_h_ratio = 0.5f;
    else
        sky_h_ratio = 1.0f;
    if (current_sky_stretch == kSkyStretchVanilla)
        solid_sky_h = sky_h_ratio * 0.98f;
    else
        solid_sky_h = sky_h_ratio * 0.75f;
    float cap_z = dist * sky_h_ratio;

    RGBAColor    fc_to_use = current_map->outdoor_fog_color_;
    float        fd_to_use = 0.01f * current_map->outdoor_fog_density_;
    BlendingMode blend     = kBlendingNoZBuffer;
    // check for sector fog
    if (fc_to_use == kRGBANoValue)
    {
        fc_to_use = view_properties->fog_color;
        fd_to_use = view_properties->fog_density;
    }
    if (draw_culling.d_)
    {
        fc_to_use = kRGBANoValue;
        fd_to_use = 0.0f;
        blend     = (BlendingMode)(blend | kBlendingNoFog);
    }
    else if (fc_to_use != kRGBANoValue)
    {
#ifdef EDGE_SOKOL
        fd_to_use *= (current_sky_stretch == kSkyStretchVanilla ? 0.03f : 0.010f);
#else
        fd_to_use *= (current_sky_stretch == kSkyStretchVanilla ? 0.03f : 0.010f);
#endif
    }

    // Render top cap
    RendererVertex *glvert   = BeginRenderUnit(GL_QUADS, 4, GL_MODULATE, 0, (GLuint)kTextureEnvironmentDisable, 0, 0,
                                               blend, fc_to_use, fd_to_use);
    RGBAColor       unit_col = sky_cap_color;

    glvert->rgba       = unit_col;
    glvert++->position = {{-cap_dist, -cap_dist, cap_z}};
    glvert->rgba       = unit_col;
    glvert++->position = {{-cap_dist, cap_dist, cap_z}};
    glvert->rgba       = unit_col;
    glvert++->position = {{cap_dist, cap_dist, cap_z}};
    glvert->rgba       = unit_col;
    glvert->position   = {{cap_dist, -cap_dist, cap_z}};

    EndRenderUnit(4);

    // Render bottom cap
    if (current_sky_stretch > kSkyStretchMirror)
        unit_col = culling_fog_color;
    if (current_sky_stretch == kSkyStretchVanilla)
        cap_z = 0;

    glvert = BeginRenderUnit(GL_QUADS, 4, GL_MODULATE, 0, (GLuint)kTextureEnvironmentDisable, 0, 0, blend, fc_to_use,
                             fd_to_use);

    glvert->rgba       = unit_col;
    glvert++->position = {{-cap_dist, -cap_dist, -cap_z}};
    glvert->rgba       = unit_col;
    glvert++->position = {{-cap_dist, cap_dist, -cap_z}};
    glvert->rgba       = unit_col;
    glvert++->position = {{cap_dist, cap_dist, -cap_z}};
    glvert->rgba       = unit_col;
    glvert->position   = {{cap_dist, -cap_dist, -cap_z}};

    EndRenderUnit(4);

    // Render skybox sides

    blend = (BlendingMode)(blend | kBlendingAlpha);

    // Check for odd sky sizes
    float tx   = 0.125f;
    float ty   = 2.0f;
    float offx = 0;
    float offy = 0;
    if (sky_image->ScaledWidth() > 256)
        tx = 0.125f / ((float)sky_image->ScaledWidth() / 256.0f);

    // Set scrolling...I guess MBF transfers should take precedence since part of their purpose is to
    // override the normal sky - Dasho
    if (sky_ref)
    {
        // Dasho - Dividing by the image height seems to work for scrolling skies, but not skies with static
        // offsets. I've tested with a few WADs (Eviternity, PD2, Toilet Doom) and things seem to be right.
        // The entire sky code needs to be rewritten anyway.
        if (!AlmostEquals(sky_ref->old_offset.Y, sky_ref->offset.Y))
        {
            if (!console_active && !paused && !menu_active && !time_stop_active && !erraticism_active)
            {
                offy = (HMM_Lerp(sky_ref->old_offset.Y, fractional_tic, sky_ref->offset.Y) - kMBFSkyYShift) /
                       sky_image->ScaledHeight();
            }
            else
                offy = (sky_ref->offset.Y - kMBFSkyYShift) / sky_image->ScaledHeight();
        }
        else
            offy = sky_ref->offset.Y - kMBFSkyYShift;
    }
    else
    {
        if (ddf_scroll_tic != game_tic)
        {
            ddf_old_sky_scroll = ddf_sky_scroll;
            ddf_sky_scroll.X += current_map->sky_scroll_x_;
            ddf_sky_scroll.Y += current_map->sky_scroll_y_;
            ddf_scroll_tic = game_tic;
        }
        if (!AlmostEquals(current_map->sky_scroll_x_, 0.0f))
        {
            if (!console_active && !paused && !menu_active && !time_stop_active && !erraticism_active)
                offx = HMM_Lerp(ddf_old_sky_scroll.X, fractional_tic, ddf_sky_scroll.X);
            else
                offx = ddf_sky_scroll.X;
        }
        if (!AlmostEquals(current_map->sky_scroll_y_, 0.0f))
        {
            if (!console_active && !paused && !menu_active && !time_stop_active && !erraticism_active)
                offy = HMM_Lerp(ddf_old_sky_scroll.Y, fractional_tic, ddf_sky_scroll.Y);
            else
                offy = ddf_sky_scroll.Y;
        }
    }

    if (current_sky_stretch == kSkyStretchMirror)
    {
        if (sky_image->ScaledHeight() > 128)
        {
            RenderSkySlice(sky_h_ratio, solid_sky_h, 0.0f, 1.0f, dist, tx, ty, offx, offy, sky_tex_id, blend, fc_to_use,
                           fd_to_use); // Top Fade
            RenderSkySlice(solid_sky_h, 0.0f, 1.0f, 1.0f, dist, tx, ty, offx, offy, sky_tex_id, blend, fc_to_use,
                           fd_to_use); // Top Solid
            RenderSkySlice(0.0f, -solid_sky_h, 1.0f, 1.0f, dist, tx, ty, offx, offy, sky_tex_id, blend, fc_to_use,
                           fd_to_use); // Bottom Solid
            RenderSkySlice(-solid_sky_h, -sky_h_ratio, 1.0f, 0.0f, dist, tx, ty, offx, offy, sky_tex_id, blend,
                           fc_to_use,
                           fd_to_use); // Bottom Fade
        }
        else
        {
            RenderSkySlice(1.0f, 0.75f, 0.0f, 1.0f, dist, tx, ty, offx, offy, sky_tex_id, blend, fc_to_use,
                           fd_to_use); // Top Fade
            RenderSkySlice(0.75f, 0.0f, 1.0f, 1.0f, dist, tx, ty, offx, offy, sky_tex_id, blend, fc_to_use,
                           fd_to_use); // Top Solid
            RenderSkySlice(0.0f, -0.75f, 1.0f, 1.0f, dist, tx, ty, offx, offy, sky_tex_id, blend, fc_to_use,
                           fd_to_use); // Bottom Solid
            RenderSkySlice(-0.75f, -1.0f, 1.0f, 0.0f, dist, tx, ty, offx, offy, sky_tex_id, blend, fc_to_use,
                           fd_to_use); // Bottom Fade
        }
    }
    else if (current_sky_stretch == kSkyStretchRepeat)
    {
        if (sky_image->ScaledHeight() > 128)
        {
            RenderSkySlice(sky_h_ratio, solid_sky_h, 0.0f, 1.0f, dist, tx, ty, offx, offy, sky_tex_id, blend, fc_to_use,
                           fd_to_use); // Top Fade
            RenderSkySlice(solid_sky_h, -solid_sky_h, 1.0f, 1.0f, dist, tx, ty, offx, offy, sky_tex_id, blend,
                           fc_to_use,
                           fd_to_use); // Middle Solid
            RenderSkySlice(-solid_sky_h, -sky_h_ratio, 1.0f, 0.0f, dist, tx, ty, offx, offy, sky_tex_id, blend,
                           fc_to_use,
                           fd_to_use); // Bottom Fade
        }
        else
        {
            RenderSkySlice(1.0f, 0.75f, 0.0f, 1.0f, dist, tx, ty, offx, offy, sky_tex_id, blend, fc_to_use,
                           fd_to_use); // Top Fade
            RenderSkySlice(0.75f, -0.75f, 1.0f, 1.0f, dist, tx, ty, offx, offy, sky_tex_id, blend, fc_to_use,
                           fd_to_use); // Middle Solid
            RenderSkySlice(-0.75f, -1.0f, 1.0f, 0.0f, dist, tx, ty, offx, offy, sky_tex_id, blend, fc_to_use,
                           fd_to_use); // Bottom Fade
        }
    }
    else if (current_sky_stretch == kSkyStretchStretch)
    {
        if (sky_image->ScaledHeight() > 128)
        {
            ty = ((float)sky_image->ScaledHeight() / 256.0f);
            RenderSkySlice(sky_h_ratio, solid_sky_h, 0.0f, 1.0f, dist, tx, ty, offx, offy, sky_tex_id, blend, fc_to_use,
                           fd_to_use); // Top Fade
            RenderSkySlice(solid_sky_h, -solid_sky_h, 1.0f, 1.0f, dist, tx, ty, offx, offy, sky_tex_id, blend,
                           fc_to_use,
                           fd_to_use); // Middle Solid
            RenderSkySlice(-solid_sky_h, -sky_h_ratio, 1.0f, 0.0f, dist, tx, ty, offx, offy, sky_tex_id, blend,
                           fc_to_use,
                           fd_to_use); // Bottom Fade
        }
        else
        {
            ty = 1.0f;
            RenderSkySlice(1.0f, 0.75f, 0.0f, 1.0f, dist, tx, ty, offx, offy, sky_tex_id, blend, fc_to_use,
                           fd_to_use); // Top Fade
            RenderSkySlice(0.75f, -0.75f, 1.0f, 1.0f, dist, tx, ty, offx, offy, sky_tex_id, blend, fc_to_use,
                           fd_to_use); // Middle Solid
            RenderSkySlice(-0.75f, -1.0f, 1.0f, 0.0f, dist, tx, ty, offx, offy, sky_tex_id, blend, fc_to_use,
                           fd_to_use); // Bottom Fade
        }
    }
    else                               // Vanilla (or sane value if somehow this gets set out of expected
                                       // range)
    {
        if (sky_image->ScaledHeight() > 128)
        {
            RenderSkySlice(sky_h_ratio, solid_sky_h, 0.0f, 1.0f, dist / 2, tx, ty, offx, offy, sky_tex_id, blend,
                           fc_to_use,
                           fd_to_use);                   // Top Fade
            RenderSkySlice(solid_sky_h, sky_h_ratio - solid_sky_h, 1.0f, 1.0f, dist / 2, tx, ty, offx, offy, sky_tex_id,
                           blend, fc_to_use, fd_to_use); // Middle Solid
            RenderSkySlice(sky_h_ratio - solid_sky_h, 0.0f, 1.0f, 0.0f, dist / 2, tx, ty, offx, offy, sky_tex_id, blend,
                           fc_to_use,
                           fd_to_use);                   // Bottom Fade
        }
        else
        {
            ty *= 1.5f;
            RenderSkySlice(1.0f, 0.98f, 0.0f, 1.0f, dist / 3, tx, ty, offx, offy, sky_tex_id, blend, fc_to_use,
                           fd_to_use); // Top Fade
            RenderSkySlice(0.98f, 0.35f, 1.0f, 1.0f, dist / 3, tx, ty, offx, offy, sky_tex_id, blend, fc_to_use,
                           fd_to_use); // Middle Solid
            RenderSkySlice(0.35f, 0.33f, 1.0f, 0.0f, dist / 3, tx, ty, offx, offy, sky_tex_id, blend, fc_to_use,
                           fd_to_use); // Bottom Fade
        }
    }
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

    RGBAColor fc_to_use = current_map->outdoor_fog_color_;
    float     fd_to_use = 0.01f * current_map->outdoor_fog_density_;
    BlendingMode blend     = kBlendingNoZBuffer;
    // check for sector fog
    if (fc_to_use == kRGBANoValue)
    {
        fc_to_use = view_properties->fog_color;
        fd_to_use = view_properties->fog_density;
    }
    if (draw_culling.d_)
    {
        fc_to_use = kRGBANoValue;
        fd_to_use = 0.0f;
        blend     = (BlendingMode)(blend | kBlendingNoFog);
    }
    else if (fc_to_use != kRGBANoValue)
    {
#ifdef EDGE_SOKOL
        fd_to_use *= (current_sky_stretch == kSkyStretchVanilla ? 0.009f : 0.045f);
#else
        fd_to_use *= (current_sky_stretch == kSkyStretchVanilla ? 0.015f : 0.045f);
        //fd_to_use *= (current_sky_stretch == kSkyStretchVanilla ? 0.015f : 0.005f);
#endif
    }

    RGBAColor unit_col =
        epi::MakeRGBA((uint8_t)(render_view_red_multiplier * 255.0f), (uint8_t)(render_view_green_multiplier * 255.0f),
                      (uint8_t)(render_view_blue_multiplier * 255.0f));
    // top
    RendererVertex *glvert =
        BeginRenderUnit(GL_QUADS, 4, GL_MODULATE, fake_box[SK].texture[kSkyboxTop], (GLuint)kTextureEnvironmentDisable,
                        0, 0, blend, fc_to_use, fd_to_use);

    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{v0, v0}};
    glvert++->position             = {{-dist, dist, +dist}};
    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{v0, v1}};
    glvert++->position             = {{-dist, -dist, +dist}};
    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{v1, v1}};
    glvert++->position             = {{dist, -dist, +dist}};
    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{v1, v0}};
    glvert++->position             = {{dist, dist, +dist}};

    EndRenderUnit(4);

    // bottom
    glvert = BeginRenderUnit(GL_QUADS, 4, GL_MODULATE, fake_box[SK].texture[kSkyboxBottom],
                             (GLuint)kTextureEnvironmentDisable, 0, 0, blend, fc_to_use, fd_to_use);

    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{v0, v0}};
    glvert++->position             = {{-dist, -dist, -dist}};
    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{v0, v1}};
    glvert++->position             = {{-dist, dist, -dist}};
    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{v1, v1}};
    glvert++->position             = {{dist, dist, -dist}};
    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{v1, v0}};
    glvert++->position             = {{dist, -dist, -dist}};

    EndRenderUnit(4);

    // north
    glvert = BeginRenderUnit(GL_QUADS, 4, GL_MODULATE, fake_box[SK].texture[kSkyboxNorth],
                             (GLuint)kTextureEnvironmentDisable, 0, 0, blend, fc_to_use, fd_to_use);

    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{v0, v0}};
    glvert++->position             = {{-dist, dist, -dist}};
    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{v0, v1}};
    glvert++->position             = {{-dist, dist, +dist}};
    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{v1, v1}};
    glvert++->position             = {{dist, dist, +dist}};
    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{v1, v0}};
    glvert++->position             = {{dist, dist, -dist}};

    EndRenderUnit(4);

    // east
    glvert = BeginRenderUnit(GL_QUADS, 4, GL_MODULATE, fake_box[SK].texture[kSkyboxEast],
                             (GLuint)kTextureEnvironmentDisable, 0, 0, blend, fc_to_use, fd_to_use);

    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{v0, v0}};
    glvert++->position             = {{dist, dist, -dist}};
    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{v0, v1}};
    glvert++->position             = {{dist, dist, +dist}};
    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{v1, v1}};
    glvert++->position             = {{dist, -dist, +dist}};
    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{v1, v0}};
    glvert++->position             = {{dist, -dist, -dist}};

    EndRenderUnit(4);

    // south
    glvert = BeginRenderUnit(GL_QUADS, 4, GL_MODULATE, fake_box[SK].texture[kSkyboxSouth],
                             (GLuint)kTextureEnvironmentDisable, 0, 0, blend, fc_to_use, fd_to_use);

    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{v0, v0}};
    glvert++->position             = {{dist, -dist, -dist}};
    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{v0, v1}};
    glvert++->position             = {{dist, -dist, +dist}};
    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{v1, v1}};
    glvert++->position             = {{-dist, -dist, +dist}};
    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{v1, v0}};
    glvert++->position             = {{-dist, -dist, -dist}};

    EndRenderUnit(4);

    // west
    glvert = BeginRenderUnit(GL_QUADS, 4, GL_MODULATE, fake_box[SK].texture[kSkyboxWest],
                             (GLuint)kTextureEnvironmentDisable, 0, 0, blend, fc_to_use, fd_to_use);

    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{v0, v0}};
    glvert++->position             = {{-dist, -dist, -dist}};
    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{v0, v1}};
    glvert++->position             = {{-dist, -dist, +dist}};
    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{v1, v1}};
    glvert++->position             = {{-dist, dist, +dist}};
    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{v1, v0}};
    glvert++->position             = {{-dist, dist, -dist}};

    EndRenderUnit(4);
}

static void FinishSkyUnit(void)
{
    EndRenderUnit(total_sky_verts);
    FinishUnitBatch();
    sky_glvert       = nullptr;
    total_sky_verts  = 0;
    sky_unit_started = false;
}

void FlushSky(void)
{
    if (sky_unit_started)
        FinishSkyUnit();
}

void FinishSky(bool use_depth_mask)
{

    render_state->ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    if (!need_to_draw_sky)
        return;

    if (draw_culling.d_)
        render_state->Disable(GL_DEPTH_TEST);

    if (use_depth_mask)
        render_state->DepthFunction(GL_GREATER);
    else
        render_state->DepthMask(false);

    StartUnitBatch(false);

#ifdef APPLE_SILICON
    uint8_t old_dumb_clamp = renderer_dumb_clamp.d_;
    renderer_dumb_clamp    = 1;
#endif

    if (custom_skybox)
        RenderSkybox();
    else
        RenderSkyCylinder();

    FinishUnitBatch();

    RendererRevertSkyMatrices();

#ifdef APPLE_SILICON
    renderer_dumb_clamp = old_dumb_clamp;
#endif

    if (draw_culling.d_)
        render_state->Enable(GL_DEPTH_TEST);

    if (use_depth_mask)
        render_state->DepthFunction(GL_LEQUAL);
    else
        render_state->DepthMask(true);
}

void RenderSkyPlane(Subsector *sub, float h)
{
    need_to_draw_sky = true;

    Seg *seg = sub->segs;
    if (!seg)
        return;

    float x0 = seg->vertex_1->X;
    float y0 = seg->vertex_1->Y;
    render_mirror_set.Coordinate(x0, y0);
    seg = seg->subsector_next;
    if (!seg)
        return;

    float x1 = seg->vertex_1->X;
    float y1 = seg->vertex_1->Y;
    render_mirror_set.Coordinate(x1, y1);
    seg = seg->subsector_next;
    if (!seg)
        return;

    if (!sky_unit_started)
        BeginSkyUnit();

    render_mirror_set.Height(h);
    RGBAColor unit_col = kRGBAWhite;

    while (seg)
    {
        float x2 = seg->vertex_1->X;
        float y2 = seg->vertex_1->Y;
        render_mirror_set.Coordinate(x2, y2);

        sky_glvert->rgba       = unit_col;
        sky_glvert++->position = {{x0, y0, h}};
        sky_glvert->rgba       = unit_col;
        sky_glvert++->position = {{x1, y1, h}};
        sky_glvert->rgba       = unit_col;
        sky_glvert++->position = {{x2, y2, h}};

        total_sky_verts += 3;

        x1  = x2;
        y1  = y2;
        seg = seg->subsector_next;
    }

    // Break up large batches
    if (total_sky_verts > kMaximumLocalVertices / 4)
    {
        EndRenderUnit(total_sky_verts);
        FinishUnitBatch();
        StartUnitBatch(false);
        sky_glvert      = BeginRenderUnit(GL_TRIANGLES, kMaximumLocalVertices, GL_MODULATE, 0,
                                          (GLuint)kTextureEnvironmentDisable, 0, 0, kBlendingNone);
        total_sky_verts = 0;
    }
}

void RenderSkyWall(Seg *seg, float h1, float h2)
{
    need_to_draw_sky = true;

    if (!sky_unit_started)
        BeginSkyUnit();

    float x1 = seg->vertex_1->X;
    float y1 = seg->vertex_1->Y;
    float x2 = seg->vertex_2->X;
    float y2 = seg->vertex_2->Y;

    render_mirror_set.Coordinate(x1, y1);
    render_mirror_set.Coordinate(x2, y2);

    render_mirror_set.Height(h1);
    render_mirror_set.Height(h2);

    RGBAColor unit_col = kRGBAWhite;

    sky_glvert->rgba       = unit_col;
    sky_glvert++->position = {{x1, y1, h1}};
    sky_glvert->rgba       = unit_col;
    sky_glvert++->position = {{x1, y1, h2}};
    sky_glvert->rgba       = unit_col;
    sky_glvert++->position = {{x2, y2, h2}};
    sky_glvert->rgba       = unit_col;
    sky_glvert++->position = {{x2, y2, h1}};
    sky_glvert->rgba       = unit_col;
    sky_glvert++->position = {{x2, y2, h2}};
    sky_glvert->rgba       = unit_col;
    sky_glvert++->position = {{x1, y1, h1}};

    total_sky_verts += 6;

    // Break up large batches
    if (total_sky_verts > kMaximumLocalVertices / 4)
    {
        EndRenderUnit(total_sky_verts);
        FinishUnitBatch();
        StartUnitBatch(false);
        sky_glvert      = BeginRenderUnit(GL_TRIANGLES, kMaximumLocalVertices, GL_MODULATE, 0,
                                          (GLuint)kTextureEnvironmentDisable, 0, 0, kBlendingNone);
        total_sky_verts = 0;
    }
}

//----------------------------------------------------------------------------

static const char *UserSkyFaceName(const char *base, int face)
{
    static char       buffer[64];
    static const char letters[] = "NESWTB";

    stbsp_sprintf(buffer, "%s_%c", base, letters[face]);
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
    const uint8_t *what_palette = nullptr;
    if (sky_image->source_palette_ >= 0)
        what_palette = (const uint8_t *)LoadLumpIntoMemory(sky_image->source_palette_);
    ImageData *tmp_img_data = ReadAsEpiBlock((Image *)sky_image);
    if (tmp_img_data->depth_ == 1)
    {
        ImageData *rgb_img_data = RGBFromPalettised(
            tmp_img_data, what_palette ? what_palette : (const uint8_t *)&playpal_data[0], sky_image->opacity_);
        delete tmp_img_data;
        tmp_img_data = rgb_img_data;
    }
    culling_fog_color = tmp_img_data->AverageColor(0, sky_image->width_, 0, sky_image->height_ / 2);
    sky_cap_color = tmp_img_data->AverageColor(0, sky_image->width_, sky_image->height_ * 3 / 4, sky_image->height_);
    delete tmp_img_data;

    if (what_palette)
        delete[] what_palette;

    if (info->face[kSkyboxNorth])
    {
        custom_skybox = true;

        info->face_size = info->face[kSkyboxNorth]->width_;

        for (int i = kSkyboxEast; i < 6; i++)
            info->face[i] = ImageLookup(UserSkyFaceName(sky_image->name_.c_str(), i), kImageNamespaceTexture);

        for (int k = 0; k < 6; k++)
            info->texture[k] = ImageCache(info->face[k], true, render_view_effect_colormap);

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

void ShutdownSky(void)
{
    sky_ref            = nullptr;
    ddf_scroll_tic     = -1;
    ddf_sky_scroll     = {{0, 0}};
    ddf_old_sky_scroll = {{0, 0}};
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
