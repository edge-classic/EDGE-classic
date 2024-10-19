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

bool                    playing_movie = false;
static bool             need_canvas_update = false;
static bool             skip_bar_active = false;
static plm_t           *decoder            = nullptr;
static int              movie_sample_rate  = 0;
static float            skip_time;
static uint8_t         *movie_bytes = nullptr;
static GLuint movie_shader_program = 0;
static GLuint movie_vertex_shader = 0;
static GLuint movie_fragment_shader = 0;
static GLuint texture_y = 0;
static GLuint texture_cb = 0;
static GLuint texture_cr = 0;

static void MovieSetupAudio(int rate)
{
    plm_set_audio_lead_time(decoder, (double)1024 / (double)rate);

    PauseMusic();
    // Need to flush Queue to keep movie audio/video from desyncing initially (I
    // think) - Dasho
    SoundQueueStop();
    SoundQueueInitialize();
}

void MovieAudioCallback(plm_t *mpeg, plm_samples_t *samples, void *user)
{
    (void)mpeg;
    (void)user;
    SoundData *movie_buf = SoundQueueGetFreeBuffer(PLM_AUDIO_SAMPLES_PER_FRAME);
    if (movie_buf)
    {
        movie_buf->length_ = PLM_AUDIO_SAMPLES_PER_FRAME;
        memcpy(movie_buf->data_, samples->interleaved, PLM_AUDIO_SAMPLES_PER_FRAME * 2 * sizeof(int16_t));
        SoundQueueAddBuffer(movie_buf, movie_sample_rate);
    }
}

static constexpr const char *kMovieVertexShader =
	"attribute vec2 vertex;\n"
	"varying vec2 tex_coord;\n\n"
	"void main() {\n"
	"	tex_coord = vertex;\n"
	"	gl_Position = vec4((vertex * 2.0 - 1.0) * vec2(1, -1), 0.0, 1.0);\n"
    "}";

static constexpr const char *kMovieFragmentShader = 
	"uniform sampler2D texture_y;\n"
	"uniform sampler2D texture_cb;\n"
	"uniform sampler2D texture_cr;\n"
	"varying vec2 tex_coord;\n\n"
	"mat4 rec601 = mat4(\n"
	"	1.16438,  0.00000,  1.59603, -0.87079,\n"
	"	1.16438, -0.39176, -0.81297,  0.52959,\n"
	"	1.16438,  2.01723,  0.00000, -1.08139,\n"
	"	0, 0, 0, 1\n"
	");\n\n"
	"void main() {\n"
	"	float y = texture2D(texture_y, tex_coord).r;\n"
	"	float cb = texture2D(texture_cb, tex_coord).r;\n"
	"	float cr = texture2D(texture_cr, tex_coord).r;\n\n"
	"	gl_FragColor = vec4(y, cb, cr, 1.0) * rec601;\n"
	"}";

static GLuint CreateMovieTexture(GLuint index, const char *name) {
	GLuint texture;
    glGenTextures(1, &texture);
	
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	
	glUniform1i(glGetUniformLocation(movie_shader_program, name), index);
	return texture;
}

static GLuint CompileMovieShader(GLenum type, const char *source) {
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);
	
	GLint success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		int log_written;
		char log[256];
		glGetShaderInfoLog(shader, 256, &log_written, log);
		FatalError("PlayMovie: Error compiling shader: %s.\n", log);
	}
	return shader;
}

static void UpdateComponentTexture(GLuint unit, GLuint texture, plm_plane_t *plane) {
	glActiveTexture(unit);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(
		GL_TEXTURE_2D, 0, GL_LUMINANCE, plane->width, plane->height, 0,
		GL_LUMINANCE, GL_UNSIGNED_BYTE, plane->data
	);
}

static void MovieVideoCallback(plm_t *mpeg, plm_frame_t *frame, void *user)
{
    (void)mpeg;
    (void)user;
    UpdateComponentTexture(GL_TEXTURE0, texture_y, &frame->y);
    UpdateComponentTexture(GL_TEXTURE1, texture_cb, &frame->cb);
    UpdateComponentTexture(GL_TEXTURE2, texture_cr, &frame->cr);
    need_canvas_update = true;
}

void PlayMovie(const std::string &name)
{
    GLint main_shader_program = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &main_shader_program);

    // gen up shaders, etc, if not already done
    if (!movie_vertex_shader)
        movie_vertex_shader = CompileMovieShader(GL_VERTEX_SHADER, kMovieVertexShader);
    if (!movie_fragment_shader)
        movie_fragment_shader = CompileMovieShader(GL_FRAGMENT_SHADER, kMovieFragmentShader);
    if (!movie_shader_program)
    {
        movie_shader_program = glCreateProgram();
        glAttachShader(movie_shader_program, movie_vertex_shader);
        glAttachShader(movie_shader_program, movie_fragment_shader);
        glLinkProgram(movie_shader_program);
        glUseProgram(movie_shader_program);
    }
    if (!texture_y)
        texture_y = CreateMovieTexture(0, "texture_y");
    if (!texture_cb)
        texture_cb = CreateMovieTexture(1, "texture_cb");
    if (!texture_cr)
        texture_cr = CreateMovieTexture(2, "texture_cr");

    MovieDefinition *movie = moviedefs.Lookup(name.c_str());

    if (!movie)
    {
        LogWarning("PlayMovie: Movie definition %s not found!\n", name.c_str());
        return;
    }

    playing_movie      = false;
    need_canvas_update = false;
    skip_bar_active    = false;
    skip_time          = 0;

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
        MovieSetupAudio(movie_sample_rate);
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

    int vx1 = current_screen_width / 2 - frame_width / 2;
    int vx2 = current_screen_width / 2 + frame_width / 2;
    int vy1 = current_screen_height / 2 + frame_height / 2;
    int vy2 = current_screen_height / 2 - frame_height / 2;

    plm_set_video_decode_callback(decoder, MovieVideoCallback, nullptr);
    plm_set_audio_decode_callback(decoder, MovieAudioCallback, nullptr);
    if (!no_sound)
    {
        plm_set_audio_enabled(decoder, 1);
        plm_set_audio_stream(decoder, 0);
    }

    BlackoutWipeTexture();

    glClearColor(0, 0, 0, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    FinishFrame();
    StartFrame();
    glClearColor(0, 0, 0, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    FinishFrame();

    SetupMatrices2D();

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
        if (elapsed_time > 1.0 / 30.0)
            elapsed_time = 1.0 / 30.0;
        last_time = current_time;

        plm_decode(decoder, elapsed_time);

        if (need_canvas_update)
        {
            StartFrame();

            glUseProgram(movie_shader_program);
            glClear(GL_COLOR_BUFFER_BIT);
	        glRectf(tx1, ty1, tx2, ty2);
            glUseProgram(main_shader_program);

            // Fade-in
            float fadein = plm_get_time(decoder);
            if (fadein <= 0.25f)
            {
                glDisable(GL_TEXTURE);
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
            if (skip_time > 1)
                playing_movie = false;
        }
    }
    last_time      = (double)SDL_GetTicks() / 1000.0;
    double fadeout = 0;
    while (fadeout <= 0.25f)
    {
        double current_time = (double)SDL_GetTicks() / 1000.0;
        fadeout             = current_time - last_time;
        StartFrame();

        glUseProgram(movie_shader_program);
        glClear(GL_COLOR_BUFFER_BIT);
	    glRectf(tx1, ty1, tx2, ty2);
        glUseProgram(main_shader_program);
        glDisable(GL_TEXTURE);
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
    delete[] movie_bytes;
    movie_bytes = nullptr;
    glClearColor(0, 0, 0, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    FinishFrame();
    StartFrame();
    glClearColor(0, 0, 0, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    FinishFrame();
    ResumeMusic();
    GetRenderState()->SetDefaultStateFull();
    return;
}