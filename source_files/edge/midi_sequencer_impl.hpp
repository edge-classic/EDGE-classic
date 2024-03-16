/*
 * BW_Midi_Sequencer - MIDI Sequencer for C++
 *
 * Copyright (c) 2015-2022 Vitaly Novichkov <admin@wohlnet.ru>
 * Copyright (c) 2024 The EDGE Team.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
 * OR OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <algorithm> // std::copy
#include <iterator>  // std::back_inserter
#include <memory>
#include <set>

#include "midi_convert_mus.hpp"
#include "midi_convert_xmi.hpp"
#include "midi_sequencer.hpp"

/**
 * @brief Utility function to read Big-Endian integer from raw binary data
 * @param buffer Pointer to raw binary buffer
 * @param nbytes Count of bytes to parse integer
 * @return Extracted unsigned integer
 */
static inline uint64_t ReadIntBigEndian(const void *buffer, size_t nbytes)
{
    uint64_t       result = 0;
    const uint8_t *data   = (const uint8_t *)(buffer);

    for (size_t n = 0; n < nbytes; ++n)
        result = (result << 8) + data[n];

    return result;
}

/**
 * @brief Utility function to read Little-Endian integer from raw binary data
 * @param buffer Pointer to raw binary buffer
 * @param nbytes Count of bytes to parse integer
 * @return Extracted unsigned integer
 */
static inline uint64_t ReadIntLittleEndian(const void *buffer, size_t nbytes)
{
    uint64_t       result = 0;
    const uint8_t *data   = (const uint8_t *)(buffer);

    for (size_t n = 0; n < nbytes; ++n)
        result = result + (uint64_t)(data[n] << (n * 8));

    return result;
}

/**
 * @brief Secure Standard MIDI Variable-Length numeric value parser with
 * anti-out-of-range protection
 * @param [_inout] ptr Pointer to memory block that contains begin of
 * variable-length value, will be iterated forward
 * @param [_in end Pointer to end of memory block where variable-length value is
 * stored (after end of track)
 * @param [_out] ok Reference to boolean which takes result of variable-length
 * value parsing
 * @return Unsigned integer that conains parsed variable-length value
 */
static inline uint64_t ReadVariableLengthValue(const uint8_t **ptr, const uint8_t *end, bool &ok)
{
    uint64_t result = 0;
    ok              = false;

    for (;;)
    {
        if (*ptr >= end)
            return 2;
        unsigned char byte = *((*ptr)++);
        result             = (result << 7) + (byte & 0x7F);
        if (!(byte & 0x80))
            break;
    }

    ok = true;
    return result;
}

MidiSequencer::MidiTrackRow::MidiTrackRow() : time_(0.0), delay_(0), absolute_position_(0), time_delay_(0.0)
{
}

void MidiSequencer::MidiTrackRow::Clear()
{
    time_              = 0.0;
    delay_             = 0;
    absolute_position_ = 0;
    time_delay_        = 0.0;
    events_.clear();
}

void MidiSequencer::MidiTrackRow::SortEvents(bool *note_states)
{
    typedef std::vector<MidiEvent> EvtArr;
    EvtArr                         sysEx;
    EvtArr                         metas;
    EvtArr                         noteOffs;
    EvtArr                         controllers;
    EvtArr                         anyOther;

    for (size_t i = 0; i < events_.size(); i++)
    {
        if (events_[i].type == MidiEvent::kNoteOff)
        {
            if (noteOffs.capacity() == 0)
                noteOffs.reserve(events_.size());
            noteOffs.push_back(events_[i]);
        }
        else if (events_[i].type == MidiEvent::kSysex || events_[i].type == MidiEvent::kSysex2)
        {
            if (sysEx.capacity() == 0)
                sysEx.reserve(events_.size());
            sysEx.push_back(events_[i]);
        }
        else if ((events_[i].type == MidiEvent::kControlChange) || (events_[i].type == MidiEvent::kPatchChange) ||
                 (events_[i].type == MidiEvent::kPitchWheel) || (events_[i].type == MidiEvent::kChannelAftertouch))
        {
            if (controllers.capacity() == 0)
                controllers.reserve(events_.size());
            controllers.push_back(events_[i]);
        }
        else if ((events_[i].type == MidiEvent::kSpecial) &&
                 ((events_[i].sub_type == MidiEvent::kMarker) || (events_[i].sub_type == MidiEvent::kDeviceSwitch) ||
                  (events_[i].sub_type == MidiEvent::kSongBeginHook) ||
                  (events_[i].sub_type == MidiEvent::kLoopStart) || (events_[i].sub_type == MidiEvent::kLoopEnd) ||
                  (events_[i].sub_type == MidiEvent::kLoopStackBegin) ||
                  (events_[i].sub_type == MidiEvent::kLoopStackEnd) ||
                  (events_[i].sub_type == MidiEvent::kLoopStackBreak)))
        {
            if (metas.capacity() == 0)
                metas.reserve(events_.size());
            metas.push_back(events_[i]);
        }
        else
        {
            if (anyOther.capacity() == 0)
                anyOther.reserve(events_.size());
            anyOther.push_back(events_[i]);
        }
    }

    /*
     * If Note-Off and it's Note-On is on the same row - move this damned note
     * off down!
     */
    if (note_states)
    {
        std::set<size_t> markAsOn;
        for (size_t i = 0; i < anyOther.size(); i++)
        {
            const MidiEvent e = anyOther[i];
            if (e.type == MidiEvent::kNoteOn)
            {
                const size_t note_i = (size_t)(e.channel * 255) + (e.data[0] & 0x7F);
                // Check, was previously note is on or off
                bool wasOn = note_states[note_i];
                markAsOn.insert(note_i);
                // Detect zero-length notes are following previously pressed
                // note
                int noteOffsOnSameNote = 0;
                for (EvtArr::iterator j = noteOffs.begin(); j != noteOffs.end();)
                {
                    // If note was off, and note-off on same row with note-on -
                    // move it down!
                    if (((*j).channel == e.channel) && ((*j).data[0] == e.data[0]))
                    {
                        // If note is already off OR more than one note-off on
                        // same row and same note
                        if (!wasOn || (noteOffsOnSameNote != 0))
                        {
                            anyOther.push_back(*j);
                            j = noteOffs.erase(j);
                            markAsOn.erase(note_i);
                            continue;
                        }
                        else
                        {
                            // When same row has many note-offs on same row
                            // that means a zero-length note follows previous
                            // note it must be shuted down
                            noteOffsOnSameNote++;
                        }
                    }
                    j++;
                }
            }
        }

        // Mark other notes as released
        for (EvtArr::iterator j = noteOffs.begin(); j != noteOffs.end(); j++)
        {
            size_t note_i       = (size_t)(j->channel * 255) + (j->data[0] & 0x7F);
            note_states[note_i] = false;
        }

        for (std::set<size_t>::iterator j = markAsOn.begin(); j != markAsOn.end(); j++)
            note_states[*j] = true;
    }
    /***********************************************************************************/

    events_.clear();
    if (!sysEx.empty())
        events_.insert(events_.end(), sysEx.begin(), sysEx.end());
    if (!noteOffs.empty())
        events_.insert(events_.end(), noteOffs.begin(), noteOffs.end());
    if (!metas.empty())
        events_.insert(events_.end(), metas.begin(), metas.end());
    if (!controllers.empty())
        events_.insert(events_.end(), controllers.begin(), controllers.end());
    if (!anyOther.empty())
        events_.insert(events_.end(), anyOther.begin(), anyOther.end());
}

MidiSequencer::MidiSequencer()
    : midi_output_interface_(nullptr), midi_format_(kFormatMidi), midi_smf_format_(0), midi_loop_format_(kLoopDefault),
      midi_loop_enabled_(false), midi_loop_hooks_only_(false), midi_full_song_time_length_(0.0),
      midi_post_song_wait_delay_(1.0), midi_loop_start_time_(-1.0), midi_loop_end_time_(-1.0),
      midi_tempo_multiplier_(1.0), midi_at_end_(false), midi_loop_count_(-1), midi_load_track_number_(0),
      midi_track_solo_(~(size_t)(0)), midi_trigger_handler_(nullptr), midi_trigger_userdata_(nullptr)
{
    midi_loop_.Reset();
    midi_loop_.invalid_loop_ = false;
    midi_time_.Init();
}

MidiSequencer::~MidiSequencer()
{
}

void MidiSequencer::SetInterface(const MidiRealTimeInterface *intrf)
{
    // Interface must NOT be nullptr
    assert(intrf);

    // Note ON hook is REQUIRED
    assert(intrf->rt_noteOn);
    // Note OFF hook is REQUIRED
    assert(intrf->rt_noteOff || intrf->rt_noteOffVel);
    // Note Aftertouch hook is REQUIRED
    assert(intrf->rt_noteAfterTouch);
    // Channel Aftertouch hook is REQUIRED
    assert(intrf->rt_channelAfterTouch);
    // Controller change hook is REQUIRED
    assert(intrf->rt_controllerChange);
    // Patch change hook is REQUIRED
    assert(intrf->rt_patchChange);
    // Pitch bend hook is REQUIRED
    assert(intrf->rt_pitchBend);
    // System Exclusive hook is REQUIRED
    assert(intrf->rt_systemExclusive);

    if (intrf->pcmSampleRate != 0 && intrf->pcmFrameSize != 0)
    {
        midi_time_.sample_rate_ = intrf->pcmSampleRate;
        midi_time_.frame_size_  = intrf->pcmFrameSize;
        midi_time_.Reset();
    }

    midi_output_interface_ = intrf;
}

int MidiSequencer::PlayStream(uint8_t *stream, size_t length)
{
    int      count      = 0;
    size_t   samples    = (size_t)(length / (size_t)(midi_time_.frame_size_));
    size_t   left       = samples;
    size_t   periodSize = 0;
    uint8_t *stream_pos = stream;

    assert(midi_output_interface_->onPcmRender);

    while (left > 0)
    {
        const double leftDelay = left / double(midi_time_.sample_rate_);
        const double maxDelay  = midi_time_.time_rest_ < leftDelay ? midi_time_.time_rest_ : leftDelay;
        if ((PositionAtEnd()) && (midi_time_.delay_ <= 0.0))
            break; // Stop to fetch samples at reaching the song end with
                   // disabled loop

        midi_time_.time_rest_ -= maxDelay;
        periodSize = (size_t)((double)(midi_time_.sample_rate_) * maxDelay);

        if (stream)
        {
            size_t generateSize = periodSize > left ? (size_t)(left) : (size_t)(periodSize);
            midi_output_interface_->onPcmRender(midi_output_interface_->onPcmRender_userdata, stream_pos,
                                                generateSize * midi_time_.frame_size_);
            stream_pos += generateSize * midi_time_.frame_size_;
            count += generateSize;
            left -= generateSize;
            assert(left <= samples);
        }

        if (midi_time_.time_rest_ <= 0.0)
        {
            midi_time_.delay_ = Tick(midi_time_.delay_, midi_time_.minimum_delay_);
            midi_time_.time_rest_ += midi_time_.delay_;
        }
    }

    return count * (int)(midi_time_.frame_size_);
}

MidiSequencer::FileFormat MidiSequencer::GetFormat()
{
    return midi_format_;
}

size_t MidiSequencer::GetTrackCount() const
{
    return midi_track_data_.size();
}

bool MidiSequencer::SetTrackEnabled(size_t track, bool enable)
{
    size_t track_count = midi_track_data_.size();
    if (track >= track_count)
        return false;
    midi_track_disabled_[track] = !enable;
    return true;
}

bool MidiSequencer::SetChannelEnabled(size_t channel, bool enable)
{
    if (channel >= 16)
        return false;

    if (!enable && m_channelDisable[channel] != !enable)
    {
        uint8_t ch = (uint8_t)(channel);

        // Releae all pedals
        midi_output_interface_->rt_controllerChange(midi_output_interface_->rtUserData, ch, 64, 0);
        midi_output_interface_->rt_controllerChange(midi_output_interface_->rtUserData, ch, 66, 0);

        // Release all notes on the channel now
        for (int i = 0; i < 127; ++i)
        {
            if (midi_output_interface_->rt_noteOff)
                midi_output_interface_->rt_noteOff(midi_output_interface_->rtUserData, ch, i);
            if (midi_output_interface_->rt_noteOffVel)
                midi_output_interface_->rt_noteOffVel(midi_output_interface_->rtUserData, ch, i, 0);
        }
    }

    m_channelDisable[channel] = !enable;
    return true;
}

void MidiSequencer::SetSoloTrack(size_t track)
{
    midi_track_solo_ = track;
}

void MidiSequencer::SetSongNum(int track)
{
    midi_load_track_number_ = track;

    if (!midi_raw_songs_data_.empty() && midi_format_ == kFormatXMidi) // Reload the song
    {
        if (midi_load_track_number_ >= (int)midi_raw_songs_data_.size())
            midi_load_track_number_ = midi_raw_songs_data_.size() - 1;

        if (midi_output_interface_ && midi_output_interface_->rt_controllerChange)
        {
            for (int i = 0; i < 15; i++)
                midi_output_interface_->rt_controllerChange(midi_output_interface_->rtUserData, i, 123, 0);
        }

        midi_at_end_ = false;
        midi_loop_.FullReset();
        midi_loop_.caught_start_ = true;

        midi_smf_format_ = 0;

        epi::MemFile *mfr = new epi::MemFile(midi_raw_songs_data_[midi_load_track_number_].data(),
                                             midi_raw_songs_data_[midi_load_track_number_].size());
        ParseSmf(mfr);

        midi_format_ = kFormatXMidi;
    }
}

int MidiSequencer::GetSongsCount()
{
    return (int)midi_raw_songs_data_.size();
}

void MidiSequencer::SetTriggerHandler(TriggerHandler handler, void *userdata)
{
    midi_trigger_handler_  = handler;
    midi_trigger_userdata_ = userdata;
}

const std::string &MidiSequencer::GetErrorString()
{
    return midi_error_string_;
}

bool MidiSequencer::GetLoopEnabled()
{
    return midi_loop_enabled_;
}

void MidiSequencer::SetLoopEnabled(bool enabled)
{
    midi_loop_enabled_ = enabled;
}

int MidiSequencer::GetLoopsCount()
{
    return midi_loop_count_ >= 0 ? (midi_loop_count_ + 1) : midi_loop_count_;
}

void MidiSequencer::SetLoopsCount(int loops)
{
    if (loops >= 1)
        loops -= 1; // Internally, loops count has the 0 base
    midi_loop_count_ = loops;
}

void MidiSequencer::SetLoopHooksOnly(bool enabled)
{
    midi_loop_hooks_only_ = enabled;
}

const std::string &MidiSequencer::GetMusicTitle()
{
    return midi_music_title_;
}

const std::string &MidiSequencer::GetMusicCopyright()
{
    return midi_music_copyright_;
}

const std::vector<std::string> &MidiSequencer::GetTrackTitles()
{
    return midi_music_track_titles_;
}

const std::vector<MidiSequencer::MidiMarkerEntry> &MidiSequencer::GetMarkers()
{
    return midi_music_markers_;
}

bool MidiSequencer::PositionAtEnd()
{
    return midi_at_end_;
}

double MidiSequencer::GetTempoMultiplier()
{
    return midi_tempo_multiplier_;
}

void MidiSequencer::BuildSmfSetupReset(size_t track_count)
{
    midi_full_song_time_length_ = 0.0;
    midi_loop_start_time_       = -1.0;
    midi_loop_end_time_         = -1.0;
    midi_loop_format_           = kLoopDefault;
    midi_track_disabled_.clear();
    memset(m_channelDisable, 0, sizeof(m_channelDisable));
    midi_track_solo_ = ~(size_t)0;
    midi_music_title_.clear();
    midi_music_copyright_.clear();
    midi_music_track_titles_.clear();
    midi_music_markers_.clear();
    midi_track_data_.clear();
    midi_track_data_.resize(track_count, std::list<MidiTrackRow>());
    midi_track_disabled_.resize(track_count);

    midi_loop_.Reset();
    midi_loop_.invalid_loop_ = false;
    midi_time_.Reset();

    midi_current_position_.began                  = false;
    midi_current_position_.absolute_time_position = 0.0;
    midi_current_position_.wait                   = 0.0;
    midi_current_position_.track.clear();
    midi_current_position_.track.resize(track_count);
}

bool MidiSequencer::BuildSmfTrackData(const std::vector<std::vector<uint8_t>> &track_data)
{
    const size_t track_count = track_data.size();
    BuildSmfSetupReset(track_count);

    bool gotGlobalLoopStart = false, gotGlobalLoopEnd = false, gotStackLoopStart = false, gotLoopEventInThisRow = false;

    //! Tick position of loop start tag
    uint64_t loop_start_ticks = 0;
    //! Tick position of loop end tag
    uint64_t loop_end_ticks = 0;
    //! Full length of song in ticks
    uint64_t ticksSongLength = 0;
    //! Cache for error message strign
    char error[150];

    //! Caches note on/off states.
    bool note_states[16 * 255];
    /* This is required to carefully detect zero-length notes           *
     * and avoid a move of "note-off" event over "note-on" while sort.  *
     * Otherwise, after sort those notes will play infinite sound       */

    //! Tempo change events list
    std::vector<MidiEvent> temposList;

    /*
     * TODO: Make this be safer for memory in case of broken input data
     * which may cause going away of available track data (and then give a
     * crash!)
     *
     * POST: Check this more carefully for possible vulnuabilities are can crash
     * this
     */
    for (size_t tk = 0; tk < track_count; ++tk)
    {
        uint64_t       abs_position = 0;
        int            status       = 0;
        MidiEvent      event;
        bool           ok       = false;
        const uint8_t *end      = track_data[tk].data() + track_data[tk].size();
        const uint8_t *trackPtr = track_data[tk].data();
        memset(note_states, 0, sizeof(note_states));

        // Time delay that follows the first event in the track
        {
            MidiTrackRow evtPos;
            if (midi_format_ == kFormatRsxx)
                ok = true;
            else
                evtPos.delay_ = ReadVariableLengthValue(&trackPtr, end, ok);
            if (!ok)
            {
                int len = snprintf(error, 150,
                                   "buildTrackData: Can't read variable-length "
                                   "value at begin of track %d.\n",
                                   (int)tk);
                if ((len > 0) && (len < 150))
                    midi_parsing_errors_string_ += std::string(error, (size_t)len);
                return false;
            }

            // HACK: Begin every track with "Reset all controllers" event to
            // avoid controllers state break came from end of song
            if (tk == 0)
            {
                MidiEvent resetEvent;
                resetEvent.type     = MidiEvent::kSpecial;
                resetEvent.sub_type = MidiEvent::kSongBeginHook;
                evtPos.events_.push_back(resetEvent);
            }

            evtPos.absolute_position_ = abs_position;
            abs_position += evtPos.delay_;
            midi_track_data_[tk].push_back(evtPos);
        }

        MidiTrackRow evtPos;
        do
        {
            event = ParseEvent(&trackPtr, end, status);
            if (!event.is_valid)
            {
                int len = snprintf(error, 150, "buildTrackData: Fail to parse event in the track %d.\n", (int)tk);
                if ((len > 0) && (len < 150))
                    midi_parsing_errors_string_ += std::string(error, (size_t)len);
                return false;
            }

            evtPos.events_.push_back(event);
            if (event.type == MidiEvent::kSpecial)
            {
                if (event.sub_type == MidiEvent::kTempoChange)
                {
                    event.absolute_tick_position = abs_position;
                    temposList.push_back(event);
                }
                else if (!midi_loop_.invalid_loop_ && (event.sub_type == MidiEvent::kLoopStart))
                {
                    /*
                     * loopStart is invalid when:
                     * - starts together with loopEnd
                     * - appears more than one time in same MIDI file
                     */
                    if (gotGlobalLoopStart || gotLoopEventInThisRow)
                        midi_loop_.invalid_loop_ = true;
                    else
                    {
                        gotGlobalLoopStart = true;
                        loop_start_ticks   = abs_position;
                    }
                    // In this row we got loop event, register this!
                    gotLoopEventInThisRow = true;
                }
                else if (!midi_loop_.invalid_loop_ && (event.sub_type == MidiEvent::kLoopEnd))
                {
                    /*
                     * loopEnd is invalid when:
                     * - starts before loopStart
                     * - starts together with loopStart
                     * - appars more than one time in same MIDI file
                     */
                    if (gotGlobalLoopEnd || gotLoopEventInThisRow)
                    {
                        midi_loop_.invalid_loop_ = true;
                        if (midi_output_interface_->onDebugMessage)
                        {
                            midi_output_interface_->onDebugMessage(
                                midi_output_interface_->onDebugMessage_userdata, "== Invalid loop detected! %s %s ==",
                                (gotGlobalLoopEnd ? "[Caught more than 1 loopEnd!]" : ""),
                                (gotLoopEventInThisRow ? "[loopEnd in same row as loopStart!]" : ""));
                        }
                    }
                    else
                    {
                        gotGlobalLoopEnd = true;
                        loop_end_ticks   = abs_position;
                    }
                    // In this row we got loop event, register this!
                    gotLoopEventInThisRow = true;
                }
                else if (!midi_loop_.invalid_loop_ && (event.sub_type == MidiEvent::kLoopStackBegin))
                {
                    if (!gotStackLoopStart)
                    {
                        if (!gotGlobalLoopStart)
                            loop_start_ticks = abs_position;
                        gotStackLoopStart = true;
                    }

                    midi_loop_.StackUp();
                    if (midi_loop_.stack_level_ >= (int)(midi_loop_.stack_.size()))
                    {
                        LoopStackEntry e;
                        e.loops    = event.data[0];
                        e.infinity = (event.data[0] == 0);
                        e.start    = abs_position;
                        e.end      = abs_position;
                        midi_loop_.stack_.push_back(e);
                    }
                }
                else if (!midi_loop_.invalid_loop_ && ((event.sub_type == MidiEvent::kLoopStackEnd) ||
                                                       (event.sub_type == MidiEvent::kLoopStackBreak)))
                {
                    if (midi_loop_.stack_level_ <= -1)
                    {
                        midi_loop_.invalid_loop_ = true; // Caught loop end without of loop start!
                        if (midi_output_interface_->onDebugMessage)
                        {
                            midi_output_interface_->onDebugMessage(midi_output_interface_->onDebugMessage_userdata,
                                                                   "== Invalid loop detected! [Caught loop end "
                                                                   "without of loop start] ==");
                        }
                    }
                    else
                    {
                        if (loop_end_ticks < abs_position)
                            loop_end_ticks = abs_position;
                        midi_loop_.GetCurrentStack().end = abs_position;
                        midi_loop_.StackDown();
                    }
                }
            }

            if (event.sub_type != MidiEvent::kEndTrack) // Don't try to read delta after
                                                        // EndOfTrack event!
            {
                evtPos.delay_ = ReadVariableLengthValue(&trackPtr, end, ok);
                if (!ok)
                {
                    /* End of track has been reached! However, there is no EOT
                     * event presented */
                    event.type     = MidiEvent::kSpecial;
                    event.sub_type = MidiEvent::kEndTrack;
                }
            }

            if ((evtPos.delay_ > 0) || (event.sub_type == MidiEvent::kEndTrack))
            {
                evtPos.absolute_position_ = abs_position;
                abs_position += evtPos.delay_;
                evtPos.SortEvents(note_states);
                midi_track_data_[tk].push_back(evtPos);
                evtPos.Clear();
                gotLoopEventInThisRow = false;
            }
        } while ((trackPtr <= end) && (event.sub_type != MidiEvent::kEndTrack));

        if (ticksSongLength < abs_position)
            ticksSongLength = abs_position;
        // Set the chain of events begin
        if (midi_track_data_[tk].size() > 0)
            midi_current_position_.track[tk].pos = midi_track_data_[tk].begin();
    }

    if (gotGlobalLoopStart && !gotGlobalLoopEnd)
    {
        gotGlobalLoopEnd = true;
        loop_end_ticks   = ticksSongLength;
    }

    // loopStart must be located before loopEnd!
    if (loop_start_ticks >= loop_end_ticks)
    {
        midi_loop_.invalid_loop_ = true;
        if (midi_output_interface_->onDebugMessage && (gotGlobalLoopStart || gotGlobalLoopEnd))
        {
            midi_output_interface_->onDebugMessage(midi_output_interface_->onDebugMessage_userdata,
                                                   "== Invalid loop detected! [loopEnd is "
                                                   "going before loopStart] ==");
        }
    }

    BuildTimeLine(temposList, loop_start_ticks, loop_end_ticks);

    return true;
}

void MidiSequencer::BuildTimeLine(const std::vector<MidiEvent> &tempos, uint64_t loop_start_ticks,
                                  uint64_t loop_end_ticks)
{
    const size_t track_count = midi_track_data_.size();
    /********************************************************************************/
    // Calculate time basing on collected tempo events
    /********************************************************************************/
    for (size_t tk = 0; tk < track_count; ++tk)
    {
        MidiFraction             currentTempo       = midi_tempo_;
        double                   time               = 0.0;
        size_t                   tempo_change_index = 0;
        std::list<MidiTrackRow> &track              = midi_track_data_[tk];
        if (track.empty())
            continue;                                // Empty track is useless!

        MidiTrackRow *posPrev = &(*(track.begin())); // First element
        for (std::list<MidiTrackRow>::iterator it = track.begin(); it != track.end(); it++)
        {
            MidiTrackRow &pos = *it;
            if ((posPrev != &pos) && // Skip first event
                (!tempos.empty()) && // Only when in-track tempo events are
                                     // available
                (tempo_change_index < tempos.size()))
            {
                // If tempo event is going between of current and previous event
                if (tempos[tempo_change_index].absolute_tick_position <= pos.absolute_position_)
                {
                    // Stop points: begin point and tempo change points are
                    // before end point
                    std::vector<TempoChangePoint> points;
                    MidiFraction                  t;
                    TempoChangePoint              firstPoint = { posPrev->absolute_position_, currentTempo };
                    points.push_back(firstPoint);

                    // Collect tempo change points between previous and current
                    // events
                    do
                    {
                        TempoChangePoint tempoMarker;
                        const MidiEvent &tempoPoint   = tempos[tempo_change_index];
                        tempoMarker.absolute_position = tempoPoint.absolute_tick_position;
                        tempoMarker.tempo =
                            midi_individual_tick_delta_ *
                            MidiFraction(ReadIntBigEndian(tempoPoint.data.data(), tempoPoint.data.size()));
                        points.push_back(tempoMarker);
                        tempo_change_index++;
                    } while ((tempo_change_index < tempos.size()) &&
                             (tempos[tempo_change_index].absolute_tick_position <= pos.absolute_position_));

                    // Re-calculate time delay of previous event
                    time -= posPrev->time_delay_;
                    posPrev->time_delay_ = 0.0;

                    for (size_t i = 0, j = 1; j < points.size(); i++, j++)
                    {
                        /* If one or more tempo events are appears between of
                         * two events, calculate delays between each tempo
                         * point, begin and end */
                        uint64_t midDelay = 0;
                        // Delay between points
                        midDelay = points[j].absolute_position - points[i].absolute_position;
                        // Time delay between points
                        t = midDelay * currentTempo;
                        posPrev->time_delay_ += t.Value();

                        // Apply next tempo
                        currentTempo = points[j].tempo;
                    }
                    // Then calculate time between last tempo change point and
                    // end point
                    TempoChangePoint tailTempo = points.back();
                    uint64_t         postDelay = pos.absolute_position_ - tailTempo.absolute_position;
                    t                          = postDelay * currentTempo;
                    posPrev->time_delay_ += t.Value();

                    // Store Common time delay
                    posPrev->time_ = time;
                    time += posPrev->time_delay_;
                }
            }

            MidiFraction t  = pos.delay_ * currentTempo;
            pos.time_delay_ = t.Value();
            pos.time_       = time;
            time += pos.time_delay_;

            // Capture markers after time value calculation
            for (size_t i = 0; i < pos.events_.size(); i++)
            {
                MidiEvent &e = pos.events_[i];
                if ((e.type == MidiEvent::kSpecial) && (e.sub_type == MidiEvent::kMarker))
                {
                    MidiMarkerEntry marker;
                    marker.label          = std::string((char *)e.data.data(), e.data.size());
                    marker.position_ticks = pos.absolute_position_;
                    marker.position_time  = pos.time_;
                    midi_music_markers_.push_back(marker);
                }
            }

            // Capture loop points time positions
            if (!midi_loop_.invalid_loop_)
            {
                // Set loop points times
                if (loop_start_ticks == pos.absolute_position_)
                    midi_loop_start_time_ = pos.time_;
                else if (loop_end_ticks == pos.absolute_position_)
                    midi_loop_end_time_ = pos.time_;
            }
            posPrev = &pos;
        }

        if (time > midi_full_song_time_length_)
            midi_full_song_time_length_ = time;
    }

    midi_full_song_time_length_ += midi_post_song_wait_delay_;
    // Set begin of the music
    midi_track_begin_position_ = midi_current_position_;
    // Initial loop position will begin at begin of track until passing of the
    // loop point
    midi_loop_begin_position_ = midi_current_position_;
    // Set lowest level of the loop stack
    midi_loop_.stack_level_ = -1;

    // Set the count of loops
    midi_loop_.loops_count_ = midi_loop_count_;
    midi_loop_.loops_left_  = midi_loop_count_;

    /********************************************************************************/
    // Find and set proper loop points
    /********************************************************************************/
    if (!midi_loop_.invalid_loop_ && !midi_current_position_.track.empty())
    {
        unsigned     caughLoopStart = 0;
        bool         scanDone       = false;
        const size_t ctrack_count   = midi_current_position_.track.size();
        Position     rowPosition(midi_current_position_);

        while (!scanDone)
        {
            const Position rowBeginPosition(rowPosition);

            for (size_t tk = 0; tk < ctrack_count; ++tk)
            {
                Position::TrackInfo &track = rowPosition.track[tk];
                if ((track.lastHandledEvent >= 0) && (track.delay <= 0))
                {
                    // Check is an end of track has been reached
                    if (track.pos == midi_track_data_[tk].end())
                    {
                        track.lastHandledEvent = -1;
                        continue;
                    }

                    for (size_t i = 0; i < track.pos->events_.size(); i++)
                    {
                        const MidiEvent &evt = track.pos->events_[i];
                        if (evt.type == MidiEvent::kSpecial && evt.sub_type == MidiEvent::kLoopStart)
                        {
                            caughLoopStart++;
                            scanDone = true;
                            break;
                        }
                    }

                    if (track.lastHandledEvent >= 0)
                    {
                        track.delay += track.pos->delay_;
                        track.pos++;
                    }
                }
            }

            // Find a shortest delay from all track
            uint64_t shortestDelay         = 0;
            bool     shortestDelayNotFound = true;

            for (size_t tk = 0; tk < ctrack_count; ++tk)
            {
                Position::TrackInfo &track = rowPosition.track[tk];
                if ((track.lastHandledEvent >= 0) && (shortestDelayNotFound || track.delay < shortestDelay))
                {
                    shortestDelay         = track.delay;
                    shortestDelayNotFound = false;
                }
            }

            // Schedule the next playevent to be processed after that delay
            for (size_t tk = 0; tk < ctrack_count; ++tk)
                rowPosition.track[tk].delay -= shortestDelay;

            if (caughLoopStart > 0)
            {
                midi_loop_begin_position_                        = rowBeginPosition;
                midi_loop_begin_position_.absolute_time_position = midi_loop_start_time_;
                scanDone                                         = true;
            }

            if (shortestDelayNotFound)
                break;
        }
    }
}

bool MidiSequencer::ProcessEvents(bool is_seek)
{
    if (midi_current_position_.track.size() == 0)
        midi_at_end_ = true; // No MIDI track data to play
    if (midi_at_end_)
        return false;        // No more events in the queue

    midi_loop_.caught_end_     = false;
    const size_t   track_count = midi_current_position_.track.size();
    const Position rowBeginPosition(midi_current_position_);
    bool           doLoopJump             = false;
    unsigned       caughLoopStart         = 0;
    unsigned       caughLoopStackStart    = 0;
    unsigned       caughLoopStackEnds     = 0;
    double         caughLoopStackEndsTime = 0.0;
    unsigned       caughLoopStackBreaks   = 0;

    for (size_t tk = 0; tk < track_count; ++tk)
    {
        Position::TrackInfo &track = midi_current_position_.track[tk];
        if ((track.lastHandledEvent >= 0) && (track.delay <= 0))
        {
            // Check is an end of track has been reached
            if (track.pos == midi_track_data_[tk].end())
            {
                track.lastHandledEvent = -1;
                break;
            }

            // Handle event
            for (size_t i = 0; i < track.pos->events_.size(); i++)
            {
                const MidiEvent &evt = track.pos->events_[i];

                if (is_seek && (evt.type == MidiEvent::kNoteOn))
                    continue;
                handleEvent(tk, evt, track.lastHandledEvent);

                if (midi_loop_.caught_start_)
                {
                    if (midi_output_interface_->onloopStart) // Loop Start hook
                        midi_output_interface_->onloopStart(midi_output_interface_->onloopStart_userdata);

                    caughLoopStart++;
                    midi_loop_.caught_start_ = false;
                }

                if (midi_loop_.caught_stack_start_)
                {
                    if (midi_output_interface_->onloopStart &&
                        (midi_loop_start_time_ >= track.pos->time_)) // Loop Start hook
                        midi_output_interface_->onloopStart(midi_output_interface_->onloopStart_userdata);

                    caughLoopStackStart++;
                    midi_loop_.caught_stack_start_ = false;
                }

                if (midi_loop_.caught_stack_break_)
                {
                    caughLoopStackBreaks++;
                    midi_loop_.caught_stack_break_ = false;
                }

                if (midi_loop_.caught_end_ || midi_loop_.IsStackEnd())
                {
                    if (midi_loop_.caught_stack_end_)
                    {
                        midi_loop_.caught_stack_end_ = false;
                        caughLoopStackEnds++;
                        caughLoopStackEndsTime = track.pos->time_;
                    }
                    doLoopJump = true;
                    break; // Stop event handling on catching loopEnd event!
                }
            }

            // Read next event time (unless the track just ended)
            if (track.lastHandledEvent >= 0)
            {
                track.delay += track.pos->delay_;
                track.pos++;
            }

            if (doLoopJump)
                break;
        }
    }

    // Find a shortest delay from all track
    uint64_t shortestDelay         = 0;
    bool     shortestDelayNotFound = true;

    for (size_t tk = 0; tk < track_count; ++tk)
    {
        Position::TrackInfo &track = midi_current_position_.track[tk];
        if ((track.lastHandledEvent >= 0) && (shortestDelayNotFound || track.delay < shortestDelay))
        {
            shortestDelay         = track.delay;
            shortestDelayNotFound = false;
        }
    }

    // Schedule the next playevent to be processed after that delay
    for (size_t tk = 0; tk < track_count; ++tk)
        midi_current_position_.track[tk].delay -= shortestDelay;

    MidiFraction t = shortestDelay * midi_tempo_;

    midi_current_position_.wait += t.Value();

    if (caughLoopStart > 0 && midi_loop_begin_position_.absolute_time_position <= 0.0)
        midi_loop_begin_position_ = rowBeginPosition;

    if (caughLoopStackStart > 0)
    {
        while (caughLoopStackStart > 0)
        {
            midi_loop_.StackUp();
            LoopStackEntry &s = midi_loop_.GetCurrentStack();
            s.start_position  = rowBeginPosition;
            caughLoopStackStart--;
        }
        return true;
    }

    if (caughLoopStackBreaks > 0)
    {
        while (caughLoopStackBreaks > 0)
        {
            LoopStackEntry &s = midi_loop_.GetCurrentStack();
            s.loops           = 0;
            s.infinity        = false;
            // Quit the loop
            midi_loop_.StackDown();
            caughLoopStackBreaks--;
        }
    }

    if (caughLoopStackEnds > 0)
    {
        while (caughLoopStackEnds > 0)
        {
            LoopStackEntry &s = midi_loop_.GetCurrentStack();
            if (s.infinity)
            {
                if (midi_output_interface_->onloopEnd &&
                    (midi_loop_end_time_ >= caughLoopStackEndsTime))               // Loop End hook
                {
                    midi_output_interface_->onloopEnd(midi_output_interface_->onloopEnd_userdata);
                    if (midi_loop_hooks_only_)                                     // Stop song on reaching loop
                                                                                   // end
                    {
                        midi_at_end_ = true;                                       // Don't handle events anymore
                        midi_current_position_.wait += midi_post_song_wait_delay_; // One second delay
                                                                                   // until stop playing
                    }
                }

                midi_current_position_       = s.start_position;
                midi_loop_.skip_stack_start_ = true;

                for (uint8_t i = 0; i < 16; i++)
                    midi_output_interface_->rt_controllerChange(midi_output_interface_->rtUserData, i, 123, 0);

                return true;
            }
            else if (s.loops >= 0)
            {
                s.loops--;
                if (s.loops > 0)
                {
                    midi_current_position_       = s.start_position;
                    midi_loop_.skip_stack_start_ = true;

                    for (uint8_t i = 0; i < 16; i++)
                        midi_output_interface_->rt_controllerChange(midi_output_interface_->rtUserData, i, 123, 0);

                    return true;
                }
                else
                {
                    // Quit the loop
                    midi_loop_.StackDown();
                }
            }
            else
            {
                // Quit the loop
                midi_loop_.StackDown();
            }
            caughLoopStackEnds--;
        }

        return true;
    }

    if (shortestDelayNotFound || midi_loop_.caught_end_)
    {
        if (midi_output_interface_->onloopEnd) // Loop End hook
            midi_output_interface_->onloopEnd(midi_output_interface_->onloopEnd_userdata);

        for (uint8_t i = 0; i < 16; i++)
            midi_output_interface_->rt_controllerChange(midi_output_interface_->rtUserData, i, 123, 0);

        // Loop if song end or loop end point has reached
        midi_loop_.caught_end_ = false;
        shortestDelay          = 0;

        if (!midi_loop_enabled_ ||
            (shortestDelayNotFound && midi_loop_.loops_count_ >= 0 && midi_loop_.loops_left_ < 1) ||
            midi_loop_hooks_only_)
        {
            midi_at_end_ = true;                                       // Don't handle events anymore
            midi_current_position_.wait += midi_post_song_wait_delay_; // One second delay until stop
                                                                       // playing
            return true;                                               // We have caugh end here!
        }

        if (midi_loop_.temporary_broken_)
        {
            midi_current_position_       = midi_track_begin_position_;
            midi_loop_.temporary_broken_ = false;
        }
        else if (midi_loop_.loops_count_ < 0 || midi_loop_.loops_left_ >= 1)
        {
            midi_current_position_ = midi_loop_begin_position_;
            if (midi_loop_.loops_count_ >= 1)
                midi_loop_.loops_left_--;
        }
    }

    return true; // Has events in queue
}

MidiSequencer::MidiEvent MidiSequencer::ParseEvent(const uint8_t **pptr, const uint8_t *end, int &status)
{
    const uint8_t          *&ptr = *pptr;
    MidiSequencer::MidiEvent evt;

    if (ptr + 1 > end)
    {
        // When track doesn't ends on the middle of event data, it's must be
        // fine
        evt.type     = MidiEvent::kSpecial;
        evt.sub_type = MidiEvent::kEndTrack;
        return evt;
    }

    unsigned char byte = *(ptr++);
    bool          ok   = false;

    if (byte == MidiEvent::kSysex || byte == MidiEvent::kSysex2) // Ignore SysEx
    {
        uint64_t length = ReadVariableLengthValue(pptr, end, ok);
        if (!ok || (ptr + length > end))
        {
            midi_parsing_errors_string_ += "ParseEvent: Can't read SysEx event - Unexpected end of track "
                                           "data.\n";
            evt.is_valid = 0;
            return evt;
        }
        evt.type = MidiEvent::kSysex;
        evt.data.clear();
        evt.data.push_back(byte);
        std::copy(ptr, ptr + length, std::back_inserter(evt.data));
        ptr += (size_t)length;
        return evt;
    }

    if (byte == MidiEvent::kSpecial)
    {
        // Special event FF
        uint8_t  evtype = *(ptr++);
        uint64_t length = ReadVariableLengthValue(pptr, end, ok);
        if (!ok || (ptr + length > end))
        {
            midi_parsing_errors_string_ += "ParseEvent: Can't read Special event - Unexpected end of "
                                           "track data.\n";
            evt.is_valid = 0;
            return evt;
        }
        std::string data(length ? (const char *)ptr : nullptr, (size_t)length);
        ptr += (size_t)length;

        evt.type     = byte;
        evt.sub_type = evtype;
        evt.data.insert(evt.data.begin(), data.begin(), data.end());

        /* TODO: Store those meta-strings separately and give ability to read
         * them by external functions (to display song title and copyright in
         * the player) */
        if (evt.sub_type == MidiEvent::kCopyright)
        {
            if (midi_music_copyright_.empty())
            {
                midi_music_copyright_ = std::string((const char *)evt.data.data(), evt.data.size());
                midi_music_copyright_.push_back('\0'); /* ending fix for UTF16 strings */
                if (midi_output_interface_->onDebugMessage)
                    midi_output_interface_->onDebugMessage(midi_output_interface_->onDebugMessage_userdata,
                                                           "Music copyright: %s", midi_music_copyright_.c_str());
            }
            else if (midi_output_interface_->onDebugMessage)
            {
                std::string str((const char *)evt.data.data(), evt.data.size());
                str.push_back('\0'); /* ending fix for UTF16 strings */
                midi_output_interface_->onDebugMessage(midi_output_interface_->onDebugMessage_userdata,
                                                       "Extra copyright event: %s", str.c_str());
            }
        }
        else if (evt.sub_type == MidiEvent::kSequenceTrackTitle)
        {
            if (midi_music_title_.empty())
            {
                midi_music_title_ = std::string((const char *)evt.data.data(), evt.data.size());
                midi_music_title_.push_back('\0'); /* ending fix for UTF16 strings */
                if (midi_output_interface_->onDebugMessage)
                    midi_output_interface_->onDebugMessage(midi_output_interface_->onDebugMessage_userdata,
                                                           "Music title: %s", midi_music_title_.c_str());
            }
            else
            {
                std::string str((const char *)evt.data.data(), evt.data.size());
                str.push_back('\0'); /* ending fix for UTF16 strings */
                midi_music_track_titles_.push_back(str);
                if (midi_output_interface_->onDebugMessage)
                    midi_output_interface_->onDebugMessage(midi_output_interface_->onDebugMessage_userdata,
                                                           "Track title: %s", str.c_str());
            }
        }
        else if (evt.sub_type == MidiEvent::kInstrumentTitle)
        {
            if (midi_output_interface_->onDebugMessage)
            {
                std::string str((const char *)evt.data.data(), evt.data.size());
                str.push_back('\0'); /* ending fix for UTF16 strings */
                midi_output_interface_->onDebugMessage(midi_output_interface_->onDebugMessage_userdata,
                                                       "Instrument: %s", str.c_str());
            }
        }
        else if (evt.sub_type == MidiEvent::kMarker)
        {
            // To lower
            for (size_t i = 0; i < data.size(); i++)
            {
                if (data[i] <= 'Z' && data[i] >= 'A')
                    data[i] = (char)(data[i] - ('Z' - 'z'));
            }

            if (data == "loopstart")
            {
                // Return a custom Loop Start event instead of Marker
                evt.sub_type = MidiEvent::kLoopStart;
                evt.data.clear(); // Data is not needed
                return evt;
            }

            if (data == "loopend")
            {
                // Return a custom Loop End event instead of Marker
                evt.sub_type = MidiEvent::kLoopEnd;
                evt.data.clear(); // Data is not needed
                return evt;
            }

            if (data.substr(0, 10) == "loopstart=")
            {
                evt.type      = MidiEvent::kSpecial;
                evt.sub_type  = MidiEvent::kLoopStackBegin;
                uint8_t loops = (uint8_t)(atoi(data.substr(10).c_str()));
                evt.data.clear();
                evt.data.push_back(loops);

                if (midi_output_interface_->onDebugMessage)
                {
                    midi_output_interface_->onDebugMessage(midi_output_interface_->onDebugMessage_userdata,
                                                           "Stack Marker Loop Start at %d to %d level with %d "
                                                           "loops",
                                                           midi_loop_.stack_level_, midi_loop_.stack_level_ + 1, loops);
                }
                return evt;
            }

            if (data.substr(0, 8) == "loopend=")
            {
                evt.type     = MidiEvent::kSpecial;
                evt.sub_type = MidiEvent::kLoopStackEnd;
                evt.data.clear();

                if (midi_output_interface_->onDebugMessage)
                {
                    midi_output_interface_->onDebugMessage(midi_output_interface_->onDebugMessage_userdata,
                                                           "Stack Marker Loop %s at %d to %d level",
                                                           (evt.sub_type == MidiEvent::kLoopStackEnd ? "End" : "Break"),
                                                           midi_loop_.stack_level_, midi_loop_.stack_level_ - 1);
                }
                return evt;
            }
        }

        if (evtype == MidiEvent::kEndTrack)
            status = -1; // Finalize track

        return evt;
    }

    // Any normal event (80..EF)
    if (byte < 0x80)
    {
        byte = (uint8_t)(status | 0x80);
        ptr--;
    }

    // Sys Com Song Select(Song #) [0-127]
    if (byte == MidiEvent::kSysComSongSelect)
    {
        if (ptr + 1 > end)
        {
            midi_parsing_errors_string_ += "ParseEvent: Can't read System Command Song Select event - "
                                           "Unexpected end of track data.\n";
            evt.is_valid = 0;
            return evt;
        }
        evt.type = byte;
        evt.data.push_back(*(ptr++));
        return evt;
    }

    // Sys Com Song Position Pntr [LSB, MSB]
    if (byte == MidiEvent::kSysComSongPositionPointer)
    {
        if (ptr + 2 > end)
        {
            midi_parsing_errors_string_ += "ParseEvent: Can't read System Command Position Pointer event "
                                           "- Unexpected end of track data.\n";
            evt.is_valid = 0;
            return evt;
        }
        evt.type = byte;
        evt.data.push_back(*(ptr++));
        evt.data.push_back(*(ptr++));
        return evt;
    }

    uint8_t midCh = byte & 0x0F, evType = (byte >> 4) & 0x0F;
    status      = byte;
    evt.channel = midCh;
    evt.type    = evType;

    switch (evType)
    {
    case MidiEvent::kNoteOff: // 2 byte length
    case MidiEvent::kNoteOn:
    case MidiEvent::kNoteTouch:
    case MidiEvent::kControlChange:
    case MidiEvent::kPitchWheel:
        if (ptr + 2 > end)
        {
            midi_parsing_errors_string_ += "ParseEvent: Can't read regular 2-byte event - Unexpected "
                                           "end of track data.\n";
            evt.is_valid = 0;
            return evt;
        }

        evt.data.push_back(*(ptr++));
        evt.data.push_back(*(ptr++));

        if ((evType == MidiEvent::kNoteOn) && (evt.data[1] == 0))
        {
            evt.type = MidiEvent::kNoteOff; // Note ON with zero velocity
                                            // is Note OFF!
        }
        else if (evType == MidiEvent::kControlChange)
        {
            // 111'th loopStart controller (RPG Maker and others)
            if (midi_format_ == kFormatMidi)
            {
                switch (evt.data[0])
                {
                case 110:
                    if (midi_loop_format_ == kLoopDefault)
                    {
                        // Change event type to custom Loop Start event
                        // and clear data
                        evt.type     = MidiEvent::kSpecial;
                        evt.sub_type = MidiEvent::kLoopStart;
                        evt.data.clear();
                        midi_loop_format_ = kLoopHmi;
                    }
                    else if (midi_loop_format_ == kLoopHmi)
                    {
                        // Repeating of 110'th point is BAD practice,
                        // treat as EMIDI
                        midi_loop_format_ = kLoopEMidi;
                    }
                    break;

                case 111:
                    if (midi_loop_format_ == kLoopHmi)
                    {
                        // Change event type to custom Loop End event
                        // and clear data
                        evt.type     = MidiEvent::kSpecial;
                        evt.sub_type = MidiEvent::kLoopEnd;
                        evt.data.clear();
                    }
                    else if (midi_loop_format_ != kLoopEMidi)
                    {
                        // Change event type to custom Loop Start event
                        // and clear data
                        evt.type     = MidiEvent::kSpecial;
                        evt.sub_type = MidiEvent::kLoopStart;
                        evt.data.clear();
                    }
                    break;

                case 113:
                    if (midi_loop_format_ == kLoopEMidi)
                    {
                        // EMIDI does using of CC113 with same purpose
                        // as CC7
                        evt.data[0] = 7;
                    }
                    break;
                }
            }

            if (midi_format_ == kFormatXMidi)
            {
                switch (evt.data[0])
                {
                case 116: // For Loop Controller
                    evt.type     = MidiEvent::kSpecial;
                    evt.sub_type = MidiEvent::kLoopStackBegin;
                    evt.data[0]  = evt.data[1];
                    evt.data.pop_back();

                    if (midi_output_interface_->onDebugMessage)
                    {
                        midi_output_interface_->onDebugMessage(midi_output_interface_->onDebugMessage_userdata,
                                                               "Stack XMI Loop Start at %d to %d level "
                                                               "with %d loops",
                                                               midi_loop_.stack_level_, midi_loop_.stack_level_ + 1,
                                                               evt.data[0]);
                    }
                    break;

                case 117: // Next/Break Loop Controller
                    evt.type     = MidiEvent::kSpecial;
                    evt.sub_type = evt.data[1] < 64 ? MidiEvent::kLoopStackBreak : MidiEvent::kLoopStackEnd;
                    evt.data.clear();

                    if (midi_output_interface_->onDebugMessage)
                    {
                        midi_output_interface_->onDebugMessage(
                            midi_output_interface_->onDebugMessage_userdata, "Stack XMI Loop %s at %d to %d level",
                            (evt.sub_type == MidiEvent::kLoopStackEnd ? "End" : "Break"), midi_loop_.stack_level_,
                            midi_loop_.stack_level_ - 1);
                    }
                    break;

                case 119: // Callback Trigger
                    evt.type     = MidiEvent::kSpecial;
                    evt.sub_type = MidiEvent::kCallbackTrigger;
                    evt.data.assign(1, evt.data[1]);
                    break;
                }
            }
        }

        return evt;
    case MidiEvent::kPatchChange: // 1 byte length
    case MidiEvent::kChannelAftertouch:
        if (ptr + 1 > end)
        {
            midi_parsing_errors_string_ += "ParseEvent: Can't read regular 1-byte event - Unexpected "
                                           "end of track data.\n";
            evt.is_valid = 0;
            return evt;
        }
        evt.data.push_back(*(ptr++));
        return evt;
    default:
        break;
    }

    return evt;
}

void MidiSequencer::handleEvent(size_t track, const MidiSequencer::MidiEvent &evt, int32_t &status)
{
    if (track == 0 && midi_smf_format_ < 2 && evt.type == MidiEvent::kSpecial &&
        (evt.sub_type == MidiEvent::kTempoChange || evt.sub_type == MidiEvent::kTimeSignature))
    {
        /* never reject track 0 timing events on SMF format != 2
           note: multi-track XMI convert to format 2 SMF */
    }
    else
    {
        if (midi_track_solo_ != ~(size_t)(0) && track != midi_track_solo_)
            return;
        if (midi_track_disabled_[track])
            return;
    }

    if (midi_output_interface_->onEvent)
    {
        midi_output_interface_->onEvent(midi_output_interface_->onEvent_userdata, evt.type, evt.sub_type, evt.channel,
                                        evt.data.data(), evt.data.size());
    }

    if (evt.type == MidiEvent::kSysex || evt.type == MidiEvent::kSysex2) // Ignore SysEx
    {
        midi_output_interface_->rt_systemExclusive(midi_output_interface_->rtUserData, evt.data.data(),
                                                   evt.data.size());
        return;
    }

    if (evt.type == MidiEvent::kSpecial)
    {
        // Special event FF
        uint_fast16_t evtype = evt.sub_type;
        uint64_t      length = (uint64_t)(evt.data.size());
        const char   *data(length ? (const char *)(evt.data.data()) : "\0\0\0\0\0\0\0\0");

        if (midi_output_interface_->rt_metaEvent) // Meta event hook
            midi_output_interface_->rt_metaEvent(midi_output_interface_->rtUserData, evtype, (const uint8_t *)(data),
                                                 size_t(length));

        if (evtype == MidiEvent::kEndTrack) // End Of Track
        {
            status = -1;
            return;
        }

        if (evtype == MidiEvent::kTempoChange) // Tempo change
        {
            midi_tempo_ =
                midi_individual_tick_delta_ * MidiFraction(ReadIntBigEndian(evt.data.data(), evt.data.size()));
            return;
        }

        if (evtype == MidiEvent::kMarker) // Meta event
        {
            // Do nothing! :-P
            return;
        }

        if (evtype == MidiEvent::kDeviceSwitch)
        {
            if (midi_output_interface_->onDebugMessage)
                midi_output_interface_->onDebugMessage(midi_output_interface_->onDebugMessage_userdata,
                                                       "Switching another device: %s", data);
            if (midi_output_interface_->rt_deviceSwitch)
                midi_output_interface_->rt_deviceSwitch(midi_output_interface_->rtUserData, track, data,
                                                        size_t(length));
            return;
        }

        // Turn on Loop handling when loop is enabled
        if (midi_loop_enabled_ && !midi_loop_.invalid_loop_)
        {
            if (evtype == MidiEvent::kLoopStart) // Special non-spec MIDI
                                                 // loop Start point
            {
                midi_loop_.caught_start_ = true;
                return;
            }

            if (evtype == MidiEvent::kLoopEnd) // Special non-spec MIDI loop End point
            {
                midi_loop_.caught_end_ = true;
                return;
            }

            if (evtype == MidiEvent::kLoopStackBegin)
            {
                if (midi_loop_.skip_stack_start_)
                {
                    midi_loop_.skip_stack_start_ = false;
                    return;
                }

                char   x      = data[0];
                size_t slevel = (size_t)(midi_loop_.stack_level_ + 1);
                while (slevel >= midi_loop_.stack_.size())
                {
                    LoopStackEntry e;
                    e.loops    = x;
                    e.infinity = (x == 0);
                    e.start    = 0;
                    e.end      = 0;
                    midi_loop_.stack_.push_back(e);
                }

                LoopStackEntry &s              = midi_loop_.stack_[slevel];
                s.loops                        = (int)(x);
                s.infinity                     = (x == 0);
                midi_loop_.caught_stack_start_ = true;
                return;
            }

            if (evtype == MidiEvent::kLoopStackEnd)
            {
                midi_loop_.caught_stack_end_ = true;
                return;
            }

            if (evtype == MidiEvent::kLoopStackBreak)
            {
                midi_loop_.caught_stack_break_ = true;
                return;
            }
        }

        if (evtype == MidiEvent::kCallbackTrigger)
        {
            if (midi_trigger_handler_)
                midi_trigger_handler_(midi_trigger_userdata_, (unsigned)(data[0]), track);
            return;
        }

        if (evtype == MidiEvent::kRawOpl) // Special non-spec ADLMIDI special for IMF
                                          // playback: Direct poke to AdLib
        {
            if (midi_output_interface_->rt_rawOPL)
                midi_output_interface_->rt_rawOPL(midi_output_interface_->rtUserData, (uint8_t)(data[0]),
                                                  (uint8_t)(data[1]));
            return;
        }

        if (evtype == MidiEvent::kSongBeginHook)
        {
            if (midi_output_interface_->onSongStart)
                midi_output_interface_->onSongStart(midi_output_interface_->onSongStart_userdata);
            return;
        }

        return;
    }

    if (evt.type == MidiEvent::kSysComSongSelect || evt.type == MidiEvent::kSysComSongPositionPointer)
        return;

    size_t midCh = evt.channel;
    if (midi_output_interface_->rt_currentDevice)
        midCh += midi_output_interface_->rt_currentDevice(midi_output_interface_->rtUserData, track);
    status = evt.type;

    switch (evt.type)
    {
    case MidiEvent::kNoteOff: // Note off
    {
        if (midCh < 16 && m_channelDisable[midCh])
            break;            // Disabled channel
        uint8_t note = evt.data[0];
        uint8_t vol  = evt.data[1];
        if (midi_output_interface_->rt_noteOff)
            midi_output_interface_->rt_noteOff(midi_output_interface_->rtUserData, (uint8_t)(midCh), note);
        if (midi_output_interface_->rt_noteOffVel)
            midi_output_interface_->rt_noteOffVel(midi_output_interface_->rtUserData, (uint8_t)(midCh), note, vol);
        break;
    }

    case MidiEvent::kNoteOn: // Note on
    {
        if (midCh < 16 && m_channelDisable[midCh])
            break;           // Disabled channel
        uint8_t note = evt.data[0];
        uint8_t vol  = evt.data[1];
        midi_output_interface_->rt_noteOn(midi_output_interface_->rtUserData, (uint8_t)(midCh), note, vol);
        break;
    }

    case MidiEvent::kNoteTouch: // Note touch
    {
        uint8_t note = evt.data[0];
        uint8_t vol  = evt.data[1];
        midi_output_interface_->rt_noteAfterTouch(midi_output_interface_->rtUserData, (uint8_t)(midCh), note, vol);
        break;
    }

    case MidiEvent::kControlChange: // Controller change
    {
        uint8_t ctrlno = evt.data[0];
        uint8_t value  = evt.data[1];
        midi_output_interface_->rt_controllerChange(midi_output_interface_->rtUserData, (uint8_t)(midCh), ctrlno,
                                                    value);
        break;
    }

    case MidiEvent::kPatchChange: // Patch change
    {
        midi_output_interface_->rt_patchChange(midi_output_interface_->rtUserData, (uint8_t)(midCh), evt.data[0]);
        break;
    }

    case MidiEvent::kChannelAftertouch: // Channel after-touch
    {
        uint8_t chanat = evt.data[0];
        midi_output_interface_->rt_channelAfterTouch(midi_output_interface_->rtUserData, (uint8_t)(midCh), chanat);
        break;
    }

    case MidiEvent::kPitchWheel: // Wheel/pitch bend
    {
        uint8_t a = evt.data[0];
        uint8_t b = evt.data[1];
        midi_output_interface_->rt_pitchBend(midi_output_interface_->rtUserData, (uint8_t)(midCh), b, a);
        break;
    }

    default:
        break;
    } // switch
}

double MidiSequencer::Tick(double s, double granularity)
{
    assert(midi_output_interface_); // MIDI output interface must be defined!

    s *= midi_tempo_multiplier_;
    midi_current_position_.wait -= s;
    midi_current_position_.absolute_time_position += s;

    int antiFreezeCounter = 10000; // Limit 10000 loops to avoid freezing
    while ((midi_current_position_.wait <= granularity * 0.5) && (antiFreezeCounter > 0))
    {
        if (!ProcessEvents())
            break;
        if (midi_current_position_.wait <= 0.0)
            antiFreezeCounter--;
    }

    if (antiFreezeCounter <= 0)
        midi_current_position_.wait += 1.0; /* Add extra 1 second when over 10000 events
                                               with zero delay are been detected */

    if (midi_current_position_.wait < 0.0)  // Avoid negative delay value!
        return 0.0;

    return midi_current_position_.wait;
}

double MidiSequencer::Seek(double seconds, const double granularity)
{
    if (seconds < 0.0)
        return 0.0;                        // Seeking negative position is forbidden! :-P
    const double granualityHalf = granularity * 0.5,
                 s              = seconds; // m_setup.delay_ < m_setup.maxdelay ?
                                           // m_setup.delay_ : m_setup.maxdelay;

    /* Attempt to go away out of song end must rewind position to begin */
    if (seconds > midi_full_song_time_length_)
    {
        this->Rewind();
        return 0.0;
    }

    bool loopFlagState = midi_loop_enabled_;
    // Turn loop pooints off because it causes wrong position rememberin on a
    // quick seek
    midi_loop_enabled_ = false;

    /*
     * Seeking search is similar to regular ticking, except of next things:
     * - We don't processsing arpeggio and vibrato
     * - To keep correctness of the state after seek, begin every search from
     * begin
     * - All sustaining notes must be killed
     * - Ignore Note-On events
     */
    this->Rewind();

    /*
     * Set "loop Start" to false to prevent overwrite of loopStart position with
     * seek destinition position
     *
     * TODO: Detect & set loopStart position on load time to don't break loop
     * while seeking
     */
    midi_loop_.caught_start_ = false;

    midi_loop_.temporary_broken_ = (seconds >= midi_loop_end_time_);

    while ((midi_current_position_.absolute_time_position < seconds) &&
           (midi_current_position_.absolute_time_position < midi_full_song_time_length_))
    {
        midi_current_position_.wait -= s;
        midi_current_position_.absolute_time_position += s;
        int    antiFreezeCounter = 10000; // Limit 10000 loops to avoid freezing
        double dstWait           = midi_current_position_.wait + granualityHalf;
        while ((midi_current_position_.wait <= granualityHalf) /*&& (antiFreezeCounter > 0)*/)
        {
            // std::fprintf(stderr, "wait = %g...\n", CurrentPosition.wait);
            if (!ProcessEvents(true))
                break;
            // Avoid freeze because of no waiting increasing in more than 10000
            // cycles
            if (midi_current_position_.wait <= dstWait)
                antiFreezeCounter--;
            else
            {
                dstWait           = midi_current_position_.wait + granualityHalf;
                antiFreezeCounter = 10000;
            }
        }
        if (antiFreezeCounter <= 0)
            midi_current_position_.wait += 1.0; /* Add extra 1 second when over 10000 events
                                                   with zero delay are been detected */
    }

    if (midi_current_position_.wait < 0.0)
        midi_current_position_.wait = 0.0;

    if (midi_at_end_)
    {
        this->Rewind();
        midi_loop_enabled_ = loopFlagState;
        return 0.0;
    }

    midi_time_.Reset();
    midi_time_.delay_ = midi_current_position_.wait;

    midi_loop_enabled_ = loopFlagState;
    return midi_current_position_.wait;
}

double MidiSequencer::Tell()
{
    return midi_current_position_.absolute_time_position;
}

double MidiSequencer::TimeLength()
{
    return midi_full_song_time_length_;
}

double MidiSequencer::GetLoopStart()
{
    return midi_loop_start_time_;
}

double MidiSequencer::GetLoopEnd()
{
    return midi_loop_end_time_;
}

void MidiSequencer::Rewind()
{
    midi_current_position_ = midi_track_begin_position_;
    midi_at_end_           = false;

    midi_loop_.loops_count_ = midi_loop_count_;
    midi_loop_.Reset();
    midi_loop_.caught_start_     = true;
    midi_loop_.temporary_broken_ = false;
    midi_time_.Reset();
}

void MidiSequencer::SetTempo(double tempo)
{
    midi_tempo_multiplier_ = tempo;
}

bool MidiSequencer::LoadMidi(const uint8_t *data, size_t size, uint16_t rate)
{
    epi::MemFile *mfr = new epi::MemFile(data, size);
    return LoadMidi(mfr, rate);
}

template <class T> class BufferGuard
{
    T *m_ptr;

  public:
    BufferGuard() : m_ptr(nullptr)
    {
    }

    ~BufferGuard()
    {
        set();
    }

    void set(T *p = nullptr)
    {
        if (m_ptr)
            free(m_ptr);
        m_ptr = p;
    }
};

/**
 * @brief Detect the EA-MUS file format
 * @param head Header part
 * @param fr Context with opened file data
 * @return true if given file was identified as EA-MUS
 */
static bool detectRSXX(const char *head, epi::MemFile *mfr)
{
    char headerBuf[7] = "";
    bool ret          = false;

    // Try to identify RSXX format
    if (head[0] >= 0x5D)
    {
        mfr->Seek(head[0] - 0x10, epi::File::kSeekpointStart);
        mfr->Read(headerBuf, 6);
        if (memcmp(headerBuf, "rsxx}u", 6) == 0)
            ret = true;
    }

    mfr->Seek(0, epi::File::kSeekpointStart);
    return ret;
}

/**
 * @brief Detect the Id-software Music File format
 * @param head Header part
 * @param fr Context with opened file data
 * @return true if given file was identified as IMF
 */
static bool detectIMF(const char *head, epi::MemFile *mfr)
{
    uint8_t raw[4];
    size_t  end = (size_t)(head[0]) + 256 * (size_t)(head[1]);

    if (end & 3)
        return false;

    size_t  backup_pos = mfr->GetPosition();
    int64_t sum1 = 0, sum2 = 0;
    mfr->Seek((end > 0 ? 2 : 0), epi::File::kSeekpointStart);

    for (size_t n = 0; n < 16383; ++n)
    {
        if (mfr->Read(raw, 4) != 4)
            break;
        int64_t value1 = raw[0];
        value1 += raw[1] << 8;
        sum1 += value1;
        int64_t value2 = raw[2];
        value2 += raw[3] << 8;
        sum2 += value2;
    }

    mfr->Seek((long)(backup_pos), epi::File::kSeekpointStart);

    return (sum1 > sum2);
}

bool MidiSequencer::LoadMidi(epi::MemFile *mfr, uint16_t rate)
{
    size_t fsize = 0;
    (void)(fsize);
    midi_parsing_errors_string_.clear();

    assert(midi_output_interface_); // MIDI output interface must be defined!

    midi_at_end_ = false;
    midi_loop_.FullReset();
    midi_loop_.caught_start_ = true;

    midi_format_     = kFormatMidi;
    midi_smf_format_ = 0;

    midi_raw_songs_data_.clear();

    const size_t headerSize            = 4 + 4 + 2 + 2 + 2; // 14
    char         headerBuf[headerSize] = "";

    fsize = mfr->Read(headerBuf, headerSize);
    if (fsize < headerSize)
    {
        midi_error_string_ = "Unexpected end of file at header!\n";
        delete mfr;
        return false;
    }

    if (memcmp(headerBuf, "MThd\0\0\0\6", 8) == 0)
    {
        mfr->Seek(0, epi::File::kSeekpointStart);
        return ParseSmf(mfr);
    }

    if (memcmp(headerBuf, "RIFF", 4) == 0)
    {
        mfr->Seek(0, epi::File::kSeekpointStart);
        return ParseRmi(mfr);
    }

    if (memcmp(headerBuf, "GMF\x1", 4) == 0)
    {
        mfr->Seek(0, epi::File::kSeekpointStart);
        return ParseGmf(mfr);
    }

    if (memcmp(headerBuf, "MUS\x1A", 4) == 0)
    {
        mfr->Seek(0, epi::File::kSeekpointStart);
        return ParseMus(mfr);
    }

    if ((memcmp(headerBuf, "FORM", 4) == 0) && (memcmp(headerBuf + 8, "XDIR", 4) == 0))
    {
        mfr->Seek(0, epi::File::kSeekpointStart);
        return ParseXmi(mfr);
    }

    if (detectIMF(headerBuf, mfr))
    {
        mfr->Seek(0, epi::File::kSeekpointStart);
        return ParseImf(mfr, rate);
    }

    if (detectRSXX(headerBuf, mfr))
    {
        mfr->Seek(0, epi::File::kSeekpointStart);
        return ParseRsxx(mfr);
    }

    midi_error_string_ = "Unknown or unsupported file format";
    delete mfr;
    return false;
}

bool MidiSequencer::ParseImf(epi::MemFile *mfr, uint16_t rate)
{
    const size_t deltaTicks   = 1;
    const size_t track_count  = 1;
    uint32_t     imfTempo     = 0;
    size_t       imfEnd       = 0;
    uint64_t     abs_position = 0;
    uint8_t      imfRaw[4];

    MidiTrackRow evtPos;
    MidiEvent    event;

    switch (rate)
    {
    case 280:
        imfTempo = 3570;
        break;
    case 560:
        imfTempo = 1785;
        break;
    case 700:
        imfTempo = 1428;
        break;
    default:
        imfTempo = 1428;
        break;
    }

    std::vector<MidiEvent> temposList;

    midi_format_ = kFormatImf;

    BuildSmfSetupReset(track_count);

    midi_individual_tick_delta_ = MidiFraction(1, 1000000l * (uint64_t)(deltaTicks));
    midi_tempo_                 = MidiFraction(1, (uint64_t)(deltaTicks) * 2);

    mfr->Seek(0, epi::File::kSeekpointStart);
    if (mfr->Read(imfRaw, 2) != 2)
    {
        midi_error_string_ = "Unexpected end of file at header!\n";
        delete mfr;
        return false;
    }

    imfEnd = (size_t)(imfRaw[0]) + 256 * (size_t)(imfRaw[1]);

    // Define the playing tempo
    event.type                   = MidiEvent::kSpecial;
    event.sub_type               = MidiEvent::kTempoChange;
    event.absolute_tick_position = 0;
    event.data.resize(4);
    event.data[0] = (uint8_t)((imfTempo >> 24) & 0xFF);
    event.data[1] = (uint8_t)((imfTempo >> 16) & 0xFF);
    event.data[2] = (uint8_t)((imfTempo >> 8) & 0xFF);
    event.data[3] = (uint8_t)((imfTempo & 0xFF));
    evtPos.events_.push_back(event);
    temposList.push_back(event);

    // Define the draft for IMF events
    event.type                   = MidiEvent::kSpecial;
    event.sub_type               = MidiEvent::kRawOpl;
    event.absolute_tick_position = 0;
    event.data.resize(2);

    mfr->Seek((imfEnd > 0) ? 2 : 0, epi::File::kSeekpointStart);

    if (imfEnd == 0) // IMF Type 0 with unlimited file length
        imfEnd = mfr->GetLength();

    while (mfr->GetPosition() < (int)imfEnd)
    {
        if (mfr->Read(imfRaw, 4) != 4)
            break;

        event.data[0]                = imfRaw[0]; // port index
        event.data[1]                = imfRaw[1]; // port value
        event.absolute_tick_position = abs_position;
        event.is_valid               = 1;

        evtPos.events_.push_back(event);
        evtPos.delay_ = (uint64_t)(imfRaw[2]) + 256 * (uint64_t)(imfRaw[3]);

        if (evtPos.delay_ > 0)
        {
            evtPos.absolute_position_ = abs_position;
            abs_position += evtPos.delay_;
            midi_track_data_[0].push_back(evtPos);
            evtPos.Clear();
        }
    }

    // Add final row
    evtPos.absolute_position_ = abs_position;
    abs_position += evtPos.delay_;
    midi_track_data_[0].push_back(evtPos);

    if (!midi_track_data_[0].empty())
        midi_current_position_.track[0].pos = midi_track_data_[0].begin();

    BuildTimeLine(temposList);

    delete mfr;

    return true;
}

bool MidiSequencer::ParseRsxx(epi::MemFile *mfr)
{
    const size_t                      headerSize            = 14;
    char                              headerBuf[headerSize] = "";
    size_t                            fsize                 = 0;
    size_t                            deltaTicks = 192, track_count = 1;
    std::vector<std::vector<uint8_t>> rawTrackData;

    fsize = mfr->Read(headerBuf, headerSize);
    if (fsize < headerSize)
    {
        midi_error_string_ = "Unexpected end of file at header!\n";
        delete mfr;
        return false;
    }

    // Try to identify RSXX format
    char start = headerBuf[0];
    if (start < 0x5D)
    {
        midi_error_string_ = "RSXX song too short!\n";
        delete mfr;
        return false;
    }
    else
    {
        mfr->Seek(headerBuf[0] - 0x10, epi::File::kSeekpointStart);
        mfr->Read(headerBuf, 6);
        if (memcmp(headerBuf, "rsxx}u", 6) == 0)
        {
            midi_format_ = kFormatRsxx;
            mfr->Seek(start, epi::File::kSeekpointStart);
            track_count = 1;
            deltaTicks  = 60;
        }
        else
        {
            midi_error_string_ = "Invalid RSXX header!\n";
            delete mfr;
            return false;
        }
    }

    rawTrackData.clear();
    rawTrackData.resize(track_count, std::vector<uint8_t>());
    midi_individual_tick_delta_ = MidiFraction(1, 1000000l * (uint64_t)(deltaTicks));
    midi_tempo_                 = MidiFraction(1, (uint64_t)(deltaTicks));

    size_t totalGotten = 0;

    for (size_t tk = 0; tk < track_count; ++tk)
    {
        // Read track header
        size_t trackLength;

        size_t pos = mfr->GetPosition();
        mfr->Seek(0, epi::File::kSeekpointEnd);
        trackLength = mfr->GetPosition() - pos;
        mfr->Seek((long)(pos), epi::File::kSeekpointStart);

        // Read track data
        rawTrackData[tk].resize(trackLength);
        fsize = mfr->Read(&rawTrackData[tk][0], trackLength);
        if (fsize < trackLength)
        {
            midi_error_string_ = "MIDI Loader: Unexpected file ending while getting raw track "
                                 "data!\n";
            delete mfr;
            return false;
        }
        totalGotten += fsize;

        // Finalize raw track data with a zero
        rawTrackData[tk].push_back(0);
    }

    for (size_t tk = 0; tk < track_count; ++tk)
        totalGotten += rawTrackData[tk].size();

    if (totalGotten == 0)
    {
        midi_error_string_ = "MIDI Loader: Empty track data";
        delete mfr;
        return false;
    }

    // Build new MIDI events table
    if (!BuildSmfTrackData(rawTrackData))
    {
        midi_error_string_ = "MIDI Loader: MIDI data parsing error has occouped!\n" + midi_parsing_errors_string_;
        delete mfr;
        return false;
    }

    midi_smf_format_        = 0;
    midi_loop_.stack_level_ = -1;

    delete mfr;

    return true;
}

bool MidiSequencer::ParseGmf(epi::MemFile *mfr)
{
    const size_t                      headerSize            = 14;
    char                              headerBuf[headerSize] = "";
    size_t                            fsize                 = 0;
    size_t                            deltaTicks = 192, track_count = 1;
    std::vector<std::vector<uint8_t>> rawTrackData;

    fsize = mfr->Read(headerBuf, headerSize);
    if (fsize < headerSize)
    {
        midi_error_string_ = "Unexpected end of file at header!\n";
        delete mfr;
        return false;
    }

    if (memcmp(headerBuf, "GMF\x1", 4) != 0)
    {
        midi_error_string_ = "MIDI Loader: Invalid format, GMF\\x1 signature is not found!\n";
        delete mfr;
        return false;
    }

    mfr->Seek(7 - (long)(headerSize), epi::File::kSeekpointCurrent);

    rawTrackData.clear();
    rawTrackData.resize(track_count, std::vector<uint8_t>());
    midi_individual_tick_delta_            = MidiFraction(1, 1000000l * (uint64_t)(deltaTicks));
    midi_tempo_                            = MidiFraction(1, (uint64_t)(deltaTicks) * 2);
    static const unsigned char EndTag[4]   = { 0xFF, 0x2F, 0x00, 0x00 };
    size_t                     totalGotten = 0;

    for (size_t tk = 0; tk < track_count; ++tk)
    {
        // Read track header
        size_t trackLength;
        size_t pos = mfr->GetPosition();
        mfr->Seek(0, epi::File::kSeekpointEnd);
        trackLength = mfr->GetPosition() - pos;
        mfr->Seek((long)(pos), epi::File::kSeekpointStart);

        // Read track data
        rawTrackData[tk].resize(trackLength);
        fsize = mfr->Read(&rawTrackData[tk][0], trackLength);
        if (fsize < trackLength)
        {
            midi_error_string_ = "MIDI Loader: Unexpected file ending while getting raw track "
                                 "data!\n";
            delete mfr;
            return false;
        }
        totalGotten += fsize;
        // Note: GMF does include the track end tag.
        rawTrackData[tk].insert(rawTrackData[tk].end(), EndTag + 0, EndTag + 4);
    }

    for (size_t tk = 0; tk < track_count; ++tk)
        totalGotten += rawTrackData[tk].size();

    if (totalGotten == 0)
    {
        midi_error_string_ = "MIDI Loader: Empty track data";
        delete mfr;
        return false;
    }

    // Build new MIDI events table
    if (!BuildSmfTrackData(rawTrackData))
    {
        midi_error_string_ = "MIDI Loader: : MIDI data parsing error has occouped!\n" + midi_parsing_errors_string_;
        delete mfr;
        return false;
    }

    delete mfr;

    return true;
}

bool MidiSequencer::ParseSmf(epi::MemFile *mfr)
{
    const size_t                      headerSize            = 14; // 4 + 4 + 2 + 2 + 2
    char                              headerBuf[headerSize] = "";
    size_t                            fsize                 = 0;
    size_t                            deltaTicks = 192, TrackCount = 1;
    unsigned                          smfFormat = 0;
    std::vector<std::vector<uint8_t>> rawTrackData;

    fsize = mfr->Read(headerBuf, headerSize);
    if (fsize < headerSize)
    {
        midi_error_string_ = "Unexpected end of file at header!\n";
        delete mfr;
        return false;
    }

    if (memcmp(headerBuf, "MThd\0\0\0\6", 8) != 0)
    {
        midi_error_string_ = "MIDI Loader: Invalid format, MThd signature is not found!\n";
        delete mfr;
        return false;
    }

    smfFormat  = (unsigned)(ReadIntBigEndian(headerBuf + 8, 2));
    TrackCount = (size_t)(ReadIntBigEndian(headerBuf + 10, 2));
    deltaTicks = (size_t)(ReadIntBigEndian(headerBuf + 12, 2));

    if (smfFormat > 2)
        smfFormat = 1;

    rawTrackData.clear();
    rawTrackData.resize(TrackCount, std::vector<uint8_t>());
    midi_individual_tick_delta_ = MidiFraction(1, 1000000l * (uint64_t)(deltaTicks));
    midi_tempo_                 = MidiFraction(1, (uint64_t)(deltaTicks) * 2);

    size_t totalGotten = 0;

    for (size_t tk = 0; tk < TrackCount; ++tk)
    {
        // Read track header
        size_t trackLength;

        fsize = mfr->Read(headerBuf, 8);
        if ((fsize < 8) || (memcmp(headerBuf, "MTrk", 4) != 0))
        {
            midi_error_string_ = "MIDI Loader: Invalid format, MTrk signature is not found!\n";
            delete mfr;
            return false;
        }
        trackLength = (size_t)ReadIntBigEndian(headerBuf + 4, 4);

        // Read track data
        rawTrackData[tk].resize(trackLength);
        fsize = mfr->Read(&rawTrackData[tk][0], trackLength);
        if (fsize < trackLength)
        {
            midi_error_string_ = "MIDI Loader: Unexpected file ending while getting raw track "
                                 "data!\n";
            delete mfr;
            return false;
        }

        totalGotten += fsize;
    }

    for (size_t tk = 0; tk < TrackCount; ++tk)
        totalGotten += rawTrackData[tk].size();

    if (totalGotten == 0)
    {
        midi_error_string_ = "MIDI Loader: Empty track data";
        delete mfr;
        return false;
    }

    // Build new MIDI events table
    if (!BuildSmfTrackData(rawTrackData))
    {
        midi_error_string_ = "MIDI Loader: MIDI data parsing error has occouped!\n" + midi_parsing_errors_string_;
        delete mfr;
        return false;
    }

    midi_smf_format_        = smfFormat;
    midi_loop_.stack_level_ = -1;

    delete mfr;

    return true;
}

bool MidiSequencer::ParseRmi(epi::MemFile *mfr)
{
    const size_t headerSize            = 4 + 4 + 2 + 2 + 2; // 14
    char         headerBuf[headerSize] = "";

    size_t fsize = mfr->Read(headerBuf, headerSize);
    if (fsize < headerSize)
    {
        midi_error_string_ = "Unexpected end of file at header!\n";
        delete mfr;
        return false;
    }

    if (memcmp(headerBuf, "RIFF", 4) != 0)
    {
        midi_error_string_ = "MIDI Loader: Invalid format, RIFF signature is not found!\n";
        delete mfr;
        return false;
    }

    midi_format_ = kFormatMidi;

    mfr->Seek(6l, epi::File::kSeekpointCurrent);
    return ParseSmf(mfr);
}

bool MidiSequencer::ParseMus(epi::MemFile *mfr)
{
    const size_t         headerSize            = 14;
    char                 headerBuf[headerSize] = "";
    size_t               fsize                 = 0;
    BufferGuard<uint8_t> cvt_buf;

    fsize = mfr->Read(headerBuf, headerSize);
    if (fsize < headerSize)
    {
        midi_error_string_ = "Unexpected end of file at header!\n";
        delete mfr;
        return false;
    }

    if (memcmp(headerBuf, "MUS\x1A", 4) != 0)
    {
        midi_error_string_ = "MIDI Loader: Invalid format, MUS\\x1A signature is not found!\n";
        delete mfr;
        return false;
    }

    size_t mus_len = mfr->GetLength();

    mfr->Seek(0, epi::File::kSeekpointStart);
    uint8_t *mus = (uint8_t *)malloc(mus_len);
    if (!mus)
    {
        midi_error_string_ = "Out of memory!";
        delete mfr;
        return false;
    }
    fsize = mfr->Read(mus, mus_len);
    if (fsize < mus_len)
    {
        midi_error_string_ = "Failed to read MUS file data!\n";
        delete mfr;
        return false;
    }

    // Close source stream
    delete mfr;
    mfr = nullptr;

    uint8_t *mid     = nullptr;
    uint32_t mid_len = 0;
    int      m2mret  = ConvertMusToMidi(mus, (uint32_t)(mus_len), &mid, &mid_len, 0);

    if (mus)
        free(mus);

    if (m2mret < 0)
    {
        midi_error_string_ = "Invalid MUS/DMX data format!";
        if (mid)
            free(mid);
        return false;
    }
    cvt_buf.set(mid);

    // Open converted MIDI file
    mfr = new epi::MemFile(mid, (size_t)(mid_len));

    return ParseSmf(mfr);
}

bool MidiSequencer::ParseXmi(epi::MemFile *mfr)
{
    const size_t                      headerSize            = 14;
    char                              headerBuf[headerSize] = "";
    size_t                            fsize                 = 0;
    std::vector<std::vector<uint8_t>> song_buf;
    bool                              ret;

    fsize = mfr->Read(headerBuf, headerSize);
    if (fsize < headerSize)
    {
        midi_error_string_ = "Unexpected end of file at header!\n";
        delete mfr;
        return false;
    }

    if (memcmp(headerBuf, "FORM", 4) != 0)
    {
        midi_error_string_ = "MIDI Loader: Invalid format, FORM signature is not found!\n";
        delete mfr;
        return false;
    }

    if (memcmp(headerBuf + 8, "XDIR", 4) != 0)
    {
        midi_error_string_ = "MIDI Loader: Invalid format\n";
        delete mfr;
        return false;
    }

    size_t mus_len = mfr->GetLength();
    mfr->Seek(0, epi::File::kSeekpointStart);

    uint8_t *mus = (uint8_t *)std::malloc(mus_len + 20);
    if (!mus)
    {
        midi_error_string_ = "Out of memory!";
        delete mfr;
        return false;
    }

    memset(mus, 0, mus_len + 20);

    fsize = mfr->Read(mus, mus_len);
    if (fsize < mus_len)
    {
        midi_error_string_ = "Failed to read XMI file data!\n";
        delete mfr;
        return false;
    }

    // Close source stream
    delete mfr;
    mfr = nullptr;

    int m2mret = ConvertXmiToMidi(mus, (uint32_t)(mus_len + 20), song_buf, kXmiNoConversion);
    if (mus)
        free(mus);
    if (m2mret < 0)
    {
        song_buf.clear();
        midi_error_string_ = "Invalid XMI data format!";
        return false;
    }

    if (midi_load_track_number_ >= (int)song_buf.size())
        midi_load_track_number_ = song_buf.size() - 1;

    for (size_t i = 0; i < song_buf.size(); ++i)
    {
        midi_raw_songs_data_.push_back(song_buf[i]);
    }

    song_buf.clear();

    // Open converted MIDI file
    mfr = new epi::MemFile(midi_raw_songs_data_[midi_load_track_number_].data(),
                           midi_raw_songs_data_[midi_load_track_number_].size());
    // Set format as XMIDI
    midi_format_ = kFormatXMidi;

    ret = ParseSmf(mfr);

    return ret;
}