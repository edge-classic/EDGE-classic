//----------------------------------------------------------------------------
//  Sound Data
//----------------------------------------------------------------------------
//
//  Copyright (c) 1999-2024 The EDGE Team.
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

#include "snd_data.h"

#include <string.h>

#include "HandmadeMath.h"

SoundData::SoundData()
    : length_(0), frequency_(0), data_(nullptr), filter_data_(nullptr), reverb_buffer_(nullptr),
      definition_data_(nullptr), is_sound_effect_(false), current_filter_(kFilterNone),
      reverbed_room_size_(kRoomReverbNone), current_ddf_reverb_ratio_(0), current_ddf_reverb_delay_(0),
      current_ddf_reverb_type_(0), reverb_is_outdoors_(false)
{
}

SoundData::~SoundData()
{
    Free();
}

void SoundData::Free()
{
    length_ = 0;

    if (data_)
        delete[] data_;
    data_ = nullptr;

    if (filter_data_)
        delete[] filter_data_;
    filter_data_ = nullptr;
    
    if (reverb_buffer_)
        delete[] reverb_buffer_;
    reverb_buffer_ = nullptr;
}

void SoundData::Allocate(int samples)
{
    // early out when requirements are already met
    if (data_ && length_ >= samples)
    {
        length_ = samples; // FIXME: perhaps keep allocated count
        return;
    }

    Free();

    length_ = samples;

    data_ = new int16_t[samples * 2];
}

void SoundData::MixSubmerged()
{
    if (current_filter_ != kFilterSubmerged)
    {
        // Setup lowpass + reverb parameters
        int  out_L   = 0;
        int  accum_L = 0;
        int  out_R   = 0;
        int  accum_R = 0;
        int  k       = 4;
        int  reverb_ratio = 25;
        int  reverb_delay = 100;
        int write_pos = 0;
        int read_pos = ((write_pos - reverb_delay * frequency_ / 1000) + length_) % (length_ * 2);
        int reverbed = 0;
        if (!filter_data_)
            filter_data_ = new int16_t[length_ * 2];
        memset(filter_data_, 0, length_ * sizeof(int16_t) * 2);
        if (!reverb_buffer_)
            reverb_buffer_ = new int[length_ * 2];
        memset(reverb_buffer_, 0, length_ * sizeof(int) * 2);
        for (int i = 0; i < length_ * 2; i++)
        {
            filter_data_[i] = out_L = accum_L >> k;
            accum_L                      = accum_L - out_L + data_[i];
            reverbed = filter_data_[i] + reverb_buffer_[HMM_MAX(0, read_pos)] * reverb_ratio / 100;
            filter_data_[i]       = HMM_Clamp(INT16_MIN, reverbed, INT16_MAX);
            reverb_buffer_[write_pos] = filter_data_[i];
            write_pos                  = (write_pos + 1) % (length_ * 2);
            read_pos                   = (read_pos + 1) % (length_ * 2);
            i++;
            filter_data_[i] = out_R = accum_R >> k;
            accum_R                      = accum_R - out_R + data_[i];
            reverbed = filter_data_[i] + reverb_buffer_[HMM_MAX(0, read_pos)] * reverb_ratio / 100;
            filter_data_[i]       = HMM_Clamp(INT16_MIN, reverbed, INT16_MAX);
            reverb_buffer_[write_pos] = filter_data_[i];
            write_pos                  = (write_pos + 1) % (length_ * 2);
            read_pos                   = (read_pos + 1) % (length_ * 2);
        }
        current_filter_ = kFilterSubmerged;
    }
}

void SoundData::MixVacuum()
{
    if (current_filter_ != kFilterVacuum)
    {
        // Setup lowpass parameters
        int out_L   = 0;
        int accum_L = 0;
        int out_R   = 0;
        int accum_R = 0;
        int k       = 5;
        if (!filter_data_)
            filter_data_ = new int16_t[length_ * 2];
        memset(filter_data_, 0, length_ * sizeof(int16_t) * 2);
        for (int i = 0; i < length_ * 2; i += 2)
        {
            filter_data_[i] = out_L = accum_L >> k;
            accum_L = accum_L - out_L + data_[i];
            filter_data_[i+1] = out_R = accum_R >> k;
            accum_R = accum_R - out_R + data_[i+1];
        }
        current_filter_ = kFilterVacuum;
    }
}

void SoundData::MixReverb(bool dynamic_reverb, float room_area, bool outdoor_reverb, int ddf_reverb_type,
                          int ddf_reverb_ratio, int ddf_reverb_delay)
{
    if (ddf_reverb_ratio > 0 && ddf_reverb_delay > 0 && ddf_reverb_type > 0)
    {
        if (current_filter_ != kFilterReverb || ddf_reverb_ratio != current_ddf_reverb_ratio_ ||
            ddf_reverb_delay != current_ddf_reverb_delay_ || ddf_reverb_type != current_ddf_reverb_type_)
        {
            // Setup reverb parameters
            int  reverb_ratio = ddf_reverb_ratio;
            int  reverb_delay = ddf_reverb_delay;
            int  write_pos    = 0;
            int  read_pos     = ((write_pos - reverb_delay * frequency_ / 1000) + length_ * 2) % (length_ * 2);
            int  reverbed = 0;
            if (!filter_data_)
                filter_data_ = new int16_t[length_ * 2];
            memset(filter_data_, 0, length_ * sizeof(int16_t) * 2);
            if (!reverb_buffer_)
                reverb_buffer_ = new int[length_ * 2];
            memset(reverb_buffer_, 0, length_ * sizeof(int) * 2);
            for (int i = 0; i < length_ * 2; i++)
            {
                if (ddf_reverb_type == 2)
                    reverb_buffer_[write_pos] = data_[i];
                reverbed         = data_[i] + reverb_buffer_[HMM_MAX(0, read_pos)] * reverb_ratio / 100;
                filter_data_[i] = HMM_Clamp(INT16_MIN, reverbed, INT16_MAX);
                if (ddf_reverb_type == 1)
                    reverb_buffer_[write_pos] = reverbed;
                write_pos = (write_pos + 1) % (length_ * 2);
                read_pos  = (read_pos + 1) % (length_ * 2);
                i++;
                if (ddf_reverb_type == 2)
                    reverb_buffer_[write_pos] = data_[i];
                reverbed         = data_[i] + reverb_buffer_[HMM_MAX(0, read_pos)] * reverb_ratio / 100;
                filter_data_[i] = HMM_Clamp(INT16_MIN, reverbed, INT16_MAX);
                if (ddf_reverb_type == 1)
                    reverb_buffer_[write_pos] = reverbed;
                write_pos = (write_pos + 1) % (length_ * 2);
                read_pos  = (read_pos + 1) % (length_ * 2);
            }
            current_filter_           = kFilterReverb;
            current_ddf_reverb_delay_ = ddf_reverb_delay;
            current_ddf_reverb_ratio_ = ddf_reverb_ratio;
            current_ddf_reverb_type_  = ddf_reverb_type;
            reverbed_room_size_       = kRoomReverbNone;
        }
    }
    else if (dynamic_reverb)
    {
        ReverbRoomSize current_room_size;
        if (room_area > 700)
        {
            current_room_size = kRoomReverbLarge;
        }
        else if (room_area > 350)
        {
            current_room_size = kRoomReverbMedium;
        }
        else
        {
            current_room_size = kRoomReverbSmall;
        }
        if (current_filter_ != kFilterReverb || reverbed_room_size_ != current_room_size ||
            reverb_is_outdoors_ != outdoor_reverb)
        {
            // Setup reverb parameters
            int  reverb_ratio = outdoor_reverb ? 25 : 30;
            int  reverb_delay;
            if (outdoor_reverb)
                reverb_delay = 50 * current_room_size + 25;
            else
                reverb_delay = 20 * current_room_size + 10;
            int  write_pos    = 0;
            int  read_pos     = ((write_pos - reverb_delay * frequency_ / 1000) + length_ * 2) % (length_ * 2);
            int  reverbed = 0;
            if (!filter_data_)
                filter_data_ = new int16_t[length_ * 2];
            memset(filter_data_, 0, length_ * sizeof(int16_t) * 2);
            if (!reverb_buffer_)
                reverb_buffer_ = new int[length_ * 2];
            memset(reverb_buffer_, 0, length_ * sizeof(int) * 2);
            for (int i = 0; i < length_ * 2; i++)
            {
                if (outdoor_reverb)
                    reverb_buffer_[write_pos] = data_[i];
                reverbed         = data_[i] + reverb_buffer_[HMM_MAX(0, read_pos)] * reverb_ratio / 100;
                filter_data_[i] = HMM_Clamp(INT16_MIN, reverbed, INT16_MAX);
                if (!outdoor_reverb)
                    reverb_buffer_[write_pos] = reverbed;
                write_pos = (write_pos + 1) % (length_ * 2);
                read_pos  = (read_pos + 1) % (length_ * 2);
                i++;
                if (outdoor_reverb)
                    reverb_buffer_[write_pos] = data_[i];
                reverbed         = data_[i] + reverb_buffer_[HMM_MAX(0, read_pos)] * reverb_ratio / 100;
                filter_data_[i] = HMM_Clamp(INT16_MIN, reverbed, INT16_MAX);
                if (!outdoor_reverb)
                    reverb_buffer_[write_pos] = reverbed;
                write_pos = (write_pos + 1) % (length_ * 2);
                read_pos  = (read_pos + 1) % (length_ * 2);
            }
            current_filter_     = kFilterReverb;
            reverbed_room_size_ = current_room_size;
            if (outdoor_reverb)
                reverb_is_outdoors_ = true;
            else
                reverb_is_outdoors_ = false;
        }
    }
    else // Just use the original buffer - Dasho
    {
        current_filter_ = kFilterNone;
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
