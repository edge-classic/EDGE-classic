//----------------------------------------------------------------------------
//  EDGE Movie Playback (MPEG)
//----------------------------------------------------------------------------
//
//  Copyright (c) 2018-2024 The EDGE Team
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

#include "e_event.h"
#include "epi.h"
#include "hu_draw.h"
#include "i_defs_gl.h"
#include "i_sound.h"
#include "i_system.h"
#include "pl_mpeg.h"
#include "r_gldefs.h"
#include "r_modes.h"
#include "r_state.h"
#include "r_wipe.h"
#include "s_blit.h"
#include "s_music.h"
#include "s_sound.h"
#include "sokol_color.h"
#include "w_files.h"
#include "w_wad.h"

extern int sound_device_frequency;

bool                  playing_movie      = false;
static bool           need_canvas_update = false;
static bool           skip_bar_active;
static GLuint         canvas            = 0;
static uint8_t       *rgb_data          = nullptr;
static plm_t         *decoder           = nullptr;
static int            movie_sample_rate = 0;
static float          skip_time;
static uint8_t       *movie_bytes   = nullptr;
static double         fadein        = 0;
static double         fadeout       = 0;
static double         elapsed_time  = 0;
static int            vx1           = 0;
static int            vx2           = 0;
static int            vy1           = 0;
static int            vy2           = 0;
static float          tx1           = 0.0f;
static float          tx2           = 1.0f;
static float          ty1           = 0.0f;
static float          ty2           = 1.0f;
static double         last_time     = 0;
static plm_frame_t   *movie_frame   = nullptr;
static plm_samples_t *movie_samples = nullptr;

static bool MovieSetupAudioStream(int rate)
{
    plm_set_audio_lead_time(decoder, (double)1024 / (double)rate);
    return true;
}

void PlayMovie(const std::string &name)
{
    MovieDefinition *movie = moviedefs.Lookup(name.c_str());

    if (!movie)
    {
        LogWarning("PlayMovie: Movie definition %s not found!\n", name.c_str());
        return;
    }

    playing_movie   = false;
    skip_bar_active = false;
    skip_time       = 0;

    int length = 0;

    if (movie->type_ == kMovieDataLump)
        movie_bytes = LoadLumpIntoMemory(movie->info_.c_str(), &length);
    else
    {
        epi::File *mf = OpenFileFromPack(movie->info_.c_str());
        if (mf)
        {
            movie_bytes = mf->LoadIntoMemory();
            length      = mf->GetLength();
        }
        delete mf;
    }

    if (!movie_bytes)
    {
        LogWarning("PlayMovie: Could not open %s!\n", movie->info_.c_str());
        return;
    }

    if (decoder)
    {
        plm_destroy(decoder);
        decoder = nullptr;
    }

    decoder = plm_create_with_memory(movie_bytes, length, 0);

    if (!decoder)
    {
        LogWarning("PlayMovie: Could not open %s!\n", name.c_str());
        delete[] movie_bytes;
        movie_bytes = nullptr;
        return;
    }

    if (!no_sound && !(movie->special_ & kMovieSpecialMute) && plm_get_num_audio_streams(decoder) > 0)
    {
        movie_sample_rate = plm_get_samplerate(decoder);
        if (!MovieSetupAudioStream(movie_sample_rate))
        {
            plm_destroy(decoder);
            delete[] movie_bytes;
            movie_bytes = nullptr;
            decoder     = nullptr;
            return;
        }
    }

    if (canvas)
        glDeleteTextures(1, &canvas);

    glGenTextures(1, &canvas);

    if (rgb_data)
    {
        delete[] rgb_data;
        rgb_data = nullptr;
    }

    int   movie_width  = plm_get_width(decoder);
    int   movie_height = plm_get_height(decoder);
    float movie_ratio  = (float)movie_width / movie_height;
    // Size frame using DDFMOVIE scaling selection
    // Should only need to be set once unless at some point
    // we allow menu access/console while a movie is playing
    int frame_height = 0;
    int frame_width  = 0;
    tx1              = 0.0f;
    tx2              = 1.0f;
    ty1              = 0.0f;
    ty2              = 1.0f;
    if (movie->scaling_ == kMovieScalingAutofit)
    {
        // If movie and display ratios match (ish), stretch it
        if (fabs((float)current_screen_width / current_screen_height / movie_ratio - 1.0f) <= 0.10f)
        {
            frame_height = current_screen_height;
            frame_width  = current_screen_width;
        }
        else // Zoom
        {
            frame_height = current_screen_height;
            frame_width  = RoundToInteger((float)current_screen_height * movie_ratio);
        }
    }
    else if (movie->scaling_ == kMovieScalingNoScale)
    {
        frame_height = movie_height;
        frame_width  = movie_width;
    }
    else if (movie->scaling_ == kMovieScalingZoom)
    {
        frame_height = current_screen_height;
        frame_width  = RoundToInteger((float)current_screen_height * movie_ratio);
    }
    else // Stretch, aspect ratio gets BTFO potentially
    {
        frame_height = current_screen_height;
        frame_width  = current_screen_width;
    }

    vx1 = current_screen_width / 2 - frame_width / 2;
    vx2 = current_screen_width / 2 + frame_width / 2;
    vy1 = current_screen_height / 2 + frame_height / 2;
    vy2 = current_screen_height / 2 - frame_height / 2;

    int num_pixels = movie_width * movie_height * 3;
    rgb_data       = new uint8_t[num_pixels];
    memset(rgb_data, 0, num_pixels);
    if (!no_sound)
    {
        plm_set_audio_enabled(decoder, 1);
        plm_set_audio_stream(decoder, 0);
    }

    BlackoutWipeTexture();

    last_time = (double)SDL_GetTicks() / 1000.0;
    fadein    = 0;
    fadeout   = 0;

    playing_movie = true;
}

static void EndMovie()
{
    plm_destroy(decoder);
    movie_frame = nullptr;
    movie_samples = nullptr;
    decoder = nullptr;
    delete[] movie_bytes;
    movie_bytes = nullptr;
    if (rgb_data)
    {
        delete[] rgb_data;
        rgb_data = nullptr;
    }
    if (canvas)
    {
        glDeleteTextures(1, &canvas);
        canvas = 0;
    }
}

void MovieDrawer()
{
    if (!playing_movie)
        return;
    if (!plm_has_ended(decoder) && movie_frame)
    {
        if (need_canvas_update)
        {
            plm_frame_to_rgb(movie_frame, rgb_data, movie_frame->width * 3);
            glBindTexture(GL_TEXTURE_2D, canvas);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, movie_frame->width, movie_frame->height, 0, GL_RGB, GL_UNSIGNED_BYTE,
                         rgb_data);
            need_canvas_update = false;
        }

        SetupMatrices2D();

        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, canvas);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glDisable(GL_ALPHA_TEST);

        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

        glBegin(GL_QUADS);

        glTexCoord2f(tx1, ty2);
        glVertex2i(vx1, vy2);

        glTexCoord2f(tx2, ty2);
        glVertex2i(vx2, vy2);

        glTexCoord2f(tx2, ty1);
        glVertex2i(vx2, vy1);

        glTexCoord2f(tx1, ty1);
        glVertex2i(vx1, vy1);

        glEnd();

        glDisable(GL_TEXTURE_2D);

        // Fade-in
        fadein = plm_get_time(decoder);
        if (fadein <= 0.25f)
        {
            glColor4f(0, 0, 0, (0.25f - fadein) / 0.25f);
            glEnable(GL_BLEND);

            glBegin(GL_QUADS);

            glVertex2i(vx1, vy2);
            glVertex2i(vx2, vy2);
            glVertex2i(vx2, vy1);
            glVertex2i(vx1, vy1);

            glEnd();

            glDisable(GL_BLEND);
        }

        if (skip_bar_active)
        {
            // Draw black box at bottom of screen
            HUDSolidBox(hud_x_left, 196, hud_x_right, 200, SG_BLACK_RGBA32);

            // Draw progress
            HUDSolidBox(hud_x_left, 197, hud_x_right * (skip_time / 0.9f), 199, SG_WHITE_RGBA32);
        }

        GetRenderState()->SetDefaultStateFull();
    }
    else
    {
        SetupMatrices2D();

        double current_time = (double)SDL_GetTicks() / 1000.0;
        fadeout             = current_time - last_time;

        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, canvas);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glDisable(GL_ALPHA_TEST);

        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

        glBegin(GL_QUADS);

        glTexCoord2f(tx1, ty2);
        glVertex2i(vx1, vy2);

        glTexCoord2f(tx2, ty2);
        glVertex2i(vx2, vy2);

        glTexCoord2f(tx2, ty1);
        glVertex2i(vx2, vy1);

        glTexCoord2f(tx1, ty1);
        glVertex2i(vx1, vy1);

        glEnd();

        glDisable(GL_TEXTURE_2D);

        // Fade-out
        glColor4f(0, 0, 0, HMM_MAX(0.0f, 1.0f - ((0.25f - fadeout) / 0.25f)));
        glEnable(GL_BLEND);

        glBegin(GL_QUADS);

        glVertex2i(vx1, vy2);
        glVertex2i(vx2, vy2);
        glVertex2i(vx2, vy1);
        glVertex2i(vx1, vy1);

        glEnd();

        glDisable(GL_BLEND);

        GetRenderState()->SetDefaultStateFull();
    }
}

bool MovieResponder(InputEvent *ev)
{
    if (playing_movie)
    {
        switch (ev->type)
        {
        case kInputEventKeyDown:
            skip_bar_active = true;
            break;

        case kInputEventKeyUp:
            skip_bar_active = false;
            skip_time       = 0;
            break;

        default:
            break;
        }
        return true; // eat it no matter what
    }
    else
        return false;
}

void MovieTicker()
{
    if (!playing_movie)
    {
        if (decoder)
            EndMovie();
        return;
    }
    if (fadeout > 0.25f)
    {
        EndMovie();
        playing_movie = false;
        return;
    }
    if (!plm_has_ended(decoder))
    {
        double current_time = (double)SDL_GetTicks() / 1000.0;
        elapsed_time        = current_time - last_time;
        if (elapsed_time > 1.0 / 30.0)
            elapsed_time = 1.0 / 30.0;
        last_time = current_time;

        movie_frame = plm_decode_video(decoder);
        if (movie_frame)
            need_canvas_update = true;
        movie_samples        = plm_decode_audio(decoder);
        SoundData *movie_buf = SoundQueueGetFreeBuffer(PLM_AUDIO_SAMPLES_PER_FRAME);
        if (movie_buf)
        {
            movie_buf->length_ = PLM_AUDIO_SAMPLES_PER_FRAME;
            if (movie_samples)
            {
                memcpy(movie_buf->data_, movie_samples->interleaved, PLM_AUDIO_SAMPLES_PER_FRAME * 2 * sizeof(int16_t));
                SoundQueueAddBuffer(movie_buf, movie_sample_rate);
            }
            else
                SoundQueueReturnBuffer(movie_buf);
        }

        if (skip_bar_active)
        {
            skip_time += elapsed_time;
            if (skip_time > 1)
                playing_movie = false;
        }
    }
}