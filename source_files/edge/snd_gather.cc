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

#include "snd_gather.h"

#include "epi.h"

class GatherChunk
{
   public:
    int16_t *samples_;

    int total_samples_;  // total number is *2 for stereo

    bool is_stereo_;

   public:
    GatherChunk(int count, bool stereo)
        : total_samples_(count), is_stereo_(stereo)
    {
        SYS_ASSERT(total_samples_ > 0);

        samples_ = new int16_t[total_samples_ * (is_stereo_ ? 2 : 1)];
    }

    ~GatherChunk() { delete[] samples_; }
};

//----------------------------------------------------------------------------

SoundGatherer::SoundGatherer() : chunks_(), total_samples_(0), request_(nullptr)
{
}

SoundGatherer::~SoundGatherer()
{
    if (request_) DiscardChunk();

    for (unsigned int i = 0; i < chunks_.size(); i++) delete chunks_[i];
}

int16_t *SoundGatherer::MakeChunk(int max_samples, bool _stereo)
{
    SYS_ASSERT(!request_);
    SYS_ASSERT(max_samples > 0);

    request_ = new GatherChunk(max_samples, _stereo);

    return request_->samples_;
}

void SoundGatherer::CommitChunk(int actual_samples)
{
    SYS_ASSERT(request_);
    SYS_ASSERT(actual_samples >= 0);

    if (actual_samples == 0)
    {
        DiscardChunk();
        return;
    }

    SYS_ASSERT(actual_samples <= request_->total_samples_);

    request_->total_samples_ = actual_samples;
    total_samples_ += actual_samples;

    chunks_.push_back(request_);
    request_ = nullptr;
}

void SoundGatherer::DiscardChunk()
{
    SYS_ASSERT(request_);

    delete request_;
    request_ = nullptr;
}

bool SoundGatherer::Finalise(SoundData *buf, bool want_stereo)
{
    if (total_samples_ == 0) return false;

    buf->Allocate(total_samples_, want_stereo ? kMixStereo : kMixMono);

    int pos = 0;

    for (unsigned int i = 0; i < chunks_.size(); i++)
    {
        if (want_stereo)
            TransferStereo(chunks_[i], buf, pos);
        else
            TransferMono(chunks_[i], buf, pos);

        pos += chunks_[i]->total_samples_;
    }

    SYS_ASSERT(pos == total_samples_);

    return true;
}

void SoundGatherer::TransferMono(GatherChunk *chunk, SoundData *buf, int pos)
{
    int count = chunk->total_samples_;

    int16_t *dest     = buf->data_left_ + pos;
    int16_t *dest_end = dest + count;

    const int16_t *src = chunk->samples_;

    if (chunk->is_stereo_)
    {
        for (; dest < dest_end; src += 2)
        {
            *dest++ = ((int)src[0] + (int)src[1]) >> 1;
        }
    }
    else { memcpy(dest, src, count * sizeof(int16_t)); }
}

void SoundGatherer::TransferStereo(GatherChunk *chunk, SoundData *buf, int pos)
{
    int count = chunk->total_samples_;

    int16_t *dest_L = buf->data_left_ + pos;
    int16_t *dest_R = buf->data_right_ + pos;

    const int16_t *src     = chunk->samples_;
    const int16_t *src_end = src + count * (chunk->is_stereo_ ? 2 : 1);

    if (chunk->is_stereo_)
    {
        for (; src < src_end; src += 2)
        {
            *dest_L++ = src[0];
            *dest_R++ = src[1];
        }
    }
    else
    {
        while (src < src_end)
        {
            *dest_L++ = *src;
            *dest_R++ = *src++;
        }
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
