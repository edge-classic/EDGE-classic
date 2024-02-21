//----------------------------------------------------------------------------
//  EDGE OpenGL Rendering (Things)
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


#include "i_defs_gl.h"

#include <math.h>

#include "math_color.h"
#include "image_data.h"
#include "image_funcs.h"
#include "str_util.h"

#include "dm_data.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "g_game.h" //currmap
#include "p_local.h"
#include "r_colormap.h"
#include "r_defs.h"
#include "r_draw.h"
#include "r_effects.h"
#include "r_gldefs.h"
#include "r_image.h"
#include "r_mdl.h"
#include "r_md2.h"
#include "r_misc.h"
#include "r_modes.h"
#include "r_shader.h"
#include "r_texgl.h"
#include "r_units.h"
#include "w_model.h"
#include "w_sprite.h"

#include "m_misc.h" // !!!! model test

#include "AlmostEquals.h"

#include "coal.h"
#include "vm_coal.h"
#include "script/compat/lua_compat.h"

#include "edge_profiling.h"

extern coal::vm_c *ui_vm;
extern double      VM_GetFloat(coal::vm_c *vm, const char *mod_name, const char *var_name);

extern bool erraticism_active;

#define DEBUG 0

EDGE_DEFINE_CONSOLE_VARIABLE(r_crosshair, "0", kConsoleVariableFlagArchive)     // shape
EDGE_DEFINE_CONSOLE_VARIABLE(r_crosscolor, "0", kConsoleVariableFlagArchive)    // 0 .. 7
EDGE_DEFINE_CONSOLE_VARIABLE(r_crosssize, "16.0", kConsoleVariableFlagArchive)  // pixels on a 320x200 screen
EDGE_DEFINE_CONSOLE_VARIABLE(r_crossbright, "1.0", kConsoleVariableFlagArchive) // 1.0 is normal

float sprite_skew;

// colour of the player's weapon
int rgl_weapon_r;
int rgl_weapon_g;
int rgl_weapon_b;

extern mobj_t *view_cam_mo;
extern float   view_expand_w;

// The minimum distance between player and a visible sprite.
#define MINZ (4.0f)

static const image_c *crosshair_image;
static int            crosshair_which;

static float GetHoverDZ(mobj_t *mo, float bob_mult = 0)
{
    if (time_stop_active || erraticism_active)
        return mo->phase;

    // compute a different phase for different objects
    BAMAngle phase = (BAMAngle)(long long)mo;
    phase ^= (BAMAngle)(phase << 19);
    phase += (BAMAngle)(leveltime << (kBAMAngleBits - 6));

    mo->phase = epi::BAMSin(phase);

    if (mo->hyperflags & kHyperFlagHover)
        mo->phase *= 4.0f;
    else if (bob_mult > 0)
        mo->phase *= (mo->height * 0.5 * bob_mult);

    return mo->phase;
}

typedef struct
{
    HMM_Vec3 vert[4];
    HMM_Vec2 texc[4];
    HMM_Vec3 lit_pos;

    multi_color_c col[4];
} psprite_coord_data_t;

static void DLIT_PSprite(mobj_t *mo, void *dataptr)
{
    psprite_coord_data_t *data = (psprite_coord_data_t *)dataptr;

    SYS_ASSERT(mo->dlight.shader);

    mo->dlight.shader->Sample(data->col + 0, data->lit_pos.X, data->lit_pos.Y, data->lit_pos.Z);
}

static int GetMulticolMaxRGB(multi_color_c *cols, int num, bool additive)
{
    int result = 0;

    for (; num > 0; num--, cols++)
    {
        int mx = additive ? cols->add_MAX() : cols->mod_MAX();

        result = HMM_MAX(result, mx);
    }

    return result;
}

static void RGL_DrawPSprite(pspdef_t *psp, int which, player_t *player, region_properties_t *props,
                            const State *state)
{
    if (state->flags & kStateFrameFlagModel)
        return;

    // determine sprite patch
    bool           flip;
    const image_c *image = R2_GetOtherSprite(state->sprite, state->frame, &flip);

    if (!image)
        return;

    GLuint tex_id = W_ImageCache(image, false, (which == ps_crosshair) ? nullptr : ren_fx_colmap);

    float w     = IM_WIDTH(image);
    float h     = IM_HEIGHT(image);
    float right = IM_RIGHT(image);
    float top   = IM_TOP(image);
    float ratio = 1.0f;

    bool is_fuzzy = (player->mo->flags & kMapObjectFlagFuzzy) ? true : false;

    float trans = player->mo->visibility;

    if (which == ps_crosshair)
    {
        if (!player->weapons[player->ready_wp].info->ignore_crosshair_scaling_)
            ratio = r_crosssize.f_ / w;

        w *= ratio;
        h *= ratio;
        is_fuzzy = false;
        trans    = 1.0f;
    }

    // Lobo: no sense having the zoom crosshair fuzzy
    if (which == ps_weapon && viewiszoomed && player->weapons[player->ready_wp].info->zoom_state_ > 0)
    {
        is_fuzzy = false;
        trans    = 1.0f;
    }

    trans *= psp->visibility;

    if (trans <= 0)
        return;

    float tex_top_h = top;  /// ## 1.00f; // 0.98;
    float tex_bot_h = 0.0f; /// ## 1.00f - top;  // 1.02 - bottom;

    float tex_x1 = 0.002f;
    float tex_x2 = right - 0.002f;

    if (flip)
    {
        tex_x1 = right - tex_x1;
        tex_x2 = right - tex_x2;
    }

    float coord_W = 320.0f * view_expand_w;
    float coord_H = 200.0f;

    float tx1 = (coord_W - w) / 2.0 + psp->sx - IM_OFFSETX(image);
    float tx2 = tx1 + w;

    float ty1 = -psp->sy + IM_OFFSETY(image) - ((h - IM_HEIGHT(image)) * 0.5f);

    if (LUA_UseLuaHud())
    {
        // Lobo 2022: Apply sprite Y offset, mainly for Heretic weapons.
        if ((state->flags & kStateFrameFlagWeapon) && (player->ready_wp >= 0))
            ty1 += LUA_GetFloat(LUA_GetGlobalVM(), "hud", "universal_y_adjust") +
                   player->weapons[player->ready_wp].info->y_adjust_;
    }
    else
    {
        // Lobo 2022: Apply sprite Y offset, mainly for Heretic weapons.
        if ((state->flags & kStateFrameFlagWeapon) && (player->ready_wp >= 0))
            ty1 += VM_GetFloat(ui_vm, "hud", "universal_y_adjust") + player->weapons[player->ready_wp].info->y_adjust_;
    }

    float ty2 = ty1 + h;

    float x1b, y1b, x1t, y1t, x2b, y2b, x2t, y2t; // screen coords

    x1b = x1t = viewwindow_w * tx1 / coord_W;
    x2b = x2t = viewwindow_w * tx2 / coord_W;

    y1b = y2b = viewwindow_h * ty1 / coord_H;
    y1t = y2t = viewwindow_h * ty2 / coord_H;

    // clip psprite to view window
    glEnable(GL_SCISSOR_TEST);

    glScissor(viewwindow_x, viewwindow_y, viewwindow_w, viewwindow_h);

    x1b = (float)viewwindow_x + x1b;
    x1t = (float)viewwindow_x + x1t;
    x2t = (float)viewwindow_x + x2t;
    x2b = (float)viewwindow_x + x2b;

    y1b = (float)viewwindow_y + y1b - 1;
    y1t = (float)viewwindow_y + y1t - 1;
    y2t = (float)viewwindow_y + y2t - 1;
    y2b = (float)viewwindow_y + y2b - 1;

    psprite_coord_data_t data;

    data.vert[0] = {{x1b, y1b, 0}};
    data.vert[1] = {{x1t, y1t, 0}};
    data.vert[2] = {{x2t, y1t, 0}};
    data.vert[3] = {{x2b, y2b, 0}};

    data.texc[0] = {{tex_x1, tex_bot_h}};
    data.texc[1] = {{tex_x1, tex_top_h}};
    data.texc[2] = {{tex_x2, tex_top_h}};
    data.texc[3] = {{tex_x2, tex_bot_h}};

    float away = 120.0;

    data.lit_pos.X = player->mo->x + viewcos * away;
    data.lit_pos.Y = player->mo->y + viewsin * away;
    data.lit_pos.Z = player->mo->z + player->mo->height * player->mo->info->shotheight_;

    data.col[0].Clear();

    int blending = BL_Masked;

    if (trans >= 0.11f && image->opacity != OPAC_Complex)
        blending = BL_Less;

    if (trans < 0.99 || image->opacity == OPAC_Complex)
        blending |= BL_Alpha;

    if (is_fuzzy)
    {
        blending = BL_Masked | BL_Alpha;
        trans    = 1.0f;
    }

    RGBAColor fc_to_use = player->mo->subsector->sector->props.fog_color;
    float    fd_to_use = player->mo->subsector->sector->props.fog_density;
    // check for DDFLEVL fog
    if (fc_to_use == kRGBANoValue)
    {
        if (IS_SKY(player->mo->subsector->sector->ceil))
        {
            fc_to_use = currmap->outdoor_fog_color_;
            fd_to_use = 0.01f * currmap->outdoor_fog_density_;
        }
        else
        {
            fc_to_use = currmap->indoor_fog_color_;
            fd_to_use = 0.01f * currmap->indoor_fog_density_;
        }
    }

    if (!is_fuzzy)
    {
        abstract_shader_c *shader = R_GetColormapShader(props, state->bright, player->mo->subsector->sector);

        shader->Sample(data.col + 0, data.lit_pos.X, data.lit_pos.Y, data.lit_pos.Z);

        if (fc_to_use != kRGBANoValue)
        {
            int          mix_factor = RoundToInteger(255.0f * (fd_to_use * 75));
            RGBAColor mixme = epi::MixRGBA(epi::MakeRGBA(data.col[0].mod_R, data.col[0].mod_G, data.col[0].mod_B), fc_to_use, mix_factor);
            data.col[0].mod_R = epi::GetRGBARed(mixme);
            data.col[0].mod_G = epi::GetRGBAGreen(mixme);
            data.col[0].mod_B = epi::GetRGBABlue(mixme);
            mixme = epi::MixRGBA(epi::MakeRGBA(data.col[0].add_R, data.col[0].add_G, data.col[0].add_B), fc_to_use, mix_factor);
            data.col[0].add_R = epi::GetRGBARed(mixme);
            data.col[0].add_G = epi::GetRGBAGreen(mixme);
            data.col[0].add_B = epi::GetRGBABlue(mixme);
        }

        if (use_dlights && ren_extralight < 250)
        {
            data.lit_pos.X = player->mo->x + viewcos * 24;
            data.lit_pos.Y = player->mo->y + viewsin * 24;

            float r = 96;

            P_DynamicLightIterator(data.lit_pos.X - r, data.lit_pos.Y - r, player->mo->z, data.lit_pos.X + r,
                                   data.lit_pos.Y + r, player->mo->z + player->mo->height, DLIT_PSprite, &data);

            P_SectorGlowIterator(player->mo->subsector->sector, data.lit_pos.X - r, data.lit_pos.Y - r, player->mo->z,
                                 data.lit_pos.X + r, data.lit_pos.Y + r, player->mo->z + player->mo->height,
                                 DLIT_PSprite, &data);
        }
    }

    // FIXME: sample at least TWO points (left and right edges)
    data.col[1] = data.col[0];
    data.col[2] = data.col[0];
    data.col[3] = data.col[0];

    /* draw the weapon */

    RGL_StartUnits(false);

    int num_pass = is_fuzzy ? 1 : (detail_level > 0 ? 4 : 3);

    for (int pass = 0; pass < num_pass; pass++)
    {
        if (pass == 1)
        {
            blending &= ~BL_Alpha;
            blending |= BL_Add;
        }

        bool is_additive = (pass > 0 && pass == num_pass - 1);

        if (pass > 0 && pass < num_pass - 1)
        {
            if (GetMulticolMaxRGB(data.col, 4, false) <= 0)
                continue;
        }
        else if (is_additive)
        {
            if (GetMulticolMaxRGB(data.col, 4, true) <= 0)
                continue;
        }

        GLuint fuzz_tex = is_fuzzy ? W_ImageCache(fuzz_image, false) : 0;

        local_gl_vert_t *glvert = RGL_BeginUnit(GL_POLYGON, 4, is_additive ? (GLuint)ENV_SKIP_RGB : GL_MODULATE, tex_id,
                                                is_fuzzy ? GL_MODULATE : (GLuint)ENV_NONE, fuzz_tex, pass, blending,
                                                pass > 0 ? kRGBANoValue : fc_to_use, fd_to_use);

        for (int v_idx = 0; v_idx < 4; v_idx++)
        {
            local_gl_vert_t *dest = glvert + v_idx;

            dest->pos     = data.vert[v_idx];
            dest->texc[0] = data.texc[v_idx];

            dest->normal = {{0, 0, 1}};

            if (is_fuzzy)
            {
                dest->texc[1].X = dest->pos.X / (float)SCREENWIDTH;
                dest->texc[1].Y = dest->pos.Y / (float)SCREENHEIGHT;

                FUZZ_Adjust(&dest->texc[1], player->mo);

                dest->rgba[0] = dest->rgba[1] = dest->rgba[2] = 0;
            }
            else if (!is_additive)
            {
                dest->rgba[0] = data.col[v_idx].mod_R / 255.0;
                dest->rgba[1] = data.col[v_idx].mod_G / 255.0;
                dest->rgba[2] = data.col[v_idx].mod_B / 255.0;

                data.col[v_idx].mod_R -= 256;
                data.col[v_idx].mod_G -= 256;
                data.col[v_idx].mod_B -= 256;
            }
            else
            {
                dest->rgba[0] = data.col[v_idx].add_R / 255.0;
                dest->rgba[1] = data.col[v_idx].add_G / 255.0;
                dest->rgba[2] = data.col[v_idx].add_B / 255.0;
            }

            dest->rgba[3] = trans;
        }

        RGL_EndUnit(4);
    }

    RGL_FinishUnits();

    glDisable(GL_SCISSOR_TEST);
}

static const RGBAColor crosshair_colors[8] = {
    SG_LIGHT_GRAY_RGBA32, SG_BLUE_RGBA32, SG_GREEN_RGBA32, SG_CYAN_RGBA32, SG_RED_RGBA32, SG_FUCHSIA_RGBA32, SG_YELLOW_RGBA32, SG_DARK_ORANGE_RGBA32,
};

static void DrawStdCrossHair(void)
{
    if (r_crosshair.d_<= 0 || r_crosshair.d_> 9)
        return;

    if (r_crosssize.f_ < 0.1 || r_crossbright.f_ < 0.1)
        return;

    if (!crosshair_image || crosshair_which != r_crosshair.d_)
    {
        crosshair_which = r_crosshair.d_;

        crosshair_image = W_ImageLookup(epi::StringFormat("STANDARD_CROSSHAIR_%d", crosshair_which).c_str());
    }

    GLuint tex_id = W_ImageCache(crosshair_image);

    static int xh_count = 0;
    static int xh_dir   = 1;

    // -jc- Pulsating
    if (xh_count == 31)
        xh_dir = -1;
    else if (xh_count == 0)
        xh_dir = 1;

    xh_count += xh_dir;

    RGBAColor color     = crosshair_colors[r_crosscolor.d_& 7];
    float    intensity = 1.0f - xh_count / 100.0f;

    intensity *= r_crossbright.f_;

    float r = epi::GetRGBARed(color) * intensity / 255.0f;
    float g = epi::GetRGBAGreen(color) * intensity / 255.0f;
    float b = epi::GetRGBABlue(color) * intensity / 255.0f;

    float x = viewwindow_x + viewwindow_w / 2;
    float y = viewwindow_y + viewwindow_h / 2;

    float w = RoundToInteger(SCREENWIDTH * r_crosssize.f_ / 640.0f);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);

    glBindTexture(GL_TEXTURE_2D, tex_id);

    // additive blending
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    glColor3f(r, g, b);

    glBegin(GL_POLYGON);

    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(x - w, y - w);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(x - w, y + w);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(x + w, y + w);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(x + w, y - w);

    glEnd();

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
}

void RGL_DrawWeaponSprites(player_t *p)
{
    // special handling for zoom: show viewfinder
    if (viewiszoomed)
    {
        pspdef_t *psp = &p->psprites[ps_weapon];

        if ((p->ready_wp < 0) || (psp->state == 0))
            return;

        WeaponDefinition *w = p->weapons[p->ready_wp].info;

        // 2023.06.13 - If zoom state missing but weapon can zoom, allow the regular
        // psprite drawing routines to occur (old EDGE behavior)
        if (w->zoom_state_ > 0)
        {
            RGL_DrawPSprite(psp, ps_weapon, p, view_props, states + w->zoom_state_);
            return;
        }
    }

    // add all active psprites
    // Note: order is significant

    // Lobo 2022:
    // Allow changing the order of weapon sprite
    // rendering so that FLASH states are
    // drawn in front of the WEAPON states
    bool FlashFirst = false;
    if (p->ready_wp >= 0)
    {
        FlashFirst = p->weapons[p->ready_wp].info->render_invert_;
    }

    if (FlashFirst == false)
    {
        for (int i = 0; i < NUMPSPRITES; i++) // normal
        {
            pspdef_t *psp = &p->psprites[i];

            if ((p->ready_wp < 0) || (psp->state == 0))
                continue;

            RGL_DrawPSprite(psp, i, p, view_props, psp->state);
        }
    }
    else
    {
        for (int i = NUMPSPRITES - 1; i >= 0; i--) // go backwards
        {
            pspdef_t *psp = &p->psprites[i];

            if ((p->ready_wp < 0) || (psp->state == 0))
                continue;

            RGL_DrawPSprite(psp, i, p, view_props, psp->state);
        }
    }
}

void RGL_DrawCrosshair(player_t *p)
{
    if (viewiszoomed && p->weapons[p->ready_wp].info->zoom_state_ > 0)
    {
        // Only skip crosshair if there is a dedicated zoom state, which
        // should be providing its own
        return;
    }
    else
    {
        pspdef_t *psp = &p->psprites[ps_crosshair];

        if (p->ready_wp >= 0 && psp->state != 0)
            return;
    }

    if (p->health > 0)
        DrawStdCrossHair();
}

void RGL_DrawWeaponModel(player_t *p)
{
    if (viewiszoomed && p->weapons[p->ready_wp].info->zoom_state_ > 0)
        return;

    pspdef_t *psp = &p->psprites[ps_weapon];

    if (p->ready_wp < 0)
        return;

    if (psp->state == 0)
        return;

    if (!(psp->state->flags & kStateFrameFlagModel))
        return;

    WeaponDefinition *w = p->weapons[p->ready_wp].info;

    modeldef_c *md = W_GetModel(psp->state->sprite);

    int skin_num = p->weapons[p->ready_wp].model_skin;

    const image_c *skin_img = md->skins[skin_num];

    if (!skin_img && md->md2_model)
    {
        // I_Debugf("Render model: no skin %d\n", skin_num);
        skin_img = W_ImageForDummySkin();
    }

    // I_Debugf("Rendering weapon model!\n");

    float x = viewx + viewright.X * psp->sx / 8.0;
    float y = viewy + viewright.Y * psp->sx / 8.0;
    float z = viewz + viewright.Z * psp->sx / 8.0;

    x -= viewup.X * psp->sy / 10.0;
    y -= viewup.Y * psp->sy / 10.0;
    z -= viewup.Z * psp->sy / 10.0;

    x += viewforward.X * w->model_forward_;
    y += viewforward.Y * w->model_forward_;
    z += viewforward.Z * w->model_forward_;

    x += viewright.X * w->model_side_;
    y += viewright.Y * w->model_side_;
    z += viewright.Z * w->model_side_;

    int   last_frame = psp->state->frame;
    float lerp       = 0.0;

    if (p->weapon_last_frame >= 0)
    {
        SYS_ASSERT(psp->state);
        SYS_ASSERT(psp->state->tics > 1);

        last_frame = p->weapon_last_frame;

        lerp = (psp->state->tics - psp->tics + 1) / (float)(psp->state->tics);
        lerp = HMM_Clamp(0, lerp, 1);
    }

    float bias = 0.0f;

    if (LUA_UseLuaHud())
    {
        bias = LUA_GetFloat(LUA_GetGlobalVM(), "hud", "universal_y_adjust") + p->weapons[p->ready_wp].info->y_adjust_;
    }
    else
    {
        bias = VM_GetFloat(ui_vm, "hud", "universal_y_adjust") + p->weapons[p->ready_wp].info->y_adjust_;        
    }

    bias /= 5;
    bias += w->model_bias_;

    if (md->md2_model)
        MD2_RenderModel(md->md2_model, skin_img, true, last_frame, psp->state->frame, lerp, x, y, z, p->mo, view_props,
                        1.0f /* scale */, w->model_aspect_, bias, w->model_rotate_);
    else if (md->mdl_model)
        MDL_RenderModel(md->mdl_model, skin_img, true, last_frame, psp->state->frame, lerp, x, y, z, p->mo, view_props,
                        1.0f /* scale */, w->model_aspect_, bias, w->model_rotate_);
}

// ============================================================================
// R2_BSP START
// ============================================================================

int sprite_kludge = 0;

static inline void LinkDrawthingIntoDrawfloor(drawfloor_t *dfloor, drawthing_t *dthing)
{
    dthing->props = dfloor->props;
    dthing->next  = dfloor->things;
    dthing->prev  = nullptr;

    if (dfloor->things)
        dfloor->things->prev = dthing;

    dfloor->things = dthing;
}

static const image_c *R2_GetThingSprite2(mobj_t *mo, float mx, float my, bool *flip)
{
    // Note: can return nullptr for no image.

    // decide which patch to use for sprite relative to player
    SYS_ASSERT(mo->state);

    if (mo->state->sprite == 0)
        return nullptr;

    spriteframe_c *frame = W_GetSpriteFrame(mo->state->sprite, mo->state->frame);

    if (!frame)
    {
        // show dummy sprite for missing frame
        (*flip) = false;
        return W_ImageForDummySprite();
    }

    int rot = 0;

    if (frame->rots >= 8)
    {
        BAMAngle ang = mo->angle;

        MIR_Angle(ang);

        BAMAngle from_view = R_PointToAngle(viewx, viewy, mx, my);

        ang = from_view - ang + kBAMAngle180;

        if (MIR_Reflective())
            ang = (BAMAngle)0 - ang;

        if (frame->rots == 16)
            rot = (ang + (kBAMAngle45 / 4)) >> (kBAMAngleBits - 4);
        else
            rot = (ang + (kBAMAngle45 / 2)) >> (kBAMAngleBits - 3);
    }

    SYS_ASSERT(0 <= rot && rot < 16);

    (*flip) = frame->flip[rot] ? true : false;

    if (MIR_Reflective())
        (*flip) = !(*flip);

    if (!frame->images[rot])
    {
        // show dummy sprite for missing rotation
        (*flip) = false;
        return W_ImageForDummySprite();
    }

    return frame->images[rot];
}

const image_c *R2_GetOtherSprite(int spritenum, int framenum, bool *flip)
{
    /* Used for non-object stuff, like weapons and finale */

    if (spritenum == 0)
        return nullptr;

    spriteframe_c *frame = W_GetSpriteFrame(spritenum, framenum);

    if (!frame || !frame->images[0])
    {
        (*flip) = false;
        return W_ImageForDummySprite();
    }

    *flip = frame->flip[0] ? true : false;

    return frame->images[0];
}

#define SY_FUDGE 2

static void R2_ClipSpriteVertically(drawsub_c *dsub, drawthing_t *dthing)
{
    drawfloor_t *dfloor = nullptr;

    // find the thing's nominal region.  This section is equivalent to
    // the R_PointInVertRegion() code (but using drawfloors).

    float z = (dthing->top + dthing->bottom) / 2.0f;

    std::vector<drawfloor_t *>::iterator DFI;

    for (DFI = dsub->floors.begin(); DFI != dsub->floors.end(); DFI++)
    {
        dfloor = *DFI;

        if (z <= dfloor->top_h)
            break;
    }

    SYS_ASSERT(dfloor);

    // link in sprite.  We'll shrink it if it gets clipped.
    LinkDrawthingIntoDrawfloor(dfloor, dthing);

    // HACK: the code below cannot handle Portals
    if (num_active_mirrors > 0)
        return;

    // handle never-clip things
    if (dthing->y_clipping == YCLIP_Never)
        return;

#if 0 // DISABLED FOR NOW : FIX LATER !!
	drawfloor_t *df_orig;
	drawthing_t *dnew, *dt_orig;

	float f1, f1_orig;
	float c1, c1_orig;

	// Note that sprites are not clipped by the lowest floor or
	// highest ceiling, OR by *solid* extrafloors (even translucent
	// ones) -- UNLESS y_clipping == YCLIP_Hard.

	f1 = dfloor->f_h;
	c1 = dfloor->c_h;

	// handle TRANSLUCENT + THICK floors (a bit of a hack)
	if (dfloor->ef && dfloor->ef->ef_info && !dfloor->is_highest &&
		(dfloor->ef->ef_info->type & kExtraFloorTypeThick) &&
		(dfloor->ef->top->translucency < 0.99f))
	{
		c1 = dfloor->top_h;
	}

	df_orig = dfloor;
	dt_orig = dthing;
	f1_orig = f1;
	c1_orig = c1;

	// Two sections here: Downward clipping (for sprite's bottom) and
	// Upward clipping (for sprite's top).  Both use the exact same
	// algorithm:
	//     
	//    WHILE (current must be clipped)
	//    {
	//       new := COPY OF current
	//       clip both current and new to the clip height
	//       current := new
	//       floor := NEXT floor
	//       link current into floor
	//    }

	// ---- downward section ----

	for (;;)
	{
		if (dfloor->is_lowest)
			break;

		if ((dthing->bottom >= f1 - SY_FUDGE) ||
			(dthing->top    <  f1 + SY_FUDGE))
			break;

		SYS_ASSERT(dfloor->lower->ef && dfloor->lower->ef->ef_info);

		if (! (dfloor->lower->ef->ef_info->type & kExtraFloorTypeLiquid))
			break;

		// sprite must be split (bottom), make a copy.

		dnew = R_GetDrawThing();

		dnew[0] = dthing[0];

		// shorten current sprite

		dthing->bottom = f1;

		SYS_ASSERT(dthing->bottom < dthing->top);

		// shorten new sprite

///----		dnew->y_offset += (dnew->top - f1);
		dnew->top = f1;

		SYS_ASSERT(dnew->bottom < dnew->top);

		// time to move on...

		dthing = dnew;
		dfloor = dfloor->lower;

		f1 = dfloor->f_h;
		c1 = dfloor->c_h;

		// handle TRANSLUCENT + THICK floors (a bit of a hack)
		if (dfloor->ef && dfloor->ef->ef_info && !dfloor->is_highest &&
			(dfloor->ef->ef_info->type & kExtraFloorTypeThick) &&
			(dfloor->ef->top->translucency < 0.99f))
		{
			c1 = dfloor->top_h;
		}

		// link new piece in
		LinkDrawthingIntoDrawfloor(dfloor, dthing);
	}

	if (dthing->y_clipping == YCLIP_Hard &&
		dthing->bottom <  f1 - SY_FUDGE &&
		dthing->top    >= f1 + SY_FUDGE)
	{
		// shorten current sprite

		dthing->bottom = f1;

		SYS_ASSERT(dthing->bottom < dthing->top);
	}

	dfloor = df_orig;
	dthing = dt_orig;
	f1 = f1_orig;
	c1 = c1_orig;

	// ---- upward section ----

	for (;;)
	{
		if (dfloor->is_highest)
			break;

		if ((dthing->bottom >= c1 - SY_FUDGE) ||
			(dthing->top    <  c1 + SY_FUDGE))
			break;

		SYS_ASSERT(dfloor->ef && dfloor->ef->ef_info);

		if (! (dfloor->ef->ef_info->type & kExtraFloorTypeLiquid))
			break;

		// sprite must be split (top), make a copy.

		dnew = R_GetDrawThing();

		dnew[0] = dthing[0];

		// shorten current sprite

///----		dthing->y_offset += (dthing->top - c1);
		dthing->top = c1;

		SYS_ASSERT(dthing->bottom < dthing->top);

		// shorten new sprite

		dnew->bottom = c1;

		SYS_ASSERT(dnew->bottom < dnew->top);

		// time to move on...

		dthing = dnew;
		dfloor = dfloor->higher;

		f1 = dfloor->f_h;
		c1 = dfloor->c_h;

		// handle TRANSLUCENT + THICK floors (a bit of a hack)
		if (dfloor->ef && dfloor->ef->ef_info && !dfloor->is_highest &&
			(dfloor->ef->ef_info->type & kExtraFloorTypeThick) &&
			(dfloor->ef->top->translucency < 0.99f))
		{
			c1 = dfloor->top_h;
		}

		// link new piece in
		LinkDrawthingIntoDrawfloor(dfloor, dthing);
	}

	if (dthing->y_clipping == YCLIP_Hard &&
		dthing->bottom <  c1 - SY_FUDGE &&
		dthing->top    >= c1 + SY_FUDGE)
	{
		// shorten current sprite

///----		dthing->y_offset += dthing->top - c1;
		dthing->top = c1;

		SYS_ASSERT(dthing->bottom < dthing->top);
	}

#endif
}

void RGL_WalkThing(drawsub_c *dsub, mobj_t *mo)
{
    EDGE_ZoneScoped;

    /* Visit a single thing that exists in the current subsector */

    SYS_ASSERT(mo->state);

    // ignore the camera itself
    if (mo == view_cam_mo && num_active_mirrors == 0)
        return;

    // ignore invisible things
    if (mo->visibility == INVISIBLE)
        return;

    // ignore things that are mid-teleport
    if (mo->teleport_tic-- > 0)
        return;

    bool is_model = (mo->state->flags & kStateFrameFlagModel) ? true : false;

    // transform the origin point
    float mx = mo->x, my = mo->y, mz = mo->z;

    // position interpolation
    if (mo->lerp_num > 1)
    {
        float along = mo->lerp_pos / (float)mo->lerp_num;

        mx = mo->lerp_from.X + (mx - mo->lerp_from.X) * along;
        my = mo->lerp_from.Y + (my - mo->lerp_from.Y) * along;
        mz = mo->lerp_from.Z + (mz - mo->lerp_from.Z) * along;
    }

    MIR_Coordinate(mx, my);

    float tr_x = mx - viewx;
    float tr_y = my - viewy;

    float tz = tr_x * viewcos + tr_y * viewsin;

    // thing is behind view plane?
    if (clip_scope != kBAMAngle180 && tz <= 0) // && !is_model)
        return;

    float tx = tr_x * viewsin - tr_y * viewcos;

    // too far off the side?
    // -ES- 1999/03/13 Fixed clipping to work with large FOVs (up to 176 deg)
    // rejects all sprites where angle>176 deg (arctan 32), since those
    // sprites would result in overflow in future calculations
    if (tz >= MINZ && fabs(tx) / 32 > tz)
        return;

    float     sink_mult = 0;
    float     bob_mult  = 0;
    sector_t *cur_sec   = mo->subsector->sector;
    if (!cur_sec->exfloor_used && !cur_sec->heightsec && abs(mo->z - cur_sec->f_h) < 1)
    {
        sink_mult = cur_sec->sink_depth;
        bob_mult  = cur_sec->bob_depth;
    }

    float hover_dz = 0;

    if (mo->hyperflags & kHyperFlagHover || ((mo->flags & kMapObjectFlagSpecial || mo->flags & kMapObjectFlagCorpse) && bob_mult > 0))
        hover_dz = GetHoverDZ(mo, bob_mult);

    if (sink_mult > 0)
        hover_dz -= (mo->height * 0.5 * sink_mult);

    bool           spr_flip = false;
    const image_c *image    = nullptr;

    float gzt = 0, gzb = 0;
    float pos1 = 0, pos2 = 0;

    if (!is_model)
    {
        image = R2_GetThingSprite2(mo, mx, my, &spr_flip);

        if (!image)
            return;

        // calculate edges of the shape
        float sprite_width  = IM_WIDTH(image);
        float sprite_height = IM_HEIGHT(image);
        float side_offset   = IM_OFFSETX(image);
        float top_offset    = IM_OFFSETY(image);

        if (spr_flip)
            side_offset = -side_offset;

        float xscale = mo->scale * mo->aspect;

        pos1 = (sprite_width / -2.0f - side_offset) * xscale;
        pos2 = (sprite_width / +2.0f - side_offset) * xscale;

        switch (mo->info->yalign_)
        {
        case SpriteYAlignmentTopDown:
            gzt = mo->z + mo->height + top_offset * mo->scale;
            gzb = gzt - sprite_height * mo->scale;
            break;

        case SpriteYAlignmentMiddle: {
            float _mz = mo->z + mo->height * 0.5 + top_offset * mo->scale;
            float dz  = sprite_height * 0.5 * mo->scale;

            gzt = _mz + dz;
            gzb = _mz - dz;
            break;
        }

        case SpriteYAlignmentBottomUp:
        default:
            gzb = mo->z + top_offset * mo->scale;
            gzt = gzb + sprite_height * mo->scale;
            break;
        }

        if (mo->hyperflags & kHyperFlagHover || (sink_mult > 0 || bob_mult > 0))
        {
            gzt += hover_dz;
            gzb += hover_dz;
        }
    } // if (! is_model)

    // fix for sprites that sit wrongly into the floor/ceiling
    int y_clipping = YCLIP_Soft;

    if (is_model || (mo->flags & kMapObjectFlagFuzzy) || ((mo->hyperflags & kHyperFlagHover) && AlmostEquals(sink_mult, 0.0f)))
    {
        y_clipping = YCLIP_Never;
    }
    // Lobo: new FLOOR_CLIP flag
    else if (mo->hyperflags & kHyperFlagFloorClip || sink_mult > 0)
    {
        // do nothing? just skip the other elseifs below
        y_clipping = YCLIP_Hard;
    }
    else if (sprite_kludge == 0 && gzb < mo->floorz)
    {
        // explosion ?
        if (mo->info->flags_ & kMapObjectFlagMissile)
        {
            y_clipping = YCLIP_Hard;
        }
        else
        {
            gzt += mo->floorz - gzb;
            gzb = mo->floorz;
        }
    }
    else if (sprite_kludge == 0 && gzt > mo->ceilingz)
    {
        // explosion ?
        if (mo->info->flags_ & kMapObjectFlagMissile)
        {
            y_clipping = YCLIP_Hard;
        }
        else
        {
            gzb -= gzt - mo->ceilingz;
            gzt = mo->ceilingz;
        }
    }

    if (!is_model)
    {
        if (gzb >= gzt)
            return;

        MIR_Height(gzb);
        MIR_Height(gzt);
    }

    // create new draw thing

    drawthing_t *dthing = R_GetDrawThing();
    dthing->Clear();

    dthing->mo = mo;
    dthing->mx = mx;
    dthing->my = my;
    dthing->mz = mz;

    dthing->props      = dsub->floors[0]->props;
    dthing->y_clipping = y_clipping;
    dthing->is_model   = is_model;

    dthing->image = image;
    dthing->flip  = spr_flip;

    dthing->tx = tx;
    dthing->tz = tz;

    dthing->top = dthing->orig_top = gzt;
    dthing->bottom = dthing->orig_bottom = gzb;
    ///----	dthing->y_offset = 0;

    float mir_scale = MIR_XYScale();

    dthing->left_dx  = pos1 * viewsin * mir_scale;
    dthing->left_dy  = pos1 * -viewcos * mir_scale;
    dthing->right_dx = pos2 * viewsin * mir_scale;
    dthing->right_dy = pos2 * -viewcos * mir_scale;

    R2_ClipSpriteVertically(dsub, dthing);
}

static void RGL_DrawModel(drawthing_t *dthing)
{
    EDGE_ZoneScoped;

    mobj_t *mo = dthing->mo;

    modeldef_c *md = W_GetModel(mo->state->sprite);

    const image_c *skin_img = md->skins[mo->model_skin];

    if (!skin_img && md->md2_model)
    {
        // I_Debugf("Render model: no skin %d\n", mo->model_skin);
        skin_img = W_ImageForDummySkin();
    }

    float z = dthing->mz;

    MIR_Height(z);

    float     sink_mult = 0;
    float     bob_mult  = 0;
    sector_t *cur_sec   = mo->subsector->sector;
    if (!cur_sec->exfloor_used && !cur_sec->heightsec && abs(mo->z - cur_sec->f_h) < 1)
    {
        sink_mult = cur_sec->sink_depth;
        bob_mult  = cur_sec->bob_depth;
    }

    if (sink_mult > 0)
        z -= mo->height * 0.5 * sink_mult;

    if (mo->hyperflags & kHyperFlagHover || ((mo->flags & kMapObjectFlagSpecial || mo->flags & kMapObjectFlagCorpse) && bob_mult > 0))
        z += GetHoverDZ(mo, bob_mult);

    int   last_frame = mo->state->frame;
    float lerp       = 0.0;

    if (mo->model_last_frame >= 0)
    {
        last_frame = mo->model_last_frame;

        SYS_ASSERT(mo->state->tics > 1);

        lerp = (mo->state->tics - mo->tics + 1) / (float)(mo->state->tics);
        lerp = HMM_Clamp(0, lerp, 1);
    }

    if (md->md2_model)
        MD2_RenderModel(md->md2_model, skin_img, false, last_frame, mo->state->frame, lerp, dthing->mx, dthing->my, z,
                        mo, mo->props, mo->model_scale, mo->model_aspect, mo->info->model_bias_, mo->info->model_rotate_);
    else if (md->mdl_model)
        MDL_RenderModel(md->mdl_model, skin_img, false, last_frame, mo->state->frame, lerp, dthing->mx, dthing->my, z,
                        mo, mo->props, mo->model_scale, mo->model_aspect, mo->info->model_bias_, mo->info->model_rotate_);
}

typedef struct
{
    mobj_t *mo;

    HMM_Vec3 vert[4];
    HMM_Vec2 texc[4];
    HMM_Vec3 normal;

    multi_color_c col[4];
} thing_coord_data_t;

static void DLIT_Thing(mobj_t *mo, void *dataptr)
{
    thing_coord_data_t *data = (thing_coord_data_t *)dataptr;

    // dynamic lights do not light themselves up!
    if (mo == data->mo)
        return;

    SYS_ASSERT(mo->dlight.shader);

    for (int v = 0; v < 4; v++)
    {
        mo->dlight.shader->Sample(data->col + v, data->vert[v].X, data->vert[v].Y, data->vert[v].Z);
    }
}

void RGL_DrawThing(drawfloor_t *dfloor, drawthing_t *dthing)
{
    EDGE_ZoneScoped;

    ecframe_stats.draw_things++;

    if (dthing->is_model)
    {
        RGL_DrawModel(dthing);
        return;
    }

    mobj_t *mo = dthing->mo;

    bool is_fuzzy = (mo->flags & kMapObjectFlagFuzzy) ? true : false;

    float trans = mo->visibility;

    float dx = 0, dy = 0;

    if (trans <= 0)
        return;

    const image_c *image = dthing->image;

    GLuint tex_id = W_ImageCache(image, false, ren_fx_colmap ? ren_fx_colmap : dthing->mo->info->palremap_);

    //	float w = IM_WIDTH(image);
    float h     = IM_HEIGHT(image);
    float right = IM_RIGHT(image);
    float top   = IM_TOP(image);

    float x1b, y1b, z1b, x1t, y1t, z1t;
    float x2b, y2b, z2b, x2t, y2t, z2t;

    x1b = x1t = dthing->mx + dthing->left_dx;
    y1b = y1t = dthing->my + dthing->left_dy;
    x2b = x2t = dthing->mx + dthing->right_dx;
    y2b = y2t = dthing->my + dthing->right_dy;

    z1b = z2b = dthing->bottom;
    z1t = z2t = dthing->top;

    // MLook: tilt sprites so they look better
    if (MIR_XYScale() >= 0.99)
    {
        float _h    = dthing->orig_top - dthing->orig_bottom;
        float skew2 = _h;

        if (mo->radius >= 1.0f && h > mo->radius)
            skew2 = mo->radius;

        float _dx = viewcos * sprite_skew * skew2;
        float _dy = viewsin * sprite_skew * skew2;

        float top_q    = ((dthing->top - dthing->orig_bottom) / _h) - 0.5f;
        float bottom_q = ((dthing->orig_top - dthing->bottom) / _h) - 0.5f;

        x1t += top_q * _dx;
        y1t += top_q * _dy;
        x2t += top_q * _dx;
        y2t += top_q * _dy;

        x1b -= bottom_q * _dx;
        y1b -= bottom_q * _dy;
        x2b -= bottom_q * _dx;
        y2b -= bottom_q * _dy;
    }

    float tex_x1 = 0.001f;
    float tex_x2 = right - 0.001f;

    float tex_y1 = dthing->bottom - dthing->orig_bottom;
    float tex_y2 = tex_y1 + (z1t - z1b);

    float yscale = mo->scale * MIR_ZScale();

    SYS_ASSERT(h > 0);
    tex_y1 = top * tex_y1 / (h * yscale);
    tex_y2 = top * tex_y2 / (h * yscale);

    if (dthing->flip)
    {
        float temp = tex_x2;
        tex_x1     = right - tex_x1;
        tex_x2     = right - temp;
    }

    thing_coord_data_t data;

    data.mo = mo;

    data.vert[0] = {{x1b + dx, y1b + dy, z1b}};
    data.vert[1] = {{x1t + dx, y1t + dy, z1t}};
    data.vert[2] = {{x2t + dx, y2t + dy, z2t}};
    data.vert[3] = {{x2b + dx, y2b + dy, z2b}};

    data.texc[0] = {{tex_x1, tex_y1}};
    data.texc[1] = {{tex_x1, tex_y2}};
    data.texc[2] = {{tex_x2, tex_y2}};
    data.texc[3] = {{tex_x2, tex_y1}};

    data.normal = {{-viewcos, -viewsin, 0}};

    data.col[0].Clear();
    data.col[1].Clear();
    data.col[2].Clear();
    data.col[3].Clear();

    int blending = BL_Masked;

    if (trans >= 0.11f && image->opacity != OPAC_Complex)
        blending = BL_Less;

    if (trans < 0.99 || image->opacity == OPAC_Complex)
        blending |= BL_Alpha;

    if (mo->hyperflags & kHyperFlagNoZBufferUpdate)
        blending |= BL_NoZBuf;

    float  fuzz_mul = 0;
    HMM_Vec2 fuzz_add;

    fuzz_add = {{0, 0}};

    if (is_fuzzy)
    {
        blending = BL_Masked | BL_Alpha;
        trans    = 1.0f;

        float dist = P_ApproxDistance(mo->x - viewx, mo->y - viewy, mo->z - viewz);

        fuzz_mul = 0.8 / HMM_Clamp(20, dist, 700);

        FUZZ_Adjust(&fuzz_add, mo);
    }

    if (!is_fuzzy)
    {
        abstract_shader_c *shader = R_GetColormapShader(dthing->props, mo->state->bright, mo->subsector->sector);

        for (int v = 0; v < 4; v++)
        {
            shader->Sample(data.col + v, data.vert[v].X, data.vert[v].Y, data.vert[v].Z);
        }

        if (use_dlights && ren_extralight < 250)
        {
            float r = mo->radius + 32;

            P_DynamicLightIterator(mo->x - r, mo->y - r, mo->z, mo->x + r, mo->y + r, mo->z + mo->height, DLIT_Thing,
                                   &data);

            P_SectorGlowIterator(mo->subsector->sector, mo->x - r, mo->y - r, mo->z, mo->x + r, mo->y + r,
                                 mo->z + mo->height, DLIT_Thing, &data);
        }
    }

    /* draw the sprite */

    int num_pass = is_fuzzy ? 1 : (detail_level > 0 ? 4 : 3);

    RGBAColor fc_to_use = dthing->mo->subsector->sector->props.fog_color;
    float    fd_to_use = dthing->mo->subsector->sector->props.fog_density;
    // check for DDFLEVL fog
    if (fc_to_use == kRGBANoValue)
    {
        if (IS_SKY(mo->subsector->sector->ceil))
        {
            fc_to_use = currmap->outdoor_fog_color_;
            fd_to_use = 0.01f * currmap->outdoor_fog_density_;
        }
        else
        {
            fc_to_use = currmap->indoor_fog_color_;
            fd_to_use = 0.01f * currmap->indoor_fog_density_;
        }
    }

    for (int pass = 0; pass < num_pass; pass++)
    {
        if (pass == 1)
        {
            blending &= ~BL_Alpha;
            blending |= BL_Add;
        }

        bool is_additive = (pass > 0 && pass == num_pass - 1);

        if (pass > 0 && pass < num_pass - 1)
        {
            if (GetMulticolMaxRGB(data.col, 4, false) <= 0)
                continue;
        }
        else if (is_additive)
        {
            if (GetMulticolMaxRGB(data.col, 4, true) <= 0)
                continue;
        }

        GLuint fuzz_tex = is_fuzzy ? W_ImageCache(fuzz_image, false) : 0;

        local_gl_vert_t *glvert = RGL_BeginUnit(GL_POLYGON, 4, is_additive ? (GLuint)ENV_SKIP_RGB : GL_MODULATE, tex_id,
                                                is_fuzzy ? GL_MODULATE : (GLuint)ENV_NONE, fuzz_tex, pass, blending,
                                                pass > 0 ? kRGBANoValue : fc_to_use, fd_to_use);

        for (int v_idx = 0; v_idx < 4; v_idx++)
        {
            local_gl_vert_t *dest = glvert + v_idx;

            dest->pos     = data.vert[v_idx];
            dest->texc[0] = data.texc[v_idx];
            dest->normal  = data.normal;

            if (is_fuzzy)
            {
                float ftx = (v_idx >= 2) ? (mo->radius * 2) : 0;
                float fty = (v_idx == 1 || v_idx == 2) ? (mo->height) : 0;

                dest->texc[1].X = ftx * fuzz_mul + fuzz_add.X;
                dest->texc[1].Y = fty * fuzz_mul + fuzz_add.Y;
                ;

                dest->rgba[0] = dest->rgba[1] = dest->rgba[2] = 0;
            }
            else if (!is_additive)
            {
                dest->rgba[0] = data.col[v_idx].mod_R / 255.0;
                dest->rgba[1] = data.col[v_idx].mod_G / 255.0;
                dest->rgba[2] = data.col[v_idx].mod_B / 255.0;

                data.col[v_idx].mod_R -= 256;
                data.col[v_idx].mod_G -= 256;
                data.col[v_idx].mod_B -= 256;
            }
            else
            {
                dest->rgba[0] = data.col[v_idx].add_R / 255.0;
                dest->rgba[1] = data.col[v_idx].add_G / 255.0;
                dest->rgba[2] = data.col[v_idx].add_B / 255.0;
            }

            dest->rgba[3] = trans;
        }

        RGL_EndUnit(4);
    }
}

void RGL_DrawSortThings(drawfloor_t *dfloor)
{
    //
    // As part my move to strip out Z_Zone usage and replace
    // it with array classes and more standard new and delete
    // calls, I've removed the QSORT() here and the array.
    // My main reason for doing that is that since I have to
    // modify the code here anyway, it is prudent to re-evaluate
    // their usage.
    //
    // The QSORT() mechanism used does an
    // allocation each time it is used and this is called
    // for each floor drawn in each subsector drawn, it is
    // reasonable to assume that removing it will give
    // something of speed improvement.
    //
    // This comes at a cost since optimisation is always
    // a balance between speed and size: drawthing_t now has
    // to hold 4 additional pointers. Two for the binary tree
    // (order building) and two for the the final linked list
    // (avoiding recursive function calls that the parsing the
    // binary tree would require).
    //
    // -ACB- 2004/08/17
    //

    EDGE_ZoneScoped;

    drawthing_t *head_dt;

    // Check we have something to draw
    head_dt = dfloor->things;
    if (!head_dt)
        return;

    drawthing_t *curr_dt, *dt, *next_dt;
    float        cmp_val;

    head_dt->rd_l = head_dt->rd_r = head_dt->rd_prev = head_dt->rd_next = nullptr;

    dt = nullptr; // Warning removal: This will always have been set

    curr_dt = head_dt->next;
    while (curr_dt)
    {
        curr_dt->rd_l = curr_dt->rd_r = nullptr;

        // Parse the tree to find our place
        next_dt = head_dt;
        do
        {
            dt = next_dt;

            cmp_val = dt->tz - curr_dt->tz;
            if (cmp_val == 0.0f)
            {
                // Resolve Z fight by letting the mobj pointer values settle it
                int offset = dt->mo - curr_dt->mo;
                cmp_val    = (float)offset;
            }

            if (cmp_val < 0.0f)
                next_dt = dt->rd_l;
            else
                next_dt = dt->rd_r;
        } while (next_dt);

        // Update our place
        if (cmp_val < 0.0f)
        {
            // Update the binary tree
            dt->rd_l = curr_dt;

            // Update the linked list (Insert behind node)
            if (dt->rd_prev)
                dt->rd_prev->rd_next = curr_dt;

            curr_dt->rd_prev = dt->rd_prev;
            curr_dt->rd_next = dt;

            dt->rd_prev = curr_dt;
        }
        else
        {
            // Update the binary tree
            dt->rd_r = curr_dt;

            // Update the linked list (Insert infront of node)
            if (dt->rd_next)
                dt->rd_next->rd_prev = curr_dt;

            curr_dt->rd_next = dt->rd_next;
            curr_dt->rd_prev = dt;

            dt->rd_next = curr_dt;
        }

        curr_dt = curr_dt->next;
    }

    // Find the first to draw
    while (head_dt->rd_prev)
        head_dt = head_dt->rd_prev;

    // Draw...
    for (dt = head_dt; dt; dt = dt->rd_next)
        RGL_DrawThing(dfloor, dt);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
