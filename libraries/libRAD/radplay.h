/*

    C++ player code for Reality Adlib Tracker 2.0a (file version 2.1) (header).

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

#ifndef __RADPLAY_H
#define __RADPLAY_H

#include <stdint.h>

#include <cstddef>

//==================================================================================================
// RAD player class.
//==================================================================================================
class RADPlayer
{

    // Various constants
    enum
    {
        kTracks      = 100,
        kChannels    = 9,
        kTrackLines  = 64,
        kRiffTracks  = 10,
        kInstruments = 127,

        cmPortamentoUp  = 0x1,
        cmPortamentoDwn = 0x2,
        cmToneSlide     = 0x3,
        cmToneVolSlide  = 0x5,
        cmVolSlide      = 0xA,
        cmSetVol        = 0xC,
        cmJumpToLine    = 0xD,
        cmSetSpeed      = 0xF,
        cmIgnore        = ('I' - 55),
        cmMultiplier    = ('M' - 55),
        cmRiff          = ('R' - 55),
        cmTranspose     = ('T' - 55),
        cmFeedback      = ('U' - 55),
        cmVolume        = ('V' - 55),
    };

    enum e_Source
    {
        SNone,
        SRiff,
        SIRiff,
    };

    enum
    {
        fKeyOn   = 1 << 0,
        fKeyOff  = 1 << 1,
        fKeyedOn = 1 << 2,
    };

    struct CInstrument
    {
        uint8_t  Feedback[2];
        uint8_t  Panning[2];
        uint8_t  Algorithm;
        uint8_t  Detune;
        uint8_t  Volume;
        uint8_t  RiffSpeed;
        uint8_t *Riff;
        uint8_t  Operators[4][5];
    };

    struct CEffects
    {
        int8_t   PortSlide;
        int8_t   VolSlide;
        uint16_t ToneSlideFreq;
        uint8_t  ToneSlideOct;
        uint8_t  ToneSlideSpeed;
        int8_t   ToneSlideDir;
    };

    struct CChannel
    {
        uint8_t      LastInstrument;
        CInstrument *Instrument;
        uint8_t      Volume;
        uint8_t      DetuneA;
        uint8_t      DetuneB;
        uint8_t      KeyFlags;
        uint16_t     CurrFreq;
        int8_t       CurrOctave;
        CEffects     FX;
        struct CRiff
        {
            CEffects FX;
            uint8_t *Track;
            uint8_t *TrackStart;
            uint8_t  Line;
            uint8_t  Speed;
            uint8_t  SpeedCnt;
            int8_t   TransposeOctave;
            int8_t   TransposeNote;
            uint8_t  LastInstrument;
        } Riff, IRiff;
    };

  public:
    RADPlayer() : Initialised(false)
    {
    }
    void Init(const void *tune, void (*opl3)(void *, uint16_t, uint8_t), void *arg);
    void Stop();
    bool Update();
    int  GetHertz() const
    {
        return Hertz;
    }
    int GetPlayTimeInSeconds() const
    {
        return PlayTime / Hertz;
    }
    int GetTunePos() const
    {
        return Order;
    }
    int GetTuneLength() const
    {
        return OrderListSize;
    }
    int GetTuneLine() const
    {
        return Line;
    }
    void SetMasterVolume(int vol)
    {
        MasterVol = vol;
    }
    int GetMasterVolume() const
    {
        return MasterVol;
    }
    int GetSpeed() const
    {
        return Speed;
    }

    uint32_t ComputeTotalTime();

  private:
    bool     UnpackNote(uint8_t *&s, uint8_t &last_instrument);
    uint8_t *GetTrack();
    uint8_t *SkipToLine(uint8_t *trk, uint8_t linenum, bool chan_riff = false);
    void     PlayLine();
    void     PlayNote(int channum, int8_t notenum, int8_t octave, uint16_t instnum, uint8_t cmd = 0, uint8_t param = 0,
                      e_Source src = SNone, int op = 0);
    void     LoadInstrumentOPL3(int channum);
    void     PlayNoteOPL3(int channum, int8_t octave, int8_t note);
    void     ResetFX(CEffects *fx);
    void     TickRiff(int channum, CChannel::CRiff &riff, bool chan_riff);
    void     ContinueFX(int channum, CEffects *fx);
    void     SetVolume(int channum, uint8_t vol);
    void     GetSlideDir(int channum, CEffects *fx);
    void     LoadInstMultiplierOPL3(int channum, int op, uint8_t mult);
    void     LoadInstVolumeOPL3(int channum, int op, uint8_t vol);
    void     LoadInstFeedbackOPL3(int channum, int which, uint8_t fb);
    void     Portamento(uint16_t channum, CEffects *fx, int8_t amount, bool toneslide);
    void     Transpose(int8_t note, int8_t octave);
    void     SetOPL3(uint16_t reg, uint8_t val)
    {
        OPL3Regs[reg] = val;
        OPL3(OPL3Arg, reg, val);
    }
    uint8_t GetOPL3(uint16_t reg) const
    {
        return OPL3Regs[reg];
    }

    void (*OPL3)(void *, uint16_t, uint8_t);
    void       *OPL3Arg;
    CInstrument Instruments[kInstruments];
    CChannel    Channels[kChannels];
    uint32_t    PlayTime;
    uint32_t    OrderMap[4];
    bool        Repeating;
    int16_t     Hertz;
    uint8_t    *OrderList;
    uint8_t    *Tracks[kTracks];
    uint8_t    *Riffs[kRiffTracks][kChannels];
    uint8_t    *Track;
    bool        Initialised;
    uint8_t     Speed;
    uint8_t     OrderListSize;
    uint8_t     SpeedCnt;
    uint8_t     Order;
    uint8_t     Line;
    int8_t      Entrances;
    uint8_t     MasterVol;
    int8_t      LineJump;
    uint8_t     OPL3Regs[512];

    // Values exported by UnpackNote()
    int8_t  NoteNum;
    int8_t  OctaveNum;
    uint8_t InstNum;
    uint8_t EffectNum;
    uint8_t Param;

    static const int8_t   NoteSize[];
    static const uint16_t ChanOffsets3[9], Chn2Offsets3[9];
    static const uint16_t NoteFreq[];
    static const uint16_t OpOffsets3[9][4];
    static const bool     AlgCarriers[7][4];
};

//==================================================================================================
// RAD helper functions.
//==================================================================================================
const char *RADValidate(const void *data, size_t data_size);

#endif // __RADPLAY_H