
//C64 emulation (SID-playback related)


#include "../libcRSID.h"

#include "MEM.c"
#include "CPU.c"
#include "CIA.c"
#include "VIC.c"
#include "SID.c"


cRSID_C64instance* cRSID_createC64 (cRSID_C64instance* C64, unsigned short samplerate) { //init a basic PAL C64 instance
 enum C64clocks { C64_PAL_CPUCLK=985248, DEFAULT_SAMPLERATE=44100 };
 if(samplerate) C64->SampleRate = samplerate;
 else C64->SampleRate = samplerate = DEFAULT_SAMPLERATE;
 C64->SampleClockRatio = (C64_PAL_CPUCLK<<4)/samplerate; //shifting (multiplication) enhances SampleClockRatio precision
 C64->Attenuation = 26; C64->SIDchipCount=1;
 C64->CPU.C64 = C64;
 cRSID_createSIDchip ( C64, &C64->SID[1], 8580, CRSID_CHANNEL_BOTH, 0xD400 ); //default C64 setup with only 1 SID and 2 CIAs and 1 VIC
 cRSID_createCIAchip ( C64, &C64->CIA[1], 0xDC00 );
 cRSID_createCIAchip ( C64, &C64->CIA[2], 0xDD00 );
 cRSID_createVICchip ( C64, &C64->VIC, 0xD000 );
 //if(C64->RealSIDmode) {
  cRSID_setROMcontent ( C64 );
 //}
 cRSID_initC64(C64);
 return C64;
}


void cRSID_setC64 (cRSID_C64instance* C64) {   //set hardware-parameters (Models, SIDs) for playback of loaded SID-tune
 enum C64clocks { C64_PAL_CPUCLK=985248, C64_NTSC_CPUCLK=1022727 };
 enum C64scanlines { C64_PAL_SCANLINES = 312, C64_NTSC_SCANLINES = 263 };
 enum C64scanlineCycles { C64_PAL_SCANLINE_CYCLES = 63, C64_NTSC_SCANLINE_CYCLES = 65 };
 //enum C64framerates { PAL_FRAMERATE = 50, NTSC_FRAMERATE = 60 }; //Hz

 static const unsigned int CPUspeeds[] = { C64_NTSC_CPUCLK, C64_PAL_CPUCLK };
 static const unsigned short ScanLines[] = { C64_NTSC_SCANLINES, C64_PAL_SCANLINES };
 static const unsigned char ScanLineCycles[] = { C64_NTSC_SCANLINE_CYCLES, C64_PAL_SCANLINE_CYCLES };
 //unsigned char FrameRates[] = { NTSC_FRAMERATE, PAL_FRAMERATE };

 static const short Attenuations[]={0,26,43,137,200}; //increase for 2SID (to 43) and 3SID (to 137)
 short SIDmodel; char SIDchannel;


 C64->VideoStandard = ( (C64->SIDheader->ModelFormatStandard & 0x0C) >> 2 ) != 2;
 if (C64->SampleRate==0) C64->SampleRate = 44100;
 C64->CPUfrequency = CPUspeeds[C64->VideoStandard];
 C64->SampleClockRatio = ( C64->CPUfrequency << 4 ) / C64->SampleRate; //shifting (multiplication) enhances SampleClockRatio precision

 C64->VIC.RasterLines = ScanLines[C64->VideoStandard];
 C64->VIC.RasterRowCycles = ScanLineCycles[C64->VideoStandard];
 C64->FrameCycles = C64->VIC.RasterLines * C64->VIC.RasterRowCycles; ///C64->SampleRate / PAL_FRAMERATE; //1x speed tune with VIC Vertical-blank timing

 C64->PrevRasterLine=-1; //so if $d012 is set once only don't disturb FrameCycleCnt

 SIDmodel = (C64->SIDheader->ModelFormatStandard&0x30) >= 0x20 ? 8580:6581;
 C64->SID[1].ChipModel = C64->SelectedSIDmodel? C64->SelectedSIDmodel : SIDmodel;


 if (C64->SIDheader->Version != CRSID_FILEVERSION_WEBSID) {

  C64->SID[1].Channel = CRSID_CHANNEL_LEFT;

  SIDmodel = C64->SIDheader->ModelFormatStandard & 0xC0;
  if (SIDmodel) SIDmodel = (SIDmodel >= 0x80) ? 8580:6581; else SIDmodel = C64->SID[1].ChipModel;
  if (C64->SelectedSIDmodel) SIDmodel = C64->SelectedSIDmodel;
  cRSID_createSIDchip ( C64, &C64->SID[2], SIDmodel, CRSID_CHANNEL_RIGHT, 0xD000 + C64->SIDheader->SID2baseAddress*16 );

  SIDmodel = C64->SIDheader->ModelFormatStandardH & 0x03;
  if (SIDmodel) SIDmodel = (SIDmodel >= 0x02) ? 8580:6581; else SIDmodel = C64->SID[1].ChipModel;
  if (C64->SelectedSIDmodel) SIDmodel = C64->SelectedSIDmodel;
  cRSID_createSIDchip ( C64, &C64->SID[3], SIDmodel, CRSID_CHANNEL_BOTH, 0xD000 + C64->SIDheader->SID3baseAddress*16 );

  C64->SID[4].BaseAddress=0x0000; C64->SID[4].BasePtr = NULL; //ensure disabling SID4 in non-WebSID format

 }
 else {

  C64->SID[1].Channel = (C64->SIDheader->ModelFormatStandardH & 0x40)? CRSID_CHANNEL_RIGHT:CRSID_CHANNEL_LEFT;
  if (C64->SIDheader->ModelFormatStandardH & 0x80) C64->SID[1].Channel = CRSID_CHANNEL_BOTH; //my own proposal for 'middle' channel

  SIDmodel = C64->SIDheader->SID2flagsL & 0x30;
  SIDchannel = (C64->SIDheader->SID2flagsL & 0x40) ? CRSID_CHANNEL_RIGHT:CRSID_CHANNEL_LEFT;
  if (C64->SIDheader->SID2flagsL & 0x80) SIDchannel = CRSID_CHANNEL_BOTH;
  if (SIDmodel) SIDmodel = (SIDmodel >= 0x20) ? 8580:6581; else SIDmodel = C64->SID[1].ChipModel;
  if (C64->SelectedSIDmodel) SIDmodel = C64->SelectedSIDmodel;
  cRSID_createSIDchip ( C64, &C64->SID[2], SIDmodel, SIDchannel, 0xD000 + C64->SIDheader->SID2baseAddress*16 );

  SIDmodel = C64->SIDheader->SID3flagsL & 0x30;
  SIDchannel = (C64->SIDheader->SID3flagsL & 0x40) ? CRSID_CHANNEL_RIGHT:CRSID_CHANNEL_LEFT;
  if (C64->SIDheader->SID3flagsL & 0x80) SIDchannel = CRSID_CHANNEL_BOTH;
  if (SIDmodel) SIDmodel = (SIDmodel >= 0x20) ? 8580:6581; else SIDmodel = C64->SID[1].ChipModel;
  if (C64->SelectedSIDmodel) SIDmodel = C64->SelectedSIDmodel;
  cRSID_createSIDchip ( C64, &C64->SID[3], SIDmodel, SIDchannel, 0xD000 + C64->SIDheader->SID3flagsH*16 );

  SIDmodel = C64->SIDheader->SID4flagsL & 0x30;
  SIDchannel = (C64->SIDheader->SID4flagsL & 0x40) ? CRSID_CHANNEL_RIGHT:CRSID_CHANNEL_LEFT;
  if (C64->SIDheader->SID4flagsL & 0x80) SIDchannel = CRSID_CHANNEL_BOTH;
  if (SIDmodel) SIDmodel = (SIDmodel >= 0x20) ? 8580:6581; else SIDmodel = C64->SID[1].ChipModel;
  if (C64->SelectedSIDmodel) SIDmodel = C64->SelectedSIDmodel;
  cRSID_createSIDchip ( C64, &C64->SID[4], SIDmodel, SIDchannel, 0xD000 + C64->SIDheader->SID4baseAddress*16 );

 }


 C64->SIDchipCount = 1 + (C64->SID[2].BaseAddress > 0) + (C64->SID[3].BaseAddress > 0) + (C64->SID[4].BaseAddress > 0);
 if (C64->SIDchipCount == 1) C64->SID[1].Channel = CRSID_CHANNEL_BOTH;
 C64->Attenuation = Attenuations[C64->SIDchipCount];
}


void cRSID_initC64 (cRSID_C64instance* C64) { //C64 Reset
 cRSID_initSIDchip( &C64->SID[1] );
 cRSID_initCIAchip( &C64->CIA[1] ); cRSID_initCIAchip( &C64->CIA[2] );
 cRSID_initMem(C64);
 cRSID_initCPU( &C64->CPU, (cRSID_readMemC64(C64,0xFFFD)<<8) + cRSID_readMemC64(C64,0xFFFC) );
 C64->IRQ = C64->NMI = 0;
 if (cRSID_C64.HighQualitySID) {
  C64->SID[1].NonFiltedSample = C64->SID[1].FilterInputSample = 0;
  C64->SID[2].NonFiltedSample = C64->SID[2].FilterInputSample = 0;
  C64->SID[3].NonFiltedSample = C64->SID[3].FilterInputSample = 0;
  C64->SID[4].NonFiltedSample = C64->SID[4].FilterInputSample = 0;
  C64->SID[1].PrevNonFiltedSample = C64->SID[1].PrevFilterInputSample = 0;
  C64->SID[2].PrevNonFiltedSample = C64->SID[2].PrevFilterInputSample = 0;
  C64->SID[3].PrevNonFiltedSample = C64->SID[3].PrevFilterInputSample = 0;
  C64->SID[4].PrevNonFiltedSample = C64->SID[4].PrevFilterInputSample = 0;
 }
 C64->SampleCycleCnt = C64->OverSampleCycleCnt = 0;
}



cRSID_Output cRSID_emulateC64 (cRSID_C64instance *C64) {
 enum C64clocks { C64_PAL_CPUCLK=985248, DEFAULT_SAMPLERATE=44100 };
 enum Oversampling { OVERSAMPLING_RATIO=7, OVERSAMPLING_CYCLES = ((C64_PAL_CPUCLK/DEFAULT_SAMPLERATE)/OVERSAMPLING_RATIO) };

 static unsigned char i, InstructionCycles, HQsampleCount;
 static int Tmp;
 static cRSID_Output Output; static cRSID_SIDwavOutput SIDwavOutput;


 //Cycle-based part of emulations:


 while (C64->SampleCycleCnt <= C64->SampleClockRatio) {

  if (!C64->RealSIDmode) {
   if (C64->FrameCycleCnt >= C64->FrameCycles) {
    C64->FrameCycleCnt -= C64->FrameCycles;
    if (C64->Finished) { //some tunes (e.g. Barbarian, A-Maze-Ing) doesn't always finish in 1 frame
     cRSID_initCPU ( &C64->CPU, C64->PlayAddress ); //(PSID docs say bank-register should always be set for each call's region)
     C64->Finished=0; //C64->SampleCycleCnt=0; //PSID workaround for some tunes (e.g. Galdrumway):
     if (C64->TimerSource==0) C64->IObankRD[0xD019] = 0x81; //always simulate to player-calls that VIC-IRQ happened
     else C64->IObankRD[0xDC0D] = 0x83; //always simulate to player-calls that CIA TIMERA/TIMERB-IRQ happened
   }}
   if (C64->Finished==0) {
    if ( (InstructionCycles = cRSID_emulateCPU()) >= 0xFE ) { InstructionCycles=6; C64->Finished=1; }
   }
   else InstructionCycles=7; //idle between player-calls
   C64->FrameCycleCnt += InstructionCycles;
   C64->IObankRD[0xDC04] += InstructionCycles; //very simple CIA1 TimerA simulation for PSID (e.g. Delta-Mix_E-Load_loader)
  }

  else { //RealSID emulations:
   if ( cRSID_handleCPUinterrupts(&C64->CPU) ) { C64->Finished=0; InstructionCycles=7; }
   else if (C64->Finished==0) {
    if ( (InstructionCycles = cRSID_emulateCPU()) >= 0xFE ) {
     InstructionCycles=6; C64->Finished=1;
    }
   }
   else InstructionCycles=7; //idle between IRQ-calls
   C64->IRQ = C64->NMI = 0; //prepare for collecting IRQ sources
   C64->IRQ |= cRSID_emulateCIA (&C64->CIA[1], InstructionCycles);
   C64->NMI |= cRSID_emulateCIA (&C64->CIA[2], InstructionCycles);
   C64->IRQ |= cRSID_emulateVIC (&C64->VIC, InstructionCycles);
  }

  C64->SampleCycleCnt += (InstructionCycles<<4);

  cRSID_emulateADSRs (&C64->SID[1], InstructionCycles);
  if ( C64->SID[2].BaseAddress != 0 ) cRSID_emulateADSRs (&C64->SID[2], InstructionCycles);
  if ( C64->SID[3].BaseAddress != 0 ) cRSID_emulateADSRs (&C64->SID[3], InstructionCycles);
  if ( C64->SID[4].BaseAddress != 0 ) cRSID_emulateADSRs (&C64->SID[4], InstructionCycles);

 }
 C64->SampleCycleCnt -= C64->SampleClockRatio;


 if (C64->HighQualitySID) { //oversampled waveform-generation
  HQsampleCount=0;
  C64->SID[1].NonFiltedSample = C64->SID[1].FilterInputSample = 0;
  C64->SID[2].NonFiltedSample = C64->SID[2].FilterInputSample = 0;
  C64->SID[3].NonFiltedSample = C64->SID[3].FilterInputSample = 0;
  C64->SID[4].NonFiltedSample = C64->SID[4].FilterInputSample = 0;

  while (C64->OverSampleCycleCnt <= C64->SampleClockRatio) {
   SIDwavOutput = cRSID_emulateHQwaves (&C64->SID[1], OVERSAMPLING_CYCLES);
   C64->SID[1].NonFiltedSample += SIDwavOutput.NonFilted; C64->SID[1].FilterInputSample += SIDwavOutput.FilterInput;
   if ( C64->SID[2].BaseAddress != 0 ) {
    SIDwavOutput = cRSID_emulateHQwaves (&C64->SID[2], OVERSAMPLING_CYCLES);
    C64->SID[2].NonFiltedSample += SIDwavOutput.NonFilted; C64->SID[2].FilterInputSample += SIDwavOutput.FilterInput;
   }
   if ( C64->SID[3].BaseAddress != 0 ) {
    SIDwavOutput = cRSID_emulateHQwaves (&C64->SID[3], OVERSAMPLING_CYCLES);
    C64->SID[3].NonFiltedSample += SIDwavOutput.NonFilted; C64->SID[3].FilterInputSample += SIDwavOutput.FilterInput;
   }
   if ( C64->SID[4].BaseAddress != 0 ) {
    SIDwavOutput = cRSID_emulateHQwaves (&C64->SID[4], OVERSAMPLING_CYCLES);
    C64->SID[4].NonFiltedSample += SIDwavOutput.NonFilted; C64->SID[4].FilterInputSample += SIDwavOutput.FilterInput;
   }
   ++HQsampleCount;
   C64->OverSampleCycleCnt += (OVERSAMPLING_CYCLES<<4);
  }
  C64->OverSampleCycleCnt -= C64->SampleClockRatio;
 }


 //Samplerate-based part of emulations:


 if (!C64->RealSIDmode) { //some PSID tunes use CIA TOD-clock (e.g. Kawasaki Synthesizer Demo)
  --C64->TenthSecondCnt;
  if (C64->TenthSecondCnt <= 0) {
   C64->TenthSecondCnt = C64->SampleRate / 10;
   ++(C64->IObankRD[0xDC08]);
   if(C64->IObankRD[0xDC08]>=10) {
    C64->IObankRD[0xDC08]=0; ++(C64->IObankRD[0xDC09]);
    //if(C64->IObankRD[0xDC09]%
   }
  }
 }

 if (C64->SecondCnt < C64->SampleRate) ++C64->SecondCnt; else { C64->SecondCnt = 0; if(C64->PlayTime<3600) ++C64->PlayTime; }


 if (C64->HighQualitySID==0) {
  if (C64->Stereo==0 || C64->SIDchipCount==1) {
   Output.L=Output.R = cRSID_emulateWaves (&C64->SID[1]);
   if ( C64->SID[2].BaseAddress != 0 ) Output.L=Output.R += cRSID_emulateWaves (&C64->SID[2]);
   if ( C64->SID[3].BaseAddress != 0 ) Output.L=Output.R += cRSID_emulateWaves (&C64->SID[3]);
   if ( C64->SID[4].BaseAddress != 0 ) Output.L=Output.R += cRSID_emulateWaves (&C64->SID[4]);
  }
  else {
   Tmp = cRSID_emulateWaves (&C64->SID[1]);
   if (C64->SID[1].Channel == CRSID_CHANNEL_LEFT)  { Output.L = Tmp * 2; Output.R=0; }
   else if (C64->SID[1].Channel == CRSID_CHANNEL_RIGHT) { Output.R = Tmp * 2; Output.L=0; }
   else Output.L = Output.R = Tmp;
   if ( C64->SID[2].BaseAddress != 0 ) {
    Tmp = cRSID_emulateWaves (&C64->SID[2]);
    if (C64->SID[2].Channel == CRSID_CHANNEL_LEFT)  Output.L += Tmp * 2;
    else if (C64->SID[2].Channel == CRSID_CHANNEL_RIGHT) Output.R += Tmp * 2;
    else { Output.L += Tmp; Output.R += Tmp; }
   }
   if ( C64->SID[3].BaseAddress != 0 ) {
    Tmp = cRSID_emulateWaves (&C64->SID[3]);
    if (C64->SID[3].Channel == CRSID_CHANNEL_LEFT)  Output.L += Tmp * 2;
    else if (C64->SID[3].Channel == CRSID_CHANNEL_RIGHT) Output.R += Tmp * 2;
    else { Output.L += Tmp; Output.R += Tmp; }
   }
   if ( C64->SID[4].BaseAddress != 0 ) {
    Tmp = cRSID_emulateWaves (&C64->SID[4]);
    if (C64->SID[4].Channel == CRSID_CHANNEL_LEFT)  Output.L += Tmp * 2;
    else if (C64->SID[4].Channel == CRSID_CHANNEL_RIGHT) Output.R += Tmp * 2;
    else { Output.L += Tmp; Output.R += Tmp; }
   }
  }
 }

 else { //SID output-stages and mono/stereo handling for High-Quality SID-emulation
  C64->SID[1].NonFiltedSample /= HQsampleCount; C64->SID[1].FilterInputSample /= HQsampleCount;
  if ( C64->SID[2].BaseAddress != 0 ) {
   C64->SID[2].NonFiltedSample /= HQsampleCount; C64->SID[2].FilterInputSample /= HQsampleCount;
  }
  if ( C64->SID[3].BaseAddress != 0 ) {
   C64->SID[3].NonFiltedSample /= HQsampleCount; C64->SID[3].FilterInputSample /= HQsampleCount;
  }
  if ( C64->SID[4].BaseAddress != 0 ) {
   C64->SID[4].NonFiltedSample /= HQsampleCount; C64->SID[4].FilterInputSample /= HQsampleCount;
  }
  if (C64->Stereo==0 || C64->SIDchipCount==1) {
   Output.L=Output.R = cRSID_emulateSIDoutputStage (&C64->SID[1]);
   if ( C64->SID[2].BaseAddress != 0 ) Output.L += cRSID_emulateSIDoutputStage (&C64->SID[2]);
   if ( C64->SID[3].BaseAddress != 0 ) Output.L += cRSID_emulateSIDoutputStage (&C64->SID[3]);
   if ( C64->SID[4].BaseAddress != 0 ) Output.L += cRSID_emulateSIDoutputStage (&C64->SID[4]);
   Output.R=Output.L;
  }
  else {
   Tmp = cRSID_emulateSIDoutputStage (&C64->SID[1]);
   if (C64->SID[1].Channel == CRSID_CHANNEL_LEFT)  { Output.L = Tmp * 2; Output.R=0; }
   else if (C64->SID[1].Channel == CRSID_CHANNEL_RIGHT) { Output.R = Tmp * 2; Output.L=0; }
   else Output.L = Output.R = Tmp;
   if ( C64->SID[2].BaseAddress != 0 ) {
    Tmp = cRSID_emulateSIDoutputStage (&C64->SID[2]);
    if (C64->SID[2].Channel == CRSID_CHANNEL_LEFT)  Output.L += Tmp * 2;
    else if (C64->SID[2].Channel == CRSID_CHANNEL_RIGHT) Output.R += Tmp * 2;
    else { Output.L += Tmp; Output.R += Tmp; }
   }
   if ( C64->SID[3].BaseAddress != 0 ) {
    Tmp = cRSID_emulateSIDoutputStage (&C64->SID[3]);
    if (C64->SID[3].Channel == CRSID_CHANNEL_LEFT)  Output.L += Tmp * 2;
    else if (C64->SID[3].Channel == CRSID_CHANNEL_RIGHT) Output.R += Tmp * 2;
    else { Output.L += Tmp; Output.R += Tmp; }
   }
   if ( C64->SID[4].BaseAddress != 0 ) {
    Tmp = cRSID_emulateSIDoutputStage (&C64->SID[4]);
    if (C64->SID[4].Channel == CRSID_CHANNEL_LEFT)  Output.L += Tmp * 2;
    else if (C64->SID[4].Channel == CRSID_CHANNEL_RIGHT) Output.R += Tmp * 2;
    else { Output.L += Tmp; Output.R += Tmp; }
   }
  }
 }


 //average level (for VU-meter)
 C64->SID[1].Level += ( (abs(C64->SID[1].Output)>>4) - C64->SID[1].Level ) / 1024;
 if ( C64->SID[2].BaseAddress != 0 ) C64->SID[2].Level += ( (abs(C64->SID[2].Output)>>4) - C64->SID[2].Level ) / 1024;
 if ( C64->SID[3].BaseAddress != 0 ) C64->SID[3].Level += ( (abs(C64->SID[3].Output)>>4) - C64->SID[3].Level ) / 1024;
 if ( C64->SID[4].BaseAddress != 0 ) C64->SID[4].Level += ( (abs(C64->SID[4].Output)>>4) - C64->SID[4].Level ) / 1024;


 return Output;
}



static inline short cRSID_playPSIDdigi(cRSID_C64instance* C64) {
 enum PSIDdigiSpecs { DIGI_VOLUME = 1200 }; //80 };
 static unsigned char PlaybackEnabled=0, NybbleCounter=0, RepeatCounter=0, Shifts;
 static unsigned short SampleAddress, RatePeriod;
 static short Output=0;
 static int PeriodCounter;

 if (C64->IObankWR[0xD41D]) {
  PlaybackEnabled = (C64->IObankWR[0xD41D] >= 0xFE);
  PeriodCounter = 0; NybbleCounter = 0;
  SampleAddress = C64->IObankWR[0xD41E] + (C64->IObankWR[0xD41F]<<8);
  RepeatCounter = C64->IObankWR[0xD43F];
 }
 C64->IObankWR[0xD41D] = 0;

 if (PlaybackEnabled) {
  RatePeriod = C64->IObankWR[0xD45D] + (C64->IObankWR[0xD45E]<<8);
  if (RatePeriod) PeriodCounter += C64->CPUfrequency / RatePeriod;
  if ( PeriodCounter >= C64->SampleRate ) {
   PeriodCounter -= C64->SampleRate;

   if ( SampleAddress < C64->IObankWR[0xD43D] + (C64->IObankWR[0xD43E]<<8) ) {
    if (NybbleCounter) {
     Shifts = C64->IObankWR[0xD47D] ? 4:0;
     ++SampleAddress;
    }
    else Shifts = C64->IObankWR[0xD47D] ? 0:4;
    Output = ( ( (C64->RAMbank[SampleAddress]>>Shifts) & 0xF) - 8 ) * DIGI_VOLUME; //* (C64->IObankWR[0xD418]&0xF);
    NybbleCounter^=1;
   }
   else if (RepeatCounter) {
    SampleAddress = C64->IObankWR[0xD47F] + (C64->IObankWR[0xD47E]<<8);
    RepeatCounter--;
   }

  }
 }

 return Output;
}

