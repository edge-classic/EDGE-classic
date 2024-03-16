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
#include "r_gldefs.h"
#include "r_image.h"
#include "r_modes.h"
#include "r_texgl.h"

extern ConsoleVariable double_framerate;

// we're limited to one wipe at a time...
static ScreenWipe current_wipe_effect = kScreenWipeNone;

static int current_wipe_progress;
static int current_wipe_last_time;

static GLuint current_wipe_texture = 0;
static float  current_wipe_right;
static float  current_wipe_top;

static constexpr uint8_t kMeltSections = 128;
static int               melt_yoffs[kMeltSections + 1];

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

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (int y = 0; y < current_screen_height; y++)
    {
        uint8_t *dest = img.PixelAt(0, y);

        glReadPixels(0, y, current_screen_width, 1, GL_RGBA, GL_UNSIGNED_BYTE, dest);

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

    current_wipe_texture = RendererUploadTexture(&img);
}

void RendererBlackoutWipeTexture(void)
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

    current_wipe_texture = RendererUploadTexture(&img);
}

static void RendererInitializeMelt(void)
{
    int x, r;

    melt_yoffs[0] = -(RandomByte() % 16);

    for (x = 1; x <= kMeltSections; x++)
    {
        r = (RandomByte() % 3) - 1;

        melt_yoffs[x] = melt_yoffs[x - 1] + r;
        melt_yoffs[x] = HMM_MAX(-15, HMM_MIN(0, melt_yoffs[x]));
    }
}

static void RendererUpdateMelt(int tics)
{
    int x, r;

    for (; tics > 0; tics--)
    {
        for (x = 0; x <= kMeltSections; x++)
        {
            r = melt_yoffs[x];

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

void RendererInitializeWipe(ScreenWipe effect)
{
    current_wipe_effect = effect;

    current_wipe_progress  = 0;
    current_wipe_last_time = -1;

    if (current_wipe_effect == kScreenWipeNone)
        return;

    CaptureScreenAsTexture(effect == kScreenWipePixelfade, effect == kScreenWipeSpooky);

    if (current_wipe_effect == kScreenWipeMelt)
        RendererInitializeMelt();
}

void RendererStopWipe(void)
{
    current_wipe_effect = kScreenWipeNone;

    if (current_wipe_texture != 0)
    {
        glDeleteTextures(1, &current_wipe_texture);
        current_wipe_texture = 0;
    }
}

//----------------------------------------------------------------------------

static void RendererWipeFading(float how_far)
{
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);

    glBindTexture(GL_TEXTURE_2D, current_wipe_texture);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f - how_far);

    glBegin(GL_QUADS);

    glTexCoord2f(0.0f, 0.0f);
    glVertex2i(0, 0);
    glTexCoord2f(0.0f, current_wipe_top);
    glVertex2i(0, current_screen_height);
    glTexCoord2f(current_wipe_right, current_wipe_top);
    glVertex2i(current_screen_width, current_screen_height);
    glTexCoord2f(current_wipe_right, 0.0f);
    glVertex2i(current_screen_width, 0);

    glEnd();

    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}

static void RendererWipePixelfade(float how_far)
{
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glEnable(GL_ALPHA_TEST);

    glAlphaFunc(GL_GEQUAL, how_far);

    glBindTexture(GL_TEXTURE_2D, current_wipe_texture);
    glColor3f(1.0f, 1.0f, 1.0f);

    glBegin(GL_QUADS);

    glTexCoord2f(0.0f, 0.0f);
    glVertex2i(0, 0);
    glTexCoord2f(0.0f, current_wipe_top);
    glVertex2i(0, current_screen_height);
    glTexCoord2f(current_wipe_right, current_wipe_top);
    glVertex2i(current_screen_width, current_screen_height);
    glTexCoord2f(current_wipe_right, 0.0f);
    glVertex2i(current_screen_width, 0);

    glEnd();

    glDisable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);

    glAlphaFunc(GL_GREATER, 0);
}

static void RendererWipeMelt(void)
{
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);

    glBindTexture(GL_TEXTURE_2D, current_wipe_texture);
    glColor3f(1.0f, 1.0f, 1.0f);

    glBegin(GL_QUAD_STRIP);

    for (int x = 0; x <= kMeltSections; x++)
    {
        int yoffs = HMM_MAX(0, melt_yoffs[x]);

        float sx = (float)x * current_screen_width / kMeltSections;
        float sy = (float)(200 - yoffs) * current_screen_height / 200.0f;

        float tx = current_wipe_right * (float)x / kMeltSections;

        glTexCoord2f(tx, current_wipe_top);
        glVertex2f(sx, sy);

        glTexCoord2f(tx, 0.0f);
        glVertex2f(sx, sy - current_screen_height);
    }

    glEnd();

    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}

static void RendererWipeSlide(float how_far, float dx, float dy)
{
    dx *= how_far;
    dy *= how_far;

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);

    glBindTexture(GL_TEXTURE_2D, current_wipe_texture);
    glColor3f(1.0f, 1.0f, 1.0f);

    glBegin(GL_QUADS);

    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(dx, dy);
    glTexCoord2f(0.0f, current_wipe_top);
    glVertex2f(dx, dy + current_screen_height);
    glTexCoord2f(current_wipe_right, current_wipe_top);
    glVertex2f(dx + current_screen_width, dy + current_screen_height);
    glTexCoord2f(current_wipe_right, 0.0f);
    glVertex2f(dx + current_screen_width, dy);

    glEnd();

    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}

static void RendererWipeDoors(float how_far)
{
    float dx = cos(how_far * HMM_PI / 2) * (current_screen_width / 2);
    float dy = sin(how_far * HMM_PI / 2) * (current_screen_height / 3);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);

    glBindTexture(GL_TEXTURE_2D, current_wipe_texture);
    glColor3f(1.0f, 1.0f, 1.0f);

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

            glBegin(GL_QUAD_STRIP);

            for (int row = 0; row <= 5; row++)
            {
                float t_y = current_wipe_top * row / 5.0f;

                float j1 = (current_screen_height - v_y1 * 2.0f) / 5.0f;
                float j2 = (current_screen_height - v_y2 * 2.0f) / 5.0f;

                glTexCoord2f(t_x2 * current_wipe_right, t_y);
                glVertex2f(v_x2, v_y2 + j2 * row);

                glTexCoord2f(t_x1 * current_wipe_right, t_y);
                glVertex2f(v_x1, v_y1 + j1 * row);
            }

            glEnd();
        }
    }

    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}

bool RendererDoWipe(void)
{
    //
    // NOTE: we assume 2D project matrix is already setup.
    //

    if (current_wipe_effect == kScreenWipeNone || current_wipe_texture == 0)
        return true;

    // determine how many tics since we started.  If this is the first
    // call to DoWipe() since InitWipe(), then the clock starts now.
    int now_time = GetTime() / (double_framerate.d_ ? 2 : 1);
    int tics     = 0;

    if (current_wipe_last_time >= 0)
        tics = HMM_MAX(0, now_time - current_wipe_last_time);

    current_wipe_last_time = now_time;

    // hack for large delays (like when loading a level)
    tics = HMM_MIN(6, tics);

    current_wipe_progress += tics;

    if (current_wipe_progress > 40) // FIXME: have option for wipe time
        return true;

    float how_far = (float)current_wipe_progress / 40.0f;

    switch (current_wipe_effect)
    {
    case kScreenWipeMelt:
        RendererWipeMelt();
        RendererUpdateMelt(tics);
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

    return false;
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
