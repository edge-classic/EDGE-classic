//----------------------------------------------------------------------------
//  EDGE OpenGL Rendering (Screen Effects)
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

#include "dm_state.h"
#include "e_player.h"
#include "epi.h"
#include "hu_draw.h" // HUD* functions
#include "i_defs_gl.h"
#include "m_misc.h"
#include "n_network.h"
#include "r_colormap.h"
#include "r_image.h"
#include "r_misc.h"
#include "r_modes.h"
#include "r_texgl.h"
#include "r_units.h"
#include "w_wad.h"

int render_view_extra_light;

float render_view_red_multiplier   = 1.0f;
float render_view_green_multiplier = 1.0f;
float render_view_blue_multiplier  = 1.0f;

const Colormap *render_view_effect_colormap;

EDGE_DEFINE_CONSOLE_VARIABLE(power_fade_out, "1", kConsoleVariableFlagArchive)
EDGE_DEFINE_CONSOLE_VARIABLE(extra_light_step, "16", kConsoleVariableFlagArchive)
EDGE_DEFINE_CONSOLE_VARIABLE(debug_fullbright, "0", kConsoleVariableFlagCheat)

static inline float EffectStrength(Player *player)
{
    if (player->effect_left_ >= kMaximumEffectTime)
        return 1.0f;

    if (power_fade_out.d_ || reduce_flash)
    {
        return player->effect_left_ / (float)kMaximumEffectTime;
    }

    return (player->effect_left_ & 8) ? 1.0f : 0.0f;
}

//
// RendererRainbowEffect
//
// Effects that modify all colours, e.g. nightvision green.
//
void RendererRainbowEffect(Player *player)
{
    render_view_extra_light = debug_fullbright.d_ ? 255 : player ? player->extra_light_ * extra_light_step.d_ : 0;

    render_view_red_multiplier = render_view_green_multiplier = render_view_blue_multiplier = 1.0f;

    render_view_effect_colormap = nullptr;

    if (!player)
        return;

    float s = EffectStrength(player);

    if (s > 0 && player->powers_[kPowerTypeInvulnerable] > 0 && (player->effect_left_ & 8) && !reduce_flash)
    {
        if (invulnerability_effect == kInvulnerabilityTextured && !reduce_flash)
        {
            render_view_effect_colormap = player->effect_colourmap_;
        }
        else
        {
            render_view_red_multiplier   = 0.90f;
            render_view_green_multiplier = render_view_red_multiplier;
            render_view_blue_multiplier  = render_view_red_multiplier;
        }

        render_view_extra_light = 255;
        return;
    }

    if (s > 0 && player->powers_[kPowerTypeNightVision] > 0 && player->effect_colourmap_ && !debug_fullbright.d_)
    {
        float r, g, b;

        GetColormapRGB(player->effect_colourmap_, &r, &g, &b);

        render_view_red_multiplier   = 1.0f - (1.0f - r) * s;
        render_view_green_multiplier = 1.0f - (1.0f - g) * s;
        render_view_blue_multiplier  = 1.0f - (1.0f - b) * s;

        render_view_extra_light = int(s * 255);
        return;
    }

    if (s > 0 && player->powers_[kPowerTypeInfrared] > 0 && !debug_fullbright.d_)
    {
        render_view_extra_light = int(s * 255);
        return;
    }

    // Lobo 2021: un-hardcode berserk color tint
    if (s > 0 && player->powers_[kPowerTypeBerserk] > 0 && player->effect_colourmap_ && !debug_fullbright.d_)
    {
        float r, g, b;

        GetColormapRGB(player->effect_colourmap_, &r, &g, &b);

        render_view_red_multiplier   = 1.0f - (1.0f - r) * s;
        render_view_green_multiplier = 1.0f - (1.0f - g) * s;
        render_view_blue_multiplier  = 1.0f - (1.0f - b) * s;

        // fallthrough...
    }

    // AJA 2022: handle BOOM colormaps (linetype 242)
    Sector *sector = player->map_object_->subsector_->sector;

    if (sector->height_sector != nullptr)
    {
        const Colormap *colmap = nullptr;

        // see which region the camera is in
        if (view_z > sector->height_sector->ceiling_height)
            colmap = sector->height_sector_side->top.boom_colormap;
        else if (view_z < sector->height_sector->floor_height)
            colmap = sector->height_sector_side->bottom.boom_colormap;
        else
            colmap = sector->height_sector_side->middle.boom_colormap;

        render_view_effect_colormap = colmap;
    }
}

//
// RendererColourmapEffect
//
// For example: all white for invulnerability.
//
void RendererColourmapEffect(Player *player)
{
    float x1, y1;
    float x2, y2;

    float s = EffectStrength(player);

    if (s > 0 && player->powers_[kPowerTypeInvulnerable] > 0 && player->effect_colourmap_ &&
        (player->effect_left_ & 8 || reduce_flash))
    {
        if (invulnerability_effect == kInvulnerabilityTextured && !reduce_flash)
            return;

        if (!reduce_flash)
        {
            StartUnitBatch(false);

            RGBAColor unit_col = kRGBAWhite;

            RendererVertex *glvert =
                BeginRenderUnit(GL_QUADS, 4, GL_MODULATE, 0, (GLuint)kTextureEnvironmentDisable, 0, 0, kBlendingInvert);

            x1 = view_window_x;
            x2 = view_window_x + view_window_width;

            y1 = view_window_y + view_window_height;
            y2 = view_window_y;

            glvert->rgba       = unit_col;
            glvert++->position = {{x1, y1, 0}};
            glvert->rgba       = unit_col;
            glvert++->position = {{x2, y1, 0}};
            glvert->rgba       = unit_col;
            glvert++->position = {{x2, y2, 0}};
            glvert->rgba       = unit_col;
            glvert->position   = {{x1, y2, 0}};

            EndRenderUnit(4);

            FinishUnitBatch();
        }
        else
        {
            float old_alpha = HUDGetAlpha();
            HUDSetAlpha(0.0f);
            s = HMM_MAX(0.5f, s);
            HUDThinBox(hud_x_left, hud_visible_top, hud_x_right, hud_visible_bottom, epi::MakeRGBAFloat(s, s, s), 25.0f,
                       kBlendingInvert);
            HUDSetAlpha(old_alpha);
        }
    }
}

//
// RendererPaletteEffect
//
// For example: red wash for pain.
//
void RendererPaletteEffect(Player *player)
{
    uint8_t rgb_data[3];

    float s = EffectStrength(player);

    float old_alpha = HUDGetAlpha();

    RGBAColor unit_col = kRGBAWhite;

    if (s > 0 && player->powers_[kPowerTypeInvulnerable] > 0 && player->effect_colourmap_ &&
        (player->effect_left_ & 8 || reduce_flash))
    {
        return;
    }
    else if (s > 0 && player->powers_[kPowerTypeNightVision] > 0 && player->effect_colourmap_)
    {
        float r, g, b;
        GetColormapRGB(player->effect_colourmap_, &r, &g, &b);
        if (!reduce_flash)
            unit_col = epi::MakeRGBAFloat(r, g, b, 0.20f * s);
        else
        {
            HUDSetAlpha(0.20f * s);
            HUDThinBox(hud_x_left, hud_visible_top, hud_x_right, hud_visible_bottom, epi::MakeRGBAFloat(r, g, b),
                       25.0f);
        }
    }
    else
    {
        PalettedColourToRGB(playpal_black, rgb_data, player->last_damage_colour_, player->damage_count_);

        int rgb_max = HMM_MAX(rgb_data[0], HMM_MAX(rgb_data[1], rgb_data[2]));

        if (rgb_max == 0)
            return;

        rgb_max = HMM_MIN(200, rgb_max);

        if (!reduce_flash)
            unit_col = epi::MakeRGBAFloat((float)rgb_data[0] / (float)rgb_max, (float)rgb_data[1] / (float)rgb_max,
                                          (float)rgb_data[2] / (float)rgb_max, (float)rgb_max / 255.0f);
        else
        {
            HUDSetAlpha((float)rgb_max / 255.0f);
            HUDThinBox(hud_x_left, hud_visible_top, hud_x_right, hud_visible_bottom,
                       epi::MakeRGBAFloat((float)rgb_data[0] / rgb_max, (float)rgb_data[1] / rgb_max,
                                          (float)rgb_data[2] / rgb_max),
                       25.0f);
        }
    }

    HUDSetAlpha(old_alpha);

    if (!reduce_flash)
    {
        StartUnitBatch(false);

        RendererVertex *glvert =
            BeginRenderUnit(GL_QUADS, 4, GL_MODULATE, 0, (GLuint)kTextureEnvironmentDisable, 0, 0, kBlendingAlpha);

        glvert->rgba       = unit_col;
        glvert++->position = {{0, (float)current_screen_height, 0}};
        glvert->rgba       = unit_col;
        glvert++->position = {{(float)current_screen_width, (float)current_screen_height, 0}};
        glvert->rgba       = unit_col;
        glvert++->position = {{(float)current_screen_width, 0, 0}};
        glvert->rgba       = unit_col;
        glvert->position   = {{0, 0, 0}};

        EndRenderUnit(4);

        FinishUnitBatch();
    }
}

//----------------------------------------------------------------------------
//  FUZZY Emulation
//----------------------------------------------------------------------------

const Image *fuzz_image;

static float fuzz_y_offset;

void FuzzUpdate(void)
{
    if (!fuzz_image)
    {
        fuzz_image = ImageLookup("FUZZ_MAP", kImageNamespaceTexture, kImageLookupExact | kImageLookupNull);
        if (!fuzz_image)
            FatalError("Cannot find essential image: FUZZ_MAP\n");
    }

    fuzz_y_offset = ((hud_tic * 3) & 1023) / 256.0;
}

void FuzzAdjust(HMM_Vec2 *tc, MapObject *mo)
{
    if (uncapped_frames.d_)
    {
        tc->X += fmod(HMM_Lerp(mo->old_x_, fractional_tic, mo->x) / 520.0, 1.0);
        tc->Y += fmod(HMM_Lerp(mo->old_y_, fractional_tic, mo->y) / 520.0, 1.0) + fuzz_y_offset;
    }
    else
    {
        tc->X += fmod(mo->x / 520.0, 1.0);
        tc->Y += fmod(mo->y / 520.0, 1.0) + fuzz_y_offset;
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
