/*
** 8bb: IT2 replayer system
**
** NOTE: MIDI logic is incomplete, and it was never meant to be ported anyway
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include "it_structs.h"
#include "it_tables.h"
#include "it_m_eff.h"
#include "it_music.h"
#include "it2drivers/sb16.h"

hostChn_t hChn[MAX_HOST_CHANNELS];
slaveChn_t sChn[MAX_SLAVE_CHANNELS];
driver_t Driver;
song_t Song;

enum
{
	NNA_NOTE_CUT = 0,
	NNA_CONTINUE = 1,
	NNA_NOTE_OFF = 2,
	NNA_NOTE_FADE = 3,

	DCT_DISABLED = 0,
	DCT_NOTE = 1,
	DCT_SAMPLE = 2,
	DCT_INSTRUMENT = 3,

	DCA_NOTE_CUT = 0
};

// 8bb: globalized
void (*DriverClose)(void) = NULL;
void (*DriverMix)(int32_t, int16_t *) = NULL;
void (*DriverResetMixer)(void) = NULL;
int32_t (*DriverPostMix)(int16_t *, int32_t) = NULL;
void (*DriverMixSamples)(void) = NULL;
void (*DriverSetTempo)(uint8_t) = NULL;
void (*DriverSetMixVolume)(uint8_t) = NULL;
void (*DriverFixSamples)(void) = NULL;
// ------------------------

static bool FirstTimeInit = true;
static uint8_t MIDIInterpretState, MIDIInterpretType; // 8bb: for MIDISendFilter()
static uint16_t RandSeed1 = 0x1234, RandSeed2 = 0x5678;

static char MIDIDataArea[(9+16+128)*32];

/* 8bb: These have been changed to be easier to understand,
** and to be 32-bit/64-bit pointer compliant.
*/
static uint8_t ChannelCountTable[100], ChannelVolumeTable[100];
static slaveChn_t *ChannelLocationTable[100];
// --------------------------------------------

static uint32_t AllocateNumChannels;
static slaveChn_t *AllocateSlaveOffset, *LastSlaveChannel;

static uint8_t EmptyPattern[72] =
{
	64,0,64,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static void (*InitCommandTable[])(hostChn_t *hc) =
{
	InitNoCommand, InitCommandA,
	InitCommandB, InitCommandC,
	InitCommandD, InitCommandE,
	InitCommandF, InitCommandG,
	InitCommandH, InitCommandI,
	InitCommandJ, InitCommandK,
	InitCommandL, InitCommandM,
	InitCommandN, InitCommandO,
	InitCommandP, InitCommandQ,
	InitCommandR, InitCommandS,
	InitCommandT, InitCommandU,
	InitCommandV, InitCommandW,
	InitCommandX, InitCommandY,
	InitCommandZ, InitNoCommand,
	InitNoCommand, InitNoCommand,
	InitNoCommand, InitNoCommand
};

static void (*CommandTable[])(hostChn_t *hc) =
{
	NoCommand, NoCommand,
	NoCommand, NoCommand,
	CommandD, CommandE,
	CommandF, CommandG,
	CommandH, CommandI,
	CommandJ, CommandK,
	CommandL, NoCommand,
	CommandN, NoCommand,
	CommandP, CommandQ,
	CommandR, CommandS,
	CommandT, CommandH,
	NoCommand, CommandW,
	NoCommand, CommandY,
	NoCommand, NoCommand,
	NoCommand, NoCommand
};

static void (*VolumeEffectTable[])(hostChn_t *hc) =
{
	NoCommand, NoCommand,
	VolumeCommandC, VolumeCommandD,
	VolumeCommandE, VolumeCommandF,
	VolumeCommandG, CommandH
};

void RecalculateAllVolumes(void)
{
	slaveChn_t *sc = sChn;
	for (uint32_t i = 0; i < Driver.NumChannels; i++, sc++)
		sc->Flags |= (SF_RECALC_PAN | SF_RECALC_VOL);
}

void Music_SetDefaultMIDIDataArea(void) // 8bb: added this
{
	// fill default MIDI configuration values (important for filters)

	memset(MIDIDataArea, 0, (9+16+128)*32); // data is padded with zeroes, not spaces!

	// MIDI commands
	memcpy(&MIDIDataArea[0*32], "FF", 2);
	memcpy(&MIDIDataArea[1*32], "FC", 2);
	memcpy(&MIDIDataArea[3*32], "9c n v", 6);
	memcpy(&MIDIDataArea[4*32], "9c n 0", 6);
	memcpy(&MIDIDataArea[8*32], "Cc p", 4);

	// macro setup (SF0)
	memcpy(&MIDIDataArea[9*32], "F0F000z", 7);

	// macro setup (Z80..Z8F)
	memcpy(&MIDIDataArea[25*32], "F0F00100", 8);
	memcpy(&MIDIDataArea[26*32], "F0F00108", 8);
	memcpy(&MIDIDataArea[27*32], "F0F00110", 8);
	memcpy(&MIDIDataArea[28*32], "F0F00118", 8);
	memcpy(&MIDIDataArea[29*32], "F0F00120", 8);
	memcpy(&MIDIDataArea[30*32], "F0F00128", 8);
	memcpy(&MIDIDataArea[31*32], "F0F00130", 8);
	memcpy(&MIDIDataArea[32*32], "F0F00138", 8);
	memcpy(&MIDIDataArea[33*32], "F0F00140", 8);
	memcpy(&MIDIDataArea[34*32], "F0F00148", 8);
	memcpy(&MIDIDataArea[35*32], "F0F00150", 8);
	memcpy(&MIDIDataArea[36*32], "F0F00158", 8);
	memcpy(&MIDIDataArea[37*32], "F0F00160", 8);
	memcpy(&MIDIDataArea[38*32], "F0F00168", 8);
	memcpy(&MIDIDataArea[39*32], "F0F00170", 8);
	memcpy(&MIDIDataArea[40*32], "F0F00178", 8);
}

char *Music_GetMIDIDataArea(void)
{
	return (char *)MIDIDataArea;
}

static void MIDISendFilter(hostChn_t *hc, slaveChn_t *sc, uint8_t Data)
{
	if (!(Driver.Flags & DF_SUPPORTS_MIDI))
		return;

	if (Data >= 0x80 && Data < 0xF0)
	{
		if (Data == Song.LastMIDIByte)
			return;

		Song.LastMIDIByte = Data;
	}

	/* 8bb: We implement the SendUARTOut() code found in the
	** SB16 MMX driver and WAV writer driver and use it here
	** instead of doing real MIDI data handling.
	**
	** It will only interpret filter commands (set and clear).
	*/
	if (MIDIInterpretState < 2)
	{
		if (Data == 0xF0)
		{
			MIDIInterpretState++;
		}
		else
		{
			if (Data == 0xFA || Data == 0xFC || Data == 0xFF)
			{
				// 8bb: reset filters
				for (int32_t i = 0; i < MAX_HOST_CHANNELS; i++)
				{
					Driver.FilterParameters[   i] = 127; // 8bb: Cutoff
					Driver.FilterParameters[64+i] = 0;   // 8bb: Q
				}
			}

			MIDIInterpretState = 0;
		}
	}
	else if (MIDIInterpretState == 2)
	{
		if (Data < 2) // 8bb: must be 0..1 (Cutoff or Q)
		{
			MIDIInterpretType = Data;
			MIDIInterpretState++;
		}
		else
		{
			MIDIInterpretState = 0;
		}
	}
	else if (MIDIInterpretState == 3)
	{
		// Have InterpretType, now get parameter, then return to normal.

		if (Data <= 0x7F)
		{
			bool IsFilterQ = (MIDIInterpretType == 1);
			if (IsFilterQ)
				Driver.FilterParameters[(64 + hc->HostChnNum) & 127] = Data;
			else
				Driver.FilterParameters[hc->HostChnNum & 127] = Data;

			if (sc != NULL)
				sc->Flags |= SF_RECALC_FINALVOL;
		}

		MIDIInterpretState = 0;
	}
}

static void SetFilterCutoff(hostChn_t *hc, slaveChn_t *sc, uint8_t value) // Assumes that channel is non-disowned
{
	MIDISendFilter(hc, sc, 0xF0);
	MIDISendFilter(hc, sc, 0xF0);
	MIDISendFilter(hc, sc, 0x00);
	MIDISendFilter(hc, sc, value);
}

static void SetFilterResonance(hostChn_t *hc, slaveChn_t *sc, uint8_t value) // Assumes that channel is non-disowned
{
	MIDISendFilter(hc, sc, 0xF0);
	MIDISendFilter(hc, sc, 0xF0);
	MIDISendFilter(hc, sc, 0x01);
	MIDISendFilter(hc, sc, value);
}

void MIDITranslate(hostChn_t *hc, slaveChn_t *sc, uint16_t Input)
{
	if (!(Driver.Flags & DF_SUPPORTS_MIDI))
		return;

	if (Input >= 0xF000)
		return; // 8bb: we don't support (nor need) MIDI commands

	if (Input/32 >= 9+16+128) // 8bb: added protection, just in case
		return;

	uint8_t MIDIData = 0;
	uint8_t CharsParsed = 0;

	while (true)
	{
		int16_t Byte = MIDIDataArea[Input++];

		if (Byte == 0)
		{
			if (CharsParsed > 0)
				MIDISendFilter(hc, sc, MIDIData);

			break; // 8bb: and we're done!
		}

		if (Byte == ' ')
		{
			if (CharsParsed > 0)
				MIDISendFilter(hc, sc, MIDIData);

			continue;
		}

		// Interpretation time.

		Byte -= '0';
		if (Byte < 0)
			continue;

		if (Byte <= 9)
		{
			MIDIData = (MIDIData << 4) | (uint8_t)Byte;
			CharsParsed++;

			if (CharsParsed >= 2)
			{
				MIDISendFilter(hc, sc, MIDIData);
				CharsParsed = 0;
				MIDIData = 0;
			}

			continue;
		}

		Byte -= 'A'-'0';
		if (Byte < 0)
			continue;

		if (Byte <= 'F'-'A')
		{
			MIDIData = (MIDIData << 4) | (uint8_t)(Byte+10);
			CharsParsed++;

			if (CharsParsed >= 2)
			{
				MIDISendFilter(hc, sc, MIDIData);
				CharsParsed = 0;
				MIDIData = 0;
			}

			continue;
		}

		Byte -= 'a'-'A';
		if (Byte < 0)
			continue;

		if (Byte > 'z'-'a')
			continue;

		if (Byte == 'c'-'a')
		{
			if (sc == NULL)
				continue;

			MIDIData = (MIDIData << 4) | (sc->MIDIChn-1);
			CharsParsed++;

			if (CharsParsed >= 2)
			{
				MIDISendFilter(hc, sc, MIDIData);
				CharsParsed = 0;
				MIDIData = 0;
			}

			continue;
		}

		if (CharsParsed > 0)
		{
			MIDISendFilter(hc, sc, MIDIData);
			MIDIData = 0;
		}

		if (Byte == 'z'-'a') // Zxx?
		{
			MIDISendFilter(hc, sc, hc->CmdVal);
		}
		else if (Byte == 'o'-'a') // 8bb: sample offset?
		{
			MIDISendFilter(hc, sc, hc->O00);
		}
		else if (sc != NULL)
		{
			if (Byte == 'n'-'a') // Note?
			{
				MIDISendFilter(hc, sc, sc->Note);
			}
			else if (Byte == 'm'-'a') // 8bb: MIDI note (sample loop direction on sample channels)
			{
				MIDISendFilter(hc, sc, sc->LoopDirection);
			}
			else if (Byte == 'v'-'a') // Velocity?
			{
				if (sc->Flags & SF_CHN_MUTED)
				{
					MIDISendFilter(hc, sc, 0);
				}
				else
				{
					uint16_t volume = (sc->VolSet * Song.GlobalVolume * sc->ChnVol) >> 4;
					volume = (volume * sc->SmpVol) >> 15;

					if (volume == 0)
						volume = 1;
					else if (volume >= 128)
						volume = 127;

					MIDISendFilter(hc, sc, (uint8_t)volume);
				}
			}
			else if (Byte == 'u'-'a') // Volume?
			{
				if (sc->Flags & SF_CHN_MUTED)
				{
					MIDISendFilter(hc, sc, 0);
				}
				else
				{
					uint16_t volume = sc->FinalVol7Bit;

					if (volume == 0)
						volume = 1;
					else if (volume >= 128)
						volume = 127;

					MIDISendFilter(hc, sc, (uint8_t)volume);
				}
			}
			else if (Byte == 'h'-'a') // HCN (8bb: host channel number)
			{
				MIDISendFilter(hc, sc, sc->HostChnNum & 0x7F);
			}
			else if (Byte == 'x'-'a') // Pan set
			{
				uint16_t value = sc->Pan * 2; // 8bb: yes sc->Pan, not sc->PS
				if (value >= 128)
					value--;

				if (value >= 128)
					value = 64;

				MIDISendFilter(hc, sc, (uint8_t)value);
			}
			else if (Byte == 'p'-'a') // Program?
			{
				MIDISendFilter(hc, sc, sc->MIDIProg);
			}
			else if (Byte == 'b'-'a') // 8bb: MIDI bank low
			{
				MIDISendFilter(hc, sc, sc->MIDIBank & 0xFF);
			}
			else if (Byte == 'a'-'a') // 8bb: MIDI bank high
			{
				MIDISendFilter(hc, sc, sc->MIDIBank >> 8);
			}
		}

		MIDIData = 0;
		CharsParsed = 0;
	}
}

void InitPlayInstrument(hostChn_t *hc, slaveChn_t *sc, instrument_t *ins)
{
	sc->InsPtr = ins;

	sc->NNA = ins->NNA;
	sc->DCT = ins->DCT;
	sc->DCA = ins->DCA;

	if (hc->MIDIChn != 0) // 8bb: MIDI?
	{
		sc->MIDIChn = ins->MIDIChn;
		sc->MIDIProg = ins->MIDIProg;
		sc->MIDIBank = ins->MIDIBank;
		sc->LoopDirection = hc->RawNote; // 8bb: during MIDI, LpD = MIDI note
	}

	sc->ChnVol = hc->ChnVol;

	uint8_t pan = (ins->DefPan & 0x80) ? hc->ChnPan : ins->DefPan;
	if (hc->Smp != 0)
	{
		sample_t *s = &Song.Smp[hc->Smp-1];
		if (s->DefPan & 0x80)
			pan = s->DefPan & 127;
	}

	if (pan != PAN_SURROUND)
	{
		int16_t newPan = pan + (((int8_t)(hc->RawNote - ins->PitchPanCenter) * (int8_t)ins->PitchPanSep) >> 3);

		if (newPan < 0)
			newPan = 0;
		else if (newPan > 64)
			newPan = 64;

		pan = (uint8_t)newPan;
	}

	sc->Pan = sc->PanSet = pan;

	// Envelope init
	sc->VolEnvState.Value = 64 << 16; // 8bb: clears fractional part
	sc->VolEnvState.Tick = sc->VolEnvState.NextTick = 0;
	sc->VolEnvState.CurNode = 0;

	sc->PanEnvState.Value = 0; // 8bb: clears fractional part
	sc->PanEnvState.Tick = sc->PanEnvState.NextTick = 0;
	sc->PanEnvState.CurNode = 0;

	sc->PitchEnvState.Value = 0; // 8bb: clears fractional part
	sc->PitchEnvState.Tick = sc->PitchEnvState.NextTick = 0;
	sc->PitchEnvState.CurNode = 0;

	sc->Flags = SF_CHAN_ON + SF_RECALC_PAN + SF_RECALC_VOL + SF_FREQ_CHANGE + SF_NEW_NOTE;

	if (ins->VolEnv.Flags & ENVF_ENABLED) sc->Flags |= SF_VOLENV_ON;
	if (ins->PanEnv.Flags & ENVF_ENABLED) sc->Flags |= SF_PANENV_ON;
	if (ins->PitchEnv.Flags & ENVF_ENABLED) sc->Flags |= SF_PITCHENV_ON;

	if (LastSlaveChannel != NULL)
	{
		slaveChn_t *lastSC = LastSlaveChannel;

		if ((ins->VolEnv.Flags & (ENVF_ENABLED|ENVF_CARRY)) == ENVF_ENABLED+ENVF_CARRY) // Transfer volume data
		{
			sc->VolEnvState.Value = lastSC->VolEnvState.Value;
			sc->VolEnvState.Delta = lastSC->VolEnvState.Delta;
			sc->VolEnvState.Tick = lastSC->VolEnvState.Tick;
			sc->VolEnvState.CurNode = lastSC->VolEnvState.CurNode;
			sc->VolEnvState.NextTick = lastSC->VolEnvState.NextTick;
		}

		if ((ins->PanEnv.Flags & (ENVF_ENABLED|ENVF_CARRY)) == ENVF_ENABLED+ENVF_CARRY) // Transfer pan data
		{
			sc->PanEnvState.Value = lastSC->PanEnvState.Value;
			sc->PanEnvState.Delta = lastSC->PanEnvState.Delta;
			sc->PanEnvState.Tick = lastSC->PanEnvState.Tick;
			sc->PanEnvState.CurNode = lastSC->PanEnvState.CurNode;
			sc->PanEnvState.NextTick = lastSC->PanEnvState.NextTick;
		}

		if ((ins->PitchEnv.Flags & (ENVF_ENABLED|ENVF_CARRY)) == ENVF_ENABLED+ENVF_CARRY) // Transfer pitch data
		{
			sc->PitchEnvState.Value = lastSC->PitchEnvState.Value;
			sc->PitchEnvState.Delta = lastSC->PitchEnvState.Delta;
			sc->PitchEnvState.Tick = lastSC->PitchEnvState.Tick;
			sc->PitchEnvState.CurNode = lastSC->PitchEnvState.CurNode;
			sc->PitchEnvState.NextTick = lastSC->PitchEnvState.NextTick;
		}
	}

	hc->Flags |= HF_APPLY_RANDOM_VOL; // Apply random volume/pan

	if (hc->MIDIChn == 0)
	{
		sc->MIDIBank = 0x00FF; // 8bb: reset filter resonance (Q) & cutoff

		if (ins->FilterCutoff & 0x80) // If IFC bit 7 == 1, then set filter cutoff
		{
			uint8_t filterCutOff = ins->FilterCutoff & 0x7F;
			SetFilterCutoff(hc, sc, filterCutOff);
		}

		if (ins->FilterResonance & 0x80) // If IFR bit 7 == 1, then set filter resonance
		{
			const uint8_t filterQ = ins->FilterResonance & 0x7F;
			sc->MIDIBank = (filterQ << 8) | (sc->MIDIBank & 0x00FF);
			SetFilterResonance(hc, sc, filterQ);
		}
	}
}

// 8bb: this function is used in AllocateChannel()
static slaveChn_t *AllocateChannelSample(hostChn_t *hc, uint8_t *hcFlags)
{
	// Sample handler

	slaveChn_t *sc = &sChn[hc->HostChnNum];
	if ((Driver.Flags & DF_USES_VOLRAMP) && (sc->Flags & SF_CHAN_ON))
	{
		// copy out channel
		sc->Flags |= SF_NOTE_STOP;
		sc->HostChnNum |= CHN_DISOWNED;
		memcpy(sc + MAX_HOST_CHANNELS, sc, sizeof (slaveChn_t));
	}

	hc->SlaveChnPtr = sc;
	sc->HostChnPtr = hc;
	sc->HostChnNum = hc->HostChnNum;

	sc->ChnVol = hc->ChnVol;
	sc->Pan = sc->PanSet = hc->ChnPan;
	sc->FadeOut = 1024;
	sc->VolEnvState.Value = (64 << 16) | (sc->VolEnvState.Value & 0xFFFF); // 8bb: keeps frac
	sc->MIDIBank = 0x00FF; // Filter cutoff
	sc->Note = hc->RawNote;
	sc->Ins = hc->Ins;

	sc->Flags = SF_CHAN_ON + SF_RECALC_PAN + SF_RECALC_VOL + SF_FREQ_CHANGE + SF_NEW_NOTE;

	if (hc->Smp > 0)
	{
		sc->Smp = hc->Smp - 1;
		sample_t *s = sc->SmpPtr = &Song.Smp[sc->Smp];

		sc->SmpBitDepth = 0; // 8bb: 8-bit
		sc->AutoVibratoDepth = sc->AutoVibratoPos = 0;
		sc->PanEnvState.Value &= 0xFFFF; // No pan deviation (8bb: keeps frac)
		sc->PitchEnvState.Value &= 0xFFFF; // No pitch deviation (8bb: keeps frac)
		sc->LoopDirection = DIR_FORWARDS; // Reset loop dirn

		if (s->Length == 0 || !(s->Flags & SMPF_ASSOCIATED_WITH_HEADER))
		{
			sc->Flags = SF_NOTE_STOP;
			*hcFlags &= ~HF_CHAN_ON;
			return NULL;
		}

		sc->SmpBitDepth = s->Flags & SMPF_16BIT;
		sc->SmpVol = s->GlobVol * 2;
		return sc;
	}
	else // No sample!
	{
		sc->Flags = SF_NOTE_STOP;
		*hcFlags &= ~HF_CHAN_ON;
		return NULL;
	}
}

// 8bb: this function is used in AllocateChannel()
static slaveChn_t *AllocateChannelInstrument(hostChn_t *hc, slaveChn_t *sc, instrument_t *ins, uint8_t *hcFlags)
{
	assert(hc != NULL && sc != NULL && ins != NULL);

	hc->SlaveChnPtr = sc;
	sc->HostChnNum = hc->HostChnNum;
	sc->HostChnPtr = hc;

	sc->SmpBitDepth = 0; // 8bb: 8-bit
	sc->AutoVibratoDepth = sc->AutoVibratoPos = 0;
	sc->LoopDirection = DIR_FORWARDS; // Reset loop dirn

	InitPlayInstrument(hc, sc, ins);

	sc->SmpVol = ins->GlobVol;
	sc->FadeOut = 1024;
	sc->Note = (hc->Smp == 101) ? hc->TranslatedNote : hc->RawNote;
	sc->Ins = hc->Ins;

	if (hc->Smp == 0)
	{
		// 8bb: shut down channel
		sc->Flags = SF_NOTE_STOP;
		*hcFlags &= ~HF_CHAN_ON;
		return NULL;
	}

	sc->Smp = hc->Smp-1;
	sample_t *s = sc->SmpPtr = &Song.Smp[sc->Smp];

	if (s->Length == 0 || !(s->Flags & SMPF_ASSOCIATED_WITH_HEADER))
	{
		// 8bb: shut down channel
		sc->Flags = SF_NOTE_STOP;
		*hcFlags &= ~HF_CHAN_ON;
		return NULL;
	}

	sc->SmpBitDepth = s->Flags & SMPF_16BIT;
	sc->SmpVol = (s->GlobVol * sc->SmpVol) >> 6; // 0->128
	return sc;
}

// 8bb: this function is used in AllocateChannel()
static bool DuplicateCheck(slaveChn_t **scOut, hostChn_t *hc, uint8_t hostChnNum, instrument_t *ins, uint8_t DCT, uint8_t DCVal)
{
	slaveChn_t *sc = AllocateSlaveOffset;
	for (uint32_t i = 0; i < AllocateNumChannels; i++, sc++) 
	{
		*scOut = sc; // 8bb: copy current slave channel pointer to scOut

		if (!(sc->Flags & SF_CHAN_ON) || (hc->Smp != 101 && sc->HostChnNum != hostChnNum) || sc->Ins != hc->Ins)
			continue;

		// 8bb: the actual duplicate test

		if (DCT == DCT_NOTE && sc->Note != DCVal)
			continue;

		if (DCT == DCT_SAMPLE && sc->Smp != DCVal)
			continue;

		if (DCT == DCT_INSTRUMENT && sc->Ins != DCVal)
			continue;

		if (hc->Smp == 101) // New note is a MIDI?
		{
			if (sc->Smp == 100 && sc->MIDIChn == hostChnNum) // Is current channel a MIDI chan
			{
				sc->Flags |= SF_NOTE_STOP;
				if (!(sc->HostChnNum & CHN_DISOWNED))
				{
					sc->HostChnNum |= CHN_DISOWNED;
					((hostChn_t *)sc->HostChnPtr)->Flags &= ~HF_CHAN_ON;
				}
			}
		}
		else if (sc->DCA == ins->DCA)
		{
			return true; // 8bb: dupe found
		}
	}
	
	return false; // 8bb: dupe not found
}

// 8bb: are you sure you want to know? ;)
slaveChn_t *AllocateChannel(hostChn_t *hc, uint8_t *hcFlags)
{
	LastSlaveChannel = NULL;

	if (!(Song.Header.Flags & ITF_INSTR_MODE) || hc->Ins == 255)
		return AllocateChannelSample(hc, hcFlags);

	// Instrument handler!

	if (hc->Ins == 0)
		return NULL;

	if (hc->Smp == 101 && Driver.NumChannels < MAX_SLAVE_CHANNELS) // 8bb: MIDI and below 256 virtual channels in driver?
	{
		AllocateNumChannels = MAX_SLAVE_CHANNELS - Driver.NumChannels;
		AllocateSlaveOffset = &sChn[Driver.NumChannels];
	}
	else
	{
		AllocateNumChannels = Driver.NumChannels;
		AllocateSlaveOffset = sChn; // 8bb: points to first virtual channel
	}

	// 8bb: some of these are initialized only to prevent compiler warnings
	uint8_t NNA = 0;
	slaveChn_t *sc = NULL;
	uint8_t hostChnNum, DCT, DCVal;

	instrument_t *ins = &Song.Ins[hc->Ins-1];

	bool scInitialized = false;
	if ((*hcFlags) & HF_CHAN_ON) // 8bb: host channel on?
	{
		sc = (slaveChn_t *)hc->SlaveChnPtr;
		if (sc->InsPtr == ins) // 8bb: slave channel has same inst. as host channel?
			LastSlaveChannel = sc;

		NNA = sc->NNA;
		if (NNA != NNA_NOTE_CUT) // 8bb: not note-cut
			sc->HostChnNum |= CHN_DISOWNED; // Disown channel

		scInitialized = true;
	}

	while (true) // New note action handling...
	{
		bool skipMIDITest = false;
		if (scInitialized)
		{
			if (NNA != NNA_NOTE_CUT && sc->VolSet > 0 && sc->ChnVol > 0 && sc->SmpVol > 0)
			{
				if (NNA == NNA_NOTE_OFF)
				{
					sc->Flags |= SF_NOTE_OFF;
					GetLoopInformation(sc); // 8bb: update sample loop (sustain released)
				}
				else if (NNA >= NNA_NOTE_FADE)
				{
					sc->Flags |= SF_FADEOUT;
				}
				// 8bb: else: NNA_CONTINUE
			}
			else
			{
				// 8bb: NNA=Note Cut (or volumes are zero)
				if (sc->Smp == 100) // MIDI?
				{
					sc->Flags |= SF_NOTE_STOP;
					sc->HostChnNum |= CHN_DISOWNED; // Disown channel

					if (hc->Smp != 101)
						break; // Sample.. (8bb: find available voice now)
				}
				else
				{
					if (Driver.Flags & DF_USES_VOLRAMP)
					{
						sc->Flags |= SF_NOTE_STOP;
						sc->HostChnNum |= CHN_DISOWNED; // Disown channel
						break; // 8bb: find available voice now
					}

					sc->Flags = SF_NOTE_STOP;
					if (ins->DCT == DCT_DISABLED)
						return AllocateChannelInstrument(hc, sc, ins, hcFlags);

					skipMIDITest = true;
				}
			}
		}

		hostChnNum = DCT = DCVal = 0; // 8bb: prevent stupid compiler warning...

		bool doDupeCheck = false;
		if (!skipMIDITest && hc->Smp == 101)
		{
			// 8bb: MIDI note, do a "duplicate note" check regardless of instrument's DCT setting
			hostChnNum = hc->MIDIChn;
			DCT = DCT_NOTE;
			DCVal = hc->TranslatedNote;

			doDupeCheck = true;
		}
		else if (ins->DCT != DCT_DISABLED)
		{
			hostChnNum = hc->HostChnNum | CHN_DISOWNED; // 8bb: only search disowned host channels
			DCT = ins->DCT;

			if (ins->DCT == DCT_NOTE)
			{
				DCVal = hc->RawNote; // 8bb: not the translated note!
			}
			else if (ins->DCT == DCT_INSTRUMENT)
			{
				DCVal = hc->Ins;
			}
			else
			{
				/* 8bb:
				** .ITs from OpenMPT can have DCT=4, which tests for duplicate instrument plugins.
				** This will be handled as DCT_SAMPLE in Impulse Tracker. Oops...
				*/
				DCVal = hc->Smp - 1;
				if ((int8_t)DCVal < 0)
					break; // 8bb: illegal (or no) sample, ignore dupe test and find available voice now
			}

			doDupeCheck = true;
		}

		if (doDupeCheck) // 8bb: NNA Duplicate Check
		{
			sc = AllocateSlaveOffset;
			if (DuplicateCheck(&sc, hc, hostChnNum, ins, DCT, DCVal))
			{
				// 8bb: dupe found!

				scInitialized = true; // 8bb: we have an sc pointer now (we could come from a shutdown host channel)
				if (ins->DCA == DCA_NOTE_CUT)
				{
					NNA = NNA_NOTE_CUT;
				}
				else
				{
					sc->DCT = DCT_DISABLED; // 8bb: turn of dupe check so that we don't do infinite NNA tests :)
					sc->DCA = DCA_NOTE_CUT;
					NNA = ins->DCA + 1;
				}

				continue; // 8bb: do another NNA test with the new NNA type
			}
		}

		break; // NNA handling done, find available voice now
	}

	// 8bb: search for inactive channels

	sc = AllocateSlaveOffset;
	if (hc->Smp != 101)
	{
		// 8bb: no MIDI
		for (uint32_t i = 0; i < AllocateNumChannels; i++, sc++)
		{
			if (!(sc->Flags & SF_CHAN_ON))
				return AllocateChannelInstrument(hc, sc, ins, hcFlags);
		}
	}
	else 
	{
		// MIDI 'slave channels' have to be maintained if still referenced
		for (uint32_t i = 0; i < AllocateNumChannels; i++, sc++)
		{
			if (!(sc->Flags & SF_CHAN_ON))
			{
				// Have a channel.. check that it's host's slave isn't SI (8bb: SI = sc)
				hostChn_t *hcTmp = (hostChn_t *)sc->HostChnPtr;

				if (hcTmp == NULL || hcTmp->SlaveChnPtr != sc)
					return AllocateChannelInstrument(hc, sc, ins, hcFlags);
			}
		}
	}

	// Common sample search

	memset(ChannelCountTable,    0, sizeof (ChannelCountTable));
	memset(ChannelVolumeTable, 255, sizeof (ChannelVolumeTable));
	memset(ChannelLocationTable, 0, sizeof (ChannelLocationTable));

	sc = AllocateSlaveOffset;
	for (uint32_t i = 0; i < AllocateNumChannels; i++, sc++)
	{
		if (sc->Smp > 99) // Just for safety
			continue;

		ChannelCountTable[sc->Smp]++;
		if ((sc->HostChnNum & CHN_DISOWNED) && sc->FinalVol7Bit < ChannelVolumeTable[sc->Smp])
		{
			ChannelLocationTable[sc->Smp] = sc;
			ChannelVolumeTable[sc->Smp] = sc->FinalVol7Bit;
		}
	}

	// OK.. now search table for maximum occurrence of sample...

	sc = NULL;
	uint8_t count = 2; // Find maximum count, has to be greater than 2 channels
	for (int32_t i = 0; i < 100; i++)
	{
		if (count < ChannelCountTable[i])
		{
			count = ChannelCountTable[i];
			sc = ChannelLocationTable[i];
		}
	}

	if (sc != NULL)
		return AllocateChannelInstrument(hc, sc, ins, hcFlags);

	/*
	** Find out which host channel has the most (disowned) slave channels.
	** Then find the softest non-single sample in that channel.
	*/

	memset(ChannelCountTable, 0, MAX_HOST_CHANNELS);

	sc = AllocateSlaveOffset;
	for (uint32_t i = 0; i < AllocateNumChannels; i++, sc++)
		ChannelCountTable[sc->HostChnNum & 63]++;

	// OK.. search through and find the most heavily used channel
	uint8_t lowestVol;
	while (true)
	{
		hostChnNum = 0;
		count = 1;
		for (uint8_t i = 0; i < MAX_HOST_CHANNELS; i++)
		{
			if (count < ChannelCountTable[i])
			{
				count = ChannelCountTable[i];
				hostChnNum = i;
			}
		}

		if (count <= 1)
		{
			// Now search for softest disowned sample (not non-single)

			sc = NULL;
			slaveChn_t *scTmp = AllocateSlaveOffset;

			lowestVol = 255;
			for (uint32_t i = 0; i < AllocateNumChannels; i++, scTmp++)
			{
				if ((scTmp->HostChnNum & CHN_DISOWNED) && scTmp->FinalVol7Bit <= lowestVol)
				{
					sc = scTmp;
					lowestVol = scTmp->FinalVol7Bit;
				}
			}

			if (sc == NULL)
			{
				*hcFlags &= ~HF_CHAN_ON;
				return NULL;
			}

			return AllocateChannelInstrument(hc, sc, ins, hcFlags);
		}

		hostChnNum |= CHN_DISOWNED; // Search for disowned only
		sc = NULL; // Offset

		lowestVol = 255;
		uint8_t targetSmp = hc->Smp-1;

		slaveChn_t *scTmp = AllocateSlaveOffset;
		for (uint32_t i = 0; i < AllocateNumChannels; i++, scTmp++)
		{
			if (scTmp->HostChnNum != hostChnNum || scTmp->FinalVol7Bit >= lowestVol)
				continue;

			// Now check if any other channel contains this sample

			if (scTmp->Smp == targetSmp)
			{
				sc = scTmp;
				lowestVol = scTmp->FinalVol7Bit;
				continue;
			}

			slaveChn_t *scTmp2 = AllocateSlaveOffset;

			uint8_t scSmp = scTmp->Smp;
			scTmp->Smp = 255;
			for (uint32_t j = 0; j < AllocateNumChannels; j++, scTmp2++)
			{
				if (scTmp2->Smp == targetSmp || scTmp2->Smp == scSmp)
				{
					// OK found a second sample.
					sc = scTmp;
					lowestVol = scTmp->FinalVol7Bit;
					break;
				}
			}
			scTmp->Smp = scSmp;
		}

		if (sc != NULL)
			break; // 8bb: done

		ChannelCountTable[hostChnNum & 63] = 0; // Next cycle...
	}

	// 8bb: we have a slave channel in sc at this point

	lowestVol = 255;

	slaveChn_t *scTmp = AllocateSlaveOffset;
	for (uint32_t i = 0; i < AllocateNumChannels; i++, scTmp++)
	{
		if (scTmp->Smp == sc->Smp && (scTmp->HostChnNum & CHN_DISOWNED) && scTmp->FinalVol7Bit < lowestVol)
		{
			sc = scTmp;
			lowestVol = scTmp->FinalVol7Bit;
		}
	}

	return AllocateChannelInstrument(hc, sc, ins, hcFlags);
}

uint8_t Random(void) // 8bb: verified to be ported correctly
{
	uint16_t r1, r2, r3, r4;
	
	r1 = RandSeed1;
	r2 = r3 = r4 = RandSeed2;

	r1 += r2;
	r1 = (r1 << (r3 & 15)) | (r1 >> ((16-r3) & 15));
	r1 ^= r4;
	r3 = (r3 >> 8) | (r3 << 8);
	r2 += r3;
	r4 += r2;
	r3 += r1;
	r1 -= r4 + (r2 & 1);
	r2 = (r2 << 15) | (r2 >> 1);

	RandSeed2 = r4;
	RandSeed1 = r1;

	return (uint8_t)r1;
}

void GetLoopInformation(slaveChn_t *sc)
{
	uint8_t LoopMode;
	int32_t LoopBegin, LoopEnd;

	assert(sc->SmpPtr != NULL);
	sample_t *s = sc->SmpPtr;

	bool LoopEnabled = !!(s->Flags & (SMPF_USE_LOOP | SMPF_USE_SUSTAINLOOP));
	bool SustainLoopOnlyAndNoteOff = (s->Flags & SMPF_USE_SUSTAINLOOP) && (sc->Flags & SF_NOTE_OFF) && !(s->Flags & SMPF_USE_LOOP);

	if (!LoopEnabled || SustainLoopOnlyAndNoteOff)
	{
		LoopBegin = 0;
		LoopEnd = s->Length;
		LoopMode = 0;
	}
	else
	{
		LoopBegin = s->LoopBegin;
		LoopEnd = s->LoopEnd;
		LoopMode = s->Flags;

		if (s->Flags & SMPF_USE_SUSTAINLOOP)
		{
			if (!(sc->Flags & SF_NOTE_OFF)) // 8bb: sustain on (note not released)?
			{
				LoopBegin = s->SustainLoopBegin;
				LoopEnd = s->SustainLoopEnd;
				LoopMode >>= 1; // 8bb: loop mode = sustain loop mode
			}
		}

		LoopMode = (LoopMode & SMPF_LOOP_PINGPONG) ? LOOP_PINGPONG : LOOP_FORWARDS; // 8bb: set loop type (Ping-Pong or Forwards)
	}

	// 8bb: if any parameter changed, update all
	if (sc->LoopMode != LoopMode || sc->LoopBegin != LoopBegin || sc->LoopEnd != LoopEnd)
	{
		sc->LoopMode = LoopMode;
		sc->LoopBegin = LoopBegin;
		sc->LoopEnd = LoopEnd;
		sc->Flags |= SF_LOOP_CHANGED;
	}
}

void ApplyRandomValues(hostChn_t *hc)
{
	slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;
	instrument_t *ins = sc->InsPtr;

	hc->Flags &= ~HF_APPLY_RANDOM_VOL;

	int8_t value = Random(); // -128->+127
	if (ins->RandVol != 0) // Random volume, 0->100
	{
		int16_t vol = (((int8_t)ins->RandVol * value) >> 6) + 1;
		vol = sc->SmpVol + ((vol * (int16_t)sc->SmpVol) / 199);

		if (vol < 0)
			vol = 0;
		else if (vol > 128)
			vol = 128;

		sc->SmpVol = (uint8_t)vol;
	}

	value = Random(); // -128->+127
	if (ins->RandPan != 0 && sc->Pan != PAN_SURROUND) // Random pan, 0->64
	{
		int16_t pan = sc->Pan + (((int8_t)ins->RandPan * value) >> 7);

		if (pan < 0)
			pan = 0;
		else if (pan > 64)
			pan = 64;

		sc->Pan = sc->PanSet = (uint8_t)pan;
	}
}

void PitchSlideUp(hostChn_t *hc, slaveChn_t *sc, int16_t SlideValue)
{
	assert(sc != NULL);
	assert(hc != NULL);

	if (Song.Header.Flags & ITF_LINEAR_FRQ)
	{
		// 8bb: linear frequencies
		PitchSlideUpLinear(hc, sc, SlideValue);
	}
	else
	{
		// 8bb: Amiga frequencies

#if USEFPUCODE // 8bb: IT2.15

		const double InitFreq = sc->Frequency;

		double dFreqDiv = (8363.0 * 1712.0) - (InitFreq * SlideValue);

		// 8bb: added this, needed to make it work in extreme edge cases
		if (dFreqDiv <= 0.0)
			dFreqDiv = 1e-9; // 8bb: any tiny positive number

		sc->Flags |= SF_FREQ_CHANGE; // recalculate pitch!

		double dNewFreq = ((8363.0 * 1712.0) * InitFreq) / dFreqDiv;
		if (dNewFreq >= INT32_MAX)
		{
			sc->Flags |= SF_NOTE_STOP; // Turn off channel
			hc->Flags &= ~HF_CHAN_ON;
			return;
		}

		sc->Frequency = (int32_t)dNewFreq; // 8bb: Do not round here! Truncate.

#else // 8bb: IT2.14 and older

		sc->Flags |= SF_FREQ_CHANGE; // recalculate pitch!

		const uint32_t PeriodBase = 1712 * 8363;

		if (SlideValue < 0)
		{
			SlideValue = -SlideValue;

			// 8bb: slide down

			uint64_t FreqSlide64 = (uint64_t)sc->Frequency * (uint32_t)SlideValue;
			if (FreqSlide64 > UINT32_MAX)
			{
				sc->Flags |= SF_NOTE_STOP; // Turn off channel
				hc->Flags &= ~HF_CHAN_ON;
				return;
			}

			FreqSlide64 += PeriodBase;

			uint32_t ShitValue = 0;
			while (FreqSlide64 > UINT32_MAX)
			{
				FreqSlide64 >>= 1;
				ShitValue++;
			}

			uint32_t Temp32 = (uint32_t)FreqSlide64;
			uint64_t Temp64 = (uint64_t)sc->Frequency * (uint32_t)PeriodBase;

			if (ShitValue > 0)
				Temp64 >>= ShitValue;

			if (Temp32 <= Temp64>>32)
			{
				sc->Flags |= SF_NOTE_STOP;
				hc->Flags &= ~HF_CHAN_ON;
				return;
			}

			sc->Frequency = (uint32_t)(Temp64 / Temp32);
		}
		else
		{
			// 8bb: slide up

			uint64_t FreqSlide64 = (uint64_t)sc->Frequency * (uint32_t)SlideValue;
			if (FreqSlide64 > UINT32_MAX)
			{
				sc->Flags |= SF_NOTE_STOP; // Turn off channel
				hc->Flags &= ~HF_CHAN_ON;
				return;
			}

			uint32_t FreqSlide32 = (uint32_t)FreqSlide64;

			uint32_t Temp32 = PeriodBase - FreqSlide32;
			if ((int32_t)Temp32 <= 0)
			{
				sc->Flags |= SF_NOTE_STOP;
				hc->Flags &= ~HF_CHAN_ON;
				return;
			}

			uint64_t Temp64 = (uint64_t)sc->Frequency * (uint32_t)PeriodBase;
			if (Temp32 <= Temp64>>32)
			{
				sc->Flags |= SF_NOTE_STOP;
				hc->Flags &= ~HF_CHAN_ON;
				return;
			}

			sc->Frequency = (uint32_t)(Temp64 / Temp32);
		}
#endif
	}
}

void PitchSlideUpLinear(hostChn_t *hc, slaveChn_t *sc, int16_t SlideValue)
{
	assert(sc != NULL);
	assert(hc != NULL);
	assert(SlideValue >= -1024 && SlideValue <= 1024);

#ifdef USEFPUCODE // 8bb: IT2.15 (registered)

	sc->Flags |= SF_FREQ_CHANGE; // recalculate pitch!

	// 8bb: yes, IT2 really uses 24-bit (float) precision here
	const float fMultiplier = powf(2.0f, SlideValue * (1.0f / 768.0f));

	double dNewFreq = sc->Frequency * (double)fMultiplier;
	if (dNewFreq >= INT32_MAX)
	{
		sc->Flags |= SF_NOTE_STOP; // Turn off channel
		hc->Flags &= ~HF_CHAN_ON;
		return;
	}

	sc->Frequency = (int32_t)nearbyint(dNewFreq); // 8bb: rounded ( nearbyint() is needed over round() )

#else // 8bb: IT2.14 and older (what the vast majority of IT2 users had)

	sc->Flags |= SF_FREQ_CHANGE; // recalculate pitch!

	if (SlideValue < 0)
	{
		// 8bb: slide down

		SlideValue = -SlideValue;

		const uint16_t *SlideTable;
		if (SlideValue <= 15)
		{
			SlideTable = FineLinearSlideDownTable;
		}
		else
		{
			SlideTable = LinearSlideDownTable;
			SlideValue >>= 2;
		}

		sc->Frequency = ((uint64_t)sc->Frequency * SlideTable[SlideValue]) >> 16;
	}
	else
	{
		// 8bb: slide up

		const uint32_t *SlideTable;
		if (SlideValue <= 15)
		{
			SlideTable = FineLinearSlideUpTable;
		}
		else
		{
			SlideTable = LinearSlideUpTable;
			SlideValue >>= 2;
		}

		uint64_t Frequency = ((uint64_t)sc->Frequency * SlideTable[SlideValue]) >> 16;
		if (Frequency & 0xFFFF000000000000)
		{
			sc->Flags |= SF_NOTE_STOP; // Turn off channel
			hc->Flags &= ~HF_CHAN_ON;
		}
		else
		{
			sc->Frequency = (uint32_t)Frequency;
		}
	}
#endif
}

void PitchSlideDown(hostChn_t *hc, slaveChn_t *sc, int16_t SlideValue)
{
	PitchSlideUp(hc, sc, -SlideValue);
}

static uint8_t *Music_GetPattern(uint32_t pattern, uint16_t *numRows)
{
	assert(pattern < MAX_PATTERNS);
	pattern_t *p = &Song.Patt[pattern];

	if (p->PackedData == NULL)
	{
		*numRows = 64;
		return EmptyPattern;
	}

	*numRows = p->Rows;
	return p->PackedData;
}

static void PreInitCommand(hostChn_t *hc)
{
	if (hc->NotePackMask & 0x33)
	{
		if (!(Song.Header.Flags & ITF_INSTR_MODE) || hc->RawNote >= 120 || hc->Ins == 0)
		{
			hc->TranslatedNote = hc->RawNote;
			hc->Smp = hc->Ins;
		}
		else
		{
			instrument_t *ins = &Song.Ins[hc->Ins-1];

			hc->TranslatedNote = ins->SmpNoteTable[hc->RawNote] & 0xFF;

			/* 8bb:
			** Added >128 check to prevent instruments with ModPlug/OpenMPT plugins
			** from being handled as MIDI (would result in silence, and crash IT2).
			*/
			if (ins->MIDIChn == 0 || ins->MIDIChn > 128)
			{
				hc->Smp = ins->SmpNoteTable[hc->RawNote] >> 8;
			}
			else // 8bb: MIDI
			{
				hc->MIDIChn = (ins->MIDIChn == 17) ? (hc->HostChnNum & 0x0F) + 1 : ins->MIDIChn;
				hc->MIDIProg = ins->MIDIProg;
				hc->Smp = 101;
			}

			if (hc->Smp == 0) // No sample?
				return;
		}
	}

	InitCommandTable[hc->Cmd & 31](hc); // Init note

	hc->Flags |= HF_ROW_UPDATED;

	bool ChannelMuted = !!(Song.Header.ChnlPan[hc->HostChnNum] & 128);
	if (ChannelMuted && !(hc->Flags & HF_FREEPLAY_NOTE) && (hc->Flags & HF_CHAN_ON))
		((slaveChn_t *)hc->SlaveChnPtr)->Flags |= SF_CHN_MUTED;
}

static void UpdateGOTONote(void) // Get offset
{
	Song.DecodeExpectedPattern = Song.CurrentPattern;

	uint8_t *p = Music_GetPattern(Song.DecodeExpectedPattern, &Song.NumberOfRows);
	if (Song.ProcessRow >= Song.NumberOfRows)
		Song.ProcessRow = 0;

	Song.DecodeExpectedRow = Song.CurrentRow = Song.ProcessRow;

	uint16_t rowsTodo = Song.ProcessRow;
	if (rowsTodo > 0)
	{
		while (true)
		{
			uint8_t chnNum = *p++;
			if (chnNum == 0)
			{
				rowsTodo--;
				if (rowsTodo == 0)
					break;

				continue;
			}

			hostChn_t *hc = &hChn[(chnNum & 0x7F) - 1];
			if (chnNum & 0x80)
				hc->NotePackMask = *p++;

			if (hc->NotePackMask & 1)
				hc->RawNote = *p++;

			if (hc->NotePackMask & 2)
				hc->Ins = *p++;

			if (hc->NotePackMask & 4)
				hc->Vol = *p++;

			if (hc->NotePackMask & 8)
			{
				hc->OldCmd = *p++;
				hc->OldCmdVal = *p++;
			}
		}
	}

	Song.PatternOffset = p;
}

static void UpdateNoteData(void)
{
	hostChn_t *hc;

	Song.PatternLooping = false;
	if (Song.CurrentPattern != Song.DecodeExpectedPattern || ++Song.DecodeExpectedRow != Song.CurrentRow)
		UpdateGOTONote();

	// First clear all old command&value.
	hc = hChn;
	for (int32_t i = 0; i < MAX_HOST_CHANNELS; i++, hc++)
		hc->Flags &= ~(HF_UPDATE_EFX_IF_CHAN_ON | HF_ALWAYS_UPDATE_EFX | HF_ROW_UPDATED | HF_UPDATE_VOLEFX_IF_CHAN_ON);

	uint8_t *p = Song.PatternOffset;
	while (true)
	{
		uint8_t chnNum = *p++;
		if (chnNum == 0) // No more! else... go through decoding
			break;

		hc = &hChn[(chnNum & 0x7F) - 1];
		if (chnNum & 0x80)
			hc->NotePackMask = *p++;

		if (hc->NotePackMask & 1)
			hc->RawNote = *p++;

		if (hc->NotePackMask & 2)
			hc->Ins = *p++;

		if (hc->NotePackMask & 4)
			hc->Vol = *p++;

		if (hc->NotePackMask & 8)
		{
			hc->Cmd = hc->OldCmd = *p++;
			hc->CmdVal = hc->OldCmdVal = *p++;
		}
		else if (hc->NotePackMask & 128)
		{
			hc->Cmd = hc->OldCmd;
			hc->CmdVal = hc->OldCmdVal;
		}
		else
		{
			hc->Cmd = 0;
			hc->CmdVal = 0;
		}

		PreInitCommand(hc);
	}

	Song.PatternOffset = p;
}

static void UpdateData(void)
{
	// 8bb: I only added the logic for "Play Song" (2) mode

	Song.ProcessTick--;
	Song.CurrentTick--;

	if (Song.CurrentTick == 0)
	{
		Song.ProcessTick = Song.CurrentTick = Song.CurrentSpeed;

		Song.RowDelay--;
		if (Song.RowDelay == 0)
		{
			Song.RowDelay = 1;
			Song.RowDelayOn = false;

			uint16_t NewRow = Song.ProcessRow + 1;
			if (NewRow >= Song.NumberOfRows)
			{
				uint16_t NewOrder = Song.ProcessOrder + 1;
				while (true)
				{
					if (NewOrder >= 256)
					{
						NewOrder = 0;
						continue;
					}

					uint8_t NewPattern = Song.Orders[NewOrder]; // next pattern
					if (NewPattern >= 200)
					{
						if (NewPattern == 0xFE) // 8bb: skip pattern separator
						{
							NewOrder++;
						}
						else
						{
							NewOrder = 0;
							Song.StopSong = true; // 8bb: for WAV rendering
						}
					}
					else
					{
						Song.CurrentPattern = NewPattern;
						break;
					}
				}

				Song.CurrentOrder = Song.ProcessOrder = NewOrder;
				NewRow = Song.BreakRow;
				Song.BreakRow = 0;
			}

			Song.CurrentRow = Song.ProcessRow = NewRow;
			UpdateNoteData();
		}
		else
		{
			hostChn_t *hc = hChn;
			for (int32_t i = 0; i < MAX_HOST_CHANNELS; i++, hc++)
			{
				if (!(hc->Flags & HF_ROW_UPDATED) || !(hc->NotePackMask & 0x88))
					continue;

				uint8_t OldNotePackMask = hc->NotePackMask;
				hc->NotePackMask &= 0x88;
				InitCommandTable[hc->Cmd & 31](hc);
				hc->NotePackMask = OldNotePackMask;
			}
		}
	}
	else
	{
		// OK. call update command.

		hostChn_t *hc = hChn;
		for (int32_t i = 0; i < MAX_HOST_CHANNELS; i++, hc++)
		{
			if ((hc->Flags & HF_CHAN_ON) && (hc->Flags & HF_UPDATE_VOLEFX_IF_CHAN_ON))
				VolumeEffectTable[hc->VolCmd & 7](hc);

			if ((hc->Flags & (HF_UPDATE_EFX_IF_CHAN_ON | HF_ALWAYS_UPDATE_EFX)) &&
				((hc->Flags & HF_ALWAYS_UPDATE_EFX) || (hc->Flags & HF_CHAN_ON)))
			{
				CommandTable[hc->Cmd & 31](hc);
			}
		}
	}
}

static void UpdateAutoVibrato(slaveChn_t *sc) // 8bb: renamed from UpdateVibrato() to UpdateAutoVibrato() for clarity
{
	assert(sc->SmpPtr != NULL);
	sample_t *smp = sc->SmpPtr;

	if (smp->AutoVibratoDepth == 0)
		return;

	sc->AutoVibratoDepth += smp->AutoVibratoRate;
	if (sc->AutoVibratoDepth>>8 > smp->AutoVibratoDepth)
		sc->AutoVibratoDepth = (smp->AutoVibratoDepth << 8) | (sc->AutoVibratoDepth & 0xFF);

	if (smp->AutoVibratoSpeed == 0)
		return;

	int16_t VibratoData;
	if (smp->AutoVibratoWaveform == 3)
	{
		VibratoData = (Random() & 127) - 64;
	}
	else
	{
		sc->AutoVibratoPos += smp->AutoVibratoSpeed; // Update pointer.

		assert(smp->AutoVibratoWaveform < 3);
		VibratoData = FineSineData[(smp->AutoVibratoWaveform << 8) + sc->AutoVibratoPos];
	}

	VibratoData = (VibratoData * (int16_t)(sc->AutoVibratoDepth >> 8)) >> 6;
	if (VibratoData != 0)
		PitchSlideUpLinear(sc->HostChnPtr, sc, VibratoData);
}

static bool UpdateEnvelope(env_t *env, envState_t *envState, bool SustainReleased)
{
	if (envState->Tick < envState->NextTick)
	{
		envState->Tick++;
		envState->Value += envState->Delta;
		return false; // 8bb: last node not reached
	}

	envNode_t *Nodes = env->NodePoints;
	envState->Value = Nodes[envState->CurNode & 0x00FF].Magnitude << 16;
	int16_t NextNode = (envState->CurNode & 0x00FF) + 1;

	if (env->Flags & 6) // 8bb: any loop at all?
	{
		uint8_t LoopBegin = env->LoopBegin;
		uint8_t LoopEnd = env->LoopEnd;

		bool HasLoop = !!(env->Flags & ENVF_LOOP);
		bool HasSustainLoop = !!(env->Flags & ENVF_SUSTAINLOOP);

		bool Looping = true;
		if (HasSustainLoop)
		{
			if (!SustainReleased)
			{
				LoopBegin = env->SustainLoopBegin;
				LoopEnd = env->SustainLoopEnd;
			}
			else if (!HasLoop)
			{
				Looping = false;
			}
		}

		if (Looping && NextNode > LoopEnd)
		{
			envState->CurNode = (envState->CurNode & 0xFF00) | LoopBegin;
			envState->Tick = envState->NextTick = Nodes[envState->CurNode & 0x00FF].Tick;
			return false; // 8bb: last node not reached
		}
	}

	if (NextNode >= env->Num)
		return true; // 8bb: last node reached

	// 8bb: new node

	envState->NextTick = Nodes[NextNode].Tick;
	envState->Tick = Nodes[envState->CurNode & 0x00FF].Tick + 1;

	int16_t TickDelta = envState->NextTick - Nodes[envState->CurNode & 0x00FF].Tick;
	if (TickDelta == 0)
		TickDelta = 1;

	int16_t Delta = Nodes[NextNode].Magnitude - Nodes[envState->CurNode & 0x00FF].Magnitude;
	envState->Delta = (Delta << 16) / TickDelta;
	envState->CurNode = (envState->CurNode & 0xFF00) | (uint8_t)NextNode;

	return false; // 8bb: last node not reached
}

static void UpdateInstruments(void)
{
	slaveChn_t *sc = sChn;
	for (int32_t i = 0; i < MAX_SLAVE_CHANNELS; i++, sc++)
	{
		if (!(sc->Flags & SF_CHAN_ON))
			continue;

		if (sc->Ins != 0xFF) // 8bb: got an instrument?
		{
			int16_t EnvVal;
			instrument_t *ins = sc->InsPtr;
			bool SustainReleased = !!(sc->Flags & SF_NOTE_OFF);

			// 8bb: handle pitch/filter envelope

			if (sc->Flags & SF_PITCHENV_ON)
			{
				if (UpdateEnvelope(&ins->PitchEnv, &sc->PitchEnvState, SustainReleased)) // 8bb: last node reached?
					sc->Flags &= ~SF_PITCHENV_ON;
			}

			if (!(ins->PitchEnv.Flags & ENVF_TYPE_FILTER)) // 8bb: pitch envelope
			{
				EnvVal = (int16_t)((uint32_t)sc->PitchEnvState.Value >> 8);
				EnvVal >>= 3; // 8bb: arithmetic shift

				if (EnvVal != 0)
				{
					PitchSlideUpLinear(sc->HostChnPtr, sc, EnvVal);
					sc->Flags |= SF_FREQ_CHANGE;
				}
			}
			else if (sc->Smp != 100) // 8bb: filter envelope
			{
				EnvVal = (int16_t)((uint32_t)sc->PitchEnvState.Value >> 8);
				EnvVal >>= 6; // 8bb: arithmetic shift, -128..128 (though -512..511 is in theory possible)

				/*
				** 8bb: Some annoying logic.
				**
				** Original asm code:
				**  add bx,128
				**  cmp bh,1
				**  adc bl,-1
				**
				** The code below is confirmed to be correct
				** for the whole -512..511 range.
				**
				** However, EnvVal should only be -128..128
				** (0..256 after +128 add) unless something
				** nasty is going on.
				*/
				EnvVal += 128;
				if (EnvVal & 0xFF00)
					EnvVal--;

				sc->MIDIBank = (sc->MIDIBank & 0xFF00) | (uint8_t)EnvVal; // 8bb: don't mess with upper byte!
				sc->Flags |= SF_RECALC_FINALVOL;
			}

			if (sc->Flags & SF_PANENV_ON)
			{
				sc->Flags |= SF_RECALC_PAN;
				if (UpdateEnvelope(&ins->PanEnv, &sc->PanEnvState, SustainReleased)) // 8bb: last node reached?
					sc->Flags &= ~SF_PANENV_ON;
			}

			bool HandleNoteFade = false;
			bool TurnOffCh = false;

			if (sc->Flags & SF_VOLENV_ON) // Volume envelope on?
			{
				sc->Flags |= SF_RECALC_VOL;

				if (UpdateEnvelope(&ins->VolEnv, &sc->VolEnvState, SustainReleased)) // 8bb: last node reached?
				{
					// Envelope turned off...

					sc->Flags &= ~SF_VOLENV_ON;

					if ((sc->VolEnvState.Value & 0x00FF0000) == 0) // Turn off if end of loop is reached (8bb: last env. point is zero?)
					{
						TurnOffCh = true;
					}
					else
					{
						sc->Flags |= SF_FADEOUT;
						HandleNoteFade = true;
					}
				}
				else
				{
					if (!(sc->Flags & SF_FADEOUT)) // Note fade on?
					{
						// Now, check if loop + sustain off
						if (SustainReleased && (ins->VolEnv.Flags & ENVF_LOOP)) // Normal vol env loop?
						{
							sc->Flags |= SF_FADEOUT;
							HandleNoteFade = true;
						}
					}
					else
					{
						HandleNoteFade = true;
					}
				}
			}
			else if (sc->Flags & SF_FADEOUT) // Note fade??
			{
				HandleNoteFade = true;
			}
			else if (sc->Flags & SF_NOTE_OFF) // Note off issued?
			{
				sc->Flags |= SF_FADEOUT;
				HandleNoteFade = true;
			}

			if (HandleNoteFade)
			{
				sc->FadeOut -= ins->FadeOut;
				if ((int16_t)sc->FadeOut <= 0)
				{
					sc->FadeOut = 0;
					TurnOffCh = true;
				}

				sc->Flags |= SF_RECALC_VOL;
			}

			if (TurnOffCh)
			{
				if (!(sc->HostChnNum & CHN_DISOWNED))
				{
					sc->HostChnNum |= CHN_DISOWNED; // Host channel exists
					((hostChn_t *)sc->HostChnPtr)->Flags &= ~HF_CHAN_ON;
				}

				sc->Flags |= (SF_RECALC_VOL | SF_NOTE_STOP);
			}
		}

		if (sc->Flags & SF_RECALC_VOL) // Calculate volume
		{
			sc->Flags &= ~SF_RECALC_VOL;
			sc->Flags |= SF_RECALC_FINALVOL;

			uint16_t volume = (sc->Vol * sc->ChnVol * sc->FadeOut) >> 7;
			volume = (volume * sc->SmpVol) >> 7;
			volume = (volume * (uint16_t)((uint32_t)sc->VolEnvState.Value >> 8)) >> 14;
			volume = (volume * Song.GlobalVolume) >> 7;
			assert(volume <= 32768);

			sc->FinalVol15Bit = volume; // 8bb: 0..32768
			sc->FinalVol7Bit = volume >> 8; // 8bb: 0..128
		}

		if (sc->Flags & SF_RECALC_PAN) // Change in panning?
		{
			sc->Flags &= ~SF_RECALC_PAN;
			sc->Flags |= SF_PAN_CHANGED;

			if (sc->Pan == PAN_SURROUND)
			{
				sc->FinalPan = sc->Pan;
			}
			else
			{
				int8_t PanVal = 32 - sc->Pan;
				if (PanVal < 0)
				{
					PanVal ^= 255;
					PanVal -= 255;
				}
				PanVal = -PanVal;
				PanVal += 32;

				const int8_t PanEnvVal = (int8_t)(sc->PanEnvState.Value >> 16);
				PanVal = sc->Pan + ((PanVal * PanEnvVal) >> 5);
				PanVal -= 32;

				sc->FinalPan = (int8_t)(((PanVal * (int8_t)(Song.Header.PanSep >> 1)) >> 6) + 32); // 8bb: 0..64
				assert(sc->FinalPan <= 64);
			}
		}

		UpdateAutoVibrato(sc);
	}
}

static void UpdateSamples(void) // 8bb: for songs without instruments
{
	slaveChn_t *sc = sChn;
	for (uint32_t i = 0; i < Driver.NumChannels; i++, sc++)
	{
		if (!(sc->Flags & SF_CHAN_ON))
			continue;

		if (sc->Flags & SF_RECALC_VOL) // 8bb: recalculate volume
		{
			sc->Flags &= ~SF_RECALC_VOL;
			sc->Flags |= SF_RECALC_FINALVOL;

			uint16_t volume = (((sc->Vol * sc->ChnVol * sc->SmpVol) >> 4) * Song.GlobalVolume) >> 7;
			assert(volume <= 32768);

			sc->FinalVol15Bit = volume; // 8bb: 0..32768
			sc->FinalVol7Bit = volume >> 8; // 8bb: 0..128
		}

		if (sc->Flags & SF_RECALC_PAN) // 8bb: recalculate panning
		{
			sc->Flags &= ~SF_RECALC_PAN;
			sc->Flags |= SF_PAN_CHANGED;

			if (sc->Pan == PAN_SURROUND)
			{
				sc->FinalPan = sc->Pan;
			}
			else
			{
				sc->FinalPan = ((((int8_t)sc->Pan - 32) * (int8_t)(Song.Header.PanSep >> 1)) >> 6) + 32; // 8bb: 0..64
				assert(sc->FinalPan <= 64);
			}
		}

		UpdateAutoVibrato(sc);
	}
}

void Update(void)
{
	slaveChn_t *sc = sChn;
	for (uint32_t i = 0; i < MAX_SLAVE_CHANNELS; i++, sc++)
	{
		if (!(sc->Flags & SF_CHAN_ON))
			continue;

		if (sc->Vol != sc->VolSet)
		{
			sc->Vol = sc->VolSet;
			sc->Flags |= SF_RECALC_VOL;
		}

		if (sc->Frequency != sc->FrequencySet)
		{
			sc->Frequency = sc->FrequencySet;
			sc->Flags |= SF_FREQ_CHANGE;
		}
	}

	UpdateData();

	if (Song.Header.Flags & ITF_INSTR_MODE)
		UpdateInstruments();
	else
		UpdateSamples();
}

void Music_FillAudioBuffer(int16_t *buffer, int32_t numSamples)
{
	if (!Song.Playing)
	{
		memset(buffer, 0, numSamples * 2 * sizeof (int16_t));
		return;
	}

	if (DriverMix != NULL)
		DriverMix(numSamples, buffer);
}

bool Music_Init(int32_t mixingFrequency, int32_t mixingBufferSize)
{
	if (FirstTimeInit)
	{
		memset(&Driver, 0, sizeof (Driver));
		FirstTimeInit = false;
	}
	else
	{
		Music_Close();
	}

	if (!SB16_InitDriver(mixingFrequency))
		return false;

	return true;
}

void Music_Close(void) // 8bb: added this
{
	if (DriverClose != NULL)
		DriverClose();
}

void Music_InitTempo(void)
{
	if (DriverSetTempo != NULL)
		DriverSetTempo((uint8_t)Song.Tempo);
}

void Music_Stop(void)
{
	Song.Playing = false;

	MIDITranslate(NULL, sChn, MIDICOMMAND_STOP);

	Song.DecodeExpectedPattern = 0xFFFE;
	Song.DecodeExpectedRow = 0xFFFE;
	Song.RowDelay = 1;
	Song.RowDelayOn = false;
	Song.CurrentRow = 0;
	Song.CurrentOrder = 0;
	Song.CurrentTick = 1;
	Song.BreakRow = 0;

	// 8bb: clear host/slave channels
	memset(hChn, 0, sizeof (hChn));
	memset(sChn, 0, sizeof (sChn));

	hostChn_t *hc = hChn;
	for (uint8_t i = 0; i < MAX_HOST_CHANNELS; i++, hc++)
	{
		hc->HostChnNum = i;
		
		// 8bb: set initial channel pan and channel vol
		hc->ChnPan = Song.Header.ChnlPan[i] & 0x7F;
		hc->ChnVol = Song.Header.ChnlVol[i];
	}
	
	slaveChn_t *sc = sChn;
	for (uint32_t i = 0; i < MAX_SLAVE_CHANNELS; i++, sc++)
		sc->Flags = SF_NOTE_STOP;

	if (Song.Loaded)
	{
		Song.GlobalVolume = Song.Header.GlobalVol;
		Song.ProcessTick = Song.CurrentSpeed = Song.Header.InitialSpeed;
		Song.Tempo = Song.Header.InitialTempo;

		Music_InitTempo();
	}
}

void Music_StopChannels(void)
{
	hostChn_t *hc = hChn;
	for (int32_t i = 0; i < MAX_HOST_CHANNELS; i++, hc++)
	{
		hc->Flags = 0;
		hc->PattLoopStartRow = hc->PattLoopCount = 0;
	}

	slaveChn_t *sc = sChn;
	for (int32_t i = 0; i < MAX_SLAVE_CHANNELS; i++, sc++)
		sc->Flags = SF_NOTE_STOP;
}

void Music_PreviousOrder(void)
{
	if (!Song.Playing)
		return;

	if (Song.CurrentOrder > 0)
	{
		Music_StopChannels();
		
		Song.CurrentOrder -= 2;
		Song.ProcessOrder = Song.CurrentOrder;
		Song.ProcessRow = 0xFFFE;
		Song.CurrentTick = 1;
		Song.RowDelay = 1;
		Song.RowDelayOn = false;
	}
}

void Music_NextOrder(void)
{
	if (!Song.Playing)
		return;

	if (Song.CurrentOrder < 255)
	{
		Music_StopChannels();

		Song.ProcessRow = 0xFFFE;
		Song.CurrentTick = 1;
		Song.RowDelay = 1;
		Song.RowDelayOn = false;
	}
}

void Music_PlaySong(uint16_t order)
{
	if (!Song.Loaded)
		return;

	Music_Stop();

	MIDITranslate(NULL, sChn, MIDICOMMAND_START); // 8bb: this will reset channel filters

	Song.CurrentOrder = order;
	Song.ProcessOrder = order - 1;
	Song.ProcessRow = 0xFFFE;

	// 8bb: reset seed (IT2 only does this at tracker startup, but let's do it here)
	RandSeed1 = 0x1234;
	RandSeed2 = 0x5678;

	MIDIInterpretState = MIDIInterpretType = 0; // 8bb: clear MIDI filter interpretor state

	if (DriverResetMixer != NULL)
		DriverResetMixer();

	Song.Playing = true;
}

void Music_ReleaseSample(uint32_t sample)
{
	assert(sample < MAX_SAMPLES);
	sample_t *smp = &Song.Smp[sample];

	if (smp->OrigData  != NULL) free(smp->OrigData);
	if (smp->OrigDataR != NULL) free(smp->OrigDataR);

	smp->Data  = smp->OrigData  = NULL;
	smp->DataR = smp->OrigDataR = NULL;
}

bool Music_AllocatePattern(uint32_t pattern, uint32_t length)
{
	assert(pattern < MAX_PATTERNS);
	pattern_t *p = &Song.Patt[pattern];

	if (p->PackedData != NULL)
		return true;

	p->PackedData = (uint8_t *)malloc(length);
	if (p->PackedData == NULL)
		return false;

	return true;
}

bool Music_AllocateSample(uint32_t sample, uint32_t length)
{
	assert(sample < MAX_SAMPLES);
	sample_t *s = &Song.Smp[sample];

	// 8bb: done a little differently than IT2

	s->OrigData = (int8_t *)malloc(length + SAMPLE_PAD_LENGTH); // 8bb: extra bytes for interpolation taps, filled later
	if (s->OrigData == NULL)
		return false;

	memset((int8_t *)s->OrigData, 0, SMP_DAT_OFFSET);
	memset((int8_t *)s->OrigData + length, 0, 32);

	// 8bb: offset sample so that we can fix negative interpolation taps
	s->Data = (int8_t *)s->OrigData + SMP_DAT_OFFSET;

	s->Length = length;
	s->Flags |= SMPF_ASSOCIATED_WITH_HEADER;

	return true;
}

bool Music_AllocateRightSample(uint32_t sample, uint32_t length) // 8bb: added this
{
	assert(sample < MAX_SAMPLES);
	sample_t *s = &Song.Smp[sample];

	s->OrigDataR = (int8_t *)malloc(length + SAMPLE_PAD_LENGTH); // 8bb: extra bytes for interpolation taps, filled later
	if (s->OrigDataR == NULL)
		return false;

	memset((int8_t *)s->OrigDataR, 0, SMP_DAT_OFFSET);
	memset((int8_t *)s->OrigDataR + length, 0, 32);

	// 8bb: offset sample so that we can fix negative interpolation taps
	s->DataR = (int8_t *)s->OrigDataR + SMP_DAT_OFFSET;

	return true;
}

void Music_ReleasePattern(uint32_t pattern)
{
	assert(pattern < MAX_PATTERNS);
	pattern_t *p = &Song.Patt[pattern];

	if (p->PackedData != NULL)
		free(p->PackedData);

	p->Rows = 0;
	p->PackedData = NULL;
}

void Music_ReleaseAllPatterns(void)
{
	for (int32_t i = 0; i < MAX_PATTERNS; i++)
		Music_ReleasePattern(i);
}

void Music_ReleaseAllSamples(void)
{
	for (int32_t i = 0; i < MAX_SAMPLES; i++)
		Music_ReleaseSample(i);
}

void Music_FreeSong(void) // 8bb: added this
{
	Music_Stop();

	Music_ReleaseAllPatterns();
	Music_ReleaseAllSamples();

	memset(&Song, 0, sizeof (Song));
	memset(Song.Orders, 255, MAX_ORDERS);

	Song.Loaded = false;
}

int32_t Music_GetActiveVoices(void) // 8bb: added this
{
	int32_t activeVoices = 0;

	slaveChn_t *sc = sChn;
	for (int32_t i = 0; i < MAX_SLAVE_CHANNELS; i++, sc++)
	{
		if (!(sc->Flags & SF_NOTE_STOP) && (sc->Flags & SF_CHAN_ON))
			activeVoices++;
	}

	return activeVoices;
}