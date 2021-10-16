//----------------------------------------------------------------------------
//  Sound Gather class
//----------------------------------------------------------------------------
// 
//  Copyright (c) 2008  The EDGE Team.
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

#ifndef __EPI_SOUND_GATHER_H__
#define __EPI_SOUND_GATHER_H__

#include <vector>

#include "sound_data.h"

namespace epi
{

// private stuff
class gather_chunk_c;
	

class sound_gather_c
{
private:
	std::vector<gather_chunk_c *> chunks;

	int total_samples;

	gather_chunk_c *request;

public:
	 sound_gather_c();
	~sound_gather_c();

	s16_t * MakeChunk(int max_samples, bool _stereo);
	// prepare to add a chunk of sound samples.  Returns a buffer
	// containing the number of samples (* 2 for stereo) which the
	// user can fill up.

	void CommitChunk(int actual_samples);
	// add the current chunk to the stored sound data.
	// The number of samples may be less than the size requested
	// by the MakeChunk() call.  Passing zero for 'actual_samples'
	// is equivalent to callng the DiscardChunk() method.

	void DiscardChunk();
	// get rid of current chunk (because it wasn't needed, e.g.
	// the sound file you were reading hit EOF).

	bool Finalise(sound_data_c *buf, bool want_stereo);
	// take all the stored sound data and transfer it to the
	// sound_data_c object, making it all contiguous, and
	// converting from/to stereoness where needed.
	//
	// Returns false (failure) if total samples was zero,
	// otherwise returns true (success).

private:
	void TransferMono  (gather_chunk_c *chunk, sound_data_c *buf, int pos);
	void TransferStereo(gather_chunk_c *chunk, sound_data_c *buf, int pos);
};

} // namespace epi

#endif /* __EPI_SOUND_GATHER_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
