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
    float *samples_;

    int total_samples_; // total number is *2 for stereo

    bool is_stereo_;

  public:
    GatherChunk(int count, bool stereo) : total_samples_(count), is_stereo_(stereo)
    {
        EPI_ASSERT(total_samples_ > 0);

        samples_ = new float[total_samples_ * (is_stereo_ ? 2 : 1)];
    }

    ~GatherChunk()
    {
        delete[] samples_;
    }
};

//----------------------------------------------------------------------------

SoundGatherer::SoundGatherer() : chunks_(), total_samples_(0), request_(nullptr)
{
}

SoundGatherer::~SoundGatherer()
{
    if (request_)
        DiscardChunk();

    for (unsigned int i = 0; i < chunks_.size(); i++)
        delete chunks_[i];
}

float *SoundGatherer::MakeChunk(int max_samples, bool _stereo)
{
    EPI_ASSERT(!request_);
    EPI_ASSERT(max_samples > 0);

    request_ = new GatherChunk(max_samples, _stereo);

    return request_->samples_;
}

void SoundGatherer::CommitChunk(int actual_samples)
{
    EPI_ASSERT(request_);
    EPI_ASSERT(actual_samples >= 0);

    if (actual_samples == 0)
    {
        DiscardChunk();
        return;
    }

    EPI_ASSERT(actual_samples <= request_->total_samples_);

    request_->total_samples_ = actual_samples;
    total_samples_ += actual_samples;

    chunks_.push_back(request_);
    request_ = nullptr;
}

void SoundGatherer::DiscardChunk()
{
    EPI_ASSERT(request_);

    delete request_;
    request_ = nullptr;
}

bool SoundGatherer::Finalise(SoundData *buf, bool want_stereo)
{
    if (total_samples_ == 0)
        return false;

    buf->Allocate(total_samples_, want_stereo ? kMixInterleaved : kMixMono);

    int pos = 0;

    for (unsigned int i = 0; i < chunks_.size(); i++)
    {
        if (want_stereo)
            TransferStereo(chunks_[i], buf, pos);
        else
            TransferMono(chunks_[i], buf, pos);

        pos += chunks_[i]->total_samples_;
    }

    EPI_ASSERT(pos == total_samples_);

    return true;
}

void SoundGatherer::TransferMono(GatherChunk *chunk, SoundData *buf, int pos)
{
    int count = chunk->total_samples_;

    float *dest     = buf->data_ + pos;
    float *dest_end = dest + count;

    float *src = chunk->samples_;

    if (chunk->is_stereo_)
    {
        for (;dest < dest_end; src += 2)
        {
            *dest++ = (src[0] + src[1]) * 0.5f;
        }
    }
    else
    {
        memcpy(dest, src, count * sizeof(float));
    }
}

void SoundGatherer::TransferStereo(GatherChunk *chunk, SoundData *buf, int pos)
{
    int count = chunk->total_samples_;

    float *dest = buf->data_ + pos;

    const float *src     = chunk->samples_;
    const float *src_end = src + count * (chunk->is_stereo_ ? 2 : 1);

    if (chunk->is_stereo_)
    {
        for (; src < src_end; src += 2)
        {
            *dest++ = src[0];
            *dest++ = src[1];
        }
    }
    else
    {
        while (src < src_end)
        {
            *dest++ = *src;
            *dest++ = *src++;
        }
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
