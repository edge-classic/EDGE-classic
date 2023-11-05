#pragma once

#include <stdint.h>
#include <stdbool.h>

enum // voice flags
{
	IS_Vol = 1,
	IS_Period = 2,
	IS_NyTon = 4,
	IS_Pan = 8,
	IS_QuickVol = 16
};

enum // note
{
	NOTE_KEYOFF = 97
};

enum // header flags
{
	LINEAR_FREQUENCIES = 1
};

enum // sample flags
{
	LOOP_OFF = 0,
	LOOP_FORWARD = 1,
	LOOP_PINGPONG = 2,
	SAMPLE_16BIT = 16
};

enum // envelope flags
{
	ENV_ENABLED = 1,
	ENV_SUSTAIN = 2,
	ENV_LOOP    = 4
};

typedef struct songTyp_t
{
	char name[20+1];
	uint8_t antChn, pattDelTime, pattDelTime2, pBreakPos, songTab[256];
	bool pBreakFlag, posJumpFlag;
	int16_t songPos, pattNr, pattPos, pattLen;
	uint16_t len, repS, speed, tempo, globVol, timer, ver;

	uint16_t antInstrs; // 8bb: added this
} songTyp;

typedef struct sampleTyp_t
{
	char name[22+1];
	int32_t len, repS, repL;
	uint8_t vol;
	int8_t fine;
	uint8_t typ, pan;
	int8_t relTon;
	int8_t *pek;
} sampleTyp;

typedef struct instrTyp_t
{
	char name[22+1];
	uint8_t ta[96];
	int16_t envVP[12][2], envPP[12][2];
	uint8_t envVPAnt, envPPAnt;
	uint8_t envVSust, envVRepS, envVRepE;
	uint8_t envPSust, envPRepS, envPRepE;
	uint8_t envVTyp, envPTyp;
	uint8_t vibTyp, vibSweep, vibDepth, vibRate;
	uint16_t fadeOut;
	uint8_t mute;
	int16_t antSamp;
	sampleTyp samp[16];
} instrTyp;

typedef struct stmTyp_t
{
	volatile uint8_t status;
	int8_t relTonNr, fineTune;
	uint8_t sampleNr, instrNr, effTyp, eff, smpOffset, tremorSave, tremorPos;
	uint8_t globVolSlideSpeed, panningSlideSpeed, mute, waveCtrl, portaDir;
	uint8_t glissFunk, vibPos, tremPos, vibSpeed, vibDepth, tremSpeed, tremDepth;
	uint8_t pattPos, loopCnt, volSlideSpeed, fVolSlideUpSpeed, fVolSlideDownSpeed;
	uint8_t fPortaUpSpeed, fPortaDownSpeed, ePortaUpSpeed, ePortaDownSpeed;
	uint8_t portaUpSpeed, portaDownSpeed, retrigSpeed, retrigCnt, retrigVol;
	uint8_t volKolVol, tonNr, envPPos, eVibPos, envVPos, realVol, oldVol, outVol;
	uint8_t oldPan, outPan, finalPan;
	bool envSustainActive;
	int16_t envVIPValue, envPIPValue;
	uint16_t outPeriod, realPeriod, finalPeriod, finalVol, tonTyp, wantPeriod, portaSpeed;
	uint16_t envVCnt, envVAmp, envPCnt, envPAmp, eVibAmp, eVibSweep;
	uint16_t fadeOutAmp, fadeOutSpeed;
	int32_t smpStartPos;
	instrTyp *instrSeg;
} stmTyp;

#ifdef _MSC_VER
#pragma pack(push)
#pragma pack(1)
#endif
typedef struct tonTyp_t // this one must be packed on some systems
{
	uint8_t ton, instr, vol, effTyp, eff;
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
tonTyp;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

// globalized
extern volatile bool interpolationFlag, volumeRampingFlag, moduleLoaded, musicPaused;
extern bool linearFrqTab;
extern volatile const uint16_t *note2Period;
extern uint16_t pattLens[256];
extern int16_t PMPTmpActiveChannel, boostLevel;
extern int32_t masterVol, PMPLeft;
extern int32_t realReplayRate, quickVolSizeVal, speedVal;
extern uint32_t frequenceDivFactor, frequenceMulFactor;
extern uint32_t CDA_Amp;
extern tonTyp *patt[256];
extern instrTyp *instr[1+128];
extern songTyp song;
extern stmTyp stm[32];

#define CLAMP(x, low, high) (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

bool initMusic(int32_t audioFrequency, int32_t audioBufferSize, bool interpolation, bool volumeRamping);
bool loadMusicFromData(const uint8_t *data, uint32_t dataLength); // .XM/.MOD/.FT
void freeMusic(void);
bool startMusic(void);
void stopMusic();
void pauseMusic(void);
void resumeMusic(void);
void setMasterVol(int32_t v); // 0..256
void setAmp(int32_t level); // 1..32
void setPos(int32_t pos, int32_t row); // input of -1 = don't change
void stopVoices(void);
void updateReplayRate(void);
void startPlaying(void);
void stopPlaying(void);

// 8bb: added these three, handy
int32_t getMasterVol(void);
int32_t getAmp(void);
uint8_t getNumActiveVoices(void);
void toggleMusic(void);
void setInterpolation(bool on);
void setVolumeRamping(bool on);
