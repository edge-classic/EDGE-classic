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

#include <string.h>

#include <unordered_map>

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

extern std::unordered_map<GLuint, GLint> texture_clamp_s;
extern std::unordered_map<GLuint, GLint> texture_clamp_t;

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
        case GL_LIGHTING:
            if (enable_lighting_ == enabled)
                return;
            enable_lighting_ = enabled;
            break;
        case GL_COLOR_MATERIAL:
            if (enable_color_material_ == enabled)
                return;
            enable_color_material_ = enabled;
            break;
        case GL_DEPTH_TEST:
            if (enable_depth_test_ == enabled)
                return;
            enable_depth_test_ = enabled;
            break;
        case GL_STENCIL_TEST:
            if (enable_stencil_test_ == enabled)
                return;
            enable_stencil_test_ = enabled;
            break;
        case GL_LINE_SMOOTH:
            if (enable_line_smooth_ == enabled)
                return;
            enable_line_smooth_ = enabled;
            break;
        case GL_NORMALIZE:
            if (enable_normalize_ == enabled)
                return;
            enable_normalize_ = enabled;
            break;
        case GL_CLIP_PLANE0:
        case GL_CLIP_PLANE1:
        case GL_CLIP_PLANE2:
        case GL_CLIP_PLANE3:
        case GL_CLIP_PLANE4:
        case GL_CLIP_PLANE5:
            if (enable_clip_plane_[cap - GL_CLIP_PLANE0] == enabled)
                return;
            enable_clip_plane_[cap - GL_CLIP_PLANE0] = enabled;
            break;
#ifndef EDGE_GL_ES2
        case GL_POLYGON_SMOOTH:
            if (enable_polygon_smooth_ == enabled)
                return;
            enable_polygon_smooth_ = enabled;
            break;
#endif
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

    void DepthFunction(GLenum func)
    {
        if (func == depth_function_)
        {
            return;
        }

        depth_function_           = func;

        glDepthFunc(depth_function_);
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

    void GLColor(const GLfloat *color)
    {
        if (AlmostEquals(color[0], gl_color_[0]) && AlmostEquals(color[1], gl_color_[1]) &&
            AlmostEquals(color[2], gl_color_[2]) && AlmostEquals(color[3], gl_color_[3]))
        {
            return;
        }

        memcpy(gl_color_, color, 4 * sizeof(float));

        glColor4fv(gl_color_);
        ec_frame_stats.draw_state_change++;
    }

    void GLColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
    {
        if (AlmostEquals(r, gl_color_[0]) && AlmostEquals(g, gl_color_[1]) &&
            AlmostEquals(b, gl_color_[2]) && AlmostEquals(a, gl_color_[3]))
        {
            return;
        }

        gl_color_[0] = r;
        gl_color_[1] = g;
        gl_color_[2] = b;
        gl_color_[3] = a;

        glColor4fv(gl_color_);
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

    void TextureMinFilter(GLint param)
    {
        GLuint index = active_texture_ - GL_TEXTURE0;

        texture_min_filter_[index] = param;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texture_min_filter_[index]);
        ec_frame_stats.draw_state_change++;
    }

    void TextureMagFilter(GLint param)
    {
        GLuint index = active_texture_ - GL_TEXTURE0;

        texture_mag_filter_[index] = param;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texture_mag_filter_[index]);
        ec_frame_stats.draw_state_change++;
    }

    void TextureWrapS(GLint param)
    {
        GLuint index = active_texture_ - GL_TEXTURE0;

        // We do it regardless of the cached value; functions should check
        // texture environments against the appropriate unordered_map and 
        // know if a change needs to occur
        texture_wrap_s_[index] = param;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, texture_wrap_s_[index]);
        ec_frame_stats.draw_state_change++;
    }

    void TextureWrapT(GLint param)
    {
        GLuint index = active_texture_ - GL_TEXTURE0;

        // We do it regardless of the cached value; functions should check
        // texture environments against the appropriate unordered_map and 
        // know if a change needs to occur
        texture_wrap_t_[index] = param;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, texture_wrap_t_[index]);
        ec_frame_stats.draw_state_change++;
    }

    void MultiTexCoord(GLuint tex, const HMM_Vec2 *coords)
    {
        if (enable_texture_2d_[tex - GL_TEXTURE0] == false)
            return;
        if (tex == GL_TEXTURE0 && enable_texture_2d_[1] == false)
            glTexCoord2fv((GLfloat *)coords);
        else
            glMultiTexCoord2fv(tex, (GLfloat *)coords);
        ec_frame_stats.draw_state_change++;
    }

    void Hint(GLenum target, GLenum mode)
    {
        glHint(target, mode);
        ec_frame_stats.draw_state_change++;
    }

    void LineWidth(float width)
    {
        if (AlmostEquals(width, line_width_))
        {
            return;
        }
        line_width_ = width;
        glLineWidth(line_width_);
        ec_frame_stats.draw_state_change++;
    }

    void DeleteTexture(const GLuint *tex_id)
    {
        if (tex_id && *tex_id > 0)
        {
            texture_clamp_s.erase(*tex_id);
            texture_clamp_t.erase(*tex_id);
            glDeleteTextures(1, tex_id);
            // We don't need to actually perform a texture bind,
            // but these should be cleared out to ensure
            // we aren't mistakenly using a tex_id that does not
            // correlate to the same texture anymore
            bind_texture_2d_[0] = 0;
            bind_texture_2d_[1] = 0;
        }
    }

    int frameStateChanges_ = 0;

  private:
    bool   enable_blend_;
    GLenum blend_source_factor_;
    GLenum blend_destination_factor_;

    bool   enable_cull_face_;
    GLenum cull_face_;

    bool enable_scissor_test_;

    bool enable_clip_plane_[6];

    GLfloat clear_red_;
    GLfloat clear_green_;
    GLfloat clear_blue_;
    GLfloat clear_alpha_;

    // texture
    bool enable_texture_2d_[2];

    GLint texture_environment_mode_[2];
    GLint texture_environment_combine_rgb_[2];
    GLint texture_environment_source_0_rgb_[2];
    GLint texture_min_filter_[2];
    GLint texture_mag_filter_[2];
    GLint texture_wrap_s_[2];
    GLint texture_wrap_t_[2];

    GLuint bind_texture_2d_[2];
    GLenum active_texture_ = GL_TEXTURE0;

    bool enable_depth_test_;
    bool depth_mask_;
    GLenum  depth_function_;

    GLfloat polygon_offset_factor_;
    GLfloat polygon_offset_units_;

    bool    enable_alpha_test_;
    GLenum  alpha_function_;
    GLfloat alpha_function_reference_;

    bool enable_lighting_;
    
    bool enable_color_material_;

    bool enable_stencil_test_;

    bool enable_line_smooth_;
    float line_width_;

    bool enable_normalize_;

#ifndef EDGE_GL_ES2
    bool enable_polygon_smooth_;
#endif

    bool    enable_fog_;
    GLint   fog_mode_;
    GLfloat fog_start_;
    GLfloat fog_end_;
    GLfloat fog_density_;
    GLfloat fog_color_[4];

    GLfloat gl_color_[4];
};

extern RenderState *global_render_state;

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
