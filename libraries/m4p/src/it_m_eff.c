// 8bb: IT2 replayer command routines

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "it_structs.h"
#include "it_tables.h"
#include "it_music.h"
#include "it_m_eff.h"

static const uint8_t SlideTable[9] = { 1, 4, 8, 16, 32, 64, 96, 128, 255 };

static void InitCommandG11(hostChn_t *hc);
static void InitCommandM2(hostChn_t *hc, uint8_t vol);
static void InitCommandX2(hostChn_t *hc, uint8_t pan); // 8bb: pan = 0..63
static void CommandH5(hostChn_t *hc, slaveChn_t *sc, int8_t VibratoData);
static void CommandR2(hostChn_t *hc, slaveChn_t *sc, int8_t TremoloData);

void NoCommand(hostChn_t *hc)
{
	(void)hc;
	return;
}

static void CommandEChain(hostChn_t *hc, uint16_t SlideValue)
{
	slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;
	PitchSlideDown(hc, sc, SlideValue);
	sc->FrequencySet = sc->Frequency;
}

static void CommandFChain(hostChn_t *hc, uint16_t SlideValue)
{
	slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;
	PitchSlideUp(hc, sc, SlideValue);
	sc->FrequencySet = sc->Frequency;
}

static void CommandD2(hostChn_t *hc, slaveChn_t *sc, uint8_t vol)
{
	sc->Vol = sc->VolSet = hc->VolSet = vol;
	sc->Flags |= SF_RECALC_VOL;
}

static void InitVibrato(hostChn_t *hc)
{
	if (Song.Header.Flags & ITF_OLD_EFFECTS)
	{
		slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;

		sc->Flags |= SF_FREQ_CHANGE;
		CommandH5(hc, sc, hc->LastVibratoData);
	}
	else
	{
		CommandH(hc);
	}
}

static void InitCommandD7(hostChn_t *hc, slaveChn_t *sc) // Jmp point for Lxx (8bb: and Dxx/Kxx)
{
	sc->Flags |= SF_RECALC_VOL;

	uint8_t hi = hc->DKL & 0xF0;
	uint8_t lo = hc->DKL & 0x0F;

	if (lo == 0)
	{
		// Slide up.
		hc->VolSlideDelta = hi >> 4;
		hc->Flags |= HF_UPDATE_EFX_IF_CHAN_ON;

		if (hc->VolSlideDelta == 0x0F)
			CommandD(hc);
	}
	else if (hi == 0)
	{
		// Slide down

		hc->VolSlideDelta = -lo;
		hc->Flags |= HF_UPDATE_EFX_IF_CHAN_ON;

		if (hc->VolSlideDelta == -15)
			CommandD(hc);
	}
	else if (lo == 0x0F)
	{
		// Slide up (fine)
		hc->VolSlideDelta = 0;

		uint8_t vol = sc->VolSet + (hi >> 4);
		if (vol > 64)
			vol = 64;

		sc->Vol = sc->VolSet = hc->VolSet = vol;
	}
	else if (hi == 0xF0)
	{
		// Slide down (fine)
		hc->VolSlideDelta = 0;

		uint8_t vol = sc->VolSet - lo;
		if ((int8_t)vol < 0)
			vol = 0;

		sc->Vol = sc->VolSet = hc->VolSet = vol;
	}
}

static void InitVolumeEffect(hostChn_t *hc)
{
	if (!(hc->NotePackMask & 0x44))
		return;

	int8_t volCmd = (hc->Vol & 0x7F) - 65;
	if (volCmd < 0)
		return;

	if (hc->Vol & 0x80)
		volCmd += 60;

	uint8_t cmd = (uint8_t)volCmd / 10;
	uint8_t val = (uint8_t)volCmd % 10;

	hc->VolCmd = cmd; // Store effect number

	/* Memory for effects A->D, (EFG)/H don't share.
	**
	** Effects Ax and Bx (fine volume slide up and down) require immediate
	** handling. No flags required. (effect 0 and 1)
	**
	** Effects Cx, Dx, Ex, Fx (volume/pitch slides) require flag to be
	** set   (effects 2->5)
	**
	** Effects Gx and Hx need init (handling) code + flags.
	** (effects 6 and 7).
	*/

	if (val > 0)
	{
		if (cmd < 4)
		{
			hc->VolCmdVal = val;
		}
		else if (cmd < 6)
		{
			hc->EFG = val << 2;
		}
		else if (cmd == 6)
		{
			if (Song.Header.Flags & ITF_COMPAT_GXX)
				hc->GOE = SlideTable[val-1];
			else
				hc->EFG = SlideTable[val-1];
		}
	}

	if (hc->Flags & HF_CHAN_ON)
	{
		slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;

		if (cmd > 1)
		{
			hc->Flags |= HF_UPDATE_VOLEFX_IF_CHAN_ON;

			if (cmd > 6)
			{
				if (val != 0)
					hc->VibratoDepth = val << 2;

				if (hc->Flags & HF_CHAN_ON)
					InitVibrato(hc);
			}
			else if (cmd == 6)
			{
				InitCommandG11(hc);
			}
		}
		else if (cmd == 1)
		{
			// Fine volume slide down

			int8_t vol = sc->VolSet - hc->VolCmdVal;
			if (vol < 0)
				vol = 0;

			CommandD2(hc, sc, vol);
		}
		else
		{
			// Fine volume slide up

			int8_t vol = sc->VolSet + hc->VolCmdVal;
			if (vol > 64)
				vol = 64;

			CommandD2(hc, sc, vol);
		}
	}
	else
	{
		// Channel not on!

		if (cmd == 7) // Vibrato?
		{
			if (val != 0)
				hc->VibratoDepth = val << 2;

			if (hc->Flags & HF_CHAN_ON)
				InitVibrato(hc);
		}
	}
}

void VolumeCommandC(hostChn_t *hc)
{
	slaveChn_t *sc = hc->SlaveChnPtr;

	int8_t vol = sc->VolSet + hc->VolCmdVal;
	if (vol > 64)
	{
		hc->Flags &= ~HF_UPDATE_VOLEFX_IF_CHAN_ON; // Turn off effect calling
		vol = 64;
	}

	CommandD2(hc, sc, vol);
}

void VolumeCommandD(hostChn_t *hc)
{
	slaveChn_t *sc = hc->SlaveChnPtr;

	int8_t vol = sc->VolSet - hc->VolCmdVal;
	if (vol < 0)
	{
		hc->Flags &= ~HF_UPDATE_VOLEFX_IF_CHAN_ON; // Turn off effect calling
		vol = 0;
	}

	CommandD2(hc, sc, vol);
}

void VolumeCommandE(hostChn_t *hc)
{
	CommandEChain(hc, hc->EFG << 2);
}

void VolumeCommandF(hostChn_t *hc)
{
	CommandFChain(hc, hc->EFG << 2);
}

void VolumeCommandG(hostChn_t *hc)
{
	if (!(hc->Flags & HF_PITCH_SLIDE_ONGOING))
		return;

	int16_t SlideValue = hc->EFG << 2;
	if (Song.Header.Flags & ITF_COMPAT_GXX)
		SlideValue = hc->GOE << 2;

	if (SlideValue == 0)
		return;

	slaveChn_t *sc = hc->SlaveChnPtr;

	if (hc->MiscEfxData[2] == 1) // 8bb: slide up?
	{
		PitchSlideUp(hc, sc, SlideValue);
		sc->FrequencySet = sc->Frequency;

		if ((sc->Flags & SF_NOTE_STOP) || sc->Frequency >= hc->PortaFreq)
		{
			sc->Flags &= ~SF_NOTE_STOP;
			hc->Flags |= HF_CHAN_ON; // Turn on

			sc->FrequencySet = sc->Frequency = hc->PortaFreq;
			hc->Flags &= ~(HF_PITCH_SLIDE_ONGOING | HF_UPDATE_VOLEFX_IF_CHAN_ON); // Turn off calling
		}
	}
	else // 8bb: slide down
	{
		PitchSlideDown(hc, sc, SlideValue);

		if (sc->Frequency <= hc->PortaFreq)
		{
			sc->Frequency = hc->PortaFreq;
			hc->Flags &= ~(HF_PITCH_SLIDE_ONGOING | HF_UPDATE_VOLEFX_IF_CHAN_ON); // Turn off calling
		}

		sc->FrequencySet = sc->Frequency;
	}
}

static void InitNoCommand3(hostChn_t *hc, uint8_t hcFlags)
{
	// Randomise volume if required.

	bool ApplyRandomVolume = !!(hc->Flags & HF_APPLY_RANDOM_VOL);

	hc->Flags = (hc->Flags & 0xFF00) | hcFlags;

	if (ApplyRandomVolume)
		ApplyRandomValues(hc);

	InitVolumeEffect(hc);
}

static void NoOldEffect(hostChn_t *hc, uint8_t hcFlags)
{
	uint8_t vol = hc->Vol;
	if (!((hc->NotePackMask & 0x44) && vol <= 64)) // 8bb: improve this yucky logic...
	{
		if ((hc->NotePackMask & 0x44) && (vol & 0x7F) < 65)
		{
			// Panning set!
			hc->Flags = (hc->Flags & 0xFF00) | hcFlags;
			InitCommandX2(hc, vol - 128);
		}

		if (!(hc->NotePackMask & 0x22) || hc->Smp == 0) // Instrument present?
		{
			InitNoCommand3(hc, hcFlags);
			return;
		}

		vol = Song.Smp[hc->Smp-1].Vol; // Default volume
	}

	hc->VolSet = vol;

	if (hcFlags & HF_CHAN_ON)
	{
		slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;
		sc->Vol = sc->VolSet = vol;
		sc->Flags |= SF_RECALC_VOL;
	}

	InitNoCommand3(hc, hcFlags);
}

static void InitNoCommand11(hostChn_t *hc, slaveChn_t *sc, uint8_t hcFlags)
{
	GetLoopInformation(sc);

	if (!(hc->NotePackMask & (0x22+0x44)))
	{
		InitNoCommand3(hc, hcFlags);
		return;
	}

	if ((Song.Header.Flags & (ITF_INSTR_MODE | ITF_OLD_EFFECTS)) == ITF_INSTR_MODE+ITF_OLD_EFFECTS)
	{
		if ((hc->NotePackMask & 0x22) && hc->Ins != 255)
		{
			sc->FadeOut = 1024;
			InitPlayInstrument(hc, sc, &Song.Ins[hc->Ins-1]);
		}
	}

	NoOldEffect(hc, hcFlags);
}

void InitNoCommand(hostChn_t *hc)
{
	uint8_t hcFlags = hc->Flags & 0xFF;

	if (!(hc->NotePackMask & 0x33))
	{
		NoOldEffect(hc, hcFlags);
		return;
	}

	// Note here! Check for noteoff.
	if (hc->TranslatedNote >= 120)
	{
		if (hcFlags & HF_CHAN_ON)
		{
			slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;

			if (hc->TranslatedNote == 255) // 8bb: note off
			{
				sc->Flags |= SF_NOTE_OFF;
				InitNoCommand11(hc, sc, hcFlags);
				return;
			}
			else if (hc->TranslatedNote == 254) // 8bb: note cut
			{
				hcFlags &= ~HF_CHAN_ON;

				if (sc->Smp == 100 || (Driver.Flags & DF_USES_VOLRAMP))
					sc->Flags |= SF_NOTE_STOP;
				else
					sc->Flags = SF_NOTE_STOP;
			}
			else // 8bb: note fade (?)
			{
				sc->Flags |= SF_FADEOUT;
			}
		}

		NoOldEffect(hc, hcFlags);
		return;
	}

	if (hcFlags & HF_CHAN_ON)
	{
		slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;
		if (!(hc->NotePackMask & 0x11) && sc->Note == hc->RawNote && sc->Ins == hc->Ins)
		{
			NoOldEffect(hc, hcFlags);
			return;
		}
	}

	if ((hc->NotePackMask & 0x44) && hc->Vol >= 193 && hc->Vol <= 202 && (hc->Flags & HF_CHAN_ON))
	{
		InitVolumeEffect(hc);
		return;
	}

	slaveChn_t *sc = AllocateChannel(hc, &hcFlags);
	if (sc == NULL)
	{
		NoOldEffect(hc, hcFlags);
		return;
	}

	// Channel allocated.

	sample_t *s = sc->SmpPtr;

	sc->Vol = sc->VolSet = hc->VolSet;

	if (!(Song.Header.Flags & ITF_INSTR_MODE))
	{
		if (s->DefPan & 0x80)
			hc->ChnPan = sc->Pan = s->DefPan & 127;
	}

	sc->SamplingPosition = 0;
	sc->Frac32 = 0; // 8bb: clear fractional sampling position
	sc->Frac64 = 0; // 8bb: also clear frac for my high-quality driver/mixer
	sc->Frequency = sc->FrequencySet = ((uint64_t)s->C5Speed * (uint32_t)PitchTable[hc->TranslatedNote]) >> 16;

	hcFlags |= HF_CHAN_ON;
	hcFlags &= ~HF_PITCH_SLIDE_ONGOING;

	InitNoCommand11(hc, sc, hcFlags);
}

void InitCommandA(hostChn_t *hc)
{
	if (hc->CmdVal != 0)
	{
		Song.CurrentTick = (Song.CurrentTick - Song.CurrentSpeed) + hc->CmdVal;
		Song.CurrentSpeed = hc->CmdVal;
	}

	InitNoCommand(hc);
}

void InitCommandB(hostChn_t *hc)
{
	/*
	if (hc->CmdVal <= Song.CurrentOrder)
		Song.StopSong = true; // 8bb: for WAV writer
	*/

	Song.ProcessOrder = hc->CmdVal - 1;
	Song.ProcessRow = 0xFFFE;

	InitNoCommand(hc);
}

void InitCommandC(hostChn_t *hc)
{
	if (!Song.PatternLooping)
	{
		Song.BreakRow = hc->CmdVal;
		Song.ProcessRow = 0xFFFE;
	}

	InitNoCommand(hc);
}

void InitCommandD(hostChn_t *hc)
{
	InitNoCommand(hc);

	uint8_t CmdVal = hc->CmdVal;
	if (CmdVal == 0)
		CmdVal = hc->DKL;

	hc->DKL = CmdVal;

	if (!(hc->Flags & HF_CHAN_ON))
		return;

	slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;
	InitCommandD7(hc, sc);
}

void InitCommandE(hostChn_t *hc)
{
	InitNoCommand(hc);

	uint8_t CmdVal = hc->CmdVal;
	if (CmdVal == 0)
		CmdVal = hc->EFG;

	hc->EFG = CmdVal;

	if (!(hc->Flags & HF_CHAN_ON) || hc->EFG == 0)
		return;

	if ((hc->EFG & 0xF0) < 0xE0)
	{
		*(uint16_t *)&hc->MiscEfxData[0] = hc->EFG << 2;
		hc->Flags |= HF_UPDATE_EFX_IF_CHAN_ON; // call update only if necess.
		return;
	}

	if ((hc->EFG & 0x0F) == 0)
		return;

	uint16_t SlideVal = hc->EFG & 0x0F;
	if ((hc->EFG & 0xF0) != 0xE0)
		SlideVal <<= 2;

	slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;
	PitchSlideDown(hc, sc, SlideVal);
	sc->FrequencySet = sc->Frequency;
}

void InitCommandF(hostChn_t *hc)
{
	InitNoCommand(hc);

	uint8_t CmdVal = hc->CmdVal;
	if (CmdVal == 0)
		CmdVal = hc->EFG;

	hc->EFG = CmdVal;

	if (!(hc->Flags & HF_CHAN_ON) || hc->EFG == 0)
		return;

	if ((hc->EFG & 0xF0) < 0xE0)
	{
		*(uint16_t *)&hc->MiscEfxData[0] = hc->EFG << 2;
		hc->Flags |= HF_UPDATE_EFX_IF_CHAN_ON; // call update only if necess.
		return;
	}

	if ((hc->EFG & 0x0F) == 0)
		return;

	uint16_t SlideVal = hc->EFG & 0x0F;
	if ((hc->EFG & 0xF0) != 0xE0)
		SlideVal <<= 2;

	slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;
	PitchSlideUp(hc, sc, SlideVal);
	sc->FrequencySet = sc->Frequency;
}

static bool Gxx_ChangeSample(hostChn_t *hc, slaveChn_t *sc, uint8_t sample)
{
	sc->Flags &= ~(SF_NOTE_STOP | SF_LOOP_CHANGED | SF_CHN_MUTED | SF_VOLENV_ON |
	               SF_PANENV_ON | SF_PITCHENV_ON  | SF_PAN_CHANGED);

	sc->Flags |= SF_NEW_NOTE;

	// Now to update sample info.

	sample_t *s = sc->SmpPtr = &Song.Smp[sample];
	sc->Smp = sample;
	sc->AutoVibratoDepth = 0;
	sc->LoopDirection = 0;
	sc->Frac32 = 0; // 8bb: reset sampling position fraction
	sc->Frac64 = 0; // 8bb: also clear frac for my high-quality driver/mixer
	sc->SamplingPosition = 0;
	sc->SmpVol = s->GlobVol * 2;

	if (!(s->Flags & SMPF_ASSOCIATED_WITH_HEADER))
	{
		// 8bb: turn off channel
		sc->Flags = SF_NOTE_STOP;
		hc->Flags &= ~HF_CHAN_ON;
		return false;
	}

	sc->SmpBitDepth = s->Flags & SMPF_16BIT;
	GetLoopInformation(sc);

	return true;
}

static void InitCommandG11(hostChn_t *hc) // Jumped to from Lxx (8bb: and normal tone portamento)
{
	slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;

	if ((hc->NotePackMask & 0x22) && hc->Smp > 0)
	{
		// Checking for change of sample or instrument.

		bool ChangeInstrument = false;

		if (Song.Header.Flags & ITF_COMPAT_GXX)
		{
			hc->Smp = sc->Smp+1;
			sc->SmpVol = Song.Smp[sc->Smp].GlobVol * 2;

			ChangeInstrument = true;
		}
		else if (hc->Smp != 101) // Don't overwrite note if MIDI!
		{
			const uint8_t hcSmp = hc->Smp-1;
			const uint8_t oldSlaveIns = sc->Ins;

			sc->Note = hc->RawNote;
			sc->Ins = hc->Ins;

			if (sc->Ins != oldSlaveIns) // Ins the same?
			{
				if (sc->Smp != hcSmp) // Sample the same?
				{
					if (!Gxx_ChangeSample(hc, sc, hcSmp))
						return; // 8bb: sample was not assciated with sample header
				}

				ChangeInstrument = true;
			}
			else if (sc->Smp != hcSmp)
			{
				if (!Gxx_ChangeSample(hc, sc, hcSmp))
					return; // 8bb: sample was not assciated with sample header

				ChangeInstrument = true;
			}
		}

		if ((Song.Header.Flags & ITF_INSTR_MODE) && ChangeInstrument)
		{
			// Now for instruments

			instrument_t *ins = &Song.Ins[hc->Ins-1];

			sc->FadeOut = 1024;

			uint16_t oldSCFlags = sc->Flags;
			InitPlayInstrument(hc, sc, ins);

			if (oldSCFlags & SF_CHAN_ON)
				sc->Flags &= ~SF_NEW_NOTE;

			sc->SmpVol = (ins->GlobVol * sc->SmpVol) >> 7;
		}
	}

	if ((Song.Header.Flags & ITF_INSTR_MODE) || (hc->NotePackMask & 0x11))
	{
		// OK. Time to calc freq.

		if (hc->TranslatedNote <= 119)
		{
			// Don't overwrite note if MIDI!
			if (hc->Smp != 101)
				sc->Note = hc->TranslatedNote;

			sample_t *s = sc->SmpPtr;

			hc->PortaFreq = ((uint64_t)s->C5Speed * (uint32_t)PitchTable[hc->TranslatedNote]) >> 16;
			hc->Flags |= HF_PITCH_SLIDE_ONGOING;
		}
		else if (hc->Flags & HF_CHAN_ON)
		{
			if (hc->TranslatedNote == 255)
			{
				sc->Flags |= SF_NOTE_OFF;
				GetLoopInformation(sc);
			}
			else if (hc->TranslatedNote == 254)
			{
				hc->Flags &= ~HF_CHAN_ON;
				sc->Flags = SF_NOTE_STOP;
			}
			else
			{
				sc->Flags |= SF_FADEOUT;
			}
		}
	}

	bool volFromVolColumn = false;
	uint8_t vol = 0; // 8bb: set to 0, just to make the compiler happy..

	if (hc->NotePackMask & 0x44)
	{
		if (hc->Vol <= 64)
		{
			vol = hc->Vol;
			volFromVolColumn = true;
		}
		else
		{
			if ((hc->Vol & 0x7F) < 65)
				InitCommandX2(hc, hc->Vol - 128);
		}
	}

	if (volFromVolColumn || (hc->NotePackMask & 0x22))
	{
		if (!volFromVolColumn)
			vol = sc->SmpPtr->Vol;

		sc->Flags |= SF_RECALC_VOL;
		sc->Vol = sc->VolSet = hc->VolSet = vol;
	}

	if (hc->Flags & HF_PITCH_SLIDE_ONGOING) // Slide on???
	{
		// Work out magnitude + dirn

		uint16_t SlideSpeed;
		if (Song.Header.Flags & ITF_COMPAT_GXX) // Command G memory
			SlideSpeed = hc->GOE << 2;
		else
			SlideSpeed = hc->EFG << 2;

		if (SlideSpeed > 0)
		{
			*(uint16_t *)&hc->MiscEfxData[0] = SlideSpeed;

			if (sc->FrequencySet != hc->PortaFreq)
			{
				if (sc->FrequencySet > hc->PortaFreq)
					hc->MiscEfxData[2] = 0; // slide down
				else
					hc->MiscEfxData[2] = 1; // slide up

				if (!(hc->Flags & HF_UPDATE_VOLEFX_IF_CHAN_ON))
					hc->Flags |= HF_UPDATE_EFX_IF_CHAN_ON; // Update effect if necess.
			}
		}
	}

	// Don't call volume effects if it has a Gxx!
	if (!(hc->Flags & HF_UPDATE_VOLEFX_IF_CHAN_ON))
		InitVolumeEffect(hc);
}

void InitCommandG(hostChn_t *hc)
{
	if (hc->CmdVal != 0)
	{
		if (Song.Header.Flags & ITF_COMPAT_GXX) // Compatibility Gxx?
			hc->GOE = hc->CmdVal;
		else
			hc->EFG = hc->CmdVal;
	}

	if (!(hc->Flags & HF_CHAN_ON))
	{
		InitNoCommand(hc);
		return;
	}

	InitCommandG11(hc);
}

void InitCommandH(hostChn_t *hc)
{
	if ((hc->NotePackMask & 0x11) && hc->RawNote <= 119)
		hc->VibratoPos = hc->LastVibratoData = 0;

	uint8_t speed = (hc->CmdVal >> 4) << 2;
	uint8_t depth = (hc->CmdVal & 0x0F) << 2;

	if (speed > 0)
		hc->VibratoSpeed = speed;

	if (depth > 0)
	{
		if (Song.Header.Flags & ITF_OLD_EFFECTS)
			depth <<= 1;

		hc->VibratoDepth = depth;
	}

	InitNoCommand(hc);

	if (hc->Flags & HF_CHAN_ON)
	{
		hc->Flags |= HF_UPDATE_EFX_IF_CHAN_ON; // Update mode.
		InitVibrato(hc);
	}
}

void InitCommandI(hostChn_t *hc)
{
	InitNoCommand(hc);

	uint8_t CmdVal = hc->CmdVal;
	if (CmdVal > 0)
		hc->I00 = CmdVal;

	if (hc->Flags & HF_CHAN_ON)
	{
		hc->Flags |= HF_UPDATE_EFX_IF_CHAN_ON;

		uint8_t OffTime = hc->I00 & 0x0F;
		uint8_t OnTime = hc->I00 >> 4;
		
		if (Song.Header.Flags & ITF_OLD_EFFECTS)
		{
			OffTime++;
			OnTime++;
		}

		hc->MiscEfxData[0] = OffTime;
		hc->MiscEfxData[1] = OnTime;

		CommandI(hc);
	}
}

void InitCommandJ(hostChn_t *hc)
{
	InitNoCommand(hc);

	*(uint16_t *)&hc->MiscEfxData[0] = 0; // 8bb: clear arp tick counter

	uint8_t CmdVal = hc->CmdVal;
	if (CmdVal == 0)
		CmdVal = hc->J00;

	hc->J00 = CmdVal;

	if (hc->Flags & HF_CHAN_ON)
	{
		hc->Flags |= HF_UPDATE_EFX_IF_CHAN_ON; // Update when channel on

		/* 8bb: Original code stores 16-bit PitchTable memory addresses here,
		** but we store notes instead because we work with bigger pointer sizes.
		** The outcome is the same.
		*/
		*(uint16_t *)&hc->MiscEfxData[2] = 60 + (hc->J00 >> 4);   // 8bb: Tick 1 note
		*(uint16_t *)&hc->MiscEfxData[4] = 60 + (hc->J00 & 0x0F); // 8bb: Tick 2 note
	}
}

void InitCommandK(hostChn_t *hc)
{
	if (hc->CmdVal > 0)
		hc->DKL = hc->CmdVal;

	InitNoCommand(hc);

	if (hc->Flags & HF_CHAN_ON)
	{
		InitVibrato(hc);
		InitCommandD7(hc, (slaveChn_t *)hc->SlaveChnPtr);

		hc->Flags |= HF_ALWAYS_UPDATE_EFX; // Always update.
	}
}

void InitCommandL(hostChn_t *hc)
{
	uint8_t CmdVal = hc->CmdVal;
	if (CmdVal > 0)
		hc->DKL = CmdVal;

	if (hc->Flags & HF_CHAN_ON)
	{
		InitCommandG11(hc);
		InitCommandD7(hc, (slaveChn_t *)hc->SlaveChnPtr);
	}
}

static void InitCommandM2(hostChn_t *hc, uint8_t vol)
{
	if (hc->Flags & HF_CHAN_ON)
	{
		slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;
		sc->ChnVol = vol;
		sc->Flags |= SF_RECALC_VOL;
	}

	hc->ChnVol = vol;
}

void InitCommandM(hostChn_t *hc)
{
	InitNoCommand(hc);

	if (hc->CmdVal <= 0x40)
		InitCommandM2(hc, hc->CmdVal);
}

void InitCommandN(hostChn_t *hc)
{
	uint8_t CmdVal = hc->CmdVal;
	if (CmdVal > 0)
		hc->N00 = CmdVal;

	InitNoCommand(hc);

	uint8_t hi = hc->N00 & 0xF0;
	uint8_t lo = hc->N00 & 0x0F;

	if (lo == 0)
	{
		hc->MiscEfxData[0] = hi >> 4;
		hc->Flags |= HF_ALWAYS_UPDATE_EFX;
	}
	else if (hi == 0)
	{
		hc->MiscEfxData[0] = -lo;
		hc->Flags |= HF_ALWAYS_UPDATE_EFX;
	}
	else if (lo == 0x0F)
	{
		uint8_t vol = hc->ChnVol + (hi >> 4);
		if (vol > 64)
			vol = 64;

		InitCommandM2(hc, vol);
	}
	else if (hi == 0xF0)
	{
		uint8_t vol = hc->ChnVol - lo;
		if ((int8_t)vol < 0)
			vol = 0;

		InitCommandM2(hc, vol);
	}
}

void InitCommandO(hostChn_t *hc)
{
	uint8_t CmdVal = hc->CmdVal;
	if (CmdVal == 0)
		CmdVal = hc->O00;

	hc->O00 = CmdVal;

	InitNoCommand(hc);

	if ((hc->NotePackMask & 0x33) && hc->TranslatedNote < 120 && (hc->Flags & HF_CHAN_ON))
	{
		slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;

		int32_t offset = ((hc->HighSmpOffs << 8) | hc->O00) << 8;
		if (offset >= sc->LoopEnd)
		{
			if (!(Song.Header.Flags & ITF_OLD_EFFECTS))
				return;

			offset = sc->LoopEnd - 1;
		}

		sc->SamplingPosition = offset;
		sc->Frac32 = 0; // 8bb: clear fractional sampling position
		sc->Frac64 = 0; // 8bb: also clear frac for my high-quality driver/mixer
	}
}

void InitCommandP(hostChn_t *hc)
{
	uint8_t CmdVal = hc->CmdVal;
	if (CmdVal > 0)
		hc->P00 = CmdVal;

	InitNoCommand(hc);

	uint8_t pan = hc->ChnPan;
	if (hc->Flags & HF_CHAN_ON)
		pan = ((slaveChn_t *)hc->SlaveChnPtr)->PanSet;

	if (pan == PAN_SURROUND) // Surround??
		return;

	uint8_t hi = hc->P00 & 0xF0;
	uint8_t lo = hc->P00 & 0x0F;

	if (lo == 0)
	{
		hc->MiscEfxData[0] = -(hi >> 4);
		hc->Flags |= HF_ALWAYS_UPDATE_EFX;
	}
	else if (hi == 0)
	{
		hc->MiscEfxData[0] = lo;
		hc->Flags |= HF_ALWAYS_UPDATE_EFX;
	}
	else if (lo == 0x0F)
	{
		pan -= hi >> 4;
		if ((int8_t)pan < 0)
			pan = 0;

		InitCommandX2(hc, pan);
	}
	else if (hi == 0xF0)
	{
		pan += lo;
		if (pan > 64)
			pan = 64;

		InitCommandX2(hc, pan);
	}
}

void InitCommandQ(hostChn_t *hc)
{
	InitNoCommand(hc);

	if (hc->CmdVal > 0)
		hc->Q00 = hc->CmdVal;

	if (!(hc->Flags & HF_CHAN_ON))
		return;

	hc->Flags |= HF_UPDATE_EFX_IF_CHAN_ON;

	if (hc->NotePackMask & 0x11)
		hc->RetrigCount = hc->Q00 & 0x0F;
	else
		CommandQ(hc);
}

static void InitTremelo(hostChn_t *hc)
{
	if (Song.Header.Flags & ITF_OLD_EFFECTS)
	{
		slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;

		sc->Flags |= SF_RECALC_FINALVOL; // Volume change...
		CommandR2(hc, sc, hc->LastTremoloData);
	}
	else
	{
		CommandR(hc);
	}
}

void InitCommandR(hostChn_t *hc)
{
	uint8_t speed = hc->CmdVal >> 4;
	uint8_t depth = hc->CmdVal & 0x0F;

	if (speed > 0)
		hc->TremoloSpeed = speed << 2;

	if (depth > 0)
		hc->TremoloDepth = depth << 1;

	InitNoCommand(hc);

	if (hc->Flags & HF_CHAN_ON)
	{
		hc->Flags |= HF_UPDATE_EFX_IF_CHAN_ON;
		InitTremelo(hc);
	}
}

void InitCommandS(hostChn_t *hc)
{
	uint8_t CmdVal = hc->CmdVal;
	if (CmdVal == 0)
		CmdVal = hc->S00;

	hc->S00 = CmdVal;

	uint8_t cmd = CmdVal & 0xF0;
	uint8_t val = CmdVal & 0x0F;

	hc->MiscEfxData[0] = cmd;
	hc->MiscEfxData[1] = val;

	switch (cmd)
	{
		default:
		case 0x00:
		case 0x10:
		case 0x20:
			InitNoCommand(hc);
		break;

		case 0x30: // set vibrato waveform
		{
			if (val <= 3)
				hc->VibratoWaveform = val;

			InitNoCommand(hc);
		}
		break;

		case 0x40: // set tremelo waveform
		{
			if (val <= 3)
				hc->TremoloWaveform = val;

			InitNoCommand(hc);
		}
		break;

		case 0x50: // set panbrello waveform
		{
			if (val <= 3)
			{
				hc->PanbrelloWaveform = val;
				hc->PanbrelloPos = 0;
			}

			InitNoCommand(hc);
		}
		break;

		case 0x60: // extra delay of x frames
		{
			Song.CurrentTick += val;
			Song.ProcessTick += val;
			InitNoCommand(hc);
		}
		break;

		case 0x70: // instrument functions
		{
			switch (val)
			{
				default:
				case 0xD:
				case 0xE:
				case 0xF:
					InitNoCommand(hc);
				break;

				case 0x0: // Past note cut
				{
					InitNoCommand(hc);

					const uint8_t targetHostChnNum = hc->HostChnNum | CHN_DISOWNED;

					slaveChn_t *sc = sChn;
					for (int32_t i = 0; i < MAX_SLAVE_CHANNELS; i++, sc++)
					{
						if (sc->HostChnNum == targetHostChnNum)
						{
							if (Driver.Flags & DF_USES_VOLRAMP)
								sc->Flags |= SF_NOTE_STOP;
							else
								sc->Flags = SF_NOTE_STOP;
						}
					}
				}
				break;

				case 0x1: // Past note off
				{
					InitNoCommand(hc);

					const uint8_t targetHostChnNum = hc->HostChnNum | CHN_DISOWNED;

					slaveChn_t *sc = sChn;
					for (int32_t i = 0; i < MAX_SLAVE_CHANNELS; i++, sc++)
					{
						if (sc->HostChnNum == targetHostChnNum)
							sc->Flags |= SF_NOTE_OFF;
					}
				}
				break;

				case 0x2: // Past note fade
				{
					InitNoCommand(hc);

					const uint8_t targetHostChnNum = hc->HostChnNum | CHN_DISOWNED;

					slaveChn_t *sc = sChn;
					for (int32_t i = 0; i < MAX_SLAVE_CHANNELS; i++, sc++)
					{
						if (sc->HostChnNum == targetHostChnNum)
							sc->Flags |= SF_FADEOUT;
					}
				}
				break;

				case 0x3: // Set NNA to cut
				{
					InitNoCommand(hc);
					if (hc->Flags & HF_CHAN_ON)
						((slaveChn_t *)hc->SlaveChnPtr)->NNA = 0;
				}
				break;

				case 0x4: // Set NNA to continue
				{
					InitNoCommand(hc);
					if (hc->Flags & HF_CHAN_ON)
						((slaveChn_t *)hc->SlaveChnPtr)->NNA = 1;
				}
				break;

				case 0x5: // Set NNA to off
				{
					InitNoCommand(hc);
					if (hc->Flags & HF_CHAN_ON)
						((slaveChn_t *)hc->SlaveChnPtr)->NNA = 2;
				}
				break;

				case 0x6: // Set NNA to fade
				{
					InitNoCommand(hc);
					if (hc->Flags & HF_CHAN_ON)
						((slaveChn_t *)hc->SlaveChnPtr)->NNA = 3;
				}
				break;

				case 0x7: // Set volume envelope off
				{
					InitNoCommand(hc);
					if (hc->Flags & HF_CHAN_ON)
						((slaveChn_t *)hc->SlaveChnPtr)->Flags &= ~SF_VOLENV_ON;
				}
				break;

				case 0x8: // Set volume envelope on
				{
					InitNoCommand(hc);
					if (hc->Flags & HF_CHAN_ON)
						((slaveChn_t *)hc->SlaveChnPtr)->Flags |= SF_VOLENV_ON;
				}
				break;

				case 0x9: // Set panning envelope off
				{
					InitNoCommand(hc);
					if (hc->Flags & HF_CHAN_ON)
						((slaveChn_t *)hc->SlaveChnPtr)->Flags &= ~SF_PANENV_ON;
				}
				break;

				case 0xA: // Set panning envelope on
				{
					InitNoCommand(hc);
					if (hc->Flags & HF_CHAN_ON)
						((slaveChn_t *)hc->SlaveChnPtr)->Flags |= SF_PANENV_ON;
				}
				break;

				case 0xB: // Set pitch envelope off
				{
					InitNoCommand(hc);
					if (hc->Flags & HF_CHAN_ON)
						((slaveChn_t *)hc->SlaveChnPtr)->Flags &= ~SF_PITCHENV_ON;
				}
				break;

				case 0xC: // Set pitch envelope on
				{
					InitNoCommand(hc);
					if (hc->Flags & HF_CHAN_ON)
						((slaveChn_t *)hc->SlaveChnPtr)->Flags |= SF_PITCHENV_ON;
				}
				break;
			}
		}
		break;

		case 0x80: // set pan
		{
			uint8_t pan = (((val << 4) | val) + 2) >> 2;
			InitNoCommand(hc);
			InitCommandX2(hc, pan);
		}
		break;

		case 0x90: // set surround
		{
			InitNoCommand(hc);
			if (val == 1)
				InitCommandX2(hc, PAN_SURROUND);
		}
		break;

		case 0xA0: // Set high order offset
		{
			hc->HighSmpOffs = val;
			InitNoCommand(hc);
		}
		break;

		case 0xB0: // loop control (8bb: pattern loop)
		{
			InitNoCommand(hc);

			if (val == 0)
			{
				hc->PattLoopStartRow = (uint8_t)Song.CurrentRow;
			}
			else if (hc->PattLoopCount == 0)
			{
				hc->PattLoopCount = val;
				Song.ProcessRow = hc->PattLoopStartRow - 1;
				Song.PatternLooping = true;
			}
			else if (--hc->PattLoopCount != 0)
			{
				Song.ProcessRow = hc->PattLoopStartRow - 1;
				Song.PatternLooping = true;
			}
			else
			{
				hc->PattLoopStartRow = (uint8_t)Song.CurrentRow + 1;
			}
		}
		break;

		case 0xC0: // note cut
		{
			hc->Flags |= HF_UPDATE_EFX_IF_CHAN_ON;
			InitNoCommand(hc);
		}
		break;

		case 0xD0: // note delay
		{
			hc->Flags |= HF_ALWAYS_UPDATE_EFX;
		}
		break;

		case 0xE0: // pattern delay
		{
			if (!Song.RowDelayOn)
			{
				Song.RowDelay = val + 1;
				Song.RowDelayOn = true;
			}

			InitNoCommand(hc);
		}
		break;

		case 0xF0: // MIDI Macro select
		{
			hc->SFx = val;
			InitNoCommand(hc);
		}
		break;
	}
}

void InitCommandT(hostChn_t *hc)
{
	uint8_t CmdVal = hc->CmdVal;
	if (CmdVal == 0)
		CmdVal = hc->T00;

	hc->T00 = CmdVal;

	if (CmdVal >= 0x20)
	{
		Song.Tempo = CmdVal;
		Music_InitTempo();
		InitNoCommand(hc);
	}
	else
	{
		InitNoCommand(hc);
		hc->Flags |= HF_ALWAYS_UPDATE_EFX; // Update mode
	}
}

void InitCommandU(hostChn_t *hc)
{
	if (hc->NotePackMask & 0x11)
		hc->VibratoPos = hc->LastVibratoData = 0;

	uint8_t speed = (hc->CmdVal >> 4) << 2;
	uint8_t depth = hc->CmdVal & 0x0F;

	if (speed > 0)
		hc->VibratoSpeed = speed;

	if (depth > 0)
	{
		if (Song.Header.Flags & ITF_OLD_EFFECTS)
			depth <<= 1;

		hc->VibratoDepth = depth;
	}

	InitNoCommand(hc);

	if (hc->Flags & HF_CHAN_ON)
	{
		hc->Flags |= HF_UPDATE_EFX_IF_CHAN_ON; // Update mode.
		InitVibrato(hc);
	}
}

void InitCommandV(hostChn_t *hc)
{
	if (hc->CmdVal <= 0x80)
	{
		Song.GlobalVolume = hc->CmdVal;
		RecalculateAllVolumes();
	}

	InitNoCommand(hc);
}

void InitCommandW(hostChn_t *hc)
{
	InitNoCommand(hc);

	if (hc->CmdVal > 0)
		hc->W00 = hc->CmdVal;

	if (hc->W00 == 0)
		return;

	uint8_t hi = hc->W00 & 0xF0;
	uint8_t lo = hc->W00 & 0x0F;

	if (lo == 0)
	{
		hc->MiscEfxData[0] = hi >> 4;
		hc->Flags |= HF_ALWAYS_UPDATE_EFX;
	}
	else if (hi == 0)
	{
		hc->MiscEfxData[0] = -lo;
		hc->Flags |= HF_ALWAYS_UPDATE_EFX;
	}
	else if (lo == 0x0F)
	{
		uint16_t vol = Song.GlobalVolume + (hi >> 4);
		if (vol > 128)
			vol = 128;

		Song.GlobalVolume = vol;
		RecalculateAllVolumes();
	}
	else if (hi == 0xF0)
	{
		uint16_t vol = Song.GlobalVolume - lo;
		if ((int16_t)vol < 0)
			vol = 0;

		Song.GlobalVolume = vol;
		RecalculateAllVolumes();
	}
}

static void InitCommandX2(hostChn_t *hc, uint8_t pan) // 8bb: pan = 0..63
{
	if (hc->Flags & HF_CHAN_ON)
	{
		slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;
		sc->Pan = sc->PanSet = pan;
		sc->Flags |= (SF_RECALC_PAN | SF_RECALC_FINALVOL);
	}

	hc->ChnPan = pan;
}

void InitCommandX(hostChn_t *hc)
{
	InitNoCommand(hc);

	uint8_t pan = (hc->CmdVal + 2) >> 2; // 8bb: 0..255 -> 0..63 (rounded)
	InitCommandX2(hc, pan);
}

void InitCommandY(hostChn_t *hc)
{
	uint8_t speed = hc->CmdVal >> 4;
	uint8_t depth = hc->CmdVal & 0x0F;

	if (speed > 0)
		hc->PanbrelloSpeed = speed;

	if (depth > 0)
		hc->PanbrelloDepth = depth << 1;

	InitNoCommand(hc);

	if (hc->Flags & HF_CHAN_ON)
	{
		hc->Flags |= HF_UPDATE_EFX_IF_CHAN_ON; // Update mode.
		CommandY(hc);
	}
}

void InitCommandZ(hostChn_t *hc) // Macros start at 120h, 320h
{
	InitNoCommand(hc);

	slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;

	if (hc->CmdVal >= 0x80) // Macros!
		MIDITranslate(hc, sc, 0x320 + ((hc->CmdVal & 0x7F) << 5));
	else
		MIDITranslate(hc, sc, 0x120 + ((hc->SFx & 0xF) << 5));
}

void CommandD(hostChn_t *hc)
{
	slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;

	uint8_t vol = sc->VolSet + hc->VolSlideDelta;
	if ((int8_t)vol < 0)
	{
		hc->Flags &= ~HF_UPDATE_EFX_IF_CHAN_ON;
		vol = 0;
	}
	else if (vol > 64)
	{
		hc->Flags &= ~HF_UPDATE_EFX_IF_CHAN_ON;
		vol = 64;
	}

	CommandD2(hc, sc, vol);
}

void CommandE(hostChn_t *hc)
{
	CommandEChain(hc, *(uint16_t *)&hc->MiscEfxData[0]);
}

void CommandF(hostChn_t *hc)
{
	CommandFChain(hc, *(uint16_t *)&hc->MiscEfxData[0]);
}

void CommandG(hostChn_t *hc)
{
	if (!(hc->Flags & HF_PITCH_SLIDE_ONGOING))
		return;

	uint16_t SlideValue = *(uint16_t *)&hc->MiscEfxData[0];
	slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;

	if (hc->MiscEfxData[2] == 1) // 8bb: slide direction
	{
		// Slide up!

		PitchSlideUp(hc, sc, SlideValue);

		/* Check that:
		**  1) Channel is on
		**  2) Frequency (set) is below porta to frequency
		*/

		if (!(sc->Flags & SF_NOTE_STOP) && sc->Frequency < hc->PortaFreq)
		{
			sc->FrequencySet = sc->Frequency;
		}
		else
		{
			sc->Flags &= ~SF_NOTE_STOP;
			hc->Flags |= HF_CHAN_ON; // Turn on.

			sc->Frequency = sc->FrequencySet = hc->PortaFreq;
			hc->Flags &= ~(HF_UPDATE_EFX_IF_CHAN_ON | HF_ALWAYS_UPDATE_EFX | HF_PITCH_SLIDE_ONGOING); // Turn off calling
		}
	}
	else
	{
		// Slide down

		PitchSlideDown(hc, sc, SlideValue);
		
		// Check that frequency is above porta to frequency.
		if (sc->Frequency > hc->PortaFreq)
		{
			sc->FrequencySet = sc->Frequency;
		}
		else
		{
			sc->Frequency = sc->FrequencySet = hc->PortaFreq;
			hc->Flags &= ~(HF_UPDATE_EFX_IF_CHAN_ON | HF_ALWAYS_UPDATE_EFX | HF_PITCH_SLIDE_ONGOING); // Turn off calling
		}
	}
}

static void CommandH5(hostChn_t *hc, slaveChn_t *sc, int8_t VibratoData)
{
	VibratoData = (((VibratoData * (int8_t)hc->VibratoDepth) << 2) + 128) >> 8;
	if (Song.Header.Flags & ITF_OLD_EFFECTS)
		VibratoData = -VibratoData;

	if (VibratoData < 0)
		PitchSlideDown(hc, sc, -VibratoData);
	else
		PitchSlideUp(hc, sc, VibratoData);
}

void CommandH(hostChn_t *hc)
{
	slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;
	sc->Flags |= SF_FREQ_CHANGE;

	hc->VibratoPos += hc->VibratoSpeed;

	int8_t VibratoData;
	if (hc->VibratoWaveform == 3)
		VibratoData = (Random() & 127) - 64;
	else
		VibratoData = FineSineData[(hc->VibratoWaveform << 8) + hc->VibratoPos];

	hc->LastVibratoData = VibratoData; // Save last vibrato.
	CommandH5(hc, sc, VibratoData);
}

void CommandI(hostChn_t *hc)
{
	slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;
	sc->Flags |= SF_RECALC_VOL;

	hc->TremorCount--;
	if ((int8_t)hc->TremorCount <= 0)
	{
		hc->TremorOnOff ^= 1;
		hc->TremorCount = hc->MiscEfxData[hc->TremorOnOff];
	}

	if (hc->TremorOnOff != 1)
		sc->Vol = 0;
}

void CommandJ(hostChn_t *hc)
{
	slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;
	uint16_t tick = *(uint16_t *)&hc->MiscEfxData[0];

	sc->Flags |= SF_FREQ_CHANGE;

	// 8bb: used as an index to a 16-bit LUT (hence increments of 2)
	tick += 2;
	if (tick >= 6)
	{
		*(uint16_t *)&hc->MiscEfxData[0] = 0;
		return;
	}

	*(uint16_t *)&hc->MiscEfxData[0] = tick;

	const uint16_t arpNote = *(uint16_t *)&hc->MiscEfxData[tick];

	uint64_t freq = (uint64_t)sc->Frequency * (uint32_t)PitchTable[arpNote];
	if (freq & 0xFFFF000000000000) // 8bb: arp freq overflow
		sc->Frequency = 0;
	else
		sc->Frequency = (uint32_t)(freq >> 16);
}

void CommandK(hostChn_t *hc)
{
	CommandH(hc);
	CommandD(hc);
}

void CommandL(hostChn_t *hc)
{
	if (hc->Flags & HF_PITCH_SLIDE_ONGOING)
	{
		CommandG(hc);
		hc->Flags |= HF_UPDATE_EFX_IF_CHAN_ON;
	}

	CommandD(hc);
}

void CommandN(hostChn_t *hc)
{
	uint8_t vol = hc->ChnVol + (int8_t)hc->MiscEfxData[0];

	if ((int8_t)vol < 0)
		vol = 0;
	else if (vol > 64)
		vol = 64;

	InitCommandM2(hc, vol);
}

void CommandP(hostChn_t *hc)
{
	uint8_t pan = hc->ChnPan;
	if (hc->Flags & HF_CHAN_ON)
		pan = ((slaveChn_t *)hc->SlaveChnPtr)->PanSet;

	pan += hc->MiscEfxData[0];

	if ((int8_t)pan < 0)
		pan = 0;
	else if (pan > 64)
		pan = 64;

	InitCommandX2(hc, pan);
}

void CommandQ(hostChn_t *hc)
{
	hc->RetrigCount--;
	if ((int8_t)hc->RetrigCount > 0)
		return;

	// OK... reset counter.
	hc->RetrigCount = hc->Q00 & 0x0F;

	// retrig count done.

	slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;
	if (Driver.Flags & DF_USES_VOLRAMP)
	{
		if (Song.Header.Flags & ITF_INSTR_MODE)
		{
			slaveChn_t *scTmp = sChn;
			for (int32_t i = 0; i < MAX_SLAVE_CHANNELS; i++, scTmp++)
			{
				if (!(scTmp->Flags & SF_CHAN_ON))
				{
					memcpy(scTmp, sc, sizeof (slaveChn_t));
					sc->Flags |= SF_NOTE_STOP; // Cut
					sc->HostChnNum |= CHN_DISOWNED;

					sc = scTmp;
					hc->SlaveChnPtr = scTmp;
					break;
				}
			}
		}
		else // 8bb: samples-only mode
		{
			slaveChn_t *scTmp = sc + MAX_HOST_CHANNELS;
			memcpy(scTmp, sc, sizeof (slaveChn_t));
			scTmp->Flags |= SF_NOTE_STOP; // Cut
			scTmp->HostChnNum |= CHN_DISOWNED;
		}
	}

	sc->Frac32 = 0; // 8bb: clear sampling position fraction
	sc->Frac64 = 0; // 8bb: also clear frac for my high-quality driver/mixer
	sc->SamplingPosition = 0;

	sc->Flags |= (SF_RECALC_FINALVOL | SF_NEW_NOTE | SF_LOOP_CHANGED);

	uint8_t vol = sc->VolSet;
	switch (hc->Q00 >> 4)
	{
		default:
		case 0x0: return;
		case 0x1: vol -=  1; break;
		case 0x2: vol -=  2; break;
		case 0x3: vol -=  4; break;
		case 0x4: vol -=  8; break;
		case 0x5: vol -= 16; break;
		case 0x6: vol = (vol << 1) / 3; break;
		case 0x7: vol >>= 1; break;
		case 0x8: return;
		case 0x9: vol +=  1; break;
		case 0xA: vol +=  2; break;
		case 0xB: vol +=  4; break;
		case 0xC: vol +=  8; break;
		case 0xD: vol += 16; break;
		case 0xE: vol = (vol * 3) >> 1; break;
		case 0xF: vol <<= 1; break;
	}
	
	if ((int8_t)vol < 0)
		vol = 0;
	else if (vol > 64)
		vol = 64;

	sc->VolSet = sc->Vol = hc->VolSet = vol;
	sc->Flags |= SF_RECALC_VOL;

	if (hc->Smp == 101) // MIDI sample
		MIDITranslate(hc, sc, MIDICOMMAND_STOPNOTE);
}

static void CommandR2(hostChn_t *hc, slaveChn_t *sc, int8_t TremoloData)
{
	TremoloData = (((TremoloData * (int8_t)hc->TremoloDepth) << 2) + 128) >> 8;

	int16_t vol = sc->Vol + TremoloData;
	if (vol < 0)
		vol = 0;
	else if (vol > 64)
		vol = 64;

	sc->Vol = (uint8_t)vol;
}

void CommandR(hostChn_t *hc)
{
	slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;
	sc->Flags |= SF_RECALC_VOL;

	hc->TremoloPos += hc->TremoloSpeed;

	int8_t TremoloData;
	if (hc->TremoloWaveform == 3)
		TremoloData = (Random() & 127) - 64;
	else
		TremoloData = FineSineData[(hc->TremoloWaveform << 8) + hc->TremoloPos];

	hc->LastTremoloData = TremoloData; // Save last tremelo
	CommandR2(hc, sc, TremoloData);
}

void CommandS(hostChn_t *hc)
{
	// Have to handle SDx, SCx

	const uint8_t SCmd = hc->MiscEfxData[0];
	if (SCmd == 0xD0) // 8bb: Note delay
	{
		hc->MiscEfxData[1]--;
		if ((int8_t)hc->MiscEfxData[1] > 0)
			return;

		hc->Flags &= ~(HF_UPDATE_EFX_IF_CHAN_ON | HF_ALWAYS_UPDATE_EFX);
		InitNoCommand(hc);
		hc->Flags |= HF_ROW_UPDATED;

		bool ChannelMuted = !!(Song.Header.ChnlPan[hc->HostChnNum] & 128);
		if (ChannelMuted && !(hc->Flags & HF_FREEPLAY_NOTE) && (hc->Flags & HF_CHAN_ON))
			((slaveChn_t *)hc->SlaveChnPtr)->Flags |= SF_CHN_MUTED;
	}
	else if (SCmd == 0xC0) // Note cut.
	{
		if (!(hc->Flags & HF_CHAN_ON))
			return;

		hc->MiscEfxData[1]--;
		if ((int8_t)hc->MiscEfxData[1] > 0)
			return;

		slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;

		hc->Flags &= ~HF_CHAN_ON;

		if (sc->Smp == 100 || (Driver.Flags & DF_USES_VOLRAMP))
			sc->Flags |= SF_NOTE_STOP;
		else
			sc->Flags = SF_NOTE_STOP;
	}
}

void CommandT(hostChn_t *hc)
{
	int16_t Tempo = Song.Tempo;

	if (hc->T00 & 0xF0)
	{
		// Slide Up
		Tempo += hc->T00 - 16;
		if (Tempo > 255)
			Tempo = 255;
	}
	else
	{
		// Slide Down
		Tempo -= hc->T00;
		if (Tempo < 32)
			Tempo = 32;
	}

	Song.Tempo = Tempo;
	DriverSetTempo((uint8_t)Tempo);
}

void CommandW(hostChn_t *hc)
{
	uint16_t vol = Song.GlobalVolume + (int8_t)hc->MiscEfxData[0];

	if ((int16_t)vol < 0)
		vol = 0;
	else if (vol > 128)
		vol = 128;

	Song.GlobalVolume = vol;
	RecalculateAllVolumes();
}

void CommandY(hostChn_t *hc)
{
	if (!(hc->Flags & HF_CHAN_ON))
		return;

	slaveChn_t *sc = (slaveChn_t *)hc->SlaveChnPtr;

	int8_t panData;
	if (hc->PanbrelloWaveform >= 3) // 8bb: panbrello waveform
	{
		// Random panning make speed the delay time.

		hc->PanbrelloPos--;
		if ((int8_t)hc->PanbrelloPos <= 0)
		{
			hc->PanbrelloPos = hc->PanbrelloSpeed; // reset countdown.
			hc->LastPanbrelloData = panData = (Random() & 127) - 64;
		}
		else
		{
			panData = hc->LastPanbrelloData;
		}
	}
	else
	{
		hc->PanbrelloPos += hc->PanbrelloSpeed;
		panData = FineSineData[(hc->PanbrelloWaveform << 8) + hc->PanbrelloPos];
	}

	if (sc->PanSet != PAN_SURROUND)
	{
		panData = (((panData * (int8_t)hc->PanbrelloDepth) << 2) + 128) >> 8;
		panData += sc->PanSet;

		if (panData < 0)
			panData = 0;
		else if (panData > 64)
			panData = 64;

		sc->Flags |= SF_RECALC_PAN;
		sc->Pan = panData;
	}
}
