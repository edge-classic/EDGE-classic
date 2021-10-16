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

#ifndef __EPI_SOUNDDATA_H__
#define __EPI_SOUNDDATA_H__

namespace epi
{

typedef enum
{
	SBUF_Mono = 0,
	SBUF_Stereo = 1,
	SBUF_Interleaved = 2
}
sfx_buffer_mode_e;


class sound_data_c
{
public:
	int length; // number of samples
	int freq;   // frequency
	int mode;   // one of the SBUF_xxx values

	// signed 16-bit samples.
	// For SBUF_Mono, both pointers refer to the same memory.
	// For SBUF_Interleaved, only data_L is used and contains
	// both channels, left samples before right samples.
	s16_t *data_L;
	s16_t *data_R;

	// values for the engine to use
	void *priv_data;

	int ref_count;

public:
	sound_data_c();
	~sound_data_c();

	void Allocate(int samples, int buf_mode);
	void Free();
};

} // namespace epi

#endif /* __EPI_SOUNDDATA_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
