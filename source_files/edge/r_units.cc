//----------------------------------------------------------------------------
//  EDGE OpenGL Rendering (Unit batching)
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


#include "i_defs_gl.h"

#include <vector>
#include <unordered_map>
#include <algorithm>

#include "image_data.h"

#include "dm_state.h"
#include "e_player.h"
#include "m_argv.h"
#include "r_gldefs.h"
#include "r_units.h"

#include "r_misc.h"
#include "r_image.h"
#include "r_texgl.h"
#include "r_shader.h"
#include "r_sky.h"

#include "r_colormap.h"

#include "AlmostEquals.h"
#include "edge_profiling.h"

// TODO review if these should be archived
EDGE_DEFINE_CONSOLE_VARIABLE(r_colorlighting, "1", kConsoleVariableFlagNone)
EDGE_DEFINE_CONSOLE_VARIABLE(r_colormaterial, "1", kConsoleVariableFlagNone)

#ifdef APPLE_SILICON
#define DUMB_CLAMP "1"
#else
#define DUMB_CLAMP "0"
#endif

EDGE_DEFINE_CONSOLE_VARIABLE(r_dumbsky, "0", kConsoleVariableFlagNone)
EDGE_DEFINE_CONSOLE_VARIABLE(r_dumbmulti, "0", kConsoleVariableFlagNone)
EDGE_DEFINE_CONSOLE_VARIABLE(r_dumbcombine, "0", kConsoleVariableFlagNone)
EDGE_DEFINE_CONSOLE_VARIABLE(r_dumbclamp, DUMB_CLAMP, kConsoleVariableFlagNone)

#define MAX_L_VERT 65545
#define MAX_L_UNIT 1024

#define DUMMY_CLAMP 789

extern ConsoleVariable r_culling;
extern ConsoleVariable r_cullfog;

std::unordered_map<GLuint, GLint> texture_clamp;

// a single unit (polygon, quad, etc) to pass to the GL
typedef struct local_gl_unit_s
{
    // unit mode (e.g. GL_TRIANGLE_FAN)
    GLuint shape;

    // environment modes (GL_REPLACE, GL_MODULATE, GL_DECAL, GL_ADD)
    GLuint env[2];

    // texture(s) used
    GLuint tex[2];

    // pass number (multiple pass rendering)
    int pass;

    // blending flags
    int blending;

    // range of local vertices
    int first, count;

    RGBAColor fog_color   = kRGBANoValue;
    float    fog_density = 0;
} local_gl_unit_t;

static local_gl_vert_t local_verts[MAX_L_VERT];
static local_gl_unit_t local_units[MAX_L_UNIT];

static std::vector<local_gl_unit_t *> local_unit_map;

static int cur_vert;
static int cur_unit;

static bool batch_sort;

RGBAColor current_fog_rgb = kRGBANoValue;
sg_color current_fog_color;
float    current_fog_density = 0;
sg_color cull_fog_color;

//
// RGL_InitUnits
//
// Initialise the unit system.  Once-only call.
//
void RGL_InitUnits(void)
{
    // Run the soft init code
    RGL_SoftInitUnits();
}

//
// RGL_SoftInitUnits
//
// -ACB- 2004/02/15 Quickly-hacked routine to reinit stuff lost on res change
//
void RGL_SoftInitUnits()
{
}

//
// RGL_StartUnits
//
// Starts a fresh batch of units.
//
// When 'sort_em' is true, the units will be sorted to keep
// texture changes to a minimum.  Otherwise, the batch is
// drawn in the same order as given.
//
void RGL_StartUnits(bool sort_em)
{
    cur_vert = cur_unit = 0;

    batch_sort = sort_em;

    local_unit_map.resize(MAX_L_UNIT);
}

//
// RGL_FinishUnits
//
// Finishes a batch of units, drawing any that haven't been drawn yet.
//
void RGL_FinishUnits(void)
{
    RGL_DrawUnits();
}

//
// RGL_BeginUnit
//
// Begin a new unit, with the given parameters (mode and texture ID).
// `max_vert' is the maximum expected vertices of the quad/poly (the
// actual number can be less, but never more).  Returns a pointer to
// the first vertex structure.  `masked' should be true if the texture
// contains "holes" (like sprites).  `blended' should be true if the
// texture should be blended (like for translucent water or sprites).
//
local_gl_vert_t *RGL_BeginUnit(GLuint shape, int max_vert, GLuint env1, GLuint tex1, GLuint env2, GLuint tex2, int pass,
                               int blending, RGBAColor fog_color, float fog_density)
{
    local_gl_unit_t *unit;

    SYS_ASSERT(max_vert > 0);
    SYS_ASSERT(pass >= 0);

    SYS_ASSERT((blending & BL_CULL_BOTH) != BL_CULL_BOTH);

    // check we have enough space left
    if (cur_vert + max_vert > MAX_L_VERT || cur_unit >= MAX_L_UNIT)
    {
        RGL_DrawUnits();
    }

    unit = local_units + cur_unit;

    if (env1 == ENV_NONE)
        tex1 = 0;
    if (env2 == ENV_NONE)
        tex2 = 0;

    unit->shape  = shape;
    unit->env[0] = env1;
    unit->env[1] = env2;
    unit->tex[0] = tex1;
    unit->tex[1] = tex2;

    unit->pass     = pass;
    unit->blending = blending;
    unit->first    = cur_vert; // count set later

    unit->fog_color   = fog_color;
    unit->fog_density = fog_density;

    return local_verts + cur_vert;
}

//
// RGL_EndUnit
//
void RGL_EndUnit(int actual_vert)
{
    local_gl_unit_t *unit;

    SYS_ASSERT(actual_vert > 0);

    unit = local_units + cur_unit;

    unit->count = actual_vert;

    // adjust colors (for special effects)
    for (int i = 0; i < actual_vert; i++)
    {
        local_gl_vert_t *v = &local_verts[cur_vert + i];

        v->rgba[0] *= ren_red_mul;
        v->rgba[1] *= ren_grn_mul;
        v->rgba[2] *= ren_blu_mul;
    }

    cur_vert += actual_vert;
    cur_unit++;

    SYS_ASSERT(cur_vert <= MAX_L_VERT);
    SYS_ASSERT(cur_unit <= MAX_L_UNIT);
}

struct Compare_Unit_pred
{
    inline bool operator()(const local_gl_unit_t *A, const local_gl_unit_t *B) const
    {
        if (A->pass != B->pass)
            return A->pass < B->pass;

        if (A->tex[0] != B->tex[0])
            return A->tex[0] < B->tex[0];

        if (A->tex[1] != B->tex[1])
            return A->tex[1] < B->tex[1];

        if (A->env[0] != B->env[0])
            return A->env[0] < B->env[0];

        if (A->env[1] != B->env[1])
            return A->env[1] < B->env[1];

        return A->blending < B->blending;
    }
};

static void EnableCustomEnv(GLuint env, bool enable)
{
    gl_state_c *state = RGL_GetState();
    switch (env)
    {
    case uint32_t(ENV_SKIP_RGB):
        if (enable)
        {
            state->texEnvMode(GL_COMBINE);
            state->texEnvCombineRGB(GL_REPLACE);
            state->texEnvSource0RGB(GL_PREVIOUS);
        }
        else
        {
            /* no need to modify TEXTURE_ENV_MODE */
            state->texEnvCombineRGB(GL_MODULATE);
            state->texEnvSource0RGB(GL_TEXTURE);
        }
        break;

    default:
        EDGEError("INTERNAL ERROR: no such custom env: %08x\n", env);
    }
}

static inline void RGL_SendRawVector(const local_gl_vert_t *V)
{
    if (r_colormaterial.d_|| !r_colorlighting.d_)
        glColor4fv(V->rgba);
    else
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, V->rgba);

    glMultiTexCoord2fv(GL_TEXTURE0, (const GLfloat *)(&V->texc[0]));
    glMultiTexCoord2fv(GL_TEXTURE1, (const GLfloat *)(&V->texc[1]));

    glNormal3fv((const GLfloat *)(&V->normal));

    // vertex must be last
    glVertex3fv((const GLfloat *)(&V->pos));
}

//
// RGL_DrawUnits
//
// Forces the set of current units to be drawn.  This call is
// optional (it never _needs_ to be called by client code).
//
void RGL_DrawUnits(void)
{
    EDGE_ZoneScoped;

    if (cur_unit == 0)
        return;

    gl_state_c *state = RGL_GetState();

    GLuint active_tex[2] = {0, 0};
    GLuint active_env[2] = {0, 0};

    int active_pass     = 0;
    int active_blending = 0;

    RGBAColor active_fog_rgb     = kRGBANoValue;
    float    active_fog_density = 0;

    for (int i = 0; i < cur_unit; i++)
        local_unit_map[i] = &local_units[i];

    if (batch_sort)
    {
        std::sort(local_unit_map.begin(), local_unit_map.begin() + cur_unit, Compare_Unit_pred());
    }

    if (r_culling.d_)
    {
        sg_color fogColor;
        switch (r_cullfog.d_)
        {
        case 0:
            fogColor = cull_fog_color;
            break;
        case 1:
            // Not pure white, but 1.0f felt like a little much - Dasho
            fogColor = sg_silver;
            break;
        case 2:
            fogColor = { 0.25f, 0.25f, 0.25f, 1.0f };
            break;
        case 3:
            fogColor = sg_black;
            break;
        default:
            fogColor = cull_fog_color;
            break;
        }

        state->clearColor(fogColor.r, fogColor.g, fogColor.b, 1.0f);
        state->fogMode(GL_LINEAR);
        state->fogColor(fogColor.r, fogColor.g, fogColor.b, 1.0f);
        state->fogStart(r_farclip.f_ - 750.0f);
        state->fogEnd(r_farclip.f_ - 250.0f);
        state->enable(GL_FOG);
    }
    else
        state->fogMode(GL_EXP); // if needed

    for (int j = 0; j < cur_unit; j++)
    {
        ecframe_stats.draw_render_units++;

        local_gl_unit_t *unit = local_unit_map[j];

        SYS_ASSERT(unit->count > 0);

        // detect changes in texture/alpha/blending state

        if (!r_culling.d_&& unit->fog_color != kRGBANoValue)
        {
            if (unit->fog_color != active_fog_rgb)
            {
                active_fog_rgb = unit->fog_color;
                sg_color fc = sg_make_color_1i(active_fog_rgb);
                state->clearColor(fc.r, fc.g, fc.b, 1.0f);
                state->fogColor(fc.r, fc.g, fc.b, 1.0f);
            }
            if (!AlmostEquals(unit->fog_density, active_fog_density))
            {
                active_fog_density = unit->fog_density;
                state->fogDensity(std::log1p(active_fog_density));
            }
            if (active_fog_density > 0.00009f)
                state->enable(GL_FOG);
            else
                state->disable(GL_FOG);
        }
        else if (!r_culling.d_)
            state->disable(GL_FOG);

        if (active_pass != unit->pass)
        {
            active_pass = unit->pass;

            state->polygonOffset(0, -active_pass);
        }

        if ((active_blending ^ unit->blending) & (BL_Masked | BL_Less))
        {
            if (unit->blending & BL_Less)
            {
                // state->alphaFunc is updated below, because the alpha
                // value can change from unit to unit while the
                // BL_Less flag remains set.
                state->enable(GL_ALPHA_TEST);
            }
            else if (unit->blending & BL_Masked)
            {
                state->enable(GL_ALPHA_TEST);
                state->alphaFunc(GL_GREATER, 0);
            }
            else
                state->disable(GL_ALPHA_TEST);
        }

        if ((active_blending ^ unit->blending) & (BL_Alpha | BL_Add))
        {
            if (unit->blending & BL_Add)
            {
                state->enable(GL_BLEND);
                state->blendFunc(GL_SRC_ALPHA, GL_ONE);
            }
            else if (unit->blending & BL_Alpha)
            {
                state->enable(GL_BLEND);
                state->blendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }
            else
                state->disable(GL_BLEND);
        }

        if ((active_blending ^ unit->blending) & BL_CULL_BOTH)
        {
            if (unit->blending & BL_CULL_BOTH)
            {
                state->enable(GL_CULL_FACE);
                state->cullFace((unit->blending & BL_CullFront) ? GL_FRONT : GL_BACK);
            }
            else
                state->disable(GL_CULL_FACE);
        }

        if ((active_blending ^ unit->blending) & BL_NoZBuf)
        {
            state->depthMask((unit->blending & BL_NoZBuf) ? false : true);
        }

        active_blending = unit->blending;

        if (active_blending & BL_Less)
        {
            // NOTE: assumes alpha is constant over whole polygon
            float a = local_verts[unit->first].rgba[3];
            state->alphaFunc(GL_GREATER, a * 0.66f);
        }

        GLint old_clamp = DUMMY_CLAMP;

        for (int t = 1; t >= 0; t--)
        {
            if (active_tex[t] != unit->tex[t] || active_env[t] != unit->env[t])
            {
                state->activeTexture(GL_TEXTURE0 + t);
            }

            if (r_culling.d_)
            {
                if (unit->pass > 0)
                    state->disable(GL_FOG);
                else
                    state->enable(GL_FOG);
            }

            if (active_tex[t] != unit->tex[t])
            {
                if (unit->tex[t] == 0)
                    state->disable(GL_TEXTURE_2D);
                else if (active_tex[t] == 0)
                    state->enable(GL_TEXTURE_2D);

                if (unit->tex[t] != 0)
                    state->bindTexture(unit->tex[t]);

                active_tex[t] = unit->tex[t];                

                if (!t && (active_blending & BL_ClampY) && active_tex[0] != 0)
                {
                    auto existing = texture_clamp.find(active_tex[0]);
                    if (existing != texture_clamp.end())
                    {
                        old_clamp = existing->second;
                    }

                    // This is very expensive, thus the map
                    // glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &old_clamp);
                    state->texWrapT(r_dumbclamp.d_? GL_CLAMP : GL_CLAMP_TO_EDGE);
                }
            }

            if (active_env[t] != unit->env[t])
            {
                if (active_env[t] >= CUSTOM_ENV_BEGIN && active_env[t] <= CUSTOM_ENV_END)
                {
                    EnableCustomEnv(active_env[t], false);
                }

                if (unit->env[t] >= CUSTOM_ENV_BEGIN && unit->env[t] <= CUSTOM_ENV_END)
                {
                    EnableCustomEnv(unit->env[t], true);
                }
                else if (unit->env[t] != ENV_NONE)
                    state->texEnvMode(unit->env[t]);

                active_env[t] = unit->env[t];
            }
        }

        glBegin(unit->shape);

        for (int v_idx = 0; v_idx < unit->count; v_idx++)
        {
            RGL_SendRawVector(local_verts + unit->first + v_idx);
        }

        glEnd();

        // restore the clamping mode
        if (old_clamp != DUMMY_CLAMP)
        {
            state->texWrapT(old_clamp);
        }
    }

    // all done
    cur_vert = cur_unit = 0;

    state->polygonOffset(0, 0);

    for (int t = 1; t >= 0; t--)
    {
        state->activeTexture(GL_TEXTURE0 + t);

        if (active_env[t] >= CUSTOM_ENV_BEGIN && active_env[t] <= CUSTOM_ENV_END)
        {
            EnableCustomEnv(active_env[t], false);
        }
        state->texEnvMode(GL_MODULATE);
        state->disable(GL_TEXTURE_2D);
    }

    state->resetDefaultState();
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
