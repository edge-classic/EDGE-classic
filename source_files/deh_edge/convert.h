//------------------------------------------------------------------------
//  Patch Conversion tables
//------------------------------------------------------------------------
// 
//  DEH_EDGE  Copyright (C) 2004-2005  The EDGE Team
// 
//  This program is under the GNU General Public License.
//  It comes WITHOUT ANY WARRANTY of any kind.
//  See COPYING.txt for the full details.
//
//------------------------------------------------------------------------

#ifndef __CONVERT_HDR__
#define __CONVERT_HDR__

namespace Deh_Edge
{

#define THINGS_1_2   103
#define FRAMES_1_2   512
#define SPRITES_1_2  105
#define SOUNDS_1_2   63

#define TEXTS_1_6  1053

#define POINTER_NUM      448
#define POINTER_NUM_BEX  468

extern short thing12to166[THINGS_1_2];
extern short frame12to166[FRAMES_1_2];
extern short sprite12to166[SPRITES_1_2];
extern short sound12to166[SOUNDS_1_2];
extern short pointerToFrame[POINTER_NUM_BEX];

}  // Deh_Edge

#endif /* __CONVERT_HDR__ */
