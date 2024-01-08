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

#ifndef __R_STATE_H__
#define __R_STATE_H__

// Need data structure definitions.
#include "i_defs.h"
#include "w_flat.h"
#include "m_math.h"
#include "r_defs.h"
#include "i_defs_gl.h"
#include "AlmostEquals.h"
#include "edge_profiling.h"

//
// Lookup tables for map data.
//
extern int       numvertexes;
extern vertex_t *vertexes;

extern int       num_gl_vertexes;
extern vertex_t *gl_vertexes;

extern int    numsegs;
extern seg_t *segs;

extern int       numsectors;
extern sector_t *sectors;

extern int          numsubsectors;
extern subsector_t *subsectors;

extern int           numextrafloors;
extern extrafloor_t *extrafloors;

extern int     numnodes;
extern node_t *nodes;

extern int     numlines;
extern line_t *lines;

extern int     numsides;
extern side_t *sides;

extern int     numvertgaps;
extern vgap_t *vertgaps;

//
// POV data.
//
extern float viewx;
extern float viewy;
extern float viewz;

extern bam_angle_t viewangle;

// -ES- 1999/03/20 Added these.
// Angles that are used for linedef clipping.
// Nearly the same as leftangle/rightangle, but slightly rounded to fit
// viewangletox lookups, and converted to BAM format.

// angles used for clipping
extern bam_angle_t clip_left, clip_right;

// the scope of the clipped area (clip_left-clip_right).
// ANG180 disables polar clipping
extern bam_angle_t clip_scope;

// the most extreme angles of the view
extern float view_x_slope, view_y_slope;

extern ECFrameStats ecframe_stats;

class gl_state_c
{
  public:
    void enable(GLenum cap, bool enabled = true)
    {
        switch (cap)
        {
        case GL_TEXTURE_2D:
            if (enableTexture2D_[activeTexture_ - GL_TEXTURE0] == enabled)
                return;
            enableTexture2D_[activeTexture_ - GL_TEXTURE0] = enabled;
            break;
        case GL_FOG:
            if (enableFog_ == enabled)
                return;
            enableFog_ = enabled;
            break;
        case GL_ALPHA_TEST:
            if (enableAlphaTest_ == enabled)
                return;
            enableAlphaTest_ = enabled;
            break;
        case GL_BLEND:
            if (enableBlend_ == enabled)
                return;
            enableBlend_ = enabled;
            break;
        case GL_CULL_FACE:
            if (enableCullFace_ == enabled)
                return;
            enableCullFace_ = enabled;
            break;
        case GL_SCISSOR_TEST:
            if (enableScissorTest_ == enabled)
                return;
            enableScissorTest_ = enabled;
            break;
        default:
            I_Error("Unknown GL State %i", cap);
        }

        if (enabled)
        {
            glEnable(cap);
        }
        else
        {
            glDisable(cap);
        }

        ecframe_stats.draw_statechange++;
    }

    void disable(GLenum cap)
    {
        enable(cap, false);
    }

    void depthMask(bool enable)
    {
        if (depthMask_ == enable)
        {
            return;
        }

        depthMask_ = enable;
        glDepthMask(enable ? GL_TRUE : GL_FALSE);
        ecframe_stats.draw_statechange++;
    }

    void cullFace(GLenum mode)
    {
        if (cullFace_ == mode)
        {
            return;
        }

        cullFace_ = mode;
        glCullFace(mode);
        ecframe_stats.draw_statechange++;
    }

    void alphaFunc(GLenum func, GLfloat ref)
    {
        if (func == alphaFunc_ && AlmostEquals(ref, alphaFuncRef_))
        {
            return;
        }

        alphaFunc_    = func;
        alphaFuncRef_ = ref;

        glAlphaFunc(alphaFunc_, alphaFuncRef_);
        ecframe_stats.draw_statechange++;
    }

    void activeTexture(GLenum activeTexture)
    {
        if (activeTexture == activeTexture_)
        {
            return;
        }

        activeTexture_ = activeTexture;
        glActiveTexture(activeTexture_);
        ecframe_stats.draw_statechange++;
    }

    void bindTexture(GLuint textureid)
    {
        GLuint index = activeTexture_ - GL_TEXTURE0;
        if (bindTexture2D_[index] == textureid)
        {
            return;
        }

        bindTexture2D_[index] = textureid;
        glBindTexture(GL_TEXTURE_2D, textureid);
        ecframe_stats.draw_texchange++;
        ecframe_stats.draw_statechange++;
    }

    void polygonOffset(GLfloat factor, GLfloat units)
    {
        if (factor == polygonOffsetFactor_ && units == polygonOffsetUnits_)
        {
            return;
        }

        polygonOffsetFactor_ = factor;
        polygonOffsetUnits_  = units;
        glPolygonOffset(polygonOffsetFactor_, polygonOffsetUnits_);
        ecframe_stats.draw_statechange++;
    }

    void clearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
    {
        if (AlmostEquals(red, clearRed_) && AlmostEquals(green, clearGreen_) && AlmostEquals(blue, clearBlue_) &&
            AlmostEquals(alpha, clearAlpha_))
        {
            return;
        }

        clearRed_   = red;
        clearGreen_ = green;
        clearBlue_  = blue;
        clearAlpha_ = alpha;
        glClearColor(clearRed_, clearGreen_, clearBlue_, clearAlpha_);
        ecframe_stats.draw_statechange++;
    }

    void fogMode(GLint fogMode)
    {
        if (fogMode_ == fogMode)
        {
            return;
        }

        fogMode_ = fogMode;
        glFogi(GL_FOG_MODE, fogMode_);
        ecframe_stats.draw_statechange++;
    }

    void fogColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
    {
        if (AlmostEquals(red, fogColor_[0]) && AlmostEquals(green, fogColor_[1]) && AlmostEquals(blue, fogColor_[2]) &&
            AlmostEquals(alpha, fogColor_[3]))
        {
            return;
        }

        fogColor_[0] = red;
        fogColor_[1] = green;
        fogColor_[2] = blue;
        fogColor_[3] = alpha;

        glFogfv(GL_FOG_COLOR, fogColor_);
        ecframe_stats.draw_statechange++;
    }

    void fogStart(GLfloat start)
    {
        if (fogStart_ == start)
        {
            return;
        }

        fogStart_ = start;
        glFogf(GL_FOG_START, fogStart_);
        ecframe_stats.draw_statechange++;
    }

    void fogEnd(GLfloat end)
    {
        if (fogEnd_ == end)
        {
            return;
        }

        fogEnd_ = end;
        glFogf(GL_FOG_END, fogEnd_);
        ecframe_stats.draw_statechange++;
    }

    void fogDensity(GLfloat density)
    {
        if (fogDensity_ == density)
        {
            return;
        }

        fogDensity_ = density;
        glFogf(GL_FOG_DENSITY, fogDensity_);
        ecframe_stats.draw_statechange++;
    }

    void blendFunc(GLenum sfactor, GLenum dfactor)
    {
        if (blendSFactor_ == sfactor && blendDFactor_ == dfactor)
        {
            return;
        }

        blendSFactor_ = sfactor;
        blendDFactor_ = dfactor;
        glBlendFunc(blendSFactor_, blendDFactor_);
        ecframe_stats.draw_statechange++;
    }

    void texEnvMode(GLint param)
    {
        GLuint index = activeTexture_ - GL_TEXTURE0;

        if (texEnvMode_[index] == param)
        {
            return;
        }

        texEnvMode_[index] = param;
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, texEnvMode_[index]);
        ecframe_stats.draw_statechange++;
    }

    void texEnvCombineRGB(GLint param)
    {
        GLuint index = activeTexture_ - GL_TEXTURE0;

        if (texEnvCombineRGB_[index] == param)
        {
            return;
        }

        texEnvCombineRGB_[index] = param;
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, texEnvCombineRGB_[index]);
        ecframe_stats.draw_statechange++;
    }

    void texEnvSource0RGB(GLint param)
    {
        GLuint index = activeTexture_ - GL_TEXTURE0;

        if (texEnvSource0RGB_[index] == param)
        {
            return;
        }

        texEnvSource0RGB_[index] = param;
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, texEnvSource0RGB_[index]);
        ecframe_stats.draw_statechange++;
    }

    void texWrapT(GLint param)
    {
        GLuint index = activeTexture_ - GL_TEXTURE0;
        
        if (texWrapT_[index] == param)
        {
            return;
        }

        texWrapT_[index] = param;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, texWrapT_[index]);
        ecframe_stats.draw_statechange++;
    }

    void resetDefaultState()
    {
        disable(GL_BLEND);        
        blendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        disable(GL_ALPHA_TEST);

        depthMask(true);

        cullFace(GL_BACK);
        disable(GL_CULL_FACE);
        

        disable(GL_FOG);

        polygonOffset(0, 0);

        for (int i = 0; i < 2; i++)
        {
            bindTexture2D_[i]   = 0;
            texEnvMode_[i]       = 0;
            texEnvCombineRGB_[i] = 0;
            texEnvSource0RGB_[i] = 0;
            texWrapT_[i] = 0;
        }
    }

    void setDefaultStateFull()
    {
        enableBlend_ = false;
        glDisable(GL_BLEND);
        ecframe_stats.draw_statechange++;

        blendSFactor_ = GL_SRC_ALPHA;
        blendDFactor_ = GL_ONE_MINUS_SRC_ALPHA;
        glBlendFunc(blendSFactor_, blendDFactor_);
        ecframe_stats.draw_statechange++;

        for (int i = 0; i < 2; i++)
        {
            enableTexture2D_[i] = false;
            bindTexture2D_[i]   = 0;
            glActiveTexture(GL_TEXTURE0 + i);
            ecframe_stats.draw_statechange++;
            glBindTexture(GL_TEXTURE_2D, 0);
            ecframe_stats.draw_texchange++;
            ecframe_stats.draw_statechange++;
            glDisable(GL_TEXTURE_2D);
            ecframe_stats.draw_statechange++;

            texEnvMode_[i]       = 0;
            texEnvCombineRGB_[i] = 0;
            texEnvSource0RGB_[i] = 0;
            texWrapT_[i] = 0;
        }

        activeTexture_ = GL_TEXTURE0;
        glActiveTexture(activeTexture_);
        ecframe_stats.draw_statechange++;

        enableAlphaTest_ = false;
        glDisable(GL_ALPHA_TEST);
        ecframe_stats.draw_statechange++;

        alphaFunc_    = GL_GREATER;
        alphaFuncRef_ = 0.0f;

        glAlphaFunc(alphaFunc_, alphaFuncRef_);
        ecframe_stats.draw_statechange++;

        depthMask_ = true;
        glDepthMask(GL_TRUE);
        ecframe_stats.draw_statechange++;

        cullFace_ = GL_BACK;
        glCullFace(cullFace_);
        ecframe_stats.draw_statechange++;
        enableCullFace_ = false;
        glDisable(GL_CULL_FACE);
        ecframe_stats.draw_statechange++;

        clearRed_   = 0.0f;
        clearGreen_ = 0.0f;
        clearBlue_  = 0.0f;
        clearAlpha_ = 1.0f;
        glClearColor(clearRed_, clearGreen_, clearBlue_, clearAlpha_);
        ecframe_stats.draw_statechange++;

        fogMode_ = GL_LINEAR;
        glFogi(GL_FOG_MODE, fogMode_);
        ecframe_stats.draw_statechange++;

        fogColor_[0] = 0.0f;
        fogColor_[1] = 0.0f;
        fogColor_[2] = 0.0f;
        fogColor_[3] = 1.0f;
        glFogfv(GL_FOG_COLOR, fogColor_);
        ecframe_stats.draw_statechange++;

        enableFog_ = false;
        glDisable(GL_FOG);
        ecframe_stats.draw_statechange++;
        fogStart_ = 0.0f;
        fogEnd_   = 0.0f;
        glFogf(GL_FOG_START, fogStart_);
        ecframe_stats.draw_statechange++;
        glFogf(GL_FOG_END, fogEnd_);
        ecframe_stats.draw_statechange++;

        fogDensity_ = 0.0f;
        glFogf(GL_FOG_DENSITY, fogDensity_);
        ecframe_stats.draw_statechange++;

        polygonOffsetFactor_ = 0;
        polygonOffsetUnits_  = 0;
        glPolygonOffset(polygonOffsetFactor_, polygonOffsetUnits_);
        ecframe_stats.draw_statechange++;

        enableScissorTest_ = false;
        glDisable(GL_SCISSOR_TEST);
        ecframe_stats.draw_statechange++;
    }

    int frameStateChanges_ = 0;

  private:
    bool   enableBlend_;
    GLenum blendSFactor_;
    GLenum blendDFactor_;

    bool   enableCullFace_;
    GLenum cullFace_;

    bool enableScissorTest_;

    GLfloat clearRed_;
    GLfloat clearGreen_;
    GLfloat clearBlue_;
    GLfloat clearAlpha_;

    // texture
    bool  enableTexture2D_[2];

    GLint texEnvMode_[2];
    GLint texEnvCombineRGB_[2];
    GLint texEnvSource0RGB_[2];
    GLint texWrapT_[2];

    GLuint bindTexture2D_[2];
    GLenum activeTexture_;

    bool depthMask_;

    GLfloat polygonOffsetFactor_;
    GLfloat polygonOffsetUnits_;

    bool    enableAlphaTest_;
    GLenum  alphaFunc_;
    GLfloat alphaFuncRef_;

    bool    enableFog_;
    GLint   fogMode_;
    GLfloat fogStart_;
    GLfloat fogEnd_;
    GLfloat fogDensity_;
    GLfloat fogColor_[4];
};

gl_state_c *RGL_GetState();

#endif /* __R_STATE_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
