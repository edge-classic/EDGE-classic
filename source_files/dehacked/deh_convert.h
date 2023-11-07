//------------------------------------------------------------------------
//  Patch Conversion tables
//------------------------------------------------------------------------
//
//  DEH_EDGE  Copyright (C) 2004-2023  The EDGE Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License (in COPYING.txt) for more details.
//
//------------------------------------------------------------------------

#ifndef __DEH_CONVERT_HDR__
#define __DEH_CONVERT_HDR__

namespace Deh_Edge
{

#define THINGS_1_2  103
#define FRAMES_1_2  512
#define SPRITES_1_2 105
#define SOUNDS_1_2  63

#define TEXTS_1_6 1053

#define POINTER_NUM     448
#define POINTER_NUM_BEX 468

extern short thing12to166[THINGS_1_2];
extern short frame12to166[FRAMES_1_2];
extern short sprite12to166[SPRITES_1_2];
extern short sound12to166[SOUNDS_1_2];

} // namespace Deh_Edge

#endif /* __DEH_CONVERT_HDR__ */
