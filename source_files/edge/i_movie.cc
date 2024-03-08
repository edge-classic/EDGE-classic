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
//  Adapted from the EDGE 2.x RoQ/FFMPEG implementation
//----------------------------------------------------------------------------

#include "hu_draw.h"
#include "i_defs_gl.h"
#include "i_sound.h"
#include "i_system.h"
#include "r_gldefs.h"
#include "r_modes.h"
#include "r_wipe.h"
#include "s_blit.h"
#include "s_music.h"
#include "s_sound.h"
#include "w_files.h"
#include "w_wad.h"
#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"
#include "sokol_color.h"

extern bool sound_device_stereo;
extern int  sound_device_frequency;

bool                    playing_movie;
static bool             need_canvas_update;
static bool             skip_bar_active;
static GLuint           canvas            = 0;
static uint8_t         *rgb_data          = nullptr;
static plm_t           *decoder           = nullptr;
static SDL_AudioStream *movie_audio_stream = nullptr;
static int              movie_sample_rate = 0;
static float            skip_time;

static bool MovieSetupAudioStream(int rate)
{
    movie_audio_stream = SDL_NewAudioStream(AUDIO_F32, 2, rate, AUDIO_S16,
                                           sound_device_stereo ? 2 : 1, sound_device_frequency);

    if (!movie_audio_stream)
    {
        LogWarning("PlayMovie: Failed to setup audio stream: %s\n",
                  SDL_GetError());
        return false;
    }

    plm_set_audio_lead_time(decoder, (double)1024 / (double)rate);

    PauseMusic();
    // Need to flush Queue to keep movie audio/video from desyncing initially (I
    // think) - Dasho
    SoundQueueStop();
    SoundQueueInitialize();

    return true;
}

void MovieAudioCallback(plm_t *mpeg, plm_samples_t *samples, void *user)
{
    (void)mpeg;
    (void)user;
    SDL_AudioStreamPut(movie_audio_stream, samples->interleaved,
                       sizeof(float) * samples->count * 2);
    int avail = SDL_AudioStreamAvailable(movie_audio_stream);
    if (avail)
    {
        SoundData *movie_buf = SoundQueueGetFreeBuffer(
            avail / 2, sound_device_stereo ? kMixInterleaved : kMixMono);
        if (movie_buf)
        {
            movie_buf->length_ = SDL_AudioStreamGet(movie_audio_stream,
                                                   movie_buf->data_left_, avail) /
                                (sound_device_stereo ? 4 : 2);
            if (movie_buf->length_ > 0)
                SoundQueueAddBuffer(movie_buf, sound_device_frequency);
            else
                SoundQueueReturnBuffer(movie_buf);
        }
    }
}

void MovieVideoCallback(plm_t *mpeg, plm_frame_t *frame, void *user)
{
    (void)mpeg;
    (void)user;

    plm_frame_to_rgb(frame, rgb_data, frame->width * 3);

    glBindTexture(GL_TEXTURE_2D, canvas);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, frame->width, frame->height, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, rgb_data);
    need_canvas_update = true;
}

void PlayMovie(const std::string &name)
{
    MovieDefinition *movie = moviedefs.Lookup(name.c_str());

    if (!movie)
    {
        LogWarning("PlayMovie: Movie definition %s not found!\n",
                  name.c_str());
        return;
    }

    playing_movie      = false;
    need_canvas_update = false;
    skip_bar_active    = false;
    skip_time          = 0;

    int      length = 0;
    uint8_t *bytes  = nullptr;

    if (movie->type_ == kMovieDataLump)
        bytes = LoadLumpIntoMemory(movie->info_.c_str(), &length);
    else
    {
        epi::File *mf = OpenFileFromPack(movie->info_.c_str());
        if (mf)
        {
            bytes  = mf->LoadIntoMemory();
            length = mf->GetLength();
        }
        delete mf;
    }

    if (!bytes)
    {
        LogWarning("PlayMovie: Could not open %s!\n", movie->info_.c_str());
        return;
    }

    if (decoder)
    {
        plm_destroy(decoder);
        decoder = nullptr;
    }

    if (movie_audio_stream)
    {
        SDL_FreeAudioStream(movie_audio_stream);
        movie_audio_stream = nullptr;
    }

    decoder = plm_create_with_memory(bytes, length, TRUE);

    if (!decoder)
    {
        LogWarning("PlayMovie: Could not open %s!\n", name.c_str());
        delete[] bytes;
        return;
    }

    if (!no_sound && !(movie->special_ & kMovieSpecialMute) &&
        plm_get_num_audio_streams(decoder) > 0)
    {
        movie_sample_rate = plm_get_samplerate(decoder);
        if (!MovieSetupAudioStream(movie_sample_rate))
        {
            plm_destroy(decoder);
            decoder = nullptr;
            return;
        }
    }

    if (canvas) glDeleteTextures(1, &canvas);

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
    int   frame_height = 0;
    int   frame_width  = 0;
    float tx1          = 0.0f;
    float tx2          = 1.0f;
    float ty1          = 0.0f;
    float ty2          = 1.0f;
    if (movie->scaling_ == kMovieScalingAutofit)
    {
        // If movie and display ratios match (ish), stretch it
        if (fabs((float)current_screen_width / current_screen_height / movie_ratio - 1.0f) <=
            0.10f)
        {
            frame_height = current_screen_height;
            frame_width  = current_screen_width;
        }
        else  // Zoom
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
    else  // Stretch, aspect ratio gets BTFO potentially
    {
        frame_height = current_screen_height;
        frame_width  = current_screen_width;
    }

    int vx1 = current_screen_width / 2 - frame_width / 2;
    int vx2 = current_screen_width / 2 + frame_width / 2;
    int vy1 = current_screen_height / 2 + frame_height / 2;
    int vy2 = current_screen_height / 2 - frame_height / 2;

    int num_pixels = movie_width * movie_height * 3;
    rgb_data       = new uint8_t[num_pixels];
    memset(rgb_data, 0, num_pixels);
    plm_set_video_decode_callback(decoder, MovieVideoCallback, nullptr);
    plm_set_audio_decode_callback(decoder, MovieAudioCallback, nullptr);
    if (!no_sound && movie_audio_stream)
    {
        plm_set_audio_enabled(decoder, TRUE);
        plm_set_audio_stream(decoder, 0);
    }

    RendererBlackoutWipeTexture();

    glClearColor(0, 0, 0, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    FinishFrame();
    StartFrame();
    glClearColor(0, 0, 0, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    FinishFrame();

    RendererSetupMatrices2D();

    double last_time = (double)SDL_GetTicks() / 1000.0;

    playing_movie = true;

    while (playing_movie)
    {
        if (plm_has_ended(decoder))
        {
            playing_movie = false;
            break;
        }

        double current_time = (double)SDL_GetTicks() / 1000.0;
        double elapsed_time = current_time - last_time;
        if (elapsed_time > 1.0 / 30.0) elapsed_time = 1.0 / 30.0;
        last_time = current_time;

        plm_decode(decoder, elapsed_time);

        if (need_canvas_update)
        {
            StartFrame();

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
            float fadein = plm_get_time(decoder);
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
                HudSolidBox(hud_x_left, 196, hud_x_right, 200, SG_BLACK_RGBA32);

                // Draw progress
                HudSolidBox(hud_x_left, 197, hud_x_right * (skip_time / 0.9f),
                            199, SG_WHITE_RGBA32);
            }

            FinishFrame();

            need_canvas_update = false;
        }

        /* check if press key/button */
        SDL_Event sdl_ev;
        while (SDL_PollEvent(&sdl_ev))
        {
            switch (sdl_ev.type)
            {
                case SDL_KEYDOWN:
                case SDL_MOUSEBUTTONDOWN:
                case SDL_CONTROLLERBUTTONDOWN:
                    skip_bar_active = true;
                    break;

                case SDL_KEYUP:
                case SDL_MOUSEBUTTONUP:
                case SDL_CONTROLLERBUTTONUP:
                    skip_bar_active = false;
                    skip_time       = 0;
                    break;

                default:
                    break;
            }
        }
        if (skip_bar_active)
        {
            skip_time += elapsed_time;
            if (skip_time > 1) playing_movie = false;
        }
    }
    last_time      = (double)SDL_GetTicks() / 1000.0;
    double fadeout = 0;
    while (fadeout <= 0.25f)
    {
        double current_time = (double)SDL_GetTicks() / 1000.0;
        fadeout             = current_time - last_time;
        StartFrame();

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

        FinishFrame();
    }
    plm_destroy(decoder);
    decoder = nullptr;
    if (movie_audio_stream)
    {
        SDL_FreeAudioStream(movie_audio_stream);
        movie_audio_stream = nullptr;
    }
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
    glClearColor(0, 0, 0, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    FinishFrame();
    StartFrame();
    glClearColor(0, 0, 0, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    FinishFrame();
    ResumeMusic();
    return;
}