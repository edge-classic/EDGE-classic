#ifndef __OPLINTF_H__
#define __OPLINTF_H__

#include "../EmuStructs.h"

#define EC_YM3812_ADLIBEMU	// enable AdLibEmu core (from DOSBox)


#ifdef SNDDEV_YM3812
extern const DEV_DEF* devDefList_YM3812[];
#endif
#ifdef SNDDEV_YM3526
extern const DEV_DEF* devDefList_YM3526[];
#endif
#ifdef SNDDEV_Y8950
extern const DEV_DEF* devDefList_Y8950[];
#endif

#endif	// __OPLINTF_H__
