//----------------------------------------------------------------------------
//  EDGE MUS to Midi conversion
//----------------------------------------------------------------------------
//
//  Copyright (c) 2002-2008  The EDGE Team.
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
//
//  Based on MMUS2MID code from PrBoom, which was based on the QMUSMID
//  program by Sebastien Bacquet.
//
//----------------------------------------------------------------------------

#ifndef __EPI_MUS2MIDI_H__
#define __EPI_MUS2MIDI_H__

#include "types.h"

namespace Mus2Midi
{
	const int DEF_DIVIS  = 70;
	const int DOOM_DIVIS = 89;  // -AJA- no idea why this value.

	extern bool Convert(const byte *mus, int mus_len,
			byte **midi, int *midi_len, int division, bool nocomp);

	extern void Free(byte *midi);

	extern const char *ReturnError(void);
}

#endif /* __EPI_MUS2MIDI_H */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
