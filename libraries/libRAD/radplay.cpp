/*

    C++ player code for Reality Adlib Tracker 2.0a (file version 2.1).

    Please note, this is just the player code.  This does no checking of the tune data before
    it tries to play it, as most use cases will be a known tune being used in a production.
    So if you're writing an application that loads unknown tunes in at run time then you'll
    want to do more validity checking.

    To use:

        - Instantiate the RADPlayer object

        - Initialise player for your tune by calling the Init() method.  Supply a pointer to the
          tune file and a function for writing to the OPL3 registers.

        - Call the Update() method a number of times per second as returned by GetHertz().  If
          your tune is using the default BPM setting you can safely just call it 50 times a
          second, unless it's a legacy "slow-timer" tune then it'll need to be 18.2 times a
          second.

        - When you're done, stop calling Update() and call the Stop() method to turn off all
          sound and reset the OPL3 hardware.

*/

#include "radplay.h"

//--------------------------------------------------------------------------------------------------
const int8_t   RADPlayer::NoteSize[]       = {0, 2, 1, 3, 1, 3, 2, 4};
const uint16_t RADPlayer::ChanOffsets3[9]  = {0, 1, 2, 0x100, 0x101, 0x102, 6, 7, 8};             // OPL3 first channel
const uint16_t RADPlayer::Chn2Offsets3[9]  = {3, 4, 5, 0x103, 0x104, 0x105, 0x106, 0x107, 0x108}; // OPL3 second channel
const uint16_t RADPlayer::NoteFreq[]       = {0x16b, 0x181, 0x198, 0x1b0, 0x1ca, 0x1e5,
                                              0x202, 0x220, 0x241, 0x263, 0x287, 0x2ae};
const uint16_t RADPlayer::OpOffsets3[9][4] = {
    {0x00B, 0x008, 0x003, 0x000}, {0x00C, 0x009, 0x004, 0x001}, {0x00D, 0x00A, 0x005, 0x002},
    {0x10B, 0x108, 0x103, 0x100}, {0x10C, 0x109, 0x104, 0x101}, {0x10D, 0x10A, 0x105, 0x102},
    {0x113, 0x110, 0x013, 0x010}, {0x114, 0x111, 0x014, 0x011}, {0x115, 0x112, 0x015, 0x012}};
const bool RADPlayer::AlgCarriers[7][4] = {
    {true, false, false, false}, // 0 - 2op - op < op
    {true, true, false, false},  // 1 - 2op - op + op
    {true, false, false, false}, // 2 - 4op - op < op < op < op
    {true, false, false, true},  // 3 - 4op - op < op < op + op
    {true, false, true, false},  // 4 - 4op - op < op + op < op
    {true, false, true, true},   // 5 - 4op - op < op + op + op
    {true, true, true, true},    // 6 - 4op - op + op + op + op
};

//==================================================================================================
// Initialise a RAD tune for playback.  This assumes the tune data is valid and does minimal data
// checking.
//==================================================================================================
void RADPlayer::Init(const void *tune, void (*opl3)(void *, uint16_t, uint8_t), void *arg)
{

    Initialised = false;

    // Version check; we only support version 2.1 tune files
    if (*((uint8_t *)tune + 0x10) != 0x21)
    {
        Hertz = -1;
        return;
    }

    // The OPL3 call-back
    OPL3    = opl3;
    OPL3Arg = arg;

    for (int i = 0; i < kTracks; i++)
        Tracks[i] = 0;

    for (int i = 0; i < kRiffTracks; i++)
        for (int j = 0; j < kChannels; j++)
            Riffs[i][j] = 0;

    uint8_t *s = (uint8_t *)tune + 0x11;

    uint8_t flags = *s++;
    Speed         = flags & 0x1F;

    // Is BPM value present?
    Hertz = 50;
    if (flags & 0x20)
    {
        Hertz = (s[0] | (int(s[1]) << 8)) * 2 / 5;
        s += 2;
    }

    // Slow timer tune?  Return an approximate hz
    if (flags & 0x40)
        Hertz = 18;

    // Skip any description
    while (*s)
        s++;
    s++;

    // Unpack the instruments
    while (1)
    {

        // Instrument number, 0 indicates end of list
        uint8_t inst_num = *s++;
        if (inst_num == 0)
            break;

        // Skip instrument name
        s += *s++;

        CInstrument &inst = Instruments[inst_num - 1];

        uint8_t alg     = *s++;
        inst.Algorithm  = alg & 7;
        inst.Panning[0] = (alg >> 3) & 3;
        inst.Panning[1] = (alg >> 5) & 3;

        if (inst.Algorithm < 7)
        {

            uint8_t b        = *s++;
            inst.Feedback[0] = b & 15;
            inst.Feedback[1] = b >> 4;

            b              = *s++;
            inst.Detune    = b >> 4;
            inst.RiffSpeed = b & 15;

            inst.Volume = *s++;

            for (int i = 0; i < 4; i++)
            {
                uint8_t *op = inst.Operators[i];
                for (int j = 0; j < 5; j++)
                    op[j] = *s++;
            }
        }
        else
        {

            // Ignore MIDI instrument data
            s += 6;
        }

        // Instrument riff?
        if (alg & 0x80)
        {
            int size = s[0] | (int(s[1]) << 8);
            s += 2;
            inst.Riff = s;
            s += size;
        }
        else
            inst.Riff = 0;
    }

    // Get order list
    OrderListSize = *s++;
    OrderList     = s;
    s += OrderListSize;

    // Locate the tracks
    while (1)
    {

        // Track number
        uint8_t track_num = *s++;
        if (track_num >= kTracks)
            break;

        // Track size in bytes
        int size = s[0] | (int(s[1]) << 8);
        s += 2;

        Tracks[track_num] = s;
        s += size;
    }

    // Locate the riffs
    while (1)
    {

        // Riff id
        uint8_t riffid  = *s++;
        uint8_t riffnum = riffid >> 4;
        uint8_t channum = riffid & 15;
        if (riffnum >= kRiffTracks || channum > kChannels)
            break;

        // Track size in bytes
        int size = s[0] | (int(s[1]) << 8);
        s += 2;

        Riffs[riffnum][channum - 1] = s;
        s += size;
    }

    // Done parsing tune, now set up for play
    for (int i = 0; i < 512; i++)
        OPL3Regs[i] = 255;
    Stop();

    Initialised = true;
}

//==================================================================================================
// Stop all sounds and reset the tune.  Tune will play from the beginning again if you continue to
// Update().
//==================================================================================================
void RADPlayer::Stop()
{

    // Clear all registers
    for (uint16_t reg = 0x20; reg < 0xF6; reg++)
    {

        // Ensure envelopes decay all the way
        uint8_t val = (reg >= 0x60 && reg < 0xA0) ? 0xFF : 0;

        SetOPL3(reg, val);
        SetOPL3(reg + 0x100, val);
    }

    // Configure OPL3
    SetOPL3(1, 0x20);  // Allow waveforms
    SetOPL3(8, 0);     // No split point
    SetOPL3(0xbd, 0);  // No drums, etc.
    SetOPL3(0x104, 0); // Everything 2-op by default
    SetOPL3(0x105, 1); // OPL3 mode on

    // The order map keeps track of which patterns we've played so we can detect when the tune
    // starts to repeat.  Jump markers can't be reliably used for this
    PlayTime  = 0;
    Repeating = false;
    for (int i = 0; i < 4; i++)
        OrderMap[i] = 0;

    // Initialise play values
    SpeedCnt  = 1;
    Order     = 0;
    Track     = GetTrack();
    Line      = 0;
    Entrances = 0;
    MasterVol = 64;

    // Initialise channels
    for (int i = 0; i < kChannels; i++)
    {
        CChannel &chan      = Channels[i];
        chan.LastInstrument = 0;
        chan.Instrument     = 0;
        chan.Volume         = 0;
        chan.DetuneA        = 0;
        chan.DetuneB        = 0;
        chan.KeyFlags       = 0;
        chan.Riff.SpeedCnt  = 0;
        chan.IRiff.SpeedCnt = 0;
    }
}

//==================================================================================================
// Playback update.  Call BPM * 2 / 5 times a second.  Use GetHertz() for this number after the
// tune has been initialised.  Returns true if tune is starting to repeat.
//==================================================================================================
bool RADPlayer::Update()
{

    if (!Initialised)
        return false;

    // Run riffs
    for (int i = 0; i < kChannels; i++)
    {
        CChannel &chan = Channels[i];
        TickRiff(i, chan.IRiff, false);
        TickRiff(i, chan.Riff, true);
    }

    // Run main track
    PlayLine();

    // Run effects
    for (int i = 0; i < kChannels; i++)
    {
        CChannel &chan = Channels[i];
        ContinueFX(i, &chan.IRiff.FX);
        ContinueFX(i, &chan.Riff.FX);
        ContinueFX(i, &chan.FX);
    }

    // Update play time.  We convert to seconds when queried
    PlayTime++;

    return Repeating;
}

//==================================================================================================
// Unpacks a single RAD note.
//==================================================================================================
bool RADPlayer::UnpackNote(uint8_t *&s, uint8_t &last_instrument)
{

    uint8_t chanid = *s++;

    InstNum   = 0;
    EffectNum = 0;
    Param     = 0;

    // Unpack note data
    uint8_t note = 0;
    if (chanid & 0x40)
    {
        uint8_t n = *s++;
        note      = n & 0x7F;

        // Retrigger last instrument?
        if (n & 0x80)
            InstNum = last_instrument;
    }

    // Do we have an instrument?
    if (chanid & 0x20)
    {
        InstNum         = *s++;
        last_instrument = InstNum;
    }

    // Do we have an effect?
    if (chanid & 0x10)
    {
        EffectNum = *s++;
        Param     = *s++;
    }

    NoteNum   = note & 15;
    OctaveNum = note >> 4;

    return ((chanid & 0x80) != 0);
}

//==================================================================================================
// Get current track as indicated by order list.
//==================================================================================================
uint8_t *RADPlayer::GetTrack()
{

    // If at end of tune start again from beginning
    if (Order >= OrderListSize)
        Order = 0;

    uint8_t track_num = OrderList[Order];

    // Jump marker?  Note, we don't recognise multiple jump markers as that could put us into an
    // infinite loop
    if (track_num & 0x80)
    {
        Order     = track_num & 0x7F;
        track_num = OrderList[Order] & 0x7F;
    }

    // Check for tune repeat, and mark order in order map
    if (Order < 128)
    {
        int      byte = Order >> 5;
        uint32_t bit  = uint32_t(1) << (Order & 31);
        if (OrderMap[byte] & bit)
            Repeating = true;
        else
            OrderMap[byte] |= bit;
    }

    return Tracks[track_num];
}

//==================================================================================================
// Skip through track till we reach the given line or the next higher one.  Returns null if none.
//==================================================================================================
uint8_t *RADPlayer::SkipToLine(uint8_t *trk, uint8_t linenum, bool chan_riff)
{

    while (1)
    {

        uint8_t lineid = *trk;
        if ((lineid & 0x7F) >= linenum)
            return trk;
        if (lineid & 0x80)
            break;
        trk++;

        // Skip channel notes
        uint8_t chanid;
        do
        {
            chanid = *trk++;
            trk += NoteSize[(chanid >> 4) & 7];
        } while (!(chanid & 0x80) && !chan_riff);
    }

    return 0;
}

//==================================================================================================
// Plays one line of current track and advances pointers.
//==================================================================================================
void RADPlayer::PlayLine()
{

    SpeedCnt--;
    if (SpeedCnt > 0)
        return;
    SpeedCnt = Speed;

    // Reset channel effects
    for (int i = 0; i < kChannels; i++)
        ResetFX(&Channels[i].FX);

    LineJump = -1;

    // At the right line?
    uint8_t *trk = Track;
    if (trk && (*trk & 0x7F) <= Line)
    {
        uint8_t lineid = *trk++;

        // Run through channels
        bool last;
        do
        {
            int       channum = *trk & 15;
            CChannel &chan    = Channels[channum];
            last              = UnpackNote(trk, chan.LastInstrument);
            PlayNote(channum, NoteNum, OctaveNum, InstNum, EffectNum, Param);
        } while (!last);

        // Was this the last line?
        if (lineid & 0x80)
            trk = 0;

        Track = trk;
    }

    // Move to next line
    Line++;
    if (Line >= kTrackLines || LineJump >= 0)
    {

        if (LineJump >= 0)
            Line = LineJump;
        else
            Line = 0;

        // Move to next track in order list
        Order++;
        Track = GetTrack();
    }
}

//==================================================================================================
// Play a single note.  Returns the line number in the next pattern to jump to if a jump command was
// found, or -1 if none.
//==================================================================================================
void RADPlayer::PlayNote(int channum, int8_t notenum, int8_t octave, uint16_t instnum, uint8_t cmd, uint8_t param,
                         e_Source src, int op)
{
    CChannel &chan = Channels[channum];

    // Recursion detector.  This is needed as riffs can trigger other riffs, and they could end up
    // in a loop
    if (Entrances >= 8)
        return;
    Entrances++;

    // Select which effects source we're using
    CEffects *fx = &chan.FX;
    if (src == SRiff)
        fx = &chan.Riff.FX;
    else if (src == SIRiff)
        fx = &chan.IRiff.FX;

    bool transposing = false;

    // For tone-slides the note is the target
    if (cmd == cmToneSlide)
    {
        if (notenum > 0 && notenum <= 12)
        {
            fx->ToneSlideOct  = octave;
            fx->ToneSlideFreq = NoteFreq[notenum - 1];
        }
        goto toneslide;
    }

    // Playing a new instrument?
    if (instnum > 0)
    {
        CInstrument *oldinst = chan.Instrument;
        CInstrument *inst    = &Instruments[instnum - 1];
        chan.Instrument      = inst;

        // Ignore MIDI instruments
        if (inst->Algorithm == 7)
        {
            Entrances--;
            return;
        }

        LoadInstrumentOPL3(channum);

        // Bounce the channel
        chan.KeyFlags |= fKeyOff | fKeyOn;

        ResetFX(&chan.IRiff.FX);

        if (src != SIRiff || inst != oldinst)
        {

            // Instrument riff?
            if (inst->Riff && inst->RiffSpeed > 0)
            {

                chan.IRiff.Track = chan.IRiff.TrackStart = inst->Riff;
                chan.IRiff.Line                          = 0;
                chan.IRiff.Speed                         = inst->RiffSpeed;
                chan.IRiff.LastInstrument                = 0;

                // Note given with riff command is used to transpose the riff
                if (notenum >= 1 && notenum <= 12)
                {
                    chan.IRiff.TransposeOctave = octave;
                    chan.IRiff.TransposeNote   = notenum;
                    transposing                = true;
                }
                else
                {
                    chan.IRiff.TransposeOctave = 3;
                    chan.IRiff.TransposeNote   = 12;
                }

                // Do first tick of riff
                chan.IRiff.SpeedCnt = 1;
                TickRiff(channum, chan.IRiff, false);
            }
            else
                chan.IRiff.SpeedCnt = 0;
        }
    }

    // Starting a channel riff?
    if (cmd == cmRiff || cmd == cmTranspose)
    {

        ResetFX(&chan.Riff.FX);

        uint8_t p0      = param / 10;
        uint8_t p1      = param % 10;
        chan.Riff.Track = p1 > 0 ? Riffs[p0][p1 - 1] : 0;
        if (chan.Riff.Track)
        {

            chan.Riff.TrackStart     = chan.Riff.Track;
            chan.Riff.Line           = 0;
            chan.Riff.Speed          = Speed;
            chan.Riff.LastInstrument = 0;

            // Note given with riff command is used to transpose the riff
            if (cmd == cmTranspose && notenum >= 1 && notenum <= 12)
            {
                chan.Riff.TransposeOctave = octave;
                chan.Riff.TransposeNote   = notenum;
                transposing               = true;
            }
            else
            {
                chan.Riff.TransposeOctave = 3;
                chan.Riff.TransposeNote   = 12;
            }

            // Do first tick of riff
            chan.Riff.SpeedCnt = 1;
            TickRiff(channum, chan.Riff, true);
        }
        else
            chan.Riff.SpeedCnt = 0;
    }

    // Play the note
    if (!transposing && notenum > 0)
    {

        // Key-off?
        if (notenum == 15)
            chan.KeyFlags |= fKeyOff;

        if (!chan.Instrument || chan.Instrument->Algorithm < 7)
            PlayNoteOPL3(channum, octave, notenum);
    }

    // Process effect
    switch (cmd)
    {

    case cmSetVol:
        SetVolume(channum, param);
        break;

    case cmSetSpeed:
        if (src == SNone)
        {
            Speed    = param;
            SpeedCnt = param;
        }
        else if (src == SRiff)
        {
            chan.Riff.Speed    = param;
            chan.Riff.SpeedCnt = param;
        }
        else if (src == SIRiff)
        {
            chan.IRiff.Speed    = param;
            chan.IRiff.SpeedCnt = param;
        }
        break;

    case cmPortamentoUp:
        fx->PortSlide = param;
        break;

    case cmPortamentoDwn:
        fx->PortSlide = -int8_t(param);
        break;

    case cmToneVolSlide:
    case cmVolSlide: {
        int8_t val = param;
        if (val >= 50)
            val = -(val - 50);
        fx->VolSlide = val;
        if (cmd != cmToneVolSlide)
            break;
    }
        // Fall through!

    case cmToneSlide: {
    toneslide:
        uint8_t speed = param;
        if (speed)
            fx->ToneSlideSpeed = speed;
        GetSlideDir(channum, fx);
        break;
    }

    case cmJumpToLine: {
        if (param >= kTrackLines)
            break;

        // Note: jump commands in riffs are checked for within TickRiff()
        if (src == SNone)
            LineJump = param;

        break;
    }

    case cmMultiplier: {
        if (src == SIRiff)
            LoadInstMultiplierOPL3(channum, op, param);
        break;
    }

    case cmVolume: {
        if (src == SIRiff)
            LoadInstVolumeOPL3(channum, op, param);
        break;
    }

    case cmFeedback: {
        if (src == SIRiff)
        {
            uint8_t which = param / 10;
            uint8_t fb    = param % 10;
            LoadInstFeedbackOPL3(channum, which, fb);
        }
        break;
    }
    }

    Entrances--;
}

//==================================================================================================
// Sets the OPL3 registers for a given instrument.
//==================================================================================================
void RADPlayer::LoadInstrumentOPL3(int channum)
{
    CChannel &chan = Channels[channum];

    const CInstrument *inst = chan.Instrument;
    if (!inst)
        return;

    uint8_t alg  = inst->Algorithm;
    chan.Volume  = inst->Volume;
    chan.DetuneA = (inst->Detune + 1) >> 1;
    chan.DetuneB = inst->Detune >> 1;

    // Turn on 4-op mode for algorithms 2 and 3 (algorithms 4 to 6 are simulated with 2-op mode)
    if (channum < 6)
    {
        uint8_t mask = 1 << channum;
        SetOPL3(0x104, (GetOPL3(0x104) & ~mask) | (alg == 2 || alg == 3 ? mask : 0));
    }

    // Left/right/feedback/algorithm
    SetOPL3(0xC0 + ChanOffsets3[channum],
            ((inst->Panning[1] ^ 3) << 4) | inst->Feedback[1] << 1 | (alg == 3 || alg == 5 || alg == 6 ? 1 : 0));
    SetOPL3(0xC0 + Chn2Offsets3[channum],
            ((inst->Panning[0] ^ 3) << 4) | inst->Feedback[0] << 1 | (alg == 1 || alg == 6 ? 1 : 0));

    // Load the operators
    for (int i = 0; i < 4; i++)
    {

        static const uint8_t blank[] = {0, 0x3F, 0, 0xF0, 0};
        const uint8_t       *op      = (alg < 2 && i >= 2) ? blank : inst->Operators[i];
        uint16_t             reg     = OpOffsets3[channum][i];

        uint16_t vol = ~op[1] & 0x3F;

        // Do volume scaling for carriers
        if (AlgCarriers[alg][i])
        {
            vol = vol * inst->Volume / 64;
            vol = vol * MasterVol / 64;
        }

        SetOPL3(reg + 0x20, op[0]);
        SetOPL3(reg + 0x40, (op[1] & 0xC0) | ((vol ^ 0x3F) & 0x3F));
        SetOPL3(reg + 0x60, op[2]);
        SetOPL3(reg + 0x80, op[3]);
        SetOPL3(reg + 0xE0, op[4]);
    }
}

//==================================================================================================
// Play note on OPL3 hardware.
//==================================================================================================
void RADPlayer::PlayNoteOPL3(int channum, int8_t octave, int8_t note)
{
    CChannel &chan = Channels[channum];

    uint16_t o1 = ChanOffsets3[channum];
    uint16_t o2 = Chn2Offsets3[channum];

    // Key off the channel
    if (chan.KeyFlags & fKeyOff)
    {
        chan.KeyFlags &= ~(fKeyOff | fKeyedOn);
        SetOPL3(0xB0 + o1, GetOPL3(0xB0 + o1) & ~0x20);
        SetOPL3(0xB0 + o2, GetOPL3(0xB0 + o2) & ~0x20);
    }

    if (note == 15)
        return;

    bool op4 = (chan.Instrument && chan.Instrument->Algorithm >= 2);

    uint16_t freq = NoteFreq[note - 1];
    uint16_t frq2 = freq;

    chan.CurrFreq   = freq;
    chan.CurrOctave = octave;

    // Detune.  We detune both channels in the opposite direction so the note retains its tuning
    freq += chan.DetuneA;
    frq2 -= chan.DetuneB;

    // Frequency low byte
    if (op4)
        SetOPL3(0xA0 + o1, frq2 & 0xFF);
    SetOPL3(0xA0 + o2, freq & 0xFF);

    // Frequency high bits + octave + key on
    if (chan.KeyFlags & fKeyOn)
        chan.KeyFlags = (chan.KeyFlags & ~fKeyOn) | fKeyedOn;
    if (op4)
        SetOPL3(0xB0 + o1, (frq2 >> 8) | (octave << 2) | ((chan.KeyFlags & fKeyedOn) ? 0x20 : 0));
    else
        SetOPL3(0xB0 + o1, 0);
    SetOPL3(0xB0 + o2, (freq >> 8) | (octave << 2) | ((chan.KeyFlags & fKeyedOn) ? 0x20 : 0));
}

//==================================================================================================
// Prepare FX for new line.
//==================================================================================================
void RADPlayer::ResetFX(CEffects *fx)
{
    fx->PortSlide    = 0;
    fx->VolSlide     = 0;
    fx->ToneSlideDir = 0;
}

//==================================================================================================
// Tick the channel riff.
//==================================================================================================
void RADPlayer::TickRiff(int channum, CChannel::CRiff &riff, bool chan_riff)
{
    uint8_t lineid;

    if (riff.SpeedCnt == 0)
    {
        ResetFX(&riff.FX);
        return;
    }

    riff.SpeedCnt--;
    if (riff.SpeedCnt > 0)
        return;
    riff.SpeedCnt = riff.Speed;

    uint8_t line = riff.Line++;
    if (riff.Line >= kTrackLines)
        riff.SpeedCnt = 0;

    ResetFX(&riff.FX);

    // Is this the current line in track?
    uint8_t *trk = riff.Track;
    if (trk && (*trk & 0x7F) == line)
    {
        lineid = *trk++;

        if (chan_riff)
        {

            // Channel riff: play current note
            UnpackNote(trk, riff.LastInstrument);
            Transpose(riff.TransposeNote, riff.TransposeOctave);
            PlayNote(channum, NoteNum, OctaveNum, InstNum, EffectNum, Param, SRiff);
        }
        else
        {

            // Instrument riff: here each track channel is an extra effect that can run, but is not
            // actually a different physical channel
            bool last;
            do
            {
                int col = *trk & 15;
                last    = UnpackNote(trk, riff.LastInstrument);
                if (EffectNum != cmIgnore)
                    Transpose(riff.TransposeNote, riff.TransposeOctave);
                PlayNote(channum, NoteNum, OctaveNum, InstNum, EffectNum, Param, SIRiff, col > 0 ? (col - 1) & 3 : 0);
            } while (!last);
        }

        // Last line?
        if (lineid & 0x80)
            trk = 0;

        riff.Track = trk;
    }

    // Special case; if next line has a jump command, run it now
    if (!trk || (*trk++ & 0x7F) != riff.Line)
        return;

    UnpackNote(trk, lineid); // lineid is just a dummy here
    if (EffectNum == cmJumpToLine && Param < kTrackLines)
    {
        riff.Line  = Param;
        riff.Track = SkipToLine(riff.TrackStart, Param, chan_riff);
    }
}

//==================================================================================================
// This continues any effects that operate continuously (eg. slides).
//==================================================================================================
void RADPlayer::ContinueFX(int channum, CEffects *fx)
{
    CChannel &chan = Channels[channum];

    if (fx->PortSlide)
        Portamento(channum, fx, fx->PortSlide, false);

    if (fx->VolSlide)
    {
        int8_t vol = chan.Volume;
        vol -= fx->VolSlide;
        if (vol < 0)
            vol = 0;
        SetVolume(channum, vol);
    }

    if (fx->ToneSlideDir)
        Portamento(channum, fx, fx->ToneSlideDir, true);
}

//==================================================================================================
// Sets the volume of given channel.
//==================================================================================================
void RADPlayer::SetVolume(int channum, uint8_t vol)
{
    CChannel &chan = Channels[channum];

    // Ensure volume is within range
    if (vol > 64)
        vol = 64;

    chan.Volume = vol;

    // Scale volume to master volume
    vol = vol * MasterVol / 64;

    CInstrument *inst = chan.Instrument;
    if (!inst)
        return;
    uint8_t alg = inst->Algorithm;

    // Set volume of all carriers
    for (int i = 0; i < 4; i++)
    {
        uint8_t *op = inst->Operators[i];

        // Is this operator a carrier?
        if (!AlgCarriers[alg][i])
            continue;

        uint8_t  opvol = uint16_t((op[1] & 63) ^ 63) * vol / 64;
        uint16_t reg   = 0x40 + OpOffsets3[channum][i];
        SetOPL3(reg, (GetOPL3(reg) & 0xC0) | (opvol ^ 0x3F));
    }
}

//==================================================================================================
// Starts a tone-slide.
//==================================================================================================
void RADPlayer::GetSlideDir(int channum, CEffects *fx)
{
    CChannel &chan = Channels[channum];

    int8_t speed = fx->ToneSlideSpeed;
    if (speed > 0)
    {
        uint8_t  oct  = fx->ToneSlideOct;
        uint16_t freq = fx->ToneSlideFreq;

        uint16_t oldfreq = chan.CurrFreq;
        uint8_t  oldoct  = chan.CurrOctave;

        if (oldoct > oct)
            speed = -speed;
        else if (oldoct == oct)
        {
            if (oldfreq > freq)
                speed = -speed;
            else if (oldfreq == freq)
                speed = 0;
        }
    }

    fx->ToneSlideDir = speed;
}

//==================================================================================================
// Load multiplier value into operator.
//==================================================================================================
void RADPlayer::LoadInstMultiplierOPL3(int channum, int op, uint8_t mult)
{
    uint16_t reg = 0x20 + OpOffsets3[channum][op];
    SetOPL3(reg, (GetOPL3(reg) & 0xF0) | (mult & 15));
}

//==================================================================================================
// Load volume value into operator.
//==================================================================================================
void RADPlayer::LoadInstVolumeOPL3(int channum, int op, uint8_t vol)
{
    uint16_t reg = 0x40 + OpOffsets3[channum][op];
    SetOPL3(reg, (GetOPL3(reg) & 0xC0) | ((vol & 0x3F) ^ 0x3F));
}

//==================================================================================================
// Load feedback value into instrument.
//==================================================================================================
void RADPlayer::LoadInstFeedbackOPL3(int channum, int which, uint8_t fb)
{

    if (which == 0)
    {

        uint16_t reg = 0xC0 + Chn2Offsets3[channum];
        SetOPL3(reg, (GetOPL3(reg) & 0x31) | ((fb & 7) << 1));
    }
    else if (which == 1)
    {

        uint16_t reg = 0xC0 + ChanOffsets3[channum];
        SetOPL3(reg, (GetOPL3(reg) & 0x31) | ((fb & 7) << 1));
    }
}

//==================================================================================================
// This adjusts the pitch of the given channel's note.  There may also be a limiting value on the
// portamento (for tone slides).
//==================================================================================================
void RADPlayer::Portamento(uint16_t channum, CEffects *fx, int8_t amount, bool toneslide)
{
    CChannel &chan = Channels[channum];

    uint16_t freq = chan.CurrFreq;
    uint8_t  oct  = chan.CurrOctave;

    freq += amount;

    if (freq < 0x156)
    {

        if (oct > 0)
        {
            oct--;
            freq += 0x2AE - 0x156;
        }
        else
            freq = 0x156;
    }
    else if (freq > 0x2AE)
    {

        if (oct < 7)
        {
            oct++;
            freq -= 0x2AE - 0x156;
        }
        else
            freq = 0x2AE;
    }

    if (toneslide)
    {

        if (amount >= 0)
        {

            if (oct > fx->ToneSlideOct || (oct == fx->ToneSlideOct && freq >= fx->ToneSlideFreq))
            {
                freq = fx->ToneSlideFreq;
                oct  = fx->ToneSlideOct;
            }
        }
        else
        {

            if (oct < fx->ToneSlideOct || (oct == fx->ToneSlideOct && freq <= fx->ToneSlideFreq))
            {
                freq = fx->ToneSlideFreq;
                oct  = fx->ToneSlideOct;
            }
        }
    }

    chan.CurrFreq   = freq;
    chan.CurrOctave = oct;

    // Apply detunes
    uint16_t frq2 = freq - chan.DetuneB;
    freq += chan.DetuneA;

    // Write value back to OPL3
    uint16_t chan_offset = Chn2Offsets3[channum];
    SetOPL3(0xA0 + chan_offset, freq & 0xFF);
    SetOPL3(0xB0 + chan_offset, (freq >> 8 & 3) | oct << 2 | (GetOPL3(0xB0 + chan_offset) & 0xE0));

    chan_offset = ChanOffsets3[channum];
    SetOPL3(0xA0 + chan_offset, frq2 & 0xFF);
    SetOPL3(0xB0 + chan_offset, (frq2 >> 8 & 3) | oct << 2 | (GetOPL3(0xB0 + chan_offset) & 0xE0));
}

//==================================================================================================
// Transpose the note returned by UnpackNote().
// Note: due to RAD's wonky legacy middle C is octave 3 note number 12.
//==================================================================================================
void RADPlayer::Transpose(int8_t note, int8_t octave)
{

    if (NoteNum >= 1 && NoteNum <= 12)
    {

        int8_t toct = octave - 3;
        if (toct != 0)
        {
            OctaveNum += toct;
            if (OctaveNum < 0)
                OctaveNum = 0;
            else if (OctaveNum > 7)
                OctaveNum = 7;
        }

        int8_t tnot = note - 12;
        if (tnot != 0)
        {
            NoteNum += tnot;
            if (NoteNum < 1)
            {
                NoteNum += 12;
                if (OctaveNum > 0)
                    OctaveNum--;
                else
                    NoteNum = 1;
            }
        }
    }
}

//==================================================================================================
// The error strings are all supplied here in case you want to translate them to another language
// (or supply your own more descriptive error messages).
//==================================================================================================
static const char *g_RADNotATuneFile       = "Not a RAD tune file.";
static const char *g_RADNotAVersion21Tune  = "Not a version 2.1 file format RAD tune.";
static const char *g_RADTruncated          = "Tune file has been truncated and is incomplete.";
static const char *g_RADBadFlags           = "Tune file has invalid flags.";
static const char *g_RADBadBPMValue        = "Tune's BPM value is out of range.";
static const char *g_RADBadInstrument      = "Tune file contains a bad instrument definition.";
static const char *g_RADUnknownMIDIVersion = "Tune file contains an unknown MIDI instrument version.";
static const char *g_RADOrderListTooLarge  = "Order list in tune file is an invalid size.";
static const char *g_RADBadJumpMarker      = "Order list jump marker is invalid.";
static const char *g_RADBadOrderEntry      = "Order list entry is invalid.";
static const char *g_RADBadPattNum         = "Tune file contains a bad pattern index.";
static const char *g_RADPattTruncated      = "Tune file contains a truncated pattern.";
static const char *g_RADPattExtraData      = "Tune file contains a pattern with extraneous data.";
static const char *g_RADPattBadLineNum     = "Tune file contains a pattern with a bad line definition.";
static const char *g_RADPattBadChanNum     = "Tune file contains a pattern with a bad channel definition.";
static const char *g_RADPattBadNoteNum     = "Pattern contains a bad note number.";
static const char *g_RADPattBadInstNum     = "Pattern contains a bad instrument number.";
static const char *g_RADPattBadEffect      = "Pattern contains a bad effect and/or parameter.";
static const char *g_RADBadRiffNum         = "Tune file contains a bad riff index.";
static const char *g_RADExtraBytes         = "Tune file contains extra bytes.";

//==================================================================================================
// Validate a RAD V2 (file format 2.1) tune file.  Note, this uses no C++ standard library code.
//==================================================================================================
static const char *RADCheckPattern(const uint8_t *&s, const uint8_t *e, bool riff)
{

    // Get pattern size
    if (s + 2 > e)
        return g_RADTruncated;
    uint16_t pattsize = s[0] | (uint16_t(s[1]) << 8);
    s += 2;

    // Calculate end of pattern
    const uint8_t *pe = s + pattsize;
    if (pe > e)
        return g_RADTruncated;

    uint8_t linedef, chandef;
    do
    {

        // Check line of pattern
        if (s >= pe)
            return g_RADPattTruncated;
        linedef         = *s++;
        uint8_t linenum = linedef & 0x7F;
        if (linenum >= 64)
            return g_RADPattBadLineNum;

        do
        {

            // Check channel of pattern
            if (s >= pe)
                return g_RADPattTruncated;
            chandef         = *s++;
            uint8_t channum = chandef & 0x0F;
            if (!riff && channum >= 9)
                return g_RADPattBadChanNum;

            // Check note
            if (chandef & 0x40)
            {
                if (s >= pe)
                    return g_RADPattTruncated;
                uint8_t note    = *s++;
                uint8_t notenum = note & 15;
                if (notenum == 0 || notenum == 13 || notenum == 14)
                    return g_RADPattBadNoteNum;
            }

            // Check instrument.  This shouldn't be supplied if bit 7 of the note byte is set,
            // but it doesn't break anything if it is so we don't check for it
            if (chandef & 0x20)
            {
                if (s >= pe)
                    return g_RADPattTruncated;
                uint8_t inst = *s++;
                if (inst == 0 || inst >= 128)
                    return g_RADPattBadInstNum;
            }

            // Check effect.  A non-existent effect could be supplied, but it'll just be
            // ignored by the player so we don't care
            if (chandef & 0x10)
            {
                if (s + 2 > pe)
                    return g_RADPattTruncated;
                uint8_t effect = *s++;
                uint8_t param  = *s++;
                if (effect > 31 || param > 99)
                    return g_RADPattBadEffect;
            }

        } while (!(chandef & 0x80));

    } while (!(linedef & 0x80));

    if (s != pe)
        return g_RADPattExtraData;

    return 0;
}
//--------------------------------------------------------------------------------------------------
const char *RADValidate(const void *data, size_t data_size)
{

    const uint8_t *s = (const uint8_t *)data;
    const uint8_t *e = s + data_size;

    // Check header
    if (data_size < 16)
        return g_RADNotATuneFile;

    const char *hdrtxt = "RAD by REALiTY!!";
    for (int i = 0; i < 16; i++)
        if (char(*s++) != *hdrtxt++)
            return g_RADNotATuneFile;

    // Check version
    if (s >= e || *s++ != 0x21)
        return g_RADNotAVersion21Tune;

    // Check flags
    if (s >= e)
        return g_RADTruncated;

    uint8_t flags = *s++;
    if (flags & 0x80)
        return g_RADBadFlags; // Bit 7 is unused

    if (flags & 0x40)
    {
        if (s + 2 > e)
            return g_RADTruncated;
        uint16_t bpm = s[0] | (uint16_t(s[1]) << 8);
        s += 2;
        if (bpm < 46 || bpm > 300)
            return g_RADBadBPMValue;
    }

    // Check description.  This is actually freeform text so there's not a lot to check, just that
    // it's a null-terminated string
    do
    {
        if (s >= e)
            return g_RADTruncated;
    } while (*s++);

    // Check instruments.  We don't actually validate the individual instrument fields as the tune
    // file will still play with bad instrument data.  We're only concerned that the tune file
    // doesn't crash the player
    uint8_t last_inst = 0;
    while (1)
    {

        // Get instrument number, or 0 for end of instrument list
        if (s >= e)
            return g_RADTruncated;
        uint8_t inst = *s++;
        if (inst == 0)
            break;

        // RAD always saves the instruments out in order
        if (inst > 127 || inst <= last_inst)
            return g_RADBadInstrument;
        last_inst = inst;

        // Check the name
        if (s >= e)
            return g_RADTruncated;
        uint8_t namelen = *s++;
        s += namelen;

        // Get algorithm
        if (s > e)
            return g_RADTruncated;
        uint8_t alg = *s;

        if ((alg & 7) == 7)
        {

            // MIDI instrument.  We need to check the version as this can affect the following
            // data size
            if (s + 6 > e)
                return g_RADTruncated;
            if (s[2] >> 4)
                return g_RADUnknownMIDIVersion;
            s += 6;
        }
        else
        {

            s += 24;
            if (s > e)
                return g_RADTruncated;
        }

        // Riff track supplied?
        if (alg & 0x80)
        {

            const char *err = RADCheckPattern(s, e, false);
            if (err)
                return err;
        }
    }

    // Get the order list
    if (s >= e)
        return g_RADTruncated;
    uint8_t        order_size = *s++;
    const uint8_t *order_list = s;
    if (order_size > 128)
        return g_RADOrderListTooLarge;
    s += order_size;

    for (uint8_t i = 0; i < order_size; i++)
    {
        uint8_t order = order_list[i];

        if (order & 0x80)
        {

            // Check jump marker
            order &= 0x7F;
            if (order >= order_size)
                return g_RADBadJumpMarker;
        }
        else
        {

            // Check pattern number.  It doesn't matter if there is no pattern with this number
            // defined later, as missing patterns are treated as empty
            if (order >= 100)
                return g_RADBadOrderEntry;
        }
    }

    // Check the patterns
    while (1)
    {

        // Get pattern number
        if (s >= e)
            return g_RADTruncated;
        uint8_t pattnum = *s++;

        // Last pattern?
        if (pattnum == 0xFF)
            break;

        if (pattnum >= 100)
            return g_RADBadPattNum;

        const char *err = RADCheckPattern(s, e, false);
        if (err)
            return err;
    }

    // Check the riffs
    while (1)
    {

        // Get riff number
        if (s >= e)
            return g_RADTruncated;
        uint8_t riffnum = *s++;

        // Last riff?
        if (riffnum == 0xFF)
            break;

        uint8_t riffpatt = riffnum >> 4;
        uint8_t riffchan = riffnum & 15;
        if (riffpatt > 9 || riffchan == 0 || riffchan > 9)
            return g_RADBadRiffNum;

        const char *err = RADCheckPattern(s, e, true);
        if (err)
            return err;
    }

    // We should be at the end of the file now.  Note, you can safely remove this check if you
    // like - extra bytes won't affect playback
    if (s != e)
        return g_RADExtraBytes;

    // Tune file is all good
    return 0;
}

//==================================================================================================
// Compute total time of tune if it didn't repeat.  Note, this stops the tune so should only be done
// prior to initial playback.
//==================================================================================================
static void RADPlayerDummyOPL3(void *arg, uint16_t reg, uint8_t data)
{
    (void)arg;
    (void)reg;
    (void)data;
}
//--------------------------------------------------------------------------------------------------
uint32_t RADPlayer::ComputeTotalTime()
{

    Stop();
    void (*old_opl3)(void *, uint16_t, uint8_t) = OPL3;
    OPL3                                        = RADPlayerDummyOPL3;

    while (!Update())
        ;
    uint32_t total = PlayTime;

    Stop();
    OPL3 = old_opl3;

    return total / Hertz;
}