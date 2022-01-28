//----------------------------------------------------------------------------
//  Sound Data
//----------------------------------------------------------------------------
// 
//  Copyright (c) 1999-2008  The EDGE Team.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//----------------------------------------------------------------------------

#include "epi.h"
#include "sound_data.h"

#include <vector>

#include "biquad.h"
#include "nh_hall.hpp"

namespace epi
{

sound_data_c::sound_data_c() :
	length(0), freq(0), mode(0),
	data_L(NULL), data_R(NULL),
	underwater_data_L(NULL), underwater_data_R(NULL),
	airless_data_L(NULL), airless_data_R(NULL),
	priv_data(NULL), ref_count(0)
{ }

sound_data_c::~sound_data_c()
{
	Free();
}

void sound_data_c::Free()
{
	length = 0;

	if (data_R && data_R != data_L)
		delete[] data_R;

	if (data_L)
		delete[] data_L;

	data_L = NULL;
	data_R = NULL;
}

void sound_data_c::Free_Underwater()
{
	if (underwater_data_R && underwater_data_R != underwater_data_L)
		delete[] underwater_data_R;

	if (underwater_data_L)
		delete[] underwater_data_L;

	underwater_data_L = NULL;
	underwater_data_R = NULL;
}

void sound_data_c::Free_Airless()
{
	if (airless_data_R && airless_data_R != airless_data_L)
		delete[] airless_data_R;

	if (airless_data_L)
		delete[] airless_data_L;

	airless_data_L = NULL;
	airless_data_R = NULL;
}

void sound_data_c::Allocate(int samples, int buf_mode)
{
	// early out when requirements are already met
	if (data_L && length >= samples && mode == buf_mode)
	{
		length = samples;  // FIXME: perhaps keep allocated count
		return;
	}

	if (data_L || data_R)
	{
		Free();
	}

	if (underwater_data_L || underwater_data_R)
	{
		Free_Underwater();
	}

	if (airless_data_L || airless_data_R)
	{
		Free_Airless();
	}

	length = samples;
	mode   = buf_mode;

	switch (buf_mode)
	{
		case SBUF_Mono:
			data_L = new s16_t[samples];
			data_R = data_L;
			break;

		case SBUF_Stereo:
			data_L = new s16_t[samples];
			data_R = new s16_t[samples];
			break;

		case SBUF_Interleaved:
			data_L = new s16_t[samples * 2];
			data_R = data_L;
			break;

		default: break;
	}
}

void sound_data_c::Mix_Underwater()
{
	float *data_L_float;
	float *data_R_float;
	Biquad *lpFilter = new Biquad(bq_type_lowpass, 750.0 / freq, 0.707, 0);
	nh_ugens::NHHall<> reverb(freq);

	switch (mode)
	{
		case SBUF_Mono:
			underwater_data_L = new s16_t[length];
			memset(underwater_data_L, 0, length * sizeof(s16_t));
			underwater_data_R = underwater_data_L;
			data_L_float = new float[length];
			memset(data_L_float, 0, length * sizeof(float));
			Signed_To_Float(data_L, data_L_float, length);
			for (int i = 0; i < length; i++) 
			{
				nh_ugens::Stereo result = reverb.process(data_L_float[i], data_L_float[i]);
				data_L_float[i] = result[0];
				data_L_float[i] = lpFilter->process(data_L_float[i]);
			}
			Float_To_Signed(data_L_float, underwater_data_L, length);
			break;

		case SBUF_Stereo:
			underwater_data_L = new s16_t[length];
			memset(underwater_data_L, 0, length * sizeof(s16_t));
			underwater_data_R = new s16_t[length];
			memset(underwater_data_R, 0, length * sizeof(s16_t));
			data_L_float = new float[length];
			data_R_float = new float[length];
			memset(data_L_float, 0, length * sizeof(float));
			memset(data_R_float, 0, length * sizeof(float));
			Signed_To_Float(data_L, data_L_float, length);
			Signed_To_Float(data_R, data_R_float, length);
			for (int i = 0; i < length; i++) 
			{
				nh_ugens::Stereo result = reverb.process(data_L_float[i], data_R_float[i]);
				data_L_float[i] = result[0];
				data_R_float[i] = result[1];
				data_L_float[i] = lpFilter->process(data_L_float[i]);
				data_R_float[i] = lpFilter->process(data_R_float[i]);
			}
			Float_To_Signed(data_L_float, underwater_data_L, length);
			Float_To_Signed(data_R_float, underwater_data_R, length);
			break;

		case SBUF_Interleaved:
			underwater_data_L = new s16_t[length * 2];
			memset(underwater_data_L, 0, length * 2 * sizeof(s16_t));
			underwater_data_R = underwater_data_L;
			data_L_float = new float[length * 2];
			memset(data_L_float, 0, length * 2 * sizeof(float));
			Signed_To_Float(data_L, data_L_float, length * 2);
			for (int i = 0; i < length; i++) 
			{
				nh_ugens::Stereo result = reverb.process(data_L_float[i], data_L_float[i]);
				data_L_float[i] = result[0];
				data_L_float[i] = lpFilter->process(data_L_float[i]);
			}
			Float_To_Signed(data_L_float, underwater_data_L, length * 2);
			break;
	}
}

void sound_data_c::Mix_Airless()
{
	float *data_L_float;
	float *data_R_float;
	Biquad *lpFilter = new Biquad(bq_type_lowpass, 200.0 / freq, 0.707, 0);

	switch (mode)
	{
		case SBUF_Mono:
			airless_data_L = new s16_t[length];
			memset(airless_data_L, 0, length * sizeof(s16_t));
			airless_data_R = airless_data_L;
			data_L_float = new float[length];
			memset(data_L_float, 0, length * sizeof(float));
			Signed_To_Float(data_L, data_L_float, length);
			for (int i = 0; i < length; i++) 
			{
				data_L_float[i] = lpFilter->process(data_L_float[i]);
			}
			Float_To_Signed(data_L_float, airless_data_L, length);
			break;

		case SBUF_Stereo:
			airless_data_L = new s16_t[length];
			memset(airless_data_L, 0, length * sizeof(s16_t));
			airless_data_R = new s16_t[length];
			memset(airless_data_R, 0, length * sizeof(s16_t));
			data_L_float = new float[length];
			data_R_float = new float[length];
			memset(data_L_float, 0, length * sizeof(float));
			memset(data_R_float, 0, length * sizeof(float));
			Signed_To_Float(data_L, data_L_float, length);
			Signed_To_Float(data_R, data_R_float, length);
			for (int i = 0; i < length; i++) 
			{
				data_L_float[i] = lpFilter->process(data_L_float[i]);
				data_R_float[i] = lpFilter->process(data_R_float[i]);
			}
			Float_To_Signed(data_L_float, airless_data_L, length);
			Float_To_Signed(data_R_float, airless_data_R, length);
			break;

		case SBUF_Interleaved:
			airless_data_L = new s16_t[length * 2];
			memset(airless_data_L, 0, length * 2 * sizeof(s16_t));
			airless_data_R = airless_data_L;
			data_L_float = new float[length * 2];
			memset(data_L_float, 0, length * 2 * sizeof(float));
			Signed_To_Float(data_L, data_L_float, length * 2);
			for (int i = 0; i < length; i++) 
			{
				data_L_float[i] = lpFilter->process(data_L_float[i]);
			}
			Float_To_Signed(data_L_float, airless_data_L, length * 2);
			break;
	}
}

void sound_data_c::Float_To_Signed(float *data_float, s16_t *data_signed, int samples)
{
	s16_t *data_signed_end = data_signed + samples;

	while (data_signed != data_signed_end)
	{
		float v = *data_float++;
		v = ((v < -1) ? -1 : ((v > 1) ? 1 : v));
    	*data_signed++ = (s16_t)(v * (v < 0 ? 32768 : 32767));
	}
}

void sound_data_c::Signed_To_Float(s16_t *data_signed, float *data_float, int samples)
{
	float *data_float_end = data_float + samples;

	while (data_float != data_float_end)
	{
		s16_t v = *data_signed++;
		*data_float++ = (float)v / (v < 0 ? 32768 : 32767);
	}
}

}  // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
