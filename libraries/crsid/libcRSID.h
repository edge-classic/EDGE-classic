// cRSID lightweight RealSID (integer-only) library-header (with API-calls) by Hermit (Mihaly Horvath)

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

    struct cRSID_Output
    {
        signed int L;
        signed int R;
    };

    struct cRSID_SIDwavOutput
    {
        signed int NonFilted;
        signed int FilterInput;
    };

    enum cRSID_Specifications
    {
        CRSID_SIDCOUNT_MAX       = 4,
        CRSID_CIACOUNT           = 2,
        CRSID_FILEVERSION_WEBSID = 0x4E
    };
    enum cRSID_Channels
    {
        CRSID_CHANNEL_LEFT  = 1,
        CRSID_CHANNEL_RIGHT = 2,
        CRSID_CHANNEL_BOTH  = 3
    };
    enum cRSID_StatusCodes
    {
        CRSID_STATUS_OK  = 0,
        CRSID_ERROR_INIT = -1,
        CRSID_ERROR_LOAD = -2
    };

    typedef struct cRSID_SIDheader    cRSID_SIDheader;
    typedef struct cRSID_C64instance  cRSID_C64instance;
    typedef struct cRSID_CPUinstance  cRSID_CPUinstance;
    typedef struct cRSID_SIDinstance  cRSID_SIDinstance;
    typedef struct cRSID_CIAinstance  cRSID_CIAinstance;
    typedef struct cRSID_VICinstance  cRSID_VICinstance;
    typedef struct cRSID_Output       cRSID_Output;
    typedef struct cRSID_SIDwavOutput cRSID_SIDwavOutput;

    struct cRSID_SIDheader
    {                             // Offset:   default/info:
        unsigned char
            MagicString[4];       //$00 - "PSID" or "RSID" (RSID must provide Reset-circumstances & CIA/VIC-interrupts)
        unsigned char VersionH00; //$04
        unsigned char Version;    //$05 - 1 for PSID v1, 2..4 for PSID v2..4 or RSID v2..4 (3/4 has 2SID/3SID support),
                                  //0x4E for 4SID (WebSID-format)
        unsigned char HeaderSizeH00; //$06
        unsigned char HeaderSize; //$07 - $76 for v1, $7C for v2..4, with WebSID-format: $7E for 2SID, $80 for 3SID, $82
                                  //for 4SID (depends on number of SIDs)
        unsigned char LoadAddressH,
            LoadAddressL;         //$08 - if 0 it's a PRG and its loadaddress is used (RSID: 0, PRG-loadaddress>=$07E8)
        unsigned char InitAddressH, InitAddressL; //$0A - if 0 it's taken from load-address (but should be set) (RSID:
                                                  //don't point to ROM, 0 if BASICflag set)
        unsigned char PlayAddressH,
            PlayAddressL;                //$0C - if 0 play-routine-call is set by the initializer (always true for RSID)
        unsigned char SubtuneAmountH00;  //$0E
        unsigned char SubtuneAmount;     //$0F - 1..256
        unsigned char DefaultSubtuneH00; //$10
        unsigned char DefaultSubtune;    //$11 - 1..256 (optional, defaults to 1)
        unsigned char SubtuneTimeSources[4]; //$12 - 0:Vsync / 1:CIA1 (for PSID) (LSB is subtune1, MSB above 32) ,
                                             //always 0 for RSID
        char Title[32];                      //$16 - strings are using 1252 codepage
        char Author[32];                     //$36
        char ReleaseInfo[32];                //$56
        // SID v2 additions:                              (if SID2/SID3 model is set to unknown, they're set to the same
        // model as SID1)
        unsigned char ModelFormatStandardH; //$76 - bit9&8/7&6/5&4: SID3/2/1 model (00:?,01:6581,10:8580,11:both)
                                            //(4SID:bit6=SID1-channel), bit3&2:VideoStandard..
        unsigned char ModelFormatStandard;  //$77 ..(01:PAL,10:NTSC,11:both),
                                           //bit1:(0:C64,1:PlaySIDsamples/RSID_BASICflag), bit0:(0:builtin-player,1:MUS)
        unsigned char RelocStartPage; //$78 - v2NG specific, if 0 the SID doesn't write outside its data-range, if $FF
                                      //there's no place for driver
        unsigned char RelocFreePages; //$79 - size of area from RelocStartPage for driver-relocation (RSID: must not
                                      //contain ROM or 0..$3FF)
        union {
            unsigned char SID2baseAddress; //$7A - (SID2BASE-$d000)/16 //SIDv3-relevant, only $42..$FE values are valid
                                           //($d420..$DFE0), else no SID2
            unsigned char
                SID2flagsH; //$7A: address of SID2 in WebSID-format too (same format as SID2baseAddress in HVSC format)
        };
        union {
            unsigned char SID3baseAddress; //$7B - (SID3BASE-$d000)/16 //SIDv4-relevant, only $42..$FE values are valid
                                           //($d420..$DFE0), else no SID3
            unsigned char SID2flagsL; //$7B: flags for WebSID-format, bit6: output-channel (0(default):left, 1:right,
                                      //?:both?), bit5..4:SIDmodel(00:setting,01:6581,10:8580,11:both)
                                      //   my own (implemented in SID-Wizard too) proposal for channel-info: bit7 should
                                      //   be 'middle' channel-flag (overriding bit6 left/right)
        };
        // WebSID-format (with 4 and more SIDs -support) additional fields: for each extra SID there's an 'nSIDflags'
        // byte-pair
        unsigned char SID3flagsH, SID3flagsL; //$7C,$7D: the same address/flag-layout for SID3 as with SID2
        union {
            unsigned char SID4flagsH;
            unsigned char SID4baseAddress;
        };                        //$7E
        unsigned char SID4flagsL; //$7F: the same address/flag-layout for SID4 as with SID2
        //... repeated for more SIDs, and end the list with $00,$00 (this determines the amount of SIDs)
    }; // music-program follows right after the header

    struct cRSID_CPUinstance
    {
        cRSID_C64instance *C64;      // reference to the containing C64
        unsigned int       PC;
        short int          A, SP;
        unsigned char      X, Y, ST; // STATUS-flags: N V - B D I Z C
        unsigned char      PrevNMI;  // used for NMI leading edge detection
    };

    struct cRSID_SIDinstance
    {
        // SID-chip data:
        cRSID_C64instance *C64;         // reference to the containing C64
        unsigned short     ChipModel;   // values: 8580 / 6581
        unsigned char      Channel;     // 1:left, 2:right, 3:both(middle)
        unsigned short     BaseAddress; // SID-baseaddress location in C64-memory (IO)
        unsigned char     *BasePtr;     // SID-baseaddress location in host's memory
        // ADSR-related:
        unsigned char  ADSRstate[15];
        unsigned short RateCounter[15];
        unsigned char  EnvelopeCounter[15];
        unsigned char  ExponentCounter[15];
        // Wave-related:
        int           PhaseAccu[15];     // 28bit precision instead of 24bit
        int           PrevPhaseAccu[15]; //(integerized ClockRatio fractionals, WebSID has similar solution)
        unsigned char SyncSourceMSBrise;
        unsigned int  RingSourceMSB;
        unsigned int  NoiseLFSR[15];
        unsigned int  PrevWavGenOut[15];
        unsigned char PrevWavData[15];
        // Filter-related:
        int PrevLowPass;
        int PrevBandPass;
        // Output-stage:
        int        NonFiltedSample;
        int        FilterInputSample;
        int        PrevNonFiltedSample;
        int        PrevFilterInputSample;
        signed int PrevVolume; // lowpass-filtered version of Volume-band register
        int        Output;     // not attenuated (range:0..0xFFFFF depending on SID's main-volume)
        int        Level;      // filtered version, good for VU-meter display
    };

    struct cRSID_CIAinstance
    {
        cRSID_C64instance *C64;         // reference to the containing C64
        char               ChipModel;   // old or new CIA? (have 1 cycle difference in cases)
        unsigned short     BaseAddress; // CIA-baseaddress location in C64-memory (IO)
        unsigned char     *BasePtrWR;   // CIA-baseaddress location in host's memory for writing
        unsigned char     *BasePtrRD;   // CIA-baseaddress location in host's memory for reading
    };

    struct cRSID_VICinstance
    {
        cRSID_C64instance *C64;         // reference to the containing C64
        char               ChipModel;   //(timing differences between models?)
        unsigned short     BaseAddress; // VIC-baseaddress location in C64-memory (IO)
        unsigned char     *BasePtrWR;   // VIC-baseaddress location in host's memory for writing
        unsigned char     *BasePtrRD;   // VIC-baseaddress location in host's memory for reading
        unsigned short     RasterLines;
        unsigned char      RasterRowCycles;
        unsigned char      RowCycleCnt;
    };

    struct cRSID_C64instance
    {
        // platform-related:
        unsigned short SampleRate;
        unsigned int   BufferSize;
        unsigned char  HighQualitySID;
        unsigned char  SIDchipCount;
        unsigned char  Stereo;
        unsigned char  PlaybackSpeed;
        unsigned char  Paused;
        // C64-machine related:
        unsigned char  VideoStandard;    // 0:NTSC, 1:PAL (based on the SID-header field)
        unsigned int   CPUfrequency;
        unsigned short SampleClockRatio; // ratio of CPU-clock and samplerate
        unsigned short SelectedSIDmodel;
        unsigned char  MainVolume;
        // SID-file related:
        union {
            cRSID_SIDheader *SIDheader;
            char            *SIDfileData;
        };
        unsigned short Attenuation;
        char           RealSIDmode;
        char           PSIDdigiMode;
        unsigned char  SubTune;
        unsigned short LoadAddress;
        unsigned short InitAddress;
        unsigned short PlayAddress;
        unsigned short EndAddress;
        char           TimerSource; // for current subtune, 0:VIC, 1:CIA (as in SID-header)
        // PSID-playback related:
        unsigned char SoundStarted;
        // char              CIAisSet; //for dynamic CIA setting from player-routine (RealSID substitution)
        int            FrameCycles;
        int            FrameCycleCnt; // this is a substitution in PSID-mode for CIA/VIC counters
        short          PrevRasterLine;
        short          SampleCycleCnt;
        short          OverSampleCycleCnt;
        short          TenthSecondCnt;
        unsigned short SecondCnt;
        short          PlayTime;
        char           Finished;
        char           Returned;
        unsigned char  IRQ; // collected IRQ line from devices
        unsigned char  NMI; // collected NMI line from devices

        // Hardware-elements:
        cRSID_CPUinstance CPU;
        cRSID_SIDinstance SID[CRSID_SIDCOUNT_MAX + 1];
        cRSID_CIAinstance CIA[CRSID_CIACOUNT + 1];
        cRSID_VICinstance VIC;
        // Overlapping system memories, which one is read/written in an address region depends on CPU-port
        // bankselect-bits) Address $00 and $01 - data-direction and data-register of port built into CPU (used as
        // bank-selection) (overriding RAM on C64)
        unsigned char RAMbank[0x10100];  //$0000..$FFFF RAM (and RAM under IO/ROM/CPUport)
        unsigned char IObankWR[0x10100]; //$D000..$DFFF IO-RAM (registers) to write (VIC/SID/CIA/ColorRAM/IOexpansion)
        unsigned char
            IObankRD[0x10100]; //$D000..$DFFF IO-RAM (registers) to read from (VIC/SID/CIA/ColorRAM/IOexpansion)
        unsigned char
            ROMbanks[0x10100]; //$1000..$1FFF/$9000..$9FFF (CHARGEN), $A000..$BFFF (BASIC), $E000..$FFFF (KERNAL)
    };

    extern cRSID_C64instance
        cRSID_C64; // the only global object (for faster & simpler access than with struct-pointers, in some places)

    cRSID_C64instance *cRSID_init(unsigned short samplerate); // init emulation objects and sound
    cRSID_SIDheader   *cRSID_processSIDfile(cRSID_C64instance *C64, unsigned char *filedata,
                                            int filesize);    // in host/file.c, copy SID-data to C64 memory
    void cRSID_initSIDtune(cRSID_C64instance *C64, cRSID_SIDheader *SIDheader, char subtune); // init tune/subtune
    void cRSID_initC64(cRSID_C64instance *C64); // hard-reset

    void cRSID_generateSound(cRSID_C64instance *C64, unsigned char *buf, unsigned short len);
    void cRSID_generateFloat(cRSID_C64instance *C64, float *buf, unsigned short len);

#ifdef __cplusplus
}
#endif