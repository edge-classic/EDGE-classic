//----------------------------------------------------------------------------
//  EDGE OpenGL Rendering (Wipes)
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

#include "r_wipe.h"

#include "i_defs_gl.h"
#include "i_system.h"
#include "im_data.h"
#include "m_random.h"
#include "n_network.h"
#include "r_backend.h"
#include "r_gldefs.h"
#include "r_image.h"
#include "r_modes.h"
#include "r_texgl.h"
#include "r_units.h"

// we're limited to one wipe at a time...
static ScreenWipe current_wipe_effect = kScreenWipeNone;

static int current_wipe_progress;
static int current_wipe_last_time;

static GLuint current_wipe_texture = 0;
static float  current_wipe_right;
static float  current_wipe_top;

static constexpr uint8_t kMeltSections = 128;
static int               melt_yoffs[kMeltSections + 1];
static int               old_melt_yoffs[kMeltSections + 1];

static inline uint8_t SpookyAlpha(int x, int y)
{
    y += (x & 32) / 2;

    x = (x & 31) - 15;
    y = (y & 31) - 15;

    return (x * x + y * y) / 2;
}

static void CaptureScreenAsTexture(bool speckly, bool spooky)
{
    int total_w = MakeValidTextureSize(current_screen_width);
    int total_h = MakeValidTextureSize(current_screen_height);

    ImageData img(total_w, total_h, 4);

    img.Clear();

    current_wipe_right = current_screen_width / (float)total_w;
    current_wipe_top   = current_screen_height / (float)total_h;

    render_backend->CaptureScreen(current_screen_width, current_screen_height, total_w * 4, img.PixelAt(0, 0));

    for (int y = 0; y < current_screen_height; y++)
    {
        uint8_t *dest = img.PixelAt(0, y);

        int rnd_val = y;

        if (spooky)
        {
            for (int x = 0; x < total_w; x++)
                dest[4 * x + 3] = SpookyAlpha(x, y);
        }
        else if (speckly)
        {
            for (int x = 0; x < total_w; x++)
            {
                rnd_val = rnd_val * 1103515245 + 12345;

                dest[4 * x + 3] = (rnd_val >> 16);
            }
        }
    }
    if (current_wipe_texture != 0)
    {
        render_state->DeleteTexture(&current_wipe_texture);
        current_wipe_texture = 0;
    }
    current_wipe_texture = UploadTexture(&img);
}

void BlackoutWipeTexture(void)
{
    int total_w = MakeValidTextureSize(current_screen_width);
    int total_h = MakeValidTextureSize(current_screen_height);

    ImageData img(total_w, total_h, 4);

    img.Clear();

    current_wipe_right = current_screen_width / (float)total_w;
    current_wipe_top   = current_screen_height / (float)total_h;

    for (int y = 0; y < current_screen_height; y++)
    {
        uint8_t *dest = img.PixelAt(0, y);

        dest[0] = dest[1] = dest[2] = 0;
        dest[3]                     = 1;
    }
    if (current_wipe_texture != 0)
    {
        render_state->DeleteTexture(&current_wipe_texture);
        current_wipe_texture = 0;
    }
    current_wipe_texture = UploadTexture(&img);
}

static void AllocateDrawStructsMelt(void)
{
    int x, r;

    melt_yoffs[0] = -(RandomByte() % 16);

    for (x = 1; x <= kMeltSections; x++)
    {
        r = (RandomByte() % 3) - 1;

        melt_yoffs[x]     = melt_yoffs[x - 1] + r;
        melt_yoffs[x]     = HMM_MAX(-15, HMM_MIN(0, melt_yoffs[x]));
        old_melt_yoffs[x] = melt_yoffs[x];
    }
}

static void UpdateMelt(int tics)
{
    int x, r;

    for (; tics > 0; tics--)
    {
        for (x = 0; x <= kMeltSections; x++)
        {
            r = melt_yoffs[x];

            old_melt_yoffs[x] = r;

            if (r < 0)
                r = 1;
            else if (r > 15)
                r = 8;
            else
                r += 1;

            melt_yoffs[x] += r;
        }
    }
}

void InitializeWipe(ScreenWipe effect)
{
    render_backend->OnFrameFinished([effect]() -> void {
        current_wipe_effect = effect;

        current_wipe_progress  = 0;
        current_wipe_last_time = -1;

        if (current_wipe_effect == kScreenWipeNone)
            return;

        CaptureScreenAsTexture(effect == kScreenWipePixelfade, effect == kScreenWipeSpooky);

        if (current_wipe_effect == kScreenWipeMelt)
            AllocateDrawStructsMelt();
    });
}

void StopWipe(void)
{
    current_wipe_effect = kScreenWipeNone;

    if (current_wipe_texture != 0)
    {
        render_state->DeleteTexture(&current_wipe_texture);
        current_wipe_texture = 0;
    }
}

//----------------------------------------------------------------------------

static void RendererWipeFading(float how_far)
{
    RGBAColor unit_col = epi::MakeRGBA(255, 255, 255, (uint8_t)(1.0f - how_far * 255.0f));

    RendererVertex *glvert = BeginRenderUnit(GL_QUADS, 4, GL_MODULATE, current_wipe_texture,
                                             (GLuint)kTextureEnvironmentDisable, 0, 0, kBlendingAlpha);

    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{0.0f, 0.0f}};
    glvert++->position             = {{0, 0, 0}};
    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{0.0f, current_wipe_top}};
    glvert++->position             = {{0, (float)current_screen_height, 0}};
    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{current_wipe_right, current_wipe_top}};
    glvert++->position             = {{(float)current_screen_width, (float)current_screen_height, 0}};
    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{current_wipe_right, 0.0f}};
    glvert->position               = {{(float)current_screen_width, 0, 0}};

    EndRenderUnit(4);
}

static void RendererWipePixelfade(float how_far)
{
    RGBAColor unit_col = epi::MakeRGBA(255, 255, 255, (uint8_t)(1.0f - how_far * 255.0f));

    RendererVertex *glvert = BeginRenderUnit(GL_QUADS, 4, GL_MODULATE, current_wipe_texture,
                                             (GLuint)kTextureEnvironmentDisable, 0, 0, kBlendingGEqual);

    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{0.0f, 0.0f}};
    glvert++->position             = {{0, 0, 0}};
    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{0.0f, current_wipe_top}};
    glvert++->position             = {{0, (float)current_screen_height, 0}};
    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{current_wipe_right, current_wipe_top}};
    glvert++->position             = {{(float)current_screen_width, (float)current_screen_height, 0}};
    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{current_wipe_right, 0.0f}};
    glvert->position               = {{(float)current_screen_width, 0, 0}};

    EndRenderUnit(4);
}

static void RendererWipeMelt(void)
{
    RGBAColor       unit_col = kRGBAWhite;
    RendererVertex *glvert = BeginRenderUnit(GL_QUAD_STRIP, (kMeltSections + 1) * 2, GL_MODULATE, current_wipe_texture,
                                             (GLuint)kTextureEnvironmentDisable, 0, 0, kBlendingNone);

    for (int x = 0; x <= kMeltSections; x++, glvert++)
    {
        int yoffs = HMM_MAX(0, HMM_Lerp(old_melt_yoffs[x], fractional_tic, melt_yoffs[x]));

        float sx = (float)x * current_screen_width / kMeltSections;
        float sy = (float)(200 - yoffs) * current_screen_height / 200.0f;

        float tx = current_wipe_right * (float)x / kMeltSections;

        glvert->rgba                   = unit_col;
        glvert->texture_coordinates[0] = {{tx, current_wipe_top}};
        glvert++->position             = {{sx, sy, 0}};
        glvert->rgba                   = unit_col;
        glvert->texture_coordinates[0] = {{tx, 0.0f}};
        glvert->position               = {{sx, sy - current_screen_height, 0}};
    }

    EndRenderUnit((kMeltSections + 1) * 2);
}

static void RendererWipeSlide(float how_far, float dx, float dy)
{
    dx *= how_far;
    dy *= how_far;

    RGBAColor unit_col = kRGBAWhite;

    RendererVertex *glvert = BeginRenderUnit(GL_QUADS, 4, GL_MODULATE, current_wipe_texture,
                                             (GLuint)kTextureEnvironmentDisable, 0, 0, kBlendingNone);

    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{0.0f, 0.0f}};
    glvert++->position             = {{dx, dy, 0}};
    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{0.0f, current_wipe_top}};
    glvert++->position             = {{dx, dy + current_screen_height, 0}};
    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{current_wipe_right, current_wipe_top}};
    glvert++->position             = {{dx + current_screen_width, dy + current_screen_height, 0}};
    glvert->rgba                   = unit_col;
    glvert->texture_coordinates[0] = {{current_wipe_right, 0.0f}};
    glvert->position               = {{dx + current_screen_width, dy, 0}};

    EndRenderUnit(4);
}

static void RendererWipeDoors(float how_far)
{
    float dx = cos(how_far * HMM_PI / 2) * (current_screen_width / 2);
    float dy = sin(how_far * HMM_PI / 2) * (current_screen_height / 3);

    RGBAColor       unit_col = kRGBAWhite;
    RendererVertex *glvert   = nullptr;

    for (int column = 0; column < 5; column++)
    {
        float c = column / 10.0f;
        float e = column / 5.0f;

        for (int side = 0; side < 2; side++)
        {
            float t_x1 = (side == 0) ? c : (0.9f - c);
            float t_x2 = t_x1 + 0.1f;

            float v_x1 = (side == 0) ? (dx * e) : (current_screen_width - dx * (e + 0.2f));
            float v_x2 = v_x1 + dx * 0.2f;

            float v_y1 = (side == 0) ? (dy * e) : (dy * (e + 0.2f));
            float v_y2 = (side == 1) ? (dy * e) : (dy * (e + 0.2f));

            glvert = BeginRenderUnit(GL_QUAD_STRIP, 12, GL_MODULATE, current_wipe_texture,
                                     (GLuint)kTextureEnvironmentDisable, 0, 0, kBlendingNone);

            for (int row = 0; row <= 5; row++, glvert++)
            {
                float t_y = current_wipe_top * row / 5.0f;

                float j1 = (current_screen_height - v_y1 * 2.0f) / 5.0f;
                float j2 = (current_screen_height - v_y2 * 2.0f) / 5.0f;

                glvert->rgba                   = unit_col;
                glvert->texture_coordinates[0] = {{t_x2 * current_wipe_right, t_y}};
                glvert++->position             = {{v_x2, v_y2 + j2 * row, 0}};
                glvert->rgba                   = unit_col;
                glvert->texture_coordinates[0] = {{t_x1 * current_wipe_right, t_y}};
                glvert->position               = {{v_x1, v_y1 + j1 * row, 0}};
            }

            EndRenderUnit(12);
        }
    }
}

bool DoWipe(void)
{
    if (current_wipe_effect == kScreenWipeNone || current_wipe_texture == 0)
        return true;

    // determine how many tics since we started.  If this is the first
    // call to DoWipe() since InitWipe(), then the clock starts now.
    int now_time = GetTime();
    int tics     = 0;

    if (current_wipe_last_time >= 0)
        tics = HMM_MAX(0, now_time - current_wipe_last_time);

    current_wipe_last_time = now_time;

    // hack for large delays (like when loading a level)
    tics = HMM_MIN(6, tics);

    current_wipe_progress += tics;

    if (current_wipe_progress >= 40) // FIXME: have option for wipe time
        return true;

    float how_far = 0.0f;

    if (tics == 0)
        how_far = ((float)current_wipe_progress + fractional_tic) / 40.0f;
    else
        how_far = (float)current_wipe_progress / 40.0f;

    if (how_far < 0.01f)
    {
        how_far = 0.01f;
    }

    if (how_far > 0.99f)
    {
        how_far = 0.99f;
    }

    StartUnitBatch(false);

    switch (current_wipe_effect)
    {
    case kScreenWipeMelt:
        RendererWipeMelt();
        UpdateMelt(tics);
        break;

    case kScreenWipeTop:
        RendererWipeSlide(how_far, 0, +current_screen_height);
        break;

    case kScreenWipeBottom:
        RendererWipeSlide(how_far, 0, -current_screen_height);
        break;

    case kScreenWipeLeft:
        RendererWipeSlide(how_far, -current_screen_width, 0);
        break;

    case kScreenWipeRight:
        RendererWipeSlide(how_far, +current_screen_width, 0);
        break;

    case kScreenWipeDoors:
        RendererWipeDoors(how_far);
        break;

    case kScreenWipeSpooky: // difference is in alpha channel
    case kScreenWipePixelfade:
        RendererWipePixelfade(how_far);
        break;

    case kScreenWipeCrossfade:
    default:
        RendererWipeFading(how_far);
        break;
    }

    FinishUnitBatch();

    return false;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
