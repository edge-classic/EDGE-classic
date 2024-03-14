//----------------------------------------------------------------------------
//  EDGE Lighting Shaders
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

#include "r_shader.h"

#include "epi.h"
#include "i_defs_gl.h"
#include "im_data.h"
#include "main.h"
#include "p_mobj.h"
#include "r_defs.h"
#include "r_gldefs.h"
#include "r_image.h"  // ImageCache
#include "r_misc.h"
#include "r_state.h"
#include "r_texgl.h"
#include "r_units.h"
#include "sokol_color.h"

//----------------------------------------------------------------------------
//  LIGHT IMAGES
//----------------------------------------------------------------------------

static constexpr uint8_t kLightImageCurveSize = 32;

class LightImage
{
   public:
    std::string name_;

    const Image *image_;

    RGBAColor curve_[kLightImageCurveSize];

   public:
    LightImage(const char *name, const Image *img)
        : name_(name), image_(img)
    {
    }

    ~LightImage() {}

    inline GLuint TextureId() const { return ImageCache(image_, false); }

    void MakeStandardCurve()  // TEMP CRUD
    {
        for (int i = 0; i < kLightImageCurveSize - 1; i++)
        {
            float d = i / (float)(kLightImageCurveSize - 1);

            float sq = exp(-5.44 * d * d);

            int r = (int)(255 * sq);
            int g = (int)(255 * sq);
            int b = (int)(255 * sq);

            curve_[i] = epi::MakeRGBA(r, g, b);
        }

        curve_[kLightImageCurveSize - 1] = SG_BLACK_RGBA32;
    }

    RGBAColor CurvePoint(float d, RGBAColor tint)
    {
        // d is distance away from centre, between 0.0 and 1.0

        d *= (float)kLightImageCurveSize;

        if (d >= kLightImageCurveSize - 1.01) return curve_[kLightImageCurveSize - 1];

        // linearly interpolate between curve points

        int p1 = (int)floor(d);
        int dd = (int)(256 * (d - p1));

        int r1 = epi::GetRGBARed(curve_[p1]);
        int g1 = epi::GetRGBAGreen(curve_[p1]);
        int b1 = epi::GetRGBABlue(curve_[p1]);

        int r2 = epi::GetRGBARed(curve_[p1 + 1]);
        int g2 = epi::GetRGBAGreen(curve_[p1 + 1]);
        int b2 = epi::GetRGBABlue(curve_[p1 + 1]);

        r1 = (r1 * (256 - dd) + r2 * dd) >> 8;
        g1 = (g1 * (256 - dd) + g2 * dd) >> 8;
        b1 = (b1 * (256 - dd) + b2 * dd) >> 8;

        r1 = r1 * epi::GetRGBARed(tint) / 255;
        g1 = g1 * epi::GetRGBAGreen(tint) / 255;
        b1 = b1 * epi::GetRGBABlue(tint) / 255;

        return epi::MakeRGBA(r1, g1, b1);
    }
};

static LightImage *GetLightImage(const MapObjectDefinition *info, int DL)
{
    // Intentional Const Overrides
    DynamicLightDefinition *D_info =
        (DynamicLightDefinition *)&info->dlight_[DL];

    if (!D_info->cache_data_)
    {
        // FIXME !!!! share light_image_c instances

        const char *shape = D_info->shape_.c_str();

        EPI_ASSERT(shape && strlen(shape) > 0);

        const Image *image =
            ImageLookup(shape, kImageNamespaceGraphic, kImageLookupNull);

        if (!image) FatalError("Missing dynamic light graphic: %s\n", shape);

        LightImage *lim = new LightImage(shape, image);

        lim->MakeStandardCurve();

        D_info->cache_data_ = lim;
    }

    return (LightImage *)D_info->cache_data_;
}

//----------------------------------------------------------------------------
//  DYNAMIC LIGHTS
//----------------------------------------------------------------------------

class dynlight_shader_c : public AbstractShader
{
   private:
    MapObject *mo;

    LightImage *lim[2];

   public:
    dynlight_shader_c(MapObject *object) : mo(object)
    {
        // Note: these are shared, we must not delete them
        lim[0] = GetLightImage(mo->info_, 0);
        lim[1] = GetLightImage(mo->info_, 1);
    }

    virtual ~dynlight_shader_c()
    { /* nothing to do */
    }

   private:
    inline float TexCoord(HMM_Vec2 *texc, float r, const HMM_Vec3 *lit_pos,
                          const HMM_Vec3 *normal)
    {
        float mx = mo->x;
        float my = mo->y;
        float mz = MapObjectMidZ(mo);

        MirrorCoordinate(mx, my);
        MirrorHeight(mz);

        float dx = lit_pos->X - mx;
        float dy = lit_pos->Y - my;
        float dz = lit_pos->Z - mz;

        float nx = normal->X;
        float ny = normal->Y;
        float nz = normal->Z;

        if (fabs(nz) > 50 * (fabs(nx) + fabs(ny)))
        {
            /* horizontal plane */
            texc->X = (1 + dx / r) / 2.0;
            texc->Y = (1 + dy / r) / 2.0;

            return fabs(dz) / r;
        }
        else
        {
            float n_len = sqrt(nx * nx + ny * ny + nz * nz);

            nx /= n_len;
            ny /= n_len;
            nz /= n_len;

            float dxy = nx * dy - ny * dx;

            r /= sqrt(nx * nx + ny * ny);  // correct ??

            texc->Y = (1 + dz / r) / 2.0;
            texc->X = (1 + dxy / r) / 2.0;

            return fabs(nx * dx + ny * dy + nz * dz) / r;
        }
    }

    inline float WhatRadius(int DL)
    {
        if (DL == 0) return mo->dynamic_light_.r * MirrorXYScale();

        return mo->info_->dlight_[1].radius_ * mo->dynamic_light_.r /
               mo->info_->dlight_[0].radius_ * MirrorXYScale();
    }

    inline RGBAColor WhatColor(int DL)
    {
        return (DL == 0) ? mo->dynamic_light_.color
                         : mo->info_->dlight_[1].colour_;
    }

    inline DynamicLightType WhatType(int DL)
    {
        return mo->info_->dlight_[DL].type_;
    }

   public:
    virtual void Sample(ColorMixer *col, float x, float y, float z)
    {
        float mx = mo->x;
        float my = mo->y;
        float mz = MapObjectMidZ(mo);

        MirrorCoordinate(mx, my);
        MirrorHeight(mz);

        float dx = x - mx;
        float dy = y - my;
        float dz = z - mz;

        float dist = sqrt(dx * dx + dy * dy + dz * dz);

        for (int DL = 0; DL < 2; DL++)
        {
            if (WhatType(DL) == kDynamicLightTypeNone) break;

            RGBAColor new_col =
                lim[DL]->CurvePoint(dist / WhatRadius(DL), WhatColor(DL));

            float L = mo->state_->bright / 255.0;

            if (new_col != SG_BLACK_RGBA32 && L > 1 / 256.0)
            {
                if (WhatType(DL) == kDynamicLightTypeAdd)
                    col->add_green_ive(new_col, L);
                else
                    col->modulate_green_ive(new_col, L);
            }
        }
    }

    virtual void Corner(ColorMixer *col, float nx, float ny, float nz,
                        MapObject *mod_pos, bool is_weapon)
    {
        float mx = mo->x;
        float my = mo->y;
        float mz = MapObjectMidZ(mo);

        if (is_weapon)
        {
            mx += view_cosine * 24;
            my += view_sine * 24;
        }

        MirrorCoordinate(mx, my);
        MirrorHeight(mz);

        float dx = mod_pos->x;
        float dy = mod_pos->y;
        float dz = MapObjectMidZ(mod_pos);

        MirrorCoordinate(dx, dy);
        MirrorHeight(dz);

        dx -= mx;
        dy -= my;
        dz -= mz;

        float dist = sqrt(dx * dx + dy * dy + dz * dz);

        dx /= dist;
        dy /= dist;
        dz /= dist;

        dist = HMM_MAX(1.0, dist - mod_pos->radius_ * MirrorXYScale());

        float L = 0.6 - 0.7 * (dx * nx + dy * ny + dz * nz);

        L *= mo->state_->bright / 255.0;

        for (int DL = 0; DL < 2; DL++)
        {
            if (WhatType(DL) == kDynamicLightTypeNone) break;

            RGBAColor new_col =
                lim[DL]->CurvePoint(dist / WhatRadius(DL), WhatColor(DL));

            if (new_col != SG_BLACK_RGBA32 && L > 1 / 256.0)
            {
                if (WhatType(DL) == kDynamicLightTypeAdd)
                    col->add_green_ive(new_col, L);
                else
                    col->modulate_green_ive(new_col, L);
            }
        }
    }

    virtual void WorldMix(GLuint shape, int num_vert, GLuint tex, float alpha,
                          int *pass_var, int blending, bool masked, void *data,
                          ShaderCoordinateFunction func)
    {
        for (int DL = 0; DL < 2; DL++)
        {
            if (WhatType(DL) == kDynamicLightTypeNone) break;

            bool is_additive = (WhatType(DL) == kDynamicLightTypeAdd);

            RGBAColor col = WhatColor(DL);

            float L = mo->state_->bright / 255.0;

            float R = L * epi::GetRGBARed(col) / 255.0;
            float G = L * epi::GetRGBAGreen(col) / 255.0;
            float B = L * epi::GetRGBABlue(col) / 255.0;

            RendererVertex *glvert = RendererBeginUnit(
                shape, num_vert,
                (is_additive && masked) ? (GLuint)kTextureEnvironmentSkipRgb
                : is_additive           ? (GLuint)kTextureEnvironmentDisable
                                        : GL_MODULATE,
                (is_additive && !masked) ? 0 : tex, GL_MODULATE,
                lim[DL]->TextureId(), *pass_var, blending,
                *pass_var > 0 ? kRGBANoValue
                              : mo->subsector_->sector->properties.fog_color,
                mo->subsector_->sector->properties.fog_density);

            for (int v_idx = 0; v_idx < num_vert; v_idx++)
            {
                RendererVertex *dest = glvert + v_idx;

                HMM_Vec3 lit_pos;

                (*func)(data, v_idx, &dest->position, dest->rgba_color,
                        &dest->texture_coordinates[0], &dest->normal, &lit_pos);

                float dist = TexCoord(&dest->texture_coordinates[1],
                                      WhatRadius(DL), &lit_pos, &dest->normal);

                float ity = exp(-5.44 * dist * dist);

                dest->rgba_color[0] = R * ity;
                dest->rgba_color[1] = G * ity;
                dest->rgba_color[2] = B * ity;
                dest->rgba_color[3] = alpha;
            }

            RendererEndUnit(num_vert);

            (*pass_var) += 1;
        }
    }
};

AbstractShader *MakeDLightShader(MapObject *mo)
{
    return new dynlight_shader_c(mo);
}

//----------------------------------------------------------------------------
//  SECTOR GLOWS
//----------------------------------------------------------------------------

class plane_glow_c : public AbstractShader
{
   private:
    MapObject *mo;

    LightImage *lim[2];

   public:
    plane_glow_c(MapObject *_glower) : mo(_glower)
    {
        lim[0] = GetLightImage(mo->info_, 0);
        lim[1] = GetLightImage(mo->info_, 1);
    }

    virtual ~plane_glow_c()
    { /* nothing to do */
    }

   private:
    inline float Dist(const Sector *sec, float z)
    {
        if (mo->info_->glow_type_ == kSectorGlowTypeFloor)
            return fabs(sec->floor_height - z);
        else
            return fabs(sec->ceiling_height - z);  // kSectorGlowTypeCeiling
    }

    inline void TexCoord(HMM_Vec2 *texc, float r, const Sector *sec,
                         const HMM_Vec3 *lit_pos, const HMM_Vec3 *normal)
    {
        texc->X = 0.5;
        texc->Y = 0.5 + Dist(sec, lit_pos->Z) / r / 2.0;
    }

    inline float WhatRadius(int DL)
    {
        if (DL == 0) return mo->dynamic_light_.r * MirrorXYScale();

        return mo->info_->dlight_[1].radius_ * mo->dynamic_light_.r /
               mo->info_->dlight_[0].radius_ * MirrorXYScale();
    }

    inline RGBAColor WhatColor(int DL)
    {
        return (DL == 0) ? mo->dynamic_light_.color
                         : mo->info_->dlight_[1].colour_;
    }

    inline DynamicLightType WhatType(int DL)
    {
        return mo->info_->dlight_[DL].type_;
    }

   public:
    virtual void Sample(ColorMixer *col, float x, float y, float z)
    {
        const Sector *sec = mo->subsector_->sector;

        float dist = Dist(sec, z);

        for (int DL = 0; DL < 2; DL++)
        {
            if (WhatType(DL) == kDynamicLightTypeNone) break;

            RGBAColor new_col =
                lim[DL]->CurvePoint(dist / WhatRadius(DL), WhatColor(DL));

            float L = mo->state_->bright / 255.0;

            if (new_col != SG_BLACK_RGBA32 && L > 1 / 256.0)
            {
                if (WhatType(DL) == kDynamicLightTypeAdd)
                    col->add_green_ive(new_col, L);
                else
                    col->modulate_green_ive(new_col, L);
            }
        }
    }

    virtual void Corner(ColorMixer *col, float nx, float ny, float nz,
                        MapObject *mod_pos, bool is_weapon)
    {
        const Sector *sec = mo->subsector_->sector;

        float dz = (mo->info_->glow_type_ == kSectorGlowTypeFloor) ? +1 : -1;
        float dist;

        if (is_weapon)
        {
            float weapon_z =
                mod_pos->z + mod_pos->height_ * mod_pos->info_->shotheight_;

            if (mo->info_->glow_type_ == kSectorGlowTypeFloor)
                dist = weapon_z - sec->floor_height;
            else
                dist = sec->ceiling_height - weapon_z;
        }
        else if (mo->info_->glow_type_ == kSectorGlowTypeFloor)
            dist = mod_pos->z - sec->floor_height;
        else
            dist = sec->ceiling_height - (mod_pos->z + mod_pos->height_);

        dist = HMM_MAX(1.0, fabs(dist));

        float L = 0.6 - 0.7 * (dz * nz);

        L *= mo->state_->bright / 255.0;

        for (int DL = 0; DL < 2; DL++)
        {
            if (WhatType(DL) == kDynamicLightTypeNone) break;

            RGBAColor new_col =
                lim[DL]->CurvePoint(dist / WhatRadius(DL), WhatColor(DL));

            if (new_col != SG_BLACK_RGBA32 && L > 1 / 256.0)
            {
                if (WhatType(DL) == kDynamicLightTypeAdd)
                    col->add_green_ive(new_col, L);
                else
                    col->modulate_green_ive(new_col, L);
            }
        }
    }

    virtual void WorldMix(GLuint shape, int num_vert, GLuint tex, float alpha,
                          int *pass_var, int blending, bool masked, void *data,
                          ShaderCoordinateFunction func)
    {
        const Sector *sec = mo->subsector_->sector;

        for (int DL = 0; DL < 2; DL++)
        {
            if (WhatType(DL) == kDynamicLightTypeNone) break;

            bool is_additive = (WhatType(DL) == kDynamicLightTypeAdd);

            RGBAColor col = WhatColor(DL);

            float L = mo->state_->bright / 255.0;

            float R = L * epi::GetRGBARed(col) / 255.0;
            float G = L * epi::GetRGBAGreen(col) / 255.0;
            float B = L * epi::GetRGBABlue(col) / 255.0;

            RendererVertex *glvert = RendererBeginUnit(
                shape, num_vert,
                (is_additive && masked) ? (GLuint)kTextureEnvironmentSkipRgb
                : is_additive           ? (GLuint)kTextureEnvironmentDisable
                                        : GL_MODULATE,
                (is_additive && !masked) ? 0 : tex, GL_MODULATE,
                lim[DL]->TextureId(), *pass_var, blending,
                *pass_var > 0 ? kRGBANoValue
                              : mo->subsector_->sector->properties.fog_color,
                mo->subsector_->sector->properties.fog_density);

            for (int v_idx = 0; v_idx < num_vert; v_idx++)
            {
                RendererVertex *dest = glvert + v_idx;

                HMM_Vec3 lit_pos;

                (*func)(data, v_idx, &dest->position, dest->rgba_color,
                        &dest->texture_coordinates[0], &dest->normal, &lit_pos);

                TexCoord(&dest->texture_coordinates[1], WhatRadius(DL), sec,
                         &lit_pos, &dest->normal);

                dest->rgba_color[0] = R;
                dest->rgba_color[1] = G;
                dest->rgba_color[2] = B;
                dest->rgba_color[3] = alpha;
            }

            RendererEndUnit(num_vert);

            (*pass_var) += 1;
        }
    }
};

AbstractShader *MakePlaneGlow(MapObject *mo) { return new plane_glow_c(mo); }

//----------------------------------------------------------------------------
//  WALL GLOWS
//----------------------------------------------------------------------------

class wall_glow_c : public AbstractShader
{
   private:
    Line      *ld;
    MapObject *mo;

    float norm_x, norm_y;  // normal

    LightImage *lim[2];

    inline float Dist(float x, float y)
    {
        return (ld->vertex_1->X - x) * norm_x + (ld->vertex_1->Y - y) * norm_y;
    }

    inline void TexCoord(HMM_Vec2 *texc, float r, const Sector *sec,
                         const HMM_Vec3 *lit_pos, const HMM_Vec3 *normal)
    {
        texc->X = 0.5;
        texc->Y = 0.5 + Dist(lit_pos->X, lit_pos->Y) / r / 2.0;
    }

    inline float WhatRadius(int DL)
    {
        if (DL == 0) return mo->dynamic_light_.r * MirrorXYScale();

        return mo->info_->dlight_[1].radius_ * mo->dynamic_light_.r /
               mo->info_->dlight_[0].radius_ * MirrorXYScale();
    }

    inline RGBAColor WhatColor(int DL)
    {
        return (DL == 0) ? mo->dynamic_light_.color
                         : mo->info_->dlight_[1].colour_;
    }

    inline DynamicLightType WhatType(int DL)
    {
        return mo->info_->dlight_[DL].type_;
    }

   public:
    wall_glow_c(MapObject *_glower) : mo(_glower)
    {
        EPI_ASSERT(mo->dynamic_light_.glow_wall);
        ld     = mo->dynamic_light_.glow_wall;
        norm_x = (ld->vertex_1->Y - ld->vertex_2->Y) / ld->length;
        norm_y = (ld->vertex_2->X - ld->vertex_1->X) / ld->length;
        // Note: these are shared, we must not delete them
        lim[0] = GetLightImage(mo->info_, 0);
        lim[1] = GetLightImage(mo->info_, 1);
    }

    virtual ~wall_glow_c()
    { /* nothing to do */
    }

    virtual void Sample(ColorMixer *col, float x, float y, float z)
    {
        float dist = Dist(x, y);

        float L = std::log1p(dist);

        L *= mo->state_->bright / 255.0;

        for (int DL = 0; DL < 2; DL++)
        {
            if (WhatType(DL) == kDynamicLightTypeNone) break;

            RGBAColor new_col =
                lim[DL]->CurvePoint(dist / WhatRadius(DL), WhatColor(DL));

            if (new_col != SG_BLACK_RGBA32 && L > 1 / 256.0)
            {
                if (WhatType(DL) == kDynamicLightTypeAdd)
                    col->add_green_ive(new_col, L);
                else
                    col->modulate_green_ive(new_col, L);
            }
        }
    }

    virtual void Corner(ColorMixer *col, float nx, float ny, float nz,
                        MapObject *mod_pos, bool is_weapon = false)
    {
        float dist = Dist(mod_pos->x, mod_pos->y);

        float L = std::log1p(dist);

        L *= mo->state_->bright / 255.0;

        for (int DL = 0; DL < 2; DL++)
        {
            if (WhatType(DL) == kDynamicLightTypeNone) break;

            RGBAColor new_col =
                lim[DL]->CurvePoint(dist / WhatRadius(DL), WhatColor(DL));

            if (new_col != SG_BLACK_RGBA32 && L > 1 / 256.0)
            {
                if (WhatType(DL) == kDynamicLightTypeAdd)
                    col->add_green_ive(new_col, L);
                else
                    col->modulate_green_ive(new_col, L);
            }
        }
    }

    virtual void WorldMix(GLuint shape, int num_vert, GLuint tex, float alpha,
                          int *pass_var, int blending, bool masked, void *data,
                          ShaderCoordinateFunction func)
    {
        const Sector *sec = mo->subsector_->sector;

        for (int DL = 0; DL < 2; DL++)
        {
            if (WhatType(DL) == kDynamicLightTypeNone) break;

            bool is_additive = (WhatType(DL) == kDynamicLightTypeAdd);

            RGBAColor col = WhatColor(DL);

            float L = mo->state_->bright / 255.0;

            float R = L * epi::GetRGBARed(col) / 255.0;
            float G = L * epi::GetRGBAGreen(col) / 255.0;
            float B = L * epi::GetRGBABlue(col) / 255.0;

            RendererVertex *glvert = RendererBeginUnit(
                shape, num_vert,
                (is_additive && masked) ? (GLuint)kTextureEnvironmentSkipRgb
                : is_additive           ? (GLuint)kTextureEnvironmentDisable
                                        : GL_MODULATE,
                (is_additive && !masked) ? 0 : tex, GL_MODULATE,
                lim[DL]->TextureId(), *pass_var, blending,
                *pass_var > 0 ? kRGBANoValue
                              : mo->subsector_->sector->properties.fog_color,
                mo->subsector_->sector->properties.fog_density);

            for (int v_idx = 0; v_idx < num_vert; v_idx++)
            {
                RendererVertex *dest = glvert + v_idx;

                HMM_Vec3 lit_pos;

                (*func)(data, v_idx, &dest->position, dest->rgba_color,
                        &dest->texture_coordinates[0], &dest->normal, &lit_pos);

                TexCoord(&dest->texture_coordinates[1], WhatRadius(DL), sec,
                         &lit_pos, &dest->normal);

                dest->rgba_color[0] = R;
                dest->rgba_color[1] = G;
                dest->rgba_color[2] = B;
                dest->rgba_color[3] = alpha;
            }

            RendererEndUnit(num_vert);

            (*pass_var) += 1;
        }
    }
};

AbstractShader *MakeWallGlow(MapObject *mo) { return new wall_glow_c(mo); }

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
