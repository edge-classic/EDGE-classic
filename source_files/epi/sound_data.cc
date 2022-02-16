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
	float_data_L(NULL), float_data_R(NULL),
	fx_data_L(NULL), fx_data_R(NULL),
	priv_data(NULL), ref_count(0), is_sfx(false),
	current_mix(SFX_None)
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

	Free_Float();
	Free_FX();
}

void sound_data_c::Free_Float()
{
	if (float_data_R && float_data_R != float_data_L)
		delete[] float_data_R;

	if (float_data_L)
		delete[] float_data_L;

	float_data_L = NULL;
	float_data_R = NULL;
}

void sound_data_c::Free_FX()
{
	if (fx_data_R && fx_data_R != fx_data_L)
		delete[] fx_data_R;

	if (fx_data_L)
		delete[] fx_data_L;

	fx_data_L = NULL;
	fx_data_R = NULL;
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

void sound_data_c::Mix_Float()
{
	switch (mode)
	{
		case SBUF_Mono:
			float_data_L = new float[length];
			for (int i = 0; i < length; i++) 
			{
				float_data_L[i] = (data_L[i] + 0.5) / (32767.5);
			}
			break;

		case SBUF_Stereo:
			float_data_L = new float[length];
			float_data_R = new float[length];
			for (int i = 0; i < length; i++) 
			{
				float_data_L[i] = (data_L[i] + 0.5) / (32767.5);
				float_data_R[i] = (data_R[i] + 0.5) / (32767.5);
			}
			break;

		case SBUF_Interleaved:
			float_data_L = new float[length * 2];
			for (int i = 0; i < length * 2; i++) 
			{
				float_data_L[i] = (data_L[i] + 0.5) / (32767.5);
			}
			break;
	}
}

void sound_data_c::Mix_Submerged()
{
	if (current_mix != SFX_Submerged)
	{
		Biquad *lpFilter = new Biquad(bq_type_lowpass, 750.0 / freq, 0.707, 0);
		nh_ugens::NHHall<> reverb(freq);
		switch (mode)
		{
			case SBUF_Mono:
				if (!fx_data_L)
					fx_data_L = new s16_t[length];
				fx_data_R = fx_data_L;
				for (int i = 0; i < length; i++) 
				{
					nh_ugens::Stereo result = reverb.process(float_data_L[i], float_data_L[i]);
					fx_data_L[i] = lpFilter->process(result[0]) * 38767.5 - .5;
				}
				current_mix = SFX_Submerged;
				break;

			case SBUF_Stereo:
				if (!fx_data_L)
					fx_data_L = new s16_t[length];
				if (!fx_data_R)
					fx_data_R = new s16_t[length];
				for (int i = 0; i < length; i++) 
				{
					nh_ugens::Stereo result = reverb.process(float_data_L[i], float_data_R[i]);
					fx_data_L[i] = lpFilter->process(result[0]) * 38767.5 - .5;
					fx_data_R[i] = lpFilter->process(result[1]) * 38767.5 - .5;
				}
				current_mix = SFX_Submerged;

			case SBUF_Interleaved:
				if (!fx_data_L)
					fx_data_L = new s16_t[length * 2];
				fx_data_R = fx_data_L;
				for (int i = 0; i < length * 2; i++) 
				{
					nh_ugens::Stereo result = reverb.process(float_data_L[i], float_data_L[i]);
					fx_data_L[i] = lpFilter->process(result[0]) * 38767.5 - .5;
				}
				current_mix = SFX_Submerged;
		}
	}
}

void sound_data_c::Mix_Vacuum()
{
	if (current_mix != SFX_Vacuum)
	{
		Biquad *lpFilter = new Biquad(bq_type_lowpass, 200.0 / freq, 0.707, 0);
		switch (mode)
		{
			case SBUF_Mono:
				if (!fx_data_L)
					fx_data_L = new s16_t[length];
				fx_data_R = fx_data_L;
				for (int i = 0; i < length; i++) 
				{
					fx_data_L[i] = lpFilter->process(float_data_L[i]) * 38767.5 - .5;
				}
				current_mix = SFX_Vacuum;
				break;

			case SBUF_Stereo:
				if (!fx_data_L)
					fx_data_L = new s16_t[length];
				if (!fx_data_R)
					fx_data_R = new s16_t[length];
				for (int i = 0; i < length; i++) 
				{
					fx_data_L[i] = lpFilter->process(float_data_L[i]) * 38767.5 - .5;
					fx_data_R[i] = lpFilter->process(float_data_R[i]) * 38767.5 - .5;
				}
				current_mix = SFX_Vacuum;

			case SBUF_Interleaved:
				if (!fx_data_L)
					fx_data_L = new s16_t[length * 2];
				fx_data_R = fx_data_L;
				for (int i = 0; i < length * 2; i++) 
				{
					fx_data_L[i] = lpFilter->process(float_data_L[i]) * 38767.5 - .5;
				}
				current_mix = SFX_Vacuum;
		}
	}
}

void sound_data_c::Mix_Reverb(float room_area)
{
	if (current_mix != SFX_Reverb)
	{
		nh_ugens::NHHall<> reverb(freq);
		if (room_area > 1000000)
			reverb.set_rt60(1.0f);
		else if (room_area > 200000)
			reverb.set_rt60(0.75f);
		else
			reverb.set_rt60(0.5f);
		reverb.set_early_diffusion(0);
		switch (mode)
		{
			case SBUF_Mono:
				if (!fx_data_L)
					fx_data_L = new s16_t[length];
				fx_data_R = fx_data_L;
				for (int i = 0; i < length; i++) 
				{
					nh_ugens::Stereo result = reverb.process(float_data_L[i], float_data_L[i]);
					fx_data_L[i] = CLAMP(INT16_MIN, result[0] * 38767.5 - .5, INT16_MAX);
				}
				current_mix = SFX_Reverb;
				break;

			case SBUF_Stereo:
				if (!fx_data_L)
					fx_data_L = new s16_t[length];
				if (!fx_data_R)
					fx_data_R = new s16_t[length];
				for (int i = 0; i < length; i++) 
				{
					nh_ugens::Stereo result = reverb.process(float_data_L[i], float_data_R[i]);
					fx_data_L[i] = CLAMP(INT16_MIN, result[0] * 38767.5 - .5, INT16_MAX);
					fx_data_R[i] = CLAMP(INT16_MIN, result[1] * 38767.5 - .5, INT16_MAX);
				}
				current_mix = SFX_Reverb;

			case SBUF_Interleaved:
				if (!fx_data_L)
					fx_data_L = new s16_t[length * 2];
				fx_data_R = fx_data_L;
				for (int i = 0; i < length * 2; i++) 
				{
					nh_ugens::Stereo result = reverb.process(float_data_L[i], float_data_L[i]);
					fx_data_L[i] = CLAMP(INT16_MIN, result[0] * 38767.5 - .5, INT16_MAX);
				}
				current_mix = SFX_Reverb;
		}
	}
}

}  // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
