//----------------------------------------------------------------------------
//  EDGE IMF to VGM conversion header
//----------------------------------------------------------------------------
//
//  Copyright (c) 2015-2020 ValleyBell
//  Copyright (c) 2022  The EDGE Team.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//----------------------------------------------------------------------------

#ifndef __EPI_IMF2VGM_H__
#define __EPI_IMF2VGM_H__

#include "epi.h"

void ConvertIMF2VGM(u8_t *IMFBuffer, u32_t IMFBufferLen, u8_t *VGMBuffer, u32_t VGMBufferLen, s32_t IMFFreq, s32_t DevFreq);

extern u32_t vgm_header_size;

#endif