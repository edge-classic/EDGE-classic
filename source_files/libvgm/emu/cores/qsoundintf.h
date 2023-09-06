#ifndef __QSOUNDINTF_H__
#define __QSOUNDINTF_H__

#include "../EmuStructs.h"

#define EC_QSOUND_MAME		// enable QSound core from MAME


#define OPT_QSOUND_NOWAIT		0x01	// don't require waiting after initialization/filter changes


extern const DEV_DEF* devDefList_QSound[];

#endif	// __QSOUNDINTF_H__
