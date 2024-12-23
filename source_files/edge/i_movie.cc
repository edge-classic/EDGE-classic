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
#include "r_units.h"
#include "r_wipe.h"
#include "s_blit.h"
#include "s_music.h"
#include "s_sound.h"
#include "w_files.h"
#include "w_wad.h"

extern int sound_device_frequency;

bool                  playing_movie      = false;
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
static float          vx1           = 0.0f;
static float          vx2           = 0.0f;
static float          vy1           = 0.0f;
static float          vy2           = 0.0f;
static float          tx1           = 0.0f;
static float          tx2           = 1.0f;
static float          ty1           = 0.0f;
static float          ty2           = 1.0f;
static double         last_time     = 0;

// Disabled until Dasho can figure out why the frame size changes between creating the texture and updating it
// Which is probably also an issue under GL1
#ifndef EDGE_SOKOL
static bool MovieSetupAudioStream(int rate)
{
    plm_set_audio_lead_time(decoder, (double)1024 / (double)rate);
    PauseMusic();
    SoundQueueStop();
    SoundQueueInitialize();
    return true;
}
#endif

void MovieAudioCallback(plm_t *mpeg, plm_samples_t *samples, void *user)
{
    EPI_UNUSED(mpeg);
    EPI_UNUSED(user);
    if (samples)
    {
        SoundData *movie_buf = SoundQueueGetFreeBuffer(PLM_AUDIO_SAMPLES_PER_FRAME);
        if (movie_buf)
        {
            movie_buf->length_ = PLM_AUDIO_SAMPLES_PER_FRAME;
            memcpy(movie_buf->data_, samples->interleaved, PLM_AUDIO_SAMPLES_PER_FRAME * 2 * sizeof(int16_t));
            SoundQueueAddBuffer(movie_buf, movie_sample_rate);
        }
    }
}

void MovieVideoCallback(plm_t *mpeg, plm_frame_t *frame, void *user)
{
    EPI_UNUSED(mpeg);
    EPI_UNUSED(user);

    plm_frame_to_rgba(frame, rgb_data, frame->width * 4);

    render_state->BindTexture(canvas);
    render_state->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frame->width, frame->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgb_data);
}

void PlayMovie(const std::string &name)
{
#ifdef EDGE_SOKOL
    // Disabled until Dasho can figure out why the frame size changes between creating the texture and updating it
    // Which is probably also an issue under GL1
    EPI_UNUSED(name);
    playing_movie   = false;
    skip_bar_active = false;
    return;    
#else    
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
        render_state->DeleteTexture(&canvas);

    render_state->GenTextures(1, &canvas);
    render_state->BindTexture(canvas);
    render_state->TextureMagFilter(GL_LINEAR);
    render_state->TextureMinFilter(GL_LINEAR);

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

#ifdef EDGE_SOKOL
    // On sokol, this sets up the texture dimenions, for dynamic texture
    render_state->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frame_width, frame_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr, kRenderUsageDynamic);
    render_state->FinishTextures(1, &canvas);
#endif    

    vx1 = current_screen_width / 2 - frame_width / 2;
    vx2 = current_screen_width / 2 + frame_width / 2;
    vy1 = current_screen_height / 2 + frame_height / 2;
    vy2 = current_screen_height / 2 - frame_height / 2;

    int num_pixels = movie_width * movie_height * 4;
    rgb_data       = new uint8_t[num_pixels];
    EPI_CLEAR_MEMORY(rgb_data, uint8_t, num_pixels);
    plm_set_video_decode_callback(decoder, MovieVideoCallback, nullptr);
    plm_set_audio_decode_callback(decoder, MovieAudioCallback, nullptr);
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
#endif
}

static void EndMovie()
{
    plm_destroy(decoder);
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
        render_state->DeleteTexture(&canvas);
        canvas = 0;
    }
    ResumeMusic();
}

void MovieDrawer()
{
    if (!playing_movie)
        return;

    if (!plm_has_ended(decoder))
    {
        StartUnitBatch (false);

        RGBAColor unit_col = kRGBAWhite;

        RendererVertex *glvert = BeginRenderUnit(GL_QUADS, 4, GL_MODULATE, canvas, (GLuint)kTextureEnvironmentDisable, 0, 0, kBlendingNone);

        glvert->rgba = unit_col;
        glvert->texture_coordinates[0] = {{tx1, ty2}};
        glvert++->position = {{vx1, vy2, 0}};
        glvert->rgba = unit_col;
        glvert->texture_coordinates[0] = {{tx2, ty2}};
        glvert++->position = {{vx2, vy2, 0}};
        glvert->rgba = unit_col;
        glvert->texture_coordinates[0] = {{tx2, ty1}};
        glvert++->position = {{vx2, vy1, 0}};
        glvert->rgba = unit_col;
        glvert->texture_coordinates[0] = {{tx1, ty1}};
        glvert->position = {{vx1, vy1, 0}};

        EndRenderUnit(4);

        // Fade-in
        fadein = plm_get_time(decoder);
        if (fadein <= 0.25f)
        {
            unit_col = epi::MakeRGBAFloat(0.0f, 0.0f, 0.0f, ((0.25f - (float)fadein) / 0.25f));
            
            glvert = BeginRenderUnit(GL_QUADS, 4, GL_MODULATE, 0, (GLuint)kTextureEnvironmentDisable, 0, 0, kBlendingAlpha);

            glvert->rgba = unit_col;
            glvert++->position = {{vx1, vy2, 0}};
            glvert->rgba = unit_col;
            glvert++->position = {{vx2, vy2, 0}};
            glvert->rgba = unit_col;
            glvert++->position = {{vx2, vy1, 0}};
            glvert->rgba = unit_col;
            glvert->position = {{vx1, vy1, 0}};

            EndRenderUnit(4);
        }

        FinishUnitBatch();

        if (skip_bar_active)
        {
            // Draw black box at bottom of screen
            HUDSolidBox(hud_x_left, 196, hud_x_right, 200, kRGBABlack);

            // Draw progress
            HUDSolidBox(hud_x_left, 197, hud_x_right * (skip_time / 0.9f), 199, kRGBAWhite);
        }
    }
    else
    {
        double current_time = (double)SDL_GetTicks() / 1000.0;
        fadeout             = current_time - last_time;

        StartUnitBatch(false);

        RGBAColor unit_col = kRGBAWhite;

        RendererVertex *glvert = BeginRenderUnit(GL_QUADS, 4, GL_MODULATE, canvas, (GLuint)kTextureEnvironmentDisable, 0, 0, kBlendingNone);

        glvert->rgba = unit_col;
        glvert->texture_coordinates[0] = {{tx1, ty2}};
        glvert++->position = {{vx1, vy2, 0}};
        glvert->rgba = unit_col;
        glvert->texture_coordinates[0] = {{tx2, ty2}};
        glvert++->position = {{vx2, vy2, 0}};
        glvert->rgba = unit_col;
        glvert->texture_coordinates[0] = {{tx2, ty1}};
        glvert++->position = {{vx2, vy1, 0}};
        glvert->rgba = unit_col;
        glvert->texture_coordinates[0] = {{tx1, ty1}};
        glvert->position = {{vx1, vy1, 0}};

        EndRenderUnit(4);

        // Fade-out
        unit_col = epi::MakeRGBAFloat(0.0f, 0.0f, 0.0f, (HMM_MAX(0.0f, 1.0f - ((0.25f - (float)fadeout) / 0.25f))));
            
        glvert = BeginRenderUnit(GL_QUADS, 4, GL_MODULATE, 0, (GLuint)kTextureEnvironmentDisable, 0, 0, kBlendingAlpha);

        glvert->rgba = unit_col;
        glvert++->position = {{vx1, vy2, 0}};
        glvert->rgba = unit_col;
        glvert++->position = {{vx2, vy2, 0}};
        glvert->rgba = unit_col;
        glvert++->position = {{vx2, vy1, 0}};
        glvert->rgba = unit_col;
        glvert->position = {{vx1, vy1, 0}};

        EndRenderUnit(4);

        FinishUnitBatch();
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

        plm_decode(decoder, elapsed_time);

        if (skip_bar_active)
        {
            skip_time += elapsed_time;
            if (skip_time > 1)
                playing_movie = false;
        }
    }
}