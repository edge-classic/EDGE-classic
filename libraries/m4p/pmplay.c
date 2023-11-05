/*
** - loaders and replayer handlers -
*/

#define DEFAULT_AMP 4
#define DEFAULT_MASTER_VOL 256

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include "pmplay.h"
#include "pmp_mix.h"
#include "snd_masm.h"
#include "ft_tables.h"

#define INSTR_HEADER_SIZE 263

#define SWAP16(value) \
( \
	(((uint16_t)((value) & 0x00FF)) << 8) | \
	(((uint16_t)((value) & 0xFF00)) >> 8)   \
)

#ifdef _MSC_VER
#pragma pack(push)
#pragma pack(1)
#endif
typedef struct songHeaderTyp_t
{
	char sig[17], name[21], progName[20];
	uint16_t ver;
	int32_t headerSize;
	uint16_t len, repS, antChn, antPtn, antInstrs, flags, defTempo, defSpeed;
	uint8_t songTab[256];
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
songHeaderTyp;

typedef struct modSampleTyp
{
	char name[22];
	uint16_t len;
	uint8_t fine, vol;
	uint16_t repS, repL;
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
modSampleTyp;

typedef struct songMOD31HeaderTyp
{
	char name[20];
	modSampleTyp sample[31];
	uint8_t len, repS, songTab[128];
	char Sig[4];
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
songMOD31HeaderTyp;

typedef struct songMOD15HeaderTyp
{
	char name[20];
	modSampleTyp sample[15];
	uint8_t len, repS, songTab[128];
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
songMOD15HeaderTyp;

typedef struct sampleHeaderTyp_t
{
	int32_t len, repS, repL;
	uint8_t vol;
	int8_t fine;
	uint8_t typ, pan;
	int8_t relTon;
	uint8_t skrap;
	char name[22];
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
sampleHeaderTyp;

typedef struct instrHeaderTyp_t
{
	int32_t instrSize;
	char name[22];
	uint8_t typ;
	uint16_t antSamp;
	int32_t sampleSize;
	uint8_t ta[96];
	int16_t envVP[12][2], envPP[12][2];
	uint8_t envVPAnt, envPPAnt, envVSust, envVRepS, envVRepE, envPSust, envPRepS;
	uint8_t envPRepE, envVTyp, envPTyp, vibTyp, vibSweep, vibDepth, vibRate;
	uint16_t fadeOut;
	uint8_t midiOn, midiChannel;
	int16_t midiProgram, midiBend;
	int8_t mute;
	uint8_t reserved[15];
	sampleHeaderTyp samp[32];
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
instrHeaderTyp;

typedef struct patternHeaderTyp_t
{
	int32_t patternHeaderSize;
	uint8_t typ;
	uint16_t pattLen, dataLen;
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
patternHeaderTyp;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

static int32_t soundBufferSize;

// globalized
volatile bool interpolationFlag, volumeRampingFlag, moduleLoaded, musicPaused;
bool linearFrqTab;
volatile const uint16_t *note2Period;
uint16_t pattLens[256];
int16_t PMPTmpActiveChannel, boostLevel = DEFAULT_AMP;
int32_t masterVol = DEFAULT_MASTER_VOL, PMPLeft = 0;
int32_t realReplayRate, quickVolSizeVal, speedVal;
uint32_t frequenceDivFactor, frequenceMulFactor, CDA_Amp = 8*DEFAULT_AMP;
tonTyp *patt[256];
instrTyp *instr[1+128];
songTyp song;
stmTyp stm[32];
// ------------------

// 8bb: added these for loader
typedef struct
{
	uint8_t *_ptr, *_base;
	bool _eof;
	size_t _cnt, _bufsiz;
} MEMFILE;

static MEMFILE *mopen(const uint8_t *src, uint32_t length);
static void mclose(MEMFILE **buf);
static size_t mread(void *buffer, size_t size, size_t count, MEMFILE *buf);
static bool meof(MEMFILE *buf);
static void mseek(MEMFILE *buf, int32_t offset, int32_t whence);
static void mrewind(MEMFILE *buf);
// --------------------------

static void resetMusic(void);
static void freeAllPatterns(void);
static void setFrqTab(bool linear);

static CIType *getVoice(int32_t ch) // 8bb: added this
{
	if (ch < 0 || ch > 31)
		return NULL;

	return &CI[chnReloc[ch]];
}

/***************************************************************************
 *        ROUTINES FOR SAMPLE HANDLING ETC.                                *
 ***************************************************************************/

// 8bb: modifies wrapped sample after loop/end (for branchless mixer interpolation)
static void fixSample(sampleTyp *s)
{
	if (s->pek == NULL)
		return; // empty sample

	const bool sample16Bit = !!(s->typ & SAMPLE_16BIT);
	uint8_t loopType = s->typ & 3;
	int16_t *ptr16 = (int16_t *)s->pek;
	int32_t len = s->len;
	int32_t loopStart = s->repS;
	int32_t loopEnd = s->repS + s->repL;

	if (sample16Bit)
	{
		len >>= 1;
		loopStart >>= 1;
		loopEnd >>= 1;
	}

	if (len < 1)
		return;

	/* 8bb:
	** This is the exact bit test order of which FT2 handles
	** the sample tap fix.
	**
	** This order is important for rare cases where both the
	** "forward" and "pingpong" loop bits are set at once.
	**
	** This means that if both flags are set, the mixer will
	** play the sample with pingpong looping, but the sample fix
	** is handled as if it was a forward loop. This results in
	** the wrong interpolation tap sample being written after the
	** loop end point.
	*/

	if (loopType & LOOP_FORWARD)
	{
		if (sample16Bit)
			ptr16[loopEnd] = ptr16[loopStart];
		else
			s->pek[loopEnd] = s->pek[loopStart];

		return;
	}
	else if (loopType & LOOP_PINGPONG)
	{
		if (sample16Bit)
			ptr16[loopEnd] = ptr16[loopEnd-1];
		else
			s->pek[loopEnd] = s->pek[loopEnd-1];
	}
	else // no loop
	{
		if (sample16Bit)
			ptr16[len] = 0;
		else
			s->pek[len] = 0;
	}
}

static void checkSampleRepeat(int32_t nr, int32_t nr2)
{
	instrTyp *i = instr[nr];
	if (i == NULL)
		return;

	sampleTyp *s = &i->samp[nr2];

	if (s->repS < 0) s->repS = 0;
	if (s->repL < 0) s->repL = 0;
	if (s->repS > s->len) s->repS = s->len;
	if (s->repS+s->repL > s->len) s->repL = s->len - s->repS;
}

static void upDateInstrs(void)
{
	for (int32_t i = 0; i <= 128; i++)
	{
		instrTyp *ins = instr[i];
		if (ins == NULL)
			continue;

		sampleTyp *s = ins->samp;
		for (int32_t j = 0; j < 16; j++, s++)
		{
			checkSampleRepeat(i, j);
			fixSample(s);

			if (s->pek == NULL)
			{
				s->len = 0;
				s->repS = 0;
				s->repL = 0;
			}
		}
	}
}

static bool patternEmpty(uint16_t nr)
{
	if (patt[nr] == NULL)
		return true;

	const uint8_t *scanPtr = (const uint8_t *)patt[nr];
	const int32_t scanLen = pattLens[nr] * song.antChn * sizeof (tonTyp);

	for (int32_t i = 0; i < scanLen; i++)
	{
		if (scanPtr[i] != 0)
			return false;
	}

	return true;
}

static bool allocateInstr(uint16_t i)
{
	if (instr[i] != NULL)
		return true;

	instrTyp *p = (instrTyp *)calloc(1, sizeof (instrTyp));
	if (p == NULL)
		return false;

	sampleTyp *s = p->samp;
	for (int32_t j = 0; j < 16; j++, s++)
	{
		s->pan = 128;
		s->vol = 64;
	}

	instr[i] = p;
	return true;
}

static void freeInstr(uint16_t nr)
{
	if (nr > 128)
		return;

	instrTyp *ins = instr[nr];
	if (ins == NULL)
		return;

	sampleTyp *s = ins->samp;
	for (uint8_t i = 0; i < 16; i++, s++)
	{
		if (s->pek != NULL)
			free(s->pek);
	}

	free(ins);
	instr[nr] = NULL;
}

static void freeAllInstr(void)
{
	for (uint16_t i = 0; i <= 128; i++)
		freeInstr(i);
}

static void freeAllPatterns(void) // 8bb: added this one, since it's handy
{
	for (int32_t i = 0; i < 256; i++)
	{
		if (patt[i] != NULL)
		{
			free(patt[i]);
			patt[i] = NULL;
		}

		pattLens[i] = 64;
	}
}

static void delta2Samp(int8_t *p, uint32_t len, bool sample16Bit)
{
	if (sample16Bit)
	{
		len >>= 1;
	
		int16_t *p16 = (int16_t *)p;

		int16_t olds16 = 0;
		for (uint32_t i = 0; i < len; i++)
		{
			const int16_t news16 = p16[i] + olds16;
			p16[i] = news16;
			olds16 = news16;
		}
	}
	else
	{
		int8_t *p8 = (int8_t *)p;

		int8_t olds8 = 0;
		for (uint32_t i = 0; i < len; i++)
		{
			const int8_t news8 = p8[i] + olds8;
			p8[i] = news8;
			olds8 = news8;
		}
	}
}

static void unpackPatt(uint8_t *dst, uint16_t inn, uint16_t len, uint8_t antChn)
{
	if (dst == NULL)
		return;

	const uint8_t *src = dst + inn;
	const int32_t srcEnd = len * (sizeof (tonTyp) * antChn);

	int32_t srcIdx = 0;
	for (int32_t i = 0; i < len; i++)
	{
		for (int32_t j = 0; j < antChn; j++)
		{
			if (srcIdx >= srcEnd)
				return; // error!

			const uint8_t note = *src++;
			if (note & 0x80)
			{
				*dst++ = (note & 0x01) ? *src++ : 0;
				*dst++ = (note & 0x02) ? *src++ : 0;
				*dst++ = (note & 0x04) ? *src++ : 0;
				*dst++ = (note & 0x08) ? *src++ : 0;
				*dst++ = (note & 0x10) ? *src++ : 0;
			}
			else
			{
				*dst++ = note;
				*dst++ = *src++;
				*dst++ = *src++;
				*dst++ = *src++;
				*dst++ = *src++;
			}

			// 8bb: added this. If note >97, remove it (prevents out-of-range read in note->sample LUT)
			if (*(dst-5) > 97)
				*(dst-5) = 0;

			srcIdx += sizeof (tonTyp);
		}
	}
}

void freeMusic(void)
{
	stopMusic();
	freeAllInstr();
	freeAllPatterns();

	song.tempo = 6;
	song.speed = 125;
	song.timer = 1;

	setFrqTab(true);
	resetMusic();
}

void stopVoices(void)
{
	stmTyp *ch = stm;
	for (uint8_t i = 0; i < 32; i++, ch++)
	{
		ch->tonTyp = 0;
		ch->relTonNr = 0;
		ch->instrNr = 0;
		ch->instrSeg = instr[0]; // 8bb: placeholder instrument
		ch->status = IS_Vol;

		ch->realVol = 0;
		ch->outVol = 0;
		ch->oldVol = 0;
		ch->finalVol = 0;
		ch->oldPan = 128;
		ch->outPan = 128;
		ch->finalPan = 128;
		ch->vibDepth = 0;
	}
}

static void resetMusic(void)
{
	song.timer = 1;
	stopVoices();
	setPos(0, 0);
}

void setPos(int32_t pos, int32_t row) // -1 = don't change
{
	if (pos != -1)
	{
		song.songPos = (int16_t)pos;
		if (song.len > 0 && song.songPos >= song.len)
			song.songPos = song.len - 1;

		song.pattNr = song.songTab[song.songPos];
		song.pattLen = pattLens[song.pattNr];
	}

	if (row != -1)
	{
		song.pattPos = (int16_t)row;
		if (song.pattPos >= song.pattLen)
			song.pattPos = song.pattLen - 1;
	}

	song.timer = 1;
}

/***************************************************************************
 *        MODULE LOADING ROUTINES                                          *
 ***************************************************************************/

static bool loadInstrHeader(MEMFILE *f, uint16_t i)
{
	instrHeaderTyp ih;

	memset(&ih, 0, INSTR_HEADER_SIZE);
	mread(&ih.instrSize, 4, 1, f);
	if (ih.instrSize > INSTR_HEADER_SIZE) ih.instrSize = INSTR_HEADER_SIZE;

	if (ih.instrSize < 4) // 8bb: added protection
		return false;

	mread(ih.name, ih.instrSize-4, 1, f);

	if (ih.antSamp > 16)
		return false;

	if (ih.antSamp > 0)
	{
		if (!allocateInstr(i))
			return false;

		instrTyp *ins = instr[i];

		memcpy(ins->name, ih.name, 22);
		ins->name[22] = '\0';

		// 8bb: copy instrument header elements to our instrument struct
		memcpy(ins->ta, ih.ta, 96);
		memcpy(ins->envVP, ih.envVP, 12*2*sizeof(int16_t));
		memcpy(ins->envPP, ih.envPP, 12*2*sizeof(int16_t));
		ins->envVPAnt = ih.envVPAnt;
		ins->envPPAnt = ih.envPPAnt;
		ins->envVSust = ih.envVSust;
		ins->envVRepS = ih.envVRepS;
		ins->envVRepE = ih.envVRepE;
		ins->envPSust = ih.envPSust;
		ins->envPRepS = ih.envPRepS;
		ins->envPRepE = ih.envPRepE;
		ins->envVTyp = ih.envVTyp;
		ins->envPTyp = ih.envPTyp;
		ins->vibTyp = ih.vibTyp;
		ins->vibSweep = ih.vibSweep;
		ins->vibDepth = ih.vibDepth;
		ins->vibRate = ih.vibRate;
		ins->fadeOut = ih.fadeOut;
		ins->mute = (ih.mute == 1) ? true : false; // 8bb: correct logic!
		ins->antSamp = ih.antSamp;

		if (mread(ih.samp, ih.antSamp * sizeof (sampleHeaderTyp), 1, f) != 1)
			return false;

		sampleTyp *s = instr[i]->samp;
		sampleHeaderTyp *src = ih.samp;
		for (int32_t j = 0; j < ih.antSamp; j++, s++, src++)
		{
			memcpy(s->name, src->name, 22);
			s->name[22] = '\0';

			s->len = src->len;
			s->repS = src->repS;
			s->repL = src->repL;
			s->vol = src->vol;
			s->fine = src->fine;
			s->typ = src->typ;
			s->pan = src->pan;
			s->relTon = src->relTon;
		}
	}

	return true;
}

static bool loadInstrSample(MEMFILE *f, uint16_t i)
{
	if (instr[i] == NULL)
		return true; // empty instrument

	sampleTyp *s = instr[i]->samp;
	for (uint16_t j = 0; j < instr[i]->antSamp; j++, s++)
	{
		if (s->len > 0)
		{
			bool sample16Bit = !!(s->typ & SAMPLE_16BIT);

			s->pek = (int8_t *)malloc(s->len+2); // 8bb: +2 for fixed interpolation tap sample
			if (s->pek == NULL)
				return false;

			mread(s->pek, 1, s->len, f);
			delta2Samp(s->pek, s->len, sample16Bit);
		}

		checkSampleRepeat(i, j);
	}

	return true;
}

static bool loadPatterns(MEMFILE *f, uint16_t antPtn)
{
	uint8_t tmpLen;
	patternHeaderTyp ph;

	for (uint16_t i = 0; i < antPtn; i++)
	{
		mread(&ph.patternHeaderSize, 4, 1, f);
		mread(&ph.typ, 1, 1, f);

		ph.pattLen = 0;
		if (song.ver == 0x0102)
		{
			mread(&tmpLen, 1, 1, f);
			mread(&ph.dataLen, 2, 1, f);
			ph.pattLen = (uint16_t)tmpLen + 1; // 8bb: +1 in v1.02

			if (ph.patternHeaderSize > 8)
				mseek(f, ph.patternHeaderSize - 8, SEEK_CUR);
		}
		else
		{
			mread(&ph.pattLen, 2, 1, f);
			mread(&ph.dataLen, 2, 1, f);

			if (ph.patternHeaderSize > 9)
				mseek(f, ph.patternHeaderSize - 9, SEEK_CUR);
		}

		if (meof(f))
		{
			mclose(&f);
			return false;
		}

		pattLens[i] = ph.pattLen;
		if (ph.dataLen)
		{
			const uint16_t a = ph.pattLen * song.antChn * sizeof (tonTyp);

			patt[i] = (tonTyp *)malloc(a);
			if (patt[i] == NULL)
				return false;

			uint8_t *pattPtr = (uint8_t *)patt[i];

			memset(pattPtr, 0, a);
			mread(&pattPtr[a - ph.dataLen], 1, ph.dataLen, f);
			unpackPatt(pattPtr, a - ph.dataLen, ph.pattLen, song.antChn);
		}

		if (patternEmpty(i))
		{
			if (patt[i] != NULL)
			{
				free(patt[i]);
				patt[i] = NULL;
			}

			pattLens[i] = 64;
		}
	}

	return true;
}

static bool loadMusicMOD(MEMFILE *f)
{
	uint8_t ha[sizeof (songMOD31HeaderTyp)];
	songMOD31HeaderTyp *h_MOD31 = (songMOD31HeaderTyp *)ha;
	songMOD15HeaderTyp *h_MOD15 = (songMOD15HeaderTyp *)ha;

	mread(ha, sizeof (ha), 1, f);
	if (meof(f))
		goto loadError2;

	memcpy(song.name, h_MOD31->name, 20);
	song.name[20] = '\0';

	uint8_t j = 0;
	for (uint8_t i = 1; i <= 16; i++)
	{
		if (memcmp(h_MOD31->Sig, MODSig[i-1], 4) == 0)
			j = i + i;
	}

	if (memcmp(h_MOD31->Sig, "M!K!", 4) == 0 || memcmp(h_MOD31->Sig, "FLT4", 4) == 0)
		j = 4;

	if (memcmp(h_MOD31->Sig, "OCTA", 4) == 0)
		j = 8;

	uint8_t typ;
	if (j > 0)
	{
		typ = 1;
		song.antChn = j;
	}
	else
	{
		typ = 2;
		song.antChn = 4;
	}

	int16_t ai;
	if (typ == 1)
	{
		mseek(f, sizeof (songMOD31HeaderTyp), SEEK_SET);
		song.len = h_MOD31->len;
		song.repS = h_MOD31->repS;
		memcpy(song.songTab, h_MOD31->songTab, 128);
		ai = 31;
	}
	else
	{
		mseek(f, sizeof (songMOD15HeaderTyp), SEEK_SET);
		song.len = h_MOD15->len;
		song.repS = h_MOD15->repS;
		memcpy(song.songTab, h_MOD15->songTab, 128);
		ai = 15;
	}

	song.antInstrs = ai; // 8bb: added this

	if (meof(f))
		goto loadError2;

	int32_t b = 0;
	for (int32_t a = 0; a < 128; a++)
	{
		if (song.songTab[a] > b)
			b = song.songTab[a];
	}

	uint8_t pattBuf[32 * 4 * 64]; // 8bb: max pattern size (32 channels, 64 rows)
	for (uint16_t a = 0; a <= b; a++)
	{
		patt[a] = (tonTyp *)calloc(song.antChn * 64, sizeof (tonTyp));
		if (patt[a] == NULL)
			goto loadError;

		pattLens[a] = 64;

		mread(pattBuf, 1, song.antChn * 4 * 64, f);
		if (meof(f))
			goto loadError;

		// convert pattern
		uint8_t *bytes = pattBuf;
		tonTyp *ton = patt[a];
		for (int32_t i = 0; i < 64 * song.antChn; i++, bytes += 4, ton++)
		{
			const uint16_t period = ((bytes[0] & 0x0F) << 8) | bytes[1];
			for (uint8_t k = 0; k < 96; k++)
			{
				if (period >= amigaPeriod[k])
				{
					ton->ton = k+1;
					break;
				}
			}

			ton->instr = (bytes[0] & 0xF0) | (bytes[2] >> 4);
			ton->effTyp = bytes[2] & 0x0F;
			ton->eff = bytes[3];

			switch (ton->effTyp)
			{
				case 0xC:
				{
					if (ton->eff > 64)
						ton->eff = 64;
				}
				break;

				case 0x1:
				case 0x2:
				{
					if (ton->eff == 0)
						ton->effTyp = 0;
				}
				break;

				case 0x5:
				{
					if (ton->eff == 0)
						ton->effTyp = 3;
				}
				break;

				case 0x6:
				{
					if (ton->eff == 0)
						ton->effTyp = 4;
				}
				break;

				case 0xA:
				{
					if (ton->eff == 0)
						ton->effTyp = 0;
				}
				break;

				case 0xE:
				{
					const uint8_t effTyp = ton->effTyp >> 4;
					const uint8_t eff = ton->effTyp & 15;

					if (eff == 0 && (effTyp == 0x1 || effTyp == 0x2 || effTyp == 0xA || effTyp == 0xB))
					{
						ton->eff = 0;
						ton->effTyp = 0;
					}
				}
				break;
				
				default: break;
			}
		}

		if (patternEmpty(a))
		{
			free(patt[a]);
			patt[a] = NULL;
			pattLens[a] = 64;
		}
	}

	for (uint16_t a = 1; a <= ai; a++)
	{
		modSampleTyp *modSmp = &h_MOD31->sample[a-1];

		uint32_t len = 2 * SWAP16(modSmp->len);
		if (len == 0)
			continue;

		if (!allocateInstr(a))
			goto loadError;

		sampleTyp *xmSmp = &instr[a]->samp[0];

		memcpy(xmSmp->name, modSmp->name, 22);
		xmSmp->name[22] = '\0';

		uint32_t repS = 2 * SWAP16(modSmp->repS);
		uint32_t repL = 2 * SWAP16(modSmp->repL);

		if (repL <= 2)
		{
			repS = 0;
			repL = 0;
		}

		if (repS+repL > len)
		{
			if (repS >= len)
			{
				repS = 0;
				repL = 0;
			}
			else
			{
				repL = len-repS;
			}
		}

		xmSmp->typ = (repL > 2) ? 1 : 0;
		xmSmp->len = len;
		xmSmp->vol = (modSmp->vol <= 64) ? modSmp->vol : 64;
		xmSmp->fine = 8 * ((2 * ((modSmp->fine & 15) ^ 8)) - 16);
		xmSmp->repL = repL;
		xmSmp->repS = repS;

		xmSmp->pek = (int8_t *)malloc(len + 2);
		if (xmSmp->pek == NULL)
			goto loadError;

		mread(xmSmp->pek, 1, len, f);
	}

	mclose(&f);

	if (song.repS > song.len)
		song.repS = 0;

	resetMusic();
	upDateInstrs();

	moduleLoaded = true;
	return true;
loadError:
	freeAllInstr();
	freeAllPatterns();
loadError2:
	mclose(&f);
	return false;
}

bool loadMusicFromData(const uint8_t *data, uint32_t dataLength) // .XM/.MOD/.FT
{
	uint16_t i;
	songHeaderTyp h;

	freeMusic();
	setFrqTab(false);

	moduleLoaded = false;

	MEMFILE *f = mopen(data, dataLength);
	if (f == NULL)
		return false;

	// 8bb: instr 0 is a placeholder for empty instruments
	allocateInstr(0);
	instr[0]->samp[0].vol = 0;

	mread(&h, sizeof (h), 1, f);
	if (meof(f))
		goto loadError2;

	if (memcmp(h.sig, "Extended Module: ", 17) != 0)
	{
		mrewind(f);
		return loadMusicMOD(f);
	}

	if (h.ver < 0x0102 || h.ver > 0x104 || h.antChn < 2 || h.antChn > 32 || (h.antChn & 1) != 0 ||
		h.antPtn > 256 || h.antInstrs > 128)
	{
		goto loadError2;
	}

	mseek(f, 60+h.headerSize, SEEK_SET);
	if (meof(f))
		goto loadError2;

	memcpy(song.name, h.name, 20);
	song.name[20] = '\0';

	song.len = h.len;
	song.repS = h.repS;
	song.antChn = (uint8_t)h.antChn;
	bool linearFrequencies = !!(h.flags & LINEAR_FREQUENCIES);
	setFrqTab(linearFrequencies);
	memcpy(song.songTab, h.songTab, 256);

	song.antInstrs = h.antInstrs; // 8bb: added this
	if (h.defSpeed == 0) h.defSpeed = 125; // 8bb: (BPM) FT2 doesn't do this, but we do it for safety
	song.speed = h.defSpeed;
	song.tempo = h.defTempo;
	song.ver = h.ver;

	// 8bb: bugfixes...
	if (song.speed < 1) song.speed = 1;
	if (song.tempo < 1) song.tempo = 1;
	// ----------------

	if (song.ver < 0x0104) // old FT2 XM format
	{
		for (i = 1; i <= h.antInstrs; i++)
		{
			if (!loadInstrHeader(f, i))
				goto loadError;
		}

		if (!loadPatterns(f, h.antPtn))
			goto loadError;

		for (i = 1; i <= h.antInstrs; i++)
		{
			if (!loadInstrSample(f, i))
				goto loadError;
		}
	}
	else // latest FT2 XM format
	{
		if (!loadPatterns(f, h.antPtn))
			goto loadError;

		for (i = 1; i <= h.antInstrs; i++)
		{
			if (!loadInstrHeader(f, i))
				goto loadError;

			if (!loadInstrSample(f, i))
				goto loadError;
		}
	}

	mclose(&f);

	if (song.repS > song.len)
		song.repS = 0;

	resetMusic();
	upDateInstrs();

	moduleLoaded = true;
	return true;

loadError:
	freeAllInstr();
	freeAllPatterns();
loadError2:
	mclose(&f);
	return false;
}

/***************************************************************************
 *        PROCESS HANDLING                                                 *
 ***************************************************************************/

bool startMusic(void)
{
	if (!moduleLoaded || song.speed == 0)
		return false;

	mix_ClearChannels();
	stopVoices();
	song.globVol = 64;

	speedVal = ((realReplayRate * 5) / 2) / song.speed;
	quickVolSizeVal = realReplayRate / 200;

	if (!mix_Init(soundBufferSize))
		return false;

	musicPaused = false;
	return true;
}

void stopMusic(void)
{
	pauseMusic();

	mix_Free();
	song.globVol = 64;

	resumeMusic();
}

void startPlaying(void)
{
	stopMusic();
	song.pattDelTime = song.pattDelTime2 = 0; // 8bb: added these
	setPos(0, 0);
	startMusic();
}

void stopPlaying(void)
{
	stopMusic();
	stopVoices();
}

void pauseMusic(void)
{
	musicPaused = true;
}

void resumeMusic(void)
{
	musicPaused = false;
}

// 8bb: added these three, handy
void toggleMusic(void)
{
	musicPaused ^= 1;
}

void setInterpolation(bool on)
{
	interpolationFlag = on;
	mix_ClearChannels();
}

void setVolumeRamping(bool on)
{
	volumeRampingFlag = on;
	mix_ClearChannels();
}

/***************************************************************************
 *        CONFIGURATION ROUTINES                                           *
 ***************************************************************************/

void setMasterVol(int32_t v) // 0..256
{
	masterVol = CLAMP(v, 0, 256);

	stmTyp *ch = stm;
	for (int32_t i = 0; i < 32; i++, ch++)
		ch->status |= IS_Vol;
}

void setAmp(int32_t level) // 1..32
{
	boostLevel = (int16_t)CLAMP(level, 1, 32);
	CDA_Amp = boostLevel * 8;
}

int32_t getMasterVol(void) // 8bb: added this
{
	return masterVol;
}

int32_t getAmp(void) // 8bb: added this
{
	return boostLevel;
}

uint8_t getNumActiveVoices(void) // 8bb: added this
{
	uint8_t activeVoices = 0;
	for (int32_t i = 0; i < song.antChn; i++)
	{
		CIType *v = getVoice(i);
		if (!(v->SType & SType_Off) && v->SVol > 0)
			activeVoices++;
	}

	return activeVoices;
}

static void setFrqTab(bool linear)
{
	linearFrqTab = linear;
	note2Period = linear ? linearPeriods : amigaPeriods;
}

void updateReplayRate(void)
{
	// 8bb: bit-exact to FT2
	frequenceDivFactor = (uint32_t)round(65536.0*1712.0/realReplayRate*8363.0);
	frequenceMulFactor = (uint32_t)round(256.0*65536.0/realReplayRate*8363.0);
}

/***************************************************************************
 *        INITIALIZATION ROUTINES                                          *
 ***************************************************************************/

bool initMusic(int32_t audioFrequency, int32_t audioBufferSize, bool interpolation, bool volumeRamping)
{
	freeMusic();
	memset(stm, 0, sizeof (stm));

	realReplayRate = CLAMP(audioFrequency, 8000, 96000);
	updateReplayRate();

	soundBufferSize = audioBufferSize;
	interpolationFlag = interpolation;
	volumeRampingFlag = volumeRamping;

	song.tempo = 6;
	song.speed = 125;
	setFrqTab(true);
	resetMusic();

	return true;
}

/***************************************************************************
 *        MEMORY READ ROUTINES (8bb: added these)                          *
 ***************************************************************************/

static MEMFILE *mopen(const uint8_t *src, uint32_t length)
{
	if (src == NULL || length == 0)
		return NULL;

	MEMFILE *b = (MEMFILE *)malloc(sizeof (MEMFILE));
	if (b == NULL)
		return NULL;

	b->_base = (uint8_t *)src;
	b->_ptr = (uint8_t *)src;
	b->_cnt = length;
	b->_bufsiz = length;
	b->_eof = false;
 
	return b;
}

static void mclose(MEMFILE **buf)
{
	if (*buf != NULL)
	{
		free(*buf);
		*buf = NULL;
	}
}

static size_t mread(void *buffer, size_t size, size_t count, MEMFILE *buf)
{
	if (buf == NULL || buf->_ptr == NULL)
		return 0;

	size_t wrcnt = size * count;
	if (size == 0 || buf->_eof)
		return 0;

	int32_t pcnt = (buf->_cnt > wrcnt) ? (int32_t)wrcnt : (int32_t)buf->_cnt;
	memcpy(buffer, buf->_ptr, pcnt);

	buf->_cnt -= pcnt;
	buf->_ptr += pcnt;

	if (buf->_cnt <= 0)
	{
		buf->_ptr = buf->_base + buf->_bufsiz;
		buf->_cnt = 0;
		buf->_eof = true;
	}

	return pcnt / size;
}

static bool meof(MEMFILE *buf)
{
	if (buf == NULL)
		return true;

	return buf->_eof;
}

static void mseek(MEMFILE *buf, int32_t offset, int32_t whence)
{
	if (buf == NULL)
		return;

	if (buf->_base)
	{
		switch (whence)
		{
			case SEEK_SET: buf->_ptr = buf->_base + offset; break;
			case SEEK_CUR: buf->_ptr += offset; break;
			case SEEK_END: buf->_ptr = buf->_base + buf->_bufsiz + offset; break;
			default: break;
		}

		buf->_eof = false;
		if (buf->_ptr >= buf->_base+buf->_bufsiz)
		{
			buf->_ptr = buf->_base + buf->_bufsiz;
			buf->_eof = true;
		}

		buf->_cnt = (buf->_base + buf->_bufsiz) - buf->_ptr;
	}
}

static void mrewind(MEMFILE *buf)
{
	mseek(buf, 0, SEEK_SET);
}
