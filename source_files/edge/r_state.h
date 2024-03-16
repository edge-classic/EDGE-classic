//----------------------------------------------------------------------------
//  EDGE Refresh internal state variables
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

#pragma once

// Need data structure definitions.

#include "AlmostEquals.h"
#include "edge_profiling.h"
#include "i_defs_gl.h"
#include "i_system.h"
#include "m_math.h"
#include "r_defs.h"
#include "w_flat.h"

//
// Lookup tables for map data.
//
extern int     total_level_vertexes;
extern Vertex *level_vertexes;

extern int     total_level_sectors;
extern Sector *level_sectors;

extern int        total_level_subsectors;
extern Subsector *level_subsectors;

extern int         total_level_extrafloors;
extern Extrafloor *level_extrafloors;

extern int      total_level_nodes;
extern BspNode *level_nodes;

extern int   total_level_lines;
extern Line *level_lines;

extern int   total_level_sides;
extern Side *level_sides;

//
// POV data.
//
extern float view_x;
extern float view_y;
extern float view_z;

extern BAMAngle view_angle;

// -ES- 1999/03/20 Added these.
// Angles that are used for linedef clipping.
// Nearly the same as leftangle/rightangle, but slightly rounded to fit
// view_angletox lookups, and converted to BAM format.

// angles used for clipping
extern BAMAngle clip_left, clip_right;

// the scope of the clipped area (clip_left-clip_right).
// kBAMAngle180 disables polar clipping
extern BAMAngle clip_scope;

// the most extreme angles of the view
extern float view_x_slope, view_y_slope;

extern ECFrameStats ec_frame_stats;

class RenderState
{
  public:
    void Enable(GLenum cap, bool enabled = true)
    {
        switch (cap)
        {
        case GL_TEXTURE_2D:
            if (enable_texture_2d_[active_texture_ - GL_TEXTURE0] == enabled)
                return;
            enable_texture_2d_[active_texture_ - GL_TEXTURE0] = enabled;
            break;
        case GL_FOG:
            if (enable_fog_ == enabled)
                return;
            enable_fog_ = enabled;
            break;
        case GL_ALPHA_TEST:
            if (enable_alpha_test_ == enabled)
                return;
            enable_alpha_test_ = enabled;
            break;
        case GL_BLEND:
            if (enable_blend_ == enabled)
                return;
            enable_blend_ = enabled;
            break;
        case GL_CULL_FACE:
            if (enable_cull_face_ == enabled)
                return;
            enable_cull_face_ = enabled;
            break;
        case GL_SCISSOR_TEST:
            if (enable_scissor_test_ == enabled)
                return;
            enable_scissor_test_ = enabled;
            break;
        default:
            FatalError("Unknown GL State %i", cap);
        }

        if (enabled)
        {
            glEnable(cap);
        }
        else
        {
            glDisable(cap);
        }

        ec_frame_stats.draw_state_change++;
    }

    void Disable(GLenum cap)
    {
        Enable(cap, false);
    }

    void DepthMask(bool enable)
    {
        if (depth_mask_ == enable)
        {
            return;
        }

        depth_mask_ = enable;
        glDepthMask(enable ? GL_TRUE : GL_FALSE);
        ec_frame_stats.draw_state_change++;
    }

    void CullFace(GLenum mode)
    {
        if (cull_face_ == mode)
        {
            return;
        }

        cull_face_ = mode;
        glCullFace(mode);
        ec_frame_stats.draw_state_change++;
    }

    void AlphaFunction(GLenum func, GLfloat ref)
    {
        if (func == alpha_function_ && AlmostEquals(ref, alpha_function_reference_))
        {
            return;
        }

        alpha_function_           = func;
        alpha_function_reference_ = ref;

        glAlphaFunc(alpha_function_, alpha_function_reference_);
        ec_frame_stats.draw_state_change++;
    }

    void ActiveTexture(GLenum activeTexture)
    {
        if (activeTexture == active_texture_)
        {
            return;
        }

        active_texture_ = activeTexture;
        glActiveTexture(active_texture_);
        ec_frame_stats.draw_state_change++;
    }

    void BindTexture(GLuint textureid)
    {
        GLuint index = active_texture_ - GL_TEXTURE0;
        if (bind_texture_2d_[index] == textureid)
        {
            return;
        }

        bind_texture_2d_[index] = textureid;
        glBindTexture(GL_TEXTURE_2D, textureid);
        ec_frame_stats.draw_texture_change++;
        ec_frame_stats.draw_state_change++;
    }

    void PolygonOffset(GLfloat factor, GLfloat units)
    {
        if (factor == polygon_offset_factor_ && units == polygon_offset_units_)
        {
            return;
        }

        polygon_offset_factor_ = factor;
        polygon_offset_units_  = units;
        glPolygonOffset(polygon_offset_factor_, polygon_offset_units_);
        ec_frame_stats.draw_state_change++;
    }

    void ClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
    {
        if (AlmostEquals(red, clear_red_) && AlmostEquals(green, clear_green_) && AlmostEquals(blue, clear_blue_) &&
            AlmostEquals(alpha, clear_alpha_))
        {
            return;
        }

        clear_red_   = red;
        clear_green_ = green;
        clear_blue_  = blue;
        clear_alpha_ = alpha;
        glClearColor(clear_red_, clear_green_, clear_blue_, clear_alpha_);
        ec_frame_stats.draw_state_change++;
    }

    void FogMode(GLint fogMode)
    {
        if (fog_mode_ == fogMode)
        {
            return;
        }

        fog_mode_ = fogMode;
        glFogi(GL_FOG_MODE, fog_mode_);
        ec_frame_stats.draw_state_change++;
    }

    void FogColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
    {
        if (AlmostEquals(red, fog_color_[0]) && AlmostEquals(green, fog_color_[1]) &&
            AlmostEquals(blue, fog_color_[2]) && AlmostEquals(alpha, fog_color_[3]))
        {
            return;
        }

        fog_color_[0] = red;
        fog_color_[1] = green;
        fog_color_[2] = blue;
        fog_color_[3] = alpha;

        glFogfv(GL_FOG_COLOR, fog_color_);
        ec_frame_stats.draw_state_change++;
    }

    void FogStart(GLfloat start)
    {
        if (fog_start_ == start)
        {
            return;
        }

        fog_start_ = start;
        glFogf(GL_FOG_START, fog_start_);
        ec_frame_stats.draw_state_change++;
    }

    void FogEnd(GLfloat end)
    {
        if (fog_end_ == end)
        {
            return;
        }

        fog_end_ = end;
        glFogf(GL_FOG_END, fog_end_);
        ec_frame_stats.draw_state_change++;
    }

    void FogDensity(GLfloat density)
    {
        if (fog_density_ == density)
        {
            return;
        }

        fog_density_ = density;
        glFogf(GL_FOG_DENSITY, fog_density_);
        ec_frame_stats.draw_state_change++;
    }

    void BlendFunction(GLenum sfactor, GLenum dfactor)
    {
        if (blend_source_factor_ == sfactor && blend_destination_factor_ == dfactor)
        {
            return;
        }

        blend_source_factor_      = sfactor;
        blend_destination_factor_ = dfactor;
        glBlendFunc(blend_source_factor_, blend_destination_factor_);
        ec_frame_stats.draw_state_change++;
    }

    void TextureEnvironmentMode(GLint param)
    {
        GLuint index = active_texture_ - GL_TEXTURE0;

        if (texture_environment_mode_[index] == param)
        {
            return;
        }

        texture_environment_mode_[index] = param;
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, texture_environment_mode_[index]);
        ec_frame_stats.draw_state_change++;
    }

    void TextureEnvironmentCombineRGB(GLint param)
    {
        GLuint index = active_texture_ - GL_TEXTURE0;

        if (texture_environment_combine_rgb_[index] == param)
        {
            return;
        }

        texture_environment_combine_rgb_[index] = param;
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, texture_environment_combine_rgb_[index]);
        ec_frame_stats.draw_state_change++;
    }

    void TextureEnvironmentSource0RGB(GLint param)
    {
        GLuint index = active_texture_ - GL_TEXTURE0;

        if (texture_environment_source_0_rgb_[index] == param)
        {
            return;
        }

        texture_environment_source_0_rgb_[index] = param;
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, texture_environment_source_0_rgb_[index]);
        ec_frame_stats.draw_state_change++;
    }

    void TextureWrapT(GLint param)
    {
        GLuint index = active_texture_ - GL_TEXTURE0;

        if (texture_wrap_t_[index] == param)
        {
            return;
        }

        texture_wrap_t_[index] = param;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, texture_wrap_t_[index]);
        ec_frame_stats.draw_state_change++;
    }

    void ResetDefaultState()
    {
        Disable(GL_BLEND);
        BlendFunction(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        Disable(GL_ALPHA_TEST);

        DepthMask(true);

        CullFace(GL_BACK);
        Disable(GL_CULL_FACE);

        Disable(GL_FOG);

        PolygonOffset(0, 0);

        for (int i = 0; i < 2; i++)
        {
            bind_texture_2d_[i]                  = 0;
            texture_environment_mode_[i]         = 0;
            texture_environment_combine_rgb_[i]  = 0;
            texture_environment_source_0_rgb_[i] = 0;
            texture_wrap_t_[i]                   = 0;
        }
    }

    void SetDefaultStateFull()
    {
        enable_blend_ = false;
        glDisable(GL_BLEND);
        ec_frame_stats.draw_state_change++;

        blend_source_factor_      = GL_SRC_ALPHA;
        blend_destination_factor_ = GL_ONE_MINUS_SRC_ALPHA;
        glBlendFunc(blend_source_factor_, blend_destination_factor_);
        ec_frame_stats.draw_state_change++;

        for (int i = 0; i < 2; i++)
        {
            enable_texture_2d_[i] = false;
            bind_texture_2d_[i]   = 0;
            glActiveTexture(GL_TEXTURE0 + i);
            ec_frame_stats.draw_state_change++;
            glBindTexture(GL_TEXTURE_2D, 0);
            ec_frame_stats.draw_texture_change++;
            ec_frame_stats.draw_state_change++;
            glDisable(GL_TEXTURE_2D);
            ec_frame_stats.draw_state_change++;

            texture_environment_mode_[i]         = 0;
            texture_environment_combine_rgb_[i]  = 0;
            texture_environment_source_0_rgb_[i] = 0;
            texture_wrap_t_[i]                   = 0;
        }

        active_texture_ = GL_TEXTURE0;
        glActiveTexture(active_texture_);
        ec_frame_stats.draw_state_change++;

        enable_alpha_test_ = false;
        glDisable(GL_ALPHA_TEST);
        ec_frame_stats.draw_state_change++;

        alpha_function_           = GL_GREATER;
        alpha_function_reference_ = 0.0f;

        glAlphaFunc(alpha_function_, alpha_function_reference_);
        ec_frame_stats.draw_state_change++;

        depth_mask_ = true;
        glDepthMask(GL_TRUE);
        ec_frame_stats.draw_state_change++;

        cull_face_ = GL_BACK;
        glCullFace(cull_face_);
        ec_frame_stats.draw_state_change++;
        enable_cull_face_ = false;
        glDisable(GL_CULL_FACE);
        ec_frame_stats.draw_state_change++;

        clear_red_   = 0.0f;
        clear_green_ = 0.0f;
        clear_blue_  = 0.0f;
        clear_alpha_ = 1.0f;
        glClearColor(clear_red_, clear_green_, clear_blue_, clear_alpha_);
        ec_frame_stats.draw_state_change++;

        fog_mode_ = GL_LINEAR;
        glFogi(GL_FOG_MODE, fog_mode_);
        ec_frame_stats.draw_state_change++;

        fog_color_[0] = 0.0f;
        fog_color_[1] = 0.0f;
        fog_color_[2] = 0.0f;
        fog_color_[3] = 1.0f;
        glFogfv(GL_FOG_COLOR, fog_color_);
        ec_frame_stats.draw_state_change++;

        enable_fog_ = false;
        glDisable(GL_FOG);
        ec_frame_stats.draw_state_change++;
        fog_start_ = 0.0f;
        fog_end_   = 0.0f;
        glFogf(GL_FOG_START, fog_start_);
        ec_frame_stats.draw_state_change++;
        glFogf(GL_FOG_END, fog_end_);
        ec_frame_stats.draw_state_change++;

        fog_density_ = 0.0f;
        glFogf(GL_FOG_DENSITY, fog_density_);
        ec_frame_stats.draw_state_change++;

        polygon_offset_factor_ = 0;
        polygon_offset_units_  = 0;
        glPolygonOffset(polygon_offset_factor_, polygon_offset_units_);
        ec_frame_stats.draw_state_change++;

        enable_scissor_test_ = false;
        glDisable(GL_SCISSOR_TEST);
        ec_frame_stats.draw_state_change++;
    }

    int frameStateChanges_ = 0;

  private:
    bool   enable_blend_;
    GLenum blend_source_factor_;
    GLenum blend_destination_factor_;

    bool   enable_cull_face_;
    GLenum cull_face_;

    bool enable_scissor_test_;

    GLfloat clear_red_;
    GLfloat clear_green_;
    GLfloat clear_blue_;
    GLfloat clear_alpha_;

    // texture
    bool enable_texture_2d_[2];

    GLint texture_environment_mode_[2];
    GLint texture_environment_combine_rgb_[2];
    GLint texture_environment_source_0_rgb_[2];
    GLint texture_wrap_t_[2];

    GLuint bind_texture_2d_[2];
    GLenum active_texture_;

    bool depth_mask_;

    GLfloat polygon_offset_factor_;
    GLfloat polygon_offset_units_;

    bool    enable_alpha_test_;
    GLenum  alpha_function_;
    GLfloat alpha_function_reference_;

    bool    enable_fog_;
    GLint   fog_mode_;
    GLfloat fog_start_;
    GLfloat fog_end_;
    GLfloat fog_density_;
    GLfloat fog_color_[4];
};

RenderState *RendererGetState();

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
