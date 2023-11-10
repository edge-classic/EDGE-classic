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

#include "i_defs.h"
#include "i_defs_gl.h"
#include "i_sound.h"
#include "i_system.h"
#include "r_gldefs.h"
#include "r_modes.h"
#include "s_sound.h"
#include "w_files.h"

#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"

static bool playing_movie;
static bool need_canvas_update;
static GLuint canvas = 0;
static uint8_t *rgb_data = nullptr;
static plm_t *decoder = nullptr;
static SDL_AudioDeviceID sounddev = 0;

// Need to close and reopen the sound device to reflect
// the movie's sample rate and the fact it uses F32 format
static bool Movie_HijackSound(int rate)
{
	S_Shutdown();
	I_ShutdownSound();
	SDL_AudioSpec audio_spec;
	SDL_zero(audio_spec);
	audio_spec.freq = rate;
	audio_spec.format = AUDIO_F32;
	audio_spec.channels = 2;
	audio_spec.samples = 4096;

	sounddev = SDL_OpenAudioDevice(NULL, 0, &audio_spec, NULL, 0);
	if (!sounddev) 
	{
		I_Warning("E_PlayMovie: Failed to hijack audio device: %s", SDL_GetError());
		return false;
	}
	SDL_PauseAudioDevice(sounddev, 0);

	plm_set_audio_lead_time(decoder, (double)audio_spec.samples / (double)rate);
	return true;
}

static void Movie_ReturnSound(void)
{
	if (sounddev)
	{
		SDL_CloseAudioDevice(sounddev);
		sounddev = 0;
	}
	nosound = false;
	I_StartupSound();
	S_Init();
}

void Movie_AudioCallback(plm_t *mpeg, plm_samples_t *samples, void *user)
{
	(void)user;

	SDL_QueueAudio(sounddev, samples->interleaved, sizeof(float) * samples->count * 2);
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

	epi::file_c *movie = W_OpenPackFile(name);

	if (!movie)
	{
		I_Warning("E_PlayMovie: Could not open %s!\n", name.c_str());
		return;
	}

	uint8_t *bytes = movie->LoadIntoMemory();

	if (!bytes)
	{
		I_Warning("E_PlayMovie: Could not open %s!\n", name.c_str());
		delete movie;
		return;
	}

	if (decoder)
	{
		plm_destroy(decoder);
		decoder = nullptr;
	}

	decoder = plm_create_with_memory(bytes, movie->GetLength(), TRUE);

	if (!decoder)
	{
		I_Warning("E_PlayMovie: Could not open %s!\n", name.c_str());
		delete movie;
		delete[] bytes;
		return;
	}

	delete movie; // No longer needed; we don't delete bytes here because plm will handle this on destroy

	if (plm_get_num_audio_streams(decoder) > 0)
	{
		if (!Movie_HijackSound(plm_get_samplerate(decoder)))
		{
			Movie_ReturnSound();
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
	//float movie_ratio = movie_width / movie_height;
	int num_pixels = movie_width * movie_height * 3;
	rgb_data = new uint8_t[num_pixels];
	memset(rgb_data, 0, num_pixels);
	plm_set_video_decode_callback(decoder, Movie_VideoCallback, nullptr);
	plm_set_audio_decode_callback(decoder, Movie_AudioCallback, nullptr);
	if (sounddev)
	{
		plm_set_audio_enabled(decoder, TRUE);
		plm_set_audio_stream(decoder, 0);
	}

	I_FinishFrame();
	I_StartFrame();

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

			// TODO: Fit this to screen dimensions while preserving aspect ratio
			int frame_width = SCREENWIDTH;
			int frame_height = SCREENHEIGHT;

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
			case SDL_KEYUP:
			case SDL_MOUSEBUTTONUP:
			case SDL_CONTROLLERBUTTONUP:
				playing_movie = false;
				break;

			default:
				break;
			}
		}
	}
	playing_movie = false;
	plm_destroy(decoder);
	decoder = nullptr;
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
	Movie_ReturnSound();
	I_FinishFrame();
	I_StartFrame();
	return;
}