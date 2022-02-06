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

#include "Biquad.h"
#include "nh_hall.hpp"

namespace epi
{

sound_data_c::sound_data_c() :
	length(0), freq(0), mode(0),
	data_L(NULL), data_R(NULL),
	submerged_data_L(NULL), submerged_data_R(NULL),
	vacuum_data_L(NULL), vacuum_data_R(NULL),
	reverb_data(NULL),
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
	if (submerged_data_R && submerged_data_R != submerged_data_L)
		delete[] submerged_data_R;

	if (submerged_data_L)
		delete[] submerged_data_L;

	submerged_data_L = NULL;
	submerged_data_R = NULL;
}

void sound_data_c::Free_Airless()
{
	if (vacuum_data_R && vacuum_data_R != vacuum_data_L)
		delete[] vacuum_data_R;

	if (vacuum_data_L)
		delete[] vacuum_data_L;

	vacuum_data_L = NULL;
	vacuum_data_R = NULL;
}

void sound_data_c::Free_Reverb()
{
	if (reverb_data->reverb_low_L)
		delete[] reverb_data->reverb_low_L;
	if (reverb_data->reverb_low_R)
		delete[] reverb_data->reverb_low_R;
	if (reverb_data->reverb_medium_L)
		delete[] reverb_data->reverb_medium_L;
	if (reverb_data->reverb_medium_R)
		delete[] reverb_data->reverb_medium_R;
	if (reverb_data->reverb_high_L)
		delete[] reverb_data->reverb_high_L;
	if (reverb_data->reverb_high_R)
		delete[] reverb_data->reverb_high_R;

	delete reverb_data;
	reverb_data = NULL;
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

	if (submerged_data_L || submerged_data_R)
	{
		Free_Underwater();
	}

	if (vacuum_data_L || vacuum_data_R)
	{
		Free_Airless();
	}

	if (reverb_data)
	{
		Free_Reverb();
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
			submerged_data_L = new s16_t[length];
			memset(submerged_data_L, 0, length * sizeof(s16_t));
			submerged_data_R = submerged_data_L;
			data_L_float = new float[length];
			memset(data_L_float, 0, length * sizeof(float));
			Signed_To_Float(data_L, data_L_float, length);
			for (int i = 0; i < length; i++) 
			{
				nh_ugens::Stereo result = reverb.process(data_L_float[i], data_L_float[i]);
				data_L_float[i] = result[0];
				data_L_float[i] = lpFilter->process(data_L_float[i]);
			}
			Float_To_Signed(data_L_float, submerged_data_L, length);
			break;

		case SBUF_Stereo:
			submerged_data_L = new s16_t[length];
			memset(submerged_data_L, 0, length * sizeof(s16_t));
			submerged_data_R = new s16_t[length];
			memset(submerged_data_R, 0, length * sizeof(s16_t));
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
			Float_To_Signed(data_L_float, submerged_data_L, length);
			Float_To_Signed(data_R_float, submerged_data_R, length);
			break;

		case SBUF_Interleaved:
			submerged_data_L = new s16_t[length * 2];
			memset(submerged_data_L, 0, length * 2 * sizeof(s16_t));
			submerged_data_R = submerged_data_L;
			data_L_float = new float[length * 2];
			memset(data_L_float, 0, length * 2 * sizeof(float));
			Signed_To_Float(data_L, data_L_float, length * 2);
			for (int i = 0; i < length * 2; i++) 
			{
				nh_ugens::Stereo result = reverb.process(data_L_float[i], data_L_float[i]);
				data_L_float[i] = result[0];
				data_L_float[i] = lpFilter->process(data_L_float[i]);
			}
			Float_To_Signed(data_L_float, submerged_data_L, length * 2);
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
			vacuum_data_L = new s16_t[length];
			memset(vacuum_data_L, 0, length * sizeof(s16_t));
			vacuum_data_R = vacuum_data_L;
			data_L_float = new float[length];
			memset(data_L_float, 0, length * sizeof(float));
			Signed_To_Float(data_L, data_L_float, length);
			for (int i = 0; i < length; i++) 
			{
				data_L_float[i] = lpFilter->process(data_L_float[i]);
			}
			Float_To_Signed(data_L_float, vacuum_data_L, length);
			break;

		case SBUF_Stereo:
			vacuum_data_L = new s16_t[length];
			memset(vacuum_data_L, 0, length * sizeof(s16_t));
			vacuum_data_R = new s16_t[length];
			memset(vacuum_data_R, 0, length * sizeof(s16_t));
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
			Float_To_Signed(data_L_float, vacuum_data_L, length);
			Float_To_Signed(data_R_float, vacuum_data_R, length);
			break;

		case SBUF_Interleaved:
			vacuum_data_L = new s16_t[length * 2];
			memset(vacuum_data_L, 0, length * 2 * sizeof(s16_t));
			vacuum_data_R = vacuum_data_L;
			data_L_float = new float[length * 2];
			memset(data_L_float, 0, length * 2 * sizeof(float));
			Signed_To_Float(data_L, data_L_float, length * 2);
			for (int i = 0; i < length * 2; i++) 
			{
				data_L_float[i] = lpFilter->process(data_L_float[i]);
			}
			Float_To_Signed(data_L_float, vacuum_data_L, length * 2);
			break;
	}
}

void sound_data_c::Mix_Reverb()
{
	float *data_L_float;
	float *data_R_float;
	nh_ugens::NHHall<> reverb_low(freq);
	nh_ugens::NHHall<> reverb_medium(freq);
	nh_ugens::NHHall<> reverb_high(freq);
	reverb_low.set_rt60(0.25f);
	reverb_medium.set_rt60(0.5f);
	reverb_high.set_rt60(0.75f);

	reverb_data = new reverb_buffers;

	/* These are the available settings to play with - Dasho

    reverb_data.set_rt60(float rt60)
        Set the 60 dB decay time for mid frequencies.

    reverb_data.set_stereo(float stereo)
        Set the stereo spread. 0 keeps the two channel paths separate, 1 causes
        them to bleed into each other almost instantly.

    reverb_data.set_low_shelf_parameters(float frequency, float ratio)
    reverb_data.set_hi_shelf_parameters(float frequency, float ratio)
        Set the frequency cutoffs and decay ratios of the damping filters.

    reverb_data.set_early_diffusion(float diffusion)
    reverb_data.set_late_diffusion(float diffusion)
        Diffusion coefficients.

    reverb_data.set_mod_rate(float mod_rate)
    reverb_data.set_mod_depth(float mod_depth)
        Rate and depth of LFO. These are arbitrarily scaled so that 0..1 offers
        musically useful ranges.*/

	switch (mode)
	{
		case SBUF_Mono:
			reverb_data->reverb_low_L = new s16_t[length];
			reverb_data->reverb_medium_L = new s16_t[length];
			reverb_data->reverb_high_L = new s16_t[length];
			memset(reverb_data->reverb_low_L, 0, length * sizeof(s16_t));
			memset(reverb_data->reverb_medium_L, 0, length * sizeof(s16_t));
			memset(reverb_data->reverb_high_L, 0, length * sizeof(s16_t));
			reverb_data->reverb_low_R = reverb_data->reverb_low_L;
			reverb_data->reverb_medium_R = reverb_data->reverb_medium_L;
			reverb_data->reverb_high_R = reverb_data->reverb_high_L;
			data_L_float = new float[length];
			memset(data_L_float, 0, length * sizeof(float));
			Signed_To_Float(data_L, data_L_float, length);
			for (int i = 0; i < length; i++) 
			{
				nh_ugens::Stereo result = reverb_low.process(data_L_float[i], data_L_float[i]);
				data_L_float[i] = result[0];
			}
			Float_To_Signed(data_L_float, reverb_data->reverb_low_L, length);
			memset(data_L_float, 0, length * sizeof(float));
			Signed_To_Float(data_L, data_L_float, length);
			for (int i = 0; i < length; i++) 
			{
				nh_ugens::Stereo result = reverb_medium.process(data_L_float[i], data_L_float[i]);
				data_L_float[i] = result[0];
			}
			Float_To_Signed(data_L_float, reverb_data->reverb_medium_L, length);
			memset(data_L_float, 0, length * sizeof(float));
			Signed_To_Float(data_L, data_L_float, length);
			for (int i = 0; i < length; i++) 
			{
				nh_ugens::Stereo result = reverb_high.process(data_L_float[i], data_L_float[i]);
				data_L_float[i] = result[0];
			}
			Float_To_Signed(data_L_float, reverb_data->reverb_high_L, length);
			break;

		case SBUF_Stereo:
			reverb_data->reverb_low_L = new s16_t[length];
			reverb_data->reverb_medium_L = new s16_t[length];
			reverb_data->reverb_high_L = new s16_t[length];
			reverb_data->reverb_low_R = new s16_t[length];
			reverb_data->reverb_medium_R = new s16_t[length];
			reverb_data->reverb_high_R = new s16_t[length];
			memset(reverb_data->reverb_low_L, 0, length * sizeof(s16_t));
			memset(reverb_data->reverb_medium_L, 0, length * sizeof(s16_t));
			memset(reverb_data->reverb_high_L, 0, length * sizeof(s16_t));
			memset(reverb_data->reverb_low_R, 0, length * sizeof(s16_t));
			memset(reverb_data->reverb_medium_R, 0, length * sizeof(s16_t));
			memset(reverb_data->reverb_high_R, 0, length * sizeof(s16_t));
			data_L_float = new float[length];
			data_R_float = new float[length];
			memset(data_L_float, 0, length * sizeof(float));
			memset(data_R_float, 0, length * sizeof(float));
			Signed_To_Float(data_L, data_L_float, length);
			Signed_To_Float(data_R, data_R_float, length);
			for (int i = 0; i < length; i++) 
			{
				nh_ugens::Stereo result = reverb_low.process(data_L_float[i], data_R_float[i]);
				data_L_float[i] = result[0];
				data_R_float[i] = result[1];
			}
			Float_To_Signed(data_L_float, reverb_data->reverb_low_L, length);
			Float_To_Signed(data_R_float, reverb_data->reverb_low_R, length);
			break;

		case SBUF_Interleaved:
			reverb_data->reverb_low_L = new s16_t[length * 2];
			reverb_data->reverb_medium_L = new s16_t[length * 2];
			reverb_data->reverb_high_L = new s16_t[length * 2];
			memset(reverb_data->reverb_low_L, 0, length * 2 * sizeof(s16_t));
			memset(reverb_data->reverb_medium_L, 0, length * 2 * sizeof(s16_t));
			memset(reverb_data->reverb_high_L, 0, length * 2 * sizeof(s16_t));
			reverb_data->reverb_low_R = reverb_data->reverb_low_L;
			reverb_data->reverb_medium_R = reverb_data->reverb_medium_L;
			reverb_data->reverb_high_R = reverb_data->reverb_high_L;
			data_L_float = new float[length * 2];
			memset(data_L_float, 0, length * 2 * sizeof(float));
			Signed_To_Float(data_L, data_L_float, length * 2);
			for (int i = 0; i < length * 2; i++) 
			{
				nh_ugens::Stereo result = reverb_low.process(data_L_float[i], data_L_float[i]);
				data_L_float[i] = result[0];
			}
			Float_To_Signed(data_L_float, reverb_data->reverb_low_L, length * 2);
			break;
	}
}

s16_t* sound_data_c::Select_Reverb(int channel, float area)
{
	if (area < 150000.0f)
	{
		return channel == 0 ? reverb_data->reverb_low_L : reverb_data->reverb_low_R;
	}
	else if (area < 800000.0f)
	{
		return channel == 0 ? reverb_data->reverb_medium_L : reverb_data->reverb_medium_R;
	}
	else
	{
		return channel == 0 ? reverb_data->reverb_high_L : reverb_data->reverb_high_R;
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
