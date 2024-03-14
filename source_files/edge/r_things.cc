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

#include <math.h>

#include "AlmostEquals.h"
#include "coal.h"
#include "dm_defs.h"
#include "dm_state.h"
#include "edge_profiling.h"
#include "g_game.h"  //current_map
#include "i_defs_gl.h"
#include "im_data.h"
#include "im_funcs.h"
#include "m_misc.h"  // !!!! model test
#include "math_color.h"
#include "p_local.h"
#include "r_colormap.h"
#include "r_defs.h"
#include "r_draw.h"
#include "r_effects.h"
#include "r_gldefs.h"
#include "r_image.h"
#include "r_md2.h"
#include "r_mdl.h"
#include "r_misc.h"
#include "r_modes.h"
#include "r_shader.h"
#include "r_texgl.h"
#include "r_units.h"
#include "script/compat/lua_compat.h"
#include "sokol_color.h"
#include "str_util.h"
#include "vm_coal.h"
#include "w_model.h"
#include "w_sprite.h"

extern coal::vm_c *ui_vm;
extern double      CoalGetFloat(coal::vm_c *vm, const char *mod_name,
                                const char *var_name);

extern bool erraticism_active;

#define EDGE_DEBUG 0

EDGE_DEFINE_CONSOLE_VARIABLE(crosshair_style, "0",
                             kConsoleVariableFlagArchive)  // shape
EDGE_DEFINE_CONSOLE_VARIABLE(crosshair_color, "0",
                             kConsoleVariableFlagArchive)  // 0 .. 7
EDGE_DEFINE_CONSOLE_VARIABLE(
    crosshair_size, "16.0",
    kConsoleVariableFlagArchive)  // pixels on a 320x200 screen
EDGE_DEFINE_CONSOLE_VARIABLE(crosshair_brightness, "1.0",
                             kConsoleVariableFlagArchive)  // 1.0 is normal

float sprite_skew;

extern MapObject *view_camera_map_object;
extern float      widescreen_view_width_multiplier;

// The minimum distance between player and a visible sprite.
static constexpr float kMinimumSpriteDistance = 4.0f;

static const Image *crosshair_image;
static int          crosshair_which;

static float GetHoverDeltaZ(MapObject *mo, float bob_mult = 0)
{
    if (time_stop_active || erraticism_active) return mo->phase_;

    // compute a different phase for different objects
    BAMAngle phase = (BAMAngle)(long long)mo;
    phase ^= (BAMAngle)(phase << 19);
    phase += (BAMAngle)(level_time_elapsed << (kBAMAngleBits - 6));

    mo->phase_ = epi::BAMSin(phase);

    if (mo->hyper_flags_ & kHyperFlagHover)
        mo->phase_ *= 4.0f;
    else if (bob_mult > 0)
        mo->phase_ *= (mo->height_ * 0.5 * bob_mult);

    return mo->phase_;
}

struct PlayerSpriteCoordinateData
{
    HMM_Vec3 vertices[4];
    HMM_Vec2 texture_coordinates[4];
    HMM_Vec3 light_position;

    ColorMixer colors[4];
};

static void DLIT_PSprite(MapObject *mo, void *dataptr)
{
    PlayerSpriteCoordinateData *data = (PlayerSpriteCoordinateData *)dataptr;

    EPI_ASSERT(mo->dynamic_light_.shader);

    mo->dynamic_light_.shader->Sample(data->colors + 0, data->light_position.X,
                                      data->light_position.Y,
                                      data->light_position.Z);
}

static int GetMulticolMaxRGB(ColorMixer *cols, int num, bool additive)
{
    int result = 0;

    for (; num > 0; num--, cols++)
    {
        int mx = additive ? cols->add_MAX() : cols->mod_MAX();

        result = HMM_MAX(result, mx);
    }

    return result;
}

static void RendererDrawPSprite(PlayerSprite *psp, int which, Player *player,
                                RegionProperties *props, const State *state)
{
    if (state->flags & kStateFrameFlagModel) return;

    // determine sprite patch
    bool         flip;
    const Image *image =
        RendererGetOtherSprite(state->sprite, state->frame, &flip);

    if (!image) return;

    GLuint tex_id = ImageCache(image, false,
                               (which == kPlayerSpriteCrosshair)
                                   ? nullptr
                                   : render_view_effect_colormap);

    float w     = image->ScaledWidthActual();
    float h     = image->ScaledHeightActual();
    float right = image->Right();
    float top   = image->Top();
    float ratio = 1.0f;

    bool is_fuzzy =
        (player->map_object_->flags_ & kMapObjectFlagFuzzy) ? true : false;

    float trans = player->map_object_->visibility_;

    if (which == kPlayerSpriteCrosshair)
    {
        if (!player->weapons_[player->ready_weapon_]
                 .info->ignore_crosshair_scaling_)
            ratio = crosshair_size.f_ / w;

        w *= ratio;
        h *= ratio;
        is_fuzzy = false;
        trans    = 1.0f;
    }

    // Lobo: no sense having the zoom crosshair fuzzy
    if (which == kPlayerSpriteWeapon && view_is_zoomed &&
        player->weapons_[player->ready_weapon_].info->zoom_state_ > 0)
    {
        is_fuzzy = false;
        trans    = 1.0f;
    }

    trans *= psp->visibility;

    if (trans <= 0) return;

    float tex_top_h = top;   /// ## 1.00f; // 0.98;
    float tex_bot_h = 0.0f;  /// ## 1.00f - top;  // 1.02 - bottom;

    float tex_x1 = 0.002f;
    float tex_x2 = right - 0.002f;

    if (flip)
    {
        tex_x1 = right - tex_x1;
        tex_x2 = right - tex_x2;
    }

    float coord_W = 320.0f * widescreen_view_width_multiplier;
    float coord_H = 200.0f;

    float tx1 = (coord_W - w) / 2.0 + psp->screen_x - image->ScaledOffsetX();
    float tx2 = tx1 + w;

    float ty1 = -psp->screen_y + image->ScaledOffsetY() -
                ((h - image->ScaledHeightActual()) * 0.5f);

    if (LuaUseLuaHud())
    {
        // Lobo 2022: Apply sprite Y offset, mainly for Heretic weapons.
        if ((state->flags & kStateFrameFlagWeapon) &&
            (player->ready_weapon_ >= 0))
            ty1 += LuaGetFloat(LuaGetGlobalVM(), "hud", "universal_y_adjust") +
                   player->weapons_[player->ready_weapon_].info->y_adjust_;
    }
    else
    {
        // Lobo 2022: Apply sprite Y offset, mainly for Heretic weapons.
        if ((state->flags & kStateFrameFlagWeapon) &&
            (player->ready_weapon_ >= 0))
            ty1 += CoalGetFloat(ui_vm, "hud", "universal_y_adjust") +
                   player->weapons_[player->ready_weapon_].info->y_adjust_;
    }

    float ty2 = ty1 + h;

    float x1b, y1b, x1t, y1t, x2b, y2b, x2t, y2t;  // screen coords

    x1b = x1t = view_window_width * tx1 / coord_W;
    x2b = x2t = view_window_width * tx2 / coord_W;

    y1b = y2b = view_window_height * ty1 / coord_H;
    y1t = y2t = view_window_height * ty2 / coord_H;

    // clip psprite to view window
    glEnable(GL_SCISSOR_TEST);

    glScissor(view_window_x, view_window_y, view_window_width,
              view_window_height);

    x1b = (float)view_window_x + x1b;
    x1t = (float)view_window_x + x1t;
    x2t = (float)view_window_x + x2t;
    x2b = (float)view_window_x + x2b;

    y1b = (float)view_window_y + y1b - 1;
    y1t = (float)view_window_y + y1t - 1;
    y2t = (float)view_window_y + y2t - 1;
    y2b = (float)view_window_y + y2b - 1;

    PlayerSpriteCoordinateData data;

    data.vertices[0] = {{x1b, y1b, 0}};
    data.vertices[1] = {{x1t, y1t, 0}};
    data.vertices[2] = {{x2t, y1t, 0}};
    data.vertices[3] = {{x2b, y2b, 0}};

    data.texture_coordinates[0] = {{tex_x1, tex_bot_h}};
    data.texture_coordinates[1] = {{tex_x1, tex_top_h}};
    data.texture_coordinates[2] = {{tex_x2, tex_top_h}};
    data.texture_coordinates[3] = {{tex_x2, tex_bot_h}};

    float away = 120.0;

    data.light_position.X = player->map_object_->x + view_cosine * away;
    data.light_position.Y = player->map_object_->y + view_sine * away;
    data.light_position.Z =
        player->map_object_->z +
        player->map_object_->height_ * player->map_object_->info_->shotheight_;

    data.colors[0].Clear();

    int blending = kBlendingMasked;

    if (trans >= 0.11f && image->opacity_ != kOpacityComplex)
        blending = kBlendingLess;

    if (trans < 0.99 || image->opacity_ == kOpacityComplex)
        blending |= kBlendingAlpha;

    if (is_fuzzy)
    {
        blending = kBlendingMasked | kBlendingAlpha;
        trans    = 1.0f;
    }

    RGBAColor fc_to_use =
        player->map_object_->subsector_->sector->properties.fog_color;
    float fd_to_use =
        player->map_object_->subsector_->sector->properties.fog_density;
    // check for DDFLEVL fog
    if (fc_to_use == kRGBANoValue)
    {
        if (EDGE_IMAGE_IS_SKY(player->map_object_->subsector_->sector->ceiling))
        {
            fc_to_use = current_map->outdoor_fog_color_;
            fd_to_use = 0.01f * current_map->outdoor_fog_density_;
        }
        else
        {
            fc_to_use = current_map->indoor_fog_color_;
            fd_to_use = 0.01f * current_map->indoor_fog_density_;
        }
    }

    if (!is_fuzzy)
    {
        AbstractShader *shader = GetColormapShader(
            props, state->bright, player->map_object_->subsector_->sector);

        shader->Sample(data.colors + 0, data.light_position.X,
                       data.light_position.Y, data.light_position.Z);

        if (fc_to_use != kRGBANoValue)
        {
            int       mix_factor = RoundToInteger(255.0f * (fd_to_use * 75));
            RGBAColor mixme =
                epi::MixRGBA(epi::MakeRGBA(data.colors[0].modulate_red_,
                                           data.colors[0].modulate_green_,
                                           data.colors[0].modulate_blue_),
                             fc_to_use, mix_factor);
            data.colors[0].modulate_red_   = epi::GetRGBARed(mixme);
            data.colors[0].modulate_green_ = epi::GetRGBAGreen(mixme);
            data.colors[0].modulate_blue_  = epi::GetRGBABlue(mixme);
            mixme = epi::MixRGBA(epi::MakeRGBA(data.colors[0].add_red_,
                                               data.colors[0].add_green_,
                                               data.colors[0].add_blue_),
                                 fc_to_use, mix_factor);
            data.colors[0].add_red_   = epi::GetRGBARed(mixme);
            data.colors[0].add_green_ = epi::GetRGBAGreen(mixme);
            data.colors[0].add_blue_  = epi::GetRGBABlue(mixme);
        }

        if (use_dynamic_lights && render_view_extra_light < 250)
        {
            data.light_position.X = player->map_object_->x + view_cosine * 24;
            data.light_position.Y = player->map_object_->y + view_sine * 24;

            float r = 96;

            DynamicLightIterator(
                data.light_position.X - r, data.light_position.Y - r,
                player->map_object_->z, data.light_position.X + r,
                data.light_position.Y + r,
                player->map_object_->z + player->map_object_->height_,
                DLIT_PSprite, &data);

            SectorGlowIterator(
                player->map_object_->subsector_->sector,
                data.light_position.X - r, data.light_position.Y - r,
                player->map_object_->z, data.light_position.X + r,
                data.light_position.Y + r,
                player->map_object_->z + player->map_object_->height_,
                DLIT_PSprite, &data);
        }
    }

    // FIXME: sample at least TWO points (left and right edges)
    data.colors[1] = data.colors[0];
    data.colors[2] = data.colors[0];
    data.colors[3] = data.colors[0];

    /* draw the weapon */

    RendererStartUnits(false);

    int num_pass = is_fuzzy ? 1 : (detail_level > 0 ? 4 : 3);

    for (int pass = 0; pass < num_pass; pass++)
    {
        if (pass == 1)
        {
            blending &= ~kBlendingAlpha;
            blending |= kBlendingAdd;
        }

        bool is_additive = (pass > 0 && pass == num_pass - 1);

        if (pass > 0 && pass < num_pass - 1)
        {
            if (GetMulticolMaxRGB(data.colors, 4, false) <= 0) continue;
        }
        else if (is_additive)
        {
            if (GetMulticolMaxRGB(data.colors, 4, true) <= 0) continue;
        }

        GLuint fuzz_tex = is_fuzzy ? ImageCache(fuzz_image, false) : 0;

        RendererVertex *glvert = RendererBeginUnit(
            GL_POLYGON, 4,
            is_additive ? (GLuint)kTextureEnvironmentSkipRgb : GL_MODULATE,
            tex_id, is_fuzzy ? GL_MODULATE : (GLuint)kTextureEnvironmentDisable,
            fuzz_tex, pass, blending, pass > 0 ? kRGBANoValue : fc_to_use,
            fd_to_use);

        for (int v_idx = 0; v_idx < 4; v_idx++)
        {
            RendererVertex *dest = glvert + v_idx;

            dest->position               = data.vertices[v_idx];
            dest->texture_coordinates[0] = data.texture_coordinates[v_idx];

            dest->normal = {{0, 0, 1}};

            if (is_fuzzy)
            {
                dest->texture_coordinates[1].X =
                    dest->position.X / (float)current_screen_width;
                dest->texture_coordinates[1].Y =
                    dest->position.Y / (float)current_screen_height;

                FuzzAdjust(&dest->texture_coordinates[1], player->map_object_);

                dest->rgba_color[0]     = dest->rgba_color[1] =
                    dest->rgba_color[2] = 0;
            }
            else if (!is_additive)
            {
                dest->rgba_color[0] = data.colors[v_idx].modulate_red_ / 255.0;
                dest->rgba_color[1] =
                    data.colors[v_idx].modulate_green_ / 255.0;
                dest->rgba_color[2] = data.colors[v_idx].modulate_blue_ / 255.0;

                data.colors[v_idx].modulate_red_ -= 256;
                data.colors[v_idx].modulate_green_ -= 256;
                data.colors[v_idx].modulate_blue_ -= 256;
            }
            else
            {
                dest->rgba_color[0] = data.colors[v_idx].add_red_ / 255.0;
                dest->rgba_color[1] = data.colors[v_idx].add_green_ / 255.0;
                dest->rgba_color[2] = data.colors[v_idx].add_blue_ / 255.0;
            }

            dest->rgba_color[3] = trans;
        }

        RendererEndUnit(4);
    }

    RendererFinishUnits();

    glDisable(GL_SCISSOR_TEST);
}

static const RGBAColor crosshair_colors[8] = {
    SG_LIGHT_GRAY_RGBA32, SG_BLUE_RGBA32,        SG_GREEN_RGBA32,
    SG_CYAN_RGBA32,       SG_RED_RGBA32,         SG_FUCHSIA_RGBA32,
    SG_YELLOW_RGBA32,     SG_DARK_ORANGE_RGBA32,
};

static void DrawStdCrossHair(void)
{
    if (crosshair_style.d_ <= 0 || crosshair_style.d_ > 9) return;

    if (crosshair_size.f_ < 0.1 || crosshair_brightness.f_ < 0.1) return;

    if (!crosshair_image || crosshair_which != crosshair_style.d_)
    {
        crosshair_which = crosshair_style.d_;

        crosshair_image = ImageLookup(
            epi::StringFormat("STANDARD_CROSSHAIR_%d", crosshair_which)
                .c_str());
    }

    GLuint tex_id = ImageCache(crosshair_image);

    static int xh_count = 0;
    static int xh_dir   = 1;

    // -jc- Pulsating
    if (xh_count == 31)
        xh_dir = -1;
    else if (xh_count == 0)
        xh_dir = 1;

    xh_count += xh_dir;

    RGBAColor color     = crosshair_colors[crosshair_color.d_ & 7];
    float     intensity = 1.0f - xh_count / 100.0f;

    intensity *= crosshair_brightness.f_;

    float r = epi::GetRGBARed(color) * intensity / 255.0f;
    float g = epi::GetRGBAGreen(color) * intensity / 255.0f;
    float b = epi::GetRGBABlue(color) * intensity / 255.0f;

    float x = view_window_x + view_window_width / 2;
    float y = view_window_y + view_window_height / 2;

    float w = RoundToInteger(current_screen_width * crosshair_size.f_ / 640.0f);

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

void RendererDrawWeaponSprites(Player *p)
{
    // special handling for zoom: show viewfinder
    if (view_is_zoomed)
    {
        PlayerSprite *psp = &p->player_sprites_[kPlayerSpriteWeapon];

        if ((p->ready_weapon_ < 0) || (psp->state == 0)) return;

        WeaponDefinition *w = p->weapons_[p->ready_weapon_].info;

        // 2023.06.13 - If zoom state missing but weapon can zoom, allow the
        // regular psprite drawing routines to occur (old EDGE behavior)
        if (w->zoom_state_ > 0)
        {
            RendererDrawPSprite(psp, kPlayerSpriteWeapon, p, view_properties,
                                states + w->zoom_state_);
            return;
        }
    }

    // add all active player_sprites_
    // Note: order is significant

    // Lobo 2022:
    // Allow changing the order of weapon sprite
    // rendering so that FLASH states are
    // drawn in front of the WEAPON states
    bool FlashFirst = false;
    if (p->ready_weapon_ >= 0)
    {
        FlashFirst = p->weapons_[p->ready_weapon_].info->render_invert_;
    }

    if (FlashFirst == false)
    {
        for (int i = 0; i < kTotalPlayerSpriteTypes; i++)  // normal
        {
            PlayerSprite *psp = &p->player_sprites_[i];

            if ((p->ready_weapon_ < 0) || (psp->state == 0)) continue;

            RendererDrawPSprite(psp, i, p, view_properties, psp->state);
        }
    }
    else
    {
        for (int i = kTotalPlayerSpriteTypes - 1; i >= 0; i--)  // go backwards
        {
            PlayerSprite *psp = &p->player_sprites_[i];

            if ((p->ready_weapon_ < 0) || (psp->state == 0)) continue;

            RendererDrawPSprite(psp, i, p, view_properties, psp->state);
        }
    }
}

void RendererDrawCrosshair(Player *p)
{
    if (view_is_zoomed && p->weapons_[p->ready_weapon_].info->zoom_state_ > 0)
    {
        // Only skip crosshair if there is a dedicated zoom state, which
        // should be providing its own
        return;
    }
    else
    {
        PlayerSprite *psp = &p->player_sprites_[kPlayerSpriteCrosshair];

        if (p->ready_weapon_ >= 0 && psp->state != 0) return;
    }

    if (p->health_ > 0) DrawStdCrossHair();
}

void RendererDrawWeaponModel(Player *p)
{
    if (view_is_zoomed && p->weapons_[p->ready_weapon_].info->zoom_state_ > 0)
        return;

    PlayerSprite *psp = &p->player_sprites_[kPlayerSpriteWeapon];

    if (p->ready_weapon_ < 0) return;

    if (psp->state == 0) return;

    if (!(psp->state->flags & kStateFrameFlagModel)) return;

    WeaponDefinition *w = p->weapons_[p->ready_weapon_].info;

    ModelDefinition *md = GetModel(psp->state->sprite);

    int skin_num = p->weapons_[p->ready_weapon_].model_skin;

    const Image *skin_img = md->skins_[skin_num];

    if (!skin_img && md->md2_model_) { skin_img = ImageForDummySkin(); }

    float x = view_x + view_right.X * psp->screen_x / 8.0;
    float y = view_y + view_right.Y * psp->screen_x / 8.0;
    float z = view_z + view_right.Z * psp->screen_x / 8.0;

    x -= view_up.X * psp->screen_y / 10.0;
    y -= view_up.Y * psp->screen_y / 10.0;
    z -= view_up.Z * psp->screen_y / 10.0;

    x += view_forward.X * w->model_forward_;
    y += view_forward.Y * w->model_forward_;
    z += view_forward.Z * w->model_forward_;

    x += view_right.X * w->model_side_;
    y += view_right.Y * w->model_side_;
    z += view_right.Z * w->model_side_;

    int   last_frame = psp->state->frame;
    float lerp       = 0.0;

    if (p->weapon_last_frame_ >= 0)
    {
        EPI_ASSERT(psp->state);
        EPI_ASSERT(psp->state->tics > 1);

        last_frame = p->weapon_last_frame_;

        lerp = (psp->state->tics - psp->tics + 1) / (float)(psp->state->tics);
        lerp = HMM_Clamp(0, lerp, 1);
    }

    float bias = 0.0f;

    if (LuaUseLuaHud())
    {
        bias = LuaGetFloat(LuaGetGlobalVM(), "hud", "universal_y_adjust") +
               p->weapons_[p->ready_weapon_].info->y_adjust_;
    }
    else
    {
        bias = CoalGetFloat(ui_vm, "hud", "universal_y_adjust") +
               p->weapons_[p->ready_weapon_].info->y_adjust_;
    }

    bias /= 5;
    bias += w->model_bias_;

    if (md->md2_model_)
        Md2RenderModel(md->md2_model_, skin_img, true, last_frame,
                       psp->state->frame, lerp, x, y, z, p->map_object_,
                       view_properties, 1.0f /* scale */, w->model_aspect_,
                       bias, w->model_rotate_);
    else if (md->mdl_model_)
        MdlRenderModel(md->mdl_model_, skin_img, true, last_frame,
                       psp->state->frame, lerp, x, y, z, p->map_object_,
                       view_properties, 1.0f /* scale */, w->model_aspect_,
                       bias, w->model_rotate_);
}

// ============================================================================
// RendererBSP START
// ============================================================================

int sprite_kludge = 0;

static inline void LinkDrawThingIntoDrawFloor(DrawFloor *dfloor,
                                              DrawThing *dthing)
{
    dthing->properties = dfloor->properties;
    dthing->next       = dfloor->things;
    dthing->previous   = nullptr;

    if (dfloor->things) dfloor->things->previous = dthing;

    dfloor->things = dthing;
}

static const Image *RendererGetThingSprite2(MapObject *mo, float mx, float my,
                                            bool *flip)
{
    // Note: can return nullptr for no image.

    // decide which patch to use for sprite relative to player
    EPI_ASSERT(mo->state_);

    if (mo->state_->sprite == 0) return nullptr;

    SpriteFrame *frame = GetSpriteFrame(mo->state_->sprite, mo->state_->frame);

    if (!frame)
    {
        // show dummy sprite for missing frame
        (*flip) = false;
        return ImageForDummySprite();
    }

    int rot = 0;

    if (frame->rotations_ >= 8)
    {
        BAMAngle ang = mo->angle_;

        MirrorAngle(ang);

        BAMAngle from_view = RendererPointToAngle(view_x, view_y, mx, my);

        ang = from_view - ang + kBAMAngle180;

        if (MirrorReflective()) ang = (BAMAngle)0 - ang;

        if (frame->rotations_ == 16)
            rot = (ang + (kBAMAngle45 / 4)) >> (kBAMAngleBits - 4);
        else
            rot = (ang + (kBAMAngle45 / 2)) >> (kBAMAngleBits - 3);
    }

    EPI_ASSERT(0 <= rot && rot < 16);

    (*flip) = frame->flip_[rot] ? true : false;

    if (MirrorReflective()) (*flip) = !(*flip);

    if (!frame->images_[rot])
    {
        // show dummy sprite for missing rotation
        (*flip) = false;
        return ImageForDummySprite();
    }

    return frame->images_[rot];
}

const Image *RendererGetOtherSprite(int spritenum, int framenum, bool *flip)
{
    /* Used for non-object stuff, like weapons and finale */

    if (spritenum == 0) return nullptr;

    SpriteFrame *frame = GetSpriteFrame(spritenum, framenum);

    if (!frame || !frame->images_[0])
    {
        (*flip) = false;
        return ImageForDummySprite();
    }

    *flip = frame->flip_[0] ? true : false;

    return frame->images_[0];
}

static void RendererClipSpriteVertically(DrawSubsector *dsub, DrawThing *dthing)
{
    DrawFloor *dfloor = nullptr;

    // find the thing's nominal region.  This section is equivalent to
    // the RendererPointInVertRegion() code (but using drawfloors).

    float z = (dthing->top + dthing->bottom) / 2.0f;

    std::vector<DrawFloor *>::iterator DFI;

    for (DFI = dsub->floors.begin(); DFI != dsub->floors.end(); DFI++)
    {
        dfloor = *DFI;

        if (z <= dfloor->top_height) break;
    }

    EPI_ASSERT(dfloor);

    // link in sprite.  We'll shrink it if it gets clipped.
    LinkDrawThingIntoDrawFloor(dfloor, dthing);
}

void RendererWalkThing(DrawSubsector *dsub, MapObject *mo)
{
    EDGE_ZoneScoped;

    /* Visit a single thing that exists in the current subsector */

    EPI_ASSERT(mo->state_);

    // ignore the camera itself
    if (mo == view_camera_map_object && total_active_mirrors == 0) return;

    // ignore invisible things
    if (AlmostEquals(mo->visibility_, 0.0f)) return;

    // ignore things that are mid-teleport
    if (mo->teleport_tic_-- > 0) return;

    bool is_model = (mo->state_->flags & kStateFrameFlagModel) ? true : false;

    // transform the origin point
    float mx = mo->x, my = mo->y, mz = mo->z;

    // position interpolation
    if (mo->interpolation_number_ > 1)
    {
        float along =
            mo->interpolation_position_ / (float)mo->interpolation_number_;

        mx = mo->interpolation_from_.X +
             (mx - mo->interpolation_from_.X) * along;
        my = mo->interpolation_from_.Y +
             (my - mo->interpolation_from_.Y) * along;
        mz = mo->interpolation_from_.Z +
             (mz - mo->interpolation_from_.Z) * along;
    }

    MirrorCoordinate(mx, my);

    float tr_x = mx - view_x;
    float tr_y = my - view_y;

    float tz = tr_x * view_cosine + tr_y * view_sine;

    // thing is behind view plane?
    if (clip_scope != kBAMAngle180 && tz <= 0)  // && !is_model)
        return;

    float tx = tr_x * view_sine - tr_y * view_cosine;

    // too far off the side?
    // -ES- 1999/03/13 Fixed clipping to work with large FOVs (up to 176 deg)
    // rejects all sprites where angle>176 deg (arctan 32), since those
    // sprites would result in overflow in future calculations
    if (tz >= kMinimumSpriteDistance && fabs(tx) / 32 > tz) return;

    float   sink_mult = 0;
    float   bob_mult  = 0;
    Sector *cur_sec   = mo->subsector_->sector;
    if (!cur_sec->extrafloor_used && !cur_sec->height_sector &&
        abs(mo->z - cur_sec->floor_height) < 1)
    {
        sink_mult = cur_sec->sink_depth;
        bob_mult  = cur_sec->bob_depth;
    }

    float hover_dz = 0;

    if (mo->hyper_flags_ & kHyperFlagHover ||
        ((mo->flags_ & kMapObjectFlagSpecial ||
          mo->flags_ & kMapObjectFlagCorpse) &&
         bob_mult > 0))
        hover_dz = GetHoverDeltaZ(mo, bob_mult);

    if (sink_mult > 0) hover_dz -= (mo->height_ * 0.5 * sink_mult);

    bool         spr_flip = false;
    const Image *image    = nullptr;

    float gzt = 0, gzb = 0;
    float pos1 = 0, pos2 = 0;

    if (!is_model)
    {
        image = RendererGetThingSprite2(mo, mx, my, &spr_flip);

        if (!image) return;

        // calculate edges of the shape
        float sprite_width  = image->ScaledWidthActual();
        float sprite_height = image->ScaledHeightActual();
        float side_offset   = image->ScaledOffsetX();
        float top_offset    = image->ScaledOffsetY();

        if (spr_flip) side_offset = -side_offset;

        float xscale = mo->scale_ * mo->aspect_;

        pos1 = (sprite_width / -2.0f - side_offset) * xscale;
        pos2 = (sprite_width / +2.0f - side_offset) * xscale;

        switch (mo->info_->yalign_)
        {
            case SpriteYAlignmentTopDown:
                gzt = mo->z + mo->height_ + top_offset * mo->scale_;
                gzb = gzt - sprite_height * mo->scale_;
                break;

            case SpriteYAlignmentMiddle:
            {
                float _mz = mo->z + mo->height_ * 0.5 + top_offset * mo->scale_;
                float dz  = sprite_height * 0.5 * mo->scale_;

                gzt = _mz + dz;
                gzb = _mz - dz;
                break;
            }

            case SpriteYAlignmentBottomUp:
            default:
                gzb = mo->z + top_offset * mo->scale_;
                gzt = gzb + sprite_height * mo->scale_;
                break;
        }

        if (mo->hyper_flags_ & kHyperFlagHover ||
            (sink_mult > 0 || bob_mult > 0))
        {
            gzt += hover_dz;
            gzb += hover_dz;
        }
    }  // if (! is_model)

    // fix for sprites that sit wrongly into the floor/ceiling
    int y_clipping = kVerticalClipSoft;

    if (is_model || (mo->flags_ & kMapObjectFlagFuzzy) ||
        ((mo->hyper_flags_ & kHyperFlagHover) && AlmostEquals(sink_mult, 0.0f)))
    {
        y_clipping = kVerticalClipNever;
    }
    // Lobo: new FLOOR_CLIP flag
    else if (mo->hyper_flags_ & kHyperFlagFloorClip || sink_mult > 0)
    {
        // do nothing? just skip the other elseifs below
        y_clipping = kVerticalClipHard;
    }
    else if (sprite_kludge == 0 && gzb < mo->floor_z_)
    {
        // explosion ?
        if (mo->info_->flags_ & kMapObjectFlagMissile)
        {
            y_clipping = kVerticalClipHard;
        }
        else
        {
            gzt += mo->floor_z_ - gzb;
            gzb = mo->floor_z_;
        }
    }
    else if (sprite_kludge == 0 && gzt > mo->ceiling_z_)
    {
        // explosion ?
        if (mo->info_->flags_ & kMapObjectFlagMissile)
        {
            y_clipping = kVerticalClipHard;
        }
        else
        {
            gzb -= gzt - mo->ceiling_z_;
            gzt = mo->ceiling_z_;
        }
    }

    if (!is_model)
    {
        if (gzb >= gzt) return;

        MirrorHeight(gzb);
        MirrorHeight(gzt);
    }

    // create new draw thing

    DrawThing *dthing       = RendererGetDrawThing();
    dthing->next            = nullptr;
    dthing->previous        = nullptr;
    dthing->map_object      = nullptr;
    dthing->image           = nullptr;
    dthing->properties      = nullptr;
    dthing->render_left     = nullptr;
    dthing->render_next     = nullptr;
    dthing->render_previous = nullptr;
    dthing->render_right    = nullptr;

    dthing->map_object = mo;
    dthing->map_x      = mx;
    dthing->map_y      = my;
    dthing->map_z      = mz;

    dthing->properties = dsub->floors[0]->properties;
    dthing->y_clipping = y_clipping;
    dthing->is_model   = is_model;

    dthing->image = image;
    dthing->flip  = spr_flip;

    dthing->translated_z = tz;

    dthing->top = dthing->original_top = gzt;
    dthing->bottom = dthing->original_bottom = gzb;

    float mir_scale = MirrorXYScale();

    dthing->left_delta_x  = pos1 * view_sine * mir_scale;
    dthing->left_delta_y  = pos1 * -view_cosine * mir_scale;
    dthing->right_delta_x = pos2 * view_sine * mir_scale;
    dthing->right_delta_y = pos2 * -view_cosine * mir_scale;

    RendererClipSpriteVertically(dsub, dthing);
}

static void RendererDrawModel(DrawThing *dthing)
{
    EDGE_ZoneScoped;

    MapObject *mo = dthing->map_object;

    ModelDefinition *md = GetModel(mo->state_->sprite);

    const Image *skin_img = md->skins_[mo->model_skin_];

    if (!skin_img && md->md2_model_)
    {
        // LogDebug("Render model: no skin %d\n", mo->model_skin);
        skin_img = ImageForDummySkin();
    }

    float z = dthing->map_z;

    MirrorHeight(z);

    float   sink_mult = 0;
    float   bob_mult  = 0;
    Sector *cur_sec   = mo->subsector_->sector;
    if (!cur_sec->extrafloor_used && !cur_sec->height_sector &&
        abs(mo->z - cur_sec->floor_height) < 1)
    {
        sink_mult = cur_sec->sink_depth;
        bob_mult  = cur_sec->bob_depth;
    }

    if (sink_mult > 0) z -= mo->height_ * 0.5 * sink_mult;

    if (mo->hyper_flags_ & kHyperFlagHover ||
        ((mo->flags_ & kMapObjectFlagSpecial ||
          mo->flags_ & kMapObjectFlagCorpse) &&
         bob_mult > 0))
        z += GetHoverDeltaZ(mo, bob_mult);

    int   last_frame = mo->state_->frame;
    float lerp       = 0.0;

    if (mo->model_last_frame_ >= 0)
    {
        last_frame = mo->model_last_frame_;

        EPI_ASSERT(mo->state_->tics > 1);

        lerp = (mo->state_->tics - mo->tics_ + 1) / (float)(mo->state_->tics);
        lerp = HMM_Clamp(0, lerp, 1);
    }

    if (md->md2_model_)
        Md2RenderModel(md->md2_model_, skin_img, false, last_frame,
                       mo->state_->frame, lerp, dthing->map_x, dthing->map_y, z,
                       mo, mo->region_properties_, mo->model_scale_,
                       mo->model_aspect_, mo->info_->model_bias_,
                       mo->info_->model_rotate_);
    else if (md->mdl_model_)
        MdlRenderModel(md->mdl_model_, skin_img, false, last_frame,
                       mo->state_->frame, lerp, dthing->map_x, dthing->map_y, z,
                       mo, mo->region_properties_, mo->model_scale_,
                       mo->model_aspect_, mo->info_->model_bias_,
                       mo->info_->model_rotate_);
}

struct ThingCoordinateData
{
    MapObject *mo;

    HMM_Vec3 vertices[4];
    HMM_Vec2 texture_coordinates[4];
    HMM_Vec3 normal;

    ColorMixer colors[4];
};

static void DLIT_Thing(MapObject *mo, void *dataptr)
{
    ThingCoordinateData *data = (ThingCoordinateData *)dataptr;

    // dynamic lights do not light themselves up!
    if (mo == data->mo) return;

    EPI_ASSERT(mo->dynamic_light_.shader);

    for (int v = 0; v < 4; v++)
    {
        mo->dynamic_light_.shader->Sample(data->colors + v, data->vertices[v].X,
                                          data->vertices[v].Y,
                                          data->vertices[v].Z);
    }
}

void RendererDrawThing(DrawFloor *dfloor, DrawThing *dthing)
{
    EDGE_ZoneScoped;

    ec_frame_stats.draw_things++;

    if (dthing->is_model)
    {
        RendererDrawModel(dthing);
        return;
    }

    MapObject *mo = dthing->map_object;

    bool is_fuzzy = (mo->flags_ & kMapObjectFlagFuzzy) ? true : false;

    float trans = mo->visibility_;

    float dx = 0, dy = 0;

    if (trans <= 0) return;

    const Image *image = dthing->image;

    GLuint tex_id = ImageCache(image, false,
                               render_view_effect_colormap
                                   ? render_view_effect_colormap
                                   : dthing->map_object->info_->palremap_);

    float h     = image->ScaledHeightActual();
    float right = image->Right();
    float top   = image->Top();

    float x1b, y1b, z1b, x1t, y1t, z1t;
    float x2b, y2b, z2b, x2t, y2t, z2t;

    x1b = x1t = dthing->map_x + dthing->left_delta_x;
    y1b = y1t = dthing->map_y + dthing->left_delta_y;
    x2b = x2t = dthing->map_x + dthing->right_delta_x;
    y2b = y2t = dthing->map_y + dthing->right_delta_y;

    z1b = z2b = dthing->bottom;
    z1t = z2t = dthing->top;

    // MLook: tilt sprites so they look better
    if (MirrorXYScale() >= 0.99)
    {
        float _h    = dthing->original_top - dthing->original_bottom;
        float skew2 = _h;

        if (mo->radius_ >= 1.0f && h > mo->radius_) skew2 = mo->radius_;

        float _dx = view_cosine * sprite_skew * skew2;
        float _dy = view_sine * sprite_skew * skew2;

        float top_q    = ((dthing->top - dthing->original_bottom) / _h) - 0.5f;
        float bottom_q = ((dthing->original_top - dthing->bottom) / _h) - 0.5f;

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

    float tex_y1 = dthing->bottom - dthing->original_bottom;
    float tex_y2 = tex_y1 + (z1t - z1b);

    float yscale = mo->scale_ * MirrorZScale();

    EPI_ASSERT(h > 0);
    tex_y1 = top * tex_y1 / (h * yscale);
    tex_y2 = top * tex_y2 / (h * yscale);

    if (dthing->flip)
    {
        float temp = tex_x2;
        tex_x1     = right - tex_x1;
        tex_x2     = right - temp;
    }

    ThingCoordinateData data;

    data.mo = mo;

    data.vertices[0] = {{x1b + dx, y1b + dy, z1b}};
    data.vertices[1] = {{x1t + dx, y1t + dy, z1t}};
    data.vertices[2] = {{x2t + dx, y2t + dy, z2t}};
    data.vertices[3] = {{x2b + dx, y2b + dy, z2b}};

    data.texture_coordinates[0] = {{tex_x1, tex_y1}};
    data.texture_coordinates[1] = {{tex_x1, tex_y2}};
    data.texture_coordinates[2] = {{tex_x2, tex_y2}};
    data.texture_coordinates[3] = {{tex_x2, tex_y1}};

    data.normal = {{-view_cosine, -view_sine, 0}};

    data.colors[0].Clear();
    data.colors[1].Clear();
    data.colors[2].Clear();
    data.colors[3].Clear();

    int blending = kBlendingMasked;

    if (trans >= 0.11f && image->opacity_ != kOpacityComplex)
        blending = kBlendingLess;

    if (trans < 0.99 || image->opacity_ == kOpacityComplex)
        blending |= kBlendingAlpha;

    if (mo->hyper_flags_ & kHyperFlagNoZBufferUpdate)
        blending |= kBlendingNoZBuffer;

    float    fuzz_mul = 0;
    HMM_Vec2 fuzz_add;

    fuzz_add = {{0, 0}};

    if (is_fuzzy)
    {
        blending = kBlendingMasked | kBlendingAlpha;
        trans    = 1.0f;

        float dist =
            ApproximateDistance(mo->x - view_x, mo->y - view_y, mo->z - view_z);

        fuzz_mul = 0.8 / HMM_Clamp(20, dist, 700);

        FuzzAdjust(&fuzz_add, mo);
    }

    if (!is_fuzzy)
    {
        AbstractShader *shader = GetColormapShader(
            dthing->properties, mo->state_->bright, mo->subsector_->sector);

        for (int v = 0; v < 4; v++)
        {
            shader->Sample(data.colors + v, data.vertices[v].X,
                           data.vertices[v].Y, data.vertices[v].Z);
        }

        if (use_dynamic_lights && render_view_extra_light < 250)
        {
            float r = mo->radius_ + 32;

            DynamicLightIterator(mo->x - r, mo->y - r, mo->z, mo->x + r,
                                 mo->y + r, mo->z + mo->height_, DLIT_Thing,
                                 &data);

            SectorGlowIterator(mo->subsector_->sector, mo->x - r, mo->y - r,
                               mo->z, mo->x + r, mo->y + r, mo->z + mo->height_,
                               DLIT_Thing, &data);
        }
    }

    /* draw the sprite */

    int num_pass = is_fuzzy ? 1 : (detail_level > 0 ? 4 : 3);

    RGBAColor fc_to_use =
        dthing->map_object->subsector_->sector->properties.fog_color;
    float fd_to_use =
        dthing->map_object->subsector_->sector->properties.fog_density;
    // check for DDFLEVL fog
    if (fc_to_use == kRGBANoValue)
    {
        if (EDGE_IMAGE_IS_SKY(mo->subsector_->sector->ceiling))
        {
            fc_to_use = current_map->outdoor_fog_color_;
            fd_to_use = 0.01f * current_map->outdoor_fog_density_;
        }
        else
        {
            fc_to_use = current_map->indoor_fog_color_;
            fd_to_use = 0.01f * current_map->indoor_fog_density_;
        }
    }

    for (int pass = 0; pass < num_pass; pass++)
    {
        if (pass == 1)
        {
            blending &= ~kBlendingAlpha;
            blending |= kBlendingAdd;
        }

        bool is_additive = (pass > 0 && pass == num_pass - 1);

        if (pass > 0 && pass < num_pass - 1)
        {
            if (GetMulticolMaxRGB(data.colors, 4, false) <= 0) continue;
        }
        else if (is_additive)
        {
            if (GetMulticolMaxRGB(data.colors, 4, true) <= 0) continue;
        }

        GLuint fuzz_tex = is_fuzzy ? ImageCache(fuzz_image, false) : 0;

        RendererVertex *glvert = RendererBeginUnit(
            GL_POLYGON, 4,
            is_additive ? (GLuint)kTextureEnvironmentSkipRgb : GL_MODULATE,
            tex_id, is_fuzzy ? GL_MODULATE : (GLuint)kTextureEnvironmentDisable,
            fuzz_tex, pass, blending, pass > 0 ? kRGBANoValue : fc_to_use,
            fd_to_use);

        for (int v_idx = 0; v_idx < 4; v_idx++)
        {
            RendererVertex *dest = glvert + v_idx;

            dest->position               = data.vertices[v_idx];
            dest->texture_coordinates[0] = data.texture_coordinates[v_idx];
            dest->normal                 = data.normal;

            if (is_fuzzy)
            {
                float ftx = (v_idx >= 2) ? (mo->radius_ * 2) : 0;
                float fty = (v_idx == 1 || v_idx == 2) ? (mo->height_) : 0;

                dest->texture_coordinates[1].X = ftx * fuzz_mul + fuzz_add.X;
                dest->texture_coordinates[1].Y = fty * fuzz_mul + fuzz_add.Y;
                ;

                dest->rgba_color[0]     = dest->rgba_color[1] =
                    dest->rgba_color[2] = 0;
            }
            else if (!is_additive)
            {
                dest->rgba_color[0] = data.colors[v_idx].modulate_red_ / 255.0;
                dest->rgba_color[1] =
                    data.colors[v_idx].modulate_green_ / 255.0;
                dest->rgba_color[2] = data.colors[v_idx].modulate_blue_ / 255.0;

                data.colors[v_idx].modulate_red_ -= 256;
                data.colors[v_idx].modulate_green_ -= 256;
                data.colors[v_idx].modulate_blue_ -= 256;
            }
            else
            {
                dest->rgba_color[0] = data.colors[v_idx].add_red_ / 255.0;
                dest->rgba_color[1] = data.colors[v_idx].add_green_ / 255.0;
                dest->rgba_color[2] = data.colors[v_idx].add_blue_ / 255.0;
            }

            dest->rgba_color[3] = trans;
        }

        RendererEndUnit(4);
    }
}

void RendererDrawSortThings(DrawFloor *dfloor)
{
    //
    // As part my move to strip out Z_Zone usage and replace
    // it with array classes and more standard new and delete
    // calls, I've removed the EDGE_QSORT() here and the array.
    // My main reason for doing that is that since I have to
    // modify the code here anyway, it is prudent to re-evaluate
    // their usage.
    //
    // The EDGE_QSORT() mechanism used does an
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

    DrawThing *head_dt;

    // Check we have something to draw
    head_dt = dfloor->things;
    if (!head_dt) return;

    DrawThing *curr_dt, *dt, *next_dt;
    float      cmp_val;

    head_dt->render_left = head_dt->render_right = head_dt->render_previous =
        head_dt->render_next                     = nullptr;

    dt = nullptr;  // Warning removal: This will always have been set

    curr_dt = head_dt->next;
    while (curr_dt)
    {
        curr_dt->render_left = curr_dt->render_right = nullptr;

        // Parse the tree to find our place
        next_dt = head_dt;
        do {
            dt = next_dt;

            cmp_val = dt->translated_z - curr_dt->translated_z;
            if (AlmostEquals(cmp_val, 0.0f))
            {
                // Resolve Z fight by letting the mobj pointer values settle it
                int offset = dt->map_object - curr_dt->map_object;
                cmp_val    = (float)offset;
            }

            if (cmp_val < 0.0f)
                next_dt = dt->render_left;
            else
                next_dt = dt->render_right;
        } while (next_dt);

        // Update our place
        if (cmp_val < 0.0f)
        {
            // Update the binary tree
            dt->render_left = curr_dt;

            // Update the linked list (Insert behind node)
            if (dt->render_previous) dt->render_previous->render_next = curr_dt;

            curr_dt->render_previous = dt->render_previous;
            curr_dt->render_next     = dt;

            dt->render_previous = curr_dt;
        }
        else
        {
            // Update the binary tree
            dt->render_right = curr_dt;

            // Update the linked list (Insert infront of node)
            if (dt->render_next) dt->render_next->render_previous = curr_dt;

            curr_dt->render_next     = dt->render_next;
            curr_dt->render_previous = dt;

            dt->render_next = curr_dt;
        }

        curr_dt = curr_dt->next;
    }

    // Find the first to draw
    while (head_dt->render_previous) head_dt = head_dt->render_previous;

    // Draw...
    for (dt = head_dt; dt; dt = dt->render_next) RendererDrawThing(dfloor, dt);
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
