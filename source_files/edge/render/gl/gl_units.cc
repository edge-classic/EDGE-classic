//----------------------------------------------------------------------------
//  EDGE GPU Rendering (Unit system)
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
// -AJA- 2000/10/09: Began work on this new unit system.
//

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "AlmostEquals.h"
#include "dm_state.h"
#include "e_player.h"
#include "edge_profiling.h"
#include "epi.h"
#include "i_defs_gl.h"
#include "im_data.h"
#include "m_argv.h"
#include "r_backend.h"
#include "r_colormap.h"
#include "r_gldefs.h"
#include "r_image.h"
#include "r_misc.h"
#include "r_shader.h"
#include "r_sky.h"
#include "r_texgl.h"
#include "r_units.h"

#ifdef APPLE_SILICON
EDGE_DEFINE_CONSOLE_VARIABLE(renderer_dumb_clamp, "1", kConsoleVariableFlagNone)
#else
EDGE_DEFINE_CONSOLE_VARIABLE(renderer_dumb_clamp, "0", kConsoleVariableFlagNone)
#endif

static constexpr uint16_t kMaximumLocalUnits = 1024;

extern ConsoleVariable draw_culling;
extern ConsoleVariable cull_fog_color;

std::unordered_map<GLuint, GLint> texture_clamp_s;
std::unordered_map<GLuint, GLint> texture_clamp_t;

// a single unit (polygon, quad, etc) to pass to the GL
struct RendererUnit
{
    // unit mode (e.g. GL_TRIANGLE_FAN)
    GLuint shape;

    // environment modes (GL_REPLACE, GL_MODULATE, GL_DECAL, GL_ADD)
    GLuint environment_mode[2];

    // texture(s) used
    GLuint texture[2];

    // pass number (multiple pass rendering)
    int pass;

    // blending flags
    BlendingMode blending;

    // range of local vertices
    int first, count;

    RGBAColor fog_color   = kRGBANoValue;
    float     fog_density = 0;
};

static RendererVertex local_verts[kMaximumLocalVertices];
static RendererUnit   local_units[kMaximumLocalUnits];

static std::vector<RendererUnit *> local_unit_map;

static int current_render_vert;
static int current_render_unit;

static bool batch_sort;

RGBAColor culling_fog_color;

//
// StartUnitBatch
//
// Starts a fresh batch of units.
//
// When 'sort_em' is true, the units will be sorted to keep
// texture changes to a minimum.  Otherwise, the batch is
// drawn in the same order as given.
//
void StartUnitBatch(bool sort_em)
{
    if (render_backend->RenderUnitsLocked())
    {
        FatalError("StartUnitBatch - Render units are locked");
    }

    current_render_vert = current_render_unit = 0;

    batch_sort = sort_em;

    local_unit_map.resize(kMaximumLocalUnits);
}

//
// FinishUnitBatch
//
// Finishes a batch of units, drawing any that haven't been drawn yet.
//
void FinishUnitBatch(void)
{
    if (render_backend->RenderUnitsLocked())
    {
        FatalError("FinishUnitBatch - Render units are locked");
    }

    RenderCurrentUnits();
}

//
// BeginRenderUnit
//
// Begin a new unit, with the given parameters (mode and texture ID).
// `max_vert' is the maximum expected vertices of the quad/poly (the
// actual number can be less, but never more).  Returns a pointer to
// the first vertex structure.  `masked' should be true if the texture
// contains "holes" (like sprites).  `blended' should be true if the
// texture should be blended (like for translucent water or sprites).
//
RendererVertex *BeginRenderUnit(GLuint shape, int max_vert, GLuint env1, GLuint tex1, GLuint env2, GLuint tex2,
                                int pass, BlendingMode blending, RGBAColor fog_color, float fog_density)
{
    if (render_backend->RenderUnitsLocked())
    {
        FatalError("BeginRenderUnit - Render units are locked");
    }

    RendererUnit *unit;

    EPI_ASSERT(max_vert > 0);
    EPI_ASSERT(pass >= 0);

    EPI_ASSERT((blending & (kBlendingCullBack | kBlendingCullFront)) != (kBlendingCullBack | kBlendingCullFront));

    // check we have enough space left
    if (current_render_vert + max_vert > kMaximumLocalVertices || current_render_unit >= kMaximumLocalUnits)
    {
        RenderCurrentUnits();
    }

    unit = local_units + current_render_unit;

    if (env1 == kTextureEnvironmentDisable)
        tex1 = 0;
    if (env2 == kTextureEnvironmentDisable)
        tex2 = 0;

    unit->shape               = shape;
    unit->environment_mode[0] = env1;
    unit->environment_mode[1] = env2;
    unit->texture[0]          = tex1;
    unit->texture[1]          = tex2;

    unit->pass     = pass;
    unit->blending = blending;
    unit->first    = current_render_vert; // count set later

    unit->fog_color   = fog_color;
    unit->fog_density = fog_density;

    return local_verts + current_render_vert;
}

//
// EndRenderUnit
//
void EndRenderUnit(int actual_vert)
{
    if (render_backend->RenderUnitsLocked())
    {
        FatalError("EndRenderUnit - Render units are locked");
    }

    RendererUnit *unit;

    EPI_ASSERT(actual_vert >= 0);

    if (actual_vert == 0)
        return;

    unit = local_units + current_render_unit;

    unit->count = actual_vert;

    current_render_vert += actual_vert;
    current_render_unit++;

    EPI_ASSERT(current_render_vert <= kMaximumLocalVertices);
    EPI_ASSERT(current_render_unit <= kMaximumLocalUnits);
}

struct Compare_Unit_pred
{
    inline bool operator()(const RendererUnit *A, const RendererUnit *B) const
    {
        if (A->pass != B->pass)
            return A->pass < B->pass;

        if (A->texture[0] != B->texture[0])
            return A->texture[0] < B->texture[0];

        if (A->texture[1] != B->texture[1])
            return A->texture[1] < B->texture[1];

        if (A->environment_mode[0] != B->environment_mode[0])
            return A->environment_mode[0] < B->environment_mode[0];

        if (A->environment_mode[1] != B->environment_mode[1])
            return A->environment_mode[1] < B->environment_mode[1];

        return A->blending < B->blending;
    }
};

static void EnableCustomEnvironment(GLuint env, bool enable)
{
    RenderState *state = render_state;
    switch (env)
    {
    case uint32_t(kTextureEnvironmentSkipRGB):
        if (enable)
        {
            state->TextureEnvironmentMode(GL_COMBINE);
            state->TextureEnvironmentCombineRGB(GL_REPLACE);
            state->TextureEnvironmentSource0RGB(GL_PREVIOUS);
        }
        else
        {
            /* no need to modify TEXTURE_ENV_MODE */
            state->TextureEnvironmentCombineRGB(GL_MODULATE);
            state->TextureEnvironmentSource0RGB(GL_TEXTURE);
        }
        break;

    default:
        FatalError("INTERNAL ERROR: no such custom env: %08x\n", env);
    }
}

//
// RenderCurrentUnits
//
// Forces the set of current units to be drawn.  This call is
// optional (it never _needs_ to be called by client code).
//
void RenderCurrentUnits(void)
{
    EDGE_ZoneScoped;

    if (render_backend->RenderUnitsLocked())
    {
        FatalError("RenderCurrentUnits - Render units are locked");
    }

    if (current_render_unit == 0)
        return;

    RenderState *state = render_state;

    GLuint active_tex[2] = {0, 0};
    GLuint active_env[2] = {0, 0};

    int active_pass     = 0;
    int active_blending = 0;

    RGBAColor active_fog_rgb     = kRGBANoValue;
    float     active_fog_density = 0;

    for (int i = 0; i < current_render_unit; i++)
        local_unit_map[i] = &local_units[i];

    if (batch_sort)
    {
        std::sort(local_unit_map.begin(), local_unit_map.begin() + current_render_unit, Compare_Unit_pred());
    }

    if (draw_culling.d_)
    {
        RGBAColor fogColor;
        switch (cull_fog_color.d_)
        {
        case 0:
            fogColor = culling_fog_color;
            break;
        case 1:
            // Not pure white, but 1.0f felt like a little much - Dasho
            fogColor = kRGBASilver;
            break;
        case 2:
            fogColor = 0x404040FF; // Find a constant to call this
            break;
        case 3:
            fogColor = kRGBABlack;
            break;
        default:
            fogColor = culling_fog_color;
            break;
        }

        state->ClearColor(fogColor);
        state->FogMode(GL_LINEAR);
        state->FogColor(fogColor);
        state->FogStart(renderer_far_clip.f_ - 750.0f);
        state->FogEnd(renderer_far_clip.f_ - 250.0f);
        state->Enable(GL_FOG);
    }
    else
        state->FogMode(GL_EXP); // if needed

    for (int j = 0; j < current_render_unit; j++)
    {
        ec_frame_stats.draw_render_units++;

        RendererUnit *unit = local_unit_map[j];

        EPI_ASSERT(unit->count > 0);

        // detect changes in texture/alpha/blending state

        if (!draw_culling.d_ && unit->fog_color != kRGBANoValue && !(unit->blending & kBlendingNoFog))
        {
            if (unit->fog_color != active_fog_rgb)
            {
                active_fog_rgb = unit->fog_color;
                state->ClearColor(active_fog_rgb);
                state->FogColor(active_fog_rgb);
            }
            if (!AlmostEquals(unit->fog_density, active_fog_density))
            {
                active_fog_density = unit->fog_density;
                state->FogDensity(std::log1p(active_fog_density));
            }
            if (!AlmostEquals(active_fog_density, 0.0f))
                state->Enable(GL_FOG);
            else
                state->Disable(GL_FOG);
        }
        else if (!draw_culling.d_ || (unit->blending & kBlendingNoFog))
            state->Disable(GL_FOG);

        if (active_pass != unit->pass)
        {
            active_pass = unit->pass;

            state->PolygonOffset(0, -active_pass);
        }

        if ((active_blending ^ unit->blending) & (kBlendingMasked | kBlendingLess | kBlendingGEqual))
        {
            if (unit->blending & kBlendingLess)
            {
                // Alpha function is updated below, because the alpha
                // value can change from unit to unit while the
                // kBlendingLess flag remains set.
                state->Enable(GL_ALPHA_TEST);
            }
            else if (unit->blending & kBlendingMasked)
            {
                state->Enable(GL_ALPHA_TEST);
                state->AlphaFunction(GL_GREATER, 0);
            }
            else if (unit->blending & kBlendingGEqual)
            {
                state->Enable(GL_ALPHA_TEST);
                state->AlphaFunction(GL_GEQUAL, 1.0f - (epi::GetRGBAAlpha(local_verts[unit->first].rgba) / 255.0f));
            }
            else
                state->Disable(GL_ALPHA_TEST);
        }

        if ((active_blending ^ unit->blending) &
            (kBlendingAlpha | kBlendingAdd | kBlendingInvert | kBlendingNegativeGamma | kBlendingPositiveGamma))
        {
            if (unit->blending & kBlendingAdd)
            {
                state->Enable(GL_BLEND);
                state->BlendFunction(GL_SRC_ALPHA, GL_ONE);
            }
            else if (unit->blending & kBlendingAlpha)
            {
                state->Enable(GL_BLEND);
                state->BlendFunction(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }
            else if (unit->blending & kBlendingInvert)
            {
                state->Enable(GL_BLEND);
                state->BlendFunction(GL_ONE_MINUS_DST_COLOR, GL_ZERO);
            }
            else if (unit->blending & kBlendingNegativeGamma)
            {
                state->Enable(GL_BLEND);
                state->BlendFunction(GL_ZERO, GL_SRC_COLOR);
            }
            else if (unit->blending & kBlendingPositiveGamma)
            {
                state->Enable(GL_BLEND);
                state->BlendFunction(GL_DST_COLOR, GL_ONE);
            }
            else
                state->Disable(GL_BLEND);
        }

        if ((active_blending ^ unit->blending) & (kBlendingCullBack | kBlendingCullFront))
        {
            if (unit->blending & (kBlendingCullBack | kBlendingCullFront))
            {
                state->Enable(GL_CULL_FACE);
                state->CullFace((unit->blending & kBlendingCullFront) ? GL_FRONT : GL_BACK);
            }
            else
                state->Disable(GL_CULL_FACE);
        }

        if ((active_blending ^ unit->blending) & kBlendingNoZBuffer)
        {
            state->DepthMask((unit->blending & kBlendingNoZBuffer) ? false : true);
        }

        active_blending = unit->blending;

        if (active_blending & kBlendingLess)
        {
            // NOTE: assumes alpha is constant over whole polygon
            float a = epi::GetRGBAAlpha(local_verts[unit->first].rgba) / 255.0f;
            state->AlphaFunction(GL_GREATER, a * 0.66f);
        }

        GLint old_clamp_s = kDummyClamp;
        GLint old_clamp_t = kDummyClamp;

        for (int t = 1; t >= 0; t--)
        {
            if (active_tex[t] != unit->texture[t] || active_env[t] != unit->environment_mode[t])
            {
                state->ActiveTexture(GL_TEXTURE0 + t);
            }

            if (draw_culling.d_ && !(unit->blending & kBlendingNoFog))
            {
                if (unit->pass > 0)
                    state->Disable(GL_FOG);
                else
                    state->Enable(GL_FOG);
            }

            if (active_tex[t] != unit->texture[t])
            {
                if (unit->texture[t] == 0)
                    state->Disable(GL_TEXTURE_2D);
                else if (active_tex[t] == 0)
                    state->Enable(GL_TEXTURE_2D);

                if (unit->texture[t] != 0)
                    state->BindTexture(unit->texture[t]);

                active_tex[t] = unit->texture[t];

                if (!t && (active_blending & kBlendingRepeatX) && active_tex[0] != 0)
                {
                    auto existing = texture_clamp_s.find(active_tex[0]);
                    if (existing != texture_clamp_s.end())
                    {
                        if (existing->second != GL_REPEAT)
                        {
                            old_clamp_s = existing->second;
                            state->TextureWrapS(GL_REPEAT);
                        }
                    }
                    else
                        state->TextureWrapS(GL_REPEAT);
                }

                if (!t && (active_blending & (kBlendingClampY | kBlendingRepeatY)) && active_tex[0] != 0)
                {
                    auto existing = texture_clamp_t.find(active_tex[0]);
                    if (existing != texture_clamp_t.end())
                    {
                        if (unit->blending & kBlendingClampY)
                        {
                            if (existing->second != (renderer_dumb_clamp.d_ ? GL_CLAMP : GL_CLAMP_TO_EDGE))
                            {
                                old_clamp_t = existing->second;
                                state->TextureWrapT(renderer_dumb_clamp.d_ ? GL_CLAMP : GL_CLAMP_TO_EDGE);
                            }
                        }
                        else
                        {
                            if (existing->second != GL_REPEAT)
                            {
                                old_clamp_t = existing->second;
                                state->TextureWrapT(GL_REPEAT);
                            }
                        }
                    }
                    else
                    {
                        if (unit->blending & kBlendingClampY)
                            state->TextureWrapT(renderer_dumb_clamp.d_ ? GL_CLAMP : GL_CLAMP_TO_EDGE);
                        else
                            state->TextureWrapT(GL_REPEAT);
                    }
                }
            }

            if (active_env[t] != unit->environment_mode[t])
            {
                if (active_env[t] == kTextureEnvironmentSkipRGB)
                {
                    EnableCustomEnvironment(active_env[t], false);
                }

                if (unit->environment_mode[t] == kTextureEnvironmentSkipRGB)
                {
                    EnableCustomEnvironment(unit->environment_mode[t], true);
                }
                else if (unit->environment_mode[t] != kTextureEnvironmentDisable)
                    state->TextureEnvironmentMode(unit->environment_mode[t]);

                active_env[t] = unit->environment_mode[t];
            }
        }

        glBegin(unit->shape);

        const RendererVertex *V = local_verts + unit->first;

        for (int v_idx = 0, v_last_idx = unit->count; v_idx < v_last_idx; v_idx++, V++)
        {
            state->GLColor(V->rgba);
            state->MultiTexCoord(GL_TEXTURE0, &V->texture_coordinates[0]);
            state->MultiTexCoord(GL_TEXTURE1, &V->texture_coordinates[1]);
            // vertex must be last
            glVertex3fv((const GLfloat *)(&V->position));
        }

        glEnd();

        // restore the clamping mode
        if (old_clamp_s != kDummyClamp)
        {
            state->TextureWrapS(old_clamp_s);
        }
        if (old_clamp_t != kDummyClamp)
        {
            state->TextureWrapT(old_clamp_t);
        }
    }

    // all done
    current_render_vert = current_render_unit = 0;

    for (int t = 1; t >= 0; t--)
    {
        state->ActiveTexture(GL_TEXTURE0 + t);

        if (active_env[t] == kTextureEnvironmentSkipRGB)
        {
            EnableCustomEnvironment(active_env[t], false);
        }
        state->TextureEnvironmentMode(GL_MODULATE);
        state->Disable(GL_TEXTURE_2D);
    }

    state->ResetGLState();
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
