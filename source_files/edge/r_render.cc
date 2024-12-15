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
#include "p_tick.h"
#include "r_colormap.h"
#include "r_defs.h"
#include "r_effects.h"
#include "r_gldefs.h"
#include "r_image.h"
#include "r_misc.h"
#include "r_modes.h"
#include "r_occlude.h"
#include "r_shader.h"
#include "r_sky.h"
#include "r_things.h"
#include "r_units.h"

static constexpr float kDoomYSlope     = 0.525f;
static constexpr float kDoomYSlopeFull = 0.625f;

static constexpr float kWavetableIncrement = 0.0009765625f;

EDGE_DEFINE_CONSOLE_VARIABLE(debug_hall_of_mirrors, "0", kConsoleVariableFlagCheat)
EDGE_DEFINE_CONSOLE_VARIABLE(force_flat_lighting, "0", kConsoleVariableFlagArchive)

extern ConsoleVariable draw_culling;

static Sector *front_sector;
static Sector *back_sector;

unsigned int root_node;

int detail_level       = 1;
int use_dynamic_lights = 0;

std::unordered_set<AbstractShader *> seen_dynamic_lights;

static int  swirl_pass   = 0;
static bool thick_liquid = false;

float view_x_slope;
float view_y_slope;

static float wave_now;    // value for doing wave table lookups
static float plane_z_bob; // for floor/ceiling bob DDFSECT stuff

// -ES- 1999/03/20 Different right & left side clip angles, for asymmetric FOVs.
BAMAngle clip_left, clip_right;
BAMAngle clip_scope;

MapObject *view_camera_map_object;

float widescreen_view_width_multiplier;

static int check_coordinates[12][4] = {{kBoundingBoxRight, kBoundingBoxTop, kBoundingBoxLeft, kBoundingBoxBottom},
                                       {kBoundingBoxRight, kBoundingBoxTop, kBoundingBoxLeft, kBoundingBoxTop},
                                       {kBoundingBoxRight, kBoundingBoxBottom, kBoundingBoxLeft, kBoundingBoxTop},
                                       {0},
                                       {kBoundingBoxLeft, kBoundingBoxTop, kBoundingBoxLeft, kBoundingBoxBottom},
                                       {0},
                                       {kBoundingBoxRight, kBoundingBoxBottom, kBoundingBoxRight, kBoundingBoxTop},
                                       {0},
                                       {kBoundingBoxLeft, kBoundingBoxTop, kBoundingBoxRight, kBoundingBoxBottom},
                                       {kBoundingBoxLeft, kBoundingBoxBottom, kBoundingBoxRight, kBoundingBoxBottom},
                                       {kBoundingBoxLeft, kBoundingBoxBottom, kBoundingBoxRight, kBoundingBoxTop}};

extern float sprite_skew;

ViewHeightZone view_height_zone;

// common stuff

static Subsector *current_subsector;
static Seg       *current_seg;

static bool solid_mode;

static std::list<DrawSubsector *> draw_subsector_list;

// ========= MIRROR STUFF ===========

static constexpr uint8_t kMaximumMirrors = 3;

static inline void ClipPlaneHorizontalLine(GLdouble *p, const HMM_Vec2 &s, const HMM_Vec2 &e)
{
    p[0] = e.Y - s.Y;
    p[1] = s.X - e.X;
    p[2] = 0.0f;
    p[3] = e.X * s.Y - s.X * e.Y;
}

static inline void ClipPlaneEyeAngle(GLdouble *p, BAMAngle ang)
{
    HMM_Vec2 s, e;

    s = {{view_x, view_y}};

    e = {{view_x + epi::BAMCos(ang), view_y + epi::BAMSin(ang)}};

    ClipPlaneHorizontalLine(p, s, e);
}

class MirrorInfo
{
  public:
    DrawMirror *draw_mirror_;

    float xc_, xx_, xy_; // x' = xc + x*xx + y*xy
    float yc_, yx_, yy_; // y' = yc + x*yx + y*yy
    float zc_, z_scale_; // z' = zc + z*z_scale

    float xy_scale_;

    BAMAngle tc_;

  public:
    void ComputeMirror()
    {
        Seg *seg = draw_mirror_->seg;

        float sdx = seg->vertex_2->X - seg->vertex_1->X;
        float sdy = seg->vertex_2->Y - seg->vertex_1->Y;

        float len_p2 = seg->length * seg->length;

        float A = (sdx * sdx - sdy * sdy) / len_p2;
        float B = (sdx * sdy * 2.0) / len_p2;

        xx_ = A;
        xy_ = B;
        yx_ = B;
        yy_ = -A;

        xc_ = seg->vertex_1->X * (1.0 - A) - seg->vertex_1->Y * B;
        yc_ = seg->vertex_1->Y * (1.0 + A) - seg->vertex_1->X * B;

        tc_ = seg->angle << 1;

        zc_       = 0;
        z_scale_  = 1.0f;
        xy_scale_ = 1.0f;
    }

    float GetAlong(const Line *ld, float x, float y)
    {
        if (fabs(ld->delta_x) >= fabs(ld->delta_y))
            return (x - ld->vertex_1->X) / ld->delta_x;
        else
            return (y - ld->vertex_1->Y) / ld->delta_y;
    }

    void ComputePortal()
    {
        Seg  *seg   = draw_mirror_->seg;
        Line *other = seg->linedef->portal_pair;

        EPI_ASSERT(other);

        float ax1 = seg->vertex_1->X;
        float ay1 = seg->vertex_1->Y;

        float ax2 = seg->vertex_2->X;
        float ay2 = seg->vertex_2->Y;

        // find corresponding coords on partner line
        float along1 = GetAlong(seg->linedef, ax1, ay1);
        float along2 = GetAlong(seg->linedef, ax2, ay2);

        float bx1 = other->vertex_2->X - other->delta_x * along1;
        float by1 = other->vertex_2->Y - other->delta_y * along1;

        float bx2 = other->vertex_2->X - other->delta_x * along2;
        float by2 = other->vertex_2->Y - other->delta_y * along2;

        // compute rotation angle
        tc_ = kBAMAngle180 + PointToAngle(0, 0, other->delta_x, other->delta_y) - seg->angle;

        xx_ = epi::BAMCos(tc_);
        xy_ = epi::BAMSin(tc_);
        yx_ = -epi::BAMSin(tc_);
        yy_ = epi::BAMCos(tc_);

        // scaling
        float a_len = seg->length;
        float b_len = PointToDistance(bx1, by1, bx2, by2);

        xy_scale_ = a_len / HMM_MAX(1, b_len);

        xx_ *= xy_scale_;
        xy_ *= xy_scale_;
        yx_ *= xy_scale_;
        yy_ *= xy_scale_;

        // translation
        xc_ = ax1 - bx1 * xx_ - by1 * xy_;
        yc_ = ay1 - bx1 * yx_ - by1 * yy_;

        // heights
        float a_h = (seg->front_sector->interpolated_ceiling_height - seg->front_sector->interpolated_floor_height);
        float b_h = (other->front_sector->interpolated_ceiling_height - other->front_sector->interpolated_floor_height);

        z_scale_ = a_h / HMM_MAX(1, b_h);
        zc_      = seg->front_sector->interpolated_floor_height - other->front_sector->interpolated_floor_height * z_scale_;
    }

    void Compute()
    {
        if (draw_mirror_->is_portal)
            ComputePortal();
        else
            ComputeMirror();
    }

    void Transform(float &x, float &y)
    {
        float tx = x, ty = y;

        x = xc_ + tx * xx_ + ty * xy_;
        y = yc_ + tx * yx_ + ty * yy_;
    }

    void Z_Adjust(float &z)
    {
        z = zc_ + z * z_scale_;
    }

    void Turn(BAMAngle &ang)
    {
        ang = (draw_mirror_->is_portal) ? (ang - tc_) : (tc_ - ang);
    }
};

static MirrorInfo active_mirrors[kMaximumMirrors];

int total_active_mirrors = 0;

void MirrorCoordinate(float &x, float &y)
{
    for (int i = total_active_mirrors - 1; i >= 0; i--)
        active_mirrors[i].Transform(x, y);
}

void MirrorHeight(float &z)
{
    for (int i = total_active_mirrors - 1; i >= 0; i--)
        active_mirrors[i].Z_Adjust(z);
}

void MirrorAngle(BAMAngle &ang)
{
    for (int i = total_active_mirrors - 1; i >= 0; i--)
        active_mirrors[i].Turn(ang);
}

float MirrorXYScale(void)
{
    float result = 1.0f;

    for (int i = total_active_mirrors - 1; i >= 0; i--)
        result *= active_mirrors[i].xy_scale_;

    return result;
}

float MirrorZScale(void)
{
    float result = 1.0f;

    for (int i = total_active_mirrors - 1; i >= 0; i--)
        result *= active_mirrors[i].z_scale_;

    return result;
}

bool MirrorReflective(void)
{
    if (total_active_mirrors == 0)
        return false;

    bool result = false;

    for (int i = total_active_mirrors - 1; i >= 0; i--)
        if (!active_mirrors[i].draw_mirror_->is_portal)
            result = !result;

    return result;
}

static bool MirrorSegOnPortal(Seg *seg)
{
    if (total_active_mirrors == 0)
        return false;

    if (seg->miniseg)
        return false;

    DrawMirror *def = active_mirrors[total_active_mirrors - 1].draw_mirror_;

    if (def->is_portal)
    {
        if (seg->linedef == def->seg->linedef->portal_pair)
            return true;
    }
    else // mirror
    {
        if (seg->linedef == def->seg->linedef)
            return true;
    }

    return false;
}

static void MirrorSetClippers()
{
    global_render_state->Disable(GL_CLIP_PLANE0);
    global_render_state->Disable(GL_CLIP_PLANE1);
    global_render_state->Disable(GL_CLIP_PLANE2);
    global_render_state->Disable(GL_CLIP_PLANE3);
    global_render_state->Disable(GL_CLIP_PLANE4);
    global_render_state->Disable(GL_CLIP_PLANE5);

    if (total_active_mirrors == 0)
        return;

    // setup planes for left and right sides of innermost mirror.
    // Angle clipping has ensured that for multiple mirrors all
    // later mirrors are limited to the earlier mirrors.

    MirrorInfo &inner = active_mirrors[total_active_mirrors - 1];

    GLdouble left_p[4];
    GLdouble right_p[4];

    ClipPlaneEyeAngle(left_p, inner.draw_mirror_->left);
    ClipPlaneEyeAngle(right_p, inner.draw_mirror_->right + kBAMAngle180);

    global_render_state->Enable(GL_CLIP_PLANE0);
    global_render_state->Enable(GL_CLIP_PLANE1);

    glClipPlane(GL_CLIP_PLANE0, left_p);
    glClipPlane(GL_CLIP_PLANE1, right_p);

    // now for each mirror, setup a clip plane that removes
    // everything that gets projected in front of that mirror.

    for (int i = 0; i < total_active_mirrors; i++)
    {
        MirrorInfo &mir = active_mirrors[i];

        HMM_Vec2 v1, v2;

        v1 = {{mir.draw_mirror_->seg->vertex_1->X, mir.draw_mirror_->seg->vertex_1->Y}};
        v2 = {{mir.draw_mirror_->seg->vertex_2->X, mir.draw_mirror_->seg->vertex_2->Y}};

        for (int k = i - 1; k >= 0; k--)
        {
            if (!active_mirrors[k].draw_mirror_->is_portal)
            {
                HMM_Vec2 tmp;
                tmp = v1;
                v1  = v2;
                v2  = tmp;
            }

            active_mirrors[k].Transform(v1.X, v1.Y);
            active_mirrors[k].Transform(v2.X, v2.Y);
        }

        GLdouble front_p[4];

        ClipPlaneHorizontalLine(front_p, v2, v1);

        global_render_state->Enable(GL_CLIP_PLANE2 + i);

        glClipPlane(GL_CLIP_PLANE2 + i, front_p);
    }
}

static void MirrorPush(DrawMirror *mir)
{
    EPI_ASSERT(mir);
    EPI_ASSERT(mir->seg);

    EPI_ASSERT(total_active_mirrors < kMaximumMirrors);

    active_mirrors[total_active_mirrors].draw_mirror_ = mir;
    active_mirrors[total_active_mirrors].Compute();

    total_active_mirrors++;

    MirrorSetClippers();
}

static void MirrorPop()
{
    EPI_ASSERT(total_active_mirrors > 0);

    total_active_mirrors--;

    MirrorSetClippers();
}

float Slope_GetHeight(SlopePlane *slope, float x, float y)
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

    int pass;
    int blending;

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
        *rgb = epi::MakeRGBA((uint8_t)(255.0f / data->R * render_view_red_multiplier), (uint8_t)(255.0f / data->G * render_view_green_multiplier), 
            (uint8_t)(255.0f / data->B * render_view_blue_multiplier), epi::GetRGBAAlpha(*rgb));
    }
    else
    {
        *rgb = epi::MakeRGBA((uint8_t)(data->R * render_view_red_multiplier), (uint8_t)(data->G * render_view_green_multiplier), 
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

    int pass;
    int blending;

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
        *rgb = epi::MakeRGBA((uint8_t)(255.0f / data->R * render_view_red_multiplier), (uint8_t)(255.0f / data->G * render_view_green_multiplier), 
            (uint8_t)(255.0f / data->B * render_view_blue_multiplier), epi::GetRGBAAlpha(*rgb));
    }
    else
    {
        *rgb = epi::MakeRGBA((uint8_t)(data->R * render_view_red_multiplier), (uint8_t)(data->G * render_view_green_multiplier), 
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
    if (!mo->info_->dlight_[0].leaky_ && !data->mid_masked &&
        !(mo->subsector_->sector->floor_vertex_slope || mo->subsector_->sector->ceiling_vertex_slope))
    {
        float mx = mo->x;
        float my = mo->y;

        MirrorCoordinate(mx, my);

        float dist = (mx - data->div.x) * data->div.delta_y - (my - data->div.y) * data->div.delta_x;

        if (dist < 0)
            return;
    }

    EPI_ASSERT(mo->dynamic_light_.shader);

    int blending = (data->blending & ~kBlendingAlpha) | kBlendingAdd;

    mo->dynamic_light_.shader->WorldMix(GL_POLYGON, data->v_count, data->tex_id, data->trans, &data->pass, blending,
                                        data->mid_masked, data, WallCoordFunc);
}

static void GLOWLIT_Wall(MapObject *mo, void *dataptr)
{
    WallCoordinateData *data = (WallCoordinateData *)dataptr;

    EPI_ASSERT(mo->dynamic_light_.shader);

    int blending = (data->blending & ~kBlendingAlpha) | kBlendingAdd;

    mo->dynamic_light_.shader->WorldMix(GL_POLYGON, data->v_count, data->tex_id, data->trans, &data->pass, blending,
                                        data->mid_masked, data, WallCoordFunc);
}

static void DLIT_Plane(MapObject *mo, void *dataptr)
{
    PlaneCoordinateData *data = (PlaneCoordinateData *)dataptr;

    // light behind the plane ?
    if (!mo->info_->dlight_[0].leaky_ &&
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

    int blending = (data->blending & ~kBlendingAlpha) | kBlendingAdd;

    mo->dynamic_light_.shader->WorldMix(GL_POLYGON, data->v_count, data->tex_id, data->trans, &data->pass, blending,
                                        false /* masked */, data, PlaneCoordFunc);
}

static void GLOWLIT_Plane(MapObject *mo, void *dataptr)
{
    PlaneCoordinateData *data = (PlaneCoordinateData *)dataptr;

    EPI_ASSERT(mo->dynamic_light_.shader);

    int blending = (data->blending & ~kBlendingAlpha) | kBlendingAdd;

    mo->dynamic_light_.shader->WorldMix(GL_POLYGON, data->v_count, data->tex_id, data->trans, &data->pass, blending,
                                        false, data, PlaneCoordFunc);
}

static constexpr uint8_t kMaximumEdgeVertices = 20;

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

    (void)opaque;

    if (surf->override_properties)
        props = surf->override_properties;

    if (!props)
        props = dfloor->properties;

    float trans = surf->translucency;

    EPI_ASSERT(image);

    // (need to load the image to know the opacity)
    GLuint tex_id = ImageCache(image, true, render_view_effect_colormap);

    // ignore non-solid walls in solid mode (& vice versa)
    if ((trans < 0.99f || image->opacity_ >= kOpacityMasked) == solid_mode)
        return;

    // must determine bbox _before_ mirror flipping
    float v_bbox[4];

    BoundingBoxClear(v_bbox);
    BoundingBoxAddPoint(v_bbox, x1, y1);
    BoundingBoxAddPoint(v_bbox, x2, y2);

    MirrorCoordinate(x1, y1);
    MirrorCoordinate(x2, y2);

    if (MirrorReflective())
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
    if (!force_flat_lighting.d_ && current_map->episode_->lighting_ == kLightingModelDoom && props->light_level > 0)
    {
        if (AlmostEquals(current_seg->vertex_1->Y, current_seg->vertex_2->Y))
            lit_adjust -= 16;
        else if (AlmostEquals(current_seg->vertex_1->X, current_seg->vertex_2->X))
            lit_adjust += 16;
    }

    float total_w = image->ScaledWidthTotal();
    float total_h = image->ScaledHeightTotal();

    /* convert tex_x1 and tex_x2 from world coords to texture coords */
    tex_x1 = (tex_x1 * surf->x_matrix.X) / total_w;
    tex_x2 = (tex_x2 * surf->x_matrix.X) / total_w;

    float tx0    = tex_x1;
    float tx_mul = tex_x2 - tex_x1;

    MirrorHeight(tex_top_h);

    float ty_mul = surf->y_matrix.Y / (total_h * MirrorZScale());
    float ty0    = image->Top() - tex_top_h * ty_mul;

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

        MirrorHeight(vertices[v_count].Z);

        v_count++;
    }

    for (int RI = right_num - 1; RI >= 0; RI--)
    {
        vertices[v_count].X = x2;
        vertices[v_count].Y = y2;
        vertices[v_count].Z = right_h[RI];

        MirrorHeight(vertices[v_count].Z);

        v_count++;
    }

    int blending;

    if (trans >= 0.99f && image->opacity_ == kOpacitySolid)
        blending = kBlendingNone;
    else if (trans < 0.11f || image->opacity_ == kOpacityComplex)
        blending = kBlendingMasked;
    else
        blending = kBlendingLess;

    if (trans < 0.99f || image->opacity_ == kOpacityComplex)
        blending |= kBlendingAlpha;

    // -AJA- 2006-06-22: fix for midmask wrapping bug
    if (mid_masked &&
        (!current_seg->linedef->special || AlmostEquals(current_seg->linedef->special->s_yspeed_,
                                                        0.0f))) // Allow vertical scroller midmasks - Dasho
        blending |= kBlendingClampY;

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
        data.tx0        = data.tx0 + 25;
        data.ty0        = data.ty0 + 25;
        swirl_pass      = 2;
        int   old_blend = data.blending;
        float old_dt    = data.trans;
        data.blending   = kBlendingMasked | kBlendingAlpha;
        data.trans      = 0.33f;
        trans           = 0.33f;
        cmap_shader->WorldMix(GL_POLYGON, data.v_count, data.tex_id, trans, &data.pass, data.blending, false, &data,
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
        if (uncapped_frames.d_ && !menu_active && !paused && !time_stop_active && !erraticism_active &&
            !rts_menu_active)
            opening = HMM_Lerp(smov->old_opening, fractional_tic, smov->opening);
        else
            opening = smov->opening;
    }

    Line *ld = current_seg->linedef;

    /// float im_width = wt->surface->image->ScaledWidthActual();

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
            return; /* NOT REACHED */
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

    if (uncapped_frames.d_ && !AlmostEquals(surf->old_offset.X, surf->offset.X) && !paused && !menu_active &&
        !time_stop_active && !erraticism_active)
        offx = fmod(HMM_Lerp(surf->old_offset.X, fractional_tic, surf->offset.X), surf->image->actual_width_);
    else
        offx = surf->offset.X;
    if (uncapped_frames.d_ && !AlmostEquals(surf->old_offset.Y, surf->offset.Y) && !paused && !menu_active &&
        !time_stop_active && !erraticism_active)
        offy = fmod(HMM_Lerp(surf->old_offset.Y, fractional_tic, surf->offset.Y), surf->image->actual_height_);
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

    bool opaque = (!seg->back_sector) || (surf->translucency >= 0.99f && image->opacity_ == kOpacitySolid);

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
        return image->ScaledHeightActual();
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

    float slope_fh = 0;
    float slope_ch = 0;
    float other_fh = 0;
    float other_ch = 0;
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

    slope_ch = sec->interpolated_ceiling_height;
    slope_ceil = &sec->ceiling;
    if (sec->height_sector)
    {
        if (view_height_zone == kHeightZoneA && view_z > sec->height_sector->interpolated_ceiling_height)
        {
            slope_ceil = &sec->height_sector->ceiling;
        }
        else if (view_height_zone == kHeightZoneC && view_z < sec->height_sector->interpolated_floor_height)
        {
            slope_ch = sec->height_sector->interpolated_floor_height;
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

        other_ch = other->interpolated_ceiling_height;
        other_ceil = &other->ceiling;
        if (other->height_sector)
        {
            if (view_height_zone == kHeightZoneA && view_z > other->height_sector->interpolated_ceiling_height)
            {
                other_ceil = &other->height_sector->ceiling;
            }
            else if (view_height_zone == kHeightZoneC && view_z < other->height_sector->interpolated_floor_height)
            {
                other_ch = other->height_sector->interpolated_floor_height;
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
            c2 = f2 + (sd->middle.image->ScaledHeightActual() / sd->middle.y_matrix.Y);
        }
        else
        {
            c2 = c1 + sd->middle_mask_offset;
            f2 = c2 - (sd->middle.image->ScaledHeightActual() / sd->middle.y_matrix.Y);
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
        if (sec != other && !(sec->height_sector || other->height_sector))// && !lower_invis)
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
    *rgb = epi::MakeRGBA((uint8_t)(data->R * render_view_red_multiplier), (uint8_t)(data->G * render_view_green_multiplier), 
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
    if (!mo->info_->dlight_[0].leaky_ &&
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

    int blending = kBlendingAdd;

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

        mo->dynamic_light_.shader->WorldMix(GL_QUAD_STRIP, data->v_count, data->tex_id, 1.0, &data->pass, blending,
                                            false, data, FloodCoordFunc);
    }
}

static void EmulateFloodPlane(const DrawFloor *dfloor, const Sector *flood_ref, int face_dir, float h1, float h2)
{
    EDGE_ZoneScoped;

    (void)dfloor;

    if (total_active_mirrors > 0)
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
    data.image_w = surf->image->ScaledWidthActual();
    data.image_h = surf->image->ScaledHeightActual();

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

static void RenderSeg(DrawFloor *dfloor, Seg *seg, bool mirror_sub = false)
{
    //
    // Analyses floor/ceiling heights, and add corresponding walls/floors
    // to the drawfloor.  Returns true if the whole region was "solid".
    //
    current_seg = seg;

    EPI_ASSERT(!seg->miniseg && seg->linedef);

    // mark the segment on the automap
    seg->linedef->flags |= kLineFlagMapped;

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
            current_seg->back_subsector &&
            b_fh > f_fh &&
            b_fh < view_z)
        {
            EmulateFloodPlane(dfloor, current_seg->back_subsector->sector, +1,
                            f_fh,
                            b_fh);
        }

        if (!debug_hall_of_mirrors.d_ && solid_mode && dfloor->is_highest && sd->top.image == nullptr &&
            current_seg->back_subsector &&
            b_ch < f_ch &&
            b_ch > view_z)
        {
            EmulateFloodPlane(dfloor, current_seg->back_subsector->sector, -1,
                            b_ch,
                            f_ch);
        }
    }
}

static void RendererWalkBspNode(unsigned int bspnum);

static void UpdateSectorInterpolation(Sector *sector)
{
    if (uncapped_frames.d_ && !time_stop_active && !paused && !erraticism_active && !menu_active && !rts_menu_active)
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

static void RendererWalkMirror(DrawSubsector *dsub, Seg *seg, BAMAngle left, BAMAngle right, bool is_portal)
{
    DrawMirror *mir = GetDrawMirror();
    mir->seg        = seg;
    mir->draw_subsectors.clear();

    mir->left      = view_angle + left;
    mir->right     = view_angle + right;
    mir->is_portal = is_portal;

    dsub->mirrors.push_back(mir);

#if defined(EDGE_GL_ES2)
    // GL4ES mirror fix for renderlist
    gl4es_flush();
#endif

    // push mirror (translation matrix)
    MirrorPush(mir);

    Subsector *save_sub = current_subsector;

    BAMAngle save_clip_L = clip_left;
    BAMAngle save_clip_R = clip_right;
    BAMAngle save_scope  = clip_scope;

    clip_left  = left;
    clip_right = right;
    clip_scope = left - right;

    // perform another BSP walk
    RendererWalkBspNode(root_node);

    current_subsector = save_sub;

    clip_left  = save_clip_L;
    clip_right = save_clip_R;
    clip_scope = save_scope;

    // pop mirror
    MirrorPop();

#if defined(EDGE_GL_ES2)
    // GL4ES mirror fix for renderlist
    gl4es_flush();
#endif
}

//
// RendererWalkSeg
//
// Visit a single seg of the subsector, and for one-sided lines update
// the 1D occlusion buffer.
//
static void RendererWalkSeg(DrawSubsector *dsub, Seg *seg)
{
    EDGE_ZoneScoped;

    // ignore segs sitting on current mirror
    if (MirrorSegOnPortal(seg))
        return;

    float sx1 = seg->vertex_1->X;
    float sy1 = seg->vertex_1->Y;

    float sx2 = seg->vertex_2->X;
    float sy2 = seg->vertex_2->Y;

    // when there are active mirror planes, segs not only need to
    // be flipped across them but also clipped across them.
    if (total_active_mirrors > 0)
    {
        for (int i = total_active_mirrors - 1; i >= 0; i--)
        {
            active_mirrors[i].Transform(sx1, sy1);
            active_mirrors[i].Transform(sx2, sy2);

            if (!active_mirrors[i].draw_mirror_->is_portal)
            {
                float tmp_x = sx1;
                sx1         = sx2;
                sx2         = tmp_x;
                float tmp_y = sy1;
                sy1         = sy2;
                sy2         = tmp_y;
            }

            Seg *clipper = active_mirrors[i].draw_mirror_->seg;

            DividingLine div;

            div.x       = clipper->vertex_1->X;
            div.y       = clipper->vertex_1->Y;
            div.delta_x = clipper->vertex_2->X - div.x;
            div.delta_y = clipper->vertex_2->Y - div.y;

            int s1 = PointOnDividingLineSide(sx1, sy1, &div);
            int s2 = PointOnDividingLineSide(sx2, sy2, &div);

            // seg lies completely in front of clipper?
            if (s1 == 0 && s2 == 0)
                return;

            if (s1 != s2)
            {
                // seg crosses clipper, need to split it
                float ix, iy;

                ComputeIntersection(&div, sx1, sy1, sx2, sy2, &ix, &iy);

                if (s2 == 0)
                    sx2 = ix, sy2 = iy;
                else
                    sx1 = ix, sy1 = iy;
            }
        }
    }

    bool precise = total_active_mirrors > 0;
    if (!precise && seg->linedef)
    {
        precise = (seg->linedef->flags & kLineFlagMirror) || (seg->linedef->portal_pair);
    }

    BAMAngle angle_L = PointToAngle(view_x, view_y, sx1, sy1, precise);
    BAMAngle angle_R = PointToAngle(view_x, view_y, sx2, sy2, precise);

    // Clip to view edges.

    BAMAngle span = angle_L - angle_R;

    // back side ?
    if (span >= kBAMAngle180)
        return;

    angle_L -= view_angle;
    angle_R -= view_angle;

    if (clip_scope != kBAMAngle180)
    {
        BAMAngle tspan1 = angle_L - clip_right;
        BAMAngle tspan2 = clip_left - angle_R;

        if (tspan1 > clip_scope)
        {
            // Totally off the left edge?
            if (tspan2 >= kBAMAngle180)
                return;

            angle_L = clip_left;
        }

        if (tspan2 > clip_scope)
        {
            // Totally off the left edge?
            if (tspan1 >= kBAMAngle180)
                return;

            angle_R = clip_right;
        }

        span = angle_L - angle_R;
    }

    // The seg is in the view range,
    // but not necessarily visible.

#if 1
    // check if visible
    if (span > (kBAMAngle1 / 4) && OcclusionTest(angle_R, angle_L))
    {
        return;
    }
#endif

    dsub->visible = true;

    if (seg->miniseg || span == 0)
        return;

    if (total_active_mirrors < kMaximumMirrors)
    {
        if (seg->linedef->flags & kLineFlagMirror)
        {
            RendererWalkMirror(dsub, seg, angle_L, angle_R, false);
            OcclusionSet(angle_R, angle_L);
            return;
        }
        else if (seg->linedef->portal_pair)
        {
            RendererWalkMirror(dsub, seg, angle_L, angle_R, true);
            OcclusionSet(angle_R, angle_L);
            return;
        }
    }

    DrawSeg *dseg = GetDrawSeg();
    dseg->seg     = seg;

    dsub->segs.push_back(dseg);

    Sector *fsector = seg->front_subsector->sector;
    Sector *bsector = nullptr;

    if (seg->back_subsector)
        bsector = seg->back_subsector->sector;

    // only 1 sided walls affect the 1D occlusion buffer

    if (seg->linedef->blocked)
        OcclusionSet(angle_R, angle_L);

    if (bsector)
        UpdateSectorInterpolation(bsector);

    // --- handle sky (using the depth buffer) ---
    float f_fh = 0;
    float f_ch = 0;
    float b_fh = 0;
    float b_ch = 0;
    MapSurface *f_floor = nullptr;
    MapSurface *f_ceil = nullptr;
    MapSurface *b_floor = nullptr;
    MapSurface *b_ceil = nullptr;

    if (!fsector->height_sector)
    {
        f_fh = fsector->interpolated_floor_height;
        f_floor = &fsector->floor;
        f_ch = fsector->interpolated_ceiling_height;
        f_ceil = &fsector->ceiling;
    }
    else
    {
        if (view_height_zone == kHeightZoneA && view_z > fsector->height_sector->interpolated_ceiling_height)
        {
            f_fh = fsector->height_sector->interpolated_ceiling_height;
            f_ch = fsector->interpolated_ceiling_height;
            f_floor = &fsector->height_sector->floor;
            f_ceil = &fsector->height_sector->ceiling;
        }
        else if (view_height_zone == kHeightZoneC && view_z < fsector->height_sector->interpolated_floor_height)
        {
            f_fh = fsector->interpolated_floor_height;
            f_ch = fsector->height_sector->interpolated_floor_height;
            f_floor = &fsector->height_sector->floor;
            f_ceil = &fsector->height_sector->ceiling;
        }
        else
        {
            f_fh = fsector->height_sector->interpolated_floor_height;
            f_ch = fsector->height_sector->interpolated_ceiling_height;
            f_floor = &fsector->floor;
            f_ceil = &fsector->ceiling;
        }
    }

    if (bsector)
    {
        if (!bsector->height_sector)
        {
            b_fh = bsector->interpolated_floor_height;
            b_floor = &bsector->floor;
            b_ch = bsector->interpolated_ceiling_height;
            b_ceil = &bsector->ceiling;
        }
        else
        {
            if (view_height_zone == kHeightZoneA && view_z > bsector->height_sector->interpolated_ceiling_height)
            {
                b_fh = bsector->height_sector->interpolated_ceiling_height;
                b_ch = bsector->interpolated_ceiling_height;
                b_floor = &bsector->height_sector->floor;
                b_ceil = &bsector->height_sector->ceiling;
            }
            else if (view_height_zone == kHeightZoneC && view_z < bsector->height_sector->interpolated_floor_height)
            {
                b_fh = bsector->interpolated_floor_height;
                b_ch = bsector->height_sector->interpolated_floor_height;
                b_floor = &bsector->height_sector->floor;
                b_ceil = &bsector->height_sector->ceiling;
            }
            else
            {
                b_fh = bsector->height_sector->interpolated_floor_height;
                b_ch = bsector->height_sector->interpolated_ceiling_height;
                b_floor = &bsector->floor;
                b_ceil = &bsector->ceiling;
            }
        }
    }

    if (bsector && EDGE_IMAGE_IS_SKY(*f_floor) && EDGE_IMAGE_IS_SKY(*b_floor) && seg->sidedef->bottom.image == nullptr)
    {
        if (f_fh < b_fh)
        {
            RenderSkyWall(seg, f_fh, b_fh);
        }
    }

    if (EDGE_IMAGE_IS_SKY(*f_ceil))
    {
        if (f_ch < fsector->sky_height &&
            (!bsector || !EDGE_IMAGE_IS_SKY(*b_ceil) ||
             b_fh >= f_ch))
        {
            RenderSkyWall(seg, f_ch, fsector->sky_height);
        }
        else if (bsector && EDGE_IMAGE_IS_SKY(*b_ceil))
        {
            float max_f = HMM_MAX(f_fh, b_fh);

            if (b_ch <= max_f && max_f < fsector->sky_height)
            {
                RenderSkyWall(seg, max_f, fsector->sky_height);
            }
        }
    }
    // -AJA- 2004/08/29: Emulate Sky-Flooding TRICK
    else if (!debug_hall_of_mirrors.d_ && bsector && EDGE_IMAGE_IS_SKY(*b_ceil) &&
             seg->sidedef->top.image == nullptr &&
             b_ch < f_ch)
    {
        RenderSkyWall(seg, b_ch, f_ch);
    }
}

//
// RendererCheckBBox
//
// Checks BSP node/subtree bounding box.
// Returns true if some part of the bbox might be visible.
//
// Placed here to be close to RendererWalkSeg(), which has similiar angle
// clipping stuff in it.
//
bool RendererCheckBBox(float *bspcoord)
{
    EDGE_ZoneScoped;

    if (total_active_mirrors > 0)
    {
        // a flipped bbox may no longer be axis aligned, hence we
        // need to find the bounding area of the transformed box.
        static float new_bbox[4];

        BoundingBoxClear(new_bbox);

        for (int p = 0; p < 4; p++)
        {
            float tx = bspcoord[(p & 1) ? kBoundingBoxLeft : kBoundingBoxRight];
            float ty = bspcoord[(p & 2) ? kBoundingBoxBottom : kBoundingBoxTop];

            MirrorCoordinate(tx, ty);

            BoundingBoxAddPoint(new_bbox, tx, ty);
        }

        bspcoord = new_bbox;
    }

    int boxx, boxy;

    // Find the corners of the box
    // that define the edges from current viewpoint.
    if (view_x <= bspcoord[kBoundingBoxLeft])
        boxx = 0;
    else if (view_x < bspcoord[kBoundingBoxRight])
        boxx = 1;
    else
        boxx = 2;

    if (view_y >= bspcoord[kBoundingBoxTop])
        boxy = 0;
    else if (view_y > bspcoord[kBoundingBoxBottom])
        boxy = 1;
    else
        boxy = 2;

    int boxpos = (boxy << 2) + boxx;

    if (boxpos == 5)
        return true;

    float x1 = bspcoord[check_coordinates[boxpos][0]];
    float y1 = bspcoord[check_coordinates[boxpos][1]];
    float x2 = bspcoord[check_coordinates[boxpos][2]];
    float y2 = bspcoord[check_coordinates[boxpos][3]];

    // check clip list for an open space
    BAMAngle angle_L = PointToAngle(view_x, view_y, x1, y1);
    BAMAngle angle_R = PointToAngle(view_x, view_y, x2, y2);

    BAMAngle span = angle_L - angle_R;

    // Sitting on a line?
    if (span >= kBAMAngle180)
        return true;

    angle_L -= view_angle;
    angle_R -= view_angle;

    if (clip_scope != kBAMAngle180)
    {
        BAMAngle tspan1 = angle_L - clip_right;
        BAMAngle tspan2 = clip_left - angle_R;

        if (tspan1 > clip_scope)
        {
            // Totally off the left edge?
            if (tspan2 >= kBAMAngle180)
                return false;

            angle_L = clip_left;
        }

        if (tspan2 > clip_scope)
        {
            // Totally off the right edge?
            if (tspan1 >= kBAMAngle180)
                return false;

            angle_R = clip_right;
        }

        if (angle_L == angle_R)
            return false;

        if (draw_culling.d_)
        {
            float closest = 1000000.0f;
            float check   = PointToSegDistance({{x1, y1}}, {{x2, y1}}, {{view_x, view_y}});
            if (check < closest)
                closest = check;
            check = PointToSegDistance({{x1, y1}}, {{x1, y2}}, {{view_x, view_y}});
            if (check < closest)
                closest = check;
            check = PointToSegDistance({{x2, y1}}, {{x2, y2}}, {{view_x, view_y}});
            if (check < closest)
                closest = check;
            check = PointToSegDistance({{x1, y2}}, {{x2, y2}}, {{view_x, view_y}});
            if (check < closest)
                closest = check;

            if (closest > (renderer_far_clip.f_ + 500.0f))
                return false;
        }
    }

    return !OcclusionTest(angle_R, angle_L);
}

static void RenderPlane(DrawFloor *dfloor, float h, MapSurface *surf, int face_dir)
{
    EDGE_ZoneScoped;

    float orig_h = h;

    MirrorHeight(h);

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

    float trans = surf->translucency;

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

    // ignore non-solid planes in solid_mode (& vice versa)
    if ((trans < 0.99f || surf->image->opacity_ >= kOpacityMasked) == solid_mode)
        return;

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

                MirrorHeight(z);
            }

            MirrorCoordinate(x, y);

            vertices[v_count].X = x;
            vertices[v_count].Y = y;
            vertices[v_count].Z = z;

            v_count++;
        }
    }

    int blending;

    if (trans >= 0.99f && surf->image->opacity_ == kOpacitySolid)
        blending = kBlendingNone;
    else if (trans < 0.11f || surf->image->opacity_ == kOpacityComplex)
        blending = kBlendingMasked;
    else
        blending = kBlendingLess;

    if (trans < 0.99f || surf->image->opacity_ == kOpacityComplex)
        blending |= kBlendingAlpha;

    PlaneCoordinateData data;

    data.v_count  = v_count;
    data.vertices = vertices;
    data.R = data.G = data.B = 255;
    if (uncapped_frames.d_ && !AlmostEquals(surf->old_offset.X, surf->offset.X) && !paused && !menu_active &&
        !time_stop_active && !erraticism_active)
        data.tx0 = fmod(HMM_Lerp(surf->old_offset.X, fractional_tic, surf->offset.X), surf->image->actual_width_);
    else
        data.tx0 = surf->offset.X;
    if (uncapped_frames.d_ && !AlmostEquals(surf->old_offset.Y, surf->offset.Y) && !paused && !menu_active &&
        !time_stop_active && !erraticism_active)
        data.ty0 = fmod(HMM_Lerp(surf->old_offset.Y, fractional_tic, surf->offset.Y), surf->image->actual_height_);
    else
        data.ty0 = surf->offset.Y;
    data.image_w    = surf->image->ScaledWidthActual();
    data.image_h    = surf->image->ScaledHeightActual();
    data.x_mat      = surf->x_matrix;
    data.y_mat      = surf->y_matrix;
    float mir_scale = MirrorXYScale();
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
        data.tx0        = data.tx0 + 25;
        data.ty0        = data.ty0 + 25;
        swirl_pass      = 2;
        int   old_blend = data.blending;
        float old_dt    = data.trans;
        data.blending   = kBlendingMasked | kBlendingAlpha;
        data.trans      = 0.33f;
        trans           = 0.33f;
        cmap_shader->WorldMix(GL_POLYGON, data.v_count, data.tex_id, trans, &data.pass, data.blending, false, &data,
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

static inline void AddNewDrawFloor(DrawSubsector *dsub, Extrafloor *ef, float floor_height, float ceiling_height,
                                   float top_h, MapSurface *floor, MapSurface *ceil, RegionProperties *props)
{
    DrawFloor *dfloor;

    dfloor = GetDrawFloor();

    dfloor->is_highest      = false;
    dfloor->is_lowest       = false;
    dfloor->render_next     = nullptr;
    dfloor->render_previous = nullptr;
    dfloor->floor           = nullptr;
    dfloor->ceiling         = nullptr;
    dfloor->extrafloor      = nullptr;
    dfloor->properties      = nullptr;
    dfloor->things          = nullptr;

    dfloor->floor_height   = floor_height;
    dfloor->ceiling_height = ceiling_height;
    dfloor->top_height     = top_h;
    dfloor->floor          = floor;
    dfloor->ceiling        = ceil;
    dfloor->extrafloor     = ef;
    dfloor->properties     = props;

    // link it in, height order

    dsub->floors.push_back(dfloor);

    // link it in, rendering order (very important)

    if (dsub->render_floors == nullptr || floor_height > view_z)
    {
        // add to head
        dfloor->render_next     = dsub->render_floors;
        dfloor->render_previous = nullptr;

        if (dsub->render_floors)
            dsub->render_floors->render_previous = dfloor;

        dsub->render_floors = dfloor;
    }
    else
    {
        // add to tail
        DrawFloor *tail;

        for (tail = dsub->render_floors; tail->render_next; tail = tail->render_next)
        { /* nothing here */
        }

        dfloor->render_next     = nullptr;
        dfloor->render_previous = tail;

        tail->render_next = dfloor;
    }
}

//
// RendererWalkSubsector
//
// Visit a subsector, and collect information, such as where the
// walls, planes (ceilings & floors) and things need to be drawn.
//
static void RendererWalkSubsector(int num)
{
    EDGE_ZoneScoped;

    Subsector *sub    = &level_subsectors[num];
    Sector    *sector = sub->sector;

    // store subsector in a global var for other functions to use
    current_subsector = sub;

#if (DEBUG >= 1)
    LogDebug("\nVISITING SUBSEC %d (sector %d)\n\n", num, sub->sector - level_sectors);
#endif

    DrawSubsector *K = GetDrawSub();
    K->subsector     = sub;
    K->visible       = false;
    K->sorted        = false;
    K->render_floors = nullptr;

    K->floors.clear();
    K->segs.clear();
    K->mirrors.clear();

    UpdateSectorInterpolation(sector);

    // --- handle sky (using the depth buffer) ---

    if (!sector->height_sector)
    {
        if (EDGE_IMAGE_IS_SKY(sub->sector->floor) && view_z > sub->sector->interpolated_floor_height)
        {
            RenderSkyPlane(sub, sub->sector->interpolated_floor_height);
        }

        if (EDGE_IMAGE_IS_SKY(sub->sector->ceiling) && view_z < sub->sector->sky_height)
        {
            RenderSkyPlane(sub, sub->sector->sky_height);
        }
    }

    float floor_h = sector->interpolated_floor_height;
    float ceil_h  = sector->interpolated_ceiling_height;

    MapSurface *floor_s = &sector->floor;
    MapSurface *ceil_s  = &sector->ceiling;

    RegionProperties *props = sector->active_properties;

    // Boom compatibility -- deep water FX
    if (sector->height_sector != nullptr)
    {
        if (view_height_zone == kHeightZoneA && view_z > sector->height_sector->interpolated_ceiling_height)
        {
            floor_h = sector->height_sector->interpolated_ceiling_height;
            ceil_h = sector->interpolated_ceiling_height;
            floor_s = &sector->height_sector->floor;
            ceil_s  = &sector->height_sector->ceiling;
            props   = sector->height_sector->active_properties;
        }
        else if (view_height_zone == kHeightZoneC && view_z < sector->height_sector->interpolated_floor_height)
        {
            floor_h = sector->interpolated_floor_height;
            ceil_h  = sector->height_sector->interpolated_floor_height;
            floor_s = &sector->height_sector->floor;
            ceil_s  = &sector->height_sector->ceiling;
            props   = sector->height_sector->active_properties;
        }
        else
        {
            floor_h = sector->height_sector->interpolated_floor_height;
            ceil_h  = sector->height_sector->interpolated_ceiling_height;
        }
        if (EDGE_IMAGE_IS_SKY(*floor_s) && view_z > floor_h)
        {
            RenderSkyPlane(sub, floor_h);
        }
        if (EDGE_IMAGE_IS_SKY(*ceil_s) && view_z < sub->sector->sky_height)
        {
            RenderSkyPlane(sub, sub->sector->sky_height);
        }
    }
    // -AJA- 2004/04/22: emulate the Deep-Water TRICK
    else if (sub->deep_water_reference != nullptr)
    {
        floor_h = sub->deep_water_reference->interpolated_floor_height;
        floor_s = &sub->deep_water_reference->floor;

        ceil_h = sub->deep_water_reference->interpolated_ceiling_height;
        ceil_s = &sub->deep_water_reference->ceiling;
    }

    // the OLD method of Boom deep water (the BOOMTEX flag)
    Extrafloor *boom_ef = sector->bottom_liquid ? sector->bottom_liquid : sector->bottom_extrafloor;
    if (boom_ef && (boom_ef->extrafloor_definition->type_ & kExtraFloorTypeBoomTex))
        floor_s = &boom_ef->extrafloor_line->front_sector->floor;

    // add in each extrafloor, traversing strictly upwards

    Extrafloor *S = sector->bottom_extrafloor;
    Extrafloor *L = sector->bottom_liquid;

    while (S || L)
    {
        Extrafloor *C = nullptr;

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
        if (C->bottom_height < floor_h || C->bottom_height > sector->interpolated_ceiling_height)
            continue;

        AddNewDrawFloor(K, C, floor_h, C->bottom_height, C->top_height, floor_s, C->bottom, C->properties);

        floor_s = C->top;
        floor_h = C->top_height;
    }

    AddNewDrawFloor(K, nullptr, floor_h, ceil_h, ceil_h, floor_s, ceil_s, props);

    K->floors[0]->is_lowest                     = true;
    K->floors[K->floors.size() - 1]->is_highest = true;

    // handle each sprite in the subsector.  Must be done before walls,
    // since the wall code will update the 1D occlusion buffer.

    if (draw_culling.d_)
    {
        bool skip = true;

        for (Seg *seg = sub->segs; seg; seg = seg->subsector_next)
        {
            if (MirrorSegOnPortal(seg))
                continue;

            float sx1 = seg->vertex_1->X;
            float sy1 = seg->vertex_1->Y;

            float sx2 = seg->vertex_2->X;
            float sy2 = seg->vertex_2->Y;

            if (PointToSegDistance({{sx1, sy1}}, {{sx2, sy2}}, {{view_x, view_y}}) <= (renderer_far_clip.f_ + 500.0f))
            {
                skip = false;
                break;
            }
        }

        if (!skip)
        {
            for (MapObject *mo = sub->thing_list; mo; mo = mo->subsector_next_)
            {
                RendererWalkThing(K, mo);
            }
            // clip 1D occlusion buffer.
            for (Seg *seg = sub->segs; seg; seg = seg->subsector_next)
            {
                RendererWalkSeg(K, seg);
            }

            // add drawsub to list (closest -> furthest)
            if (total_active_mirrors > 0)
                active_mirrors[total_active_mirrors - 1].draw_mirror_->draw_subsectors.push_back(K);
            else
                draw_subsector_list.push_back(K);
        }
    }
    else
    {
        for (MapObject *mo = sub->thing_list; mo; mo = mo->subsector_next_)
        {
            RendererWalkThing(K, mo);
        }
        // clip 1D occlusion buffer.
        for (Seg *seg = sub->segs; seg; seg = seg->subsector_next)
        {
            RendererWalkSeg(K, seg);
        }

        // add drawsub to list (closest -> furthest)
        if (total_active_mirrors > 0)
            active_mirrors[total_active_mirrors - 1].draw_mirror_->draw_subsectors.push_back(K);
        else
            draw_subsector_list.push_back(K);
    }
}

static void RenderSubsector(DrawSubsector *dsub, bool mirror_sub = false);

static void RenderSubList(std::list<DrawSubsector *> &dsubs, bool for_mirror = false)
{
    // draw all solid walls and planes
    solid_mode = true;
    StartUnitBatch(solid_mode);

    std::list<DrawSubsector *>::iterator FI; // Forward Iterator

    for (FI = dsubs.begin(); FI != dsubs.end(); FI++)
        RenderSubsector(*FI, for_mirror);

    FinishUnitBatch();

    // draw all sprites and masked/translucent walls/planes
    solid_mode = false;
    StartUnitBatch(solid_mode);

    std::list<DrawSubsector *>::reverse_iterator RI;

    for (RI = dsubs.rbegin(); RI != dsubs.rend(); RI++)
        RenderSubsector(*RI, for_mirror);

    FinishUnitBatch();
}

static void DrawMirrorPolygon(DrawMirror *mir)
{
    float alpha = 0.15 + 0.10 * total_active_mirrors;

    Line *ld = mir->seg->linedef;
    EPI_ASSERT(ld);
    RGBAColor unit_col;

    if (ld->special)
    {
        uint8_t col_r = epi::GetRGBARed(ld->special->fx_color_);
        uint8_t col_g = epi::GetRGBAGreen(ld->special->fx_color_);
        uint8_t col_b = epi::GetRGBABlue(ld->special->fx_color_);

        // looks better with reduced color in multiple reflections
        float reduce = 1.0f / (1 + 1.5 * total_active_mirrors);

        unit_col = epi::MakeRGBA((uint8_t)(reduce * col_r), (uint8_t)(reduce * col_g), 
            (uint8_t)(reduce * col_b), (uint8_t)(alpha * 255.0f));
    }
    else
       unit_col = epi::MakeRGBA(255, 0, 0, (uint8_t)(alpha * 255.0f));

    float x1 = mir->seg->vertex_1->X;
    float y1 = mir->seg->vertex_1->Y;
    float z1 = ld->front_sector->interpolated_floor_height;

    float x2 = mir->seg->vertex_2->X;
    float y2 = mir->seg->vertex_2->Y;
    float z2 = ld->front_sector->interpolated_ceiling_height;

    MirrorCoordinate(x1, y1);
    MirrorCoordinate(x2, y2);

    RendererVertex *glvert = BeginRenderUnit(GL_POLYGON, 4, GL_MODULATE, 0, (GLuint)kTextureEnvironmentDisable, 0,
                                                 0, alpha < 0.99f ? kBlendingAlpha : kBlendingNone);

    glvert->rgba = unit_col;
    glvert++->position = {{x1, y1, z1}};
    glvert->rgba = unit_col;
    glvert++->position = {{x1, y1, z2}};
    glvert->rgba = unit_col;
    glvert++->position = {{x2, y2, z2}};
    glvert->rgba = unit_col;
    glvert->position = {{x2, y2, z1}};

    EndRenderUnit(4);
}

static void DrawPortalPolygon(DrawMirror *mir)
{
    Line *ld = mir->seg->linedef;
    EPI_ASSERT(ld);

    const MapSurface *surf = &mir->seg->sidedef->middle;

    if (!surf->image || !ld->special || !(ld->special->portal_effect_ & kPortalEffectTypeStandard))
    {
        DrawMirrorPolygon(mir);
        return;
    }

    // set texture
    GLuint tex_id = ImageCache(surf->image);

    // set colour & alpha
    float alpha = ld->special->translucency_ * surf->translucency;

    RGBAColor unit_col = ld->special->fx_color_;
    epi::SetRGBAAlpha(unit_col, alpha);

    // get polygon coordinates
    float x1 = mir->seg->vertex_1->X;
    float y1 = mir->seg->vertex_1->Y;
    float z1 = ld->front_sector->interpolated_floor_height;

    float x2 = mir->seg->vertex_2->X;
    float y2 = mir->seg->vertex_2->Y;
    float z2 = ld->front_sector->interpolated_ceiling_height;

    MirrorCoordinate(x1, y1);
    MirrorCoordinate(x2, y2);

    // get texture coordinates
    float total_w = surf->image->ScaledWidthTotal();
    float total_h = surf->image->ScaledHeightTotal();

    float tx1 = mir->seg->offset;
    float tx2 = tx1 + mir->seg->length;

    float ty1 = 0;
    float ty2 = (z2 - z1);

    tx1 = tx1 * surf->x_matrix.X / total_w;
    tx2 = tx2 * surf->x_matrix.X / total_w;

    ty1 = ty1 * surf->y_matrix.Y / total_h;
    ty2 = ty2 * surf->y_matrix.Y / total_h;

    RendererVertex *glvert = BeginRenderUnit(GL_POLYGON, 4, GL_MODULATE, tex_id, (GLuint)kTextureEnvironmentDisable, 0,
                                                 0, alpha < 0.99f ? kBlendingAlpha : kBlendingNone);

    glvert->rgba = unit_col;
    glvert->position = {{x1, y1, z1}};
    glvert++->texture_coordinates[0] = {{tx1, ty1}};
    glvert->rgba = unit_col;
    glvert->position = {{x1, y1, z2}};
    glvert++->texture_coordinates[0] = {{tx1, ty2}};
    glvert->rgba = unit_col;
    glvert->position = {{x2, y2, z2}};
    glvert++->texture_coordinates[0] = {{tx2, ty2}};
    glvert->rgba = unit_col;
    glvert->position = {{x2, y2, z1}};
    glvert->texture_coordinates[0] = {{tx2, ty1}};

    EndRenderUnit(4);
}

static void RenderMirror(DrawMirror *mir)
{
    // mark the segment on the automap
    mir->seg->linedef->flags |= kLineFlagMapped;

    FinishUnitBatch();

#if defined(EDGE_GL_ES2)
    // GL4ES mirror fix for renderlist
    gl4es_flush();
#endif

    MirrorPush(mir);
    {
        RenderSubList(mir->draw_subsectors, true);
    }
    MirrorPop();

    StartUnitBatch(false);

    if (mir->is_portal)
        DrawPortalPolygon(mir);
    else
        DrawMirrorPolygon(mir);

    FinishUnitBatch();

#if defined(EDGE_GL_ES2)
    // GL4ES mirror fix for renderlist
    gl4es_flush();
#endif

    solid_mode = true;
    StartUnitBatch(solid_mode);
}

static void RenderSubsector(DrawSubsector *dsub, bool mirror_sub)
{
    EDGE_ZoneScoped;

    Subsector *sub = dsub->subsector;

#if (DEBUG >= 1)
    LogDebug("\nREVISITING SUBSEC %d\n\n", (int)(sub - subsectors));
#endif

    current_subsector = sub;

    if (solid_mode)
    {
        std::list<DrawMirror *>::iterator MRI;

        for (MRI = dsub->mirrors.begin(); MRI != dsub->mirrors.end(); MRI++)
        {
            RenderMirror(*MRI);
        }
    }

    current_subsector = sub;

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

        if (!solid_mode)
        {
            SortRenderThings(dfloor);
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

    glClear(GL_DEPTH_BUFFER_BIT);

    solid_mode = false;
    StartUnitBatch(solid_mode);

    RenderWeaponModel(pl);

    FinishUnitBatch();
}

//
// RendererWalkBspNode
//
// Walks all subsectors below a given node, traversing subtree
// recursively, collecting information.  Just call with BSP root.
//
static void RendererWalkBspNode(unsigned int bspnum)
{
    EDGE_ZoneScoped;

    BspNode *node;
    int      side;

    // Found a subsector?
    if (bspnum & kLeafSubsector)
    {
        RendererWalkSubsector(bspnum & (~kLeafSubsector));
        return;
    }

    node = &level_nodes[bspnum];

    // Decide which side the view point is on.

    DividingLine nd_div;

    nd_div.x       = node->divider.x;
    nd_div.y       = node->divider.y;
    nd_div.delta_x = node->divider.x + node->divider.delta_x;
    nd_div.delta_y = node->divider.y + node->divider.delta_y;

    MirrorCoordinate(nd_div.x, nd_div.y);
    MirrorCoordinate(nd_div.delta_x, nd_div.delta_y);

    if (MirrorReflective())
    {
        float tx       = nd_div.x;
        nd_div.x       = nd_div.delta_x;
        nd_div.delta_x = tx;
        float ty       = nd_div.y;
        nd_div.y       = nd_div.delta_y;
        nd_div.delta_y = ty;
    }

    nd_div.delta_x -= nd_div.x;
    nd_div.delta_y -= nd_div.y;

    side = PointOnDividingLineSide(view_x, view_y, &nd_div);

    // Recursively divide front space.
    if (RendererCheckBBox(node->bounding_boxes[side]))
        RendererWalkBspNode(node->children[side]);

    // Recursively divide back space.
    if (RendererCheckBBox(node->bounding_boxes[side ^ 1]))
        RendererWalkBspNode(node->children[side ^ 1]);
}

//
// RenderTrueBsp
//
// OpenGL BSP rendering.  Initialises all structures, then walks the
// BSP tree collecting information, then renders each subsector:
// firstly front to back (drawing all solid walls & planes) and then
// from back to front (drawing everything else, sprites etc..).
//
static void RenderTrueBsp(void)
{
    EDGE_ZoneScoped;

    FuzzUpdate();

    ClearBSP();
    OcclusionClear();

    draw_subsector_list.clear();

    Player *v_player = view_camera_map_object->player_;

    // handle powerup effects and BOOM colormaps
    RendererRainbowEffect(v_player);

    SetupMatrices3d();

    glClear(GL_DEPTH_BUFFER_BIT);
    global_render_state->Enable(GL_DEPTH_TEST);

    // needed for drawing the sky
    BeginSky();

    // walk the bsp tree
    RendererWalkBspNode(root_node);

    FinishSky();

    RenderSubList(draw_subsector_list);

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

    if (FlashFirst == false)
    {
        DoWeaponModel();
    }

    global_render_state->Disable(GL_DEPTH_TEST);

    // now draw 2D stuff like psprites, and add effects
    SetupWorldMatrices2D();

    if (v_player)
    {
        RenderWeaponSprites(v_player);

        RendererColourmapEffect(v_player);
        RendererPaletteEffect(v_player);
        SetupMatrices2D();
        RenderCrosshair(v_player);
    }

    if (FlashFirst == true)
    {
        SetupMatrices3d();
        glClear(GL_DEPTH_BUFFER_BIT);
        global_render_state->Enable(GL_DEPTH_TEST);
        DoWeaponModel();
        global_render_state->Disable(GL_DEPTH_TEST);
        SetupMatrices2D();
    }

#if (DEBUG >= 3)
    LogDebug("\n\n");
#endif
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

    if (uncapped_frames.d_ && level_time_elapsed && mo->player_ && mo->interpolate_ && !paused && !menu_active &&
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

    view_subsector  = mo->subsector_;
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
    RenderTrueBsp();
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
