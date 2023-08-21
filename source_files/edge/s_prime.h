//----------------------------------------------------------------------------
//  EDGE Primesynth Music Player
//----------------------------------------------------------------------------
// 
//  Copyright (c) 2023 The EDGE Team.
// 
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//----------------------------------------------------------------------------

#ifndef __S_PRIME_H__
#define __S_PRIME_H__

#include "i_defs.h"

extern bool prime_disabled;

bool S_StartupPrime(void);

void S_RestartPrime(void);

abstract_music_c * S_PlayPrime(byte *data, int length, bool loop);

#endif /* __S_PRIME_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
