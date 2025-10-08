//----------------------------------------------------------------------------
//  EDGE OpenGL Rendering (BSP Traversal)
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
//
//  Based on the DOOM source code, released by Id Software under the
//  following copyright:
//
//    Copyright (C) 1993-1996 by id Software, Inc.
//
//----------------------------------------------------------------------------

#include "r_render.h"

#include <math.h>

#include <unordered_map>
#include <unordered_set>

#include "AlmostEquals.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "edge_profiling.h"
#include "epi.h"
#include "epi_doomdefs.h"
#include "g_game.h"
#include "i_defs_gl.h"
#include "m_bbox.h"
#include "n_network.h" // NetworkUpdate
#include "p_local.h"
#include "p_spec.h"
#include "p_tick.h"
#include "r_backend.h"
#include "r_colormap.h"
#include "r_defs.h"
#include "r_effects.h"
#include "r_gldefs.h"
#include "r_image.h"
#include "r_mirror.h"
#include "r_misc.h"
#include "r_modes.h"
#include "r_occlude.h"
#include "r_shader.h"
#include "r_sky.h"
#include "r_state.h"
#include "r_things.h"
#include "r_units.h"

extern std::vector<PlaneMover *> active_planes;
std::unordered_set<Line *>       newly_seen_lines;

extern ConsoleVariable draw_culling;

extern MapObject      *view_camera_map_object;
extern ConsoleVariable debug_hall_of_mirrors;

extern float sprite_skew;

extern ViewHeightZone view_height_zone;

static Subsector     *current_subsector;
static DrawSubsector *current_draw_subsector;
static Seg           *current_seg;

extern unsigned int root_node;

EDGE_DEFINE_CONSOLE_VARIABLE(default_lighting, "1", kConsoleVariableFlagArchive)

bool solid_mode;
int  detail_level       = 1;
int  use_dynamic_lights = 0;

float view_x_slope;
float view_y_slope;
float widescreen_view_width_multiplier;

std::unordered_set<AbstractShader *> seen_dynamic_lights;

static constexpr float kDoomYSlope     = 0.525f;
static constexpr float kDoomYSlopeFull = 0.625f;

static constexpr uint8_t kMaximumEdgeVertices = 20;

static constexpr float kWavetableIncrement = 0.0009765625f;

static Sector *front_sector;
static Sector *back_sector;

static int  swirl_pass   = 0;
static bool thick_liquid = false;

static float wave_now;    // value for doing wave table lookups
static float plane_z_bob; // for floor/ceiling bob DDFSECT stuff

MirrorSet render_mirror_set(kMirrorSetRender);

#ifndef EDGE_SOKOL
extern std::list<DrawSubsector *> draw_subsector_list;
#else
// Sky items from previous frame, delayed a frame so can render the BSP as we traverse it
static std::list<RenderItem *> deferred_sky_items;
#endif

static void EmulateFloodPlane(const DrawFloor *dfloor, const Sector *flood_ref, int face_dir, float h1, float h2);

inline BlendingMode GetSurfaceBlending(float alpha, ImageOpacity opacity)
{
    BlendingMode blending;

    if (alpha >= 0.99f && opacity == kOpacitySolid)
        blending = kBlendingNone;
    else if (alpha < 0.11f || opacity == kOpacityComplex)
        blending = kBlendingMasked;
    else
        blending = kBlendingLess;

    if (alpha < 0.99f || opacity == kOpacityComplex)
        blending = (BlendingMode)(blending | kBlendingAlpha);

    return blending;
}

static float Slope_GetHeight(SlopePlane *slope, float x, float y)
{
    // FIXME: precompute (store in slope_plane_t)
    float dx = slope->x2 - slope->x1;
    float dy = slope->y2 - slope->y1;

    float d_len = dx * dx + dy * dy;

    float along = ((x - slope->x1) * dx + (y - slope->y1) * dy) / d_len;

    return slope->delta_z1 + along * (slope->delta_z2 - slope->delta_z1);
}

// Adapted from Quake 3 GPL release - Dasho
static void CalcTurbulentTexCoords(HMM_Vec2 *texc, HMM_Vec3 *pos)
{
    float amplitude = 0.05;
    float now       = wave_now * (thick_liquid ? 0.5 : 1.0);

    if (swirling_flats == kLiquidSwirlParallax)
    {
        if (thick_liquid)
        {
            if (swirl_pass == 1)
            {
                texc->X = texc->X + sine_table[(int)(((pos->X + pos->Z) * kWavetableIncrement + now) * kSineTableSize) &
                                               (kSineTableMask)] *
                                        amplitude;
                texc->Y = texc->Y +
                          sine_table[(int)((pos->Y * kWavetableIncrement + now) * kSineTableSize) & (kSineTableMask)] *
                              amplitude;
            }
            else
            {
                amplitude = 0;
                texc->X = texc->X - sine_table[(int)(((pos->X + pos->Z) * kWavetableIncrement + now) * kSineTableSize) &
                                               (kSineTableMask)] *
                                        amplitude;
                texc->Y = texc->Y -
                          sine_table[(int)((pos->Y * kWavetableIncrement + now) * kSineTableSize) & (kSineTableMask)] *
                              amplitude;
            }
        }
        else
        {
            if (swirl_pass == 1)
            {
                amplitude = 0.025;
                texc->X = texc->X + sine_table[(int)(((pos->X + pos->Z) * kWavetableIncrement + now) * kSineTableSize) &
                                               (kSineTableMask)] *
                                        amplitude;
                texc->Y = texc->Y +
                          sine_table[(int)((pos->Y * kWavetableIncrement + now) * kSineTableSize) & (kSineTableMask)] *
                              amplitude;
            }
            else
            {
                amplitude = 0.015;
                texc->X = texc->X - sine_table[(int)(((pos->X + pos->Z) * kWavetableIncrement + now) * kSineTableSize) &
                                               (kSineTableMask)] *
                                        amplitude;
                texc->Y = texc->Y -
                          sine_table[(int)((pos->Y * kWavetableIncrement + now) * kSineTableSize) & (kSineTableMask)] *
                              amplitude;
            }
        }
    }
    else
    {
        texc->X =
            texc->X +
            sine_table[(int)(((pos->X + pos->Z) * kWavetableIncrement + now) * kSineTableSize) & (kSineTableMask)] *
                amplitude;
        texc->Y =
            texc->Y +
            sine_table[(int)((pos->Y * kWavetableIncrement + now) * kSineTableSize) & (kSineTableMask)] * amplitude;
    }
}

struct WallCoordinateData
{
    int             v_count;
    const HMM_Vec3 *vertices;

    GLuint tex_id;

    int          pass;
    BlendingMode blending;

    uint8_t R, G, B;
    uint8_t trans;

    DividingLine div;

    float tx0, ty0;
    float tx_mul, ty_mul;

    HMM_Vec3 normal;

    bool mid_masked;
};

static void WallCoordFunc(void *d, int v_idx, HMM_Vec3 *pos, RGBAColor *rgb, HMM_Vec2 *texc, HMM_Vec3 *normal,
                          HMM_Vec3 *lit_pos)
{
    const WallCoordinateData *data = (WallCoordinateData *)d;

    *pos    = data->vertices[v_idx];
    *normal = data->normal;

    if (swirl_pass > 1)
    {
        *rgb = epi::MakeRGBA((uint8_t)(255.0f / data->R * render_view_red_multiplier),
                             (uint8_t)(255.0f / data->G * render_view_green_multiplier),
                             (uint8_t)(255.0f / data->B * render_view_blue_multiplier), epi::GetRGBAAlpha(*rgb));
    }
    else
    {
        *rgb = epi::MakeRGBA((uint8_t)(data->R * render_view_red_multiplier),
                             (uint8_t)(data->G * render_view_green_multiplier),
                             (uint8_t)(data->B * render_view_blue_multiplier), epi::GetRGBAAlpha(*rgb));
    }

    float along;

    if (fabs(data->div.delta_x) > fabs(data->div.delta_y))
    {
        along = (pos->X - data->div.x) / data->div.delta_x;
    }
    else
    {
        along = (pos->Y - data->div.y) / data->div.delta_y;
    }

    texc->X = data->tx0 + along * data->tx_mul;
    texc->Y = data->ty0 + pos->Z * data->ty_mul;

    if (swirl_pass > 0)
        CalcTurbulentTexCoords(texc, pos);

    *lit_pos = *pos;
}

struct PlaneCoordinateData
{
    int             v_count;
    const HMM_Vec3 *vertices;

    GLuint tex_id;

    int          pass;
    BlendingMode blending;

    float R, G, B;
    float trans;

    float tx0, ty0;
    float image_w, image_h;

    HMM_Vec2 x_mat;
    HMM_Vec2 y_mat;

    HMM_Vec3 normal;

    // multiplier for plane_z_bob
    float bob_amount = 0;

    SlopePlane *slope;

    BAMAngle rotation = 0;
};

static void PlaneCoordFunc(void *d, int v_idx, HMM_Vec3 *pos, RGBAColor *rgb, HMM_Vec2 *texc, HMM_Vec3 *normal,
                           HMM_Vec3 *lit_pos)
{
    PlaneCoordinateData *data = (PlaneCoordinateData *)d;

    *pos    = data->vertices[v_idx];
    *normal = data->normal;

    if (swirl_pass > 1)
    {
        *rgb = epi::MakeRGBA((uint8_t)(255.0f / data->R * render_view_red_multiplier),
                             (uint8_t)(255.0f / data->G * render_view_green_multiplier),
                             (uint8_t)(255.0f / data->B * render_view_blue_multiplier), epi::GetRGBAAlpha(*rgb));
    }
    else
    {
        *rgb = epi::MakeRGBA((uint8_t)(data->R * render_view_red_multiplier),
                             (uint8_t)(data->G * render_view_green_multiplier),
                             (uint8_t)(data->B * render_view_blue_multiplier), epi::GetRGBAAlpha(*rgb));
    }

    HMM_Vec2 rxy = {{(data->tx0 + pos->X), (data->ty0 + pos->Y)}};

    if (data->rotation)
        rxy = HMM_RotateV2(rxy, epi::RadiansFromBAM(data->rotation));

    rxy.X /= data->image_w;
    rxy.Y /= data->image_h;

    texc->X = rxy.X * data->x_mat.X + rxy.Y * data->x_mat.Y;
    texc->Y = rxy.X * data->y_mat.X + rxy.Y * data->y_mat.Y;

    if (swirl_pass > 0)
        CalcTurbulentTexCoords(texc, pos);

    if (data->bob_amount > 0)
        pos->Z += (plane_z_bob * data->bob_amount);

    *lit_pos = *pos;
}

static void DLIT_Wall(MapObject *mo, void *dataptr)
{
    WallCoordinateData *data = (WallCoordinateData *)dataptr;

    // light behind the plane ?
    if (!mo->info_->dlight_.leaky_ && !data->mid_masked &&
        !(mo->subsector_->sector->floor_vertex_slope || mo->subsector_->sector->ceiling_vertex_slope))
    {
        float mx = mo->x;
        float my = mo->y;

        render_mirror_set.Coordinate(mx, my);

        float dist = (mx - data->div.x) * data->div.delta_y - (my - data->div.y) * data->div.delta_x;

        if (dist < 0)
            return;
    }

    EPI_ASSERT(mo->dynamic_light_.shader);

    BlendingMode blending = (BlendingMode)((data->blending & ~kBlendingAlpha) | kBlendingAdd);

    mo->dynamic_light_.shader->WorldMix(GL_POLYGON, data->v_count, data->tex_id, data->trans, &data->pass, blending,
                                        data->mid_masked, data, WallCoordFunc);
}

static void GLOWLIT_Wall(MapObject *mo, void *dataptr)
{
    WallCoordinateData *data = (WallCoordinateData *)dataptr;

    EPI_ASSERT(mo->dynamic_light_.shader);

    BlendingMode blending = (BlendingMode)((data->blending & ~kBlendingAlpha) | kBlendingAdd);

    mo->dynamic_light_.shader->WorldMix(GL_POLYGON, data->v_count, data->tex_id, data->trans, &data->pass, blending,
                                        data->mid_masked, data, WallCoordFunc);
}

static void DLIT_Plane(MapObject *mo, void *dataptr)
{
    PlaneCoordinateData *data = (PlaneCoordinateData *)dataptr;

    // light behind the plane ?
    if (!mo->info_->dlight_.leaky_ &&
        !(mo->subsector_->sector->floor_vertex_slope || mo->subsector_->sector->ceiling_vertex_slope))
    {
        float z = data->vertices[0].Z;

        if (data->slope)
            z += Slope_GetHeight(data->slope, mo->x, mo->y);

        if ((MapObjectMidZ(mo) > z) != (data->normal.Z > 0))
            return;
    }

    // NOTE: distance already checked in DynamicLightIterator

    EPI_ASSERT(mo->dynamic_light_.shader);

    BlendingMode blending = (BlendingMode)((data->blending & ~kBlendingAlpha) | kBlendingAdd);

    mo->dynamic_light_.shader->WorldMix(GL_POLYGON, data->v_count, data->tex_id, data->trans, &data->pass, blending,
                                        false /* masked */, data, PlaneCoordFunc);
}

static void GLOWLIT_Plane(MapObject *mo, void *dataptr)
{
    PlaneCoordinateData *data = (PlaneCoordinateData *)dataptr;

    EPI_ASSERT(mo->dynamic_light_.shader);

    BlendingMode blending = (BlendingMode)((data->blending & ~kBlendingAlpha) | kBlendingAdd);

    mo->dynamic_light_.shader->WorldMix(GL_POLYGON, data->v_count, data->tex_id, data->trans, &data->pass, blending,
                                        false, data, PlaneCoordFunc);
}

static inline void GreetNeighbourSector(float *hts, int &num, VertexSectorList *seclist)
{
    if (!seclist)
        return;

    for (int k = 0; k < (seclist->total * 2); k++)
    {
        Sector *sec = level_sectors + seclist->sectors[k / 2];

        float h = (k & 1) ? sec->interpolated_ceiling_height : sec->interpolated_floor_height;

        // does not intersect current height range?
        if (h <= hts[0] + 0.1 || h >= hts[num - 1] - 0.1)
            continue;

        // check if new height already present, and at same time
        // find place to insert new height.

        int pos;

        for (pos = 1; pos < num; pos++)
        {
            if (h < hts[pos] - 0.1)
                break;

            if (h < hts[pos] + 0.1)
            {
                pos = -1; // already present
                break;
            }
        }

        if (pos > 0 && pos < num)
        {
            for (int i = num; i > pos; i--)
                hts[i] = hts[i - 1];

            hts[pos] = h;

            num++;

            if (num >= kMaximumEdgeVertices)
                return;
        }
    }
}

enum WallTileFlag
{
    kWallTileIsExtra = (1 << 0),
    kWallTileExtraX  = (1 << 1), // side of an extrafloor
    kWallTileExtraY  = (1 << 2), //
    kWallTileMidMask = (1 << 4), // the mid-masked part (gratings etc)
};

static void DrawWallPart(DrawFloor *dfloor, float x1, float y1, float lz1, float lz2, float x2, float y2, float rz1,
                         float rz2, float tex_top_h, MapSurface *surf, const Image *image, bool mid_masked, bool opaque,
                         float tex_x1, float tex_x2, RegionProperties *props = nullptr)
{
    // Note: tex_x1 and tex_x2 are in world coordinates.
    //       top, bottom and tex_top_h as well.

    ec_frame_stats.draw_wall_parts++;

    EPI_UNUSED(opaque);

    if (surf->override_properties)
        props = surf->override_properties;

    if (!props)
        props = dfloor->properties;

    const float trans = surf->translucency;

    EPI_ASSERT(image);

    // (need to load the image to know the opacity)
    GLuint tex_id = ImageCache(image, true, render_view_effect_colormap);

    BlendingMode blending = GetSurfaceBlending(trans, (ImageOpacity)image->opacity_);

    // ignore non-solid walls in solid mode (& vice versa)
    if ((solid_mode && (blending & kBlendingAlpha)) || (!solid_mode && !(blending & kBlendingAlpha)))
    {
        if (solid_mode)
        {
            current_draw_subsector->solid = false;
        }

        return;
    }

    // must determine bbox _before_ mirror flipping
    float v_bbox[4];

    BoundingBoxClear(v_bbox);
    BoundingBoxAddPoint(v_bbox, x1, y1);
    BoundingBoxAddPoint(v_bbox, x2, y2);

    render_mirror_set.Coordinate(x1, y1);
    render_mirror_set.Coordinate(x2, y2);

    if (render_mirror_set.Reflective())
    {
        float tmp_x = x1;
        x1          = x2;
        x2          = tmp_x;
        float tmp_y = y1;
        y1          = y2;
        y2          = tmp_y;

        tmp_x  = tex_x1;
        tex_x1 = tex_x2;
        tex_x2 = tmp_x;
    }

    EPI_ASSERT(current_map);

    int lit_adjust = 0;

    // do the N/S/W/E bizzo...
    if ((current_map->episode_->lighting_ == kLightingModelDoom || default_lighting.d_ == kLightingModelDoom) && props->light_level > 0)
    {
        if (AlmostEquals(current_seg->vertex_1->Y, current_seg->vertex_2->Y))
            lit_adjust -= 16;
        else if (AlmostEquals(current_seg->vertex_1->X, current_seg->vertex_2->X))
            lit_adjust += 16;
    }

    float total_w = image->ScaledWidth();
    float total_h = image->ScaledHeight();

    /* convert tex_x1 and tex_x2 from world coords to texture coords */
    tex_x1 = (tex_x1 * surf->x_matrix.X) / total_w;
    tex_x2 = (tex_x2 * surf->x_matrix.X) / total_w;

    float tx0    = tex_x1;
    float tx_mul = tex_x2 - tex_x1;

    render_mirror_set.Height(tex_top_h);

    float ty_mul = surf->y_matrix.Y / (total_h * render_mirror_set.ZScale());
    float ty0    = 1.0f - tex_top_h * ty_mul;

#if (DEBUG >= 3)
    LogDebug("WALL (%d,%d,%d) -> (%d,%d,%d)\n", (int)x1, (int)y1, (int)top, (int)x2, (int)y2, (int)bottom);
#endif

    // -AJA- 2007/08/07: ugly code here ensures polygon edges
    //       match up with adjacent linedefs (otherwise small
    //       gaps can appear which look bad).

    float left_h[kMaximumEdgeVertices];
    int   left_num = 2;
    float right_h[kMaximumEdgeVertices];
    int   right_num = 2;

    left_h[0]  = lz1;
    left_h[1]  = lz2;
    right_h[0] = rz1;
    right_h[1] = rz2;

    if (solid_mode && !mid_masked)
    {
        GreetNeighbourSector(left_h, left_num, current_seg->vertex_sectors[0]);
        GreetNeighbourSector(right_h, right_num, current_seg->vertex_sectors[1]);
    }

    HMM_Vec3 vertices[kMaximumEdgeVertices * 2];

    int v_count = 0;

    for (int LI = 0; LI < left_num; LI++)
    {
        vertices[v_count].X = x1;
        vertices[v_count].Y = y1;
        vertices[v_count].Z = left_h[LI];

        render_mirror_set.Height(vertices[v_count].Z);

        v_count++;
    }

    for (int RI = right_num - 1; RI >= 0; RI--)
    {
        vertices[v_count].X = x2;
        vertices[v_count].Y = y2;
        vertices[v_count].Z = right_h[RI];

        render_mirror_set.Height(vertices[v_count].Z);

        v_count++;
    }

    // -AJA- 2006-06-22: fix for midmask wrapping bug
    if (mid_masked &&
        (!current_seg->linedef->special || AlmostEquals(current_seg->linedef->special->s_yspeed_,
                                                        0.0f))) // Allow vertical scroller midmasks - Dasho
        blending = (BlendingMode)(blending | kBlendingClampY);

    WallCoordinateData data;

    data.v_count  = v_count;
    data.vertices = vertices;

    data.R = data.G = data.B = 255;

    data.div.x       = x1;
    data.div.y       = y1;
    data.div.delta_x = x2 - x1;
    data.div.delta_y = y2 - y1;

    data.tx0    = tx0;
    data.ty0    = ty0;
    data.tx_mul = tx_mul;
    data.ty_mul = ty_mul;

    data.normal = {{(y2 - y1), (x1 - x2), 0}};

    data.tex_id     = tex_id;
    data.pass       = 0;
    data.blending   = blending;
    data.trans      = trans;
    data.mid_masked = mid_masked;

    if (surf->image && surf->image->liquid_type_ == kLiquidImageThick)
        thick_liquid = true;
    else
        thick_liquid = false;

    if (surf->image && surf->image->liquid_type_ > kLiquidImageNone && swirling_flats > kLiquidSwirlSmmu)
        swirl_pass = 1;

    AbstractShader *cmap_shader = GetColormapShader(props, lit_adjust, current_subsector->sector);

    cmap_shader->WorldMix(GL_POLYGON, data.v_count, data.tex_id, trans, &data.pass, data.blending, data.mid_masked,
                          &data, WallCoordFunc);

    if (surf->image && surf->image->liquid_type_ > kLiquidImageNone && swirling_flats == kLiquidSwirlParallax)
    {
        data.tx0               = data.tx0 + 25;
        data.ty0               = data.ty0 + 25;
        swirl_pass             = 2;
        BlendingMode old_blend = data.blending;
        float        old_dt    = data.trans;
        data.blending          = (BlendingMode)(kBlendingMasked | kBlendingAlpha);
        data.trans             = 85;
        cmap_shader->WorldMix(GL_POLYGON, data.v_count, data.tex_id, 0.33f, &data.pass, data.blending, false, &data,
                              WallCoordFunc);
        data.blending = old_blend;
        data.trans    = old_dt;
    }

    if (use_dynamic_lights && render_view_extra_light < 250)
    {
        float bottom = HMM_MIN(lz1, rz1);
        float top    = HMM_MAX(lz2, rz2);

        DynamicLightIterator(v_bbox[kBoundingBoxLeft], v_bbox[kBoundingBoxBottom], bottom, v_bbox[kBoundingBoxRight],
                             v_bbox[kBoundingBoxTop], top, DLIT_Wall, &data);

        SectorGlowIterator(current_seg->front_sector, v_bbox[kBoundingBoxLeft], v_bbox[kBoundingBoxBottom], bottom,
                           v_bbox[kBoundingBoxRight], v_bbox[kBoundingBoxTop], top, GLOWLIT_Wall, &data);
    }

    swirl_pass = 0;
}

static void DrawSlidingDoor(DrawFloor *dfloor, float c, float f, float tex_top_h, MapSurface *surf, bool opaque,
                            float x_offset)
{
    /* smov may be nullptr */
    SlidingDoorMover *smov = current_seg->linedef->slider_move;

    float opening = 0;

    if (smov)
    {
        if (!console_active && !menu_active && !paused && !time_stop_active && !erraticism_active && !rts_menu_active)
            opening = HMM_Lerp(smov->old_opening, fractional_tic, smov->opening);
        else
            opening = smov->opening;
    }

    Line *ld = current_seg->linedef;

    /// float im_width = wt->surface->image->ScaledWidth();

    int num_parts = 1;
    if (current_seg->linedef->slide_door->s_.type_ == kSlidingDoorTypeCenter)
        num_parts = 2;

    // extent of current seg along the linedef
    float s_seg, e_seg;

    if (current_seg->side == 0)
    {
        s_seg = current_seg->offset;
        e_seg = s_seg + current_seg->length;
    }
    else
    {
        e_seg = ld->length - current_seg->offset;
        s_seg = e_seg - current_seg->length;
    }

    for (int part = 0; part < num_parts; part++)
    {
        // coordinates along the linedef (0.00 at V1, 1.00 at V2)
        float s_along, s_tex;
        float e_along, e_tex;

        switch (current_seg->linedef->slide_door->s_.type_)
        {
        case kSlidingDoorTypeLeft:
            s_along = 0;
            e_along = ld->length - opening;

            s_tex = -e_along;
            e_tex = 0;
            break;

        case kSlidingDoorTypeRight:
            s_along = opening;
            e_along = ld->length;

            s_tex = 0;
            e_tex = e_along - s_along;
            break;

        case kSlidingDoorTypeCenter:
            if (part == 0)
            {
                s_along = 0;
                e_along = (ld->length - opening) / 2;

                e_tex = ld->length / 2;
                s_tex = e_tex - (e_along - s_along);
            }
            else
            {
                s_along = (ld->length + opening) / 2;
                e_along = ld->length;

                s_tex = ld->length / 2;
                e_tex = s_tex + (e_along - s_along);
            }
            break;

        default:
            FatalError("INTERNAL ERROR: unknown slidemove type!\n");
        }

        // limit sliding door coordinates to current seg
        if (s_along < s_seg)
        {
            s_tex += (s_seg - s_along);
            s_along = s_seg;
        }
        if (e_along > e_seg)
        {
            e_tex += (e_seg - e_along);
            e_along = e_seg;
        }

        if (s_along >= e_along)
            continue;

        float x1 = ld->vertex_1->X + ld->delta_x * s_along / ld->length;
        float y1 = ld->vertex_1->Y + ld->delta_y * s_along / ld->length;

        float x2 = ld->vertex_1->X + ld->delta_x * e_along / ld->length;
        float y2 = ld->vertex_1->Y + ld->delta_y * e_along / ld->length;

        s_tex += x_offset;
        e_tex += x_offset;

        DrawWallPart(dfloor, x1, y1, f, c, x2, y2, f, c, tex_top_h, surf, surf->image, true, opaque, s_tex, e_tex);
    }
}

// Mirror the texture on the back of the line
static void DrawGlass(DrawFloor *dfloor, float c, float f, float tex_top_h, MapSurface *surf, bool opaque,
                      float x_offset)
{
    Line *ld = current_seg->linedef;

    // extent of current seg along the linedef
    float s_seg, e_seg;

    if (current_seg->side == 0)
    {
        s_seg = current_seg->offset;
        e_seg = s_seg + current_seg->length;
    }
    else
    {
        e_seg = ld->length - current_seg->offset;
        s_seg = e_seg - current_seg->length;
    }

    // coordinates along the linedef (0.00 at V1, 1.00 at V2)
    float s_along, s_tex;
    float e_along, e_tex;

    s_along = 0;
    e_along = ld->length - 0;

    s_tex = -e_along;
    e_tex = 0;

    // limit glass coordinates to current seg
    if (s_along < s_seg)
    {
        s_tex += (s_seg - s_along);
        s_along = s_seg;
    }
    if (e_along > e_seg)
    {
        e_tex += (e_seg - e_along);
        e_along = e_seg;
    }

    if (s_along < e_along)
    {
        float x1 = ld->vertex_1->X + ld->delta_x * s_along / ld->length;
        float y1 = ld->vertex_1->Y + ld->delta_y * s_along / ld->length;

        float x2 = ld->vertex_1->X + ld->delta_x * e_along / ld->length;
        float y2 = ld->vertex_1->Y + ld->delta_y * e_along / ld->length;

        s_tex += x_offset;
        e_tex += x_offset;

        DrawWallPart(dfloor, x1, y1, f, c, x2, y2, f, c, tex_top_h, surf, surf->image, true, opaque, s_tex, e_tex);
    }
}

static void DrawTile(Seg *seg, DrawFloor *dfloor, float lz1, float lz2, float rz1, float rz2, float tex_z, int flags,
                     MapSurface *surf)
{
    EDGE_ZoneScoped;

    // tex_z = texturing top, in world coordinates

    const Image *image = surf->image;

    if (!image)
        image = ImageForHomDetect();

    float offx, offy;

    if (!AlmostEquals(surf->old_offset.X, surf->offset.X) && !console_active && !paused && !menu_active &&
        !time_stop_active && !erraticism_active)
        offx = fmod(HMM_Lerp(surf->old_offset.X, fractional_tic, surf->offset.X), surf->image->width_);
    else
        offx = surf->offset.X;
    if (!AlmostEquals(surf->old_offset.Y, surf->offset.Y) && !console_active && !paused && !menu_active &&
        !time_stop_active && !erraticism_active)
        offy = fmod(HMM_Lerp(surf->old_offset.Y, fractional_tic, surf->offset.Y), surf->image->height_);
    else
        offy = surf->offset.Y;

    float tex_top_h = tex_z + offy;
    float x_offset  = offx;

    if (flags & kWallTileExtraX)
    {
        x_offset += seg->sidedef->middle.offset.X;
    }
    if (flags & kWallTileExtraY)
    {
        // needed separate Y flag to maintain compatibility
        tex_top_h += seg->sidedef->middle.offset.Y;
    }

    int32_t blending = GetSurfaceBlending(surf->translucency, (ImageOpacity)image->opacity_);
    bool    opaque   = !seg->back_sector || !(blending & kBlendingAlpha);

    // check for horizontal sliders
    if ((flags & kWallTileMidMask) && seg->linedef->slide_door)
    {
        if (surf->image)
            DrawSlidingDoor(dfloor, lz2, lz1, tex_top_h, surf, opaque, x_offset);
        return;
    }

    // check for breakable glass
    if (seg->linedef->special)
    {
        if ((flags & kWallTileMidMask) && seg->linedef->special->glass_)
        {
            if (surf->image)
                DrawGlass(dfloor, lz2, lz1, tex_top_h, surf, opaque, x_offset);
            return;
        }
    }

    float x1 = seg->vertex_1->X;
    float y1 = seg->vertex_1->Y;
    float x2 = seg->vertex_2->X;
    float y2 = seg->vertex_2->Y;

    float tex_x1 = seg->offset;
    float tex_x2 = tex_x1 + seg->length;

    tex_x1 += x_offset;
    tex_x2 += x_offset;

    if (seg->sidedef->sector->properties.special && seg->sidedef->sector->properties.special->floor_bob_ > 0)
    {
        lz1 -= seg->sidedef->sector->properties.special->floor_bob_;
        rz1 -= seg->sidedef->sector->properties.special->floor_bob_;
    }

    if (seg->sidedef->sector->properties.special && seg->sidedef->sector->properties.special->ceiling_bob_ > 0)
    {
        lz2 += seg->sidedef->sector->properties.special->ceiling_bob_;
        rz2 += seg->sidedef->sector->properties.special->ceiling_bob_;
    }

    DrawWallPart(dfloor, x1, y1, lz1, lz2, x2, y2, rz1, rz2, tex_top_h, surf, image,
                 (flags & kWallTileMidMask) ? true : false, opaque, tex_x1, tex_x2,
                 (flags & kWallTileMidMask) ? &seg->sidedef->sector->properties : nullptr);
}

static inline void AddWallTile(Seg *seg, DrawFloor *dfloor, MapSurface *surf, float z1, float z2, float tex_z,
                               int flags, float f_min, float c_max)
{
    z1 = HMM_MAX(f_min, z1);
    z2 = HMM_MIN(c_max, z2);

    if (z1 >= z2 - 0.01)
        return;

    DrawTile(seg, dfloor, z1, z2, z1, z2, tex_z, flags, surf);
}

static inline void AddWallTile2(Seg *seg, DrawFloor *dfloor, MapSurface *surf, float lz1, float lz2, float rz1,
                                float rz2, float tex_z, int flags)
{
    DrawTile(seg, dfloor, lz1, lz2, rz1, rz2, tex_z, flags, surf);
}

static inline float SafeImageHeight(const Image *image)
{
    if (image)
        return image->ScaledHeight();
    else
        return 0;
}

static void ComputeWallTiles(Seg *seg, DrawFloor *dfloor, int sidenum, float f_min, float c_max,
                             bool mirror_sub = false)
{
    EDGE_ZoneScoped;

    Line       *ld = seg->linedef;
    Side       *sd = ld->side[sidenum];
    Sector     *sec, *other;
    MapSurface *surf;

    Extrafloor *S, *L, *C;
    float       floor_h;
    float       tex_z;

    bool lower_invis = false;
    bool upper_invis = false;

    if (!sd)
        return;

    sec   = sd->sector;
    other = sidenum ? ld->front_sector : ld->back_sector;

    float       slope_fh   = 0;
    float       slope_ch   = 0;
    float       other_fh   = 0;
    float       other_ch   = 0;
    MapSurface *slope_ceil = nullptr;
    MapSurface *other_ceil = nullptr;

    slope_fh = sec->interpolated_floor_height;
    if (sec->height_sector)
    {
        if (view_height_zone == kHeightZoneA && view_z > sec->height_sector->interpolated_ceiling_height)
        {
            slope_fh = sec->height_sector->interpolated_ceiling_height;
        }
        else if (view_height_zone == kHeightZoneC && view_z < sec->height_sector->interpolated_floor_height)
        {
        }
        else
        {
            slope_fh = sec->height_sector->interpolated_floor_height;
        }
    }
    else if (sec->floor_slope)
    {
        slope_fh += HMM_MIN(sec->floor_slope->delta_z1, sec->floor_slope->delta_z2);
    }

    slope_ch   = sec->interpolated_ceiling_height;
    slope_ceil = &sec->ceiling;
    if (sec->height_sector)
    {
        if (view_height_zone == kHeightZoneA && view_z > sec->height_sector->interpolated_ceiling_height)
        {
            slope_ceil = &sec->height_sector->ceiling;
        }
        else if (view_height_zone == kHeightZoneC && view_z < sec->height_sector->interpolated_floor_height)
        {
            slope_ch   = sec->height_sector->interpolated_floor_height;
            slope_ceil = &sec->height_sector->ceiling;
        }
        else
        {
            slope_ch = sec->height_sector->interpolated_ceiling_height;
        }
    }
    else if (sec->ceiling_slope)
    {
        slope_ch += HMM_MAX(sec->ceiling_slope->delta_z1, sec->ceiling_slope->delta_z2);
    }

    if (other)
    {
        other_fh = other->interpolated_floor_height;
        if (other->height_sector)
        {
            if (view_height_zone == kHeightZoneA && view_z > other->height_sector->interpolated_ceiling_height)
            {
                other_fh = other->height_sector->interpolated_ceiling_height;
            }
            else if (view_height_zone == kHeightZoneC && view_z < other->height_sector->interpolated_floor_height)
            {
            }
            else
            {
                other_fh = other->height_sector->interpolated_floor_height;
            }
        }
        else if (other->floor_slope)
        {
            other_fh += HMM_MIN(other->floor_slope->delta_z1, other->floor_slope->delta_z2);
        }

        other_ch   = other->interpolated_ceiling_height;
        other_ceil = &other->ceiling;
        if (other->height_sector)
        {
            if (view_height_zone == kHeightZoneA && view_z > other->height_sector->interpolated_ceiling_height)
            {
                other_ceil = &other->height_sector->ceiling;
            }
            else if (view_height_zone == kHeightZoneC && view_z < other->height_sector->interpolated_floor_height)
            {
                other_ch   = other->height_sector->interpolated_floor_height;
                other_ceil = &other->height_sector->ceiling;
            }
            else
            {
                other_ch = other->height_sector->interpolated_ceiling_height;
            }
        }
        else if (other->ceiling_slope)
        {
            other_ch += HMM_MAX(other->ceiling_slope->delta_z1, other->ceiling_slope->delta_z2);
        }
    }

    RGBAColor sec_fc = sec->properties.fog_color;
    float     sec_fd = sec->properties.fog_density;
    // check for DDFLEVL fog
    if (sec_fc == kRGBANoValue)
    {
        if (EDGE_IMAGE_IS_SKY(*slope_ceil))
        {
            sec_fc = current_map->outdoor_fog_color_;
            sec_fd = 0.01f * current_map->outdoor_fog_density_;
        }
        else
        {
            sec_fc = current_map->indoor_fog_color_;
            sec_fd = 0.01f * current_map->indoor_fog_density_;
        }
    }
    RGBAColor other_fc = (other ? other->properties.fog_color : kRGBANoValue);
    float     other_fd = (other ? other->properties.fog_density : 0.0f);
    if (other_fc == kRGBANoValue)
    {
        if (other)
        {
            if (EDGE_IMAGE_IS_SKY(*other_ceil))
            {
                other_fc = current_map->outdoor_fog_color_;
                other_fd = 0.01f * current_map->outdoor_fog_density_;
            }
            else
            {
                other_fc = current_map->indoor_fog_color_;
                other_fd = 0.01f * current_map->indoor_fog_density_;
            }
        }
    }

    if (sd->middle.fog_wall && draw_culling.d_)
        sd->middle.image = nullptr; // Don't delete image in case culling is toggled again

    if (!sd->middle.image && !draw_culling.d_)
    {
        if (sec_fc == kRGBANoValue && other_fc != kRGBANoValue)
        {
            Image *fw               = (Image *)ImageForFogWall(other_fc);
            fw->opacity_            = kOpacityComplex;
            sd->middle.image        = fw;
            sd->middle.translucency = other_fd * 100;
            sd->middle.fog_wall     = true;
        }
        else if (sec_fc != kRGBANoValue && other_fc != sec_fc)
        {
            Image *fw               = (Image *)ImageForFogWall(sec_fc);
            fw->opacity_            = kOpacityComplex;
            sd->middle.image        = fw;
            sd->middle.translucency = sec_fd * 100;
            sd->middle.fog_wall     = true;
        }
    }

    if (!other)
    {
        if (!sd->middle.image && !debug_hall_of_mirrors.d_)
            return;

        AddWallTile(seg, dfloor, &sd->middle, slope_fh, slope_ch,
                    (ld->flags & kLineFlagLowerUnpegged)
                        ? sec->interpolated_floor_height + (SafeImageHeight(sd->middle.image) / sd->middle.y_matrix.Y)
                        : sec->interpolated_ceiling_height,
                    0, f_min, c_max);
        return;
    }

    // handle lower, upper and mid-masker

    if (slope_fh < other->interpolated_floor_height || (sec->floor_vertex_slope || other->floor_vertex_slope))
    {
        if (!sec->floor_vertex_slope && other->floor_vertex_slope)
        {
            float zv1 = seg->vertex_1->Z;
            float zv2 = seg->vertex_2->Z;
            if (mirror_sub)
                std::swap(zv1, zv2);
            AddWallTile2(seg, dfloor, sd->bottom.image ? &sd->bottom : &other->floor, sec->interpolated_floor_height,
                         (zv1 < 32767.0f && zv1 > -32768.0f) ? zv1 : sec->interpolated_floor_height,
                         sec->interpolated_floor_height,
                         (zv2 < 32767.0f && zv2 > -32768.0f) ? zv2 : sec->interpolated_floor_height,
                         (ld->flags & kLineFlagLowerUnpegged)
                             ? sec->interpolated_ceiling_height
                             : HMM_MAX(sec->interpolated_floor_height, HMM_MAX(zv1, zv2)),
                         0);
        }
        else if (sec->floor_vertex_slope && !other->floor_vertex_slope)
        {
            float zv1 = seg->vertex_1->Z;
            float zv2 = seg->vertex_2->Z;
            if (mirror_sub)
                std::swap(zv1, zv2);
            AddWallTile2(seg, dfloor, sd->bottom.image ? &sd->bottom : &sec->floor,
                         (zv1 < 32767.0f && zv1 > -32768.0f) ? zv1 : other->interpolated_floor_height,
                         other->interpolated_floor_height,
                         (zv2 < 32767.0f && zv2 > -32768.0f) ? zv2 : other->interpolated_floor_height,
                         other->interpolated_floor_height,
                         (ld->flags & kLineFlagLowerUnpegged)
                             ? other->interpolated_ceiling_height
                             : HMM_MAX(other->interpolated_floor_height, HMM_MAX(zv1, zv2)),
                         0);
        }
        else if (!sd->bottom.image && !debug_hall_of_mirrors.d_)
        {
            lower_invis = true;
        }
        else if (other->floor_slope)
        {
            float lz1 = slope_fh;
            float rz1 = slope_fh;

            float lz2 = other->interpolated_floor_height +
                        Slope_GetHeight(other->floor_slope, seg->vertex_1->X, seg->vertex_1->Y);
            float rz2 = other->interpolated_floor_height +
                        Slope_GetHeight(other->floor_slope, seg->vertex_2->X, seg->vertex_2->Y);

            // Test fix for slope walls under 3D floors having 'flickering'
            // light levels - Dasho
            if (dfloor->extrafloor && seg->sidedef->sector->tag == dfloor->extrafloor->sector->tag)
            {
                dfloor->properties->light_level              = dfloor->extrafloor->properties->light_level;
                seg->sidedef->sector->properties.light_level = dfloor->extrafloor->properties->light_level;
            }

            AddWallTile2(seg, dfloor, &sd->bottom, lz1, lz2, rz1, rz2,
                         (ld->flags & kLineFlagLowerUnpegged) ? sec->interpolated_ceiling_height
                                                              : other->interpolated_floor_height,
                         0);
        }
        else
        {
            AddWallTile(seg, dfloor, &sd->bottom, slope_fh, other_fh,
                        (ld->flags & kLineFlagLowerUnpegged) ? sec->interpolated_ceiling_height
                                                             : other->interpolated_floor_height,
                        0, f_min, c_max);
        }
    }

    if ((slope_ch > other->interpolated_ceiling_height || (sec->ceiling_vertex_slope || other->ceiling_vertex_slope)) &&
        !(EDGE_IMAGE_IS_SKY(*slope_ceil) && EDGE_IMAGE_IS_SKY(*other_ceil)))
    {
        if (!sec->ceiling_vertex_slope && other->ceiling_vertex_slope)
        {
            float zv1 = seg->vertex_1->W;
            float zv2 = seg->vertex_2->W;
            if (mirror_sub)
                std::swap(zv1, zv2);
            AddWallTile2(seg, dfloor, sd->top.image ? &sd->top : &other->ceiling, sec->interpolated_ceiling_height,
                         (zv1 < 32767.0f && zv1 > -32768.0f) ? zv1 : sec->interpolated_ceiling_height,
                         sec->interpolated_ceiling_height,
                         (zv2 < 32767.0f && zv2 > -32768.0f) ? zv2 : sec->interpolated_ceiling_height,
                         (ld->flags & kLineFlagUpperUnpegged) ? sec->interpolated_floor_height : HMM_MIN(zv1, zv2), 0);
        }
        else if (sec->ceiling_vertex_slope && !other->ceiling_vertex_slope)
        {
            float zv1 = seg->vertex_1->W;
            float zv2 = seg->vertex_2->W;
            if (mirror_sub)
                std::swap(zv1, zv2);
            AddWallTile2(seg, dfloor, sd->top.image ? &sd->top : &sec->ceiling, other->interpolated_ceiling_height,
                         (zv1 < 32767.0f && zv1 > -32768.0f) ? zv1 : other->interpolated_ceiling_height,
                         other->interpolated_ceiling_height,
                         (zv2 < 32767.0f && zv2 > -32768.0f) ? zv2 : other->interpolated_ceiling_height,
                         (ld->flags & kLineFlagUpperUnpegged) ? other->interpolated_floor_height : HMM_MIN(zv1, zv2),
                         0);
        }
        else if (!sd->top.image && !debug_hall_of_mirrors.d_)
        {
            upper_invis = true;
        }
        else if (other->ceiling_slope)
        {
            float lz1 = other->interpolated_ceiling_height +
                        Slope_GetHeight(other->ceiling_slope, seg->vertex_1->X, seg->vertex_1->Y);
            float rz1 = other->interpolated_ceiling_height +
                        Slope_GetHeight(other->ceiling_slope, seg->vertex_2->X, seg->vertex_2->Y);

            float lz2 = slope_ch;
            float rz2 = slope_ch;

            AddWallTile2(seg, dfloor, &sd->top, lz1, lz2, rz1, rz2,
                         (ld->flags & kLineFlagUpperUnpegged)
                             ? sec->interpolated_ceiling_height
                             : other->interpolated_ceiling_height + SafeImageHeight(sd->top.image),
                         0);
        }
        else
        {
            AddWallTile(seg, dfloor, &sd->top, other_ch, slope_ch,
                        (ld->flags & kLineFlagUpperUnpegged)
                            ? sec->interpolated_ceiling_height
                            : other->interpolated_ceiling_height + SafeImageHeight(sd->top.image),
                        0, f_min, c_max);
        }
    }

    if (sd->middle.image)
    {
        float f1 = HMM_MAX(sec->interpolated_floor_height, other->interpolated_floor_height);
        float c1 = HMM_MIN(sec->interpolated_ceiling_height, other->interpolated_ceiling_height);

        float f2, c2;

        if (sd->middle.fog_wall)
        {
            float ofh = other->interpolated_floor_height;
            if (other->floor_slope)
            {
                float lz2 = other->interpolated_floor_height +
                            Slope_GetHeight(other->floor_slope, seg->vertex_1->X, seg->vertex_1->Y);
                float rz2 = other->interpolated_floor_height +
                            Slope_GetHeight(other->floor_slope, seg->vertex_2->X, seg->vertex_2->Y);
                ofh = HMM_MIN(ofh, HMM_MIN(lz2, rz2));
            }
            f2 = f1   = HMM_MAX(HMM_MIN(sec->interpolated_floor_height, slope_fh), ofh);
            float och = other->interpolated_ceiling_height;
            if (other->ceiling_slope)
            {
                float lz2 = other->interpolated_ceiling_height +
                            Slope_GetHeight(other->ceiling_slope, seg->vertex_1->X, seg->vertex_1->Y);
                float rz2 = other->interpolated_ceiling_height +
                            Slope_GetHeight(other->ceiling_slope, seg->vertex_2->X, seg->vertex_2->Y);
                och = HMM_MAX(och, HMM_MAX(lz2, rz2));
            }
            c2 = c1 = HMM_MIN(HMM_MAX(sec->interpolated_ceiling_height, slope_ch), och);
        }
        else if (ld->flags & kLineFlagLowerUnpegged)
        {
            f2 = f1 + sd->middle_mask_offset;
            c2 = f2 + (sd->middle.image->ScaledHeight() / sd->middle.y_matrix.Y);
        }
        else
        {
            c2 = c1 + sd->middle_mask_offset;
            f2 = c2 - (sd->middle.image->ScaledHeight() / sd->middle.y_matrix.Y);
        }

        tex_z = c2;

        // hack for transparent doors
        {
            if (lower_invis)
                f1 = sec->interpolated_floor_height;
            if (upper_invis)
                c1 = sec->interpolated_ceiling_height;
        }

        // hack for "see-through" lines (same sector on both sides)
        if (sec != other && !(sec->height_sector || other->height_sector)) // && !lower_invis)
        {
            f2 = HMM_MAX(f2, f1);
            c2 = HMM_MIN(c2, c1);
        }

        /*if (sec == other)
        {
            f2 = HMM_MAX(f2, f1);
            c2 = HMM_MIN(c2, c1);
        }*/

        if (c2 > f2)
        {
            AddWallTile(seg, dfloor, &sd->middle, f2, c2, tex_z, kWallTileMidMask, f_min, c_max);
        }
    }

    // -- thick extrafloor sides --

    // -AJA- Don't bother drawing extrafloor sides if the front/back
    //       sectors have the same tag (and thus the same extrafloors).
    //
    if (other->tag == sec->tag)
        return;

    floor_h = other->interpolated_floor_height;

    S = other->bottom_extrafloor;
    L = other->bottom_liquid;

    while (S || L)
    {
        if (!L || (S && S->bottom_height < L->bottom_height))
        {
            C = S;
            S = S->higher;
        }
        else
        {
            C = L;
            L = L->higher;
        }

        EPI_ASSERT(C);

        // ignore liquids in the middle of THICK solids, or below real
        // floor or above real ceiling
        //
        if (C->bottom_height < floor_h || C->bottom_height > other->interpolated_ceiling_height)
            continue;

        if (C->extrafloor_definition->type_ & kExtraFloorTypeThick)
        {
            int flags = kWallTileIsExtra;

            // -AJA- 1999/09/25: Better DDF control of side texture.
            if (C->extrafloor_definition->type_ & kExtraFloorTypeSideUpper)
                surf = &sd->top;
            else if (C->extrafloor_definition->type_ & kExtraFloorTypeSideLower)
                surf = &sd->bottom;
            else
            {
                surf = &C->extrafloor_line->side[0]->middle;

                flags |= kWallTileExtraX;

                if (C->extrafloor_definition->type_ & kExtraFloorTypeSideMidY)
                    flags |= kWallTileExtraY;
            }

            if (!surf->image && !debug_hall_of_mirrors.d_)
                continue;

            tex_z = (C->extrafloor_line->flags & kLineFlagLowerUnpegged)
                        ? C->bottom_height + (SafeImageHeight(surf->image) / surf->y_matrix.Y)
                        : C->top_height;

            AddWallTile(seg, dfloor, surf, C->bottom_height, C->top_height, tex_z, flags, f_min, c_max);
        }

        floor_h = C->top_height;
    }
}

static void RenderSeg(DrawFloor *dfloor, Seg *seg, bool mirror_sub = false)
{
    //
    // Analyses floor/ceiling heights, and add corresponding walls/floors
    // to the drawfloor.  Returns true if the whole region was "solid".
    //
    current_seg = seg;

    EPI_ASSERT(!seg->miniseg && seg->linedef);

    // mark the line on the automap
    if (!(seg->linedef->flags & kLineFlagMapped))
        newly_seen_lines.emplace(seg->linedef);

    front_sector = seg->front_subsector->sector;
    back_sector  = nullptr;

    if (seg->back_subsector)
        back_sector = seg->back_subsector->sector;

    Side *sd = seg->sidedef;

    float f_min = dfloor->is_lowest ? -32767.0 : dfloor->floor_height;
    float c_max = dfloor->is_highest ? +32767.0 : dfloor->ceiling_height;

#if (DEBUG >= 3)
    LogDebug("   BUILD WALLS %1.1f .. %1.1f\n", f_min, c1);
#endif

    // handle TRANSLUCENT + THICK floors (a bit of a hack)
    if (dfloor->extrafloor && !dfloor->is_highest &&
        (dfloor->extrafloor->extrafloor_definition->type_ & kExtraFloorTypeThick) &&
        (dfloor->extrafloor->top->translucency < 0.99f))
    {
        c_max = dfloor->extrafloor->top_height;
    }

    ComputeWallTiles(seg, dfloor, seg->side, f_min, c_max, mirror_sub);

    if ((sd->bottom.image == nullptr || sd->top.image == nullptr) && back_sector)
    {
        float f_fh = 0;
        float b_fh = 0;
        float f_ch = 0;
        float b_ch = 0;

        // Unlike other places where we check Line 242 stuff, it seems to look "right" when using
        // the control sector heights regardless of being in view zone A/B/C. To be fair I have
        // only tested this with Firerainbow MAP01 - Dasho
        if (!front_sector->height_sector)
        {
            if (seg->front_subsector->deep_water_reference)
            {
                f_fh = seg->front_subsector->deep_water_reference->interpolated_floor_height;
                f_ch = seg->front_subsector->deep_water_reference->interpolated_ceiling_height;
            }
            else
            {
                f_fh = front_sector->interpolated_floor_height;
                f_ch = front_sector->interpolated_ceiling_height;
            }
        }
        else
        {
            f_fh = front_sector->height_sector->interpolated_floor_height;
            f_ch = front_sector->height_sector->interpolated_ceiling_height;
        }
        if (!back_sector->height_sector)
        {
            if (seg->back_subsector->deep_water_reference)
            {
                b_fh = seg->back_subsector->deep_water_reference->interpolated_floor_height;
                b_ch = seg->back_subsector->deep_water_reference->interpolated_ceiling_height;
            }
            else
            {
                b_fh = back_sector->interpolated_floor_height;
                b_ch = back_sector->interpolated_ceiling_height;
            }
        }
        else
        {
            b_fh = back_sector->height_sector->interpolated_floor_height;
            b_ch = back_sector->height_sector->interpolated_ceiling_height;
        }

        // -AJA- 2004/04/21: Emulate Flat-Flooding TRICK
        if (!debug_hall_of_mirrors.d_ && solid_mode && dfloor->is_lowest && sd->bottom.image == nullptr &&
            current_seg->back_subsector && b_fh > f_fh && b_fh < view_z)
        {
            EmulateFloodPlane(dfloor, current_seg->back_subsector->sector, +1, f_fh, b_fh);
        }

        if (!debug_hall_of_mirrors.d_ && solid_mode && dfloor->is_highest && sd->top.image == nullptr &&
            current_seg->back_subsector && b_ch < f_ch && b_ch > view_z)
        {
            EmulateFloodPlane(dfloor, current_seg->back_subsector->sector, -1, b_ch, f_ch);
        }
    }
}

static void RenderPlane(DrawFloor *dfloor, float h, MapSurface *surf, int face_dir)
{
    EDGE_ZoneScoped;

    float orig_h = h;

    render_mirror_set.Height(h);

    int num_vert, i;

    if (!surf->image)
        return;

    // ignore sky
    if (EDGE_IMAGE_IS_SKY(*surf))
        return;

    ec_frame_stats.draw_planes++;

    RegionProperties *props = dfloor->properties;

    // more deep water hackitude
    if (current_subsector->deep_water_reference && !current_subsector->sector->height_sector &&
        ((face_dir > 0 && dfloor->render_previous == nullptr) || (face_dir < 0 && dfloor->render_next == nullptr)))
    {
        props = &current_subsector->deep_water_reference->properties;
    }

    if (surf->override_properties)
        props = surf->override_properties;

    SlopePlane *slope = nullptr;

    if (face_dir > 0 && dfloor->is_lowest)
        slope = current_subsector->sector->floor_slope;

    if (face_dir < 0 && dfloor->is_highest)
        slope = current_subsector->sector->ceiling_slope;

    const float trans = surf->translucency;

    // ignore invisible planes
    if (trans < 0.01f)
        return;

    // ignore non-facing planes
    if ((view_z > h) != (face_dir > 0) && !slope && !current_subsector->sector->floor_vertex_slope)
        return;

    // ignore dud regions (floor >= ceiling)
    if (dfloor->floor_height > dfloor->ceiling_height && !slope && !current_subsector->sector->ceiling_vertex_slope)
        return;

    // ignore empty subsectors
    if (current_subsector->segs == nullptr)
        return;

    // (need to load the image to know the opacity)
    GLuint tex_id = ImageCache(surf->image, true, render_view_effect_colormap);

    BlendingMode blending = GetSurfaceBlending(trans, (ImageOpacity)surf->image->opacity_);

    // ignore non-solid walls in solid mode (& vice versa)
    if ((solid_mode && (blending & kBlendingAlpha)) || (!solid_mode && !(blending & kBlendingAlpha)))
    {
        if (solid_mode)
        {
            current_draw_subsector->solid = false;
        }

        return;
    }

    // count number of actual vertices
    Seg *seg;
    for (seg = current_subsector->segs, num_vert = 0; seg; seg = seg->subsector_next, num_vert++)
    {
        /* no other code needed */
    }

    // -AJA- make sure polygon has enough vertices.  Sometimes a subsector
    // ends up with only 1 or 2 segs due to level problems (e.g. MAP22).
    if (num_vert < 3)
        return;

    if (num_vert > kMaximumPolygonVertices)
        num_vert = kMaximumPolygonVertices;

    HMM_Vec3 vertices[kMaximumPolygonVertices];

    float v_bbox[4];

    BoundingBoxClear(v_bbox);

    int v_count = 0;

    for (seg = current_subsector->segs, i = 0; seg && (i < kMaximumPolygonVertices); seg = seg->subsector_next, i++)
    {
        if (v_count < kMaximumPolygonVertices)
        {
            float x = seg->vertex_1->X;
            float y = seg->vertex_1->Y;
            float z = h;

            // must do this before mirror adjustment
            BoundingBoxAddPoint(v_bbox, x, y);

            if (current_subsector->sector->floor_vertex_slope && face_dir > 0)
            {
                // floor - check vertex heights
                if (seg->vertex_1->Z < 32767.0f && seg->vertex_1->Z > -32768.0f)
                    z = seg->vertex_1->Z;
            }

            if (current_subsector->sector->ceiling_vertex_slope && face_dir < 0)
            {
                // ceiling - check vertex heights
                if (seg->vertex_1->W < 32767.0f && seg->vertex_1->W > -32768.0f)
                    z = seg->vertex_1->W;
            }

            if (slope)
            {
                z = orig_h + Slope_GetHeight(slope, x, y);

                render_mirror_set.Height(z);
            }

            render_mirror_set.Coordinate(x, y);

            vertices[v_count].X = x;
            vertices[v_count].Y = y;
            vertices[v_count].Z = z;

            v_count++;
        }
    }

    PlaneCoordinateData data;

    data.v_count  = v_count;
    data.vertices = vertices;
    data.R = data.G = data.B = 255;
    if (!AlmostEquals(surf->old_offset.X, surf->offset.X) && !console_active && !paused && !menu_active &&
        !time_stop_active && !erraticism_active)
        data.tx0 = fmod(HMM_Lerp(surf->old_offset.X, fractional_tic, surf->offset.X), surf->image->width_);
    else
        data.tx0 = surf->offset.X;
    if (!AlmostEquals(surf->old_offset.Y, surf->offset.Y) && !console_active && !paused && !menu_active &&
        !time_stop_active && !erraticism_active)
        data.ty0 = fmod(HMM_Lerp(surf->old_offset.Y, fractional_tic, surf->offset.Y), surf->image->height_);
    else
        data.ty0 = surf->offset.Y;
    data.image_w    = surf->image->ScaledWidth();
    data.image_h    = surf->image->ScaledHeight();
    data.x_mat      = surf->x_matrix;
    data.y_mat      = surf->y_matrix;
    float mir_scale = render_mirror_set.XYScale();
    data.x_mat.X /= mir_scale;
    data.x_mat.Y /= mir_scale;
    data.y_mat.X /= mir_scale;
    data.y_mat.Y /= mir_scale;
    data.normal   = {{0, 0, (view_z > h) ? 1.0f : -1.0f}};
    data.tex_id   = tex_id;
    data.pass     = 0;
    data.blending = blending;
    data.trans    = trans;
    data.slope    = slope;
    data.rotation = surf->rotation;

    if (current_subsector->sector->properties.special)
    {
        if (face_dir > 0)
            data.bob_amount = current_subsector->sector->properties.special->floor_bob_;
        else
            data.bob_amount = current_subsector->sector->properties.special->ceiling_bob_;
    }

    if (surf->image->liquid_type_ == kLiquidImageThick)
        thick_liquid = true;
    else
        thick_liquid = false;

    if (surf->image->liquid_type_ > kLiquidImageNone && swirling_flats > kLiquidSwirlSmmu)
        swirl_pass = 1;

    AbstractShader *cmap_shader = GetColormapShader(props, 0, current_subsector->sector);

    cmap_shader->WorldMix(GL_POLYGON, data.v_count, data.tex_id, trans, &data.pass, data.blending, false /* masked */,
                          &data, PlaneCoordFunc);

    if (surf->image->liquid_type_ > kLiquidImageNone &&
        swirling_flats == kLiquidSwirlParallax) // Kept as an example for future effects
    {
        data.tx0               = data.tx0 + 25;
        data.ty0               = data.ty0 + 25;
        swirl_pass             = 2;
        BlendingMode old_blend = data.blending;
        float        old_dt    = data.trans;
        data.blending          = (BlendingMode)(kBlendingMasked | kBlendingAlpha);
        data.trans             = 0.33f;
        cmap_shader->WorldMix(GL_POLYGON, data.v_count, data.tex_id, 0.33f, &data.pass, data.blending, false, &data,
                              PlaneCoordFunc);
        data.blending = old_blend;
        data.trans    = old_dt;
    }

    if (use_dynamic_lights && render_view_extra_light < 250)
    {
        DynamicLightIterator(v_bbox[kBoundingBoxLeft], v_bbox[kBoundingBoxBottom], h, v_bbox[kBoundingBoxRight],
                             v_bbox[kBoundingBoxTop], h, DLIT_Plane, &data);

        SectorGlowIterator(current_subsector->sector, v_bbox[kBoundingBoxLeft], v_bbox[kBoundingBoxBottom], h,
                           v_bbox[kBoundingBoxRight], v_bbox[kBoundingBoxTop], h, GLOWLIT_Plane, &data);
    }

    swirl_pass = 0;
}

static void RenderSubsector(DrawSubsector *dsub, bool mirror_sub = false);

void RenderSubList(std::list<DrawSubsector *> &dsubs, bool for_mirror)
{
    // draw all solid walls and planes
    solid_mode = true;
    // if (!for_mirror)
    render_backend->SetRenderLayer(kRenderLayerSolid, false);
    StartUnitBatch(solid_mode);

    std::list<DrawSubsector *>::iterator FI; // Forward Iterator

    for (FI = dsubs.begin(); FI != dsubs.end(); FI++)
        RenderSubsector(*FI, for_mirror);

    FinishUnitBatch();

    // draw all sprites and masked/translucent walls/planes
    solid_mode = false;
    // if (!for_mirror)
    render_backend->SetRenderLayer(kRenderLayerTransparent, false);
    StartUnitBatch(solid_mode);

    std::list<DrawSubsector *>::reverse_iterator RI;

    for (RI = dsubs.rbegin(); RI != dsubs.rend(); RI++)
        RenderSubsector(*RI, for_mirror);

    FinishUnitBatch();
}

static void RenderSubsector(DrawSubsector *dsub, bool mirror_sub)
{
    EDGE_ZoneScoped;

    Subsector *sub = dsub->subsector;

#if (DEBUG >= 1)
    LogDebug("\nREVISITING SUBSEC %d\n\n", (int)(sub - subsectors));
#endif

    current_subsector      = sub;
    current_draw_subsector = dsub;

    if (solid_mode)
    {
        std::list<DrawMirror *>::iterator MRI;

        for (MRI = dsub->mirrors.begin(); MRI != dsub->mirrors.end(); MRI++)
        {
            RenderMirror(*MRI);
        }
    }

    current_subsector      = sub;
    current_draw_subsector = dsub;

    DrawFloor *dfloor;

    // handle each floor, drawing planes and things
    for (dfloor = dsub->render_floors; dfloor != nullptr; dfloor = dfloor->render_next)
    {
        for (std::list<DrawSeg *>::iterator iter = dsub->segs.begin(), iter_end = dsub->segs.end(); iter != iter_end;
             iter++)
        {
            RenderSeg(dfloor, (*iter)->seg, mirror_sub);
        }

        RenderPlane(dfloor, dfloor->ceiling_height, dfloor->ceiling, -1);
        RenderPlane(dfloor, dfloor->floor_height, dfloor->floor, +1);

        if (!RenderThings(dfloor, solid_mode))
        {
            current_draw_subsector->solid = false;
        }
    }
}

static void DoWeaponModel(void)
{
    Player *pl = view_camera_map_object->player_;

    if (!pl)
        return;

    // clear the depth buffer, so that the weapon is never clipped
    // by the world geometry.  NOTE: a tad expensive, but I don't
    // know how any better way to prevent clipping -- the model
    // needs the depth buffer for overlapping parts of itself.

    render_state->Clear(GL_DEPTH_BUFFER_BIT);

    solid_mode = false;
    StartUnitBatch(solid_mode);

    RenderWeaponModel(pl);

    FinishUnitBatch();
}

static void InitializeCamera(MapObject *mo, bool full_height, float expand_w)
{
    float fov = HMM_Clamp(5, field_of_view.f_, 175);

    wave_now    = level_time_elapsed / 100.0f;
    plane_z_bob = sine_table[(int)((kWavetableIncrement + wave_now) * kSineTableSize) & (kSineTableMask)];

    view_x_slope = tan(90.0f * HMM_PI / 360.0);

    if (full_height)
        view_y_slope = kDoomYSlopeFull;
    else
        view_y_slope = kDoomYSlope;

    if (!AlmostEquals(fov, 90.0f))
    {
        float new_slope = tan(fov * HMM_PI / 360.0);

        view_y_slope *= new_slope / view_x_slope;
        view_x_slope = new_slope;
    }

    view_is_zoomed = false;

    if (mo->player_ && mo->player_->zoom_field_of_view_ > 0)
    {
        view_is_zoomed = true;

        float new_slope = tan(mo->player_->zoom_field_of_view_ * HMM_PI / 360.0);

        view_y_slope *= new_slope / view_x_slope;
        view_x_slope = new_slope;
    }

    // wide-screen adjustment
    widescreen_view_width_multiplier = expand_w;

    view_x_slope *= widescreen_view_width_multiplier;

    if (level_time_elapsed && mo->player_ && mo->interpolate_ && !console_active && !paused && !menu_active &&
        !rts_menu_active)
    {
        view_x     = HMM_Lerp(mo->old_x_, fractional_tic, mo->x);
        view_y     = HMM_Lerp(mo->old_y_, fractional_tic, mo->y);
        view_z     = HMM_Lerp(mo->old_z_, fractional_tic, mo->z);
        view_angle = epi::BAMInterpolate(mo->old_angle_, mo->angle_, fractional_tic);
        view_z += HMM_Lerp(mo->player_->old_view_z_, fractional_tic, mo->player_->view_z_);
        view_vertical_angle = epi::BAMInterpolate(mo->old_vertical_angle_, mo->vertical_angle_, fractional_tic);
    }
    else
    {
        view_x     = mo->x;
        view_y     = mo->y;
        view_z     = mo->z;
        view_angle = mo->angle_;
        if (mo->player_)
            view_z += mo->player_->view_z_;
        else
            view_z += mo->height_ * 9 / 10;
        view_vertical_angle = mo->vertical_angle_;
    }

    view_subsector = mo->subsector_;
    if (view_subsector->sector->height_sector)
    {
        if (view_z > view_subsector->sector->height_sector->interpolated_ceiling_height)
        {
            view_height_zone = kHeightZoneA;
        }
        else if (view_z < view_subsector->sector->height_sector->interpolated_floor_height)
        {
            view_height_zone = kHeightZoneC;
        }
        else
        {
            view_height_zone = kHeightZoneB;
        }
    }
    else
        view_height_zone = kHeightZoneNone;
    view_properties = GetPointProperties(view_subsector, view_z);

    if (mo->player_)
    {
        if (!level_flags.mouselook)
            view_vertical_angle = 0;

        view_vertical_angle += epi::BAMFromATan(mo->player_->kick_offset_);

        // No heads above the ceiling
        if (view_z > mo->player_->map_object_->ceiling_z_ - 2)
            view_z = mo->player_->map_object_->ceiling_z_ - 2;

        // No heads below the floor, please
        if (view_z < mo->player_->map_object_->floor_z_ + 2)
            view_z = mo->player_->map_object_->floor_z_ + 2;
    }

    // do some more stuff
    view_sine   = epi::BAMSin(view_angle);
    view_cosine = epi::BAMCos(view_angle);

    float lk_sin = epi::BAMSin(view_vertical_angle);
    float lk_cos = epi::BAMCos(view_vertical_angle);

    view_forward.X = lk_cos * view_cosine;
    view_forward.Y = lk_cos * view_sine;
    view_forward.Z = lk_sin;

    view_up.X = -lk_sin * view_cosine;
    view_up.Y = -lk_sin * view_sine;
    view_up.Z = lk_cos;

    // cross product
    view_right.X = view_forward.Y * view_up.Z - view_up.Y * view_forward.Z;
    view_right.Y = view_forward.Z * view_up.X - view_up.Z * view_forward.X;
    view_right.Z = view_forward.X * view_up.Y - view_up.X * view_forward.Y;

    // compute the 1D projection of the view angle
    BAMAngle oned_side_angle;
    {
        float k, d;

        // k is just the mlook angle (in radians)
        k = epi::DegreesFromBAM(view_vertical_angle);
        if (k > 180.0)
            k -= 360.0;
        k = k * HMM_PI / 180.0f;

        sprite_skew = tan((-k) / 2.0);

        k = fabs(k);

        // d is just the distance horizontally forward from the eye to
        // the top/bottom edge of the view rectangle.
        d = cos(k) - sin(k) * view_y_slope;

        oned_side_angle = (d <= 0.01f) ? kBAMAngle180 : epi::BAMFromATan(view_x_slope / d);
    }

    // setup clip angles
    if (oned_side_angle != kBAMAngle180)
    {
        clip_left  = 0 + oned_side_angle;
        clip_right = 0 - oned_side_angle;
        clip_scope = clip_left - clip_right;
    }
    else
    {
        // not clipping to the viewport.  Dummy values.
        clip_scope = kBAMAngle180;
        clip_left  = 0 + kBAMAngle45;
        clip_right = uint32_t(0 - kBAMAngle45);
    }
}

// Render world index, the root world render is 0
static int32_t render_world_index = 0;
void           RendererEndFrame()
{
    render_world_index = 0;
}

void RendererShutdownLevel()
{
#ifdef EDGE_SOKOL
    deferred_sky_items.clear();
#endif
    ShutdownSky();
}

void UpdateSectorInterpolation(Sector *sector)
{
    if (!time_stop_active && !console_active && !paused && !erraticism_active && !menu_active && !rts_menu_active)
    {
        // Interpolate between current and last floor/ceiling position.
        if (!AlmostEquals(sector->floor_height, sector->old_floor_height))
            sector->interpolated_floor_height =
                HMM_Lerp(sector->old_floor_height, fractional_tic, sector->floor_height);
        else
            sector->interpolated_floor_height = sector->floor_height;
        if (!AlmostEquals(sector->ceiling_height, sector->old_ceiling_height))
            sector->interpolated_ceiling_height =
                HMM_Lerp(sector->old_ceiling_height, fractional_tic, sector->ceiling_height);
        else
            sector->interpolated_ceiling_height = sector->ceiling_height;
    }
    else
    {
        sector->interpolated_floor_height   = sector->floor_height;
        sector->interpolated_ceiling_height = sector->ceiling_height;
    }
}

//
// RenderTrueBSP
//
// OpenGL BSP rendering.  Initialises all structures, then walks the
// BSP tree collecting information, then renders each subsector:
// firstly front to back (drawing all solid walls & planes) and then
// from back to front (drawing everything else, sprites etc..).
//
void RenderTrueBSP(void)
{
    EDGE_ZoneScoped;

    FuzzUpdate();

    ClearBSP();
    OcclusionClear();

    Player *v_player = view_camera_map_object->player_;

    // handle powerup effects and BOOM colormaps
    RendererRainbowEffect(v_player);

    // update interpolation for moving sectors
    for (std::vector<PlaneMover *>::iterator PMI = active_planes.begin(), PMI_END = active_planes.end(); PMI != PMI_END;
         ++PMI)
    {
        PlaneMover *pmov = *PMI;
        if (pmov->sector)
            UpdateSectorInterpolation(pmov->sector);
    }

#ifdef EDGE_SOKOL

    render_state->Enable(GL_DEPTH_TEST);

    BeginSky();

    if (!render_world_index)
    {
        need_to_draw_sky = deferred_sky_items.size();

        if (need_to_draw_sky)
        {
            render_backend->SetRenderLayer(kRenderLayerSkyDeferred, true);

            // Render deferred sky walls and planes from previous frame
            std::list<RenderItem *>::iterator I;
            for (I = deferred_sky_items.begin(); I != deferred_sky_items.end(); I++)
            {
                RenderItem *item = (*I);

                if (item->type_ == kRenderSkyWall)
                {
                    RenderSkyWall(item->wallSeg_, item->height1_, item->height2_);
                }
                else
                {
                    RenderSkyPlane(item->wallPlane_, item->height1_);
                }
            }

            FlushSky(); // flush any deferred sky units
            // Always render a simple sky, when rendering sky walls/planes they are deferred
            // from previous frame, so fast movement will cause issus on screen edges
            render_backend->SetRenderLayer(kRenderLayerSky, false);
            FinishSky();
        }

        deferred_sky_items.clear();
    }
    else
    {
        // Always render simple skies for additional world renders
        need_to_draw_sky = true;
        render_backend->SetRenderLayer(kRenderLayerSky, true);
        FinishSky();
    }

    // draw all solid walls and planes
    solid_mode = true;
    render_backend->SetRenderLayer(kRenderLayerSolid, false);
    StartUnitBatch(solid_mode);

    BSPTraverse();

    std::list<RenderItem *> items;
    while (BSPTraversing())
    {
        RenderBatch *batch = BSPReadRenderBatch();
        if (!batch)
        {
            continue;
        }

        for (int32_t i = 0; i < batch->num_items_; i++)
        {
            RenderItem *item = &batch->items_[i];

            switch (item->type_)
            {
            case kRenderSubsector:
                items.push_back(item);
                RenderSubsector(item->subsector_, false);
                break;
            case kRenderSkyWall:
                // Save off item for next frame
                if (!render_world_index)
                    deferred_sky_items.push_back(item);
                break;
            case kRenderSkyPlane:
                // Save off item for next frame
                if (!render_world_index)
                    deferred_sky_items.push_back(item);
                break;
            }
        }
    }
    FinishUnitBatch();

    // draw all sprites and masked/translucent walls/planes
    solid_mode = false;
    render_backend->SetRenderLayer(kRenderLayerTransparent, false);

    StartUnitBatch(solid_mode);

    std::list<RenderItem *>::reverse_iterator RI;
    for (RI = items.rbegin(); RI != items.rend(); RI++)
    {
        RenderItem *item = *RI;
        if (item->type_ == kRenderSubsector)
        {
            if (item->subsector_->solid)
            {
                continue;
            }

            RenderSubsector(item->subsector_, false);
        }
    }

    FinishUnitBatch();

#else

    draw_subsector_list.clear();

    render_backend->SetupMatrices3D();
    render_state->Clear(GL_DEPTH_BUFFER_BIT);
    render_state->Enable(GL_DEPTH_TEST);

    // needed for drawing the sky
    BeginSky();

    // walk the bsp tree
    BSPWalkNode(root_node);

    FlushSky();
    FinishSky();

    RenderSubList(draw_subsector_list);

#endif

    // Add lines seen during render to the automap
    if (!newly_seen_lines.empty())
    {
        for (Line *li : newly_seen_lines)
        {
            li->flags |= kLineFlagMapped;
        }
        newly_seen_lines.clear();
    }

    // Lobo 2022:
    // Allow changing the order of weapon model rendering to be
    // after RenderWeaponSprites() so that FLASH states are
    // drawn in front of the weapon
    bool FlashFirst = false;

    if (v_player)
    {
        if (v_player->ready_weapon_ >= 0)
        {
            FlashFirst = v_player->weapons_[v_player->ready_weapon_].info->render_invert_;
        }
    }

    render_backend->SetRenderLayer(kRenderLayerWeapon);

    if (FlashFirst == false)
    {
        DoWeaponModel();
    }

    render_state->Disable(GL_DEPTH_TEST);

    // now draw 2D stuff like psprites, and add effects
    render_backend->SetupWorldMatrices2D();

    if (v_player)
    {
        RenderWeaponSprites(v_player);

        RendererColourmapEffect(v_player);
        RendererPaletteEffect(v_player);
        render_backend->SetupMatrices2D();
        RenderCrosshair(v_player);
    }

    if (FlashFirst == true)
    {
        render_backend->SetupMatrices3D();
        render_state->Enable(GL_DEPTH_TEST);
        DoWeaponModel();
        render_state->Disable(GL_DEPTH_TEST);
        render_backend->SetupMatrices2D();
    }

#if (DEBUG >= 3)
    LogDebug("\n\n");
#endif
}

void RenderView(int x, int y, int w, int h, MapObject *camera, bool full_height, float expand_w)
{
    EDGE_ZoneScoped;

    view_window_x      = x;
    view_window_y      = y;
    view_window_width  = w;
    view_window_height = h;

    view_camera_map_object = camera;

    // Load the details for the camera
    InitializeCamera(camera, full_height, expand_w);

    // Profiling
    render_frame_count++;
    valid_count++;

    seen_dynamic_lights.clear();
    RenderTrueBSP();

    render_world_index++;
}

static constexpr uint8_t kMaximumFloodVertices = 16;

struct FloodEmulationData
{
    int      v_count;
    HMM_Vec3 vertices[2 * (kMaximumFloodVertices + 1)];

    GLuint tex_id;
    int    pass;

    float R, G, B;

    float plane_h;

    float tx0, ty0;
    float image_w, image_h;

    HMM_Vec2 x_mat;
    HMM_Vec2 y_mat;

    HMM_Vec3 normal;

    int piece_row;
    int piece_col;

    float h1, dh;
};

static void FloodCoordFunc(void *d, int v_idx, HMM_Vec3 *pos, RGBAColor *rgb, HMM_Vec2 *texc, HMM_Vec3 *normal,
                           HMM_Vec3 *lit_pos)
{
    const FloodEmulationData *data = (FloodEmulationData *)d;

    *pos    = data->vertices[v_idx];
    *normal = data->normal;
    *rgb    = epi::MakeRGBA((uint8_t)(data->R * render_view_red_multiplier),
                            (uint8_t)(data->G * render_view_green_multiplier),
                            (uint8_t)(data->B * render_view_blue_multiplier), epi::GetRGBAAlpha(*rgb));

    float along = (view_z - data->plane_h) / (view_z - pos->Z);

    lit_pos->X = view_x + along * (pos->X - view_x);
    lit_pos->Y = view_y + along * (pos->Y - view_y);
    lit_pos->Z = data->plane_h;

    float rx = (data->tx0 + lit_pos->X) / data->image_w;
    float ry = (data->ty0 + lit_pos->Y) / data->image_h;

    texc->X = rx * data->x_mat.X + ry * data->x_mat.Y;
    texc->Y = rx * data->y_mat.X + ry * data->y_mat.Y;
}

static void DLIT_Flood(MapObject *mo, void *dataptr)
{
    FloodEmulationData *data = (FloodEmulationData *)dataptr;

    // light behind the plane ?
    if (!mo->info_->dlight_.leaky_ &&
        !(mo->subsector_->sector->floor_vertex_slope || mo->subsector_->sector->ceiling_vertex_slope))
    {
        if ((MapObjectMidZ(mo) > data->plane_h) != (data->normal.Z > 0))
            return;
    }

    // NOTE: distance already checked in DynamicLightIterator

    EPI_ASSERT(mo->dynamic_light_.shader);

    float sx = current_seg->vertex_1->X;
    float sy = current_seg->vertex_1->Y;

    float dx = current_seg->vertex_2->X - sx;
    float dy = current_seg->vertex_2->Y - sy;

    BlendingMode blending = kBlendingAdd;

    for (int row = 0; row < data->piece_row; row++)
    {
        float z = data->h1 + data->dh * row / (float)data->piece_row;

        for (int col = 0; col <= data->piece_col; col++)
        {
            float x = sx + dx * col / (float)data->piece_col;
            float y = sy + dy * col / (float)data->piece_col;

            data->vertices[col * 2 + 0] = {{x, y, z}};
            data->vertices[col * 2 + 1] = {{x, y, z + data->dh / data->piece_row}};
        }

        if (data->pass > 5)
        {
            break;
        }

        mo->dynamic_light_.shader->WorldMix(GL_QUAD_STRIP, data->v_count, data->tex_id, 1.0, &data->pass, blending,
                                            false, data, FloodCoordFunc);
    }
}

void EmulateFloodPlane(const DrawFloor *dfloor, const Sector *flood_ref, int face_dir, float h1, float h2)
{
    EDGE_ZoneScoped;

    EPI_UNUSED(dfloor);

    if (render_mirror_set.TotalActive() > 0)
        return;

    const MapSurface *surf = (face_dir > 0) ? &flood_ref->floor : &flood_ref->ceiling;

    if (!surf->image)
        return;

    // ignore sky and invisible planes
    if (EDGE_IMAGE_IS_SKY(*surf) || surf->translucency < 0.01f)
        return;

    // ignore transparent doors (TNT MAP02)
    if (flood_ref->interpolated_floor_height >= flood_ref->interpolated_ceiling_height)
        return;

    // ignore fake 3D bridges (Batman MAP03)
    if (current_seg->linedef && current_seg->linedef->front_sector == current_seg->linedef->back_sector)
        return;

    const RegionProperties *props = surf->override_properties ? surf->override_properties : &flood_ref->properties;

    EPI_ASSERT(props);

    FloodEmulationData data;

    data.tex_id = ImageCache(surf->image, true, render_view_effect_colormap);
    data.pass   = 0;

    data.R = data.G = data.B = 255;

    data.plane_h = (face_dir > 0) ? h2 : h1;

    // I don't think we need interpolation here...are there Boom scrollers which are also flat flooding hacks? - Dasho
    data.tx0     = surf->offset.X;
    data.ty0     = surf->offset.Y;
    data.image_w = surf->image->ScaledWidth();
    data.image_h = surf->image->ScaledHeight();

    data.x_mat = surf->x_matrix;
    data.y_mat = surf->y_matrix;

    data.normal = {{0, 0, (float)face_dir}};

    // determine number of pieces to subdivide the area into.
    // The more the better, upto a limit of 64 pieces, and
    // also limiting the size of the pieces.

    float piece_w = current_seg->length;
    float piece_h = h2 - h1;

    int piece_col = 1;
    int piece_row = 1;

    while (piece_w > 16 || piece_h > 16)
    {
        if (piece_col * piece_row >= 64)
            break;

        if (piece_col >= kMaximumFloodVertices && piece_row >= kMaximumFloodVertices)
            break;

        if (piece_w >= piece_h && piece_col < kMaximumFloodVertices)
        {
            piece_w /= 2.0;
            piece_col *= 2;
        }
        else
        {
            piece_h /= 2.0;
            piece_row *= 2;
        }
    }

    EPI_ASSERT(piece_col <= kMaximumFloodVertices);

    float sx = current_seg->vertex_1->X;
    float sy = current_seg->vertex_1->Y;

    float dx = current_seg->vertex_2->X - sx;
    float dy = current_seg->vertex_2->Y - sy;
    float dh = h2 - h1;

    data.piece_row = piece_row;
    data.piece_col = piece_col;
    data.h1        = h1;
    data.dh        = dh;

    AbstractShader *cmap_shader = GetColormapShader(props, 0, current_subsector->sector);

    data.v_count = (piece_col + 1) * 2;

    for (int row = 0; row < piece_row; row++)
    {
        float z = h1 + dh * row / (float)piece_row;

        for (int col = 0; col <= piece_col; col++)
        {
            float x = sx + dx * col / (float)piece_col;
            float y = sy + dy * col / (float)piece_col;

            data.vertices[col * 2 + 0] = {{x, y, z}};
            data.vertices[col * 2 + 1] = {{x, y, z + dh / piece_row}};
        }

        cmap_shader->WorldMix(GL_QUAD_STRIP, data.v_count, data.tex_id, 1.0, &data.pass, kBlendingNone, false, &data,
                              FloodCoordFunc);
    }

    if (use_dynamic_lights && solid_mode && render_view_extra_light < 250)
    {
        // Note: dynamic lights could have been handled in the row-by-row
        //       loop above (after the cmap_shader).  However it is more
        //       efficient to handle them here, and duplicate the striping
        //       code in the DLIT_Flood function.

        float ex = current_seg->vertex_2->X;
        float ey = current_seg->vertex_2->Y;

        // compute bbox for finding dlights (use 'lit_pos' coords).
        float other_h = (face_dir > 0) ? h1 : h2;

        float along = (view_z - data.plane_h) / (view_z - other_h);

        float sx2 = view_x + along * (sx - view_x);
        float sy2 = view_y + along * (sy - view_y);
        float ex2 = view_x + along * (ex - view_x);
        float ey2 = view_y + along * (ey - view_y);

        float lx1 = HMM_MIN(HMM_MIN(sx, sx2), HMM_MIN(ex, ex2));
        float ly1 = HMM_MIN(HMM_MIN(sy, sy2), HMM_MIN(ey, ey2));
        float lx2 = HMM_MAX(HMM_MAX(sx, sx2), HMM_MAX(ex, ex2));
        float ly2 = HMM_MAX(HMM_MAX(sy, sy2), HMM_MAX(ey, ey2));

        //		LogDebug("Flood BBox size: %1.0f x %1.0f\n", lx2-lx1,
        // ly2-ly1);

        DynamicLightIterator(lx1, ly1, data.plane_h, lx2, ly2, data.plane_h, DLIT_Flood, &data);
    }
}
