/* - main XM replayer -
**
** NOTE: Effect handling is slightly different because
** I've removed the channel muting logic.
** Muted channels would only process *some* effects, but
** since we can't mute channels, we don't care about this.
**
** In FT2, the only way to mute a channel is through the
** tracker itself, so this is not really needed in a replayer.
*/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "pmplay.h"
#include "pmp_mix.h"
#include "snd_masm.h"
#include "ft_tables.h"

#define MAX_FRQ 32000
#define MAX_NOTES (10*12*16+16)

static tonTyp nilPatternLine[32]; // 8bb: used for non-allocated (empty) patterns

typedef void (*volKolEfxRoutine)(stmTyp *ch);
typedef void (*volKolEfxRoutine2)(stmTyp *ch, uint8_t *volKol);
typedef void (*efxRoutine)(stmTyp *ch, uint8_t param);

static void retrigVolume(stmTyp *ch)
{
	ch->realVol = ch->oldVol;
	ch->outVol = ch->oldVol;
	ch->outPan = ch->oldPan;
	ch->status |= IS_Vol + IS_Pan + IS_QuickVol;
}

static void retrigEnvelopeVibrato(stmTyp *ch)
{
	// 8bb: reset vibrato position
	if (!(ch->waveCtrl & 0x04))
		ch->vibPos = 0;

	/*
	** 8bb:
	** In FT2.00 .. FT2.09, if the sixth bit of "ch->waveCtrl" is set
	** (from effect E7x where x is $4..$7 or $C..$F) and you trigger a note,
	** the replayer interrupt will freeze / lock up. This is because of a
	** label bug in the original code, causing it to jump back to itself
	** indefinitely.
	*/

	// 8bb: safely reset tremolo position
	if (!(ch->waveCtrl & 0x40))
		ch->tremPos = 0;

	ch->retrigCnt = 0;
	ch->tremorPos = 0;

	ch->envSustainActive = true;

	instrTyp *ins = ch->instrSeg;

	if (ins->envVTyp & ENV_ENABLED)
	{
		ch->envVCnt = 65535;
		ch->envVPos = 0;
	}

	if (ins->envPTyp & ENV_ENABLED)
	{
		ch->envPCnt = 65535;
		ch->envPPos = 0;
	}

	ch->fadeOutSpeed = ins->fadeOut; // 8bb: ranges 0..4095 (FT2 doesn't check if it's higher than 4095!)

	// 8bb: final fadeout range is in fact 0..32768, and not 0..65536 like the XM format doc says
	ch->fadeOutAmp = 32768;

	if (ins->vibDepth > 0)
	{
		ch->eVibPos = 0;

		if (ins->vibSweep > 0)
		{
			ch->eVibAmp = 0;
			ch->eVibSweep = (ins->vibDepth << 8) / ins->vibSweep;
		}
		else
		{
			ch->eVibAmp = ins->vibDepth << 8;
			ch->eVibSweep = 0;
		}
	}
}

static void keyOff(stmTyp *ch)
{
	instrTyp *ins = ch->instrSeg;

	if (!(ins->envPTyp & ENV_ENABLED)) // 8bb: probably an FT2 bug
	{
		if (ch->envPCnt >= (uint16_t)ins->envPP[ch->envPPos][0])
			ch->envPCnt = ins->envPP[ch->envPPos][0]-1;
	}

	if (ins->envVTyp & ENV_ENABLED)
	{
		if (ch->envVCnt >= (uint16_t)ins->envVP[ch->envVPos][0])
			ch->envVCnt = ins->envVP[ch->envVPos][0]-1;
	}
	else
	{
		ch->realVol = 0;
		ch->outVol = 0;
		ch->status |= IS_Vol + IS_QuickVol;
	}

	ch->envSustainActive = false;
}

uint32_t getFrequenceValue(uint16_t period) // 8bb: converts period to 16.16fp resampling delta
{
	uint32_t delta;

	if (period == 0)
		return 0;

	if (linearFrqTab)
	{
		const uint16_t invPeriod = (12 * 192 * 4) - period; // 8bb: this intentionally underflows uint16_t to be accurate to FT2

		const uint32_t quotient = invPeriod / 768;
		const uint32_t remainder = invPeriod % 768;

		const int32_t octShift = 14 - quotient;

		delta = (uint32_t)(((int64_t)logTab[remainder] * (int32_t)frequenceMulFactor) >> 24);
		delta >>= (octShift & 31); // 8bb: added needed 32-bit bitshift mask
	}
	else
	{
		delta = frequenceDivFactor / (uint32_t)period;
	}

	return delta;
}

static void startTone(uint8_t ton, uint8_t effTyp, uint8_t eff, stmTyp *ch)
{
	if (ton == NOTE_KEYOFF)
	{
		keyOff(ch);
		return;
	}

	// 8bb: if we came from Rxy (retrig), we didn't check note (Ton) yet
	if (ton == 0)
	{
		ton = ch->tonNr;
		if (ton == 0)
			return; // 8bb: if still no note, return
	}

	ch->tonNr = ton;

	instrTyp *ins = instr[ch->instrNr];
	if (ins == NULL)
		ins = instr[0];

	ch->instrSeg = ins;
	ch->mute = ins->mute;

	uint8_t smp = ins->ta[ton-1] & 0xF; // 8bb: added for safety
	ch->sampleNr = smp;

	sampleTyp *s = &ins->samp[smp];
	ch->relTonNr = s->relTon;

	ton += ch->relTonNr;
	if (ton >= 10*12)
		return;

	ch->oldVol = s->vol;
	ch->oldPan = s->pan;

	if (effTyp == 0x0E && (eff & 0xF0) == 0x50) // 8bb: EFx - Set Finetune
		ch->fineTune = ((eff & 0x0F) << 4) - 128;
	else
		ch->fineTune = s->fine;

	if (ton != 0)
	{
		const uint16_t tmpTon = ((ton-1) << 4) + (((int8_t)ch->fineTune >> 3) + 16); // 8bb: 0..1935
		if (tmpTon < MAX_NOTES) // 8bb: tmpTon is *always* below MAX_NOTES here, this check is not needed
			ch->outPeriod = ch->realPeriod = note2Period[tmpTon];
	}

	ch->status |= IS_Period + IS_Vol + IS_Pan + IS_NyTon + IS_QuickVol;

	if (effTyp == 9) // 8bb: 9xx - Set Sample Offset
	{
		if (eff)
			ch->smpOffset = ch->eff;

		ch->smpStartPos = ch->smpOffset << 8;
	}
	else
	{
		ch->smpStartPos = 0;
	}

	P_StartTone(s, ch->smpStartPos);
}

static void volume(stmTyp *ch, uint8_t param); // 8bb: volume slide
static void vibrato2(stmTyp *ch);
static void tonePorta(stmTyp *ch, uint8_t param);

static void dummy(stmTyp *ch, uint8_t param)
{
	return;

	(void)ch;
	(void)param;
}

static void finePortaUp(stmTyp *ch, uint8_t param)
{
	if (param == 0)
		param = ch->fPortaUpSpeed;

	ch->fPortaUpSpeed = param;

	ch->realPeriod -= param << 2;
	if ((int16_t)ch->realPeriod < 1)
		ch->realPeriod = 1;

	ch->outPeriod = ch->realPeriod;
	ch->status |= IS_Period;
}

static void finePortaDown(stmTyp *ch, uint8_t param)
{
	if (param == 0)
		param = ch->fPortaDownSpeed;

	ch->fPortaDownSpeed = param;

	ch->realPeriod += param << 2;
	if ((int16_t)ch->realPeriod > MAX_FRQ-1) // 8bb: FT2 bug, should've been unsigned comparison!
		ch->realPeriod = MAX_FRQ-1;

	ch->outPeriod = ch->realPeriod;
	ch->status |= IS_Period;
}

static void setGlissCtrl(stmTyp *ch, uint8_t param)
{
	ch->glissFunk = param;
}

static void setVibratoCtrl(stmTyp *ch, uint8_t param)
{
	ch->waveCtrl = (ch->waveCtrl & 0xF0) | param;
}

static void jumpLoop(stmTyp *ch, uint8_t param)
{
	if (param == 0)
	{
		ch->pattPos = song.pattPos & 0xFF;
	}
	else if (ch->loopCnt == 0)
	{
		ch->loopCnt = param;

		song.pBreakPos = ch->pattPos;
		song.pBreakFlag = true;
	}
	else if (--ch->loopCnt > 0)
	{
		song.pBreakPos = ch->pattPos;
		song.pBreakFlag = true;
	}
}

static void setTremoloCtrl(stmTyp *ch, uint8_t param)
{
	ch->waveCtrl = (param << 4) | (ch->waveCtrl & 0x0F);
}

static void volFineUp(stmTyp *ch, uint8_t param)
{
	if (param == 0)
		param = ch->fVolSlideUpSpeed;

	ch->fVolSlideUpSpeed = param;

	ch->realVol += param;
	if (ch->realVol > 64)
		ch->realVol = 64;

	ch->outVol = ch->realVol;
	ch->status |= IS_Vol;
}

static void volFineDown(stmTyp *ch, uint8_t param)
{
	if (param == 0)
		param = ch->fVolSlideDownSpeed;

	ch->fVolSlideDownSpeed = param;

	ch->realVol -= param;
	if ((int8_t)ch->realVol < 0)
		ch->realVol = 0;

	ch->outVol = ch->realVol;
	ch->status |= IS_Vol;
}

static void noteCut0(stmTyp *ch, uint8_t param)
{
	if (param == 0) // 8bb: only a parameter of zero is handled here
	{
		ch->realVol = 0;
		ch->outVol = 0;
		ch->status |= IS_Vol + IS_QuickVol;
	}
}

static void pattDelay(stmTyp *ch, uint8_t param)
{
	if (song.pattDelTime2 == 0)
		song.pattDelTime = param + 1;

	(void)ch;
}

static const efxRoutine EJumpTab_TickZero[16] =
{
	dummy, // 0
	finePortaUp, // 1
	finePortaDown, // 2
	setGlissCtrl, // 3
	setVibratoCtrl, // 4
	dummy, // 5
	jumpLoop, // 6
	setTremoloCtrl, // 7
	dummy, // 8
	dummy, // 9
	volFineUp, // A
	volFineDown, // B
	noteCut0, // C
	dummy, // D
	pattDelay, // E
	dummy // F
};

static void E_Effects_TickZero(stmTyp *ch, uint8_t param)
{
	EJumpTab_TickZero[param >> 4](ch, param & 0x0F);
}

static void posJump(stmTyp *ch, uint8_t param)
{
	song.songPos = (int16_t)param - 1;
	song.pBreakPos = 0;
	song.posJumpFlag = true;

	(void)ch;
}

static void pattBreak(stmTyp *ch, uint8_t param)
{
	song.posJumpFlag = true;

	param = ((param >> 4) * 10) + (param & 0x0F);
	if (param <= 63)
		song.pBreakPos = param;
	else
		song.pBreakPos = 0;

	(void)ch;
}

static void setSpeed(stmTyp *ch, uint8_t param)
{
	if (param >= 32)
	{
		song.speed = param;
		P_SetSpeed(song.speed);
	}
	else
	{
		song.timer = song.tempo = param;
	}

	(void)ch;
}

static void setGlobaVol(stmTyp *ch, uint8_t param)
{
	if (param > 64)
		param = 64;

	song.globVol = param;

	stmTyp *c = stm;
	for (int32_t i = 0; i < song.antChn; i++, c++) // 8bb: this updates the volume for all voices
		c->status |= IS_Vol;

	(void)ch;
}

static void setEnvelopePos(stmTyp *ch, uint8_t param)
{
	int8_t envPos;
	bool envUpdate;
	int16_t newEnvPos;

	instrTyp *ins = ch->instrSeg;

	// *** VOLUME ENVELOPE ***
	if (ins->envVTyp & ENV_ENABLED)
	{
		ch->envVCnt = param - 1;

		envPos = 0;
		envUpdate = true;
		newEnvPos = param;

		if (ins->envVPAnt > 1)
		{
			envPos++;
			for (int32_t i = 0; i < ins->envVPAnt-1; i++)
			{
				if (newEnvPos < ins->envVP[envPos][0])
				{
					envPos--;

					newEnvPos -= ins->envVP[envPos][0];
					if (newEnvPos == 0)
					{
						envUpdate = false;
						break;
					}

					if (ins->envVP[envPos+1][0] <= ins->envVP[envPos][0])
					{
						envUpdate = true;
						break;
					}

					ch->envVIPValue = ((ins->envVP[envPos+1][1] - ins->envVP[envPos][1]) & 0xFF) << 8;
					ch->envVIPValue /= (ins->envVP[envPos+1][0] - ins->envVP[envPos][0]);

					ch->envVAmp = (ch->envVIPValue * (newEnvPos - 1)) + ((ins->envVP[envPos][1] & 0xFF) << 8);

					envPos++;

					envUpdate = false;
					break;
				}

				envPos++;
			}

			if (envUpdate)
				envPos--;
		}

		if (envUpdate)
		{
			ch->envVIPValue = 0;
			ch->envVAmp = (ins->envVP[envPos][1] & 0xFF) << 8;
		}

		if (envPos >= ins->envVPAnt)
		{
			envPos = ins->envVPAnt - 1;
			if (envPos < 0)
				envPos = 0;
		}

		ch->envVPos = envPos;
	}

	// *** PANNING ENVELOPE ***
	if (ins->envVTyp & ENV_SUSTAIN) // 8bb: FT2 bug? (should probably have been "ins->envPTyp & ENV_ENABLED")
	{
		ch->envPCnt = param - 1;

		envPos = 0;
		envUpdate = true;
		newEnvPos = param;

		if (ins->envPPAnt > 1)
		{
			envPos++;
			for (int32_t i = 0; i < ins->envPPAnt-1; i++)
			{
				if (newEnvPos < ins->envPP[envPos][0])
				{
					envPos--;

					newEnvPos -= ins->envPP[envPos][0];
					if (newEnvPos == 0)
					{
						envUpdate = false;
						break;
					}

					if (ins->envPP[envPos + 1][0] <= ins->envPP[envPos][0])
					{
						envUpdate = true;
						break;
					}

					ch->envPIPValue = ((ins->envPP[envPos+1][1] - ins->envPP[envPos][1]) & 0xFF) << 8;
					ch->envPIPValue /= (ins->envPP[envPos+1][0] - ins->envPP[envPos][0]);

					ch->envPAmp = (ch->envPIPValue * (newEnvPos - 1)) + ((ins->envPP[envPos][1] & 0xFF) << 8);

					envPos++;

					envUpdate = false;
					break;
				}

				envPos++;
			}

			if (envUpdate)
				envPos--;
		}

		if (envUpdate)
		{
			ch->envPIPValue = 0;
			ch->envPAmp = (ins->envPP[envPos][1] & 0xFF) << 8;
		}

		if (envPos >= ins->envPPAnt)
		{
			envPos = ins->envPPAnt - 1;
			if (envPos < 0)
				envPos = 0;
		}

		ch->envPPos = envPos;
	}
}

/* -- tick-zero volume column effects --
** 2nd parameter is used for a volume column quirk with the Rxy command (multiretrig)
*/

static void v_SetVibSpeed(stmTyp *ch, uint8_t *volKol)
{
	*volKol = (ch->volKolVol & 0x0F) << 2;
	if (*volKol != 0)
		ch->vibSpeed = *volKol;
}

static void v_Volume(stmTyp *ch, uint8_t *volKol)
{
	*volKol -= 16;
	if (*volKol > 64) // 8bb: no idea why FT2 has this check, this can't happen...
		*volKol = 64;

	ch->outVol = ch->realVol = *volKol;
	ch->status |= IS_Vol + IS_QuickVol;
}

static void v_FineSlideDown(stmTyp *ch, uint8_t *volKol)
{
	*volKol = (uint8_t)(0 - (ch->volKolVol & 0x0F)) + ch->realVol;
	if ((int8_t)*volKol < 0)
		*volKol = 0;

	ch->outVol = ch->realVol = *volKol;
	ch->status |= IS_Vol;
}

static void v_FineSlideUp(stmTyp *ch, uint8_t *volKol)
{
	*volKol = (ch->volKolVol & 0x0F) + ch->realVol;
	if (*volKol > 64)
		*volKol = 64;

	ch->outVol = ch->realVol = *volKol;
	ch->status |= IS_Vol;
}

static void v_SetPan(stmTyp *ch, uint8_t *volKol)
{
	*volKol <<= 4;

	ch->outPan = *volKol;
	ch->status |= IS_Pan;
}

// -- non-tick-zero volume column effects --

static void v_SlideDown(stmTyp *ch)
{
	uint8_t newVol = (uint8_t)(0 - (ch->volKolVol & 0x0F)) + ch->realVol;
	if ((int8_t)newVol < 0)
		newVol = 0;

	ch->outVol = ch->realVol = newVol;
	ch->status |= IS_Vol;
}

static void v_SlideUp(stmTyp *ch)
{
	uint8_t newVol = (ch->volKolVol & 0x0F) + ch->realVol;
	if (newVol > 64)
		newVol = 64;

	ch->outVol = ch->realVol = newVol;
	ch->status |= IS_Vol;
}

static void v_Vibrato(stmTyp *ch)
{
	const uint8_t param = ch->volKolVol & 0xF;
	if (param > 0)
		ch->vibDepth = param;

	vibrato2(ch);
}

static void v_PanSlideLeft(stmTyp *ch)
{
	uint16_t tmp16 = (uint8_t)(0 - (ch->volKolVol & 0x0F)) + ch->outPan;
	if (tmp16 < 256) // 8bb: includes an FT2 bug: pan-slide-left of 0 = set pan to 0
		tmp16 = 0;

	ch->outPan = (uint8_t)tmp16;
	ch->status |= IS_Pan;
}

static void v_PanSlideRight(stmTyp *ch)
{
	uint16_t tmp16 = (ch->volKolVol & 0x0F) + ch->outPan;
	if (tmp16 > 255)
		tmp16 = 255;

	ch->outPan = (uint8_t)tmp16;
	ch->status |= IS_Pan;
}

static void v_TonePorta(stmTyp *ch)
{
	tonePorta(ch, 0); // 8bb: the last parameter is actually not used in tonePorta()
}

static void v_dummy(stmTyp *ch)
{
	(void)ch;
	return;
}

static void v_dummy2(stmTyp *ch, uint8_t *volKol)
{
	(void)ch;
	(void)volKol;
	return;
}

static const volKolEfxRoutine VJumpTab_TickNonZero[16] =
{
	v_dummy,        v_dummy,         v_dummy,  v_dummy,
	v_dummy,        v_dummy,     v_SlideDown, v_SlideUp,
	v_dummy,        v_dummy,         v_dummy, v_Vibrato,
	v_dummy, v_PanSlideLeft, v_PanSlideRight, v_TonePorta
};

static const volKolEfxRoutine2 VJumpTab_TickZero[16] =
{
	       v_dummy2,      v_Volume,      v_Volume, v_Volume,
	       v_Volume,      v_Volume,      v_dummy2, v_dummy2,
	v_FineSlideDown, v_FineSlideUp, v_SetVibSpeed, v_dummy2,
	       v_SetPan,      v_dummy2,      v_dummy2, v_dummy2
};

static void setPan(stmTyp *ch, uint8_t param)
{
	ch->outPan = param;
	ch->status |= IS_Pan;
}

static void setVol(stmTyp *ch, uint8_t param)
{
	if (param > 64)
		param = 64;

	ch->outVol = ch->realVol = param;
	ch->status |= IS_Vol + IS_QuickVol;
}

static void xFinePorta(stmTyp *ch, uint8_t param)
{
	const uint8_t type = param >> 4;
	param &= 0x0F;

	if (type == 0x1) // extra fine porta up
	{
		if (param == 0)
			param = ch->ePortaUpSpeed;

		ch->ePortaUpSpeed = param;

		uint16_t newPeriod = ch->realPeriod;

		newPeriod -= param;
		if ((int16_t)newPeriod < 1)
			newPeriod = 1;

		ch->outPeriod = ch->realPeriod = newPeriod;
		ch->status |= IS_Period;
	}
	else if (type == 0x2) // extra fine porta down
	{
		if (param == 0)
			param = ch->ePortaDownSpeed;

		ch->ePortaDownSpeed = param;

		uint16_t newPeriod = ch->realPeriod;

		newPeriod += param;
		if ((int16_t)newPeriod > MAX_FRQ-1) // 8bb: FT2 bug, should've been unsigned comparison!
			newPeriod = MAX_FRQ-1;

		ch->outPeriod = ch->realPeriod = newPeriod;
		ch->status |= IS_Period;
	}
}

static void doMultiRetrig(stmTyp *ch, uint8_t param) // 8bb: "param" is never used (needed for efx jumptable structure)
{
	uint8_t cnt = ch->retrigCnt + 1;
	if (cnt < ch->retrigSpeed)
	{
		ch->retrigCnt = cnt;
		return;
	}

	ch->retrigCnt = 0;

	int16_t vol = ch->realVol;
	switch (ch->retrigVol)
	{
		case 0x1: vol -= 1; break;
		case 0x2: vol -= 2; break;
		case 0x3: vol -= 4; break;
		case 0x4: vol -= 8; break;
		case 0x5: vol -= 16; break;
		case 0x6: vol = (vol >> 1) + (vol >> 3) + (vol >> 4); break;
		case 0x7: vol >>= 1; break;
		case 0x8: break; // 8bb: does not change the volume
		case 0x9: vol += 1; break;
		case 0xA: vol += 2; break;
		case 0xB: vol += 4; break;
		case 0xC: vol += 8; break;
		case 0xD: vol += 16; break;
		case 0xE: vol = (vol >> 1) + vol; break;
		case 0xF: vol += vol; break;
		default: break;
	}
	vol = CLAMP(vol, 0, 64);

	ch->realVol = (uint8_t)vol;
	ch->outVol = ch->realVol;

	if (ch->volKolVol >= 0x10 && ch->volKolVol <= 0x50) // 8bb: Set Volume (volume column)
	{
		ch->outVol = ch->volKolVol - 0x10;
		ch->realVol = ch->outVol;
	}
	else if (ch->volKolVol >= 0xC0 && ch->volKolVol <= 0xCF) // 8bb: Set Panning (volume column)
	{
		ch->outPan = (ch->volKolVol & 0x0F) << 4;
	}

	startTone(0, 0, 0, ch);

	(void)param;
}

static void multiRetrig(stmTyp *ch, uint8_t param, uint8_t volumeColumnData)
{
	uint8_t tmpParam;

	tmpParam = param & 0x0F;
	if (tmpParam == 0)
		tmpParam = ch->retrigSpeed;

	ch->retrigSpeed = tmpParam;

	tmpParam = param >> 4;
	if (tmpParam == 0)
		tmpParam = ch->retrigVol;

	ch->retrigVol = tmpParam;

	if (volumeColumnData == 0)
		doMultiRetrig(ch, 0); // 8bb: the second parameter is never used (needed for efx jumptable structure)
}

static const efxRoutine JumpTab_TickZero[36] =
{
	dummy, // 0
	dummy, // 1
	dummy, // 2
	dummy, // 3
	dummy, // 4
	dummy, // 5
	dummy, // 6
	dummy, // 7
	setPan, // 8
	dummy, // 9
	dummy, // A
	posJump, // B
	setVol, // C
	pattBreak, // D
	E_Effects_TickZero, // E
	setSpeed, // F
	setGlobaVol, // G
	dummy, // H
	dummy, // I
	dummy, // J
	dummy, // K
	setEnvelopePos, // L
	dummy, // M
	dummy, // N
	dummy, // O
	dummy, // P
	dummy, // Q
	dummy, // R
	dummy, // S
	dummy, // T
	dummy, // U
	dummy, // V
	dummy, // W
	xFinePorta, // X
	dummy, // Y
	dummy  // Z
};

static void checkEffects(stmTyp *ch) // tick0 effect handling
{
	// volume column effects
	uint8_t newVolKol = ch->volKolVol; // 8bb: manipulated by vol. column effects, then used for multiretrig check (FT2 quirk)
	VJumpTab_TickZero[ch->volKolVol >> 4](ch, &newVolKol);

	// normal effects
	const uint8_t param = ch->eff;

	if ((ch->effTyp == 0 && param == 0) || ch->effTyp > 35)
		return;

	// 8bb: this one has to be done here instead of in the jumptable, as it needs the "newVolKol" parameter (FT2 quirk)
	if (ch->effTyp == 27) // 8bb: Rxy - Multi Retrig
	{
		multiRetrig(ch, param, newVolKol);
		return;
	}

	JumpTab_TickZero[ch->effTyp](ch, ch->eff);
}

static void fixTonePorta(stmTyp *ch, const tonTyp *p, uint8_t inst)
{
	if (p->ton > 0)
	{
		if (p->ton == NOTE_KEYOFF)
		{
			keyOff(ch);
		}
		else
		{
			const uint16_t portaTmp = (((p->ton-1) + ch->relTonNr) << 4) + (((int8_t)ch->fineTune >> 3) + 16);
			if (portaTmp < MAX_NOTES)
			{
				ch->wantPeriod = note2Period[portaTmp];

				if (ch->wantPeriod == ch->realPeriod)
					ch->portaDir = 0;
				else if (ch->wantPeriod > ch->realPeriod)
					ch->portaDir = 1;
				else
					ch->portaDir = 2;
			}
		}
	}

	if (inst > 0)
	{
		retrigVolume(ch);

		if (p->ton != NOTE_KEYOFF)
			retrigEnvelopeVibrato(ch);
	}
}

static void getNewNote(stmTyp *ch, const tonTyp *p)
{
	ch->volKolVol = p->vol;

	if (ch->effTyp == 0)
	{
		if (ch->eff != 0) // 8bb: we have an arpeggio (0xy) running, set period back
		{
			ch->outPeriod = ch->realPeriod;
			ch->status |= IS_Period;
		}
	}
	else
	{
		// 8bb: if we have a vibrato (4xy/6xy) on previous row (ch) that ends at current row (p), set period back
		if ((ch->effTyp == 4 || ch->effTyp == 6) && (p->effTyp != 4 && p->effTyp != 6))
		{
			ch->outPeriod = ch->realPeriod;
			ch->status |= IS_Period;
		}
	}

	ch->effTyp = p->effTyp;
	ch->eff = p->eff;
	ch->tonTyp = (p->instr << 8) | p->ton;

	// 8bb: 'inst' var is used for later if-checks
	uint8_t inst = p->instr;
	if (inst > 0)
	{
		if (inst <= 128)
			ch->instrNr = inst;
		else
			inst = 0;
	}

	bool checkEfx = true;
	if (p->effTyp == 0x0E) // 8bb: check for EDx (Note Delay) and E90 (Retrigger Note)
	{
		if (p->eff >= 0xD1 && p->eff <= 0xDF) // 8bb: ED1..EDF (Note Delay)
			return;
		else if (p->eff == 0x90) // 8bb: E90 (Retrigger Note)
			checkEfx = false;
	}

	if (checkEfx)
	{
		if ((ch->volKolVol & 0xF0) == 0xF0) // 8bb: Portamento (volume column)
		{
			const uint8_t volKolParam = ch->volKolVol & 0x0F;
			if (volKolParam > 0)
				ch->portaSpeed = volKolParam << 6;

			fixTonePorta(ch, p, inst);
			checkEffects(ch);
			return;
		}

		if (p->effTyp == 3 || p->effTyp == 5) // 8bb: Portamento (3xx/5xx)
		{
			if (p->effTyp != 5 && p->eff != 0)
				ch->portaSpeed = p->eff << 2;

			fixTonePorta(ch, p, inst);
			checkEffects(ch);
			return;
		}

		if (p->effTyp == 0x14 && p->eff == 0) // 8bb: K00 (Key Off - only handle tick 0 here)
		{
			keyOff(ch);

			if (inst)
				retrigVolume(ch);

			checkEffects(ch);
			return;
		}

		if (p->ton == 0)
		{
			if (inst > 0)
			{
				retrigVolume(ch);
				retrigEnvelopeVibrato(ch);
			}

			checkEffects(ch);
			return;
		}
	}

	if (p->ton == NOTE_KEYOFF)
		keyOff(ch);
	else
		startTone(p->ton, p->effTyp, p->eff, ch);

	if (inst > 0)
	{
		retrigVolume(ch);
		if (p->ton != NOTE_KEYOFF)
			retrigEnvelopeVibrato(ch);
	}

	checkEffects(ch);
}

static void fixaEnvelopeVibrato(stmTyp *ch)
{
	bool envInterpolateFlag, envDidInterpolate;
	uint8_t envPos;
	int16_t autoVibVal;
	uint16_t autoVibAmp, envVal;
	uint32_t vol;

	instrTyp *ins = ch->instrSeg;

	// *** FADEOUT ***
	if (!ch->envSustainActive)
	{
		ch->status |= IS_Vol;

		if (ch->fadeOutAmp >= ch->fadeOutSpeed)
		{
			ch->fadeOutAmp -= ch->fadeOutSpeed;
		}
		else
		{
			ch->fadeOutAmp = 0;
			ch->fadeOutSpeed = 0;
		}
	}

	if (!ch->mute)
	{
		// *** VOLUME ENVELOPE ***
		envVal = 0;
		if (ins->envVTyp & ENV_ENABLED)
		{
			envDidInterpolate = false;
			envPos = ch->envVPos;

			if (++ch->envVCnt == ins->envVP[envPos][0])
			{
				ch->envVAmp = ins->envVP[envPos][1] << 8;

				envPos++;
				if (ins->envVTyp & ENV_LOOP)
				{
					envPos--;

					if (envPos == ins->envVRepE)
					{
						if (!(ins->envVTyp & ENV_SUSTAIN) || envPos != ins->envVSust || ch->envSustainActive)
						{
							envPos = ins->envVRepS;

							ch->envVCnt = ins->envVP[envPos][0];
							ch->envVAmp = ins->envVP[envPos][1] << 8;
						}
					}

					envPos++;
				}

				if (envPos < ins->envVPAnt)
				{
					envInterpolateFlag = true;
					if ((ins->envVTyp & ENV_SUSTAIN) && ch->envSustainActive)
					{
						if (envPos-1 == ins->envVSust)
						{
							envPos--;
							ch->envVIPValue = 0;
							envInterpolateFlag = false;
						}
					}

					if (envInterpolateFlag)
					{
						ch->envVPos = envPos;

						ch->envVIPValue = 0;
						if (ins->envVP[envPos][0] > ins->envVP[envPos-1][0])
						{
							ch->envVIPValue = (ins->envVP[envPos][1] - ins->envVP[envPos-1][1]) << 8;
							ch->envVIPValue /= (ins->envVP[envPos][0] - ins->envVP[envPos-1][0]);

							envVal = ch->envVAmp;
							envDidInterpolate = true;
						}
					}
				}
				else
				{
					ch->envVIPValue = 0;
				}
			}

			if (!envDidInterpolate)
			{
				ch->envVAmp += ch->envVIPValue;

				envVal = ch->envVAmp;
				if (envVal > 64*256)
				{
					if (envVal > 128*256)
						envVal = 64*256;
					else
						envVal = 0;

					ch->envVIPValue = 0;
				}
			}

			envVal >>= 8;

			vol = (envVal * ch->outVol * ch->fadeOutAmp) >> (16+2);
			vol = (vol * song.globVol) >> 7;

			ch->status |= IS_Vol; // 8bb: this updates vol on every tick (because vol envelope is enabled)
		}
		else
		{
			vol = ((ch->outVol << 4) * ch->fadeOutAmp) >> 16;
			vol = (vol * song.globVol) >> 7;
		}

		ch->finalVol = (uint16_t)vol; // 0..256
	}
	else
	{
		ch->finalVol = 0;
	}

	// *** PANNING ENVELOPE ***

	envVal = 0;
	if (ins->envPTyp & ENV_ENABLED)
	{
		envDidInterpolate = false;
		envPos = ch->envPPos;

		if (++ch->envPCnt == ins->envPP[envPos][0])
		{
			ch->envPAmp = ins->envPP[envPos][1] << 8;

			envPos++;
			if (ins->envPTyp & ENV_LOOP)
			{
				envPos--;

				if (envPos == ins->envPRepE)
				{
					if (!(ins->envPTyp & ENV_SUSTAIN) || envPos != ins->envPSust || ch->envSustainActive)
					{
						envPos = ins->envPRepS;

						ch->envPCnt = ins->envPP[envPos][0];
						ch->envPAmp = ins->envPP[envPos][1] << 8;
					}
				}

				envPos++;
			}

			if (envPos < ins->envPPAnt)
			{
				envInterpolateFlag = true;
				if ((ins->envPTyp & ENV_SUSTAIN) && ch->envSustainActive)
				{
					if (envPos-1 == ins->envPSust)
					{
						envPos--;
						ch->envPIPValue = 0;
						envInterpolateFlag = false;
					}
				}

				if (envInterpolateFlag)
				{
					ch->envPPos = envPos;

					ch->envPIPValue = 0;
					if (ins->envPP[envPos][0] > ins->envPP[envPos-1][0])
					{
						ch->envPIPValue = (ins->envPP[envPos][1] - ins->envPP[envPos-1][1]) << 8;
						ch->envPIPValue /= (ins->envPP[envPos][0] - ins->envPP[envPos-1][0]);

						envVal = ch->envPAmp;
						envDidInterpolate = true;
					}
				}
			}
			else
			{
				ch->envPIPValue = 0;
			}
		}

		if (!envDidInterpolate)
		{
			ch->envPAmp += ch->envPIPValue;

			envVal = ch->envPAmp;
			if (envVal > 64*256)
			{
				if (envVal > 128*256)
					envVal = 64*256;
				else
					envVal = 0;

				ch->envPIPValue = 0;
			}
		}

		int16_t panTmp = ch->outPan - 128;
		if (panTmp > 0)
			panTmp = 0 - panTmp;
		panTmp += 128;

		panTmp <<= 3;
		envVal -= 32*256;

		ch->finalPan = ch->outPan + (uint8_t)(((int16_t)envVal * panTmp) >> 16);
		ch->status |= IS_Pan;
	}
	else
	{
		ch->finalPan = ch->outPan;
	}

	// *** AUTO VIBRATO ***
	if (ins->vibDepth > 0)
	{
		if (ch->eVibSweep > 0)
		{
			autoVibAmp = ch->eVibSweep;
			if (ch->envSustainActive)
			{
				autoVibAmp += ch->eVibAmp;
				if ((autoVibAmp >> 8) > ins->vibDepth)
				{
					autoVibAmp = ins->vibDepth << 8;
					ch->eVibSweep = 0;
				}

				ch->eVibAmp = autoVibAmp;
			}
		}
		else
		{
			autoVibAmp = ch->eVibAmp;
		}

		ch->eVibPos += ins->vibRate;

		     if (ins->vibTyp == 1) autoVibVal = (ch->eVibPos > 127) ? 64 : -64; // square
		else if (ins->vibTyp == 2) autoVibVal = (((ch->eVibPos >> 1) + 64) & 127) - 64; // ramp up
		else if (ins->vibTyp == 3) autoVibVal = ((-(ch->eVibPos >> 1) + 64) & 127) - 64; // ramp down
		else autoVibVal = vibSineTab[ch->eVibPos]; // sine

		autoVibVal <<= 2;
		uint16_t tmpPeriod = (autoVibVal * (int16_t)autoVibAmp) >> 16;

		tmpPeriod += ch->outPeriod;
		if (tmpPeriod >= MAX_FRQ)
			tmpPeriod = 0; // 8bb: yes, FT2 does this (!)

		ch->finalPeriod = tmpPeriod;
		ch->status |= IS_Period;
	}
	else
	{
		ch->finalPeriod = ch->outPeriod;
	}
}

// 8bb: converts period to note number, for arpeggio and portamento (in semitone-slide mode)
static uint16_t relocateTon(uint16_t period, uint8_t arpNote, stmTyp *ch)
{
	int32_t tmpPeriod;

	const int32_t fineTune = ((int8_t)ch->fineTune >> 3) + 16;
	
	/* 8bb: FT2 bug, should've been 10*12*16. Notes above B-7 (95) will have issues.
	** You can only achieve such high notes by having a high relative note value
	** in the sample.
	*/
	int32_t hiPeriod = 8*12*16;
	
	int32_t loPeriod = 0;

	for (int32_t i = 0; i < 8; i++)
	{
		tmpPeriod = (((loPeriod + hiPeriod) >> 1) & ~15) + fineTune;

		int32_t lookUp = tmpPeriod - 8;
		if (lookUp < 0)
			lookUp = 0; // 8bb: safety fix (C-0 w/ ftune <= -65). This buggy read seems to return 0 in FT2 (TODO: verify)

		if (period >= note2Period[lookUp])
			hiPeriod = (tmpPeriod - fineTune) & ~15;
		else
			loPeriod = (tmpPeriod - fineTune) & ~15;
	}

	tmpPeriod = loPeriod + fineTune + (arpNote << 4);
	if (tmpPeriod >= (8*12*16+15)-1) // 8bb: FT2 bug, should've been 10*12*16+16 (also notice the +2 difference)
		tmpPeriod = (8*12*16+16)-1;

	return note2Period[tmpPeriod];
}

static void vibrato2(stmTyp *ch)
{
	uint8_t tmpVib = (ch->vibPos >> 2) & 0x1F;

	switch (ch->waveCtrl & 3)
	{
		// 0: sine
		case 0: tmpVib = vibTab[tmpVib]; break;

		// 1: ramp
		case 1:
		{
			tmpVib <<= 3;
			if ((int8_t)ch->vibPos < 0)
				tmpVib = ~tmpVib;
		}
		break;

		// 2/3: square
		default: tmpVib = 255; break;
	}

	tmpVib = (tmpVib * ch->vibDepth) >> 5;

	if ((int8_t)ch->vibPos < 0)
		ch->outPeriod = ch->realPeriod - tmpVib;
	else
		ch->outPeriod = ch->realPeriod + tmpVib;

	ch->status |= IS_Period;
	ch->vibPos += ch->vibSpeed;
}

static void arp(stmTyp *ch, uint8_t param)
{
	/* 8bb: The original arpTab table only supports 16 ticks, so it can and will overflow.
	** I have added overflown values to the table so that we can handle up to 256 ticks.
	** The added overflow entries are accurate to the overflow-read in FT2.08/FT2.09.
	*/
	const uint8_t tick = arpTab[song.timer & 0xFF];
	
	if (tick == 0)
	{
		ch->outPeriod = ch->realPeriod;
	}
	else
	{
		const uint8_t note = (tick == 1) ? (param >> 4) : (param & 0x0F);
		ch->outPeriod = relocateTon(ch->realPeriod, note, ch);
	}

	ch->status |= IS_Period;
}

static void portaUp(stmTyp *ch, uint8_t param)
{
	if (param == 0)
		param = ch->portaUpSpeed;

	ch->portaUpSpeed = param;

	ch->realPeriod -= param << 2;
	if ((int16_t)ch->realPeriod < 1)
		ch->realPeriod = 1;

	ch->outPeriod = ch->realPeriod;
	ch->status |= IS_Period;
}

static void portaDown(stmTyp *ch, uint8_t param)
{
	if (param == 0)
		param = ch->portaDownSpeed;

	ch->portaDownSpeed = param;

	ch->realPeriod += param << 2;
	if ((int16_t)ch->realPeriod > MAX_FRQ-1) // 8bb: FT2 bug, should've been unsigned comparison!
		ch->realPeriod = MAX_FRQ-1;

	ch->outPeriod = ch->realPeriod;
	ch->status |= IS_Period;
}

static void tonePorta(stmTyp *ch, uint8_t param) // 8bb: param is a placeholder (not used)
{
	if (ch->portaDir == 0)
		return;

	if (ch->portaDir > 1)
	{
		ch->realPeriod -= ch->portaSpeed;
		if ((int16_t)ch->realPeriod <= (int16_t)ch->wantPeriod)
		{
			ch->portaDir = 1;
			ch->realPeriod = ch->wantPeriod;
		}
	}
	else
	{
		ch->realPeriod += ch->portaSpeed;
		if (ch->realPeriod >= ch->wantPeriod)
		{
			ch->portaDir = 1;
			ch->realPeriod = ch->wantPeriod;
		}
	}

	if (ch->glissFunk) // 8bb: semitone-slide flag
		ch->outPeriod = relocateTon(ch->realPeriod, 0, ch);
	else
		ch->outPeriod = ch->realPeriod;

	ch->status |= IS_Period;

	(void)param;
}

static void vibrato(stmTyp *ch, uint8_t param)
{
	uint8_t tmp8;

	if (ch->eff > 0)
	{
		tmp8 = param & 0x0F;
		if (tmp8 > 0)
			ch->vibDepth = tmp8;

		tmp8 = (param & 0xF0) >> 2;
		if (tmp8 > 0)
			ch->vibSpeed = tmp8;
	}

	vibrato2(ch);
}

static void tonePlusVol(stmTyp *ch, uint8_t param)
{
	tonePorta(ch, 0); // 8bb: the last parameter is not used in tonePorta()
	volume(ch, param);

	(void)param;
}

static void vibratoPlusVol(stmTyp *ch, uint8_t param)
{
	vibrato2(ch);
	volume(ch, param);

	(void)param;
}

static void tremolo(stmTyp *ch, uint8_t param)
{
	uint8_t tmp8;
	int16_t tremVol;

	const uint8_t tmpEff = param;
	if (tmpEff > 0)
	{
		tmp8 = tmpEff & 0x0F;
		if (tmp8 > 0)
			ch->tremDepth = tmp8;

		tmp8 = (tmpEff & 0xF0) >> 2;
		if (tmp8 > 0)
			ch->tremSpeed = tmp8;
	}

	uint8_t tmpTrem = (ch->tremPos >> 2) & 0x1F;
	switch ((ch->waveCtrl >> 4) & 3)
	{
		// 0: sine
		case 0: tmpTrem = vibTab[tmpTrem]; break;

		// 1: ramp
		case 1:
		{
			tmpTrem <<= 3;
			if ((int8_t)ch->vibPos < 0) // 8bb: FT2 bug, should've been ch->tremPos
				tmpTrem = ~tmpTrem;
		}
		break;

		// 2/3: square
		default: tmpTrem = 255; break;
	}
	tmpTrem = (tmpTrem * ch->tremDepth) >> 6;

	if ((int8_t)ch->tremPos < 0)
	{
		tremVol = ch->realVol - tmpTrem;
		if (tremVol < 0)
			tremVol = 0;
	}
	else
	{
		tremVol = ch->realVol + tmpTrem;
		if (tremVol > 64)
			tremVol = 64;
	}

	ch->outVol = (uint8_t)tremVol;
	ch->status |= IS_Vol;
	ch->tremPos += ch->tremSpeed;
}

static void volume(stmTyp *ch, uint8_t param) // 8bb: volume slide
{
	if (param == 0)
		param = ch->volSlideSpeed;

	ch->volSlideSpeed = param;

	uint8_t newVol = ch->realVol;
	if ((param & 0xF0) == 0)
	{
		newVol -= param;
		if ((int8_t)newVol < 0)
			newVol = 0;
	}
	else
	{
		param >>= 4;

		newVol += param;
		if (newVol > 64)
			newVol = 64;
	}

	ch->outVol = ch->realVol = newVol;
	ch->status |= IS_Vol;
}

static void globalVolSlide(stmTyp *ch, uint8_t param)
{
	if (param == 0)
		param = ch->globVolSlideSpeed;

	ch->globVolSlideSpeed = param;

	uint8_t newVol = (uint8_t)song.globVol;
	if ((param & 0xF0) == 0)
	{
		newVol -= param;
		if ((int8_t)newVol < 0)
			newVol = 0;
	}
	else
	{
		param >>= 4;

		newVol += param;
		if (newVol > 64)
			newVol = 64;
	}

	song.globVol = newVol;

	stmTyp *c = stm;
	for (int32_t i = 0; i < song.antChn; i++, c++) // 8bb: this updates the volume for all voices
		c->status |= IS_Vol;
}

static void keyOffCmd(stmTyp *ch, uint8_t param)
{
	if ((uint8_t)(song.tempo-song.timer) == (param & 31))
		keyOff(ch);
}

static void panningSlide(stmTyp *ch, uint8_t param)
{
	if (param == 0)
		param = ch->panningSlideSpeed;

	ch->panningSlideSpeed = param;

	int16_t newPan = (int16_t)ch->outPan;
	if ((param & 0xF0) == 0)
	{
		newPan -= param;
		if (newPan < 0)
			newPan = 0;
	}
	else
	{
		param >>= 4;

		newPan += param;
		if (newPan > 255)
			newPan = 255;
	}

	ch->outPan = (uint8_t)newPan;
	ch->status |= IS_Pan;
}

static void tremor(stmTyp *ch, uint8_t param)
{
	if (param == 0)
		param = ch->tremorSave;

	ch->tremorSave = param;

	uint8_t tremorSign = ch->tremorPos & 0x80;
	uint8_t tremorData = ch->tremorPos & 0x7F;

	tremorData--;
	if ((int8_t)tremorData < 0)
	{
		if (tremorSign == 0x80)
		{
			tremorSign = 0x00;
			tremorData = param & 0x0F;
		}
		else
		{
			tremorSign = 0x80;
			tremorData = param >> 4;
		}
	}

	ch->tremorPos = tremorSign | tremorData;
	ch->outVol = (tremorSign == 0x80) ? ch->realVol : 0;
	ch->status |= IS_Vol + IS_QuickVol;
}

static void retrigNote(stmTyp *ch, uint8_t param)
{
	if (param == 0) // 8bb: E9x with a param of zero is handled in getNewNote()
		return;

	if ((song.tempo-song.timer) % param == 0)
	{
		startTone(0, 0, 0, ch);
		retrigEnvelopeVibrato(ch);
	}
}

static void noteCut(stmTyp *ch, uint8_t param)
{
	if ((uint8_t)(song.tempo-song.timer) == param)
	{
		ch->outVol = ch->realVol = 0;
		ch->status |= IS_Vol + IS_QuickVol;
	}
}

static void noteDelay(stmTyp *ch, uint8_t param)
{
	if ((uint8_t)(song.tempo-song.timer) == param)
	{
		startTone(ch->tonTyp & 0xFF, 0, 0, ch);

		if ((ch->tonTyp & 0xFF00) > 0) // 8bb: do we have an instrument number?
			retrigVolume(ch);

		retrigEnvelopeVibrato(ch);

		if (ch->volKolVol >= 0x10 && ch->volKolVol <= 0x50) // 8bb: Set Volume (volume column)
		{
			ch->outVol = ch->volKolVol - 16;
			ch->realVol = ch->outVol;
		}
		else if (ch->volKolVol >= 0xC0 && ch->volKolVol <= 0xCF) // 8bb: Set Panning (volume column)
		{
			ch->outPan = (ch->volKolVol & 0x0F) << 4;
		}
	}
}

static const efxRoutine EJumpTab_TickNonZero[16] =
{
	dummy, // 0
	dummy, // 1
	dummy, // 2
	dummy, // 3
	dummy, // 4
	dummy, // 5
	dummy, // 6
	dummy, // 7
	dummy, // 8
	retrigNote, // 9
	dummy, // A
	dummy, // B
	noteCut, // C
	noteDelay, // D
	dummy, // E
	dummy // F
};

static void E_Effects_TickNonZero(stmTyp *ch, uint8_t param)
{
	EJumpTab_TickNonZero[param >> 4](ch, param & 0xF);
}

static const efxRoutine JumpTab_TickNonZero[36] =
{
	arp, // 0
	portaUp, // 1
	portaDown, // 2
	tonePorta, // 3
	vibrato, // 4
	tonePlusVol, // 5
	vibratoPlusVol, // 6
	tremolo, // 7
	dummy, // 8
	dummy, // 9
	volume, // A
	dummy, // B
	dummy, // C
	dummy, // D
	E_Effects_TickNonZero, // E
	dummy, // F
	dummy, // G
	globalVolSlide, // H
	dummy, // I
	dummy, // J
	keyOffCmd, // K
	dummy, // L
	dummy, // M
	dummy, // N
	dummy, // O
	panningSlide, // P
	dummy, // Q
	doMultiRetrig, // R
	dummy, // S
	tremor, // T
	dummy, // U
	dummy, // V
	dummy, // W
	dummy, // X
	dummy, // Y
	dummy  // Z
};

static void doEffects(stmTyp *ch) // tick>0 effect handling
{
	const uint8_t volKolEfx = ch->volKolVol >> 4;
	if (volKolEfx > 0)
		VJumpTab_TickNonZero[volKolEfx](ch);

	if ((ch->eff == 0 && ch->effTyp == 0) || ch->effTyp > 35)
		return;

	JumpTab_TickNonZero[ch->effTyp](ch, ch->eff);
}

static void getNextPos(void)
{
	song.pattPos++;

	if (song.pattDelTime > 0)
	{
		song.pattDelTime2 = song.pattDelTime;
		song.pattDelTime = 0;
	}

	if (song.pattDelTime2 > 0)
	{
		song.pattDelTime2--;
		if (song.pattDelTime2 > 0)
			song.pattPos--;
	}

	if (song.pBreakFlag)
	{
		song.pBreakFlag = false;
		song.pattPos = song.pBreakPos;
	}

	if (song.pattPos >= song.pattLen || song.posJumpFlag)
	{
		song.pattPos = song.pBreakPos;
		song.pBreakPos = 0;
		song.posJumpFlag = false;

		song.songPos++;
		if (song.songPos >= song.len)
			song.songPos = song.repS;

		song.pattNr = song.songTab[(uint8_t)song.songPos];
		song.pattLen = pattLens[(uint8_t)song.pattNr];
	}
}

void mainPlayer(void)
{
	if (musicPaused)
		return;

	bool tickZero = false;

	song.timer--;
	if (song.timer == 0)
	{
		song.timer = song.tempo;
		tickZero = true;
	}

	const bool readNewNote = tickZero && (song.pattDelTime2 == 0);
	if (readNewNote)
	{
		const tonTyp *pattPtr = nilPatternLine;
		if (patt[song.pattNr] != NULL)
			pattPtr = &patt[song.pattNr][song.pattPos * song.antChn];

		stmTyp *c = stm;
		for (uint8_t i = 0; i < song.antChn; i++, c++, pattPtr++)
		{
			PMPTmpActiveChannel = i; // 8bb: for P_StartTone()
			getNewNote(c, pattPtr);
			fixaEnvelopeVibrato(c);
		}
	}
	else
	{
		stmTyp *c = stm;
		for (uint8_t i = 0; i < song.antChn; i++, c++)
		{
			PMPTmpActiveChannel = i; // 8bb: for P_StartTone()
			doEffects(c);
			fixaEnvelopeVibrato(c);
		}
	}

	if (song.timer == 1)
		getNextPos();
}
