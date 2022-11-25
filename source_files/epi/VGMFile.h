#ifndef __VGMFILE_H__
#define __VGMFILE_H__

#include "epi.h"
// Header file for VGM file handling

typedef struct _vgm_file_header
{
	u32_t fccVGM;
	u32_t lngEOFOffset;
	u32_t lngVersion;
	u32_t lngHzPSG;
	u32_t lngHzYM2413;
	u32_t lngGD3Offset;
	u32_t lngTotalSamples;
	u32_t lngLoopOffset;
	u32_t lngLoopSamples;
	u32_t lngRate;
	u16_t shtPSG_Feedback;
	u8_t bytPSG_SRWidth;
	u8_t bytPSG_Flags;
	u32_t lngHzYM2612;
	u32_t lngHzYM2151;
	u32_t lngDataOffset;
	u32_t lngHzSPCM;
	u32_t lngSPCMIntf;
	u32_t lngHzRF5C68;
	u32_t lngHzYM2203;
	u32_t lngHzYM2608;
	u32_t lngHzYM2610;
	u32_t lngHzYM3812;
	u32_t lngHzYM3526;
	u32_t lngHzY8950;
	u32_t lngHzYMF262;
	u32_t lngHzYMF278B;
	u32_t lngHzYMF271;
	u32_t lngHzYMZ280B;
	u32_t lngHzRF5C164;
	u32_t lngHzPWM;
	u32_t lngHzAY8910;
	u8_t bytAYType;
	u8_t bytAYFlag;
	u8_t bytAYFlagYM2203;
	u8_t bytAYFlagYM2608;
	u8_t bytVolumeModifier;
	u8_t bytReserved2;
	s8_t bytLoopBase;
	u8_t bytLoopModifier;
	u32_t lngHzGBDMG;
	u32_t lngHzNESAPU;
	u32_t lngHzMultiPCM;
	u32_t lngHzUPD7759;
	u32_t lngHzOKIM6258;
	u8_t bytOKI6258Flags;
	u8_t bytK054539Flags;
	u8_t bytC140Type;
	u8_t bytReservedFlags;
	u32_t lngHzOKIM6295;
	u32_t lngHzK051649;
	u32_t lngHzK054539;
	u32_t lngHzHuC6280;
	u32_t lngHzC140;
	u32_t lngHzK053260;
	u32_t lngHzPokey;
	u32_t lngHzQSound;
	u32_t lngHzSCSP;
//	u32_t lngHzOKIM6376;
	//u8_t bytReserved[0x04];
	u32_t lngExtraOffset;
	u32_t lngHzWSwan;
	u32_t lngHzVSU;
	u32_t lngHzSAA1099;
	u32_t lngHzES5503;
	u32_t lngHzES5506;
	u8_t bytES5503Chns;
	u8_t bytES5506Chns;
	u8_t bytC352ClkDiv;
	u8_t bytESReserved;
	u32_t lngHzX1_010;
	u32_t lngHzC352;
	u32_t lngHzGA20;
} VGM_HEADER;
typedef struct _vgm_header_extra
{
	u32_t DataSize;
	u32_t Chp2ClkOffset;
	u32_t ChpVolOffset;
} VGM_HDR_EXTRA;
typedef struct _vgm_extra_chip_data32
{
	u8_t Type;
	u32_t Data;
} VGMX_CHIP_DATA32;
typedef struct _vgm_extra_chip_data16
{
	u8_t Type;
	u8_t Flags;
	u16_t Data;
} VGMX_CHIP_DATA16;
typedef struct _vgm_extra_chip_extra32
{
	u8_t ChipCnt;
	VGMX_CHIP_DATA32* CCData;
} VGMX_CHP_EXTRA32;
typedef struct _vgm_extra_chip_extra16
{
	u8_t ChipCnt;
	VGMX_CHIP_DATA16* CCData;
} VGMX_CHP_EXTRA16;
typedef struct _vgm_header_extra_data
{
	VGMX_CHP_EXTRA32 Clocks;
	VGMX_CHP_EXTRA16 Volumes;
} VGM_EXTRA;

#define VOLUME_MODIF_WRAP	0xC0
typedef struct _vgm_gd3_tag
{
	u32_t fccGD3;
	u32_t lngVersion;
	u32_t lngTagLength;
	wchar_t* strTrackNameE;
	wchar_t* strTrackNameJ;
	wchar_t* strGameNameE;
	wchar_t* strGameNameJ;
	wchar_t* strSystemNameE;
	wchar_t* strSystemNameJ;
	wchar_t* strAuthorNameE;
	wchar_t* strAuthorNameJ;
	wchar_t* strReleaseDate;
	wchar_t* strCreator;
	wchar_t* strNotes;
} GD3_TAG;
typedef struct _vgm_pcm_bank_data
{
	u32_t DataSize;
	u8_t* Data;
	u32_t DataStart;
} VGM_PCM_DATA;
typedef struct _vgm_pcm_bank
{
	u32_t BankCount;
	VGM_PCM_DATA* Bank;
	u32_t DataSize;
	u8_t* Data;
	u32_t DataPos;
	u32_t BnkPos;
} VGM_PCM_BANK;

#define FCC_VGM	0x206D6756	// 'Vgm '
#define FCC_GD3	0x20336447	// 'Gd3 '

#endif	// __VGMFILE_H__
