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

#include <list>
#include <string>
#include <vector>

#include "file.h"
#include "fraction.hpp"
#include "midi_sequencer.h"

class MidiSequencer
{
    /**
     * @brief MIDI Event utility container
     */
    struct MidiEvent
    {
        /**
         * @brief Main MIDI event types
         */
        enum Types
        {
            //! Unknown event
            kUnknown = 0x00,
            //! Note-Off event
            kNoteOff = 0x08,            // size == 2
                                        //! Note-On event
            kNoteOn = 0x09,             // size == 2
                                        //! Note After-Touch event
            kNoteTouch = 0x0A,          // size == 2
                                        //! Controller change event
            kControlChange = 0x0B,      // size == 2
                                        //! Patch change event
            kPatchChange = 0x0C,        // size == 1
                                        //! Channel After-Touch event
            kChannelAftertouch = 0x0D,  // size == 1
                                        //! Pitch-bend change event
            kPitchWheel = 0x0E,         // size == 2

            //! System Exclusive message, type 1
            kSysex = 0xF0,  // size == len
                            //! Sys Com Song Position Pntr [LSB, MSB]
            kSysComSongPositionPointer =
                0xF2,                  // size == 2
                                       //! Sys Com Song Select(Song #) [0-127]
            kSysComSongSelect = 0xF3,  // size == 1
                                       //! System Exclusive message, type 2
            kSysex2 = 0xF7,            // size == len
                                       //! Special event
            kSpecial = 0xFF
        };
        /**
         * @brief Special MIDI event sub-types
         */
        enum SubTypes
        {
            //! Sequension number
            kSequensionNumber = 0x00,    // size == 2
                                         //! Text label
            kText = 0x01,                // size == len
                                         //! Copyright notice
            kCopyright = 0x02,           // size == len
                                         //! Sequence track title
            kSequenceTrackTitle = 0x03,  // size == len
                                         //! Instrument title
            kInstrumentTitle = 0x04,     // size == len
                                         //! Lyrics text fragment
            kLyrics = 0x05,              // size == len
                                         //! MIDI Marker
            kMarker = 0x06,              // size == len
                                         //! Cue Point
            kCuePoint = 0x07,            // size == len
                                         //! [Non-Standard] Device Switch
            kDeviceSwitch = 0x09,        // size == len <CUSTOM>
                                         //! MIDI Channel prefix
            kMidiChannelPrefix = 0x20,   // size == 1

            //! End of Track event
            kEndTrack = 0x2F,       // size == 0
                                    //! Tempo change event
            kTempoChange = 0x51,    // size == 3
                                    //! SMPTE offset
            kSmpteOffset = 0x54,    // size == 5
                                    //! Time signature
            kTimeSignature = 0x55,  // size == 4
                                    //! Key signature
            kKeySignature = 0x59,   // size == 2
                                    //! Sequencer specs
            kSequencerSpec = 0x7F,  // size == len

            /* Non-standard, internal ADLMIDI usage only */
            //! [Non-Standard] Loop Start point
            kLoopStart = 0xE1,  // size == 0 <CUSTOM>
                                //! [Non-Standard] Loop End point
            kLoopEnd = 0xE2,    // size == 0 <CUSTOM>
                                //! [Non-Standard] Raw OPL data
            kRawOpl = 0xE3,     // size == 0 <CUSTOM>

            //! [Non-Standard] Loop Start point with support of multi-loops
            kLoopStackBegin = 0xE4,   // size == 1 <CUSTOM>
                                      //! [Non-Standard] Loop End point with
                                      //! support of multi-loops
            kLoopStackEnd = 0xE5,     // size == 0 <CUSTOM>
                                      //! [Non-Standard] Loop End point with
                                      //! support of multi-loops
            kLoopStackBreak = 0xE6,   // size == 0 <CUSTOM>
                                      //! [Non-Standard] Callback Trigger
            kCallbackTrigger = 0xE7,  // size == 1 <CUSTOM>

            // Built-in hooks
            kSongBeginHook = 0x101
        };
        //! Main type of event
        uint_fast16_t type = kUnknown;
        //! Sub-type of the event
        uint_fast16_t sub_type = kUnknown;
        //! Targeted MIDI channel
        uint_fast16_t channel = 0;
        //! Is valid event
        uint_fast16_t is_valid = 1;
        //! Reserved 5 bytes padding
        uint_fast16_t padding[4];
        //! Absolute tick position (Used for the tempo calculation only)
        uint64_t absolute_tick_position = 0;
        //! Raw data of this event
        std::vector<uint8_t> data;
    };

    /**
     * @brief A track position event contains a chain of MIDI events until next
     * delay value
     *
     * Created with purpose to sort events by type in the same position
     * (for example, to keep controllers always first than note on events or
     * lower than note-off events)
     */
    class MidiTrackRow
    {
       public:
        MidiTrackRow();
        //! Clear MIDI row data
        void Clear();
        //! Absolute time position in seconds
        double time_;
        //! Delay to next event in ticks
        uint64_t delay_;
        //! Absolute position in ticks
        uint64_t absolute_position_;
        //! Delay to next event in seconds
        double time_delay_;
        //! List of MIDI events in the current row
        std::vector<MidiEvent> events_;
        /**
         * @brief Sort events in this position
         * @param note_states Buffer of currently pressed/released note keys in
         * the track
         */
        void SortEvents(bool *note_states = nullptr);
    };

    /**
     * @brief Tempo change point entry. Used in the MIDI data building function
     * only.
     */
    struct TempoChangePoint
    {
        uint64_t           absolute_position;
        fraction<uint64_t> tempo;
    };

    /**
     * @brief Song position context
     */
    struct Position
    {
        //! Was track began playing
        bool began = false;
        //! Reserved
        char padding[7] = {0, 0, 0, 0, 0, 0, 0};
        //! Waiting time before next event in seconds
        double wait = 0.0;
        //! Absolute time position on the track in seconds
        double absolute_time_position = 0.0;
        //! Track information
        struct TrackInfo
        {
            //! Delay to next event in a track
            uint64_t delay = 0;
            //! Last handled event type
            int32_t lastHandledEvent = 0;
            //! Reserved
            char padding2[4];
            //! MIDI Events queue position iterator
            std::list<MidiTrackRow>::iterator pos;
        };
        std::vector<TrackInfo> track;
    };

    //! MIDI Output interface context
    const MidiRealTimeInterface *midi_output_interface_;

    /**
     * @brief Prepare internal events storage for track data building
     * @param track_count Count of tracks
     */
    void BuildSmfSetupReset(size_t track_count);

    /**
     * @brief Build MIDI track data from the raw track data storage
     * @return true if everything successfully processed, or false on any error
     */
    bool BuildSmfTrackData(const std::vector<std::vector<uint8_t>> &track_data);

    /**
     * @brief Build the time line from off loaded events
     * @param tempos Pre-collected list of tempo events
     * @param loop_start_ticks Global loop start tick (give zero if no global
     * loop presented)
     * @param loop_end_ticks Global loop end tick (give zero if no global loop
     * presented)
     */
    void BuildTimeLine(const std::vector<MidiEvent> &tempos,
                       uint64_t                      loop_start_ticks = 0,
                       uint64_t                      loop_end_ticks   = 0);

    /**
     * @brief Parse one event from raw MIDI track stream
     * @param [_inout] ptr pointer to pointer to current position on the raw
     * data track
     * @param [_in] end address to end of raw track data, needed to validate
     * position and size
     * @param [_inout] status status of the track processing
     * @return Parsed MIDI event entry
     */
    MidiEvent ParseEvent(const uint8_t **ptr, const uint8_t *end, int &status);

    /**
     * @brief Process MIDI events on the current tick moment
     * @param is_seek is a seeking process
     * @return returns false on reaching end of the song
     */
    bool ProcessEvents(bool is_seek = false);

    /**
     * @brief Handle one event from the chain
     * @param tk MIDI track
     * @param evt MIDI event entry
     * @param status Recent event type, -1 returned when end of track event was
     * handled.
     */
    void handleEvent(size_t tk, const MidiEvent &evt, int32_t &status);

   public:
    /**
     * @brief MIDI marker entry
     */
    struct MidiMarkerEntry
    {
        //! Label
        std::string label;
        //! Position time in seconds
        double position_time;
        //! Position time in MIDI ticks
        uint64_t position_ticks;
    };

    /**
     * @brief The FileFormat enum
     */
    enum FileFormat
    {
        //! MIDI format
        kFormatMidi,
        //! Id-Software Music File
        kFormatImf,
        //! EA-MUS format
        kFormatRsxx,
        //! AIL's XMIDI format (act same as MIDI, but with exceptions)
        kFormatXMidi
    };

    /**
     * @brief Format of loop points implemented by CC events
     */
    enum LoopFormat
    {
        kLoopDefault,
        kLoopRpgMaker = 1,
        kLoopEMidi,
        kLoopHmi
    };

   private:
    //! Music file format type. MIDI is default.
    FileFormat midi_format_;
    //! SMF format identifier.
    unsigned midi_smf_format_;
    //! Loop points format
    LoopFormat midi_loop_format_;

    //! Current position
    Position midi_current_position_;
    //! Track begin position
    Position midi_track_begin_position_;
    //! Loop start point
    Position midi_loop_begin_position_;

    //! Is looping enabled or not
    bool midi_loop_enabled_;
    //! Don't process loop: trigger hooks only if they are set
    bool midi_loop_hooks_only_;

    //! Full song length in seconds
    double midi_full_song_time_length_;
    //! Delay after song playd before rejecting the output stream requests
    double midi_post_song_wait_delay_;

    //! Global loop start time
    double midi_loop_start_time_;
    //! Global loop end time
    double midi_loop_end_time_;

    //! Pre-processed track data storage
    std::vector<std::list<MidiTrackRow>> midi_track_data_;

    //! Title of music
    std::string midi_music_title_;
    //! Copyright notice of music
    std::string midi_music_copyright_;
    //! List of track titles
    std::vector<std::string> midi_music_track_titles_;
    //! List of MIDI markers
    std::vector<MidiMarkerEntry> midi_music_markers_;

    //! Time of one tick
    fraction<uint64_t> midi_individual_tick_delta_;
    //! Current tempo
    fraction<uint64_t> midi_tempo_;

    //! Tempo multiplier factor
    double midi_tempo_multiplier_;
    //! Is song at end
    bool midi_at_end_;

    //! Set the number of loops limit. Lesser than 0 - loop infinite
    int midi_loop_count_;

    //! The number of track of multi-track file (for exmaple, XMI) to load
    int midi_load_track_number_;

    //! The XMI-specific list of raw songs, converted into SMF format
    std::vector<std::vector<uint8_t>> midi_raw_songs_data_;

    /**
     * @brief Loop stack entry
     */
    struct LoopStackEntry
    {
        //! is infinite loop
        bool infinity = false;
        //! Count of loops left to break. <0 - infinite loop
        int loops = 0;
        //! Start position snapshot to return back
        Position start_position;
        //! Loop start tick
        uint64_t start = 0;
        //! Loop end tick
        uint64_t end = 0;
    };

    class LoopState
    {
       public:
        //! Loop start has reached
        bool caught_start_;
        //! Loop end has reached, reset on handling
        bool caught_end_;

        //! Loop start has reached
        bool caught_stack_start_;
        //! Loop next has reached, reset on handling
        bool caught_stack_end_;
        //! Loop break has reached, reset on handling
        bool caught_stack_break_;
        //! Skip next stack loop start event handling
        bool skip_stack_start_;

        //! Are loop points invalid?
        bool invalid_loop_; /*Loop points are invalid (loopStart after loopEnd
                             or loopStart and loopEnd are on same place)*/

        //! Is look got temporarily broken because of post-end seek?
        bool temporary_broken_;

        //! How much times the loop should start repeat? For example, if you
        //! want to loop song twice, set value 1
        int loops_count_;

        //! how many loops left until finish the song
        int loops_left_;

        //! Stack of nested loops
        std::vector<LoopStackEntry> stack_;
        //! Current level on the loop stack (<0 - out of loop, 0++ - the index
        //! in the loop stack)
        int stack_level_;

        /**
         * @brief Reset loop state to initial
         */
        void Reset()
        {
            caught_start_       = false;
            caught_end_         = false;
            caught_stack_start_ = false;
            caught_stack_end_   = false;
            caught_stack_break_ = false;
            skip_stack_start_   = false;
            loops_left_         = loops_count_;
        }

        void FullReset()
        {
            loops_count_ = -1;
            Reset();
            invalid_loop_     = false;
            temporary_broken_ = false;
            stack_.clear();
            stack_level_ = -1;
        }

        bool IsStackEnd()
        {
            if (caught_stack_end_ && (stack_level_ >= 0) &&
                (stack_level_ < (int)(stack_.size())))
            {
                const LoopStackEntry &e = stack_[(size_t)(stack_level_)];
                if (e.infinity || (!e.infinity && e.loops > 0)) return true;
            }
            return false;
        }

        void StackUp(int count = 1) { stack_level_ += count; }

        void StackDown(int count = 1) { stack_level_ -= count; }

        LoopStackEntry &GetCurrentStack()
        {
            if ((stack_level_ >= 0) && (stack_level_ < (int)(stack_.size())))
                return stack_[(size_t)(stack_level_)];
            if (stack_.empty())
            {
                LoopStackEntry d;
                d.loops    = 0;
                d.infinity = 0;
                d.start    = 0;
                d.end      = 0;
                stack_.push_back(d);
            }
            return stack_[0];
        }
    };

    LoopState midi_loop_;

    //! Whether the nth track has playback disabled
    std::vector<bool> midi_track_disabled_;
    //! Index of solo track, or max for disabled
    size_t midi_track_solo_;
    //! MIDI channel disable (exception for extra port-prefix-based channels)
    bool m_channelDisable[16];

    /**
     * @brief Handler of callback trigger events
     * @param userdata Pointer to user data (usually, context of something)
     * @param trigger Value of the event which triggered this callback.
     * @param track Identifier of the track which triggered this callback.
     */
    typedef void (*TriggerHandler)(void *userdata, unsigned trigger,
                                   size_t track);

    //! Handler of callback trigger events
    TriggerHandler midi_trigger_handler_;
    //! User data of callback trigger events
    void *midi_trigger_userdata_;

    //! File parsing errors string (adding into midi_error_string_ on aborting
    //! of the process)
    std::string midi_parsing_errors_string_;
    //! Common error string
    std::string midi_error_string_;

    class SequencerTime
    {
       public:
        //! Time buffer
        double time_rest_;
        //! Sample rate
        uint32_t sample_rate_;
        //! Size of one frame in bytes
        uint32_t frame_size_;
        //! Minimum possible delay, granuality
        double minimum_delay_;
        //! Last delay
        double delay_;

        void Init()
        {
            sample_rate_ = 44100;
            frame_size_  = 2;
            Reset();
        }

        void Reset()
        {
            time_rest_     = 0.0;
            minimum_delay_ = 1.0 / (double)(sample_rate_);
            delay_         = 0.0;
        }
    };

    SequencerTime midi_time_;

   public:
    MidiSequencer();
    virtual ~MidiSequencer();

    /**
     * @brief Sets the RT interface
     * @param intrf Pre-Initialized interface structure (pointer will be taken)
     */
    void SetInterface(const MidiRealTimeInterface *intrf);

    /**
     * @brief Runs ticking in a sync with audio streaming. Use this together
     * with onPcmRender hook to easily play MIDI.
     * @param stream pointer to the output PCM stream
     * @param length length of the buffer in bytes
     * @return Count of recorded data in bytes
     */
    int PlayStream(uint8_t *stream, size_t length);

    /**
     * @brief Returns file format type of currently loaded file
     * @return File format type enumeration
     */
    FileFormat GetFormat();

    /**
     * @brief Returns the number of tracks
     * @return Track count
     */
    size_t GetTrackCount() const;

    /**
     * @brief Sets whether a track is playing
     * @param track Track identifier
     * @param enable Whether to enable track playback
     * @return true on success, false if there was no such track
     */
    bool SetTrackEnabled(size_t track, bool enable);

    /**
     * @brief Disable/enable a channel is sounding
     * @param channel Channel number from 0 to 15
     * @param enable Enable the channel playback
     * @return true on success, false if there was no such channel
     */
    bool SetChannelEnabled(size_t channel, bool enable);

    /**
     * @brief Enables or disables solo on a track
     * @param track Identifier of solo track, or max to disable
     */
    void SetSoloTrack(size_t track);

    /**
     * @brief Set the song number of a multi-song file (such as XMI)
     * @param trackNumber Identifier of the song to load (or -1 to mix all songs
     * as one song)
     */
    void SetSongNum(int track);

    /**
     * @brief Retrive the number of songs in a currently opened file
     * @return Number of songs in the file. If 1 or less, means, the file has
     * only one song inside.
     */
    int GetSongsCount();

    /**
     * @brief Defines a handler for callback trigger events
     * @param handler Handler to invoke from the sequencer when triggered, or
     * nullptr.
     * @param userdata Instance of the library
     */
    void SetTriggerHandler(TriggerHandler handler, void *userdata);

    /**
     * @brief Get string that describes reason of error
     * @return Error string
     */
    const std::string &GetErrorString();

    /**
     * @brief Check is loop enabled
     * @return true if loop enabled
     */
    bool GetLoopEnabled();

    /**
     * @brief Switch loop on/off
     * @param enabled Enable loop
     */
    void SetLoopEnabled(bool enabled);

    /**
     * @brief Get the number of loops set
     * @return number of loops or -1 if loop infinite
     */
    int GetLoopsCount();

    /**
     * @brief How many times song should loop
     * @param loops count or -1 to loop infinite
     */
    void SetLoopsCount(int loops);

    /**
     * @brief Switch loop hooks-only mode on/off
     * @param enabled Don't loop: trigger hooks only without loop
     */
    void SetLoopHooksOnly(bool enabled);

    /**
     * @brief Get music title
     * @return music title string
     */
    const std::string &GetMusicTitle();

    /**
     * @brief Get music copyright notice
     * @return music copyright notice string
     */
    const std::string &GetMusicCopyright();

    /**
     * @brief Get list of track titles
     * @return array of track title strings
     */
    const std::vector<std::string> &GetTrackTitles();

    /**
     * @brief Get list of MIDI markers
     * @return Array of MIDI marker structures
     */
    const std::vector<MidiMarkerEntry> &GetMarkers();

    /**
     * @brief Is position of song at end
     * @return true if end of song was reached
     */
    bool PositionAtEnd();

    /**
     * @brief Load MIDI file from a memory block
     * @param data Pointer to memory block with MIDI data
     * @param size Size of source memory block
     * @param rate For IMF formats, the proper playback rate in Hz
     * @return true if file successfully opened, false on any error
     */
    bool LoadMidi(const uint8_t *data, size_t size, uint16_t rate = 0);

    /**
     * @brief Load MIDI file by using FileAndMemReader interface
     * @param mfr mem_file_c with opened source file
     * @param rate For IMF formats, the proper playback rate in Hz
     * @return true if file successfully opened, false on any error
     */
    bool LoadMidi(epi::MemFile *mfr, uint16_t rate);

    /**
     * @brief Periodic tick handler.
     * @param s seconds since last call
     * @param granularity don't expect intervals smaller than this, in seconds
     * @return desired number of seconds until next call
     */
    double Tick(double s, double granularity);

    /**
     * @brief Change current position to specified time position in seconds
     * @param granularity don't expect intervals smaller than this, in seconds
     * @param seconds Absolute time position in seconds
     * @return desired number of seconds until next call of Tick()
     */
    double Seek(double seconds, const double granularity);

    /**
     * @brief Gives current time position in seconds
     * @return Current time position in seconds
     */
    double Tell();

    /**
     * @brief Gives time length of current song in seconds
     * @return Time length of current song in seconds
     */
    double TimeLength();

    /**
     * @brief Gives loop start time position in seconds
     * @return Loop start time position in seconds or -1 if song has no loop
     * points
     */
    double GetLoopStart();

    /**
     * @brief Gives loop end time position in seconds
     * @return Loop end time position in seconds or -1 if song has no loop
     * points
     */
    double GetLoopEnd();

    /**
     * @brief Return to begin of current song
     */
    void Rewind();

    /**
     * @brief Get current tempor multiplier value
     * @return
     */
    double GetTempoMultiplier();

    /**
     * @brief Set tempo multiplier
     * @param tempo Tempo multiplier: 1.0 - original tempo. >1 - faster, <1 -
     * slower
     */
    void SetTempo(double tempo);

   private:
    /**
     * @brief Load file as Id-software-Music-File (Wolfenstein)
     * @param mfr mem_file_c with opened source file
     * @param rate For IMF formats, the proper playback rate in Hz
     * @return true on successful load
     */
    bool ParseImf(epi::MemFile *mfr, uint16_t rate);

    /**
     * @brief Load file as EA MUS
     * @param mfr mem_file_c with opened source file
     * @return true on successful load
     */
    bool ParseRsxx(epi::MemFile *mfr);

    /**
     * @brief Load file as GMD/MUS files (ScummVM)
     * @param mfr mem_file_c with opened source file
     * @return true on successful load
     */
    bool ParseGmf(epi::MemFile *mfr);

    /**
     * @brief Load file as Standard MIDI file
     * @param mfr mem_file_c with opened source file
     * @return true on successful load
     */
    bool ParseSmf(epi::MemFile *mfr);

    /**
     * @brief Load file as RIFF MIDI
     * @param mfr mem_file_c with opened source file
     * @return true on successful load
     */
    bool ParseRmi(epi::MemFile *mfr);

    /**
     * @brief Load file as DMX MUS file (Doom)
     * @param mfr mem_file_c with opened source file
     * @return true on successful load
     */
    bool ParseMus(epi::MemFile *mfr);

    /**
     * @brief Load file as AIL eXtended MIdi
     * @param mfr mem_file_c with opened source file
     * @return true on successful load
     */
    bool ParseXmi(epi::MemFile *mfr);
};