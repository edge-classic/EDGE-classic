//----------------------------------------------------------------------------
//  Sound Gather class
//----------------------------------------------------------------------------
//
//  Copyright (c) 2008-2024 The EDGE Team.
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

#pragma once

#include <vector>

#include "snd_data.h"

// private stuff
class GatherChunk;

class SoundGatherer
{
  private:
    std::vector<GatherChunk *> chunks_;

    int total_samples_;

    GatherChunk *request_;

  public:
    SoundGatherer();
    ~SoundGatherer();

    float *MakeChunk(int max_samples, bool stereo);
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

    bool Finalise(SoundData *buf, bool want_stereo);
    // take all the stored sound data and transfer it to the
    // SoundData object, making it all contiguous, and
    // converting from/to stereoness where needed.
    //
    // Returns false (failure) if total samples was zero,
    // otherwise returns true (success).

  private:
    void TransferMono(GatherChunk *chunk, SoundData *buf, int pos);
    void TransferStereo(GatherChunk *chunk, SoundData *buf, int pos);
};

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
