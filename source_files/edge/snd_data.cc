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

#include <vector>

#include "HandmadeMath.h"
#include "epi.h"

SoundData::SoundData()
    : length_(0),
      frequency_(0),
      mode_(0),
      data_left_(nullptr),
      data_right_(nullptr),
      filter_data_left_(nullptr),
      filter_data_right_(nullptr),
      definition_data_(nullptr),
      is_sound_effect_(false),
      current_filter_(kFilterNone),
      reverbed_room_size_(kRoomReverbNone),
      current_ddf_reverb_ratio_(0),
      current_ddf_reverb_delay_(0),
      current_ddf_reverb_type_(0),
      reverb_is_outdoors_(false)
{
}

SoundData::~SoundData() { Free(); }

void SoundData::Free()
{
    length_ = 0;

    if (data_right_ && data_right_ != data_left_) delete[] data_right_;

    if (data_left_) delete[] data_left_;

    data_left_  = nullptr;
    data_right_ = nullptr;

    FreeFilter();
}

void SoundData::FreeFilter()
{
    if (filter_data_right_ && filter_data_right_ != filter_data_left_)
        delete[] filter_data_right_;

    if (filter_data_left_) delete[] filter_data_left_;

    filter_data_left_  = nullptr;
    filter_data_right_ = nullptr;
}

void SoundData::Allocate(int samples, int buf_mode)
{
    // early out when requirements are already met
    if (data_left_ && length_ >= samples && mode_ == buf_mode)
    {
        length_ = samples;  // FIXME: perhaps keep allocated count
        return;
    }

    if (data_left_ || data_right_) { Free(); }

    length_ = samples;
    mode_   = buf_mode;

    switch (buf_mode)
    {
        case kMixMono:
            data_left_  = new int16_t[samples];
            data_right_ = data_left_;
            break;

        case kMixStereo:
            data_left_  = new int16_t[samples];
            data_right_ = new int16_t[samples];
            break;

        case kMixInterleaved:
            data_left_  = new int16_t[samples * 2];
            data_right_ = data_left_;
            break;

        default:
            break;
    }
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
        int *reverb_buffer_L;
        int *reverb_buffer_R;
        int  write_pos    = 0;
        int  read_pos     = 0;
        int  reverb_ratio = 25;
        int  reverb_delay = 100;

        switch (mode_)
        {
            case kMixMono:
                if (!filter_data_left_)
                    filter_data_left_ = new int16_t[length_];
                filter_data_right_ = filter_data_left_;
                reverb_buffer_L    = new int[length_];
                memset(reverb_buffer_L, 0, length_ * sizeof(int));
                read_pos =
                    ((write_pos - reverb_delay * frequency_ / 1000) + length_) %
                    (length_);
                for (int i = 0; i < length_; i++)
                {
                    filter_data_left_[i] = out_L = accum_L >> k;
                    accum_L      = accum_L - out_L + data_left_[i];
                    int reverbed = filter_data_left_[i] +
                                   reverb_buffer_L[HMM_MAX(0, read_pos)] *
                                       reverb_ratio / 100;
                    filter_data_left_[i] =
                        HMM_Clamp(INT16_MIN, reverbed, INT16_MAX);
                    reverb_buffer_L[write_pos] = filter_data_left_[i];
                    write_pos                  = (write_pos + 1) % (length_);
                    read_pos                   = (read_pos + 1) % (length_);
                }
                current_filter_ = kFilterSubmerged;
                delete[] reverb_buffer_L;
                reverb_buffer_L = nullptr;
                break;

            case kMixStereo:
                if (!filter_data_left_)
                    filter_data_left_ = new int16_t[length_];
                if (!filter_data_right_)
                    filter_data_right_ = new int16_t[length_];
                reverb_buffer_L = new int[length_];
                reverb_buffer_R = new int[length_];
                memset(reverb_buffer_L, 0, length_ * sizeof(int));
                memset(reverb_buffer_R, 0, length_ * sizeof(int));
                read_pos =
                    ((write_pos - reverb_delay * frequency_ / 1000) + length_) %
                    (length_);
                for (int i = 0; i < length_; i++)
                {
                    filter_data_left_[i] = out_L = accum_L >> k;
                    accum_L               = accum_L - out_L + data_left_[i];
                    filter_data_right_[i] = out_R = accum_R >> k;
                    accum_R        = accum_R - out_R + data_right_[i];
                    int reverbed_L = filter_data_left_[i] +
                                     reverb_buffer_L[HMM_MAX(0, read_pos)] *
                                         reverb_ratio / 100;
                    int reverbed_R = filter_data_right_[i] +
                                     reverb_buffer_R[HMM_MAX(0, read_pos)] *
                                         reverb_ratio / 100;
                    filter_data_left_[i] =
                        HMM_Clamp(INT16_MIN, reverbed_L, INT16_MAX);
                    filter_data_right_[i] =
                        HMM_Clamp(INT16_MIN, reverbed_R, INT16_MAX);
                    reverb_buffer_L[write_pos] = filter_data_left_[i];
                    reverb_buffer_R[write_pos] = filter_data_right_[i];
                    write_pos                  = (write_pos + 1) % (length_);
                    read_pos                   = (read_pos + 1) % (length_);
                }
                current_filter_ = kFilterSubmerged;
                delete[] reverb_buffer_L;
                delete[] reverb_buffer_R;
                reverb_buffer_L = nullptr;
                reverb_buffer_R = nullptr;
                break;

            case kMixInterleaved:
                if (!filter_data_left_)
                    filter_data_left_ = new int16_t[length_ * 2];
                filter_data_right_ = filter_data_left_;
                reverb_buffer_L    = new int[length_ * 2];
                memset(reverb_buffer_L, 0, length_ * sizeof(int) * 2);
                read_pos =
                    ((write_pos - reverb_delay * frequency_ / 1000) + length_) %
                    (length_ * 2);
                for (int i = 0; i < length_ * 2; i++)
                {
                    filter_data_left_[i] = out_L = accum_L >> k;
                    accum_L      = accum_L - out_L + data_left_[i];
                    int reverbed = filter_data_left_[i] +
                                   reverb_buffer_L[HMM_MAX(0, read_pos)] *
                                       reverb_ratio / 100;
                    filter_data_left_[i] =
                        HMM_Clamp(INT16_MIN, reverbed, INT16_MAX);
                    reverb_buffer_L[write_pos] = filter_data_left_[i];
                    write_pos = (write_pos + 1) % (length_ * 2);
                    read_pos  = (read_pos + 1) % (length_ * 2);
                }
                current_filter_ = kFilterSubmerged;
                delete[] reverb_buffer_L;
                reverb_buffer_L = nullptr;
                break;
        }
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

        switch (mode_)
        {
            case kMixMono:
                if (!filter_data_left_)
                    filter_data_left_ = new int16_t[length_];
                filter_data_right_ = filter_data_left_;
                for (int i = 0; i < length_; i++)
                {
                    filter_data_left_[i] = out_L = accum_L >> k;
                    accum_L = accum_L - out_L + data_left_[i];
                }
                current_filter_ = kFilterVacuum;
                break;

            case kMixStereo:
                if (!filter_data_left_)
                    filter_data_left_ = new int16_t[length_];
                if (!filter_data_right_)
                    filter_data_right_ = new int16_t[length_];
                for (int i = 0; i < length_; i++)
                {
                    filter_data_left_[i] = out_L = accum_L >> k;
                    accum_L               = accum_L - out_L + data_left_[i];
                    filter_data_right_[i] = out_R = accum_R >> k;
                    accum_R = accum_R - out_R + data_right_[i];
                }
                current_filter_ = kFilterVacuum;
                break;

            case kMixInterleaved:
                if (!filter_data_left_)
                    filter_data_left_ = new int16_t[length_ * 2];
                filter_data_right_ = filter_data_left_;
                for (int i = 0; i < length_ * 2; i++)
                {
                    filter_data_left_[i] = out_L = accum_L >> k;
                    accum_L = accum_L - out_L + data_left_[i];
                }
                current_filter_ = kFilterVacuum;
                break;
        }
    }
}

void SoundData::MixReverb(bool dynamic_reverb, float room_area,
                          bool outdoor_reverb, int ddf_reverb_type,
                          int ddf_reverb_ratio, int ddf_reverb_delay)
{
    if (ddf_reverb_ratio > 0 && ddf_reverb_delay > 0 && ddf_reverb_type > 0)
    {
        if (current_filter_ != kFilterReverb ||
            ddf_reverb_ratio != current_ddf_reverb_ratio_ ||
            ddf_reverb_delay != current_ddf_reverb_delay_ ||
            ddf_reverb_type != current_ddf_reverb_type_)
        {
            // Setup reverb parameters
            int *reverb_buffer_L;
            int *reverb_buffer_R;
            int  write_pos    = 0;
            int  read_pos     = 0;
            int  reverb_ratio = ddf_reverb_ratio;
            int  reverb_delay = ddf_reverb_delay;
            switch (mode_)
            {
                case kMixMono:
                    if (!filter_data_left_)
                        filter_data_left_ = new int16_t[length_];
                    filter_data_right_ = filter_data_left_;
                    reverb_buffer_L    = new int[length_];
                    memset(reverb_buffer_L, 0, length_ * sizeof(int));
                    read_pos = ((write_pos - reverb_delay * frequency_ / 1000) +
                                length_) %
                               (length_);
                    for (int i = 0; i < length_; i++)
                    {
                        if (ddf_reverb_type == 2)
                            reverb_buffer_L[write_pos] = data_left_[i];
                        int reverbed = data_left_[i] +
                                       reverb_buffer_L[HMM_MAX(0, read_pos)] *
                                           reverb_ratio / 100;
                        filter_data_left_[i] =
                            HMM_Clamp(INT16_MIN, reverbed, INT16_MAX);
                        if (ddf_reverb_type == 1)
                            reverb_buffer_L[write_pos] = reverbed;
                        write_pos = (write_pos + 1) % (length_);
                        read_pos  = (read_pos + 1) % (length_);
                    }
                    current_filter_           = kFilterReverb;
                    current_ddf_reverb_delay_ = ddf_reverb_delay;
                    current_ddf_reverb_ratio_ = ddf_reverb_ratio;
                    current_ddf_reverb_type_  = ddf_reverb_type;
                    reverbed_room_size_       = kRoomReverbNone;
                    delete[] reverb_buffer_L;
                    reverb_buffer_L = nullptr;
                    break;

                case kMixStereo:
                    if (!filter_data_left_)
                        filter_data_left_ = new int16_t[length_];
                    if (!filter_data_right_)
                        filter_data_right_ = new int16_t[length_];
                    reverb_buffer_L = new int[length_];
                    reverb_buffer_R = new int[length_];
                    memset(reverb_buffer_L, 0, length_ * sizeof(int));
                    memset(reverb_buffer_R, 0, length_ * sizeof(int));
                    read_pos = ((write_pos - reverb_delay * frequency_ / 1000) +
                                length_) %
                               (length_);
                    for (int i = 0; i < length_; i++)
                    {
                        if (ddf_reverb_type == 2)
                        {
                            reverb_buffer_L[write_pos] = data_left_[i];
                            reverb_buffer_R[write_pos] = data_right_[i];
                        }
                        int reverbed_L = data_left_[i] +
                                         reverb_buffer_L[HMM_MAX(0, read_pos)] *
                                             reverb_ratio / 100;
                        int reverbed_R = data_right_[i] +
                                         reverb_buffer_R[HMM_MAX(0, read_pos)] *
                                             reverb_ratio / 100;
                        filter_data_left_[i] =
                            HMM_Clamp(INT16_MIN, reverbed_L, INT16_MAX);
                        filter_data_right_[i] =
                            HMM_Clamp(INT16_MIN, reverbed_R, INT16_MAX);
                        if (ddf_reverb_type == 1)
                        {
                            reverb_buffer_L[write_pos] = reverbed_L;
                            reverb_buffer_R[write_pos] = reverbed_R;
                        }
                        write_pos = (write_pos + 1) % (length_);
                        read_pos  = (read_pos + 1) % (length_);
                    }
                    current_filter_           = kFilterReverb;
                    current_ddf_reverb_delay_ = ddf_reverb_delay;
                    current_ddf_reverb_ratio_ = ddf_reverb_ratio;
                    current_ddf_reverb_type_  = ddf_reverb_type;
                    reverbed_room_size_       = kRoomReverbNone;
                    delete[] reverb_buffer_L;
                    delete[] reverb_buffer_R;
                    reverb_buffer_L = nullptr;
                    reverb_buffer_R = nullptr;
                    break;

                case kMixInterleaved:
                    if (!filter_data_left_)
                        filter_data_left_ = new int16_t[length_ * 2];
                    filter_data_right_ = filter_data_left_;
                    reverb_buffer_L    = new int[length_ * 2];
                    memset(reverb_buffer_L, 0, length_ * sizeof(int) * 2);
                    read_pos = ((write_pos - reverb_delay * frequency_ / 1000) +
                                length_ * 2) %
                               (length_ * 2);
                    for (int i = 0; i < length_ * 2; i++)
                    {
                        if (ddf_reverb_type == 2)
                            reverb_buffer_L[write_pos] = data_left_[i];
                        int reverbed = data_left_[i] +
                                       reverb_buffer_L[HMM_MAX(0, read_pos)] *
                                           reverb_ratio / 100;
                        filter_data_left_[i] =
                            HMM_Clamp(INT16_MIN, reverbed, INT16_MAX);
                        if (ddf_reverb_type == 1)
                            reverb_buffer_L[write_pos] = reverbed;
                        write_pos = (write_pos + 1) % (length_ * 2);
                        read_pos  = (read_pos + 1) % (length_ * 2);
                    }
                    current_filter_           = kFilterReverb;
                    current_ddf_reverb_delay_ = ddf_reverb_delay;
                    current_ddf_reverb_ratio_ = ddf_reverb_ratio;
                    current_ddf_reverb_type_  = ddf_reverb_type;
                    reverbed_room_size_       = kRoomReverbNone;
                    delete[] reverb_buffer_L;
                    reverb_buffer_L = nullptr;
                    break;
            }
        }
    }
    else if (dynamic_reverb)
    {
        ReverbRoomSize current_room_size;
        if (room_area > 700) { current_room_size = kRoomReverbLarge; }
        else if (room_area > 350) { current_room_size = kRoomReverbMedium; }
        else { current_room_size = kRoomReverbSmall; }

        if (current_filter_ != kFilterReverb ||
            reverbed_room_size_ != current_room_size ||
            reverb_is_outdoors_ != outdoor_reverb)
        {
            // Setup reverb parameters
            int *reverb_buffer_L;
            int *reverb_buffer_R;
            int  write_pos    = 0;
            int  read_pos     = 0;
            int  reverb_ratio = outdoor_reverb ? 25 : 30;
            int  reverb_delay;
            if (outdoor_reverb)
                reverb_delay = 50 * current_room_size + 25;
            else
                reverb_delay = 20 * current_room_size + 10;
            switch (mode_)
            {
                case kMixMono:
                    if (!filter_data_left_)
                        filter_data_left_ = new int16_t[length_];
                    filter_data_right_ = filter_data_left_;
                    reverb_buffer_L    = new int[length_];
                    memset(reverb_buffer_L, 0, length_ * sizeof(int));
                    read_pos = ((write_pos - reverb_delay * frequency_ / 1000) +
                                length_) %
                               (length_);
                    for (int i = 0; i < length_; i++)
                    {
                        if (outdoor_reverb)
                            reverb_buffer_L[write_pos] = data_left_[i];
                        int reverbed = data_left_[i] +
                                       reverb_buffer_L[HMM_MAX(0, read_pos)] *
                                           reverb_ratio / 100;
                        filter_data_left_[i] =
                            HMM_Clamp(INT16_MIN, reverbed, INT16_MAX);
                        if (!outdoor_reverb)
                            reverb_buffer_L[write_pos] = reverbed;
                        write_pos = (write_pos + 1) % (length_);
                        read_pos  = (read_pos + 1) % (length_);
                    }
                    current_filter_     = kFilterReverb;
                    reverbed_room_size_ = current_room_size;
                    if (outdoor_reverb)
                        reverb_is_outdoors_ = true;
                    else
                        reverb_is_outdoors_ = false;
                    delete[] reverb_buffer_L;
                    reverb_buffer_L = nullptr;
                    break;

                case kMixStereo:
                    if (!filter_data_left_)
                        filter_data_left_ = new int16_t[length_];
                    if (!filter_data_right_)
                        filter_data_right_ = new int16_t[length_];
                    reverb_buffer_L = new int[length_];
                    reverb_buffer_R = new int[length_];
                    memset(reverb_buffer_L, 0, length_ * sizeof(int));
                    memset(reverb_buffer_R, 0, length_ * sizeof(int));
                    read_pos = ((write_pos - reverb_delay * frequency_ / 1000) +
                                length_) %
                               (length_);
                    for (int i = 0; i < length_; i++)
                    {
                        if (outdoor_reverb)
                        {
                            reverb_buffer_L[write_pos] = data_left_[i];
                            reverb_buffer_R[write_pos] = data_right_[i];
                        }
                        int reverbed_L = data_left_[i] +
                                         reverb_buffer_L[HMM_MAX(0, read_pos)] *
                                             reverb_ratio / 100;
                        int reverbed_R = data_right_[i] +
                                         reverb_buffer_R[HMM_MAX(0, read_pos)] *
                                             reverb_ratio / 100;
                        filter_data_left_[i] =
                            HMM_Clamp(INT16_MIN, reverbed_L, INT16_MAX);
                        filter_data_right_[i] =
                            HMM_Clamp(INT16_MIN, reverbed_R, INT16_MAX);
                        if (!outdoor_reverb)
                        {
                            reverb_buffer_L[write_pos] = reverbed_L;
                            reverb_buffer_R[write_pos] = reverbed_R;
                        }
                        write_pos = (write_pos + 1) % (length_);
                        read_pos  = (read_pos + 1) % (length_);
                    }
                    current_filter_     = kFilterReverb;
                    reverbed_room_size_ = current_room_size;
                    if (outdoor_reverb)
                        reverb_is_outdoors_ = true;
                    else
                        reverb_is_outdoors_ = false;
                    delete[] reverb_buffer_L;
                    delete[] reverb_buffer_R;
                    reverb_buffer_L = nullptr;
                    reverb_buffer_R = nullptr;
                    break;

                case kMixInterleaved:
                    if (!filter_data_left_)
                        filter_data_left_ = new int16_t[length_ * 2];
                    filter_data_right_ = filter_data_left_;
                    reverb_buffer_L    = new int[length_ * 2];
                    memset(reverb_buffer_L, 0, length_ * sizeof(int) * 2);
                    read_pos = ((write_pos - reverb_delay * frequency_ / 1000) +
                                length_ * 2) %
                               (length_ * 2);
                    for (int i = 0; i < length_ * 2; i++)
                    {
                        if (outdoor_reverb)
                            reverb_buffer_L[write_pos] = data_left_[i];
                        int reverbed = data_left_[i] +
                                       reverb_buffer_L[HMM_MAX(0, read_pos)] *
                                           reverb_ratio / 100;
                        filter_data_left_[i] =
                            HMM_Clamp(INT16_MIN, reverbed, INT16_MAX);
                        if (!outdoor_reverb)
                            reverb_buffer_L[write_pos] = reverbed;
                        write_pos = (write_pos + 1) % (length_ * 2);
                        read_pos  = (read_pos + 1) % (length_ * 2);
                    }
                    current_filter_     = kFilterReverb;
                    reverbed_room_size_ = current_room_size;
                    if (outdoor_reverb)
                        reverb_is_outdoors_ = true;
                    else
                        reverb_is_outdoors_ = false;
                    delete[] reverb_buffer_L;
                    reverb_buffer_L = nullptr;
                    break;
            }
        }
    }
    else  // Just use the original buffer - Dasho
    {
        current_filter_ = kFilterNone;
    }
}

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
