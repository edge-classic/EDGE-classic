//----------------------------------------------------------------------------
//  EDGE Movie Playback (MPEG)
//----------------------------------------------------------------------------
//
//  Copyright (c) 2018-2023 The EDGE Team
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

#include "path.h"
#include "i_defs.h"
#include "i_defs_gl.h"
#include "i_sound.h"
#include "i_system.h"
#include "r_gldefs.h"
#include "r_modes.h"
#include "r_wipe.h"
#include "s_sound.h"
#include "w_files.h"

#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"

extern bool dev_stereo;
extern int dev_freq;

bool playing_movie;
static bool need_canvas_update;
static GLuint canvas = 0;
static uint8_t *rgb_data = nullptr;
static plm_t *decoder = nullptr;
SDL_AudioStream *movie_audiostream = nullptr;
static int movie_sample_rate = 0;

static bool Movie_SetupAudioStream(int rate)
{
	movie_audiostream = SDL_NewAudioStream(AUDIO_F32, 2, rate, AUDIO_S16, dev_stereo ? 2 : 1, dev_freq);

	if (!movie_audiostream)
	{
		I_Warning("E_PlayMovie: Failed to setup audio stream\n", SDL_GetError());
		return false;
	}

	plm_set_audio_lead_time(decoder, (double)1024 / (double)rate);

	S_StopAllFX();

	return true;
}

void Movie_AudioCallback(plm_t *mpeg, plm_samples_t *samples, void *user)
{
	(void)user;
	SDL_AudioStreamPut(movie_audiostream, samples->interleaved, sizeof(float) * samples->count * 2);
}

void Movie_VideoCallback(plm_t *mpeg, plm_frame_t *frame, void *user) 
{
	(void)user;
	
	plm_frame_to_rgb(frame, rgb_data, frame->width * 3);

	glBindTexture(GL_TEXTURE_2D, canvas);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, frame->width, frame->height, 0,
		GL_RGB, GL_UNSIGNED_BYTE, rgb_data);
	need_canvas_update = true;
}

void E_PlayMovie(const std::string &name)
{
	playing_movie = false;
	need_canvas_update = false;

	int length = 0;
	uint8_t *bytes = W_OpenPackOrLumpInMemory(epi::PATH_GetBasename(name).string(), {".mpg", ".mpeg"}, &length);

	if (!bytes)
	{
		I_Warning("E_PlayMovie: Could not open %s!\n", name.c_str());
		return;
	}

	if (decoder)
	{
		plm_destroy(decoder);
		decoder = nullptr;
	}

	if (movie_audiostream)
	{
		SDL_FreeAudioStream(movie_audiostream);
		movie_audiostream = nullptr;
	}

	decoder = plm_create_with_memory(bytes, length, TRUE);

	if (!decoder)
	{
		I_Warning("E_PlayMovie: Could not open %s!\n", name.c_str());
		delete[] bytes;
		return;
	}

	if (!nosound && plm_get_num_audio_streams(decoder) > 0)
	{
		movie_sample_rate = plm_get_samplerate(decoder);
		if (!Movie_SetupAudioStream(movie_sample_rate))
		{
			plm_destroy(decoder);
			decoder = nullptr;
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

	int movie_width = plm_get_width(decoder);
	int movie_height = plm_get_height(decoder);
	float movie_ratio = (float)movie_width / movie_height;
	// Size frame to display with correct ratio
	// Should only need to be set once unless at some point
	// we allow menu access/console while a movie is playing
	int frame_height = SCREENHEIGHT;
	int frame_width = I_ROUND((float)SCREENHEIGHT * movie_ratio);
	if (frame_width > SCREENWIDTH)
	{
		frame_width = SCREENWIDTH;
		frame_height = I_ROUND((float)SCREENWIDTH / movie_ratio);
	}

	int num_pixels = movie_width * movie_height * 3;
	rgb_data = new uint8_t[num_pixels];
	memset(rgb_data, 0, num_pixels);
	plm_set_video_decode_callback(decoder, Movie_VideoCallback, nullptr);
	plm_set_audio_decode_callback(decoder, Movie_AudioCallback, nullptr);
	if (!nosound && movie_audiostream)
	{
		plm_set_audio_enabled(decoder, TRUE);
		plm_set_audio_stream(decoder, 0);
	}

	RGL_BlackoutWipeTex();

	glClearColor(0, 0, 0, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	I_FinishFrame();
	I_StartFrame();
	glClearColor(0, 0, 0, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	I_FinishFrame();

	RGL_SetupMatrices2D();

	double last_time = (double)SDL_GetTicks() / 1000.0;

	playing_movie = true;

	while (playing_movie)
	{
		if (plm_has_ended(decoder))
			break;

		double current_time = (double)SDL_GetTicks() / 1000.0;
		double elapsed_time = current_time - last_time;
		if (elapsed_time > 1.0 / 30.0) 
			elapsed_time = 1.0 / 30.0;
		last_time = current_time;

		plm_decode(decoder, elapsed_time);

		if (need_canvas_update)
		{
			I_StartFrame();

			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, canvas);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glDisable(GL_ALPHA_TEST);

			glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

			glBegin(GL_QUADS);

			glTexCoord2f(0.0f, 1.0f);
			glVertex2i(SCREENWIDTH/2 - frame_width/2, SCREENHEIGHT/2 - frame_height/2);

			glTexCoord2f(1.0f, 1.0f);
			glVertex2i(SCREENWIDTH/2 + frame_width/2, SCREENHEIGHT/2 - frame_height/2);

			glTexCoord2f(1.0f, 0.0f);
			glVertex2i(SCREENWIDTH/2 + frame_width/2, SCREENHEIGHT/2 + frame_height/2);

			glTexCoord2f(0.0f, 0.0f);
			glVertex2i(SCREENWIDTH/2 - frame_width/2, SCREENHEIGHT/2 + frame_height/2);

			glEnd();

			glDisable(GL_TEXTURE_2D);

			// Fade-in
			float fadein = plm_get_time(decoder);
			if (fadein <= 0.25f)
			{
				glColor4f(0, 0, 0, (0.25f-fadein)/0.25f);
				glEnable(GL_BLEND);

				glBegin(GL_QUADS);

				glVertex2i(SCREENWIDTH/2 - frame_width/2, SCREENHEIGHT/2 - frame_height/2);
				glVertex2i(SCREENWIDTH/2 + frame_width/2, SCREENHEIGHT/2 - frame_height/2);
				glVertex2i(SCREENWIDTH/2 + frame_width/2, SCREENHEIGHT/2 + frame_height/2);
				glVertex2i(SCREENWIDTH/2 - frame_width/2, SCREENHEIGHT/2 + frame_height/2);

				glEnd();

				glDisable(GL_BLEND);
			}

			I_FinishFrame();

			need_canvas_update = false;
		}

		/* check if press key/button */
		SDL_PumpEvents();
		SDL_Event sdl_ev;
		while (SDL_PollEvent(&sdl_ev))
		{
			switch (sdl_ev.type)
			{
			case SDL_KEYDOWN:
			case SDL_MOUSEBUTTONDOWN:
			case SDL_CONTROLLERBUTTONDOWN:
				playing_movie = false;
				break;

			default:
				break;
			}
		}
	}
	playing_movie = false;
	last_time = (double)SDL_GetTicks() / 1000.0;
	double fadeout = 0;
	while (fadeout <= 0.25f)
	{
		double current_time = (double)SDL_GetTicks() / 1000.0;
		fadeout = current_time - last_time;
		I_StartFrame();

		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, canvas);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glDisable(GL_ALPHA_TEST);

		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

		glBegin(GL_QUADS);

		glTexCoord2f(0.0f, 1.0f);
		glVertex2i(SCREENWIDTH/2 - frame_width/2, SCREENHEIGHT/2 - frame_height/2);

		glTexCoord2f(1.0f, 1.0f);
		glVertex2i(SCREENWIDTH/2 + frame_width/2, SCREENHEIGHT/2 - frame_height/2);

		glTexCoord2f(1.0f, 0.0f);
		glVertex2i(SCREENWIDTH/2 + frame_width/2, SCREENHEIGHT/2 + frame_height/2);

		glTexCoord2f(0.0f, 0.0f);
		glVertex2i(SCREENWIDTH/2 - frame_width/2, SCREENHEIGHT/2 + frame_height/2);

		glEnd();

		glDisable(GL_TEXTURE_2D);

		// Fade-out
		glColor4f(0, 0, 0, MAX(0.0f, 1.0f - ((0.25f-fadeout)/0.25f)));
		glEnable(GL_BLEND);

		glBegin(GL_QUADS);

		glVertex2i(SCREENWIDTH/2 - frame_width/2, SCREENHEIGHT/2 - frame_height/2);
		glVertex2i(SCREENWIDTH/2 + frame_width/2, SCREENHEIGHT/2 - frame_height/2);
		glVertex2i(SCREENWIDTH/2 + frame_width/2, SCREENHEIGHT/2 + frame_height/2);
		glVertex2i(SCREENWIDTH/2 - frame_width/2, SCREENHEIGHT/2 + frame_height/2);

		glEnd();

		glDisable(GL_BLEND);

		I_FinishFrame();
	}
	plm_destroy(decoder);
	decoder = nullptr;
	if (movie_audiostream)
	{
		SDL_FreeAudioStream(movie_audiostream);
		movie_audiostream = nullptr;
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
	I_FinishFrame();
	I_StartFrame();
	glClearColor(0, 0, 0, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	I_FinishFrame();
	return;
}